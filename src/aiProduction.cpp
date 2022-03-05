#include <set>
#include <set>
#include <map>
#include <map>
#include "engine.h"
#include "game.h"
#include "game_wtp.h"
#include "terranx_wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiProduction.h"
#include "aiMoveColony.h"

const std::set<int> MANAGED_FACILITIES
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
		FAC_COMMAND_CENTER,
		FAC_NAVAL_YARD,
		FAC_AEROSPACE_COMPLEX,
	}
;

// combat priority used globally

double globalMilitaryPriority;
double regionMilitaryPriority;

// global parameters

double hurryCost;
double globalBaseDemand;
double baseColonyDemandMultiplier;

// currently processing base production demand

PRODUCTION_DEMAND productionDemand;

/*
Prepare production choices.
*/
void productionStrategy()
{
	
	setupProductionVariables();
	
	// evaluate global demands

	evaluateGlobalBaseDemand();
	
}

void setupProductionVariables()
{
	// set energy to mineral conversion ratio
	
	int hurryCostUnit = (conf.flat_hurry_cost ? conf.flat_hurry_cost_multiplier_unit : 4);
	int hurryCostFacility = (conf.flat_hurry_cost ? conf.flat_hurry_cost_multiplier_facility : 2);

	hurryCost = ((double)hurryCostUnit + (double)hurryCostFacility) / 2.0;
	
}

/*
Evaluates need for new bases based on current inefficiency.
*/
void evaluateGlobalBaseDemand()
{
	debug("evaluateGlobalBaseDemand - %s\n", MFactions[aiFactionId].noun_faction);
	
	Faction *faction = &(Factions[aiFactionId]);
	
	// set default global base demand multiplier
	
	globalBaseDemand = 1.0;
	
	// --------------------------------------------------
	// inefficiency
	// --------------------------------------------------
	
	// collect statistics on inefficiency and estimate on b-drone spent
	// energy intake
	// energy inefficiency
	// energy spending per drone
	
	double totalEnergyIntake = 0.0;
	double totalEnergyInefficiency = 0.0;
	double totalDroneEnergySpending = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		// summarize energy intake
		
		totalEnergyIntake += (double)base->energy_intake;
		
		// summarize energy inefficiency
		
		totalEnergyInefficiency += (double)base->energy_inefficiency;
		
		// calculate psych multiplier
		
		double psychMultiplier = 1.0;
		
		if (has_facility(baseId, FAC_HOLOGRAM_THEATRE))
		{
			psychMultiplier += 0.5;
		}
		
		if (has_facility(baseId, FAC_TREE_FARM))
		{
			psychMultiplier += 0.5;
		}
		
		if (has_facility(baseId, FAC_HYBRID_FOREST))
		{
			psychMultiplier += 0.5;
		}
		
		if (has_facility(baseId, FAC_RESEARCH_HOSPITAL))
		{
			psychMultiplier += 0.25;
		}
		
		if (has_facility(baseId, FAC_NANOHOSPITAL))
		{
			psychMultiplier += 0.25;
		}
		
		// estimate energy spending per drone
		
		double droneEnergySpending = 2.0 / psychMultiplier;
		
		// summarize drone spending
		
		totalDroneEnergySpending += droneEnergySpending;
		
	}
	
	// calculate average b-drones per base
	
	double baseLimit = (double)(8 - *diff_level) * (double)(4 + std::max(0, faction->SE_effic_pending)) * ((double)(*map_area_sq_root) / sqrt(3200.0)) / 2.0;
	double bDronesPerBase = std::max(0.0, ((double)aiData.baseIds.size() / baseLimit) - 1.0);
	
	// proportional inefficiency
	
	double inefficiencyDemandMultiplier;
	
	if (totalEnergyIntake <= 0.0)
	{
		inefficiencyDemandMultiplier = 0.0;
	}
	else
	{
		inefficiencyDemandMultiplier = std::max(0.0, 1.0 - (totalEnergyInefficiency + totalDroneEnergySpending * bDronesPerBase) / totalEnergyIntake);
	}
	
	debug("\ttotalEnergyIntake            %6.2f\n", totalEnergyIntake);
	debug("\ttotalEnergyInefficiency      %6.2f\n", totalEnergyInefficiency);
	debug("\ttotalDroneEnergySpending     %6.2f\n", totalDroneEnergySpending);
	debug("\tbaseLimit                    %6.2f\n", baseLimit);
	debug("\tbDronesPerBase               %6.2f\n", bDronesPerBase);
	debug("\tinefficiencyDemandMultiplier %6.2f\n", inefficiencyDemandMultiplier);
	
	// reduce colony demand based on energy loss
	
	globalBaseDemand *= inefficiencyDemandMultiplier;
	debug("\tglobalBaseDemand=%f\n", globalBaseDemand);
	
	// --------------------------------------------------
	// existing colony production
	// --------------------------------------------------
	
	// calculate existing colonies
	
	int existingColoniesCount = 0.0;
	
	for (int vehicleId : aiData.vehicleIds)
	{
		// colony
		
		if (!isColonyVehicle(vehicleId))
			continue;
		
		existingColoniesCount++;
		
	}
	
	debug("\texistingColoniesCount=%d\n", existingColoniesCount);
	
	// calculate support demand multiplier
	
	double supportDemandMultiplier;
	
	if (aiData.baseIds.size() == 0)
	{
		supportDemandMultiplier = 0.0;
	}
	else
	{
		supportDemandMultiplier = (double)std::max(0, (int)aiData.baseIds.size() - existingColoniesCount) / (double)aiData.baseIds.size();
	}
	
	// reduce demand based on existing colonies to reduce support burden
	
	globalBaseDemand *= supportDemandMultiplier;
	debug("\tglobalBaseDemand=%f\n", globalBaseDemand);
	
}

/*
Sets faction all bases production.
*/
void setProduction()
{
    debug("\nsetProduction - %s\n\n", MFactions[*active_faction].noun_faction);
    
    for (int baseId : aiData.baseIds)
	{
		BASE* base = &(Bases[baseId]);
		
		debug("setProduction - %s\n", base->name);

		// reset base specific variables
		
		baseColonyDemandMultiplier = 1.0;
		
		// current production choice

		int choice = base->queue_items[0];
		
		debug("(%s)\n", prod_name(choice));

		// compute military priority
		
		globalMilitaryPriority = std::max(conf.ai_production_military_priority_minimal, conf.ai_production_military_priority * aiData.mostVulnerableBaseDefenseDemand);
		
		debug("globalMilitaryPriority = %f\n", globalMilitaryPriority);
		
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
			// calculate production turns
			
			double productionTurns = (double)(getBaseMineralCost(baseId, base->queue_items[0]) - base->minerals_accumulated) / (double)base->mineral_surplus;
			
			// calculate project priority
			
			vanillaPriority = conf.ai_production_project_build_time / productionTurns;
			
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
			debug("\t\tvanilla choice is military item\n");
			
			double militaryPriority = (choice >= 0 && Units[choice].triad() == TRIAD_AIR ? globalMilitaryPriority : regionMilitaryPriority);
			
			// raise vanilla priority to military priority
			
			if (militaryPriority > vanillaPriority)
			{
				vanillaPriority = militaryPriority;
			}
			
		}
		
		// drop priority for impossible colony
		
		if (choice >= 0 && isColonyUnit(choice) && !canBaseProduceColony(baseId))
		{
			vanillaPriority = 0.0;
		}
		
		// WTP

		int wtpChoice = aiSuggestBaseProduction(baseId, choice);

		// get wtp priority
		
		double wtpPriority = productionDemand.priority;
		
		debug("production selection\n\t%-10s(%5.2f) %s\n\t%-10s(%5.2f) %s\n", "vanilla", vanillaPriority, prod_name(choice), "wtp", wtpPriority, prod_name(wtpChoice));

		// select production based on priority

		if (wtpPriority >= vanillaPriority)
		{
			choice = wtpChoice;
		}
		
		debug("=> %-25s\n", prod_name(choice));
		
		// set base production
		
		base_prod_change(baseId, choice);
		
		debug("\n");

	}

}

int aiSuggestBaseProduction(int baseId, int choice)
{
	BASE *base = &(Bases[baseId]);

    // initialize productionDemand

    productionDemand.baseId = baseId;
    productionDemand.base = base;
    productionDemand.item = choice;
    productionDemand.priority = 0.0;
    
	// ignore new bases
	
	if (aiData.baseStrategies.count(baseId) == 0)
		return productionDemand.item;
	
	// evaluate other demands
	
	evaluateFacilitiesDemand();

	evaluatePoliceDemand();

	evaluateLandFormerDemand();
	evaluateSeaFormerDemand();

	evaluateLandColonyDemand();
	evaluateSeaColonyDemand();

	evaluateAlienDefenseDemand();
	evaluateAlienHuntingDemand();
	
	evaluatePodPoppingDemand();

	evaluatePrototypingDemand();
	evaluateCombatDemand();
	
	evaluateTransportDemand();

	return productionDemand.item;

}

void evaluateFacilitiesDemand()
{
	debug("evaluateFacilitiesDemand\n");

	evaluatePopulationLimitFacilitiesDemand();
	evaluatePsychFacilitiesDemand();
	evaluateEcoDamageFacilitiesDemand();
	evaluateMultiplyingFacilitiesDemand();
	evaluateDefensiveFacilitiesDemand();
	evaluateMilitaryFacilitiesDemand();

}

void evaluatePopulationLimitFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("\tevaluatePopulationLimitFacilitiesDemand\n");
	
	// determine next facility to build
	
	int facilityId = getFirstUnbuiltFacilityFromList(baseId, {FAC_HAB_COMPLEX, FAC_HABITATION_DOME});
	
	// everything is built
	
	if (facilityId == -1)
		return;
	
	debug("\t\t%s\n", Facility[facilityId].name);
	
	// get limit
	
	int populationLimit = getFactionFacilityPopulationLimit(base->faction_id, facilityId);
	debug("\t\tpopulationLimit=%d\n", populationLimit);
	
	// estimate turns to build facility
	
	int buildTurns = std::min(conf.ai_production_population_projection_turns, estimateBaseProductionTurnsToCompleteItem(baseId, -facilityId));

	// estimate projected population

	int projectedPopulation = projectBasePopulation(baseId, buildTurns);
	debug("\t\tcurrentPopulation=%d, projectedTurns=%d, projectedPopulation=%d\n", base->pop_size, buildTurns, projectedPopulation);
	
	// set priority
	
	double priority = 0.0;
	
	if (conf.habitation_facility_disable_explicit_population_limit)
	{
		// [WTP]
		
		priority = conf.ai_production_population_limit_priority;
		
		if (projectedPopulation >= populationLimit)
		{
			priority *= pow(1.0 + 0.5, projectedPopulation - populationLimit);
		}
		else
		{
			priority *= pow(1.0 + 0.2, projectedPopulation - populationLimit);
		}
		
		// get base growth rate
		
		int baseGrowthRate = getBaseGrowthRate(baseId);
		
		// current stagnation doubles the priority
		
		debug("\t\tbaseGrowthRate=%d\n", baseGrowthRate);
		
		if (baseGrowthRate < conf.se_growth_rating_min)
		{
			priority *= 2.0;
		}
		
		debug("\t\tpriority=%f\n", priority);
		
		// add demand
		
		addProductionDemand(-facilityId, priority);

	}
	else
	{
		// [vanilla]
		
		if (projectedPopulation > populationLimit)
		{
			priority = conf.ai_production_population_limit_priority;
		}
		
	}
	
	// add demand
	
	addProductionDemand(-facilityId, priority);
	
	// add additional colony demand
	
	baseColonyDemandMultiplier += 1.0;
	debug("> baseColonyDemandMultiplier=%f\n", baseColonyDemandMultiplier);
	
}

void evaluatePsychFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("\tevaluatePsychFacilitiesDemand\n");

	// determine next facility to build
	
	int facilityId = getFirstUnbuiltFacilityFromList(baseId, {FAC_RECREATION_COMMONS, FAC_HOLOGRAM_THEATRE, FAC_PARADISE_GARDEN,});
	
	// everything is built
	
	if (facilityId == -1)
		return;
	
	debug("\t\t%s\n", Facility[facilityId].name);
	
	// calculate variables

	int doctors = getDoctors(baseId);
	debug("\t\tdoctors=%d, talent_total=%d, drone_total=%d\n", doctors, base->talent_total, base->drone_total);

	// estimate turns to build facility
	
	int buildTurns = std::min(conf.ai_production_population_projection_turns, estimateBaseProductionTurnsToCompleteItem(baseId, -facilityId));

	// estimate projected population

	int projectedPopulation = projectBasePopulation(baseId, buildTurns);
	int projectedPopulationIncrease = projectedPopulation - base->pop_size;
	debug("\t\tcurrentPopulation=%d, projectedTurns=%d, projectedPopulation=%d, projectedPopulationIncrease=%d\n", base->pop_size, buildTurns, projectedPopulation, projectedPopulationIncrease);
	
	// no expected riots in future

	if (!(doctors > 0 || (base->drone_total > 0 && base->drone_total + projectedPopulationIncrease > base->talent_total)))
	{
		debug("\t\tno expected riots in future\n");
		return;
	}
	
	// set priority
	
	double priority = conf.ai_production_psych_priority;
	debug("\t\tpriority=%f\n", priority);
	
	// add demand
	
	addProductionDemand(-facilityId, priority);
	
	// add additional colony demand
	
	baseColonyDemandMultiplier += 1.0;
	debug("> baseColonyDemandMultiplier=%f\n", baseColonyDemandMultiplier);
	
}

void evaluateEcoDamageFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	const int ECO_DAMAGE_FACILITIES[] =
	{
		FAC_TREE_FARM,
		FAC_CENTAURI_PRESERVE,
		FAC_HYBRID_FOREST,
		FAC_TEMPLE_OF_PLANET,
		FAC_NANOREPLICATOR,
		FAC_SINGULARITY_INDUCTOR,
	};

	debug("\tevaluateEcoDamageFacilitiesDemand\n");
	
	// calculate priority
	
	double priority = (double)base->eco_damage / conf.ai_production_eco_damage_threshold * conf.ai_production_eco_damage_priority;
	debug("\t\teco_damage=%d, priority=%f\n", base->eco_damage, priority);

	// no eco damage

	if (base->eco_damage <= 0)
		return;

	for (const int facilityId : ECO_DAMAGE_FACILITIES)
	{
		// only available

		if (!isBaseFacilityAvailable(baseId, facilityId))
			continue;

		debug("\t\t\t%s\n", Facility[facilityId].name);

		// add demand

		addProductionDemand(-facilityId, priority);

		break;

	}

}

void evaluateMultiplyingFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	MAP *baseTile = getBaseMapTile(baseId);
	Faction *faction = &(Factions[base->faction_id]);

	debug("\tevaluateMultiplyingFacilitiesDemand\n");
	
	// calculate base values for multiplication

	int mineralSurplus = base->mineral_surplus;
	double mineralIntake = (double)base->mineral_intake;
	double economyIntake = (double)base->energy_surplus * (double)(10 - faction->SE_alloc_psych - faction->SE_alloc_labs) / 10.0;
	double psychIntake = (double)base->energy_surplus * (double)(faction->SE_alloc_psych) / 10.0;
	double labsIntake = (double)base->energy_surplus * (double)(faction->SE_alloc_labs) / 10.0;

	debug("\t\tmineralSurplus=%d, mineralIntake=%f, economyIntake=%f, psychIntake=%f, labsIntake=%f\n", mineralSurplus, mineralIntake, economyIntake, psychIntake, labsIntake);

	// no production power - multiplying facilities are useless

	if (mineralSurplus <= 0)
		return;

	// multipliers
	// bit flag: 1 = minerals, 2 = economy, 4 = psych, 8 = labs, 16 = fixed labs for biology lab (value >> 16 = fixed value)
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
		{FAC_TREE_FARM, 0},
		{FAC_HYBRID_FOREST, 0},
		{FAC_AQUAFARM, 0},
		{FAC_THERMOCLINE_TRANSDUCER, 0},
		{FAC_SUBSEA_TRUNKLINE, 0},
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
			
			// extra weight for ocean bases
			
			if (isOceanRegion(baseTile->region))
			{
				bonus *= 2;
			}
			
		}

		// economy multiplication
		
		if (mask & 2)
		{
			bonus += economyIntake / 2.0 / hurryCost;
		}

		// psych multiplication
		
		if (mask & 4)
		{
			bonus += psychIntake / 2.0 / hurryCost;
		}

		// labs multiplication
		
		if (mask & 8)
		{
			bonus += labsIntake / 2.0 / hurryCost;
		}

		// labs fixed
		
		if (mask & 16)
		{
			bonus += (double)(mask >> 16) / hurryCost;
		}
		
		// special case for forest facilities
		
		if (facilityId == FAC_TREE_FARM || facilityId == FAC_HYBRID_FOREST)
		{
			// count improvements around base
			
			int improvementCount = 0;
			int workedImprovementCount = 0;
			
			for (MAP *tile : getBaseWorkableTiles(baseId, false))
			{
				if (map_has_item(tile, TERRA_FOREST))
				{
					improvementCount++;
				}
				
			}
			
			for (MAP *tile : getBaseWorkedTiles(baseId))
			{
				if (map_has_item(tile, TERRA_FOREST))
				{
					workedImprovementCount++;
				}
				
			}
			
			// estimate potentially worked improvements after facility is built
			
			double potentialImprovementCount = (double)workedImprovementCount + 0.5 * (double)(improvementCount - workedImprovementCount);
			
			// count bonus
			
			switch (facilityId)
			{
			case FAC_TREE_FARM:
				// +1 nutrient
				bonus += 1.0 * potentialImprovementCount;
				break;
				
			case FAC_HYBRID_FOREST:
				// +1 nutrient +1 energy
				bonus += (1.0 +  1.0 / hurryCost) * potentialImprovementCount;
				break;
				
			}
			
		}
		
		// special case for aquafarm
		
		if (facilityId == FAC_AQUAFARM)
		{
			// count improvements around base
			
			int improvementCount = 0;
			int workedImprovementCount = 0;
			
			for (MAP *tile : getBaseWorkableTiles(baseId, false))
			{
				if (is_ocean(tile) && map_has_item(tile, TERRA_FARM))
				{
					improvementCount++;
				}
				
			}
			
			for (MAP *tile : getBaseWorkedTiles(baseId))
			{
				if (is_ocean(tile) && map_has_item(tile, TERRA_FARM))
				{
					workedImprovementCount++;
				}
				
			}
			
			// estimate potentially worked improvements after facility is built
			
			double potentialImprovementCount = (double)workedImprovementCount + 0.5 * (double)(improvementCount - workedImprovementCount);
			
			// count bonus
			
			// +1 nutrient
			bonus += 1.0 * potentialImprovementCount;
			
		}
		
		// special case for themrocline transducer
		
		if (facilityId == FAC_THERMOCLINE_TRANSDUCER)
		{
			// count improvements around base
			
			int improvementCount = 0;
			int workedImprovementCount = 0;
			
			for (MAP *tile : getBaseWorkableTiles(baseId, false))
			{
				if (is_ocean(tile) && map_has_item(tile, TERRA_SOLAR))
				{
					improvementCount++;
				}
				
			}
			
			for (MAP *tile : getBaseWorkedTiles(baseId))
			{
				if (is_ocean(tile) && map_has_item(tile, TERRA_SOLAR))
				{
					workedImprovementCount++;
				}
				
			}
			
			// estimate potentially worked improvements after facility is built
			
			double potentialImprovementCount = (double)workedImprovementCount + 0.5 * (double)(improvementCount - workedImprovementCount);
			
			// count bonus
			
			// +1 energy
			bonus += 1.0 / hurryCost * potentialImprovementCount;
			
		}
		
		// special case for subsea trunkline
		
		if (facilityId == FAC_SUBSEA_TRUNKLINE)
		{
			// count improvements around base
			
			int improvementCount = 0;
			int workedImprovementCount = 0;
			
			for (MAP *tile : getBaseWorkableTiles(baseId, false))
			{
				if (is_ocean(tile) && map_has_item(tile, TERRA_MINE))
				{
					improvementCount++;
				}
				
			}
			
			for (MAP *tile : getBaseWorkedTiles(baseId))
			{
				if (is_ocean(tile) && map_has_item(tile, TERRA_MINE))
				{
					workedImprovementCount++;
				}
				
			}
			
			// estimate potentially worked improvements after facility is built
			
			double potentialImprovementCount = (double)workedImprovementCount + 0.5 * (double)(improvementCount - workedImprovementCount);
			
			// count bonus
			
			// +1 mineral
			bonus += 1.0 * potentialImprovementCount;
			
		}
		
		// calculate benefit

		double benefit = bonus - ((double)maintenance / hurryCost);
		debug("\t\t\tbonus=%f, benefit=%f\n", bonus, benefit);

		// no benefit

		if (benefit <= 0)
			continue;

		// calculate construction time

		double constructionTime = (double)mineralCost / (double)(std::max(1, mineralSurplus));

		// calculate payoff time

		double payoffTime = (double)mineralCost / benefit;

		// calculate priority

		double priority = conf.ai_production_facility_priority_time / (constructionTime + payoffTime) - conf.ai_production_facility_priority_penalty;

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
	// defensive facility is more desirable if there is a bigger guarrison
	
	double priority = conf.ai_production_military_priority * aiData.baseStrategies[baseId].defenseDemand * (aiData.baseStrategies[baseId].garrison.size() / 2.0);
	
	// add demand

	addProductionDemand(-facilityId, priority);

	debug("\t\tfacility=%-25s, ai_production_military_priority=%f, regionDefenseDemand=%f, exposure=%f, priority=%f\n", Facility[facilityId].name, conf.ai_production_military_priority, aiData.regionDefenseDemand[baseTile->region], aiData.baseStrategies[baseId].exposure, priority);
	
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
		double priority = conf.ai_production_command_center_priority * regionMilitaryPriority * base->mineral_surplus_final / aiData.regionMaxMineralSurpluses[region];
		addProductionDemand(-facilityId, priority);
		debug("\t\tfacility=%-25s, ai_production_command_center_priority=%f, priority=%f\n", Facility[facilityId].name, conf.ai_production_command_center_priority, priority);
	}
	
	// naval yard
	
	if (has_facility_tech(aiFactionId, FAC_NAVAL_YARD) && !has_facility(baseId, FAC_NAVAL_YARD) && (ocean || connectedOceanRegions.size() >= 1))
	{
		int facilityId = FAC_NAVAL_YARD;
		double priority = conf.ai_production_naval_yard_priority * regionMilitaryPriority * base->mineral_surplus_final / aiData.regionMaxMineralSurpluses[region];
		addProductionDemand(-facilityId, priority);
		debug("\t\tfacility=%-25s, ai_production_naval_yard_priority=%f, priority=%f\n", Facility[facilityId].name, conf.ai_production_naval_yard_priority, priority);
	}
	
	// aerospace complex
	
	if (has_facility_tech(aiFactionId, FAC_AEROSPACE_COMPLEX) && !has_facility(baseId, FAC_AEROSPACE_COMPLEX))
	{
		int facilityId = FAC_AEROSPACE_COMPLEX;
		double priority = conf.ai_production_aerospace_complex_priority * globalMilitaryPriority * base->mineral_surplus_final / aiData.maxMineralSurplus;
		addProductionDemand(-facilityId, priority);
		debug("\t\tfacility=%-25s, ai_production_aerospace_complex_priority=%f, priority=%f\n", Facility[facilityId].name, conf.ai_production_aerospace_complex_priority, priority);
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
		
		// add additional colony demand
		
		baseColonyDemandMultiplier += 1.0;
		debug("> baseColonyDemandMultiplier=%f\n", baseColonyDemandMultiplier);
		
	}

}

/*
Evaluates demand for land improvement.
*/
void evaluateLandFormerDemand()
{
	BASE *base = productionDemand.base;

	debug("evaluateLandFormerDemand\n");
	
	// check demand is positive
	
	if (aiData.production.landFormerDemand <= 0.0)
	{
		debug("\tlandFormerDemand <= 0.0\n");
		return;
	}

	// find former unit

	int formerUnitId = findFormerUnit(base->faction_id, TRIAD_LAND);

	if (formerUnitId == -1)
	{
		debug("\t\tno former unit\n");
		return;
	}

	debug("\t\tformerUnit=%-25s\n", Units[formerUnitId].name);

	// set priority

	double priority = conf.ai_production_improvement_priority * aiData.production.landFormerDemand * ((double)base->mineral_surplus / (double)aiData.maxMineralSurplus);

	debug("\t\tpriority=%f, ai_production_improvement_priority=%f, landFormerDemand=%f, mineral_surplus=%d, maxMineralSurplus=%d\n", priority, conf.ai_production_improvement_priority, aiData.production.landFormerDemand, base->mineral_surplus, aiData.maxMineralSurplus);

	addProductionDemand(formerUnitId, priority);
	
}

/*
Evaluates demand for sea improvement.
*/
void evaluateSeaFormerDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	int baseOceanAssociation = getBaseOceanAssociation(baseId);

	debug("evaluateSeaFormerDemand\n");
	
	// base is not a port
	
	if (baseOceanAssociation == -1)
	{
		debug("base is not a port\n");
		return;
	}

	// check demand exists
	
	if (aiData.production.seaFormerDemands.count(baseOceanAssociation) == 0)
	{
		debug("\tno seaFormerDemand\n");
		return;
	}
	
	// check demand is positive
	
	double seaFormerDemand = aiData.production.seaFormerDemands.at(baseOceanAssociation);
	
	if (seaFormerDemand <= 0.0)
	{
		debug("\tseaFormerDemand <= 0.0\n");
		return;
	}
	
	// find former unit
	
	int formerUnitId = findFormerUnit(base->faction_id, TRIAD_SEA);
	
	if (formerUnitId == -1)
	{
		debug("\t\tno former unit\n");
		return;
	}
	
	debug("\t\tformerUnit=%-25s\n", Units[formerUnitId].name);
	
	// set priority
	
	int maxMineralSurplus = aiData.maxMineralSurplus;
	
	if (aiData.regionMaxMineralSurpluses.count(baseOceanAssociation) != 0)
	{
		maxMineralSurplus = aiData.regionMaxMineralSurpluses.at(baseOceanAssociation);
	}

	double priority = conf.ai_production_improvement_priority * seaFormerDemand * ((double)base->mineral_surplus / (double)maxMineralSurplus);

	debug("\t\tpriority=%f, ai_production_improvement_priority=%f, seaFormerDemand=%f, mineral_surplus=%d, maxMineralSurplus=%d\n", priority, conf.ai_production_improvement_priority, seaFormerDemand, base->mineral_surplus, maxMineralSurplus);

	addProductionDemand(formerUnitId, priority);
	
}

/*
Evaluates need for colonies.
*/
void evaluateLandColonyDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluateLandColonyDemand\n");

	// verify base can build a colony

	if (!canBaseProduceColony(baseId))
	{
		debug("\tbase cannot build a colony\n");
		return;
	}
	
	// get colony demand
	
	double landColonyDemand = aiData.production.landColonyDemand;
	
	// too low demand

	if (landColonyDemand <= 0.0)
	{
		debug("\tlandColonyDemand <= 0.0\n");
		return;
	}

	// calculate priority

	double priority =
		(
			landColonyDemand
			*
			(conf.ai_production_expansion_priority + conf.ai_production_expansion_priority_per_population * (double)base->pop_size)
		)
		*
		globalBaseDemand
		*
		baseColonyDemandMultiplier
	;
	
	debug
	(
		"\t\tpriority=%f, landColonyDemand=%f, multiplier=%f, globalBaseDemand=%f, baseColonyDemandMultiplier=%f\n",
		priority,
		landColonyDemand,
		conf.ai_production_expansion_priority + conf.ai_production_expansion_priority_per_population * (double)base->pop_size,
		globalBaseDemand,
		baseColonyDemandMultiplier
	)
	;

	// find optimal colony unit

	int optimalColonyUnitId = findOptimalColonyUnit(base->faction_id, TRIAD_LAND, base->mineral_surplus);

	if (optimalColonyUnitId == -1)
	{
		debug("\t\tno land/air colony unit found\n");
		return;
	}

	debug("\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, optimalColonyUnitId=%-25s\n", TRIAD_LAND, base->mineral_surplus, Units[optimalColonyUnitId].name);

	// set base demand

	addProductionDemand(optimalColonyUnitId, priority);

}

