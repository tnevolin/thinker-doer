#include "aiProduction.h"
#include "aiCombat.h"
#include "ai.h"
#include "engine.h"

std::map<int, double> unitTypeProductionDemands;
std::set<int> bestProductionBaseIds;
std::map<int, std::map<int, double>> unitDemands;
std::map<int, PRODUCTION_DEMAND> productionDemands;
std::map<int, PRODUCTION_CHOICE> productionChoices;
double averageFacilityMoraleBoost;
double territoryBonusMultiplier;

/*
Prepares production orders.
*/
void aiProductionStrategy()
{
	// populate lists

	populateProducitonLists();

	// evaluate exploration demand

	evaluateExplorationDemand();

	// evaluate native protection demand

	evaluateNativeProtectionDemand();

	// evaluate expansion demand

	evaluateExpansionDemand();

	// evaluate land improvement demand

	evaluateLandImprovementDemand();

//	// evaluate conventional defense demand
//
//	evaluateFactionProtectionDemand();
//
	// sort production demands

	setProductionChoices();

}

void populateProducitonLists()
{
	unitTypeProductionDemands.clear();
	bestProductionBaseIds.clear();
	unitDemands.clear();
	productionDemands.clear();
	productionChoices.clear();

	// calculate average base production

	double totalBaseProduction = 0.0;

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		totalBaseProduction += (double)base->mineral_surplus_final;

	}

	double averageBaseProduction = totalBaseProduction / (double)baseIds.size();

	// populate best production bases

	double sumMoraleFacilityBoost = 0.0;

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		if (base->mineral_surplus_final >= averageBaseProduction)
		{
			// add best production base

			bestProductionBaseIds.insert(id);

			// summarize morale facility boost

			if (has_facility(id, FAC_COMMAND_CENTER) || has_facility(id, FAC_NAVAL_YARD) || has_facility(id, FAC_AEROSPACE_COMPLEX))
			{
				sumMoraleFacilityBoost += 2;
			}

			if (has_facility(id, FAC_BIOENHANCEMENT_CENTER))
			{
				sumMoraleFacilityBoost += 2;
			}

		}

	}

	averageFacilityMoraleBoost = sumMoraleFacilityBoost / bestProductionBaseIds.size();

	// calculate territory bonus multiplier

	territoryBonusMultiplier = (1.0 + (double)conf.combat_bonus_territory / 100.0);

}

/*
Evalueates need for exploration.
*/
void evaluateExplorationDemand()
{
	debug("evaluateExplorationDemand %-25s\n", tx_metafactions[*active_faction].noun_faction);

	for
	(
		std::map<int, std::vector<int>>::iterator regionBaseGroupsIterator = regionBaseGroups.begin();
		regionBaseGroupsIterator != regionBaseGroups.end();
		regionBaseGroupsIterator++
	)
	{
		int region = regionBaseGroupsIterator->first;
		std::vector<int> *regionBaseIds = &(regionBaseGroupsIterator->second);
		int triad = (isOceanRegion(region) ? TRIAD_SEA : TRIAD_LAND);

		debug("\tregion=%d\n", region);

		// find scout unit

		int scoutUnitId = findScoutUnit(triad);

		if (scoutUnitId == -1)
			continue;

		debug("\t\tscoutUnit=%-25s\n", tx_units[scoutUnitId].name);

		// calculate number of unexplored tiles

		int unexploredTileCount = 0;

		for (int x = 0; x < *map_axis_x; x++)
		{
			for (int y = 0; y < *map_axis_y; y++)
			{
				MAP *tile = getMapTile(x, y);

				if (tile == NULL)
					continue;

				// this region only

				if (tile->region != region)
					continue;

				// unexplored only

				if ((tile->visibility & (0x1 << *active_faction)) != 0)
					continue;

				// search for nearby explored tiles not on unfriendly territory

				bool reachable = false;

				for (MAP *adjacentTile : getAdjacentTiles(x, y, false))
				{
					// same region only

					if (adjacentTile->region != region)
						continue;

					// explored only

					if ((adjacentTile->visibility & (0x1 << *active_faction)) == 0)
						continue;

					// neutral, own, or pact

					if (adjacentTile->owner == -1 || adjacentTile->owner == *active_faction || tx_factions[*active_faction].diplo_status[adjacentTile->owner] == DIPLO_PACT)
					{
						reachable = true;
						break;
					}

				}

				if (reachable)
				{
					unexploredTileCount++;
				}

			}

		}

		debug("\t\tunexploredTileCount=%d\n", unexploredTileCount);

		// count existing units outside

		int existingExplorersCombinedChassisSpeed = 0;

		for (int id : combatVehicleIds)
		{
			VEH *vehicle = &(tx_vehicles[id]);
			MAP *tile = getMapTile(vehicle->x, vehicle->y);

			// exclude units in bases

			if (tile->items & TERRA_BASE_IN_TILE)
				continue;

			// exclude vehilces in different regions

			if (tile->region != region)
				continue;

			// exclude non combat units

			if (!isCombatVehicle(id))
				continue;

			existingExplorersCombinedChassisSpeed += veh_chassis_speed(id);

		}

		debug("\t\texistingExplorersCombinedChassisSpeed=%d\n", existingExplorersCombinedChassisSpeed);

		// calculate need for infantry explorers

		double infantryExplorersDemand = (double)unexploredTileCount / conf.ai_production_exploration_coverage - (double)existingExplorersCombinedChassisSpeed;

		debug("\t\tinfantryExplorersTotalDemand=%f\n", (double)unexploredTileCount / conf.ai_production_exploration_coverage);
		debug("\t\tinfantryExplorersRemainingDemand=%f\n", infantryExplorersDemand);

		// we have enough

		if (infantryExplorersDemand <= 0)
			continue;

		// otherwise, set priority

		double priority = ((double)unexploredTileCount / conf.ai_production_exploration_coverage - (double)existingExplorersCombinedChassisSpeed) / ((double)unexploredTileCount / conf.ai_production_exploration_coverage);

		debug("\t\tpriority=%f\n", priority);

		// reduce priority for higher explorer speed

		priority /= unit_chassis_speed(scoutUnitId);

		debug("\t\tpriority=%f (speed adjusted)\n", priority);

		// find max mineral surplus in region

		int maxMineralSurplus = 0;

		for (int id : *regionBaseIds)
		{
			BASE *base = &(tx_bases[id]);

			maxMineralSurplus = max(maxMineralSurplus, base->mineral_surplus);

		}

		debug("\t\tmaxMineralSurplus=%d\n", maxMineralSurplus);

		// set demand based on mineral surplus

		for (int id : *regionBaseIds)
		{
			BASE *base = &(tx_bases[id]);

			double baseProductionAdjustedPriority = priority * (double)base->mineral_surplus / (double)maxMineralSurplus;

			debug("\t\t\t%-25s base->mineral_surplus=%d, baseProductionAdjustedPriority=%f\n", base->name, base->mineral_surplus, baseProductionAdjustedPriority);

			addProductionDemand(id, false, scoutUnitId, baseProductionAdjustedPriority);

		}

	}

	debug("\n");

}

