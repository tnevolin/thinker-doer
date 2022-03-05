#include "aiTask.h"
#include "main.h"
#include "game_wtp.h"
#include "aiData.h"

Task::Task(TaskType _type, int _vehicleId)
{
	this->type = _type;
	this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
	
}

Task::Task(TaskType _type, int _vehicleId, MAP *_targetLocation)
{
	this->type = _type;
	this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
	this->targetLocation = _targetLocation;
	
}

Task::Task(TaskType _type, int _vehicleId, MAP *_targetLocation, int _targetVehicleId)
{
	this->type = _type;
	this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
	this->targetLocation = _targetLocation;
	this->targetVehiclePad0 = Vehicles[_targetVehicleId].pad_0;
	
}

Task::Task(TaskType _type, int _vehicleId, MAP *_targetLocation, int _targetVehicleId, int _order, int _terraformingAction)
{
	this->type = _type;
	this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
	this->targetLocation = _targetLocation;
	this->targetVehiclePad0 = Vehicles[_targetVehicleId].pad_0;
	this->order = _order;
	this->terraformingAction = _terraformingAction;
	
}

int Task::getVehicleId()
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

int Task::getTargetVehicleId()
{
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		if (vehicle->pad_0 == targetVehiclePad0)
		{
			return vehicleId;
		}
		
	}
	
	return -1;
	
}

MAP *Task::getDestination()
{
	if (targetLocation != nullptr)
	{
		return targetLocation;
	}
	else if (targetVehiclePad0 != -1)
	{
		int targetVehicleId = getTargetVehicleId();
		
		if (targetVehicleId == -1)
		{
			debug("Task::getDestination()\n");
			debug("\tERROR: cannot find target vehicle id by pad_0.\n");
			return nullptr;
		}
		
		VEH *targetVehicle = &(Vehicles[targetVehicleId]);
		
		return getMapTile(targetVehicle->x, targetVehicle->y);
		
	}
	else
	{
		return nullptr;
	}
		
}

int Task::getDestinationRange()
{
	MAP *destination = getDestination();
	
	// no range for no destination
	
	if (destination == nullptr)
		return 0;
	
	int x = getX(destination);
	int y = getY(destination);
	
	int vehicleId = getVehicleId();
	
	if (vehicleId == -1)
	{
		debug("Task::getDestinationRange()\n");
		debug("\tERROR: cannot find vehicle id by pad_0.\n");
		return 0;
	}
	
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	return map_range(vehicle->x, vehicle->y, x, y);
	
}

int Task::execute()
{
	debug("Task::execute()\n");
	
	int vehicleId = getVehicleId();
	
	if (vehicleId == -1)
	{
		debug("ERROR: cannot find vehicle id by pad_0.\n");
		return EM_DONE;
	}
	
	return execute(vehicleId);
	
}

