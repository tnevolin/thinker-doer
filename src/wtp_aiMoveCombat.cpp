#pragma GCC diagnostic ignored "-Wshadow"

#include "wtp_aiMoveCombat.h"

#include <map>

#include "wtp_aiRoute.h"
#include "wtp_aiMove.h"

// ProvidedEffect

void ProvidedEffect::addDestructive(double effect)
{
	destructive += effect;
}

void ProvidedEffect::addBombardment(double effect)
{
	bombardment += effect;
}

double ProvidedEffect::getCombined()
{
	return destructive == 0.0 ? 0.0 : destructive + bombardment;
}

// CombatAction

void CombatAction::setMove(int _vehicleId, double _gain, MAP *_destination, int _remainingMoves)
{
	this->vehicleId = _vehicleId;
	this->gain = _gain;
	this->destination = _destination;
	this->remainingMoves = _remainingMoves;
	this->target = nullptr;
	this->target = nullptr;
	this->hastyCoefficient = 1.0;
}

void CombatAction::setAttack(int _vehicleId, double _gain, MAP *_destination, int _remainingMoves, ENGAGEMENT_MODE _engagementMode, MAP *_target, double _hastyCoefficient)
{
	this->vehicleId = _vehicleId;
	this->gain = _gain;
	this->destination = _destination;
	this->remainingMoves = _remainingMoves;
	this->engagementMode = _engagementMode;
	this->target = _target;
	this->hastyCoefficient = _hastyCoefficient;
}

/*
Prepares combat orders.
*/
void moveCombatStrategy()
{
	Profiling::start("moveCombatStrategy", "moveStrategy");
	
	// compute strategy
	
	moveDefensiveProbes();
	
//	immediateAttack();
	
	movePolice2x();
	moveBaseProtectors();
	movePolice();
	moveBunkerProtectors();
	moveCombat();
//	coordinateAttack();
	
	Profiling::stop("moveCombatStrategy");
	
}

void moveDefensiveProbes()
{
	debug("moveDefensiveProbes - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	populateDefensiveProbeTasks(taskPriorities);
	
	// sort vehicle available tasks
	
	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
	
//	if (DEBUG)
//	{
//		debug("\tsortedTasks\n");
//		
//		for (TaskPriority const &taskPriority : taskPriorities)
//		{
//			debug
//			(
//				"\t\t%5.2f:"
//				" [%4d] %s -> %s"
//				"\n"
//				, taskPriority.priority
//				, taskPriority.vehicleId
//				, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
//				, getLocationString(taskPriority.destination).c_str()
//			);
//			
//		}
//		
//	}
	
	// select tasks
	
	debug("\tselected tasks\n");
	
	robin_hood::unordered_flat_map<int, TaskPriority *> vehicleAssignments;
	
	for (TaskPriority &taskPriority : taskPriorities)
	{
		// skip already assigned vehicles
		
		if (vehicleAssignments.find(taskPriority.vehicleId) != vehicleAssignments.end())
			continue;
		
		// base probe should not be yet satisfied
		
		BaseInfo &baseInfo = aiData.getBaseInfo(taskPriority.baseId);
		
		if (baseInfo.probeData.isSatisfied(false))
			continue;
		
		// assign to base
		
		baseInfo.probeData.addVehicle(taskPriority.vehicleId, false);
		vehicleAssignments.emplace(taskPriority.vehicleId, &taskPriority);
		
		debug
		(
			"\t\t%5.2f:"
			" [%4d] %s -> %s"
			" %-25s"
			" baseInfo.probeData.isSatisfied=%d"
			"\n"
			, taskPriority.priority
			, taskPriority.vehicleId
			, getLocationString(getVehicleMapTile(taskPriority.vehicleId))
			, getLocationString(taskPriority.destination)
			, getBase(taskPriority.baseId)->name
			, baseInfo.probeData.isSatisfied(false)
		);
		
	}
			
	// set tasks
	
	for (robin_hood::pair<int, TaskPriority *> vehicleAssignmentEntry : vehicleAssignments)
	{
		TaskPriority *taskPriority = vehicleAssignmentEntry.second;
		
		// set task
		
		if (!transitVehicle(Task(taskPriority->vehicleId, TT_HOLD, taskPriority->destination)))
			continue;
		
	}
	
}

/**
Analyze exact one turn movement/attack conditions.
*/
void immediateAttack()
{
	Profiling::start("immediateAttack", "moveCombatStrategy");
	
	debug("immediateAttack - %s\n", aiMFaction->noun_faction);
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		bool melee = isMeleeVehicle(vehicleId);
		bool artillery = isArtilleryVehicle(vehicleId);
		int triad = vehicle->triad();
		int speed = getVehicleSpeed(vehicleId);
		int moveRange = (triad == TRIAD_LAND ? Rules->move_rate_roads : 1) * speed;
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// exclude harmless
		
		if (!(melee || artillery))
			continue;
		
		int meleeAttackRange = melee ? moveRange : 0;
		int artilleryAttackRange = artillery ? moveRange + (Rules->artillery_max_rng - 1) : 0;
		
		// collect attackable targets
		
		robin_hood::unordered_flat_set<MAP const *> meleeAttackTargets;
		robin_hood::unordered_flat_set<MAP const *> artilleryAttackTargets;
		
		for (int enemyBaseId : aiData.emptyEnemyBaseIds)
		{
			BASE *enemyBase = &Bases[enemyBaseId];
			MAP *enemyBaseTile = getBaseMapTile(enemyBaseId);
			
			if (isFriendly(aiFactionId, enemyBase->faction_id))
				continue;
			
			int range = map_range(vehicle->x, vehicle->y, enemyBase->x, enemyBase->y);
			
			if (range <= meleeAttackRange && isVehicleCanCaptureBase(vehicleId, enemyBaseTile))
			{
				meleeAttackTargets.insert(enemyBaseTile);
			}
			
		}
		
		for (robin_hood::pair<MAP *, EnemyStackInfo> const &enemyStackEntry : aiData.enemyStacks)
		{
			MAP const *enemyStackTile = enemyStackEntry.first;
			EnemyStackInfo const &enemyStackInfo = enemyStackEntry.second;
			
			if (!enemyStackInfo.hostile)
				continue;
			
			int range = getRange(vehicleTile, enemyStackTile);
			
			if
			(range < meleeAttackRange && enemyStackInfo.isUnitCanMeleeAttackStack(vehicle->unit_id, nullptr))
			{
				meleeAttackTargets.insert(enemyStackTile);
			}
			if (range < artilleryAttackRange && enemyStackInfo.isUnitCanArtilleryAttackStack(vehicle->unit_id, nullptr))
			{
				artilleryAttackTargets.insert(enemyStackTile);
			}
			
		}
		
		// select best attack priority for each vehicle
		
		std::vector<AttackAction> meleeAttackActions = getMeleeAttackActions(vehicleId, false, true);
		std::vector<AttackAction> artilleryAttackActions = getArtilleryAttackActions(vehicleId, false, true);
		
		bool selected = false;
		TaskType selectedTaskType;
		MAP *selectedPosition;
		MAP *selectedTarget;
		double selectedPriority = 0.0;
		
		for (AttackAction const &attackAction : meleeAttackActions)
		{
			TaskType taskType = TT_MELEE_ATTACK;
			
			// empty enemy base
			
			if (aiData.isEmptyEnemyBaseAt(attackAction.target))
			{
				selected = true;
				selectedTaskType = TT_MOVE;
				selectedPosition = attackAction.target;
				selectedTarget = nullptr;
				selectedPriority = 100.0;
			}
			
			// enemy stack
			
			else if (aiData.isEnemyStackAt(attackAction.target))
			{
				EnemyStackInfo const &enemyStackInfo = aiData.getEnemyStackInfo(attackAction.target);
				
				COMBAT_MODE combatMode = CM_MELEE;
				
				double combatEffect = attackAction.hastyCoefficient * enemyStackInfo.getVehicleOffenseEffect(vehicleId, combatMode);
				double winningProbability = getWinningProbability(combatEffect);
				double priority = enemyStackInfo.destructionGain * winningProbability;
				
				if (priority > selectedPriority)
				{
					selected = true;
					selectedTaskType = taskType;
					selectedPosition = attackAction.destination;
					selectedTarget = attackAction.target;
					selectedPriority = priority;
				}
				
			}
			
//			debug
//			(
//				"\t\tmelee -> %s => %s"
//				" effect=%5.2f"
//				" priority=%5.2f"
//				"\n"
//				, getLocationString(attackAction.position).c_str(), getLocationString(attackAction.target).c_str()
//				, effect
//				, priority
//			);
			
		}
		
		for (AttackAction const &attackAction : artilleryAttackActions)
		{
			TaskType taskType = TT_ARTILLERY_ATTACK;
			
			// enemy stack
			
			if (aiData.isEnemyStackAt(attackAction.target))
			{
				EnemyStackInfo const &enemyStackInfo = aiData.getEnemyStackInfo(attackAction.target);
				
				double combatEffect;
				double winningProbability;
				double priority;
				
				if (enemyStackInfo.artillery)
				{
					COMBAT_MODE combatMode = CM_ARTILLERY_DUEL;
					combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, combatMode);
					winningProbability = getWinningProbability(combatEffect);
					priority = enemyStackInfo.destructionGain * winningProbability;
				}
				else if (enemyStackInfo.bombardment)
				{
					COMBAT_MODE combatMode = CM_BOMBARDMENT;
					combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, combatMode) + 1.0;
					winningProbability = getWinningProbability(combatEffect);
					priority = enemyStackInfo.destructionGain * winningProbability;
				}
				else
				{
					continue;
				}
				
				if (priority > selectedPriority)
				{
					selected = true;
					selectedTaskType = taskType;
					selectedPosition = attackAction.destination;
					selectedTarget = attackAction.target;
					selectedPriority = priority;
				}
				
				debug
				(
					"\t\tartyl -> %s => %s"
					" combatEffect=%5.2f"
					" winningProbability=%5.2f"
					" priority=%5.2f"
					"\n"
					, getLocationString(attackAction.destination), getLocationString(attackAction.target)
					, combatEffect
					, winningProbability
					, priority
				);
				
			}
				
		}
		
		if (!selected)
			continue;
		
		if (selectedPriority < MIN_IMMEDIATE_ATTACK_PRIORITY)
			continue;
		
		// set task
		
		setTask(Task(vehicleId, selectedTaskType, selectedPosition, selectedTarget));
		
		debug
		(
			"\t[%4d] %s %-32s"
			"%-5s"
			" -> %s"
			" => %s"
			" priority=%5.2f"
			"\n"
			, vehicleId, getLocationString(getVehicleMapTile(vehicleId)), vehicle->name()
			, Task::typeName(selectedTaskType)
			, getLocationString(selectedPosition)
			, getLocationString(selectedTarget)
			, selectedPriority
		);
		
	}
	
	Profiling::stop("immediateAttack");
	
}

