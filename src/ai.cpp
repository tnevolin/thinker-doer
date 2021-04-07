#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "ai.h"
#include "wtp.h"
#include "game.h"
#include "aiProduction.h"
#include "aiFormer.h"
#include "aiHurry.h"

// global variables

// Controls which faction uses WtP algorithm.
// for debugging
// -1 : all factions

int wtpAIFactionId = -1;

// global variables for faction upkeep

int processedTurn = -1;
int processedFactionId = -1;

// global faction precomputed values

FACTION_INFO factionInfos[8];
std::vector<int> baseIds;
std::unordered_map<MAP *, int> baseLocations;
std::map<int, std::vector<int>> regionBaseGroups;
std::map<int, BASE_STRATEGY> baseStrategies;
std::vector<int> combatVehicleIds;
std::vector<int> outsideCombatVehicleIds;
std::vector<int> prototypes;
std::vector<int> colonyVehicleIds;
std::vector<int> formerVehicleIds;
double threatLevel;

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
	
	// recalculate global faction parameters on this turn if not yet done
	
	if (*current_turn != processedTurn || vehicle->faction_id != processedFactionId)
	{
		aiStrategy(vehicle->faction_id);
		
		processedTurn = *current_turn;
		processedFactionId = vehicle->faction_id;
		
	}
	
	// land vehicle on transport
	
	if (isVehicleLandUnitOnTransport(vehicleId))
	{
		// do not control boarded land units
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
Top level faction upkeep entry point.
*/
int aiFactionUpkeep(const int factionId)
{
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
AI strategy.
*/
void aiStrategy(int id)
{
	// no natives

	if (id == 0)
		return;

	// use WTP algorithm for selected faction only

	if (wtpAIFactionId != -1 && *active_faction != wtpAIFactionId)
		return;

	debug("aiStrategy: *active_faction=%d\n", *active_faction);

	// populate shared strategy lists

	populateSharedLists();

	// prepare former orders

	aiTerraformingStrategy();

	// prepare combat orders

	aiCombatStrategy();

}

void populateSharedLists()
{
	// clear lists

	baseIds.clear();
	baseLocations.clear();
	regionBaseGroups.clear();
	baseStrategies.clear();
	combatVehicleIds.clear();
	outsideCombatVehicleIds.clear();
	prototypes.clear();
	colonyVehicleIds.clear();
	formerVehicleIds.clear();

	// populate factions combat modifiers

	debug("%-24s\nfactionCombatModifiers:\n", MFactions[*active_faction].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// store combat modifiers

		factionInfos[id].offenseMultiplier = getFactionOffenseMultiplier(id);
		factionInfos[id].defenseMultiplier = getFactionDefenseMultiplier(id);
		factionInfos[id].fanaticBonusMultiplier = getFactionFanaticBonusMultiplier(id);

		debug
		(
			"\t%-24s: offenseMultiplier=%4.2f, defenseMultiplier=%4.2f, fanaticBonusMultiplier=%4.2f\n",
			MFactions[id].noun_faction,
			factionInfos[id].offenseMultiplier,
			factionInfos[id].defenseMultiplier,
			factionInfos[id].fanaticBonusMultiplier
		);

	}

	debug("\n");

	// populate other factions threat koefficients

	debug("%-24s\notherFactionThreatKoefficients:\n", MFactions[*active_faction].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// skip self

		if (id == *active_faction)
			continue;

		// get relation from other faction

		int otherFactionRelation = Factions[id].diplo_status[*active_faction];

		// calculate threat koefficient

		double threatKoefficient;

		if (otherFactionRelation & DIPLO_VENDETTA)
		{
			threatKoefficient = conf.ai_production_threat_coefficient_vendetta;
		}
		else if (otherFactionRelation & DIPLO_TRUCE)
		{
			threatKoefficient = conf.ai_production_threat_coefficient_truce;
		}
		else if (otherFactionRelation & DIPLO_TREATY)
		{
			threatKoefficient = conf.ai_production_threat_coefficient_treaty;
		}
		else if (otherFactionRelation & DIPLO_PACT)
		{
			threatKoefficient = conf.ai_production_threat_coefficient_pact;
		}
		else
		{
			threatKoefficient = conf.ai_production_threat_coefficient_truce;
		}

		// add extra for treacherous human player

		if (is_human(id))
		{
			threatKoefficient += 0.50;
		}

		// store threat koefficient

		factionInfos[id].threatKoefficient = threatKoefficient;

		debug("\t%-24s: %08x => %4.2f\n", MFactions[id].noun_faction, otherFactionRelation, threatKoefficient);

	}

	debug("\n");

	// populate bases

	debug("baseStrategies\n");

	for (int id = 0; id < *total_num_bases; id++)
	{
		BASE *base = &(Bases[id]);

		// exclude not own bases

		if (base->faction_id != *active_faction)
			continue;

		// add base

		baseIds.push_back(id);

		// add base location

		MAP *baseLocation = getMapTile(base->x, base->y);
		baseLocations[baseLocation] = id;

		// add base strategy

		baseStrategies[id] = {};
		baseStrategies[id].base = base;

		debug("\n[%3d] %-25s\n", id, baseStrategies[id].base->name);

		// add base regions

		std::set<int> baseConnectedRegions = getBaseConnectedRegions(id);

		for (int region : baseConnectedRegions)
		{
			if (regionBaseGroups.find(region) == regionBaseGroups.end())
			{
				regionBaseGroups[region] = std::vector<int>();
			}

			regionBaseGroups[region].push_back(id);

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

		if (vehicle->faction_id != *active_faction)
			continue;

		// combat vehicles

		if (isVehicleCombat(id))
		{
			// add vehicle

			combatVehicleIds.push_back(id);

			// find if vehicle is at base

			MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);
			std::unordered_map<MAP *, int>::iterator baseLocationsIterator = baseLocations.find(vehicleLocation);

			if (baseLocationsIterator == baseLocations.end())
			{
				// add outside vehicle

				outsideCombatVehicleIds.push_back(id);

			}
			else
			{
				BASE_STRATEGY *baseStrategy = &(baseStrategies[baseLocationsIterator->second]);

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
			colonyVehicleIds.push_back(id);
		}
		else if (isVehicleFormer(id))
		{
			formerVehicleIds.push_back(id);
		}

	}

	// populate prototypes

    for (int i = 0; i < 128; i++)
	{
        int id = (i < 64 ? i : (*active_faction - 1) * 64 + i);

        UNIT *unit = &Units[id];

		// skip not enabled

		if (id < 64 && !has_tech(*active_faction, unit->preq_tech))
			continue;

        // skip empty

        if (strlen(unit->name) == 0)
			continue;

		// add prototype

		prototypes.push_back(id);

	}

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

