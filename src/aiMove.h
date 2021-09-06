#pragma once

#include <memory>
#include "game_wtp.h"

enum ENEMY_MOVE_RETURN_VALUE
{
	EM_SYNC = 0,
	EM_DONE = 1,
};

struct TravelCost
{
	Location location;
	int travelCost;
};

// forward declaration for this method used in below struc
int getVehicleIdByPad0(int vehiclePad0);

struct Task
{
	int vehiclePad0;
	
	Task()
	{
		
	}
	
	Task(int _vehicleId)
	{
		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
	}
	
	int execute()
	{
		int vehicleId = getVehicleIdByPad0(this->vehiclePad0);
		
		if (vehicleId == -1)
		{
			debug("ERROR: Task::execute: cannot find vehicle id by pad_0.\n");
			return EM_DONE;
		}
		else
		{
			return execute(vehicleId);
		}
		
	}
	
	virtual Location getDestination() = 0;
	
	virtual int execute(int vehicleId) = 0;
	
};

struct KillTask : Task
{
	KillTask(int _vehicleId) : Task(_vehicleId)
	{
		
	}
	
	virtual int execute(int vehicleId)
	{
		debug("KillTask::execute(int vehicleId)\n");
		
		veh_kill(vehicleId);
		return EM_DONE;
		
	}
	
	virtual Location getDestination()
	{
		return Location();
		
	}
	
};

struct SkipTask : Task
{
	SkipTask(int _vehicleId) : Task(_vehicleId)
	{
		
	}
	
	virtual int execute(int vehicleId)
	{
		debug("SkipTask::execute(int vehicleId)\n");
		
		mod_veh_skip(vehicleId);
		return EM_DONE;
		
	}
	
	virtual Location getDestination()
	{
		return Location();
		
	}
	
};

struct DestinationTask : Task
{
	int destinationRadius = 0;
	
	DestinationTask(int _vehicleId) : Task(_vehicleId)
	{
		
	}
	
	virtual int executeAction(int vehicleId) = 0;
	
	virtual int execute(int vehicleId)
	{
		debug("DestinationTask::execute(int vehicleId)\n");
		
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// get location
		
		Location location = getDestination();
		
		if (!isValidLocation(location))
		{
			mod_veh_skip(vehicleId);
			return EM_DONE;
		}
		
		// at destination
		
		if (map_range(vehicle->x, vehicle->y, location.x, location.y) <= destinationRadius)
		{
			return executeAction(vehicleId);
		}
		
		// not at destination
		
		else
		{
			// move to site
			
			setMoveTo(vehicleId, location.x, location.y);
			return EM_SYNC;
			
		}
		
	}
	
};

struct TargetTask : DestinationTask
{
	int targetVehiclePad0;
	
	TargetTask(int _vehicleId, int _targetVehicleId) : DestinationTask(_vehicleId)
	{
		this->targetVehiclePad0 = Vehicles[_targetVehicleId].pad_0;
		
	}
	
	int getTargetVehicleId()
	{
		return getVehicleIdByPad0(this->targetVehiclePad0);
	}
	
	virtual int executeAction(int vehicleId)
	{
		// release vehicle and continue actions
		
		Vehicles[vehicleId].move_status = ORDER_NONE;
		return EM_SYNC;
		
	}
	
	virtual Location getDestination()
	{
		Location location;
		
		int targetVehicleId = getTargetVehicleId();
		
		if (targetVehicleId >= 0)
		{
			VEH *targetVehicle = &(Vehicles[targetVehicleId]);
			
			location.set(targetVehicle->x, targetVehicle->y);
			
		}
		
		return location;
		
	}
	
};

struct MoveTask : DestinationTask
{
	int x;
	int y;
	
	MoveTask(int _vehicleId, int _x, int _y) : DestinationTask(_vehicleId)
	{
		this->x = _x;
		this->y = _y;
		
	}
	
	virtual Location getDestination()
	{
		return Location(x, y);
		
	}
	
	virtual int executeAction(int vehicleId)
	{
		// skip turn
		
		mod_veh_skip(vehicleId);
		return EM_DONE;
		
	}
	
};

struct BuildTask : MoveTask
{
	BuildTask(int _vehicleId, int _x, int _y) : MoveTask(_vehicleId, _x, _y)
	{
		
	}
	
	virtual int executeAction(int vehicleId)
	{
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
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
	
};

struct LoadTask : TargetTask
{
	LoadTask(int _vehicleId, int _targetVehicleId) : TargetTask(_vehicleId, _targetVehicleId)
	{
		
	}
	
	virtual int executeAction(int vehicleId)
	{
		// wait
		
		mod_veh_skip(vehicleId);
		return EM_DONE;
		
	}
	
};

struct UnloadTask : MoveTask
{
	int passengerVehiclePad0;
	
	UnloadTask(int _vehicleId, int _passengerVehicleId, int _x, int _y) : MoveTask(_vehicleId, _x, _y)
	{
		this->destinationRadius = 1;
		this->passengerVehiclePad0 = Vehicles[_passengerVehicleId].pad_0;
		
	}
	
	virtual int executeAction(int vehicleId)
	{
		// wait
		
		mod_veh_skip(vehicleId);
		return EM_DONE;
		
	}
	
};

struct BoardTask : TargetTask
{
	BoardTask(int _vehicleId, int _targetVehicleId) : TargetTask(_vehicleId, _targetVehicleId)
	{
		
	}
	
	virtual int executeAction(int vehicleId)
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
	
};

struct UnboardTask : MoveTask
{
	UnboardTask(int _vehicleId, int _x, int _y) : MoveTask(_vehicleId, _x, _y)
	{
		this->destinationRadius = 1;
		
	}
	
	virtual int executeAction(int vehicleId)
	{
		// unboard
		
		setMoveTo(vehicleId, this->x, this->y);
		return EM_DONE;
		
	}
	
};

struct TerraformingTask : MoveTask
{
	int action;
	
	TerraformingTask(int _vehicleId, int _x, int _y, int _action) : MoveTask(_vehicleId, _x, _y)
	{
		this->action = _action;
		
	}
	
	virtual int executeAction(int vehicleId)
	{
		// execute terraforming action
		
		setTerraformingAction(vehicleId, this->action);
		return EM_DONE;
		
	}
	
};

void __cdecl modified_enemy_units_check(int factionId);
int __cdecl modified_enemy_move(const int vehicleId);
void moveStrategy();
int moveVehicle(const int vehicleId);
void setTask(int vehicleId, std::shared_ptr<Task> task);
bool hasTask(int vehicleId);
std::shared_ptr<Task> getTask(int vehicleId);
int executeTask(int vehicleId);
void setTaskIfCloser(int vehicleId, std::shared_ptr<Task> task);
int getVehicleIdByPad0(int vehiclePad0);
void transitVehicle(int vehicleId, std::shared_ptr<Task> task);

