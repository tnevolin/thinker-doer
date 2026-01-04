#pragma once

#include <vector>
#include <string>

#include "main.h"
#include "engine.h"

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
	TT_ARTIFACT_CONTRIBUTE,		// 13
	TT_MELEE_ATTACK,			// 14
	TT_ARTILLERY_ATTACK,		// 15
	TT_CONVOY,					// 16
};

struct Task
{
	int vehiclePad0;
	TaskType type;
	MAP *destination;
	MAP *attackTarget;
	int order;
	int terraformingAction;
	double priority = 1.0;

	Task(int _vehicleId, TaskType _type, MAP *_destination, MAP *_attackTarget, int _order, int _terraformingAction)
	: vehiclePad0(Vehs[_vehicleId].pad_0), type(_type), destination(_destination), attackTarget(_attackTarget), order(_order), terraformingAction(_terraformingAction)
	{}
	Task(int _vehicleId, TaskType _type, MAP *_destination, MAP *_attackTarget)
	: Task(_vehicleId, _type, _destination, _attackTarget, -1, -1)
	{}
	Task(int _vehicleId, TaskType _type, MAP *_destination)
	: Task(_vehicleId, _type, _destination, nullptr, -1, -1)
	{}
	Task(int _vehicleId, TaskType _type)
	: Task(_vehicleId, _type, nullptr, nullptr, -1, -1)
	{}

	char const *typeName();
	int getTaskVehicleId() const;
	VEH *getTaskVehicle() const;
	void clearDestination();
	void setDestination(MAP *_destination);
	MAP *getDestination() const;
	MAP *getAttackTarget() const;
	int getDestinationRange();
	char const *toString();

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
	int executeConvoy(int vehicleId);

};

void setTask(Task const &task);
bool hasTask(int vehicleId);
void deleteTask(int vehicleId);
Task *getTask(int vehicleId);

