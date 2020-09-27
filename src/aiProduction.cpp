#include <map>
#include "aiProduction.h"
#include "engine.h"
#include "game.h"

std::set<int> bestProductionBaseIds;
std::map<int, std::map<int, double>> unitDemands;
std::map<int, PRODUCTION_DEMAND> productionDemands;
double averageFacilityMoraleBoost;
double territoryBonusMultiplier;

// currently processing base production demand

PRODUCTION_DEMAND productionDemand;

/*
Selects base production.
*/
HOOK_API int suggestBaseProduction(int baseId, int a2, int a3, int a4)
{
    BASE* base = &(tx_bases[baseId]);
    bool productionDone = (base->status_flags & BASE_PRODUCTION_DONE);
    bool withinExemption = isBaseProductionWithinRetoolingExemption(baseId);

	debug("suggestBaseProduction\n========================================\n");
	debug("%s (%s), active_faction=%s, base_faction=%s, productionDone=%s, withinExemption=%s\n", base->name, prod_name(base->queue_items[0]), tx_metafactions[*active_faction].noun_faction, tx_metafactions[base->faction_id].noun_faction, (productionDone ? "y" : "n"), (withinExemption ? "y" : "n"));

	// skip human

    if (is_human(base->faction_id))
	{
        debug("skipping human base\n");
        return base->queue_items[0];
    }

    // skip not enabled AI

	if (!ai_enabled(base->faction_id))
	{
        debug("skipping computer base\n");
        return tx_base_prod_choices(baseId, a2, a3, a4);
	}

	// defaults to Thinker if WTP algorithms are not enabled

	if (!conf.ai_useWTPAlgorithms)
	{
		debug("Not using WTP algorithms. Switcthing to Thinker code.\n");
		return base_production(baseId, a2, a3, a4);
	}

	// set default choice to vanilla

    int choice = tx_base_prod_choices(baseId, a2, a3, a4);
	debug("\n===>    %-10s: %-25s\n\n", "Vanilla", prod_name(choice));

	// do not override vanilla choice in emergency scrambling - it does pretty goood job on it

	if (base->faction_id != *active_faction)
	{
        debug("use Vanilla algorithm for emergency scrambe\n\n");
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
	debug("\n===>    %-10s: %-25s\n", "Thinker", prod_name(thinkerChoice));

	if (random_double(1.0) < conf.ai_production_Thinker_proportion)
	{
		choice = thinkerChoice;
		debug("\t\tused: YES\n");
	}
	else
	{
		debug("\t\tused: NO\n");
	}
	debug("\n");


    // WTP

    choice = aiSuggestBaseProduction(baseId, choice);
	debug("\n===>    %-10s: %-25s\n\n", "WTP", prod_name(choice));

	debug("========================================\n\n");

	return choice;

}

int aiSuggestBaseProduction(int baseId, int choice)
{
	BASE *base = &(tx_bases[baseId]);

    // initialize productionDemand

    productionDemand.baseId = baseId;
    productionDemand.base = base;
    productionDemand.item = choice;
    productionDemand.priority = 0.0;

	evaluateFacilitiesDemand();

	evaluateLandImprovementDemand();

	evaluateLandExpansionDemand();
	evaluateOceanExpansionDemand();

	evaluateExplorationDemand();

	evaluatePoliceDemand();

	evaluateNativeProtectionDemand();

//	evaluateFactionProtectionDemand();
//
	// update choice
	// WTP choice overrides previous choice with high enough priority.

	if (random_double(0.5) < productionDemand.priority)
	{
		choice = productionDemand.item;
	}

	return choice;

}

void evaluateFacilitiesDemand()
{
	debug("evaluateFacilitiesDemand\n");

	evaluateBasePopulationLimitFacilitiesDemand();
	evaluateBasePsychFacilitiesDemand();
	evaluateBaseMultiplyingFacilitiesDemand();

}

void evaluateBasePopulationLimitFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("\tevaluateBasePopulationLimitFacilitiesDemand\n");

	const int POPULATION_LIMIT_FACILITIES[] =
	{
		FAC_HAB_COMPLEX,
		FAC_HABITATION_DOME,
	};

	// estimate projected population

	int projectedPopulation = projectBasePopulation(baseId, conf.ai_production_population_projection_turns);
	debug("\t\tcurrentPopulation=%d, projectedTurns=%d, projectedPopulation=%d\n", base->pop_size, conf.ai_production_population_projection_turns, projectedPopulation);

	for (const int facilityId : POPULATION_LIMIT_FACILITIES)
	{
		// only available

		if (!isBaseFacilityAvailable(baseId, facilityId))
			continue;

		debug("\t\t\t%s\n", tx_facility[facilityId].name);

		// get limit

		int populationLimit = getFactionFacilityPopulationLimit(base->faction_id, facilityId);
		debug("\t\t\t\tpopulationLimit=%d\n", populationLimit);

		// limit overrun

		if (projectedPopulation > populationLimit)
		{
			// add demand

			addProductionDemand(-facilityId, 1.0);

			break;

		}

	}

}

void evaluateBasePsychFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	const int PSYCH_FACILITIES[] =
	{
		FAC_RECREATION_COMMONS,
		FAC_HOLOGRAM_THEATRE,
		FAC_PARADISE_GARDEN,
	};

	// do not evaluate psych facility if psych facility or colony is currently building or just completed one
	// this is because program does not recompute doctors at this time and we have no clue whether just produced psych facility or colony changed anything

	bool buildingPsychFacilityOrColony = false;

	if (base->queue_items[0] >= 0)
	{
		if (isColonyUnit(base->queue_items[0]))
		{
			buildingPsychFacilityOrColony = true;
		}
	}
	else
	{
		for (int psychFacilityId : PSYCH_FACILITIES)
		{
			if (-base->queue_items[0] == psychFacilityId)
			{
				buildingPsychFacilityOrColony = true;
				break;
			}

		}
	}

	if (buildingPsychFacilityOrColony)
		return;

	debug("\tevaluateBasePsychFacilitiesDemand\n");

	// calculate variables

	int doctors = getDoctors(baseId);
	int projectedPopulationIncrease = projectBasePopulation(baseId, conf.ai_production_population_projection_turns) - base->pop_size;

	debug("\t\tdoctors=%d, talent_total=%d, drone_total=%d, projectedTurns=%d, projectedPopulationIncrease=%d\n", doctors, base->talent_total, base->drone_total, conf.ai_production_population_projection_turns, projectedPopulationIncrease);

	// no expected riots in future

	if (!(doctors > 0 || (base->drone_total > 0 && base->drone_total + projectedPopulationIncrease > base->talent_total)))
		return;

	for (const int facilityId : PSYCH_FACILITIES)
	{
		// only available

		if (!isBaseFacilityAvailable(baseId, facilityId))
			continue;

		debug("\t\t\t%s\n", tx_facility[facilityId].name);

		// add demand

		addProductionDemand(-facilityId, 1.0);

		break;

	}

}

void evaluateBaseMultiplyingFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	Faction *faction = &(tx_factions[base->faction_id]);

	debug("\tevaluateBaseMultiplyingFacilitiesDemand\n");

	// calculate base values for multiplication

	double mineralSurplus = (double)base->mineral_surplus;
	double mineralIntake = (double)base->mineral_intake;
	double economyIntake = (double)base->energy_surplus * (double)(10 - faction->SE_alloc_psych - faction->SE_alloc_labs) / 10.0;
	double psychIntake = (double)base->energy_surplus * (double)(faction->SE_alloc_psych) / 10.0;
	double labsIntake = (double)base->energy_surplus * (double)(faction->SE_alloc_labs) / 10.0;

	debug("\tmineralSurplus=%f, mineralIntake=%f, economyIntake=%f, psychIntake=%f, labsIntake=%f\n", mineralSurplus, mineralIntake, economyIntake, psychIntake, labsIntake);

	// no production power - multiplying facilities are useless

	if (mineralSurplus <= 0.0)
		return;

	// multipliers
	// bit flag: 1 = minerals, 2 = economy, 4 = psych, 8 = fixed labs for biology lab (value >> 16 = fixed value)
	// for simplicity sake, hospitals are assumed to add 50% psych instead of 25% psych and -1 drone

	const int MULTIPLYING_FACILITIES[][2] =
	{
		{FAC_RECYCLING_TANKS, 1},
		{FAC_ENERGY_BANK, 2},
		{FAC_NETWORK_NODE, 8},
		{FAC_BIOLOGY_LAB, 16 + (4 << 16),},
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

		// check if base can build facility

		if (!isBaseFacilityAvailable(baseId, facilityId))
			continue;

		debug("\t\t%s\n", facility->name);

		// get facility parameters

		int cost = facility->cost;
		int maintenance = facility->maint;
		debug("\t\t\tcost=%d, maintenance=%d\n", cost, maintenance);

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

		// labs fixed
		if (mask & 16)
		{
			bonus += (double)(mask >> 16) / 2.0;
		}

		// calculate benefit

		double benefit = bonus - ((double)maintenance / 2.0);
		debug("\t\t\tbonus=%f, benefit=%f\n", bonus, benefit);

		// no benefit

		if (benefit <= 0)
			continue;

		// calculate effect and priority

		double effect = benefit / (double)cost;
		double priority = conf.ai_production_facility_effect_coefficient * effect;
		debug("\t\t\teffect=%f, priority=%f\n", effect, priority);

		// add demand

		addProductionDemand(-facilityId, priority);

	}

}

/*
Evaluates demand for land improvement.
*/
void evaluateLandImprovementDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluateLandImprovementDemand\n");

	// get base connected regions

	std::set<int> baseConnectedRegions = getBaseConnectedRegions(baseId);

	for
	(
		std::set<int>::iterator baseConnectedRegionsIterator = baseConnectedRegions.begin();
		baseConnectedRegionsIterator != baseConnectedRegions.end();
		baseConnectedRegionsIterator++
	)
	{
		int region =  *baseConnectedRegionsIterator;
		bool ocean = isOceanRegion(region);
		int triad = (ocean ? TRIAD_SEA : TRIAD_LAND);

		debug("\tregion=%d, type=%s\n", region, (ocean ? "ocean" : "land"));

		// find former unit

		int formerUnitId = findFormerUnit(base->faction_id, triad);

		if (formerUnitId == -1)
		{
			debug("\t\tno former unit\n");
			continue;
		}

		debug("\t\tformerUnit=%-25s\n", tx_units[formerUnitId].name);

		// count not improved worked tiles

		int notImprovedWorkedTileCount = 0;

		for (int regionBaseId : getRegionBases(base->faction_id, region))
		{
			for (MAP *workedTile : getBaseWorkedTiles(regionBaseId))
			{
				// do not count land rainy tiles for ocean region

				if (ocean && !is_ocean(workedTile) && map_rainfall(workedTile) == 2)
					continue;

				// ignore already improved tiles

				if (map_has_item(workedTile, TERRA_MINE | TERRA_FUNGUS | TERRA_SOLAR | TERRA_SOIL_ENR | TERRA_FOREST | TERRA_CONDENSER | TERRA_ECH_MIRROR | TERRA_THERMAL_BORE | TERRA_MONOLITH))
					continue;

				// count terraforming needs

				notImprovedWorkedTileCount++;

			}

		}

		debug("\t\tnotImprovedWorkedTileCount=%d\n", notImprovedWorkedTileCount);

		// count formers

		int formerCount = calculateRegionSurfaceUnitTypeCount(base->faction_id, region, WPN_TERRAFORMING_UNIT);
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

		int maxMineralSurplus = getRegionBasesMaxMineralSurplus(base->faction_id, region);
		debug("\t\tmaxMineralSurplus=%d\n", maxMineralSurplus);

		// set demand based on mineral surplus

		double baseProductionAdjustedPriority = priority * (double)base->mineral_surplus / (double)maxMineralSurplus;
		debug("\t\t\tmineral_surplus=%d, baseProductionAdjustedPriority=%f\n", base->mineral_surplus, baseProductionAdjustedPriority);

		addProductionDemand(formerUnitId, baseProductionAdjustedPriority);

	}

}

