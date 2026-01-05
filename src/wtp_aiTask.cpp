
#include "wtp_aiTask.h"
#include "wtp_game.h"
#include "wtp_aiMove.h"

char const * Task::typeName(TaskType &taskType)
{
	return taskTypeNames[taskType];
}

int Task::getVehicleId()
{
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);

		if (vehicle->pad_0 == vehiclePad0)
		{
			return vehicleId;
		}

	}

	return -1;

}

void Task::clearDestination()
{
	destination = nullptr;
	attackTarget = nullptr;
}

void Task::setDestination(MAP *_destination)
{
	assert(_destination >= *MapTiles && _destination < *MapTiles + *MapAreaTiles);
	this->destination = _destination;
}

/**
Returns vehicle destination if specified.
Otherwise, current vehicle location.
*/
MAP *Task::getDestination()
{
	int vehicleId = getVehicleId();
	
	// unknown vehicle
	
	if (vehicleId == -1)
		return nullptr;
	
	// return destination if set
	
	if (destination != nullptr)
	{
		return destination;
	}
	
	// otherwise, return vehicle location
	
	return getVehicleMapTile(vehicleId);
	
}

MAP *Task::getAttackTarget()
{
	int vehicleId = getVehicleId();
	
	if (vehicleId == -1)
		return nullptr;
	
	if (attackTarget != nullptr)
	{
		return attackTarget;
	}
	else
	{
		return nullptr;
	}
	
}

int Task::getDestinationRange()
{
	// no range for no destination

	if (destination == nullptr)
		return 0;

	assert(isOnMap(destination));
	
	int x = getX(destination);
	int y = getY(destination);

	int vehicleId = getVehicleId();

	if (vehicleId == -1)
	{
		debug("Task::getDestinationRange()\n");
		debug("\tERROR: cannot find vehicle id by pad_0.\n");
		return 0;
	}

	VEH *vehicle = getVehicle(vehicleId);

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

	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	// move

	if (destination != nullptr && vehicleTile != destination)
	{
		// proceed to destination

		debug("[%4d] %s -> %s\n", vehicleId, getLocationString({vehicle->x, vehicle->y}), getLocationString(destination));

		if (isCombatVehicle(vehicleId))
		{
			setCombatMoveTo(vehicleId, destination);
		}
		else
		{
			setSafeMoveTo(vehicleId, destination);
		}

		// make sure to declare vendetta if moving into neutral base

		if (getRange(vehicleTile, destination) == 1 && getVehicleRemainingMovement(vehicleId) >= Rules->move_rate_roads && isBaseAt(destination))
		{
			int destinationOwner = destination->owner;

			if (destinationOwner != -1 && isNeutral(vehicle->faction_id, destinationOwner))
			{
				enemies_war(vehicle->faction_id, destinationOwner);
			}

		}

		return EM_SYNC;

	}

	// execute action

	return executeAction(vehicleId);

}

int Task::executeAction(int vehicleId)
{
	debug("Task::executeAction(%d)\n", vehicleId);
	
	switch (type)
	{
	case TT_NONE:
		return executeNone(vehicleId);
		break;
	
	case TT_KILL:
		return executeKill(vehicleId);
		break;
	
	case TT_SKIP:
		return executeSkip(vehicleId);
		break;
	
	case TT_BUILD:
		return executeBuild(vehicleId);
		break;
	
	case TT_LOAD:
		return executeLoad(vehicleId);
		break;
	
	case TT_BOARD:
		return executeBoard(vehicleId);
		break;
	
	case TT_UNLOAD:
		return executeUnload(vehicleId);
		break;
	
	case TT_UNBOARD:
		return executeUnboard(vehicleId);
		break;
	
	case TT_TERRAFORM:
		return executeTerraformingAction(vehicleId);
		break;
	
	case TT_ORDER:
		return executeOrder(vehicleId);
		break;
	
	case TT_HOLD:
		return executeHold(vehicleId);
		break;
	
	case TT_ALERT:
		return executeAlert(vehicleId);
		break;
	
	case TT_MOVE:
		return executeMove(vehicleId);
		break;
	
	case TT_ARTIFACT_CONTRIBUTE:
		return executeArtifactContribute(vehicleId);
		break;
	
	case TT_MELEE_ATTACK:
		return executeAttack(vehicleId);
		break;
	
	case TT_LONG_RANGE_FIRE:
		return executeLongRangeFire(vehicleId);
		break;
	
	case TT_CONVOY:
		return executeConvoy(vehicleId);
		break;
	
	default:
		return EM_DONE;
	
	}
	
}