void movePolice2x()
{
	debug("movePolice2x - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	populatePolice2xTasks(taskPriorities);
	
	// sort vehicle available tasks
	
	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
	
//	if (DEBUG)
//	{
//		debug("\tsortedTasks\n");
//		
//		for (TaskPriority const &taskPriority : taskPriorities)
//		{
//			debug
//			(
//				"\t\t%5.2f:"
//				" [%4d] %s -> %s"
//				"\n"
//				, taskPriority.priority
//				, taskPriority.vehicleId
//				, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
//				, getLocationString(taskPriority.destination).c_str()
//			);
//			
//		}
//		
//	}
	
	// select tasks
	
	debug("\tselected tasks\n");
	
	robin_hood::unordered_flat_map<int, TaskPriority *> vehicleAssignments;
	
	for (TaskPriority &taskPriority : taskPriorities)
	{
		// skip already assigned vehicles
		
		if (vehicleAssignments.find(taskPriority.vehicleId) != vehicleAssignments.end())
			continue;
		
		// base police should not be yet satisfied
		
		BaseInfo &baseInfo = aiData.getBaseInfo(taskPriority.baseId);
		
		if (baseInfo.policeData.isSatisfied(1))
			continue;
		
		// assign to base
		
		baseInfo.addProtector(taskPriority.vehicleId);
		vehicleAssignments.insert({taskPriority.vehicleId, &taskPriority});
		
		debug
		(
			"\t\t%5.2f:"
			" [%4d] %s -> %s"
			" %-25s"
			" baseInfo.isSatisfied(1)=%d"
			"\n"
			, taskPriority.priority
			, taskPriority.vehicleId
			, getLocationString(getVehicleMapTile(taskPriority.vehicleId))
			, getLocationString(taskPriority.destination)
			, getBase(taskPriority.baseId)->name
			, baseInfo.isSatisfied(1)
		);
		
	}
	
	// set tasks
	
	for (robin_hood::pair<int, TaskPriority *> vehicleAssignmentEntry : vehicleAssignments)
	{
		TaskPriority *taskPriority = vehicleAssignmentEntry.second;
		
		// set task
		
		if (!transitVehicle(Task(taskPriority->vehicleId, TT_HOLD, taskPriority->destination)))
			continue;
		
	}
	
}

void movePolice()
{
	debug("movePolice - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	populatePoliceTasks(taskPriorities);
	
	// sort vehicle available tasks
	
	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
	
//	if (DEBUG)
//	{
//		debug("\tsortedTasks\n");
//		
//		for (TaskPriority const &taskPriority : taskPriorities)
//		{
//			debug
//			(
//				"\t\t%5.2f:"
//				" [%4d] %s -> %s"
//				"\n"
//				, taskPriority.priority
//				, taskPriority.vehicleId
//				, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
//				, getLocationString(taskPriority.destination).c_str()
//			);
//			
//		}
//		
//	}
	
	// select tasks
	
	debug("\tselected tasks\n");
	
	robin_hood::unordered_flat_map<int, TaskPriority *> vehicleAssignments;
	
	for (TaskPriority &taskPriority : taskPriorities)
	{
		// skip already assigned vehicles
		
		if (vehicleAssignments.find(taskPriority.vehicleId) != vehicleAssignments.end())
			continue;
		
		// base police should not be yet satisfied
		
		BaseInfo &baseInfo = aiData.getBaseInfo(taskPriority.baseId);
		
		if (baseInfo.policeData.isSatisfied(0))
			continue;
		
		// assign to base
		
		baseInfo.addProtector(taskPriority.vehicleId);
		vehicleAssignments.insert({taskPriority.vehicleId, &taskPriority});
		
		debug
		(
			"\t\t%5.2f:"
			" [%4d] %s -> %s"
			" %-25s"
			" baseInfo.isSatisfied(0)=%d"
			"\n"
			, taskPriority.priority
			, taskPriority.vehicleId
			, getLocationString(getVehicleMapTile(taskPriority.vehicleId))
			, getLocationString(taskPriority.destination)
			, getBase(taskPriority.baseId)->name
			, baseInfo.isSatisfied(0)
		);
		
	}
	
	// set tasks
	
	for (robin_hood::pair<int, TaskPriority *> vehicleAssignmentEntry : vehicleAssignments)
	{
		TaskPriority *taskPriority = vehicleAssignmentEntry.second;
		
		// set task
		
		if (!transitVehicle(Task(taskPriority->vehicleId, TT_HOLD, taskPriority->destination)))
			continue;
		
	}
	
}

void moveInterceptors()
{
//	debug("moveInterceptors - %s\n", MFactions[aiFactionId].noun_faction);
//	
//	// populate tasks
//	
//	std::vector<TaskPriority> taskPriorities;
//	populateBaseProtectorTasks(taskPriorities);
//	
//	// sort vehicle available tasks
//	
//	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
//	
//	if (DEBUG)
//	{
//		debug("\tsortedTasks\n");
//		
//		for (TaskPriority const &taskPriority : taskPriorities)
//		{
//			debug
//			(
//				"\t\t%5.2f:"
//				" [%4d] %s -> %s"
//				"\n"
//				, taskPriority.priority
//				, taskPriority.vehicleId
//				, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
//				, getLocationString(taskPriority.destination).c_str()
//			);
//			
//		}
//		
//	}
//	
//	// select tasks
//	
//	debug("\tselected tasks\n");
//	
//	robin_hood::unordered_flat_map<int, TaskPriority *> vehicleAssignments;
//	
//	for (TaskPriority &taskPriority : taskPriorities)
//	{
//		// skip already assigned vehicles
//		
//		if (vehicleAssignments.find(taskPriority.vehicleId) != vehicleAssignments.end())
//			continue;
//		
//		// base police and protection should not be yet satisfied
//		
//		BaseInfo &baseInfo = aiData.getBaseInfo(taskPriority.baseId);
//		
//		if (baseInfo.isSatisfied(taskPriority.vehicleId, false))
//			continue;
//		
//		// assign to base
//		
//		baseInfo.addProtector(taskPriority.vehicleId);
//		vehicleAssignments.insert({taskPriority.vehicleId, &taskPriority});
//		
//		debug
//		(
//			"\t\t%5.2f:"
//			" [%4d] %s -> %s"
//			" %-25s"
//			" baseInfo.isSatisfied=%d"
//			" baseInfo.combatData.isSatisfied= %d %d"
//			"\n"
//			, taskPriority.priority
//			, taskPriority.vehicleId
//			, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
//			, getLocationString(taskPriority.destination).c_str()
//			, getBase(taskPriority.baseId)->name
//			, baseInfo.isSatisfied(taskPriority.vehicleId, false)
//			, baseInfo.combatData.isSatisfied(false), baseInfo.combatData.isSatisfied(true)
//		);
//		
//	}
//			
//	// set tasks
//	
//	for (robin_hood::pair<int, TaskPriority *> vehicleAssignmentEntry : vehicleAssignments)
//	{
//		TaskPriority *taskPriority = vehicleAssignmentEntry.second;
//		
//		// set task
//		
//		if (!transitVehicle(Task(taskPriority->vehicleId, TT_HOLD, taskPriority->destination)))
//			continue;
//		
//	}
//	
//	// hold base current protectors until future protectors arrive
//	
//	debug("\thold current protectors\n");
//	
//	for (int baseId : aiData.baseIds)
//	{
//		MAP *baseTile = getBaseMapTile(baseId);
//		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
//		
//		if (!baseInfo.combatData.isSatisfied(true))
//		{
//			debug("\t\t%-25s\n", Bases[baseId].name)
//			
//			for (int vehicleId : baseInfo.combatData.garrison)
//			{
//				// infantry defensive
//				
//				if (!isInfantryDefensiveVehicle(vehicleId))
//					continue;
//				
//				// not yet assigned to this base
//				
//				robin_hood::unordered_flat_map<int, TaskPriority *>::iterator vehicleAssignmentIterator = vehicleAssignments.find(vehicleId);
//				if (vehicleAssignmentIterator != vehicleAssignments.end() && vehicleAssignmentIterator->second->destination == baseTile)
//					continue;
//				
//				// assign to base
//				
//				if (!transitVehicle(Task(vehicleId, TT_HOLD, baseTile)))
//					continue;
//				
//				// add base protection
//				
//				baseInfo.combatData.addVehicle(vehicleId, true);
//				
//				debug("\t\t\t[%4d] %-32s baseInfo.combatData.isSatisfied(true) = %d\n", vehicleId, Vehs[vehicleId].name(), baseInfo.combatData.isSatisfied(true))
//				if (baseInfo.combatData.isSatisfied(true))
//					break;
//				
//			}
//			
//		}
//		
//	}
//	
}

void moveBaseProtectors()
{
	debug("moveBaseProtectors - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	populateBaseProtectorTasks(taskPriorities);
	
	if (DEBUG)
	{
		debug("\tsortedTasks\n");
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			debug
			(
				"\t\t%5.2f:"
				" [%4d] %s -> %s"
				"\n"
				, taskPriority.priority
				, taskPriority.vehicleId
				, getLocationString(getVehicleMapTile(taskPriority.vehicleId))
				, getLocationString(taskPriority.destination)
			);
			
		}
		
	}
	
	// select tasks
	
	debug("\tselected tasks\n");
	
	robin_hood::unordered_flat_map<int, TaskPriority *> vehicleAssignments;
	
	while (true)
	{
		// reevaluate dynamicPriorities
		
		TaskPriority *bestTaskPriority = nullptr;
		double bestPriority = 0.0;
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			// skip already assigned vehicles
			
			if (vehicleAssignments.find(taskPriority.vehicleId) != vehicleAssignments.end())
				continue;
			
			// base police and protection should not be yet both satisfied
			
			BaseInfo &baseInfo = aiData.getBaseInfo(taskPriority.baseId);
			
			if (baseInfo.isSatisfied(0))
				continue;
			
			// compute priority
			
			double combatEffect = baseInfo.combatData.getProtectorVehicleEffect(taskPriority.vehicleId);
			double winningProbability = getWinningProbability(combatEffect);
			double priority = taskPriority.priority * winningProbability;
			
			if (priority > bestPriority)
			{
				bestTaskPriority = &taskPriority;
				bestPriority = priority;
			}
			
		}
		
		if (bestTaskPriority == nullptr)
			break;
		
		// assign to base
		
		BaseInfo &baseInfo = aiData.getBaseInfo(bestTaskPriority->baseId);
		
		baseInfo.addProtector(bestTaskPriority->vehicleId);
		vehicleAssignments.insert({bestTaskPriority->vehicleId, bestTaskPriority});
		
		debug
		(
			"\t\t%5.2f:"
			" [%4d] %s -> %s"
			" %-25s"
			" baseInfo.isSatisfied(0)=%d"
			" baseInfo.combatData.isSufficientProtect()=%d"
			"\n"
			, bestTaskPriority->priority
			, bestTaskPriority->vehicleId
			, getLocationString(getVehicleMapTile(bestTaskPriority->vehicleId))
			, getLocationString(bestTaskPriority->destination)
			, getBase(bestTaskPriority->baseId)->name
			, baseInfo.isSatisfied(0)
			, baseInfo.combatData.isSufficientProtect()
		);
		
	}
			
	// set tasks
	
	for (robin_hood::pair<int, TaskPriority *> vehicleAssignmentEntry : vehicleAssignments)
	{
		TaskPriority *taskPriority = vehicleAssignmentEntry.second;
		
		// set task
		
		if (!transitVehicle(Task(taskPriority->vehicleId, TT_HOLD, taskPriority->destination)))
			continue;
		
	}
	
	// hold base current protectors until future protectors arrive
	
	debug("\thold current protectors\n");
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
		
		if (!baseInfo.combatData.isSufficientProtect())
		{
			debug("\t\t%-25s\n", Bases[baseId].name)
			
			for (int vehicleId : baseTileInfo.playerVehicleIds)
			{
				// infantry defensive
				
				if (!isInfantryDefensiveVehicle(vehicleId))
					continue;
				
				// not yet assigned to this base
				
				robin_hood::unordered_flat_map<int, TaskPriority *>::iterator vehicleAssignmentIterator = vehicleAssignments.find(vehicleId);
				if (vehicleAssignmentIterator != vehicleAssignments.end() && vehicleAssignmentIterator->second->destination == baseTile)
					continue;
				
				// assign to base
				
				if (!transitVehicle(Task(vehicleId, TT_HOLD, baseTile)))
					continue;
				
				// add base protection
				
				baseInfo.combatData.addProtector(vehicleId);
				
				debug("\t\t\t[%4d] %-32s baseInfo.combatData.isSufficientProtect() = %d\n", vehicleId, Vehs[vehicleId].name(), baseInfo.combatData.isSufficientProtect())
				if (baseInfo.combatData.isSufficientProtect())
					break;
				
			}
			
		}
		
	}
	
}

