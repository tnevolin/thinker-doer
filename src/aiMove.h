#pragma once

#include <memory>
#include "game_wtp.h"
#include "aiTransport.h"

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

/*
Compound structure to store cross ocean unload and unboard locations.
*/
struct CrossOceanDestination
{
	int crossOceanAssociation;
	int unboardContinentAssociation;
	Location unloadLocation;
	Location unboardLocation;
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
	Location targetLocation;
	int targetVehiclePad0 = -1;
	int order = -1;
	int terraformingAction = -1;
	
	Task(TaskType _type, int _vehicleId)
	{
		this->type = _type;
		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
		
	}
	
	Task(TaskType _type, int _vehicleId, Location _targetLocation)
	{
		this->type = _type;
		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
		this->targetLocation.set(_targetLocation);
		
	}
	
	Task(TaskType _type, int _vehicleId, Location _targetLocation, int _targetVehicleId)
	{
		this->type = _type;
		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
		this->targetLocation.set(_targetLocation);
		this->targetVehiclePad0 = Vehicles[_targetVehicleId].pad_0;
		
	}
	
	Task(TaskType _type, int _vehicleId, Location _targetLocation, int _targetVehicleId, int _order, int _terraformingAction)
	{
		this->type = _type;
		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
		this->targetLocation.set(_targetLocation);
		this->targetVehiclePad0 = Vehicles[_targetVehicleId].pad_0;
		this->order = _order;
		this->terraformingAction = _terraformingAction;
		
	}
	
	Location getDestination()
	{
		debug("Task::getDestination()\n");
		
		Location destination;
		
		if (isValidLocation(targetLocation))
		{
			return targetLocation;
			
		}
		else if (targetVehiclePad0 != -1)
		{
			int targetVehicleId = getVehicleIdByPad0(this->targetVehiclePad0);
			
			if (targetVehicleId == -1)
			{
				debug("ERROR: cannot find target vehicle id by pad_0.\n");
				return destination;
			}
			
			VEH *targetVehicle = &(Vehicles[targetVehicleId]);
			
			return {targetVehicle->x, targetVehicle->y};
			
		}
		else
		{
			return destination;
			
		}
			
	}
	
