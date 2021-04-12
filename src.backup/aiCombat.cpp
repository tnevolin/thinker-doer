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

//	aiNativeCombatStrategy();

}

void populateCombatLists()
{
	combatOrders.clear();

}

/*
Composes anti-native combat strategy.
*/
void aiNativeCombatStrategy()
{
	debug("aiNativeCombatStrategy %s\n", MFactions[aiFactionId].noun_faction);

	// protect bases

	debug("\tbase native protection\n");

	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseLocation = getBaseMapTile(baseId);
		bool ocean = isOceanRegion(baseLocation->region);
		debug("\t%s\n", base->name);

		// compose list of available vehicles

		std::vector<int> availableVehicles;

		std::vector<int> baseGarrison = getBaseGarrison(baseId);
		availableVehicles.insert(availableVehicles.end(), baseGarrison.begin(), baseGarrison.end());

		if (!ocean)
		{
			std::vector<int> regionSurfaceVehicles = getRegionSurfaceVehicles(base->faction_id, baseLocation->region, false);
			availableVehicles.insert(availableVehicles.end(), regionSurfaceVehicles.begin(), regionSurfaceVehicles.end());
		}

		// compose list of protectors

		std::vector<VEHICLE_VALUE> protectors;

		for (int vehicleId : availableVehicles)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);

			// not having orders already

			if (combatOrders.find(vehicleId) != combatOrders.end())
				continue;

			// combat vehicles only

			if (!isVehicleCombat(vehicleId))
				continue;

			// infantry vehicles only

			if (Units[vehicle->unit_id].chassis_type != CHS_INFANTRY)
				continue;

			// exclude battle ogres

			if (vehicle->unit_id == BSC_BATTLE_OGRE_MK1 || vehicle->unit_id == BSC_BATTLE_OGRE_MK2 || vehicle->unit_id == BSC_BATTLE_OGRE_MK3)
				continue;

			// get protection potential

			double protectionPotential = getVehicleBaseNativeProtectionPotential(vehicleId);
			debug("\t\t[%3d](%3d,%3d) protectionPotential=%f\n", vehicleId, vehicle->x, vehicle->y, protectionPotential);

			// build vehicle value

			double value = getVehicleBaseNativeProtectionEfficiency(vehicleId);

			// pull police into bases

			if (vehicle_has_ability(vehicleId, ABL_POLICE_2X))
			{
				value *= 4.0;
			}

			// reduce value with range

			value /= std::max(1.0, (double)map_range(vehicle->x, vehicle->y, base->x, base->y));

			// add vehicle

			protectors.push_back({vehicleId, value, protectionPotential});

		}

		// sort protectors

		std::sort(protectors.begin(), protectors.end(), compareVehicleValue);

		// assign protectors

		double remainingProtection = (base->pop_size == 1 ? 2.5 : 2.0);

		for (VEHICLE_VALUE vehicleValue : protectors)
		{
			int vehicleId = vehicleValue.id;
			double damage = vehicleValue.damage;

			if (remainingProtection > 0.0)
			{
				setDefendOrder(vehicleId, base->x, base->y);
				remainingProtection -= damage;
			}
		}

	}

	// check native artillery and towers

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);
		
		// native unit
		if (vehicle->faction_id == 0)
		{
			// spore launcher
			if (vehicle->unit_id == BSC_SPORE_LAUNCHER)
			{
				attackNativeArtillery(id);
			}
			// spore launcher
			else if (vehicle->unit_id == BSC_FUNGAL_TOWER)
			{
				attackNativeTower(id);
			}
			
		}

	}

	debug("\n");

}