int Task::executeNone(int)
{
	debug("Task::executeNone\n");

	return EM_SYNC;

}

int Task::executeKill(int vehicleId)
{
	debug("Task::executeKill\n");

	mod_veh_kill(vehicleId);
	return EM_DONE;

}

int Task::executeSkip(int vehicleId)
{
	debug("Task::executeSkip\n");

	mod_veh_skip(vehicleId);
	return EM_DONE;

}

int Task::executeBuild(int vehicleId)
{
	debug("Task::executeBuild\n");

	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	// check there is no adjacent bases

	for (MAP *adjacentTile : getBaseAdjacentTiles(vehicle->x, vehicle->y, true))
	{
		// base in tile

		if (map_has_item(adjacentTile, BIT_BASE_IN_TILE))
		{
			mod_veh_skip(vehicleId);
			return EM_DONE;
		}

	}

	// remove fungus and rocks if any before building

	vehicleTile->items &= ~((uint32_t)BIT_FUNGUS);
	if (map_rockiness(vehicleTile) == 2)
	{
		vehicleTile->val3 &= ~((byte)TILE_ROCKY);
		vehicleTile->val3 |= (byte)TILE_ROLLING;
	}

	// make all tiles visible around base

	for (MAP *tile : getBaseRadiusTiles(vehicle->x, vehicle->y, true))
	{
		setMapTileVisibleToFaction(tile, vehicle->faction_id);
	}

	// build base

	action_build(vehicleId, 0);

	return EM_DONE;

}

int Task::executeLoad(int seaTransportVehicleId)
{
	debug("Task::executeLoad\n");
	
	VEH *seaTransportVehicle = getVehicle(seaTransportVehicleId);
	
	// retrieve transit request
	
	std::vector<TransitRequest *> transitRequests;

	for (TransitRequest &transitRequest : aiData.transportControl.transitRequests)
	{
		if (transitRequest.getSeaTransportVehicleId() == seaTransportVehicleId)
		{
			transitRequests.push_back(&transitRequest);
		}
		
	}
	
	for (TransitRequest *transitRequest : transitRequests)
	{
		int passengerVehicleId = transitRequest->getVehicleId();
		
		if (passengerVehicleId == -1)
		{
			debug("\tpassenger is not found\n");
			return EM_DONE;
		}
		
		VEH *passengerVehicle = getVehicle(passengerVehicleId);
		
		// passenger should be adjacent to the transport
		
		int range = map_range(passengerVehicle->x, passengerVehicle->y, seaTransportVehicle->x, seaTransportVehicle->y);
		
		if (range > 1)
		{
			debug("\tpassenger range=%2d\n", range);
			return EM_DONE;
		}
		
		// board passenger
		
		debug("\tboard passenger: [%3d] %s->%s\n", passengerVehicleId, getLocationString({passengerVehicle->x, passengerVehicle->y}), getLocationString({seaTransportVehicle->x, seaTransportVehicle->y}));
		veh_put(passengerVehicleId, seaTransportVehicle->x, seaTransportVehicle->y);
		board(passengerVehicleId, seaTransportVehicleId);
		
	}
	
	mod_veh_skip(seaTransportVehicleId);
	return EM_SYNC;

}

int Task::executeBoard(int vehicleId)
{
	debug("Task::executeBoard\n");

	mod_veh_skip(vehicleId);
	
	return EM_SYNC;

}

