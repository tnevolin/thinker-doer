#include <map>
#include <map>
#include "engine.h"
#include "terranx_wtp.h"
#include "wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMove.h"
#include "aiMoveCombat.h"
#include "aiTransport.h"

//std::map<int, COMBAT_ORDER> combatOrders;
//
/*
Prepares combat orders.
*/
void moveCombatStrategy()
{
	movePolice2x();
	moveProtector();
	moveAlienProtector();
	
	nativeCombatStrategy();
	
	popPods();

	synchronizeAttack();
	
}

/*
Distribute police2x vehicles among bases.
*/
void movePolice2x()
{
	debug("movePolice2x - %s\n", MFactions[aiFactionId].noun_faction);
	
	// delete task from police2x vehicles
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		// exclude not infantry police2x
		
		if (!isInfantryPolice2xVehicle(vehicleId))
			continue;
		
		// deleteTask
		
		deleteTask(vehicleId);
		
	}
	
	// process existing garrisons
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		
		// exclude those without police need
		
		if (baseInfo->policeAllowed <= 0 || baseInfo->policeDrones <= 0)
			continue;
		
		debug("\t%-25s\n", base->name);
		
		std::vector<IdDoubleValue> basePolice2xVehicles;
		
		// collect available police2x vehicles
		
		for (int vehicleId : aiData.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			// exclude not available
			
			if (!isVehicleAvailable(vehicleId))
				continue;
			
			// exclude not infantry police2x
			
			if (!isInfantryPolice2xVehicle(vehicleId))
				continue;
			
			// exclude not within base protection range
			
			int protectionRange = getVehicleProtectionRange(vehicleId);
			int range = map_range(base->x, base->y, vehicle->x, vehicle->y);
			
			if (range > protectionRange)
				continue;
			
			// store vehicle value
			
			basePolice2xVehicles.push_back({vehicleId, baseInfo->getVehicleAverageCombatEffect(vehicleId)});
			
		}
		
		// sort them descending by average combat effect
		
		std::sort(basePolice2xVehicles.begin(), basePolice2xVehicles.end(), compareIdDoubleValueDescending);
		
		// assign vehicles to current base
		
		for (const IdDoubleValue &basePolice2xVehicleEntry : basePolice2xVehicles)
		{
			int vehicleId = basePolice2xVehicleEntry.id;
			VEH *vehicle = &(Vehicles[vehicleId]);
			debug("\t\t(%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
			
			// task vehicle
			
			int protectionRange = getVehicleProtectionRange(vehicleId);
			TaskType taskType = (protectionRange == 0 ? TT_HOLD : TT_NONE);
			transitVehicle(vehicleId, Task(vehicleId, taskType));
			
			// update police demand
			
			baseInfo->policeAllowed--;
			baseInfo->policeDrones -= (baseInfo->policeEffect + 1);
			
			// update protection demand
			
			baseInfo->getUnitTypeInfo(vehicle->unit_id)->providedProtection += baseInfo->getVehicleAverageCombatEffect(vehicleId);
			
			debug("\t\t\tunitType=%-15s requiredProtection=%5.2f, providedProtection=%5.2f\n", getUnitTypeName(getUnitType(vehicle->unit_id)), baseInfo->getUnitTypeInfo(vehicle->unit_id)->requiredProtection, baseInfo->getUnitTypeInfo(vehicle->unit_id)->providedProtection);
					
			// stop when police request is satisfied
			
			if (baseInfo->policeAllowed <= 0 || baseInfo->policeDrones <= 0)
				break;
			
		}
		
	}
	
	std::vector<IdDoubleValue> police2xVehicles;
	
	// collect available police2x vehicles
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		// exclude not available
		
		if (!isVehicleAvailable(vehicleId))
			continue;
		
		// exclude not infantry police2x
		
		if (!isInfantryPolice2xVehicle(vehicleId))
			continue;
		
		// store vehicle value
		
		police2xVehicles.push_back({vehicleId, aiData.getVehicleAverageCombatEffect(vehicleId)});
		
	}
	
	// sort them descending by average combat effect
	
	std::sort(police2xVehicles.begin(), police2xVehicles.end(), compareIdDoubleValueDescending);
	
	// distribute vehicles accross bases
	
	debug("\tdistribute vehicles accross bases\n");
	for (const IdDoubleValue &police2xVehicleEntry : police2xVehicles)
	{
		int vehicleId = police2xVehicleEntry.id;
		VEH *vehicle = &(Vehicles[vehicleId]);
		debug("\t\t(%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
		
		// find closest base in need for police
		
		int targetBaseId = -1;
		int targetBaseRange = INT_MAX;
		
		for (int baseId : aiData.baseIds)
		{
			BASE *base = &(Bases[baseId]);
			BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
			
			// exclude those without police need
			
			if (baseInfo->policeAllowed <= 0 || baseInfo->policeDrones <= 0)
				continue;
			
			// get range
			
			int range = map_range(vehicle->x, vehicle->y, base->x, base->y);
			
			// update best values
			
			if (range < targetBaseRange)
			{
				targetBaseId = baseId;
				targetBaseRange = range;
			}
			
		}
		
		// not found
		
		if (targetBaseId == -1)
		{
			debug("\t\t\tbase with police need is not found\n");
			continue;
		}
		
		BASE *targetBase = &(Bases[targetBaseId]);
		MAP *targetBaseTile = getBaseMapTile(targetBaseId);
		BaseInfo *targetBaseInfo = aiData.getBaseInfo(targetBaseId);
		debug("\t\t\t-> %-25s\n", targetBase->name);
		
		// send vehicle
		
		transitVehicle(vehicleId, Task(vehicleId, TT_HOLD, targetBaseTile));
		
		// update police demand
		
		targetBaseInfo->policeAllowed--;
		targetBaseInfo->policeDrones -= (targetBaseInfo->policeEffect + 1);
		
		// update protection demand
		
		targetBaseInfo->getUnitTypeInfo(vehicle->unit_id)->providedProtection += targetBaseInfo->getVehicleAverageCombatEffect(vehicleId);
		
		debug("\t\t\tunitType=%-15s providedProtection=%5.2f\n", getUnitTypeName(getUnitType(vehicle->unit_id)), targetBaseInfo->getUnitTypeInfo(vehicle->unit_id)->providedProtection);
				
	}
	
	// set production demand
	
	unsigned int infantryPolice2xRemainedRequest = 0;
	
	for (int baseId : aiData.baseIds)
	{
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		
		// exclude those without police need
		
		if (baseInfo->policeAllowed <= 0 || baseInfo->policeDrones <= 0)
			continue;
		
		// accumulate requests
		
		infantryPolice2xRemainedRequest++;
		
	}
	
	unsigned int infantryPolice2xTotaRequest = police2xVehicles.size() + infantryPolice2xRemainedRequest;
	
	aiData.production.infantryPolice2xDemand = (infantryPolice2xTotaRequest == 0 ? 0.0 : 1.0 - (double)police2xVehicles.size() / (double)infantryPolice2xTotaRequest);
	
}