/*
Evaluates need for bases protection against natives.
*/
void evaluateNativeProtectionDemand()
{
	debug("evaluateNativeProtectionDemand %-25s\n", tx_metafactions[*active_faction].noun_faction);

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);
		BASE_STRATEGY *baseStrategy = &(baseStrategies[id]);

		debug("\t%-25s\n", base->name);

		// calculate current native protection

		double totalNativeProtection = 0.0;

		for (int vehicleId : baseStrategy->garrison)
		{
			totalNativeProtection += estimateVehicleBaseLandNativeProtection(base->faction_id, vehicleId);
		}

		// add currently built purely defensive vehicle to protection

		if (base->queue_items[0] >= 0 && isCombatVehicle(base->queue_items[0]) && tx_weapon[tx_units[base->queue_items[0]].weapon_type].offense_value == 1)
		{
			totalNativeProtection += estimateUnitBaseLandNativeProtection(base->faction_id, base->queue_items[0]);
		}

		debug("\t\ttotalNativeProtection=%f\n", totalNativeProtection);

		// add protection demand

		int item = findStrongestNativeDefensePrototype(base->faction_id);

		debug("\t\t%-25s\n", tx_units[item].name);

		if (totalNativeProtection < conf.ai_production_min_native_protection)
		{
			addProductionDemand(id, true, item, 1.0);
			debug("\t\timmediate demand added\n");
		}
		else if (totalNativeProtection < conf.ai_production_max_native_protection)
		{
			double protectionPriority = conf.ai_production_native_protection_priority_multiplier * (conf.ai_production_max_native_protection - totalNativeProtection) / (conf.ai_production_max_native_protection - conf.ai_production_min_native_protection);
			addProductionDemand(id, false, item, protectionPriority);
			debug("\t\tregular demand added\n");
			debug("\t\tprotectionPriority=%f\n", protectionPriority);
		}

	}

	debug("\n");

}

