#include <map>
#include <map>
#include "engine.h"
#include "terranx_wtp.h"
#include "wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMove.h"
#include "aiMoveCombat.h"
#include "aiMoveTransport.h"
#include "aiRoute.h"

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

TaskPriority::TaskPriority(double _priority, int _vehicleId, TaskType _taskType, MAP *_destination)
: priority{_priority}, vehicleId{_vehicleId}, taskType{_taskType}, destination{_destination}
{
	
}

TaskPriority::TaskPriority(double _priority, int _vehicleId, TaskType _taskType, MAP *_destination, int _travelTime)
: priority{_priority}, vehicleId{_vehicleId}, taskType{_taskType}, destination{_destination}, travelTime{_travelTime}
{
	
}

TaskPriority::TaskPriority(double _priority, int _vehicleId, TaskType _taskType, MAP *_destination, int _travelTime, int _baseId)
: priority{_priority}, vehicleId{_vehicleId}, taskType{_taskType}, destination{_destination}, travelTime{_travelTime}, baseId{_baseId}
{
	
}

TaskPriority::TaskPriority
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
)
:
	priority{_priority},
	vehicleId{_vehicleId},
	taskType{_taskType},
	destination{_destination},
	travelTime{_travelTime},
	combatType{_combatType},
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
	// compute strategy
	
	holdBasePolice2x();
	
	moveBasePolice2x();
	
	holdBaseProtectors();
	
	moveBaseProtectors();
	
	moveCombat();
//	selectAssemblyLocation();
	coordinateAttack();
	
}

void holdBasePolice2x()
{
	debug("holdBasePolice2x - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int baseId : aiData.baseIds)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		PoliceData &basePoliceData = baseInfo.policeData;
		BaseCombatData &baseCombatData = baseInfo.combatData;
		
		// exclude base without police need
		
		if (basePoliceData.isSatisfied())
			continue;
		
		debug("\t%-25s\n", getBase(baseId)->name);
		
		// iterate police2x vechiles at base
		
		for (int vehicleId : baseInfo.garrison)
		{
			// exclude unavailable
			
			if (hasTask(vehicleId))
				continue;
			
			// police2x
			
			if (!isInfantryDefensivePolice2xVehicle(vehicleId))
				continue;
			
			// set task
			
			setTask(Task(vehicleId, TT_HOLD));
			
			// update provided police
			
			basePoliceData.addVehicle(vehicleId);
			
			// update baseProtection
			
			baseCombatData.addProvidedEffect(vehicleId, true);
			
			debug
			(
				"\t\t[%4d] %-32s"
				" effect=%d"
				" unitsAllowed=%d"
				" unitsProvided=%d"
				" dronesExisting=%d"
				" dronesQuelled=%d"
				" satisfied=%s\n"
				, vehicleId, getVehicle(vehicleId)->name()
				, basePoliceData.effect
				, basePoliceData.unitsAllowed
				, basePoliceData.unitsProvided
				, basePoliceData.dronesExisting
				, basePoliceData.dronesQuelled
				, (basePoliceData.isSatisfied() ? "Y" : "N")
			);
			
			// exit condition
			
			if (basePoliceData.isSatisfied())
				break;
			
		}
		
	}
	
}

void moveBasePolice2x()
{
	debug("moveBasePolice2x - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;

	populateBasePolice2xTasks(taskPriorities);
	
	// sort task priorities
	
	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
	
	if (DEBUG)
	{
		debug("\tsortedTasks\n");
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			int vehicleId = taskPriority.vehicleId;
			MAP *location = getVehicleMapTile(vehicleId);
			MAP *destination = taskPriority.destination;
			
			debug
			(
				"\t\t%5.2f:"
				" (%3d,%3d)"
				" -> (%3d,%3d)"
				"\n"
				, taskPriority.priority
				, getX(location), getY(location)
				, getX(destination), getY(location)
			);
			
		}

	}
	
	// process tasks
	
	debug("\tselectedTasks\n");
	
	for (TaskPriority &taskPriority : taskPriorities)
	{
		int vehicleId = taskPriority.vehicleId;
		VEH *vehicle = getVehicle(vehicleId);
		MAP *destination = taskPriority.destination;
		int baseId = taskPriority.baseId;
		
		// no base at destination ?
		
		if (baseId == -1)
			continue;
		
		BASE *base = getBase(baseId);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		PoliceData &basePoliceData = baseInfo.policeData;
		BaseCombatData &baseCombatData = baseInfo.combatData;
		
		// available
		
		if (hasTask(vehicleId))
			continue;
		
		// skip base not needing police and defence
		
		if (basePoliceData.isSatisfied() && baseCombatData.isSatisfied(false))
			return;
		
		// set task
		
		if (!transitVehicle(Task(vehicleId, TT_HOLD, destination)))
			continue;
		
		debug
		(
			"\t\t[%4d] (%3d,%3d) -> (%3d,%3d)\n"
			, vehicleId, vehicle->x, vehicle->y
			, getX(destination), getY(destination)
		);
		
		// update police and protection provided
		
		basePoliceData.addVehicle(vehicleId);
		baseCombatData.addProvidedEffect(vehicleId, false);
		
		debug
		(
			"\t\t\t%-25s"
			" required=%5.2f"
			" providedEffect=%5.2f"
			" satisfied: %s\n"
			, base->name
			, baseCombatData.requiredEffect
			, baseCombatData.providedEffect
			, (baseCombatData.isSatisfied(false) ? "Yes" : "No")
		);
		
	}
	
}

void holdBaseProtectors()
{
	debug("holdBaseProtectors - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int baseId : aiData.baseIds)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		PoliceData &basePoliceData = baseInfo.policeData;
		BaseCombatData &baseCombatData = baseInfo.combatData;
		
		// find if base is safe building protector
		
		int scoutPatrolBuildTime = getBaseItemBuildTime(baseId, BSC_SCOUT_PATROL, true);
		bool safe = (scoutPatrolBuildTime <= baseInfo.safeTime);
		
		debug
		(
			"\t%-25s"
			" safe=%d"
			" scoutPatrolBuildTime=%2d"
			" baseInfo.safeTime=%2d"
			"\n"
			, getBase(baseId)->name
			, safe
			, scoutPatrolBuildTime
			, baseInfo.safeTime
		);
		
		// list protectors
		
		std::vector<int> protectors;
		
		for (int vehicleId : baseInfo.garrison)
		{
			// exclude unavailable
			
			if (hasTask(vehicleId))
				continue;
			
			// infantry defender
			
			if (!isInfantryDefensiveVehicle(vehicleId))
				continue;
			
			// add protector
			
			protectors.push_back(vehicleId);
			
		}
		
		if (!safe && protectors.size() == 0)
		{
			protectors = baseInfo.garrison;
		}
		
		// evaluate garrison
		
		std::vector<IdDoubleValue> garrisonRelativeUnitEffects;
		
		for (int vehicleId : protectors)
		{
			VEH *vehicle = getVehicle(vehicleId);
			
			// exclude unavailable
			
			if (hasTask(vehicleId))
				continue;
			
			// relativeUnitEffect is ratio of this base effect over global average effect
			
			double relativeUnitEffect = baseCombatData.getUnitEffect(vehicle->unit_id) / aiData.getGlobalAverageUnitEffect(vehicle->unit_id);
			
			// slightly favor wounded units
			
			if (vehicle->damage_taken > 0)
			{
				relativeUnitEffect *= (1.0 + getVehicleRelativeDamage(vehicleId));
			}
			
			// store relativeUnitEffect
			
			garrisonRelativeUnitEffects.push_back({vehicleId, relativeUnitEffect});
			
		}
		
		// sort by relativeUnitEffect descending
		
		std::sort(garrisonRelativeUnitEffects.begin(), garrisonRelativeUnitEffects.end(), compareIdDoubleValueDescending);
		
		// keep vehicles at base to required protection and police level
		
		for (IdDoubleValue &garrisonRelativeUnitEffect : garrisonRelativeUnitEffects)
		{
			int vehicleId = garrisonRelativeUnitEffect.id;
			
			// verify we have enough police and protection
			
			if (basePoliceData.isSatisfied() && baseCombatData.isSatisfied(true))
				break;
			
			// set task
			
			setTask(Task(vehicleId, TT_HOLD));
			
			// update provided police and protection
			
			basePoliceData.addVehicle(vehicleId);
			baseCombatData.addProvidedEffect(vehicleId, true);
			
			debug
			(
				"\t\t[%4d] %-32s"
				" police satisfied = %s"
				" combat effect = %5.2f"
				" combat requiredEffect = %5.2f"
				" combat providedEffect=%5.2f"
				" combat satisfied=%s\n"
				, vehicleId, getVehicle(vehicleId)->name()
				, (basePoliceData.isSatisfied() ? "Y" : "N")
				, baseCombatData.getVehicleEffect(vehicleId)
				, baseCombatData.requiredEffect
				, baseCombatData.providedEffect
				, (baseCombatData.isSatisfied(true) ? "Y" : "N")
			);
			
		}
		
	}
	
}