int Task::executeUnload(int vehicleId)
{
	debug("Task::executeUnload\n");

	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	// wake up passengers and direct to unboard location

	for (UnloadRequest &unloadRequest : aiData.transportControl.getSeaTransportUnloadRequests(vehicleId))
	{
		// correct location

		if (unloadRequest.destination != vehicleTile)
			continue;

		// passenger

		int passengerVehicleId = unloadRequest.getVehicleId();

		if (passengerVehicleId == -1)
			continue;

		// verify passenger is indeed a passenger of this transport

		int vehicleTransportId = getVehicleTransportId(passengerVehicleId);

		if (vehicleTransportId != vehicleId)
			continue;

		// move to unboarding location

		executeUnboard(passengerVehicleId);

	}

	// wait

	mod_veh_skip(vehicleId);
	return EM_DONE;

}

int Task::executeUnboard(int vehicleId)
{
	setMoveTo(vehicleId, destination);
	return EM_DONE;

}

int Task::executeTerraformingAction(int vehicleId)
{
	debug("Task::executeTerraformingAction\n");

	// execute terraforming action

	setTerraformingAction(vehicleId, this->terraformingAction);
	return EM_SYNC;

}

int Task::executeOrder(int vehicleId)
{
	debug("Task::executeOrder\n");

	// set order

	setVehicleOrder(vehicleId, order);
	return EM_DONE;

}

int Task::executeHold(int vehicleId)
{
	debug("Task::executeHold\n");

	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	// set order

	setVehicleOrder(vehicleId, ORDER_HOLD);

	if (isLandArtilleryVehicle(vehicleId) && map_has_item(vehicleTile, BIT_BASE_IN_TILE))
	{
		// set land artillery on alert

		setVehicleOnAlert(vehicleId);

	}

	// complete move

	return EM_DONE;

}

int Task::executeAlert(int vehicleId)
{
	debug("Task::executeAlert\n");

	// set order

	setVehicleOnAlert(vehicleId);

	// complete move

	return EM_DONE;

}

int Task::executeMove(int vehicleId)
{
	debug("Task::executeMove\n");

	// set order

	mod_veh_skip(vehicleId);
	return EM_DONE;

}

int Task::executeArtifactContribute(int vehicleId)
{
	debug("Task::executeArtifactContribute\n");

	MAP *vechileTile = getVehicleMapTile(vehicleId);

	// in own base

	int baseId = getBaseAt(vechileTile);
	if (baseId != -1)
	{
		BASE *base = getBase(baseId);

		if (base->faction_id == aiFactionId)
		{
			// base is building project

			if (isBaseBuildingProject(baseId))
			{
				// destroy vehicle and contribute to project

				killVehicle(vehicleId);
				base->minerals_accumulated += 50;

				return EM_DONE;

			}

		}

	}

	// otherwise, skip

	mod_veh_skip(vehicleId);
	return EM_DONE;

}

int Task::executeAttack(int vehicleId)
{
	debug("Task::executeAttack\n");
	
	// check attackLocation
	
	if (attackTarget == nullptr)
	{
		mod_veh_skip(vehicleId);
		return EM_DONE;
	}
	
	assert(isOnMap(attackTarget));
	
	// get base and vehicle at attackLocation
	
	bool baseAtAttackLocation = map_has_item(attackTarget, BIT_BASE_IN_TILE);
	bool vehicleAtAttackLocation = map_has_item(attackTarget, BIT_VEH_IN_TILE);
	
	// empty base - move in
	
	if (baseAtAttackLocation && !vehicleAtAttackLocation)
	{
		setMoveTo(vehicleId, attackTarget);
		return EM_SYNC;
	}
	
	// nobody is there
	
	if (!vehicleAtAttackLocation)
	{
		mod_veh_skip(vehicleId);
		return EM_DONE;
	}
	
	// get defender
	
	int defenderVehicleId = veh_at(getX(attackTarget), getY(attackTarget));
	
	if (defenderVehicleId == -1)
		return EM_DONE;
	
	// compute battle odds accounting for hasty attack
	
	int remainingMovement = getVehicleRemainingMovement(vehicleId);
	double hastyCoefficient = std::min(1.0, (double)remainingMovement / (double)Rules->move_rate_roads);
	double battleOdds = getBattleOdds(vehicleId, defenderVehicleId, false);
	double adjustedBattleOdds = battleOdds * hastyCoefficient;
	
	// attack if good odds
	
	if (adjustedBattleOdds >= 1.25)
	{
		setMoveTo(vehicleId, attackTarget);
		return EM_SYNC;
	}
	
	// otherwise attack only if not hasty
	
	if (hastyCoefficient == 1.0)
	{
		setMoveTo(vehicleId, attackTarget);
		return EM_SYNC;
	}
	
	// otherwise skip
	
	mod_veh_skip(vehicleId);
	return EM_DONE;
	
}