/*
Evaluates need for colonies.
*/
void evaluateLandExpansionDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluateLandExpansionDemand\n");

	// calcualte max base size

	int maxBaseSize = getMaxBaseSize(base->faction_id);
	debug("\t\tmaxBaseSize=%d\n", maxBaseSize);

	// calculate unpopulated discovered land tiles in the whole world

	double globalUnpopulatedLandTileWeighedCount = 0.0;
	int closeTileCount = 0;
	int closeTileRangeSum = 0;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int x = getXCoordinateByMapIndex(mapIndex);
		int y = getYCoordinateByMapIndex(mapIndex);
		MAP *tile = getMapTile(x, y);
		int region = tile->region;
		bool ocean = isOceanRegion(region);

		// only land regions

		if (ocean)
			continue;

		// discovered only

		if (~tile->visibility & (0x1 << base->faction_id))
			continue;

		// exclude territory claimed by other factions

		if (!(tile->owner == -1 || tile->owner == base->faction_id))
			continue;

		// exclude base and base radius

		if (tile->items & (TERRA_BASE_IN_TILE | TERRA_BASE_RADIUS))
			continue;

		// calculate distance to nearest own base

		int nearestBaseRange = getNearestFactionBaseRange(base->faction_id, x, y);

		// count available tiles

		globalUnpopulatedLandTileWeighedCount += min(1.0 , (double)conf.ai_production_max_unpopulated_range / (double)nearestBaseRange );

		// gather distance statistics

		int currentBaseRange = map_range(x, y, base->x, base->y);

		if (currentBaseRange <= conf.ai_production_max_unpopulated_range)
		{
			closeTileCount++;
			closeTileRangeSum += currentBaseRange;
		}

	}

	double averageCurrentBaseRange = (double)closeTileRangeSum / (double)closeTileCount;

	debug("\t\tglobalUnpopulatedLandTileWeighedCount=%f, averageCurrentBaseRange=%f\n", globalUnpopulatedLandTileWeighedCount, averageCurrentBaseRange);

	// evaluate need for land expansion

	double landExpansionDemand = globalUnpopulatedLandTileWeighedCount / conf.ai_production_expansion_coverage;
	debug("\t\tlandExpansionDemand=%f\n", landExpansionDemand);

	// too low demand

	if (landExpansionDemand < 1.0)
		return;

	// calculate existing and building colonies

	int existingLandColoniesCount = calculateUnitTypeCount(base->faction_id, WPN_COLONY_MODULE, TRIAD_LAND);
	debug("\t\texistingLandColoniesCount=%d\n", existingLandColoniesCount);

	// we have enough

	if (existingLandColoniesCount >= landExpansionDemand)
		return;

	// calculate priority

	double priority = conf.ai_production_expansion_priority * (landExpansionDemand - (double)existingLandColoniesCount) / landExpansionDemand;
	debug("\t\tpriority=%f\n", priority);

	// calculate infantry turns to destination assuming land units travel by roads 2/3 of time

	double infantryTurnsToDestination = 0.5 * averageCurrentBaseRange;

	// find optimal colony unit

	int optimalColonyUnitId = findOptimalColonyUnit(base->faction_id, TRIAD_LAND, base->mineral_surplus, infantryTurnsToDestination);

	debug("\t\t\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, infantryTurnsToDestination=%f, optimalColonyUnitId=%-25s\n", TRIAD_LAND, base->mineral_surplus, infantryTurnsToDestination, tx_units[optimalColonyUnitId].name);

	// verify base has enough growth potential to build a colony

	if (!canBaseProduceColony(baseId, optimalColonyUnitId))
	{
		debug("\t\t\t\tbase is too small to produce a colony\n");
		return;
	}

	// adjust priority based on base size

	double baseAdjustedPriority = priority * ((double)base->pop_size / (double)maxBaseSize);

	debug("\t\t\t\tpop_size=%d, baseAdjustedPriority=%f\n", base->pop_size, baseAdjustedPriority);

	// set base demand

	addProductionDemand(optimalColonyUnitId, baseAdjustedPriority);

}

