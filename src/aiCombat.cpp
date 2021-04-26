#include <unordered_map>
#include "engine.h"
#include "terranx_wtp.h"
#include "ai.h"
#include "aiCombat.h"

std::unordered_map<int, COMBAT_ORDER> combatOrders;

/*
Prepares combat orders.
*/
void aiCombatStrategy()
{
	populateCombatLists();

	nativeCombatStrategy();
	
	factionCombatStrategy();
	
	popPods();

}

void populateCombatLists()
{
	combatOrders.clear();

}

/*
Composes anti-native combat strategy.
*/
void nativeCombatStrategy()
{
	debug("nativeCombatStrategy %s\n", MFactions[aiFactionId].noun_faction);

	// protect bases

	debug("\tbase native protection\n");

	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseLocation = getBaseMapTile(baseId);
		bool ocean = isOceanRegion(baseLocation->region);
		debug("\t%s\n", base->name);

		// get base native protection demand
		
		if (activeFactionInfo.baseAnticipatedNativeAttackStrengths.count(baseId) == 0)
			continue;
		
		double baseAnticipatedNativeAttackStrength = activeFactionInfo.baseAnticipatedNativeAttackStrengths[baseId];
		
		debug("\tbaseAnticipatedNativeAttackStrength=%f\n", baseAnticipatedNativeAttackStrength);
		
		if (baseAnticipatedNativeAttackStrength <= 0.0)
			continue;

		// collect available vehicles
		
		std::vector<int> availableVehicles;
		
		std::vector<int> baseGarrison = getBaseGarrison(baseId);
		
		for (int vehicleId : baseGarrison)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			if
			(
				// no orders
				combatOrders.count(vehicleId) == 0
				&&
				// combat
				isVehicleCombat(vehicleId)
				&&
				// infantry
				Units[vehicle->unit_id].chassis_type == CHS_INFANTRY
				&&
				// defender
				vehicle->weapon_type() == WPN_HAND_WEAPONS
				&&
				// trance
				vehicle_has_ability(vehicleId, ABL_TRANCE)
			)
			{
				availableVehicles.push_back(vehicleId);
			}
			
		}

		for (int vehicleId : baseGarrison)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			if
			(
				// no orders
				combatOrders.count(vehicleId) == 0
				&&
				// combat
				isVehicleCombat(vehicleId)
				&&
				// infantry
				Units[vehicle->unit_id].chassis_type == CHS_INFANTRY
				&&
				// defender
				vehicle->weapon_type() == WPN_HAND_WEAPONS
				&&
				// not trance
				!vehicle_has_ability(vehicleId, ABL_TRANCE)
			)
			{
				availableVehicles.push_back(vehicleId);
			}
			
		}
		
		// for land bases also add vehicles in vicinity
		
		if (!ocean && activeFactionInfo.regionSurfaceScoutVehicleIds.count(baseLocation->region) > 0)
		{
			for (int vehicleId : activeFactionInfo.regionSurfaceScoutVehicleIds[baseLocation->region])
			{
				VEH *vehicle = &(Vehicles[vehicleId]);
				MAP *vehicleTile = getVehicleMapTile(vehicleId);
				
				if
				(
					// no orders
					combatOrders.count(vehicleId) == 0
					&&
					// combat
					isVehicleCombat(vehicleId)
					&&
					// scout
					isScoutVehicle(vehicleId)
					&&
					// not in base
					!map_has_item(vehicleTile, TERRA_BASE_IN_TILE)
					&&
					// close
					map_range(base->x, base->y, vehicle->x, vehicle->y) < 10
				)
				{
					availableVehicles.push_back(vehicleId);
				}
				
			}
			
		}

		// set remaining protection

		double remainingProtection = baseAnticipatedNativeAttackStrength;
		
		// assign protectors

		for (int vehicleId : availableVehicles)
		{
			if (remainingProtection <= 0.0)
				break;
			
			VEH *vehicle = &(Vehicles[vehicleId]);

			// get protection potential

			double protectionPotential = getVehicleBaseNativeProtection(baseId, vehicleId);
			debug("\t\t[%3d](%3d,%3d) protectionPotential=%f\n", vehicleId, vehicle->x, vehicle->y, protectionPotential);

			// add vehicle

			setMoveOrder(vehicleId, base->x, base->y, true);
			remainingProtection -= protectionPotential;

		}

	}

	// spore launchers

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);
		MAP *vehicleTile = getVehicleMapTile(id);
		
		if (vehicle->faction_id == 0 && vehicle->unit_id == BSC_SPORE_LAUNCHER && vehicleTile->owner == aiFactionId)
		{
			attackNative(id);
		}

	}

	// mind worms

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);
		MAP *vehicleTile = getVehicleMapTile(id);
		
		if (vehicle->faction_id == 0 && vehicle->unit_id == BSC_MIND_WORMS && vehicleTile->owner == aiFactionId)
		{
			attackNative(id);
		}

	}

	// fungal towers

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);
		MAP *vehicleTile = getVehicleMapTile(id);
		
		if (vehicle->faction_id == 0 && vehicle->unit_id == BSC_FUNGAL_TOWER && vehicleTile->owner == aiFactionId)
		{
			attackNative(id);
		}

	}

	debug("\n");

}