void moveBunkerProtectors()
{
	debug("moveBunkerProtectors - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	populateBunkerProtectorTasks(taskPriorities);
	
	if (DEBUG)
	{
		debug("\tsortedTasks\n");
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			debug
			(
				"\t\t%5.2f:"
				" [%4d] %s -> %s"
				"\n"
				, taskPriority.priority
				, taskPriority.vehicleId
				, getLocationString(getVehicleMapTile(taskPriority.vehicleId))
				, getLocationString(taskPriority.destination)
			);
			
		}
		
	}
	
	// select tasks
	
	debug("\tselected tasks\n");
	
	robin_hood::unordered_flat_map<int, TaskPriority *> vehicleAssignments;
	
	while (true)
	{
		// reevaluate dynamicPriorities
		
		TaskPriority *bestTaskPriority = nullptr;
		double bestPriority = 0.0;
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			// skip already assigned vehicles
			
			if (vehicleAssignments.find(taskPriority.vehicleId) != vehicleAssignments.end())
				continue;
			
			// bunker protection should not be yet satisfied
			
			CombatData &combatData = aiData.bunkerCombatDatas.at(taskPriority.destination);
			
			if (combatData.isSufficientProtect())
				continue;
			
			// compute priority
			
			double combatEffect = combatData.getProtectorVehicleEffect(taskPriority.vehicleId);
			double winningProbability = getWinningProbability(combatEffect);
			double priority = taskPriority.priority * winningProbability;
			
			if (priority > bestPriority)
			{
				bestTaskPriority = &taskPriority;
				bestPriority = priority;
			}
			
		}
		
		if (bestTaskPriority == nullptr)
			break;
		
		// assign to target
		
		CombatData &combatData = aiData.bunkerCombatDatas.at(bestTaskPriority->destination);
		
		combatData.addProtector(bestTaskPriority->vehicleId);
		vehicleAssignments.insert({bestTaskPriority->vehicleId, bestTaskPriority});
		
		debug
		(
			"\t\t%5.2f:"
			" [%4d] %s -> %s"
			" %-25s"
			" combatData.isSufficientProtect()=%d"
			"\n"
			, bestTaskPriority->priority
			, bestTaskPriority->vehicleId
			, getLocationString(getVehicleMapTile(bestTaskPriority->vehicleId))
			, getLocationString(bestTaskPriority->destination)
			, getBase(bestTaskPriority->baseId)->name
			, combatData.isSufficientProtect()
		);
		
	}
			
	// set tasks
	
	for (robin_hood::pair<int, TaskPriority *> vehicleAssignmentEntry : vehicleAssignments)
	{
		TaskPriority *taskPriority = vehicleAssignmentEntry.second;
		
		// set task
		
		if (!transitVehicle(Task(taskPriority->vehicleId, TT_HOLD, taskPriority->destination)))
			continue;
		
	}
	
	// hold bunker current protectors until future protectors arrive
	
	debug("\thold current bunker protectors\n");
	
	for (robin_hood::pair<MAP *, CombatData> &bunkerCombatDataEntry : aiData.bunkerCombatDatas)
	{
		MAP *bunkerTile = bunkerCombatDataEntry.first;
		CombatData &bunkerCombatData = bunkerCombatDataEntry.second;
		TileInfo &bunkerTileInfo = aiData.getTileInfo(bunkerTile);
		
		if (!bunkerCombatData.isSufficientProtect())
		{
			debug("\t\t%s\n", getLocationString(bunkerTile))
			
			for (int vehicleId : bunkerTileInfo.playerVehicleIds)
			{
				// infantry defensive
				
				if (!isInfantryDefensiveVehicle(vehicleId))
					continue;
				
				// not yet assigned to this bunker
				
				robin_hood::unordered_flat_map<int, TaskPriority *>::iterator vehicleAssignmentIterator = vehicleAssignments.find(vehicleId);
				if (vehicleAssignmentIterator != vehicleAssignments.end() && vehicleAssignmentIterator->second->destination == bunkerTile)
					continue;
				
				// assign to bunker
				
				if (!transitVehicle(Task(vehicleId, TT_HOLD, bunkerTile)))
					continue;
				
				// add bunker protection
				
				bunkerCombatData.addProtector(vehicleId);
				
				debug("\t\t\t[%4d] %-32s bunkerCombatData.isSufficientProtect() = %d\n", vehicleId, Vehs[vehicleId].name(), bunkerCombatData.isSufficientProtect())
				
				if (bunkerCombatData.isSufficientProtect())
					break;
				
			}
			
		}
		
	}
	
}

void moveCombat()
{
	debug("moveCombat - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	
	populateRepairTasks(taskPriorities);
	populateMonolithTasks(taskPriorities);
	populatePodPoppingTasks(taskPriorities);
	populateEmptyBaseCaptureTasks(taskPriorities);
	populateEnemyStackAttackTasks(taskPriorities);
	
	// sort available tasks
	
	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
	
//	if (DEBUG)
//	{
//		debug("\tsortedTasks\n");
//		
//		for (TaskPriority &taskPriority : taskPriorities)
//		{
//			debug
//			(
//				"\t\t%5.2f:"
//				" [%4d] %s"
//				" -> %s"
//				" => %s"
//				"\n"
//				, taskPriority.priority
//				, taskPriority.vehicleId
//				, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
//				, getLocationString(taskPriority.destination).c_str()
//				, getLocationString(taskPriority.attackTarget).c_str()
//			);
//			
//		}
//		
//	}
	
	// select tasks
	
	debug("\tselected tasks\n");
	
	robin_hood::unordered_flat_set<EnemyStackInfo *> excludedEnemyStacks;
	robin_hood::unordered_flat_map<int, TaskPriority *> vehicleAssignments;
	
	robin_hood::unordered_flat_set<MAP *> targetedLocations;
	robin_hood::unordered_flat_set<EnemyStackInfo *> targetedEnemyStacks;
	
	while (true)
	{
		// clear assignments
		
		vehicleAssignments.clear();
		
		targetedLocations.clear();
		targetedEnemyStacks.clear();
		
		for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackInfoEntry : aiData.enemyStacks)
		{
			EnemyStackInfo &enemyStackInfo = enemyStackInfoEntry.second;
			enemyStackInfo.resetAttackParameters();
		}
		
		// iterate task priorities
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			// skip already assigned vehicles
			
			if (vehicleAssignments.find(taskPriority.vehicleId) != vehicleAssignments.end())
				continue;
			
			switch (taskPriority.taskPriorityRestriction)
			{
			case TPR_NONE:
				{
					// set vehicle task
					
					vehicleAssignments.emplace(taskPriority.vehicleId, &taskPriority);
					
				}
				break;
				
			case TPR_ONE:
				{
					// skip already targeted location
					
					if (targetedLocations.find(taskPriority.destination) != targetedLocations.end())
						continue;
					
					// set vehicle task
					
					vehicleAssignments.emplace(taskPriority.vehicleId, &taskPriority);
					
					// add targeted location
					
					targetedLocations.insert(taskPriority.destination);
					
				}
				break;
				
			case TPR_STACK:
				{
					EnemyStackInfo *enemyStack = &(aiData.enemyStacks.at(taskPriority.attackTarget));
					
					// skip excluded stack
					
					if (excludedEnemyStacks.find(enemyStack) != excludedEnemyStacks.end())
						continue;
					
					// try to add vehicle to stack attackers
					
					if (!enemyStack->addAttacker({taskPriority.vehicleId, taskPriority.travelTime}))
						continue;
					
					// set vehicle task
					
					vehicleAssignments.emplace(taskPriority.vehicleId, &taskPriority);
					
					// add targeted enemy stack
					
					targetedEnemyStacks.insert(enemyStack);
					
				}
				break;
				
			default:
				// no action
				break;
				
			}
			
//			if (DEBUG)
//			{
//				debug
//				(
//					"\t\t[%4d]"
//					" %s"
//					" %s"
//					" -> %s"
//					" => %s"
//					" combatMode=%d"
//					" destructive=%d"
//					" effect=%5.2f"
//					"\n"
//					, taskPriority.vehicleId
//					, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
//					, Task::typeName(taskPriority.taskType).c_str()
//					, getLocationString(taskPriority.destination).c_str()
//					, getLocationString(taskPriority.attackTarget).c_str()
//					, taskPriority.combatMode
//					, taskPriority.destructive
//					, taskPriority.effect
//				);
//			}
			
		}
		
		// compute targetedEnemyStacks attack parameters
		
		for (EnemyStackInfo *enemyStack : targetedEnemyStacks)
		{
			enemyStack->computeAttackParameters();
		}
			
		// find most undertargeted enemy stacks
		
		debug("\tundertargeted stacks\n");
		
		EnemyStackInfo *mostUndertargetedEnemyStack = nullptr;
		double mostUndertargetedEnemyStackDestructionRatio = DBL_MAX;
		
		for (EnemyStackInfo *enemyStack : targetedEnemyStacks)
		{
			// exclude excluded
			
			if (excludedEnemyStacks.find(enemyStack) != excludedEnemyStacks.end())
				continue;
			
			// always attack alien spore launcher
			
			if (enemyStack->alienSporeLauncher)
				continue;
			
			// skip sufficiently targeted stack
			// target can be destroyed eventually
			
			if (enemyStack->requiredSuperioritySatisfied)
				continue;
			
			debug
			(
				"\t\t%s"
				" destructionRatio=%5.2f"
				" requiredSuperioritySatisfied=%d"
				"\n"
				, getLocationString(enemyStack->tile)
				, enemyStack->destructionRatio
				, enemyStack->requiredSuperioritySatisfied
			);
			
			// update worst
			
			if (enemyStack->destructionRatio < mostUndertargetedEnemyStackDestructionRatio)
			{
				mostUndertargetedEnemyStack = enemyStack;
				mostUndertargetedEnemyStackDestructionRatio = enemyStack->destructionRatio;
			}
			
		}
		
		// no undertargeted stacks - exit
		
		if (mostUndertargetedEnemyStack == nullptr)
			break;
		
		// exclude stack
		
		excludedEnemyStacks.insert(mostUndertargetedEnemyStack);
		
		debug("\tmost untargeted enemy stack: %s\n", getLocationString(mostUndertargetedEnemyStack->tile));
		
	}
	
	// optimize pod popping traveling
	
	debug("optimize pod popping traveling - %s\n", aiMFaction->noun_faction);
	
	std::vector<robin_hood::pair<int, TaskPriority *> *> podPoppingAssignments;
	
	for (robin_hood::unordered_flat_map<int, TaskPriority *>::iterator assignmentIterator = vehicleAssignments.begin(); assignmentIterator != vehicleAssignments.end(); assignmentIterator++)
	{
		TaskPriority *taskPriority = assignmentIterator->second;
		
		if (taskPriority->taskType == TT_MOVE && taskPriority->attackTarget == nullptr && isPodAt(taskPriority->destination))
		{
			podPoppingAssignments.push_back(&*assignmentIterator);
		}
		
	}
	
	bool changed = false;
	
	for (int i = 0; i < 1000 && changed; i++)
	{
		changed = false;
		
		for (robin_hood::pair<int, TaskPriority *> *podPoppingAssignment1 : podPoppingAssignments)
		{
			for (robin_hood::pair<int, TaskPriority *> *podPoppingAssignment2 : podPoppingAssignments)
			{
				// skip the same assignment
				
				if (podPoppingAssignment1 == podPoppingAssignment2)
					continue;
				
				// get vehicles and locations
				
				int vehicleId1 = podPoppingAssignment1->first;
				int vehicleId2 = podPoppingAssignment2->first;
				TaskPriority *taskPriority1 = podPoppingAssignment1->second;
				TaskPriority *taskPriority2 = podPoppingAssignment2->second;
				
				int triad1 = Vehs[vehicleId1].triad();
				int triad2 = Vehs[vehicleId2].triad();
				MAP *vehicleTile1 = getVehicleMapTile(vehicleId1);
				MAP *vehicleTile2 = getVehicleMapTile(vehicleId2);
				bool destinationOcean1 = is_ocean(taskPriority1->destination);
				bool destinationOcean2 = is_ocean(taskPriority2->destination);
				
				// same triad and realm
				
				if (!(triad1 == triad2 && destinationOcean1 == destinationOcean2))
					continue;
				
				// reachable destinations
				
				if (!(isVehicleDestinationReachable(vehicleId1, taskPriority2->destination) && isVehicleDestinationReachable(vehicleId2, taskPriority1->destination)))
					continue;
				
				isVehicleDestinationReachable(vehicleId1, taskPriority2->destination);
				isVehicleDestinationReachable(vehicleId2, taskPriority1->destination);
				
				double travelTime11 = taskPriority1->travelTime;
				double travelTime22 = taskPriority2->travelTime;
				double travelTime12 = getVehicleTravelTime(vehicleId1, taskPriority2->destination);
				double travelTime21 = getVehicleTravelTime(vehicleId2, taskPriority1->destination);
				
				double oldTravelTime = travelTime11 + travelTime22;
				double newTravelTime = travelTime12 + travelTime21;
				double improvement = 1.0 - newTravelTime / oldTravelTime;
				
				debug
				(
					"\t1: %s -> %s 2: %s -> %s"
					" oldTravelTime=%7.2f"
					" newTravelTime=%7.2f"
					" improvement=%+6.2f"
					"\n"
					, getLocationString(vehicleTile1), getLocationString(taskPriority1->destination)
					, getLocationString(vehicleTile2), getLocationString(taskPriority2->destination)
					, oldTravelTime
					, newTravelTime
					, improvement
				);
				
				if (improvement < 0.1)
					continue;
				
				// swap tasks
				
				podPoppingAssignment1->second = taskPriority2;
				podPoppingAssignment2->second = taskPriority1;
				
				changed = true;
				
				debug("\t\tswapped+++\n");
				
				break;
				
			}
			
			if (changed)
				break;
			
		}
		
	}
	
	// execute tasks
	
	debug("\tselectedTasks\n");
	
	for (robin_hood::pair<int, TaskPriority *> &vehicleAssignmentEntry : vehicleAssignments)
	{
		int vehicleId = vehicleAssignmentEntry.first;
		TaskPriority &taskPriority = *(vehicleAssignmentEntry.second);

//		if (DEBUG)
//		{
//			debug
//			(
//				"\t\t[%4d] %s"
//				" -> %s"
//				" => %s"
//				"\n"
//				, vehicleId, getLocationString(getVehicleMapTile(vehicleId))
//				, getLocationString(taskPriority.destination)
//				, getLocationString(taskPriority.attackTarget)
//			);
//			
//		}
		
		Task t(vehicleId, taskPriority.taskType, taskPriority.destination, taskPriority.attackTarget);
		if (!transitVehicle(t))
		{
			// TODO amphibious vehicles cannot find their path to empty enemy bases
			debug("ERROR: transitVehicle failed.");
		}
		
		if (DEBUG)
		{
			debug
			(
				"\t\t[%4d] %s"
				" -> %s"
				" => %s"
				"\n"
				, vehicleId, getLocationString(getVehicleMapTile(vehicleId))
				, getLocationString(taskPriority.destination)
				, getLocationString(taskPriority.attackTarget)
			);
			
		}
		
	}
	
	// store untargeted stacks for production demand
	
	for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		EnemyStackInfo *enemyStackInfo = &(enemyStackEntry.second);
		
		if (targetedEnemyStacks.find(enemyStackInfo) != targetedEnemyStacks.end())
			continue;
		
		aiData.production.untargetedEnemyStackInfos.push_back(enemyStackInfo);
		
	}
	