/*
Calculates need for colonies.
*/
void evaluateExpansionDemand()
{
	debug("evaluateExpansionDemand %-25s\n", tx_metafactions[*active_faction].noun_faction);

	for
	(
		std::map<int, std::vector<int>>::iterator regionBaseGroupsIterator = regionBaseGroups.begin();
		regionBaseGroupsIterator != regionBaseGroups.end();
		regionBaseGroupsIterator++
	)
	{
		int region = regionBaseGroupsIterator->first;
		std::vector<int> *regionBaseIds = &(regionBaseGroupsIterator->second);
		int triad = (isOceanRegion(region) ? TRIAD_SEA : TRIAD_LAND);

		debug("\tregion=%d\n", region);

		// reset range to unpopulated tile statistics

		for (int id : *regionBaseIds)
		{
			BASE_STRATEGY *baseStrategy = &(baseStrategies[id]);

			baseStrategy->unpopulatedTileCount = 0;
			baseStrategy->unpopulatedTileRangeSum = 0;

		}

		// calculate unpopulated tiles per region

		int unpopulatedTileCount = 0;

		for (int x = 0; x < *map_axis_x; x++)
		{
			for (int y = 0; y < *map_axis_y; y++)
			{
				MAP *tile = getMapTile(x, y);

				if (tile == NULL)
					continue;

				// this region only

				if (tile->region != region)
					continue;

				// exclude territory claimed by someone else

				if (!(tile->owner == -1 || tile->owner == *active_faction))
					continue;

				// exclude base and base radius

				if (tile->items & (TERRA_BASE_IN_TILE | TERRA_BASE_RADIUS))
					continue;

				// only tiles not on a map border

				if ((*map_toggle_flat && !(x >= 0 && x < *map_axis_x)) || !(y >= 0 && y < *map_axis_y))
					continue;

				// only tiles at least one step away from base borders and enemy territory

				bool good = true;

				for (MAP *adjacentTile : getAdjacentTiles(x, y, false))
				{
					// only claimable territory

					if (!(adjacentTile->owner == -1 || adjacentTile->owner == *active_faction))
					{
						good = false;
						break;
					}

					// only territory not within base borders

					if (adjacentTile->items & TERRA_BASE_RADIUS)
					{
						good = false;
						break;
					}

				}

				if (!good)
					continue;

				// calculate distance to nearest own base within region

				BASE *nearestBase = NULL;
				int nearestBaseRange = 0;

				for (int id : *regionBaseIds)
				{
					BASE *base = &(tx_bases[id]);

					// calculate range to tile

					int range = map_range(x, y, base->x, base->y);

					if (nearestBase == NULL || range < nearestBaseRange)
					{
						nearestBase = base;
						nearestBaseRange = range;
					}

				}

				// exclude too far or uncertain tiles

				if (nearestBase == NULL || nearestBaseRange > conf.ai_production_max_unpopulated_range)
					continue;

				// count available tiles

				unpopulatedTileCount++;

				// collect range to unpopulated tile statistics

				for (int id : *regionBaseIds)
				{
					BASE *base = &(tx_bases[id]);
					BASE_STRATEGY *baseStrategy = &(baseStrategies[id]);

					int range = map_range(x, y, base->x, base->y);

					baseStrategy->unpopulatedTileCount++;
					baseStrategy->unpopulatedTileRangeSum += range;

				}

			}

		}

		debug("\t\tunpopulatedTileCount=%d\n", unpopulatedTileCount);

		// check there is at least one colony unit for given triad

		int colonyUnitId = findColonyUnit(triad);

		if (colonyUnitId == -1)
			continue;

		// evaluate need for expansion

		if (unpopulatedTileCount < conf.ai_production_min_unpopulated_tiles)
			continue;

		// calcualte max base size in region and max average range to destination

		int maxBaseSize = 1;
		double minAverageUnpopulatedTileRange = 9999.0;

		for (int id : *regionBaseIds)
		{
			BASE *base = &(tx_bases[id]);
			BASE_STRATEGY *baseStrategy = &(baseStrategies[id]);

			maxBaseSize = max(maxBaseSize, (int)base->pop_size);

			baseStrategy->averageUnpopulatedTileRange = (double)baseStrategy->unpopulatedTileRangeSum / (double)baseStrategy->unpopulatedTileCount;
			minAverageUnpopulatedTileRange = min(minAverageUnpopulatedTileRange, baseStrategy->averageUnpopulatedTileRange);

		}

		debug("\t\tmaxBaseSize=%d\n", maxBaseSize);
		debug("\t\tminAverageUnpopulatedTileRange=%f\n", minAverageUnpopulatedTileRange);

		// set demands

		for (int id : *regionBaseIds)
		{
			BASE *base = &(tx_bases[id]);
			BASE_STRATEGY *baseStrategy = &(baseStrategies[id]);

			debug("\t\t\t%-25s\n", base->name);

			// calculate infantry turns to destination assuming land units travel by roads 2/3 of time

			double infantryTurnsToDestination = (triad == TRIAD_LAND ? 0.5 : 1.0) * baseStrategy->averageUnpopulatedTileRange;

			// find optimal colony unit

			int optimalColonyUnitId = findOptimalColonyUnit(triad, base->mineral_surplus, infantryTurnsToDestination);

			debug("\t\t\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, infantryTurnsToDestination=%f, optimalColonyUnitId=%-25s\n", triad, base->mineral_surplus, infantryTurnsToDestination, tx_units[optimalColonyUnitId].name);

			// verify base has enough growth potential to build a colony

			if (!canBaseProduceColony(id, optimalColonyUnitId))
			{
				debug("\t\t\t\tbase is too small to produce a colony\n");
				continue;
			}

			// adjust priority based on base size and range to destination

			double priority = conf.ai_production_colony_priority * ((double)base->pop_size / (double)maxBaseSize) * (minAverageUnpopulatedTileRange / baseStrategy->averageUnpopulatedTileRange);

			debug("\t\t\t\tpop_size=%d, averageUnpopulatedTileRange=%f, priority=%f\n", base->pop_size, baseStrategy->averageUnpopulatedTileRange, priority);

			// set base demand

			addProductionDemand(id, false, optimalColonyUnitId, priority);

		}

	}

	debug("\n");

}

