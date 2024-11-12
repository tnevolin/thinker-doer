#pragma once

#include "main.h"
#include "game.h"
#include "wtp_game.h"
#include "wtp_aiTask.h"
#include "wtp_aiData.h"

const int MAX_REPAIR_DISTANCE = 10;
const int MAX_PODPOP_DISTANCE = 20;
const size_t MAX_ENEMY_STACKS = 10;

const double OWN_TERRITORY_ECONOMIC_EFFECT_MULTIPLIER = 1.5;
const double ENEMY_BASE_ECONOMIC_EFFECT_MULTIPLIER = 2.0;
const double MIN_IMMEDIATE_ATTACK_PRIORITY = 1.0;

enum TaskPriorityRestriction
{
	TPR_NONE,
	TPR_ONE,
	TPR_BASE,
	TPR_STACK,
};
struct TaskPriority
{
	int vehicleId;
	double priority;
	TaskPriorityRestriction taskPriorityRestriction;
	TaskType taskType;
	MAP *destination;
	double travelTime = INF;
	// protected base
	int baseId = -1;
	// combat parameters
	COMBAT_MODE combatMode = CM_MELEE;
	MAP *attackTarget = nullptr;
	bool destructive = true;
	double effect = 0.0;
	
	TaskPriority(int _vehicleId, double _priority, TaskPriorityRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination, double _travelTime, int _baseId, COMBAT_MODE _combatMode, MAP *_attackTarget, bool _destructive, double _effect)
	: vehicleId(_vehicleId), priority(_priority), taskPriorityRestriction(_taskPriorityRestriction), taskType(_taskType), destination(_destination), travelTime(_travelTime), baseId(_baseId), combatMode(_combatMode), attackTarget(_attackTarget), destructive(_destructive), effect(_effect)
	{}
	
	TaskPriority(int _vehicleId, double _priority, TaskPriorityRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination)
	: TaskPriority(_vehicleId, _priority, _taskPriorityRestriction, _taskType, _destination, INF, -1, CM_MELEE, nullptr, true, 0.0)
	{}
	
	TaskPriority(int _vehicleId, double _priority, TaskPriorityRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination, double _travelTime)
	: TaskPriority(_vehicleId, _priority, _taskPriorityRestriction, _taskType, _destination, _travelTime, -1, CM_MELEE, nullptr, true, 0.0)
	{}
	
	TaskPriority(int _vehicleId, double _priority, TaskPriorityRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination, double _travelTime, int _baseId)
	: TaskPriority(_vehicleId, _priority, _taskPriorityRestriction, _taskType, _destination, _travelTime, _baseId, CM_MELEE, nullptr, true, 0.0)
	{}
	
	TaskPriority(int _vehicleId, double _priority, TaskPriorityRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination, double _travelTime, COMBAT_MODE _combatMode, MAP *_attackTarget, bool _destructive, double _effect)
	: TaskPriority(_vehicleId, _priority, _taskPriorityRestriction, _taskType, _destination, _travelTime, -1, _combatMode, _attackTarget, _destructive, _effect)
	{}
	
};

struct ProvidedEffect
{
	double destructive = 0.0;
	double bombardment = 0.0;
	double firstDestructiveAttackTime = DBL_MAX;
	std::vector<TaskPriority *> nonDestructiveTaskPriorities;
	
	void addDestructive(double effect);
	void addBombardment(double effect);
	double getCombined();
	
};

struct AttackActionEffect
{
	int vehicleId;
	COMBAT_MODE combatMode;
	MAP *position;
	MAP *target;
	double effect;
	double priority;
};

struct EnemyStackAttackInfo
{
	EnemyStackInfo *enemyStackInfo;
	std::vector<AttackActionEffect> attackActionEffects;
	double combinedEffect;
	double superiority;
	double averageEffect;
	double averagePriority;
	
	EnemyStackAttackInfo(EnemyStackInfo *_enemyStackInfo)
	: enemyStackInfo(_enemyStackInfo)
	{}
	
};

void moveCombatStrategy();
void movePolice2x();
void movePolice();
void moveProtectors();
void immediateAttack();
void moveCombat();
void populateRepairTasks(std::vector<TaskPriority> &taskPriorities);
void populateMonolithTasks(std::vector<TaskPriority> &taskPriorities);
void populatePodPoppingTasks(std::vector<TaskPriority> &taskPriorities);
void populatePolice2xTasks(std::vector<TaskPriority> &taskPriorities);
void populatePoliceTasks(std::vector<TaskPriority> &taskPriorities);
void populateProtectorTasks(std::vector<TaskPriority> &taskPriorities);
void populateEmptyBaseCaptureTasks(std::vector<TaskPriority> &taskPriorities);
void populateEnemyStackAttackTasks(std::vector<TaskPriority> &taskPriorities);
void coordinateAttack();
bool isVehicleAvailable(int vehicleId, bool notAvailableInWarzone);
bool isVehicleAvailable(int vehicleId);
int getVehicleProtectionRange(int vehicleId);
bool compareTaskPriorityDescending(const TaskPriority &a, const TaskPriority &b);
double getDuelCombatCostCoefficient(int vehicleId, double effect, double enemyUnitCost);
double getBombardmentCostCoefficient(int vehicleId, double effect, double enemyUnitCost);
bool isPrimaryEffect(int vehicleId, MAP *enemyStackTile, EnemyStackInfo &enemyStackInfo, COMBAT_MODE combatMode);
bool isProtecting(int vehicleId);

