#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "ai.h"
#include "wtp.h"
#include "game.h"
#include "ai.h"
#include "aiProduction.h"
#include "aiFormer.h"
#include "aiHurry.h"

// global variables

int currentTurn = -1;
int aiFactionId = -1;

ActiveFactionInfo activeFactionInfo;

/*
Top level faction upkeep entry point.
*/
int aiFactionUpkeep(const int factionId)
{
	// recalculate global faction parameters on this turn if not yet done
	
	if (*current_turn != currentTurn || factionId != aiFactionId)
	{
		currentTurn = *current_turn;
		aiFactionId = factionId;
		
		aiStrategy();
		
	}
	
    // expire infiltrations for normal factions
    
    if (factionId > 0)
	{
		expireInfiltrations(factionId);
	}

	// consider hurrying production in all bases
	// affects AI factions only
	
	if (factionId > 0 && !is_human(factionId))
	{
		considerHurryingProduction(factionId);
	}

	// redirect to vanilla function for the rest of processing
	// that, in turn, is overriden by Thinker
	
	return faction_upkeep(factionId);
	
}

/*
Top level AI enemy move entry point
*/
/*

Transporting land vehicles by sea transports idea.

- Land vehicle not on transport
Land vehicle moves wherever it wants regardless if destination is reachable.
Land vehicle waits ferry pickup when it is at ocean or at the coast and wants to get to another land region.

- Empty transport
Empty transport rushes to nearest higher priority vehicle waiting for ferry.
When at or next to awaiting land vehicle it stops and explicitly commands passenger to board.

- Land vehicle on transport
Land vehicle on transport is not controlled by this AI code.

- Loaded transport
Loaded transport selects highest priority passenger.
Loaded transport then request highest priority passenger to generate desired destination.
Loaded transport then computes unload location closest to passenger destination location.
When at or next unload location it stops and explicitly commands passenger to unboard to unload location.

*/
int aiEnemyMove(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// do not try multiple iterations
	
	if (vehicle->iter_count > 0)
	{
		return mod_enemy_move(vehicleId);
	}
	
	// do not control boarded land units
	
	if (isVehicleLandUnitOnTransport(vehicleId))
	{
		return mod_enemy_move(vehicleId);
	}
	
	if (isVehicleColony(vehicleId))
	{
		return moveColony(vehicleId);
	}
	
	if (isVehicleFormer(vehicleId))
	{
		return moveFormer(vehicleId);
	}
	
	if (isVehicleSeaTransport(vehicleId))
	{
		return moveSeaTransport(vehicleId);
	}
	
	if (isVehicleCombat(vehicleId))
	{
		return moveCombat(vehicleId);
	}
	
	// unhandled cases default to Thinker
	
	return mod_enemy_move(vehicleId);
	
}

/*
AI strategy.
*/
void aiStrategy()
{
	debug("aiStrategy: aiFactionId=%d\n", aiFactionId);

	// populate shared strategy lists

	populateGlobalVariables();

	// prepare former orders

	aiTerraformingStrategy();

	// prepare combat orders

	aiCombatStrategy();

}