/*
Checks whether native artillery is dangerous and attack it if so.
*/
void attackNativeArtillery(int enemyVehicleId)
{
	VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);

	// check if they are our improvements or non combat units in bombardment range

	bool danger = false;

	for (int dx = -4; dx <= +4; dx++)
	{
		for (int dy = -(4 - abs(dx)); dy <= +(4 - abs(dx)); dy += 2)
		{
			int x = enemyVehicle->x + dx;
			int y = enemyVehicle->y + dy;
			MAP *tile = getMapTile(x, y);

			if (!tile)
				continue;

			if
			(
				tile->owner == aiFactionId
				&&
				(map_base(tile) || isImprovedTile(x, y))
			)
			{
				danger = true;
				break;
			}
			
			bool ourNonCombatVehicleIsInDanger = false;
			for (int vehicleId : getStackedVehicleIds(veh_at(x, y)))
			{
				VEH *vehicle = &(Vehicles[vehicleId]);
				
				if
				(
					vehicle->faction_id == aiFactionId
					&&
					!isVehicleCombat(vehicleId)
				)
				{
					ourNonCombatVehicleIsInDanger = true;
					break;
				}
				
			}
			
			if (ourNonCombatVehicleIsInDanger)
			{
				danger = true;
				break;
			}
			
		}
	}

	if (danger)
	{
		attackVehicle(enemyVehicleId);
	}
	
}

/*
Checks whether native tower is within our borders and attack it if so.
*/
void attackNativeTower(int enemyVehicleId)
{
	MAP *enemyVehicleTile = getVehicleMapTile(enemyVehicleId);
	
	if (enemyVehicleTile->owner == aiFactionId)
	{
		attackVehicle(enemyVehicleId);
	}
	
}

void attackVehicle(int enemyVehicleId)
{
	VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);
	
	// assemble attacking forces

	debug("attackVehicle (%3d,%3d)\n", enemyVehicle->x, enemyVehicle->y);

	// list available units

	std::vector<VEHICLE_VALUE> vehicleDamageValues;

	for (int id : activeFactionInfo.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[id]);

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
	if (combatOrder->defendX != -1 && combatOrder->defendY != -1)
	{
		return applyDefendOrder(id, combatOrder->defendX, combatOrder->defendY);
	}
	if (combatOrder->enemyAIId != -1)
	{
		return applyAttackOrder(id, combatOrder);
	}
	else
	{
		return enemy_move(id);
	}

}