void factionCombatStrategy()
{
	
}

void popPods()
{
	// collect pods in regions
	
	std::unordered_map<int, std::vector<Location>> regionPodLocations;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		Location location = getMapIndexLocation(mapIndex);
		MAP *tile = getMapTile(mapIndex);
		
		if (goody_at(location.x, location.y) != 0)
		{
			// exclude those targetted by own vehicle
			
			if (isFactionTargettedLocation(location, aiFactionId))
				continue;
			
			if (regionPodLocations.count(tile->region) == 0)
			{
				regionPodLocations[tile->region] = std::vector<Location>();
			}
			regionPodLocations[tile->region].push_back(location);
		}
		
	}
	
	if (DEBUG)
	{
		debug("\n");
		debug("regionPodLocations - %s\n", MFactions[aiFactionId].noun_faction);
		for (const auto &kv : regionPodLocations)
		{
			debug("\tregion = %3d\n", kv.first);
			for (Location location : kv.second)
			{
				debug("\t\t(%3d,%3d)\n", location.x, location.y);
			}
		}
		debug("\n");
	}
	
	// match scouts with pods
	
	for (auto const& x : activeFactionInfo.regionSurfaceScoutVehicleIds)
	{
		int region = x.first;
		std::vector<int> vehicleIds = x.second;
		debug("region = %3d\n", region);
		
		// no pods in region
		
		if (regionPodLocations.count(region) == 0)
			continue;
		
		// get pod locations
		
		std::vector<Location> podLocations = regionPodLocations[region];
		
		// iterate scouts
		
		for (int vehicleId : vehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			// scouts only
			
			if (!isScoutVehicle(vehicleId))
				continue;
			
			// without combat orders
			
			if (combatOrders.count(vehicleId) > 0)
				continue;
			
			debug("\t[%3d] (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y);
			
			// find nearest pod
			
			std::vector<Location>::iterator nearestPodLocationIterator = podLocations.end();
			int minRange = INT_MAX;
			
			for (std::vector<Location>::iterator podLocationsIterator = podLocations.begin(); podLocationsIterator != podLocations.end(); podLocationsIterator++)
			{
				Location location = *podLocationsIterator;
				
				int range = map_range(vehicle->x, vehicle->y, location.x, location.y);
				
				if (range < minRange)
				{
					nearestPodLocationIterator = podLocationsIterator;
					minRange = range;
				}
				
			}
			
			if (nearestPodLocationIterator != podLocations.end())
			{
				Location nearestPodLocation = *nearestPodLocationIterator;
				setMoveOrder(vehicleId, nearestPodLocation.x, nearestPodLocation.y, false);
				podLocations.erase(nearestPodLocationIterator);
			}
			
		}
		
	}
	
}

