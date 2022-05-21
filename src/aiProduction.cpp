#include <math.h>
#include <set>
#include <map>
#include "engine.h"
#include "game.h"
#include "game_wtp.h"
#include "terranx_wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiProduction.h"
#include "aiMoveColony.h"

// combat priority used globally

double globalMilitaryPriority;
double regionMilitaryPriority;

// global parameters

double hurryCost;
double averageBaseMineralSurplus;
double averagePerCitizenIncome;
double averageBaseProjectedIncomeGrowth;
double normalGain;

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
	// populate global variables
	
	evaluateGlobalBaseDemand();
	evaluateGlobalFormerDemand();
	evaluabeGlobalProtectionDemand();
	
	// set production
	
	setProduction();
	
}

void setupProductionVariables()
{
	debug("setupProductionVariables - %s\n", MFactions[aiFactionId].noun_faction);
	
	// set energy to mineral conversion ratio
	
	int hurryCostUnit = (conf.flat_hurry_cost ? conf.flat_hurry_cost_multiplier_unit : 4);
	int hurryCostFacility = (conf.flat_hurry_cost ? conf.flat_hurry_cost_multiplier_facility : 2);

	hurryCost = ((double)hurryCostUnit + (double)hurryCostFacility) / 2.0;
	
	// calculate base and per citizen averages
	
	double sumBaseMineralSurplus = 0.0;
	double sumPerCitizenIncome = 0.0;
	double sumBaseProjectedIncomeGrowth = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		sumBaseMineralSurplus += (double)(base->mineral_surplus);
		sumPerCitizenIncome += getBaseIncome(baseId) / (double)base->pop_size;
		sumBaseProjectedIncomeGrowth += (getBaseIncome(baseId) / (double)base->pop_size) * ((double)base->nutrient_surplus / (double)((base->pop_size + 1) * cost_factor(base->faction_id, 0, baseId)));
		
	}
	
	averageBaseMineralSurplus = (aiData.baseIds.size() == 0 ? 0.0 : sumBaseMineralSurplus / (double)aiData.baseIds.size());
	averagePerCitizenIncome = (aiData.baseIds.size() == 0 ? 0.0 : sumPerCitizenIncome / (double)aiData.baseIds.size());
	averageBaseProjectedIncomeGrowth = (aiData.baseIds.size() == 0 ? 0.0 : sumBaseProjectedIncomeGrowth / (double)aiData.baseIds.size());
	
	debug("\taverageBaseMineralSurplus = %f\n", averageBaseMineralSurplus);
	debug("\taveragePerCitizenIncome = %f\n", averagePerCitizenIncome);
	debug("\taverageBaseProjectedIncomeGrowth = %f\n", averageBaseProjectedIncomeGrowth);
	
	// calculate colony gain for debugging purposes
	
	double productionTime = (double)(Units[BSC_COLONY_POD].cost * 10) / averageBaseMineralSurplus;
	double travelTime = 5.0;
	
	double colonyGain =
		// income loss due to depopulation
		-
		getIncomeGain(averagePerCitizenIncome, productionTime)
		// income gain due to founding new base with delay
		+
		getIncomeGain(averagePerCitizenIncome, productionTime + travelTime)
		// growth gain due to founding new base with delay
		+
		getGrowthGain(averageBaseProjectedIncomeGrowth, productionTime + travelTime)
	;
	
	debug("\tcolonyGain=%+f, productionTime=%f, travelTime=%f, depopulation=%+f, population=%+f, growth=%+f\n", colonyGain, productionTime, travelTime, -getIncomeGain(averagePerCitizenIncome, productionTime), +getIncomeGain(averagePerCitizenIncome, productionTime + travelTime), +getGrowthGain(averageBaseProjectedIncomeGrowth, productionTime + travelTime));
	
	// set normal gain to colony gain
	
	normalGain = colonyGain;
	
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
	// occupancy coverage
	// --------------------------------------------------
	
	int ownedTileCount = 0;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		
		if (tile->owner != -1)
		{
			ownedTileCount++;
		}
		
	}
	
	double occupancyCoverageThreshold = 0.25;
	double occupancyCoverage = (double)ownedTileCount / (double)*map_area_tiles;
	
	// increase base demand with low map occupancy
	
	if (occupancyCoverage < occupancyCoverageThreshold)
	{
		globalBaseDemand *= 1.0 + 0.25 * (1.0 - occupancyCoverage / occupancyCoverageThreshold);
	}
	
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
	
}

