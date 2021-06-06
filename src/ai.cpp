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
#include "aiProduction.h"

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
//	return mod_enemy_move(vehicleId);
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// do not try multiple iterations
	
	if (vehicle->iter_count > 0)
	{
		return mod_enemy_move(vehicleId);
	}
	
    // do not control probes
    // Thinker ignores subversion opportunities

    if (vehicle->weapon_type() == WPN_PROBE_TEAM)
	{
		return enemy_move(vehicleId);
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
	evaluateBaseExposures();
	setSharedOceanRegions();
	
	// compute production demands

	aiProductionStrategy();

	// prepare former orders

	aiTerraformingStrategy();

	// prepare combat orders

	aiCombatStrategy();

}

void populateGlobalVariables()
{
	// clear lists
	
	activeFactionInfo.clear();
	
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
		// skip aliens

		if (id == 0)
			continue;

		// skip self

		if (id == aiFactionId)
			continue;

		// get relation from other faction

		int otherFactionRelation = Factions[id].diplo_status[aiFactionId];

		// calculate threat koefficient

		double threatKoefficient;

		if (is_human(id))
		{
			threatKoefficient = conf.ai_production_threat_coefficient_vendetta;
		}
		else if (otherFactionRelation & DIPLO_VENDETTA)
		{
			threatKoefficient = conf.ai_production_threat_coefficient_vendetta;
		}
		else if (otherFactionRelation & DIPLO_PACT)
		{
			threatKoefficient = conf.ai_production_threat_coefficient_pact;
		}
		else if (otherFactionRelation & DIPLO_TREATY)
		{
			threatKoefficient = conf.ai_production_threat_coefficient_treaty;
		}
		else
		{
			threatKoefficient = conf.ai_production_threat_coefficient_other;
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
		activeFactionInfo.baseStrategies[id].conventionalDefenseMultiplier[TRIAD_LAND] = getBaseConventionalDefenseMultiplier(id, TRIAD_LAND);
		activeFactionInfo.baseStrategies[id].conventionalDefenseMultiplier[TRIAD_SEA] = getBaseConventionalDefenseMultiplier(id, TRIAD_SEA);
		activeFactionInfo.baseStrategies[id].conventionalDefenseMultiplier[TRIAD_AIR] = getBaseConventionalDefenseMultiplier(id, TRIAD_AIR);
		activeFactionInfo.baseStrategies[id].withinFriendlySensorRange = isWithinFriendlySensorRange(base->faction_id, base->x, base->y);

		debug("\n[%3d] %-25s\n", id, activeFactionInfo.baseStrategies[id].base->name);

		// add base regions
		
		int baseRegion = getBaseMapTile(id)->region;
		
		activeFactionInfo.presenceRegions.insert(getBaseMapTile(id)->region);
		activeFactionInfo.regionBaseIds[baseRegion].insert(id);

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
	
	debug("populate vehicles - %s\n", MFactions[aiFactionId].noun_faction);
	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);
		MAP *vehicleTile = getVehicleMapTile(id);

		// store all vehicle current id in pad_0 field

		vehicle->pad_0 = id;

		// further process only own vehicles

		if (vehicle->faction_id != aiFactionId)
			continue;
		
		debug("\t[%3d] (%3d,%3d) region = %3d\n", id, vehicle->x, vehicle->y, vehicleTile->region);
		
		// add vehicle
		
		activeFactionInfo.vehicleIds.push_back(id);

		// combat vehicles

		if (isVehicleCombat(id))
		{
			// add vehicle to global list

			activeFactionInfo.combatVehicleIds.push_back(id);
			
			// scout
			
			if (isScoutVehicle(id))
			{
				activeFactionInfo.scoutVehicleIds.push_back(id);
			}

			// add surface vehicle to region list
			// except land unit in ocean
			
			if (vehicle->triad() != TRIAD_AIR && !(vehicle->triad() == TRIAD_LAND && isOceanRegion(vehicleTile->region)))
			{
				if (activeFactionInfo.regionSurfaceCombatVehicleIds.count(vehicleTile->region) == 0)
				{
					activeFactionInfo.regionSurfaceCombatVehicleIds[vehicleTile->region] = std::vector<int>();
				}
				activeFactionInfo.regionSurfaceCombatVehicleIds[vehicleTile->region].push_back(id);
				
				// add scout to region list
				
				if (isScoutVehicle(id))
				{
					if (activeFactionInfo.regionSurfaceScoutVehicleIds.count(vehicleTile->region) == 0)
					{
						activeFactionInfo.regionSurfaceScoutVehicleIds[vehicleTile->region] = std::vector<int>();
					}
					activeFactionInfo.regionSurfaceScoutVehicleIds[vehicleTile->region].push_back(id);
				}
				
			}

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
	
	// bases native defense
	
	evaluateBaseNativeDefenseDemands();
	
	// max mineral surplus
	
	activeFactionInfo.maxMineralSurplus = 0;
	
	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		activeFactionInfo.maxMineralSurplus = std::max(activeFactionInfo.maxMineralSurplus, base->mineral_surplus_final);
		
		if (activeFactionInfo.regionMaxMineralSurpluses.count(baseTile->region) == 0)
		{
			activeFactionInfo.regionMaxMineralSurpluses[baseTile->region] = 0;
		}
		
		activeFactionInfo.regionMaxMineralSurpluses[baseTile->region] = std::max(activeFactionInfo.regionMaxMineralSurpluses[baseTile->region], base->mineral_surplus_final);
		
	}
	
}