void moveBaseProtectors()
{
	debug("moveBaseProtectors - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tasks
	
	std::vector<TaskPriority> taskPriorities;
	
	populateBaseProtectionTasks(taskPriorities);
	
	// sort available tasks
	
	std::sort(taskPriorities.begin(), taskPriorities.end(), compareTaskPriorityDescending);
	
	if (DEBUG)
	{
		debug("\tsortedTasks\n");
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			double priority = taskPriority.priority;
			int vehicleId = taskPriority.vehicleId;
			MAP *location = getVehicleMapTile(vehicleId);
			int locationX = getX(location);
			int locationY = getY(location);
			MAP *destination = taskPriority.destination;
			int destinationX = getX(destination);
			int destinationY = getY(destination);
			
			debug("\t\t%5.2f: (%3d,%3d) -> (%3d,%3d)\n", priority, locationX, locationY, destinationX, destinationY);
			
		}

	}
	
	// execute tasks
	
	debug("\tselectedTasks\n");
	
	for (TaskPriority &taskPriority : taskPriorities)
	{
		int vehicleId = taskPriority.vehicleId;
		VEH *vehicle = getVehicle(vehicleId);
		MAP *destination = taskPriority.destination;
		int baseId = taskPriority.baseId;
		
		// no base at destination ?
		
		if (baseId == -1)
			continue;
		
		BASE *base = getBase(baseId);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		PoliceData &basePoliceData = baseInfo.policeData;
		BaseCombatData &baseCombatData = baseInfo.combatData;
		
		// available
		
		if (hasTask(vehicleId))
			continue;
		
		// skip base not needing police and defence
		
		if (basePoliceData.isSatisfied() && baseCombatData.isSatisfied(false))
			return;
		
		// set task
		
		if (!transitVehicle(Task(vehicleId, TT_HOLD, destination)))
			continue;
		
		debug
		(
			"\t\t[%4d] (%3d,%3d) -> (%3d,%3d)\n"
			, vehicleId, vehicle->x, vehicle->y
			, getX(destination), getY(destination)
		);
		
		// update police and protection provided
		
		basePoliceData.addVehicle(vehicleId);
		baseCombatData.addProvidedEffect(vehicleId, false);
		
		debug
		(
			"\t\t\t%-25s"
			" required=%5.2f"
			" providedEffect=%5.2f"
			" satisfied: %s\n"
			, base->name
			, baseCombatData.requiredEffect
			, baseCombatData.providedEffect
			, (baseCombatData.isSatisfied(false) ? "Yes" : "No")
		);
		
	}
	
}