int Task::executeLongRangeFire(int vehicleId)
{
	debug("Task::executeLongRangeFire\n");

	// check attackLocation

	if (attackTarget == nullptr)
	{
		mod_veh_skip(vehicleId);
		return EM_DONE;
	}

	debug("\t^ %s\n", getLocationString(attackTarget));

	// get base and vehicle at attackLocation

	bool baseAtAttackLocation = map_has_item(attackTarget, BIT_BASE_IN_TILE);
	bool vehicleAtAttackLocation = map_has_item(attackTarget, BIT_VEH_IN_TILE);

	// empty base - move in

	if (baseAtAttackLocation && !vehicleAtAttackLocation)
	{
		setMoveTo(vehicleId, attackTarget);
		return EM_SYNC;
	}

	// nobody is there

	if (!vehicleAtAttackLocation)
	{
		mod_veh_skip(vehicleId);
		return EM_DONE;
	}

	// long range fire

	longRangeFire(vehicleId, attackTarget);
	return EM_SYNC;

}

int Task::executeConvoy(int vehicleId)
{
	debug("Task::executeConvoy\n");
	
	// set order
	
	setVehicleOrder(vehicleId, ORDER_CONVOY);
	return EM_DONE;
	
}

// TaskList

void TaskList::setTasks(const std::vector<Task> _tasks)
{
	this->tasks.clear();
	this->tasks.insert(this->tasks.end(), _tasks.begin(), _tasks.end());
}
std::vector<Task> &TaskList::getTasks()
{
	return this->tasks;
}
void TaskList::setTask(Task _task)
{
	this->tasks.clear();
	this->tasks.push_back(_task);
}
Task *TaskList::getTask()
{
	return (this->tasks.size() >= 1 ? &(this->tasks.front()) : nullptr);
}
void TaskList::addTask(Task _task)
{
	this->tasks.push_back(_task);
}

// static functions

void setTask(Task task)
{
	int vehicleId = task.getVehicleId();
	VEH *vehicle = getVehicle(vehicleId);

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
	VEH *vehicle = getVehicle(vehicleId);
	return (aiData.tasks.count(vehicle->pad_0) != 0);
}

bool hasExecutableTask(int vehicleId)
{
	// no task
	
	if (!hasTask(vehicleId))
		return false;
	
	// get task
	
	Task *task = getTask(vehicleId);
	
	// special task type that is NO task
	
	if (task->type == TT_NONE)
		return false;
	
	// do not attack no longer hostile vehicles
	
	MAP *attackTarget = task->getAttackTarget();
debug(">attackTarget=%s\n", getLocationString(attackTarget));flushlog();
	if (attackTarget != nullptr)
	{
		int vehicleFactionId = veh_at(getX(attackTarget), getY(attackTarget));
		if (isVendettaStoppedWith(vehicleFactionId))
			return false;
	}
	
	return true;
	
}

void deleteTask(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);

	aiData.tasks.erase(vehicle->pad_0);

}

Task *getTask(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	
	robin_hood::unordered_flat_map<int, Task>::iterator taskIterator = aiData.tasks.find(vehicle->pad_0);
	
	return taskIterator == aiData.tasks.end() ? nullptr : &(taskIterator->second);
	
}

int executeTask(int vehicleId)
{
	debug("executeTask\n");

	Task *task = getTask(vehicleId);

	if (task == nullptr)
		return EM_DONE;
	
	return task->execute(vehicleId);

}

