#include "aiCombat.h"
#include "ai.h"
#include "engine.h"

/*
Prepares combat orders.
*/
void aiCombatStrategy()
{
	// natives

	aiNativeCombatStrategy();

}

/*
Composes anti-native combat strategy.
*/
void aiNativeCombatStrategy()
{
	debug("aiNativeCombatStrategy\n");

	// check bases protection

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);
		MAP *baseLocation = getMapTile(base->x, base->y);
		BASE_STRATEGY *baseStrategy = &(baseStrategies[id]);

		// estimate potential threat

		int nativeAttackerTriad = (is_ocean(baseLocation) ? TRIAD_SEA : TRIAD_LAND);
		double potentialNativeThreat = getPsiCombatBaseOdds(nativeAttackerTriad);

		// calculate native protection

		double nativeProtection =
			(*current_turn < 50 ? 2.0 : 1.0)
			*
			(1.0 + (double)tx_basic->combat_bonus_intrinsic_base_def / 100.0)
			*
			baseStrategy->nativeProtection
		;

		// calculate demand for native defense

		baseStrategy->nativeDefenseProductionDemand = potentialNativeThreat / (potentialNativeThreat + nativeProtection);

		debug
		(
			"\t%-25s potentialNativeThreat=%4.1f, nativeProtection=%4.1f, nativeDefenseProductionDemand=%4.2f\n",
			baseStrategy->base->name,
			potentialNativeThreat,
			nativeProtection,
			baseStrategy->nativeDefenseProductionDemand
		);

	}

}

void evaluateNativeArtilleryThreat(int nativeArtilleryVehicleId)
{
	VEH *nativeArtilleryVehicle = &(tx_vehicles[nativeArtilleryVehicleId]);
	int nativeArtilleryVehicleTriad = veh_triad(nativeArtilleryVehicleId);

	// find if close to bases

	bool close = false;

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		if (map_range(base->x, base->y, nativeArtilleryVehicle->x, nativeArtilleryVehicle->y) <= 4)
		{
			close = true;
			break;
		}

	}

	if (!close)
		return;

	// list available units

	double nativeArtilleryVehicleHealth = (double)(10 - nativeArtilleryVehicle->damage_taken);
	double totalDamage = 0.0;

	std::vector<VEHICLE_DAMAGE_VALUE> vehicleDamageValues;

	for (int id : combatVehicleIds)
	{
		VEH *vehicle = &(tx_vehicles[id]);
		int triad = veh_triad(id);

		// don't fight natives with air units

		if (triad == TRIAD_AIR)
			continue;

		// same region vehicles only

		if (triad != TRIAD_AIR && triad != nativeArtilleryVehicleTriad)
			continue;

		// calculate damage

		double damage = getVehiclePsiAttackDamage(aiFactionId, id);
		totalDamage += damage;

		// calculate value

		int distance = map_range(vehicle->x, vehicle->y, nativeArtilleryVehicle->x, nativeArtilleryVehicle->y);
		int cost = tx_units[vehicle->proto_id].cost;
		double value = damage / (double)cost / (double)distance;

		// store vehicle

		vehicleDamageValues.push_back({id, damage, value});

	}

	// sort vehicles

	std::sort(vehicleDamageValues.begin(), vehicleDamageValues.end(), compareVehicleDamageValue);

	// select attackers

	double deliveredDamage = 0.0;

	for (VEHICLE_DAMAGE_VALUE vehicleDamageValue : vehicleDamageValues)
	{
		// store order

		vehicleOrders[vehicleDamageValue.id] = {ORDER_MOVE_TO, nativeArtilleryVehicle->x, nativeArtilleryVehicle->y};

		// deliver damage

		deliveredDamage += vehicleDamageValue.damage;

		// enough!

		if (deliveredDamage >= 2.0 * nativeArtilleryVehicleHealth)
			break;

	}

}

int compareVehicleDamageValue(VEHICLE_DAMAGE_VALUE o1, VEHICLE_DAMAGE_VALUE o2)
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

	// find vehicle order

	std::unordered_map<int, VEHICLE_ORDER>::iterator vehicleOrdersIterator = vehicleOrders.find(aiVehicleId);

	// skip vehicles without orders

	if (vehicleOrdersIterator == vehicleOrders.end())
		return SYNC;

	// get vehicle order

	VEHICLE_ORDER *vehicleOrder = &(vehicleOrdersIterator->second);

	// apply order

	applyVehicleOrder(id, vehicleOrder);

	return SYNC;

}

