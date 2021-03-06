#include <set>
#include <map>
#include "engine.h"
#include "game.h"
#include "game_wtp.h"
#include "terranx_wtp.h"
#include "aiProduction.h"
#include "ai.h"

const std::unordered_set<int> MANAGED_FACILITIES
	{
		FAC_HAB_COMPLEX,
		FAC_HABITATION_DOME,
		FAC_RECREATION_COMMONS,
		FAC_HOLOGRAM_THEATRE,
		FAC_PARADISE_GARDEN,
		FAC_RECYCLING_TANKS,
		FAC_ENERGY_BANK,
		FAC_NETWORK_NODE,
		FAC_BIOLOGY_LAB,
		FAC_HOLOGRAM_THEATRE,
		FAC_TREE_FARM,
		FAC_HYBRID_FOREST,
		FAC_FUSION_LAB,
		FAC_QUANTUM_LAB,
		FAC_RESEARCH_HOSPITAL,
		FAC_NANOHOSPITAL,
		FAC_GENEJACK_FACTORY,
		FAC_ROBOTIC_ASSEMBLY_PLANT,
		FAC_QUANTUM_CONVERTER,
		FAC_NANOREPLICATOR,
		FAC_PERIMETER_DEFENSE,
		FAC_TACHYON_FIELD,
		FAC_NAVAL_YARD,
		FAC_AEROSPACE_COMPLEX,
	}
;

std::set<int> bestProductionBaseIds;
std::map<int, std::map<int, double>> unitDemands;
std::map<int, PRODUCTION_DEMAND> productionDemands;
double averageFacilityMoraleBoost;
double territoryBonusMultiplier;

// combat priority used globally

double globalMilitaryPriority;
double regionMilitaryPriority;

// currently processing base production demand

PRODUCTION_DEMAND productionDemand;

/*
Prepare production choices.
*/
void aiProductionStrategy()
{
	evaluateDefenseDemand();
	
}

/*
Selects base production.
*/
HOOK_API int modifiedBaseProductionChoice(int baseId, int a2, int a3, int a4)
{
	// fallback to Thinker for test
//	return mod_base_production(baseId, a2, a3, a4);
	
    BASE* base = &(Bases[baseId]);
    bool productionDone = (base->status_flags & BASE_PRODUCTION_DONE);
    bool withinExemption = isBaseProductionWithinRetoolingExemption(baseId);
    MAP *baseTile = getBaseMapTile(baseId);

	debug("\nmodifiedBaseProductionChoice\n========================================\n");
	debug("%s (%s), active_faction=%s, base_faction=%s, productionDone=%s, withinExemption=%s\n", base->name, prod_name(base->queue_items[0]), MFactions[aiFactionId].noun_faction, MFactions[base->faction_id].noun_faction, (productionDone ? "y" : "n"), (withinExemption ? "y" : "n"));

	// execute original code for human bases

    if (is_human(base->faction_id))
	{
        debug("skipping human base\n");
        return base_prod_choice(baseId, a2, a3, a4);
    }

	// execute original code for not enabled computer bases

	if (!ai_enabled(base->faction_id))
	{
        debug("skipping computer base\n");
        return base_prod_choice(baseId, a2, a3, a4);
	}

	// defaults to Thinker if WTP algorithms are not enabled

	if (!conf.ai_useWTPAlgorithms)
	{
		debug("not using WTP algorithms - default to Thinker\n");
		return mod_base_production(baseId, a2, a3, a4);
	}

	// do not disturb base building project

	if (isBaseBuildingProjectBeyondRetoolingExemption(baseId))
	{
		return getBaseBuildingItem(baseId);
	}
	
	// compute military priority
	
	globalMilitaryPriority = std::max(conf.ai_production_military_priority_minimal, conf.ai_production_military_priority * activeFactionInfo.defenseDemand);
	regionMilitaryPriority = std::max(conf.ai_production_military_priority_minimal, conf.ai_production_military_priority * activeFactionInfo.regionDefenseDemand[baseTile->region]);
	
	debug("\tglobalMilitaryPriority = %f, regionMilitaryPriority = %f\n", globalMilitaryPriority, regionMilitaryPriority);
	
	// vanilla choice

    int choice = base_prod_choice(baseId, a2, a3, a4);
	debug("\n===>    %-10s: %-25s\n\n", "vanilla", prod_name(choice));

	// do not override vanilla choice in emergency scrambling - it does pretty goood job at it

	if (base->faction_id != aiFactionId)
	{
        debug("use Vanilla algorithm for emergency scrambe\n\n");
		return choice;
	}
	
	// calculate vanilla priority

	double vanillaPriority = 0.0;
	
	// unit
	if (choice > 0)
	{
		vanillaPriority = conf.ai_production_vanilla_priority_unit;
	}
	// project
	else if (choice <= -PROJECT_ID_FIRST)
	{
		vanillaPriority = conf.ai_production_vanilla_priority_project;
	}
	// facility
	else
	{
		// only not managed facilities
		if (MANAGED_FACILITIES.count(-choice) == 0)
		{
			vanillaPriority = conf.ai_production_vanilla_priority_facility;
		}
	}
	
	// raise vanilla priority for military items
	
	if (isMilitaryItem(choice))
	{
		debug("vanilla choice is military item\n");
		
		double militaryPriority = (choice >= 0 && Units[choice].triad() == TRIAD_AIR ? globalMilitaryPriority : regionMilitaryPriority);
		
		// raise vanilla priority to military priority
		
		if (militaryPriority > vanillaPriority)
		{
			vanillaPriority = militaryPriority;
		}
		
	}
	
	// Thinker
	
    int thinkerChoice = thinker_base_prod_choice(baseId, a2, a3, a4);
	debug("\n===>    %-10s: %-25s\n\n", "Thinker", prod_name(thinkerChoice));

	// calculate Thinker priority
	
	double thinkerPriority = conf.ai_production_thinker_priority;
	
	// drop priority for managed facilities
	if (thinkerChoice < 0 && MANAGED_FACILITIES.count(-thinkerChoice) != 0)
	{
		thinkerPriority = 0.0;
	}

	// raise thinker priority for combat items
	
	if (isMilitaryItem(thinkerChoice))
	{
		debug("Thinker choice is militray item\n");
		
		double militaryPriority = (thinkerChoice >= 0 && Units[thinkerChoice].triad() == TRIAD_AIR ? globalMilitaryPriority : regionMilitaryPriority);
		
		// raise thinker priority to military priority
		
		if (militaryPriority > thinkerPriority)
		{
			thinkerPriority = militaryPriority;
		}
		
	}
	
    // WTP

    int wtpChoice = aiSuggestBaseProduction(baseId, choice);
	debug("\n===>    %-10s: %-25s\n\n", "WTP", prod_name(wtpChoice));
	
	// calculate wtp priority
	
	double wtpPriority = productionDemand.priority;
	
	debug("production selection between algorithms\n\t%-10s: (%5.2f) %s\n\t%-10s: (%5.2f) %s\n\t%-10s: (%5.2f) %s\n", "vanilla", vanillaPriority, prod_name(choice), "thinker", thinkerPriority, prod_name(thinkerChoice), "wtp", wtpPriority, prod_name(wtpChoice));

	// select production based on priority

	if (wtpPriority >= thinkerPriority && wtpPriority >= vanillaPriority)
	{
		choice = wtpChoice;
	}
	else if (thinkerPriority >= vanillaPriority && thinkerPriority >= wtpPriority)
	{
		choice = thinkerChoice;
	}
	
	debug("===>    %-25s\n", prod_name(choice));

	debug("========================================\n\n");

	return choice;

}