void attackNative(int enemyVehicleId)
{
	VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);
	
	// assemble attacking forces

	debug("attackNative (%3d,%3d)\n", enemyVehicle->x, enemyVehicle->y);

	// list available units

	std::vector<VEHICLE_VALUE> vehicleDamageValues;

	for (int id : activeFactionInfo.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[id]);
		
		// skip not scouts
		
		if (!isScoutVehicle(id))
			continue;

		// skip those with orders

		if (combatOrders.count(id) != 0)
			continue;

		// do not bother if unreachable

		if (!isreachable(id, enemyVehicle->x, enemyVehicle->y))
			continue;

		// calculate damage

		double damage = (*current_turn < 15 ? 2.0 : 1.0) * calculatePsiDamageAttack(id, enemyVehicleId);

		// calculate value

		int distance = map_range(vehicle->x, vehicle->y, enemyVehicle->x, enemyVehicle->y);
		int cost = Units[vehicle->unit_id].cost;
		double value = damage / (double)cost / (double)distance;

		// store vehicle

		vehicleDamageValues.push_back({id, damage, value});

	}

	// sort vehicles

	std::sort(vehicleDamageValues.begin(), vehicleDamageValues.end(), compareVehicleValue);

	// select attackers

	double enemyVehicleHealth = (double)(10 - enemyVehicle->damage_taken);
	double requiredDamage = 2.0 * enemyVehicleHealth;
	double combinedDamage = 0.0;

	debug
	(
		"enemyVehicleHealth=%4.1f, requiredDamage=%4.1f\n",
		enemyVehicleHealth,
		requiredDamage
	);

	for (VEHICLE_VALUE vehicleDamageValue : vehicleDamageValues)
	{
		// store order

		combatOrders[vehicleDamageValue.id] = {};
		combatOrders[vehicleDamageValue.id].enemyAIId = enemyVehicleId;

		// deliver damage

		combinedDamage += vehicleDamageValue.damage;

		debug
		(
			"\t[%3d] value=%4.1f, damage=%4.1f, combinedDamage=%4.1f\n",
			vehicleDamageValue.id,
			vehicleDamageValue.value,
			vehicleDamageValue.damage,
			combinedDamage
		);

		// enough!

		if (combinedDamage >= requiredDamage)
			break;

	}

	debug("\n");

}

int compareVehicleValue(VEHICLE_VALUE o1, VEHICLE_VALUE o2)
{
	return (o1.value > o2.value);
}

int moveCombat(int vehicleId)
{
	// get vehicle

	VEH *vehicle = &(Vehicles[vehicleId]);

	// restore ai vehicleId

	int aiVehicleId = vehicle->pad_0;

	debug("[%d->%d] (%3d,%3d)\n", aiVehicleId, vehicleId, vehicle->x, vehicle->y);

	// process vehicle order if any assigned

	if (combatOrders.count(aiVehicleId) != 0)
	{
		// get order

		COMBAT_ORDER *combatOrder = &(combatOrders[aiVehicleId]);

		// apply order

		return applyCombatOrder(vehicleId, combatOrder);

	}

	// process sea explorers

    if (vehicle->triad() == TRIAD_SEA && Units[vehicle->unit_id].unit_plan == PLAN_RECONNAISANCE && vehicle->move_status == ORDER_NONE)
	{
		return processSeaExplorer(vehicleId);
	}

	// default to Thinker

	return mod_enemy_move(vehicleId);

}

int applyCombatOrder(int id, COMBAT_ORDER *combatOrder)
{
	if (combatOrder->x != -1 && combatOrder->y != -1)
	{
		return applyMoveOrder(id, combatOrder);
	}
	if (combatOrder->enemyAIId != -1)
	{
		return applyAttackOrder(id, combatOrder);
	}
	else
	{
		return mod_enemy_move(id);
	}

}

int applyMoveOrder(int id, COMBAT_ORDER *combatOrder)
{
	VEH *vehicle = &(Vehicles[id]);

	// at destination

	if (vehicle->x == combatOrder->x && vehicle->y == combatOrder->y)
	{
		// hold if instructed
		
		if (combatOrder->hold)
		{
			setVehicleOrder(id, ORDER_HOLD);
		}

	}

	// not at destination

	else
	{
		setMoveTo(id, combatOrder->x, combatOrder->y);
	}

	return SYNC;

}

int applyAttackOrder(int id, COMBAT_ORDER *combatOrder)
{
	// find enemy vehicle by AI ID

	VEH *enemyVehicle = getVehicleByAIId(combatOrder->enemyAIId);

	// enemy not found

	if (enemyVehicle == NULL || enemyVehicle->x == -1 || enemyVehicle->y == -1)
		return enemy_move(id);

	// enemy is unreachable

	if (!isreachable(id, enemyVehicle->x, enemyVehicle->y))
		return enemy_move(id);

	// set move to order

	return setMoveTo(id, enemyVehicle->x, enemyVehicle->y);

}