/*
Evaluates demand for land improvement.
*/
void evaluateLandImprovementDemand()
{

}

/*
Calculate demand for protection against other factions.
*/
void evaluateFactionProtectionDemand()
{
	debug("%-24s evaluateFactionProtectionDemand\n", tx_metafactions[aiFactionId].noun_faction);

	// hostile conventional offense

	debug("\tconventional offense\n");

	double hostileConventionalToConventionalOffenseStrength = 0.0;
	double hostileConventionalToPsiOffenseStrength = 0.0;

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(tx_vehicles[id]);
		MAP *location = getMapTile(vehicle->x, vehicle->y);
		int triad = veh_triad(id);

		// exclude own vehicles

		if (vehicle->faction_id == aiFactionId)
			continue;

		// exclude natives

		if (vehicle->faction_id == 0)
			continue;

		// exclude non combat

		if (!isCombatVehicle(id))
			continue;

		// find vehicle weapon offense and defense values

		int vehicleOffenseValue = getVehicleOffenseValue(id);
		int vehicleDefenseValue = getVehicleDefenseValue(id);

		// only conventional offense

		if (vehicleOffenseValue == -1)
			continue;

		// exclude conventional vehicle with too low attack/defense ratio

		if (vehicleOffenseValue != -1 && vehicleDefenseValue != -1 && vehicleOffenseValue < vehicleDefenseValue / 2)
			continue;

		// get morale modifier for attack

		double moraleModifierAttack = getMoraleModifierAttack(id);

		// get PLANET bonus on attack

		double planetModifierAttack = getSEPlanetModifierAttack(vehicle->faction_id);

		// get vehicle region

		int region = (triad == TRIAD_AIR ? -1 : location->region);

		// find nearest own base

		int nearestOwnBaseId = findNearestOwnBaseId(vehicle->x, vehicle->y, region);

		// base not found

		if (nearestOwnBaseId == -1)
			continue;

		// find range to nearest own base

		BASE *nearestOwnBase = &(tx_bases[nearestOwnBaseId]);
		int nearestOwnBaseRange = map_range(vehicle->x, vehicle->y, nearestOwnBase->x, nearestOwnBase->y);

		// calculate vehicle speed
		// for land units assuming there are roads everywhere

		double vehicleSpeed = (triad == TRIAD_LAND ? getLandVehicleSpeedOnRoads(id) : getVehicleSpeedWithoutRoads(id));

		// calculate threat time koefficient

		double timeThreatKoefficient = max(0.0, (MAX_THREAT_TURNS - ((double)nearestOwnBaseRange / vehicleSpeed)) / MAX_THREAT_TURNS);

		// calculate conventional anti conventional defense demand

		double conventionalCombatStrength =
			(double)vehicleOffenseValue
			* factionInfos[vehicle->faction_id].offenseMultiplier
			* factionInfos[vehicle->faction_id].fanaticBonusMultiplier
			* moraleModifierAttack
			* factionInfos[vehicle->faction_id].threatKoefficient
			/ getBaseDefenseMultiplier(nearestOwnBaseId, triad, true, true)
			* timeThreatKoefficient
		;

		// calculate psi anti conventional defense demand

		double psiCombatStrength =
			getPsiCombatBaseOdds(triad)
			* factionInfos[vehicle->faction_id].offenseMultiplier
			* moraleModifierAttack
			* planetModifierAttack
			* factionInfos[vehicle->faction_id].threatKoefficient
			/ getBaseDefenseMultiplier(nearestOwnBaseId, triad, false, true)
			* timeThreatKoefficient
		;

		debug
		(
			"\t\t[%3d](%3d,%3d), %-25s\n",
			id,
			vehicle->x,
			vehicle->y,
			tx_metafactions[vehicle->faction_id].noun_faction
		);
		debug
		(
			"\t\t\tvehicleOffenseValue=%2d, offenseMultiplier=%4.1f, fanaticBonusMultiplier=%4.1f, moraleModifierAttack=%4.1f, threatKoefficient=%4.1f, nearestOwnBaseRange=%3d, baseDefenseMultiplier=%4.1f, timeThreatKoefficient=%4.2f, conventionalCombatStrength=%4.1f\n",
			vehicleOffenseValue,
			factionInfos[vehicle->faction_id].offenseMultiplier,
			factionInfos[vehicle->faction_id].fanaticBonusMultiplier,
			moraleModifierAttack,
			factionInfos[vehicle->faction_id].threatKoefficient,
			nearestOwnBaseRange,
			getBaseDefenseMultiplier(nearestOwnBaseId, triad, true, true),
			timeThreatKoefficient,
			conventionalCombatStrength
		);
		debug
		(
			"\t\t\tpsiCombatBaseOdds=%4.1f, offenseMultiplier=%4.1f, moraleModifierAttack=%4.1f, planetModifierAttack=%4.1f, threatKoefficient=%4.1f, nearestOwnBaseRange=%3d, baseDefenseMultiplier=%4.1f, timeThreatKoefficient=%4.2f, psiCombatStrength=%4.1f\n",
			getPsiCombatBaseOdds(triad),
			factionInfos[vehicle->faction_id].offenseMultiplier,
			moraleModifierAttack,
			planetModifierAttack,
			factionInfos[vehicle->faction_id].threatKoefficient,
			nearestOwnBaseRange,
			getBaseDefenseMultiplier(nearestOwnBaseId, triad, false, true),
			timeThreatKoefficient,
			psiCombatStrength
		);

		// update total

		hostileConventionalToConventionalOffenseStrength += conventionalCombatStrength;
		hostileConventionalToPsiOffenseStrength += psiCombatStrength;

	}

	debug
	(
		"\thostileConventionalToConventionalOffenseStrength=%.0f, hostileConventionalToPsiOffenseStrength=%.0f\n",
		hostileConventionalToConventionalOffenseStrength,
		hostileConventionalToPsiOffenseStrength
	);

	// own defense strength

	debug("\tanti conventional defense\n");

	double ownConventionalToConventionalDefenseStrength = 0.0;
	double ownConventionalToPsiDefenseStrength = 0.0;

	for (int id : combatVehicleIds)
	{
		VEH *vehicle = &(tx_vehicles[id]);

		// find vehicle defense and offense values

		int vehicleDefenseValue = getVehicleDefenseValue(id);

		// conventional defense

		if (vehicleDefenseValue == -1)
			continue;

		// get vehicle morale multiplier

		double moraleModifierDefense = getMoraleModifierDefense(id);

		// calculate vehicle defense strength

		double vehicleDefenseStrength =
			(double)vehicleDefenseValue
			* factionInfos[*active_faction].defenseMultiplier
			* moraleModifierDefense
			* territoryBonusMultiplier
		;

		debug
		(
			"\t\t[%3d](%3d,%3d) vehicleDefenseValue=%2d, defenseMultiplier=%4.1f, moraleModifierDefense=%4.1f, vehicleDefenseStrength=%4.1f\n",
			id,
			vehicle->x,
			vehicle->y,
			vehicleDefenseValue,
			factionInfos[vehicle->faction_id].defenseMultiplier,
			moraleModifierDefense,
			vehicleDefenseStrength
		);

		// update total

		ownConventionalToConventionalDefenseStrength += vehicleDefenseStrength;

	}

	for (int id : combatVehicleIds)
	{
		VEH *vehicle = &(tx_vehicles[id]);

		// find vehicle defense and offense values

		int vehicleDefenseValue = getVehicleDefenseValue(id);

		// psi defense only

		if (vehicleDefenseValue != -1)
			continue;

		// get vehicle psi combat defense strength

		double psiDefenseStrength = getVehiclePsiDefenseStrength(id);

		// calculate vehicle defense strength

		double vehicleDefenseStrength =
			psiDefenseStrength
			* factionInfos[*active_faction].defenseMultiplier
			* territoryBonusMultiplier
		;

		debug
		(
			"\t\t[%3d](%3d,%3d) psiDefenseStrength=%4.1f, defenseMultiplier=%4.1f, territoryBonusMultiplier=%4.1f, vehicleDefenseStrength=%4.1f\n",
			id,
			vehicle->x,
			vehicle->y,
			psiDefenseStrength,
			factionInfos[vehicle->faction_id].defenseMultiplier,
			territoryBonusMultiplier,
			vehicleDefenseStrength
		);

		// update total

		ownConventionalToPsiDefenseStrength += vehicleDefenseStrength;

	}

	debug
	(
		"\townConventionalToConventionalDefenseStrength=%.0f, ownConventionalToPsiDefenseStrength=%.0f\n",
		ownConventionalToConventionalDefenseStrength,
		ownConventionalToPsiDefenseStrength
	);

	// we have enough protection

	if
	(
		(ownConventionalToConventionalDefenseStrength >= hostileConventionalToConventionalOffenseStrength)
		||
		(ownConventionalToPsiDefenseStrength >= hostileConventionalToPsiOffenseStrength)
	)
	{
		return;
	}