int aiSuggestBaseProduction(int baseId, int choice)
{
	BASE *base = &(Bases[baseId]);

    // initialize productionDemand

    productionDemand.baseId = baseId;
    productionDemand.base = base;
    productionDemand.item = choice;
    productionDemand.priority = 0.0;

	evaluateFacilitiesDemand();

	evaluateLandImprovementDemand();

	evaluateLandExpansionDemand();
	evaluateOceanExpansionDemand();

	evaluatePoliceDemand();

	evaluateNativeDefenseDemand();
	evaluateTerritoryNativeProtectionDemand();
	
	evaluatePodPoppingDemand();

	evaluatePrototypingDemand();
	evaluateCombatDemand();

	return productionDemand.item;

}

void evaluateFacilitiesDemand()
{
	debug("evaluateFacilitiesDemand\n");

	evaluatePopulationLimitFacilitiesDemand();
	evaluatePsychFacilitiesDemand();
	evaluateMultiplyingFacilitiesDemand();
	evaluateDefensiveFacilitiesDemand();
	evaluateMilitaryFacilitiesDemand();

}

void evaluatePopulationLimitFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("\tevaluatePopulationLimitFacilitiesDemand\n");

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

		debug("\t\t\t%s\n", Facility[facilityId].name);

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

void evaluatePsychFacilitiesDemand()
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
		if (isUnitColony(base->queue_items[0]))
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

	debug("\tevaluatePsychFacilitiesDemand\n");

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

		debug("\t\t\t%s\n", Facility[facilityId].name);

		// add demand

		addProductionDemand(-facilityId, 1.0);

		break;

	}

}

void evaluateMultiplyingFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	Faction *faction = &(Factions[base->faction_id]);

	debug("\tevaluateMultiplyingFacilitiesDemand\n");

	// calculate base values for multiplication

	int mineralSurplus = base->mineral_surplus;
	double mineralIntake = (double)base->mineral_intake;
	double economyIntake = (double)base->energy_surplus * (double)(10 - faction->SE_alloc_psych - faction->SE_alloc_labs) / 10.0;
	double psychIntake = (double)base->energy_surplus * (double)(faction->SE_alloc_psych) / 10.0;
	double labsIntake = (double)base->energy_surplus * (double)(faction->SE_alloc_labs) / 10.0;

	debug("\tmineralSurplus=%d, mineralIntake=%f, economyIntake=%f, psychIntake=%f, labsIntake=%f\n", mineralSurplus, mineralIntake, economyIntake, psychIntake, labsIntake);

	// no production power - multiplying facilities are useless

	if (mineralSurplus <= 0)
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
		CFacility *facility = &(Facility[facilityId]);

		// check if base can build facility

		if (!isBaseFacilityAvailable(baseId, facilityId))
			continue;

		debug("\t\t%s\n", facility->name);

		// get facility parameters

		int cost = facility->cost;
		int mineralCost = getBaseMineralCost(baseId, -facilityId);
		int maintenance = facility->maint;
		debug("\t\t\tcost=%d, mineralCost=%d, maintenance=%d\n", cost, mineralCost, maintenance);

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

		// calculate construction time

		double constructionTime = (double)mineralCost / (double)(std::max(1, mineralSurplus));

		// calculate payoff time

		double payoffTime = (double)mineralCost / benefit;

		// calculate priority

		double priority = 1 / (constructionTime + payoffTime) - conf.ai_production_facility_priority_penalty;

		debug("\t\t\tconstructionTime=%f, payoffTime=%f, ai_production_facility_priority_penalty=%f, priority=%f\n", constructionTime, payoffTime, conf.ai_production_facility_priority_penalty, priority);

		// priority is too low

		if (priority <= 0)
			continue;

		// add demand

		addProductionDemand(-facilityId, priority);

	}

}

