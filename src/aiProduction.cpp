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
std::unordered_set<int> evaluatedFacilities;

/*
Prepares production orders.
*/
void aiProductionStrategy()
{
	// populate lists

	populateProducitonLists();

	// evaluate facilities demand

	evaluateFacilitiesDemand();

	// evaluate land improvement demand

	evaluateLandImprovementDemand();

	// evaluate expansion demand

	evaluateExpansionDemand();

	// evaluate exploration demand

	evaluateExplorationDemand();

	// evaluate native protection demand

	evaluateNativeProtectionDemand();

//	// evaluate conventional defense demand
//
//	evaluateFactionProtectionDemand();
//
	// set production choices

	setProductionChoices();

}

void populateProducitonLists()
{
	unitTypeProductionDemands.clear();
	bestProductionBaseIds.clear();
	unitDemands.clear();
	productionDemands.clear();
	productionChoices.clear();
	evaluatedFacilities.clear();

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

void evaluateFacilitiesDemand()
{
	debug("evaluateFacilitiesDemand %s\n", tx_metafactions[*active_faction].noun_faction);

	for (int baseId : baseIds)
	{
		BASE *base = &(tx_bases[baseId]);

		debug("\t%s\n", base->name);

		// no production power

		if (base->mineral_surplus <= 0)
			continue;

		evaluateBasePopulationLimitFacilitiesDemand(baseId);
		evaluateBasePsychFacilitiesDemand(baseId);
		evaluateBaseMultiplyingFacilitiesDemand(baseId);

	}

	if (DEBUG)
	{
		debug("evaluatedFacilities\n");

		for (int evaluatedFacilityId : evaluatedFacilities)
		{
			debug("\t%s\n", tx_facility[evaluatedFacilityId].name);
		}

		debug("\n");

	}

}

void evaluateBasePopulationLimitFacilitiesDemand(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	debug("\t\tevaluateBasePopulationLimitFacilitiesDemand\n");

	const int POPULATION_LIMIT_FACILITIES[] =
	{
		FAC_HAB_COMPLEX,
		FAC_HABITATION_DOME,
	};

	for (const int facilityId : POPULATION_LIMIT_FACILITIES)
	{
		debug("\t\t\t%s\n", tx_facility[facilityId].name);

		// store evaluated facility

		evaluatedFacilities.insert(facilityId);

		// only available

		if (!isBaseFacilityAvailable(baseId, facilityId))
			continue;

		// check need

		int populationLimit = getBasePopulationLimit(baseId, facilityId);
		int projectedPopulationIncrease = estimateBaseProjectedPopulationIncrease(baseId, conf.ai_production_population_projection_turns);
		int projectedPopulation = base->pop_size + projectedPopulationIncrease;

		// no increase in population

		if (projectedPopulation <= populationLimit)
			continue;

		// add demand

		addProductionDemand(baseId, -facilityId, true, 1.0);

		// only one facility at a time

		break;

	}

}

void evaluateBasePsychFacilitiesDemand(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	debug("\t\tevaluateBasePsychFacilitiesDemand\n");

	// calculate variables

	int doctors = getDoctors(baseId);
	int projectedPopulationIncrease = estimateBaseProjectedPopulationIncrease(baseId, conf.ai_production_population_projection_turns);

	debug("\t\t\tdoctors=%d, talent_total=%d, drone_total=%d, projectedPopulationIncrease=%d\n", doctors, base->talent_total, base->drone_total, projectedPopulationIncrease);

	const int PSYCH_FACILITIES[] =
	{
		FAC_RECREATION_COMMONS,
		FAC_HOLOGRAM_THEATRE,
		FAC_PARADISE_GARDEN,
	};

	for (const int facilityId : PSYCH_FACILITIES)
	{
		// store evaluated facility

		evaluatedFacilities.insert(facilityId);

		// only available

		if (!isBaseFacilityAvailable(baseId, facilityId))
			continue;

		// no expected riots in future

		if (!(doctors > 0 || (base->drone_total > 0 && base->drone_total + projectedPopulationIncrease > base->talent_total)))
			return;

		// add demand

		addProductionDemand(baseId, -facilityId, true, 1.0);

		// no need more than one

		break;

	}

}

void evaluateBaseMultiplyingFacilitiesDemand(int baseId)
{
	BASE *base = &(tx_bases[baseId]);
	Faction *faction = &(tx_factions[base->faction_id]);

	debug("\t\tevaluateBaseMultiplyingFacilitiesDemand\n");

	// calculate base values for multiplication

	double mineralSurplus = (double)base->mineral_surplus;
	double mineralIntake = (double)base->mineral_intake;
	double economyIntake = (double)base->energy_surplus * (double)(10 - faction->SE_alloc_psych - faction->SE_alloc_labs) / 10.0;
	double psychIntake = (double)base->energy_surplus * (double)(faction->SE_alloc_psych) / 10.0;
	double labsIntake = (double)base->energy_surplus * (double)(faction->SE_alloc_labs) / 10.0;

	// adjust values for bases building colony

	if (isBaseBuildingColony(baseId))
	{
		double reductionRatio = (double)(base->pop_size + 1 - 1) / (double)(base->pop_size + 1);

		mineralSurplus *= reductionRatio;
		mineralIntake *= reductionRatio;
		economyIntake *= reductionRatio;
		psychIntake *= reductionRatio;
		labsIntake *= reductionRatio;

	}

	debug("\t\tmineralSurplus=%f, mineralIntake=%f, economyIntake=%f, psychIntake=%f, labsIntake=%f\n", mineralSurplus, mineralIntake, economyIntake, psychIntake, labsIntake);

	// multipliers
	// bit flag: 1 = minerals, 2 = economy, 4 = psych, 8 = labs
	// for simplicity sake, hospitals are assumed to add 50% psych instead of 25% psych and -1 drone

	const int MULTIPLYING_FACILITIES[][2] =
	{
		{FAC_RECYCLING_TANKS, 1},
		{FAC_ENERGY_BANK, 2},
		{FAC_NETWORK_NODE, 8},
		{FAC_HOLOGRAM_THEATRE, 4},
		{FAC_TREE_FARM, 2+4},
		{FAC_HYBRID_FOREST, 2+4},
		{FAC_FUSION_LAB, 2+8},
		{FAC_QUANTUM_LAB, 2+8},
		{FAC_RESEARCH_HOSPITAL, 4+8},
		{FAC_NANOHOSPITAL, 4+8},
		{FAC_GENEJACK_FACTORY, 1},
		{FAC_ROBOTIC_ASSEMBLY_PLANT, 1},
		{FAC_QUANTUM_CONVERTER, 1},
		{FAC_NANOREPLICATOR, 1},
	};

	for (const int *multiplyingFacility : MULTIPLYING_FACILITIES)
	{
		int facilityId = multiplyingFacility[0];
		int mask = multiplyingFacility[1];
		R_Facility *facility = &(tx_facility[facilityId]);

		debug("\t\t\t%s\n", facility->name);

		// store evaluated facility

		evaluatedFacilities.insert(facilityId);

		// check if base can build facility

		if (!isBaseFacilityAvailable(baseId, facilityId))
			continue;

		// no production power

		if (mineralSurplus <= 0.0)
			return;

		// get facility mineral cost and maintenance cost

		int mineralCost = mineral_cost(base->faction_id, -facilityId, baseId);
		int maintenance = facility->maint;
		debug("\t\t\t\tmineralCost=%d, maintenance=%d\n", mineralCost, maintenance);

		// calculate build time

		double buildTime = (double)mineralCost / mineralSurplus;
		debug("\t\t\t\tbuildTime=%f\n", buildTime);

		// calculate bonus

		double bonus = 0.0;

		// minerals multiplication
		if (mask & 1)
		{
			bonus += mineralIntake / 2.0;
		}

		// economy multiplication
		if (mask & 2)
		{
			bonus += economyIntake / 2.0 / 2.0;
		}

		// psych multiplication
		if (mask & 4)
		{
			bonus += psychIntake / 2.0 / 2.0;
		}

		// labs multiplication
		if (mask & 8)
		{
			bonus += labsIntake / 2.0 / 2.0;
		}

		debug("\t\t\tbonus=%f\n", bonus);

		// calculate benefit

		double benefit = bonus - ((double)maintenance / 2.0);
		debug("\t\t\t\tbenefit=%f\n", benefit);

		// no benefit

		if (benefit <= 0)
			continue;

		// calculate production payoff time

		double productionPayoffTime = (double)mineralCost / benefit;
		debug("\t\t\t\tproductionPayoffTime=%f\n", productionPayoffTime);

		// calculate total payoff time

		double totalPayoffTime = buildTime + productionPayoffTime;
		debug("\t\t\t\ttotalPayoffTime=%f\n", totalPayoffTime);

		// calculate priority

		double priority;

		if (totalPayoffTime <= conf.ai_production_min_payoff_turns)
		{
			priority = 1.0;
		}
		else if (totalPayoffTime >= conf.ai_production_max_payoff_turns)
		{
			priority = 0.0;
		}
		else
		{
			priority = (conf.ai_production_max_payoff_turns - totalPayoffTime) / (conf.ai_production_max_payoff_turns - conf.ai_production_min_payoff_turns);
		}

		debug("\t\t\t\tpriority=%f\n", priority);

		// ignore low priority items

		if (priority <= 0.0)
			continue;

		// add demand

		addProductionDemand(baseId, -facilityId, false, priority);

	}

}

/*
Evaluates demand for land improvement.
*/
void evaluateLandImprovementDemand()
{
	debug("evaluateLandImprovementDemand %-25s\n", tx_metafactions[*active_faction].noun_faction);

	for
	(
		std::map<int, std::vector<int>>::iterator regionBaseGroupsIterator = regionBaseGroups.begin();
		regionBaseGroupsIterator != regionBaseGroups.end();
		regionBaseGroupsIterator++
	)
	{
		int region = regionBaseGroupsIterator->first;
		std::vector<int> *regionBaseIds = &(regionBaseGroupsIterator->second);
		bool ocean = isOceanRegion(region);
		int triad = (ocean ? TRIAD_SEA : TRIAD_LAND);

		debug("\tregion=%d\n", region);

		// find former unit

		int formerUnitId = findFormerUnit(triad);

		if (formerUnitId == -1)
			continue;

		debug("\t\tformerUnit=%-25s\n", tx_units[formerUnitId].name);

		// count not improved worked tiles

		int notImprovedWorkedTileCount = 0;

		for (int baseId : *regionBaseIds)
		{
			for (MAP *workedTile : getBaseWorkedTiles(baseId))
			{
				// do not count land rainy tiles for ocean region

				if (ocean && !is_ocean(workedTile) && map_rainfall(workedTile) == 2)
					continue;

				// count needs for all regions

				if (!(map_has_item(workedTile, TERRA_MINE | TERRA_FUNGUS | TERRA_SOLAR | TERRA_SOIL_ENR | TERRA_FOREST | TERRA_CONDENSER | TERRA_ECH_MIRROR | TERRA_THERMAL_BORE | TERRA_MONOLITH)))
					notImprovedWorkedTileCount++;

			}

		}

		debug("\t\tnotImprovedWorkedTileCount=%d\n", notImprovedWorkedTileCount);

		// count formers

		int formerCount = 0;

		for (int vehicleId : formerVehicleIds)
		{
			VEH *vehicle = &(tx_vehicles[vehicleId]);
			MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);

			// this region only

			if (vehicleLocation->region != region)
				continue;

			// matching triad only

			if (veh_triad(vehicleId) != triad)
				continue;

			formerCount++;

		}

		for (int baseId : *regionBaseIds)
		{
			// get production itemId

			int itemId = getBaseBuildingItem(baseId);

			// units only

			if (itemId < 0)
				continue;

			// formers only

			if (!isFormerUnit(itemId))
				continue;

			// matching triad only

			if (unit_triad(itemId) != triad)
				continue;

			formerCount++;

		}

		debug("\t\tformerCount=%d\n", formerCount);

		// calculate improvement demand

		double improvementDemand = (double)notImprovedWorkedTileCount / conf.ai_production_improvement_coverage - (double)formerCount;

		debug("\t\timprovementDemand=%f\n", improvementDemand);

		// we have enough

		if (improvementDemand <= 0)
			continue;

		// otherwise, set priority

		double priority = ((double)notImprovedWorkedTileCount / conf.ai_production_improvement_coverage - (double)formerCount) / ((double)notImprovedWorkedTileCount / conf.ai_production_improvement_coverage);

		debug("\t\tpriority=%f\n", priority);

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

			debug("\t\t\t%-25s mineral_surplus=%d, baseProductionAdjustedPriority=%f\n", base->name, base->mineral_surplus, baseProductionAdjustedPriority);

			addProductionDemand(id, formerUnitId, false, baseProductionAdjustedPriority);

		}

	}

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

		// check there is at least one colony unit for given triad

		int colonyUnitId = findColonyUnit(triad);

		if (colonyUnitId == -1)
			continue;

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

				// exclude map border

				if ((*map_toggle_flat && (x <= 1 || x >= *map_axis_x - 2)) || (y <= 1 || y >= *map_axis_y - 2))
					continue;

				// discovered only

				if (~tile->visibility & (0x1 << *active_faction))
					continue;

				// exclude territory claimed by other factions

				if (!(tile->owner == -1 || tile->owner == *active_faction))
					continue;

				// exclude base and base radius

				if (tile->items & (TERRA_BASE_IN_TILE | TERRA_BASE_RADIUS))
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

		// evaluate need for expansion

		double expansionDemand = (double)unpopulatedTileCount / conf.ai_production_expansion_coverage - 1.0;

		debug("\t\texpansionDemand=%f\n", expansionDemand);

		// calculate existing and building colonies

		int existingColoniesCount = 0;

		for (int vehicleId : colonyVehicleIds)
		{
			VEH *vehicle = &(tx_vehicles[vehicleId]);
			MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);

			if (vehicleLocation->region != region)
				continue;

			existingColoniesCount++;

		}

		for (int baseId : *regionBaseIds)
		{
			BASE *base = &(tx_bases[baseId]);
			int item = base->queue_items[0];

			if (item < 0)
				continue;

			if (!isColonyUnit(item))
				continue;

			if (unit_triad(item) != triad)
				continue;

			existingColoniesCount++;

		}

		debug("\t\texistingColoniesCount=%d\n", existingColoniesCount);

		// we have enough

		if (existingColoniesCount >= expansionDemand)
			continue;

		// calculate priority

		double priority = conf.ai_production_expansion_priority * (expansionDemand - (double)existingColoniesCount) / expansionDemand;

		debug("\t\tpriority=%f\n", priority);

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

			double baseAdjustedPriority = priority * (((double)base->pop_size / (double)maxBaseSize) + (minAverageUnpopulatedTileRange / baseStrategy->averageUnpopulatedTileRange)) / 2;

			debug("\t\t\t\tpop_size=%d, averageUnpopulatedTileRange=%f, baseAdjustedPriority=%f\n", base->pop_size, baseStrategy->averageUnpopulatedTileRange, baseAdjustedPriority);

			// set base demand

			addProductionDemand(id, optimalColonyUnitId, false, baseAdjustedPriority);

		}

	}

	debug("\n");

}