/*
Evaluates need for colonies.
*/
void evaluateOceanExpansionDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluateOceanExpansionDemand\n");

	// base cannot build ships

	if (!isBaseCanBuildShips(baseId))
		return;

	// calcualte max base size

	int maxBaseSize = getMaxBaseSize(base->faction_id);
	debug("\t\tmaxBaseSize=%d\n", maxBaseSize);

	// get base connected ocean regions

	std::set<int> baseConnectedOceanRegions = getBaseConnectedOceanRegions(baseId);

	for (int region : baseConnectedOceanRegions)
	{
		debug("\tregion=%d, type=%s\n", region, "ocean");

		// check there is at least one colony unit for given triad

		int colonyUnitId = findColonyUnit(base->faction_id, TRIAD_SEA);

		if (colonyUnitId == -1)
		{
			debug("\t\tno colony unit\n")
			continue;
		}

		// calculate unpopulated tiles per region

		double oceanRegionUnpopulatedLandTileWeighedCount = 0.0;
		int oceanRegionCloseTileCount = 0;
		int oceanRegionCloseTileRangeSum = 0;

		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			int x = getXCoordinateByMapIndex(mapIndex);
			int y = getYCoordinateByMapIndex(mapIndex);
			MAP *tile = getMapTile(x, y);

			// this region only

			if (tile->region != region)
				continue;

			// discovered only

			if (~tile->visibility & (0x1 << base->faction_id))
				continue;

			// exclude territory claimed by other factions

			if (!(tile->owner == -1 || tile->owner == base->faction_id))
				continue;

			// exclude base and base radius

			if (tile->items & (TERRA_BASE_IN_TILE | TERRA_BASE_RADIUS))
				continue;

			// calculate distance to nearest own base

			int nearestBaseRange = getNearestFactionBaseRange(base->faction_id, x, y);

			// count available tiles

			oceanRegionUnpopulatedLandTileWeighedCount += min(1.0 , (double)conf.ai_production_max_unpopulated_range / (double)nearestBaseRange );

			// gather distance statistics

			int currentBaseRange = map_range(x, y, base->x, base->y);

			if (currentBaseRange <= conf.ai_production_max_unpopulated_range)
			{
				oceanRegionCloseTileCount++;
				oceanRegionCloseTileRangeSum += currentBaseRange;
			}

		}

		double averageOceanRegionCurrentBaseRange = (double)oceanRegionCloseTileRangeSum / (double)oceanRegionCloseTileCount;

		debug("\t\toceanRegionUnpopulatedLandTileWeighedCount=%f, averageOceanRegionCurrentBaseRange=%f\n", oceanRegionUnpopulatedLandTileWeighedCount, averageOceanRegionCurrentBaseRange);

		// evaluate need for land expansion

		double oceanExpansionDemand = oceanRegionUnpopulatedLandTileWeighedCount / conf.ai_production_ocean_expansion_coverage;
		debug("\t\toceanExpansionDemand=%f\n", oceanExpansionDemand);

		// too low demand

		if (oceanExpansionDemand < 1.0)
			continue;

		// calculate existing and building colonies

		int existingSeaColoniesCount = calculateUnitTypeCount(base->faction_id, WPN_COLONY_MODULE, TRIAD_SEA);
		debug("\t\texistingSeaColoniesCount=%d\n", existingSeaColoniesCount);

		// we have enough

		if (existingSeaColoniesCount >= oceanExpansionDemand)
			continue;

		// calculate priority

		double oceanRegionPriority = conf.ai_production_expansion_priority * (oceanExpansionDemand - (double)existingSeaColoniesCount) / oceanExpansionDemand;

		debug("\t\toceanRegionPriority=%f\n", oceanRegionPriority);

		// calculate infantry turns to destination assuming land units travel by roads 2/3 of time

		double infantryTurnsToDestination = averageOceanRegionCurrentBaseRange;

		// find optimal colony unit

		int optimalOceanRegionColonyUnitId = findOptimalColonyUnit(base->faction_id, TRIAD_SEA, base->mineral_surplus, infantryTurnsToDestination);

		debug("\t\t\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, infantryTurnsToDestination=%f, optimalOceanRegionColonyUnitId=%-25s\n", TRIAD_SEA, base->mineral_surplus, infantryTurnsToDestination, tx_units[optimalOceanRegionColonyUnitId].name);

		// verify base has enough growth potential to build a colony

		if (!canBaseProduceColony(baseId, optimalOceanRegionColonyUnitId))
		{
			debug("\t\t\t\tbase is too small to produce a colony\n");
			continue;
		}

		// adjust priority based on base size

		double baseAdjustedOceanRegionPriority = oceanRegionPriority * ((double)base->pop_size / (double)maxBaseSize);

		debug("\t\t\t\tpop_size=%d, baseAdjustedOceanRegionPriority=%f\n", base->pop_size, baseAdjustedOceanRegionPriority);

		// set base demand

		addProductionDemand(optimalOceanRegionColonyUnitId, baseAdjustedOceanRegionPriority);

	}

}

/*
Evaluates need for exploration.
*/
void evaluateExplorationDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluateExplorationDemand\n");

	// get base connected regions

	std::set<int> baseConnectedRegions = getBaseConnectedRegions(baseId);

	for
	(
		std::set<int>::iterator baseConnectedRegionsIterator = baseConnectedRegions.begin();
		baseConnectedRegionsIterator != baseConnectedRegions.end();
		baseConnectedRegionsIterator++
	)
	{
		int region =  *baseConnectedRegionsIterator;
		bool ocean = isOceanRegion(region);
		int triad = (ocean ? TRIAD_SEA : TRIAD_LAND);

		debug("\tregion=%d, type=%s\n", region, (ocean ? "ocean" : "land"));

		// find scout unit

		int scoutUnitId = findScoutUnit(base->faction_id, triad);

		if (scoutUnitId == -1)
		{
			debug("\t\tno scout unit\n");
			continue;
		}

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

				for (int regionBaseId : getRegionBases(base->faction_id, region))
				{
					BASE *regionBase = &(tx_bases[regionBaseId]);

					// calculate range to tile

					int range = map_range(x, y, regionBase->x, regionBase->y);

					if (nearestBase == NULL || range < nearestBaseRange)
					{
						nearestBase = regionBase;
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

		for (int vehicleId : getRegionSurfaceVehicles(base->faction_id, region, true))
		{
			VEH *vehicle = &(tx_vehicles[vehicleId]);
			MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);

			// exclude non combat units

			if (!isCombatVehicle(vehicleId))
				continue;

			// only units with hand weapon

			if (tx_units[vehicle->proto_id].weapon_type != WPN_HAND_WEAPONS)
				continue;

			// exclude infantry units at base

			if (tx_units[vehicle->proto_id].chassis_type == CHS_INFANTRY && (vehicleLocation->items & TERRA_BASE_IN_TILE))
				continue;

			// estimate vehicle speed

			int vehicleSpeed = veh_chassis_speed(vehicleId);

			// adjust native speed assuming there is a lot of fungus

			if (!ocean && isNativeLandVehicle(vehicleId))
			{
				vehicleSpeed *= 2;
			}

			existingExplorersCombinedSpeed += vehicleSpeed;

		}

		debug("\t\texistingExplorersCombinedSpeed=%d\n", existingExplorersCombinedSpeed);

		// calculate need for infantry explorers

		double infantryExplorersTotalDemand = (double)unexploredTileCount / conf.ai_production_exploration_coverage;
		double infantryExplorersRemainingDemand = infantryExplorersTotalDemand - (double)existingExplorersCombinedSpeed;

		debug("\t\tinfantryExplorersTotalDemand=%f, infantryExplorersRemainingDemand=%f\n", infantryExplorersTotalDemand, infantryExplorersRemainingDemand);

		// we have enough

		if (infantryExplorersRemainingDemand <= 0)
			continue;

		// otherwise, set priority

		double priority = infantryExplorersRemainingDemand / infantryExplorersTotalDemand;

		debug("\t\tpriority=%f\n", priority);

		// find max mineral surplus in region

		int maxMineralSurplus = getRegionBasesMaxMineralSurplus(base->faction_id, region);
		debug("\t\tmaxMineralSurplus=%d\n", maxMineralSurplus);

		// set demand based on mineral surplus

		double baseProductionAdjustedPriority = priority * (double)base->mineral_surplus / (double)maxMineralSurplus;

		debug("\t\t\t%-25s mineral_surplus=%d, baseProductionAdjustedPriority=%f\n", base->name, base->mineral_surplus, baseProductionAdjustedPriority);

		addProductionDemand(scoutUnitId, baseProductionAdjustedPriority);

	}

}