/*
Distribute other vehicles among bases.
*/
void moveProtector()
{
	debug("moveProtector - %s\n", MFactions[aiFactionId].noun_faction);
	
	// collect bases in need for protection
	
	std::vector<IdDoubleValue> baseProtectionRequests;
	
	for (int baseId : aiData.baseIds)
	{
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		
		// exclude base without protection need
		
		if (baseInfo->isProtectionRequestSatisfied())
			continue;
		
		// get protection request
		
		double remainingProtectionRequest = baseInfo->getRemainingProtectionRequest();
		
		// store base
		
		baseProtectionRequests.push_back({baseId, remainingProtectionRequest});
		
	}
	
	// process bases
	
	while (baseProtectionRequests.size() > 0)
	{
		// sort baseProtectionRequests descending
		
		std::sort(baseProtectionRequests.begin(), baseProtectionRequests.end(), compareIdDoubleValueDescending);
		
		// get most demanding base
		
		IdDoubleValue *baseProtectionRequestEntry = &(baseProtectionRequests.at(0));
		int baseId = baseProtectionRequestEntry->id;
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		
		debug("\t%-25s\n", base->name);
		
		// find closest available combat vehicle
		
		int closestVehicleId = -1;
		int closestVehicleRange = INT_MAX;
		
		for (int vehicleId : aiData.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			// check vehicle is withing base protection range
			
			int range = map_range(base->x, base->y, vehicle->x, vehicle->y);
			int protectionRange = getVehicleProtectionRange(vehicleId);
			bool notAvailableInWarzone = !(range <= protectionRange);
			
			// exclude not available
			
			if (!isVehicleAvailable(vehicleId, notAvailableInWarzone))
				continue;
			
			// exclude not needed in this base
			
			if (baseInfo->getUnitTypeInfo(vehicle->unit_id)->isProtectionRequestSatisfied())
				continue;
			
			// update best
			
			if (range < closestVehicleRange)
			{
				closestVehicleId = vehicleId;
				closestVehicleRange = range;
			}
			
		}
		
		// not found
		
		if (closestVehicleId == -1)
		{
			// remove base from list and continue to next iteration
			
			baseProtectionRequests.erase(baseProtectionRequests.begin());
			continue;
			
		}
		
		VEH *closestVehicle = &(Vehicles[closestVehicleId]);
		
		// task vehicle
		
		int protectionRange = getVehicleProtectionRange(closestVehicleId);
		TaskType taskType = (protectionRange == 0 ? TT_HOLD : TT_NONE);
		
		transitVehicle(closestVehicleId, Task(closestVehicleId, taskType, baseTile));
		
		// update accumulatedTotalCombatEffect
		
		UnitTypeInfo *baseUnitTypeInfo = baseInfo->getUnitTypeInfo(closestVehicle->unit_id);
		baseUnitTypeInfo->providedProtection += baseInfo->getVehicleAverageCombatEffect(closestVehicleId);
		
		debug("\t\t<- (%3d,%3d) %-32s unitType=%-15s, requiredProtection=%5.2f, providedProtection=%5.2f, protectionRequestSatisfied: %s\n", closestVehicle->x, closestVehicle->y, Units[closestVehicle->unit_id].name, getUnitTypeName(getUnitType(closestVehicle->unit_id)), baseUnitTypeInfo->requiredProtection, baseUnitTypeInfo->providedProtection, (baseUnitTypeInfo->isProtectionRequestSatisfied() ? "Yes" : "No"));
		
		// verify base is still in need for protection
		
		if (baseInfo->isProtectionRequestSatisfied())
		{
			// remove base from list and continue to next iteration
			
			baseProtectionRequests.erase(baseProtectionRequests.begin());
			
		}
		else
		{
			// update remainingProtectionRequest
			
			baseProtectionRequestEntry->value = baseInfo->getRemainingProtectionRequest();
			
		}
		
	}
	
}

/*
Ensures each base has at least one protector against natives.
*/
void moveAlienProtector()
{
	debug("moveAlienProtector - %s\n", MFactions[aiFactionId].noun_faction);
	
	// clear production request
	
	aiData.production.alienProtectorRequestCount = 0;
	
	// process bases
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// exclude base with infrantry vehicle
		
		bool baseHasInfantryVehicle = false;
		
		for (int vehicleId : getBaseGarrison(baseId))
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			UNIT *unit = &(Units[vehicle->unit_id]);
			
			// exclude not infantry
			
			if (unit->chassis_type != CHS_INFANTRY)
				continue;
			
			// base has infantry vehicle
			
			baseHasInfantryVehicle = true;
			break;
			
		}
		
		if (baseHasInfantryVehicle)
			continue;
		
		debug("\t%-25s\n", base->name);
		
		// find nearest available vehicle
		
		int nearestVehicleId = -1;
		int nearestVehicleTrance = 0;
		int nearestVehicleRange = INT_MAX;
		
		for (int vehicleId : aiData.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			UNIT *unit = &(Units[vehicle->unit_id]);
			
			// exclude not available
			
			if (!isVehicleAvailable(vehicleId, true))
				continue;
			
			// exclude not infantry
			
			if (unit->chassis_type != CHS_INFANTRY)
				continue;
			
			// get trance ability
			
			int trance = (isVehicleHasAbility(vehicleId, ABL_TRANCE) ? 1 : 0);
			
			// get range
			
			int range = map_range(vehicle->x, vehicle->y, base->x, base->y);
			
			// update best
			
			if
			(
				// trance
				trance > nearestVehicleTrance
				||
				// or closer
				trance == nearestVehicleTrance && range < nearestVehicleRange
			)
			{
				nearestVehicleId = vehicleId;
				nearestVehicleTrance = trance;
				nearestVehicleRange = range;
			}
			
		}
		
		// not found
		
		if (nearestVehicleId == -1)
		{
			debug("\t\tno available vehicle found\n");
			aiData.production.alienProtectorRequestCount++;
			continue;
		}
		
		// task vehicle
		
		transitVehicle(nearestVehicleId, Task(nearestVehicleId, TT_HOLD, baseTile));
		
		debug("\t\t<- (%3d,%3d) %-32s\n", Vehicles[nearestVehicleId].x, Vehicles[nearestVehicleId].y, Units[Vehicles[nearestVehicleId].unit_id].name);
		
	}
	
}