void evaluateDefensiveFacilitiesDemand()
{
	debug("\tevaluateDefensiveFacilitiesDemand\n");
	
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	bool ocean = isOceanRegion(baseTile->region);
	
	// select facility
	
	int facilityId = -1;
	
	if (ocean)
	{
		if (has_facility_tech(aiFactionId, FAC_NAVAL_YARD) && !has_facility(baseId, FAC_NAVAL_YARD))
		{
			facilityId = FAC_NAVAL_YARD;
		}
		else if (has_facility_tech(aiFactionId, FAC_AEROSPACE_COMPLEX) && !has_facility(baseId, FAC_AEROSPACE_COMPLEX))
		{
			facilityId = FAC_AEROSPACE_COMPLEX;
		}
		else if (has_facility_tech(aiFactionId, FAC_PERIMETER_DEFENSE) && !has_facility(baseId, FAC_PERIMETER_DEFENSE))
		{
			facilityId = FAC_PERIMETER_DEFENSE;
		}
		else if (has_facility_tech(aiFactionId, FAC_TACHYON_FIELD) && has_facility(baseId, FAC_PERIMETER_DEFENSE) && !has_facility(baseId, FAC_TACHYON_FIELD))
		{
			facilityId = FAC_TACHYON_FIELD;
		}
		
	}
	else
	{
		if (has_facility_tech(aiFactionId, FAC_PERIMETER_DEFENSE) && !has_facility(baseId, FAC_PERIMETER_DEFENSE))
		{
			facilityId = FAC_PERIMETER_DEFENSE;
		}
		else if (has_facility_tech(aiFactionId, FAC_AEROSPACE_COMPLEX) && !has_facility(baseId, FAC_AEROSPACE_COMPLEX))
		{
			facilityId = FAC_AEROSPACE_COMPLEX;
		}
		else if (has_facility_tech(aiFactionId, FAC_TACHYON_FIELD) && has_facility(baseId, FAC_PERIMETER_DEFENSE) && !has_facility(baseId, FAC_TACHYON_FIELD))
		{
			facilityId = FAC_TACHYON_FIELD;
		}
		
	}
	
	// nothing selected
	
	if (facilityId == -1)
		return;
	
	// calculate priority
	
	double priority = conf.ai_production_military_priority * activeFactionInfo.regionDefenseDemand[baseTile->region] * activeFactionInfo.baseStrategies[baseId].exposure;
	
	// add demand

	addProductionDemand(-facilityId, priority);

	debug("\t\tfacility=%-25s, ai_production_military_priority=%f, regionDefenseDemand=%f, exposure=%f, priority=%f\n", Facility[facilityId].name, conf.ai_production_military_priority, activeFactionInfo.regionDefenseDemand[baseTile->region], activeFactionInfo.baseStrategies[baseId].exposure, priority);
	
}

void evaluateMilitaryFacilitiesDemand()
{
	debug("\tevaluateMilitaryFacilitiesDemand\n");
	
	int baseId = productionDemand.baseId;
	BASE *base = &(Bases[baseId]);
	MAP *baseTile = getBaseMapTile(baseId);
	int region = baseTile->region;
	bool ocean = isOceanRegion(region);
	std::set<int> connectedOceanRegions = getBaseConnectedOceanRegions(baseId);
	
	// command center
	
	if (has_facility_tech(aiFactionId, FAC_COMMAND_CENTER) && !has_facility(baseId, FAC_COMMAND_CENTER) && !ocean)
	{
		int facilityId = FAC_COMMAND_CENTER;
		double priority = conf.ai_production_command_center_priority * base->mineral_surplus_final / activeFactionInfo.regionMaxMineralSurpluses[region];
		addProductionDemand(-facilityId, priority);
		debug("\t\tfacility=%-25s, ai_production_command_center_priority=%f, priority=%f\n", Facility[facilityId].name, conf.ai_production_command_center_priority, priority);
	}
	
	// naval yard
	
	if (has_facility_tech(aiFactionId, FAC_NAVAL_YARD) && !has_facility(baseId, FAC_NAVAL_YARD) && (ocean || connectedOceanRegions.size() >= 1))
	{
		int facilityId = FAC_NAVAL_YARD;
		double priority = conf.ai_production_naval_yard_priority * base->mineral_surplus_final / activeFactionInfo.regionMaxMineralSurpluses[region];
		addProductionDemand(-facilityId, priority);
		debug("\t\tfacility=%-25s, ai_production_naval_yard_priority=%f, priority=%f\n", Facility[facilityId].name, conf.ai_production_naval_yard_priority, priority);
	}
	
	// aerospace complex
	
	if (has_facility_tech(aiFactionId, FAC_AEROSPACE_COMPLEX) && !has_facility(baseId, FAC_AEROSPACE_COMPLEX))
	{
		int facilityId = FAC_AEROSPACE_COMPLEX;
		double priority = conf.ai_production_aerospace_complex_priority * base->mineral_surplus_final / activeFactionInfo.maxMineralSurplus;
		addProductionDemand(-facilityId, priority);
		debug("\t\tfacility=%-25s, ai_production_aerospace_complex_priority=%f, priority=%f\n", Facility[facilityId].name, conf.ai_production_aerospace_complex_priority, priority);
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

		debug("\t\tformerUnit=%-25s\n", Units[formerUnitId].name);

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

		double baseProductionAdjustedPriority = priority * (double)base->mineral_surplus / (double)(std::max(1, maxMineralSurplus));
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

	// verify base can build a colony

	if (!canBaseProduceColony(baseId))
	{
		debug("\t\t\t\tbase cannot build a colony\n");
		return;
	}

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

		globalUnpopulatedLandTileWeighedCount += std::min(1.0 , (double)conf.ai_production_max_unpopulated_range / (double)(nearestBaseRange * nearestBaseRange) );

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

	int existingLandColoniesCount = calculateUnitTypeCount(base->faction_id, WPN_COLONY_MODULE, TRIAD_LAND, baseId);
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

	debug("\t\t\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, infantryTurnsToDestination=%f, optimalColonyUnitId=%-25s\n", TRIAD_LAND, base->mineral_surplus, infantryTurnsToDestination, Units[optimalColonyUnitId].name);

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

	// base cannot build ships

	if (!isBaseCanBuildShips(baseId))
		return;

	debug("evaluateOceanExpansionDemand\n");

	// verify base can build a colony

	if (!canBaseProduceColony(baseId))
	{
		debug("\t\t\t\tbase cannot build a colony\n");
		return;
	}

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

			oceanRegionUnpopulatedLandTileWeighedCount += std::min(1.0 , (double)conf.ai_production_max_unpopulated_range / (double)nearestBaseRange );

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

		int existingSeaColoniesCount = calculateUnitTypeCount(base->faction_id, WPN_COLONY_MODULE, TRIAD_SEA, baseId);
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

		debug("\t\t\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, infantryTurnsToDestination=%f, optimalOceanRegionColonyUnitId=%-25s\n", TRIAD_SEA, base->mineral_surplus, infantryTurnsToDestination, Units[optimalOceanRegionColonyUnitId].name);

		// adjust priority based on base size

		double baseAdjustedOceanRegionPriority = oceanRegionPriority * ((double)base->pop_size / (double)maxBaseSize);

		debug("\t\t\t\tpop_size=%d, baseAdjustedOceanRegionPriority=%f\n", base->pop_size, baseAdjustedOceanRegionPriority);

		// set base demand

		addProductionDemand(optimalOceanRegionColonyUnitId, baseAdjustedOceanRegionPriority);

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
		debug("\tpoliceUnitId=%s, priority=%f\n", Units[policeUnitId].name, priority);
		addProductionDemand(policeUnitId, priority);
	}

}

