#include <map>
#include <unordered_map>
#include "engine.h"
#include "terranx_wtp.h"
#include "ai.h"
#include "aiMoveCombat.h"

std::unordered_map<int, COMBAT_ORDER> combatOrders;

/*
Prepares combat orders.
*/
void moveCombatStrategy()
{
	populateCombatLists();
	
	baseProtection();
	
	fightStrategy();
	
	popPods();

	nativeCombatStrategy();
	
	factionCombatStrategy();
	
	countTransportableVehicles();
	
}

void populateCombatLists()
{
	combatOrders.clear();

}

/*
Comparison function for base defenders.
Sort by police then trance then range.
*/
bool compareDefenderVehicleRanges(const int vehicleRange1, const int vehicleRange2)
{
	int id1 = vehicleRange1 % 10000;
	int range1 = vehicleRange1 / 10000;
	int id2 = vehicleRange2 % 10000;
	int range2 = vehicleRange2 / 10000;
	
	return
		vehicle_has_ability(id1, ABL_POLICE_2X) && !vehicle_has_ability(id2, ABL_POLICE_2X)
		||
		vehicle_has_ability(id1, ABL_TRANCE) && !vehicle_has_ability(id2, ABL_TRANCE)
		||
		range1 < range2
	;
	
}

void baseProtection()
{
	debug("baseProtection - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		int factionId = base->faction_id;
		Faction *faction = &(Factions[factionId]);
		BaseStrategy *baseStrategy = &(aiData.baseStrategies[baseId]);
		std::vector<int> garrison = baseStrategy->garrison;
		
		debug("\t%-25s\n", base->name);
		
		// calculate base psi defense modifier
		
		double basePsiDefenseMultiplier =
			(!is_ocean(base) ? (double)Rules->psi_combat_land_numerator / (double)Rules->psi_combat_land_denominator : (double)Rules->psi_combat_sea_numerator / (double)Rules->psi_combat_sea_denominator)
			*
			getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def)
			*
			(isWithinFriendlySensorRange(base->faction_id, base->x, base->y) && (!is_ocean(base) || conf.combat_bonus_sensor_ocean) ? getPercentageBonusMultiplier(Rules->combat_defend_sensor) : 1.0)
		;
		
		// get allowed police unit count
		
		int allowedPoliceUnitCount;
		
		switch (faction->SE_police_pending)
		{
		case 3:
		case 2:
			allowedPoliceUnitCount = 3;
			break;
		case 1:
			allowedPoliceUnitCount = 2;
			break;
		case 0:
		case -1:
			allowedPoliceUnitCount = 1;
			break;
		default:
			allowedPoliceUnitCount = 0;
		}
		
		// get required police unit count
		
		int garrisonSize = (int)garrison.size();
		int requiredPoliceUnitCount = ((base->talent_total == 0 && base->drone_total == 0) ? 0 : std::min(allowedPoliceUnitCount, garrisonSize + (base->talent_total - base->drone_total)));
		
		// get required native protection
		
		double requiredNativeProtection = (*current_turn <= conf.aliens_fight_half_strength_unit_turn ? 0.5 : 1.0) * getAlienMoraleModifier() * conf.ai_combat_native_protection_minimal + conf.ai_combat_native_protection_per_pop * (double)faction->fungal_pop_count * ((double)base->eco_damage / 100.0);
		
		debug("\t\trequiredPoliceUnitCount=%d, requiredNativeProtection=%f\n", requiredPoliceUnitCount, requiredNativeProtection);
		
		// running variables
		
		int policeUnitCount = 0;
		double nativeProtection = 0.0;
		
		// select defenders
		
		debug("\t\tselect defenders\n");
		
		std::vector<int> defenders;
		
		for (int vehicleId : aiData.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			UNIT *unit = &(Units[vehicle->unit_id]);
			int vehicleWeaponOffenseValue = getVehicleWeaponOffenseValue(vehicleId);
			int vehicleArmorDefenseValue = getVehicleArmorDefenseValue(vehicleId);
			
			// without task and order
			
			if (!isVehicleAvailable(vehicleId))
				continue;
			
			// land
			
			if (vehicle->triad() != TRIAD_LAND)
				continue;
			
			// infantry
			
			if (unit->chassis_type != CHS_INFANTRY)
				continue;
			
			// scout or defensive
			
			if (vehicleWeaponOffenseValue > vehicleArmorDefenseValue)
				continue;
			
			// not in other base
			
			if (!(vehicle->x == base->x && vehicle->y == base->y) && map_has_item(getVehicleMapTile(vehicleId), TERRA_BASE_IN_TILE))
				continue;
			
			// close
			
			int range = map_range(base->x, base->y, vehicle->x, vehicle->y);
			
			if (range > 10)
				continue;
			
			// police and trance abilities
			
			int abilityMarker;
			
			if (vehicle_has_ability(vehicleId, ABL_POLICE_2X))
			{
				abilityMarker = 0;
			}
			else if (vehicle_has_ability(vehicleId, ABL_TRANCE))
			{
				abilityMarker = 1;
			}
			else
			{
				abilityMarker = 2;
			}
			
			// add defender
			
			defenders.push_back(vehicleId + 10000 * range + 100000 * abilityMarker);
			
			debug("\t\t\t[%4d] (%3d,%3d) %-32s \n", vehicleId, vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
			
		}
		
		if (DEBUG)
		{
			debug("\t\tvehicleRange\n");
			for(int vehicleRange : defenders)
			{
				debug("\t\t\t%8d = {%4d, %5d}\n", vehicleRange, vehicleRange % 10000, vehicleRange / 10000);
			}
			
			fflush(debug_log);
			
		}
		
		// sort defenders
		
		std::sort(defenders.begin(), defenders.end());
		
		// assign defenders
		
		debug("\t\tassign defenders\n");
		
		while (defenders.size() > 0)
		{
			int vehicleId = defenders.front() % 10000;
			defenders.erase(defenders.begin());
			
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			// hold defender
			
			transitVehicle(vehicleId, Task(HOLD, vehicleId, getMapTile(base->x, base->y)));
			
			debug("\t\t\t(%3d,%3d) %-32s \n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
			
			// update counters
			
			policeUnitCount++;
			nativeProtection += getVehiclePsiDefenseStrength(vehicleId, true) * basePsiDefenseMultiplier;
			
			debug("\t\t\t\tpoliceUnitCount=%d, nativeProtection=%f\n", policeUnitCount, nativeProtection);
			
			// exit condition
			
			if (policeUnitCount >= requiredPoliceUnitCount && nativeProtection >= requiredNativeProtection)
				break;
			
		}
		
		// handle defense demand
		
		if (baseStrategy->defenseDemand <= 0.0)
		{
			// release one defender without task
			
			debug("\t\trelease defender\n");
			
			for (int vehicleRange : defenders)
			{
				int vehicleId = vehicleRange % 10000;
				VEH *vehicle = &(Vehicles[vehicleId]);
				
				// in base
				
				if (!(vehicle->x == base->x && vehicle->y == base->y))
					continue;
				
				// on hold
				
				if (vehicle->move_status != ORDER_HOLD)
					continue;
				
				// release
				
				vehicle->move_status = ORDER_NONE;
				
				debug("\t\t\t(%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
				
				break;
				
			}
			
		}
		else if (baseStrategy->defenseDemand <= conf.ai_combat_defense_demand_threshold)
		{
			// do nothing
		}
		else
		{
			// keep current defender
			
			debug("\t\tkeep defender\n");
			
			for (int vehicleRange : defenders)
			{
				int vehicleId = vehicleRange % 10000;
				VEH *vehicle = &(Vehicles[vehicleId]);
				
				// in base
				
				if (!(vehicle->x == base->x && vehicle->y == base->y))
					continue;
				
				// send to base
				
				transitVehicle(vehicleId, Task(HOLD, vehicleId, getMapTile(base->x, base->y)));
				
				debug("\t\t\t(%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
				
			}
			
			// pull one more defender
			
			debug("\t\tpull defender\n");
			
			for (int vehicleRange : defenders)
			{
				int vehicleId = vehicleRange % 10000;
				VEH *vehicle = &(Vehicles[vehicleId]);
				
				// not in base
				
				if ((vehicle->x == base->x && vehicle->y == base->y))
					continue;
				
				// send to base
				
				transitVehicle(vehicleId, Task(HOLD, vehicleId, getMapTile(base->x, base->y)));
				
				debug("\t\t\t(%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
				
				break;
				
			}
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("- tasks\n");
		
		for (std::pair<const int, Task> &taskEntry : aiData.tasks)
		{
			int vehiclePad0 = taskEntry.first;
			Task *task = &(taskEntry.second);
			
			int vehicleId = getVehicleIdByPad0(vehiclePad0);
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			debug("(%3d,%3d) vehiclePad0=%d, vehicleId=%d, taskType=%d, range=%d\n", vehicle->x, vehicle->y, vehiclePad0, vehicleId, task->type, task->getDestinationRange());
			
		}
			
	}
		
}

/*
Composes anti-native combat strategy.
*/
void nativeCombatStrategy()
{
	debug("nativeCombatStrategy %s\n", MFactions[aiFactionId].noun_faction);

	// list aliens on land territory
	
	std::unordered_set<int> alienVehicleIds;

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);
		MAP *vehicleTile = getVehicleMapTile(id);
		
		if (vehicle->faction_id == 0 && isLandRegion(vehicleTile->region) && vehicleTile->owner == aiFactionId)
		{
			alienVehicleIds.insert(id);
		}

	}
	
	// list scouts
	
	std::unordered_set<int> scoutVehicleIds;
	
	for (int scoutVehicleId : aiData.scoutVehicleIds)
	{
		VEH *scoutVehicle = &(Vehicles[scoutVehicleId]);
		MAP *scoutVehicleTile = getVehicleMapTile(scoutVehicleId);
		
		// no task or order

		if (!isVehicleAvailable(scoutVehicleId))
			continue;

		// exclude immobile
		
		if (scoutVehicle->triad() == (isOceanRegion(scoutVehicleTile->region) ? TRIAD_LAND : TRIAD_SEA))
			continue;
		
		// add to list
		
		scoutVehicleIds.insert(scoutVehicleId);
		
	}
		
	// direct scouts
	
	for (int alienVehicleId : alienVehicleIds)
	{
		VEH *alienVehicle = &(Vehicles[alienVehicleId]);
		MAP *alienVehicleTile = getVehicleMapTile(alienVehicleId);
		
		double requiredDamage = 1.5 * (double)(10 - alienVehicle->damage_taken / alienVehicle->reactor_type());
		
		while (requiredDamage > 0.0)
		{
			int bestScoutVehicleId = -1;
			double bestWeight = 0.0;
			
			for (int scoutVehicleId : scoutVehicleIds)
			{
				VEH *scoutVehicle = &(Vehicles[scoutVehicleId]);
				MAP *scoutVehicleTile = getVehicleMapTile(scoutVehicleId);
				
				// same region
				
				if (alienVehicleTile->region != scoutVehicleTile->region)
					continue;
				
				// get range
				
				int range = map_range(scoutVehicle->x, scoutVehicle->y, alienVehicle->x, alienVehicle->y);
				
				// get time
				
				double time = (double)range / (double)veh_chassis_speed(scoutVehicleId);
				
				// build weight on time and cost and morale
				
				double weight = getVehicleMorale(scoutVehicleId, false) / (double)Units[scoutVehicle->unit_id].cost * pow(0.5, time / 10.0);
				
				// prefer empath
				
				if (vehicle_has_ability(scoutVehicleId, ABL_EMPATH))
				{
					weight *= 2.0;
				}
				
				// update best weight
				
				if (bestScoutVehicleId == -1 || weight > bestWeight)
				{
					bestScoutVehicleId = scoutVehicleId;
					bestWeight = weight;
				}
				
			}
			
			// not found
			
			if (bestScoutVehicleId == -1)
			{
				// add to hunter demand
				
				std::unordered_map<int, int>::iterator regionAlienHunterDemandsIterator = aiData.regionAlienHunterDemands.find(alienVehicleTile->region);
				
				if (regionAlienHunterDemandsIterator == aiData.regionAlienHunterDemands.end())
				{
					aiData.regionAlienHunterDemands.insert({alienVehicleTile->region, 1});
				}
				else
				{
					regionAlienHunterDemandsIterator->second++;
				}
				
				// exit cycle
				
				break;
				
			}
			
			// set attack order
			
			setAttackOrder(bestScoutVehicleId, alienVehicleId);
			
			// remove from list
			
			scoutVehicleIds.erase(bestScoutVehicleId);
			
			// compute damage
			
			VEH *bestScoutVehicle = &(Vehicles[bestScoutVehicleId]);
			
			double damage = (double)(10 - bestScoutVehicle->damage_taken / bestScoutVehicle->reactor_type()) * getVehiclePsiOffenseStrength(bestScoutVehicleId);
			
			// update required damage
			
			requiredDamage -= damage;
			
		}
		
	}

}

void factionCombatStrategy()
{
	// clear orders for vehicles in war zone
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int vehicleTriad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		for (int otherVehicleId = 0; otherVehicleId < *total_num_vehicles; otherVehicleId++)
		{
			VEH *otherVehicle = &(Vehicles[otherVehicleId]);
			int otherVehicleTriad = otherVehicle->triad();
			MAP *otherVehicleTile = getVehicleMapTile(otherVehicleId);
			
			// skip alien faction
			
			if (otherVehicle->faction_id == 0)
				continue;
			
			// skip not hostile
			
			if (!at_war(aiFactionId, otherVehicle->faction_id))
				continue;
			
			// skip different realm
			
			if
			(
				(vehicleTriad == TRIAD_LAND && otherVehicleTriad == TRIAD_SEA)
				||
				(vehicleTriad == TRIAD_SEA && otherVehicleTriad == TRIAD_LAND)
			)
			{
				continue;
			}
			
			// skip different regions
			
			if (vehicleTriad != TRIAD_AIR && vehicleTile->region != otherVehicleTile->region)
				continue;
			
			// skip not within two turns reach
			
			int reachRange = 2 * Chassis[Units[vehicle->unit_id].chassis_type].speed;
			int range = map_range(vehicle->x, vehicle->y, otherVehicle->x, otherVehicle->y);
			
			if (range > reachRange)
				continue;
			
			// otherwise clear order
			
			combatOrders.erase(vehicleId);
			
		}
		
	}
	
}

void fightStrategy()
{
	debug("fightStrategy - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		// without task
		
		if (hasTask(vehicleId))
			continue;
		
		// no task or order
		
		if (!isVehicleAvailable(vehicleId))
			continue;
		
		VEH *vehicle = &(Vehicles[vehicleId]);
		debug("\t(%3d,%3d) %s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
		
		// attack for win
		
		int bestEnemyVehicleId = -1;
		double bestBattleOdds = 0.0;
		
		for (int enemyVehicleId : getReachableEnemies(vehicleId))
		{
			double battleOdds = getBattleOdds(vehicleId, enemyVehicleId);
			
			// ignore not sure kill
			
			if (battleOdds < 1.5)
				continue;
			
			// update best
			
			if (battleOdds > bestBattleOdds)
			{
				bestEnemyVehicleId = enemyVehicleId;
				bestBattleOdds = battleOdds;
			}
			
		}
		
		if (bestEnemyVehicleId != -1)
		{
			transitVehicle(vehicleId, Task(MOVE, vehicleId, nullptr, bestEnemyVehicleId));
			
			VEH *bestEnemyVehicle = &(Vehicles[bestEnemyVehicleId]);
			debug("\t\t->(%3d,%3d) %s\n", bestEnemyVehicle->x, bestEnemyVehicle->y, Units[bestEnemyVehicle->unit_id].name);
			
		}
		
	}
	
}

void popPods()
{
	popLandPods();
	popOceanPods();
}

void popLandPods()
{
	debug("popLandPods\n");
	
	// collect scouts
	
	std::unordered_set<int> scoutVehicleIds;
	
	for (int vehicleId : aiData.scoutVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// no task or order
		
		if (!isVehicleAvailable(vehicleId))
			continue;
		
		// not damaged
		
		if (isVehicleCanHealAtThisLocation(vehicleId))
			continue;
		
		// land triad
		
		if (vehicle->triad() != TRIAD_LAND)
			continue;
		
		// if next to alien attack immediatelly and don't use for pod popping
		
		bool reactionFire = false;
		
		for (int alienVehicleId = 0; alienVehicleId < *total_num_vehicles; alienVehicleId++)
		{
			VEH *alienVehicle = &(Vehicles[alienVehicleId]);
			
			if (alienVehicle->faction_id != 0)
				continue;
			
			if (alienVehicle->triad() != TRIAD_LAND)
				continue;
			
			if (map_range(vehicle->x, vehicle->y, alienVehicle->x, alienVehicle->y) <= 2)
			{
				// set task
				
				transitVehicle(vehicleId, Task(MOVE, vehicleId, nullptr, alienVehicleId));
				
				debug("\t(%3d,%3d) -> (%3d,%3d)\n", vehicle->x, vehicle->y, alienVehicle->x, alienVehicle->y);
				
				// do not add to list
				
				reactionFire = true;
				
				break;
				
			}
			
		}
		
		if (reactionFire)
			continue;
		
		// add to list
		
		scoutVehicleIds.insert(vehicleId);
		
	}
	
	// collect pods
	
	std::unordered_set<MAP *> podLocations;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int x = getX(mapIndex);
		int y = getY(mapIndex);
		MAP *tile = getMapTile(mapIndex);
		
		// land
		
		if (!isLandRegion(tile->region))
			continue;
		
		if (goody_at(x, y) != 0)
		{
			// exclude those targetted by own vehicle
			
			if (isFactionTargettedLocation(tile, aiFactionId))
				continue;
			
			podLocations.insert(tile);
			
		}
		
	}
	
	// match scouts with pods
	
	while (scoutVehicleIds.size() > 0 && podLocations.size() > 0)
	{
		int closestScoutVehicleId = -1;
		MAP *closestPodLocation;
		int minRange = INT_MAX;
		
		for (int vehicleId : scoutVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			for (MAP *podLocation : podLocations)
			{
				int podLocationX = getX(podLocation);
				int podLocationY = getY(podLocation);
				
				int range = map_range(vehicle->x, vehicle->y, podLocationX, podLocationY);
				
				if (range < minRange)
				{
					closestScoutVehicleId = vehicleId;
					closestPodLocation = podLocation;
					minRange = range;
				}
				
			}
			
		}
		
		// not found or too far
		
		if (!(closestScoutVehicleId != -1 && closestPodLocation != nullptr && minRange <= 10))
			break;
		
		// set tasks
		
		transitVehicle(closestScoutVehicleId, Task(MOVE, closestScoutVehicleId, closestPodLocation));
		
		debug("\t(%3d,%3d) -> (%3d,%3d)\n", Vehicles[closestScoutVehicleId].x, Vehicles[closestScoutVehicleId].y, getX(closestPodLocation), getY(closestPodLocation));
		
		// remove mathed pair
		
		scoutVehicleIds.erase(closestScoutVehicleId);
		podLocations.erase(closestPodLocation);
		
	}
	
}

void popOceanPods()
{
	debug("popOceanPods\n");
	
	// process regions
	
	for (int region : aiData.presenceRegions)
	{
		// ocean region
		
		if (!isOceanRegion(region))
			continue;
		
		// collect scouts
		
		std::unordered_set<int> scoutVehicleIds;
		
		for (int vehicleId : aiData.scoutVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// exclude in bases
			
			if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
				continue;
			
			// without combat orders
			
			if (!isVehicleAvailable(vehicleId))
				continue;
			
			// without task
			
			if (hasTask(vehicleId))
				continue;
			
			// not damaged
			
			if (isVehicleCanHealAtThisLocation(vehicleId))
				continue;
			
			// within region
			
			if (vehicleTile->region != region)
				continue;
			
			// sea triad
			
			if (vehicle->triad() != TRIAD_SEA)
				continue;
			
			// add to list
			
			scoutVehicleIds.insert(vehicleId);
			
		}
		
		// collect pods
		
		std::unordered_set<MAP *> podLocations;
		
		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			int x = getX(mapIndex);
			int y = getY(mapIndex);
			MAP *tile = getMapTile(mapIndex);
			
			// within region
			
			if (tile->region != region)
				continue;
			
			if (goody_at(x, y) != 0)
			{
				// exclude those targetted by own vehicle
				
				if (isFactionTargettedLocation(tile, aiFactionId))
					continue;
				
				podLocations.insert(tile);
				
			}
			
		}
		
		// match scouts with pods
		
		while (scoutVehicleIds.size() > 0 && podLocations.size() > 0)
		{
			int closestScoutVehicleId = -1;
			MAP *closestPodLocation;
			int minRange = INT_MAX;
			
			for (int vehicleId : scoutVehicleIds)
			{
				VEH *vehicle = &(Vehicles[vehicleId]);
				
				for (MAP *podLocation : podLocations)
				{
					int podLocationX = getX(podLocation);
					int podLocationY = getY(podLocation);
					
					int range = map_range(vehicle->x, vehicle->y, podLocationX, podLocationY);
					
					if (range < minRange)
					{
						closestScoutVehicleId = vehicleId;
						closestPodLocation = podLocation;
						minRange = range;
					}
					
				}
				
			}
			
			// not found
			
			if (closestScoutVehicleId == -1 || closestPodLocation == nullptr)
				break;
			
			// set tasks
			
			transitVehicle(closestScoutVehicleId, Task(MOVE, closestScoutVehicleId, closestPodLocation));
			
			debug("\t(%3d,%3d) -> (%3d,%3d)\n", Vehicles[closestScoutVehicleId].x, Vehicles[closestScoutVehicleId].y, getX(closestPodLocation), getY(closestPodLocation));
			
			// remove mathed pair
			
			scoutVehicleIds.erase(closestScoutVehicleId);
			podLocations.erase(closestPodLocation);
			
		}
		
	}
	
}

int compareVehicleValue(VEHICLE_VALUE o1, VEHICLE_VALUE o2)
{
	return (o1.value > o2.value);
}

int moveCombat(int vehicleId)
{
	// get vehicle

	VEH *vehicle = &(Vehicles[vehicleId]);

	// restore ai vehicleId

	int aiVehicleId = vehicle->pad_0;

	debug("moveCombat: [%d->%d] (%3d,%3d)\n", aiVehicleId, vehicleId, vehicle->x, vehicle->y);

	// process vehicle order if any assigned

	if (combatOrders.count(aiVehicleId) != 0)
	{
		// get order

		COMBAT_ORDER *combatOrder = &(combatOrders[aiVehicleId]);

		// apply order

		return applyCombatOrder(vehicleId, combatOrder);

	}
	
	// default to Thinker for war algorithms
	
	if (isProbeVehicle(vehicleId))
	{
		return enemy_move(vehicleId);
	}
	else
	{
//		return mod_enemy_move(vehicleId);
		return enemy_move(vehicleId);
	}
	
}

int applyCombatOrder(int id, COMBAT_ORDER *combatOrder)
{
	VEH *vehicle = &(Vehicles[id]);
	
	// kill
	
	if (combatOrder->kill)
	{
		veh_kill(id);
	}
	
	// get coordinates
	
	int x;
	int y;
	
	if (combatOrder->x != -1 && combatOrder->y != -1)
	{
		x = combatOrder->x;
		y = combatOrder->y;
	}
	else if (combatOrder->enemyAIId != -1)
	{
		VEH *enemyVehicle = getVehicleByAIId(combatOrder->enemyAIId);

		// enemy not found

		if (enemyVehicle == NULL || enemyVehicle->x == -1 || enemyVehicle->y == -1)
			return mod_enemy_move(id);
		
		x = enemyVehicle->x;
		y = enemyVehicle->y;
		
	}
	else
	{
		x = vehicle->x;
		y = vehicle->y;
		
	}

	// destination is unreachable

	if (!isreachable(id, x, y))
		return mod_enemy_move(id);
	
	// at destination

	if (vehicle->x == x && vehicle->y == y)
	{
		// execute order if instructed
		
		if (combatOrder->order != -1)
		{
			setVehicleOrder(id, combatOrder->order);
		}
		else
		{
			mod_veh_skip(id);
		}

	}

	// not at destination

	else
	{
		setMoveTo(id, x, y);
	}

	return SYNC;

}

void setMoveOrder(int vehicleId, int x, int y, int order)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	debug("setMoveOrder: [%3d](%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, x, y);

	if (combatOrders.find(vehicleId) == combatOrders.end())
	{
		transitVehicle(vehicleId, Task(ORDER, vehicleId, getMapTile(x, y), -1, order, -1));
	}

}

void setAttackOrder(int vehicleId, int enemyVehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	debug("setAttackOrder: [%3d](%3d,%3d) -> [%3d]\n", vehicleId, vehicle->x, vehicle->y, enemyVehicleId);

	if (combatOrders.find(vehicleId) == combatOrders.end())
	{
		transitVehicle(vehicleId, Task(MOVE, vehicleId, nullptr, enemyVehicleId));
	}

}

void setKillOrder(int vehicleId)
{
	setTask(vehicleId, Task(KILL, vehicleId));
	
}

/*
Overrides sea explorer routine.
*/
int processSeaExplorer(int vehicleId)
{
	VEH* vehicle = &(Vehicles[vehicleId]);

	// keep healing if can

	if (isVehicleCanHealAtThisLocation(vehicleId))
	{
		return mod_enemy_move(vehicleId);
	}

	// find the nearest unexplored connected ocean region tile

	MAP *nearestUnexploredLocation = getNearestUnexploredConnectedOceanRegionLocation(vehicle->faction_id, vehicle->x, vehicle->y);

	if (nearestUnexploredLocation == nullptr)
	{
		// nothing more to explore

		// kill unit

		debug("vehicle killed: [%3d] (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y);
		return killVehicle(vehicleId);

	}
	else
	{
		int nearestUnexploredLocationX = getX(nearestUnexploredLocation);
		int nearestUnexploredLocationY = getY(nearestUnexploredLocation);
		
		// move vehicle to unexplored destination

		debug("vehicle moved: [%3d] (%3d,%3d) -> (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, nearestUnexploredLocationX, nearestUnexploredLocationY);
		return moveVehicle(vehicleId, nearestUnexploredLocationX, nearestUnexploredLocationY);

	}

}

/*
Kills vehicle and returns proper value as enemy_move would.
*/
int killVehicle(int vehicleId)
{
	tx_kill(vehicleId);
	return NO_SYNC;

}

/*
Sets vehicle move to order AND moves it there exhausting moves along the way.
This is used in enemy_move routines to avoid vanilla code overriding move_to order.
*/
int moveVehicle(int vehicleId, int x, int y)
{
	setMoveTo(vehicleId, x, y);
	tx_action(vehicleId);
	return SYNC;

}

MAP *getNearestUnexploredConnectedOceanRegionLocation(int factionId, int initialLocationX, int initialLocationY)
{
	debug("getNearestUnexploredConnectedOceanRegionTile: factionId=%d, initialLocationX=%d, initialLocationY=%d\n", factionId, initialLocationX, initialLocationY);

	MAP *location = nullptr;

	// get ocean connected regions

	std::unordered_set<int> connectedOceanRegions = getConnectedOceanRegions(factionId, initialLocationX, initialLocationY);

	debug("\tconnectedOceanRegions\n");

	if (DEBUG)
	{
		for (int connectedOceanRegion : connectedOceanRegions)
		{
			debug("\t\t%d\n", connectedOceanRegion);

		}

	}

	// search map

	int nearestUnexploredConnectedOceanRegionTileRange = 9999;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int x = getX(mapIndex);
		int y = getY(mapIndex);
		MAP *tile = getMapTile(x, y);
		int region = tile->region;

		// only ocean regions

		if (!isOceanRegion(region))
			continue;

		// only connected regions

		if (!connectedOceanRegions.count(region))
			continue;

		// only undiscovered tiles

		if (isMapTileVisibleToFaction(tile, factionId))
			continue;

		// calculate range

		int range = map_range(initialLocationX, initialLocationY, x, y);

		// update map info

		if (location == nullptr || range < nearestUnexploredConnectedOceanRegionTileRange)
		{
			location = tile;
			nearestUnexploredConnectedOceanRegionTileRange = range;
		}

	}

	return location;

}

/*
Returns vehicle own conventional offense value counting weapon, morale, abilities.
*/
double getVehicleConventionalOffenseValue(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);
	
	// get offense value
	
	double offenseValue = (double)Weapon[unit->weapon_type].offense_value;
	
	// psi weapon is evaluated as psi
	
	if (offenseValue < 0)
	{
		return getVehiclePsiOffenseValue(vehicleId);
	}
	
	// times morale modifier
	
	offenseValue *= getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_DISSOCIATIVE_WAVE))
	{
		offenseValue *= 1.0 + 0.5 * 0.25;
	}
	
	if (isVehicleHasAbility(vehicleId, ABL_NERVE_GAS))
	{
		offenseValue *= 1.5;
	}
	
	if (isVehicleHasAbility(vehicleId, ABL_SOPORIFIC_GAS))
	{
		offenseValue *= 1.25;
	}
	
	return offenseValue;
	
}