/*
Composes anti-native combat strategy.
*/
void nativeCombatStrategy()
{
	debug("nativeCombatStrategy - %s\n", MFactions[aiFactionId].noun_faction);
	
	// list aliens on land territory
	
	std::vector<IdIntValue> alienVehicleIds;
	std::vector<IdIntValue> sporeLauncherVehicleIds;
	std::vector<IdIntValue> mindWormVehicleIds;
	std::vector<IdIntValue> fungalTowerVehicleIds;

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// alien
		
		if (vehicle->faction_id != 0)
			continue;
		
		// land vehicle
		
		if (vehicle->triad() != TRIAD_LAND)
			continue;
		
		// on land
		
		if (is_ocean(vehicleTile))
			continue;
		
		// on own territory
		
		if (vehicleTile->owner != aiFactionId)
			continue;
		
		// find range to nearest own base
		
		int nearestBaseRange = getNearestAIFactionBaseRange(vehicle->x, vehicle->y);
		
		// add vehicle
		
		switch (vehicle->unit_id)
		{
		case BSC_SPORE_LAUNCHER:
			sporeLauncherVehicleIds.push_back({vehicleId, nearestBaseRange});
			break;
			
		case BSC_MIND_WORMS:
			mindWormVehicleIds.push_back({vehicleId, nearestBaseRange});
			break;
			
		case BSC_FUNGAL_TOWER:
			fungalTowerVehicleIds.push_back({vehicleId, nearestBaseRange});
			break;
			
		}
		
	}
	
	// sort sublists
	
	std::sort(sporeLauncherVehicleIds.begin(), sporeLauncherVehicleIds.end(), compareIdIntValueAscending);
	std::sort(mindWormVehicleIds.begin(), mindWormVehicleIds.end(), compareIdIntValueAscending);
	std::sort(fungalTowerVehicleIds.begin(), fungalTowerVehicleIds.end(), compareIdIntValueAscending);
	
	// combine sublists
	
	alienVehicleIds.insert(alienVehicleIds.end(), sporeLauncherVehicleIds.begin(), sporeLauncherVehicleIds.end());
	alienVehicleIds.insert(alienVehicleIds.end(), mindWormVehicleIds.begin(), mindWormVehicleIds.end());
	alienVehicleIds.insert(alienVehicleIds.end(), fungalTowerVehicleIds.begin(), fungalTowerVehicleIds.end());
	
	// no aliens in territory
	
	if (alienVehicleIds.size() == 0)
	{
		aiData.landAlienHunterDemand = 0.0;
		return;
	}
	
	// list scouts
	
	std::set<int> scoutVehicleIds;
	
	debug("\tscoutVehicles\n");
	
	for (int scoutVehicleId : aiData.scoutVehicleIds)
	{
		VEH *scoutVehicle = &(Vehicles[scoutVehicleId]);
		
		// land/air vehicle
		
		if (!(scoutVehicle->triad() == TRIAD_LAND || scoutVehicle->triad() == TRIAD_AIR))
			continue;
		
		// available

		if (!isVehicleAvailable(scoutVehicleId, false))
			continue;

		// is not damaged more than 50%
		
		if (scoutVehicle->damage_taken > 5 * scoutVehicle->reactor_type())
			continue;
		
		// add to list
		
		scoutVehicleIds.insert(scoutVehicleId);
		
		debug("\t\t[%3d] (%3d,%3d) %s\n", scoutVehicleId, scoutVehicle->x, scoutVehicle->y, Units[scoutVehicle->unit_id].name);
		
	}
		
	// update hunter demand
	
	aiData.landAlienHunterDemand = 1.0 - 0.5 * (double)scoutVehicleIds.size() / (double)alienVehicleIds.size();

	// direct scouts
	
	debug("\tdirect scouts %d\n", scoutVehicleIds.size());
	
	for (IdIntValue &alienVehicleEntry : alienVehicleIds)
	{
		int alienVehicleId = alienVehicleEntry.id;
		int alienVehicleNearestBaseRange = alienVehicleEntry.value;
		VEH *alienVehicle = &(Vehicles[alienVehicleId]);
		MAP *alienVehicleTile = getVehicleMapTile(alienVehicleId);
		debug("\t\t[%3d] (%3d,%3d) alienVehicleNearestBaseRange=%d\n", alienVehicleId, alienVehicle->x, alienVehicle->y, alienVehicleNearestBaseRange);
		
		double requiredDamage = 2.0 * getVehiclePsiDefense(alienVehicleId);
		
		while (requiredDamage > 0.0)
		{
			double bestWeight = 0.0;
			int bestScoutVehicleId = -1;
			double bestScoutVehicleDamage = 0.0;
			
			for (int scoutVehicleId : scoutVehicleIds)
			{
				VEH *scoutVehicle = &(Vehicles[scoutVehicleId]);
				MAP *scoutVehicleTile = getVehicleMapTile(scoutVehicleId);
				
				debug("\t\t\t[%3d] (%3d,%3d)\n", scoutVehicleId, scoutVehicle->x, scoutVehicle->y);
				
				// can reach
				
				if (!isDestinationReachable(scoutVehicleId, getX(alienVehicleTile), getY(alienVehicleTile), false))
					continue;
				
				// get range
				
				int range = std::max(1, map_range(scoutVehicle->x, scoutVehicle->y, alienVehicle->x, alienVehicle->y));
				
				// get time
				
				double time = (double)range / (double)veh_chassis_speed(scoutVehicleId);
				
				// add pickup/delivery time for different region
				
				if (scoutVehicleTile->region != alienVehicleTile->region)
				{
					time += 5.0;
				}
				
				// build weight on time and cost and damage
				
				double damage = getVehiclePsiOffense(scoutVehicleId);
				
				double weight = damage / getUnitMaintenanceCost(scoutVehicle->unit_id) / time;
				
				debug("\t\t\t[%3d] (%3d,%3d) weight=%f, range=%d, time=%f, damage=%f\n", scoutVehicleId, scoutVehicle->x, scoutVehicle->y, weight, range, time, damage);
				
				// update best weight
				
				if (bestScoutVehicleId == -1 || weight > bestWeight)
				{
					bestWeight = weight;
					bestScoutVehicleId = scoutVehicleId;
					bestScoutVehicleDamage = damage;
				}
				
			}
			
			// not found
			
			if (bestScoutVehicleId == -1)
				break;
			
			VEH *bestScoutVehicle = &(Vehicles[bestScoutVehicleId]);
			
			// set attack order
			
			transitVehicle(bestScoutVehicleId, Task(bestScoutVehicleId, TT_MOVE, nullptr, alienVehicleId));
			debug("\t\t<- [%3d] (%3d,%3d)\n", bestScoutVehicleId, bestScoutVehicle->x, bestScoutVehicle->y);
			
			// remove from list
			
			scoutVehicleIds.erase(bestScoutVehicleId);
			
			// update required damage except if land artillery
			
			if (!isLandArtilleryVehicle(bestScoutVehicleId))
			{
				requiredDamage -= bestScoutVehicleDamage;
			}
			
		}
		
	}
	
}