/*
Finds all regions those are accessible by other factions.
Set bases accessible to shared ocean regions.
*/
void setSharedOceanRegions()
{
	// find shared ocean regions
	
	std::unordered_set<int> sharedOceanRegions;
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		
		// skip own
		
		if (base->faction_id == aiFactionId)
			continue;
		
		// get connected ocean regions
		
		std::set<int> baseConnectedOceanRegions = getBaseConnectedOceanRegions(baseId);
		
		// add regions
		
		sharedOceanRegions.insert(baseConnectedOceanRegions.begin(), baseConnectedOceanRegions.end());
		
	}
	
	// update own bases
	
	for (int baseId : activeFactionInfo.baseIds)
	{
		// clear flag by default
		
		activeFactionInfo.baseStrategies[baseId].inSharedOceanRegion = false;
		
		// get connected ocean regions
		
		std::set<int> baseConnectedOceanRegions = getBaseConnectedOceanRegions(baseId);
		
		for (int baseConnectedOceanRegion : baseConnectedOceanRegions)
		{
			if (sharedOceanRegions.count(baseConnectedOceanRegion))
			{
				activeFactionInfo.baseStrategies[baseId].inSharedOceanRegion = true;
				break;
			}
			
		}
		
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

Location getNearestPodLocation(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehilceTile = getVehicleMapTile(vehicleId);
	int triad = vehicle->triad();
	
	Location nearestPodLocation;
	int minRange = INT_MAX;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		Location location = getMapIndexLocation(mapIndex);
		MAP* tile = getMapTile(mapIndex);
		
		if
		(
			(triad == TRIAD_AIR || tile->region == vehilceTile->region)
			&&
			map_has_item(tile, TERRA_SUPPLY_POD)
		)
		{
			int range = map_range(vehicle->x, vehicle->y, location.x, location.y);
			
			if (range < minRange)
			{
				location.set(location);
				minRange = range;
			}
			
		}
		
	}
	
	return nearestPodLocation;
	
}