/*
Returns vehicle own conventional defense value counting armor, morale, abilities.
*/
double getVehicleConventionalDefenseValue(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);
	
	// get defense value
	
	double defenseValue = Armor[unit->armor_type].defense_value;
	
	// psi armor is evaluated as psi
	
	if (defenseValue < 0)
	{
		return getVehiclePsiDefenseValue(vehicleId);
	}
	
	// times morale modifier
	
	defenseValue *= getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_AAA))
	{
		defenseValue *= 1.0 + 0.5 * ((double)Rules->combat_AAA_bonus_vs_air / 100.0);
	}
	
	if (isVehicleHasAbility(vehicleId, ABL_COMM_JAMMER))
	{
		defenseValue *= 1.0 + 0.5 * ((double)Rules->combat_comm_jammer_vs_mobile / 100.0);
	}
	
	return defenseValue;
	
}

/*
Returns vehicle own psi offense value counting morale, abilities.
Also count faction PLANET.
*/
double getVehiclePsiOffenseValue(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	Faction *faction = &(Factions[vehicle->faction_id]);
	
	// get offense value
	
	double offenseValue = getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_EMPATH))
	{
		offenseValue *= 1.0 + (double)Rules->combat_bonus_empath_song_vs_psi / 100.0;
	}
	
	// times faction PLANET
	
	offenseValue *= 1.0 + (double)Rules->combat_psi_bonus_per_PLANET / 100.0 * (double)faction->SE_planet_pending;
	
	// return value
	
	return offenseValue;
	
}