int applyDefendOrder(int id, int x, int y)
{
	VEH *vehicle = &(Vehicles[id]);

	// at destination

	if (vehicle->x == x && vehicle->y == y)
	{
		// hold position

		setVehicleOrder(id, ORDER_HOLD);

	}

	// not at destination

	else
	{
		setMoveTo(id, x, y);
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

void setDefendOrder(int vehicleId, int x, int y)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	debug("setDefendOrder: [%3d](%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, x, y);

	if (combatOrders.find(vehicleId) == combatOrders.end())
	{
		combatOrders[vehicleId] = {};
		combatOrders[vehicleId].defendX = x;
		combatOrders[vehicleId].defendY = y;
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
Returns vehicle own morale modifier regardless of SE settings.
*/
double getVehicleMoraleModifier(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// get vehicle morale
	
	int vehicleMorale = std::max(0, std::min(6, (int)vehicle->morale));

	// calculate modifier

	return 1.0 + 0.125 * (double)(vehicleMorale - 2);

}

/*
Returns vehicle own conventional offense value counting weapon, morale, abilities.
*/
double getVehicleConventionalOffenseValue(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);
	
	// get offense value
	
	double offenseValue = Weapon[unit->weapon_type].offense_value;
	
	// psi weapon is evaluated as psi
	
	if (offenseValue < 0)
	{
		return getVehiclePsiOffenseValue(vehicleId);
	}
	
	// times morale modifier
	
	offenseValue *= getVehicleMoraleModifier(vehicleId);
	
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
	
	defenseValue *= getVehicleMoraleModifier(vehicleId);
	
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
	
	double offenseValue = getVehicleMoraleModifier(vehicleId);
	
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
	
	double defenseValue = getVehicleMoraleModifier(vehicleId);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_TRANCE))
	{
		defenseValue *= 1.0 + ((double)Rules->combat_bonus_trance_vs_psi / 100.0);
	}
	
	return defenseValue;
	
}

/*
Computes estimated threat level as ratio of potential thread to defense.
*/
double getThreatLevel()
{
	debug("\ngetThreatLevel - %s\n", MFactions[aiFactionId].noun_faction);
	
	// compute faction strength
	
	double factionStrength = 0.0;
	
	for (int vehicleId : activeFactionInfo.combatVehicleIds)
	{
		// get vehicle combat values
		
		double offenseValue = (getVehicleConventionalOffenseValue(vehicleId) + getVehiclePsiOffenseValue(vehicleId)) / 2.0;
		double defenseValue = (getVehicleConventionalDefenseValue(vehicleId) + getVehiclePsiDefenseValue(vehicleId)) / 2.0;
		
		// store
		
		factionStrength += (offenseValue + defenseValue);
		
	}
	
	debug("\tfactionStrength = %f\n", factionStrength);
	
	// zero strength - return some arbitrary big value
	
	if (factionStrength == 0)
		return 10.0;
	
	// compute enemy faction strengths
	
	std::unordered_map<int, double> enemyFactionStrengths;
	
	for (int factionId = 1; factionId < 8; factionId++)
	{
		if (factionId == aiFactionId)
			continue;
		
		enemyFactionStrengths[factionId] = 0.0;
		
	}
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// enemy factions only
		
		if (vehicle->faction_id == 0 || vehicle->faction_id == aiFactionId)
			continue;
		
		// combat only
		
		if (!isVehicleCombat(vehicleId))
			continue;
		
		// get vehicle combat values
		
		double offenseValue = (getVehicleConventionalOffenseValue(vehicleId) + getVehiclePsiOffenseValue(vehicleId)) / 2.0;
		double defenseValue = (getVehicleConventionalDefenseValue(vehicleId) + getVehiclePsiDefenseValue(vehicleId)) / 2.0;
		
		// calculate range to nearest faction base
		
		int rangeToNearestActiveFactionBase = getRangeToNearestActiveFactionBase(vehicle->x, vehicle->y);
		
		// compute range multiplier
		// every 20 tiles threat halves
		
		double rangeMultiplier = pow(0.5, (double)rangeToNearestActiveFactionBase / 20.0);
		
		// get threat coefficient
		
		double threatCoefficient;
		
		if (isDiplomaticStatus(aiFactionId, vehicle->faction_id, DIPLO_VENDETTA))
		{
			threatCoefficient = conf.ai_production_threat_coefficient_vendetta;
		}
		else if (isDiplomaticStatus(aiFactionId, vehicle->faction_id, DIPLO_PACT))
		{
			threatCoefficient = conf.ai_production_threat_coefficient_pact;
		}
		else if (isDiplomaticStatus(aiFactionId, vehicle->faction_id, DIPLO_TREATY))
		{
			threatCoefficient = conf.ai_production_threat_coefficient_treaty;
		}
		else
		{
			threatCoefficient = conf.ai_production_threat_coefficient_other;
		}
		
		// store value
		
		enemyFactionStrengths[vehicle->faction_id] += threatCoefficient * (offenseValue + defenseValue) * rangeMultiplier;
		
	}
	
	// select strongest enemy faction
	
	double maxEnemyFactionStrength = 0.0;
	
	for (auto const& kv : enemyFactionStrengths)
	{
		maxEnemyFactionStrength = std::max(maxEnemyFactionStrength, kv.second);
	}
	
	debug("\tmaxEnemyFactionStrength = %f\n", maxEnemyFactionStrength);
	
	// compose threat level
	
	double threatLevel = maxEnemyFactionStrength / factionStrength;

	debug("\tthreatLevel = %f\n\n", maxEnemyFactionStrength / factionStrength);
	
	return threatLevel;

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

