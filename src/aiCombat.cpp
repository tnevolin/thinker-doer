#include "aiCombat.h"
#include "ai.h"
#include "engine.h"

std::unordered_map<int, COMBAT_ORDER> combatOrders;

/*
Prepares combat orders.
*/
void aiCombatStrategy()
{
	populateCombatLists();

	aiNativeCombatStrategy();

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
	debug("aiNativeCombatStrategy %s\n", tx_metafactions[*active_faction].noun_faction);

	// protect bases

	debug("\tbase native protection\n");

	for (int baseId : baseIds)
	{
		BASE *base = &(tx_bases[baseId]);
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
			VEH *vehicle = &(tx_vehicles[vehicleId]);

			// not having orders already

			if (combatOrders.find(vehicleId) != combatOrders.end())
				continue;

			// combat vehicles only

			if (!isCombatVehicle(vehicleId))
				continue;

			// infantry vehicles only

			if (tx_units[vehicle->proto_id].chassis_type != CHS_INFANTRY)
				continue;

			// exclude battle ogres

			if (vehicle->proto_id == BSC_BATTLE_OGRE_MK1 || vehicle->proto_id == BSC_BATTLE_OGRE_MK2 || vehicle->proto_id == BSC_BATTLE_OGRE_MK3)
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

			value /= max(1.0, (double)map_range(vehicle->x, vehicle->y, base->x, base->y));

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

	// check native artillery

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(tx_vehicles[id]);

		// native units only

		if (vehicle->faction_id != 0)
			continue;

		// spore launchers

		if (vehicle->proto_id == BSC_SPORE_LAUNCHER)
		{
			attackNativeArtillery(id);
		}

	}

	debug("\n");

}

void attackNativeArtillery(int enemyVehicleId)
{
	VEH *enemyVehicle = &(tx_vehicles[enemyVehicleId]);

	// check if there are our improvements in bombardment range

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

			if (tile->owner != aiFactionId)
				continue;

			if (map_base(tile) || isImprovedTile(x, y))
			{
				danger = true;
				break;
			}

		}
	}

	if (!danger)
		return;

	// assemble attacking forces

	debug("attackNativeArtillery (%3d,%3d)\n", enemyVehicle->x, enemyVehicle->y);

	// list available units

	std::vector<VEHICLE_VALUE> vehicleDamageValues;

	for (int id : combatVehicleIds)
	{
		VEH *vehicle = &(tx_vehicles[id]);

		// do not bother if unreachable

		if (!isreachable(id, enemyVehicle->x, enemyVehicle->y))
			continue;

		// calculate damage

		double damage = (*current_turn < 15 ? 2.0 : 1.0) * calculatePsiDamageAttack(id, enemyVehicleId);

		// calculate value

		int distance = map_range(vehicle->x, vehicle->y, enemyVehicle->x, enemyVehicle->y);
		int cost = tx_units[vehicle->proto_id].cost;
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

int enemyMoveCombat(int id)
{
	// use WTP algorithm for selected faction only

	if (wtpAIFactionId != -1 && aiFactionId != wtpAIFactionId)
		return SYNC;

	// get vehicle

	VEH *vehicle = &(tx_vehicles[id]);

	// restore ai id

	int aiVehicleId = vehicle->pad_0;

	debug("[%d->%d] (%3d,%3d)\n", aiVehicleId, id, vehicle->x, vehicle->y);

	// find vehicle order

	std::unordered_map<int, COMBAT_ORDER>::iterator combatOrdersIterator = combatOrders.find(aiVehicleId);

	// skip vehicles without orders

	if (combatOrdersIterator == combatOrders.end())
		return tx_enemy_move(id);

	// get order

	COMBAT_ORDER *combatOrder = &(combatOrdersIterator->second);

	// apply order

	return applyCombatOrder(id, combatOrder);

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
		return tx_enemy_move(id);
	}

}

int applyDefendOrder(int id, int x, int y)
{
	VEH *vehicle = &(tx_vehicles[id]);

	// at destination

	if (vehicle->x == x && vehicle->y == y)
	{
		// hold position

		setVehicleOrder(id, ORDER_HOLD);

	}

	// not at destination

	else
	{
		set_move_to(id, x, y);
	}

	return SYNC;

}

int applyAttackOrder(int id, COMBAT_ORDER *combatOrder)
{
	// find enemy vehicle by AI ID

	VEH *enemyVehicle = getVehicleByAIId(combatOrder->enemyAIId);

	// enemy not found

	if (enemyVehicle == NULL || enemyVehicle->x == -1 || enemyVehicle->y == -1)
		return tx_enemy_move(id);

	// enemy is unreachable

	if (!isreachable(id, enemyVehicle->x, enemyVehicle->y))
		return tx_enemy_move(id);

	// set move to order

	return set_move_to(id, enemyVehicle->x, enemyVehicle->y);

}

void setDefendOrder(int vehicleId, int x, int y)
{
	VEH *vehicle = &(tx_vehicles[vehicleId]);

	debug("setDefendOrder: [%3d](%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, x, y);

	if (combatOrders.find(vehicleId) == combatOrders.end())
	{
		combatOrders[vehicleId] = {};
		combatOrders[vehicleId].defendX = x;
		combatOrders[vehicleId].defendY = y;
	}

}

/*
Checks if sea explorer is in land port.
*/
bool isHealthySeaExplorerInLandPort(int vehicleId)
{
	VEH *vehicle = &(tx_vehicles[vehicleId]);
	MAP *vehicleLocation = getVehicleMapTile(vehicleId);
	int triad = veh_triad(vehicleId);
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
	VEH *vehicle = &(tx_vehicles[vehicleId]);

	// check if this tile has access to ocean

	MAP_INFO adjacentOceanTileInfo = getAdjacentOceanTileInfo(vehicle->x, vehicle->y);

	if (adjacentOceanTileInfo.tile == NULL)
		return SYNC;

	debug("kickSeaExplorerFromLandPort: [%3d] (%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, adjacentOceanTileInfo.x, adjacentOceanTileInfo.y);

	// send unit to this tile

//	vehicle->state &= ~VSTATE_EXPLORE;
	set_move_to(vehicleId, adjacentOceanTileInfo.x, adjacentOceanTileInfo.y);
	tx_action(vehicleId);

	return tx_enemy_move(vehicleId);

}