/*
Returns vehicle own Psi defense value counting morale, abilities.
Also count faction PLANET.
*/
double getVehiclePsiDefenseValue(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	Faction *faction = &(Factions[vehicle->faction_id]);
	
	// get defense value
	
	double defenseValue = getVehicleMoraleModifier(vehicleId, false);
	
	// times some abilities
	
	if (isVehicleHasAbility(vehicleId, ABL_TRANCE))
	{
		defenseValue *= 1.0 + ((double)Rules->combat_bonus_trance_vs_psi / 100.0);
	}
	
	// times faction PLANET
	
	if (conf.planet_combat_bonus_on_defense)
	{
		defenseValue *= 1.0 + (double)Rules->combat_psi_bonus_per_PLANET / 100.0 * (double)faction->SE_planet_pending;
	}
	
	// return value
	
	return defenseValue;
	
}

int getRangeToNearestActiveFactionBase(int x, int y)
{
	int minRange = INT_MAX;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		minRange = std::min(minRange, map_range(x, y, base->x, base->y));
		
	}
	
	return minRange;
	
}

/*
Searches for nearest accessible monolith on non-enemy territory.
*/
MAP *getNearestMonolith(int x, int y, int triad)
{
	MAP *startingTile = getMapTile(x, y);
	bool ocean = isOceanRegion(startingTile->region);
	
	MAP *nearestMonolithLocation = nullptr;
	
	// search only for realm unit can move on
	
	if (!(triad == TRIAD_AIR || triad == (ocean ? TRIAD_SEA : TRIAD_LAND)))
		return nullptr;
	
	int minRange = INT_MAX;
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int mapX = getX(mapIndex);
		int mapY = getY(mapIndex);
		MAP* tile = getMapTile(mapIndex);
		
		// same region for non-air units
		
		if (triad != TRIAD_AIR && tile->region != startingTile->region)
			continue;
		
		// friendly territory only
		
		if (tile->owner != -1 && tile->owner != aiFactionId && at_war(tile->owner, aiFactionId))
			continue;
		
		// monolith
		
		if (!map_has_item(tile, TERRA_MONOLITH))
			continue;
		
		int range = map_range(x, y, mapX, mapY);
		
		if (range < minRange)
		{
			nearestMonolithLocation = tile;
			minRange = range;
		}
		
	}
		
	return nearestMonolithLocation;
	
}