void moveCombat()
{
	const bool TRACE = DEBUG && true;
	
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
			double priority = taskPriority.priority;
			int vehicleId = taskPriority.vehicleId;
			MAP *location = getVehicleMapTile(vehicleId);
			int locationX = getX(location);
			int locationY = getY(location);
			MAP *destination = taskPriority.destination;
			int destinationX = getX(destination);
			int destinationY = getY(destination);
			MAP *attackTarget = taskPriority.attackTarget;
			int attackTargetX = getX(attackTarget);
			int attackTargetY = getY(attackTarget);
			
			debug
			(
				"\t\t%5.2f:"
				" (%3d,%3d)"
				" -> (%3d,%3d)"
				" => (%3d,%3d)"
				"\n"
				, priority
				, locationX, locationY
				, destinationX, destinationY
				, attackTargetX, attackTargetY
			);
			
		}
		
	}
	
	// select tasks
	
	debug("\tselecting tasks\n");
	
	std::set<MAP *> excludedEnemyStacks;
	std::map<int, TaskPriority *> vehicleAssignments;
	
	while (true)
	{
		// clear assignments
		
		vehicleAssignments.clear();
		
		// iterate task priorities
		
		std::set<MAP *> targetedPods;
		std::set<MAP *> targetedEmptyBases;
		std::map<MAP *, ProvidedEffect> providedEffects;
		
		for (TaskPriority &taskPriority : taskPriorities)
		{
			int vehicleId = taskPriority.vehicleId;
			MAP *destination = taskPriority.destination;
			MAP *attackTarget = taskPriority.attackTarget;
			COMBAT_TYPE combatType = taskPriority.combatType;
			bool destructive = taskPriority.destructive;
			double effect = taskPriority.effect;
			
			// skip already assigned
			
			if (vehicleAssignments.count(vehicleId) != 0)
				continue;
			
			if (TRACE)
			{
				debug
				(
					"\t\t[%4d]"
					" (%3d,%3d)"
					"->(%3d,%3d)"
					"=>(%3d,%3d)"
					" combatType=%d"
					" destructive=%d"
					" effect=%5.2f"
					"\n"
					, vehicleId
					, getVehicle(vehicleId)->x, getVehicle(vehicleId)->y
					, getX(destination), getY(destination)
					, getX(attackTarget), getY(attackTarget)
					, combatType
					, destructive
					, effect
				);
			}
			
			if (isPodAt(destination)) // pod
			{
				if (TRACE)
				{
					debug("\t\t\tpod\n");
				}
				
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
				if (TRACE) { debug("\t\t\tempty base\n"); }
				
				// skip already targeted empty base
				
				if (targetedEmptyBases.count(destination) != 0)
				{
					if (TRACE) { debug("\t\t\t\ttargeted\n"); }
					continue;
				}
				
				// set vehicle task
				
				vehicleAssignments.insert({vehicleId, &taskPriority});
				
				// add targeted empty base
				
				targetedEmptyBases.insert(destination);
				
			}
			else if (attackTarget != nullptr && aiData.enemyStacks.count(attackTarget) != 0) // enemy stack
			{
				EnemyStackInfo &enemyStackInfo = aiData.enemyStacks.at(attackTarget);
				ProvidedEffect &providedEffect = providedEffects[attackTarget];
				
				debug("\t\t\tenemy stack bombardmentDestructive=%d\n", enemyStackInfo.bombardmentDestructive);
				
				// skip excluded stack
				
				if (excludedEnemyStacks.count(attackTarget) != 0)
				{
					debug("\t\t\t\texcluded\n");
					continue;
				}
				
				// skip sufficiently targeted stack
				// stack already can be surely destroyed by others in one turn
				
				if (providedEffect.destructive >= enemyStackInfo.desiredEffect)
				{
					debug("\t\t\t\tdestructive=%5.2f >= desiredEffect=%5.2f\n", providedEffect.destructive, enemyStackInfo.desiredEffect);
					continue;
				}
				
				// set vehicle task
				
				vehicleAssignments.insert({vehicleId, &taskPriority});
				
				// accumulate provided effect
				
				if (taskPriority.destructive) // destructive
				{
					// update destructionTime
					
					providedEffect.firstDestructiveAttackTime = std::min(providedEffect.firstDestructiveAttackTime, taskPriority.travelTime);
					debug("\t\t\t\tfirstDestructiveAttackTime=%2d\n", providedEffect.firstDestructiveAttackTime);
					
					// add effect
					
					providedEffect.addDestructive(effect);
					
					debug
					(
						"\t\t\t\tdestructive"
						" effect=%5.2f"
						" provided: (d=%5.2f b=%5.2f)"
						" required=%5.2f"
						" requiredBombarded=%5.2f"
						" desired=%5.2f"
						"\n"
						, effect
						, providedEffect.destructive
						, providedEffect.bombardment
						, enemyStackInfo.requiredEffect
						, enemyStackInfo.requiredEffectBombarded
						, enemyStackInfo.desiredEffect
					);
					
				}
				else // non destructive
				{
					// add non destructive vehicle
					
					providedEffect.nonDestructiveTaskPriorities.push_back(&taskPriority);
					
					// add effect
					
					providedEffect.addBombardment(effect);
					
					debug
					(
						"\t\t\t\tbombardment"
						" effect=%5.2f"
						" provided: (d=%5.2f b=%5.2f)"
						" required=%5.2f"
						" requiredBombarded=%5.2f"
						" desired=%5.2f"
						"\n"
						, effect
						, providedEffect.destructive
						, providedEffect.bombardment
						, enemyStackInfo.requiredEffect
						, enemyStackInfo.requiredEffectBombarded
						, enemyStackInfo.desiredEffect
					);
					
					
				}
				
			}
			else
			{
				// set vehicle task
				
				vehicleAssignments.insert({vehicleId, &taskPriority});
				
			}
			
		}
		
		// redirect non destructive vehicles if too long to wait
		
		for (std::pair<MAP * const, ProvidedEffect> &providedEffectEntry : providedEffects)
		{
			ProvidedEffect &providedEffect = providedEffectEntry.second;
			
			for (TaskPriority *effectTaskPriority : providedEffect.nonDestructiveTaskPriorities)
			{
				int vehicleId = effectTaskPriority->vehicleId;
				int waitTime = providedEffect.firstDestructiveAttackTime - effectTaskPriority->travelTime;
				
				if (waitTime < 10)
					continue;
				
				for (TaskPriority &taskPriority : taskPriorities)
				{
					MAP *attackTarget = taskPriority.attackTarget;
					
					if (taskPriority.vehicleId != vehicleId)
						continue;
					
					if (attackTarget != nullptr && aiData.enemyStacks.count(attackTarget) != 0) // enemy stack
						continue;
					
					debug
					(
						"- redirected"
						" [%4d] (%3d,%3d)"
						" waitTime=%2d"
						"\n"
						, vehicleId, getVehicle(vehicleId)->x, getVehicle(vehicleId)->y
						, waitTime
					);
					
					vehicleAssignments.erase(vehicleId);
					vehicleAssignments.insert({vehicleId, &taskPriority});
					break;
					
				}
				
			}
			
		}
		
		// find worst undertargeted enemy stacks
		
		if (TRACE)
		{
			debug("\tundertargeted stacks\n");
		}
		
		MAP *mostUndertargetedEnemyStack = nullptr;
		double mostUndertargetedEnemyStackEffectRatio = 1.0;
		
		for (std::pair<MAP * const, ProvidedEffect> &providedEffectEntry : providedEffects)
		{
			MAP *stackTile = providedEffectEntry.first;
			ProvidedEffect &providedEffect = providedEffectEntry.second;
			EnemyStackInfo &enemyStackInfo = aiData.enemyStacks.at(stackTile);
			
			// exclude excluded
			
			if (excludedEnemyStacks.count(stackTile) != 0)
				continue;
			
			// always attack alien spore launcher
			
			if (enemyStackInfo.alienSporeLauncher)
				continue;
			
			// effect ratio
			
			double effectRatio = providedEffect.destructive / enemyStackInfo.getRequiredEffect(providedEffect.bombardment > 0.0);
			
			// skip sufficiently targeted stack
			// target can be destroyed eventually
			
			if (effectRatio >= 1.0)
				continue;
			
			if (TRACE)
			{
				debug
				(
					"\t\t(%3d,%3d)"
					" effectRatio=%5.2f"
					" provided: (d=%5.2f b=%5.2f)"
					" required=%5.2f"
					" requiredBombarded=%5.2f"
					" desired=%5.2f"
					"\n"
					, getX(stackTile), getY(stackTile)
					, effectRatio
					, providedEffect.destructive
					, providedEffect.bombardment
					, enemyStackInfo.requiredEffect
					, enemyStackInfo.requiredEffectBombarded
					, enemyStackInfo.desiredEffect
				);
			}
			
			// update worst
			
			if (effectRatio < mostUndertargetedEnemyStackEffectRatio)
			{
				mostUndertargetedEnemyStack = stackTile;
				mostUndertargetedEnemyStackEffectRatio = effectRatio;
			}
			
		}
		
		// no undertargeted stacks - exit
		
		if (mostUndertargetedEnemyStack == nullptr)
			break;
		
		// exclude stack
		
		excludedEnemyStacks.insert(mostUndertargetedEnemyStack);
		
		debug("\tmost untargeted enemy stack: (%3d,%3d)\n", getX(mostUndertargetedEnemyStack), getY(mostUndertargetedEnemyStack));
		
	}
	
	// execute tasks
	
	debug("\tselectedTasks\n");
	
	for (std::pair<int const, TaskPriority *> &vehicleAssignmentEntry : vehicleAssignments)
	{
		int vehicleId = vehicleAssignmentEntry.first;
		TaskPriority &taskPriority = *(vehicleAssignmentEntry.second);
		
		transitVehicle(Task(vehicleId, taskPriority.taskType, taskPriority.destination, taskPriority.attackTarget));
		
		if (DEBUG)
		{
			MAP *location = getVehicleMapTile(vehicleId);
			int locationX = getX(location);
			int locationY = getY(location);
			MAP *destination = taskPriority.destination;
			int destinationX = getX(destination);
			int destinationY = getY(destination);
			MAP *attackTarget = taskPriority.attackTarget;
			int attackTargetX = getX(attackTarget);
			int attackTargetY = getY(attackTarget);
			
			debug
			(
				"\t\t[%4d] (%3d,%3d)"
				" -> (%3d,%3d)"
				" => (%3d,%3d)"
				"\n"
				, vehicleId, locationX, locationY
				, destinationX, destinationY
				, attackTargetX, attackTargetY
			);
			
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
		
		debug("\t\t(%3d,%3d)\n", vehicle->x, vehicle->y);
		
		// find closest monolith
		
		MapValue closestMonolithLocaionTravelTime = findClosestItemLocation(vehicleId, TERRA_MONOLITH, MAX_REPAIR_DISTANCE, true);
		
		// not found
		
		if (closestMonolithLocaionTravelTime.tile == nullptr)
		{
			debug("\t\t\tmonolith location is not found\n");
			continue;
		}
		
		// calculate promotion priority
		
		double promotionPriority = conf.ai_combat_priority_monolith_promotion;
		
		// calculate travel time coefficient
		
		int travelTime = closestMonolithLocaionTravelTime.value;
		double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
		
		// adjust priority by travelTimeCoefficient
		
		double priority = promotionPriority * travelTimeCoefficient;
		
		debug
		(
			"\t\t\t-> (%3d,%3d)"
			" priority=%5.2f"
			", promotionPriority=%5.2f"
			", conf.ai_combat_priority_monolith_promotion=%5.2f"
			", travelTimeCoefficient=%5.2f\n"
			, getX(closestMonolithLocaionTravelTime.tile), getY(closestMonolithLocaionTravelTime.tile)
			, priority
			, promotionPriority
			, conf.ai_combat_priority_monolith_promotion
			, travelTimeCoefficient
		);
		
		// add task
		
		taskPriorities.emplace_back(priority, vehicleId, TT_MOVE, closestMonolithLocaionTravelTime.tile, travelTime);
		
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
		
		debug("\t\t(%3d,%3d)\n", vehicle->x, vehicle->y);
		
		// scan nearby location for best repair rate
		
		MAP *bestRepairLocaion = nullptr;
		double bestRepairPriority = 0.0;
		
		for (MAP *tile : getRangeTiles(vehicleTile, MAX_REPAIR_DISTANCE, true))
		{
			int x = getX(tile);
			int y = getY(tile);
			bool ocean = is_ocean(tile);
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[aiFactionId];
			
			// vehicle can repair there
			
			switch (triad)
			{
			case TRIAD_AIR:
				// airbase
				if (!isAirbaseAt(tile))
					continue;
				break;
				
			case TRIAD_SEA:
				// same ocean association
				if (!isSameOceanAssociation(vehicleTile, tile, aiFactionId))
					continue;
				break;
				
			case TRIAD_LAND:
				// land
				if (ocean)
					continue;
				break;
				
			}
			
			// exclude too far field location
			
			if (!(map_has_item(tile, TERRA_BASE_IN_TILE | TERRA_BUNKER | TERRA_MONOLITH)) && map_range(vehicle->x, vehicle->y, x, y) > 2)
				continue;
			
			// exclude blocked
			
			if (tileMovementInfo.blocked)
				continue;
			
			// exclude unreachable locations
			
			if (!isVehicleDestinationReachable(vehicleId, tile, true))
				continue;
			
			// get repair parameters
			
			RepairInfo repairInfo = getVehicleRepairInfo(vehicleId, tile);
			
			// no repair happening
			
			if (repairInfo.damage <= 0)
				continue;
			
			// repair priority coefficient
			
			double repairPriorityCoefficient = (repairInfo.full ? conf.ai_combat_priority_repair : conf.ai_combat_priority_repair_partial);
			
			// calculate travel time and total time
			
			int travelTime = getVehicleATravelTime(vehicleId, tile);
			
			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
				continue;
			
			int totalTime = std::max(1, travelTime + repairInfo.time);
			
			// calculate repair rate
			
			double repairRate = (double)repairInfo.damage / (double)totalTime;
			
			// warzone coefficient
			
			double warzoneCoefficient = (tileInfo.warzone ? 0.7 : 1.0);
			
			// get repairPriority
			
			double repairPriority = repairPriorityCoefficient * repairRate * warzoneCoefficient;
			
			debug
			(
				"\t\t\t-> (%3d,%3d)"
				" repairPriority=%5.2f"
				" repairPriorityCoefficient=%5.2f"
				" travelTime=%2d"
				" totalTime=%2d"
				" repairRate=%5.2f"
				" warzoneCoefficient=%5.2f"
				"\n"
				, x, y
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
		
		if (map_has_item(bestRepairLocaion, TERRA_MONOLITH) && ((vehicle->state & VSTATE_MONOLITH_UPGRADED) == 0))
		{
			promotionPriority = conf.ai_combat_priority_monolith_promotion;
		}
		
		// summarize priority
		
		double priority = repairPriority + promotionPriority;
		
		debug
		(
			"\t\t-> (%3d,%3d)"
			" priority=%5.2f"
			", conf.ai_combat_priority_repair=%5.2f"
			", repairPriority=%5.2f"
			", promotionPriority=%5.2f\n"
			, getX(bestRepairLocaion), getY(bestRepairLocaion)
			, priority
			, conf.ai_combat_priority_repair
			, repairPriority
			, promotionPriority
		);
		
		// add task
		
		taskPriorities.push_back({priority, vehicleId, TT_SKIP, bestRepairLocaion});
		
	}
	
}

void populateBasePolice2xTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateBasePolice2xTasks\n");
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		PoliceData &basePoliceData = baseInfo.policeData;
		BaseCombatData &baseCombatData = baseInfo.combatData;
		
		// exclude base without police need
		
		if (basePoliceData.isSatisfied())
			continue;
		
		debug("\t%-25s\n", base->name);
		
		for (int vehicleId : aiData.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			// exclude unavailable
			
			if (hasTask(vehicleId))
				continue;
			
			// police2x
			
			if (!isInfantryDefensivePolice2xVehicle(vehicleId))
				continue;
			
			// exclude not reachable destination
			
			if (!isVehicleDestinationReachable(vehicleId, baseTile, false))
				continue;
			
			debug("\t\t(%3d,%3d) [%4d] %-32s\n", vehicle->x, vehicle->y, vehicleId, getVehicleUnitName(vehicleId));
			
			// protection priority
			
			double protectionPriority = 0.0;
			
			if (!baseCombatData.isSatisfied(false))
			{
				double effect = baseCombatData.getUnitEffect(vehicle->unit_id);
				
				// calculate protection priority
				
				protectionPriority =
					conf.ai_combat_priority_base_protection
					*
					effect
				;
				
			}
			
			// summarize priorities
			
			double combinedPriority = protectionPriority;
			
			// skip base with no priority
			
			if (combinedPriority <= 0.0)
				continue;
			
			// get travel time coefficient
			
			int travelTime = getVehicleATravelTime(vehicleId, baseTile);
			double travelTimeCoefficient = exp(- travelTime / conf.ai_combat_travel_time_scale_base_protection);
			
			// calculate priority
			
			double priority =
				combinedPriority
				*
				travelTimeCoefficient
			;
			
			debug
			(
				"\t\t\t-> %-25s"
				" priority=%5.2f"
				", combinedPriority=%5.2f"
				", protectionPriority=%5.2f"
				", travelTime=%2d, travelTimeCoefficient=%5.2f"
				"\n"
				, base->name
				, priority
				, combinedPriority
				, protectionPriority
				, travelTime, travelTimeCoefficient
			);
			
			// add task
			
			taskPriorities.push_back({priority, vehicleId, TT_HOLD, baseTile, travelTime, baseId});
			
		}
		
	}
	
}

void populateBaseProtectionTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateBaseProtectionTasks\n");
	
	if (aiData.baseIds.size() == 0)
		return;
	
	// summarize base protection requests for normalization
	
	double totalRequiredEffect = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		BaseCombatData &baseCombatData = baseInfo.combatData;
		
		totalRequiredEffect += baseCombatData.requiredEffect;
		
	}
	
	double averageRequiredEffect = totalRequiredEffect / aiData.baseIds.size();
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
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
			BaseCombatData &baseCombatData = baseInfo.combatData;
			
			// exclude not reachable destination
			
			if (!isVehicleDestinationReachable(vehicleId, baseTile, false))
				continue;
			
			// compute priorities
			
			// protection priority
			
			double protectionPriority = 0.0;
			
			if (!baseCombatData.isSatisfied(false))
			{
				double relativeRequiredEffect = baseCombatData.requiredEffect / averageRequiredEffect;
				double effect = baseCombatData.getUnitEffect(vehicle->unit_id);
				
				protectionPriority =
					conf.ai_combat_priority_base_protection
					*
					relativeRequiredEffect
					*
					effect
				;
				
			}
			
			// summarize priorities
			
			double combinedPriority = protectionPriority;
			
			// skip base with no priority
			
			if (combinedPriority <= 0.0)
				continue;
			
			// get travel time and coresponding coefficient
			
			int range = map_range(vehicle->x, vehicle->y, base->x, base->y);
			int speed = getVehicleSpeedWithoutRoads(vehicleId);
			int travelTime = divideIntegerRoundUp(range, speed);
			
			double travelTimeCoefficient = exp(- (double)travelTime / conf.ai_combat_travel_time_scale_base_protection);
			
			// adjust priority by travelTimeCoefficient
			
			double priority = combinedPriority * travelTimeCoefficient;
			
			debug
			(
				"\t\t[%4d] (%3d,%3d) %-32s -> %-25s"
				" priority=%5.2f"
				", combinedPriority=%5.2f"
				", protectionPriority=%5.2f"
				", range=%2d, speed=%2d, travelTime=%3d, travelTimeCoefficient=%5.2f"
				"\n"
				, vehicleId, vehicle->x, vehicle->y, getVehicleUnitName(vehicleId), base->name
				, priority
				, combinedPriority
				, protectionPriority
				, range, speed, travelTime, travelTimeCoefficient
			);
			
			// add task
			
			taskPriorities.push_back({priority, vehicleId, TT_HOLD, baseTile, travelTime, baseId});
			
		}
		
	}
	
}