/*
Evaluates need for colonies.
*/
void evaluateSeaColonyDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	int baseOceanAssociation = getBaseOceanAssociation(baseId);

	debug("evaluateSeaColonyDemand\n");
	
	// verify base is a port
	
	if (baseOceanAssociation == -1)
	{
		debug("\tbase is not a port\n");
		return;
	}

	// verify base can build a colony

	if (!canBaseProduceColony(baseId))
	{
		debug("\tbase cannot build a colony\n");
		return;
	}
	
	// get sea association demand
	
	if (aiData.production.seaColonyDemands.count(baseOceanAssociation) == 0)
	{
		debug("\tERROR: no seaColonyDemand for this association.\n")
		return;
	}
	
	// get colony demand
	
	double seaColonyDemand = aiData.production.seaColonyDemands.at(baseOceanAssociation);
	
	// too low demand

	if (seaColonyDemand <= 0.0)
	{
		debug("\tseaColonyDemand <= 0.0\n");
		return;
	}

	// calculate priority

	double priority =
		(
			seaColonyDemand
			*
			(conf.ai_production_expansion_priority + conf.ai_production_expansion_priority_per_population * (double)base->pop_size)
		)
		*
		globalBaseDemand
		*
		baseColonyDemandMultiplier
	;
	
	debug
	(
		"\t\tpriority=%f, seaColonyDemand=%f, multiplier=%f, globalBaseDemand=%f, baseColonyDemandMultiplier=%f\n",
		priority,
		seaColonyDemand,
		conf.ai_production_expansion_priority + conf.ai_production_expansion_priority_per_population * (double)base->pop_size,
		globalBaseDemand,
		baseColonyDemandMultiplier
	)
	;

	// find optimal colony unit

	int optimalColonyUnitId = findOptimalColonyUnit(base->faction_id, TRIAD_SEA, base->mineral_surplus);
	
	if (optimalColonyUnitId == -1)
	{
		debug("\t\tno sea/air colony unit found\n");
		return;
	}

	debug("\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, optimalColonyUnitId=%-25s\n", TRIAD_SEA, base->mineral_surplus, Units[optimalColonyUnitId].name);

	// set base demand

	addProductionDemand(optimalColonyUnitId, priority);

}

/*
Evaluates need for bases protection against natives.
*/
void evaluateAlienDefenseDemand()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	if (isLandRegion(baseTile->region))
	{
		evaluateLandAlienDefenseDemand();
	}
	else
	{
		evaluateSeaAlienDefenseDemand();
	}
	
}

void evaluateLandAlienDefenseDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = &(Bases[baseId]);
	MAP *baseTile = getBaseMapTile(baseId);
	int region = baseTile->region;
	
	debug("evaluateLandAlienDefenseDemand\n");
	
	// count aliens
	
	int activeAlienCount = 0;
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// this region only
		
		if (vehicleTile->region != region)
			continue;
		
		// our territory only
		
		if (vehicleTile->owner != aiFactionId)
			continue;
		
		// nearby vehicles only
		
		if (map_range(base->x, base->y, vehicle->x, vehicle->y) > 10)
			continue;
		
		// aliens only
		
		if (vehicle->faction_id != 0)
			continue;
		
		// active aliens only
		
		if (!(vehicle->unit_id == BSC_MIND_WORMS))
			continue;
		
		// count
		
		activeAlienCount++;
		
	}
	
	debug("\tactiveAlienCount=%d\n", activeAlienCount);
	
	// count potential alien spawn places
	
	int alienSpawnPlacesCount = 0;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int mapX = getX(mapIndex);
		int mapY = getY(mapIndex);
		MAP *tile = getMapTile(mapIndex);
		
		// this region only
		
		if (tile->region != region)
			continue;
		
		// our or unclaimed territory only
		
		if (!(tile->owner == -1 || tile->owner == aiFactionId))
			continue;
		
		// nearby maps only
		
		if (map_range(base->x, base->y, mapX, mapY) > 10)
			continue;
		
		// not within base radius
		
		if (map_has_item(tile, TERRA_BASE_RADIUS))
			continue;
		
		// fungus
		
		if (!map_has_item(tile, TERRA_FUNGUS))
			continue;
		
		// count tiles
		
		alienSpawnPlacesCount++;
		
	}
	
	debug("\talienSpawnPlacesCount=%d\n", alienSpawnPlacesCount);
	
	// calculate potential alien threat per tile
	
	double potentialAlienThreatPerTile = conf.ai_production_alien_threat_per_tile * (*map_native_lifeforms + 1) * alienSpawnPlacesCount;
	
	// calculate potential alien threat per base
	
	double potentialAlienThreatPerBase = conf.ai_production_alien_threat_per_base;
	
	// calculate potential alien threat
	
	double potentialAlienThreat = std::min(potentialAlienThreatPerTile, potentialAlienThreatPerBase);
	
	// summarize active alien threats
	
	double activeAlienThreat = activeAlienCount + potentialAlienThreat;
	
	debug("\t\tpotentialAlienThreatPerTile=%f, potentialAlienThreatPerBase=%f, potentialAlienThreat=%f, activeAlienThreat=%f\n", potentialAlienThreatPerTile, potentialAlienThreatPerBase, potentialAlienThreat, activeAlienThreat);
	
	// calculate sensor strength multiplier
	
	double sensorDefenseMultipler = (isWithinFriendlySensorRange(aiFactionId, base->x, base->y) ? getPercentageBonusMultiplier(Rules->combat_defend_sensor) : 1.0);
		
	debug("\tsensorDefenseMultipler=%f\n", sensorDefenseMultipler);
	
	// count scout psi strength
	
	double scoutPsiDefense = 0.0;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();
		
		// this region only
		
		if (vehicleTile->region != region)
			continue;
		
		// nearby vehicles only
		
		if (map_range(base->x, base->y, vehicle->x, vehicle->y) > 5)
			continue;
		
		// land only
		
		if (triad != TRIAD_LAND)
			continue;
		
		// scout or any combat unit at base
		
		if (!(isScoutVehicle(vehicleId) || (isCombatVehicle(vehicleId) && (vehicle->x == base->x && vehicle->y == base->y))))
			continue;
		
		// calculate psi strength
		
		double psiDefense = getVehiclePsiDefenseStrength(vehicleId, true);
		
		// modify psiDefense based on opponent offense
		
		psiDefense /= (Rules->psi_combat_land_numerator / Rules->psi_combat_land_denominator);
		
		// modify strength based on base defense
		
		psiDefense *= getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def);
		
		// modify psi strength with sensor multiplier
		
		psiDefense *= sensorDefenseMultipler;
		
		// update totals
		
		scoutPsiDefense += psiDefense;
		
	}
	
	debug("\tscoutPsiDefense=%f\n", scoutPsiDefense);
	
	// defense demand
	
	if (activeAlienThreat <= 0.0 || scoutPsiDefense >= activeAlienThreat)
	{
		debug("\tdemand is satisfied\n");
	}
	else
	{
		// calculate demand
		
		double alienDefenseDemand = (activeAlienThreat - scoutPsiDefense) / activeAlienThreat;
		
		debug("\tactiveAlienThreat=%f, scoutPsiDefense=%f, alienDefenseDemand=%f\n", activeAlienThreat, scoutPsiDefense, alienDefenseDemand);
		
		// find best anti-native defense unit
		
		int unitId = findNativeDefenderUnit();
		
		// no unit found
		
		if (unitId == -1)
		{
			debug("\tno anti-native defense unit found\n");
		}
		else
		{
			debug("\t%s\n", Units[unitId].name);
			
			// set priority

			double priority = conf.ai_production_native_protection_priority * alienDefenseDemand;

			debug("\t\talienDefenseDemand=%f, priority=%f\n", alienDefenseDemand, priority);
			
			// add production demand

			addProductionDemand(unitId, priority);

		}
		
	}
	
}

void evaluateSeaAlienDefenseDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = &(Bases[baseId]);
	MAP *baseTile = getBaseMapTile(baseId);
	int region = baseTile->region;
	
	debug("evaluateSeaAlienDefenseDemand\n");
	
	// count aliens
	
	int activeAlienCount = 0;
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// this region only
		
		if (vehicleTile->region != region)
			continue;
		
		// our territory only
		
		if (vehicleTile->owner != aiFactionId)
			continue;
		
		// nearby vehicles only
		
		if (map_range(base->x, base->y, vehicle->x, vehicle->y) > 10)
			continue;
		
		// aliens only
		
		if (vehicle->faction_id != 0)
			continue;
		
		// active aliens only
		
		if (!(vehicle->unit_id == BSC_SEALURK || vehicle->unit_id == BSC_ISLE_OF_THE_DEEP))
			continue;
		
		// count
		
		activeAlienCount++;
		
	}
	
	debug("\tactiveAlienCount=%d\n", activeAlienCount);
	
	// count potential alien spawn places
	
	int alienSpawnPlacesCount = 0;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int mapX = getX(mapIndex);
		int mapY = getY(mapIndex);
		MAP *tile = getMapTile(mapIndex);
		
		// this region only
		
		if (tile->region != region)
			continue;
		
		// our or unclaimed territory only
		
		if (!(tile->owner == -1 || tile->owner == aiFactionId))
			continue;
		
		// nearby tiles only
		
		if (map_range(base->x, base->y, mapX, mapY) > 10)
			continue;
		
		// not within base radius
		
		if (map_has_item(tile, TERRA_BASE_RADIUS))
			continue;
		
		// count tiles
		
		alienSpawnPlacesCount++;
		
	}
	
	debug("\talienSpawnPlacesCount=%d\n", alienSpawnPlacesCount);
	
	// calculate potential alien threat per tile
	
	double potentialAlienThreatPerTile = conf.ai_production_alien_threat_per_tile * (*map_native_lifeforms + 1) * alienSpawnPlacesCount;
	
	// calculate potential alien threat per base
	
	double potentialAlienThreatPerBase = conf.ai_production_alien_threat_per_base;
	
	// calculate potential alien threat
	
	double potentialAlienThreat = std::min(potentialAlienThreatPerTile, potentialAlienThreatPerBase);
	
	// summarize active alien threats
	
	double activeAlienThreat = activeAlienCount + potentialAlienThreat;
	
	debug("\tpotentialAlienThreatPerTile=%f, potentialAlienThreatPerBase=%f, potentialAlienThreat=%f, activeAlienThreat=%f\n", potentialAlienThreatPerTile, potentialAlienThreatPerBase, potentialAlienThreat, activeAlienThreat);
	
	// calculate sensor strength multiplier
	
	double sensorDefenseMultipler = (conf.combat_bonus_sensor_ocean && isWithinFriendlySensorRange(aiFactionId, base->x, base->y) ? getPercentageBonusMultiplier(Rules->combat_defend_sensor) : 1.0);
		
	debug("\tsensorDefenseMultipler=%f\n", sensorDefenseMultipler);
	
	// count scout psi strength
	// this base guarrison only
	
	double scoutPsiDefense = 0.0;
	
	for (int vehicleId : getBaseGarrison(baseId))
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int triad = vehicle->triad();
		
		// land only
		
		if (triad != TRIAD_LAND)
			continue;
		
		// scout or any combat vehicle at base
		
		if (!(isScoutVehicle(vehicleId) || (isCombatVehicle(vehicleId) && (vehicle->x == base->x && vehicle->y == base->y))))
			continue;
		
		// calculate psi strength
		
		double psiDefense = getVehiclePsiDefenseStrength(vehicleId, true);
		
		// modify psiDefense based on opponent offense
		
		psiDefense /= (Rules->psi_combat_land_numerator / Rules->psi_combat_land_denominator);
		
		// modify strength based on base defense
		
		psiDefense *= getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def);
		
		// modify psi strength with sensor multiplier
		
		psiDefense *= sensorDefenseMultipler;
		
		// update totals
		
		scoutPsiDefense += psiDefense;
		
	}
	
	debug("\tscoutPsiDefense=%f\n", scoutPsiDefense);
	
	// defense demand
	
	if (activeAlienThreat <= 0.0 || scoutPsiDefense >= activeAlienThreat)
	{
		debug("\tdemand is satisfied\n");
	}
	else
	{
		// calculate demand
		
		double alienDefenseDemand = (activeAlienThreat - scoutPsiDefense) / activeAlienThreat;
		
		debug("\tactiveAlienThreat=%f, scoutPsiDefense=%f, alienDefenseDemand=%f\n", activeAlienThreat, scoutPsiDefense, alienDefenseDemand);
		
		// find best anti-native defense unit
		
		int unitId = findNativeDefenderUnit();
		
		// no unit found
		
		if (unitId == -1)
		{
			debug("\tno anti-native defense unit found\n");
		}
		else
		{
			debug("\t%s\n", Units[unitId].name);
			
			// set priority

			double priority = conf.ai_production_native_protection_priority * alienDefenseDemand;

			debug("\talienDefenseDemand=%f, priority=%f\n", alienDefenseDemand, priority);
			
			// add production demand

			addProductionDemand(unitId, priority);

		}
		
	}
	
}

/*
Evaluates need for patrolling natives.
*/
void evaluateAlienHuntingDemand()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	if (isLandRegion(baseTile->region))
	{
		evaluateLandAlienHuntingDemand();
	}
	
}

void evaluateLandAlienHuntingDemand()
{
	BASE *base = productionDemand.base;

	debug("evaluateLandAlienHuntingDemand\n");
	
	// no demand
	
	if (aiData.landAlienHunterDemand <= 0)
	{
		debug("\tlandAlienHunterDemand <= 0\n");
		return;
	}

	// find best anti-native offense unit
	
	int unitId = findNativeAttackerUnit(false);
	
	// no unit found
	
	if (unitId == -1)
	{
		debug("\tno anti-native offense unit found\n");
		return;
	}
	
	debug("\t%s\n", Units[unitId].name);
	
	// set priority
	
	double priority = conf.ai_production_native_protection_priority * ((double)base->mineral_surplus / (double)aiData.maxMineralSurplus);
	
	debug("\tmaxMineralSurplus=%d, mineral_surplus=%d, priority=%f\n", aiData.maxMineralSurplus, base->mineral_surplus, priority);
	
	// add production demand

	addProductionDemand(unitId, priority);
	
}

void evaluatePodPoppingDemand()
{
	evaluateLandPodPoppingDemand();
	evaluateSeaPodPoppingDemand();
}

void evaluateLandPodPoppingDemand()
{
	BASE *base = productionDemand.base;

	debug("evaluateLandPodPoppingDemand\n");
	
	// base requires at least twice minimal mineral surplus requirement to build scout
	
	if (base->mineral_surplus < conf.ai_production_unit_min_mineral_surplus)
	{
		debug("\tmineral surplus < %d\n", conf.ai_production_unit_min_mineral_surplus);
		return;
	}
	
	// no demand
	
	if (aiData.production.landPodPoppingDemand <= 0.0)
	{
		debug("\tlandPodPoppingDemand < 0.0\n");
		return;
	}

	// set priority

	double priority =
		conf.ai_production_pod_popping_priority
		*
		aiData.production.landPodPoppingDemand
		*
		((double)base->mineral_surplus / (double)aiData.maxMineralSurplus)
	;
	
	debug("\tlandPodPoppingDemand=%f, mineral_surplus=%d, priority=%f\n", aiData.production.landPodPoppingDemand, base->mineral_surplus, priority);
	
	// select scout unit
	
	int podPopperUnitId = findScoutUnit(aiFactionId, TRIAD_LAND);
	
	if (podPopperUnitId < 0)
	{
		debug("\tno scout unit\n");
		return;
	}
	
	debug("\tscout unit: %s\n", prod_name(podPopperUnitId));
	
	// add production demand
	
	addProductionDemand(podPopperUnitId, priority);

}

void evaluateSeaPodPoppingDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	int baseOceanAssociation = getBaseOceanAssociation(baseId);

	debug("evaluateSeaPodPoppingDemand\n");
	
	// should be sea base
	
	if (baseOceanAssociation == -1)
	{
		debug("\tnot a sea base\n");
		return;
	}
	
	// base requires at least twice minimal mineral surplus requirement to build scout
	
	if (base->mineral_surplus < conf.ai_production_unit_min_mineral_surplus)
	{
		debug("\tmineral surplus < %d\n", conf.ai_production_unit_min_mineral_surplus);
		return;
	}
	
	// no demand
	
	if (aiData.production.seaPodPoppingDemand.at(baseOceanAssociation) <= 0.0)
	{
		debug("\tseaPodPoppingDemand < 0.0\n");
		return;
	}

	// set priority

	double priority =
		conf.ai_production_pod_popping_priority
		*
		aiData.production.seaPodPoppingDemand.at(baseOceanAssociation)
		*
		((double)base->mineral_surplus / (double)aiData.maxMineralSurplus)
	;
	
	debug("\tseaPodPoppingDemand=%f, mineral_surplus=%d, priority=%f\n", aiData.production.seaPodPoppingDemand.at(baseOceanAssociation), base->mineral_surplus, priority);
	
	// select scout unit
	
	int podPopperUnitId = findScoutUnit(aiFactionId, TRIAD_SEA);
	
	if (podPopperUnitId < 0)
	{
		debug("\tno scout unit\n");
		return;
	}
	
	debug("\tscout unit: %s\n", prod_name(podPopperUnitId));
	
	// add production demand
	
	addProductionDemand(podPopperUnitId, priority);

}