/*
Searches for allied base.
*/
MAP *getNearestBase(int x, int y, int triad)
{
	MAP *startingTile = getMapTile(x, y);
	bool ocean = isOceanRegion(startingTile->region);
	
	MAP *nearestBaseLocation = nullptr;
	
	// search only for realm unit can move on
	
	if (!(triad == TRIAD_AIR || triad == (ocean ? TRIAD_SEA : TRIAD_LAND)))
		return nullptr;
	
	int minRange = INT_MAX;
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// same region for non-air units
		
		if (triad != TRIAD_AIR && baseTile->region != startingTile->region)
			continue;
		
		// friendly base only
		
		if (!(base->faction_id == aiFactionId || has_pact(base->faction_id, aiFactionId)))
			continue;
		
		int range = map_range(x, y, base->x, base->y);
		
		if (range < minRange)
		{
			nearestBaseLocation = baseTile;
			minRange = range;
		}
		
	}
		
	return nearestBaseLocation;
	
}

/*
Checks if vehicle can be used in war operations.
*/
bool isInWarZone(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	debug("isInWarZone (%3d,%3d) %s - %s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name, MFactions[vehicle->faction_id].noun_faction);
	
	for (int otherBaseId = 0; otherBaseId < *total_num_bases; otherBaseId++)
	{
		BASE *otherBase = &(Bases[otherBaseId]);
		MAP *otherBaseTile = getBaseMapTile(otherBaseId);
		
		if (otherBase->faction_id == vehicle->faction_id)
			continue;
		
		if (at_war(vehicle->faction_id, otherBase->faction_id))
		{
			if
			(
				// air vehicle can attack any base
				triad == TRIAD_AIR
				||
				// land vehicle can attack land base within same region
				(triad = TRIAD_LAND && isLandRegion(otherBaseTile->region) && vehicleTile->region == otherBaseTile->region)
				||
				// sea vehicle can attack ocean base within same region
				(triad = TRIAD_SEA && isOceanRegion(otherBaseTile->region) && isInOceanRegion(vehicleId, otherBaseTile->region))
			)
			{
				return true;
			}
			
		}
		
	}
	
	for (int otherVehicleId = 0; otherVehicleId < *total_num_vehicles; otherVehicleId++)
	{
		VEH *otherVehicle = &(Vehicles[otherVehicleId]);
		int otherVehicleTriad = otherVehicle->triad();
		MAP *otherVehicleTile = getVehicleMapTile(otherVehicleId);
		
		if (otherVehicle->faction_id == 0)
			continue;
		
		if (otherVehicle->faction_id == vehicle->faction_id)
			continue;
		
		if (!at_war(vehicle->faction_id, otherVehicle->faction_id))
			continue;
		
		if (!isCombatVehicle(otherVehicleId))
			continue;
		
		debug("\t(%3d,%3d) %s - %s\n", otherVehicle->x, otherVehicle->y, Units[otherVehicle->unit_id].name, MFactions[otherVehicle->faction_id].noun_faction);
		
		if
		(
			// air vehicle can attack everywhere
			triad == TRIAD_AIR
			||
			// land vehicle can attack on the same land region
			(triad = TRIAD_LAND && isLandRegion(otherVehicleTile->region) && vehicleTile->region == otherVehicleTile->region)
			||
			// sea vehicle can attacl on the same ocean region
			(triad = TRIAD_SEA && isOceanRegion(otherVehicleTile->region) && isInOceanRegion(vehicleId, otherVehicleTile->region))
		)
		{
			return true;
		}
		
		if
		(
			// air vehicle can attack everywhere
			otherVehicleTriad == TRIAD_AIR
			||
			// land vehicle can attack on the same land region
			(otherVehicleTriad = TRIAD_LAND && isLandRegion(vehicleTile->region) && otherVehicleTile->region == vehicleTile->region)
			||
			// sea vehicle can attacl on the same ocean region
			(otherVehicleTriad = TRIAD_SEA && isOceanRegion(vehicleTile->region) && isInOceanRegion(otherVehicleId, vehicleTile->region))
		)
		{
			return true;
		}
		
	}
	
	return false;
	
}

void countTransportableVehicles()
{
	debug("countTransportableVehicles - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// land triad
		
		if (vehicle->triad() != TRIAD_LAND)
			continue;
		
		// at ocean base without orders and not being transported
		
		if
		(
			isOceanRegion(vehicleTile->region)
			&&
			map_has_item(vehicleTile, TERRA_BASE_IN_TILE)
			&&
			!isLandVehicleOnSeaTransport(vehicleId)
			&&
			isVehicleAvailable(vehicleId)
		)
		{
			if (aiData.seaTransportRequests.count(vehicleTile->region) == 0)
			{
				aiData.seaTransportRequests[vehicleTile->region] = 0;
			}
			
			aiData.seaTransportRequests[vehicleTile->region]++;
			
			debug("\t(%3d,%3d) %s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
			
		}
		
	}
	
}

/*
Collects all enemies reachable in time.
Currently works for sea units only.
*/
std::vector<int> getReachableEnemies(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	std::vector<int> reachableEnemies;
	
	if (vehicle->triad() != TRIAD_SEA)
		return reachableEnemies;
	
	// process steps
	
	int remainedMovementPoints = mod_veh_speed(vehicleId) - vehicle->road_moves_spent;
	
	std::unordered_map<MAP *, int> travelCosts;
	travelCosts.insert({vehicleTile, 0});
	
	std::set<MAP *> iterableLocations;
	iterableLocations.insert(vehicleTile);
	
	for (int movementPoints = 1; movementPoints <= remainedMovementPoints; movementPoints++)
	{
		// iterate iterable locations
		
		std::unordered_set<MAP *> newIterableLocations;
		
		for
		(
			std::set<MAP *>::iterator iterableLocationsIterator = iterableLocations.begin();
			iterableLocationsIterator != iterableLocations.end();
		)
		{
			MAP *tile = *iterableLocationsIterator;
			int x = getX(tile);
			int y = getY(tile);
			int travelCost = travelCosts[tile];
			
			// check zoc
			
			int zoc = isZoc(vehicle->faction_id, x, y);
			
			// iterate adjacent tiles
			
			bool keepIterable = false;
			
			for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
			{
				int adjacentTileX = getX(adjacentTile);
				int adjacentTileY = getY(adjacentTile);
				
				// ignore already processed locations
				
				if (travelCosts.find(adjacentTile) != travelCosts.end())
					continue;
				
				// add to enemy vehicles if there are any
				
				int enemyVehicleId = veh_at(adjacentTileX, adjacentTileY);
				
				if (enemyVehicleId != -1)
				{
					VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);
					
					if (at_war(vehicle->faction_id, enemyVehicle->faction_id))
					{
						reachableEnemies.push_back(enemyVehicleId);
					}
					
				}
				
				// ignore non friendly base
				
				if (map_has_item(adjacentTile, TERRA_BASE_IN_TILE) && adjacentTile->owner != -1 && adjacentTile->owner != vehicle->faction_id && !has_pact(vehicle->faction_id, adjacentTile->owner))
					continue;
				
				// ignore non friendly vehicle
				
				int adjacentTileOwner = veh_who(adjacentTileX, adjacentTileY);
				
				if (adjacentTileOwner != -1 && adjacentTileOwner != vehicle->faction_id && !has_pact(vehicle->faction_id, adjacentTileOwner))
					continue;
				
				// ignore zoc
				
				if (zoc && isZoc(vehicle->faction_id, adjacentTileX, adjacentTileY))
					continue;
				
				// get movement cost
				
				int hexCost = mod_hex_cost(vehicle->unit_id, vehicle->faction_id, x, y, adjacentTileX, adjacentTileY, 0);
				int adjacentTileTravelCost = travelCost + hexCost;
				
				if (adjacentTileTravelCost > movementPoints)
				{
					// will be iterated in future cycles
					
					keepIterable = true;
					
				}
				else if (adjacentTileTravelCost == movementPoints)
				{
					// insert new location
					
					travelCosts.insert({adjacentTile, movementPoints});
					newIterableLocations.insert(adjacentTile);
					
				}
				
			}
			
			if (keepIterable)
			{
				// keep this location and advance to next iterable location
				
				iterableLocationsIterator++;
				
			}
			else
			{
				// erase this location and advance to next iterable location
				
				iterableLocationsIterator = iterableLocations.erase(iterableLocationsIterator);
				
			}
			
		}
		
		// update iterable locations
		
		iterableLocations.insert(newIterableLocations.begin(), newIterableLocations.end());
		
	}
	
	return reachableEnemies;
	
}

/*
Checks if unit has not task/order.
*/
bool isVehicleAvailable(int vehicleId)
{
	return (!hasTask(vehicleId) && combatOrders.count(vehicleId) == 0);
}