void populateEnemyStackAttackTasks(std::vector<TaskPriority> &taskPriorities)
{
	const bool TRACE = DEBUG && true;
	
	debug("\tpopulateEnemyStackAttackTasks\n");
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		int vehicleMapSpeed = getVehicleSpeedWithoutRoads(vehicleId);
		double vehicleRelativePower = getVehicleRelativePower(vehicleId);
		
		// exclude unavailable
		
		if (hasTask(vehicleId))
			continue;
		
		if (TRACE)
		{
			debug("\t\t[%4d] (%3d,%3d)\n", vehicleId, getVehicle(vehicleId)->x, getVehicle(vehicleId)->y);
		}
		
		for (std::pair<MAP * const, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
		{
			MAP *enemyStackTile = enemyStackEntry.first;
			EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
			bool enemyStackTileOcean = is_ocean(enemyStackTile);
			
			if (TRACE)
			{
				debug("\t\t\t-> (%3d,%3d)\n", getX(enemyStackTile), getY(enemyStackTile));
			}
			
			// attack priority coefficient
			
			double attackPriority;
			
			if (enemyStackInfo.base)
			{
				attackPriority = conf.ai_combat_attack_priority_base;
			}
			else
			{
				attackPriority = enemyStackInfo.priority;
			}
			
			// baseRange coefficient
			
			double baseRangeCoefficient;
			
			if (enemyStackInfo.baseOrBunker || enemyStackInfo.baseRange >= conf.ai_territory_threat_range_scale)
			{
				baseRangeCoefficient = 1.0;
			}
			else
			{
				baseRangeCoefficient =
					getExponentialCoefficient
					(
						conf.ai_territory_threat_range_scale,
						(double)enemyStackInfo.baseRange - conf.ai_territory_threat_range_scale
					)
				;
			}
			
			// attack position and travel time
			
			MAP *meleeAttackPosition = nullptr;
			int meleeAttackTravelTime = -1;
			
			if (isMeleeVehicle(vehicleId))
			{
				meleeAttackPosition = getVehicleMeleeAttackPosition(vehicleId, enemyStackTile, false);
				
				if (meleeAttackPosition != nullptr)
				{
					meleeAttackTravelTime = getVehicleAttackATravelTime(vehicleId, meleeAttackPosition, false);
				}
				
			}
			
			MAP *artilleryAttackPosition = nullptr;
			int artilleryAttackTravelTime = -1;
			
			if (isArtilleryVehicle(vehicleId))
			{
				artilleryAttackPosition = getVehicleArtilleryAttackPosition(vehicleId, enemyStackTile, false);
				
				if (artilleryAttackPosition != nullptr)
				{
					artilleryAttackTravelTime = getVehicleAttackATravelTime(vehicleId, artilleryAttackPosition, false);
				}
				
			}
			
			// worth chasing
			
			if
			(
				enemyStackInfo.baseOrBunker
				||
				enemyStackTile->owner == aiFactionId
				||
				meleeAttackTravelTime != -1 && meleeAttackTravelTime <= 1
				||
				artilleryAttackTravelTime != -1 && artilleryAttackTravelTime <= 1
				||
				vehicleMapSpeed > enemyStackInfo.lowestSpeed
			)
			{
				// worth chasing
			}
			else
			{
				if (TRACE) { debug("\t\t\t\tnot worth chasing\n"); }
				continue;
			}
			
			// effects
			
			CombatEffect meleeCombatEffect = enemyStackInfo.getVehicleEffect(vehicleId, AT_MELEE);
			CombatEffect artilleryCombatEffect = enemyStackInfo.getVehicleEffect(vehicleId, AT_ARTILLERY);
			
			// process attack modes
			
			if (artilleryCombatEffect.effect > 0.0 && artilleryCombatEffect.combatType == CT_MELEE && artilleryAttackPosition != nullptr)
			{
				if (TRACE) { debug("\t\t\t\tbombardment - melee\n"); }
				
				CombatEffect &combatEffect = artilleryCombatEffect;
				
				// do not use severely damaged units with low effect
				
				if (vehicleRelativePower < 0.8 && combatEffect.effect < 1.0)
				{
					if (TRACE) { debug("\t\t\t\tseverely damaged\n"); }
					continue;
				}
				
				// travel time coefficient
				
				MAP *destination = artilleryAttackPosition;
				int travelTime = getVehicleAttackATravelTime(vehicleId, destination, false);
				
				if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
					continue;
				
				double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				
				// effect coefficient
				
				double effectCoefficient = std::min(2.0, combatEffect.effect);
				
				// priority
				
				double priority = effectCoefficient * attackPriority * baseRangeCoefficient * travelTimeCoefficient;
				
				if (TRACE)
				{
					debug
					(
						"\t\t\t\t"
						" priority=%5.2f"
						" effect=%5.2f"
						" effectCoefficient=%5.2f"
						" attackPriority=%5.2f"
						" baseRangeCoefficient=%5.2f"
						" travelTime=%2d"
						" travelTimeCoefficient=%5.2f"
						"\n"
						, priority
						, combatEffect.effect
						, effectCoefficient
						, attackPriority
						, baseRangeCoefficient
						, travelTime
						, travelTimeCoefficient
					);
				}
				
				taskPriorities.push_back
				(
						{
							priority,
							vehicleId,
							TT_LONG_RANGE_FIRE,
							destination,
							travelTime,
							CT_MELEE,
							enemyStackTile,
							true,
							combatEffect.effect,
						}
				);
				
			}
			else if (artilleryCombatEffect.effect > 0.0 && artilleryCombatEffect.combatType == CT_BOMBARDMENT && artilleryAttackPosition != nullptr)
			{
				if (TRACE) { debug("\t\t\t\tbombardment - bombardment\n"); }
				
				CombatEffect &combatEffect = artilleryCombatEffect;
				
				// travel time coefficient
				
				MAP *destination = artilleryAttackPosition;
				int travelTime = getVehicleAttackATravelTime(vehicleId, destination, false);
				
				if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
					continue;
				
				double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				
				// destructive bombardment
				
				bool destructive = enemyStackInfo.bombardmentDestructive;
				
				// effect coefficient
				// bombardment effect is multiplied by 10 to account for relatively small bombardment damage per turn
				
				double effectCoefficient = std::min(2.0, 10.0 * combatEffect.effect);
				
				// corrections for sea unit bombarding ocean base
				
				if (enemyStackTileOcean && enemyStackInfo.base && triad == TRIAD_SEA)
				{
					destructive = true;
					effectCoefficient *= 2.0;
				}
				
				// non destructive bombardment does not inherit special priority except base
				
				if (!enemyStackInfo.base && !destructive)
				{
					attackPriority = 1.0;
				}
				
				// priority
				
				double priority = effectCoefficient * attackPriority * baseRangeCoefficient * travelTimeCoefficient;
				
				if (TRACE)
				{
					debug
					(
						"\t\t\t\t"
						" priority=%5.2f"
						" destructive=%d"
						" effect=%5.2f"
						" effectCoefficient=%5.2f"
						" attackPriority=%5.2f"
						" baseRangeCoefficient=%5.2f"
						" travelTime=%2d"
						" travelTimeCoefficient=%5.2f"
						"\n"
						, priority
						, destructive
						, combatEffect.effect
						, effectCoefficient
						, attackPriority
						, baseRangeCoefficient
						, travelTime
						, travelTimeCoefficient
					);
				}
				
				taskPriorities.push_back
				(
					{
						priority,
						vehicleId,
						TT_LONG_RANGE_FIRE,
						destination,
						travelTime,
						CT_BOMBARDMENT,
						enemyStackTile,
						destructive,
						combatEffect.effect,
					}
				);
				
			}
			else // not bombardment
			{
				// find best attack type
				
				double effect = 0.0;
				TaskType taskType = TT_MELEE_ATTACK;
				MAP *destination = nullptr;
				COMBAT_TYPE combatType = CT_MELEE;
				
				if (artilleryCombatEffect.effect > effect && artilleryAttackPosition != nullptr && artilleryCombatEffect.combatType == CT_ARTILLERY_DUEL)
				{
					effect = artilleryCombatEffect.effect > effect;
					taskType = TT_LONG_RANGE_FIRE;
					destination = artilleryAttackPosition;
					combatType = CT_ARTILLERY_DUEL;
				}
				
				if (meleeCombatEffect.effect > effect && meleeAttackPosition != nullptr)
				{
					effect = meleeCombatEffect.effect;
					taskType = TT_MELEE_ATTACK;
					destination = meleeAttackPosition;
					combatType = CT_MELEE;
				}
				
				// no effect
				
				if (effect <= 0.0)
				{
					if (TRACE) { debug("\t\t\t\teffect <= 0.0\n"); }
					continue;
				}
				
				// do not use severely damaged units with low effect
				
				if (vehicleRelativePower < 0.8 && effect < 1.40)
				{
					if (TRACE) { debug("\t\t\t\tseverely damaged\n"); }
					continue;
				}
				
				// travel time coefficient
				
				int travelTime = getVehicleAttackATravelTime(vehicleId, destination, false);
				
				if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
					continue;
				
				double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				
				// effect coefficient
				
				double effectCoefficient = std::min(2.0, effect);
				
				// priority
				
				double priority = effectCoefficient * attackPriority * baseRangeCoefficient * travelTimeCoefficient;
				
				if (TRACE)
				{
					debug
					(
						"\t\t\t\t"
						" priority=%5.2f"
						" effect=%5.2f"
						" effectCoefficient=%5.2f"
						" attackPriority=%5.2f"
						" baseRangeCoefficient=%5.2f"
						" travelTime=%2d"
						" travelTimeCoefficient=%5.2f"
						"\n"
						, priority
						, effect
						, effectCoefficient
						, attackPriority
						, baseRangeCoefficient
						, travelTime
						, travelTimeCoefficient
					);
				}
				
				taskPriorities.push_back
				(
					{
						priority,
						vehicleId,
						taskType,
						destination,
						travelTime,
						combatType,
						enemyStackTile,
						true,
						effect,
					}
				);
				
			}
			
		}
		
	}
			
}

