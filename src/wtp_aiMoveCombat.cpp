#pragma GCC diagnostic ignored "-Wshadow"

#include <map>
#include "wtp_aiMoveCombat.h"
#include "wtp_aiTask.h"
#include "wtp_aiMove.h"
#include "wtp_ai.h"
#include "wtp_aiRoute.h"

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

// 	TaskPriority

TaskPriority::TaskPriority(int _vehicleId, double _priority, TaskType _taskType, MAP *_destination)
: vehicleId{_vehicleId}, priority{_priority}, taskType{_taskType}, destination{_destination}
{

}

TaskPriority::TaskPriority(int _vehicleId, double _priority, TaskType _taskType, MAP *_destination, double _travelTime)
: vehicleId{_vehicleId}, priority{_priority}, taskType{_taskType}, destination{_destination}, travelTime{_travelTime}
{

}

TaskPriority::TaskPriority(int _vehicleId, double _priority, TaskType _taskType, MAP *_destination, double _travelTime, int _baseId)
: vehicleId{_vehicleId}, priority{_priority}, taskType{_taskType}, destination{_destination}, travelTime{_travelTime}, baseId{_baseId}
{

}

TaskPriority::TaskPriority
(
	int _vehicleId,
	double _priority,
	TaskType _taskType,
	MAP *_destination,
	double _travelTime,
	COMBAT_MODE _combatMode,
	MAP *_attackTarget,
	bool _destructive,
	double _effect
)
:
	vehicleId{_vehicleId},
	priority{_priority},
	taskType{_taskType},
	destination{_destination},
	travelTime{_travelTime},
	combatMode{_combatMode},
	attackTarget{_attackTarget},
	destructive{_destructive},
	effect{_effect}
{

}

/*
Prepares combat orders.
*/
void moveCombatStrategy()
{
	executionProfiles["1.5.4. moveCombatStrategy"].start();
	
	// compute strategy
	
	movePolice2x();
	moveProtectors();
	movePolice();
	
	immediateAttack();
	
	moveCombat();
	coordinateAttack();
	
	executionProfiles["1.5.4. moveCombatStrategy"].stop();
	
}