//	// get defenders
//
//	int conventionalDefenderUnitId = findDefenderPrototype();
//	int defenderUnitCost = tx_units[defenderUnitId].cost;
//
//	int defenderUnitDefenseValue = getUnitDefenseValue(defenderUnitId);
//	double defenderUnitMoraleModifierDefense = getNewVehicleMoraleModifierDefense(*active_faction, averageFacilityMoraleBoost);
//	double defenderUnitStrength =
//		(double)defenderUnitDefenseValue
//		* factionInfos[*active_faction].defenseMultiplier
//		* defenderUnitMoraleModifierDefense
//		* territoryBonusMultiplier
//	;
//
//
//
//	// set defensive unit production demand
//
//	if (ownConventionalDefenseStrength < hostileConventionalOffenseStrength)
//	{
//		double priority = (hostileConventionalOffenseStrength - ownConventionalDefenseStrength) / ownConventionalDefenseStrength;
//		int item = findDefenderPrototype();
//
//		for (int id : bestProductionBaseIds)
//		{
//			addProductionDemand(id, true, item, priority);
//		}
//
//	}
//
	debug("\n");

}

/*
Set base production choices.
*/
void setProductionChoices()
{
	for
	(
		std::map<int, PRODUCTION_DEMAND>::iterator productionDemandsIterator = productionDemands.begin();
		productionDemandsIterator != productionDemands.end();
		productionDemandsIterator++
	)
	{
		int id = productionDemandsIterator->first;
		PRODUCTION_DEMAND *baseProductionDemands = &(productionDemandsIterator->second);

		// immediate priorities

		if (baseProductionDemands->immediate)
		{
			// create immediate production choice

			productionChoices[id] = {baseProductionDemands->immediateItem, true};

		}

		// not immediate priorities

		else if (baseProductionDemands->regular.size() > 0)
		{
			std::vector<PRODUCTION_PRIORITY> *regularBaseProductionPriorities = &(baseProductionDemands->regular);

			// summarize priorities

			double sumPriorities = 0.0;

			for
			(
				std::vector<PRODUCTION_PRIORITY>::iterator regularBaseProductionPrioritiesIterator = regularBaseProductionPriorities->begin();
				regularBaseProductionPrioritiesIterator != regularBaseProductionPriorities->end();
				regularBaseProductionPrioritiesIterator++
			)
			{
				PRODUCTION_PRIORITY *productionPriority = &(*regularBaseProductionPrioritiesIterator);

				sumPriorities += productionPriority->priority;

			}

			// normalize priorities

			if (sumPriorities > 1.0)
			for
			(
				std::vector<PRODUCTION_PRIORITY>::iterator regularBaseProductionPrioritiesIterator = regularBaseProductionPriorities->begin();
				regularBaseProductionPrioritiesIterator != regularBaseProductionPriorities->end();
				regularBaseProductionPrioritiesIterator++
			)
			{
				PRODUCTION_PRIORITY *productionPriority = &(*regularBaseProductionPrioritiesIterator);

				productionPriority->priority /= sumPriorities;

			}

			// random roll

			double randomRoll = (double)rand() / (double)(RAND_MAX + 1);

			// choose item

			for
			(
				std::vector<PRODUCTION_PRIORITY>::iterator regularBaseProductionPrioritiesIterator = regularBaseProductionPriorities->begin();
				regularBaseProductionPrioritiesIterator != regularBaseProductionPriorities->end();
				regularBaseProductionPrioritiesIterator++
			)
			{
				PRODUCTION_PRIORITY *productionPriority = &(*regularBaseProductionPrioritiesIterator);

				if (randomRoll < productionPriority->priority)
				{
					// create non-immediate production choice

					productionChoices[id] = {productionPriority->item, false};

					break;

				}
				else
				{
					randomRoll -= productionPriority->priority;
				}

			}

		}

	}

	if (DEBUG)
	{
		debug("productionDemands:\n");

		for
		(
			std::map<int, PRODUCTION_DEMAND>::iterator productionDemandsIterator = productionDemands.begin();
			productionDemandsIterator != productionDemands.end();
			productionDemandsIterator++
		)
		{
			int id = productionDemandsIterator->first;
			PRODUCTION_DEMAND *baseProductionDemands = &(productionDemandsIterator->second);

			debug("\t%-25s\n", tx_bases[id].name);

			debug("\t\timmediate\n");

			if (baseProductionDemands->immediate)
			{
				debug("\t\t\t%-25s %4.2f\n", prod_name(baseProductionDemands->immediateItem), baseProductionDemands->immediatePriority);

			}

			debug("\t\tregular\n");

			for
			(
				std::vector<PRODUCTION_PRIORITY>::iterator immediateBaseProductionPrioritiesIterator = baseProductionDemands->regular.begin();
				immediateBaseProductionPrioritiesIterator != baseProductionDemands->regular.end();
				immediateBaseProductionPrioritiesIterator++
			)
			{
				PRODUCTION_PRIORITY *productionPriority = &(*immediateBaseProductionPrioritiesIterator);

				debug("\t\t\t%-25s %4.2f\n", prod_name(productionPriority->item), productionPriority->priority);

			}

			debug("\t\tselected\n");

			if (productionChoices.find(id) != productionChoices.end())
			{
				debug("\t\t\t%-25s %s\n", prod_name(productionChoices[id].item), (productionChoices[id].immediate ? "immediate" : "regular"));
			}

		}

		debug("\n");

	}

}