/*
Evaluates need for bases protection against natives.
*/
void evaluateNativeDefenseDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	
	debug("evaluateNativeDefenseDemand\n");
	
	// get base native protection demand
	
	if (activeFactionInfo.baseAnticipatedNativeAttackStrengths.count(baseId) == 0 || activeFactionInfo.baseRemainingNativeProtectionDemands.count(baseId) == 0)
		return;
	
	double baseAnticipatedNativeAttackStrength = activeFactionInfo.baseAnticipatedNativeAttackStrengths[baseId];
	double baseRemainingNativeProtectionDemand = activeFactionInfo.baseRemainingNativeProtectionDemands[baseId];
	
	debug("\tbaseAnticipatedNativeAttackStrength=%f, baseRemainingNativeProtectionDemand=%f\n", baseAnticipatedNativeAttackStrength, baseRemainingNativeProtectionDemand);
	
	if (baseAnticipatedNativeAttackStrength <= 0.0 || baseRemainingNativeProtectionDemand <= 0.0)
		return;

	// calculate priority

	double priority = baseRemainingNativeProtectionDemand / baseAnticipatedNativeAttackStrength;

	debug("\tpriority=%f\n", priority);

	if (priority > 0.0)
	{
		int item = findStrongestNativeDefensePrototype(base->faction_id);
		debug("\tprotectionUnit=%-25s\n", Units[item].name);

		addProductionDemand(item, priority);
		
	}

}

/*
Evaluates need for patrolling natives.
*/
void evaluateTerritoryNativeProtectionDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluateTerritoryNativeProtectionDemand\n");

	// get base connected regions

	std::set<int> baseConnectedRegions = getBaseConnectedRegions(baseId);

	for (int region : baseConnectedRegions)
	{
		bool ocean = isOceanRegion(region);

		debug("\tregion=%d, type=%s\n", region, (ocean ? "ocean" : "land"));

		// find native attack unit

		int nativeAttackUnitId = findNativeAttackPrototype(ocean);

		if (nativeAttackUnitId == -1)
		{
			debug("\t\tno native attack unit\n");
			continue;
		}

		debug("\t\tnativeAttackUnit=%-25s\n", Units[nativeAttackUnitId].name);

		// calculate natives defense value

		double nativePsiDefenseValue = 0.0;

		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			if (vehicle->faction_id == 0 && vehicleTile->region == region && vehicleTile->owner == aiFactionId)
			{
				nativePsiDefenseValue += getVehiclePsiDefenseValue(vehicleId);
			}
			
		}

		debug("\t\tnativePsiDefenseValue=%f\n", nativePsiDefenseValue);

		// none

		if (nativePsiDefenseValue == 0.0)
			continue;

		// summarize existing attack value

		double existingPsiAttackValue = 0.0;

		for (int vehicleId : activeFactionInfo.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// exclude wrong triad
			
			if (vehicle->triad() == (ocean ? TRIAD_LAND : TRIAD_AIR))
				continue;

			// exclude wrong region
			
			if (!(vehicle->triad() == TRIAD_AIR || vehicleTile->region == region))
				continue;

			// only units with hand weapon and no armor

			if (!(Units[vehicle->unit_id].weapon_type == WPN_HAND_WEAPONS || Units[vehicle->unit_id].armor_type == ARM_NO_ARMOR))
				continue;

			existingPsiAttackValue += getVehiclePsiAttackStrength(vehicleId);

		}

		debug("\t\texistingPsiAttackValue=%f\n", existingPsiAttackValue);

		// calculate need for psi attackers

		double nativeAttackTotalDemand = (double)nativePsiDefenseValue;
		double nativeAttackRemainingDemand = nativeAttackTotalDemand - (double)existingPsiAttackValue;

		debug("\t\tnativeAttackTotalDemand=%f, nativeAttackRemainingDemand=%f\n", nativeAttackTotalDemand, nativeAttackRemainingDemand);

		// we have enough

		if (nativeAttackRemainingDemand <= 0.0)
			continue;

		// otherwise, set priority

		double priority = nativeAttackRemainingDemand / nativeAttackTotalDemand;

		debug("\t\tpriority=%f\n", priority);

		// find max mineral surplus in region

		int maxMineralSurplus = getRegionBasesMaxMineralSurplus(base->faction_id, region);
		debug("\t\tmaxMineralSurplus=%d\n", maxMineralSurplus);

		// set demand based on mineral surplus

		double baseProductionAdjustedPriority = priority * (double)base->mineral_surplus / (double)maxMineralSurplus;

		debug("\t\t\t%-25s mineral_surplus=%d, baseProductionAdjustedPriority=%f\n", base->name, base->mineral_surplus, baseProductionAdjustedPriority);

		addProductionDemand(nativeAttackUnitId, baseProductionAdjustedPriority);

	}

}

void evaluatePodPoppingDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluatePodPoppingDemand\n");

	// get base connected regions

	std::set<int> baseConnectedRegions = getBaseConnectedRegions(baseId);

	for (int region : baseConnectedRegions)
	{
		bool ocean = isOceanRegion(region);

		debug("\tregion=%d, type=%s\n", region, (ocean ? "ocean" : "land"));

		// count pods
		
		int regionPodCount = getRegionPodCount(region);
		
		// none

		if (regionPodCount == 0)
			continue;

		// count scouts
		
		int scoutCount = 0;
		
		for (int vehicleId : activeFactionInfo.combatVehicleIds)
		{
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			if (isVehicleLandUnitOnTransport(vehicleId))
				continue;
			
			if (vehicleTile->region != region)
				continue;
			
			if (isScoutVehicle(vehicleId))
			{
				scoutCount++;
			}
			
		}
		
		// calculate need for pod poppers

		double podPoppersNeeded = (double)regionPodCount / 5.0;
		double podPoppingDemand = (podPoppersNeeded - scoutCount) / podPoppersNeeded;

		debug("\t\tregionPodCount=%d, scoutCount=%d, podPoppingDemand=%f\n", regionPodCount, scoutCount, podPoppingDemand);

		// we have enough

		if (podPoppingDemand <= 0.0)
			continue;

		// otherwise, set priority

		double priority = podPoppingDemand;

		debug("\t\tpriority=%f\n", priority);

		// find max mineral surplus in region

		int maxMineralSurplus = getRegionBasesMaxMineralSurplus(base->faction_id, region);
		debug("\t\tmaxMineralSurplus=%d\n", maxMineralSurplus);

		// set demand based on mineral surplus

		double baseProductionAdjustedPriority = priority * (double)base->mineral_surplus / (double)maxMineralSurplus;

		debug("\t\t\t%-25s mineral_surplus=%d, baseProductionAdjustedPriority=%f\n", base->name, base->mineral_surplus, baseProductionAdjustedPriority);
		
		// select pod popper
		
		int podPopperUnitId = findScoutUnit(aiFactionId, ocean ? TRIAD_SEA : TRIAD_LAND);
		
		if (podPopperUnitId < 0)
			continue;
		
		// add production demand
		
		addProductionDemand(podPopperUnitId, baseProductionAdjustedPriority);

	}

}

void evaluatePrototypingDemand()
{
	debug("evaluatePrototypingDemand - %s\n", MFactions[aiFactionId].noun_faction);
	
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	bool inSharedOceanRegion = activeFactionInfo.baseStrategies[baseId].inSharedOceanRegion;
	
	// find cheapest unprototyped unit
	
	int unprototypedUnitId = -1;
	int unprototypedUnitCost = INT_MAX;
	
	for (int unitId : activeFactionInfo.prototypes)
	{
		UNIT *unit = &(Units[unitId]);
		
		// skip sea units for bases without shared ocean region
		
		if (unit->triad() == TRIAD_SEA && !inSharedOceanRegion)
			continue;
		
		// find unprototyped
		
		if (unitId >= 64 && !(unit->unit_flags & UNIT_PROTOTYPED))
		{
			int cost = Units[unitId].cost;
			
			if (cost < unprototypedUnitCost)
			{
				unprototypedUnitId = unitId;
				unprototypedUnitCost = cost;
			}
			
		}
		
	}
	
	if (unprototypedUnitId == -1)
	{
		debug("\tno unprototyped\n");
		return;
	}
	
	// get unit
	
	UNIT *unprototypedUnit = &(Units[unprototypedUnitId]);
	
	debug("\tunprototypedUnit=%s\n", unprototypedUnit->name);
	
	// calculate priority
	
	double priority = conf.ai_production_prototyping_priority;
	
	// calculate adjusted priority based on mineral surplus
	
	double adjustedPriority = priority * ((double)base->mineral_surplus_final / (double)activeFactionInfo.maxMineralSurplus);
	
	debug("\tpriority=%f, adjustedPriority=%f", priority, adjustedPriority);
	
	// add production demand
	
	addProductionDemand(unprototypedUnitId, adjustedPriority);

}

void evaluateCombatDemand()
{
//	debug("evaluateCombatDemand\n");
//
//	int baseId = productionDemand.baseId;
//	BASE *base = productionDemand.base;
//	
//	
}