void populateEmptyBaseCaptureTasks(std::vector<TaskPriority> &taskPriorities)
{
	bool TRACE = DEBUG && true;
	
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
		
		debug("\t\t[%4d] (%3d,%3d)\n", vehicleId, getVehicle(vehicleId)->x, getVehicle(vehicleId)->y);
		
		for (MAP *enemyBaseTile : aiData.emptyEnemyBaseTiles)
		{
			debug("\t\t\t-> (%3d,%3d)\n", getX(enemyBaseTile), getY(enemyBaseTile));
			
			// can capture this base
			
			if (!isVehicleCanCaptureBase(vehicleId, enemyBaseTile))
				continue;
			
			// travel time
			
			int travelTime = getVehicleATravelTime(vehicleId, enemyBaseTile);
			
			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
				continue;
			
			// do not capture base farther than 10 turn travel time
			
			if (travelTime > 10)
				continue;
			
			// do not capture base if movement is severely impacted
			
			int vehicleSpeed = getVehicleSpeed(vehicleId);
			int vehicleDamage = vehicle->damage_taken;
			vehicle->damage_taken = 0;
			int undamagedVehicleSpeed = getVehicleSpeed(vehicleId);
			vehicle->damage_taken = vehicleDamage;
			
			double undamagedTravelTime = (double)travelTime * (double)vehicleSpeed / (double)undamagedVehicleSpeed;
			
			debug
			(
				"\t\t\tvehicleSpeed=%2d"
				" vehicleDamage=%2d"
				" undamagedVehicleSpeed=%2d"
				" undamagedTravelTime=%5.2f"
				" travelTime=%3d"
				, vehicleSpeed
				, vehicleDamage
				, undamagedVehicleSpeed
				, undamagedTravelTime
				, travelTime
			);
			
			if ((double)travelTime - undamagedTravelTime > 5.0)
				continue;
			
			// travelTime coefficient
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
			
			// priority
			
			double priority = attackPriority * travelTimeCoefficient;
			
			// add task
			
			taskPriorities.push_back({priority, vehicleId, TT_MOVE, enemyBaseTile, travelTime});
			
			if (TRACE)
			{
				debug
				(
					"\t\t\t->(%3d,%3d)"
					" priority=%5.2f"
					" attackPriority=%5.2f"
					" travelTime=%2d travelTimeCoefficient=%5.2f\n"
					, getX(enemyBaseTile), getY(enemyBaseTile)
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
	
	std::vector<MAP *> podLocations;
	
	// collect land pods
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		int association = getAssociation(tile, aiFactionId);
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[aiFactionId];
		
		// land
		
		if (is_ocean(tile))
			continue;
		
		// pod
		
		if (isPodAt(tile) == 0)
			continue;
		
		// reachable
		
		if (!isPotentiallyReachableAssociation(association, aiFactionId))
			continue;
		
		// not hostile territory
		
		if (isWar(aiFactionId, tile->owner))
			continue;
		
		// not blocked location
		
		if (tileMovementInfo.blocked)
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
			
			int travelTime = getVehicleATravelTime(vehicleId, podLocation);
			
			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
			
			// adjust priority by travelTimeCoefficient
			
			double priority = basePriority * travelTimeCoefficient;
			
			debug
			(
				"\t\t(%3d,%3d) -> (%3d,%3d)"
				" priority=%5.2f"
				" basePriority=%5.2f"
				" travelTime=%2d travelTimeCoefficient=%5.2f\n"
				, vehicle->x, vehicle->y, getX(podLocation), getY(podLocation)
				, priority
				, basePriority
				, travelTime, travelTimeCoefficient
			);
			
			// add task
			
			taskPriorities.push_back({priority, vehicleId, TT_MOVE, podLocation, travelTime});
			
		}
		
	}
	
}

void populateSeaPodPoppingTasks(std::vector<TaskPriority> &taskPriorities)
{
	debug("\tpopulateSeaPodPoppingTasks\n");
	
	// get pod locations
	
	std::vector<MAP *> podLocations;
	
	for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
	{
		int tileAssociation = getOceanAssociation(tile, aiFactionId);
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[aiFactionId];
		
		// ocean
		
		if (!is_ocean(tile))
			continue;
		
		// pod
		
		if (isPodAt(tile) == 0)
			continue;
		
		// reachable
		
		if (!isPotentiallyReachableAssociation(tileAssociation, aiFactionId))
			continue;
		
		// not hostile land territory
		
		if (!is_ocean(tile) && isWar(aiFactionId, tile->owner))
			continue;
		
		// not blocked location
		
		if (tileMovementInfo.blocked)
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
			
			int travelTime = getVehicleATravelTime(vehicleId, podLocation);
			
			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
				continue;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
			
			// adjust priority by travelTimeCoefficient
			
			double priority = conf.ai_combat_priority_pod * travelTimeCoefficient;
			
			debug
			(
				"\t\t(%3d,%3d) -> (%3d,%3d)"
				" priority=%5.2f"
				" conf.ai_combat_priority_pod=%5.2f"
				" travelTime=%2d travelTimeCoefficient=%5.2f\n"
				, vehicle->x, vehicle->y, getX(podLocation), getY(podLocation)
				, priority
				, conf.ai_combat_priority_pod
				, travelTime, travelTimeCoefficient
			);
			
			// add task
			
			taskPriorities.push_back({priority, vehicleId, TT_MOVE, podLocation, travelTime});
			
		}
		
	}
	
}