int findNearestOwnBaseId(int x, int y, int region)
{
	int nearestOwnBaseId = -1;
	int nearestOwnBaseRange = 9999;

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);
		MAP *location = getMapTile(base->x, base->y);

		// skip bases in other region

		if (region != -1 && location->region != region)
			continue;

		// calculate range

		int range = map_range(base->x, base->y, x, y);

		// update closest base

		if (nearestOwnBaseId == -1 || range < nearestOwnBaseRange)
		{
			nearestOwnBaseId = id;
			nearestOwnBaseRange = range;
		}

	}

	return nearestOwnBaseId;

}

/*
Selects base production.
*/
int suggestBaseProduction(int id, bool productionDone, int choice)
{
	BASE *base = &(tx_bases[id]);

	debug("suggestBaseProduction: %-25s, productionDone=%s, (%s)\n", base->name, (productionDone ? "y" : "n"), prod_name(base->queue_items[0]));

	std::map<int, PRODUCTION_CHOICE>::iterator productionChoicesIterator = productionChoices.find(id);

	if (productionChoicesIterator != productionChoices.end())
	{
		PRODUCTION_CHOICE *productionChoice = &(productionChoicesIterator->second);

		// determine if production change is allowed

		if (productionChoice->immediate || productionDone)
		{
			debug("\t%-25s\n", prod_name(productionChoice->item));

			choice = productionChoice->item;

		}

	}

	debug("\n");

	return choice;

}

