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
	TT_NONE,
	TT_KILL,
	TT_SKIP,
	TT_BUILD,
	TT_LOAD,
	TT_BOARD,
	TT_UNLOAD,
	TT_UNBOARD,
	TT_TERRAFORMING,
	TT_ORDER,
	TT_HOLD,
	TT_MOVE,
	TT_ARTIFACT_CONTRIBUTE,
};

struct Task
{
	int vehiclePad0;
	TaskType type;
	MAP *targetLocation = nullptr;
	int targetVehiclePad0 = -1;
	int order = -1;
	int terraformingAction = -1;
	
	Task(int _vehicleId, TaskType _type);
	Task(int _vehicleId, TaskType _type, MAP *_targetLocation);
	Task(int _vehicleId, TaskType _type, MAP *_targetLocation, int _targetVehicleId);
	Task(int _vehicleId, TaskType _type, MAP *_targetLocation, int _targetVehicleId, int _order, int _terraformingAction);
	int getVehicleId();
	int getTargetVehicleId();
	MAP *getDestination();
	int getDestinationRange();
	int execute();
	int execute(int vehicleId);
	int executeAction(int vehicleId);
	int executeNone(int vehicleId);
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
	int executeArtifactContribute(int vehicleId);
	
};

void setTask(int vehicleId, Task task);
bool hasTask(int vehicleId);
bool hasExecutableTask(int vehicleId);
void deleteTask(int vehicleId);
Task *getTask(int vehicleId);
int executeTask(int vehicleId);
void setTaskIfCloser(int vehicleId, Task task);