//	if (DEBUG)
//	{
//		debug("targetedEnemyStacks - %s\n", aiMFaction->noun_faction);
//		
//		for (robin_hood::pair<MAP *, EnemyStackInfo> const &enemyStackEntry : aiData.enemyStacks)
//		{
//			EnemyStackInfo const &enemyStack = enemyStackEntry.second;
//			
//			debug
//			(
//				"\t%s"
//				" requiredSuperiority=%5.2f"
//				" desiredSuperiority=%5.2f"
//				" maxBombardmentEffect=%5.2f"
//				" combinedBombardmentEffect=%5.2f"
//				" combinedDirectEffect=%5.2f"
//				" sufficient: %d %d"
//				"\n"
//				, getLocationString(enemyStack.tile).c_str()
//				, enemyStack.requiredSuperiority
//				, enemyStack.desiredSuperiority
//				, enemyStack.maxBombardmentEffect
//				, enemyStack.combinedBombardmentEffect
//				, enemyStack.combinedDirectEffect
//				, enemyStack.isSufficient(false)
//				, enemyStack.isSufficient(true)
//			);
//			
//			debug("\tattackers\n");
//			
//			for (IdDoubleValue vehicleIdTravelTile : enemyStack.bombardmentVechileTravelTimes)
//			{
//				int vehicleId = vehicleIdTravelTile.id;
//				double travelTime = vehicleIdTravelTile.value;
//				MAP *vehicleTile = getVehicleMapTile(vehicleId);
//				Task *task = getTask(vehicleId);
//				
//				if (task == nullptr)
//					continue;
//				
//				debug
//				(
//					"\t\t[%4d] %s"
//					" task=%d"
//					" travelTime=%7.2f"
//					"\n"
//					, vehicleId, getLocationString(vehicleTile)
//					, (int)task
//					, travelTime
//				);
//				
//			}
//			
//			for (IdDoubleValue vehicleIdTravelTile : enemyStack.directVechileTravelTimes)
//			{
//				int vehicleId = vehicleIdTravelTile.id;
//				double travelTime = vehicleIdTravelTile.value;
//				MAP *vehicleTile = getVehicleMapTile(vehicleId);
//				Task *task = getTask(vehicleId);
//				
//				if (task == nullptr)
//					continue;
//				
//				debug
//				(
//					"\t\t[%4d] %s"
//					" task=%d"
//					" travelTime=%7.2f"
//					"\n"
//					, vehicleId, getLocationString(vehicleTile)
//					, (int)task
//					, travelTime
//				);
//				
//			}
//			
//		}
//		
//	}
	
	// remove fully destroyed stacks from retaliation threat
	
	for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackInfoEntry : aiData.enemyStacks)
	{
		EnemyStackInfo &enemyStackInfo = enemyStackInfoEntry.second;
		
		if (!enemyStackInfo.isSufficient(false))
			continue;
		
		for (int vehiclePad0 : enemyStackInfo.vehiclePad0s)
		{
			// remove potential attack
			
			for (TileInfo &mapTileInfo : aiData.tileInfos)
			{
				std::vector<PotentialAttack> &potentialAttacks = mapTileInfo.potentialAttacks;
				
				for (std::vector<PotentialAttack>::iterator potentialAttackIterator = potentialAttacks.begin(); potentialAttackIterator != potentialAttacks.end(); )
				{
					PotentialAttack &potentialAttack = *potentialAttackIterator;
					
					if (potentialAttack.vehiclePad0 == vehiclePad0)
					{
						potentialAttackIterator = potentialAttacks.erase(potentialAttackIterator);
					}
					else
					{
						potentialAttackIterator++;
					}
					
				}
				
			}
			
		}
		
	}
	
}

/**
protection priority:
- base total threat
- travel time
*/
void populateDefensiveProbeTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateDefensiveProbeTasks\n");
	
	if (aiData.baseIds.size() == 0)
		return;
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// AI faction
		
		if (vehicle->faction_id != aiFactionId)
			continue;
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// defensive probe
		
		if (!(isInfantryVehicle(vehicleId) && isProbeVehicle(vehicleId)))
			continue;
		
		// process bases
		
		for (int baseId : aiData.baseIds)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
			
			// exclude not reachable destination
			
			if (!isVehicleDestinationReachable(vehicleId, baseTile))
				continue;
			
			// required protection
			
			double requiredEffect = baseInfo.probeData.requiredEffect;
			
			if (requiredEffect <= 0.0)
				continue;
			
			// get travel time and coresponding coefficient
			
			double travelTime = getVehicleTravelTime(vehicleId, baseTile);
			if (travelTime == INF)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// combat effect
			
			double combatEffect = getVehicleMoraleMultiplier(vehicleId);
			
			// priority
			
			double priority =
				requiredEffect
				* combatEffect
				* travelTimeCoefficient
			;
			
			// add task
			
			TaskPriority taskPriority(vehicleId, priority, TPR_BASE, TT_HOLD, baseTile, travelTime, baseId);
			taskPriority.baseId = baseId;
			taskPriorities.push_back(taskPriority);
			
			debug
			(
				"\t\t[%4d] (%3d,%3d) %-32s -> %-25s"
				" priority=%5.2f"
				" requiredEffect=%5.2f"
				" combatEffect=%5.2f"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				"\n"
				, vehicleId, vehicle->x, vehicle->y, getVehicleUnitName(vehicleId), base->name
				, priority
				, requiredEffect
				, combatEffect
				, travelTime
				, travelTimeCoefficient
			);
			
		}
		
	}
	
}

void populateRepairTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateRepairTasks\n");
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// exclude battle ogres
		
		if (isOgreVehicle(vehicleId))
			continue;
		
		// more than barely damaged
		
		if (vehicle->damage_taken < 2)
			continue;
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		debug("\t\t[%4d] %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)));
		
		// vehicle mineral cost
		
		int unitMineralCost = Rules->mineral_cost_multi * vehicle->cost();
		
		// repair bonus
		
		double fullRepairBonus = conf.ai_combat_strength_increase_value * std::max(0.0, getVehicleRelativeDamage(vehicleId) - 0.0) * (double)unitMineralCost;
		double partRepairBonus = conf.ai_combat_strength_increase_value * std::max(0.0, getVehicleRelativeDamage(vehicleId) - 0.2) * (double)unitMineralCost;
		
		// best priority
		
		MAP *bestRepairLocation = nullptr;
		double bestRepairLocationPriority = 0.0;
		
		for (MAP *tile : getRangeTiles(vehicleTile, MAX_REPAIR_DISTANCE, true))
		{
			int x = getX(tile), y = getY(tile);
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			// exclude monolith, they are handled by monolith function
			
			if (map_has_item(tile, BIT_MONOLITH))
				continue;
			
			// exclude blocked
			
			if (isBlocked(tile))
				continue;
			
			// exclude warzone
			
			if (tileInfo.hostileDangerZone || tileInfo.artilleryDangerZone)
				continue;
			
			// exclude too far field location
			
			if (!(map_has_item(tile, BIT_BASE_IN_TILE | BIT_BUNKER | BIT_MONOLITH)) && map_range(vehicle->x, vehicle->y, x, y) > 0)
				continue;
			
			// exclude unreachable locations
			
			if (!isVehicleDestinationReachable(vehicleId, tile))
				continue;
			
			// vehicle can repair there
			
			switch (triad)
			{
			case TRIAD_AIR:
				// airbase
				if (!isAirbaseAt(tile))
					continue;
				break;
				
			case TRIAD_SEA:
				// same ocean cluster
				if (!isSameSeaCluster(vehicleTile, tile))
					continue;
				break;
				
			case TRIAD_LAND:
				// same land transported cluster
				if (!isSameLandTransportedCluster(vehicleTile, tile))
					continue;
				break;
				
			}
			
			// get repair parameters
			
			RepairInfo repairInfo = getVehicleRepairInfo(vehicleId, tile);
			
			// no repair happening
			
			if (repairInfo.damage <= 0)
				continue;
			
			// repair priority coefficient
			
			double repairPriorityCoefficient = (repairInfo.full ? conf.ai_combat_priority_repair : conf.ai_combat_priority_repair_partial);
			
			// warzone coefficient
			
			double warzoneCoefficient = (tileInfo.hostileDangerZone ? 0.7 : 1.0);
			
			// travel time and total time
			
			double travelTime = getVehicleTravelTime(vehicleId, tile);
			if (travelTime == INF)
				continue;
			
			double totalTime = std::max(1.0, travelTime + (double)repairInfo.time);
			double totalTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, totalTime);
			
			// repairGain
			
			double repairBonus = (repairInfo.full ? fullRepairBonus : partRepairBonus);
			double repairGain = getGainBonus(repairBonus) * totalTimeCoefficient;
			
			double repairPriority =
				repairPriorityCoefficient
				* warzoneCoefficient
				* repairGain
				;
			
			debug
			(
				"\t\t\t-> %s"
				" repairPriority=%5.2f"
				" repairPriorityCoefficient=%5.2f"
				" warzoneCoefficient=%5.2f"
				" unitMineralCost=%2d"
				" repairBonus=%5.2f"
				" travelTime=%7.2f"
				" totalTime=%5.2f"
				" totalTimeCoefficient=%7.2f"
				" repairGain=%5.2f"
				"\n"
				, getLocationString({x, y})
				, repairPriority
				, repairPriorityCoefficient
				, warzoneCoefficient
				, unitMineralCost
				, repairBonus
				, travelTime
				, totalTime
				, totalTimeCoefficient
				, repairGain
			);
			
			// update best
			
			if (repairPriority > bestRepairLocationPriority)
			{
				bestRepairLocation = tile;
				bestRepairLocationPriority = repairPriority;
			}
			
		}
		
		// not found
		
		if (bestRepairLocation == nullptr)
		{
			debug("\t\t\trepair location is not found\n");
			continue;
		}
		
		// add task
		
		taskPriorities.emplace_back(vehicleId, bestRepairLocationPriority, TPR_NONE, TT_SKIP, bestRepairLocation);
		
	}
	
}

void populateMonolithTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateMonolithTasks\n");
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		
		// exclude battle ogres
		
		if (isOgreVehicle(vehicleId))
			continue;
		
		// not air
		// air cannot be either repaired or promoted by monolith
		
		if (triad == TRIAD_AIR)
			continue;
		
		// either barely damaged or not promoted for monolith promotion/repair to make sense
		
		if (!(vehicle->damage_taken >= 2 || (vehicle->morale < 6 && (vehicle->state & VSTATE_MONOLITH_UPGRADED) == 0)))
			continue;
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		debug("\t\t[%4d] %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)));
		
		// find closest monolith
		
		MapDoubleValue closestMonolithLocaionTravelTime = findClosestMonolith(vehicleId, MAX_REPAIR_DISTANCE, true);
		
		// not found
		
		if (closestMonolithLocaionTravelTime.tile == nullptr)
		{
			debug("\t\t\tmonolith location is not found\n");
			continue;
		}
		
		// travel time
		
		double travelTime = closestMonolithLocaionTravelTime.value;
		double totalTime = std::max(1.0, travelTime);
		double totalTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, totalTime);
		
		// repair
		
		double repairPriority = 0.0;
		
		if (vehicle->damage_taken > 0)
		{
			TileInfo &tileInfo = aiData.getTileInfo(closestMonolithLocaionTravelTime.tile);
			
			// repair priority coefficient
			
			double repairPriorityCoefficient = conf.ai_combat_priority_repair;
			
			// warzone coefficient
			
			double warzoneCoefficient = (tileInfo.hostileDangerZone ? 0.7 : 1.0);
			
			// repair gain
			
			int unitMineralCost = Rules->mineral_cost_multi * vehicle->cost();
			double fullRepairBonus = conf.ai_combat_strength_increase_value * getVehicleRelativeDamage(vehicleId) * (double)unitMineralCost;
			double repairBonus = fullRepairBonus;
			double repairGain = getGainBonus(repairBonus) * totalTimeCoefficient;
			
			repairPriority =
				repairPriorityCoefficient
				* warzoneCoefficient
				* repairGain
			;
			
			debug
			(
				"\t\t\t-> %s"
				" repairPriority=%5.2f"
				" repairPriorityCoefficient=%5.2f"
				" warzoneCoefficient=%5.2f"
				" unitMineralCost=%2d"
				" repairBonus=%5.2f"
				" travelTime=%7.2f"
				" totalTime=%7.2f"
				" totalTimeCoefficient=%5.2f"
				" repairGain=%5.2f"
				"\n"
				, getLocationString(closestMonolithLocaionTravelTime.tile)
				, repairPriority
				, repairPriorityCoefficient
				, warzoneCoefficient
				, unitMineralCost
				, repairBonus
				, travelTime
				, totalTime
				, totalTimeCoefficient
				, repairGain
			);
			
		}
		
		// promotion
		
		double promotionPriority = 0.0;
		
		if (vehicle->morale < 6 && (vehicle->state & VSTATE_MONOLITH_UPGRADED) == 0)
		{
			// promotion priority coefficient
			
			double promotionPriorityCoefficient = conf.ai_combat_priority_monolith_promotion;
			
			// promotion gain
			
			int unitMineralCost = Rules->mineral_cost_multi * vehicle->cost();
			double strengthIncrease = getMoraleMultiplier(vehicle->morale + 1) / getMoraleMultiplier(vehicle->morale) - 1.0;
			double promotionBonus = conf.ai_combat_strength_increase_value * strengthIncrease * (double)unitMineralCost;
			double promotionGain = getGainBonus(promotionBonus) * totalTimeCoefficient;
			
			promotionPriority =
				promotionPriorityCoefficient
				* promotionGain
			;
			
			debug
			(
				"\t\t\t-> %s"
				" promotionPriority=%5.2f"
				" promotionPriorityCoefficient=%5.2f"
				" unitMineralCost=%2d"
				" promotionBonus=%5.2f"
				" travelTime=%7.2f"
				" totalTime=%7.2f"
				" totalTimeCoefficient=%5.2f"
				" promotionGain=%5.2f"
				"\n"
				, getLocationString(closestMonolithLocaionTravelTime.tile)
				, promotionPriority
				, promotionPriorityCoefficient
				, unitMineralCost
				, promotionBonus
				, travelTime
				, totalTime
				, totalTimeCoefficient
				, promotionGain
			);
			
		}
		
		// combined priority
		
		double priority = repairPriority + promotionPriority;
		
		// add task
		
		taskPriorities.emplace_back(vehicleId, priority, TPR_NONE, TT_MOVE, closestMonolithLocaionTravelTime.tile);
		
	}
	
}

void populatePodPoppingTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulatePodPoppingTasks\n");
	
	// collect pod locations
	
	std::vector<std::pair<MAP *, int>> seaPodLocations;
	std::vector<std::pair<MAP *, int>> landPodLocations;
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		// pod
		
		if (isPodAt(tile) == 0)
			continue;
		
		// exclude neutral territory base radius
		
		if (isNeutralTerritory(aiFactionId, tile) && map_has_item(tile, BIT_BASE_RADIUS))
			continue;
		
		// not blocked
		
		if (isBlocked(tile))
			continue;
		
		if (is_ocean(tile))
		{
			int seaCluster = getSeaCluster(tile);
			seaPodLocations.emplace_back(tile, seaCluster);
		}
		else
		{
			int landTransportedCluster = getLandTransportedCluster(tile);
			landPodLocations.emplace_back(tile, landTransportedCluster);
		}
		
	}
	
	// iterate available scouts
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// surface vehicle
		
		if (!(triad == TRIAD_LAND || triad == TRIAD_SEA))
			continue;
		
		// not damaged
		
		if (isVehicleCanHealAtThisLocation(vehicleId))
			continue;
		
		debug("\t\t[%4d] %s\n", vehicleId, getLocationString(vehicleTile));
		
		// cluster and pod locations
		
		int vehicleCluster = triad == TRIAD_SEA ? getSeaCluster(vehicleTile) : getLandTransportedCluster(vehicleTile);
		std::vector<std::pair<MAP *, int>> &podLocations = triad == TRIAD_SEA ? seaPodLocations : landPodLocations;
		
		// iterate pods
		
		for (std::pair<MAP *, int> &podLocation : podLocations)
		{
			MAP *podTile = podLocation.first;
			int podCluster = podLocation.second;
			
			// same cluster
			
			if (podCluster != vehicleCluster)
				continue;
			
			// limit search range
			
			if (getRange(vehicleTile, podTile) > MAX_PODPOP_DISTANCE)
				continue;
			
			// get travel time and coresponding coefficient
			
			double travelTime = getVehicleTravelTime(vehicleId, podTile);
			if (travelTime == INF)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
			
			// priority
			
			double podpopBonus = conf.ai_production_pod_bonus;
			double podpopGain = getGainBonus(podpopBonus) * travelTimeCoefficient;
			
			double podpopPriority =
				conf.ai_combat_priority_pod
				* podpopGain
			;
			
			debug
			(
				"\t\t\t-> %s"
				" podpopPriority=%5.2f"
				" ai_combat_priority_pod=%5.2f"
				" podpopBonus=%5.2f"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				" podpopGain=%5.2f"
				"\n"
				, getLocationString(podTile)
				, podpopPriority
				, conf.ai_combat_priority_pod
				, podpopBonus
				, travelTime
				, travelTimeCoefficient
				, podpopGain
			);
			
			// add task
			
			taskPriorities.emplace_back(vehicleId, podpopPriority, TPR_ONE, TT_MOVE, podTile, travelTime);
			
		}
		
	}
	
}

/**
base police2x priority
- required police power
- travel time
*/
void populatePolice2xTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulatePolice2xTasks\n");
	
	if (aiData.baseIds.size() == 0)
		return;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// police2x but not ogres
		
		if (!isPolice2xVehicle(vehicleId) || isOgreVehicle(vehicleId))
			continue;
		
		// process bases
		
		for (int baseId : aiData.baseIds)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
			
			// exclude not reachable destination
			
			if (!isVehicleDestinationReachable(vehicleId, baseTile))
				continue;
			
			// police required power
			
			double requiredPower = (double)baseInfo.policeData.requiredPower;
			
			// travel time coefficient
			
			double travelTime = getVehicleTravelTime(vehicleId, baseTile);
			if (travelTime == INF)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// priority
			
			double priority = requiredPower * travelTimeCoefficient;
			
			// add task
			
			TaskPriority taskPriority(vehicleId, priority, TPR_BASE, TT_HOLD, baseTile, travelTime, baseId);
			taskPriority.baseId = baseId;
			taskPriorities.push_back(taskPriority);
			
			debug
			(
				"\t\t[%4d] (%3d,%3d) %-32s -> %-25s"
				" priority=%5.2f"
				" requiredPower=%5.2f"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				"\n"
				, vehicleId, vehicle->x, vehicle->y, getVehicleUnitName(vehicleId), base->name
				, priority
				, requiredPower
				, travelTime
				, travelTimeCoefficient
			);
			
		}
		
	}
	
}

