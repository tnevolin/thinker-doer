#include "aiMove.h"
#include "terranx_wtp.h"
#include "ai.h"
#include "aiMoveColony.h"

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
	
	// combat

	moveCombatStrategy();

	// colony
	
	moveColonyStrategy();
	
	// former

	moveFormerStrategy();

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
		return moveFormer(vehicleId);
	}
	
	// sea transport
	
	if (isSeaTransportVehicle(vehicleId))
	{
		return moveSeaTransport(vehicleId);
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

void setTask(int vehicleId, std::shared_ptr<Task> task)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	std::unordered_map<int, std::shared_ptr<Task>>::iterator taskIterator = activeFactionInfo.tasks.find(vehicle->pad_0);
	
	if (taskIterator == activeFactionInfo.tasks.end())
	{
		activeFactionInfo.tasks.insert({vehicle->pad_0, task});
	}
	else
	{
		taskIterator->second = task;
	}
	
}

bool hasTask(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	return (activeFactionInfo.tasks.count(vehicle->pad_0) != 0);
	
}

std::shared_ptr<Task> getTask(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	std::unordered_map<int, std::shared_ptr<Task>>::iterator findIterator = activeFactionInfo.tasks.find(vehicle->pad_0);
	
	if (findIterator == activeFactionInfo.tasks.end())
	{
		return nullptr;
	}
	else
	{
		return findIterator->second;
	}
	
}

int executeTask(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	debug("executeTask\n");
	
	std::shared_ptr<Task> task = getTask(vehicleId);
	
	Location destination = task->getDestination();
	
	debug("\t(%3d,%3d)->(%3d,%3d)\n", vehicle->x, vehicle->y, destination.x, destination.y);
	
	return task->execute(vehicleId);
	
}

