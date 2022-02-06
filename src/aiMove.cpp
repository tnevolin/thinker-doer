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
		// should be same faction that had upkeep earlier this turn
		
		assert(factionId == aiFactionId);
		
		// generate move strategy
		
		moveStrategy();
		
		// compute production demands

		aiProductionStrategy();

		// update base production
		
		setProduction();
		
	}
	
	// execute original code
	
	enemy_units_check(factionId);
	
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
	
	// generic
	
	moveAllStrategly();
	
	// combat

	moveCombatStrategy();

	// colony
	
	moveColonyStrategy();
	
	// former

	moveFormerStrategy();

	// transport

	moveTranportStrategy();

}

void moveAllStrategly()
{
	// heal
	
	healStrategy();
	
}

void healStrategy()
{
	for (int vehicleId : data.vehicleIds)
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
			
			Location nearestMonolithLocation = getNearestMonolithLocation(vehicle->x, vehicle->y, vehicle->triad());
			
			if (isValidLocation(nearestMonolithLocation) && map_range(vehicle->x, vehicle->y, nearestMonolithLocation.x, nearestMonolithLocation.y) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
//				setMoveOrder(vehicleId, nearestMonolithLocation.x, nearestMonolithLocation.y, -1);
				setTask(vehicleId, Task(MOVE, vehicleId, nearestMonolithLocation));
				continue;
			}
			
			// find nearest base
			
			Location nearestBaseLocation = getNearestBaseLocation(vehicle->x, vehicle->y, vehicle->triad());
			
			if (isValidLocation(nearestBaseLocation) && map_range(vehicle->x, vehicle->y, nearestBaseLocation.x, nearestBaseLocation.y) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
//				setMoveOrder(vehicleId, nearestBaseLocation.x, nearestBaseLocation.y, ORDER_SENTRY_BOARD);
				setTask(vehicleId, Task(HOLD, vehicleId, nearestBaseLocation));
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
	
	data.tasks.insert({vehicle->pad_0, task});
	
}

bool hasTask(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	return (data.tasks.count(vehicle->pad_0) != 0);
	
}

Task *getTask(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	Task *task;
	
	if (data.tasks.count(vehicle->pad_0) == 0)
	{
		task = nullptr;
	}
	else
	{
		task = &(data.tasks.at(vehicle->pad_0));
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
	
	Location destination = task.getDestination();
	
	if (!isValidLocation(destination))
	{
		debug("\tinvalid destination\n");
		return;
	}
	
	// get destination tile
	
	MAP *destinationTile = getMapTile(destination);
	
	debug("\t->(%3d,%3d)\n", destination.x, destination.y);
	
	// vehicle is at destination already
	
	if (vehicle->x == destination.x && vehicle->y == destination.y)
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
	
	if (isLandRegion(vehicleTile->region) && isLandRegion(destinationTile->region) && isSameAssociation(vehicleTile, destinationTile, vehicle->faction_id))
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
		
		Location unboardLocation = getSeaTransportUnboardLocation(seaTransportVehicleId, destination);
		
		if (!isValidLocation(unboardLocation))
		{
			debug("\tcannot find unboard location\n");
			return;
		}
		
		debug("\tunboardLocation=(%3d,%3d)\n", unboardLocation.x, unboardLocation.y);
		
		// find unload location
		
		Location unloadLocation = getSeaTransportUnloadLocation(seaTransportVehicleId, destination, unboardLocation);
		
		if (!isValidLocation(unloadLocation))
		{
			debug("\tcannot find unload location\n");
			return;
		}
		
		debug("\tunloadLocation=(%3d,%3d)\n", unloadLocation.x, unloadLocation.y);
		
		if (vehicle->x == unloadLocation.x && vehicle->y == unloadLocation.y)
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
	
	int crossOceanAssociation = getCrossOceanAssociation({vehicle->x, vehicle->y}, destination, vehicle->faction_id);
	
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
		
		std::unordered_map<int, int>::iterator seaTransportRequestIterator = data.seaTransportRequests.find(crossOceanAssociation);
		
		if (seaTransportRequestIterator == data.seaTransportRequests.end())
		{
			data.seaTransportRequests.insert({crossOceanAssociation, 1});
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
	
	Location availableSeaTransportLoadLocation = getSeaTransportLoadLocation(availableSeaTransportVehicleId, vehicleId);
	
	if (!isValidLocation(availableSeaTransportLoadLocation))
		return;
	
	// add boarding tasks
	
	debug("\tadd boarding tasks: [%3d]\n", availableSeaTransportVehicleId);
	
	setTask(vehicleId, Task(BOARD, vehicleId, {-1, -1}, availableSeaTransportVehicleId));
	setTaskIfCloser(availableSeaTransportVehicleId, Task(LOAD, availableSeaTransportVehicleId, availableSeaTransportLoadLocation));
	
	return;
	
}