int Task::execute(int vehicleId)
{
	debug("Task::execute(%d)\n", vehicleId);
	
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// get destination
	
	MAP *destination = getDestination();
	
	// valid destination and not at destination
	
	if (destination != nullptr)
	{
		int x = getX(destination);
		int y = getY(destination);
		
		if (map_range(vehicle->x, vehicle->y, x, y) > 0)
		{
			// move to site
			
			debug("[%3d] (%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, x, y);
			
			setMoveTo(vehicleId, x, y);
			return EM_SYNC;
			
		}
		
	}
	
	// otherwise execute action
	
	return executeAction(vehicleId);
	
}

int Task::executeAction(int vehicleId)
{
	debug("Task::executeAction(%d)\n", vehicleId);
	
	switch (type)
	{
	case NONE:
		return executeNone(vehicleId);
		break;
		
	case KILL:
		return executeKill(vehicleId);
		break;
		
	case SKIP:
		return executeSkip(vehicleId);
		break;
		
	case BUILD:
		return executeBuild(vehicleId);
		break;
		
	case LOAD:
		return executeLoad(vehicleId);
		break;
		
	case BOARD:
		return executeBoard(vehicleId);
		break;
		
	case UNLOAD:
		return executeUnload(vehicleId);
		break;
		
	case UNBOARD:
		return executeUnboard(vehicleId);
		break;
		
	case TERRAFORMING:
		return executeTerraformingAction(vehicleId);
		break;
		
	case ORDER:
		return executeOrder(vehicleId);
		break;
		
	case HOLD:
		return executeHold(vehicleId);
		break;
		
	case MOVE:
		return executeMove(vehicleId);
		break;
		
	case ARTIFACT_CONTRIBUTE:
		return executeArtifactContribute(vehicleId);
		break;
		
	default:
		return EM_DONE;
		
	}
	
}

int Task::executeNone(int)
{
	debug("SYNC\n");
	
	return EM_SYNC;
	
}

int Task::executeKill(int vehicleId)
{
	debug("KILL\n");
	
	veh_kill(vehicleId);
	return EM_DONE;
	
}

int Task::executeSkip(int vehicleId)
{
	debug("SKIP\n");
	
	mod_veh_skip(vehicleId);
	return EM_DONE;
	
}

int Task::executeBuild(int vehicleId)
{
	debug("BUILD\n");
	
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	// check there is no adjacent bases
	
	for (MAP *adjacentTile : getBaseAdjacentTiles(vehicle->x, vehicle->y, true))
	{
		// base in tile
		
		if (map_has_item(adjacentTile, TERRA_BASE_IN_TILE))
		{
			mod_veh_skip(vehicleId);
			return EM_DONE;
		}
		
	}
	
	// remove fungus and rocks if any before building
	
	vehicleTile->items &= ~((uint32_t)TERRA_FUNGUS);
	if (map_rockiness(vehicleTile) == 2)
	{
		vehicleTile->val3 &= ~((byte)TILE_ROCKY);
		vehicleTile->val3 |= (byte)TILE_ROLLING;
	}
	
	// build base
	
	action_build(vehicleId, 0);
	return EM_DONE;
	
}

int Task::executeLoad(int vehicleId)
{
	// wait
	
	mod_veh_skip(vehicleId);
	return EM_DONE;
	
}

int Task::executeBoard(int vehicleId)
{
	int targetVehicleId = getTargetVehicleId();
	
	if (targetVehicleId == -1)
	{
		mod_veh_skip(vehicleId);
		return EM_DONE;
	}
	
	// board
	
	board(vehicleId, targetVehicleId);
	return EM_DONE;
	
}

int Task::executeUnload(int vehicleId)
{
	// wake up passenger
	
	if (targetVehiclePad0 != -1)
	{
		int targetVehicleId = getTargetVehicleId();
		
		if (targetVehicleId != -1)
		{
			setVehicleOrder(targetVehicleId, ORDER_NONE);
		}
		
	}
	
	mod_veh_skip(vehicleId);
	return EM_DONE;
	
	// wait
	
	mod_veh_skip(vehicleId);
	return EM_DONE;
	
}

int Task::executeUnboard(int vehicleId)
{
	assert(vehicleId >= 0 && vehicleId < *total_num_vehicles);
	
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// unboard
	
	unboard(vehicleId);
	
	// get destination
	
	MAP *destination = getDestination();
	
	// no destination
	
	if (destination == nullptr)
	{
		mod_veh_skip(vehicleId);
		return EM_DONE;
	}
	
	// get coordinates
		
	int x = getX(destination);
	int y = getY(destination);
	
	// next action
	
	if (vehicle->x == x && vehicle->y == y)
	{
		// at location - do nothing
		
		mod_veh_skip(vehicleId);
		return EM_DONE;
		
	}
	else
	{
		// not at location - move to unboard location
		
		setMoveTo(vehicleId, x, y);
		return EM_DONE;
		
	}
	
}

int Task::executeTerraformingAction(int vehicleId)
{
	// execute terraforming action
	
	setTerraformingAction(vehicleId, this->terraformingAction);
	return EM_DONE;
	
}

int Task::executeOrder(int vehicleId)
{
	// set order
	
	setVehicleOrder(vehicleId, order);
	return EM_DONE;
	
}

int Task::executeHold(int vehicleId)
{
	// set order
	
	setVehicleOrder(vehicleId, ORDER_HOLD);
	return EM_DONE;
	
}

int Task::executeMove(int vehicleId)
{
	// set order
	
	mod_veh_skip(vehicleId);
	return EM_DONE;
	
}

int Task::executeArtifactContribute(int vehicleId)
{
	MAP *vechileTile = getVehicleMapTile(vehicleId);
	
	// in own base
	
	if (aiData.baseTiles.count(vechileTile) != 0)
	{
		int baseId = aiData.baseTiles.at(vechileTile);
		BASE *base = &(Bases[baseId]);
		
		// base is building project
		
		if (isBaseBuildingProject(baseId))
		{
			// destroy vehicle and contribute to project
			
			killVehicle(vehicleId);
			base->minerals_accumulated += 50;
			
			return EM_DONE;
			
		}
		
	}
	
	// otherwise, skip
	
	mod_veh_skip(vehicleId);
	return EM_DONE;
	
}

void setTask(int vehicleId, Task task)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	if (aiData.tasks.count(vehicle->pad_0) == 0)
	{
		aiData.tasks.insert({vehicle->pad_0, task});
	}
	else
	{
		aiData.tasks.at(vehicle->pad_0) = task;
	}
	
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
	debug("setTaskIfCloser\n");
	
	// task exists
	if (hasTask(vehicleId))
	{
		// get current task
		
		Task *currentTask = getTask(vehicleId);
		if (currentTask == nullptr)
		{
			debug("\tERROR: Current task exists but it is nullptr.\n");
			return;
		}
		
		// get target vehicle ID for current and replacing tasks
		
		int currentTaskTargetVehicleId = currentTask->getTargetVehicleId();
		if (currentTaskTargetVehicleId == -1)
		{
			debug("\tERROR: Current task targetVehicleId == -1. This should not happen for LOAD/UNLOAD task.\n");
			return;
		}
		
		int replacingTaskTargetVehicleId = task.getTargetVehicleId();
		if (replacingTaskTargetVehicleId == -1)
		{
			debug("\tERROR: Replacing task targetVehicleId == -1. This should not happen for LOAD/UNLOAD task.\n");
			return;
		}
		
		// colony has priority 1
		if (isColonyVehicle(currentTaskTargetVehicleId) && !isColonyVehicle(replacingTaskTargetVehicleId))
		{
			return;
		}
		if (!isColonyVehicle(currentTaskTargetVehicleId) && isColonyVehicle(replacingTaskTargetVehicleId))
		{
			setTask(vehicleId, task);
			return;
		}
		// former has priority 2
		if (isFormerVehicle(currentTaskTargetVehicleId) && !isFormerVehicle(replacingTaskTargetVehicleId))
		{
			return;
		}
		if (!isFormerVehicle(currentTaskTargetVehicleId) && isFormerVehicle(replacingTaskTargetVehicleId))
		{
			setTask(vehicleId, task);
			return;
		}
		
		// otherwise compare distance to destinations
		
		// get current and replacing task ranges
		
		int currentTaskDestinationRange = currentTask->getDestinationRange();
		int replacingTaskDestinationRange = task.getDestinationRange();
		
		// replace task only if given one has MUCH shorter range
		
		if (replacingTaskDestinationRange < currentTaskDestinationRange / 2)
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