void evaluateGlobalFormerDemand()
{
	debug("evaluateGlobalFormerDemand - %s\n", MFactions[aiFactionId].noun_faction);
	
	// reset demand variables
	
	aiData.production.landFormerDemand = 0.0;
	aiData.production.seaFormerDemands.clear();
	
	// count formers
	
	debug("\tcount formers\n");
	
	int airFormerCount = 0;
	int landFormerCount = 0;
	std::map<int, int> seaFormerCounts;
	
	for (int vehicleId : aiData.formerVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		switch (vehicle->triad())
		{
		case TRIAD_AIR:
			airFormerCount++;
			break;
			
		case TRIAD_LAND:
			landFormerCount++;
			break;
			
		case TRIAD_SEA:
			{
				int vehicleAssociation = getVehicleAssociation(vehicleId);
				
				if (vehicleAssociation == -1)
					break;
				
				if (seaFormerCounts.count(vehicleAssociation) == 0)
				{
					seaFormerCounts.insert({vehicleAssociation, 0});
				}
				seaFormerCounts.at(vehicleAssociation)++;
				
				break;
				
			}
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("\t\t%3d air\n", airFormerCount);
		debug("\t\t%3d land\n", landFormerCount);
		for (const auto &seaFormerCountEntry : seaFormerCounts)
		{
			debug("\t\t%3d sea[%3d]\n", seaFormerCountEntry.second, seaFormerCountEntry.first);
		}
	}
	
	// compute demands
	
	if (aiData.production.landTerraformingRequestCount > 0)
	{
		aiData.production.landFormerDemand = 1.0 - (double)(landFormerCount + airFormerCount) / (conf.ai_production_improvement_coverage * (double)aiData.production.landTerraformingRequestCount);
		debug("\tlandFormerDemand=%f, landFormerCount=%d, airFormerCount=%d, ai_production_improvement_coverage=%f, landTerraformingRequestCount=%d\n", aiData.production.landFormerDemand, landFormerCount, airFormerCount, conf.ai_production_improvement_coverage, aiData.production.landTerraformingRequestCount);
	}
	
	for (const auto &seaTerraformingRequestCountEntry : aiData.production.seaTerraformingRequestCounts)
	{
		int oceanAssociation = seaTerraformingRequestCountEntry.first;
		int seaTerraformingRequestCount = seaTerraformingRequestCountEntry.second;
		int seaFormerCount = (seaFormerCounts.count(oceanAssociation) == 0 ? 0 : seaFormerCounts.at(oceanAssociation));
		
		if (seaTerraformingRequestCount > 0)
		{
			double seaFormerDemand = 1.0 - (double)(seaFormerCount + airFormerCount) / (conf.ai_production_improvement_coverage * (double)seaTerraformingRequestCount);
			aiData.production.seaFormerDemands.insert({oceanAssociation, seaFormerDemand});
			debug("\tseaFormerDemand(%3d)=%f, seaFormerCount=%d, airFormerCount=%d, ai_production_improvement_coverage=%f, seaTerraformingRequestCount=%d\n", oceanAssociation, seaFormerDemand, seaFormerCount, airFormerCount, conf.ai_production_improvement_coverage, seaTerraformingRequestCount);
		}
		
	}
	
}

void evaluabeGlobalProtectionDemand()
{
	// find max defense demand
	
	double globalProtectionDemand = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		
		for (unsigned int unitType = 0; unitType < UNIT_TYPE_COUNT; unitType++)
		{
			UnitTypeInfo *unitTypeInfo = &(baseInfo->unitTypeInfos[unitType]);
			
			if (unitTypeInfo->providedProtection < unitTypeInfo->requiredProtection)
			{
				double demand = 1.0 - unitTypeInfo->providedProtection / unitTypeInfo->requiredProtection;
				
				globalProtectionDemand = std::max(globalProtectionDemand, demand);
				
			}
			
		}
		
	}
	
	aiData.production.globalProtectionDemand = globalProtectionDemand;
	
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
		
		globalMilitaryPriority = std::max(conf.ai_production_military_priority_minimal, conf.ai_production_military_priority * aiData.production.globalProtectionDemand);
		debug("globalMilitaryPriority = %f\n", globalMilitaryPriority);
		
		// calculate vanilla priority

		double vanillaPriority = 0.0;
		
		// unit
		if (choice > 0)
		{
			// only not managed unit types
			
			if (MANAGED_UNIT_TYPES.count(choice) == 0)
			{
				vanillaPriority = conf.ai_production_vanilla_priority_unit * getUnitPriorityMultiplier(baseId, choice);
			}
			
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
    
	// default: stockpile energy
	
	addProductionDemand(-FAC_STOCKPILE_ENERGY, 0.25);
	
	// mandatory defense
	
	evaluateMandatoryDefenseDemand();
	
	// evaluate other demands
	
	evaluateFacilitiesDemand();
	evaluateProjectDemand();
	
	evaluatePoliceDemand();

	evaluateLandFormerDemand();
	evaluateSeaFormerDemand();

	evaluateLandColonyDemand();
	evaluateSeaColonyDemand();
	evaluateAirColonyDemand();

	evaluateAlienDefenseDemand();
	evaluateAlienHuntingDemand();
	
	evaluatePodPoppingDemand();

	evaluatePrototypingDemand();
	evaluateCombatDemand();
	evaluateAlienProtectorDemand();
	
	evaluateTransportDemand();

	return productionDemand.item;

}

/*
Generates mandatory defense demand and return true if generated.
*/
void evaluateMandatoryDefenseDemand()
{
	int baseId = productionDemand.baseId;
	
	debug("evaluateMandatoryDefenseDemand\n");
	
	// validate base is not protected
	
	if (getBaseGarrison(baseId).size() > 0)
		return;
	
	// find best anti-native defense unit
	
	int unitId = findNativeDefenderUnit();
	
	// no unit found
	
	if (unitId == -1)
	{
		debug("\tno anti-native defense unit found\n");
		return;
	}
	
	debug("\t%s\n", Units[unitId].name);
	
	// set priority

	double priority = 4.0;

	debug("\t\tpriority=%f\n", priority);
	
	// add production demand

	addProductionDemand(unitId, priority);
	
}

/*
Crude project offer - by cost only.
*/
void evaluateProjectDemand()
{
	int baseId = productionDemand.baseId;

	debug("evaluateProjectDemand\n");
	
	// find cheapest project
	
	int cheapestProjectFacilityId = -1;
	int cheapestProjectProductionTurns = INT_MAX;
	
	for (int projectFacilityId = FAC_HUMAN_GENOME_PROJ; projectFacilityId <= FAC_PLANETARY_ENERGY_GRID; projectFacilityId++)
	{
		// exclude not available project
		
		if (!isProjectAvailable(aiFactionId, projectFacilityId))
			continue;
		
		// get production turns
		
		int productionTurns = estimateBaseProductionTurnsToCompleteItem(baseId, -projectFacilityId);
		
		// update best
		
		if (productionTurns < cheapestProjectProductionTurns)
		{
			cheapestProjectFacilityId = projectFacilityId;
			cheapestProjectProductionTurns = productionTurns;
		}
		
	}
	
	// not found
	
	if (cheapestProjectFacilityId == -1)
		return;
	
	// calculate priority
	
	double priority = 20.0 / (double)cheapestProjectProductionTurns;
	
	// add demand
	
	addProductionDemand(-cheapestProjectFacilityId, priority);
	
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
	
	int facilityId = getFirstUnbuiltAvailableFacilityFromList(baseId, {FAC_HAB_COMPLEX, FAC_HABITATION_DOME});
	
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

	int projectedPopulation = getProjectedBasePopulation(baseId, buildTurns);
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
			priority *= 3.0;
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
	
	baseColonyDemandMultiplier += std::min(0.25, 0.25 * priority);
	debug("> baseColonyDemandMultiplier=%f\n", baseColonyDemandMultiplier);
	
}

void evaluatePsychFacilitiesDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("\tevaluatePsychFacilitiesDemand\n");

	// determine next facility to build
	
	int facilityId = getFirstUnbuiltAvailableFacilityFromList(baseId, {FAC_RECREATION_COMMONS, FAC_HOLOGRAM_THEATRE, FAC_PARADISE_GARDEN,});
	
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

	int projectedPopulation = getProjectedBasePopulation(baseId, buildTurns);
	int projectedPopulationIncrease = projectedPopulation - base->pop_size;
	debug("\t\tcurrentPopulation=%d, projectedTurns=%d, projectedPopulation=%d, projectedPopulationIncrease=%d\n", base->pop_size, buildTurns, projectedPopulation, projectedPopulationIncrease);
	
	// no expected riots in future

	if (!(doctors > 0 || (base->drone_total > 0 && base->drone_total + projectedPopulationIncrease > base->talent_total)))
	{
		debug("\t\tno expected riots in future\n");
		return;
	}
	
	// set priority
	
	double priority = conf.ai_production_psych_priority * (double)std::max(1, doctors);
	debug("\t\tpriority=%f\n", priority);
	
	// add demand
	
	addProductionDemand(-facilityId, priority);
	
	// add additional colony demand
	
	baseColonyDemandMultiplier += std::min(0.25, 0.25 * priority);
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
	
	MFaction *metaFaction = &(MFactions[aiFactionId]);

	debug("\tevaluateMultiplyingFacilitiesDemand\n");
	
	// calculate base values for multiplication

	int mineralSurplus = base->mineral_surplus;
	int mineralIntake = base->mineral_intake;
	int economyIntake = getBaseEconomyIntake(baseId);
	int labsIntake = getBaseLabsIntake(baseId);
	int psychIntake = getBasePsychIntake(baseId);
	
	debug("\t\tmineralSurplus=%d, mineralIntake=%d, economyIntake=%d, psychIntake=%d, labsIntake=%d\n", mineralSurplus, mineralIntake, economyIntake, psychIntake, labsIntake);
	
	// no production power
	
	if (mineralSurplus <= 0)
	{
		debug("mineralSurplus <= 0\n");
		return;
	}
	
	// base variables
	
	double basePopulationGrowth = getBasePopulationGrowth(baseId);
	double baseGrowthRate = basePopulationGrowth / (double)base->pop_size;
		
	// multipliers
	// bit flag: 1 = minerals, 2 = economy, 4 = psych, 8 = labs, 16 = fixed labs for biology lab (value >> 16 = fixed value)
	// for simplicity sake, hospitals are assumed to add 50% psych instead of 25% psych and -1 drone
	
	const int MULTIPLYING_FACILITIES[][2] =
	{
		{FAC_RECYCLING_TANKS, 1},
		{FAC_ENERGY_BANK, 2},
		{FAC_NETWORK_NODE, 8},
		{FAC_BIOLOGY_LAB, 16 + (conf.biology_lab_research_bonus << 16),},
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
	
	// iterate facilities
	
	for (const int *multiplyingFacility : MULTIPLYING_FACILITIES)
	{
		int facilityId = multiplyingFacility[0];
		int mask = multiplyingFacility[1];
		CFacility *facility = &(Facility[facilityId]);

		// check if base can build facility
		
		if (!isBaseFacilityAvailable(baseId, facilityId))
			continue;
		
		debug("\t\t%s\n", facility->name);
		
		// production time
		
		double productionTime = (double)getBaseMineralCost(baseId, -facilityId) / (double)base->mineral_surplus;
		
		// accumulate gain
		
		double gain = 0.0;
		double gainDelta;
		
		// minerals multiplication
		
		if (mask & 1)
		{
			double mineralBonus = (double)mineralIntake / 2.0;
			double bonusResourceScore = getResourceScore(mineralBonus, 0);
			
			// increase in income
			
			gainDelta = +getIncomeGain(bonusResourceScore, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (mineral multiplication income)\n", gainDelta);
			
			// increase in growth
			
			gainDelta = +getGrowthGain(bonusResourceScore * baseGrowthRate, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (mineral multiplication growth)\n", gainDelta);
			
		}
		
		// energy multiplication
		
		if ((mask & 2) != 0 || (mask & 8) != 0 || (mask & 4) != 0)
		{
			double economyBonus = 0.0;
			double labsBonus = 0.0;
			double psychBonus = 0.0;
			
			if ((mask & 2) != 0)
			{
				economyBonus += (double)economyIntake / 2.0;
			}
			
			if ((mask & 8) != 0)
			{
				economyBonus += (double)labsIntake / 2.0;
			}
			
			if ((mask & 4) != 0)
			{
				psychBonus += (double)psychIntake / 2.0;
			}
			
			// reduce labs bonus appeal for SHARETECH/TECHSHARE factions
			
			double sharetechCoefficient = 1.0;
			
			if (metaFaction->rule_sharetech > 0)
			{
				sharetechCoefficient = 0.1 * metaFaction->rule_sharetech;
				
				if (conf.infiltration_expire && isFactionSpecial(aiFactionId, FACT_TECHSHARE))
				{
					sharetechCoefficient *= 2.0;
				}
				
			}
			
			sharetechCoefficient = std::min(1.0, sharetechCoefficient);
			
			labsBonus *= sharetechCoefficient;
			
			// summarize energy bonus
			
			double energyBonus = economyBonus + labsBonus + psychBonus;
			
			// calculate bonusResourceScore
			
			double bonusResourceScore = getResourceScore(0, energyBonus);
			
			// increase in income
			
			gainDelta = +getIncomeGain(bonusResourceScore, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (energy multiplication income)\n", gainDelta);
			
			// increase in growth
			
			gainDelta = +getGrowthGain(bonusResourceScore * baseGrowthRate, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (energy multiplication growth)\n", gainDelta);
			
		}
		
		// labs fixed
		
		if (mask & 16)
		{
			double energyBonus = (double)(mask >> 16);
			double bonusResourceScore = getResourceScore(0, energyBonus);
			
			// increase in income
			
			gainDelta = +getIncomeGain(bonusResourceScore, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (energy fixed income)\n", gainDelta);
			
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
			
			// count bonuses
			
			double nutrientBonus = 0.0;
			double energyBonus = 0.0;
			
			switch (facilityId)
			{
			case FAC_TREE_FARM:
				// +1 nutrient
				nutrientBonus += potentialImprovementCount;
				break;
				
			case FAC_HYBRID_FOREST:
				// +1 nutrient +1 energy
				nutrientBonus += potentialImprovementCount;
				energyBonus += potentialImprovementCount;
				break;
				
			}
			
			// calculate gain
			
			double bonusResourceScore = getResourceScore(0, energyBonus);
			
			// increase in income
			
			gainDelta = +getIncomeGain(bonusResourceScore, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (resource increase income)\n", gainDelta);
			
			// increase in growth due to regular population growth
			
			gainDelta = +getGrowthGain(bonusResourceScore * baseGrowthRate, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (resource increase growth)\n", gainDelta);
			
			// increase in growth due to extra nutrients
			
			gainDelta = +getGrowthGain(averageBaseProjectedIncomeGrowth * (nutrientBonus / (double)std::max(1, base->nutrient_surplus)), productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (population increase growth)\n", gainDelta);
			
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
			
			// count bonuses
			
			double nutrientBonus = potentialImprovementCount;
			
			// calculate gain
			
			// increase in growth due to extra nutrients
			
			gainDelta = +getGrowthGain(averageBaseProjectedIncomeGrowth * (nutrientBonus / (double)std::max(1, base->nutrient_surplus)), productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (population increase growth)\n", gainDelta);
			
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
			
			// count bonuses
			
			double mineralBonus = potentialImprovementCount;
			
			// calculate gain
			
			double bonusResourceScore = getResourceScore(mineralBonus, 0);
			
			// increase in income
			
			gainDelta = +getIncomeGain(bonusResourceScore, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (resource increase income)\n", gainDelta);
			
			// increase in growth due to regular population growth
			
			gainDelta = +getGrowthGain(bonusResourceScore * baseGrowthRate, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (resource increase growth)\n", gainDelta);
			
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
			
			// count bonuses
			
			double energyBonus = potentialImprovementCount;
			
			// calculate gain
			
			double bonusResourceScore = getResourceScore(0, energyBonus);
			
			// increase in income
			
			gainDelta = +getIncomeGain(bonusResourceScore, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (resource increase income)\n", gainDelta);
			
			// increase in growth due to regular population growth
			
			gainDelta = +getGrowthGain(bonusResourceScore * baseGrowthRate, productionTime);
			gain += gainDelta;
			debug("\t\t\t%+4.0f (resource increase growth)\n", gainDelta);
			
		}
		
		// factor maintenance
		
		gainDelta = -getIncomeGain(getResourceScore(0, facility->maint), productionTime);
		gain += gainDelta;
		debug("\t\t\t%+4.0f (maintenance)\n", gainDelta);
		
		// normalize rate
		
		double normalizedGain = getNormalizedGain(gain);
		
		// calculate priority
		
		double priority = conf.ai_production_multiplying_facility_priority * normalizedGain;
		debug("\t\t\tpriority=%+f, gain=%+f, normalizedGain=%+f, ai_production_multiplying_facility_priority=%+f\n", priority, gain, normalizedGain, conf.ai_production_multiplying_facility_priority);
		
		// priority is too low
		
		if (priority <= 0.0)
		{
			debug("\t\t\tpriority <= 0.0\n");
			continue;
		}
		
		// add demand
		
		addProductionDemand(-facilityId, priority);
		
	}
	
}

void evaluateDefensiveFacilitiesDemand()
{
	debug("\tevaluateDefensiveFacilitiesDemand\n");
	
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	bool ocean = is_ocean(baseTile);
	BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
	
	// select facility
	
	int facilityId = -1;
	
	if (!ocean)
	{
		facilityId = getFirstUnbuiltAvailableFacilityFromList(baseId, {FAC_PERIMETER_DEFENSE, FAC_AEROSPACE_COMPLEX, FAC_TACHYON_FIELD});
	}
	else
	{
		facilityId = getFirstUnbuiltAvailableFacilityFromList(baseId, {FAC_NAVAL_YARD, FAC_AEROSPACE_COMPLEX, FAC_TACHYON_FIELD});
	}
	
	// nothing selected
	
	if (facilityId == -1)
		return;
	
	// calculate exposure
	
	double exposure;
	
	if (facilityId == FAC_AEROSPACE_COMPLEX)
	{
		exposure = 1.5 * exp(- (double)baseInfo->borderBaseDistance / 20.0);
	}
	else
	{
		if (!ocean)
		{
			exposure = 1.5 * exp(- (double)baseInfo->borderBaseDistance / 10.0);
		}
		else
		{
			exposure = 1.5 * exp(- (double)baseInfo->borderBaseDistance / 20.0);
		}
		
	}
	
	// calculate priority
	// defensive facility is more desirable when closer to border
	
	double priority = exposure * log(1.0 + std::max(0.0, std::min(baseInfo->unitTypeInfos[UT_MELEE].requiredProtection, baseInfo->unitTypeInfos[UT_MELEE].ownTotalCombatEffect)));
	
	// add demand

	addProductionDemand(-facilityId, priority);

	debug("\t\tfacility=%-25s priority=%f, ai_production_military_priority=%5.2f, borderBaseDistance=%d, exposure=%f, meleeProtection.required=%5.2f, ownTotalCombatEffect=%5.2f\n", Facility[facilityId].name, priority, conf.ai_production_military_priority, baseInfo->borderBaseDistance, exposure, baseInfo->unitTypeInfos[UT_MELEE].requiredProtection, baseInfo->unitTypeInfos[UT_MELEE].ownTotalCombatEffect);
	
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
	
	// military facility requires some minimal mineral intake
	
	if (base->mineral_surplus < 10)
	{
		debug("\t\tbase->mineral_surplus < 10\n");
		return;
	}
	
	// priority coefficient
	
	double mineralSurplusCoefficient = ((double)base->mineral_surplus / (double)aiData.maxMineralSurplus);
	double priorityCoefficient = pow(mineralSurplusCoefficient, 2);
	
	// command center
	
	if (has_facility_tech(aiFactionId, FAC_COMMAND_CENTER) && !has_facility(baseId, FAC_COMMAND_CENTER) && !ocean)
	{
		int facilityId = FAC_COMMAND_CENTER;
		double priority = priorityCoefficient * conf.ai_production_command_center_priority;
		addProductionDemand(-facilityId, priority);
		debug("\t\tfacility=%-25s priority=%5.2f, mineralSurplusCoefficient=%5.2f, priorityCoefficient=%5.2f, ai_production_command_center_priority=%5.2f\n", Facility[facilityId].name, priority, mineralSurplusCoefficient, priorityCoefficient, conf.ai_production_command_center_priority);
	}
	
	// naval yard
	
	if (has_facility_tech(aiFactionId, FAC_NAVAL_YARD) && !has_facility(baseId, FAC_NAVAL_YARD) && (ocean || connectedOceanRegions.size() >= 1))
	{
		int facilityId = FAC_NAVAL_YARD;
		double priority = priorityCoefficient * conf.ai_production_naval_yard_priority;
		addProductionDemand(-facilityId, priority);
		debug("\t\tfacility=%-25s priority=%5.2f, mineralSurplusCoefficient=%5.2f, priorityCoefficient=%5.2f, ai_production_naval_yard_priority=%5.2f\n", Facility[facilityId].name, priority, mineralSurplusCoefficient, priorityCoefficient, conf.ai_production_naval_yard_priority);
	}
	
	// aerospace complex
	
	if (has_facility_tech(aiFactionId, FAC_AEROSPACE_COMPLEX) && !has_facility(baseId, FAC_AEROSPACE_COMPLEX))
	{
		int facilityId = FAC_AEROSPACE_COMPLEX;
		double priority = priorityCoefficient * conf.ai_production_aerospace_complex_priority;
		addProductionDemand(-facilityId, priority);
		debug("\t\tfacility=%-25s priority=%5.2f, mineralSurplusCoefficient=%5.2f, priorityCoefficient=%5.2f, ai_production_aerospace_complex_priority=%5.2f\n", Facility[facilityId].name, priority, mineralSurplusCoefficient, priorityCoefficient, conf.ai_production_aerospace_complex_priority);
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

	int allowedPolice = getBasePoliceAllowed(baseId);
	int basePoliceUnitCount = getBaseGarrison(baseId).size();
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
		
		baseColonyDemandMultiplier += std::min(0.5, 0.5 * priority);
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

	debug("\tformerUnit=%-25s\n", Units[formerUnitId].name);

	// set priority

	double priority = conf.ai_production_improvement_priority * (double)aiData.production.landFormerDemand * ((double)base->mineral_surplus / (double)aiData.maxMineralSurplus);

	debug("\tpriority=%f, ai_production_improvement_priority=%f, landFormerDemand=%f, mineral_surplus=%d, maxMineralSurplus=%d\n", priority, conf.ai_production_improvement_priority, aiData.production.landFormerDemand, base->mineral_surplus, aiData.maxMineralSurplus);

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
		debug("\tseaFormerDemand does not exist\n");
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
	
	debug("\tformerUnit=%-25s\n", Units[formerUnitId].name);
	
	// set priority
	
	int maxMineralSurplus = aiData.maxMineralSurplus;
	if (aiData.oceanAssociationMaxMineralSurpluses.count(baseOceanAssociation) != 0)
	{
		maxMineralSurplus = aiData.oceanAssociationMaxMineralSurpluses.at(baseOceanAssociation);
	}

	double priority = conf.ai_production_improvement_priority * seaFormerDemand * ((double)base->mineral_surplus / (double)maxMineralSurplus);

	debug("\tpriority=%f, ai_production_improvement_priority=%f, seaFormerDemand=%f, mineral_surplus=%d, maxMineralSurplus=%d\n", priority, conf.ai_production_improvement_priority, seaFormerDemand, base->mineral_surplus, maxMineralSurplus);

	addProductionDemand(formerUnitId, priority);
	
}

/*
Evaluates need for land colony.
*/
void evaluateLandColonyDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	MAP *baseTile = getBaseMapTile(baseId);

	debug("evaluateLandColonyDemand\n");

	// verify base can build a colony

	if (!canBaseProduceColony(baseId))
	{
		debug("\tbase cannot build a colony\n");
		return;
	}
	
	// no colony units
	
	if (aiData.landColonyUnitIds.size() == 0)
	{
		debug("\tno land colony units\n");
		return;
	}
	
	// populate colony unit production times
	
	std::vector<IdIntValue> colonyUnitProductionTimes;
	
	for (int unitId : aiData.landColonyUnitIds)
	{
		int productionTime = estimateBaseProductionTurnsToCompleteItem(baseId, unitId);
		colonyUnitProductionTimes.push_back({unitId, productionTime});
	}
	
	// find best build site score
	
	MAP *bestBuildSite = nullptr;
	double bestCombinedBuildSiteScore = -DBL_MAX;
	double bestBuildSiteScore = 0.0;
	int optimalColonyUnitId = -1;
	
	for (MAP *tile : buildSites)
	{
		bool ocean = is_ocean(tile);
		ExpansionTileInfo *expansionTileInfo = getExpansionTileInfo(tile);
		
		// exclude unmatching realm
		
		if (ocean)
			continue;
		
		// iterate over available colony units
		
		for (IdIntValue colonyUnitProductionTimeEntry : colonyUnitProductionTimes)
		{
			int unitId = colonyUnitProductionTimeEntry.id;
			int productionTime = colonyUnitProductionTimeEntry.value;
			
			// get travel time and score
			
			double travelTime = estimateTravelTime(baseTile, tile, unitId);
			double travelTimeScore = getBuildSiteTravelTimeScore(travelTime);
			double buildSiteScore = expansionTileInfo->buildScore + travelTimeScore;
			
			// get combined time and score
			
			double combinedTime = (double)productionTime + travelTime;
			double combinedTimeScore = getBuildSiteTravelTimeScore(combinedTime);
			double combinedBuildSiteScore = expansionTileInfo->buildScore + combinedTimeScore;
			
			// update best
			
			if (combinedBuildSiteScore > bestCombinedBuildSiteScore)
			{
				bestBuildSite = tile;
				bestCombinedBuildSiteScore = combinedBuildSiteScore;
				bestBuildSiteScore = buildSiteScore;
				optimalColonyUnitId = unitId;
			}
			
		}
		
	}
	
	// not found
	
	if (bestBuildSite == nullptr)
	{
		debug("\tcannot find build site\n");
		return;
	}
	
	// set colony demand
	
	double landColonyDemand = bestBuildSiteScore;
	
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

	if (optimalColonyUnitId == -1)
	{
		debug("\t\tno land colony unit found\n");
		return;
	}

	debug("\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, optimalColonyUnitId=%-25s\n", TRIAD_LAND, base->mineral_surplus, Units[optimalColonyUnitId].name);

	// set base demand

	addProductionDemand(optimalColonyUnitId, priority);

}

/*
Evaluates need for sea colony.
*/
void evaluateSeaColonyDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	MAP *baseTile = getBaseMapTile(baseId);
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
	
	// no colony units
	
	if (aiData.seaColonyUnitIds.size() == 0)
	{
		debug("\tno sea colony units\n");
		return;
	}
	
	// populate colony unit production times
	
	std::vector<IdIntValue> colonyUnitProductionTimes;
	
	for (int unitId : aiData.seaColonyUnitIds)
	{
		int productionTime = estimateBaseProductionTurnsToCompleteItem(baseId, unitId);
		colonyUnitProductionTimes.push_back({unitId, productionTime});
	}
	
	// find best build site score
	
	MAP *bestBuildSite = nullptr;
	double bestCombinedBuildSiteScore = -DBL_MAX;
	double bestBuildSiteScore = 0.0;
	int optimalColonyUnitId = -1;
	
	for (MAP *tile : buildSites)
	{
		bool ocean = is_ocean(tile);
		int oceanAssociation = getOceanAssociation(tile, aiFactionId);
		ExpansionTileInfo *expansionTileInfo = getExpansionTileInfo(tile);
		
		// exclude unmatching realm
		
		if (!ocean)
			continue;
		
		// exclude different ocean association
		
		if (ocean && oceanAssociation != baseOceanAssociation)
			continue;
		
		// iterate over available colony units
		
		for (IdIntValue colonyUnitProductionTimeEntry : colonyUnitProductionTimes)
		{
			int unitId = colonyUnitProductionTimeEntry.id;
			int productionTime = colonyUnitProductionTimeEntry.value;
			
			// get travel time and score
			
			double travelTime = estimateTravelTime(baseTile, tile, unitId);
			double travelTimeScore = getBuildSiteTravelTimeScore(travelTime);
			double buildSiteScore = expansionTileInfo->buildScore + travelTimeScore;
			
			// get combined time and score
			
			double combinedTime = (double)productionTime + travelTime;
			double combinedTimeScore = getBuildSiteTravelTimeScore(combinedTime);
			double combinedBuildSiteScore = expansionTileInfo->buildScore + combinedTimeScore;
			
			// update best
			
			if (combinedBuildSiteScore > bestCombinedBuildSiteScore)
			{
				bestBuildSite = tile;
				bestCombinedBuildSiteScore = combinedBuildSiteScore;
				bestBuildSiteScore = buildSiteScore;
				optimalColonyUnitId = unitId;
			}
			
		}
		
	}
	
	// not found
	
	if (bestBuildSite == nullptr)
	{
		debug("\tcannot find build site\n");
		return;
	}
	
	// set colony demand
	
	double seaColonyDemand = bestBuildSiteScore;
	
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

	if (optimalColonyUnitId == -1)
	{
		debug("\t\tno sea colony unit found\n");
		return;
	}

	debug("\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, optimalColonyUnitId=%-25s\n", TRIAD_SEA, base->mineral_surplus, Units[optimalColonyUnitId].name);

	// set base demand

	addProductionDemand(optimalColonyUnitId, priority);

}

/*
Evaluates need for air colony.
*/
void evaluateAirColonyDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	MAP *baseTile = getBaseMapTile(baseId);

	debug("evaluateAirColonyDemand\n");

	// verify base can build a colony

	if (!canBaseProduceColony(baseId))
	{
		debug("\tbase cannot build a colony\n");
		return;
	}
	
	// no colony units
	
	if (aiData.airColonyUnitIds.size() == 0)
	{
		debug("\tno air colony units\n");
		return;
	}
	
	// populate colony unit production times
	
	std::vector<IdIntValue> colonyUnitProductionTimes;
	
	for (int unitId : aiData.airColonyUnitIds)
	{
		int productionTime = estimateBaseProductionTurnsToCompleteItem(baseId, unitId);
		colonyUnitProductionTimes.push_back({unitId, productionTime});
	}
	
	// find best build site score
	
	MAP *bestBuildSite = nullptr;
	double bestCombinedBuildSiteScore = -DBL_MAX;
	double bestBuildSiteScore = 0.0;
	int optimalColonyUnitId = -1;
	
	for (MAP *tile : buildSites)
	{
		ExpansionTileInfo *expansionTileInfo = getExpansionTileInfo(tile);
		
		// iterate over available colony units
		
		for (IdIntValue colonyUnitProductionTimeEntry : colonyUnitProductionTimes)
		{
			int unitId = colonyUnitProductionTimeEntry.id;
			int productionTime = colonyUnitProductionTimeEntry.value;
			
			// get travel time and score
			
			double travelTime = estimateTravelTime(baseTile, tile, unitId);
			double travelTimeScore = getBuildSiteTravelTimeScore(travelTime);
			double buildSiteScore = expansionTileInfo->buildScore + travelTimeScore;
			
			// get combined time and score
			
			double combinedTime = (double)productionTime + travelTime;
			double combinedTimeScore = getBuildSiteTravelTimeScore(combinedTime);
			double combinedBuildSiteScore = expansionTileInfo->buildScore + combinedTimeScore;
			
			// update best
			
			if (combinedBuildSiteScore > bestCombinedBuildSiteScore)
			{
				bestBuildSite = tile;
				bestCombinedBuildSiteScore = combinedBuildSiteScore;
				bestBuildSiteScore = buildSiteScore;
				optimalColonyUnitId = unitId;
			}
			
		}
		
	}
	
	// not found
	
	if (bestBuildSite == nullptr)
	{
		debug("\tcannot find build site\n");
		return;
	}
	
	// set colony demand
	
	double landColonyDemand = bestBuildSiteScore;
	
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

	if (optimalColonyUnitId == -1)
	{
		debug("\t\tno air colony unit found\n");
		return;
	}

	debug("\t\tfindOptimalColonyUnit: triad=%d, mineral_surplus=%d, optimalColonyUnitId=%-25s\n", TRIAD_LAND, base->mineral_surplus, Units[optimalColonyUnitId].name);

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
		
		double psiDefense = getVehiclePsiDefenseStrength(vehicleId);
		
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
		
		double psiDefense = getVehiclePsiDefenseStrength(vehicleId);
		
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
//	int baseId = productionDemand.baseId;
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
	
	if (aiData.production.seaPodPoppingDemand.count(baseOceanAssociation) == 0 || aiData.production.seaPodPoppingDemand.at(baseOceanAssociation) <= 0.0)
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
	
	int prototypeUnitId = -1;
	int prototypeUnitMineralCost = INT_MAX;
	
	for (int unitId : aiData.prototypeUnitIds)
	{
		UNIT *unit = &(Units[unitId]);
		
		// skip sea units for non-port bases
		
		if (unit->triad() == TRIAD_SEA && !isBaseCanBuildShips(baseId))
			continue;
		
		int mineralCost = getBaseMineralCost(baseId, unitId);
		
		if (mineralCost < prototypeUnitMineralCost)
		{
			prototypeUnitId = unitId;
			prototypeUnitMineralCost = mineralCost;
		}
		
	}
	
	if (prototypeUnitId == -1)
	{
		debug("\tno prototype to build\n");
		return;
	}
	
	// get unit
	
	UNIT *prototypeUnit = &(Units[prototypeUnitId]);
	
	debug("\tprototypeUnit=%s\n", prototypeUnit->name);
	
	// calculate priority
	
	double priority = conf.ai_production_prototyping_priority * ((double)base->mineral_surplus / (double)aiData.maxMineralSurplus);
	debug("\tpriority=%f, ai_production_prototyping_priority=%5.2f, mineral_surplus=%d, maxMineralSurplus=%d\n", priority, conf.ai_production_prototyping_priority, base->mineral_surplus, aiData.maxMineralSurplus);
	
	// add production demand
	
	addProductionDemand(prototypeUnitId, priority);

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
	
	// find nearest base with melee protection demand
	
	int targetBaseId = -1;
	int targetBaseRange = INT_MAX;
	
	for (int otherBaseId : aiData.baseIds)
	{
		BASE *otherBase = &(Bases[otherBaseId]);
		BaseInfo *otherBaseInfo = aiData.getBaseInfo(otherBaseId);
		
		debug("> %-25s melee.protectionDemand=%5.2f\n", otherBase->name, otherBaseInfo->unitTypeInfos[UT_MELEE].protectionDemand);
		
		// exclude those without melee protection demand
		
		if (otherBaseInfo->unitTypeInfos[UT_MELEE].protectionDemand <= 0.0)
			continue;
		
		int range = map_range(base->x, base->y, otherBase->x, otherBase->y);
		
		if (range < targetBaseRange)
		{
			targetBaseId = otherBaseId;
			targetBaseRange = range;
		}
		
	}
	
	// no target base
	
	if (targetBaseId == -1)
	{
		debug("\tno target base\n");
		return;
	}
	
	debug("\t-> %s\n", Bases[targetBaseId].name);

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
	
	double priority = globalMilitaryPriority;
	
	// priority is reduced for low production bases except selftargetted base
	
	if (targetBaseId != baseId)
	{
		priority *= ((double)base->mineral_surplus / (double)aiData.maxMineralSurplus);
	}
	
	// add demand
	
	addProductionDemand(unitId, priority);
	debug("\t%s -> %s, %s, priority=%5.2f, globalMilitaryPriority=%5.2f, mineral_surplus=%2d, maxMineralSurplus=%2d\n", base->name, Bases[targetBaseId].name, Units[unitId].name, priority, globalMilitaryPriority, base->mineral_surplus, aiData.maxMineralSurplus);
	
}

void evaluateAlienProtectorDemand()
{
	BASE *base = productionDemand.base;
	
	debug("evaluateAlienProtectorDemand - %-25s\n", base->name);

	// check mineral surplus
	
	if (base->mineral_surplus < conf.ai_production_unit_min_mineral_surplus)
	{
		debug("\tweak production\n");
		return;
	}
	
	// find unit
	
	int unitId = selectAlienProtectorUnit();
	
	// no unit found
	
	if (unitId == -1)
	{
		debug("\tno unit found\n");
		return;
	}
	
	debug("\t%s\n", Units[unitId].name);
	
	// calculate priority
	
	double priority = (double)aiData.production.alienProtectorRequestCount;
	debug("\t%s, priority=%5.2f, alienProtectorRequestCount=%d\n", Units[unitId].name, priority, aiData.production.alienProtectorRequestCount);
	
	// add demand
	
	addProductionDemand(unitId, priority);
	
}

void addProductionDemand(int item, double priority)
{
	int baseId = productionDemand.baseId;
	
	if (item >= 0)
	{
		// update unit priority based on base support allowance
		
		priority *= getUnitPriorityMultiplier(baseId, item);
		
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

	for (int unitId : getFactionUnitIds(factionId, false))
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

	for (int unitId : getFactionUnitIds(factionId, false))
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

	for (int unitId : getFactionUnitIds(factionId, false))
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

	for (int unitId : getFactionUnitIds(factionId, false))
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

	for (int unitId : getFactionUnitIds(factionId, false))
	{
		UNIT *unit = &(Units[unitId]);
		CChassis *chassis = &(Chassis[unit->chassis_type]);

		// given triad only or air with unlimited range

		if (!(triad == TRIAD_AIR && chassis->range == 0 || unit_triad(unitId) == triad))
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
	
	// size 2+ - always allowed
	
	if (base->pop_size >= 2)
		return true;
	
	// mineral surplus not positive - allowed
	
	if (base->mineral_surplus <= 0)
		return true;
	
	// get colony minimal mineral cost
	
	int colonyMineralCost = getBaseMineralCost(baseId, BSC_COLONY_POD);
	
	if (getBaseOceanAssociation(baseId) != -1)
	{
		colonyMineralCost = std::min(colonyMineralCost, getBaseMineralCost(baseId, BSC_SEA_ESCAPE_POD));
	}

	// get colony production time
	
	int colonyProductionTime = ((colonyMineralCost - base->minerals_accumulated) + (base->mineral_surplus - 1)) / base->mineral_surplus;
	
	// get projected base population by the time colony is produced
	
	int projectedBasePopulation = getProjectedBasePopulation(baseId, colonyProductionTime);
	
	// return true if projected base population is >= 2
	
	return projectedBasePopulation >= 2;

}

int findFormerUnit(int factionId, int triad)
{
	int bestUnitId = -1;
	double bestUnitValue = -1;

	for (int unitId : getFactionUnitIds(factionId, false))
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
	int projectedPopulationIncrease = getProjectedBasePopulation(baseId, conf.ai_production_population_projection_turns) - base->pop_size;

	return (doctors > 0 || (base->drone_total > 0 && base->drone_total + projectedPopulationIncrease > base->talent_total));

}

int findPoliceUnit(int factionId)
{
	int policeUnitId = -1;
	double policeUnitEffectiveness = 0.0;

	for (int unitId : getFactionUnitIds(factionId, false))
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

int findAttackerUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	int baseOceanAssociation = getBaseOceanAssociation(baseId);
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseOceanAssociation = getBaseOceanAssociation(targetBaseId);
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();
	
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();

        // exclude land artillery
        
        if (triad == TRIAD_LAND && unit_has_ability(unitId, ABL_ARTILLERY))
			continue;
		
		// skip unmatching triad
		
		switch (triad)
		{
		case TRIAD_LAND:
			{
				// no land attacker for ocean base
				
				if (targetBaseOcean)
					continue;
				
			}
			break;
			
		case TRIAD_SEA:
			{
				// no sea attacker for land base
				
				if (!targetBaseOcean)
					continue;
				
				// no sea attacker if in different associations
				
				if (baseOceanAssociation == -1 || targetBaseOceanAssociation == -1 || targetBaseOceanAssociation != baseOceanAssociation)
					continue;
				
			}
			
		}
		
		// skip units unable to attack each other
		
		if
		(
			foeUnitTriad == TRIAD_LAND && triad == TRIAD_SEA
			||
			foeUnitTriad == TRIAD_SEA && triad == TRIAD_LAND
			||
			foeUnit->chassis_type == CHS_NEEDLEJET && !unit_has_ability(unitId, ABL_AIR_SUPERIORITY)
		)
		{
			continue;
		}
		
		// relative strength

		double relativeStrength;
		
		if (unit->weapon_type == WPN_PSI_ATTACK || foeUnit->armor_type == ARM_PSI_DEFENSE)
		{
			relativeStrength =
				(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
				/ foeUnitStrength->psiDefense
			;
			
		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_type) * getNewUnitConventionalOffenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalDefense
				* getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;
			
		}
		
		// chassis speed factor
		
		int chassisSpeed = unit->speed() * (triad == TRIAD_LAND ? 3 : 1);
		double speedFactor = (double)(chassisSpeed + 3) / (double)(Chassis[CHS_INFANTRY].speed + 3);
		
		// calculate cost counting maintenance
		
		int cost = getUnitMaintenanceCost(unitId);
		
		// calculate effectiveness
		
		double effectiveness = relativeStrength * speedFactor / (double)cost;
		
		// update best effectiveness
		
		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}
		
	}

    return bestUnitId;

}

int findDefenderUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	int baseOceanAssociation = getBaseOceanAssociation(baseId);
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseOceanAssociation = getBaseOceanAssociation(targetBaseId);
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();
	
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();

        // exclude land artillery
        
        if (triad == TRIAD_LAND && unit_has_ability(unitId, ABL_ARTILLERY))
			continue;
		
		// exclude air
		
		if (triad == TRIAD_AIR)
			continue;
		
		// skip unmatching triad
		
		switch (triad)
		{
		case TRIAD_LAND:
			{
				// land defender is allowed for ocean base
				
			}
			break;
			
		case TRIAD_SEA:
			{
				// no sea defender for land base
				
				if (!targetBaseOcean)
					continue;
				
				// no sea defender if in different associations
				
				if (baseOceanAssociation == -1 || targetBaseOceanAssociation == -1 || targetBaseOceanAssociation != baseOceanAssociation)
					continue;
				
			}
			
		}
		
		// skip units unable to attack each other
		
		if
		(
			foeUnitTriad == TRIAD_LAND && triad == TRIAD_SEA
			||
			foeUnitTriad == TRIAD_SEA && triad == TRIAD_LAND
		)
		{
			continue;
		}
		
		// relative strength

		double relativeStrength;
		
		if (unit->armor_type == ARM_PSI_DEFENSE || foeUnit->weapon_type == WPN_PSI_ATTACK)
		{
			relativeStrength =
				(10.0 * getNewUnitPsiDefenseStrength(unitId, baseId))
				/ foeUnitStrength->psiOffense
			;
			
			
		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_type) * getNewUnitConventionalDefenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalOffense
				/ getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;
			
		}
		
		// chassis speed factor
		// not important for base defender
		
		double speedFactor = 1.0;
		
		// calculate cost counting maintenance
		
		int cost = getUnitMaintenanceCost(unitId);
		
		// calculate effectiveness
		
		double effectiveness = relativeStrength * speedFactor / (double)cost;
		
		// update best effectiveness
		
		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}
		
	}

    return bestUnitId;

}

int findMixedUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	int baseOceanAssociation = getBaseOceanAssociation(baseId);
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseOceanAssociation = getBaseOceanAssociation(targetBaseId);
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();
	
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();
        
        // exclude land artillery
        
        if (triad == TRIAD_LAND && unit_has_ability(unitId, ABL_ARTILLERY))
			continue;
		
		// exclude air
		
		if (triad == TRIAD_AIR)
			continue;
		
		// skip unmatching triad
		
		switch (triad)
		{
		case TRIAD_LAND:
			{
				// no land attacker/mixed for ocean base
				
				if (targetBaseOcean)
					continue;
				
			}
			break;
			
		case TRIAD_SEA:
			{
				// no sea attacker/mixed for land base
				
				if (!targetBaseOcean)
					continue;
				
				// no sea attacker/mixed if in different associations
				
				if (baseOceanAssociation == -1 || targetBaseOceanAssociation == -1 || targetBaseOceanAssociation != baseOceanAssociation)
					continue;
				
			}
			
		}
		
		// skip units unable to attack each other
		
		if
		(
			foeUnitTriad == TRIAD_LAND && triad == TRIAD_SEA
			||
			foeUnitTriad == TRIAD_SEA && triad == TRIAD_LAND
		)
		{
			continue;
		}
		
		// relative strength

		double relativeStrength = 1.0;
		
		if (unit->weapon_type == WPN_PSI_ATTACK || foeUnit->armor_type == ARM_PSI_DEFENSE)
		{
			relativeStrength *=
				(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
				/ foeUnitStrength->psiDefense
			;
			
		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_type) * getNewUnitConventionalOffenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalDefense
				* getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;
			
		}
		
		if (unit->armor_type == ARM_PSI_DEFENSE || foeUnit->weapon_type == WPN_PSI_ATTACK)
		{
			relativeStrength *=
				(10.0 * getNewUnitPsiDefenseStrength(unitId, baseId))
				/ foeUnitStrength->psiOffense
			;
			
		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_type) * getNewUnitConventionalDefenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalOffense
				/ getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;
			
		}
		
		// chassis speed factor
		// average between attacker and defender
		
		int chassisSpeed = unit->speed() * (triad == TRIAD_LAND ? 3 : 1);
		double speedFactor = (double)(chassisSpeed + 3) / (double)(Chassis[CHS_INFANTRY].speed + 3);
		speedFactor = sqrt(speedFactor);
		
		// calculate cost counting maintenance
		
		int cost = getUnitMaintenanceCost(unitId);
		
		// calculate effectiveness
		
		double effectiveness = relativeStrength * speedFactor / (double)cost;
		
		// update best effectiveness
		
		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}
		
	}

    return bestUnitId;

}

int findArtilleryUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();
	
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();

        // exclude everything but land artillery
        
        if (!(triad == TRIAD_LAND && unit_has_ability(unitId, ABL_ARTILLERY)))
			continue;
		
		// skip unmatching triad
		
		switch (triad)
		{
		case TRIAD_LAND:
			{
				// no land artillery for ocean base
				
				if (targetBaseOcean)
					continue;
				
			}
			break;
			
		}
		
		// skip units unable to attack each other
		// artillery cannot attack air
		
		if (foeUnitTriad == TRIAD_AIR)
		{
			continue;
		}
		
		// relative strength

		double relativeStrength;
		
		if (unit_has_ability(foeUnitId, ABL_ARTILLERY))
		{
			if (unit->weapon_type == WPN_PSI_ATTACK || foeUnit->weapon_type == WPN_PSI_ATTACK)
			{
				relativeStrength =
					(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
					/ foeUnitStrength->psiOffense
				;
				
			}
			else
			{
				if (conf.conventional_artillery_duel_uses_weapon_and_armor)
				{
					relativeStrength =
						((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_type) * (getNewUnitConventionalOffenseStrength(unitId, baseId) + getNewUnitConventionalDefenseStrength(unitId, baseId)))
						/ (foeUnitStrength->conventionalOffense + foeUnitStrength->conventionalDefense)
					;
					
				}
				else
				{
					relativeStrength =
						((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_type) * getNewUnitConventionalOffenseStrength(unitId, baseId))
						/ foeUnitStrength->conventionalOffense
					;
					
				}
				
			}
			
		}
		else
		{
			if (unit->weapon_type == WPN_PSI_ATTACK || foeUnit->armor_type == ARM_PSI_DEFENSE)
			{
				relativeStrength =
					(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
					/ foeUnitStrength->psiDefense
				;
				
			}
			else
			{
				relativeStrength =
					((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_type) * getNewUnitConventionalOffenseStrength(unitId, baseId))
					/ foeUnitStrength->conventionalDefense
				;
				
			}
			
		}
		
		// chassis speed factor
		// average between attacker and defender
		
		int chassisSpeed = unit->speed() * (triad == TRIAD_LAND ? 3 : 1);
		double speedFactor = (double)(chassisSpeed + 3) / (double)(Chassis[CHS_INFANTRY].speed + 3);
		speedFactor = sqrt(speedFactor);
		
		// calculate cost counting maintenance
		
		int cost = getUnitMaintenanceCost(unitId);
		
		// calculate effectiveness
		
		double effectiveness = relativeStrength * speedFactor / (double)cost;
		
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
	int baseOceanAssociation = getBaseOceanAssociation(baseId);
	
	debug("selectCombatUnit\n");
	
	// no target base
	
	if (targetBaseId == -1)
	{
		return -1;
	}
	
	// get target base strategy
	
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseOceanAssociation = getBaseOceanAssociation(targetBaseId);
	BaseInfo *targetBaseInfo = aiData.getBaseInfo(targetBaseId);
	
	// no protection demand
	
	if (targetBaseInfo->getTotalProtectionDemand() <= 0.0)
	{
		debug("\tno protection demand\n");
		return -1;
	}
	
	// iterate best protectors at target base
	
	int bestUnitId = -1;
	
	for (UnitTypeInfo &unitTypeInfo : targetBaseInfo->unitTypeInfos)
	{
		for (IdDoubleValue protectorEntry : unitTypeInfo.protectors)
		{
			int unitId = protectorEntry.id;
			int triad = unit_triad(unitId);
			int offenseValue = getUnitOffenseValue(unitId);
			
			// skip those unfit for target base or cannot travel there
			
			if (!targetBaseOcean)
			{
				// don't send sea units to land base
				
				if (triad == TRIAD_SEA)
					continue;
			}
			else
			{
				// don't send sea units to ocean base in different association
				
				if (triad == TRIAD_SEA && (baseOceanAssociation == -1 || targetBaseOceanAssociation == -1 || targetBaseOceanAssociation != baseOceanAssociation))
					continue;
				
				// don't send land offensive units to ocean base unless amphibious
				
				if (triad == TRIAD_LAND && (offenseValue < 0 || offenseValue > 1) && !unit_has_ability(unitId, ABL_AMPHIBIOUS))
					continue;
				
			}
			
			// select first best unit
			
			bestUnitId = unitId;
			break;
			
		}
		
	}
	// return
	
	return bestUnitId;
	
}

int selectAlienProtectorUnit()
{
	int bestUnitId = -1;
	int bestUnitTrance = 0;
	int bestUnitCost = INT_MAX;
	
	for (int unitId : aiData.unitIds)
	{
		UNIT *unit = &(Units[unitId]);
		
		// exclude not infantry
		
		if (unit->chassis_type != CHS_INFANTRY)
			continue;
		
		// get trance
		
		int trance = (isUnitHasAbility(unitId, ABL_TRANCE) ? 1 : 0);
		
		// get cost
		
		int cost = unit->cost;
		
		// update best
		
		if
		(
			trance > bestUnitTrance
			||
			trance == bestUnitTrance && cost < bestUnitCost
		)
		{
			bestUnitId = unitId;
			bestUnitTrance = trance;
			bestUnitCost = cost;
		}
		
	}
	
	return bestUnitId;
	
}

void evaluateTransportDemand()
{
	int baseId = productionDemand.baseId;
	
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
					0.5
					*
					(double)tx_veh_cargo(vehicleId)
					*
					((double)Units[Vehicles[vehicleId].unit_id].speed() / (double)Units[BSC_TRANSPORT_FOIL].speed())
				;
				
			}
			
		}
		
		// demand is satisfied
		
		if (transportCount >= (double)seaTransportDemand)
		{
			debug("\ttransportCount=%5.2f >= seaTransportDemand=%d\n", transportCount, seaTransportDemand)
			continue;
		}
		
		// calculate demand
		
		double transportDemand = 1.0 - transportCount / (double)seaTransportDemand;
		
		debug("\tseaTransportDemand=%d, transportCount=%f, transportDemand=%f\n", seaTransportDemand, transportCount, transportDemand);
		
		// set priority
		
		double priority = conf.ai_production_transport_priority * transportDemand;
		
		debug("\tpriority=%f, conf.ai_production_transport_priority=%f, transportDemand=%f\n", priority, conf.ai_production_transport_priority, transportDemand);
		
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

/*
Returns first available but unbuilt facility from list.
*/
int getFirstUnbuiltAvailableFacilityFromList(int baseId, std::vector<int> facilityIds)
{
	BASE *base = &(Bases[baseId]);
	
	for (int facilityId : facilityIds)
	{
		if (has_facility_tech(base->faction_id, facilityId) && !has_facility(baseId, facilityId))
		{
			return facilityId;
		}
		
	}
	
	// everything available is built
	
	return -1;
	
}

/*
Calculates aggreated minerals + energy resource score.
*/
double getResourceScore(double minerals, double energy)
{
	return minerals + energy / hurryCost;
}

/*
Calculates gain/loss from one time resource intake.
score is aggregated intake equivalent
delay is time shift in turns from now when this event takes place
*/
double getIntakeGain(double score, double delay)
{
	return score * exp(- conf.ai_production_faction_development_rate * delay);
}

/*
Calculates gain/loss from constant income.
score is aggregated income equivalent
delay is time shift in turns from now when this event takes place
*/
double getIncomeGain(double score, double delay)
{
	return score / (conf.ai_production_faction_development_rate) * exp(- conf.ai_production_faction_development_rate * delay);
}

/*
Calculates gain/loss from income growth (separately from income itself).
score is aggregated growth equivalent
delay is time shift in turns from now when this event takes place
*/
double getGrowthGain(double score, double delay)
{
	return score / (conf.ai_production_faction_development_rate * conf.ai_production_faction_development_rate) * exp(- conf.ai_production_faction_development_rate * delay);
}

/*
Calculates gain normalized by colony gain.
*/
double getNormalizedGain(double gain)
{
	return gain / normalGain;
}

double getBaseIncome(int baseId)
{
	return getResourceScore(getBaseTotalMineral(baseId), getBaseTotalEnergy(baseId));
}

/*
Calculates how much population growth base receives in one turn.
*/
double getBasePopulationGrowth(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	return (double)base->nutrient_surplus / (double)((base->pop_size + 1) * cost_factor(base->faction_id, 0, baseId));
	
}

int getBaseEconomyIntake(int baseId)
{
	BASE *base = &(Bases[baseId]);
	Faction *faction = &(Factions[base->faction_id]);
	
	return (base->energy_surplus * (10 - faction->SE_alloc_psych - faction->SE_alloc_labs) + 4) / 10;
	
}

int getBasePsychIntake(int baseId)
{
	BASE *base = &(Bases[baseId]);
	Faction *faction = &(Factions[base->faction_id]);
	
	return (base->energy_surplus * (faction->SE_alloc_psych) + 4) / 10;
	
}

int getBaseLabsIntake(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	return base->energy_surplus - getBasePsychIntake(baseId) - getBaseEconomyIntake(baseId);
	
}

int getBaseTotalMineral(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	return base->mineral_intake_2;
	
}

int getBaseTotalEnergy(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	return base->economy_total + base->psych_total + base->labs_total;
	
}

std::vector<int> getProtectionCounterUnits(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	debug("getProtectionCounterUnits\n");
	
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();
	
	std::vector<int> counterUnitIds;
	
	switch (foeUnitTriad)
	{
	case TRIAD_LAND:
		{
			if (unit_has_ability(foeUnitId, ABL_ARTILLERY))
			{
				int attackerUnit = findAttackerUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (attackerUnit != -1)
				{
					counterUnitIds.push_back(attackerUnit);
				}
				
				int artilleryUnit = findArtilleryUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (artilleryUnit != -1)
				{
					counterUnitIds.push_back(artilleryUnit);
				}
				
			}
			else
			{
				int defenderUnit = findDefenderUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (defenderUnit != -1)
				{
					counterUnitIds.push_back(defenderUnit);
				}
				
				int mixedUnit = findMixedUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (mixedUnit != -1)
				{
					counterUnitIds.push_back(mixedUnit);
				}
				
				int attackerUnit = findAttackerUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (attackerUnit != -1)
				{
					counterUnitIds.push_back(attackerUnit);
				}
				
				int artilleryUnit = findArtilleryUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (artilleryUnit != -1)
				{
					counterUnitIds.push_back(artilleryUnit);
				}
				
			}
			
		}
		break;
		
	case TRIAD_SEA:
		{
			int defenderUnit = findDefenderUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (defenderUnit != -1)
			{
				counterUnitIds.push_back(defenderUnit);
			}
			
			int mixedUnit = findMixedUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (mixedUnit != -1)
			{
				counterUnitIds.push_back(mixedUnit);
			}
			
		}
		break;
		
	case TRIAD_AIR:
		{
			int defenderUnit = findDefenderUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (defenderUnit != -1)
			{
				counterUnitIds.push_back(defenderUnit);
			}
			
			int mixedUnit = findMixedUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (mixedUnit != -1)
			{
				counterUnitIds.push_back(mixedUnit);
			}
			
			int attackerUnit = findAttackerUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (attackerUnit != -1)
			{
				counterUnitIds.push_back(attackerUnit);
			}
			
		}
		break;
		
	}
	
	return counterUnitIds;
	
}

/*
Calculates unit priority reduction based on existing support.
*/
double getUnitPriorityMultiplier(int baseId, int unitId)
{
	double unitPriorityMultiplier = 1.0;
	
	// exclude colony
	
	if (isColonyUnit(unitId))
		return unitPriorityMultiplier;
	
	// get base support allowance
	
	int baseSupportAllowance = getBaseSupportAllowance(baseId);
	
	// disable priority if not enough allowance
	
	if (baseSupportAllowance < 1)
	{
		unitPriorityMultiplier = 0.0;
	}
	
	debug("> unitPriorityMultiplier=%5.2f, baseSupportAllowance=%2d\n", unitPriorityMultiplier, baseSupportAllowance);
	return unitPriorityMultiplier;
	
}