void fightStrategy()
{
	debug("fightStrategy - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// exclude not available vehicles
		
		if (!isVehicleAvailable(vehicleId, false))
			continue;
		
		// road moves left
		
		int movementAllowance = mod_veh_speed(vehicleId) - vehicle->road_moves_spent;
		
		// max reach range
		
		int maxReachRange;
		
		switch (triad)
		{
		case TRIAD_LAND:
			if (!is_ocean(vehicleTile))
			{
				maxReachRange = movementAllowance;
			}
			else
			{
				if (vehicle_has_ability(vehicleId, ABL_AMPHIBIOUS))
				{
					// amphibious vehicle can attack adjacent units at sea
					maxReachRange = 1;
				}
				else
				{
					// non amphibious vehicle cannot attack at sea
					continue;
				}
			}
			break;
			
		default:
			maxReachRange = (movementAllowance + (Rules->mov_rate_along_roads - 1)) / Rules->mov_rate_along_roads;
			break;
			
		}
		
		debug("\t(%3d,%3d) %-25s movementAllowance=%d, maxReachRange=%d\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name, movementAllowance, maxReachRange);
		
		// attack for win
		
		MAP *bestAttackTile = nullptr;
		double bestBattleOdds = 0.0;
		
		for (MAP *rangeTile : getRangeTiles(vehicle->x, vehicle->y, maxReachRange, false))
		{
			int x = getX(rangeTile);
			int y = getY(rangeTile);
			
			int range = map_range(vehicle->x, vehicle->y, x, y);
			
			// exclude opposite realm
			
			switch (triad)
			{
			case TRIAD_LAND:
				// land unit cannot attack at ocean unless amphibious unit at range 1
				if (is_ocean(rangeTile) && !(vehicle_has_ability(vehicle, ABL_AMPHIBIOUS) && range == 1))
					continue;
				break;
				
			case TRIAD_SEA:
				// sea unit cannot attack at land
				if (!is_ocean(rangeTile))
					continue;
				break;
				
			}
			
			// get path movement cost before attack
			
			int pathMovementCost = getVehiclePathMovementCost(vehicleId, x, y, true);
			
			if (pathMovementCost == -1)
				continue;
			
			// too far
			
			if (pathMovementCost >= movementAllowance)
				continue;
			
			// movementAllowanceLeft
			
			int movementAllowanceLeft = movementAllowance - pathMovementCost;
			
			// get vehicle at location
			
			int otherVehicleId = veh_at(x, y);
			
			if (otherVehicleId == -1)
			{
				// check for empty enemy base
				
				if (map_has_item(rangeTile, TERRA_BASE_IN_TILE) && isWar(aiFactionId, rangeTile->owner))
				{
					bestAttackTile = rangeTile;
					bestBattleOdds = 100.0;
				}
					
				continue;
				
			}
			
			if (!isWar(vehicle->faction_id, Vehicles[otherVehicleId].faction_id))
				continue;
			
			int enemyVehicleId = modifiedBestDefender(otherVehicleId, vehicleId, 0);
			
			if (enemyVehicleId == -1)
				continue;
			
			VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);
			
			// exclude vehicle from faction we have no war with
			
			if (!isWar(vehicle->faction_id, enemyVehicle->faction_id))
				continue;
			
			// calculate battle odds
			
			double battleOdds = getBattleOdds(vehicleId, enemyVehicleId) * (double)(std::min(Rules->mov_rate_along_roads, movementAllowanceLeft)) / (double)Rules->mov_rate_along_roads;
			
			debug("\t\t-> (%3d,%3d) %-25s battleOdds=%6.2f, pathMovementCost=%d\n", Vehicles[enemyVehicleId].x, Vehicles[enemyVehicleId].y, Units[Vehicles[enemyVehicleId].unit_id].name, battleOdds, pathMovementCost);
			
			// update best
			
			if (battleOdds > bestBattleOdds)
			{
				bestAttackTile = rangeTile;
				bestBattleOdds = battleOdds;
			}
			
		}
		
		// target enemy vehicle found and battle odds are good enough
		
		if (bestAttackTile != nullptr && bestBattleOdds >= 1.5)
		{
			transitVehicle(vehicleId, Task(vehicleId, TT_MOVE, bestAttackTile));
			debug("\t\t->(%3d,%3d)\n", getX(bestAttackTile), getY(bestAttackTile));
		}
		
	}
	
}

void popPods()
{
	popLandPods();
	popSeaPods();
	
}

void popLandPods()
{
	debug("popLandPods\n");
	
	std::vector<MAP *> podLocations;
	int assignedScouts = 0;
	
	// collect land pods
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int x = getX(mapIndex);
		int y = getY(mapIndex);
		MAP *tile = getMapTile(mapIndex);
		TileInfo *tileInfo = aiData.getTileInfo(tile);
		
		// land
		
		if (is_ocean(tile))
			continue;
		
		// exclude tile without pod
		
		if (goody_at(x, y) == 0)
			continue;
		
		// exclude blocked location
		
		if (tileInfo->blocked)
			continue;
		
		// store pod location
		
		podLocations.push_back(tile);
		
	}
	
	// no pods
	
	if (podLocations.size() == 0)
	{
		debug("\tno pods\n");
		aiData.production.landPodPoppingDemand = 0.0;
		return;
	}
	
	debug("\t%d total pods\n", podLocations.size());
	
	// iterate available scouts
	
	for (int vehicleId : aiData.scoutVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// exclude not available
		
		if (!isVehicleAvailable(vehicleId, false))
			continue;
		
		// land triad
		
		if (vehicle->triad() != TRIAD_LAND)
			continue;
		
		// available
		
		if (!isVehicleAvailable(vehicleId, true))
			continue;
		
		// not damaged
		
		if (isVehicleCanHealAtThisLocation(vehicleId))
			continue;
		
		// search for the nearest land pod
		
		std::vector<MAP *>::iterator nearestPodLocationIterator = podLocations.end();
		int nearestPodRange = INT_MAX;
		
		for (std::vector<MAP *>::iterator podLocationsIterator = podLocations.begin(); podLocationsIterator != podLocations.end(); podLocationsIterator++)
		{
			MAP *tile = *podLocationsIterator;
			
			// skip assigned
			
			if (tile == nullptr)
				continue;
			
			// get location
			
			int x = getX(tile);
			int y = getY(tile);
			
			// location should be reachable by vehicle
			
			if (!isDestinationReachable(vehicleId, x, y, false))
				continue;
			
			// range
			
			int range = map_range(vehicle->x, vehicle->y, x, y);
			
			// update best range
			
			if (range < nearestPodRange)
			{
				nearestPodLocationIterator = podLocationsIterator;
				nearestPodRange = range;
			}
			
		}
		
		// cannot find reachable pod for vehicle
		
		if (nearestPodLocationIterator == podLocations.end())
			continue;
		
		// transit vehicle and clear pod location
		
		transitVehicle(vehicleId, Task(vehicleId, TT_MOVE, *nearestPodLocationIterator));
		*nearestPodLocationIterator = nullptr;
		assignedScouts++;
		
	}
	
	// update remaining demand
	
	aiData.production.landPodPoppingDemand = std::max(0.0, 1.0 - conf.ai_production_pod_per_scout * (double)assignedScouts / (double)podLocations.size());
	debug("\t%d pods assigned\n", assignedScouts);
	debug("\tlandPodPoppingDemand = %f\n", aiData.production.landPodPoppingDemand);
	
}

