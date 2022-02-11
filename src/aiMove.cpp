#include "aiMove.h"
#include "terranx_wtp.h"
#include "ai.h"
#include "aiMoveColony.h"
#include "aiProduction.h"

/*
Movement phase entry point.
*/
void __cdecl modified_enemy_units_check(int factionId)
{
	// run WTP AI code for AI eanbled factions
	
	if (isUseWtpAlgorithms(factionId))
	{
		clock_t s = clock();
		clock_t s1;
		
		// should be same faction that had upkeep earlier this turn
		
		assert(factionId == aiFactionId);
		
		// create map arrays
		
		s1 = clock();
		aiData.createMapArrays();
		debug("(time) [WTP] aiData.createMapArrays: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
		
		// generate move strategy
		
		s1 = clock();
		moveStrategy();
		debug("(time) [WTP] moveStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
		
		// compute production demands

		s1 = clock();
		aiProductionStrategy();
		debug("(time) [WTP] aiProductionStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);

		// update base production
		
		s1 = clock();
		setProduction();
		debug("(time) [WTP] setProduction: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
		
		// delete map arrays
		
		s1 = clock();
		aiData.deleteMapArrays();
		debug("(time) [WTP] aiData.deleteMapArrays: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
		
		debug("(time) [WTP] modified_enemy_units_check: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
		
	}
	
	// execute original code
	
	clock_t s = clock();
	enemy_units_check(factionId);
	debug("(time) [vanilla] enemy_units_check: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
}

/*
Modified vehicle movement.
*/
int __cdecl modified_enemy_move(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// choose AI logic
	
	// run WTP AI code for AI eanbled factions
	if (isUseWtpAlgorithms(vehicle->faction_id))
	{
		return moveVehicle(vehicleId);
	}
	// default
	else
	{
		return mod_enemy_move(vehicleId);
	}
	
}

void moveStrategy()
{
	debug("moveStrategy - %s\n", MFactions[aiFactionId].noun_faction);
	
	clock_t s1;
	
	// update variables
	
	s1 = clock();
	updateGlobalVariables();
	debug("(time) [WTP] -updateGlobalVariables: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
	
	// generic
	
	s1 = clock();
	moveAllStrategy();
	debug("(time) [WTP] -moveAllStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
	
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

}

void updateGlobalVariables()
{
	// populate vehicles
	
	aiData.vehicleIds.clear();
	aiData.combatVehicleIds.clear();
	aiData.scoutVehicleIds.clear();
	aiData.regionSurfaceCombatVehicleIds.clear();
	aiData.outsideCombatVehicleIds.clear();
	aiData.colonyVehicleIds.clear();
	aiData.formerVehicleIds.clear();
	
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

			std::unordered_map<MAP *, int>::iterator baseTilesIterator = aiData.baseTiles.find(vehicleTile);

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
		
		// at base
		
		if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
		{
			// damaged
			
			if (vehicle->damage_taken == 0)
				continue;
			
			// not under alien artillery bombardment
			
			if (isWithinAlienArtilleryRange(vehicleId))
				continue;
			
			// heal
			
			setTask(vehicleId, Task(HOLD, vehicleId));
//			setMoveOrder(vehicleId, vehicle->x, vehicle->y, ORDER_SENTRY_BOARD);
			
		}
		
		// in the field
		
		else
		{
			// repairable
			
			if (vehicle->damage_taken <= (has_project(aiFactionId, FAC_NANO_FACTORY) ? 0 : 2 * vehicle->reactor_type()))
				continue;
			
			// find nearest monolith
			
			MAP *nearestMonolith = getNearestMonolith(vehicle->x, vehicle->y, vehicle->triad());
			
			if (nearestMonolith != nullptr && map_range(vehicle->x, vehicle->y, getX(nearestMonolith), getY(nearestMonolith)) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
				setTask(vehicleId, Task(MOVE, vehicleId, nearestMonolith));
				continue;
			}
			
			// find nearest base
			
			MAP *nearestBase = getNearestBase(vehicle->x, vehicle->y, vehicle->triad());
			
			if (nearestBase != nullptr && map_range(vehicle->x, vehicle->y, getX(nearestBase), getY(nearestBase)) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
				setTask(vehicleId, Task(HOLD, vehicleId, nearestBase));
				continue;
			}
			
			// heal in field
			
//			setMoveOrder(vehicleId, vehicle->x, vehicle->y, ORDER_SENTRY_BOARD);
			setTask(vehicleId, Task(HOLD, vehicleId));
			
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
	
	debug("moveVehicle (%3d,%3d) %s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
	
	// do not try multiple iterations
	
	if (vehicle->iter_count > 0)
	{
		return mod_enemy_move(vehicleId);
	}
	
	// execute task
	
	if (hasTask(vehicleId))
	{
		return executeTask(vehicleId);
	}
	
	// combat
	
	if (isCombatVehicle(vehicleId))
	{
		return moveCombat(vehicleId);
	}
	
	// colony
	// all colony movement is scheduled in strategy phase
	
	// former
	
	if (isFormerVehicle(vehicleId))
	{
		// WTP vs. Thinker terraforming comparison
		
		return (aiFactionId <= conf.ai_terraforming_factions_enabled ? moveFormer(vehicleId) : mod_enemy_move(vehicleId));
		
	}
	
    // use vanilla algorithm for probes
    // Thinker ignores subversion opportunities

    if (isProbeVehicle(vehicleId))
	{
		return enemy_move(vehicleId);
	}

	// unhandled cases default to Thinker
	
	return mod_enemy_move(vehicleId);
	
}

void setTask(int vehicleId, Task task)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	aiData.tasks.insert({vehicle->pad_0, task});
	
}

bool hasTask(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	return (aiData.tasks.count(vehicle->pad_0) != 0);
	
}

Task *getTask(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	Task *task;
	
	if (aiData.tasks.count(vehicle->pad_0) == 0)
	{
		task = nullptr;
	}
	else
	{
		task = &(aiData.tasks.at(vehicle->pad_0));
	}
	
	return task;
	
}

int executeTask(int vehicleId)
{
	debug("executeTask\n");
	
	Task *task = getTask(vehicleId);
	
	return task->execute(vehicleId);
	
}

/*
Updates vehicle task if new task has closer destination.
*/
void setTaskIfCloser(int vehicleId, Task task)
{
	// task exists
	if (hasTask(vehicleId))
	{
		// get current task
		
		Task *currentTask = getTask(vehicleId);
		
		if (currentTask == nullptr)
			return;
		
		// get current task range
		
		int currentTaskDestinationRange = currentTask->getDestinationRange();
		
		// get task range
		
		int taskDestinationRange = task.getDestinationRange();
		
		// update task only if given one has shorter range
		
		if (taskDestinationRange < currentTaskDestinationRange)
		{
			setTask(vehicleId, task);
		}
		
	}
	// current task do not exist
	else
	{
		setTask(vehicleId, task);
	}
	
}

int getVehicleIdByPad0(int vehiclePad0)
{
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		if (vehicle->pad_0 == vehiclePad0)
		{
			return vehicleId;
		}
		
	}
	
	return -1;
	
}

/*
Creates tasks to transit vehicle to destination.
*/
void transitVehicle(int vehicleId, Task task)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	debug("transitVehicle (%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
	
	// get destination
	
	MAP *destination = task.getDestination();
	
	if (destination == nullptr)
	{
		debug("\tinvalid destination\n");
		return;
	}
	
	int x = getX(destination);
	int y = getY(destination);
	
	debug("\t->(%3d,%3d)\n", x, y);
	
	// vehicle is at destination already
	
	if (vehicle->x == x && vehicle->y == y)
	{
		debug("\tat destination\n");
		setTask(vehicleId, task);
		return;
	}
	
	// sea and air units do not need sophisticated transportation algorithms
	
	if (vehicle->triad() == TRIAD_SEA || vehicle->triad() == TRIAD_AIR)
	{
		debug("\tsea/air unit\n");
		setTask(vehicleId, task);
		return;
	}
	
	// vehicle is on land same association
	
	if (isLandRegion(vehicleTile->region) && isLandRegion(destination->region) && isSameAssociation(vehicleTile, destination, vehicle->faction_id))
	{
		debug("\tsame association\n");
		setTask(vehicleId, task);
		return;
	}
	
	// get transport id
	
	int seaTransportVehicleId = getLandVehicleSeaTransportVehicleId(vehicleId);
	
	// is being transported
	
	if (seaTransportVehicleId != -1)
	{
		debug("\tis being transported\n");
		
		// find unboard location
		
		MAP *unboardLocation = getSeaTransportUnboardLocation(seaTransportVehicleId, destination);
		
		if (unboardLocation == nullptr)
		{
			debug("\tcannot find unboard location\n");
			return;
		}
		
		int unboardLocationX = getX(unboardLocation);
		int unboardLocationY = getY(unboardLocation);
		
		debug("\tunboardLocation=(%3d,%3d)\n", unboardLocationX, unboardLocationY);
		
		// find unload location
		
		MAP *unloadLocation = getSeaTransportUnloadLocation(seaTransportVehicleId, destination, unboardLocation);
		
		if (unloadLocation == nullptr)
		{
			debug("\tcannot find unload location\n");
			return;
		}
		
		int unloadLocationX = getX(unloadLocation);
		int unloadLocationY = getY(unloadLocation);
		
		debug("\tunloadLocation=(%3d,%3d)\n", unloadLocationX, unloadLocationY);
		
		if (vehicle->x == unloadLocationX && vehicle->y == unloadLocationY)
		{
			debug("\tat unload location\n");
			
			// wake up vehicle
			
			setVehicleOrder(vehicleId, ORDER_NONE);
			
			// skip transport
			
			setTask(seaTransportVehicleId, Task(SKIP, seaTransportVehicleId));
			
			// move to unboard location
			
			setTask(vehicleId, Task(MOVE, vehicleId, unboardLocation));
			
			return;
			
		}
		
		// add orders
		
		setTask(vehicleId, Task(UNBOARD, vehicleId, unboardLocation));
//		setTaskIfCloser(seaTransportVehicleId, Task(UNLOAD, seaTransportVehicleId, unloadLocation, vehicleId));
		setTaskIfCloser(seaTransportVehicleId, Task(UNLOAD, seaTransportVehicleId, unloadLocation));
		
		return;
		
	}
	
	// get cross ocean association
	
	int crossOceanAssociation = getCrossOceanAssociation(getMapTile(vehicle->x, vehicle->y), destination, vehicle->faction_id);
	
	if (crossOceanAssociation == -1)
	{
		debug("\tcannot find cross ocean region\n");
		return;
	}
	
	debug("\tcrossOceanAssociation=%d\n", crossOceanAssociation);
	
	// search for available sea transport
	
	int availableSeaTransportVehicleId = getAvailableSeaTransport(crossOceanAssociation, vehicleId);
	
	if (availableSeaTransportVehicleId == -1)
	{
		debug("\tno available sea transports\n");
		
		// request a transport to be built
		
		std::unordered_map<int, int>::iterator seaTransportRequestIterator = aiData.seaTransportRequests.find(crossOceanAssociation);
		
		if (seaTransportRequestIterator == aiData.seaTransportRequests.end())
		{
			aiData.seaTransportRequests.insert({crossOceanAssociation, 1});
		}
		else
		{
			seaTransportRequestIterator->second++;
		}
		
		// meanwhile continue with current task
		
		setTask(vehicleId, task);
		
		return;
		
	}
	
	// get load location
	
	MAP *availableSeaTransportLoadLocation = getSeaTransportLoadLocation(availableSeaTransportVehicleId, vehicleId);
	
	if (availableSeaTransportLoadLocation == nullptr)
		return;
	
	// add boarding tasks
	
	debug("\tadd boarding tasks: [%3d]\n", availableSeaTransportVehicleId);
	
	setTask(vehicleId, Task(BOARD, vehicleId, nullptr, availableSeaTransportVehicleId));
	setTaskIfCloser(availableSeaTransportVehicleId, Task(LOAD, availableSeaTransportVehicleId, availableSeaTransportLoadLocation));
	
	return;
	
}