/*
Evaluates need for police units.
*/
void evaluatePoliceDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluatePoliceDemand\n");

	// calculate police variables

	int allowedPolice = getAllowedPolice(base->faction_id);
	int basePoliceUnitCount = getBasePoliceUnitCount(baseId);
	bool morePoliceUnitsAllowed = basePoliceUnitCount < allowedPolice;
	bool baseNeedPsych = isBaseNeedPsych(baseId);
	int policeUnitId = -1;
	double priority = 0.0;
	debug("\tallowedPolice=%d, basePoliceUnitCount=%d, morePoliceUnitsAllowed=%d, baseNeedPsych=%d\n", allowedPolice, basePoliceUnitCount, morePoliceUnitsAllowed, baseNeedPsych);

	// more police is not allowed or base doesn't need psych at all

	if (!morePoliceUnitsAllowed || !baseNeedPsych)
		return;

	// determine police need

	if (base->pop_size == 1 && basePoliceUnitCount == 0)
	{
		// for new base police priority is higher than psych facility priority as Scout Partrol is cheaper to both build and buy
		// it is also to protect empty base as soon as possible

		policeUnitId = BSC_SCOUT_PATROL;
		priority = 2.0;
		debug("\tNew base urgently needs police.\n");

	}
	else
	{
		// for not new base police priority is lower than psych facility priority as it is more costly to support

		policeUnitId = findPoliceUnit(base->faction_id);
		priority = 0.5;
		debug("\tRegular police demand.\n");

		return;

	}

	if (policeUnitId >= 0 && priority > 0.0)
	{
		debug("\tpoliceUnitId=%s, priority=%f\n", tx_units[policeUnitId].name, priority);
		addProductionDemand(policeUnitId, priority);
	}

}

/*
Evaluates need for bases protection against natives.
*/
void evaluateNativeProtectionDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluateNativeProtectionDemand\n");

	// calculate current native protection

	double baseNativeProtection = getBaseNativeProtection(baseId);
	debug("\tbaseNativeProtection=%f\n", baseNativeProtection);

	// add protection demand

	int item = findStrongestNativeDefensePrototype(base->faction_id);

	debug("\tprotectionUnit=%-25s\n", tx_units[item].name);

	// calculate priority

	double priority;

	if (baseNativeProtection < conf.ai_production_min_native_protection)
	{
		priority = 1.0;
	}
	else if (baseNativeProtection < conf.ai_production_max_native_protection)
	{
		priority = conf.ai_production_native_protection_priority * (conf.ai_production_max_native_protection - baseNativeProtection) / (conf.ai_production_max_native_protection - conf.ai_production_min_native_protection);
	}
	else
	{
		priority = 0.0;
	}

	debug("\tpriority=%f\n", priority);

	if (priority > 0.0)
	{
		addProductionDemand(item, priority);
	}

}