/*
Updates vehicle task if new task has closer destination.
*/
void setTaskIfCloser(int vehicleId, std::shared_ptr<Task> task)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// task exists
	if (hasTask(vehicleId))
	{
		// get current task
		
		std::shared_ptr<Task> currentTask = getTask(vehicleId);
		
		if (currentTask == nullptr)
			return;
		
		// get current task destination
		
		Location currentTaskDestination = currentTask->getDestination();
		
		if (!isValidLocation(currentTaskDestination))
			return;
		
		// get current task destination range
		
		int currentTaskDestinationRange = map_range(vehicle->x, vehicle->y, currentTaskDestination.x, currentTaskDestination.y);
		
		// get task destination
		
		Location taskDestination = task->getDestination();
		
		if (!isValidLocation(taskDestination))
			return;
		
		// get task destination range
		
		int taskDestinationRange = map_range(vehicle->x, vehicle->y, taskDestination.x, taskDestination.y);
		
		// update task only if given one has shorter range
		
		if (taskDestinationRange < currentTaskDestinationRange)
		{
			setTask(vehicleId, task);
		}
		
	}
	// task do not exist
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
void transitVehicle(int vehicleId, std::shared_ptr<Task> task)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	// get destination
	
	Location destination = task->getDestination();
	
	if (!isValidLocation(destination))
		return;
	
	// get destination tile
	
	MAP *destinationTile = getMapTile(destination);
	
	// sea and units do not need sophisticated transportation algorithms
	
	if (vehicle->triad() == TRIAD_SEA || vehicle->triad() == TRIAD_AIR)
	{
		setTask(vehicleId, task);
		return;
	}
	
	// land unit may need transportation
	
	// vehicle is on land same region
	
	if (isLandRegion(destinationTile->region) && vehicleTile->region == destinationTile->region)
	{
		setTask(vehicleId, task);
		return;
	}
	
	// all other cases require to cross the ocean
	
	// get cross ocean region
	
	int crossOceanRegion = getCrossOceanRegion({vehicle->x, vehicle->y}, destination);
	
	if (crossOceanRegion == -1)
		return;
	
	// find unload location
	
	Location unloadLocation = getSeaTransportUnloadLocation(crossOceanRegion, destination);
	
	if (!isValidLocation(unloadLocation))
		return;
	
	// check if vehicle is on unload location
	
	if (map_range(vehicle->x, vehicle->y, unloadLocation.x, unloadLocation.y) == 0)
	{
		// wake up vehicle
		
		setVehicleOrder(vehicleId, ORDER_NONE);
		
		// skip turn
		
		setTask(vehicleId, std::shared_ptr<Task>(new SkipTask(vehicleId)));
		
		return;
		
	}
	
	// check if vehicle is next to unload location and in ocean
	
	else if (map_range(vehicle->x, vehicle->y, unloadLocation.x, unloadLocation.y) == 1 && isOceanRegion(vehicleTile->region))
	{
		// in base
		
		if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
		{
			int seaTransportId = getSeaTransportInStack(vehicleId);
			
			// amphibious
			
			if (vehicle_has_ability(vehicleId, ABL_AMPHIBIOUS))
			{
				// wake up wehicle
				
				setVehicleOrder(vehicleId, ORDER_NONE);
				
				// step on unload location
				
				setTask(vehicleId, std::shared_ptr<Task>(new MoveTask(vehicleId, unloadLocation.x, unloadLocation.y)));
				
				return;
				
			}
			
			// there is a boat in stack
			
			else if (seaTransportId != -1)
			{
				// hold the boat
				
				setTask(seaTransportId, std::shared_ptr<Task>(new SkipTask(seaTransportId)));
				
				// wake up wehicle
				
				setVehicleOrder(vehicleId, ORDER_NONE);
				
				// step on unload location
				
				setTask(vehicleId, std::shared_ptr<Task>(new MoveTask(vehicleId, unloadLocation.x, unloadLocation.y)));
				
				return;
				
			}
			
		}
		
		// not in base
		
		else
		{
			int seaTransportId = getLandVehicleSeaTransportVehicleId(vehicleId);
			
			// on transport
			
			if (seaTransportId != -1)
			{
				// hold the boat
				
				setTask(seaTransportId, std::shared_ptr<Task>(new SkipTask(seaTransportId)));
				
				// wake up wehicle
				
				setVehicleOrder(vehicleId, ORDER_NONE);
				
				// step on unload location
				
				setTask(vehicleId, std::shared_ptr<Task>(new MoveTask(vehicleId, unloadLocation.x, unloadLocation.y)));
				
				return;
				
			}
			
		}
		
	}
	
	// get transport id
	
	int seaTransportVehicleId = getLandVehicleSeaTransportVehicleId(vehicleId);
	
	// is being transported
	
	if (seaTransportVehicleId != -1)
	{
		// add orders
		
		setTask(vehicleId, std::shared_ptr<Task>(new UnboardTask(vehicleId, unloadLocation.x, unloadLocation.y)));
		setTaskIfCloser(seaTransportVehicleId, std::shared_ptr<Task>(new UnloadTask(seaTransportVehicleId, vehicleId, unloadLocation.x, unloadLocation.y)));
		
		return;
		
	}
	
	// other cases require to freight a ferry
	
	// search for available sea transport
	
	int availableSeaTransportVehicleId = getAvailableSeaTransport(crossOceanRegion, vehicleId);
	
	if (availableSeaTransportVehicleId == -1)
	{
		debug("\tno available sea transports\n");
		
		// request a transport to be built
		
		std::unordered_map<int, int>::iterator seaTransportRequestIterator = activeFactionInfo.seaTransportRequests.find(crossOceanRegion);
		
		if (seaTransportRequestIterator == activeFactionInfo.seaTransportRequests.end())
		{
			activeFactionInfo.seaTransportRequests.insert({crossOceanRegion, 1});
		}
		else
		{
			seaTransportRequestIterator->second++;
		}
		
		// meanwhile continue with current task
		
		setTask(vehicleId, task);
		
		return;
		
	}
	
	// add boarding tasks
	
	setTask(vehicleId, std::shared_ptr<Task>(new BoardTask(vehicleId, availableSeaTransportVehicleId)));
	setTaskIfCloser(availableSeaTransportVehicleId, std::shared_ptr<Task>(new LoadTask(availableSeaTransportVehicleId, vehicleId)));
	
	return;
	
}