void addProductionDemand(int id, bool immediate, int item, double priority)
{
	BASE *base = &(tx_bases[id]);

	// create entry if not created yet

	if (productionDemands.find(id) == productionDemands.end())
	{
		productionDemands[id] = PRODUCTION_DEMAND();
	}

	PRODUCTION_DEMAND *productionDemand = &(productionDemands[id]);

	if (immediate)
	{
		if (!productionDemand->immediate)
		{
			productionDemand->immediate = true;
			productionDemand->immediateItem = item;
			productionDemand->immediatePriority = priority;
		}
		else if (priority > productionDemand->immediatePriority)
		{
			productionDemand->immediateItem = item;
			productionDemand->immediatePriority = priority;
		}
	}
	else
	{
		// do not build combat unit in weak bases

		if (item >= 0 && isCombatUnit(item))
		{
			if (base->mineral_surplus < conf.ai_production_combat_unit_min_mineral_surplus)
				return;

		}

		// lookup for existing item and add to it

		bool found = false;

		for
		(
			std::vector<PRODUCTION_PRIORITY>::iterator productionPrioritiesIterator = productionDemand->regular.begin();
			productionPrioritiesIterator != productionDemand->regular.end();
			productionPrioritiesIterator++
		)
		{
			PRODUCTION_PRIORITY *productionPriority = &(*productionPrioritiesIterator);

			if (productionPriority->item == item)
			{
				productionPriority->priority += priority;
				found = true;
				break;
			}

		}

		if (!found)
		{
			productionDemand->regular.push_back({item, priority});
		}

	}

}

int findStrongestNativeDefensePrototype(int factionId)
{
	// initial prototype

    int bestUnitId = BSC_SCOUT_PATROL;
    UNIT *bestUnit = &(tx_units[bestUnitId]);

    for (int i = 0; i < 128; i++)
	{
        int id = (i < 64 ? i : (factionId - 1) * 64 + i);

        UNIT *unit = &tx_units[id];

		// skip current

		if (id == bestUnitId)
			continue;

        // skip empty

        if (strlen(unit->name) == 0)
			continue;

		// skip not enabled

		if (id < 64 && !has_tech(factionId, unit->preq_tech))
			continue;

		// skip non land

		if (unit_triad(id) != TRIAD_LAND)
			continue;

		// prefer trance or police unit

		if
		(
			((unit->ability_flags & ABL_TRANCE) && (~bestUnit->ability_flags & ABL_TRANCE))
			||
			((unit->ability_flags & ABL_POLICE_2X) && (~bestUnit->ability_flags & ABL_POLICE_2X))
		)
		{
			bestUnitId = id;
			bestUnit = unit;
		}

	}

    return bestUnitId;

}