void selectAssemblyLocation()
{
	debug("selectAssemblyLocation - %s\n", MFactions[aiFactionId].noun_faction);
	
	bool fungusRoad = has_project(aiFactionId, FAC_XENOEMPATHY_DOME);
	bool fungusEasy = getFaction(aiFactionId)->SE_planet_pending > 0;
	
	// group tasks by attack target
	
	std::map<MAP *, std::vector<Task *>> attackTargetTasks;
	
	for (std::pair<const int, Task> &taskEntry : aiData.tasks)
	{
		int vehicleId = taskEntry.first;
		Task &task = taskEntry.second;
		
		if (task.type != TT_MELEE_ATTACK)
			continue;
		
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *attackTarget = task.getAttackTarget();
		
		if (attackTarget == nullptr)
			continue;
		
		bool attackTargetOcean = is_ocean(attackTarget);
		
		if (attackTargetOcean && triad == TRIAD_LAND || !attackTargetOcean && triad == TRIAD_SEA)
			continue;
		
		// add task
		
		attackTargetTasks[attackTarget].push_back(&task);
		
	}
	
	// process attackTargetTasks
	
	for (std::pair<MAP * const, std::vector<Task *>> &attackTargetTaskEntry : attackTargetTasks)
	{
		MAP *attackTarget = attackTargetTaskEntry.first;
		std::vector<Task *> &tasks = attackTargetTaskEntry.second;
		bool attackTargetOcean = is_ocean(attackTarget);
		
		if (tasks.size() == 0)
			continue;
		
		debug("\t(%3d,%3d) <-\n", getX(attackTarget), getY(attackTarget));
		
		// find 
		
		double xSum = 0.0;
		double ySum = 0.0;
		
		for (Task *task : tasks)
		{
			int vehicleId = task->getVehicleId();
			VEH *vehicle = getVehicle(vehicleId);
			
			xSum += (double)vehicle->x;
			ySum += (double)vehicle->y;
			
		}
		
		double centerX = xSum / (double)tasks.size();
		double centerY = ySum / (double)tasks.size();
		
		MAP *assemblyLocation = nullptr;
		double assemblyLocationProximity = DBL_MAX;
		int assemblyLocationPreference = 0;
		
		for (MAP *tile : getAdjacentTiles(attackTarget))
		{
			int tileX = getX(tile);
			int tileY = getY(tile);
			bool tileOcean = is_ocean(tile);
			
			if (attackTargetOcean != tileOcean)
				continue;
			
			double dx = abs(centerX - (double)tileX);
			double dy = abs(centerY - (double)tileY);
			
			if (dx > (double)*map_half_x)
			{
				dx = (double)*map_axis_x - dx;
			}
			
			double proximity = sqrt(dx * dx + dy * dy);
			
			int preference = 0;
			
			if (!attackTargetOcean)
			{
				if (map_has_item(tile, TERRA_MAGTUBE))
				{
					preference = 10;
				}
				else if (map_has_item(tile, TERRA_ROAD) || fungusRoad && map_has_item(tile, TERRA_FUNGUS))
				{
					preference = 8;
				}
				else if (map_has_item(tile, TERRA_RIVER))
				{
					preference = 6;
				}
				else if (fungusEasy || !map_has_item(tile, TERRA_FUNGUS))
				{
					preference = 4;
				}
				else
				{
					preference = 2;
				}
				
				if (isRoughTerrain(tile))
				{
					preference++;
				}
				
			}
			
			if (proximity < assemblyLocationProximity || proximity < assemblyLocationProximity + 1.0 && preference > assemblyLocationPreference)
			{
				assemblyLocation = tile;
				assemblyLocationProximity = proximity;
				assemblyLocationPreference = preference;
			}
			
		}
		
		if (assemblyLocation == nullptr)
			continue;
		
		debug("\t\t(%3d,%3d)\n", getX(assemblyLocation), getY(assemblyLocation));
		
		// redirect vehicles
		
		for (Task *task : tasks)
		{
			int vehicleId = task->getVehicleId();
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// exclude those in close proximity of target
			
			if (getRange(vehicleTile, attackTarget) <= 1)
				continue;
			
			// change destination
			
			task->destination = assemblyLocation;
			debug
			(
				"\t\t\t(%3d,%3d)"
				" -> (%3d,%3d)"
				" => (%3d,%3d)"
				"\n"
				, getVehicle(task->getVehicleId())->x, getVehicle(task->getVehicleId())->y
				, getX(task->getDestination()), getY(task->getDestination())
				, getX(task->attackTarget), getY(task->attackTarget)
			);
			
		}
		
	}
	
}

