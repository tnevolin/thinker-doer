#include "aiMove.h"
#include "terranx_wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMoveArtifact.h"
#include "aiMoveColony.h"
#include "aiMoveCombat.h"
#include "aiMoveFormer.h"
#include "aiProduction.h"
#include "aiTransport.h"

void moveStrategy()
{
	debug("moveStrategy - %s\n", MFactions[aiFactionId].noun_faction);
	
	clock_t s1;
	
	// vanilla fix for transport dropoff
	
	fixUndesiredTransportDropoff();
	
	// generic
	
	s1 = clock();
	moveAllStrategy();
	debug("(time) [WTP] -moveAllStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
	
	// artifact

	s1 = clock();
	moveArtifactStrategy();
	debug("(time) [WTP] -moveCombatStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);

	// combat

	s1 = clock();
	moveCombatStrategy();
	debug("(time) [WTP] -moveCombatStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);

	// colony
	
	s1 = clock();
	moveColonyStrategy();
	debug("(time) [WTP] -moveColonyStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
	
	// former

	s1 = clock();
	moveFormerStrategy();
	debug("(time) [WTP] -moveFormerStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);

	// transport

	s1 = clock();
	moveTranportStrategy();
	debug("(time) [WTP] -moveTranportStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
	
	// vanilla fix for transpoft pickup
	
	fixUndesiredTransportPickup();

}

void populateVehicles()
{
	// populate vehicles
	
	aiData.vehicleIds.clear();
	aiData.combatVehicleIds.clear();
	aiData.scoutVehicleIds.clear();
	aiData.regionSurfaceCombatVehicleIds.clear();
	aiData.outsideCombatVehicleIds.clear();
	aiData.colonyVehicleIds.clear();
	aiData.formerVehicleIds.clear();
	aiData.activeCombatUnitIds.clear();
	
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
		
		aiData.vehicleIds.push_back(id);

		// combat vehicles

		if (isCombatVehicle(id))
		{
			// add vehicle to global list

			aiData.combatVehicleIds.push_back(id);
			
			// add vehicle unit to global list
			
			if (std::find(aiData.activeCombatUnitIds.begin(), aiData.activeCombatUnitIds.end(), vehicle->unit_id) == aiData.activeCombatUnitIds.end())
			{
				aiData.activeCombatUnitIds.push_back(vehicle->unit_id);
			}
			
			// scout and native
			
			if (isScoutVehicle(id) || isNativeVehicle(id))
			{
				aiData.scoutVehicleIds.push_back(id);
			}

			// add surface vehicle to region list
			// except land unit in ocean
			
			if (vehicle->triad() != TRIAD_AIR && !(vehicle->triad() == TRIAD_LAND && isOceanRegion(vehicleTile->region)))
			{
				if (aiData.regionSurfaceCombatVehicleIds.count(vehicleTile->region) == 0)
				{
					aiData.regionSurfaceCombatVehicleIds[vehicleTile->region] = std::vector<int>();
				}
				aiData.regionSurfaceCombatVehicleIds[vehicleTile->region].push_back(id);
				
				// add scout to region list
				
				if (isScoutVehicle(id))
				{
					if (aiData.regionSurfaceScoutVehicleIds.count(vehicleTile->region) == 0)
					{
						aiData.regionSurfaceScoutVehicleIds[vehicleTile->region] = std::vector<int>();
					}
					aiData.regionSurfaceScoutVehicleIds[vehicleTile->region].push_back(id);
				}
				
			}

			// find if vehicle is at base

			std::map<MAP *, int>::iterator baseTilesIterator = aiData.baseTiles.find(vehicleTile);

			if (baseTilesIterator == aiData.baseTiles.end())
			{
				// add outside vehicle

				aiData.outsideCombatVehicleIds.push_back(id);

			}
			else
			{
				// temporarily disable as I don't know what to do with it
//				BaseStrategy *baseStrategy = &(aiData.baseStrategies[baseTilesIterator->second]);
//
//				// add combat vehicle to garrison
//				
//				if (isCombatVehicle(id))
//				{
//					baseStrategy->garrison.push_back(id);
//				}
//
//				// add to native protection
//
//				double nativeProtection = calculateNativeDamageDefense(id) / 10.0;
//
//				if (vehicle_has_ability(vehicle, ABL_TRANCE))
//				{
//					nativeProtection *= (1 + (double)Rules->combat_bonus_trance_vs_psi / 100.0);
//				}
//
//				baseStrategy->nativeProtection += nativeProtection;
//
			}

		}
		else if (isColonyVehicle(id))
		{
			aiData.colonyVehicleIds.push_back(id);
		}
		else if (isFormerVehicle(id))
		{
			aiData.formerVehicleIds.push_back(id);
		}

	}

}

/*
vanilla enemy_strategy clears up SENTRY/BOARD status from ground unit in land port
restore boarded units order back to SENTRY/BOARD so they be able to coninue transported
*/	
void fixUndesiredTransportDropoff()
{
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// verify transportVehicleId is set
		
		if (!(vehicle->waypoint_1_y == 0 && vehicle->waypoint_1_x >= 0))
			continue;
		
		// get transportVehicleId
		
		int transportVehicleId = vehicle->waypoint_1_x;
		
		// verify transportVehicleId is within range
		
		if (!(transportVehicleId >= 0 && transportVehicleId < *total_num_vehicles))
			continue;
		
		// get transport vehicle
		
		VEH *transportVehicle = &(Vehicles[transportVehicleId]);
		
		// verify transportVehicle is in same location
		
		if (!(transportVehicle->x == vehicle->x && transportVehicle->y == vehicle->y))
			continue;
		
		// restore status
		
		setVehicleOrder(vehicleId, ORDER_SENTRY_BOARD);
		
	}
	
}

/*
AI transport picks up idle units from port
set their order to hold to prevent this
*/	
void fixUndesiredTransportPickup()
{
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// verify vehicle is at own base
		
		if (!(vehicleTile->owner == aiFactionId && map_has_item(vehicleTile, TERRA_BASE_IN_TILE)))
			continue;
		
		// verify vehicle is idle
		
		if (!(vehicle->move_status == ORDER_NONE))
			continue;
		
		// put vehicle on hold
		
		setVehicleOrder(vehicleId, ORDER_HOLD);
		
	}
	
}

void moveAllStrategy()
{
	// heal
	
	healStrategy();
	
}

void healStrategy()
{
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// exclude ogres
		
		if (vehicle->unit_id == BSC_BATTLE_OGRE_MK1 || vehicle->unit_id == BSC_BATTLE_OGRE_MK2 || vehicle->unit_id == BSC_BATTLE_OGRE_MK3)
			continue;
		
		// exclude artifact
		
		if (isArtifactVehicle(vehicleId))
			continue;
		
		// exclude colony
		
		if (isColonyVehicle(vehicleId))
			continue;
		
		// exclude former
		
		if (isFormerVehicle(vehicleId))
			continue;
		
		// exclude loaded transport
		
		if (isTransportVehicle(vehicleId) && getTransportUsedCapacity(vehicleId) > 0)
			continue;
		
		// at base
		
		if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
		{
			// exclude undamaged
			
			if (vehicle->damage_taken == 0)
				continue;
			
			// exclude under alien artillery bombardment
			
			if (isWithinAlienArtilleryRange(vehicleId))
				continue;
			
			// heal
			
			setTask(vehicleId, Task(vehicleId, TT_HOLD));
			
		}
		
		// in the field
		
		else
		{
			// exclude irrepairable
			
			if (vehicle->damage_taken <= (has_project(aiFactionId, FAC_NANO_FACTORY) ? 0 : 2 * vehicle->reactor_type()))
				continue;
			
			// find nearest monolith
			
			MAP *nearestMonolith = getNearestMonolith(vehicle->x, vehicle->y, vehicle->triad());
			
			if (nearestMonolith != nullptr && map_range(vehicle->x, vehicle->y, getX(nearestMonolith), getY(nearestMonolith)) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
				setTask(vehicleId, Task(vehicleId, TT_MOVE, nearestMonolith));
				continue;
			}
			
			// find nearest base
			
			MAP *nearestBase = getNearestBase(vehicleId);
			
			if (nearestBase != nullptr && map_range(vehicle->x, vehicle->y, getX(nearestBase), getY(nearestBase)) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
				setTask(vehicleId, Task(vehicleId, TT_HOLD, nearestBase));
				continue;
			}
			
			// find nearest not warzone tile
			
			MAP *healingTile = nullptr;
			
			for (int range = 0; range < 10; range++)
			{
				for (MAP *tile : getEqualRangeTiles(vehicle->x, vehicle->y, range))
				{
					TileInfo *tileInfo = aiData.getTileInfo(tile);
					
					// exclude warzone
					
					if (tileInfo->warzone)
						continue;
					
					// select healingTile
					
					healingTile = tile;
					break;
					
				}
				
				if (healingTile != nullptr)
					break;
				
			}
			
			if (healingTile == nullptr)
			{
				healingTile = vehicleTile;
			}
			
			// heal
			
			transitVehicle(vehicleId, Task(vehicleId, TT_HOLD, healingTile));
			
		}
		
	}
	
}

/*
Top level AI enemy move entry point

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
int moveVehicle(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int vehicleSpeed = getVehicleSpeedWithoutRoads(vehicleId);
	
	debug("moveVehicle (%3d,%3d) %s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
	
	// default enemy_move function
	
	int (__attribute__((cdecl)) *defaultEnemyMove)(int) = enemy_move;
//	int (__attribute__((cdecl)) *defaultEnemyMove)(int) = mod_enemy_move;
	
	// do not try more iterations than vehicle has moves
	
	if (vehicle->iter_count > vehicleSpeed)
	{
		return defaultEnemyMove(vehicleId);
	}
	
	// reassign task for sure kill if any
	
	findSureKill(vehicleId);
	
	// execute task
	
	if (hasExecutableTask(vehicleId))
	{
		return executeTask(vehicleId);
	}
	
	// colony
	// all colony movement is scheduled in strategy phase
	
    // use vanilla algorithm for probes
    // Thinker ignores subversion opportunities

    if (isProbeVehicle(vehicleId))
	{
		return enemy_move(vehicleId);
	}

	// unhandled cases handled by default
	
	return defaultEnemyMove(vehicleId);
	
}

/*
Creates tasks to transit vehicle to destination.
*/
void transitVehicle(int vehicleId, Task task)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
//	debug("transitVehicle (%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
	
	// get destination
	
	MAP *destination = task.getDestination();
	
	if (destination == nullptr)
	{
//		debug("\tno destination\n");
		setTask(vehicleId, task);
		return;
	}
	
	int x = getX(destination);
	int y = getY(destination);
	
//	debug("\t->(%3d,%3d)\n", x, y);
	
	// vehicle is at destination already
	
	if (vehicle->x == x && vehicle->y == y)
	{
//		debug("\tat destination\n");
		setTask(vehicleId, task);
		return;
	}
	
	// sea and air units do not need sophisticated transportation algorithms
	
	if (vehicle->triad() == TRIAD_SEA || vehicle->triad() == TRIAD_AIR)
	{
//		debug("\tsea/air unit\n");
		setTask(vehicleId, task);
		return;
	}
	
	// vehicle is on land same association
	
	if (!is_ocean(vehicleTile) && !is_ocean(destination) && isSameAssociation(vehicleTile, destination, vehicle->faction_id))
	{
//		debug("\tsame association\n");
		
		// get range
		
		int range = map_range(vehicle->x, vehicle->y, x, y);
		
		// get path distance
		
		int pathDistance = getPathDistance(vehicle->x, vehicle->y, x, y, vehicle->unit_id, vehicle->faction_id);
		
//		debug("\trange=%d, pathDistance=%d\n", range, pathDistance);
		
		// allow pathDistance to be 3 times longer than range
		
		if (pathDistance != -1 && pathDistance <= 3 * range)
		{
//			debug("\tallow self travel\n");
			setTask(vehicleId, task);
			return;
		}
		
		// otherwise, continue with crossing ocean
//		debug("\tsearch for ferry\n");
		
	}
	
	// get cross ocean association
	
	int crossOceanAssociation = getCrossOceanAssociation(vehicleId, destination);
	
	if (crossOceanAssociation == -1)
	{
//		debug("\tcannot find cross ocean region\n");
		return;
	}
	
//	debug("\tcrossOceanAssociation=%d\n", crossOceanAssociation);
	
	// vehicle needs to be transported or is already transported
	// either way it cannot get to destination itself so we increase transport request counter
	
	if (aiData.seaTransportRequestCounts.count(crossOceanAssociation) == 0)
	{
		aiData.seaTransportRequestCounts.insert({crossOceanAssociation, 0});
	}
	aiData.seaTransportRequestCounts.at(crossOceanAssociation)++;
	
	// get transport id
	
	int seaTransportVehicleId = getVehicleTransportId(vehicleId);
	
	// if not transported see if it at available transport position
	
	if (seaTransportVehicleId == -1)
	{
		seaTransportVehicleId = getAvailableSeaTransportInStack(vehicleId);
		
		// if found - board immediately
		
		if (seaTransportVehicleId != -1)
		{
			board(vehicleId, seaTransportVehicleId);
		}
		
	}
	
	// is being transported
	
	if (seaTransportVehicleId != -1)
	{
//		debug("\tis being transported\n");
		
		VEH *seaTransportVehicle = &(Vehicles[seaTransportVehicleId]);
		
		// do not disturb damaged transport
		
		if (seaTransportVehicle->damage_taken > (2 * seaTransportVehicle->reactor_type()))
			return;
		
		// find delivery location
		
		DeliveryLocation deliveryLocation = getSeaTransportDeliveryLocation(seaTransportVehicleId, vehicleId, destination);
		
		if (deliveryLocation.unloadLocation == nullptr || deliveryLocation.unboardLocation == nullptr)
		{
//			debug("\tcannot find delivery location\n");
			return;
		}
		
//		debug("\tdeliveryLocation=(%3d,%3d):(%3d,%3d)\n", getX(deliveryLocation.unloadLocation), getY(deliveryLocation.unloadLocation), getX(deliveryLocation.unboardLocation), getY(deliveryLocation.unboardLocation));
		
		if (vehicleTile == deliveryLocation.unloadLocation)
		{
//			debug("\tat unload location\n");
			
			// wake up vehicle
			
			setVehicleOrder(vehicleId, ORDER_NONE);
			
			// skip transport
			
			setTask(seaTransportVehicleId, Task(seaTransportVehicleId, TT_SKIP));
			
			// move to unboard location
			
			setTask(vehicleId, Task(vehicleId, TT_MOVE, deliveryLocation.unboardLocation));
			
			return;
			
		}
		
		// add orders
		
		setTask(vehicleId, Task(vehicleId, TT_UNBOARD, deliveryLocation.unboardLocation));
		setTaskIfCloser(seaTransportVehicleId, Task(seaTransportVehicleId, TT_UNLOAD, deliveryLocation.unloadLocation, vehicleId));
		
		return;
		
	}
	
	// search for available sea transport
	
	int availableSeaTransportVehicleId = getAvailableSeaTransport(crossOceanAssociation, vehicleId);
	
	if (availableSeaTransportVehicleId == -1)
	{
//		debug("\tno available sea transports\n");
		
		// meanwhile continue with current task
		
		setTask(vehicleId, task);
		
		return;
		
	}
	
	// get load location
	
	MAP *availableSeaTransportLoadLocation = getSeaTransportLoadLocation(availableSeaTransportVehicleId, vehicleId, destination);
	
	if (availableSeaTransportLoadLocation == nullptr)
		return;
	
	// add boarding tasks
	
//	debug("\tadd boarding tasks: [%3d]\n", availableSeaTransportVehicleId);
	
	setTask(vehicleId, Task(vehicleId, TT_BOARD, nullptr, availableSeaTransportVehicleId));
	setTaskIfCloser(availableSeaTransportVehicleId, Task(availableSeaTransportVehicleId, TT_LOAD, availableSeaTransportLoadLocation, vehicleId));
	
	// clear vehicle status
	// sometimes it is stuck in sentry on transport
	
	setVehicleOrder(vehicleId, ORDER_NONE);
	
}

/*
Delete unit to reduce support burden.
*/
void deleteVehicles()
{
	debug("deleteVehicles - %s\n", MFactions[aiFactionId].noun_faction);
	
	// count total support vs. total intake
	
	int totalMineralIntake2 = 0;
	int totalSupport = 0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		totalMineralIntake2 += base->mineral_intake_2;
		totalSupport += base->mineral_intake_2 - base->mineral_surplus;
		
	}
	
	// check if we are not overburdened
	
	int allowedTotalSupport = 2 + totalMineralIntake2 * 1 / 3;
	
	debug("\ttotalMineralIntake2=%d, allowedTotalSupport=%d, totalSupport=%d\n", totalMineralIntake2, allowedTotalSupport, totalSupport);
	
	if (totalSupport <= allowedTotalSupport)
	{
		debug("\tsupport is bearable\n");
		return;
	}
	
	// find weakest and cheapest combat vehicle
	// assume cheapest is weakest
	
	int weakestCombatVehicleId = -1;
	double weakestCombatVehicleValue = DBL_MAX;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// exclude natives
		
		if (isNativeVehicle(vehicleId))
			continue;
		
		int cost = Units[vehicle->unit_id].cost;
		double moraleModifier = getVehicleMoraleModifier(vehicleId, false);
		
		// increase cost for those without clean reactor
		
		if (!isVehicleHasAbility(vehicleId, ABL_CLEAN_REACTOR))
		{
			cost += 4;
		}
		
		// build value
		
		double value = (double)cost * moraleModifier;
		
		// update best
		
		if (value < weakestCombatVehicleValue)
		{
			weakestCombatVehicleId = vehicleId;
			weakestCombatVehicleValue = value;
		}
		
	}
	
	// not found
	
	if (weakestCombatVehicleId == -1)
		return;
	
	// delete vehicle
	
	debug("\tdelete weakest combat vehicle: (%3d,%3d) %-32s weakestCombatVehicleValue=%5.2f\n", Vehicles[weakestCombatVehicleId].x, Vehicles[weakestCombatVehicleId].y, Units[Vehicles[weakestCombatVehicleId].unit_id].name, weakestCombatVehicleValue);
	setTask(weakestCombatVehicleId, Task(weakestCombatVehicleId, TT_KILL));
	
}