///*
//Calculate demand for protection against other factions.
//*/
//void evaluateFactionProtectionDemand()
//{
//	debug("%-24s evaluateFactionProtectionDemand\n", tx_metafactions[aiFactionId].noun_faction);
//
//	// hostile conventional offense
//
//	debug("\tconventional offense\n");
//
//	double hostileConventionalToConventionalOffenseStrength = 0.0;
//	double hostileConventionalToPsiOffenseStrength = 0.0;
//
//	for (int id = 0; id < *total_num_vehicles; id++)
//	{
//		VEH *vehicle = &(tx_vehicles[id]);
//		MAP *location = getMapTile(vehicle->x, vehicle->y);
//		int triad = veh_triad(id);
//
//		// exclude own vehicles
//
//		if (vehicle->faction_id == aiFactionId)
//			continue;
//
//		// exclude natives
//
//		if (vehicle->faction_id == 0)
//			continue;
//
//		// exclude non combat
//
//		if (!isCombatVehicle(id))
//			continue;
//
//		// find vehicle weapon offense and defense values
//
//		int vehicleOffenseValue = getVehicleOffenseValue(id);
//		int vehicleDefenseValue = getVehicleDefenseValue(id);
//
//		// only conventional offense
//
//		if (vehicleOffenseValue == -1)
//			continue;
//
//		// exclude conventional vehicle with too low attack/defense ratio
//
//		if (vehicleOffenseValue != -1 && vehicleDefenseValue != -1 && vehicleOffenseValue < vehicleDefenseValue / 2)
//			continue;
//
//		// get morale modifier for attack
//
//		double moraleModifierAttack = getMoraleModifierAttack(id);
//
//		// get PLANET bonus on attack
//
//		double planetModifierAttack = getSEPlanetModifierAttack(vehicle->faction_id);
//
//		// get vehicle region
//
//		int region = (triad == TRIAD_AIR ? -1 : location->region);
//
//		// find nearest own base
//
//		int nearestOwnBaseId = findNearestOwnBaseId(vehicle->x, vehicle->y, region);
//
//		// base not found
//
//		if (nearestOwnBaseId == -1)
//			continue;
//
//		// find range to nearest own base
//
//		BASE *nearestOwnBase = &(tx_bases[nearestOwnBaseId]);
//		int nearestOwnBaseRange = map_range(vehicle->x, vehicle->y, nearestOwnBase->x, nearestOwnBase->y);
//
//		// calculate vehicle speed
//		// for land units assuming there are roads everywhere
//
//		double vehicleSpeed = (triad == TRIAD_LAND ? getLandVehicleSpeedOnRoads(id) : getVehicleSpeedWithoutRoads(id));
//
//		// calculate threat time koefficient
//
//		double timeThreatKoefficient = max(0.0, (MAX_THREAT_TURNS - ((double)nearestOwnBaseRange / vehicleSpeed)) / MAX_THREAT_TURNS);
//
//		// calculate conventional anti conventional defense demand
//
//		double conventionalCombatStrength =
//			(double)vehicleOffenseValue
//			* factionInfos[vehicle->faction_id].offenseMultiplier
//			* factionInfos[vehicle->faction_id].fanaticBonusMultiplier
//			* moraleModifierAttack
//			* factionInfos[vehicle->faction_id].threatKoefficient
//			/ getBaseDefenseMultiplier(nearestOwnBaseId, triad, true, true)
//			* timeThreatKoefficient
//		;
//
//		// calculate psi anti conventional defense demand
//
//		double psiCombatStrength =
//			getPsiCombatBaseOdds(triad)
//			* factionInfos[vehicle->faction_id].offenseMultiplier
//			* moraleModifierAttack
//			* planetModifierAttack
//			* factionInfos[vehicle->faction_id].threatKoefficient
//			/ getBaseDefenseMultiplier(nearestOwnBaseId, triad, false, true)
//			* timeThreatKoefficient
//		;
//
//		debug
//		(
//			"\t\t[%3d](%3d,%3d), %-25s\n",
//			id,
//			vehicle->x,
//			vehicle->y,
//			tx_metafactions[vehicle->faction_id].noun_faction
//		);
//		debug
//		(
//			"\t\t\tvehicleOffenseValue=%2d, offenseMultiplier=%4.1f, fanaticBonusMultiplier=%4.1f, moraleModifierAttack=%4.1f, threatKoefficient=%4.1f, nearestOwnBaseRange=%3d, baseDefenseMultiplier=%4.1f, timeThreatKoefficient=%4.2f, conventionalCombatStrength=%4.1f\n",
//			vehicleOffenseValue,
//			factionInfos[vehicle->faction_id].offenseMultiplier,
//			factionInfos[vehicle->faction_id].fanaticBonusMultiplier,
//			moraleModifierAttack,
//			factionInfos[vehicle->faction_id].threatKoefficient,
//			nearestOwnBaseRange,
//			getBaseDefenseMultiplier(nearestOwnBaseId, triad, true, true),
//			timeThreatKoefficient,
//			conventionalCombatStrength
//		);
//		debug
//		(
//			"\t\t\tpsiCombatBaseOdds=%4.1f, offenseMultiplier=%4.1f, moraleModifierAttack=%4.1f, planetModifierAttack=%4.1f, threatKoefficient=%4.1f, nearestOwnBaseRange=%3d, baseDefenseMultiplier=%4.1f, timeThreatKoefficient=%4.2f, psiCombatStrength=%4.1f\n",
//			getPsiCombatBaseOdds(triad),
//			factionInfos[vehicle->faction_id].offenseMultiplier,
//			moraleModifierAttack,
//			planetModifierAttack,
//			factionInfos[vehicle->faction_id].threatKoefficient,
//			nearestOwnBaseRange,
//			getBaseDefenseMultiplier(nearestOwnBaseId, triad, false, true),
//			timeThreatKoefficient,
//			psiCombatStrength
//		);
//
//		// update total
//
//		hostileConventionalToConventionalOffenseStrength += conventionalCombatStrength;
//		hostileConventionalToPsiOffenseStrength += psiCombatStrength;
//
//	}
//
//	debug
//	(
//		"\thostileConventionalToConventionalOffenseStrength=%.0f, hostileConventionalToPsiOffenseStrength=%.0f\n",
//		hostileConventionalToConventionalOffenseStrength,
//		hostileConventionalToPsiOffenseStrength
//	);
//
//	// own defense strength
//
//	debug("\tanti conventional defense\n");
//
//	double ownConventionalToConventionalDefenseStrength = 0.0;
//	double ownConventionalToPsiDefenseStrength = 0.0;
//
//	for (int id : combatVehicleIds)
//	{
//		VEH *vehicle = &(tx_vehicles[id]);
//
//		// find vehicle defense and offense values
//
//		int vehicleDefenseValue = getVehicleDefenseValue(id);
//
//		// conventional defense
//
//		if (vehicleDefenseValue == -1)
//			continue;
//
//		// get vehicle morale multiplier
//
//		double moraleModifierDefense = getMoraleModifierDefense(id);
//
//		// calculate vehicle defense strength
//
//		double vehicleDefenseStrength =
//			(double)vehicleDefenseValue
//			* factionInfos[*active_faction].defenseMultiplier
//			* moraleModifierDefense
//			* territoryBonusMultiplier
//		;
//
//		debug
//		(
//			"\t\t[%3d](%3d,%3d) vehicleDefenseValue=%2d, defenseMultiplier=%4.1f, moraleModifierDefense=%4.1f, vehicleDefenseStrength=%4.1f\n",
//			id,
//			vehicle->x,
//			vehicle->y,
//			vehicleDefenseValue,
//			factionInfos[vehicle->faction_id].defenseMultiplier,
//			moraleModifierDefense,
//			vehicleDefenseStrength
//		);
//
//		// update total
//
//		ownConventionalToConventionalDefenseStrength += vehicleDefenseStrength;
//
//	}
//
//	for (int id : combatVehicleIds)
//	{
//		VEH *vehicle = &(tx_vehicles[id]);
//
//		// find vehicle defense and offense values
//
//		int vehicleDefenseValue = getVehicleDefenseValue(id);
//
//		// psi defense only
//
//		if (vehicleDefenseValue != -1)
//			continue;
//
//		// get vehicle psi combat defense strength
//
//		double psiDefenseStrength = getVehiclePsiDefenseStrength(id);
//
//		// calculate vehicle defense strength
//
//		double vehicleDefenseStrength =
//			psiDefenseStrength
//			* factionInfos[*active_faction].defenseMultiplier
//			* territoryBonusMultiplier
//		;
//
//		debug
//		(
//			"\t\t[%3d](%3d,%3d) psiDefenseStrength=%4.1f, defenseMultiplier=%4.1f, territoryBonusMultiplier=%4.1f, vehicleDefenseStrength=%4.1f\n",
//			id,
//			vehicle->x,
//			vehicle->y,
//			psiDefenseStrength,
//			factionInfos[vehicle->faction_id].defenseMultiplier,
//			territoryBonusMultiplier,
//			vehicleDefenseStrength
//		);
//
//		// update total
//
//		ownConventionalToPsiDefenseStrength += vehicleDefenseStrength;
//
//	}
//
//	debug
//	(
//		"\townConventionalToConventionalDefenseStrength=%.0f, ownConventionalToPsiDefenseStrength=%.0f\n",
//		ownConventionalToConventionalDefenseStrength,
//		ownConventionalToPsiDefenseStrength
//	);
//
//	// we have enough protection
//
//	if
//	(
//		(ownConventionalToConventionalDefenseStrength >= hostileConventionalToConventionalOffenseStrength)
//		||
//		(ownConventionalToPsiDefenseStrength >= hostileConventionalToPsiOffenseStrength)
//	)
//	{
//		return;
//	}
//
//	debug("\n");
//
//}
//
void addProductionDemand(int item, double priority)
{
	BASE *base = productionDemand.base;

	// prechecks

	// do not exhaust weak base support

	if (base->mineral_consumption > 0 && item >= 0 && isCombatUnit(item))
	{
		if (base->mineral_surplus < conf.ai_production_combat_unit_min_mineral_surplus)
			return;

	}

	// update demand

	if (priority > productionDemand.priority)
	{
		productionDemand.item = item;
		productionDemand.priority = priority;
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
	double bestUnitDefenseEffectiveness = evaluateUnitConventionalDefenseEffectiveness(bestUnitId);

	for (int unitId : getFactionPrototypes(factionId, false))
	{
		// skip non combat units

		if (!isCombatUnit(unitId))
			continue;

		// skip air units

		if (unit_triad(unitId) == TRIAD_AIR)
			continue;

		double unitDefenseEffectiveness = evaluateUnitConventionalDefenseEffectiveness(unitId);

		if (bestUnitId == -1 || unitDefenseEffectiveness > bestUnitDefenseEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitDefenseEffectiveness = unitDefenseEffectiveness;
		}

	}

	return bestUnitId;

}

int findFastAttackerUnit(int factionId)
{
	int bestUnitId = BSC_SCOUT_PATROL;
	double bestUnitOffenseEffectiveness = evaluateUnitConventionalOffenseEffectiveness(bestUnitId);

	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(tx_units[unitId]);

		// skip non combat units

		if (!isCombatUnit(unitId))
			continue;

		// skip infantry units

		if (unit->chassis_type == CHS_INFANTRY)
			continue;

		double unitOffenseEffectiveness = evaluateUnitConventionalOffenseEffectiveness(unitId);

		if (bestUnitId == -1 || unitOffenseEffectiveness > bestUnitOffenseEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitOffenseEffectiveness = unitOffenseEffectiveness;
		}

	}

	return bestUnitId;

}

