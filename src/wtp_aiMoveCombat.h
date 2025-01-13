#pragma once

#include "main.h"
#include "game.h"
#include "wtp_game.h"
#include "wtp_aiTask.h"
#include "wtp_aiData.h"

const int MAX_REPAIR_DISTANCE = 10;
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

enum CombatActionRestriction
{
	TPR_NONE,
	TPR_ONE,
	TPR_BASE,
	TPR_STACK,
};
/*
Potential combat vehicle action.
*/
struct CombatAction
{
	int vehicleId;
	double priority;
	CombatActionRestriction taskPriorityRestriction;
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
	
	CombatAction(int _vehicleId, double _priority, CombatActionRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination, double _travelTime, int _baseId, COMBAT_MODE _combatMode, MAP *_attackTarget, bool _destructive, double _effect)
	: vehicleId(_vehicleId), priority(_priority), taskPriorityRestriction(_taskPriorityRestriction), taskType(_taskType), destination(_destination), travelTime(_travelTime), baseId(_baseId), combatMode(_combatMode), attackTarget(_attackTarget), destructive(_destructive), effect(_effect)
	{}
	
	CombatAction(int _vehicleId, double _priority, CombatActionRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination)
	: CombatAction(_vehicleId, _priority, _taskPriorityRestriction, _taskType, _destination, INF, -1, CM_MELEE, nullptr, true, 0.0)
	{}
	
	CombatAction(int _vehicleId, double _priority, CombatActionRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination, double _travelTime)
	: CombatAction(_vehicleId, _priority, _taskPriorityRestriction, _taskType, _destination, _travelTime, -1, CM_MELEE, nullptr, true, 0.0)
	{}
	
	CombatAction(int _vehicleId, double _priority, CombatActionRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination, double _travelTime, int _baseId)
	: CombatAction(_vehicleId, _priority, _taskPriorityRestriction, _taskType, _destination, _travelTime, _baseId, CM_MELEE, nullptr, true, 0.0)
	{}
	
	CombatAction(int _vehicleId, double _priority, CombatActionRestriction _taskPriorityRestriction, TaskType _taskType, MAP *_destination, double _travelTime, COMBAT_MODE _combatMode, MAP *_attackTarget, bool _destructive, double _effect)
	: CombatAction(_vehicleId, _priority, _taskPriorityRestriction, _taskType, _destination, _travelTime, -1, _combatMode, _attackTarget, _destructive, _effect)
	{}
	
};

struct ProvidedEffect
{
	double destructive = 0.0;
	double bombardment = 0.0;
	double firstDestructiveAttackTime = DBL_MAX;
	std::vector<CombatAction *> nonDestructiveTaskPriorities;
	
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
void moveDefensiveProbes();
void immediateAttack();
void movePolice2x();
void movePolice();
void moveProtectors();
void moveCombat();
void populateDefensiveProbeTasks(std::vector<CombatAction> &taskPriorities);
void populateRepairTasks(std::vector<CombatAction> &taskPriorities);
void populateMonolithTasks(std::vector<CombatAction> &taskPriorities);
void populatePodPoppingTasks(std::vector<CombatAction> &taskPriorities);
void populatePolice2xTasks(std::vector<CombatAction> &taskPriorities);
void populatePoliceTasks(std::vector<CombatAction> &taskPriorities);
void populateProtectorTasks(std::vector<CombatAction> &taskPriorities);
void populateEmptyBaseCaptureTasks(std::vector<CombatAction> &taskPriorities);
void populateEnemyStackAttackTasks(std::vector<CombatAction> &taskPriorities);
void coordinateAttack();
bool isVehicleAvailable(int vehicleId, bool notAvailableInWarzone);
bool isVehicleAvailable(int vehicleId);
int getVehicleProtectionRange(int vehicleId);
bool compareCombatActionDescending(const CombatAction &a, const CombatAction &b);
double getDuelCombatCostCoefficient(int vehicleId, double effect, double enemyUnitCost);
double getBombardmentCostCoefficient(int vehicleId, double effect, double enemyUnitCost);
bool isPrimaryEffect(int vehicleId, MAP *enemyStackTile, EnemyStackInfo &enemyStackInfo, COMBAT_MODE combatMode);
bool isProtecting(int vehicleId);

