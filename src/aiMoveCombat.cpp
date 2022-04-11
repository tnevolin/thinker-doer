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
	moveInfantryDefender();
	moveOtherProtectors();
	
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
	
	std::vector<IdDoubleValue> police2xVehicles;
	
	// clear all police2x tasks
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		// exclude not infantry police2x
		
		if (!isInfantryPolice2xVehicle(vehicleId))
			continue;
		
		// clear task
		
		aiData.tasks.erase(vehicleId);
		
	}
	
	// collect available police2x vehicles
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		// exclude not infantry police2x
		
		if (!isInfantryPolice2xVehicle(vehicleId))
			continue;
		
		// exclude not available
		
		if (!isVehicleAvailable(vehicleId, true))
			continue;
		
		// store vehicle value
		
		police2xVehicles.push_back({vehicleId, aiData.getVehicleAverageCombatEffect(vehicleId)});
		
	}
	
	// sort them descending by average combat effect
	
	std::sort(police2xVehicles.begin(), police2xVehicles.end(), compareIdDoubleValueDescending);
	
	// distribute vehicles accross bases
	
	for (const IdDoubleValue &police2xVehicleEntry : police2xVehicles)
	{
		int vehicleId = police2xVehicleEntry.id;
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// find best base
		
		int bestBaseId = -1;
		double bestBaseWeight = 0.0;
		double bestBaseVehicleAverageCombatEffect = 0.0;
		
		for (int baseId : aiData.baseIds)
		{
			BASE *base = &(Bases[baseId]);
			BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
			debug("\t%s\n", Bases[baseId].name);
			
			// exclude those without police need
			
			if (baseInfo->policeAllowed <= 0 || baseInfo->policeDrones <= 0)
				continue;
			
			// calculate weight
			
			double baseMeleeProtectionRemainingRequest = baseInfo->unitTypeInfos[UT_MELEE].getRemainingProtectionRequest();
			double baseVechicleAverageCombatEffect = baseInfo->getVehicleAverageCombatEffect(vehicleId);
			int range = map_range(vehicle->x, vehicle->y, base->x, base->y);
			double rangeCoefficient = exp(- (double)range / 10.0);
			double weight = baseMeleeProtectionRemainingRequest * baseVechicleAverageCombatEffect * rangeCoefficient;
			
			// update best base
			
			if (weight > bestBaseWeight)
			{
				bestBaseId = baseId;
				bestBaseWeight = weight;
				bestBaseVehicleAverageCombatEffect = baseVechicleAverageCombatEffect;
			}
			
		}
		
		// not found
		
		if (bestBaseId == -1)
			continue;
		
		MAP *bestBaseTile = getBaseMapTile(bestBaseId);
		BaseInfo *bestBaseInfo = aiData.getBaseInfo(bestBaseId);
		
		// send vehicle
		
		setTask(vehicleId, Task(vehicleId, HOLD, bestBaseTile));
		
		// update police demand
		
		bestBaseInfo->policeAllowed--;
		bestBaseInfo->policeDrones -= (bestBaseInfo->policeEffect + 1);
		
		// update defense demand
		
		bestBaseInfo->unitTypeInfos[UT_MELEE].providedProtection += bestBaseVehicleAverageCombatEffect;
		
		debug("\t\t[%3d](%3d,%3d) %-25s -> %s\n", vehicleId, vehicle->x, vehicle->y, Units[vehicle->unit_id].name, Bases[bestBaseId].name);
				
	}
	
	// set production demand
	
	unsigned int infantryPolice2xRemainedRequest = 0;
	
	for (int baseId : aiData.baseIds)
	{
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		debug("\t%s\n", Bases[baseId].name);
		
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
Distribute defensive vehicles among bases.
*/
void moveInfantryDefender()
{
	debug("moveInfantryDefender - %s\n", MFactions[aiFactionId].noun_faction);
	
	std::vector<IdDoubleValue> vehicles;
	
	// collect available vehicles
	
	debug("\tavailable vehicles\n");
	for (int vehicleId : aiData.combatVehicleIds)
	{
		// exclude not available
		
		if (!isVehicleAvailable(vehicleId, true))
			continue;
		
		// exclude not infantry defensive
		
		if (!isInfantryDefensiveVehicle(vehicleId))
			continue;
		
		// store vehicle value
		
		vehicles.push_back({vehicleId, aiData.getVehicleAverageCombatEffect(vehicleId)});
		debug("\t\t(%3d,%3d) %s\n", Vehicles[vehicleId].x, Vehicles[vehicleId].y, Units[Vehicles[vehicleId].unit_id].name);
		
	}
	
	// sort them descending by average combat effect
	
	std::sort(vehicles.begin(), vehicles.end(), compareIdDoubleValueDescending);
	
	// distribute vehicles accross bases
	
	debug("\tsatisfy local base defense demand\n");
	
	for (const IdDoubleValue &vehicleEntry : vehicles)
	{
		int vehicleId = vehicleEntry.id;
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		debug("\t\t(%3d,%3d) %-25s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
		
		// exclude with task
		
		if (hasTask(vehicleId))
			continue;
		
		// exclude vehicle outside of base
		
		if (aiData.baseTiles.count(vehicleTile) == 0)
			continue;
		
		// get local base
		
		int baseId = aiData.baseTiles.at(vehicleTile);
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		debug("\t\t\t%-25s meleeProtection.required=%5.2f, meleeProtection.provided=%5.2f\n", Bases[baseId].name, baseInfo->unitTypeInfos[UT_MELEE].requiredProtection, baseInfo->unitTypeInfos[UT_MELEE].providedProtection)
		
		// exclude base without protection need
		
		if (baseInfo->unitTypeInfos[UT_MELEE].isProtectionSatisfied())
			continue;
		
		// hold vehicle
		
		setTask(vehicleId, Task(vehicleId, HOLD));
		
		// update accumulatedTotalCombatEffect
		
		baseInfo->unitTypeInfos[UT_MELEE].providedProtection += baseInfo->getVehicleAverageCombatEffect(vehicleId);
		
		debug("\t\t\t-> %-25s meleeProtection.required=%5.2f, meleeProtection.provided=%5.2f\n", Bases[baseId].name, baseInfo->unitTypeInfos[UT_MELEE].requiredProtection, baseInfo->unitTypeInfos[UT_MELEE].providedProtection);
				
	}
	
	// distribute vehicles accross bases
	
	debug("\tdistribute vehicles\n");
	for (const IdDoubleValue &vehicleEntry : vehicles)
	{
		int vehicleId = vehicleEntry.id;
		VEH *vehicle = &(Vehicles[vehicleId]);
		int triad = vehicle->triad();
		int vehicleAssociation = getVehicleAssociation(vehicleId);
		
		// exclude with task
		
		if (hasTask(vehicleId))
			continue;
		
		debug("\t\t(%3d,%3d) %-25s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
		
		// find best base
		
		int bestBaseId = -1;
		double bestBaseWeight = 0.0;
		double bestBaseVehicleAverageCombatEffect = 0.0;
		
		for (int baseId : aiData.baseIds)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			bool ocean = is_ocean(baseTile);
			int baseOceanAssociation = getBaseOceanAssociation(baseId);
			BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
			
			// don't send sea unit to land base
			
			if (triad == TRIAD_SEA && !ocean)
				continue;
			
			// don't send sea unit to different ocean association
			
			if (triad == TRIAD_SEA && ocean && baseOceanAssociation != vehicleAssociation)
				continue;
			
			// exclude base without protection need
			
			if (baseInfo->unitTypeInfos[UT_MELEE].isProtectionSatisfied())
				continue;
			
			// calculate weight
			
			double baseMeleeProtectionRemainingRequest = baseInfo->unitTypeInfos[UT_MELEE].getRemainingProtectionRequest();
			double baseVechicleAverageCombatEffect = baseInfo->getVehicleAverageCombatEffect(vehicleId);
			int range = map_range(vehicle->x, vehicle->y, base->x, base->y);
			double rangeCoefficient = exp(- (double)range / 10.0);
			double weight = baseMeleeProtectionRemainingRequest * baseVechicleAverageCombatEffect * rangeCoefficient;
			debug("\t\t\t%-25s weight=%5.2f, baseMeleeProtectionRemainingRequest=%5.2f, baseVechicleAverageCombatEffect=%5.2f, rangeCoefficient=%5.2f\n", base->name, weight, baseMeleeProtectionRemainingRequest, baseVechicleAverageCombatEffect, rangeCoefficient)
			
			// update best base
			
			if (weight > bestBaseWeight)
			{
				bestBaseId = baseId;
				bestBaseWeight = weight;
				bestBaseVehicleAverageCombatEffect = baseVechicleAverageCombatEffect;
			}
			
		}
		
		// not found
		
		if (bestBaseId == -1)
			continue;
		
		BASE *bestBase = &(Bases[bestBaseId]);
		MAP *bestBaseTile = getBaseMapTile(bestBaseId);
		BaseInfo *bestBaseInfo = aiData.getBaseInfo(bestBaseId);
		
		// send vehicle
		
		if(isInfantryDefensiveVehicle(vehicleId))
		{
			setTask(vehicleId, Task(vehicleId, HOLD, bestBaseTile));
		}
		else
		{
			// move other units close enough to the base
			
			int range = map_range(vehicle->x, vehicle->y, bestBase->x, bestBase->y);
			
			int allowedRange;
			
			switch (Units[vehicle->unit_id].chassis_type)
			{
			case CHS_INFANTRY:
				allowedRange = 2;
				break;
			case CHS_SPEEDER:
				allowedRange = 4;
				break;
			case CHS_HOVERTANK:
				allowedRange = 6;
				break;
			case CHS_FOIL:
				allowedRange = 8;
				break;
			case CHS_CRUISER:
				allowedRange = 12;
				break;
			case CHS_NEEDLEJET:
			case CHS_COPTER:
			case CHS_MISSILE:
			case CHS_GRAVSHIP:
				allowedRange = 15;
				break;
			default:
				allowedRange = 0;
			}
			
			if (range > allowedRange)
			{
				setTask(vehicleId, Task(vehicleId, MOVE, bestBaseTile));
			}
			
		}
		
		// update accumulatedTotalCombatEffect
		
		bestBaseInfo->unitTypeInfos[UT_MELEE].providedProtection += bestBaseVehicleAverageCombatEffect;
		
		debug("\t\t\t-> %s\n", Bases[bestBaseId].name);
				
	}
	
}

/*
Distribute other vehicles among bases.
*/
void moveOtherProtectors()
{
	debug("moveOtherProtectors - %s\n", MFactions[aiFactionId].noun_faction);
	
	std::vector<IdDoubleValue> vehicles;
	
	// collect available vehicles
	
	debug("\tavailable vehicles\n");
	for (int vehicleId : aiData.combatVehicleIds)
	{
		// exclude not available
		
		if (!isVehicleAvailable(vehicleId, true))
			continue;
		
		// exclude infantry defensive
		
		if (isInfantryDefensiveVehicle(vehicleId))
			continue;
		
		// store vehicle value
		
		vehicles.push_back({vehicleId, aiData.getVehicleAverageCombatEffect(vehicleId)});
		
		debug("\t\t(%3d,%3d) %s\n", Vehicles[vehicleId].x, Vehicles[vehicleId].y, Units[Vehicles[vehicleId].unit_id].name);
		
	}
	
	// sort them descending by average combat effect
	
	std::sort(vehicles.begin(), vehicles.end(), compareIdDoubleValueDescending);
	
	// distribute vehicles accross bases
	
	debug("\tdistribute vehicles\n");
	for (const IdDoubleValue &vehicleEntry : vehicles)
	{
		int vehicleId = vehicleEntry.id;
		VEH *vehicle = &(Vehicles[vehicleId]);
		int triad = vehicle->triad();
		int vehicleAssociation = getVehicleAssociation(vehicleId);
		debug("\t\t(%3d,%3d) %-25s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
		
		// find best base
		
		int bestBaseId = -1;
		double bestBaseWeight = 0.0;
		double bestBaseVehicleAverageCombatEffect = 0.0;
		
		for (int baseId : aiData.baseIds)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			bool ocean = is_ocean(baseTile);
			int baseOceanAssociation = getBaseOceanAssociation(baseId);
			BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
			
			// don't send sea unit to land base
			
			if (triad == TRIAD_SEA && !ocean)
				continue;
			
			// don't send sea unit to different ocean association
			
			if (triad == TRIAD_SEA && ocean && baseOceanAssociation != vehicleAssociation)
				continue;
			
			// exclude base without protection need
			
			if (baseInfo->unitTypeInfos[UT_MELEE].isProtectionSatisfied())
				continue;
			
			// calculate weight
			
			double baseMeleeProtectionRemainingRequest = baseInfo->unitTypeInfos[UT_MELEE].getRemainingProtectionRequest();
			double baseVechicleAverageCombatEffect = baseInfo->getVehicleAverageCombatEffect(vehicleId);
			int range = map_range(vehicle->x, vehicle->y, base->x, base->y);
			double rangeCoefficient = exp(- (double)range / 10.0);
			double weight = baseMeleeProtectionRemainingRequest * baseVechicleAverageCombatEffect * rangeCoefficient;
			debug("\t\t\t%-25s weight=%5.2f, baseMeleeProtectionRemainingRequest=%5.2f, baseVechicleAverageCombatEffect=%5.2f, rangeCoefficient=%5.2f\n", base->name, weight, baseMeleeProtectionRemainingRequest, baseVechicleAverageCombatEffect, rangeCoefficient)
			
			// update best base
			
			if (weight > bestBaseWeight)
			{
				bestBaseId = baseId;
				bestBaseWeight = weight;
				bestBaseVehicleAverageCombatEffect = baseVechicleAverageCombatEffect;
			}
			
		}
		
		// not found
		
		if (bestBaseId == -1)
			continue;
		
		BASE *bestBase = &(Bases[bestBaseId]);
		MAP *bestBaseTile = getBaseMapTile(bestBaseId);
		BaseInfo *bestBaseInfo = aiData.getBaseInfo(bestBaseId);
		
		// send vehicle
		
		if(isInfantryDefensiveVehicle(vehicleId))
		{
			setTask(vehicleId, Task(vehicleId, HOLD, bestBaseTile));
		}
		else
		{
			// move other units close enough to the base
			
			int range = map_range(vehicle->x, vehicle->y, bestBase->x, bestBase->y);
			
			int allowedRange;
			
			switch (Units[vehicle->unit_id].chassis_type)
			{
			case CHS_INFANTRY:
				allowedRange = 2;
				break;
			case CHS_SPEEDER:
				allowedRange = 4;
				break;
			case CHS_HOVERTANK:
				allowedRange = 6;
				break;
			case CHS_FOIL:
				allowedRange = 8;
				break;
			case CHS_CRUISER:
				allowedRange = 12;
				break;
			case CHS_NEEDLEJET:
			case CHS_COPTER:
			case CHS_MISSILE:
			case CHS_GRAVSHIP:
				allowedRange = 15;
				break;
			default:
				allowedRange = 0;
			}
			
			if (range > allowedRange)
			{
				setTask(vehicleId, Task(vehicleId, MOVE, bestBaseTile));
			}
			
		}
		
		// update accumulatedTotalCombatEffect
		
		bestBaseInfo->unitTypeInfos[UT_MELEE].providedProtection += bestBaseVehicleAverageCombatEffect;
		
		debug("\t\t\t-> %s\n", Bases[bestBaseId].name);
				
	}
	
}

/*
Composes anti-native combat strategy.
*/
void nativeCombatStrategy()
{
	debug("nativeCombatStrategy - %s\n", MFactions[aiFactionId].noun_faction);
	
	// list aliens on land territory
	
	std::vector<int> alienVehicleIds;
	std::vector<int> sporeLauncherVehicleIds;
	std::vector<int> mindWormVehicleIds;
	std::vector<int> fungalTowerVehicleIds;

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
		
		// add vehicle
		
		switch (vehicle->unit_id)
		{
		case BSC_SPORE_LAUNCHER:
			sporeLauncherVehicleIds.push_back(vehicleId);
			break;
			
		case BSC_MIND_WORMS:
			mindWormVehicleIds.push_back(vehicleId);
			break;
			
		case BSC_FUNGAL_TOWER:
			fungalTowerVehicleIds.push_back(vehicleId);
			break;
			
		}
		

	}
	
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
	
	for (int alienVehicleId : alienVehicleIds)
	{
		VEH *alienVehicle = &(Vehicles[alienVehicleId]);
		MAP *alienVehicleTile = getVehicleMapTile(alienVehicleId);
		debug("\t\t[%3d] (%d,%d)\n", alienVehicleId, alienVehicle->x, alienVehicle->y);
		
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
				
				debug("\t\t\t[%3d] (%d,%d)\n", scoutVehicleId, scoutVehicle->x, scoutVehicle->y);
				
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
				
				debug("\t\t\t[%3d] (%d,%d) weight=%f, range=%d, time=%f, damage=%f\n", scoutVehicleId, scoutVehicle->x, scoutVehicle->y, weight, range, time, damage);
				
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
			
			// set attack order
			
			setTask(bestScoutVehicleId, Task(bestScoutVehicleId, MOVE, nullptr, alienVehicleId));
			
			// remove from list
			
			scoutVehicleIds.erase(bestScoutVehicleId);
			
			// update required damage
			
			requiredDamage -= bestScoutVehicleDamage;
			
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
			transitVehicle(vehicleId, Task(vehicleId, MOVE, bestAttackTile));
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
		
		transitVehicle(vehicleId, Task(vehicleId, MOVE, *nearestPodLocationIterator));
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
			
			transitVehicle(vehicleId, Task(vehicleId, MOVE, *nearestPodLocationIterator));
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
		
		// check wehicle at destination
		
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
		
		int vehiclePower = getVehiclePower(enemyVehicleId);
		
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
		setTask(holdedVehicleId, Task(holdedVehicleId, SKIP));
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
bool isVehicleAvailable(int vehicleId, bool notInWarzone)
{
	return (!hasTask(vehicleId) && (!notInWarzone || !aiData.getTileInfo(getVehicleMapTile(vehicleId))->warzone));
}

/*
Check if combat unit can sure kill enemy faction unit.
Set task for this if found.
*/
void findSureKill(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	// combat vehicles only
	
	if (!isCombatVehicle(vehicleId))
		return;
	
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
				return;
			}
		}
		break;
		
	default:
		maxReachRange = (movementAllowance + (Rules->mov_rate_along_roads - 1)) / Rules->mov_rate_along_roads;
		break;
		
	}
	
	debug("sureKill [%3d] (%3d,%3d) %s\n", vehicleId, vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
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
				bestBattleOdds = DBL_MAX;
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
		transitVehicle(vehicleId, Task(vehicleId, MOVE, bestAttackTile));
		debug("\t\t->(%3d,%3d)\n", getX(bestAttackTile), getY(bestAttackTile));
	}
	
}