/**
base police priority
- travel time
*/
void populatePoliceTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulatePoliceTasks\n");
	
	if (aiData.baseIds.size() == 0)
		return;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// infantry defensive
		
		if (!isInfantryDefensiveVehicle(vehicleId))
			continue;
		
		// process bases
		
		for (int baseId : aiData.baseIds)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			
			// exclude not reachable destination
			
			if (!isVehicleDestinationReachable(vehicleId, baseTile))
				continue;
			
			// travel time coefficient
			
			double travelTime = getVehicleTravelTime(vehicleId, baseTile);
			if (travelTime == INF)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// priority
			
			double priority = travelTimeCoefficient;
			
			// add task
			
			TaskPriority taskPriority(vehicleId, priority, TPR_BASE, TT_HOLD, baseTile, travelTime, baseId);
			taskPriority.baseId = baseId;
			taskPriorities.push_back(taskPriority);
			
			debug
			(
				"\t\t[%4d] (%3d,%3d) %-32s -> %-25s"
				" priority=%5.2f"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				"\n"
				, vehicleId, vehicle->x, vehicle->y, getVehicleUnitName(vehicleId), base->name
				, priority
				, travelTime
				, travelTimeCoefficient
			);
			
		}
		
	}
	
}

/**
protection priority:
- base total threat
- travel time

effect is not included as it changes with staffing location with protectors and is checked dynamically

*/
void populateBaseProtectorTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateBaseProtectorTasks\n");
	
	if (aiData.baseIds.size() == 0)
		return;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// not ogres
		
		if (isOgreVehicle(vehicleId))
			continue;
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// process bases
		
		for (int baseId : aiData.baseIds)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
			
			// exclude not reachable destination
			
			if (!isVehicleDestinationReachable(vehicleId, baseTile))
				continue;
			
			// initial threat
			
			double threat = baseInfo.combatData.assailantWeight;
			
			// get travel time and coresponding coefficient
			
			double travelTime = getVehicleTravelTime(vehicleId, baseTile);
			if (travelTime == INF)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// priority
			
			double priority =
				threat
				* travelTimeCoefficient
			;
			
			// add task
			
			TaskPriority taskPriority(vehicleId, priority, TPR_BASE, TT_HOLD, baseTile, travelTime, baseId);
			taskPriority.baseId = baseId;
			taskPriorities.push_back(taskPriority);
			
			debug
			(
				"\t\t[%4d] (%3d,%3d) %-32s -> %-25s"
				" priority=%5.2f"
				" threat=%5.2f"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				"\n"
				, vehicleId, vehicle->x, vehicle->y, getVehicleUnitName(vehicleId), base->name
				, priority
				, threat
				, travelTime
				, travelTimeCoefficient
			);
			
		}
		
	}
	
}

/**
protection priority:
- bunker total threat
- travel time

effect is not included as it changes with staffing location with protectors and is checked dynamically

*/
void populateBunkerProtectorTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateBunkerProtectorTasks\n");
	
	if (aiData.bunkerCombatDatas.size() == 0)
		return;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// not ogres
		
		if (isOgreVehicle(vehicleId))
			continue;
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// exclude ranged air units - they cannot land at bunker
		
		if (isRangedAirVehicle(vehicleId))
			continue;
		
		// process bunkers
		
		for (robin_hood::pair<MAP *, CombatData> &bunkerCombatDataEntry : aiData.bunkerCombatDatas)
		{
			MAP *bunkerTile = bunkerCombatDataEntry.first;
			CombatData &bunkerCombatData = bunkerCombatDataEntry.second;
			
			// exclude not reachable destination
			
			if (!isVehicleDestinationReachable(vehicleId, bunkerTile))
				continue;
			
			// required protection
			
			double threat = bunkerCombatData.assailantWeight;
			
			// get travel time and coresponding coefficient
			
			double travelTime = getVehicleTravelTime(vehicleId, bunkerTile);
			if (travelTime == INF)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// priority
			
			double priority =
				threat
				* travelTimeCoefficient
			;
			
			// add task
			
			TaskPriority taskPriority(vehicleId, priority, TPR_NONE, TT_HOLD, bunkerTile, travelTime);
			taskPriorities.push_back(taskPriority);
			
			debug
			(
				"\t\t[%4d] (%3d,%3d) %-32s -> %s"
				" priority=%5.2f"
				" threat=%5.2f"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				"\n"
				, vehicleId, vehicle->x, vehicle->y, getVehicleUnitName(vehicleId), getLocationString(bunkerTile)
				, priority
				, threat
				, travelTime
				, travelTimeCoefficient
			);
			
		}
		
	}
	
}

void populateEmptyBaseCaptureTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateEmptyBaseCaptureTasks\n");
	
	// iterate vehicles and bases
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		debug("\t\t[%4d] %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)));
		
		for (int enemyBaseId : aiData.emptyEnemyBaseIds)
		{
			MAP *enemyBaseTile = getBaseMapTile(enemyBaseId);
			BaseInfo &enemyBaseInfo = aiData.getBaseInfo(enemyBaseId);
			
			// can capture this base
			
			if (!isVehicleCanCaptureBase(vehicleId, enemyBaseTile))
				continue;
			
			// travel time
			
			MapDoubleValue meleeAttackPosition = getMeleeAttackPosition(vehicleId, enemyBaseTile);
			
			if (meleeAttackPosition.tile == nullptr || meleeAttackPosition.value == INF)
				continue;
			
			double travelTime = meleeAttackPosition.value;
			
			// do not capture base farther than 10 turn travel time
			
			if (travelTime > 10.0)
				continue;
			
			// priority
			
			double enemyBaseGain = enemyBaseInfo.gain;
			double enemyBaseCaptureGain = getGainRepetion(enemyBaseGain, 1.0, travelTime);
			double priority =
				conf.ai_combat_attack_priority_base
				* enemyBaseCaptureGain
			;
			
			// add task
			
			taskPriorities.emplace_back(vehicleId, priority, TPR_ONE, TT_MELEE_ATTACK, meleeAttackPosition.tile, travelTime, CM_MELEE, enemyBaseTile, false, 0.0);
			
//			if (DEBUG)
//			{
//				debug
//				(
//					"\t\t\t-> %s"
//					" priority=%5.2f"
//					" ai_combat_attack_priority_base=%5.2f"
//					" travelTime=%7.2f"
//					" enemyBaseGain=%5.2f"
//					" enemyBaseCaptureGain=%5.2f"
//					"\n"
//					, getLocationString(enemyBaseTile).c_str()
//					, priority
//					, conf.ai_combat_attack_priority_base
//					, travelTime
//					, enemyBaseGain
//					, enemyBaseCaptureGain
//				);
//			}
			
		}
		
	}
	
}

void populateEnemyStackAttackTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateEnemyStackAttackTasks\n");
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int speed = getVehicleSpeed(vehicleId);
		double vehicleRelativeDamage = getVehicleRelativeDamage(vehicleId);
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// do not send severely damaged units to assault
		
		if (vehicleRelativeDamage > 0.5)
			continue;
		
		debug("\t\t[%4d] %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)));
		
		for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
		{
			MAP *enemyStackTile = enemyStackEntry.first;
			EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
			
			// do not attack alien sea vehicles far from bases
			
			if (enemyStackInfo.alien && is_ocean(enemyStackInfo.tile) && enemyStackInfo.baseRange > 3)
				continue;
			
			// do not attack non-hostiles vehicles
			
			if (!enemyStackInfo.hostile)
				continue;
			
			// land artillery do not attack fungal tower
			
			if (isLandArtilleryVehicle(vehicleId) && enemyStackInfo.alienFungalTower)
				continue;
			
			// ship do not attack fungal tower
			
			if (vehicle->triad() == TRIAD_SEA && enemyStackInfo.alienFungalTower)
				continue;
			
			debug("\t\t\t-> %s\n", getLocationString(enemyStackTile));
			
			// attack position and travel time
			
			MapDoubleValue meleeAttackPosition(nullptr, INF);
			
			if (enemyStackInfo.isUnitCanMeleeAttackStack(vehicle->unit_id))
			{
				meleeAttackPosition = getMeleeAttackPosition(vehicleId, enemyStackTile);
			}
			
			MapDoubleValue artilleryAttackPosition(nullptr, INF);
			
			if (enemyStackInfo.isUnitCanArtilleryAttackStack(vehicle->unit_id))
			{
				artilleryAttackPosition = getArtilleryAttackPosition(vehicleId, enemyStackTile);
			}
			
			// worth chasing
			
			if
			(
				enemyStackInfo.baseOrBunker
				||
				enemyStackTile->owner == aiFactionId
				||
				(meleeAttackPosition.value <= 1.0)
				||
				(artilleryAttackPosition.value <= 1.0)
				||
				speed > enemyStackInfo.lowestSpeed
			)
			{
				// worth chasing
			}
			else
			{
				debug("\t\t\t\tnot worth chasing\n");
				continue;
			}
			
			// process attack modes
			
			if (meleeAttackPosition.tile != nullptr)
			{
				// effect
				
				COMBAT_MODE combatMode = CM_MELEE;
				bool destructive = true;
				double combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, CM_MELEE);
				double winningProbability = getWinningProbability(combatEffect);
				
				// attackGain
				
				double attackGain = enemyStackInfo.destructionGain;
				
				// travel time coefficient
				
				MAP *destination = meleeAttackPosition.tile;
				double travelTime = meleeAttackPosition.value;
				double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				
				// priority
				
				double priority =
					1.0
					* attackGain
					* winningProbability
					* travelTimeCoefficient
				;
				
				debug
				(
					"\t\t\t\t%-15s"
					" priority=%5.2f"
					" combatEffect=%5.2f"
					" winningProbability=%5.2f"
					" attackGain=%5.2f"
					" travelTime=%7.2f"
					" travelTimeCoefficient=%5.2f"
					"\n"
					, "direct"
					, priority
					, combatEffect
					, winningProbability
					, attackGain
					, travelTime
					, travelTimeCoefficient
				);
				
				taskPriorities.emplace_back(vehicleId, priority, TPR_STACK, TT_MELEE_ATTACK, destination, travelTime, combatMode, enemyStackTile, destructive, combatEffect);
				
			}
			
			if (artilleryAttackPosition.tile != nullptr && enemyStackInfo.artillery)
			{
				// effect
				
				COMBAT_MODE combatMode = CM_ARTILLERY_DUEL;
				bool destructive = true;
				double combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, CM_ARTILLERY_DUEL);
				double winningProbability = getWinningProbability(combatEffect);
				
				// attackGain
				
				double attackGain = enemyStackInfo.destructionGain;
				
				// travel time coefficient
				
				MAP *destination = artilleryAttackPosition.tile;
				double travelTime = artilleryAttackPosition.value;
				double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				
				// priority
				
				double priority =
					winningProbability
					* attackGain
					* travelTimeCoefficient
				;
				
				debug
				(
					"\t\t\t\t%-15s"
					" priority=%5.2f"
					" combatEffect=%5.2f"
					" winningProbability=%5.2f"
					" attackGain=%5.2f"
					" travelTime=%7.2f"
					" travelTimeCoefficient=%5.2f"
					"\n"
					, "direct"
					, priority
					, combatEffect
					, winningProbability
					, attackGain
					, travelTime
					, travelTimeCoefficient
				);
				
				taskPriorities.emplace_back(vehicleId, priority, TPR_STACK, TT_ARTILLERY_ATTACK, destination, travelTime, combatMode, enemyStackTile, destructive, combatEffect);
				
			}
			
			if (artilleryAttackPosition.tile != nullptr && !enemyStackInfo.artillery && enemyStackInfo.bombardment)
			{
				// effect
				
				COMBAT_MODE combatMode = CM_BOMBARDMENT;
				bool destructive = enemyStackInfo.bombardmentDestructive;
				double combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, CM_BOMBARDMENT);
				
				// bombardment combatEffect adjustment for the purpose of prioritization
				
				combatEffect = 1.0 + 4.0 * combatEffect;
				double winningProbability = getWinningProbability(combatEffect);
				
				// attackGain
				
				double attackGain = enemyStackInfo.destructionGain;
				
				// travel time coefficient
				
				MAP *destination = artilleryAttackPosition.tile;
				double travelTime = artilleryAttackPosition.value;
				double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				
				// priority
				
				double priority =
					winningProbability
					* attackGain
					* travelTimeCoefficient
				;
				
				debug
				(
					"\t\t\t\t%-15s"
					" priority=%5.2f"
					" combatEffect=%5.2f"
					" winningProbability=%5.2f"
					" attackGain=%5.2f"
					" travelTime=%7.2f"
					" travelTimeCoefficient=%5.2f"
					"\n"
					, "direct"
					, priority
					, combatEffect
					, winningProbability
					, attackGain
					, travelTime
					, travelTimeCoefficient
				);
				
				taskPriorities.emplace_back(vehicleId, priority, TPR_STACK, TT_ARTILLERY_ATTACK, destination, travelTime, combatMode, enemyStackTile, destructive, combatEffect);
				
			}
			
		}
		
	}
	
}