void movePolice2x()
{
	debug("movePolice2x - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	populatePolice2xTasks(taskPriorities);
	
	// sort vehicle available tasks
	
	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
	
	if (DEBUG)
	{
		debug("\tsortedTasks\n");
		
		for (TaskPriority const &taskPriority : taskPriorities)
		{
			debug
			(
				"\t\t%5.2f:"
				" [%4d] %s -> %s"
				"\n"
				, taskPriority.priority
				, taskPriority.vehicleId
				, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
				, getLocationString(taskPriority.destination).c_str()
			);
			
		}
		
	}
	
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
		
		if (baseInfo.policeData.isSatisfied2x())
			continue;
		
		// assign to base
		
		baseInfo.addProtector(taskPriority.vehicleId);
		vehicleAssignments.insert({taskPriority.vehicleId, &taskPriority});
		
		debug
		(
			"\t\t%5.2f:"
			" [%4d] %s -> %s"
			" %-25s"
			" baseInfo.isSatisfied=%d"
			"\n"
			, taskPriority.priority
			, taskPriority.vehicleId
			, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
			, getLocationString(taskPriority.destination).c_str()
			, getBase(taskPriority.baseId)->name
			, baseInfo.isSatisfied(taskPriority.vehicleId, false)
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
	
	if (DEBUG)
	{
		debug("\tsortedTasks\n");
		
		for (TaskPriority const &taskPriority : taskPriorities)
		{
			debug
			(
				"\t\t%5.2f:"
				" [%4d] %s -> %s"
				"\n"
				, taskPriority.priority
				, taskPriority.vehicleId
				, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
				, getLocationString(taskPriority.destination).c_str()
			);
			
		}
		
	}
	
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
		
		if (baseInfo.policeData.isSatisfied2x())
			continue;
		
		// assign to base
		
		baseInfo.addProtector(taskPriority.vehicleId);
		vehicleAssignments.insert({taskPriority.vehicleId, &taskPriority});
		
		debug
		(
			"\t\t%5.2f:"
			" [%4d] %s -> %s"
			" %-25s"
			" baseInfo.isSatisfied=%d"
			"\n"
			, taskPriority.priority
			, taskPriority.vehicleId
			, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
			, getLocationString(taskPriority.destination).c_str()
			, getBase(taskPriority.baseId)->name
			, baseInfo.isSatisfied(taskPriority.vehicleId, false)
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

void moveProtectors()
{
	debug("moveProtectors - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	populateProtectorTasks(taskPriorities);
	
	// sort vehicle available tasks
	
	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
	
	if (DEBUG)
	{
		debug("\tsortedTasks\n");
		
		for (TaskPriority const &taskPriority : taskPriorities)
		{
			debug
			(
				"\t\t%5.2f:"
				" [%4d] %s -> %s"
				"\n"
				, taskPriority.priority
				, taskPriority.vehicleId
				, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
				, getLocationString(taskPriority.destination).c_str()
			);
			
		}
		
	}
	
	// select tasks
	
	debug("\tselected tasks\n");
	
	robin_hood::unordered_flat_map<int, TaskPriority *> vehicleAssignments;
	
	for (TaskPriority &taskPriority : taskPriorities)
	{
		// skip already assigned vehicles
		
		if (vehicleAssignments.find(taskPriority.vehicleId) != vehicleAssignments.end())
			continue;
		
		// base police and protection should not be yet satisfied
		
		BaseInfo &baseInfo = aiData.getBaseInfo(taskPriority.baseId);
		
		if (baseInfo.isSatisfied(taskPriority.vehicleId, false))
			continue;
		
		// assign to base
		
		baseInfo.addProtector(taskPriority.vehicleId);
		vehicleAssignments.insert({taskPriority.vehicleId, &taskPriority});
		
		debug
		(
			"\t\t%5.2f:"
			" [%4d] %s -> %s"
			" %-25s"
			" baseInfo.isSatisfied=%d"
			"\n"
			, taskPriority.priority
			, taskPriority.vehicleId
			, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
			, getLocationString(taskPriority.destination).c_str()
			, getBase(taskPriority.baseId)->name
			, baseInfo.isSatisfied(taskPriority.vehicleId, false)
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
	debug("immediateAttack - %s\n", playerMFaction->noun_faction);
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
//		debug("\t[%4d] %s %-32s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), vehicle->name());
		
		// select best attack priority for each vehicle
		
		bool selected = false;
		TaskType selectedTaskType;
		MAP *selectedPosition;
		MAP *selectedTarget;
		double selectedPriority = 0.0;
		
		for (AttackAction const &attackAction : getMeleeAttackActions(vehicleId))
		{
			TaskType taskType = TT_MELEE_ATTACK;
			
			if (!aiData.isEnemyStackAt(attackAction.target))
				continue;
			
			EnemyStackInfo const &enemyStackInfo = aiData.getEnemyStackInfo(attackAction.target);
			
			if (!enemyStackInfo.isUnitCanMeleeAttackStack(vehicle->unit_id, attackAction.position))
				continue;
			
			COMBAT_MODE combatMode = CM_MELEE;
			double effect = attackAction.hastyCoefficient * enemyStackInfo.getVehicleOffenseEffect(vehicleId, combatMode);
			double priority = enemyStackInfo.priority * effect;
			
			if (priority > selectedPriority)
			{
				selected = true;
				selectedTaskType = taskType;
				selectedPosition = attackAction.position;
				selectedTarget = attackAction.target;
				selectedPriority = priority;
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
		
		for (AttackAction const &attackAction : getArtilleryAttackActions(vehicleId))
		{
			TaskType taskType = TT_LONG_RANGE_FIRE;
			
			if (!aiData.isEnemyStackAt(attackAction.target))
				continue;
			
			EnemyStackInfo const &enemyStackInfo = aiData.getEnemyStackInfo(attackAction.target);
			
			if (!enemyStackInfo.isUnitCanArtilleryAttackStack(vehicle->unit_id, attackAction.position))
				continue;
			
			double effect;
			double priority;
			
			if (enemyStackInfo.artillery)
			{
				COMBAT_MODE combatMode = CM_ARTILLERY_DUEL;
				effect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, combatMode);
				priority = enemyStackInfo.priority * effect;
				
			}
			else if (enemyStackInfo.bombardment)
			{
				COMBAT_MODE combatMode = CM_BOMBARDMENT;
				effect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, combatMode) + 1.0;
				priority = enemyStackInfo.priority * effect;
				
			}
			else
			{
				continue;
			}
			
			if (priority > selectedPriority)
			{
				selected = true;
				selectedTaskType = taskType;
				selectedPosition = attackAction.position;
				selectedTarget = attackAction.target;
				selectedPriority = priority;
			}
			
			debug
			(
				"\t\tartyl -> %s => %s"
				" effect=%5.2f"
				" priority=%5.2f"
				"\n"
				, getLocationString(attackAction.position).c_str(), getLocationString(attackAction.target).c_str()
				, effect
				, priority
			);
			
		}
		
		if (!selected)
			continue;
		
		if (selectedPriority < MIN_IMMEDIATE_ATTACK_PRIORITY)
			continue;
		
		// set task
		
		setTask(Task(vehicleId, selectedTaskType, selectedPosition, selectedPosition, selectedTarget));
		
		debug
		(
			"\t[%4d] %s %-32s"
			"%-5s"
			" -> %s"
			" => %s"
			" priority=%5.2f"
			"\n"
			, vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), vehicle->name()
			, Task::typeName(selectedTaskType).c_str()
			, getLocationString(selectedPosition).c_str()
			, getLocationString(selectedTarget).c_str()
			, selectedPriority
		);
		
	}
	
}

void moveCombat()
{
	debug("moveCombat - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	
	populateMonolithPromotionTasks(taskPriorities);
	populateRepairTasks(taskPriorities);
	populateLandPodPoppingTasks(taskPriorities);
	populateSeaPodPoppingTasks(taskPriorities);
	populateEmptyBaseCaptureTasks(taskPriorities);
	populateEnemyStackAttackTasks(taskPriorities);
	
	// sort available tasks
	
	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
	
	if (DEBUG)
	{
		debug("\tsortedTasks\n");
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			debug
			(
				"\t\t%5.2f:"
				" [%4d] %s"
				" -> %s"
				" => %s"
				"\n"
				, taskPriority.priority
				, taskPriority.vehicleId
				, getLocationString(getVehicleMapTile(taskPriority.vehicleId)).c_str()
				, getLocationString(taskPriority.destination).c_str()
				, getLocationString(taskPriority.attackTarget).c_str()
			);
			
		}
		
	}
	
	// select tasks
	
	debug("\tselected tasks\n");
	
	robin_hood::unordered_flat_set<EnemyStackInfo *> excludedEnemyStacks;
	robin_hood::unordered_flat_map<int, TaskPriority *> vehicleAssignments;
	
	while (true)
	{
		// clear assignments
		
		vehicleAssignments.clear();
		
		for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackInfoEntry : aiData.enemyStacks)
		{
			EnemyStackInfo &enemyStackInfo = enemyStackInfoEntry.second;
			enemyStackInfo.resetAttackParameters();
		}
		
		// iterate task priorities
		
		robin_hood::unordered_flat_set<MAP *> targetedPods;
		robin_hood::unordered_flat_set<MAP *> targetedEmptyBases;
		robin_hood::unordered_flat_set<EnemyStackInfo *> targetedEnemyStacks;
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			int vehicleId = taskPriority.vehicleId;
			double travelTime = taskPriority.travelTime;
			MAP *destination = taskPriority.destination;
			MAP *attackTarget = taskPriority.attackTarget;
			COMBAT_MODE combatMode = taskPriority.combatMode;
			bool destructive = taskPriority.destructive;
			double effect = taskPriority.effect;
			
			// skip already assigned vehicles
			
			if (vehicleAssignments.count(vehicleId) != 0)
				continue;
			
			if (DEBUG)
			{
				debug
				(
					"\t\t[%4d]"
					" %s"
					" %s"
					" -> %s"
					" => %s"
					" combatMode=%d"
					" destructive=%d"
					" effect=%5.2f"
					"\n"
					, vehicleId
					, getLocationString(getVehicleMapTile(vehicleId)).c_str()
					, Task::typeName(taskPriority.taskType).c_str()
					, getLocationString(destination).c_str()
					, getLocationString(attackTarget).c_str()
					, combatMode
					, destructive
					, effect
				);
			}
			
			if (isPodAt(destination)) // pod
			{
				// skip already targeted pod
				
				if (targetedPods.count(destination) != 0)
					continue;
				
				// set vehicle task
				
				vehicleAssignments.insert({vehicleId, &taskPriority});
				
				// add targeted pod
				
				targetedPods.insert(destination);
				
			}
			else if (isEmptyHostileBaseAt(destination, aiFactionId) || isEmptyNeutralBaseAt(destination, aiFactionId)) // empty base
			{
				debug("\t\t\tempty base\n");
				
				// skip already targeted empty base
				
				if (targetedEmptyBases.count(destination) != 0)
				{
					debug("\t\t\t\ttargeted\n");
					continue;
				}
				
				// set vehicle taskPrority
				
				vehicleAssignments.insert({vehicleId, &taskPriority});
				
				// add targeted empty base
				
				targetedEmptyBases.insert(destination);
				
			}
			else if (attackTarget != nullptr && aiData.enemyStacks.count(attackTarget) != 0) // enemy stack
			{
				EnemyStackInfo *enemyStack = &(aiData.enemyStacks.at(attackTarget));
				
				// skip excluded stack
				
				if (excludedEnemyStacks.find(enemyStack) != excludedEnemyStacks.end())
					continue;
				
				// try to add vehicle to stack attackers
				
				if (!enemyStack->addAttacker({vehicleId, travelTime}))
					continue;
				
				// set vehicle taskPriority
				
				vehicleAssignments.insert({vehicleId, &taskPriority});
				
				// add targeted enemy stack
				
				targetedEnemyStacks.insert(enemyStack);
				
			}
			else
			{
				// set vehicle task
				
				vehicleAssignments.insert({vehicleId, &taskPriority});
				
			}
			
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
				, getLocationString(enemyStack->tile).c_str()
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
		
		debug("\tmost untargeted enemy stack: %s\n", getLocationString(mostUndertargetedEnemyStack->tile).c_str());
		
	}
	
	// optimize pod popping traveling
	
	debug("optimize pod popping traveling - %s\n", playerMFaction->noun_faction);
	
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
				
				int triad1 = Vehicles[vehicleId1].triad();
				int triad2 = Vehicles[vehicleId2].triad();
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
				double travelTime12 = getVehicleATravelTime(vehicleId1, taskPriority2->destination);
				double travelTime21 = getVehicleATravelTime(vehicleId2, taskPriority1->destination);
				
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
					, getLocationString(vehicleTile1).c_str(), getLocationString(taskPriority1->destination).c_str()
					, getLocationString(vehicleTile2).c_str(), getLocationString(taskPriority2->destination).c_str()
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
if (i == 900)
{
	debug("ERROR: too many optimization iteration."); exit(1);
}
			
		}
		
	}
	
	// execute tasks
	
	debug("\tselectedTasks\n");
	
	for (robin_hood::pair<int, TaskPriority *> &vehicleAssignmentEntry : vehicleAssignments)
	{
		int vehicleId = vehicleAssignmentEntry.first;
		TaskPriority &taskPriority = *(vehicleAssignmentEntry.second);

		if (DEBUG)
		{
			debug
			(
				"\t\t[%4d] %s"
				" -> %s"
				" => %s"
				"\n"
				, vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str()
				, getLocationString(taskPriority.destination).c_str()
				, getLocationString(taskPriority.attackTarget).c_str()
			);
			
		}
		
		if (!transitVehicle(Task(vehicleId, taskPriority.taskType, taskPriority.destination, taskPriority.destination, taskPriority.attackTarget)))
		{
			debug("ERROR: transitVehicle failed."); exit(1);
		}
		
		
		if (DEBUG)
		{
			debug
			(
				"\t\t[%4d] %s"
				" -> %s"
				" => %s"
				"\n"
				, vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str()
				, getLocationString(taskPriority.destination).c_str()
				, getLocationString(taskPriority.attackTarget).c_str()
			);
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("targetedEnemyStacks - %s\n", playerMFaction->noun_faction);
		
		for (robin_hood::pair<MAP *, EnemyStackInfo> const &enemyStackEntry : aiData.enemyStacks)
		{
			EnemyStackInfo const &enemyStack = enemyStackEntry.second;
			
			debug
			(
				"\t%s"
				" requiredSuperiority=%5.2f"
				" desiredSuperiority=%5.2f"
				" maxBombardmentEffect=%5.2f"
				" combinedBombardmentEffect=%5.2f"
				" combinedDirectEffect=%5.2f"
				" sufficient: %d %d"
				"\n"
				, getLocationString(enemyStack.tile).c_str()
				, enemyStack.requiredSuperiority
				, enemyStack.desiredSuperiority
				, enemyStack.maxBombardmentEffect
				, enemyStack.combinedBombardmentEffect
				, enemyStack.combinedDirectEffect
				, enemyStack.isSufficient(false)
				, enemyStack.isSufficient(true)
			);
			
			debug("\tattackers\n");
			
			for (IdDoubleValue vehicleIdTravelTile : enemyStack.bombardmentVechileTravelTimes)
			{
				int vehicleId = vehicleIdTravelTile.id;
				double travelTime = vehicleIdTravelTile.value;
				MAP *vehicleTile = getVehicleMapTile(vehicleId);
				Task *task = getTask(vehicleId);
				
				debug
				(
					"\t\t[%4d] %s"
					" task=%d"
					" travelTime=%5.2f"
					"\n"
					, vehicleId, getLocationString(vehicleTile).c_str()
					, (int)task
					, travelTime
				);
				
			}
			
			for (IdDoubleValue vehicleIdTravelTile : enemyStack.directVechileTravelTimes)
			{
				int vehicleId = vehicleIdTravelTile.id;
				double travelTime = vehicleIdTravelTile.value;
				MAP *vehicleTile = getVehicleMapTile(vehicleId);
				Task *task = getTask(vehicleId);
				
				debug
				(
					"\t\t[%4d] %s"
					" task=%d"
					" travelTime=%5.2f"
					"\n"
					, vehicleId, getLocationString(vehicleTile).c_str()
					, (int)task
					, travelTime
				);
				
			}
			
		}
		
	}
	
}

void populateMonolithPromotionTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateMonolithPromotionTasks\n");
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int triad = vehicle->triad();
		
		// not air
		
		if (triad == TRIAD_AIR)
			continue;
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// exclude battle ogres
		
		switch (vehicle->unit_id)
		{
		case BSC_BATTLE_OGRE_MK1:
		case BSC_BATTLE_OGRE_MK2:
		case BSC_BATTLE_OGRE_MK3:
			continue;
		}
		
		// exclude already promoted
		
		if ((vehicle->state & VSTATE_MONOLITH_UPGRADED) != 0)
			continue;
		
		debug("\t\t[%4d] %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str());
		
		// find closest monolith
		
		MapValue closestMonolithLocaionTravelTime = findClosestItemLocation(vehicleId, BIT_MONOLITH, MAX_REPAIR_DISTANCE, true);
		
		// not found
		
		if (closestMonolithLocaionTravelTime.tile == nullptr)
		{
			debug("\t\t\tmonolith location is not found\n");
			continue;
		}
		
		// time and timeCoefficient
		
		double travelTime = std::max(1.0, closestMonolithLocaionTravelTime.value);
		double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
		
		// priority
		
		double priority =
			conf.ai_combat_priority_monolith_promotion
			* travelTimeCoefficient
		;
		
		debug
		(
			"\t\t\t-> %s"
			" priority=%5.2f"
			" ai_combat_priority_monolith_promotion=%5.2f"
			" travelTime=%5.2f"
			" travelTimeCoefficient=%5.2f"
			, getLocationString(closestMonolithLocaionTravelTime.tile).c_str()
			, priority
			, conf.ai_combat_priority_monolith_promotion
			, travelTime
			, travelTimeCoefficient
		);

		// add task

		taskPriorities.emplace_back(vehicleId, priority, TT_MOVE, closestMonolithLocaionTravelTime.tile, travelTime);

	}

}

void populateRepairTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateRepairTasks\n");

	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		// exclude unavailable

		if (hasTask(vehicleId))
			continue;

		// exclude battle ogres

		switch (vehicle->unit_id)
		{
		case BSC_BATTLE_OGRE_MK1:
		case BSC_BATTLE_OGRE_MK2:
		case BSC_BATTLE_OGRE_MK3:
			continue;
		}

		// vehicle should be damaged

		if (vehicle->damage_taken == 0)
			continue;

		debug("\t\t%s\n", getLocationString(getVehicleMapTile(vehicleId)).c_str());

		// scan nearby location for best repair rate

		MAP *bestRepairLocaion = nullptr;
		double bestRepairPriority = 0.0;

		for (MAP *tile : getRangeTiles(vehicleTile, MAX_REPAIR_DISTANCE, true))
		{
			int x = getX(tile), y = getY(tile);
			bool ocean = is_ocean(tile);
			TileInfo &tileInfo = aiData.getTileInfo(tile);

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
				// land
				if (ocean)
					continue;
				break;

			}

			// exclude too far field location

			if (!(map_has_item(tile, BIT_BASE_IN_TILE | BIT_BUNKER | BIT_MONOLITH)) && map_range(vehicle->x, vehicle->y, x, y) > 2)
				continue;

			// exclude blocked

			if (isBlocked(tile))
				continue;

			// exclude unreachable locations

			if (!isVehicleDestinationReachable(vehicleId, tile))
				continue;

			// get repair parameters

			RepairInfo repairInfo = getVehicleRepairInfo(vehicleId, tile);

			// no repair happening

			if (repairInfo.damage <= 0)
				continue;

			// repair priority coefficient

			double repairPriorityCoefficient = (repairInfo.full ? conf.ai_combat_priority_repair : conf.ai_combat_priority_repair_partial);

			// calculate travel time and total time

			double travelTime = getVehicleATravelTime(vehicleId, tile);

			if (travelTime == -1 || travelTime >= INT_INFINITY)
				continue;

			double totalTime = std::max(1.0, travelTime + (double)repairInfo.time);

			// calculate repair rate

			double repairRate = (double)repairInfo.damage / totalTime;

			// warzone coefficient

			double warzoneCoefficient = (tileInfo.warzone ? 0.7 : 1.0);

			// get repairPriority

			double repairPriority = repairPriorityCoefficient * repairRate * warzoneCoefficient;

			debug
			(
				"\t\t\t-> %s"
				" repairPriority=%5.2f"
				" repairPriorityCoefficient=%5.2f"
				" travelTime=%5.2f"
				" totalTime=%5.2f"
				" repairRate=%5.2f"
				" warzoneCoefficient=%5.2f"
				"\n"
				, getLocationString({x, y}).c_str()
				, repairPriority
				, repairPriorityCoefficient
				, travelTime
				, totalTime
				, repairRate
				, warzoneCoefficient
			);

			// update best

			if (repairPriority > bestRepairPriority)
			{
				bestRepairLocaion = tile;
				bestRepairPriority = repairPriority;
			}

		}

		// not found

		if (bestRepairLocaion == nullptr)
		{
			debug("\t\trepair location is not found\n");
			continue;
		}

		// calculate repair priority

		double repairPriority = bestRepairPriority;

		// calculate promotion priority

		double promotionPriority = 0.0;

		if (map_has_item(bestRepairLocaion, BIT_MONOLITH) && ((vehicle->state & VSTATE_MONOLITH_UPGRADED) == 0))
		{
			promotionPriority = conf.ai_combat_priority_monolith_promotion;
		}

		// summarize priority

		double priority = repairPriority + promotionPriority;

		debug
		(
			"\t\t-> %s"
			" priority=%5.2f"
			", conf.ai_combat_priority_repair=%5.2f"
			", repairPriority=%5.2f"
			", promotionPriority=%5.2f\n"
			, getLocationString(bestRepairLocaion).c_str()
			, priority
			, conf.ai_combat_priority_repair
			, repairPriority
			, promotionPriority
		);

		// add task

		taskPriorities.push_back({vehicleId, priority, TT_SKIP, bestRepairLocaion});

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
		
		// police2x
		
		if (!isPolice2xVehicle(vehicleId))
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
			
			double travelTime = getVehicleATravelTime(vehicleId, baseTile);
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale_base_protection, travelTime);
			
			// priority
			
			double priority = requiredPower * travelTimeCoefficient;
			
			// add task
			
			TaskPriority taskPriority(vehicleId, priority, TT_HOLD, baseTile);
			taskPriority.baseId = baseId;
			taskPriorities.push_back(taskPriority);
			
			debug
			(
				"\t\t[%4d] (%3d,%3d) %-32s -> %-25s"
				" priority=%5.2f"
				" requiredPower=%5.2f"
				" travelTime=%5.2f"
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
			
			double travelTime = getVehicleATravelTime(vehicleId, baseTile);
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale_base_protection, travelTime);
			
			// priority
			
			double priority = travelTimeCoefficient;
			
			// add task
			
			TaskPriority taskPriority(vehicleId, priority, TT_HOLD, baseTile);
			taskPriority.baseId = baseId;
			taskPriorities.push_back(taskPriority);
			
			debug
			(
				"\t\t[%4d] (%3d,%3d) %-32s -> %-25s"
				" priority=%5.2f"
				" travelTime=%5.2f"
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
- effect
- travel time
*/
void populateProtectorTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateProtectorTasks\n");
	
	if (aiData.baseIds.size() == 0)
		return;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// infantry defender
		
		if (!isInfantryDefensiveVehicle(vehicleId))
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
			
			double requiredEffect = baseInfo.combatData.requiredEffect;
			
			// vehicle effect
			
			double vehicleEffect = baseInfo.combatData.getVehicleEffect(vehicleId);
			
			// damageCoefficient
			// more damaged units have priority
			
			double damageCoefficient = 1.0 + getVehicleRelativeDamage(vehicleId);
			
			// get travel time and coresponding coefficient
			
			double travelTime = getVehicleATravelTime(vehicleId, baseTile);
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale_base_protection, travelTime);
			
			// priority
			
			double priority =
				requiredEffect
				* vehicleEffect
				* damageCoefficient
				* travelTimeCoefficient
			;
			
			// add task
			
			TaskPriority taskPriority(vehicleId, priority, TT_HOLD, baseTile);
			taskPriority.baseId = baseId;
			taskPriorities.push_back(taskPriority);
			
			debug
			(
				"\t\t[%4d] (%3d,%3d) %-32s -> %-25s"
				" priority=%5.2f"
				" requiredEffect=%5.2f"
				" vehicleEffect=%5.2f"
				" damageCoefficient=%5.2f"
				" travelTime=%5.2f"
				" travelTimeCoefficient=%5.2f"
				"\n"
				, vehicleId, vehicle->x, vehicle->y, getVehicleUnitName(vehicleId), base->name
				, priority
				, requiredEffect
				, vehicleEffect
				, damageCoefficient
				, travelTime
				, travelTimeCoefficient
			);
			
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
		
		debug("\t\t[%4d] %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str());
		
		for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
		{
			MAP *enemyStackTile = enemyStackEntry.first;
			EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
			
			debug("\t\t\t-> %s\n", getLocationString(enemyStackTile).c_str());
			
			// land artillery do not attack fungal tower
			
			if (isLandArtilleryVehicle(vehicleId) && enemyStackInfo.alienFungalTower)
				continue;
			
			// ship do not attack fungal tower
			
			if (vehicle->triad() == TRIAD_SEA && enemyStackInfo.alienFungalTower)
				continue;
			
			// attack position and travel time
			
			MAP *meleeAttackPosition = nullptr;
			double meleeAttackTravelTime = INF;
			
			if (enemyStackInfo.isUnitCanMeleeAttackStack(vehicle->unit_id))
			{
				meleeAttackPosition = getMeleeAttackPosition(vehicleId, enemyStackTile);
				
				if (meleeAttackPosition != nullptr)
				{
					meleeAttackTravelTime = getVehicleATravelTime(vehicleId, meleeAttackPosition);
				}
				
			}
			
			MAP *artilleryAttackPosition = nullptr;
			double artilleryAttackTravelTime = INF;
			
			if (enemyStackInfo.isUnitCanArtilleryAttackStack(vehicle->unit_id))
			{
				artilleryAttackPosition = getVehicleArtilleryAttackPosition(vehicleId, enemyStackTile);
				
				if (artilleryAttackPosition != nullptr)
				{
					artilleryAttackTravelTime = getVehicleATravelTime(vehicleId, artilleryAttackPosition);
				}
				
			}
			
			// worth chasing
			
			if
			(
				enemyStackInfo.baseOrBunker
				||
				enemyStackTile->owner == aiFactionId
				||
				(meleeAttackTravelTime <= 1.0)
				||
				(artilleryAttackTravelTime <= 1.0)
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
			
			if (meleeAttackPosition != nullptr)
			{
				debug("\n");
				
				COMBAT_MODE combatMode = CM_MELEE;
				bool destructive = true;
				double combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, CM_MELEE);
				combatEffect = std::min(2.0, combatEffect);
				
				// travel time coefficient
				
				MAP *destination = meleeAttackPosition;
				double travelTime = getVehicleATravelTime(vehicleId, destination);
				
				double travelTimeCoefficient = travelTime == INF ? 0.0 : getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				
				// priority
				
				double priority = enemyStackInfo.priority * combatEffect * travelTimeCoefficient;
				
				debug
				(
					"\t\t\t\t%-15s"
					" priority=%5.2f"
					" enemyStackInfo.priority=%5.2f"
					" combatEffect=%5.2f"
					" travelTime=%5.2f"
					" travelTimeCoefficient=%5.2f"
					"\n"
					, "direct"
					, priority
					, enemyStackInfo.priority
					, combatEffect
					, travelTime
					, travelTimeCoefficient
				);
				
				taskPriorities.push_back
				(
						{
							vehicleId,
							priority,
							TT_MELEE_ATTACK,
							destination,
							travelTime,
							combatMode,
							enemyStackTile,
							destructive,
							combatEffect,
						}
				);
				
			}
			
			if (artilleryAttackPosition != nullptr && enemyStackInfo.artillery)
			{
				COMBAT_MODE combatMode = CM_ARTILLERY_DUEL;
				bool destructive = true;
				double combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, CM_ARTILLERY_DUEL);
				
				// travel time coefficient
				
				MAP *destination = artilleryAttackPosition;
				double travelTime = getVehicleATravelTime(vehicleId, destination);
				
				double travelTimeCoefficient = travelTime == INF ? 0.0 : getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				
				// priority
				
				double priority = enemyStackInfo.priority * combatEffect * travelTimeCoefficient;
				
				debug
				(
					"\t\t\t\t%-15s"
					" priority=%5.2f"
					" enemyStackInfo.priority=%5.2f"
					" combatEffect=%5.2f"
					" travelTime=%5.2f"
					" travelTimeCoefficient=%5.2f"
					"\n"
					, "artillery duel"
					, priority
					, enemyStackInfo.priority
					, combatEffect
					, travelTime
					, travelTimeCoefficient
				);
				
				taskPriorities.push_back
				(
						{
							vehicleId,
							priority,
							TT_LONG_RANGE_FIRE,
							destination,
							travelTime,
							combatMode,
							enemyStackTile,
							destructive,
							combatEffect,
						}
				);
				
			}
			
			if (artilleryAttackPosition != nullptr && !enemyStackInfo.artillery && enemyStackInfo.bombardment)
			{
				COMBAT_MODE combatMode = CM_BOMBARDMENT;
				bool destructive = enemyStackInfo.bombardmentDestructive;
				double combatEffect = enemyStackInfo.getVehicleOffenseEffect(vehicleId, CM_BOMBARDMENT);
				
				// bombardment combatEffect adjustment for the purpose of prioritization
				
				combatEffect = 1.0 + 4.0 * combatEffect;
				
				// travel time coefficient
				
				MAP *destination = artilleryAttackPosition;
				double travelTime = getVehicleATravelTime(vehicleId, destination);
				
				double travelTimeCoefficient = travelTime == INF ? 0.0 : getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				
				// priority
				
				double priority = enemyStackInfo.priority * combatEffect * travelTimeCoefficient;
				
				debug
				(
					"\t\t\t\t%-15s"
					" priority=%5.2f"
					" enemyStackInfo.priority=%5.2f"
					" combatEffect=%5.2f"
					" travelTime=%5.2f"
					" travelTimeCoefficient=%5.2f"
					"\n"
					, "bombardment"
					, priority
					, enemyStackInfo.priority
					, combatEffect
					, travelTime
					, travelTimeCoefficient
				);
				
				taskPriorities.push_back
				(
						{
							vehicleId,
							priority,
							TT_LONG_RANGE_FIRE,
							destination,
							travelTime,
							combatMode,
							enemyStackTile,
							destructive,
							combatEffect,
						}
				);
				
			}
			
		}
		
	}
	
}

