#pragma once

#include <vector>
#include "engine.h"

enum ENEMY_MOVE_RETURN_VALUE
{
	EM_SYNC = 0,
	EM_DONE = 1,
};

enum TaskType
{
	TT_NONE,					//  0
	TT_KILL,					//  1
	TT_SKIP,					//  2
	TT_BUILD,					// 	3
	TT_LOAD,					//  4
	TT_BOARD,					//  5
	TT_UNLOAD,					//  6
	TT_UNBOARD,					//  7
	TT_TERRAFORM,				//  8
	TT_ORDER,					//  9
	TT_HOLD,					// 10
	TT_ALERT,					// 11
	TT_MOVE,					// 12
	TT_ARTIRFLAG_CONTRIBUTE,		// 13
	TT_MELEE_ATTACK,			// 14
	TT_LONG_RANGE_FIRE,			// 15
};

struct Task
{
	int vehiclePad0 = -1;
	TaskType type;
	MAP *destination = nullptr;
	MAP *attackTarget = nullptr;
	int order = -1;
	int terraformingAction = -1;

	Task(int _vehicleId, TaskType _type);
	Task(int _vehicleId, TaskType _type, MAP *_destination);
	Task(int _vehicleId, TaskType _type, MAP *_destination, MAP *_attackTarget);
	Task(int _vehicleId, TaskType _type, MAP *_destination, MAP *_attackTarget, int _order, int _terraformingAction);

	int getVehicleId();
	void clearDestination();
	MAP *getDestination();
	MAP *getAttackTarget();
	MAP *getGoal();
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
	int executeAlert(int vehicleId);
	int executeMove(int vehicleId);
	int executeArtifactContribute(int vehicleId);
	int executeAttack(int vehicleId);
	int executeLongRangeFire(int vehicleId);

};

/*
Holds potential tasks for the vehicle in preference order.
*/
struct TaskList
{
	std::vector<Task> tasks;

	void setTasks(const std::vector<Task> _tasks);
	std::vector<Task> &getTasks();
	void setTask(const Task _task);
	Task *getTask();
	void addTask(const Task _task);

};

void setTask(Task task);
bool hasTask(int vehicleId);
bool hasExecutableTask(int vehicleId);
void deleteTask(int vehicleId);
Task *getTask(int vehicleId);
int executeTask(int vehicleId);