int findScoutUnit(int factionId, int triad)
{
	int bestUnitId = -1;
	double bestProportionalUnitChassisSpeed = -1;
	int bestUnitChassisSpeed = -1;

	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(tx_units[unitId]);

		// skip non combat units

		if (!isCombatUnit(unitId))
			continue;

		// given triad only

		if (unit_triad(unitId) != triad)
			continue;

		// calculate speed and proportional speed

		int unitChassisSpeed = unit_chassis_speed(unitId);
		double proportionalUnitChassisSpeed = (double)unit_chassis_speed(unitId) / (double)unit->cost;

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
			bestUnitId = unitId;
			bestProportionalUnitChassisSpeed = proportionalUnitChassisSpeed;
			bestUnitChassisSpeed = unitChassisSpeed;
		}

	}

	return bestUnitId;

}

int findColonyUnit(int factionId, int triad)
{
	int bestUnitId = -1;

	for (int unitId : getFactionPrototypes(factionId, false))
	{
		// given triad only

		if (unit_triad(unitId) != triad)
			continue;

		// colony only

		if (!isColonyUnit(unitId))
			continue;

		bestUnitId = unitId;
		break;

	}

	return bestUnitId;

}

int findOptimalColonyUnit(int factionId, int triad, int mineralSurplus, double infantryTurnsToDestination)
{
	int bestUnitId = -1;
	double bestTotalTurns = 0.0;

	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(tx_units[unitId]);

		// given triad only

		if (unit_triad(unitId) != triad)
			continue;

		// colony only

		if (!isColonyUnit(unitId))
			continue;

		// calculate total turns

		double totalTurns = (double)(tx_cost_factor(*active_faction, 1, -1) * unit->cost) / (double)mineralSurplus + infantryTurnsToDestination / (double)unit_chassis_speed(unitId);

		// update best unit

		if (bestUnitId == -1 || totalTurns < bestTotalTurns)
		{
			bestUnitId = unitId;
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

int findFormerUnit(int factionId, int triad)
{
	int bestUnitId = -1;
	double bestUnitValue = -1;

	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(tx_units[unitId]);

		// formers only

		if (!isFormerUnit(unitId))
			continue;

		// given triad only

		if (unit_triad(unitId) != triad)
			continue;

		// calculate value

		double unitTerraformingSpeedValue = (unit_has_ability(unitId, ABL_FUNGICIDAL) ? 1.2 : 1.0) * (unit_has_ability(unitId, ABL_SUPER_TERRAFORMER) ? 2.5 : 1.0);
		double unitNativeProtectionValue = (unit_has_ability(unitId, ABL_TRANCE) ? 2.0 : 1.0);
		double unitChassisSpeedValue = 1.0 + (double)(unit_chassis_speed(unitId) - 1) * 1.5;
		double unitValue = unitTerraformingSpeedValue * unitNativeProtectionValue * unitChassisSpeedValue / (double)unit->cost;

		// find best unit

		if (bestUnitId == -1 || unitValue > bestUnitValue)
		{
			bestUnitId = unitId;
			bestUnitValue = unitValue;
		}

	}

	return bestUnitId;

}

int getRegionBasesMaxMineralSurplus(int factionId, int region)
{
	int maxMineralSurplus = 0;

	for (int baseId : getRegionBases(factionId, region))
	{
		BASE *base = &(tx_bases[baseId]);

		maxMineralSurplus = max(maxMineralSurplus, base->mineral_surplus);

	}

	return maxMineralSurplus;

}

int getRegionBasesMaxPopulationSize(int factionId, int region)
{
	int maxPopulationSize = 0;

	for (int baseId : getRegionBases(factionId, region))
	{
		BASE *base = &(tx_bases[baseId]);

		maxPopulationSize = max(maxPopulationSize, (int)base->pop_size);

	}

	return maxPopulationSize;

}

int getMaxBaseSize(int factionId)
{
	int maxBaseSize = 0;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(tx_bases[baseId]);

		if (base->faction_id != factionId)
			continue;

		maxBaseSize = max(maxBaseSize, (int)base->pop_size);

	}

	return maxBaseSize;

}

