#pragma once

#include <memory>
#include "game_wtp.h"
#include "aiTransport.h"

enum ENEMY_MOVE_RETURN_VALUE
{
	EM_SYNC = 0,
	EM_DONE = 1,
};

// forward declaration for this method used in below struct
int getVehicleIdByPad0(int vehiclePad0);

enum TaskType
{
	KILL,
	SKIP,
	BUILD,
	LOAD,
	BOARD,
	UNLOAD,
	UNBOARD,
	TERRAFORMING,
	ORDER,
	HOLD,
	MOVE,
};

struct Task
{
	TaskType type;
	int vehiclePad0;
	MAP *targetLocation;
	int targetVehiclePad0 = -1;
	int order = -1;
	int terraformingAction = -1;
	
	Task(TaskType _type, int _vehicleId)
	{
		this->type = _type;
		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
		
	}
	
	Task(TaskType _type, int _vehicleId, MAP *_targetLocation)
	{
		this->type = _type;
		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
		this->targetLocation = _targetLocation;
		
	}
	
	Task(TaskType _type, int _vehicleId, MAP *_targetLocation, int _targetVehicleId)
	{
		this->type = _type;
		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
		this->targetLocation = _targetLocation;
		this->targetVehiclePad0 = Vehicles[_targetVehicleId].pad_0;
		
	}
	
	Task(TaskType _type, int _vehicleId, MAP *_targetLocation, int _targetVehicleId, int _order, int _terraformingAction)
	{
		this->type = _type;
		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
		this->targetLocation = _targetLocation;
		this->targetVehiclePad0 = Vehicles[_targetVehicleId].pad_0;
		this->order = _order;
		this->terraformingAction = _terraformingAction;
		
	}
	
	MAP *getDestination()
	{
		debug("Task::getDestination()\n");
		
		if (targetLocation != nullptr)
		{
			return targetLocation;
		}
		else if (targetVehiclePad0 != -1)
		{
			int targetVehicleId = getVehicleIdByPad0(this->targetVehiclePad0);
			
			if (targetVehicleId == -1)
			{
				debug("ERROR: cannot find target vehicle id by pad_0.\n");
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
	
	int getDestinationRange()
	{
		debug("Task::getDestinationRange()\n");
		
		MAP *destination = getDestination();
		
		// no range for no destination
		
		if (destination == nullptr)
			return 0;
		
		int x = getX(destination);
		int y = getY(destination);
		
		int vehicleId = getVehicleIdByPad0(this->vehiclePad0);
		
		if (vehicleId == -1)
		{
			debug("ERROR: cannot find vehicle id by pad_0.\n");
			return 0;
		}
		
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		return map_range(vehicle->x, vehicle->y, x, y);
		
	}
	
	int execute()
	{
		debug("Task::execute()\n");
		
		int vehicleId = getVehicleIdByPad0(this->vehiclePad0);
		
		if (vehicleId == -1)
		{
			debug("ERROR: cannot find vehicle id by pad_0.\n");
			return EM_DONE;
		}
		
		return execute(vehicleId);
		
	}
	
	int execute(int vehicleId)
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
	
	int executeAction(int vehicleId)
	{
		debug("Task::executeAction(%d)\n", vehicleId);
		
		switch (type)
		{
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
			
		default:
			return EM_DONE;
			
		}
		
	}
	
	int executeKill(int vehicleId)
	{
		debug("KILL\n");
		
		veh_kill(vehicleId);
		return EM_DONE;
		
	}
	
	int executeSkip(int vehicleId)
	{
		debug("SKIP\n");
		
		mod_veh_skip(vehicleId);
		return EM_DONE;
		
	}
	
	int executeBuild(int vehicleId)
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
	
	int executeLoad(int vehicleId)
	{
		// wait
		
		mod_veh_skip(vehicleId);
		return EM_DONE;
		
	}
	
	int executeBoard(int vehicleId)
	{
		int targetVehicleId = getVehicleIdByPad0(this->targetVehiclePad0);
		
		if (targetVehicleId == -1)
		{
			veh_skip(vehicleId);
			return EM_DONE;
		}
		
		// board
		
		set_board_to(vehicleId, targetVehicleId);
		return EM_DONE;
		
	}
	
	int executeUnload(int vehicleId)
	{
		// wait
		
		mod_veh_skip(vehicleId);
		return EM_DONE;
		
	}
	
	int executeUnboard(int vehicleId)
	{
		// get destination
		
		MAP *destination = getDestination();
		
		// no destination
		
		if (destination != nullptr)
		{
			mod_veh_skip(vehicleId);
			return EM_DONE;
		}
		
		// get coordinates
			
		int x = getX(destination);
		int y = getY(destination);
		
		// unboard
		
		setMoveTo(vehicleId, x, y);
		return EM_DONE;
		
	}
	
	int executeTerraformingAction(int vehicleId)
	{
		// execute terraforming action
		
		setTerraformingAction(vehicleId, this->terraformingAction);
		return EM_DONE;
		
	}
	
	int executeOrder(int vehicleId)
	{
		// set order
		
		setVehicleOrder(vehicleId, order);
		return EM_DONE;
		
	}
	
	int executeHold(int vehicleId)
	{
		// set order
		
		setVehicleOrder(vehicleId, ORDER_HOLD);
		return EM_DONE;
		
	}
	
	int executeMove(int vehicleId)
	{
		// set order
		
		mod_veh_skip(vehicleId);
		return EM_DONE;
		
	}
	
};

void __cdecl modified_enemy_units_check(int factionId);
int __cdecl modified_enemy_move(const int vehicleId);
void moveStrategy();
void updateGlobalVariables();
void moveAllStrategy();
void healStrategy();
int moveVehicle(const int vehicleId);
void setTask(int vehicleId, Task task);
bool hasTask(int vehicleId);
Task *getTask(int vehicleId);
int executeTask(int vehicleId);
void setTaskIfCloser(int vehicleId, Task *task);
int getVehicleIdByPad0(int vehiclePad0);
void transitVehicle(int vehicleId, Task task);

