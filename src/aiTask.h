#pragma once

#include "terranx.h"
#include "terranx_types.h"
#include "terranx_enums.h"

enum ENEMY_MOVE_RETURN_VALUE
{
	EM_SYNC = 0,
	EM_DONE = 1,
};

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
	
	Task(TaskType _type, int _vehicleId);
	Task(TaskType _type, int _vehicleId, MAP *_targetLocation);
	Task(TaskType _type, int _vehicleId, MAP *_targetLocation, int _targetVehicleId);
	Task(TaskType _type, int _vehicleId, MAP *_targetLocation, int _targetVehicleId, int _order, int _terraformingAction);
	MAP *getDestination();
	int getDestinationRange();
	int execute();
	int execute(int vehicleId);
	int executeAction(int vehicleId);
	int executeKill(int vehicleId);
	int executeSkip(int vehicleId);
	int executeBuild(int vehicleId);
	int executeLoad(int vehicleId);
	int executeBoard(int vehicleId);
	int executeUnload(int vehicleId);
	int executeUnboard(int vehicleId);
	int executeTerraformingAction(int vehicleId);
	int executeOrder(int vehicleId);
	int executeHold(int vehicleId);
	int executeMove(int vehicleId);
	
};

int getVehicleIdByPad0(int vehiclePad0);
void setTask(int vehicleId, Task task);
bool hasTask(int vehicleId);
Task *getTask(int vehicleId);
int executeTask(int vehicleId);
void setTaskIfCloser(int vehicleId, Task task);
void transitVehicle(int vehicleId, Task task);