void populateGlobalVariables()
{
	// clear lists

	activeFactionInfo.baseIds.clear();
	activeFactionInfo.baseLocations.clear();
	activeFactionInfo.regionBaseGroups.clear();
	activeFactionInfo.baseStrategies.clear();
	activeFactionInfo.combatVehicleIds.clear();
	activeFactionInfo.outsideCombatVehicleIds.clear();
	activeFactionInfo.prototypes.clear();
	activeFactionInfo.colonyVehicleIds.clear();
	activeFactionInfo.formerVehicleIds.clear();
	activeFactionInfo.threatLevel = 0.0;

	// populate factions combat modifiers

	debug("%-24s\nfactionCombatModifiers:\n", MFactions[aiFactionId].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// store combat modifiers

		activeFactionInfo.factionInfos[id].offenseMultiplier = getFactionOffenseMultiplier(id);
		activeFactionInfo.factionInfos[id].defenseMultiplier = getFactionDefenseMultiplier(id);
		activeFactionInfo.factionInfos[id].fanaticBonusMultiplier = getFactionFanaticBonusMultiplier(id);

		debug
		(
			"\t%-24s: offenseMultiplier=%4.2f, defenseMultiplier=%4.2f, fanaticBonusMultiplier=%4.2f\n",
			MFactions[id].noun_faction,
			activeFactionInfo.factionInfos[id].offenseMultiplier,
			activeFactionInfo.factionInfos[id].defenseMultiplier,
			activeFactionInfo.factionInfos[id].fanaticBonusMultiplier
		);

	}

	debug("\n");

	// populate other factions threat koefficients

	debug("%-24s\notherFactionThreatKoefficients:\n", MFactions[aiFactionId].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// skip self

		if (id == aiFactionId)
			continue;

		// get relation from other faction

		int otherFactionRelation = Factions[id].diplo_status[aiFactionId];

		// calculate threat koefficient

		double threatKoefficient;

		if (otherFactionRelation & DIPLO_VENDETTA)
		{
			threatKoefficient = conf.ai_production_threat_coefficient_vendetta;
		}
		else if (otherFactionRelation & DIPLO_PACT)
		{
			threatKoefficient = conf.ai_production_threat_coefficient_pact;
		}
		else
		{
			threatKoefficient = conf.ai_production_threat_coefficient_other;
		}

		// add extra for treacherous human player

		if (is_human(id))
		{
			threatKoefficient += 0.50;
		}

		// store threat koefficient

		activeFactionInfo.factionInfos[id].threatKoefficient = threatKoefficient;

		debug("\t%-24s: %08x => %4.2f\n", MFactions[id].noun_faction, otherFactionRelation, threatKoefficient);

	}

	debug("\n");

	// populate bases

	debug("activeFactionInfo.baseStrategies\n");

	for (int id = 0; id < *total_num_bases; id++)
	{
		BASE *base = &(Bases[id]);

		// exclude not own bases

		if (base->faction_id != aiFactionId)
			continue;

		// add base

		activeFactionInfo.baseIds.push_back(id);

		// add base location

		MAP *baseLocation = getMapTile(base->x, base->y);
		activeFactionInfo.baseLocations[baseLocation] = id;

		// add base strategy

		activeFactionInfo.baseStrategies[id] = {};
		activeFactionInfo.baseStrategies[id].base = base;

		debug("\n[%3d] %-25s\n", id, activeFactionInfo.baseStrategies[id].base->name);

		// add base regions

		std::set<int> baseConnectedRegions = getBaseConnectedRegions(id);

		for (int region : baseConnectedRegions)
		{
			if (activeFactionInfo.regionBaseGroups.find(region) == activeFactionInfo.regionBaseGroups.end())
			{
				activeFactionInfo.regionBaseGroups[region] = std::vector<int>();
			}

			activeFactionInfo.regionBaseGroups[region].push_back(id);

		}

	}

	debug("\n");

	// populate vehicles

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);

		// store all vehicle current id in pad_0 field

		vehicle->pad_0 = id;

		// further process only own vehicles

		if (vehicle->faction_id != aiFactionId)
			continue;

		// combat vehicles

		if (isVehicleCombat(id))
		{
			// add vehicle

			activeFactionInfo.combatVehicleIds.push_back(id);

			// find if vehicle is at base

			MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);
			std::unordered_map<MAP *, int>::iterator baseLocationsIterator = activeFactionInfo.baseLocations.find(vehicleLocation);

			if (baseLocationsIterator == activeFactionInfo.baseLocations.end())
			{
				// add outside vehicle

				activeFactionInfo.outsideCombatVehicleIds.push_back(id);

			}
			else
			{
				BASE_STRATEGY *baseStrategy = &(activeFactionInfo.baseStrategies[baseLocationsIterator->second]);

				// add to garrison

				baseStrategy->garrison.push_back(id);

				// add to native protection

				double nativeProtection = calculateNativeDamageDefense(id) / 10.0;

				if (vehicle_has_ability(vehicle, ABL_TRANCE))
				{
					nativeProtection *= (1 + (double)Rules->combat_bonus_trance_vs_psi / 100.0);
				}

				baseStrategy->nativeProtection += nativeProtection;

			}

		}
		else if (isVehicleColony(id))
		{
			activeFactionInfo.colonyVehicleIds.push_back(id);
		}
		else if (isVehicleFormer(id))
		{
			activeFactionInfo.formerVehicleIds.push_back(id);
		}

	}

	// populate prototypes

    for (int i = 0; i < 128; i++)
	{
        int id = (i < 64 ? i : (aiFactionId - 1) * 64 + i);

        UNIT *unit = &Units[id];

		// skip not enabled

		if (id < 64 && !has_tech(aiFactionId, unit->preq_tech))
			continue;

        // skip empty

        if (strlen(unit->name) == 0)
			continue;

		// add prototype

		activeFactionInfo.prototypes.push_back(id);

	}
	
	// ==================================================
	// threat level
	// ==================================================
	
	activeFactionInfo.threatLevel = getThreatLevel();
	
}

VEH *getVehicleByAIId(int aiId)
{
	// check if ID didn't change

	VEH *oldVehicle = &(Vehicles[aiId]);

	if (oldVehicle->pad_0 == aiId)
		return oldVehicle;

	// otherwise, scan all vehicles

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);

		if (vehicle->pad_0 == aiId)
			return vehicle;

	}

	return NULL;

}

