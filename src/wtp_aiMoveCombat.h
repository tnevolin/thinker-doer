#pragma once

#include <float.h>

#include "main.h"
#include "engine.h"
#include "wtp_ai_game.h"

const int MAX_REPAIR_DISTANCE = 6;
const int MAX_PODPOP_DISTANCE = 20;

const double MIN_IMMEDIATE_ATTACK_PRIORITY = 1.0;

enum CombatRequestType
{
	CRT_POD_POPPING,
	CRT_BASE_DEFENSE,
	CRT_STACK_ATTACK,
};
/*
Request for combat operation.
*/
struct CombatRequest
{
	CombatRequestType type;
	MAP *tile;
	double gain;
	int baseId;
	EnemyStackInfo *enemyStackInfo;
};

enum TaskPriorityRestriction
{
	TPR_NONE,
	TPR_ONE,
	TPR_BASE,
	TPR_STACK,
};
/*
Potential combat vehicle action.
*/
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

struct CombatAction
{
	int vehicleId = -1;
	double gain;
	MAP *destination;
	int remainingMoves;
	ENGAGEMENT_MODE engagementMode;
	MAP *target;
	double hastyCoefficient;
	
	void setMove(int _vehicleId, double _gain, MAP *_destination, int _remainingMoves);
	void setAttack(int _vehicleId, double _gain, MAP *_destination, int _remainingMoves, ENGAGEMENT_MODE _engagementMode, MAP *_target, double _hastyCoefficient);
	
};

void moveCombatStrategy();
void moveDefensiveProbes();
void immediateAttack();
void movePolice2x();
void movePolice();
void moveInterceptors();
void moveBaseProtectors();
void moveBunkerProtectors();
void moveCombat();
void populateDefensiveProbeTasks(std::vector<TaskPriority> &taskPriorities);
void populateRepairTasks(std::vector<TaskPriority> &taskPriorities);
void populateMonolithTasks(std::vector<TaskPriority> &taskPriorities);
void populatePodPoppingTasks(std::vector<TaskPriority> &taskPriorities);
void populatePolice2xTasks(std::vector<TaskPriority> &taskPriorities);
void populatePoliceTasks(std::vector<TaskPriority> &taskPriorities);
void populateBaseProtectorTasks(std::vector<TaskPriority> &taskPriorities);
void populateBunkerProtectorTasks(std::vector<TaskPriority> &taskPriorities);
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
double getDefendGain(int defenderVehicleId, MAP *tile, double defenderHealth, int excludeEnemyVehiclePad0 = -1);
double getProximityGain(int vehicleId, MAP *destination);
double getMeleeAttackGain(int vehicleId, MAP *destination, MAP *target, double hastyCoefficient);
double getArtilleryAttackGain(int vehicleId, MAP *destination, MAP *target);
double getMutualCombatGain(double attackerDestructionGain, double defenderDestructionGain, double combatEffect);
double getBombardmentGain(double defenderDestructionGain, double relativeBombardmentDamage);
CombatAction selectVehicleCombatAction(int vehicleId);
void aiEnemyMoveCombatVehicles();