void setMoveOrder(int vehicleId, int x, int y, bool hold)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	debug("setMoveOrder: [%3d](%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, x, y);

	if (combatOrders.find(vehicleId) == combatOrders.end())
	{
		combatOrders[vehicleId] = {};
		combatOrders[vehicleId].x = x;
		combatOrders[vehicleId].y = y;
		combatOrders[vehicleId].hold = hold;
	}

}

/*
Overrides sea explorer routine.
*/
int processSeaExplorer(int vehicleId)
{
	VEH* vehicle = &(Vehicles[vehicleId]);

	// keep healing if can

	if (isVehicleCanHealAtThisLocation(vehicleId))
	{
		return enemy_move(vehicleId);
	}

	// find the nearest unexplored connected ocean region tile

	MAP_INFO nearestUnexploredTile = getNearestUnexploredConnectedOceanRegionTile(vehicle->faction_id, vehicle->x, vehicle->y);

	if (nearestUnexploredTile.tile == NULL)
	{
		// nothing more to explore

		// kill unit

		debug("vehicle killed: [%3d] (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y);
		return killVehicle(vehicleId);

	}
	else
	{
		// move vehicle to unexplored destination

		debug("vehicle moved: [%3d] (%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, nearestUnexploredTile.x, nearestUnexploredTile.y);
		return moveVehicle(vehicleId, nearestUnexploredTile.x, nearestUnexploredTile.y);

	}

}

/*
Checks if sea explorer is in land port.
*/
bool isHealthySeaExplorerInLandPort(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleLocation = getVehicleMapTile(vehicleId);
	int triad = vehicle->triad();
	bool ocean = is_ocean(vehicleLocation);

	// only sea units

	if (triad != TRIAD_SEA)
		return false;

	// only explorers

	if (~vehicle->state & VSTATE_EXPLORE)
		return false;

	// only in base

	if (!map_has_item(vehicleLocation, TERRA_BASE_IN_TILE))
		return false;

	// only on land

	if (ocean)
		return false;

	// only healthy

	if (vehicle->damage_taken > 0)
		return false;

	// check if this tile has access to ocean

	MAP_INFO adjacentOceanTileInfo = getAdjacentOceanTileInfo(vehicle->x, vehicle->y);

	if (adjacentOceanTileInfo.tile == NULL)
		return false;

	// all conditions met

	return true;

}

/*
Fixes vanilla bug when sea explorer is stuck in land port.
*/
int kickSeaExplorerFromLandPort(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// check if this tile has access to ocean

	MAP_INFO adjacentOceanTileInfo = getAdjacentOceanTileInfo(vehicle->x, vehicle->y);

	if (adjacentOceanTileInfo.tile == NULL)
		return SYNC;

	debug("kickSeaExplorerFromLandPort: [%3d] (%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, adjacentOceanTileInfo.x, adjacentOceanTileInfo.y);

	// send unit to this tile

//	vehicle->state &= ~VSTATE_EXPLORE;
	setMoveTo(vehicleId, adjacentOceanTileInfo.x, adjacentOceanTileInfo.y);
	tx_action(vehicleId);

	return enemy_move(vehicleId);

}

/*
Kills vehicle and returns proper value as enemy_move would.
*/
int killVehicle(int vehicleId)
{
	tx_kill(vehicleId);
	return NO_SYNC;

}

/*
Sets vehicle move to order AND moves it there exhausting moves along the way.
This is used in enemy_move routines to avoid vanilla code overriding move_to order.
*/
int moveVehicle(int vehicleId, int x, int y)
{
	setMoveTo(vehicleId, x, y);
	tx_action(vehicleId);
	return SYNC;

}

MAP_INFO getNearestUnexploredConnectedOceanRegionTile(int factionId, int initialLocationX, int initialLocationY)
{
	debug("getNearestUnexploredConnectedOceanRegionTile: factionId=%d, initialLocationX=%d, initialLocationY=%d\n", factionId, initialLocationX, initialLocationY);

	MAP_INFO mapInfo;

	// get ocean connected regions

	std::unordered_set<int> connectedOceanRegions = getConnectedOceanRegions(factionId, initialLocationX, initialLocationY);

	debug("\tconnectedOceanRegions\n");

	if (DEBUG)
	{
		for (int connectedOceanRegion : connectedOceanRegions)
		{
			debug("\t\t%d\n", connectedOceanRegion);

		}

	}

	// search map

	int nearestUnexploredConnectedOceanRegionTileRange = 9999;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int x = getXCoordinateByMapIndex(mapIndex);
		int y = getYCoordinateByMapIndex(mapIndex);
		MAP *tile = getMapTile(x, y);
		int region = tile->region;

		// only ocean regions

		if (!isOceanRegion(region))
			continue;

		// only connected regions

		if (!connectedOceanRegions.count(region))
			continue;

		// only undiscovered tiles

		if (isMapTileVisibleToFaction(tile, factionId))
			continue;

		// calculate range

		int range = map_range(initialLocationX, initialLocationY, x, y);

		// update map info

		if (mapInfo.tile == NULL || range < nearestUnexploredConnectedOceanRegionTileRange)
		{
			mapInfo.x = x;
			mapInfo.y = y;
			mapInfo.tile = tile;
			nearestUnexploredConnectedOceanRegionTileRange = range;
		}

	}

	return mapInfo;

}

/*
Returns vehicle own conventional offense value counting weapon, morale, abilities.
*/
double getVehicleConventionalOffenseValue(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);
	
	// get offense value
	
	double offenseValue = (double)Weapon[unit->weapon_type].offense_value;
	
	// psi weapon is evaluated as psi
	
	if (offenseValue < 0)
	{
		return getVehiclePsiOffenseValue(vehicleId);
	}
	
	// times morale modifier
	
	offenseValue *= getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_DISSOCIATIVE_WAVE))
	{
		offenseValue *= 1.0 + 0.5 * 0.25;
	}
	
	if (isVehicleHasAbility(vehicleId, ABL_NERVE_GAS))
	{
		offenseValue *= 1.5;
	}
	
	if (isVehicleHasAbility(vehicleId, ABL_SOPORIFIC_GAS))
	{
		offenseValue *= 1.25;
	}
	
	return offenseValue;
	
}

