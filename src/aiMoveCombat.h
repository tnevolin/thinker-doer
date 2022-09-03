#pragma once

#include "main.h"
#include "game.h"
#include "game_wtp.h"

const int MAX_REPAIR_DISTANCE = 10;
const int MAX_PODPOP_DISTANCE = 20;
const size_t MAX_ENEMY_STACKS = 10;

const double OWN_TERRITORY_ECONOMIC_EFFECT_MULTIPLIER = 1.5;
const double ENEMY_BASE_ECONOMIC_EFFECT_MULTIPLIER = 2.0;

struct TaskPriority
{
	double priority;
	int vehicleId;
	TaskType taskType;
	MAP *destination;
	int travelTime = -1;
	// base parameters
	int baseId = -1;
	// combat parameters
	COMBAT_TYPE combatType = CT_MELEE;
	MAP *attackTarget = nullptr;
	bool destructive = true;
	double effect = 0.0;
	
	TaskPriority(double _priority, int _vehicleId, TaskType _taskType, MAP *_destination);
	TaskPriority(double _priority, int _vehicleId, TaskType _taskType, MAP *_destination, int _travelTime);
	TaskPriority(double _priority, int _vehicleId, TaskType _taskType, MAP *_destination, int _travelTime, int baseId);
	TaskPriority
	(
		double _priority,
		int _vehicleId,
		TaskType _taskType,
		MAP *_destination,
		int _travelTime,
		COMBAT_TYPE _combatType,
		MAP *_attackTarget,
		bool _destructive,
		double _effect
	);
	
};

struct ProvidedEffect
{
	double destructive = 0.0;
	double bombardment = 0.0;
	int firstDestructiveAttackTime = INT_MAX;
	std::vector<TaskPriority *> nonDestructiveTaskPriorities;
	
	void addDestructive(double effect);
	void addBombardment(double effect);
	double getCombined();
	
};

void moveCombatStrategy();
void holdBasePolice2x();
void moveBasePolice2x();
void holdBaseProtectors();
void moveBaseProtectors();
void moveCombat();
void populateMonolithPromotionTasks(std::vector<TaskPriority> &taskPriorities);
void populateRepairTasks(std::vector<TaskPriority> &taskPriorities);
void populateBasePolice2xTasks(std::vector<TaskPriority> &taskPriorities);
void populateBaseProtectionTasks(std::vector<TaskPriority> &taskPriorities);
void populateEnemyStackAttackTasks(std::vector<TaskPriority> &taskPriorities);
void populateEmptyBaseCaptureTasks(std::vector<TaskPriority> &taskPriorities);
void populateLandPodPoppingTasks(std::vector<TaskPriority> &taskPriorities);
void populateSeaPodPoppingTasks(std::vector<TaskPriority> &taskPriorities);
void selectAssemblyLocation();
void coordinateAttack();
bool isVehicleAvailable(int vehicleId, bool notAvailableInWarzone);
bool isVehicleAvailable(int vehicleId);
//void enemyMoveCombatVehicle(int vehicleId);
int getVehicleProtectionRange(int vehicleId);
bool compareTaskPriorityDescending(const TaskPriority &a, const TaskPriority &b);
MAP *findTarget(int vehicleId, MAP *destination);
double getDuelCombatCostCoefficient(int vehicleId, double effect, double enemyUnitCost);
double getBombardmentCostCoefficient(int vehicleId, double effect, double enemyUnitCost);
bool isPrimaryEffect(int vehicleId, MAP *enemyStackTile, EnemyStackInfo &enemyStackInfo, COMBAT_TYPE combatType);