/**
Holds atacking forces for coordinated attack.
*/
void coordinateAttack()
{
	debug("coordinateAttack - %s\n", MFactions[aiFactionId].noun_faction);
	
	// process targetedStacks
	
	for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		EnemyStackInfo &enemyStack = enemyStackEntry.second;
		
		// do not coordinate attack for not targeted stacks
		
		if (enemyStack.directVechileTravelTimes.empty())
			   continue;
		
		// do not coordinate attack against mobile aliens
		
		if (enemyStack.alien && !enemyStack.alienFungalTower)
			continue;
		
		debug("\t%s <-\n", getLocationString(enemyStack.tile));
		
		// hold bombarders if bombarding would break a treaty
		
		if (enemyStack.breakTreaty)
		{
			for (IdDoubleValue const &vehicleTravelTime : enemyStack.bombardmentVechileTravelTimes)
			{
				int vehicleId = vehicleTravelTime.id;
				double travelTime = vehicleTravelTime.value;
				Task *task = getTask(vehicleId);
				
				if (task == nullptr)
					continue;
				
				task->type = TT_HOLD;
				task->clearDestination();
				debug("\t\t[%4d] %s travelTime=%7.2f - bombardment wait for breaking treaty\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)), travelTime);
				
			}
			
		}
		
		// hold too early vehicles
		
		for (IdDoubleValue const &vehicleTravelTime : enemyStack.directVechileTravelTimes)
		{
			int vehicleId = vehicleTravelTime.id;
			double travelTime = vehicleTravelTime.value;
			
			// do not hold vehicle on transport
			
			if (isLandVehicleOnTransport(vehicleId))
			{
				debug("\t\t[%4d] %s travelTime=%7.2f\t\t(on transport)\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)), travelTime);
				continue;
			}
			
			// skip not too early vehicles
			
			if (travelTime >= enemyStack.coordinatorTravelTime - 2.0)
			{
				debug("\t\t[%4d] %s travelTime=%7.2f\t\t(behind)\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)), travelTime);
				continue;
			}
			
			// hold
			
			Task *task = getTask(vehicleId);
			
			if (task == nullptr)
				continue;
			
			task->type = TT_HOLD;
			task->clearDestination();
			debug("\t\t[%4d] %s travelTime=%7.2f - melee wait for coordinator\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)), travelTime);
			
		}
		
		// hold vehicles to wait for bombardment
		
		if (enemyStack.getTotalBombardmentEffectRatio() > 0.25)
		{
			for (IdDoubleValue const &vehicleTravelTime : enemyStack.directVechileTravelTimes)
			{
				int vehicleId = vehicleTravelTime.id;
				double travelTime = vehicleTravelTime.value;
				
				// do not hold too far vehicles
				
				if (travelTime > 2.0)
				{
					debug("\t\t[%4d] %s travelTime=%7.2f\t\t(too far)\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)), travelTime);
					continue;
				}
				
				// hold too close vehicles
				
				Task *task = getTask(vehicleId);
				
				if (task == nullptr)
					continue;
				
				task->type = TT_HOLD;
				task->clearDestination();
				debug("\t\t[%4d] %s travelTime=%7.2f - melee wait for bombardment\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)), travelTime);
				
			}
			
		}
		
	}
	
}

/*
Checks if unit available for orders.
*/
bool isVehicleAvailable(int vehicleId, bool notAvailableInWarzone)
{
	// should not have task and should not be notAvailableInWarzone
	return (!hasTask(vehicleId) && !(notAvailableInWarzone && aiData.getTileInfo(getVehicleMapTile(vehicleId)).hostileDangerZone));
}

/*
Checks if unit available for orders.
*/
bool isVehicleAvailable(int vehicleId)
{
	return isVehicleAvailable(vehicleId, false);
}

/*
Determines how far from base vehicle should be to effectivelly protect it.
*/
int getVehicleProtectionRange(int vehicleId)
{
	int protectionRange;

	if(isInfantryDefensiveVehicle(vehicleId))
	{
		protectionRange = 0;
	}
	else if(isLandArtilleryVehicle(vehicleId))
	{
		protectionRange = 0;
	}
	else
	{
		protectionRange = 2;
	}

	return protectionRange;

}

bool compareTaskPriorityDescending(const TaskPriority &a, const TaskPriority &b)
{
	return a.priority > b.priority;
}

/*
Computes resulting cost gain after attack.
*/
double getDuelCombatCostCoefficient(int vehicleId, double effect, double enemyUnitCost)
{
	bool TRACE = DEBUG && false;

	if (TRACE) { debug("getDuelCombatCostCoefficient\n"); }

	double vehicleUnitCost = (double)getVehicleUnitCost(vehicleId);

	if (vehicleUnitCost <= 0.0 || enemyUnitCost <= 0.0)
		return 0.0;

	// compute damage

	double attackerRelativeDamage = (effect <= 1.0 ? 1.0 : 1.0 / effect);
	double defenderRelativeDamage = (effect >= 1.0 ? 1.0 : effect);

	// compute cost bonus

	double costBonus =
		+ vehicleUnitCost * (1.0 - attackerRelativeDamage)
		+ enemyUnitCost * defenderRelativeDamage
	;

	double relativeCostBonus = costBonus / (vehicleUnitCost + enemyUnitCost);

	if (TRACE)
	{
		debug
		(
			"\trelativeCostBonus=%5.2f"
			" effect=%5.2f"
			" vehicleUnitCost=%5.2f"
			" enemyUnitCost=%5.2f"
			" attackerRelativeDamage=%5.2f"
			" defenderRelativeDamage=%5.2f"
			"\n"
			, relativeCostBonus
			, effect
			, vehicleUnitCost
			, enemyUnitCost
			, attackerRelativeDamage
			, defenderRelativeDamage
		);
	}

	return relativeCostBonus;

}

/*
Computes resulting cost gain after attack.
*/
double getBombardmentCostCoefficient(int vehicleId, double effect, double enemyUnitCost)
{
	bool TRACE = DEBUG && false;

	if (TRACE) { debug("getDuelCombatCostCoefficient\n"); }

	double vehicleUnitCost = (double)getVehicleUnitCost(vehicleId);

	if (vehicleUnitCost <= 0.0 || enemyUnitCost <= 0.0)
		return 0.0;

	// compute damage

	double attackerRelativeDamage = 0.0;
	double defenderRelativeDamage = (effect >= 1.0 ? 1.0 : effect);

	// compute cost bonus

	double costBonus =
		+ vehicleUnitCost * (1.0 - attackerRelativeDamage)
		+ enemyUnitCost * defenderRelativeDamage
	;

	double relativeCostBonus = costBonus / (vehicleUnitCost + enemyUnitCost);

	if (TRACE)
	{
		debug
		(
			"\trelativeCostBonus=%5.2f"
			" effect=%5.2f"
			" vehicleUnitCost=%5.2f"
			" enemyUnitCost=%5.2f"
			" attackerRelativeDamage=%5.2f"
			" defenderRelativeDamage=%5.2f"
			"\n"
			, relativeCostBonus
			, effect
			, vehicleUnitCost
			, enemyUnitCost
			, attackerRelativeDamage
			, defenderRelativeDamage
		);
	}

	return relativeCostBonus;

}

bool isPrimaryEffect(int vehicleId, MAP *enemyStackTile, EnemyStackInfo &enemyStackInfo, COMBAT_MODE combatMode)
{
	VEH *vehicle = getVehicle(vehicleId);
	int triad = vehicle->triad();
	bool enemyStackTileOcean = is_ocean(enemyStackTile);
	
	// melee is always primary
	
	if (combatMode == CM_MELEE)
		return true;
	
	// artillery duel
	
	if (combatMode == CM_ARTILLERY_DUEL)
		return true;
	
	// ship attacking ocean base
	
	if (triad == TRIAD_SEA && enemyStackTileOcean)
		return true;
	
	// bombarding destructible objects
	
	if (enemyStackInfo.bombardmentDestructive)
		return true;
	
	// otherwise, not
	
	return false;
	
}

/**
Checks if vehicle is currently protecting location.
*/
bool isProtecting(int vehicleId)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	Task *task = getTask(vehicleId);
	
	if (task == nullptr)
		return false;
	
	MAP *destination = task->getDestination();
	TileInfo const &destinationTileInfo = aiData.getTileInfo(destination);
	
	// destined to hold at the base or bunker and is at destination
	
	return (destinationTileInfo.base || destinationTileInfo.bunker) && task->type == TT_HOLD && vehicleTile == destination;
	
}

/*
Estimates the gain of vehicle defending at this location.
Gain is composed of estimated losses at both sides times unit cost.
*/
double getDefendGain(int defenderVehicleId, MAP *tile, double defenderHealth, int excludeEnemyVehiclePad0)
{
	VEH &defenderVehicle = Vehs[defenderVehicleId];
	
	// tileInfo
	
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	// defender is already destroyed
	
	if (defenderHealth <= 0.0)
	{
		return -getVehicleDestructionGain(defenderVehicleId);
	}
	
	// bombardment
	
	for (PotentialAttack const &potentialAttack : tileInfo.potentialAttacks)
	{
		// exclude enemy vehicle
		
		if (potentialAttack.vehiclePad0 == excludeEnemyVehiclePad0)
			continue;
		
		int attackerVehicleId = getVehicleIdByPad0(potentialAttack.vehiclePad0);
		if (!(attackerVehicleId >= 0 && attackerVehicleId < *VehCount))
			continue;
		
		// bombardment
		
		ENGAGEMENT_MODE engagementMode = potentialAttack.engagementMode;
		COMBAT_MODE combatMode = getCombatMode(engagementMode, defenderVehicle.unit_id);
		if (combatMode != CM_BOMBARDMENT)
			continue;
		
		double combatEffect = getTileCombatEffect(attackerVehicleId, defenderVehicleId, engagementMode, tile, true);
		defenderHealth -= combatEffect;
		
	}
	
	double maxBombardmentDamage = getMaxBombardmentDamage((Triad) defenderVehicle.triad(), tile);
	double minBombardmentHealth = 1.0 - maxBombardmentDamage;
	defenderHealth = std::max(minBombardmentHealth, defenderHealth);
	
	// defender is destroyed by bombardment
	
	if (defenderHealth <= 0.0)
	{
		return -getVehicleDestructionGain(defenderVehicleId);
	}
	
	// TODO apply all attacks until one side is destroyed
	// rank attackers by combat effect descending
	
	std::vector<PotentialAttack *> bombardmentPotentialAttacks;
	std::vector<PotentialAttack *> mutualPotentialAttacks;
	
	int bestAttackerVehicleId = -1;
	double bestAttackerGain = std::numeric_limits<double>::lowest();
	
	for (PotentialAttack const &potentialAttack : tileInfo.potentialAttacks)
	{
		// exclude enemy vehicle
		
		if (potentialAttack.vehiclePad0 == excludeEnemyVehiclePad0)
			continue;
		
		int attackerVehicleId = getVehicleIdByPad0(potentialAttack.vehiclePad0);
		if (!(attackerVehicleId >= 0 && attackerVehicleId < *VehCount))
			continue;
		
		// not bombardment
		
		ENGAGEMENT_MODE engagementMode = potentialAttack.engagementMode;
		COMBAT_MODE combatMode = getCombatMode(engagementMode, defenderVehicle.unit_id);
		if (combatMode == CM_BOMBARDMENT)
			continue;
		
		double gain = getCombatGain(attackerVehicleId, defenderVehicleId, engagementMode, tile, true, getVehicleRelativeHealth(attackerVehicleId), defenderHealth);
		
		if (gain > bestAttackerGain)
		{
			bestAttackerVehicleId = attackerVehicleId;
			bestAttackerGain = gain;
		}
		
	}
	
	if (bestAttackerVehicleId == -1)
		return 0.0;
	
	// defense gain is an opposite of attack gain
	
	double gain = -bestAttackerGain;
	
debug(">getDefendGain=%5.2f excludeEnemyVehiclePad0=%d\n", gain, excludeEnemyVehiclePad0);
	return gain;
	
}

/*
Evaluates gain for approaching the desired destination.
*/
double getProximityGain(int vehicleId, MAP *tile)
{
	if (!hasTask(vehicleId))
		return 0.0;
	
	Task *task = getTask(vehicleId);
	MAP *taskDestination = task->getDestination();
	
	if (taskDestination == nullptr)
		return 0.0;
	
	// already at task destination
	if (getVehicleMapTile(vehicleId) == taskDestination)
		return 0.0;
	
	double initialTravelTime = getVehicleTravelTime(vehicleId, taskDestination, true);
	double tileTravelTime = getVehicleTravelTime(vehicleId, tile, taskDestination, true);
	double proximityDestTimeGain = conf.ai_combat_proximity_dest_time_coefficient * (initialTravelTime - tileTravelTime);
	
	return proximityDestTimeGain;
	
}

/*
Estimates the gain of vehicle melee attacking and then defending at this location.
Gain is composed of estimated losses at both sides times unit cost.
*/
double getMeleeAttackGain(int vehicleId, MAP *destination, MAP *target, double hastyCoefficient)
{
	if (!aiData.hasEnemyStack(target))
		return 0.0;
	
	EnemyStackInfo const &enemyStackInfo = aiData.getEnemyStackInfo(target);
	
	if (enemyStackInfo.vehiclePad0s.size() == 0)
		return 0.0;
	
	// defender
	
	int defenderVehicleId = -1;
	for (int defenderVehiclePad0 : enemyStackInfo.vehiclePad0s)
	{
		int vehicleId = getVehicleIdByPad0(defenderVehiclePad0);
		if (!(vehicleId >= 0 && vehicleId < *VehCount))
			continue;
		
		defenderVehicleId = vehicleId;
		break;
		
	}
	if (defenderVehicleId == -1)
		return 0.0;
	
	defenderVehicleId = best_defender(defenderVehicleId, vehicleId, 0);
	
	// compute gain
	
	double combatEffect = hastyCoefficient * enemyStackInfo.getVehicleOffenseEffect(vehicleId, CM_MELEE);
	double winningProbability = getWinningProbability(combatEffect);
	double attackerDamage = combatEffect <= 1.0 ? 1.0 : (1.0 / combatEffect);
	double attackerHealth = (1.0 - attackerDamage) * getVehicleRelativeHealth(vehicleId);
	
	// attack gain
	
	double attackGain = getMutualCombatGain(getVehicleDestructionGain(vehicleId), enemyStackInfo.destructionGain, combatEffect);
	
	// defend gain if survived
	
	double defendGain = 0.0;
	if (winningProbability > 0.0 && attackerHealth > 0.0)
	{
		defendGain = winningProbability * getDefendGain(vehicleId, destination, attackerHealth, Vehs[defenderVehicleId].pad_0);
	}
	
	return attackGain + defendGain;
	
}

/*
Estimates the gain of vehicle artillery attacking and then defending at this location.
Gain is composed of estimated losses at both sides times unit cost.
*/
double getArtilleryAttackGain(int vehicleId, MAP *destination, MAP *target)
{
	if (!aiData.hasEnemyStack(target))
		return 0.0;
	
	EnemyStackInfo const &enemyStackInfo = aiData.getEnemyStackInfo(target);
	
	if (enemyStackInfo.vehiclePad0s.size() == 0)
		return 0.0;
	
	double survivalProbability;
	double attackerHealth;
	double attackGain;
	
	int defenderVehicleId = -1;
	for (int defenderVehiclePad0 : enemyStackInfo.vehiclePad0s)
	{
		int vehicleId = getVehicleIdByPad0(defenderVehiclePad0);
		if (!(vehicleId >= 0 && vehicleId < *VehCount))
			continue;
		
		defenderVehicleId = vehicleId;
		break;
		
	}
	if (defenderVehicleId == -1)
		return 0.0;
	
	if (enemyStackInfo.artillery)
	{
		defenderVehicleId = best_defender(defenderVehicleId, vehicleId, 1);
		
		double combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, CM_ARTILLERY_DUEL);
		double attackerDamage = combatEffect <= 1.0 ? 1.0 : (1.0 / combatEffect);
		
		attackerHealth = (1.0 - attackerDamage) * getVehicleRelativeHealth(vehicleId);
		survivalProbability = getWinningProbability(combatEffect);
		
		attackGain = getMutualCombatGain(getVehicleDestructionGain(vehicleId), enemyStackInfo.destructionGain, combatEffect);
		
	}
	else
	{
		defenderVehicleId = -1;
		
		double combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, CM_BOMBARDMENT);
		
		attackerHealth = 1.0 * getVehicleRelativeHealth(vehicleId);
		survivalProbability = 1.0;
		attackGain = getBombardmentGain(enemyStackInfo.destructionGain, combatEffect);
		
	}
	
	// defend gain if survived
	
	double defendGain = 0.0;
	if (survivalProbability > 0.0 && attackerHealth > 0.0)
	{
		defendGain = survivalProbability * getDefendGain(vehicleId, destination, attackerHealth, Vehs[defenderVehicleId].pad_0);
	}
	
	return attackGain + defendGain;
	
}

/*
Combat gain to attacker.
*/
double getMutualCombatGain(double attackerDestructionGain, double defenderDestructionGain, double combatEffect)
{
debug(">getMutualCombatGain( attackerDestructionGain=%5.2f defenderDestructionGain=%5.2f combatEffect=%5.2f )\n", attackerDestructionGain, defenderDestructionGain, combatEffect);
	double winningProbability = getWinningProbability(combatEffect);
	
	double attackerLoss = (1.0 - winningProbability);
	double defenderLoss = winningProbability;

	double attackerLossGain = attackerLoss * attackerDestructionGain;
	double defenderLossGain = defenderLoss * defenderDestructionGain;
	
	double gain =
		-attackerLossGain
		+defenderLossGain
	;

debug(">getMutualCombatGain=%5.2f\n", gain);
	return gain;
	
}

/*
Combat gain to attacker.
*/
double getBombardmentGain(double defenderDestructionGain, double relativeBombardmentDamage)
{
	double defenderLoss = relativeBombardmentDamage;
	double defenderLossGain = defenderLoss * defenderDestructionGain;
	
	double gain =
		+defenderLossGain
	;
	
	return gain;
	
}

CombatAction selectVehicleCombatAction(int vehicleId)
{
//	debug("\tselectVehicleCombatAction %s\n", getVehicleLocationIdNameString(vehicleId));
	
	CombatAction bestCombatAction;
	bestCombatAction.gain = -DBL_MAX;
	
	// move
	
	for (MoveAction const &moveAction : getVehicleMoveActions(vehicleId, true))
	{
		MAP *destination = moveAction.destination;
		
		double defendGain = getDefendGain(vehicleId, destination, getVehicleRelativeHealth(vehicleId));
		double gain = defendGain;
		
		debug("\t\t-> %s => %s gain= %+5.2f", getLocationString(destination), getLocationString(nullptr), gain);
		
		if (gain > bestCombatAction.gain)
		{
			bestCombatAction.setMove(vehicleId, gain, destination, moveAction.movementAllowance);
			debug("\t- best\n");
		}
		else
		{
			debug("\n");
		}
		
	}
	
	// melee attack
	
	if (isMeleeVehicle(vehicleId))
	{
		for (AttackAction const &attackAction : getMeleeAttackActions(vehicleId, true, true))
		{
			double gain = getMeleeAttackGain(vehicleId, attackAction.destination, attackAction.target, attackAction.hastyCoefficient);
			debug("\t\t-> %s => %s gain= %+5.2f", getLocationString(attackAction.destination), getLocationString(attackAction.target), gain);
			
			if (gain > bestCombatAction.gain)
			{
				bestCombatAction.setAttack(vehicleId, gain, attackAction.destination, 0, EM_MELEE, attackAction.target, attackAction.hastyCoefficient);
				debug("\t- best\n");
			}
			else
			{
				debug("\n");
			}
			
		}
		
	}
	
	// artillery attack
	
	if (isArtilleryVehicle(vehicleId))
	{
		for (AttackAction const &attackAction : getArtilleryAttackActions(vehicleId, true, true))
		{
			double gain = getArtilleryAttackGain(vehicleId, attackAction.destination, attackAction.target);
			debug("\t\t-> %s => %s gain= %+5.2f", getLocationString(attackAction.destination), getLocationString(attackAction.target), gain);
			
			if (gain > bestCombatAction.gain)
			{
				bestCombatAction.setAttack(vehicleId, gain, attackAction.destination, 0, EM_ARTILLERY, attackAction.target, 1.0);
				debug("\t- best\n");
			}
			else
			{
				debug("\n");
			}
			
		}
		
	}
	
	return bestCombatAction;
	
}

/*
Move faction combat vehicles.
*/
void aiEnemyMoveCombatVehicles()
{
	debug("aiEnemyMoveCombatVehicles - %s\n", aiMFaction->noun_faction);
	
	// move combat vehicles
	
	while (true)
	{
		// select best action
		
		CombatAction bestCombatAction;
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH &vehicle = Vehs[vehicleId];
			
			if (vehicle.faction_id != aiFactionId)
				continue;
			
			if (!isCombatVehicle(vehicleId))
				continue;
			
			if (getVehicleRemainingMoves(vehicleId) <= 0)
				continue;
			
			debug("\t%s\n", getVehicleLocationIdNameString(vehicleId));
			
			CombatAction combatAction = selectVehicleCombatAction(vehicleId);
			debug("\t-> %s => %s gain=%5.2f", getLocationString(combatAction.destination), getLocationString(combatAction.target), combatAction.gain);
			
			if (combatAction.gain > bestCombatAction.gain)
			{
				bestCombatAction = combatAction;
				debug("\t\t- best vehicle\n");
			}
			else
			{
				debug("\n");
			}
			
		}
		
		if (bestCombatAction.vehicleId == -1)
			break;
		
		TaskType taskType = bestCombatAction.target == nullptr ? TT_MOVE : bestCombatAction.engagementMode == EM_MELEE ? TT_MELEE_ATTACK : TT_ARTILLERY_ATTACK;
		aiData.tacticalTasks.emplace(Vehs[bestCombatAction.vehicleId].pad_0, Task(bestCombatAction.vehicleId, taskType, bestCombatAction.destination, bestCombatAction.target));
		mod_enemy_veh(bestCombatAction.vehicleId);
		
	}
	
}