void popSeaPods()
{
	debug("popSeaPods\n");
	
	// list associations
	
	std::set<int> associations;
	
	for (const auto &associationsEntry : aiData.geography.associations)
	{
		int association = associationsEntry.second;
		
		associations.insert(association);
		
	}
		
	for (const auto &associationsEntry : aiData.geography.faction[aiFactionId].associations)
	{
		int association = associationsEntry.second;
		
		associations.insert(association);
		
	}
		
	for (int association : associations)
	{
		// ocean association
		
		if (!isOceanRegion(association))
			continue;
		
		debug("-> %4d\n", association);
		
		// initialize demands
		
		aiData.production.seaPodPoppingDemand.insert({association, 0.0});
		
		// collect sea pods
		
		std::vector<MAP *> podLocations;
		int assignedScouts = 0;
		
		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			int x = getX(mapIndex);
			int y = getY(mapIndex);
			MAP *tile = getMapTile(mapIndex);
			TileInfo *tileInfo = aiData.getTileInfo(tile);
			int tileAssociation = getAssociation(tile, aiFactionId);
			
			// sea
			
			if (!is_ocean(tile))
				continue;
			
			// this association
			
			if (tileAssociation != association)
				continue;
			
			// exclude tile without pod
			
			if (goody_at(x, y) == 0)
				continue;
			
			// exclude blocked location
			
			if (tileInfo->blocked)
				continue;
			
			// store pod location
			
			podLocations.push_back(tile);
			
		}
		
		// no pods
		
		if (podLocations.size() == 0)
		{
			debug("\tno pods\n");
			continue;
		}
		
		debug("\t%d total pods\n", podLocations.size());
		
		// iterate available scouts
		
		for (int vehicleId : aiData.scoutVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			int vehicleAssociation = getVehicleAssociation(vehicleId);
			
			// exclude not available
			
			if (!isVehicleAvailable(vehicleId, false))
				continue;
			
			// sea triad
			
			if (vehicle->triad() != TRIAD_SEA)
				continue;
			
			// this association
			
			if (vehicleAssociation != association)
				continue;
			
			// no task or order
			
			if (!isVehicleAvailable(vehicleId, true))
				continue;
			
			// not damaged
			
			if (isVehicleCanHealAtThisLocation(vehicleId))
				continue;
			
			// search for the nearest sea pod
			
			std::vector<MAP *>::iterator nearestPodLocationIterator = podLocations.end();
			int nearestPodRange = INT_MAX;
			
			for (std::vector<MAP *>::iterator podLocationsIterator = podLocations.begin(); podLocationsIterator != podLocations.end(); podLocationsIterator++)
			{
				MAP *tile = *podLocationsIterator;
				
				// skip assigned
				
				if (tile == nullptr)
					continue;
				
				// get location
				
				int x = getX(tile);
				int y = getY(tile);
				
				// location should be reachable by vehicle
				
				if (!isDestinationReachable(vehicleId, x, y, false))
					continue;
				
				// range
				
				int range = map_range(vehicle->x, vehicle->y, x, y);
				
				// update best range
				
				if (range < nearestPodRange)
				{
					nearestPodLocationIterator = podLocationsIterator;
					nearestPodRange = range;
				}
				
			}
			
			// no reachable pods for vehicle
			
			if (nearestPodLocationIterator == podLocations.end())
				continue;
			
			// transit vehicle and clear pod location
			
			transitVehicle(vehicleId, Task(vehicleId, TT_MOVE, *nearestPodLocationIterator));
			*nearestPodLocationIterator = nullptr;
			assignedScouts++;
			
		}
		
		// update remaining demand
		
		aiData.production.seaPodPoppingDemand.at(association) = std::max(0.0, 1.0 - conf.ai_production_pod_per_scout * (double)assignedScouts / (double)podLocations.size());
		debug("\t%d pods assigned\n", assignedScouts);
		debug("\tseaPodPoppingDemand = %f\n", aiData.production.seaPodPoppingDemand.at(association));
		
	}
	
}

/*
Try to not attack with low chances and without backup.
*/
void synchronizeAttack()
{
	debug("synchronizeAttack - %s\n", MFactions[aiFactionId].noun_faction);
	
	std::vector<int> holdedVehicleIds;
	
	for (auto &taskEntry : aiData.tasks)
	{
		Task *task = &(taskEntry.second);
		int vehicleId = task->getVehicleId();
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// get destination
		
		MAP *destination = task->getDestination();
		
		if (destination == nullptr)
			continue;
		
		int x = getX(destination);
		int y = getY(destination);
		
		// check destination is no farther than 3 tiles
		
		int range = map_range(vehicle->x, vehicle->y, x, y);
		
		if (range > 3)
			continue;
		
		// check vehicle at destination
		
		int enemyVehicleId = veh_at(getX(destination), getY(destination));
		
		if (enemyVehicleId == -1)
			continue;
		
		VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);
		
		// skip the one we have no war with
		
		if (!isWar(vehicle->faction_id, enemyVehicle->faction_id))
			continue;
		
		debug("\t[%3d] (%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, x, y);
		
		// get vehicle movementAllowance
		
		int movementAllowance = mod_veh_speed(vehicleId);
		
		// get path movement cost before attack
		
		int pathMovementCost = getVehiclePathMovementCost(vehicleId, enemyVehicle->x, enemyVehicle->y, true);
		
		if (pathMovementCost == -1)
			continue;
		
		// reduce movement allowance if vehicle can attack on same turn
		
		int movementAllowanceLeft = (pathMovementCost < movementAllowance ? movementAllowance - pathMovementCost : movementAllowance);
		
		// calculate attacker stack damage
		
		double attackerStackDamage = getAttackerStackDamage(vehicleId, enemyVehicleId) / ((double)(std::min(Rules->mov_rate_along_roads, movementAllowanceLeft)) / (double)Rules->mov_rate_along_roads);
		
		// get vehicle power
		
		int vehiclePower = getVehiclePower(vehicleId);
		
		debug("\t\tvehiclePower=%d, attackerStackDamage=%f\n", vehiclePower, attackerStackDamage);
		
		// verify attacker can destroy stack without dying
		
		if (attackerStackDamage < 0.7 * (double)vehiclePower)
		{
			debug("\t\tvehicle can destroy stack\n");
			continue;
		}
		
		debug("\t\tvehicle can NOT destroy stack\n");
		
		// otherwise look for backup (vehicles targeting same tile at same or closer range)
		
		debug("\t\tbackup\n");
		
		int attackerCount = 1;
		for (auto &otherTaskEntry : aiData.tasks)
		{
			Task *otherTask = &(otherTaskEntry.second);
			int otherVehicleId = otherTask->getVehicleId();
			VEH *otherVehicle = &(Vehicles[otherVehicleId]);
			
			// skip self
			
			if (otherVehicleId == vehicleId)
				continue;
			
			// get destination
			
			MAP *otherDestination = otherTask->getDestination();
			
			if (otherDestination == nullptr)
				continue;
			
			// verify this is same destination
			
			if (otherDestination != destination)
				continue;
			
			int otherX = getX(otherDestination);
			int otherY = getY(otherDestination);
			
			// check otherDestination is no farther than our destination
			
			int otherRange = map_range(otherVehicle->x, otherVehicle->y, otherX, otherY);
			
			if (otherRange > range)
				continue;
			
			debug("\t\t\t[%3d] (%3d,%3d)\n", otherVehicleId, otherVehicle->x, otherVehicle->y);
			
			attackerCount++;
			
		}
		
		// verify multiple attackers can destroy stack without dying
		
		if (attackerStackDamage / (double)attackerCount < 0.7 * (double)vehiclePower)
		{
			debug("\t\tbackup can destroy stack\n");
			continue;
		}
		
		debug("\t\tbackup can NOT destroy stack\n");
		
		// otherwise, hold the vehicle
		
		holdedVehicleIds.push_back(vehicleId);
		
	}
	
	for (int holdedVehicleId : holdedVehicleIds)
	{
		transitVehicle(holdedVehicleId, Task(holdedVehicleId, TT_SKIP));
	}
	
}