void populateEmptyBaseCaptureTasks(std::vector<TaskPriority> &taskPriorities)
{
	bool TRACE = DEBUG && false;

	debug("\tpopulateEmptyBaseCaptureTasks TRACE=%d\n", TRACE);

	// attack priority coefficient

	double attackPriority = 2.0 * conf.ai_combat_attack_priority_base;

	// iterate vehicles and bases

	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);

		// exclude unavailable

		if (hasTask(vehicleId))
			continue;

		debug("\t\t[%4d] %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str());

		for (MAP *enemyBaseTile : aiData.emptyEnemyBaseTiles)
		{
			debug("\t\t\t-> %s\n", getLocationString(enemyBaseTile).c_str());

			// can capture this base

			if (!isVehicleCanCaptureBase(vehicleId, enemyBaseTile))
				continue;

			// travel time

			double travelTime = getVehicleATravelTime(vehicleId, enemyBaseTile);

			if (travelTime == INF)
				continue;

			// do not capture base farther than 10 turn travel time

			if (travelTime > 10)
				continue;

			// do not capture base if movement is severely impacted

			int vehicleSpeed = getVehicleSpeed(vehicleId);
			int vehicleUnitSpeed = getUnitSpeed(vehicle->faction_id, vehicle->unit_id);

			double vehicleUnitTravelTime = travelTime * (double)vehicleSpeed / (double)vehicleUnitSpeed;

			debug
			(
				"\t\t\tvehicleSpeed=%2d"
				" vehicleUnitSpeed=%2d"
				" vehicleUnitTravelTime=%5.2f"
				" travelTime=%5.2f"
				, vehicleSpeed
				, vehicleUnitSpeed
				, vehicleUnitTravelTime
				, travelTime
			);

			if (travelTime - vehicleUnitTravelTime > 5.0)
				continue;

			// travelTime coefficient

			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);

			// priority

			double priority = attackPriority * travelTimeCoefficient;

			// add task

			taskPriorities.push_back({vehicleId, priority, TT_MOVE, enemyBaseTile, travelTime});

			if (TRACE)
			{
				debug
				(
					"\t\t\t->%s"
					" priority=%5.2f"
					" attackPriority=%5.2f"
					" travelTime=%5.2f travelTimeCoefficient=%5.2f\n"
					, getLocationString(enemyBaseTile).c_str()
					, priority
					, attackPriority
					, travelTime, travelTimeCoefficient
				);
			}

		}

	}

}

void populateLandPodPoppingTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateLandPodPoppingTasks\n");
	
	// collect reachable land transported clusters
	
	robin_hood::unordered_flat_set<int> reachableLandTransfportedClusters;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();
		
		// land
		
		if (triad != TRIAD_LAND)
			continue;
		
		// pod popping vehicle
		
		if (!isPodPoppingVehicle(vehicleId))
			continue;
		
		// get land transfported cluster
		
		int landTransportedCluster = getLandTransportedCluster(vehicleTile);
		
		reachableLandTransfportedClusters.insert(landTransportedCluster);
		
	}
	
	std::vector<MAP *> podLocations;

	// collect land pods
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int landTransportedCluster = getLandTransportedCluster(tile);
		
		// land
		
		if (is_ocean(tile))
			continue;
		
		// pod
		
		if (isPodAt(tile) == 0)
			continue;
		
		// friendly territory
		
		if (isUnfriendlyTerritory(aiFactionId, tile))
			continue;
		
		// reachable
		
		if (reachableLandTransfportedClusters.find(landTransportedCluster) == reachableLandTransfportedClusters.end())
			continue;
		
		// not blocked location
		
		if (isBlocked(tile))
			continue;
		
		// store pod location
		
		podLocations.push_back(tile);
		
	}
	
	// iterate available scouts
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// land triad
		
		if (vehicle->triad() != TRIAD_LAND)
			continue;
		
		// not damaged
		
		if (isVehicleCanHealAtThisLocation(vehicleId))
			continue;
		
		// compute base priority
		
		double basePriority = conf.ai_combat_priority_pod;
		
		// iterate pods
		
		for (MAP * podLocation : podLocations)
		{
			// limit search range
			
			if (getRange(vehicleTile, podLocation) > MAX_PODPOP_DISTANCE)
				continue;
			
			// get travel time and coresponding coefficient
			
			double travelTime = getVehicleATravelTime(vehicleId, podLocation);
			
			if (travelTime == INF)
				continue;
			
			// do not consider pods farther than so many turns
			
			if (travelTime > 10.0)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
			
			// adjust priority by travelTimeCoefficient
			
			double priority = basePriority * travelTimeCoefficient;
			
			debug
			(
				"\t\t%s -> %s"
				" priority=%5.2f"
				" basePriority=%5.2f"
				" travelTime=%5.2f travelTimeCoefficient=%5.2f\n"
				, getLocationString({vehicle->x, vehicle->y}).c_str(), getLocationString(podLocation).c_str()
				, priority
				, basePriority
				, travelTime, travelTimeCoefficient
			);
			
			// add task
			
			taskPriorities.push_back({vehicleId, priority, TT_MOVE, podLocation, travelTime});
			
		}
		
	}
	
}

void populateSeaPodPoppingTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateSeaPodPoppingTasks\n");

	// collect reachable sea clusters
	
	robin_hood::unordered_flat_set<int> reachableSeaClusters;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();
		
		// sea
		
		if (triad != TRIAD_SEA)
			continue;
		
		// pod popping vehicle
		
		if (!isPodPoppingVehicle(vehicleId))
			continue;
		
		// get land transfported cluster
		
		int seaCluster = getSeaCluster(vehicleTile);
		
		reachableSeaClusters.insert(seaCluster);
		
	}
	
	// get pod locations
	
	std::vector<MAP *> podLocations;
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int seaCluster = getSeaCluster(tile);
		
		// ocean
		
		if (!is_ocean(tile))
			continue;
		
		// pod
		
		if (isPodAt(tile) == 0)
			continue;
		
		// friendly territory
		
		if (isUnfriendlyTerritory(aiFactionId, tile))
			continue;
		
		// reachable

		if (reachableSeaClusters.find(seaCluster) == reachableSeaClusters.end())
			continue;
		
		// not blocked location
		
		if (isBlocked(tile))
			continue;
		
		// store pod location
		
		podLocations.push_back(tile);
		
	}
	
	// iterate available scouts
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		// sea triad
		
		if (vehicle->triad() != TRIAD_SEA)
			continue;
		
		// not damaged
		
		if (isVehicleCanHealAtThisLocation(vehicleId))
			continue;
		
		// iterate pods
		
		for (MAP * podLocation : podLocations)
		{
			// get travel time and coresponding coefficient
			
			double travelTime = getVehicleATravelTime(vehicleId, podLocation);
			
			if (travelTime == INF)
				continue;
			
			// do not consider pods farther than so many turns
			
			if (travelTime > 10.0)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
			
			// adjust priority by travelTimeCoefficient
			
			double priority = conf.ai_combat_priority_pod * travelTimeCoefficient;
			
			debug
			(
				"\t\t%s -> %s"
				" priority=%5.2f"
				" conf.ai_combat_priority_pod=%5.2f"
				" travelTime=%5.2f travelTimeCoefficient=%5.2f\n"
				, getLocationString({vehicle->x, vehicle->y}).c_str(), getLocationString(podLocation).c_str()
				, priority
				, conf.ai_combat_priority_pod
				, travelTime, travelTimeCoefficient
			);
			
			// add task
			
			taskPriorities.push_back({vehicleId, priority, TT_MOVE, podLocation, travelTime});
			
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
		
		// do not coordinate attach agains mobile aliens
		
		if (enemyStack.alien && !enemyStack.alienFungalTower)
			continue;
		
		debug("\t%s <-\n", getLocationString(enemyStack.tile).c_str());
		
		// hold bombarders if bombarding would breack a treaty
		
		if (enemyStack.breakTreaty)
		{
			for (IdDoubleValue const &vehicleTravelTime : enemyStack.bombardmentVechileTravelTimes)
			{
				int vehicleId = vehicleTravelTime.id;
				double travelTime = vehicleTravelTime.value;
				Task *task = getTask(vehicleId);
				
				task->type = TT_HOLD;
				task->clearDestination();
				debug("\t\t[%4d] %s travelTime=%5.2f - bombardment wait for breaking treaty\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), travelTime);
				
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
				debug("\t\t[%4d] %s travelTime=%5.2f\t\t(on transport)\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), travelTime);
				continue;
			}
			
			// skip not too early vehicles
			
			if (travelTime >= enemyStack.coordinatorTravelTime - 2.0)
			{
				debug("\t\t[%4d] %s travelTime=%5.2f\t\t(behind)\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), travelTime);
				continue;
			}
			
			// hold
			
			Task *task = getTask(vehicleId);
			task->type = TT_HOLD;
			task->clearDestination();
			debug("\t\t[%4d] %s travelTime=%5.2f - melee wait for coordinator\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), travelTime);
			
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
					debug("\t\t[%4d] %s travelTime=%5.2f\t\t(too far)\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), travelTime);
					continue;
				}
				
				// hold too close vehicles
				
				Task *task = getTask(vehicleId);
				task->type = TT_HOLD;
				task->clearDestination();
				debug("\t\t[%4d] %s travelTime=%5.2f - melee wait for bombardment\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), travelTime);
				
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
	return (!hasTask(vehicleId) && !(notAvailableInWarzone && aiData.getTileInfo(getVehicleMapTile(vehicleId)).warzone));
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