void evaluatePrototypingDemand()
{
	debug("evaluatePrototypingDemand - %s\n", MFactions[aiFactionId].noun_faction);
	
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	bool inSharedOceanRegion = aiData.baseStrategies[baseId].inSharedOceanRegion;
	
	// base requires at least two times minimal mineral surplus requirement to build prototype
	
	if (base->mineral_surplus < 2 * conf.ai_production_unit_min_mineral_surplus)
		return;

	// do not produce prototype if other bases already produce them
	
	for (int otherBaseId : aiData.baseIds)
	{
		BASE *otherBase = &(Bases[otherBaseId]);
		
		// skip self
		
		if (otherBaseId == baseId)
			continue;
		
		int otherBaseProductionItem = otherBase->queue_items[0];
		
		if (otherBaseProductionItem >= 0)
		{
			if (isPrototypeUnit(otherBaseProductionItem))
				return;
		}
		
	}
	
	// find cheapest unprototyped unit
	
	int unprototypedUnitId = -1;
	int unprototypedUnitMineralCost = INT_MAX;
	
	for (int unitId : aiData.unitIds)
	{
		UNIT *unit = &(Units[unitId]);
		
		// skip sea units for bases without shared ocean region
		
		if (unit->triad() == TRIAD_SEA && !inSharedOceanRegion)
			continue;
		
		// find unprototyped
		
		if (isPrototypeUnit(unitId))
		{
			int mineralCost = getBaseMineralCost(baseId, unitId);
			
			if (mineralCost < unprototypedUnitMineralCost)
			{
				unprototypedUnitId = unitId;
				unprototypedUnitMineralCost = mineralCost;
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
	
	double priority = conf.ai_production_prototyping_priority * globalMilitaryPriority;
	
	// calculate adjusted priority based on mineral surplus
	
	double adjustedPriority = priority * ((double)base->mineral_surplus / (double)aiData.maxMineralSurplus);
	
	debug("\tpriority=%f, mineral_surplus=%d, maxMineralSurplus=%d, adjustedPriority=%f\n", priority, base->mineral_surplus, aiData.maxMineralSurplus, adjustedPriority);
	
	// add production demand
	
	addProductionDemand(unprototypedUnitId, adjustedPriority);

}

void evaluateCombatDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	
	debug("evaluateCombatDemand\n");

	// check mineral surplus
	
	if (base->mineral_surplus < conf.ai_production_unit_min_mineral_surplus)
	{
		debug("\tweak production\n");
		return;
	}
	
	// get target base
	
	int targetBaseId = aiData.baseStrategies[baseId].targetBaseId;
	
	// no target base
	
	if (targetBaseId == -1)
	{
		debug("\tno target base\n");
		return;
	}
	
	debug("\t-> %s\n", Bases[targetBaseId].name);

	// get target base strategy
	
	BaseStrategy *targetBaseStrategy = &(aiData.baseStrategies[targetBaseId]);
	
	// no defense demand
	
	if (targetBaseStrategy->defenseDemand <= 0.0)
	{
		debug("\tno defense demand\n");
		return;
	}
	
	// find unit
	
	int unitId = selectCombatUnit(baseId, targetBaseId);
	
	// no unit found
	
	if (unitId == -1)
	{
		debug("\tno unit found\n");
		return;
	}
	
	debug("\t%s\n", Units[unitId].name);
	
	// calculate priority
	
	double priority = conf.ai_production_military_priority * targetBaseStrategy->defenseDemand;
	
	// priority is reduced for low production bases except selftargetted base
	
	if (targetBaseId != baseId)
	{
		priority *= ((double)base->mineral_surplus / (double)aiData.maxMineralSurplus);
	}
	
	// add demand
	
	addProductionDemand(unitId, priority);
	
	debug("\t%s -> %s, %s, ai_production_military_priority=%f, defenseDemand=%f, priority=%f\n", base->name, Bases[targetBaseId].name, Units[unitId].name, conf.ai_production_military_priority, targetBaseStrategy->defenseDemand, priority);
	
}

void addProductionDemand(int item, double priority)
{
	BASE *base = productionDemand.base;

	// prechecks

	// do not exhaust support

	if (item >= 0)
	{
//		if (isFormerUnit(item))
//		{
//			// former mineral surplus requirement
//			
//			if (base->mineral_surplus < conf.ai_production_unit_min_mineral_surplus)
//			{
//				debug("...> cannot build former: mineral_surplus < %d\n", conf.ai_production_unit_min_mineral_surplus);
//				return;
//			}
//			
//		}
//		else if (isColonyUnit(item))
//		{
//			// colony mineral surplus requirement
//			
//			if (base->mineral_surplus < conf.ai_production_unit_min_mineral_surplus)
//			{
//				debug("...> cannot build former: mineral_surplus < %d\n", conf.ai_production_unit_min_mineral_surplus);
//				return;
//			}
//			
//		}
//		else
		{
			// other unit requires at least conf.ai_production_unit_min_mineral_surplus mineral surplus
			
			if (base->mineral_surplus < conf.ai_production_unit_min_mineral_surplus)
			{
				debug("...> cannot build unit: mineral_surplus < %d\n", conf.ai_production_unit_min_mineral_surplus);
				return;
			}
			
			// and no more than 1/2 of mineral production spent on support
			
			if (base->mineral_surplus < base->mineral_consumption)
			{
				debug("...> cannot build unit: mineral_surplus < mineral_consumption\n");
				return;
			}
			
		}
		
	}

	// update demand

	if (priority > productionDemand.priority)
	{
		productionDemand.item = item;
		productionDemand.priority = priority;
	}

}

int findNativeDefenderUnit()
{
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.unitIds)
	{
        UNIT *unit = &Units[unitId];

		// skip non land

		if (unit->triad() != TRIAD_LAND)
			continue;
		
		// skip not scout
		
		if (!isScoutUnit(unitId))
			continue;
		
		// prefer police and trance unit
		
		double effectiveness;

		if (unit_has_ability(unitId, ABL_POLICE_2X))
		{
			effectiveness = 2.0;
		}
		else if (unit_has_ability(unitId, ABL_TRANCE))
		{
			effectiveness = 1.5;
		}
		else
		{
			effectiveness = 1.0;
		}
		
		// update best
		
		if (bestUnitId == -1 || effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

    return bestUnitId;

}

int findNativeAttackerUnit(bool ocean)
{
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.unitIds)
	{
		UNIT *unit = &(Units[unitId]);
		
		// skip wrong triad

		if (!(unit->triad() == (ocean ? TRIAD_SEA : TRIAD_LAND)))
			continue;

		// skip not scout
		
		if (!isScoutUnit(unitId))
			continue;
		
		// prefer empath unit
		
		double effectiveness;

		if (unit_has_ability(unitId, ABL_EMPATH))
		{
			effectiveness = 1.5;
		}
		else
		{
			effectiveness = 1.0;
		}
		
		// prefer fast unit after turn 100
		
		if (*current_turn > 50)
		{
			effectiveness *= (double)unit->speed();
		}
		
		// update best
		
		if (bestUnitId == -1 || effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}
		
	}

    return bestUnitId;

}

/*
Returns most effective prototyped land defender unit id.
*/
int findRegularDefenderUnit(int factionId)
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
		UNIT *unit = &(Units[unitId]);

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
		UNIT *unit = &(Units[unitId]);

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

int findOptimalColonyUnit(int factionId, int triad, int mineralSurplus)
{
	assert(triad == TRIAD_LAND || triad == TRIAD_SEA);
	
	int bestUnitId = -1;
	double bestTotalTurns = 0.0;
	int travelTurns = 6;

	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(Units[unitId]);
		CChassis *chassis = &(Chassis[unit->chassis_type]);

		// given triad only or air with unlimited range

		if (!(triad == TRIAD_AIR && chassis->range > 0 || unit_triad(unitId) == triad))
			continue;
		
		// colony only

		if (!isColonyUnit(unitId))
			continue;

		// calculate total turns assuming travelTurns

		double totalTurns = ((double)(cost_factor(aiFactionId, 1, -1) * unit->cost) / (double)mineralSurplus) + ((double)travelTurns / (double)unit_chassis_speed(unitId));

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

		if (!isFormerUnit(unitId))
			continue;

		// given triad only or air

		if (!(unit_triad(unitId) == TRIAD_AIR || unit_triad(unitId) == triad))
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

		if ((triad != -1) && (vehicle->triad() != triad))
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
Checks for military purpose unit/structure.
*/
bool isMilitaryItem(int item)
{
	if (item >= 0)
	{
		UNIT *unit = &(Units[item]);
		int weaponId = unit->weapon_type;
		
		return
			(isCombatUnit(item) && !isScoutUnit(item))
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

int findBestAntiNativeUnitId(int baseId, int targetBaseId, int opponentTriad)
{
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = isOceanRegion(targetBaseTile->region);
	
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();

		// skip unmatching triad

		if (targetBaseOcean)
		{
			if (triad == TRIAD_LAND)
				continue;
		}
		else
		{
			if (triad == TRIAD_SEA)
				continue;
		}
		
		// skip units unable to attack each other
		
		if
		(
			opponentTriad == TRIAD_LAND && triad == TRIAD_SEA
			||
			opponentTriad == TRIAD_SEA && triad == TRIAD_LAND
		)
			continue;
		
		// strength

		double offenseStrength = getPsiCombatBaseOdds(triad) * getMoraleModifier(getNewVehicleMorale(unitId, baseId, false));
		double defenseStrength = (1 / getPsiCombatBaseOdds(opponentTriad)) * getMoraleModifier(getNewVehicleMorale(unitId, baseId, true));
		
		// ability modifiers
		
		if (unit_has_ability(unitId, ABL_EMPATH))
		{
			offenseStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_empath_song_vs_psi);
		}
		
		if (unit_has_ability(unitId, ABL_TRANCE))
		{
			defenseStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi);
		}
		
		if (unit_has_ability(unitId, ABL_AAA) && opponentTriad == TRIAD_AIR)
		{
			defenseStrength *= getPercentageBonusMultiplier(Rules->combat_AAA_bonus_vs_air);
		}
		
		// pick strongest trait
		
		double strength = std::max(offenseStrength, defenseStrength);
		
		// calculate cost counting maintenance
		
		int cost = getUnitMaintenanceCost(unitId);
		
		// calculate effectiveness
		
		double effectiveness = strength / (double)cost;
		
		// update best effectiveness
		
		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}
		
	}

    return bestUnitId;

}

int findBestAntiRegularUnitId(int baseId, int targetBaseId, int opponentTriad)
{
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = isOceanRegion(targetBaseTile->region);
	
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();

		// skip unmatching triad

		if (targetBaseOcean)
		{
			if (triad == TRIAD_LAND)
				continue;
		}
		else
		{
			if (triad == TRIAD_SEA)
				continue;
		}
		
		// skip units unable to attack each other
		
		if (opponentTriad == TRIAD_LAND && triad == TRIAD_SEA || opponentTriad == TRIAD_SEA && triad == TRIAD_LAND)
			continue;
		
		// strength

		double offenseStrength = getPsiCombatBaseOdds(triad) * getMoraleModifier(getNewVehicleMorale(unitId, baseId, false));
		double defenseStrength = (1 / getPsiCombatBaseOdds(opponentTriad)) * getMoraleModifier(getNewVehicleMorale(unitId, baseId, true));
		
		// ability modifiers
		
		if (unit_has_ability(unitId, ABL_EMPATH))
		{
			offenseStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_empath_song_vs_psi);
		}
		
		if (unit_has_ability(unitId, ABL_TRANCE))
		{
			defenseStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi);
		}
		
		if (unit_has_ability(unitId, ABL_AAA) && opponentTriad == TRIAD_AIR)
		{
			defenseStrength *= getPercentageBonusMultiplier(Rules->combat_AAA_bonus_vs_air);
		}
		
		// pick strongest trait
		
		double strength = std::max(offenseStrength, defenseStrength);
		
		// calculate cost counting maintenance
		
		int cost = getUnitMaintenanceCost(unitId);
		
		// calculate effectiveness
		
		double effectiveness = strength / (double)cost;
		
		// update best effectiveness
		
		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}
		
	}

    return bestUnitId;

}