MAP *getNearestUnexploredConnectedOceanRegionLocation(int factionId, int initialLocationX, int initialLocationY)
{
	debug("getNearestUnexploredConnectedOceanRegionTile: factionId=%d, initialLocationX=%d, initialLocationY=%d\n", factionId, initialLocationX, initialLocationY);

	MAP *location = nullptr;

	// get ocean connected regions

	std::set<int> connectedOceanRegions = getConnectedOceanRegions(factionId, initialLocationX, initialLocationY);

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
		int x = getX(mapIndex);
		int y = getY(mapIndex);
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

		if (location == nullptr || range < nearestUnexploredConnectedOceanRegionTileRange)
		{
			location = tile;
			nearestUnexploredConnectedOceanRegionTileRange = range;
		}

	}

	return location;

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
Also count faction PLANET.
*/
double getVehiclePsiOffenseValue(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	Faction *faction = &(Factions[vehicle->faction_id]);
	
	// get offense value
	
	double offenseValue = getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_EMPATH))
	{
		offenseValue *= 1.0 + (double)Rules->combat_bonus_empath_song_vs_psi / 100.0;
	}
	
	// times faction PLANET
	
	offenseValue *= 1.0 + (double)Rules->combat_psi_bonus_per_PLANET / 100.0 * (double)faction->SE_planet_pending;
	
	// return value
	
	return offenseValue;
	
}

/*
Returns vehicle own Psi defense value counting morale, abilities.
Also count faction PLANET.
*/
double getVehiclePsiDefenseValue(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	Faction *faction = &(Factions[vehicle->faction_id]);
	
	// get defense value
	
	double defenseValue = getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_TRANCE))
	{
		defenseValue *= 1.0 + ((double)Rules->combat_bonus_trance_vs_psi / 100.0);
	}
	
	// times faction PLANET
	
	if (conf.planet_combat_bonus_on_defense)
	{
		defenseValue *= 1.0 + (double)Rules->combat_psi_bonus_per_PLANET / 100.0 * (double)faction->SE_planet_pending;
	}
	
	// return value
	
	return defenseValue;
	
}

int getRangeToNearestActiveFactionBase(int x, int y)
{
	int minRange = INT_MAX;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		minRange = std::min(minRange, map_range(x, y, base->x, base->y));
		
	}
	
	return minRange;
	
}

/*
Searches for nearest accessible monolith on non-enemy territory.
*/
MAP *getNearestMonolith(int x, int y, int triad)
{
	MAP *startingTile = getMapTile(x, y);
	bool ocean = isOceanRegion(startingTile->region);
	
	MAP *nearestMonolithLocation = nullptr;
	
	// search only for realm unit can move on
	
	if (!(triad == TRIAD_AIR || triad == (ocean ? TRIAD_SEA : TRIAD_LAND)))
		return nullptr;
	
	int minRange = INT_MAX;
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int mapX = getX(mapIndex);
		int mapY = getY(mapIndex);
		MAP* tile = getMapTile(mapIndex);
		
		// same region for non-air units
		
		if (triad != TRIAD_AIR && tile->region != startingTile->region)
			continue;
		
		// friendly territory only
		
		if (tile->owner != -1 && tile->owner != aiFactionId && isWar(tile->owner, aiFactionId))
			continue;
		
		// monolith
		
		if (!map_has_item(tile, TERRA_MONOLITH))
			continue;
		
		int range = map_range(x, y, mapX, mapY);
		
		if (range < minRange)
		{
			nearestMonolithLocation = tile;
			minRange = range;
		}
		
	}
		
	return nearestMonolithLocation;
	
}

/*
Searches for allied base where vehicle can get into.
*/
MAP *getNearestBase(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	int association = getVehicleAssociation(vehicleId);
	
	MAP *nearestBaseLocation = nullptr;
	
	// iterate bases
	
	int minRange = INT_MAX;
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// friendly base only
		
		if (!(base->faction_id == aiFactionId || has_pact(base->faction_id, aiFactionId)))
			continue;
		
		// same association for land unit
		
		if (triad == TRIAD_LAND)
		{
			if (!(getAssociation(baseTile, vehicle->faction_id) == association))
				continue;
		}
		
		// same ocean association for sea unit
		
		if (triad == TRIAD_SEA)
		{
			if (!(getBaseOceanAssociation(baseId) == association))
				continue;
		}
		
		int range = map_range(vehicle->x, vehicle->y, base->x, base->y);
		
		if (range < minRange)
		{
			nearestBaseLocation = baseTile;
			minRange = range;
		}
		
	}
		
	return nearestBaseLocation;
	
}