/*
Redistribute vehicle between bases to proportinally burden them with support.
*/
void balanceVehicleSupport()
{
	// find base with lowest/highest mineral surplus
	
	int lowestMineralSurplusBaseId = -1;
	int lowestMineralSurplus = INT_MAX;
	
	int highestMineralSurplusBaseId = -1;
	int highestMineralSurplus = 0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		int mineralIntake2 = base->mineral_intake_2;
		int mineralSurplus = base->mineral_surplus;
		int support = mineralIntake2 - mineralSurplus;
		
		if (support > 0 && mineralSurplus < lowestMineralSurplus)
		{
			lowestMineralSurplusBaseId = baseId;
			lowestMineralSurplus = mineralSurplus;
		}
		
		if (mineralSurplus > highestMineralSurplus)
		{
			highestMineralSurplusBaseId = baseId;
			highestMineralSurplus = mineralSurplus;
		}
		
	}
	
	// not found
	
	if (lowestMineralSurplusBaseId == -1 || highestMineralSurplusBaseId == -1)
		return;
	
	// not enough difference
	
	if (highestMineralSurplus - lowestMineralSurplus < 4)
		return;
	
	// find vehicle to reassign
	
	int reassignVehicleId = -1;
	
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		if (vehicle->home_base_id == lowestMineralSurplusBaseId)
		{
			reassignVehicleId = vehicleId;
			break;
		}
		
	}
	
	// not found
	
	if (reassignVehicleId == -1)
		return;
	
	// reassign
	
	Vehicles[reassignVehicleId].home_base_id = highestMineralSurplusBaseId;
	
}

// ==================================================
// enemy_move entry point
// ==================================================

/*
Modified vehicle movement.
*/
int __cdecl modified_enemy_move(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// choose AI logic
	
	// run WTP AI code for AI eanbled factions
	if (isWtpEnabledFaction(vehicle->faction_id))
	{
		return moveVehicle(vehicleId);
	}
	// default
	else
	{
		return mod_enemy_move(vehicleId);
	}
	
}