/*
Assemble atacking forces for time coordinated attack.
Hold early vehicles those until powerful enough army accumulates.
*/
void coordinateAttack()
{
	debug("coordinateAttack - %s\n", MFactions[aiFactionId].noun_faction);
	
	// group tasks by attack target
	
	std::map<MAP *, std::vector<Task *>> enemyStackTasks;
	
	for (std::pair<const int, Task> &taskEntry : aiData.tasks)
	{
		Task &task = taskEntry.second;
		MAP *destination = task.getDestination();
		MAP *attackTarget = task.getAttackTarget();
		
		if (task.type != TT_MELEE_ATTACK && task.type != TT_LONG_RANGE_FIRE)
			continue;
		
		if (destination == nullptr)
			continue;
		
		// get enemy stack info
		
		if (aiData.enemyStacks.count(attackTarget) == 0)
			continue;
		
		EnemyStackInfo enemyStackInfo = aiData.enemyStacks.at(attackTarget);
		
		// coordination exclusions
		
		if (enemyStackInfo.alienSporeLauncher || enemyStackInfo.alienMindWorms)
			continue;
		
		// add task
		
		enemyStackTasks[attackTarget].push_back(&task);
		
	}
	
	// process destinationTasks
	
	for (std::pair<MAP * const, std::vector<Task *>> &enemyStackTaskEntry : enemyStackTasks)
	{
		MAP *attackTarget = enemyStackTaskEntry.first;
		std::vector<Task *> &tasks = enemyStackTaskEntry.second;
		
		// get enemy stack
		
		EnemyStackInfo &enemyStackInfo = aiData.enemyStacks.at(attackTarget);
		
		debug("\t(%3d,%3d) <-\n", getX(attackTarget), getY(attackTarget));
		
		// group vehicles by travel time
		
		std::map<int, std::vector<int>> travelTimeVehicleGroups;
		std::map<int, double> vehiclePrimaryEffects;
		
		for (Task *task : tasks)
		{
			int vehicleId = task->getVehicleId();
			MAP *destination = task->getDestination();
			ATTACK_TYPE attackType = (task->type == TT_MELEE_ATTACK ? AT_MELEE : AT_ARTILLERY);
			CombatEffect combatEffect = enemyStackInfo.getVehicleEffect(vehicleId, attackType);
			bool primary = isPrimaryEffect(vehicleId, attackTarget, enemyStackInfo, combatEffect.combatType);
			
			if (!primary)
				continue;
			
			// get travelTime
			
			int travelTime = getVehicleATravelTime(vehicleId, destination);
			
			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
			{
				debug("[ERROR] Cannot determine travel time.\n");
				continue;
			}
			
			// store vehicle
			
			travelTimeVehicleGroups[travelTime].push_back(vehicleId);
			
			vehiclePrimaryEffects[vehicleId] = combatEffect.effect;
			
		}
		
		// find minimum travel time groups to destroy target stack
		
		int minTravelTime = -1;
		double providedEffect = 0.0;
		
		for (const std::pair<int, std::vector<int>> &travelTimeVehicleGroupEntry : travelTimeVehicleGroups)
		{
			int travelTime = travelTimeVehicleGroupEntry.first;
			const std::vector<int> &vehicleGroup = travelTimeVehicleGroupEntry.second;
			
			for (int vehicleId : vehicleGroup)
			{
				// accumulate providedEffect
				
				providedEffect += vehiclePrimaryEffects[vehicleId];
				
			}
			
			// check if accumulated providedEffect is sufficient
			
			if (providedEffect >= enemyStackInfo.requiredEffect)
			{
				minTravelTime = travelTime;
				break;
			}
			
		}
		
		debug
		(
			"\t\tminTravelTime=%2d"
			" providedEffect=%5.2f"
			" enemyStackInfo.requiredEffect=%5.2f"
			"\n"
			, minTravelTime
			, providedEffect
			, enemyStackInfo.requiredEffect
		);
		
		// do not hold vehicle if travel time is undefined
		
		if (minTravelTime == -1)
			continue;
		
		// release vehicle too close to destination if they cannot effectivelly survive
			
		for (const std::pair<int, std::vector<int>> &travelTimeVehicleGroupEntry : travelTimeVehicleGroups)
		{
			int travelTime = travelTimeVehicleGroupEntry.first;
			const std::vector<int> &vehicleGroup = travelTimeVehicleGroupEntry.second;
			
			if (travelTime < minTravelTime)
			{
				for (int vehicleId : vehicleGroup)
				{
					VEH *vehicle = getVehicle(vehicleId);
					Task *task = getTask(vehicleId);
					MAP *vehicleTile = getVehicleMapTile(vehicleId);
					double effect = vehiclePrimaryEffects.at(vehicleId);
					
					debug("\t\t[%4d] (%3d,%3d) travelTime=%3d\n", vehicleId, vehicle->x, vehicle->y, travelTime);
					
					if (effect >= 1.25)
					{
						debug("\t\t\teffective attack - do not wait\n");
						continue;
					}
					
					// do not stop vehicle far from fungal tower
					
					if (enemyStackInfo.alienFungalTower && getRange(vehicleTile, attackTarget) > 1)
					{
						debug("\t\t\tfungal tower\n");
						continue;
					}
					
					// do not coordinate attack for bombardment (even though it may be dangerous)
					
					if (task->type == TT_LONG_RANGE_FIRE && enemyStackInfo.artilleryVehicleIds.size() == 0)
					{
						debug("\t\t\tbombardment\n");
						continue;
					}
					
					// exclude surface vehicle in different association
					
					switch (vehicle->triad())
					{
					case TRIAD_SEA:
						if (!is_ocean(attackTarget) || !isSameOceanAssociation(vehicleTile, attackTarget, aiFactionId))
						{
							debug("\t\t\tsea unreachable\n");
							continue;
						}
						break;
					case TRIAD_LAND:
						if (is_ocean(attackTarget) || !isSameLandAssociation(vehicleTile, attackTarget, aiFactionId))
						{
							debug("\t\t\tland different association\n");
							continue;
						}
						break;
					}
					
					task->type = TT_HOLD;
					task->clearDestination();
					debug("\t\t\twait\n");
					
				}
				
			}
			else
			{
				for (int vehicleId : vehicleGroup)
				{
					VEH *vehicle = getVehicle(vehicleId);
					debug("\t\t[%4d] (%3d,%3d) travelTime=%3d\n", vehicleId, vehicle->x, vehicle->y, travelTime);
				}
				
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
Finds first hostile target on the shortest path to destination.
Shortest path to destination ignores all hostile locations and zocs.
*/
MAP *findTarget(int vehicleId, MAP *destination)
{
	VEH *vehicle = getVehicle(vehicleId);
	UNIT *unit = getUnit(vehicle->unit_id);
	MAP *origin = getVehicleMapTile(vehicleId);
	int triad = vehicle->triad();
	int speed = getVehicleSpeedWithoutRoads(vehicleId);
	int destinationX = getX(destination);
	int destinationY = getY(destination);
	
	switch (triad)
	{
	case TRIAD_AIR:
		{
			switch (unit->chassis_type)
			{
			case CHS_NEEDLEJET:
			case CHS_COPTER:
			case CHS_MISSILE:
				{
					// find closest airbase
					
					MAP *nearestAirbase = getNearestAirbase(vehicle->faction_id, destination);
					int nearestAirbaseRange = map_range(destinationX, destinationY, getX(nearestAirbase), getY(nearestAirbase));
					
					// too far
					
					if (speed < nearestAirbaseRange)
						return nullptr;
					
					// set new origin
					
					origin = nearestAirbase;
					
				}
				break;
				
			}
			
			int originX = getX(origin);
			int originY = getY(origin);
			
			int directVectorDistance = vector_dist(originX, originY, destinationX, destinationY);
			int directVectorDistanceSquared = directVectorDistance * directVectorDistance;
			
			// find closest enemy stacks between origin and destination close enough to the path
			
			MAP *target = nullptr;
			int targetRange = INT_MAX;
			
			for (std::pair<MAP *, EnemyStackInfo> enemyStackEntry : aiData.enemyStacks)
			{
				MAP *enemyStackLocation = enemyStackEntry.first;
				int enemyStackLocationX = getX(enemyStackLocation);
				int enemyStackLocationY = getY(enemyStackLocation);
				
				// check ellipse vector distance
				
				int originVectorDistance = vector_dist(originX, originY, enemyStackLocationX, enemyStackLocationY);
				int destinationVectorDistance = vector_dist(destinationX, destinationY, enemyStackLocationX, enemyStackLocationY);

				int originVectorDistanceSquared = originVectorDistance * originVectorDistance;
				int destinationVectorDistanceSquared = destinationVectorDistance * destinationVectorDistance;
				
				if (originVectorDistanceSquared + destinationVectorDistanceSquared > directVectorDistanceSquared)
					continue;
				
				// select best range
				
				int range = map_range(originX, originY, enemyStackLocationX, enemyStackLocationY);
				
				if (range < targetRange)
				{
					target = enemyStackLocation;
					targetRange = range;
				}
				
			}
			
			return target;
			
		}
		break;
		
	case TRIAD_SEA:
		{
			
		}
		break;
		
	}
	
	return nullptr;
	
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

bool isPrimaryEffect(int vehicleId, MAP *enemyStackTile, EnemyStackInfo &enemyStackInfo, COMBAT_TYPE combatType)
{
	VEH *vehicle = getVehicle(vehicleId);
	int triad = vehicle->triad();
	bool enemyStackTileOcean = is_ocean(enemyStackTile);
	
	// melee is always primary
	
	if (combatType == CT_MELEE)
		return true;
	
	// artillery duel
	
	if (combatType == CT_ARTILLERY_DUEL)
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