/*
Checks if vehicle can be used in war operations.
*/
bool isInWarZone(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	debug("isInWarZone (%3d,%3d) %s - %s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name, MFactions[vehicle->faction_id].noun_faction);
	
	for (int otherBaseId = 0; otherBaseId < *total_num_bases; otherBaseId++)
	{
		BASE *otherBase = &(Bases[otherBaseId]);
		MAP *otherBaseTile = getBaseMapTile(otherBaseId);
		
		if (otherBase->faction_id == vehicle->faction_id)
			continue;
		
		if (isWar(vehicle->faction_id, otherBase->faction_id))
		{
			if
			(
				// air vehicle can attack any base
				triad == TRIAD_AIR
				||
				// land vehicle can attack land base within same region
				(triad = TRIAD_LAND && isLandRegion(otherBaseTile->region) && vehicleTile->region == otherBaseTile->region)
				||
				// sea vehicle can attack ocean base within same region
				(triad = TRIAD_SEA && isOceanRegion(otherBaseTile->region) && isInOceanRegion(vehicleId, otherBaseTile->region))
			)
			{
				return true;
			}
			
		}
		
	}
	
	for (int otherVehicleId = 0; otherVehicleId < *total_num_vehicles; otherVehicleId++)
	{
		VEH *otherVehicle = &(Vehicles[otherVehicleId]);
		int otherVehicleTriad = otherVehicle->triad();
		MAP *otherVehicleTile = getVehicleMapTile(otherVehicleId);
		
		if (otherVehicle->faction_id == 0)
			continue;
		
		if (otherVehicle->faction_id == vehicle->faction_id)
			continue;
		
		if (!isWar(vehicle->faction_id, otherVehicle->faction_id))
			continue;
		
		if (!isCombatVehicle(otherVehicleId))
			continue;
		
		debug("\t(%3d,%3d) %s - %s\n", otherVehicle->x, otherVehicle->y, Units[otherVehicle->unit_id].name, MFactions[otherVehicle->faction_id].noun_faction);
		
		if
		(
			// air vehicle can attack everywhere
			triad == TRIAD_AIR
			||
			// land vehicle can attack on the same land region
			(triad = TRIAD_LAND && isLandRegion(otherVehicleTile->region) && vehicleTile->region == otherVehicleTile->region)
			||
			// sea vehicle can attacl on the same ocean region
			(triad = TRIAD_SEA && isOceanRegion(otherVehicleTile->region) && isInOceanRegion(vehicleId, otherVehicleTile->region))
		)
		{
			return true;
		}
		
		if
		(
			// air vehicle can attack everywhere
			otherVehicleTriad == TRIAD_AIR
			||
			// land vehicle can attack on the same land region
			(otherVehicleTriad = TRIAD_LAND && isLandRegion(vehicleTile->region) && otherVehicleTile->region == vehicleTile->region)
			||
			// sea vehicle can attacl on the same ocean region
			(otherVehicleTriad = TRIAD_SEA && isOceanRegion(vehicleTile->region) && isInOceanRegion(otherVehicleId, vehicleTile->region))
		)
		{
			return true;
		}
		
	}
	
	return false;
	
}

/*
Collects all enemies reachable in this turn.
Currently works for sea units only.
*/
std::vector<int> getReachableEnemies(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	std::vector<int> reachableEnemies;
	
	if (vehicle->triad() != TRIAD_SEA)
		return reachableEnemies;
	
	// process steps
	
	int remainedMovementPoints = mod_veh_speed(vehicleId) - vehicle->road_moves_spent;
	
	std::map<MAP *, int> travelCosts;
	travelCosts.insert({vehicleTile, 0});
	
	std::set<MAP *> iterableLocations;
	iterableLocations.insert(vehicleTile);
	
	for (int movementPoints = 1; movementPoints <= remainedMovementPoints; movementPoints++)
	{
		// iterate iterable locations
		
		std::set<MAP *> newIterableLocations;
		
		for
		(
			std::set<MAP *>::iterator iterableLocationsIterator = iterableLocations.begin();
			iterableLocationsIterator != iterableLocations.end();
		)
		{
			MAP *tile = *iterableLocationsIterator;
			int x = getX(tile);
			int y = getY(tile);
			int travelCost = travelCosts[tile];
			
			// check zoc
			
			int zoc = isZoc(vehicle->faction_id, x, y);
			
			// iterate adjacent tiles
			
			bool keepIterable = false;
			
			for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
			{
				int adjacentTileX = getX(adjacentTile);
				int adjacentTileY = getY(adjacentTile);
				
				// ignore already processed locations
				
				if (travelCosts.find(adjacentTile) != travelCosts.end())
					continue;
				
				// add to enemy vehicles if there are any
				
				int enemyVehicleId = veh_at(adjacentTileX, adjacentTileY);
				
				if (enemyVehicleId != -1)
				{
					VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);
					
					if (isWar(vehicle->faction_id, enemyVehicle->faction_id))
					{
						reachableEnemies.push_back(enemyVehicleId);
					}
					
				}
				
				// ignore non friendly base
				
				if (map_has_item(adjacentTile, TERRA_BASE_IN_TILE) && adjacentTile->owner != -1 && adjacentTile->owner != vehicle->faction_id && !has_pact(vehicle->faction_id, adjacentTile->owner))
					continue;
				
				// ignore non friendly vehicle
				
				int adjacentTileOwner = veh_who(adjacentTileX, adjacentTileY);
				
				if (adjacentTileOwner != -1 && adjacentTileOwner != vehicle->faction_id && !has_pact(vehicle->faction_id, adjacentTileOwner))
					continue;
				
				// ignore zoc
				
				if (zoc && isZoc(vehicle->faction_id, adjacentTileX, adjacentTileY))
					continue;
				
				// get movement cost
				
				int hexCost = getHexCost(vehicle->unit_id, vehicle->faction_id, x, y, adjacentTileX, adjacentTileY);
				int adjacentTileTravelCost = travelCost + hexCost;
				
				if (adjacentTileTravelCost > movementPoints)
				{
					// will be iterated in future cycles
					
					keepIterable = true;
					
				}
				else if (adjacentTileTravelCost == movementPoints)
				{
					// insert new location
					
					travelCosts.insert({adjacentTile, movementPoints});
					newIterableLocations.insert(adjacentTile);
					
				}
				
			}
			
			if (keepIterable)
			{
				// keep this location and advance to next iterable location
				
				iterableLocationsIterator++;
				
			}
			else
			{
				// erase this location and advance to next iterable location
				
				iterableLocationsIterator = iterableLocations.erase(iterableLocationsIterator);
				
			}
			
		}
		
		// update iterable locations
		
		iterableLocations.insert(newIterableLocations.begin(), newIterableLocations.end());
		
	}
	
	return reachableEnemies;
	
}

/*
Checks if unit available for orders.
*/
bool isVehicleAvailable(int vehicleId, bool notAvailableInWarzone)
{
	// should not have task and should not be notAvailableInWarzone
	return (!hasTask(vehicleId) && !(notAvailableInWarzone && aiData.getTileInfo(getVehicleMapTile(vehicleId))->warzone));
}

/*
Checks if unit available for orders.
*/
bool isVehicleAvailable(int vehicleId)
{
	return isVehicleAvailable(vehicleId, false);
}