void addProductionDemand(int item, double priority)
{
	BASE *base = productionDemand.base;

	// prechecks

	// do not exhaust weak base support

	if (base->mineral_consumption > 0 && item >= 0 && isUnitCombat(item))
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
    UNIT *bestUnit = &(Units[bestUnitId]);

    for (int i = 0; i < 128; i++)
	{
        int id = (i < 64 ? i : (factionId - 1) * 64 + i);

        UNIT *unit = &Units[id];

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

int findNativeAttackPrototype(bool ocean)
{
	// initial prototype

    int bestUnitId = -1;
    UNIT *bestUnit = NULL;

    for (int unitId : activeFactionInfo.prototypes)
	{
		UNIT *unit = &(Units[unitId]);
		
		// skip wrong triad

		if (!(unit->triad() == (ocean ? TRIAD_SEA : TRIAD_LAND)))
			continue;

		// only with no weapon and armor
		
		if (!isScoutUnit(unitId))
			continue;
		
		if
		(
			// initial assignment
			(bestUnitId == -1)
			||
			// prefer empath
			(!unit_has_ability(bestUnitId, ABL_EMPATH) && unit_has_ability(unitId, ABL_EMPATH))
			||
			// prefer faster
			(unit->speed() > bestUnit->speed())
			||
			// prefer cheaper
			(unit->cost < bestUnit->cost)
		)
		{
			bestUnitId = unitId;
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

		if (!isUnitCombat(unitId))
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
		UNIT *unit = &(Units[unitId]);

		// skip non combat units

		if (!isUnitCombat(unitId))
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
		UNIT *unit = &(Units[unitId]);

		// skip non combat units

		if (!isUnitCombat(unitId))
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

		if (!isUnitColony(unitId))
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
		UNIT *unit = &(Units[unitId]);

		// given triad only

		if (unit_triad(unitId) != triad)
			continue;

		// colony only

		if (!isUnitColony(unitId))
			continue;

		// calculate total turns

		double totalTurns = (double)(cost_factor(aiFactionId, 1, -1) * unit->cost) / (double)mineralSurplus + infantryTurnsToDestination / (double)unit_chassis_speed(unitId);

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
bool canBaseProduceColony(int baseId)
{
	BASE *base = &(Bases[baseId]);

	// do not produce colony if insufficient population

	if (base->pop_size < 2)
		return false;

	// do not produce colony in low productive bases

	if (base->mineral_surplus < 2)
		return false;

	// true otherwise

	return true;

}

int findFormerUnit(int factionId, int triad)
{
	int bestUnitId = -1;
	double bestUnitValue = -1;

	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(Units[unitId]);

		// formers only

		if (!isUnitFormer(unitId))
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
		BASE *base = &(Bases[baseId]);

		maxMineralSurplus = std::max(maxMineralSurplus, base->mineral_surplus);

	}

	return maxMineralSurplus;

}

int getRegionBasesMaxPopulationSize(int factionId, int region)
{
	int maxPopulationSize = 0;

	for (int baseId : getRegionBases(factionId, region))
	{
		BASE *base = &(Bases[baseId]);

		maxPopulationSize = std::max(maxPopulationSize, (int)base->pop_size);

	}

	return maxPopulationSize;

}

int getMaxBaseSize(int factionId)
{
	int maxBaseSize = 0;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		if (base->faction_id != factionId)
			continue;

		maxBaseSize = std::max(maxBaseSize, (int)base->pop_size);

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
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (Units[vehicle->unit_id].weapon_type != weaponType)
			continue;

		if (vehicle->triad() != triad)
			continue;

		regionUnitTypeCount++;

	}

	for (int baseId : getRegionBases(factionId, region))
	{
		BASE *base = &(Bases[baseId]);
		int item = base->queue_items[0];

		if (item < 0)
			continue;

		if (Units[item].weapon_type != weaponType)
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
int calculateUnitTypeCount(int factionId, int weaponType, int triad, int excludedBaseId)
{
	int unitTypeCount = 0;

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (vehicle->faction_id != factionId)
			continue;

		if (Units[vehicle->unit_id].weapon_type != weaponType)
			continue;

		if (vehicle->triad() != triad)
			continue;

		unitTypeCount++;

	}

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		// exclude this base

		if (baseId == excludedBaseId)
			continue;

		BASE *base = &(Bases[baseId]);
		int item = base->queue_items[0];

		if (base->faction_id != factionId)
			continue;

		if (item < 0)
			continue;

		if (Units[item].weapon_type != weaponType)
			continue;

		if (unit_triad(item) != triad)
			continue;

		unitTypeCount++;

	}

	return unitTypeCount;

}

bool isBaseNeedPsych(int baseId)
{
	BASE *base = &(Bases[baseId]);

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
		UNIT *unit = &(Units[unitId]);

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
	BASE *base = &(Bases[baseId]);

	// execute original code

	tx_base_first(baseId);

	// override default choice with WTP choice for enabled computer factions
	
	// execute original code for not enabled computer faction bases

	if (!is_human(base->faction_id) && ai_enabled(base->faction_id) && conf.ai_useWTPAlgorithms)
	{
		base->queue_items[0] = modifiedBaseProductionChoice(baseId, 0, 0, 0);
	}

}

/*
Checks for military purpose unit/structure.
*/
bool isMilitaryItem(int item)
{
	if (item >= 0)
	{
		UNIT *unit = &(Units[item]);
		int weaponId = unit->weapon_type;
		
		return
			(isUnitCombat(item) && !isScoutUnit(item))
			||
			weaponId == WPN_PROBE_TEAM
			||
			weaponId == WPN_CONVENTIONAL_PAYLOAD
			||
			weaponId == WPN_PLANET_BUSTER
		;

	}
	else
	{
		return false;
	}
	
}

/*
Slightly updated Thinker function with excluded side effects (hurry).
*/
int thinker_base_prod_choice(int id, int v1, int v2, int v3) {
    assert(id >= 0 && id < *total_num_bases);
    BASE* base = &Bases[id];
    int prod = base->queue_items[0];
    int faction = base->faction_id;
    int choice = 0;
    print_base(id);

    if (is_human(faction)) {
        debug("skipping human base\n");
        choice = base_prod_choices(id, v1, v2, v3);
    } else if (!ai_enabled(faction)) {
        debug("skipping computer base\n");
        choice = base_prod_choices(id, v1, v2, v3);
    } else {
        set_base(id);
        base_compute(1);
        if ((choice = need_psych(id)) != 0 && choice != prod) {
            debug("BUILD PSYCH\n");
        } else if (base->status_flags & BASE_PRODUCTION_DONE) {
            choice = select_production(id);
            base->status_flags &= ~BASE_PRODUCTION_DONE;
        } else if (prod >= 0 && !can_build_unit(faction, prod)) {
            debug("BUILD FACILITY\n");
            choice = find_facility(id);
        } else if (prod < 0 && !can_build(id, abs(prod))) {
            debug("BUILD CHANGE\n");
            if (base->minerals_accumulated > Rules->retool_exemption) {
                choice = find_facility(id);
            } else {
                choice = select_production(id);
            }
        } else if (need_defense(id)) {
            debug("BUILD DEFENSE\n");
            choice = find_proto(id, TRIAD_LAND, COMBAT, DEF);
        } else {
        	// don't hurry
//            consider_hurry(id);
//            debug("BUILD OLD\n");
//            choice = prod;
        }
        debug("choice: %d %s\n", choice, prod_name(choice));
    }
    fflush(debug_log);
    return choice;
}

void evaluateDefenseDemand()
{
	debug("evaluateDefenseDemand - %s\n", MFactions[aiFactionId].noun_faction);
	
	// evaluate region defense demands
	
	for (int region : activeFactionInfo.presenceRegions)
	{
		bool ocean = isOceanRegion(region);
		
		debug("\tregion=%3d, ocean=%d\n", region, ocean);
		
		// evaluate own military strength in region
		
		debug("\t\townStrengths\n");
		
		MILITARY_STRENGTH ownStrength;
		
		for (int vehicleId : activeFactionInfo.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			UNIT *unit = &(Units[vehicle->unit_id]);
			bool regularUnit = !(Weapon[unit->weapon_type].offense_value < 0 || Armor[unit->armor_type].defense_value < 0);
			
			// exclude vehicle not in region
			
			if (!isVehicleInRegion(vehicleId, region))
				continue;
			
			// vehicle value
			
			double psiOffenseValue = getVehiclePsiOffenseValue(vehicleId);
			double psiDefenseValue = getVehiclePsiDefenseValue(vehicleId);
			double conventionalOffenseValue = 0.0;
			double conventionalDefenseValue = 0.0;
			
			if (regularUnit)
			{
				conventionalOffenseValue = getVehicleConventionalOffenseValue(vehicleId);
				conventionalDefenseValue = getVehicleConventionalDefenseValue(vehicleId);
			}
			
			// check vehicles in opposite realm
			
			if (vehicle->triad() == (is_ocean(vehicleTile) ? TRIAD_LAND : TRIAD_SEA))
			{
				if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
				{
					// vehicle in opposite realm at base can defend only and cannot attack
					
					psiOffenseValue = 0.0;
					conventionalOffenseValue = 0.0;
					
				}
				else
				{
					// vehicle in opposite realm and not at base is assumed to be transported and does not contribute to this realm defense
					
					continue;
					
				}
				
			}
			
			debug("\t\t\t\t(%3d,%3d) %-25s: psiOff=%f, psiDef=%f, conOff=%f, conDef=%f\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name, psiOffenseValue, psiDefenseValue, conventionalOffenseValue, conventionalDefenseValue);
			
			// add to region strength
			
			ownStrength.totalUnitCount++;
			ownStrength.psiStrength += evaluateCombatStrength(psiOffenseValue, psiDefenseValue);
			if (regularUnit)
			{
				ownStrength.regularUnitCount++;
				ownStrength.conventionalStrength += evaluateCombatStrength(conventionalOffenseValue, conventionalDefenseValue);
			}
			
		}
		debug("\t\t\t%-40s = %7d\n", "totalUnitCount", ownStrength.totalUnitCount);
		debug("\t\t\t%-40s = %7d\n", "regularUnitCount", ownStrength.regularUnitCount);
		debug("\t\t\t%-40s = %7.2f\n", "psiStrength", ownStrength.psiStrength);
		debug("\t\t\t%-40s = %7.2f\n", "conventionalStrength", ownStrength.conventionalStrength);
		
		// evaluate opponents military strength in region
		
		debug("\t\topponentStrengths\n");
		
		MILITARY_STRENGTH opponentStrength;
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			UNIT *unit = &(Units[vehicle->unit_id]);
			bool regularUnit = !(Weapon[unit->weapon_type].offense_value < 0 || Armor[unit->armor_type].defense_value < 0);
			
			// not alien
			
			if (vehicle->faction_id == 0)
				continue;
			
			// not own
			
			if (vehicle->faction_id == aiFactionId)
				continue;
			
			// combat only
			
			if (!isVehicleCombat(vehicleId))
				continue;
			
			// exclude vehicles of opposite region realm
			
			if (vehicle->triad() == (ocean ? TRIAD_LAND : TRIAD_SEA))
				continue;
			
			// exclude sea units in different region - they cannot possibly reach us
			
			if (vehicle->triad() == TRIAD_SEA && vehicleTile->region != region)
				continue;
			
			// vehicle value
			
			double psiOffenseValue = getVehiclePsiOffenseValue(vehicleId);
			double psiDefenseValue = getVehiclePsiDefenseValue(vehicleId);
			double conventionalOffenseValue = 0.0;
			double conventionalDefenseValue = 0.0;
			
			if (regularUnit)
			{
				conventionalOffenseValue = getVehicleConventionalOffenseValue(vehicleId);
				conventionalDefenseValue = getVehicleConventionalDefenseValue(vehicleId);
			}
			
			// get nearest base
			
			int nearestRegionBaseId = getNearestBaseId(vehicle->x, vehicle->y, activeFactionInfo.regionBaseIds[region]);
			
			if (nearestRegionBaseId < 0)
				continue;
			
			BASE *nearestRegionBase = &(Bases[nearestRegionBaseId]);
			
			// reduce attack and defense based on base bonuses
			
			psiOffenseValue /= activeFactionInfo.baseStrategies[nearestRegionBaseId].psiDefenseMultiplier;
			conventionalOffenseValue /= activeFactionInfo.baseStrategies[nearestRegionBaseId].conventionalDefenseMultipliers[vehicle->triad()];
			
			double sensorCombatStrengthMultiplier = (1.0 + (double)Rules->combat_defend_sensor / 100.0);
			if (activeFactionInfo.baseStrategies[nearestRegionBaseId].withinFriendlySensorRange)
			{
				psiOffenseValue /= sensorCombatStrengthMultiplier;
				conventionalOffenseValue /= sensorCombatStrengthMultiplier;
				if (conf.combat_bonus_sensor_offense)
				{
					psiDefenseValue /= sensorCombatStrengthMultiplier;
					conventionalDefenseValue /= sensorCombatStrengthMultiplier;
				}
			}
			
			// compute strength multiplier based on diplomatic relations
			
			double strengthMultiplier = activeFactionInfo.factionInfos[vehicle->faction_id].threatKoefficient;
			
			// apply region penalty for land units in other region and not yet transported - it is difficult for them to reach us
			
			if (vehicle->triad() == TRIAD_LAND && vehicleTile->region != region && !isVehicleLandUnitOnTransport(vehicleId))
			{
				strengthMultiplier /= 5.0;
			}
			
			// reduce strength based on time to reach the base
			
			int nearestRegionBaseRange = map_range(vehicle->x, vehicle->y, nearestRegionBase->x, nearestRegionBase->y);
			double timeToReachTheBase = (double)nearestRegionBaseRange / getVehicleSpeedWithoutRoads(vehicleId);
			strengthMultiplier /= std::max(1.0, timeToReachTheBase / 5.0);
			
			debug("\t\t\t\t(%3d,%3d) %-25s -> (%3d,%3d) %-25s: psiOffenseValue=%f, psiDefenseValue=%f, conventionalOffenseValue=%f, conventionalDefenseValue=%f, strengthMultiplier=%f\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name, nearestRegionBase->x, nearestRegionBase->y, nearestRegionBase->name, psiOffenseValue, psiDefenseValue, conventionalOffenseValue, conventionalDefenseValue, strengthMultiplier);
			
			// add to region strength
			
			opponentStrength.totalUnitCount++;
			opponentStrength.psiStrength += strengthMultiplier * evaluateCombatStrength(psiOffenseValue, psiDefenseValue);
			if (regularUnit)
			{
				opponentStrength.regularUnitCount++;
				opponentStrength.conventionalStrength += strengthMultiplier * evaluateCombatStrength(conventionalOffenseValue, conventionalDefenseValue);
			}
			
		}
		debug("\t\t\t%-40s = %7d\n", "totalUnitCount", opponentStrength.totalUnitCount);
		debug("\t\t\t%-40s = %7d\n", "regularUnitCount", opponentStrength.regularUnitCount);
		debug("\t\t\t%-40s = %7.2f\n", "psiStrength", opponentStrength.psiStrength);
		debug("\t\t\t%-40s = %7.2f\n", "conventionalStrength", opponentStrength.conventionalStrength);
		
		// compute region defense demands
		
		double defenseDemand;
		
		if (opponentStrength.totalUnitCount == 0)
		{
			defenseDemand = 0.0;
		}
		else if (ownStrength.totalUnitCount == 0)
		{
			defenseDemand = 1.0;
		}
		else
		{
			double conventionalCombatPortion =
				((double)ownStrength.regularUnitCount / (double)ownStrength.totalUnitCount)
				*
				((double)opponentStrength.regularUnitCount / (double)opponentStrength.totalUnitCount)
			;
			double psiCombatPortion = 1.0 - conventionalCombatPortion;
			
			double psiStrengthRatio = (opponentStrength.psiStrength == 0.0 ? 1.0 : ownStrength.psiStrength / opponentStrength.psiStrength);
			double conventionalStrengthRatio = (opponentStrength.conventionalStrength == 0.0 ? 1.0 : ownStrength.conventionalStrength / opponentStrength.conventionalStrength);
			double weightedStrengthRatio = psiCombatPortion * psiStrengthRatio + conventionalCombatPortion * conventionalStrengthRatio;
			
			debug("\t\t%-30s = %7.2f\n", "psiCombatPortion", psiCombatPortion);
			debug("\t\t%-30s = %7.2f\n", "conventionalCombatPortion", conventionalCombatPortion);
			debug("\t\t%-30s = %7.2f\n", "psiStrengthRatio", psiStrengthRatio);
			debug("\t\t%-30s = %7.2f\n", "conventionalStrengthRatio", conventionalStrengthRatio);
			debug("\t\t%-30s = %7.2f\n", "weightedStrengthRatio", weightedStrengthRatio);
			
			defenseDemand = 1.0 - weightedStrengthRatio;
			
		}
		
		debug("\t\t%-20s = %7.2f\n", "defenseDemand", defenseDemand);
		
		activeFactionInfo.regionDefenseDemand[region] = defenseDemand;
		
	}
	
	// find average defense demand across all regions
	
	activeFactionInfo.defenseDemand = 0.0;
	
	for (const auto &kv : activeFactionInfo.regionDefenseDemand)
	{
		activeFactionInfo.defenseDemand += kv.second;
	}
	
	activeFactionInfo.defenseDemand /= activeFactionInfo.regionDefenseDemand.size();
	
}