/*
Returns vehicle own conventional defense value counting armor, morale, abilities.
*/
double getVehicleConventionalDefenseValue(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);
	
	// get defense value
	
	double defenseValue = Armor[unit->armor_type].defense_value;
	
	// psi armor is evaluated as psi
	
	if (defenseValue < 0)
	{
		return getVehiclePsiDefenseValue(vehicleId);
	}
	
	// times morale modifier
	
	defenseValue *= getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_AAA))
	{
		defenseValue *= 1.0 + 0.5 * ((double)Rules->combat_AAA_bonus_vs_air / 100.0);
	}
	
	if (isVehicleHasAbility(vehicleId, ABL_COMM_JAMMER))
	{
		defenseValue *= 1.0 + 0.5 * ((double)Rules->combat_comm_jammer_vs_mobile / 100.0);
	}
	
	return defenseValue;
	
}

/*
Returns vehicle own psi offense value counting morale, abilities.
*/
double getVehiclePsiOffenseValue(int vehicleId)
{
	// get offense value
	
	double offenseValue = getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_EMPATH))
	{
		offenseValue *= 1.0 + ((double)Rules->combat_bonus_empath_song_vs_psi / 100.0);
	}
	
	return offenseValue;
	
}

/*
Returns vehicle own Psi defense value counting morale, abilities.
*/
double getVehiclePsiDefenseValue(int vehicleId)
{
	// get defense value
	
	double defenseValue = getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_TRANCE))
	{
		defenseValue *= 1.0 + ((double)Rules->combat_bonus_trance_vs_psi / 100.0);
	}
	
	return defenseValue;
	
}

int getRangeToNearestActiveFactionBase(int x, int y)
{
	int minRange = INT_MAX;
	
	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		minRange = std::min(minRange, map_range(x, y, base->x, base->y));
		
	}
	
	return minRange;
	
}