void evaluateBaseNativeDefenseDemands()
{
	debug("evaluateBaseNativeDefenseDemands\n");
	
	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool ocean = isOceanRegion(baseTile->region);
		
		debug("\t%s\n", base->name);
		
		// estimate native attack strength
		
		double nativePsiAttackStrength = getNativePsiAttackStrength(ocean ? TRIAD_SEA : TRIAD_LAND);
		debug("\t\tnativePsiAttackStrength = %f\n", nativePsiAttackStrength);
		
		// calculate count of the nearest unpopulated tiles
		
		int unpopulatedTiles = 28 - (nearby_items(base->x, base->y, 3, TERRA_BASE_RADIUS) - 21);
		debug("\t\tunpopulatedTiles = %d\n", unpopulatedTiles);
		
		// estimate count
		
		double anticipatedCount = 0.5 + 1.0 * ((double)unpopulatedTiles / 28.0);
		debug("\t\tanticipatedCount = %f\n", anticipatedCount);
		
		// calculate pop chance
		
		double popChance = 0.0;
		
		for (int otherBaseId : activeFactionInfo.baseIds)
		{
			BASE *otherBase =&(Bases[otherBaseId]);
			MAP *otherBaseTile = getBaseMapTile(otherBaseId);
			
			// same region only
			
			if (otherBaseTile->region != baseTile->region)
				continue;
			
			// calculate range
			
			int range = map_range(base->x, base->y, otherBase->x, otherBase->y);
			
			// only in vicinity
			
			if (range >= 10)
				continue;
			
			// compute contribution
			
			popChance += ((double)std::min(100, otherBase->eco_damage) / 100.0) * (1.0 - (double)range / 10.0);
			
		}
		
		anticipatedCount += (double)((Factions[aiFactionId].pop_total + 1) / 3) * popChance;
		
		double anticipatedNativePsiAttackStrength = std::max(0.0, nativePsiAttackStrength * anticipatedCount);

		// calculate current native protectors in base

		double baseNativeProtection = std::max(0.0, getBaseNativeProtection(baseId));
		double remainingBaseNativeProtection = anticipatedNativePsiAttackStrength - baseNativeProtection;
		
		debug("\t\tbaseNativeProtection=%f, remainingBaseNativeProtection=%f\n", baseNativeProtection, remainingBaseNativeProtection);
		
		// store demand
		
		activeFactionInfo.baseAnticipatedNativeAttackStrengths[baseId] = anticipatedNativePsiAttackStrength;
		activeFactionInfo.baseRemainingNativeProtectionDemands[baseId] = remainingBaseNativeProtection;
		
	}
	
}

int getNearestFactionBaseRange(int factionId, int x, int y)
{
	int nearestFactionBaseRange = 9999;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		// own bases

		if (base->faction_id != factionId)
			continue;

		nearestFactionBaseRange = std::min(nearestFactionBaseRange, map_range(x, y, base->x, base->y));

	}

	return nearestFactionBaseRange;

}

int getNearestBaseId(int x, int y, std::unordered_set<int> baseIds)
{
	int nearestBaseId = -1;
	int nearestBaseRange = INT_MAX;

	for (int baseId : baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		int range = map_range(x, y, base->x, base->y);
		
		if (nearestBaseId == -1 || range < nearestBaseRange)
		{
			nearestBaseId = baseId;
			nearestBaseRange = range;
		}

	}

	return nearestBaseId;

}

int getNearestBaseRange(int x, int y, std::unordered_set<int> baseIds)
{
	int nearestBaseRange = INT_MAX;

	for (int baseId : baseIds)
	{
		BASE *base = &(Bases[baseId]);

		nearestBaseRange = std::min(nearestBaseRange, map_range(x, y, base->x, base->y));

	}

	return nearestBaseRange;

}

void evaluateBaseExposures()
{
	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool ocean = isOceanRegion(baseTile->region);
		
		// find nearest not own tile
		
		int nearestNotOwnTileRange = INT_MAX;

		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			Location location = getMapIndexLocation(mapIndex);
			MAP* tile = getMapTile(mapIndex);
			
			// exclude own tiles
			
			if (tile->owner == aiFactionId)
				continue;
			
			// calculate range
			
			int range = map_range(base->x, base->y, location.x, location.y);
			
			nearestNotOwnTileRange = std::min(nearestNotOwnTileRange, range);
			
		}
		
		// set base distance based on realm
		
		double baseDistance = (ocean ? 7.0 : 3.0);
		
		// estimate exposure
		
		double exposure = 1.0 / std::max(1.0, (double)nearestNotOwnTileRange / baseDistance);
		
		activeFactionInfo.baseStrategies[baseId].exposure = exposure;
		
	}
	
}