int findDefenderPrototype()
{
	int bestUnitId = BSC_SCOUT_PATROL;
	double bestUnitDefenseEffectiveness = evaluateUnitDefenseEffectiveness(bestUnitId);

	for (int id : prototypes)
	{
		// skip non combat units

		if (!isCombatUnit(id))
			continue;

		// skip air units

		if (unit_triad(id) == TRIAD_AIR)
			continue;

		double unitDefenseEffectiveness = evaluateUnitDefenseEffectiveness(id);

		if (bestUnitId == -1 || unitDefenseEffectiveness > bestUnitDefenseEffectiveness)
		{
			bestUnitId = id;
			bestUnitDefenseEffectiveness = unitDefenseEffectiveness;
		}

	}

	return bestUnitId;

}

int findFastAttackerUnit()
{
	int bestUnitId = BSC_SCOUT_PATROL;
	double bestUnitOffenseEffectiveness = evaluateUnitOffenseEffectiveness(bestUnitId);

	for (int id : prototypes)
	{
		UNIT *unit = &(tx_units[id]);

		// skip non combat units

		if (!isCombatUnit(id))
			continue;

		// skip infantry units

		if (unit->chassis_type == CHS_INFANTRY)
			continue;

		double unitOffenseEffectiveness = evaluateUnitOffenseEffectiveness(id);

		if (bestUnitId == -1 || unitOffenseEffectiveness > bestUnitOffenseEffectiveness)
		{
			bestUnitId = id;
			bestUnitOffenseEffectiveness = unitOffenseEffectiveness;
		}

	}

	return bestUnitId;

}

int findScoutUnit(int triad)
{
	int bestUnitId = -1;
	int bestUnitChassisSpeed = -1;
	int bestUnitCost = -1;

	for (int id : prototypes)
	{
		UNIT *unit = &(tx_units[id]);

		// skip non combat units

		if (!isCombatUnit(id))
			continue;

		// given triad only

		if (unit_triad(id) != triad)
			continue;

		// find fastest cheapest unit

		if
		(
			bestUnitId == -1
			||
			unit_chassis_speed(id) > bestUnitChassisSpeed
			||
			unit->cost < bestUnitCost
		)
		{
			bestUnitId = id;
			bestUnitChassisSpeed = unit_chassis_speed(id);
			bestUnitCost = unit->cost;
		}

	}

	return bestUnitId;

}

int findColonyUnit(int triad)
{
	int bestUnitId = -1;

	for (int id : prototypes)
	{
		// given triad only

		if (unit_triad(id) != triad)
			continue;

		// colony only

		if (!isUnitColony(id))
			continue;

		bestUnitId = id;
		break;

	}

	return bestUnitId;

}

int findOptimalColonyUnit(int triad, int mineralSurplus, double infantryTurnsToDestination)
{
	int bestUnitId = -1;
	double bestTotalTurns = 0.0;

	for (int id : prototypes)
	{
		UNIT *unit = &(tx_units[id]);

		// given triad only

		if (unit_triad(id) != triad)
			continue;

		// colony only

		if (!isUnitColony(id))
			continue;

		// calculate total turns

		double totalTurns = (double)(tx_cost_factor(*active_faction, 1, -1) * unit->cost) / (double)mineralSurplus + infantryTurnsToDestination / (double)unit_chassis_speed(id);

		// update best unit

		if (bestUnitId == -1 || totalTurns < bestTotalTurns)
		{
			bestUnitId = id;
			bestTotalTurns = totalTurns;
		}

	}

	return bestUnitId;

}

/*
Checks if base has enough population to issue a colony by the time it is built.
*/
bool canBaseProduceColony(int baseId, int unitId)
{
	BASE *base = &(tx_bases[baseId]);
	UNIT *unit = &(tx_units[unitId]);

	// verify the unit is indeed a colony

	if (!isUnitColony(unitId))
		return false;

	// do not produce colony in unproductive bases

	if (base->mineral_surplus <= 0)
		return false;

	// calculate production time

	int productionTurns = (tx_cost_factor(base->faction_id, 1, -1) * unit->cost) / max(1, base->mineral_surplus);

	// calculate base population at the time

	double projectedPopulation = (double)base->pop_size + (double)(base->nutrients_accumulated + base->nutrient_surplus * productionTurns) / (double)(tx_cost_factor(base->faction_id, 0, baseId) * (base->pop_size + 1));

	// verify projected population is at least 2.5 or 3.5 if colony is already in production

	return (projectedPopulation >= (isUnitColony(base->queue_items[0]) ? 3.5 : 2.5));

}