/*
Calculates number of surface units with specific weaponType in region: existing and building.
Only corresponding triad units are counted.
*/
int calculateRegionSurfaceUnitTypeCount(int factionId, int region, int weaponType)
{
	bool ocean = isOceanRegion(region);
	int triad = (ocean ? TRIAD_SEA : TRIAD_LAND);

	int regionUnitTypeCount = 0;

	for (int vehicleId : getRegionSurfaceVehicles(factionId, region, true))
	{
		VEH *vehicle = &(tx_vehicles[vehicleId]);

		if (tx_units[vehicle->proto_id].weapon_type != weaponType)
			continue;

		if (veh_triad(vehicleId) != triad)
			continue;

		regionUnitTypeCount++;

	}

	for (int baseId : getRegionBases(factionId, region))
	{
		BASE *base = &(tx_bases[baseId]);
		int item = base->queue_items[0];

		if (item < 0)
			continue;

		if (tx_units[item].weapon_type != weaponType)
			continue;

		if (unit_triad(item) != triad)
			continue;

		regionUnitTypeCount++;

	}

	return regionUnitTypeCount;

}

/*
Calculates number of units with specific weaponType: existing and building.
Only corresponding triad units are counted.
*/
int calculateUnitTypeCount(int factionId, int weaponType, int triad)
{
	int unitTypeCount = 0;

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(tx_vehicles[vehicleId]);

		if (vehicle->faction_id != factionId)
			continue;

		if (tx_units[vehicle->proto_id].weapon_type != weaponType)
			continue;

		if (veh_triad(vehicleId) != triad)
			continue;

		unitTypeCount++;

	}

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(tx_bases[baseId]);
		int item = base->queue_items[0];

		if (base->faction_id != factionId)
			continue;

		if (item < 0)
			continue;

		if (tx_units[item].weapon_type != weaponType)
			continue;

		if (unit_triad(item) != triad)
			continue;

		unitTypeCount++;

	}

	return unitTypeCount;

}

bool isBaseNeedPsych(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	int doctors = getDoctors(baseId);
	int projectedPopulationIncrease = projectBasePopulation(baseId, conf.ai_production_population_projection_turns) - base->pop_size;

	return (doctors > 0 || (base->drone_total > 0 && base->drone_total + projectedPopulationIncrease > base->talent_total));

}

int findPoliceUnit(int factionId)
{
	int policeUnitId = -1;
	double policeUnitEffectiveness = 0.0;

	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(tx_units[unitId]);

		double value = 1.0;

		if (unit_has_ability(unitId, ABL_POLICE_2X))
		{
			value *= 2.0;
		}

		if (unit_has_ability(unitId, ABL_TRANCE))
		{
			value *= 1.5;
		}

		int cost = unit->cost;

		if (!unit_has_ability(unitId, ABL_CLEAN_REACTOR))
		{
			cost += 4;
		}

		double effectiveness = value / (double)cost;

		if (policeUnitId == -1 || effectiveness > policeUnitEffectiveness)
		{
			policeUnitId = unitId;
			policeUnitEffectiveness = effectiveness;
		}

	}

	return policeUnitId;

}

/*
Wraps first time base production setup.
*/
HOOK_API void modifiedBaseFirst(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	// execute original code

	tx_base_first(baseId);

	// that's it for human faction

	// override default choice with WTP choice

	base->queue_items[0] = suggestBaseProduction(baseId, 0, 0, 0);

}

int getNearestFactionBaseRange(int factionId, int x, int y)
{
	int nearestFactionBaseRange = 9999;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(tx_bases[baseId]);

		// own bases

		if (base->faction_id != factionId)
			continue;

		nearestFactionBaseRange = min(nearestFactionBaseRange, map_range(x, y, base->x, base->y));

	}

	return nearestFactionBaseRange;

}