	int getDestinationRange()
	{
		debug("Task::getDestinationRange()\n");
		
		Location destination = getDestination();
		
		if (isValidLocation(destination))
		{
			int vehicleId = getVehicleIdByPad0(this->vehiclePad0);
			
			if (vehicleId == -1)
			{
				debug("ERROR: cannot find vehicle id by pad_0.\n");
				return 0;
			}
			
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			return map_range(vehicle->x, vehicle->y, destination.x, destination.y);
			
		}
		else
		{
			return 0;
		}
		
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
		
		Location destination = getDestination();
		
		// valid destination and not at destination
		
		if (isValidLocation(destination))
		{
			if (map_range(vehicle->x, vehicle->y, destination.x, destination.y) > 0)
			{
				// move to site
				
				debug("[%3d] (%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, destination.x, destination.y);
				
				setMoveTo(vehicleId, destination.x, destination.y);
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
		
		for (MAP* adjacentTile : getAdjacentTiles(vehicle->x, vehicle->y, true))
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
		
		Location destination = getDestination();
		
		// invalid destination
		
		if (!isValidLocation(destination))
		{
			mod_veh_skip(vehicleId);
			return EM_DONE;
			
		}
			
		// unboard
		
		setMoveTo(vehicleId, destination.x, destination.y);
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

//struct Task
//{
//	int vehiclePad0;
//	
//	Task()
//	{
//		
//	}
//	
//	Task(int _vehicleId)
//	{
//		this->vehiclePad0 = Vehicles[_vehicleId].pad_0;
//	}
//	
//	int getDestinationRange()
//	{
//		Location destination = getDestination();
//		
//		if (isValidLocation(destination))
//		{
//			int vehicleId = getVehicleIdByPad0(this->vehiclePad0);
//			
//			if (vehicleId == -1)
//			{
//				debug("ERROR: Task::getDestinationRange: cannot find vehicle id by pad_0.\n");
//				return 0;
//			}
//			
//			VEH *vehicle = &(Vehicles[vehicleId]);
//			
//			return map_range(vehicle->x, vehicle->y, destination.x, destination.y);
//			
//		}
//		else
//		{
//			return 0;
//		}
//		
//	}
//	
//	int execute()
//	{
//		int vehicleId = getVehicleIdByPad0(this->vehiclePad0);
//		
//		if (vehicleId == -1)
//		{
//			debug("ERROR: Task::execute: cannot find vehicle id by pad_0.\n");
//			return EM_DONE;
//		}
//		else
//		{
//			return execute(vehicleId);
//		}
//		
//	}
//	
//	virtual Location getDestination() = 0;
//	
//	virtual int execute(int vehicleId) = 0;
//	
//};
//
//struct KillTask : Task
//{
//	KillTask(int _vehicleId) : Task(_vehicleId)
//	{
//		
//	}
//	
//	virtual int execute(int vehicleId)
//	{
//		debug("KillTask::execute(%d)\n", vehicleId);
//		
//		veh_kill(vehicleId);
//		return EM_DONE;
//		
//	}
//	
//	virtual Location getDestination()
//	{
//		return Location();
//		
//	}
//	
//};
//
//struct SkipTask : Task
//{
//	SkipTask(int _vehicleId) : Task(_vehicleId)
//	{
//		
//	}
//	
//	virtual int execute(int vehicleId)
//	{
//		debug("SkipTask::execute(%d)\n", vehicleId);
//		
//		mod_veh_skip(vehicleId);
//		return EM_DONE;
//		
//	}
//	
//	virtual Location getDestination()
//	{
//		return Location();
//		
//	}
//	
//};
//
//struct DestinationTask : Task
//{
//	int destinationRadius = 0;
//	
//	DestinationTask(int _vehicleId) : Task(_vehicleId)
//	{
//		
//	}
//	
//	virtual int executeAction(int vehicleId) = 0;
//	
//	virtual int execute(int vehicleId)
//	{
//		debug("DestinationTask::execute(%d)\n", vehicleId);
//		
//		VEH *vehicle = &(Vehicles[vehicleId]);
//		
//		// get location
//		
//		Location location = getDestination();
//		
//		if (!isValidLocation(location))
//		{
//			mod_veh_skip(vehicleId);
//			return EM_DONE;
//		}
//		
//		// at destination
//		
//		if (map_range(vehicle->x, vehicle->y, location.x, location.y) <= destinationRadius)
//		{
//			return executeAction(vehicleId);
//		}
//		
//		// not at destination
//		
//		else
//		{
//			// move to site
//			
//			setMoveTo(vehicleId, location.x, location.y);
//			return EM_SYNC;
//			
//		}
//		
//	}
//	
//};
//
//struct TargetTask : DestinationTask
//{
//	int targetVehiclePad0;
//	
//	TargetTask(int _vehicleId, int _targetVehicleId) : DestinationTask(_vehicleId)
//	{
//		this->targetVehiclePad0 = Vehicles[_targetVehicleId].pad_0;
//		
//	}
//	
//	int getTargetVehicleId()
//	{
//		return getVehicleIdByPad0(this->targetVehiclePad0);
//	}
//	
//	virtual int executeAction(int vehicleId)
//	{
//		// release vehicle and continue actions
//		
//		Vehicles[vehicleId].move_status = ORDER_NONE;
//		return EM_SYNC;
//		
//	}
//	
//	virtual Location getDestination()
//	{
//		Location location;
//		
//		int targetVehicleId = getTargetVehicleId();
//		
//		if (targetVehicleId >= 0)
//		{
//			VEH *targetVehicle = &(Vehicles[targetVehicleId]);
//			
//			location.set(targetVehicle->x, targetVehicle->y);
//			
//		}
//		
//		return location;
//		
//	}
//	
//};
//
//struct MoveTask : DestinationTask
//{
//	int x;
//	int y;
//	
//	MoveTask(int _vehicleId, int _x, int _y) : DestinationTask(_vehicleId)
//	{
//		this->x = _x;
//		this->y = _y;
//		
//	}
//	
//	virtual Location getDestination()
//	{
//		return Location(x, y);
//		
//	}
//	
//	virtual int executeAction(int vehicleId)
//	{
//		// skip turn
//		
//		mod_veh_skip(vehicleId);
//		return EM_DONE;
//		
//	}
//	
//};
//
//struct BuildTask : MoveTask
//{
//	BuildTask(int _vehicleId, int _x, int _y) : MoveTask(_vehicleId, _x, _y)
//	{
//		
//	}
//	
//	virtual int executeAction(int vehicleId)
//	{
//		VEH *vehicle = &(Vehicles[vehicleId]);
//		MAP *vehicleTile = getVehicleMapTile(vehicleId);
//		
//		// check there is no adjacent bases
//		
//		for (MAP* adjacentTile : getAdjacentTiles(vehicle->x, vehicle->y, false))
//		{
//			// base in tile
//			
//			if (map_has_item(adjacentTile, TERRA_BASE_IN_TILE))
//			{
//				mod_veh_skip(vehicleId);
//				return EM_DONE;
//			}
//			
//		}
//		
//		// remove fungus and rocks if any before building
//		
//		vehicleTile->items &= ~((uint32_t)TERRA_FUNGUS);
//		if (map_rockiness(vehicleTile) == 2)
//		{
//			vehicleTile->val3 &= ~((byte)TILE_ROCKY);
//			vehicleTile->val3 |= (byte)TILE_ROLLING;
//		}
//		
//		// build base
//		
//		action_build(vehicleId, 0);
//		return EM_DONE;
//		
//	}
//	
//};
//
//struct LoadTask : TargetTask
//{
//	LoadTask(int _vehicleId, int _targetVehicleId) : TargetTask(_vehicleId, _targetVehicleId)
//	{
//		
//	}
//	
//	virtual Location getDestination()
//	{
//		Location location;
//		
//		int vehicleId = getVehicleIdByPad0(this->vehiclePad0);
//		
//		if (vehicleId == -1)
//		{
//			debug("ERROR: Task::getDestination: cannot find vehicle id by pad_0.\n");
//			return location;
//		}
//		
//		VEH *vehicle = &(Vehicles[vehicleId]);
//		
//		int targetVehicleId = getTargetVehicleId();
//		
//		if (targetVehicleId >= 0)
//		{
//			VEH *targetVehicle = &(Vehicles[targetVehicleId]);
//			
//			if (vehicle->triad() == TRIAD_AIR || vehicle->triad() == TRIAD_LAND)
//			{
//				// pick at same spot as target vehicle
//				
//				location.set(targetVehicle->x, targetVehicle->y);
//				
//			}
//			else
//			{
//				location = getSeaTransportLoadLocation(vehicleId, targetVehicleId);
//				
//			}
//			
//		}
//		
//		return location;
//		
//	}
//	
//	virtual int executeAction(int vehicleId)
//	{
//		// wait
//		
//		mod_veh_skip(vehicleId);
//		return EM_DONE;
//		
//	}
//	
//};
//
//struct UnloadTask : MoveTask
//{
//	int passengerVehiclePad0;
//	
//	UnloadTask(int _vehicleId, int _passengerVehicleId, int _x, int _y) : MoveTask(_vehicleId, _x, _y)
//	{
//		this->destinationRadius = 1;
//		this->passengerVehiclePad0 = Vehicles[_passengerVehicleId].pad_0;
//		
//	}
//	
//	virtual int executeAction(int vehicleId)
//	{
//		// wait
//		
//		mod_veh_skip(vehicleId);
//		return EM_DONE;
//		
//	}
//	
//};
//
//struct BoardTask : TargetTask
//{
//	BoardTask(int _vehicleId, int _targetVehicleId) : TargetTask(_vehicleId, _targetVehicleId)
//	{
//		
//	}
//	
//	virtual int executeAction(int vehicleId)
//	{
//		int targetVehicleId = getVehicleIdByPad0(this->targetVehiclePad0);
//		
//		if (targetVehicleId == -1)
//		{
//			veh_skip(vehicleId);
//			return EM_DONE;
//		}
//		
//		// board
//		
//		set_board_to(vehicleId, targetVehicleId);
//		return EM_DONE;
//		
//	}
//	
//};
//
//struct UnboardTask : MoveTask
//{
//	UnboardTask(int _vehicleId, int _x, int _y) : MoveTask(_vehicleId, _x, _y)
//	{
//		this->destinationRadius = 1;
//		
//	}
//	
//	virtual int executeAction(int vehicleId)
//	{
//		// unboard
//		
//		setMoveTo(vehicleId, this->x, this->y);
//		return EM_DONE;
//		
//	}
//	
//};
//
//struct TerraformingTask : MoveTask
//{
//	int action;
//	
//	TerraformingTask(int _vehicleId, int _x, int _y, int _action) : MoveTask(_vehicleId, _x, _y)
//	{
//		this->action = _action;
//		
//	}
//	
//	virtual int executeAction(int vehicleId)
//	{
//		// execute terraforming action
//		
//		setTerraformingAction(vehicleId, this->action);
//		return EM_DONE;
//		
//	}
//	
//};
//
//struct OrderTask : MoveTask
//{
//	int order;
//	
//	OrderTask(int _vehicleId, int _x, int _y, int _order) : MoveTask(_vehicleId, _x, _y)
//	{
//		this->order = _order;
//	}
//	
//	virtual int executeAction(int vehicleId)
//	{
//		// set order
//		
//		setVehicleOrder(vehicleId, order);
//		return EM_DONE;
//		
//	}
//	
//};
//
void __cdecl modified_enemy_units_check(int factionId);
int __cdecl modified_enemy_move(const int vehicleId);
void moveStrategy();
void moveAllStrategly();
void healStrategy();
int moveVehicle(const int vehicleId);
void setTask(int vehicleId, Task task);
bool hasTask(int vehicleId);
Task *getTask(int vehicleId);
int executeTask(int vehicleId);
void setTaskIfCloser(int vehicleId, Task *task);
int getVehicleIdByPad0(int vehiclePad0);
void transitVehicle(int vehicleId, Task task);