/*
Evaluates need for exploration.
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
		bool ocean = isOceanRegion(region);
		int triad = (ocean ? TRIAD_SEA : TRIAD_LAND);

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

				// not reachable

				if (!reachable)
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

				// all conditions are met

				unexploredTileCount++;

			}

		}

		debug("\t\tunexploredTileCount=%d\n", unexploredTileCount);

		// summarize existing explorers' speed

		int existingExplorersCombinedSpeed = 0;

		for (int id : combatVehicleIds)
		{
			VEH *vehicle = &(tx_vehicles[id]);
			MAP *tile = getMapTile(vehicle->x, vehicle->y);

			// exclude vehilces in different regions

			if (tile->region != region)
				continue;

			// exclude non combat units

			if (!isCombatVehicle(id))
				continue;

			// exclude units in bases

			if (tile->items & TERRA_BASE_IN_TILE)
				continue;

			// estimate vehicle speed

			int vehicleSpeed = veh_chassis_speed(id);

			// adjust native speed assuming there is a lot of fungus

			if (!ocean && isNativeLandVehicle(id))
			{
				vehicleSpeed *= 2;
			}

			existingExplorersCombinedSpeed += vehicleSpeed;

		}

		debug("\t\texistingExplorersCombinedSpeed=%d\n", existingExplorersCombinedSpeed);

		// calculate need for infantry explorers

		double infantryExplorersDemand = (double)unexploredTileCount / conf.ai_production_exploration_coverage - (double)existingExplorersCombinedSpeed;

		debug("\t\tinfantryExplorersTotalDemand=%f\n", (double)unexploredTileCount / conf.ai_production_exploration_coverage);
		debug("\t\tinfantryExplorersRemainingDemand=%f\n", infantryExplorersDemand);

		// we have enough

		if (infantryExplorersDemand <= 0)
			continue;

		// otherwise, set priority

		double priority = ((double)unexploredTileCount / conf.ai_production_exploration_coverage - (double)existingExplorersCombinedSpeed) / ((double)unexploredTileCount / conf.ai_production_exploration_coverage);

		debug("\t\tpriority=%f\n", priority);

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

			debug("\t\t\t%-25s mineral_surplus=%d, baseProductionAdjustedPriority=%f\n", base->name, base->mineral_surplus, baseProductionAdjustedPriority);

			addProductionDemand(id, scoutUnitId, false, baseProductionAdjustedPriority);

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
			addProductionDemand(id, item, true, 1.0);
			debug("\t\turgent demand added\n");
		}
		else if (totalNativeProtection < conf.ai_production_max_native_protection)
		{
			double protectionPriority = conf.ai_production_native_protection_priority * (conf.ai_production_max_native_protection - totalNativeProtection) / (conf.ai_production_max_native_protection - conf.ai_production_min_native_protection);
			addProductionDemand(id, item, false, protectionPriority);
			debug("\t\tregular demand added\n");
			debug("\t\tprotectionPriority=%f\n", protectionPriority);
		}

	}

	debug("\n");

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

	debug("\n");

}

/*
Set base production choices.
*/
void setProductionChoices()
{
	debug("setProductionChoices\n");

	for
	(
		std::map<int, PRODUCTION_DEMAND>::iterator productionDemandsIterator = productionDemands.begin();
		productionDemandsIterator != productionDemands.end();
		productionDemandsIterator++
	)
	{
		int baseId = productionDemandsIterator->first;
		PRODUCTION_DEMAND *productionDemand = &(productionDemandsIterator->second);

		debug("\t%-25s, item=%-25.25s, urgent=%s, priority=%f, sumPriorities=%f\n", tx_bases[baseId].name, prod_name(productionDemand->item), (productionDemand->urgent ? "y" : "n"), productionDemand->priority, productionDemand->sumPriorities);

		productionChoices[baseId] = {productionDemand->item, productionDemand->urgent};

	}

	debug("\n");

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
HOOK_API int suggestBaseProduction(int baseId, int a2, int a3, int a4)
{
    BASE* base = &(tx_bases[baseId]);
    bool productionDone = (base->status_flags & BASE_PRODUCTION_DONE);
    bool withinExemption = isBaseProductionWithinRetoolingExemption(baseId);

	debug("suggestBaseProduction: %s (%s), productionDone=%s, withinExemption=%s\n", base->name, prod_name(base->queue_items[0]), (productionDone ? "y" : "n"), (withinExemption ? "y" : "n"));
	debug("active_faction=%-25s, base_faction=%-25s\n", tx_metafactions[*active_faction].noun_faction, tx_metafactions[base->faction_id].noun_faction);

	// set default choice to vanilla

    int choice = tx_base_prod_choices(baseId, a2, a3, a4);
	debug("\t%-10s: %-25s\n", "Vanilla", prod_name(choice));

	// do not override vanilla choice in emergency scrambling - it does pretty goood job on it

	if (base->faction_id != *active_faction)
	{
        debug("vanilla emergency scrambe algorithm\n");
		return choice;
	}

    // do not suggest production for human factions

    if (is_human(base->faction_id))
	{
        debug("skipping human base\n");
        return choice;
    }

    // do not override production choice for not AI enabled factions

	if (!ai_enabled(base->faction_id))
	{
        debug("skipping not AI enabled computer base\n");
        return choice;
    }

	// do not disturb base building project

	if (isBaseBuildingProjectBeyondRetoolingExemption(baseId))
	{
		return choice;
	}

	// Thinker

	int thinkerChoice = base_production(baseId, a2, a3, a4);

	// use Thinker choice only for not managed facilities

	if (!(thinkerChoice < 0 && evaluatedFacilities.find(-thinkerChoice) != evaluatedFacilities.end()))
	{
		choice = thinkerChoice;
		debug("\t%-10s: %-25s\n", "Thiker", prod_name(choice));
	}

    // WTP

	if (base->faction_id != *active_faction)
	{
		debug("Do not use suggestBaseProduction in this case as it was generated for active faction only.\n");
		return choice;
	}

	// some facilities are taken care by WTP completely

	if (choice < 0 && evaluatedFacilities.find(-choice) != evaluatedFacilities.end())
	{
		productionDone = true;
	}

	// find strategy production choice

	PRODUCTION_CHOICE *productionChoice = (productionChoices.find(baseId) != productionChoices.end() ? &(productionChoices[baseId]) : NULL);

	// no WTP suggestion for this base

	if (productionChoice == NULL)
	{
		return choice;
	}

	// urgent change
	if (productionChoice->urgent)
	{
		choice = productionChoice->item;

		debug("\turgent\n");
		debug("\t%-10s: %-25s\n", "WTP", prod_name(choice));

	}
	// not urgent change
	else if (productionDone)
	{
		choice = productionChoice->item;

		debug("\tnot urgent\n");
		debug("\t%-10s: %-25s\n", "WTP", prod_name(choice));

	}

	debug("\n");

	return choice;

}

void addProductionDemand(int baseId, int item, bool urgent, double priority)
{
	BASE *base = &(tx_bases[baseId]);

	// prechecks

	// do not build combat unit in weak bases

	if (item >= 0 && isCombatUnit(item))
	{
		if (base->mineral_surplus < conf.ai_production_combat_unit_min_mineral_surplus)
			return;

	}

	// create or update demand

	// new entry
	if (productionDemands.find(baseId) == productionDemands.end())
	{
		productionDemands[baseId] = PRODUCTION_DEMAND({item, urgent, priority, priority});
	}
	// update entry
	else
	{
		PRODUCTION_DEMAND *productionDemand = &(productionDemands[baseId]);

		if
		(
			// new item is urgent but old one is not
			(urgent && !productionDemand->urgent)
			||
			// both item are same urgency but new one has higher priority
			(urgent == productionDemand->urgent && priority > productionDemand->priority)
		)
		{
			productionDemand->item = item;
			productionDemand->urgent = urgent;
			productionDemand->priority = priority;
			productionDemand->sumPriorities += priority;
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

/*
Returns most effective prototyped land defender unit id.
*/
int findConventionalDefenderUnit(int factionId)
{
	int bestUnitId = BSC_SCOUT_PATROL;
	double bestUnitDefenseEffectiveness = evaluateUnitDefenseEffectiveness(bestUnitId);

	for (int id : getFactionPrototypes(factionId, true))
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
	double bestProportionalUnitChassisSpeed = -1;
	int bestUnitChassisSpeed = -1;

	for (int id : prototypes)
	{
		UNIT *unit = &(tx_units[id]);

		// skip non combat units

		if (!isCombatUnit(id))
			continue;

		// given triad only

		if (unit_triad(id) != triad)
			continue;

		// calculate speed and proportional speed

		int unitChassisSpeed = unit_chassis_speed(id);
		double proportionalUnitChassisSpeed = (double)unit_chassis_speed(id) / (double)unit->cost;

		// find fastest/cheapest unit

		if
		(
			bestUnitId == -1
			||
			proportionalUnitChassisSpeed > bestProportionalUnitChassisSpeed
			||
			(proportionalUnitChassisSpeed == bestProportionalUnitChassisSpeed && unitChassisSpeed > bestUnitChassisSpeed)
		)
		{
			bestUnitId = id;
			bestProportionalUnitChassisSpeed = proportionalUnitChassisSpeed;
			bestUnitChassisSpeed = unitChassisSpeed;
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

		if (!isColonyUnit(id))
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

		if (!isColonyUnit(id))
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

	if (!isColonyUnit(unitId))
		return false;

	// do not produce colony in unproductive bases

	if (base->mineral_surplus <= 0)
		return false;

	// calculate production time

	int productionTurns = (tx_cost_factor(base->faction_id, 1, -1) * unit->cost) / max(1, base->mineral_surplus);

	// calculate base population at the time

	double projectedPopulation = (double)base->pop_size + (double)(base->nutrients_accumulated + base->nutrient_surplus * productionTurns) / (double)(tx_cost_factor(base->faction_id, 0, baseId) * (base->pop_size + 1));

	// reduce projected population if colony is being built

	if (isBaseBuildingColony(baseId))
	{
		projectedPopulation--;
	}

	// verify projected population is at least 2.5

	return (projectedPopulation >= 2.5);

}

int findFormerUnit(int triad)
{
	int bestUnitId = -1;
	double bestUnitValue = -1;

	for (int id : prototypes)
	{
		UNIT *unit = &(tx_units[id]);

		// formers only

		if (!isFormerUnit(id))
			continue;

		// given triad only

		if (unit_triad(id) != triad)
			continue;

		// calculate value

		double unitTerraformingSpeedValue = (unit_has_ability(id, ABL_FUNGICIDAL) ? 1.2 : 1.0) * (unit_has_ability(id, ABL_SUPER_TERRAFORMER) ? 2.5 : 1.0);
		double unitNativeProtectionValue = (unit_has_ability(id, ABL_TRANCE) ? 2.0 : 1.0);
		double unitChassisSpeedValue = 1.0 + (double)(unit_chassis_speed(id) - 1) * 1.5;
		double unitValue = unitTerraformingSpeedValue * unitNativeProtectionValue * unitChassisSpeedValue / (double)unit->cost;

		// find best unit

		if (bestUnitId == -1 || unitValue > bestUnitValue)
		{
			bestUnitId = id;
			bestUnitValue = unitValue;
		}

	}

	return bestUnitId;

}