/*
Check if combat unit can sure kill enemy faction unit.
Set task for this if found.
*/
void findSureKill(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	int vehicleSpeed = getVehicleSpeedWithoutRoads(vehicleId);
	int vehicleSpeed1 = (vehicleSpeed == 1 ? 1 : 0);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	// combat vehicles only
	
	if (!isCombatVehicle(vehicleId))
		return;
	
	// exclude air vehicle
	
	if (triad == TRIAD_AIR)
		return;
	
	// exclude land artillery
	
	if (isLandArtilleryVehicle(vehicleId))
		return;
	
	// road moves left
	
	int movementAllowance = mod_veh_speed(vehicleId) - vehicle->road_moves_spent;
	
	debug("findSureKill (%3d,%3d) %-32s movementAllowance=%d\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name, movementAllowance);
	
	// iterate paths
	
	MAP *bestAttackTile = nullptr;
	double bestBattleOdds = 0.0;
	
	std::set<MAP *> processedTiles;
	std::map<MAP *, int> processingTiles;
	processingTiles.insert({vehicleTile, 0});
	
	for (int movementCost = 0; movementCost < movementAllowance; movementCost++)
	{
		debug("\tmovementCost=%2d\n", movementCost);
		
		std::vector<MAP *> completedProcessingTiles;
		std::vector<std::pair<MAP *, int>> addedProcessingTiles;
		
		for (auto &processingTileEntry : processingTiles)
		{
			MAP *tile = processingTileEntry.first;
			int cost = processingTileEntry.second;
			int tileX = getX(tile);
			int tileY = getY(tile);
			TileInfo *tileInfo = aiData.getTileInfo(tile);
			
			// process only current tile with given movementCost
			
			if (cost > movementCost)
				continue;
			
			debug("\t\t(%3d,%3d)\n", tileX, tileY);
			
			// add this tile to removal list
			
			completedProcessingTiles.push_back(tile);
			
			// iterate adjacent tiles
			
			for (MAP *adjacentTile : getAdjacentTiles(tile, false))
			{
				int adjacentTileX = getX(adjacentTile);
				int adjacentTileY = getY(adjacentTile);
				bool adjacentOcean = is_ocean(adjacentTile);
				TileInfo *adjacentTileInfo = aiData.getTileInfo(adjacentTile);
				
				// ignore already processed tiles
				
				if (processedTiles.count(adjacentTile) != 0)
					continue;
				
				debug("\t\t\t(%3d,%3d)\n", adjacentTileX, adjacentTileY);
	
				if
				(
					// empty enemy base
					map_has_item(adjacentTile, TERRA_BASE_IN_TILE) && isWar(aiFactionId, adjacentTile->owner) && !adjacentTileInfo->blocked
					&&
					(
						// land unit can capture land base
						triad == TRIAD_LAND && !adjacentOcean
						||
						// land amphibious unit can capture ocean base
						triad == TRIAD_LAND && isVehicleHasAbility(vehicleId, ABL_AMPHIBIOUS) && adjacentOcean
						||
						// sea unit can capture ocean base
						triad == TRIAD_SEA && adjacentOcean
					)
				)
				{
					debug("\t\t\t\tempty base\n");
					
					// set task
					
					transitVehicle(vehicleId, Task(vehicleId, TT_MOVE, adjacentTile));
					debug("\t->(%3d,%3d)\n", adjacentTileX, adjacentTileY);
					
					return;
					
				}
				else if (adjacentTileInfo->blocked)
				{
					debug("\t\t\t\tblocked location\n");
					
					// add this tile to processed locations
					
					processedTiles.insert(adjacentTile);
					
					// get vehicle at location
					
					int otherVehicleId = veh_at(adjacentTileX, adjacentTileY);
					
					if (otherVehicleId == -1)
					{
						debug("\t\t\t\tERROR: blocked location contains no vehicle.\n");
						return;
					}
					
					VEH *otherVehicle = &(Vehicles[otherVehicleId]);
					
					// exclude not enemy
					
					if (!isWar(vehicle->faction_id, otherVehicle->faction_id))
					{
						debug("\t\t\t\tnot enemy\n");
						continue;
					}
					
					// check whether vehicle can attack tile
					
					if (canVehicleAttackTile(vehicleId, adjacentTile))
					{
						// select best defender
						
						int enemyVehicleId = modifiedBestDefender(otherVehicleId, vehicleId, 0);
						
						// best defender not found
						
						if (enemyVehicleId == -1)
						{
							debug("\t\t\t\tno best defender\n");
							continue;
						}
						
						// get enemy vehicle
						
						VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);
						
						// exclude vehicle from faction we have no war with
						
						if (!isWar(vehicle->faction_id, enemyVehicle->faction_id))
						{
							debug("\t\t\t\tnot enemy\n");
							continue;
						}
						
						// get remainedMovementAllowance
						
						int remainedMovementAllowance = movementAllowance - movementCost;
						
						// get haste coefficient
						
						double hasteCoefficient = (remainedMovementAllowance >= Rules->mov_rate_along_roads ? 1.0 : (double)remainedMovementAllowance / (double)Rules->mov_rate_along_roads);
						
						// calculate battle odds
						
						double battleOdds = getBattleOdds(vehicleId, enemyVehicleId) * hasteCoefficient;
						debug("\t\t\t\t(%3d,%3d) %-32s battleOdds=%6.2f, remainedMovementAllowance=%d\n", enemyVehicle->x, enemyVehicle->y, Units[enemyVehicle->unit_id].name, battleOdds, remainedMovementAllowance);
						
						// update best
						
						if (battleOdds > bestBattleOdds)
						{
							bestAttackTile = adjacentTile;
							bestBattleOdds = battleOdds;
						}
						
					}
					else
					{
						// ignore
					}
					
				}
				else if (triad == TRIAD_LAND && !isVehicleHasAbility(vehicleId, ABL_ID_CLOAKED) && tileInfo->zoc && adjacentTileInfo->zoc)
				{
					// ignore movement along ZOC
					
					debug("\t\t\t\tzoc\n");
					
					// add this tile to processed but ignore it from further computations
					
					processedTiles.insert(adjacentTile);
					
				}
				else
				{
					// get hexCost
					
					int hexCost = mod_hex_cost(vehicle->unit_id, vehicle->faction_id, tileX, tileY, adjacentTileX, adjacentTileY, vehicleSpeed1);
					
					// get adjacentTileMovementCost
					
					int adjacentTileMovementCost = movementCost + hexCost;
					
					debug("\t\t\t\thexCost=%3d, adjacentTileMovementCost=%3d\n", hexCost, adjacentTileMovementCost);
					
					// ignore tiles equal or above movementAllowance
					
					if (adjacentTileMovementCost >= movementAllowance)
						continue;
					
					// add to processing tiles
					
					addedProcessingTiles.push_back({adjacentTile, adjacentTileMovementCost});
					
				}
				
			}
			
		}
		
		// move completedProcessingTiles to processedTiles
		
		for (MAP *tile : completedProcessingTiles)
		{
			processingTiles.erase(tile);
			processedTiles.insert(tile);
		}
		
		// move addedProcessingTiles to processingTiles
		
		for (auto &addedProcessingTileEntry : addedProcessingTiles)
		{
			MAP *tile = addedProcessingTileEntry.first;
			int cost = addedProcessingTileEntry.second;
			
			if (processingTiles.count(tile) == 0)
			{
				processingTiles.insert({tile, cost});
			}
			else if (cost < processingTiles.at(tile))
			{
				processingTiles.at(tile) = cost;
			}
			
		}
		
	}
	
	// target enemy vehicle found and battle odds are good enough
	
	if (bestAttackTile != nullptr && bestBattleOdds >= 1.5)
	{
		transitVehicle(vehicleId, Task(vehicleId, TT_MOVE, bestAttackTile));
		debug("\t\t->(%3d,%3d) bestBattleOdds=%5.2f\n", getX(bestAttackTile), getY(bestAttackTile), bestBattleOdds);
	}
	
}

/*
Determines how far from base vehicle should be to effectivelly protect it.
*/
int getVehicleProtectionRange(int vehicleId)
{
	int protectionRange;
	
	if(isInfantryDefensiveVehicle(vehicleId))
	{
		protectionRange = 0;
	}
	else if(isLandArtilleryVehicle(vehicleId))
	{
		protectionRange = 0;
	}
	else
	{
		protectionRange = 2;
	}
	
	return protectionRange;
	
}