/*
Finds best combat unit built at source base to help target base against target base threats.
*/
int selectCombatUnit(int baseId, int targetBaseId)
{
	debug("selectCombatUnit\n");
	
	MAP *baseTile = getBaseMapTile(baseId);
	bool ocean = isOceanRegion(baseTile->region);
	
	// no target base
	
	if (targetBaseId == -1)
	{
		debug("\tno target base\n");
		return -1;
	}
	
	debug("\t-> %s\n", Bases[targetBaseId].name);
	
	// get target base info
	
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	
	// get target base strategy
	
	BaseStrategy *targetBaseStrategy = &(aiData.baseStrategies[targetBaseId]);
	MilitaryStrength *defenderStrength = &(targetBaseStrategy->defenderStrength);
	MilitaryStrength *opponentStrength = &(targetBaseStrategy->opponentStrength);
	
	// no defense demand
	
	if (targetBaseStrategy->defenseDemand <= 0.0)
	{
		debug("\tno defense demand\n");
		return -1;
	}
	
	// no opponents
	
	if (opponentStrength->unitStrengths.size() == 0)
	{
		debug("\tno opponents\n");
		return -1;
	}
	
	// process units
	
	std::map<int, int> unitClassBestUnits;
	std::map<int, double> unitEffectivenesses;
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = &(Units[unitId]);
		int triad = unit->triad();
		int unitWeaponOffenseValue = getUnitWeaponOffenseValue(unitId);
		int unitArmorDefenseValue = getUnitArmorDefenseValue(unitId);
		
		int defenderUnitId = unitId;
		UNIT *defenderUnit = unit;
		
		debug("\t%s\n", unit->name);
		
		// do not produce sea unit in base that is not in shared ocean region
		
		if (triad == TRIAD_SEA && !aiData.baseStrategies[baseId].inSharedOceanRegion)
			continue;
		
		// ocean base
		if (ocean)
		{
			// no land attackers
			
			if (triad == TRIAD_LAND && (unit->chassis_type != CHS_INFANTRY || unitWeaponOffenseValue > 1))
			{
				debug("\t\tocean base: no land attackers\n");
				continue;
			}
			
			// same base
			if (baseId == targetBaseId)
			{
				// no restrictions
			}
			// other base
			else
			{
				// connected ocean region
				if (isBaseConnectedToRegion(baseId, targetBaseTile->region))
				{
					// sea and air only
					if (!(triad == TRIAD_SEA || triad == TRIAD_AIR))
					{
						debug("\t\tocean base, connected ocean region: sea and air only\n");
						continue;
					}
					
				}
				// not connected ocean region
				else
				{
					// air only
					if (!(triad == TRIAD_AIR))
					{
						debug("\t\tocean base, not connected ocean region: air only\n");
						continue;
					}
					
				}
				
			}
			
		}
		// land base
		else
		{
			// no sea attackers
			
			if (triad == TRIAD_SEA)
			{
				debug("\t\tland base: no sea units\n");
				continue;
			}
			
			// same base
			if (baseId == targetBaseId)
			{
				// no restrictions
			}
			// other base
			else
			{
				// same region
				if (baseTile->region == targetBaseTile->region)
				{
					// land and air only
					if (!(triad == TRIAD_LAND || triad == TRIAD_AIR))
					{
						debug("\t\tland base, same region: land and air only\n");
						continue;
					}
					
				}
				// not same region
				else
				{
					// air only
					if (!(triad == TRIAD_AIR))
					{
						debug("\t\tland base, not same region: air only\n");
						continue;
					}
					
				}
				
			}
			
		}
		
		// exclude regular with mediocre weapon or armor
		
		if
		(
			!isNativeUnit(unitId)
			&&
			unitWeaponOffenseValue > 1 && unitWeaponOffenseValue < aiData.bestWeaponOffenseValue
			&&
			unitArmorDefenseValue > 1 && unitArmorDefenseValue < aiData.bestArmorDefenseValue
		)
		{
			debug("\t\tmediocre unit\n");
			continue;
		}
		
		// calculate unit strength
		
		double psiOffenseStrenght = getUnitPsiOffenseStrength(unitId, baseId);
		double psiDefenseStrenght = getUnitPsiDefenseStrength(unitId, baseId, true);
		
		double conventionalOffenseStrenght = getUnitConventionalOffenseStrength(unitId, baseId);
		double conventionalDefenseStrenght = getUnitConventionalDefenseStrength(unitId, baseId, true);
		
		// determine unit relative strength
		
		double totalWeight = 0.0;
		double totalWeightedRelativeStrength = 0.0;
		
		for (std::pair<const int, UnitStrength> &opponentUnitStrengthEntry : opponentStrength->unitStrengths)
		{
			int opponentUnitId = opponentUnitStrengthEntry.first;
			UnitStrength *opponentUnitStrength = &(opponentUnitStrengthEntry.second);
			
			UNIT *opponentUnit = &(Units[opponentUnitId]);
			
			// impossible combinations
			
			// two needlejets without air superiority
			if
			(
				(opponentUnit->chassis_type == CHS_NEEDLEJET && !unit_has_ability(defenderUnitId, ABL_AIR_SUPERIORITY))
				&&
				(defenderUnit->chassis_type == CHS_NEEDLEJET && !unit_has_ability(opponentUnitId, ABL_AIR_SUPERIORITY))
			)
			{
				continue;
			}
			
			// calculate odds
			
			double attackOdds;
			double defendOdds;
			
			if (isNativeUnit(opponentUnitId) || isNativeUnit(defenderUnitId))
			{
				attackOdds =
					opponentUnitStrength->psiOffense
					/
					psiDefenseStrenght
				;
				
				defendOdds =
					opponentUnitStrength->psiDefense
					/
					psiOffenseStrenght
				;
				
			}
			else
			{
				attackOdds =
					opponentUnitStrength->conventionalOffense
					/
					conventionalDefenseStrenght
					// type to type combat modifier (opponent attacks)
					*
					getConventionalCombatBonusMultiplier(opponentUnitId, defenderUnitId)
				;
				
				defendOdds =
					opponentUnitStrength->conventionalDefense
					/
					conventionalOffenseStrenght
					// type to type combat modifier (defender attacks)
					/
					getConventionalCombatBonusMultiplier(defenderUnitId, opponentUnitId)
				;
				
			}
			
			// calculate attack and defend probabilities
			
			double attackProbability;
			double defendProbability;
			
			// defender cannot attack
			if
			(
				// unit without air superiority cannot attack neeglejet
				(opponentUnit->chassis_type == CHS_NEEDLEJET && !unit_has_ability(defenderUnitId, ABL_AIR_SUPERIORITY))
				||
				// land unit cannot attack from sea base
				(ocean && defenderUnit->triad() == TRIAD_LAND)
			)
			{
				attackProbability = 1.0;
				defendProbability = 0.0;
			}
			// opponent cannot attack
			else if
			(
				// unit without air superiority cannot attack neeglejet
				(defenderUnit->chassis_type == CHS_NEEDLEJET && !unit_has_ability(opponentUnitId, ABL_AIR_SUPERIORITY))
			)
			{
				attackProbability = 0.0;
				defendProbability = 1.0;
			}
			// opponent disengages
			else if
			(
				(
					(opponentUnit->triad() == TRIAD_LAND && defenderUnit->triad() == TRIAD_LAND)
					||
					(opponentUnit->triad() == TRIAD_SEA && defenderUnit->triad() == TRIAD_SEA)
				)
				&&
				(unit_chassis_speed(opponentUnitId) > unit_chassis_speed(defenderUnitId))
			)
			{
				attackProbability = 1.0;
				defendProbability = 0.0;
			}
			// defender disengages
			else if
			(
				(
					(opponentUnit->triad() == TRIAD_LAND && defenderUnit->triad() == TRIAD_LAND)
					||
					(opponentUnit->triad() == TRIAD_SEA && defenderUnit->triad() == TRIAD_SEA)
				)
				&&
				(unit_chassis_speed(defenderUnitId) > unit_chassis_speed(opponentUnitId))
			)
			{
				attackProbability = 0.0;
				defendProbability = 1.0;
			}
			else
			{
				attackProbability = 0.5;
				defendProbability = 0.5;
			}
			
			// work out defender choice
			// defender can choose not to attack if it worse than defending
			
			if
			(
				defendProbability > 0.0
				&&
				attackOdds < defendOdds
			)
			{
				attackProbability = 1.0;
				defendProbability = 0.0;
			}
			
			// calculate defender destroyed
			
			double defenderDestroyedTypeVsType =
				(
					attackProbability * attackOdds
					+
					defendProbability * defendOdds
				)
			;
			
			debug
			(
				"\t\t\topponent: %-32s weight=%f\n\t\t\tdefender: %-32s\n\t\t\t\tattack: probability=%f, odds=%f\n\t\t\t\tdefend: probatility=%f, odds=%f\n\t\t\t\tdefenderDestroyedTypeVsType=%f\n",
				opponentUnit->name,
				opponentUnitStrength->weight,
				defenderUnit->name,
				attackProbability,
				attackOdds,
				defendProbability,
				defendOdds,
				defenderDestroyedTypeVsType
			)
			;
			
			// calculate weightedRelativeStrength
			
			double weightedRelativeStrength = opponentUnitStrength->weight * (1.0 / defenderDestroyedTypeVsType);
			debug("\t\topponentUnitStrength->weight=%f, weightedRelativeStrength=%f\n", opponentUnitStrength->weight, weightedRelativeStrength);
			
			// update totals
			
			totalWeight += opponentUnitStrength->weight;
			totalWeightedRelativeStrength += weightedRelativeStrength;
			
		}
		
		// calculate normalizedRelativeStrength
		
		double normalizedRelativeStrength = totalWeightedRelativeStrength / totalWeight;
		debug("\t\tnormalizedRelativeStrength=%f\n", normalizedRelativeStrength);
		
		// get maintenance cost
		
		int maintenanceCost = getUnitMaintenanceCost(unitId);
		debug("\t\tmaintenanceCost=%d\n", maintenanceCost);
		
		// calculate effectiveness
		
		double effectiveness = normalizedRelativeStrength / (double)maintenanceCost;
		debug("\t\teffectiveness=%f\n", effectiveness);
		
		// add to list and to total
		
		unitEffectivenesses.insert({unitId, effectiveness});
		
		// update best in class
		
		int unitClass = getUnitClass(unitId);
		debug("\t\tunitClass=%d\n", unitClass);
		
		if (unitClassBestUnits.find(unitClass) == unitClassBestUnits.end())
		{
			unitClassBestUnits[unitClass] = unitId;
			debug("\t\tbest in class\n");
		}
		else
		{
			int bestUnitId = unitClassBestUnits[unitClass];
			double bestUnitEffectiveness = unitEffectivenesses[bestUnitId];
			
			if (effectiveness > bestUnitEffectiveness)
			{
				unitClassBestUnits[unitClass] = unitId;
				debug("\t\tbest in class\n");
			}
			
		}
		
	}
	
	if (unitClassBestUnits.size() == 0)
		return -1;
	
	// calculate existing unit class proportions
	
	std::map<int, double> existingUnitClassWeights;
	
	for (const std::pair<int, UnitStrength> &defenderUnitStrengthEntry : defenderStrength->unitStrengths)
	{
		int unitId = defenderUnitStrengthEntry.first;
		const UnitStrength *unitStrength = &(defenderUnitStrengthEntry.second);
		
		int unitClass = getUnitClass(unitId);
		
		existingUnitClassWeights[unitClass] += unitStrength->weight;
		
	}
	
	// select class to produce
	
	debug("\tselect unit class to produce\n");
	
	int bestUnitClass = -1;
	double bestUnitClassPriority = 0.0;
	
	for (const std::pair<int, int> &unitClassBestUnitEntry : unitClassBestUnits)
	{
		int unitClass = unitClassBestUnitEntry.first;
		
		debug("\t\tunitClass=%d\n", unitClass);
		
		// get existing unit class proportion
		
		std::map<int, double>::iterator existingUnitClassWeightsIterator = existingUnitClassWeights.find(unitClass);
		double existingUnitClassWeight = (existingUnitClassWeightsIterator == existingUnitClassWeights.end() ? 0.0 : existingUnitClassWeightsIterator->second);
		
		// calculate priority
		
		double priority = 1.0 - (existingUnitClassWeight / COMBAT_UNIT_CLASS_WEIGHTS[unitClass]);
		debug("\t\tCOMBAT_UNIT_CLASS_WEIGHT=%f, existingUnitClassWeight=%f, priority=%f\n", COMBAT_UNIT_CLASS_WEIGHTS[unitClass], existingUnitClassWeight, priority);
		
		// update best
		
		if (bestUnitClass == -1 || priority > bestUnitClassPriority)
		{
			bestUnitClass = unitClass;
			bestUnitClassPriority = priority;
		}
		
	}
	
	if (bestUnitClass == -1)
		return -1;
	
	// return best unit in class
	
	return unitClassBestUnits[bestUnitClass];
	
}

void evaluateTransportDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = &(Bases[baseId]);
	
	debug("evaluateTransportDemand\n");
	
	for (const auto &k : aiData.seaTransportRequestCounts)
	{
		int association = k.first;
		int seaTransportDemand = k.second;
		
		// no demand
		
		if (seaTransportDemand <= 0)
		{
			debug("seaTransportDemand <= 0\n");
			continue;
		}
		
		// base in association
		
		if (getBaseOceanAssociation(baseId) != association)
			continue;
		
		debug("\tassociation=%d\n", association);
		
		// count number of transports in association
		
		double transportCount = 0.0;
		
		for (int vehicleId : aiData.vehicleIds)
		{
			if
			(
				isTransportVehicle(vehicleId)
				&&
				getVehicleAssociation(vehicleId) == association
			)
			{
				// adjust value for capacity and speed
				
				transportCount +=
					0.75
					*
					(double)tx_veh_cargo(vehicleId)
					*
					((double)Units[Vehicles[vehicleId].unit_id].speed() / (double)Units[BSC_TRANSPORT_FOIL].speed())
				;
				
			}
			
		}
		
		// demand is satisfied
		
		if (transportCount >= (double)seaTransportDemand)
			continue;
		
		// calculate demand
		
		double transportDemand = 1.0 - transportCount / (double)seaTransportDemand;
		
		debug("\tseaTransportDemand=%d, transportCount=%f, transportDemand=%f\n", seaTransportDemand, transportCount, transportDemand);
		
		// set priority
		
		double priority = conf.ai_production_transport_priority * transportDemand * ((double)base->mineral_surplus / (double)aiData.maxMineralSurplus);
		
		debug("\tpriority=%f\n", priority);
		
		// find transport unit
		
		int unitId = findTransportUnit();
		
		if (unitId == -1)
		{
			debug("\tno transport unit");
			continue;
		}
		
		debug("\t%s\n", Units[unitId].name);
		
		// set demand
		
		addProductionDemand(unitId, priority);
		
	}
	
}

int findTransportUnit()
{
	int bestUnitId = -1;
	double bestUnitEffectiveness = 0.0;
	
	for (int unitId : aiData.unitIds)
	{
		UNIT *unit = &(Units[unitId]);
		int triad = unit->triad();
		
		// sea
		
		if (triad != TRIAD_SEA)
			continue;
		
		// transport
		
		if (!isTransportUnit(unitId))
			continue;
		
		// calculate effectiveness
		
		double effectiveness = 1.0;
		
		// faster unit
		
		effectiveness *= unit->speed();
		
		// more capacity unit
		
		if (unit_has_ability(unitId, ABL_HEAVY_TRANSPORT))
		{
			effectiveness *= 1.5;
		}
		
		// update best
		
		if (bestUnitId == -1 || effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}
		
	}
	
	return bestUnitId;
	
}

int getUnitClass(int unitId)
{
	UNIT *unit = &(Units[unitId]);
	int unitWeaponOffenseValue = getUnitWeaponOffenseValue(unitId);
	
	if (!isCombatUnit(unitId))
	{
		return UC_NOT_COMBAT;
	}
	
	switch (unit->triad())
	{
	case TRIAD_LAND:
		
		// that includes spore launcher
		if (unit_has_ability(unitId, ABL_ARTILLERY))
		{
			return UC_LAND_ARTILLERY;
		}
		else
		{
			if (unit_chassis_speed(unitId) > 1)
			{
				return UC_LAND_FAST_ATTACKER;
			}
			else
			{
				if (unitWeaponOffenseValue == 1)
				{
					return UC_INFANTRY_DEFENDER;
				}
				// that includes mind worms
				else
				{
					return UC_INFANTRY_ATTACKER;
				}
				
			}
			
		}
		
		break;
		
	case TRIAD_SEA:
		
		// that includes sealurk and isle of deep
		return UC_SHIP;
		
		break;
		
	case TRIAD_AIR:
		
		if (unit_has_ability(unitId, ABL_AIR_SUPERIORITY))
		{
			return UC_INTERCEPTOR;
		}
		else
		{
			return UC_BOMBER;
		}
		
	}
	
	return UC_NOT_COMBAT;
	
}

int getFirstUnbuiltFacilityFromList(int baseId, std::vector<int> facilityIds)
{
	for (int facilityId : facilityIds)
	{
		if (!has_facility(baseId, facilityId))
		{
			return facilityId;
		}
		
	}
	
	// everything is built
	
	return -1;
	
}

