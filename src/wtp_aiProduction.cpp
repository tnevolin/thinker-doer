#pragma GCC diagnostic ignored "-Wshadow"

#include <math.h>
#include <set>
#include <map>
#include <numeric>
#include <deque>
#include "wtp_aiProduction.h"
#include "engine.h"
#include "game.h"
#include "wtp_game.h"
#include "wtp_terranx.h"
#include "wtp_ai.h"
#include "wtp_aiData.h"
#include "wtp_aiMoveColony.h"
#include "wtp_aiRoute.h"
#include "wtp_aiHurry.h"

// global statistics

double meanNewBaseGain;
double landColonyGain;

// global parameters

double globalColonyDemand;
double baseColonyDemandMultiplier;
robin_hood::unordered_flat_map<int, BaseProductionInfo> baseProductionInfos;
robin_hood::unordered_flat_map<int, double> weakestEnemyBaseProtection;

// sea transport parameters by sea cluster

robin_hood::unordered_flat_map<int, double> seaTransportDemands;
robin_hood::unordered_flat_map<int, int> bestSeaTransportProductionBaseId;

// military power in count of existing and potential military units cost
robin_hood::unordered_flat_map<int, double> militaryPowers;

int selectedProject = -1;

// currently processing base production demand

ProductionDemand productionDemand;

// ProductionDemand

void ProductionDemand::initialize(int _baseId)
{
	baseId = _baseId;
	base = getBase(_baseId);
	
	baseSeaCluster = getSeaCluster(getBaseMapTile(baseId));
	
	item = -FAC_STOCKPILE_ENERGY;
	priority = 0.0;
	
	baseGain = getBaseGain(baseId);
	baseCitizenGain = baseGain / (double)base->pop_size;
	baseWorkerGain = baseGain / (double)getBaseWorkerCount(baseId);
	
}

void ProductionDemand::addItemPriority(int _item, double _priority)
{
	if (_priority > this->priority)
	{
		this->item = _item;
		this->priority = _priority;
	}
	
}

/**
Prepares production choices.
*/
void productionStrategy()
{
	executionProfiles["1.6. productionStrategy"].start();
	
	// set global statistics
	
	meanNewBaseGain = getNewBaseGain();
	landColonyGain = getLandColonyGain();
	
	// base production infos
	
	for (int baseId : aiData.baseIds)
	{
		baseProductionInfos.emplace(baseId, BaseProductionInfo());
		baseProductionInfos.at(baseId).baseGain = getBaseGain(baseId);
		baseProductionInfos.at(baseId).extraPoliceGains.at(0) = getBasePoliceGain(baseId, false);
		baseProductionInfos.at(baseId).extraPoliceGains.at(1) = getBasePoliceGain(baseId, true);
	}
	
	// evaluate global demands
	// populate global variables
	
	populateFactionProductionData();
	evaluateGlobalColonyDemand();
	evaluateGlobalSeaTransportDemand();
	
	// set production
	
	setProduction();
	
	// build project
	
	selectProject();
	assignProject();
	
	// hurry protective unit production
	
	hurryProtectiveUnit();
	
	executionProfiles["1.6. productionStrategy"].stop();
	
}

/**
Compares terraforming requests by improvementIncome then by fitnessScore.
1. compare by yield: superior yield goes first.
2. compare by gain.
*/
bool compareFormerRequests(FormerRequest const &formerRequest1, FormerRequest const &formerRequest2)
{
	return formerRequest1.income > formerRequest2.income;
}

void populateFactionProductionData()
{
	const int AVAILABLE_FORMER_COUNT_DIVISOR = 3;
	
	// available former counts
	
	int availableAirFormerCount = aiFactionInfo->airFormerCount / AVAILABLE_FORMER_COUNT_DIVISOR;
	
	robin_hood::unordered_flat_map<int, int> availableSeaFormerCounts;
	
	for (robin_hood::pair<int, int> seaClusterFormerCountEntry : aiFactionInfo->seaClusterFormerCounts)
	{
		int cluster = seaClusterFormerCountEntry.first;
		int count = seaClusterFormerCountEntry.second;
		availableSeaFormerCounts.emplace(cluster, count / AVAILABLE_FORMER_COUNT_DIVISOR);
	}
	
	robin_hood::unordered_flat_map<int, int> availableLandFormerCounts;
	
	for (robin_hood::pair<int, int> landTransportedClusterFormerCountEntry : aiFactionInfo->landTransportedClusterFormerCounts)
	{
		int cluster = landTransportedClusterFormerCountEntry.first;
		int count = landTransportedClusterFormerCountEntry.second;
		availableLandFormerCounts.emplace(cluster, count / AVAILABLE_FORMER_COUNT_DIVISOR);
	}
	
	// sort former requests
	
	std::sort(aiData.production.formerRequests.begin(), aiData.production.formerRequests.end(), compareFormerRequests);
	
	// remove potentially claimed requests
	
	std::vector<FormerRequest>::iterator formerRequestIterator;
	for (formerRequestIterator = aiData.production.formerRequests.begin(); formerRequestIterator != aiData.production.formerRequests.end(); )
	{
		FormerRequest &formerRequest = *formerRequestIterator;
		
		bool removed = false;
		
		if (is_ocean(formerRequest.tile))
		{
			int seaCluster = getSeaCluster(formerRequest.tile);
			
			if (seaCluster != -1 && availableSeaFormerCounts.find(seaCluster) != availableSeaFormerCounts.end() && availableSeaFormerCounts.at(seaCluster) >= 1)
			{
				availableSeaFormerCounts.at(seaCluster)--;
				removed = true;
			}
			else if (availableAirFormerCount >= 1)
			{
				availableAirFormerCount--;
				removed = true;
			}
			
		}
		else
		{
			int landTransportedCluster = getLandTransportedCluster(formerRequest.tile);
			
			if (landTransportedCluster != -1 && availableLandFormerCounts.find(landTransportedCluster) != availableLandFormerCounts.end() && availableLandFormerCounts.at(landTransportedCluster) >= 1)
			{
				availableLandFormerCounts.at(landTransportedCluster)--;
				removed = true;
			}
			else if (availableAirFormerCount >= 1)
			{
				availableAirFormerCount--;
				removed = true;
			}
			
		}
		
		if (removed)
		{
			formerRequestIterator = aiData.production.formerRequests.erase(formerRequestIterator);
		}
		else
		{
			formerRequestIterator++;
		}
		
	}
	
	if (DEBUG)
	{
		debug("formerRequests - %s\n", aiMFaction->noun_faction);
		
		for (FormerRequest &formerRequest : aiData.production.formerRequests)
		{
			debug
			(
				"\t%5.2f %s %s"
				"\n"
				, formerRequest.income
				, getLocationString(formerRequest.tile).c_str()
				, formerRequest.option->name
			);
			
		}
		
	}
	
}

/**
Evaluates need for new colonies based on current inefficiency.
Every new colony creates more drones and, thus, is less effective in larger empire.
*/
void evaluateGlobalColonyDemand()
{
	debug("evaluateGlobalColonyDemand - %s\n", MFactions[aiFactionId].noun_faction);
	
	Faction *faction = getFaction(aiFactionId);
	
	// set default global colony demand multiplier
	
	globalColonyDemand = 1.0;
	
	// do not compute if there are no bases to produce anything
	
	if (aiData.baseIds.size() == 0)
		return;
	
	// --------------------------------------------------
	// inefficiency
	// --------------------------------------------------
	
	// how many b-drones new base generates
	
	int contentPop, baseLimit;
	mod_psych_check(aiFactionId, &contentPop, &baseLimit);
	int newBaseBDrones = faction->base_count / baseLimit;
	
	// averages
	
	int sumPopulation = 0.0;
	double sumBaseIncome = 0.0;
	double sumPsychMultiplier = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = getBase(baseId);
		
		sumPopulation += base->pop_size;
		sumBaseIncome += getBaseIncome(baseId);
		sumPsychMultiplier += getBasePsychMultiplier(baseId);
		
	}
	
	double averagePsychMultiplier = sumPsychMultiplier / (double)aiData.baseIds.size();
	double averageBaseSize = (double)sumPopulation / (double)aiData.baseIds.size();
	
	// average efficiency loss
	
	double doctorsPerDrone = 1.0 / averagePsychMultiplier;
	double doctorsPerNewBase = doctorsPerDrone * (double)newBaseBDrones;
	globalColonyDemand = 1.0 - doctorsPerNewBase / averageBaseSize;
	
	// reduce colony demand proportionally
	
	
	debug
	(
		"\tglobalColonyDemand=%5.2f"
		" baseLimit=%4d"
		" newBaseBDrones=%2d"
		" averagePsychMultiplier=%5.2f"
		" doctorsPerDrone=%5.2f"
		" doctorsPerNewBase=%5.2f"
		"\n"
		, globalColonyDemand
		, baseLimit
		, newBaseBDrones
		, averagePsychMultiplier
		, doctorsPerDrone
		, doctorsPerNewBase
	);
	
}

void evaluateGlobalSeaTransportDemand()
{
	debug("evaluateGlobalSeaTransportDemand - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// reset demands
	
	seaTransportDemands.clear();
	
	// demand and best base by cluster
	
	const int transportFoilSpeed = getUnitSpeed(aiFactionId, BSC_TRANSPORT_FOIL);
	
	for (robin_hood::pair<int, int> seaTransportRequestCountEntry : aiData.seaTransportRequestCounts)
	{
		int seaCluster = seaTransportRequestCountEntry.first;
		int seaTransportRequestCount = seaTransportRequestCountEntry.second;
		
		// demand
		
		double transportationCoverage = 0.0;
		
		for (int vehicleId : aiData.seaTransportVehicleIds[seaCluster])
		{
			int vehicleSpeed = getVehicleSpeed(vehicleId);
			int remainingCapacity = getTransportRemainingCapacity(vehicleId);
			
			// adjust value for capacity and speed
			
			transportationCoverage += 0.5 * remainingCapacity * ((double)vehicleSpeed / (double)transportFoilSpeed);
			
		}
		
		seaTransportDemands[seaCluster] = std::max(0.0, (double)seaTransportRequestCount - transportationCoverage);
		
		// best base
		
		int bestBaseId = -1;
		int bestBaseMineralSurplus = 0;
		
		for (int baseId : aiData.baseIds)
		{
			int baseSeaCluster = getBaseSeaCluster(baseId);
			
			// same seaCluster
			
			if (baseSeaCluster != seaCluster)
				continue;
			
			// compute coefficient
			
			int mineralSurplus = Bases[baseId].mineral_surplus;
			
			// update the best
			
			if (mineralSurplus > bestBaseMineralSurplus)
			{
				bestBaseId = baseId;
				bestBaseMineralSurplus = mineralSurplus;
			}
			
		}
		
		bestSeaTransportProductionBaseId[seaCluster] = bestBaseId;
		
	}
	
	if (DEBUG)
	{
		for (robin_hood::pair<int, double> seaTransportDemandEntry : seaTransportDemands)
		{
			int cluster = seaTransportDemandEntry.first;
			double demand = seaTransportDemandEntry.second;
			
			debug("%2d %5.2f\n", cluster, demand);
			
		}
		
	}
	
}

/**
Sets faction bases production.
*/
void setProduction()
{
    debug("\nsetProduction - %s\n\n", MFactions[*CurrentFaction].noun_faction);
	
    for (int baseId : aiData.baseIds)
	{
		BASE* base = &(Bases[baseId]);
		
		debug("setProduction - %s\n", base->name);
		
		// current production choice
		
		int choice = base->queue_items[0];
		
		debug("(%s)\n", prod_name(choice));
		
		// calculate vanilla priority
		
		double vanillaPriority = 0.0;
		
		// unit
		if (choice >= 0)
		{
			// only not managed unit types
			
			if (!isCombatUnit(choice) && MANAGED_UNIT_TYPES.count(Units[choice].weapon_id) == 0)
			{
				vanillaPriority = conf.ai_production_vanilla_priority_unit * getUnitPriorityCoefficient(baseId, choice);
			}
			
		}
		// project
		else if (choice <= -SP_ID_First)
		{
			// project priority is handled by WTP
			vanillaPriority = 0.0;
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
		
		// drop priority for impossible colony
		
		if (choice >= 0 && isColonyUnit(choice) && !canBaseProduceColony(baseId))
		{
			vanillaPriority = 0.0;
		}
		
		// WTP
		
		suggestBaseProduction(baseId);
		
		int wtpChoice = productionDemand.item;
		double wtpPriority = productionDemand.priority;
		
		debug
		(
			"production selection\n"
			"\t%-10s(%5.2f) %s\n"
			"\t%-10s(%5.2f) %s\n"
			, "vanilla" , vanillaPriority, prod_name(choice)
			, "wtp", wtpPriority, prod_name(wtpChoice)
		);
		
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
	
    debug("\nsetProduction - %s [end]\n\n", MFactions[*CurrentFaction].noun_faction);
	
}

void suggestBaseProduction(int baseId)
{
    // initialize productionDemand
	
	productionDemand.initialize(baseId);
	
	// evaluate economical items
	
	evaluateFacilities();
	evaluateExpansionUnits();
	evaluateTerraformingUnits();
	evaluateCrawlingUnits();
	
	// evaluate combat units
	
	evaluatePodPoppingUnits();
	evaluateBaseDefenseUnits();
	evaluateTerritoryProtectionUnits();
	evaluateEnemyBaseAssaultUnits();
	
	// evaluate transport
	// should be after unit evaluations to substitute transport as needed
	
	evaluateSeaTransport();
	
	// evaluate projects
	
	evaluateProject();
	
}

void evaluateFacilities()
{
	debug("evaluateFacilities\n");
	
	evaluateStockpileEnergy();
	evaluateHeadquarters();
	evaluatePsychFacilitiesRemoval();
	evaluatePsychFacilities();
	evaluateIncomeFacilities();
	evaluateMineralMultiplyingFacilities();
	evaluatePopulationLimitFacilities();
	evaluateMilitaryFacilities();
	evaluatePrototypingFacilities();

}

/*
Evaluates base stockpile energy.
*/
void evaluateStockpileEnergy()
{
	debug("- evaluateStockpileEnergy\n");
	
	BASE *base = productionDemand.base;
	
	int facilityId = FAC_STOCKPILE_ENERGY;
	
	double extraEnergy = 0.5 * (isFactionHasProject(aiFactionId, FAC_PLANETARY_ENERGY_GRID) ? 1.25 : 1.00) * (double) base->mineral_surplus;
	double income = getResourceScore(0.0, extraEnergy);
	double gain = getGainBonus(income);
	double cost = (double)base->mineral_surplus / (double)mod_cost_factor(base->faction_id, RSC_MINERAL, -1);
	double priority = gain / cost;
	
	productionDemand.addItemPriority(-facilityId, priority);
	
	debug
	(
		"\t%-32s"
		" priority=%5.2f   |"
		" extraEnergy=%5.2f"
		" income=%5.2f"
		" gain=%5.2f"
		" cost=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, extraEnergy
		, income
		, gain
		, cost
	)
	;
	
}

void evaluateHeadquarters()
{
	debug("- evaluateHeadquarters\n");
	
	int baseId = productionDemand.baseId;
	int facilityId = getFirstAvailableFacility(baseId, {FAC_HEADQUARTERS});
	
	// cannot build
	
	if (facilityId == -1)
		return;
	
	// find HQ base
	// compute old total energy surplus
	
	int hqBaseId = -1;
	int oldTotalBudget = 0;
	
	for (int otherBaseId : aiData.baseIds)
	{
		BASE *otherBase = getBase(otherBaseId);
		
		if (isBaseHasFacility(otherBaseId, facilityId))
		{
			hqBaseId = otherBaseId;
		}
		
		computeBase(otherBaseId, false);
		
		int oldBudget = otherBase->economy_total + otherBase->psych_total + otherBase->labs_total;
		oldTotalBudget += oldBudget;
		
	}
	
	// relocate HQ
	// compute new total energy surplus
	
	if (hqBaseId != -1)
	{
		setBaseFacility(hqBaseId, facilityId, false);
	}
	setBaseFacility(baseId, facilityId, true);
	
	int newTotalBudget = 0;
	
	for (int otherBaseId : aiData.baseIds)
	{
		BASE *otherBase = getBase(otherBaseId);
		
		computeBase(otherBaseId, false);
		
		int newBudget = otherBase->economy_total + otherBase->psych_total + otherBase->labs_total;
		newTotalBudget += newBudget;
		
	}
	
	// restore HQ
	
	if (hqBaseId != -1)
	{
		setBaseFacility(hqBaseId, facilityId, true);
	}
	setBaseFacility(baseId, facilityId, false);
	
	for (int otherBaseId : aiData.baseIds)
	{
		computeBase(otherBaseId, false);
	}
	
	// find initial economical impact
	
	double income = getResourceScore(0, newTotalBudget - oldTotalBudget);
	double gain = getGainIncome(income);
	double priority = getItemPriority(-facilityId, gain);
	
	// add demand
	
	productionDemand.addItemPriority(-facilityId, priority);
	
	debug
	(
		"\t%-32s"
		" priority=%5.2f   |"
		" income=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, income
		, gain
	)
	;
	
}

/**
Evaluates base income facilities removal.
*/
void evaluatePsychFacilitiesRemoval()
{
	debug("- evaluatePsychFacilitiesRemoval\n");
	
	int baseId = productionDemand.baseId;
	
	// facilityIds
	
	int facilityIds[] = {FAC_PUNISHMENT_SPHERE, FAC_RECREATION_COMMONS, FAC_HOLOGRAM_THEATRE, FAC_PARADISE_GARDEN, };
	
	for (int facilityId : facilityIds)
	{
		// facility should be present
		
		if (!isBaseHasFacility(baseId, facilityId))
			continue;
		
		// gain
		
		double gain = getFacilityGain(baseId, facilityId, false, false);
		
		// remove if positive gain
		
		if (gain >= 0)
		{
			setBaseFacility(baseId, facilityId, false);
			
			debug
			(
				"\t%-32s"
				" removed"
				" gain=%5.2f"
				"\n"
				, getFacility(facilityId)->name
				, gain
			);
			
		}
		
	}
	
}

void evaluatePsychFacilities()
{
	debug("- evaluatePsychFacilities\n");
	
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	
	// facilityIds
	
	const std::vector<int> facilityIds
	{
		FAC_PUNISHMENT_SPHERE,
		FAC_RECREATION_COMMONS, FAC_HOLOGRAM_THEATRE, FAC_PARADISE_GARDEN,
	};
	
	for (int facilityId : facilityIds)
	{
		if (!isBaseCanBuildFacility(baseId, facilityId))
			continue;
		
		// old workers
		
		int oldWorkerCount = base->pop_size - base->specialist_total;
		
		// new workers
		
		setBaseFacility(baseId, facilityId, true);
//		computeBaseComplete(baseId);
		computeBase(baseId, true);
		
		int newWorkerCount = base->pop_size - base->specialist_total;
		
		// restore base
		
		setBaseFacility(baseId, facilityId, false);
//		computeBaseComplete(baseId);
		computeBase(baseId, true);
		
		// gain
		
		int workerCountIncrease = newWorkerCount - oldWorkerCount;
		double nutrientSurplusIncrease = aiData.averageWorkerNutrientIntake2 * workerCountIncrease;
		double resourceIncomeIncrease = aiData.averageWorkerResourceIncome * workerCountIncrease;
		
		double incomeIncrease = resourceIncomeIncrease;
		double incomeGain = getGainIncome(incomeIncrease);
		
		double populationGrowthIncrease = getBasePopulationGrowthIncrease(baseId, nutrientSurplusIncrease);
		double incomeGrowthIncrease = aiData.averageCitizenResourceIncome * populationGrowthIncrease;
		double incomeGrowthGain = getGainIncomeGrowth(incomeGrowthIncrease);
		
		// extra bonus for Hologram Theatre because of +50% psych
		
		if (facilityId == FAC_HOLOGRAM_THEATRE)
		{
			incomeGrowthGain *= (1.0 + PSYCH_MULTIPLIER_RESOURCE_GROWTH_INCREASE);
		}
		
		double upkeep = getResourceScore(0.0, -Facility[facilityId].maint);
		double upkeepGain = getGainIncome(upkeep);
		
		double gain = incomeGain + incomeGrowthGain + upkeepGain;
		double priority = getItemPriority(-facilityId, gain);
		
		// add demand
		
		productionDemand.addItemPriority(-facilityId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" workerCountIncrease=%d"
			" nutrientSurplusIncrease=%5.2f"
			" resourceIncomeIncrease=%5.2f"
			" incomeIncrease=%5.2f"
			" incomeGain=%5.2f"
			" populationGrowthIncrease=%5.2f"
			" incomeGrowthIncrease=%5.2f"
			" incomeGrowthGain=%5.2f"
			" upkeep=%5.2f"
			" upkeepGain=%5.2f"
			" gain=%5.2f"
			"\n"
			, getFacility(facilityId)->name
			, priority
			, workerCountIncrease
			, nutrientSurplusIncrease
			, resourceIncomeIncrease
			, incomeIncrease
			, incomeGain
			, populationGrowthIncrease
			, incomeGrowthIncrease
			, incomeGrowthGain
			, upkeep
			, upkeepGain
			, gain
		);
		
	}
	
}

/**
Evaluates base income facilities.
*/
void evaluateIncomeFacilities()
{
	debug("- evaluateIncomeFacilities\n");
	
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	
	// facilityIds
	
	const std::vector<int> facilityIds
	{
		FAC_CHILDREN_CRECHE,
		FAC_ENERGY_BANK,
		FAC_NETWORK_NODE,
		FAC_RESEARCH_HOSPITAL, FAC_NANOHOSPITAL,
		FAC_FUSION_LAB, FAC_QUANTUM_LAB,
		FAC_TREE_FARM, FAC_HYBRID_FOREST,
		FAC_AQUAFARM,
		FAC_SUBSEA_TRUNKLINE,
		FAC_THERMOCLINE_TRANSDUCER,
		FAC_BIOLOGY_LAB,
		FAC_BROOD_PIT,
		FAC_CENTAURI_PRESERVE, FAC_TEMPLE_OF_PLANET,
	};
	
	const robin_hood::unordered_flat_map<int, int> facilityLifecycles
	{
		{FAC_BIOLOGY_LAB, 1},
		{FAC_BROOD_PIT, 3}, // +1 lifecycle and -25% of the cost
		{FAC_CENTAURI_PRESERVE, 1},
		{FAC_TEMPLE_OF_PLANET, 1},
	};
	
	for (int facilityId : facilityIds)
	{
		if (!isBaseCanBuildFacility(baseId, facilityId))
			continue;
		
		// gain
		
		double facilityGain = getFacilityGain(baseId, facilityId, true, true);
		
		// moraleIncome
		
		double moraleIncome = 0.0;
		
		robin_hood::unordered_flat_map<int,int>::const_iterator facilityLifecycleIterator = facilityLifecycles.find(facilityId);
		if (facilityLifecycleIterator != facilityLifecycles.end())
		{
			int lifecycle = facilityLifecycleIterator->second;
			
			double proportionalMineralBonus = getMoraleProportionalMineralBonus({0, 0, 0, lifecycle});
			moraleIncome = getResourceScore(proportionalMineralBonus * (double)base->mineral_intake_2, 0.0);
			
		}
		
		double moraleGain = getGainIncome(moraleIncome);
		
		// combined gain
		
		double gain = facilityGain + moraleGain;
		double priority = getItemPriority(-facilityId, gain);
		
		// add demand
		
		productionDemand.addItemPriority(-facilityId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" facilityGain=%5.2f"
			" moraleIncome=%5.2f"
			" moraleGain=%5.2f"
			" gain=%5.2f"
			"\n"
			, getFacility(facilityId)->name
			, priority
			, facilityGain
			, moraleIncome
			, moraleGain
			, gain
		);
		
	}
	
}

void evaluateMineralMultiplyingFacilities()
{
	debug("- evaluateMineralMultiplyingFacilities\n");
	
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	
	// facilityIds
	
	const std::vector<int> facilityIds
	{
		FAC_RECYCLING_TANKS, FAC_GENEJACK_FACTORY, FAC_ROBOTIC_ASSEMBLY_PLANT, FAC_NANOREPLICATOR, FAC_QUANTUM_CONVERTER,
	};
	
	for (int facilityId : facilityIds)
	{
		if (!isBaseCanBuildFacility(baseId, facilityId))
			continue;
		
		// gain
		
		double extraMineralIntake = 0.5 * (double)base->mineral_intake;
		double income = getResourceScore(extraMineralIntake, 0.0);
		double incomeGain = getGainIncome(income);
		
		double populationGrowth = getBasePopulationGrowth(baseId);
		double extraMineralIntakeGrowth = 0.5 * aiData.averageCitizenMineralIntake * populationGrowth;
		double incomeGrowth = getResourceScore(extraMineralIntakeGrowth, 0.0);
		double incomeGrowthGain = getGainIncomeGrowth(incomeGrowth);
		
		// combined income
		
		double upkeep = getResourceScore(0.0, - Facility[facilityId].maint);
		double upkeepGain = getGainIncome(upkeep);
		
		double gain =
			+ incomeGain
			+ incomeGrowthGain
			+ upkeepGain
		;
		double priority = getItemPriority(-facilityId, gain);
		
		// add demand
		
		productionDemand.addItemPriority(-facilityId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" extraMineralIntake=%5.2f"
			" income=%5.2f"
			" incomeGain=%5.2f"
			" aiData.averageCitizenMineralIntake=%5.2f"
			" populationGrowth=%5.2f"
			" extraMineralIntakeGrowth=%5.2f"
			" incomeGrowth=%5.2f"
			" incomeGrowthGain=%5.2f"
			" upkeep=%5.2f"
			" upkeepGain=%5.2f"
			" gain=%5.2f"
			"\n"
			, getFacility(facilityId)->name
			, priority
			, extraMineralIntake
			, income
			, incomeGain
			, aiData.averageCitizenMineralIntake
			, populationGrowth
			, extraMineralIntakeGrowth
			, incomeGrowth
			, incomeGrowthGain
			, upkeep
			, upkeepGain
			, gain
		);
		
	}
	
}

void evaluatePopulationLimitFacilities()
{
	debug("- evaluatePopulationLimitFacilities\n");
	
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	
	// facilityIds
	
	std::vector<int> facilityIds
	{
		FAC_HAB_COMPLEX, FAC_HABITATION_DOME,
	};
	
	for (int facilityId : facilityIds)
	{
		if (!isBaseCanBuildFacility(baseId, facilityId))
			continue;
		
		// parameters
		
		int populationLimit = getFacilityPopulationLimit(base->faction_id, facilityId);
		
		if (populationLimit == -1)
		{
			debug("\tnot a population limit facility\n");
			continue;
		}
		
		double timeToPopulationLimit = getBaseTimeToPopulation(baseId, populationLimit + 1);
		
		// gain from building a facility
		
		double facilityGain = getFacilityGain(baseId, facilityId, true, true);
		
		// gain from population limit lifted
		
		double populationLimitIncome = getResourceScore(base->nutrient_surplus, 0, 0);
		double populationLimitUpkeep = getResourceScore(0.0, -Facility[facilityId].maint);
		double populationLimitGain = getGainDelay(getGainIncome(populationLimitIncome + populationLimitUpkeep), timeToPopulationLimit);
		
		// total
		
		double gain =
			+ facilityGain
			+ populationLimitGain
		;
		double priority = getItemPriority(-facilityId, gain);
		
		// add demand
		
		productionDemand.addItemPriority(-facilityId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" populationLimit=%2d"
			" timeToPopulationLimit=%5.2f"
			" facilityGain=%5.2f"
			" populationLimitIncome=%5.2f"
			" populationLimitUpkeep=%5.2f"
			" populationLimitGain=%5.2f"
			" gain=%5.2f"
			"\n"
			, getFacility(facilityId)->name
			, priority
			, populationLimit
			, timeToPopulationLimit
			, facilityGain
			, populationLimitIncome
			, populationLimitUpkeep
			, populationLimitGain
			, gain
		);
		
	}
	
}

void evaluateMilitaryFacilities()
{
	debug("- evaluateMilitaryFacilities\n");
	
	struct MilitaryFacility
	{
		int facilityId;
		bool morale;
		bool defense;
		std::array<int,4> moraleBonuses;
		std::array<int,4> defenseLevels;
	};
	
	// how much each defense structure level increases defense
	const double defenseStructureMultipliers[2] =
	{
		(1.0 + (double)conf.facility_defense_bonus[0] / 2.0) / getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def),
		(1.0 + (double)conf.facility_defense_bonus[3] / 2.0) / (1.0 + (double)conf.facility_defense_bonus[0] / 2.0),
	};
	
	// how often land/ocean base experiences each triad attack
	const double attackTriadWeights[2][4] =
	{
		{0.6, 0.0, 0.2, 0.2, },
		{0.0, 0.6, 0.2, 0.2, },
	};
	
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	bool ocean = is_ocean(getBaseMapTile(baseId));
	BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
	
	int const mineralCostFactor = mod_cost_factor(aiFactionId, RSC_MINERAL, -1);
	
	// facilities
	
	std::vector<MilitaryFacility> militaryFacilities
	{
		{FAC_COMMAND_CENTER			, true , false, {2, 0, 0, 0, }, {0, 0, 0, 0, }},
		{FAC_PERIMETER_DEFENSE		, false, true , {0, 0, 0, 0, }, {1, 0, 0, 0, }},
		{FAC_NAVAL_YARD				, true , true , {0, 2, 0, 0, }, {0, 1, 0, 0, }},
		{FAC_AEROSPACE_COMPLEX		, true , true , {0, 0, 2, 0, }, {0, 0, 1, 0, }},
		{FAC_BIOENHANCEMENT_CENTER	, true , false, {2, 2, 2, 1, }, {0, 0, 0, 0, }},
		{FAC_TACHYON_FIELD			, false, true , {0, 0, 0, 0, }, {2, 2, 2, 0, }},
	};
	
	for (MilitaryFacility &militaryFacility : militaryFacilities)
	{
		int facilityId = militaryFacility.facilityId;
		
		if (!isBaseCanBuildFacility(baseId, facilityId))
			continue;
		
		// morale income
		
		double moraleIncome = 0.0;
		
		if (militaryFacility.morale)
		{
			double proportionalMineralBonus = getMoraleProportionalMineralBonus(militaryFacility.moraleBonuses);
			double mineralBonus = proportionalMineralBonus * (double)base->mineral_intake_2;
			moraleIncome = getResourceScore(mineralBonus, 0.0);
		}
		
		// defense income
		
		double defenseBuildCostSave = 0.0;
		double defenseSupportSave = 0.0;
		
		if (militaryFacility.defense)
		{
			// count cost and support for defense vehicles in this base: old and new (with improved defense)
			
			for (int vehicleId : baseInfo.garrison)
			{
				VEH *vehicle = getVehicle(vehicleId);
				
				int offenseValue = getVehicleOffenseValue(vehicleId);
				int defenseValue = getVehicleDefenseValue(vehicleId);
				
				// conventional defense
				
				if (defenseValue <= 0)
					continue;
				
				// convert psi offense to max possible offense
				
				if (offenseValue < 0)
				{
					offenseValue = aiData.maxConOffenseValue;
				}
				
				// defense reduction
				
				double reduction = 0.0;
				
				for (int triad = 0; triad < 4; triad++)
				{
					double triadWeight = attackTriadWeights[ocean][triad];
					
					if (triadWeight == 0.0)
						continue;
					
					int defenseLevel = militaryFacility.defenseLevels[triad];
					
					if (defenseLevel == 0)
						continue;
					
					double defenseStructureMultiplier = defenseStructureMultipliers[defenseLevel - 1];
					double triadReduction = 1.0 - ((double)offenseValue + (double)defenseValue) / ((double)offenseValue + (double)defenseValue * defenseStructureMultiplier);
					reduction += triadWeight * triadReduction;
					
				}
				
				defenseBuildCostSave += mineralCostFactor * vehicle->cost() * reduction;
				defenseSupportSave += (double)getVehicleSupport(vehicleId) * reduction;
				
			}
			
		}
		
		// combined income and gain
		
		double bonus = defenseBuildCostSave;
		double income = moraleIncome + defenseSupportSave;
		double upkeep = getResourceScore(0, -Facility[facilityId].maint);
		
		double gain =
			+ getGainBonus(bonus)
			+ getGainIncome(income + upkeep)
		;
		
		double priority = getItemPriority(-facilityId, gain);
		
		// add demand
		
		productionDemand.addItemPriority(-facilityId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" moraleIncome=%5.2f"
			" defenseBuildCostSave=%5.2f"
			" defenseSupportSave=%5.2f"
			" bonus=%5.2f"
			" income=%5.2f"
			" upkeep=%5.2f"
			" gain=%5.2f"
			"\n"
			, Facility[facilityId].name
			, priority
			, moraleIncome
			, defenseBuildCostSave
			, defenseSupportSave
			, bonus
			, income
			, upkeep
			, gain
		);
		
	}
	
}

void evaluatePrototypingFacilities()
{
	debug("- evaluatePrototypingFacilities\n");
	
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	
	int facilityId = FAC_SKUNKWORKS;
	
	// free prototype faction cannot build Skunkworks
	
	if (isFactionSpecial(aiFactionId, RFLAG_FREEPROTO))
	{
		debug("\tfree prototype faction cannot build Skunkworks\n");
		return;
	}
	
	// facility should be available
	
	if (!isBaseCanBuildFacility(baseId, facilityId))
		return;
	
	// base should be 90% productive
	
	if (base->mineral_intake_2 < aiData.maxMineralIntake2)
	{
		debug("\tweak production\n");
		return;
	}
	
	// existing Skunkworks ratio
	
	int skunkworksCount = 0;
	
	for (int otherBaseId : aiData.baseIds)
	{
		// exclude self
		
		if (otherBaseId == baseId)
			continue;
		
		// has Skunkworks
		
		if (isBaseHasFacility(baseId, facilityId))
		{
			skunkworksCount++;
		}
		
	}
	
	double skunkworksRatio = (double)skunkworksCount / (double)aiData.baseIds.size();
	double useMultiplier = std::max(0.0, (0.1 - skunkworksRatio) / 0.1);
	
	if (useMultiplier <= 0.0)
	{
		debug("\tnot needed\n");
		return;
	}
	
	// gain
	
	double gain = 5.0 * useMultiplier;
	double priority = getItemPriority(-facilityId, gain);
	
	
	productionDemand.addItemPriority(-facilityId, priority);
	
	debug
	(
		"\t%-32s"
		" priority=%5.2f   |"
		" skunkworksRatio=%5.2f"
		" useMultiplier=%5.2f"
		" gain=%5.2f"
		"\n"
		, Facility[facilityId].name
		, priority
		, skunkworksRatio
		, useMultiplier
		, gain
	);
	
}

void evaluateExpansionUnits()
{
	const int TRACE = DEBUG && false;
	
	debug("evaluateExpansionUnits\n");
	
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	// verify base can build a colony
	
	if (!canBaseProduceColony(baseId))
	{
		debug("\tbase cannot build a colony\n");
		return;
	}
	
	// citizen loss gain
	
	double citizenIncome = getBaseCitizenIncome(baseId);
	double citizenLossGain = getGainIncome(-citizenIncome);
	
	// process colony units
	
	for (int unitId : aiData.colonyUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		
		// base cannot build sea colony
		
		if (triad == TRIAD_SEA && !isBaseAccessesWater(baseId))
			continue;
			
		// base adjacent sea region area is too small
		
		if (triad == TRIAD_SEA)
		{
			int seaCluster = getSeaCluster(baseTile);
			int seaClusterArea = getSeaClusterArea(seaCluster);
			
			if (seaClusterArea <= 10)
				continue;
			
		}
			
		if (TRACE) { debug("\t[%3d] %-32s\n", unitId, unit->name); }
		
		// iterate available build sites
		
		MAP *bestBuildSite = nullptr;
		double bestBuildSiteGain = 0.0;
		
		for (MAP *tile : availableBuildSites)
		{
			TileExpansionInfo &expansionTileInfo = getTileExpansionInfo(tile);
			bool ocean = aiData.getTileInfo(tile).ocean;
			
			// exclude unavailable build sites
			
			if (aiData.production.unavailableBuildSites.count(tile) != 0)
				continue;
			
			// matching realm
			
			if ((triad == TRIAD_LAND && ocean) || (triad == TRIAD_SEA && !ocean))
				continue;
				
			// check unit can reach destination
			
			if (!isUnitDestinationReachable(unitId, baseTile, tile))
				continue;
			
			// travel time
			
			double travelTime = getUnitATravelTime(unitId, baseTile, tile);
			
			// yield score
			
			double buildSiteBaseGain = expansionTileInfo.buildSiteBaseGain;
			
			// build gain
			
			double buildSiteBuildGain = getGainDelay(buildSiteBaseGain, travelTime);
			
			// upkeep
			
			double upkeepGain = getGainTimeInterval(getGainIncome(getResourceScore(-getBaseNextUnitSupport(baseId, unitId), 0)), 0, travelTime);
			
			// combine
			
			double buildSiteGain = buildSiteBuildGain + upkeepGain;
			
			// update best
			
			if (buildSiteGain > bestBuildSiteGain)
			{
				bestBuildSite = tile;
				bestBuildSiteGain = buildSiteGain;
			}
			
			if (TRACE)
			{
				debug
				(
					"\t\t%s"
					" travelTime=%7.2f"
					" buildSiteBaseGain=%5.2f"
					" buildSiteBuildGain=%5.2f"
					"\n"
					, getLocationString(tile).c_str()
					, travelTime
					, buildSiteBaseGain
					, buildSiteBuildGain
				);
			}
			
		}
		
		// combine with citizen loss gain
		
		double gain = bestBuildSiteGain + citizenLossGain;
		
		// priority
		
		double rawPriority = getItemPriority(unitId, gain);
		double priority =
			conf.ai_production_expansion_priority
			* globalColonyDemand
			* rawPriority
		;
		
		// set base demand
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" citizenLossGain=%5.2f"
			" bestBuildSite=%s"
			" bestBuildSiteGain=%5.2f"
			" gain=%5.2f"
			" conf.ai_production_expansion_priority=%5.2f"
			" globalColonyDemand=%5.2f"
			" rawPriority=%5.2f"
			"\n"
			, unit->name
			, priority
			, citizenLossGain
			, getLocationString(bestBuildSite).c_str()
			, bestBuildSiteGain
			, gain
			, conf.ai_production_expansion_priority
			, globalColonyDemand
			, rawPriority
		);
		
	}
	
}

void evaluateTerraformingUnits()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	// base clusters
	
	int baseSeaCluster = getSeaCluster(baseTile);
	int baseLandTransportedCluster = getLandTransportedCluster(baseTile);
	
	debug("evaluateTerraformingUnits\n");
	
	// process available former units
	
	for (int unitId : aiData.formerUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		
		// for sea former base should have access to water
		
		if (triad == TRIAD_SEA && !isBaseAccessesWater(baseId))
			continue;
		
		// unit priority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		
		if (unitPriorityCoefficient <= 0.0)
			continue;
		
		// best terraforming gain
		
		double bestTerraformingGain = 0.0;
		
		for (FormerRequest &formerRequest : aiData.production.formerRequests)
		{
			TileInfo &tileInfo = aiData.getTileInfo(formerRequest.tile);
			
			// compatible surface
			
			if ((triad == TRIAD_LAND && tileInfo.ocean) || (triad == TRIAD_SEA && !tileInfo.ocean))
				continue;
			
			// same cluster
			
			switch (triad)
			{
			case TRIAD_AIR:
				// assuming air formers can get anywhere
				break;
			case TRIAD_SEA:
				{
					int seaCluster = getSeaCluster(formerRequest.tile);
					if (seaCluster != baseSeaCluster)
						continue;
				}
				break;
			case TRIAD_LAND:
				{
					int landTransportedCluster = getLandTransportedCluster(formerRequest.tile);
					if (landTransportedCluster != baseLandTransportedCluster)
						continue;
				}
				break;
			}
			
			// travel time
			
			double travelTime = getUnitATravelTime(unitId, baseTile, formerRequest.tile);
			
			if (travelTime == INF)
				continue;
			
			// terraforming gain
			
			double effectiveTravelTime = conf.ai_terraforming_travel_time_multiplier * travelTime;
			double totalEffectiveTime = effectiveTravelTime + formerRequest.terraformingTime;
			double terraformingIncome = formerRequest.income / totalEffectiveTime;
			double terraformingGain = getGainIncomeGrowth(terraformingIncome);
			
			// update best
			
			bestTerraformingGain = std::max(bestTerraformingGain, terraformingGain);
			
		}
		
		if (bestTerraformingGain == 0.0)
			continue;
		
		// upkeep gain
		
		double upkeepIncome = getResourceScore(-(double)getBaseNextUnitSupport(baseId, unitId), 0.0);
		double upkeepGain = getGainIncome(upkeepIncome);
		
		// gain
		
		double gain = bestTerraformingGain + upkeepGain;
		
		// priority
		
		double rawPriority = getItemPriority(unitId, gain);
		double priority =
			conf.ai_production_improvement_priority
			* unitPriorityCoefficient
			* rawPriority
		;
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" bestTerraformingGain=%5.2f"
			" upkeepGain=%5.2f"
			" gain=%5.2f"
			" rawPriority=%5.2f"
			" conf.ai_production_improvement_priority=%5.2f"
			" unitPriorityCoefficient=%5.2f"
			"\n"
			, unit->name
			, priority
			, bestTerraformingGain
			, upkeepGain
			, gain
			, rawPriority
			, conf.ai_production_improvement_priority
			, unitPriorityCoefficient
		);
		
	}
	
}

void evaluateCrawlingUnits()
{
	int baseId = productionDemand.baseId;
//	MAP *baseTile = getBaseMapTile(baseId);
	
	// base clusters
	
//	int baseSeaCluster = getSeaCluster(baseTile);
//	int baseLandTransportedCluster = getLandTransportedCluster(baseTile);
//	
	debug("evaluateCrawlingUnits\n");
	
	// process available supply units
	
	for (int unitId : aiData.unitIds)
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		
		// crawler
		
		if (unit->weapon_id != WPN_SUPPLY_TRANSPORT)
			continue;
		
		// for sea former base should have access to water
		
		if (triad == TRIAD_SEA && !isBaseAccessesWater(baseId))
			continue;
		
		// unit priority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		
		if (unitPriorityCoefficient <= 0.0)
			continue;
		
		// fixed priority for now will work it out later
		
		double priority = 0.5 * unitPriorityCoefficient;
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" unitPriorityCoefficient=%5.2f"
//			" bestTerraformingGain=%5.2f"
//			" upkeepGain=%5.2f"
//			" gain=%5.2f"
//			" conf.ai_production_improvement_priority=%5.2f"
			"\n"
			, unit->name
			, priority
			, unitPriorityCoefficient
//			, bestTerraformingGain
//			, upkeepGain
//			, gain
//			, conf.ai_production_improvement_priority
		);
		
	}
	
}

void evaluatePodPoppingUnits()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	std::set<int> baseSeaRegions = getBaseSeaRegions(baseId);
	
	debug("evaluatePodPoppingUnits\n");
	
	std::array<int, 2> const podRanges {10, 20};
	std::array<SurfacePodData, 2> surfacePodDatas;
	
	for (int surface = 0; surface < 2; surface++)
	{
		SurfacePodData &surfacePodData = surfacePodDatas.at(surface);
		surfacePodData.scanRange = podRanges[surface];
		
		// base has access to water to collect sea pods
		
		if (surface == 1 && !isBaseAccessesWater(baseId))
			continue;
		
		// count pods around the base
		
		for (MAP *tile : getRangeTiles(baseTile, surfacePodData.scanRange, false))
		{
			int x = getX(tile);
			int y = getY(tile);
			
			// matching surface
			
			if (is_ocean(tile) != surface)
				continue;
			
			// same cluster
			
			if ((surface == 0 && !isSameLandTransportedCluster(baseTile, tile)) || (surface == 1 && !isSameSeaCluster(baseTile, tile)))
				continue;
			
			// pod
			
			if (!mod_goody_at(x, y))
				continue;
			
			// not hostile territory
			
			if (isHostileTerritory(aiFactionId, tile))
				continue;
			
			// accumulate
			
			surfacePodData.podCount++;
			
		}
		
		if (surfacePodData.podCount == 0)
			continue;
		
		// average pod distance
		
		surfacePodData.averagePodDistance = sqrt(0.5 * (2 * surfacePodData.scanRange + 1) * (2 * surfacePodData.scanRange + 1) / surfacePodData.podCount);
		
		// consumption rate
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			int triad = vehicle->triad();
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// not alien
			
			if (vehicle->faction_id == 0)
				continue;
			
			// pop popping vehicle
			
			if (!isPodPoppingVehicle(vehicleId))
				continue;
			
			// matching triad
			
			if (triad != surface)
				continue;
			
			// within range
			
			if (getRange(baseTile, vehicleTile) > surfacePodData.scanRange)
				continue;
			
			// same cluster
			
			if (vehicle->faction_id == aiFactionId)
			{
				if ((surface == 0 && !isSameLandTransportedCluster(baseTile, vehicleTile)) || (surface == 1 && !isSameSeaCluster(baseTile, vehicleTile)))
					continue;
			}
			else
			{
				if ((surface == 0 && !isSameEnemyLandCombatCluster(vehicle->faction_id, baseTile, vehicleTile)) || (surface == 1 && !isSameEnemySeaCombatCluster(vehicle->faction_id, baseTile, vehicleTile)))
					continue;
			}
			
			// not holding in base
			
			if (isBaseAt(vehicleTile) && triad == TRIAD_LAND && vehicle->order == ORDER_HOLD)
				continue;
			
			// consumption rate
			
			int vehicleSpeed = getVehicleSpeed(vehicleId);
			
			if (vehicleSpeed <= 0)
				continue;
			
			double travelTime = 0.5 * surfacePodData.averagePodDistance / (double)vehicleSpeed;
			double consumptionInterval = travelTime + (vehicleSpeed == 1 ? 4.0 : 1.0); // for average repair time
			double consumptionRate = 1.0 / consumptionInterval;
			
			// accumulate
			
			surfacePodData.totalConsumptionRate += consumptionRate;
			
			if (vehicle->faction_id == aiFactionId)
			{
				surfacePodData.factionConsumptionRate += consumptionRate;
			}
			
		}
		
		double consumptionTime = surfacePodData.podCount / surfacePodData.totalConsumptionRate;
		double factionIncome = conf.ai_production_pod_bonus * surfacePodData.factionConsumptionRate;
		double factionGain = getGainTimeInterval(getGainIncome(factionIncome), 0.0, consumptionTime);
		surfacePodData.factionConsumptionGain = factionGain;
		
	}
	
	// scan combat units
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		int surface = triad;
		
		// surface triad
		
		if (triad == TRIAD_AIR)
			continue;
		
		SurfacePodData &surfacePodData = surfacePodDatas.at(triad);
		
		// no pods
		
		if (surfacePodData.podCount == 0)
			continue;
		
		// pop popping unit
		
		if (!isPodPoppingUnit(unitId))
			continue;
		
		// exclude unproducible
		
		if (!isBaseCanBuildUnit(baseId, unitId))
			continue;
		
		// unit priority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		
		if (unitPriorityCoefficient <= 0.0)
			return;
		
		// consumption rate
		
		int unitSpeed = getUnitSpeed(aiFactionId, unitId);
		
		if (unitSpeed <= 0)
			continue;
		
		double unitTravelTime = 0.5 * surfacePodData.averagePodDistance / (double)unitSpeed;
		double unitConsumptionInterval = unitTravelTime + (unitSpeed == 1 ? 4.0 : 1.0); // for averate repair time
		double unitConsumptionRate = 1.0 / unitConsumptionInterval;
		
		double unitTotalConsumptionRate = surfacePodData.totalConsumptionRate + unitConsumptionRate;
		double unitFactionConsumptionRate = surfacePodData.factionConsumptionRate + unitConsumptionRate;
		
		double unitConsumptionTime = surfacePodData.podCount / unitTotalConsumptionRate;
		double unitFactionConsumptionIncome = conf.ai_production_pod_bonus * unitFactionConsumptionRate;
		double unitFactionConsumptionGain = getGainTimeInterval(getGainIncome(unitFactionConsumptionIncome), 0.0, unitConsumptionTime);
		
		double factionConsumptionGainIcrease = unitFactionConsumptionGain - surfacePodData.factionConsumptionGain;
		double podPoppingGain = factionConsumptionGainIcrease;
		
		debug
		(
			"\t\t%-32s"
			" surface=%2d"
			" podCount=%2d"
			" totalConsumptionRate=%5.2f"
			" factionConsumptionRate=%5.2f"
			" unitConsumptionRate=%5.2f"
			" factionConsumptionGain=%5.2f"
			" unitFactionConsumptionGain=%5.2f"
			" factionConsumptionGainIcrease=%5.2f"
			" podPoppingGain=%5.2f"
			"\n"
			, unit->name
			, surface
			, surfacePodData.podCount
			, surfacePodData.totalConsumptionRate
			, surfacePodData.factionConsumptionRate
			, unitConsumptionRate
			, surfacePodData.factionConsumptionGain
			, unitFactionConsumptionGain
			, factionConsumptionGainIcrease
			, podPoppingGain
		);
		
		// upkeep
		
		double upkeepGain = getGainIncome(getResourceScore(-getBaseNextUnitSupport(baseId, unitId), 0.0));
		
		// combined
		
		double gain =
			+ podPoppingGain
			+ upkeepGain
		;
		
		// priority
		
		double rawPriority = getItemPriority(unitId, gain);
		double priority =
			conf.ai_production_pod_popping_priority
			* unitPriorityCoefficient
			* rawPriority
		;
		
		// add production
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" conf.ai_production_pod_popping_priority=%5.2f"
			" unitPriorityCoefficient=%5.2f"
			" gain=%5.2f"
			" rawPriority=%5.2f"
			"\n"
			, Units[unitId].name
			, priority
			, conf.ai_production_pod_popping_priority
			, unitPriorityCoefficient
			, gain
			, rawPriority
		);
		
	}
	
}

void evaluateBaseDefenseUnits()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	debug("evaluateBaseDefenseUnits\n");
	
	// scan combat units for protection
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		
		// defensive
		
		if (!isInfantryDefensiveUnit(unitId))
			continue;
		
		// exclude those base cannot produce
		
		if (!isBaseCanBuildUnit(baseId, unitId))
			continue;
		
		// unit priority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		
		if (unitPriorityCoefficient <= 0.0)
			continue;
		
		// seek for best target base gain
		
		double bestGain = 0.0;
		
		for (int targetBaseId : aiData.baseIds)
		{
			MAP *targetBaseTile = getBaseMapTile(targetBaseId);
			BaseInfo &targetBaseInfo = aiData.getBaseInfo(targetBaseId);
			BasePoliceData &targetBasePoliceData = targetBaseInfo.policeData;
			BaseCombatData &targetBaseCombatData = targetBaseInfo.combatData;
			
			double travelTime = getUnitATravelTime(unitId, baseTile, targetBaseTile);
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// police
			
			double unitPoliceGain = targetBasePoliceData.isSatisfied(false) ? 0.0 : targetBasePoliceData.getUnitPoliceGain(unitId, aiFactionId);
			double policeGain = conf.ai_production_priority_police * getGainDelay(unitPoliceGain, travelTime);
			
			// protection
			
			double combatEffect = targetBaseCombatData.getUnitEffect(unitId);
			double survivalEffect = getSurvivalEffect(combatEffect);
			double unitProtectionGain = targetBaseCombatData.isSatisfied(false) ? 0.0 : aiFactionInfo->averageBaseGain * survivalEffect;
			double protectionGain = unitProtectionGain * travelTimeCoefficient;
			
			double upkeep = getResourceScore(-getBaseNextUnitSupport(baseId, unitId), 0);
			double upkeepGain = getGainIncome(upkeep);
			
			// combined
			
			double gain =
				+ policeGain
				+ protectionGain
				+ upkeepGain
			;
			
			bestGain = std::max(bestGain, gain);
			
			debug
			(
				"\t\t%-32s"
				" %-25s"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				" ai_production_priority_police=%5.2f"
				" unitPoliceGain=%5.2f"
				" policeGain=%5.2f"
				" ai_production_base_protection_priority=%5.2f"
				" combatEffect=%5.2f"
				" survivalEffect=%5.2f"
				" unitProtectionGain=%5.2f"
				" protectionGain=%5.2f"
				" upkeepGain=%5.2f"
				" gain=%7.2f"
				"\n"
				, unit->name
				, Bases[targetBaseId].name
				, travelTime
				, travelTimeCoefficient
				, conf.ai_production_priority_police
				, unitPoliceGain
				, policeGain
				, conf.ai_production_base_protection_priority
				, combatEffect
				, survivalEffect
				, unitProtectionGain
				, protectionGain
				, upkeepGain
				, gain
			);
			
		}
		
		// priority
		
		double rawPriority = getItemPriority(unitId, bestGain);
		double priority =
			conf.ai_production_base_protection_priority
			* unitPriorityCoefficient
			* rawPriority
		;
		
		// add production
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" ai_production_base_protection_priority=%5.2f"
			" unitPriorityCoefficient=%5.2f"
			" bestGain=%5.2f"
			" rawPriority=%5.2f"
			"\n"
			, Units[unitId].name
			, priority
			, conf.ai_production_base_protection_priority
			, unitPriorityCoefficient
			, bestGain
			, rawPriority
		);
		
	}
	
}

void evaluateTerritoryProtectionUnits()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	debug("evaluateTerritoryProtectionUnits\n");
	
	// scan combat units for protection
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		
		// exclude those base cannot produce
		
		if (!isBaseCanBuildUnit(baseId, unitId))
			continue;
		
		// unit priority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		
		if (unitPriorityCoefficient <= 0.0)
			continue;
		
		// seek for best unsatisfied target stack gain
		
		double bestGain = 0.0;
		
		for (EnemyStackInfo *enemyStackInfo : aiData.production.untargetedEnemyStackInfos)
		{
			// enemy within territory or neutral on land within base radius
			
			if
			(
				!(
					(enemyStackInfo->hostile && enemyStackInfo->tile->owner == aiFactionId)
					||
					(!is_ocean(enemyStackInfo->tile) && enemyStackInfo->tile->owner == aiFactionId && map_has_item(enemyStackInfo->tile, BIT_BASE_RADIUS))
				)
			)
				continue;
			
			// ship and artillery do not attack tower
			
			if (isArtilleryUnit(unitId) && enemyStackInfo->alienFungalTower)
				continue;
			
			// get attack position
			
			MAP *position = nullptr;
			
			if (position == nullptr && enemyStackInfo->isUnitCanMeleeAttackStack(unitId))
			{
				position = getMeleeAttackPosition(unitId, baseTile, enemyStackInfo->tile);
			}
			
			if (position == nullptr && enemyStackInfo->isUnitCanArtilleryAttackStack(unitId))
			{
				position = getArtilleryAttackPosition(unitId, baseTile, enemyStackInfo->tile);
			}
			
			if (position == nullptr)
				continue;
			
			// travel time
			
			double travelTime = getUnitATravelTime(unitId, baseTile, position);
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// gain
			
			double combatEffect = enemyStackInfo->getUnitEffect(unitId);
			double survivalEffect = getSurvivalEffect(combatEffect);
			double attackGain = enemyStackInfo->averageUnitGain;
			double protectionGain =
				attackGain
				* survivalEffect
				* travelTimeCoefficient
			;
			
			double upkeep = getResourceScore(-getBaseNextUnitSupport(baseId, unitId), 0);
			double upkeepGain = getGainTimeInterval(getGainIncome(upkeep), 0.0, travelTime);
			
			double gain =
				+ protectionGain
				+ upkeepGain
			;
			
			bestGain = std::max(bestGain, gain);
			
			debug
			(
				"\t\t%-32s"
				" -> %s"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				" ai_production_base_protection_priority=%5.2f"
				" combatEffect=%5.2f"
				" survivalEffect=%5.2f"
				" attackGain=%5.2f"
				" protectionGain=%5.2f"
				" upkeepGain=%5.2f"
				" gain=%7.2f"
				"\n"
				, unit->name
				, getLocationString(enemyStackInfo->tile).c_str()
				, travelTime
				, travelTimeCoefficient
				, conf.ai_production_base_protection_priority
				, combatEffect
				, survivalEffect
				, attackGain
				, protectionGain
				, upkeepGain
				, gain
			);
			
		}
		
		// priority
		
		double rawPriority = getItemPriority(unitId, bestGain);
		double priority =
			conf.ai_production_base_protection_priority
			* unitPriorityCoefficient
			* rawPriority
		;
		
		// add production
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" unitPriorityCoefficient=%5.2f"
			" bestGain=%5.2f"
			" rawPriority=%5.2f"
			"\n"
			, Units[unitId].name
			, priority
			, unitPriorityCoefficient
			, bestGain
			, rawPriority
		);
		
	}
	
}

void evaluateEnemyBaseAssaultUnits()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	debug("evaluateEnemyBaseAssaultUnits\n");
	
	// scan combat units
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		
		// unit priority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		
		if (unitPriorityCoefficient <= 0.0)
			return;
		
		// exclude those base cannot produce
		
		if (!isBaseCanBuildUnit(baseId, unitId))
			continue;
		
		debug("\t%-32s\n", unit->name);
		
		// iterate potential enemy bases
		
		double bestGain = 0.0;
		
		for (int enemyBaseId = 0; enemyBaseId < *BaseCount; enemyBaseId++)
		{
			BASE *enemyBase = getBase(enemyBaseId);
			MAP *enemyBaseTile = getBaseMapTile(enemyBaseId);
			BaseInfo &enemyBaseInfo = aiData.getBaseInfo(enemyBaseId);
			FactionInfo &factionInfo = aiData.factionInfos[enemyBase->faction_id];
			
			// enemy base stack
			
			if (!aiData.isEnemyStackAt(enemyBaseTile))
				continue;
			
			EnemyStackInfo &enemyStackInfo = aiData.getEnemyStackInfo(enemyBaseTile);
			
			// exclude player base
			
			if (enemyBase->faction_id == aiFactionId)
				continue;
			
			// travel time
			
			double travelTime = getPlayerApproachTime(enemyBaseId, unitId, baseTile);
			
			if (travelTime == INF)
				continue;
			
			// proportional base capture value
			
			double assaultEffect = enemyBaseInfo.assaultEffects.at(unitId);
			double productionRatio = aiFactionInfo->productionPower / factionInfo.productionPower;
			double costRatio = enemyStackInfo.averageUnitCost / (double)unit->cost;
			double adjustedEffect = assaultEffect * productionRatio * costRatio;
			double adjustedSuperiority = adjustedEffect - 2.0;
			
			if (adjustedSuperiority <= 0.0)
				continue;
			
			double enemyBaseCaptureGain = enemyBaseInfo.captureGain;
			double adjustedEnemyBaseCaptureGain  = enemyBaseCaptureGain * adjustedSuperiority;
			double incomeGain = getGainDelay(adjustedEnemyBaseCaptureGain, travelTime);
			
			// range coefficient
			
			double rangeCoefficient = enemyBaseInfo.closestPlayerBaseRange <= TARGET_ENEMY_BASE_RANGE ? 1.0 : (double)TARGET_ENEMY_BASE_RANGE / (double)enemyBaseInfo.closestPlayerBaseRange;
			double captureGain = rangeCoefficient * incomeGain;
			
			// upkeep gain
			
			double upkeepGain = getGainTimeInterval(getGainIncome(getResourceScore(-getUnitSupport(unitId), 0)), 0.0, travelTime);
			
			// combined gain
			
			double gain =
				+ captureGain
				+ upkeepGain
			;
			
			bestGain = std::max(bestGain, gain);
			
			debug
			(
				"\t\t%-25s"
				" travelTime=%7.2f"
				" assaultEffect=%5.2f"
				" productionRatio=%5.2f"
				" costRatio=%5.2f"
				" adjustedEffect=%5.2f"
				" adjustedSuperiority=%5.2f"
				" enemyBaseCaptureGain=%5.2f"
				" adjustedEnemyBaseCaptureGain=%5.2f"
				" incomeGain=%5.2f"
				" closestPlayerBaseRange=%2d"
				" rangeCoefficient=%5.2f"
				" captureGain=%5.2f"
				" upkeepGain=%5.2f"
				" gain=%5.2f"
				"\n"
				, enemyBase->name
				, travelTime
				, assaultEffect
				, productionRatio
				, costRatio
				, adjustedEffect
				, adjustedSuperiority
				, enemyBaseCaptureGain
				, adjustedEnemyBaseCaptureGain
				, incomeGain
				, enemyBaseInfo.closestPlayerBaseRange
				, rangeCoefficient
				, captureGain
				, upkeepGain
				, gain
			);
			
		}
		
		// priority
		
		double rawPriority = getItemPriority(unitId, bestGain);
		double priority =
			unitPriorityCoefficient
			* rawPriority
		;
		
		// add production
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" unitPriorityCoefficient=%5.2f"
			" bestGain=%5.2f"
			" rawPriority=%5.2f"
			"\n"
			, Units[unitId].name
			, priority
			, unitPriorityCoefficient
			, bestGain
			, rawPriority
		);
		
	}
	
}

void evaluateSeaTransport()
{
	debug("evaluateSeaTransport\n");
	
	int baseId = productionDemand.baseId;
	int baseSeaCluster = productionDemand.baseSeaCluster;
	
	// base should have access to water
	
	if (baseSeaCluster == -1)
		return;
	
	// find transport unit
	
	int unitId = aiFactionInfo->bestSeaTransportUnitId;
	
	if (unitId == -1)
	{
		debug("\tno seaTransport unit\n");
		return;
	}
	
	double seaTransportDemand = seaTransportDemands[baseSeaCluster];
	
	if (seaTransportDemand <= 0.0)
	{
		debug("seaTransportDemand <= 0.0\n");
		return;
	}
	
	// set priority to the current highest priority for best producing base
	
	if (bestSeaTransportProductionBaseId.find(baseSeaCluster) != bestSeaTransportProductionBaseId.end() && bestSeaTransportProductionBaseId.at(baseSeaCluster) == baseId)
	{
		productionDemand.item = unitId;
		debug("\tmandatory seaTransport\n");
		debug
		(
			"\t%-32s"
			" seaTransportDemand=%5.2f"
			"\n"
			, getUnit(unitId)->name
			, seaTransportDemand
		);
	}
	else
	{
		// piority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		double priority =
			conf.ai_production_transport_priority
			* seaTransportDemand
			* unitPriorityCoefficient
		;
		
		// set priority
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" seaTransportDemand=%5.2f"
			" conf.ai_production_transport_priority=%5.2f"
			" unitPriorityCoefficient=%5.2f"
			"\n"
			, getUnit(unitId)->name
			, priority
			, seaTransportDemand
			, conf.ai_production_transport_priority
			, unitPriorityCoefficient
		);
		
	}
	
}

int findAntiNativeDefenderUnit(bool ocean)
{
	debug("findAntiNativeDefenderUnit\n");

    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];

		// infantry

		if (unit->chassis_id != CHS_INFANTRY)
			continue;

		// get true cost

		int cost = getCombatUnitTrueCost(unitId);

		// compute protection against corresponding realm alien

		int basicAlienUnit = (ocean ? BSC_SEALURK : BSC_MIND_WORMS);
		double defendCombatEffect = aiData.combatEffectTable.getCombatEffect(unitId, 0, basicAlienUnit, AS_FOE, CM_MELEE);

		// compute effectiveness

		double effectiveness = defendCombatEffect / (double)cost;

		// prefer police

		if (unit_has_ability(unitId, ABL_POLICE_2X))
		{
			effectiveness *= 2.0;
		}

		// update best

		if (effectiveness > bestUnitEffectiveness)
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

		if (*CurrentTurn > 50)
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

int findScoutUnit(int triad)
{
	int bestUnitId = -1;
	double bestProportionalUnitChassisSpeed = -1;
	int bestUnitChassisSpeed = -1;

	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = &(Units[unitId]);

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

/**
Checks if base has enough population and minerals to issue a colony by the time it is built.
*/
bool canBaseProduceColony(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	if (base->mineral_surplus <= 1)
		return false;
	
	// projectedTime
	
	int buildTime = getBaseItemBuildTime(baseId, BSC_COLONY_POD, true);
	int projectedSize = getBaseProjectedSize(baseId, buildTime);
	
	if (projectedSize == 1)
		return false;
	
	// current parameters
	
	int currentSize = base->pop_size;
	
	// set projected
	
	base->pop_size = projectedSize - 1;
	computeBase(baseId, true);
	
	int projectedMineralSurplus = base->mineral_surplus;
	
	// restore
	
	base->pop_size = currentSize;
	computeBase(baseId, true);
	
	// check surplus is not below zero
	
	if (projectedMineralSurplus < 0)
		return false;
	
	// all checks passed
	
	return true;
	
}

int findFormerUnit(int triad)
{
	int bestUnitId = -1;
	double bestUnitValue = -1;

	for (int unitId : aiData.unitIds)
	{
		UNIT *unit = &(Units[unitId]);

		// formers only

		if (!isFormerUnit(unitId))
			continue;

		// given triad only or air

		if (!(unit_triad(unitId) == TRIAD_AIR || unit_triad(unitId) == triad))
			continue;

		// calculate value

		double unitTerraformingSpeedValue =
			(unit_has_ability(unitId, ABL_FUNGICIDAL) ? 1.2 : 1.0)
			*
			(unit_has_ability(unitId, ABL_SUPER_TERRAFORMER) ? 2.5 : 1.0)
		;
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
Calculates number of units with specific weaponType: existing and building.
Only corresponding triad units are counted.
*/
int calculateUnitTypeCount(int factionId, int weaponType, int triad, int excludedBaseId)
{
	int unitTypeCount = 0;

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (vehicle->faction_id != factionId)
			continue;

		if (Units[vehicle->unit_id].weapon_id != weaponType)
			continue;

		if ((triad != -1) && (vehicle->triad() != triad))
			continue;

		unitTypeCount++;

	}

	for (int baseId = 0; baseId < *BaseCount; baseId++)
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

		if (Units[item].weapon_id != weaponType)
			continue;

		if (unit_triad(item) != triad)
			continue;

		unitTypeCount++;

	}

	return unitTypeCount;

}

/*
Checks for military purpose unit/structure.
*/
bool isMilitaryItem(int item)
{
	if (item >= 0)
	{
		UNIT *unit = &(Units[item]);
		int weaponId = unit->weapon_id;

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
	int baseSeaCluster = getBaseSeaCluster(baseId);
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseSeaCluster = getBaseSeaCluster(targetBaseId);
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();

    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();
        int chassisSpeed = unit->speed();

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

				// no sea attacker if in different cluster

				if (baseSeaCluster == -1 || targetBaseSeaCluster == -1 || baseSeaCluster != targetBaseSeaCluster)
					continue;

			}

		}

		// skip units unable to attack each other

		if
		(
			(foeUnitTriad == TRIAD_LAND && triad == TRIAD_SEA)
			||
			(foeUnitTriad == TRIAD_SEA && triad == TRIAD_LAND)
			||
			(foeUnit->chassis_id == CHS_NEEDLEJET && !unit_has_ability(unitId, ABL_AIR_SUPERIORITY))
		)
		{
			continue;
		}

		// relative strength

		double relativeStrength;

		if (unit->weapon_id == WPN_PSI_ATTACK || foeUnit->armor_id == ARM_PSI_DEFENSE)
		{
			relativeStrength =
				(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
				/ foeUnitStrength->psiDefense
			;

		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power ? 10.0 : unit->reactor_id) * getNewUnitConOffenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalDefense
				* getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;

		}

		// unit speed factor
		// land unit is assumed to move on roads

		int infantrySpeed = getLandUnitSpeedOnRoads(BSC_SCOUT_PATROL);
		int speed = (triad == TRIAD_LAND ? getLandUnitSpeedOnRoads(unitId) : chassisSpeed);
		double speedFactor = (double)(speed + 3) / (double)(infantrySpeed + 3);

		// calculate cost counting support

		int cost = getCombatUnitTrueCost(unitId);

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
	int baseSeaCluster = getBaseSeaCluster(baseId);
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseSeaCluster = getBaseSeaCluster(targetBaseId);
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

				// no sea defender if in different cluster

				if (baseSeaCluster == -1 || targetBaseSeaCluster == -1 || baseSeaCluster != targetBaseSeaCluster)
					continue;

			}

		}

		// skip units unable to attack each other

		if
		(
			(foeUnitTriad == TRIAD_LAND && triad == TRIAD_SEA)
			||
			(foeUnitTriad == TRIAD_SEA && triad == TRIAD_LAND)
		)
		{
			continue;
		}

		// relative strength

		double relativeStrength;

		if (unit->armor_id == ARM_PSI_DEFENSE || foeUnit->weapon_id == WPN_PSI_ATTACK)
		{
			relativeStrength =
				(10.0 * getNewUnitPsiDefenseStrength(unitId, baseId))
				/ foeUnitStrength->psiOffense
			;


		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power ? 10.0 : unit->reactor_id) * getNewUnitConDefenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalOffense
				/ getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;

		}

		// chassis speed factor
		// not important for base defender

		double speedFactor = 1.0;

		// calculate cost counting maintenance

		int cost = getCombatUnitTrueCost(unitId);

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
	int baseSeaCluster = getBaseSeaCluster(baseId);
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseSeaCluster = getBaseSeaCluster(targetBaseId);
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

				// no sea attacker/mixed if in different cluster

				if (baseSeaCluster == -1 || targetBaseSeaCluster == -1 || baseSeaCluster != targetBaseSeaCluster)
					continue;

			}

		}

		// skip units unable to attack each other

		if
		(
			(foeUnitTriad == TRIAD_LAND && triad == TRIAD_SEA)
			||
			(foeUnitTriad == TRIAD_SEA && triad == TRIAD_LAND)
		)
		{
			continue;
		}

		// relative strength

		double relativeStrength = 1.0;

		if (unit->weapon_id == WPN_PSI_ATTACK || foeUnit->armor_id == ARM_PSI_DEFENSE)
		{
			relativeStrength *=
				(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
				/ foeUnitStrength->psiDefense
			;

		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power ? 10.0 : unit->reactor_id) * getNewUnitConOffenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalDefense
				* getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;

		}

		if (unit->armor_id == ARM_PSI_DEFENSE || foeUnit->weapon_id == WPN_PSI_ATTACK)
		{
			relativeStrength *=
				(10.0 * getNewUnitPsiDefenseStrength(unitId, baseId))
				/ foeUnitStrength->psiOffense
			;

		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power ? 10.0 : unit->reactor_id) * getNewUnitConDefenseStrength(unitId, baseId))
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

		int cost = getCombatUnitTrueCost(unitId);

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
			if (unit->weapon_id == WPN_PSI_ATTACK || foeUnit->weapon_id == WPN_PSI_ATTACK)
			{
				relativeStrength =
					(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
					/ foeUnitStrength->psiOffense
				;

			}
			else
			{
				relativeStrength =
					((conf.ignore_reactor_power ? 10.0 : unit->reactor_id) * getNewUnitConOffenseStrength(unitId, baseId))
					/ foeUnitStrength->conventionalOffense
				;

			}

		}
		else
		{
			if (unit->weapon_id == WPN_PSI_ATTACK || foeUnit->armor_id == ARM_PSI_DEFENSE)
			{
				relativeStrength =
					(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
					/ foeUnitStrength->psiDefense
				;

			}
			else
			{
				relativeStrength =
					((conf.ignore_reactor_power ? 10.0 : unit->reactor_id) * getNewUnitConOffenseStrength(unitId, baseId))
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

		int cost = getCombatUnitTrueCost(unitId);

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
int selectProtectionUnit(int baseId, int targetBaseId)
{
	bool TRACE = DEBUG && false;
	
	if (TRACE) { debug("selectProtectionUnit\n"); }
	
	int baseSeaCluster = getBaseSeaCluster(baseId);
	
	// no target base
	
	if (targetBaseId == -1)
	{
		return -1;
	}
	
	// get target base strategy
	
	bool targetBaseOcean = is_ocean(getBaseMapTile(targetBaseId));
	int targetBaseSeaCluster = getBaseSeaCluster(targetBaseId);
	BaseInfo &targetBaseInfo = aiData.getBaseInfo(targetBaseId);
	BaseCombatData &targetBaseCombatData = targetBaseInfo.combatData;
	
	// iterate best protectors at target base
	
	int bestUnitId = -1;
	double bestUnitPreference = 0.0;
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		int offenseValue = getUnitOffenseValue(unitId);
		int defenseValue = getUnitDefenseValue(unitId);
		double averageCombatEffect = targetBaseCombatData.getUnitEffect(unitId);
		
		// infantry defender
		
		if (!isInfantryDefensiveUnit(unitId))
			continue;
			
		// skip that unfit for target base or cannot travel there
		
		switch (triad)
		{
		case TRIAD_LAND:
			{
				// do not send any land unit besides infantry defensive to ocean base unless amphibious
				
				if (targetBaseOcean && !isInfantryDefensiveUnit(unitId))
					continue;
					
			}
			break;
			
		case TRIAD_SEA:
			{
				// do not send sea units to different ocean cluster
				
				if (baseSeaCluster == -1 || targetBaseSeaCluster == -1 || baseSeaCluster != targetBaseSeaCluster)
					continue;
					
			}
			break;
			
		}
		
		// compute true cost
		// this should not include prototype cost
		
		int cost = getCombatUnitTrueCost(unitId);
		
		// compute effectiveness
		
		double effectiveness = averageCombatEffect / (double)cost;
		
		// give slight preference to more advanced units
		
		double preferenceModifier = 1.0;
		
		if (!isNativeUnit(unitId))
		{
			preferenceModifier *= (1.0 + 0.2 * (double)((offenseValue - 1) + (defenseValue - 1)));
		}
		
		// combined preference
		
		double preference = effectiveness * preferenceModifier;
		
		// update best
		
		if (TRACE)
		{
			debug
			(
				"\t%-32s"
				" preference=%5.2f"
				" cost=%2d"
				" averageCombatEffect=%5.2f"
				" effectiveness=%5.2f"
				" preferenceModifier=%5.2f"
				"\n"
				, getUnit(unitId)->name
				, preference
				, cost
				, averageCombatEffect
				, effectiveness
				, preferenceModifier
			);
		}
		
		if (preference > bestUnitPreference)
		{
			bestUnitId = unitId;
			bestUnitPreference = preference;
			if (TRACE) { debug("\t\t- best\n"); }
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

		if (unit->chassis_id != CHS_INFANTRY)
			continue;

		// get trance

		int trance = (isUnitHasAbility(unitId, ABL_TRANCE) ? 1 : 0);

		// get cost

		int cost = unit->cost;

		// update best

		if
		(
			(trance > bestUnitTrance)
			||
			(trance == bestUnitTrance && cost < bestUnitCost)
		)
		{
			bestUnitId = unitId;
			bestUnitTrance = trance;
			bestUnitCost = cost;
		}

	}

	return bestUnitId;

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

void evaluateProject()
{
	debug("evaluateProject\n");
	
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	
	double priority =
		conf.ai_production_current_project_priority
	;
	
	if (isBaseBuildingProject(baseId))
	{
		productionDemand.addItemPriority(base->queue_items[0], priority);
	}
	
}

/*
Checks if base can build unit.
*/
bool isBaseCanBuildUnit(int baseId, int unitId)
{
	BASE *base = getBase(baseId);
	UNIT *unit = getUnit(unitId);
	
	int baseSeaCluster = getBaseSeaCluster(baseId);
	
	// no sea unit without access to water
	
	if (baseSeaCluster == -1 && unit->triad() == TRIAD_SEA)
		return false;
	
	// require technology for predefined
	
	if (unitId < MaxProtoFactionNum)
		return isFactionHasTech(base->faction_id, unit->preq_tech);
	
	// no restrictions
	
	return true;
	
}

/*
Checks if base can build facility.
*/
bool isBaseCanBuildFacility(int baseId, int facilityId)
{
	BASE *base = getBase(baseId);
	CFacility *facility = getFacility(facilityId);
	
	int baseSeaCluster = getBaseSeaCluster(baseId);
	
	// no sea facility without access to water
	
	if (baseSeaCluster == -1 && (facilityId == FAC_NAVAL_YARD || facilityId == FAC_AQUAFARM || facilityId == FAC_SUBSEA_TRUNKLINE || facilityId == FAC_THERMOCLINE_TRANSDUCER))
		return false;
	
	// require technology and facility should not exist
	
	return isFactionHasTech(base->faction_id, facility->preq_tech) && !isBaseHasFacility(baseId, facilityId);
	
}

/*
Returns first available but unbuilt facility from list.
*/
int getFirstAvailableFacility(int baseId, std::vector<int> facilityIds)
{
	for (int facilityId : facilityIds)
	{
		if (isBaseCanBuildFacility(baseId, facilityId))
		{
			return facilityId;
		}
		
	}
	
	// nothing is available
	
	return -1;
	
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

/**
Calculates unit priority reduction based on existing support.
*/
double getUnitPriorityCoefficient(int baseId, int unitId)
{
	BASE *base = &(Bases[baseId]);
	
	// at max
	
	if (*VehCount >= MaxVehNum)
		return 0.0;
	
	// baseNextUnitSupport
	
	int nextUnitSupport = getBaseNextUnitSupport(baseId, unitId);
	
	if (nextUnitSupport == 0)
		return 1.0;
	
	// reserved mineral surplus
	
	int reservedMineralSurplus = isFormerUnit(unitId) ? 1 : 2;
	
	if (base->mineral_surplus <= reservedMineralSurplus)
		return 0.0;
	
	// reduce priority quadratic proportional to remained surplus below reserved
	
	int maxSurplus = base->mineral_intake_2 - reservedMineralSurplus;
	int newSurplus = base->mineral_surplus - reservedMineralSurplus - nextUnitSupport;
	double suprlusRatio = (double)newSurplus / (double)maxSurplus;
	
	return suprlusRatio * suprlusRatio;
	
}

int findInfantryPoliceUnit(bool first)
{
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    // search among police2x

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int defenseValue = getUnitDefenseValue(unitId);

        // infantry police2x

        if (!isInfantryDefensivePolice2xUnit(unitId, aiFactionId))
			continue;

		// calculate effectiveness

		double benefit = 1.0 + (first ? 0.0 : ((double)(defenseValue - 1) / (double)aiData.maxConDefenseValue) / 2.0);
		double cost = (double)unit->cost;

		if (!isUnitHasAbility(unitId, ABL_CLEAN_REACTOR))
		{
			cost += 8.0;
		}

		if (isUnitHasAbility(unitId, ABL_TRANCE))
		{
			benefit *= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi / 2);
		}

		double effectiveness = (cost <= 0.0 ? 0.0 : benefit / cost);

		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

	if (bestUnitId != -1)
	{
		return bestUnitId;
	}

	// search among others

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int defenseValue = getUnitDefenseValue(unitId);

        // infantry

        if (!isInfantryDefensiveUnit(unitId))
			continue;

		// calculate effectiveness

		double benefit = 1.0 + (first ? 0.0 : ((double)(defenseValue - 1) / (double)aiData.maxConDefenseValue) / 2.0);
		double cost = (double)unit->cost;

		if (!isUnitHasAbility(unitId, ABL_CLEAN_REACTOR))
		{
			cost += 8.0;
		}

		if (isUnitHasAbility(unitId, ABL_TRANCE))
		{
			benefit *= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi / 2);
		}

		double effectiveness = (cost <= 0.0 ? 0.0 : benefit / cost);

		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

	return bestUnitId;

}

void selectProject()
{
	debug("selectProject - %s\n", getMFaction(aiFactionId)->noun_faction);

	// find project

	int cheapestProjectFacilityId = -1;
	int cheapestProjectFacilityCost = INT_MAX;

	for (int projectFacilityId = FAC_HUMAN_GENOME_PROJECT; projectFacilityId <= FAC_PLANETARY_ENERGY_GRID; projectFacilityId++)
	{
		// exclude not available project

		if (!isProjectAvailable(aiFactionId, projectFacilityId))
			continue;

		// get project cost

		int cost = getFacility(projectFacilityId)->cost;

		debug("\t%-30s cost=%3d\n", getFacility(projectFacilityId)->name, getFacility(projectFacilityId)->cost);

		// update best

		if (cost < cheapestProjectFacilityCost)
		{
			debug("\t\t- best\n");
			cheapestProjectFacilityId = projectFacilityId;
			cheapestProjectFacilityCost = cost;
		}

	}

	if (cheapestProjectFacilityId != -1)
	{
		selectedProject = cheapestProjectFacilityId;
	}

}

void assignProject()
{
	debug("assignProject - %s\n", getMFaction(aiFactionId)->noun_faction);

	if (selectedProject == -1)
	{
		debug("\tno selected project\n");
		return;
	}

	CFacility *projectFacility = getFacility(selectedProject);

	debug("\t%s\n", projectFacility->name);

	// check if project is building

	bool projectBuilding = false;
	int totalMineralSurplus = 0;
	int mostSuitableBaseId = -1;
	double mostSuitableBaseValue = 0.0;
	int mostSuitableBaseMineralSurplus = 0;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = getBase(baseId);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);

		if (isBaseBuildingProject(baseId))
		{
			projectBuilding = true;
			base->queue_items[0] = -selectedProject;
			break;
		}

		// mineral surplus

		int mineralSurplus = base->mineral_surplus;
		totalMineralSurplus += mineralSurplus;

		// threat

		double threat = baseInfo.combatData.requiredEffect;

		// value

		double value = (double)mineralSurplus - threat;

		if (value > mostSuitableBaseValue)
		{
			mostSuitableBaseId = baseId;
			mostSuitableBaseValue = value;
			mostSuitableBaseMineralSurplus = mineralSurplus;
		}

	}

	if (projectBuilding)
	{
		debug("\tproject is building\n");
		return;
	}

	if (mostSuitableBaseId == -1)
	{
		debug("\tmost suitable base not found\n");
		return;
	}

	BASE *mostSuitableBase = getBase(mostSuitableBaseId);

	debug("\t%s\n", mostSuitableBase->name);

	// compute base production fraction

	double baseProductionFraction = (double)mostSuitableBaseMineralSurplus / (double)std::max(1, totalMineralSurplus);

	if (baseProductionFraction > conf.ai_production_project_mineral_surplus_fraction)
	{
		debug
		(
			"\tbaseProductionFraction=%5.2f > conf.ai_production_project_mineral_surplus_fraction=%5.2f\n"
			, baseProductionFraction
			, conf.ai_production_project_mineral_surplus_fraction
		);
		return;
	}

	// exclude base building psych facility

	switch (getBaseBuildingItem(mostSuitableBaseId))
	{
	case -FAC_RECREATION_COMMONS:
	case -FAC_HOLOGRAM_THEATRE:
	case -FAC_PARADISE_GARDEN:
	case -FAC_RESEARCH_HOSPITAL:
	case -FAC_NANOHOSPITAL:
		return;
	}

	// build project

	mostSuitableBase->queue_items[0] = - selectedProject;

	debug("\tproject set\n");

	// build mineral multiplying facility instead if more effective
	// This is a hack. I should better balance all priorities.

	int mineralMultiplyingFacilityId =
		getFirstAvailableFacility
		(
			mostSuitableBaseId,
			{FAC_RECYCLING_TANKS, FAC_GENEJACK_FACTORY, FAC_ROBOTIC_ASSEMBLY_PLANT, FAC_QUANTUM_CONVERTER, FAC_NANOREPLICATOR}
		)
	;

	// no available mineral multiplying facility

	if (mineralMultiplyingFacilityId == -1)
		return;

	// this multiplying facility is already building

	int currentItem = getBaseBuildingItem(mostSuitableBaseId);

	if (currentItem == -mineralMultiplyingFacilityId)
		return;

	int currentMineralMultiplierNumerator = divideIntegerRoundUp(2 * mostSuitableBase->mineral_intake_2, mostSuitableBase->mineral_intake);
	int improvedMineralMultiplierNumerator = currentMineralMultiplierNumerator + 1;
	int currentMineralIntake2 = mostSuitableBase->mineral_intake_2;
	int improvedMineralIntake2 = mostSuitableBase->mineral_intake * improvedMineralMultiplierNumerator / 2;
	int currentMineralProduction = mostSuitableBase->mineral_surplus;
	int improvedMineralProduction = currentMineralProduction + (improvedMineralIntake2 - currentMineralIntake2);
	int currentItemBuildTime = getBaseItemBuildTime(mostSuitableBaseId, currentItem);
	int improvedItemBuildTime = divideIntegerRoundUp(getBaseMineralCost(mostSuitableBaseId, currentItem), improvedMineralProduction);
	int itemBuildTimeReduction = std::max(0, currentItemBuildTime - improvedItemBuildTime);
	int mineralMultiplyingFacilityBuildTime = getBaseItemBuildTime(mostSuitableBaseId, -mineralMultiplyingFacilityId);

	// not enough time reduction

	if (itemBuildTimeReduction < divideIntegerRoundUp(mineralMultiplyingFacilityBuildTime, 2))
		return;

	// build mineral multiplying facility instead

	mostSuitableBase->queue_items[0] = -mineralMultiplyingFacilityId;

	debug("\tbuild mineral multiplying facility first\n");

	debug
	(
		"\t%-32s"
		" itemBuildTimeReduction=%2d"
		" mineralMultiplyingFacilityBuildTime=%2d"
		" currentMineralMultiplierNumerator=%2d"
		" improvedMineralMultiplierNumerator=%2d"
		" currentMineralIntake2=%2d"
		" improvedMineralIntake2=%2d"
		" currentMineralProduction=%2d"
		" improvedMineralProduction=%2d"
		" currentItemBuildTime=%2d"
		" improvedItemBuildTime=%2d"
		"\n"
		, getFacility(mineralMultiplyingFacilityId)->name
		, itemBuildTimeReduction
		, mineralMultiplyingFacilityBuildTime
		, currentMineralMultiplierNumerator
		, improvedMineralMultiplierNumerator
		, currentMineralIntake2
		, improvedMineralIntake2
		, currentMineralProduction
		, improvedMineralProduction
		, currentItemBuildTime
		, improvedItemBuildTime
	);

}

void hurryProtectiveUnit()
{
	debug("hurryProtectiveUnit - %s\n", getMFaction(aiFactionId)->noun_faction);

	int mostVulnerableBaseId = -1;
	double mostVulnerableBaseRelativeRemainingEffect = 0.50;

	for (int baseId : aiData.baseIds)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);

		debug("\t%-25s\n", getBase(baseId)->name);

		int item = getBaseBuildingItem(baseId);

		debug("\t\titem=%3d\n", item);

		if (item < 0 || !isInfantryDefensiveUnit(item))
		{
			debug("\t\tnot infantry devensive\n");
			continue;
		}

		if (baseInfo.combatData.requiredEffect <= 0.0)
			continue;

		double relativeRemainingEffect = (1.0 - baseInfo.combatData.providedEffect / baseInfo.combatData.requiredEffect);

		debug("\t\tremainingEffect=%3d\n", item);

		if (relativeRemainingEffect > mostVulnerableBaseRelativeRemainingEffect)
		{
			mostVulnerableBaseId = baseId;
			mostVulnerableBaseRelativeRemainingEffect = relativeRemainingEffect;
		}

	}

	if (mostVulnerableBaseId == -1)
		return;

	debug
	(
		"\tmostVulnerableBase=%-25s"
		" mostVulnerableBaseRelativeRemainingEffect=%5.2f"
		"\n"
		, getBase(mostVulnerableBaseId)->name
		, mostVulnerableBaseRelativeRemainingEffect
	);

	int item = getBaseBuildingItem(mostVulnerableBaseId);

	if (item < 0 || !isInfantryDefensiveUnit(item))
	{
		debug("\t\tnot infantry devensive\n");
		return;
	}

	// hurry production

	int spendPool = getFaction(aiFactionId)->energy_credits;

	hurryProductionPartially(mostVulnerableBaseId, spendPool);

	debug("\t%-25s spendPool=%4d\n", getBase(mostVulnerableBaseId)->name, spendPool);

}

//=======================================================
// estimated income functions
//=======================================================

/**
Computes facility gain by comparing to base state with and without it.
1. immediate resouce income improvement
2. income growth due to increase in nutrient surplus
3. implicit income due to reduction in eco-damage
*/
double getFacilityGain(int baseId, int facilityId, bool build, bool includeMaintenance)
{
	BASE *base = getBase(baseId);
	
	if (build)
	{
		// cannot build
		
		if (!isBaseFacilityAvailable(baseId, facilityId))
		{
			return 0.0;
		}
		
	}
	else
	{
		// cannot remove
		
		if (!isBaseHasFacility(baseId, facilityId))
		{
			return 0.0;
		}
		
	}
	
	// compute old income
	
	double oldPopulationGrowth = getBasePopulationGrowth(baseId);
	double oldIncome = getBaseIncome(baseId);
	int oldEcoDamage = std::min(100, base->eco_damage);
	
	// compute new income
	
	setBaseFacility(baseId, facilityId, build);
//	computeBaseComplete(baseId);
	computeBase(baseId, true);
	
	double newPopulationGrowth = getBasePopulationGrowth(baseId);
	double newIncome = getBaseIncome(baseId);
	int newEcoDamage = std::min(100, base->eco_damage);
	
	// restore base
	
	setBaseFacility(baseId, facilityId, !build);
//	computeBaseComplete(baseId);
	computeBase(baseId, true);
	
	// estimate eco damage effect
	// assume eco damage is the chance of number of workers to lose half or their productivity
	
	double ecoDamageIncome = 0.0;
	
	if (newEcoDamage != oldEcoDamage)
	{
		// fungal pop increase
		
		int ecoDamage = newEcoDamage - oldEcoDamage;
		double fungalPops = (double)ecoDamage / 100.0;
		double fungalPopTiles = fungalPops * (double)getFaction(aiFactionId)->clean_minerals_modifier / 3.0;
		
		// fungal pop gain
		
		ecoDamageIncome = - 0.5 * fungalPopTiles * getBaseIncome(baseId) / (double)base->pop_size;
		
	}
	
	// gain
	
	double income = (newIncome - oldIncome) + ecoDamageIncome;
	double incomeGain = getGainIncome(income);
	
	double populationGrowthIncrease = newPopulationGrowth - oldPopulationGrowth;
	double incomeGrowth = aiData.averageCitizenResourceIncome * populationGrowthIncrease;
	double incomeGrowthGain = getGainIncomeGrowth(incomeGrowth);
	
	double gain = incomeGain + incomeGrowthGain;
	
	if (includeMaintenance)
	{
		double maintenanceIncome = getResourceScore(0.0, -Facility[facilityId].maint);
		double maintenanceIncomeGain = getGainIncome(maintenanceIncome);
		gain += maintenanceIncomeGain;
	}
	
	// return combined gain
	
	return gain;
	
}

/**
Computes statistical average faction development score at given point in time.
*/
double getDevelopmentScore(int turn)
{
	double mineralIntake2 = getFactionStatisticalMineralIntake2(turn);
	double budgetIntake2 = getFactionStatisticalBudgetIntake2(turn);
	return getResourceScore(mineralIntake2, budgetIntake2);
}

/**
Computes statistical average faction development score growth.
*/
double getDevelopmentScoreGrowth(int turn)
{
	double mineralIntake2Growth = getFactionStatisticalMineralIntake2Growth(turn);
	double budgetIntake2Growth = getFactionStatisticalBudgetIntake2Growth(turn);
	return getResourceScore(mineralIntake2Growth, budgetIntake2Growth);
}

/**
Computes development scale at current point in time.
Approximate time when faction power growth 2 times.
*/
double getDevelopmentScale()
{
	return std::min(conf.ai_development_scale_max, conf.ai_development_scale_min + (conf.ai_development_scale_max - conf.ai_development_scale_min) * (*CurrentTurn / conf.ai_development_scale_max_turn));
}

/*
Evaluates gain production priority based on cost.
*/
double getRawProductionPriority(double gain, double cost)
{
	assert(cost > 0.0);
	return gain / cost;
}

/*
Normalizes production priority by normal raw production priority.
*/
double getProductionPriority(double gain, double cost)
{
	return getRawProductionPriority(gain, cost) / landColonyGain;
}

/*
Computes base production constant income gain.
*/
double getProductionConstantIncomeGain(double score, double buildTime, double bonusDelay)
{
	return
		score
		* aiData.developmentScale
		* getBuildTimeEffectCoefficient(buildTime) * getBonusDelayEffectCoefficient(bonusDelay)
	;
}

/*
Computes base production linear income gain.
*/
double getProductionLinearIncomeGain(double score, double buildTime, double bonusDelay)
{
	return
		score
		* aiData.developmentScale * aiData.developmentScale
		* getBuildTimeEffectCoefficient(buildTime) * getBonusDelayEffectCoefficient(bonusDelay)
	;
}

/*
Estimates morale/lifecycle effect.
Returns proportional mineral bonus for average faction base.
*/
double getMoraleProportionalMineralBonus(std::array<int,4> levels)
{
	// summarize costs and supports

	int totalUnitWeightedCount = 0;
	int totalUnitWeightedCost = 0;

	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		int unitCost = getVehicleUnitCost(vehicleId);
		bool native = isNativeVehicle(vehicleId);

		// type

		int type = (native ? 3 : triad);

		// accumulate weighted values

		totalUnitWeightedCount += 1 * levels[type];
		totalUnitWeightedCost += unitCost * levels[type];

	}

	// reduction in build cost and support
	// assuming every 20-th combat unit is dying every turn

	double mineralReduction = 0.125 * (totalUnitWeightedCount + 0.05 * totalUnitWeightedCost);

	// proportional bonus on average base production

	return (aiData.totalMineralIntake2 <= 0 ? 0.0 : std::min(1.0, mineralReduction / (double)aiData.totalMineralIntake2));

}

BaseMetric getBaseMetric(int baseId)
{
	BASE *base = getBase(baseId);

	BaseMetric baseMetric;

	baseMetric.nutrient = base->nutrient_surplus;
	baseMetric.mineral = base->mineral_surplus;
	baseMetric.economy = base->economy_total;
	baseMetric.psych = base->economy_total;
	baseMetric.labs = base->labs_total;

	return baseMetric;

}

double getDemand(double required, double provided)
{
	double demand;

	if (required <= 0.0)
	{
		demand = 0.0;
	}
	else if (provided >= required)
	{
		demand = 0.0;
	}
	else
	{
		demand = 1.0 - provided / required;
	}

	return demand;

}

//=======================================================
// statistical estimates
//=======================================================

/**
Computes faction statistical mineral intake2.
*/
double getFactionStatisticalMineralIntake2(double turn)
{
	return
		+ conf.ai_faction_mineral_a0
		+ conf.ai_faction_mineral_a1 * turn
		+ conf.ai_faction_mineral_a2 * turn * turn
	;
}

/**
Computes faction statistical mineral intake2 growth.
*/
double getFactionStatisticalMineralIntake2Growth(double turn)
{
	return
		+ conf.ai_faction_mineral_a1
		+ conf.ai_faction_mineral_a2 * 2 * turn
	;
}

/**
Computes faction statistical budget intake2.
*/
double getFactionStatisticalBudgetIntake2(double turn)
{
	return
		+ conf.ai_faction_budget_a0
		+ conf.ai_faction_budget_a1 * turn
		+ conf.ai_faction_budget_a2 * turn * turn
		+ conf.ai_faction_budget_a3 * turn * turn * turn
	;
}

/**
Computes faction statistical budget intake2 growth.
*/
double getFactionStatisticalBudgetIntake2Growth(double turn)
{
	return
		+ conf.ai_faction_budget_a1
		+ conf.ai_faction_budget_a2 * 2 * turn
		+ conf.ai_faction_budget_a3 * 3 * turn * turn
	;
}

/*
Returns base statistical size with age.
*/
double getBaseStatisticalSize(double age)
{
	double value =
		+ conf.ai_base_size_a * log(age - conf.ai_base_size_b)
		+ conf.ai_base_size_c
	;

	return std::max(1.0, value);

}

/*
Returns base statistical mineral intake.
*/
double getBaseStatisticalMineralIntake(double age)
{
	double value =
		+ conf.ai_base_mineral_intake_a * log(age - conf.ai_base_mineral_intake_b)
		+ conf.ai_base_mineral_intake_c
	;

	return std::max(1.0, value);

}

/*
Returns base statistical mineral intake2.
*/
double getBaseStatisticalMineralIntake2(double age)
{
	double value =
		+ conf.ai_base_mineral_intake2_a * age
		+ conf.ai_base_mineral_intake2_b
	;

	return std::max(1.0, value);

}

/*
Returns base statistical mineral multiplier.
*/
double getBaseStatisticalMineralMultiplier(double age)
{
	double value =
		+ conf.ai_base_mineral_multiplier_a * age * age
		+ conf.ai_base_mineral_multiplier_b * age
		+ conf.ai_base_mineral_multiplier_c
	;

	return std::max(1.0, value);

}

/*
Reages base statistical budget intake.
*/
double getBaseStatisticalBudgetIntake(double age)
{
	double value =
		+ conf.ai_base_budget_intake_a * age
		+ conf.ai_base_budget_intake_b
	;

	return std::max(1.0, value);

}

/*
Reages base statistical budget intake2.
*/
double getBaseStatisticalBudgetIntake2(double age)
{
	double value =
		+ conf.ai_base_budget_intake2_a * age * age
		+ conf.ai_base_budget_intake2_b * age
		+ conf.ai_base_budget_intake2_c
	;

	return std::max(1.0, value);

}

/*
Reages base statistical budget multiplier.
*/
double getBaseStatisticalBudgetMultiplier(double age)
{
	double value =
		+ conf.ai_base_budget_multiplier_a * age * age
		+ conf.ai_base_budget_multiplier_b * age
		+ conf.ai_base_budget_multiplier_c
	;

	return std::max(1.0, value);

}

/*
Estimates flat mineral intake gain.
*/
double getFlatMineralIntakeGain(int delay, double value)
{
	assert(delay >= 1);

	int initialTurn = *CurrentTurn + delay;
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;

		double futureMineralWeight = value * futureMineralDecayCoefficient;

		gain += getResourceScore(futureMineralWeight, 0);

	}

	return gain;

}

/*
Estimates flat budget intake gain.
*/
double getFlatBudgetIntakeGain(int delay, double value)
{
	assert(delay >= 1);

	int initialTurn = *CurrentTurn + delay;
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBudgetWeight = value * futureBudgetDecayCoefficient;

		gain += getResourceScore(futureBudgetWeight, 0);

	}

	return gain;

}

/*
Estimates base extra population gain.
Contributing population: (minSize, maxSize].
*/
double getBasePopulationGain(int baseId, const Interval &baseSizeInterval)
{
	bool TRACE = DEBUG & false;

	if (TRACE) { debug("getBasePopulationGain\n"); }

	BASE *base = getBase(baseId);

	int initialTurn = *CurrentTurn + 1;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);

	// base proportional coefficients

	int baseSize = base->pop_size;
	double baseStatisticalSize = getBaseStatisticalSize(baseAge);
	double baseSizeProportionalCoefficient = (double)baseSize / baseStatisticalSize;

	int baseMineralIntake2 = base->mineral_intake_2;
	double baseStatisticalMineralIntake2 = getBaseStatisticalMineralIntake2(baseAge);
	double baseMineralProportionalCoefficient = (double)baseMineralIntake2 / baseStatisticalMineralIntake2;

	int baseBudgetIntake2 = base->economy_total + base->psych_total + base->labs_total;
	double baseStatisticalBudgetIntake2 = getBaseStatisticalBudgetIntake2(baseAge);
	double baseBudgetProportionalCoefficient = (double)baseBudgetIntake2 / baseStatisticalBudgetIntake2;

	if (TRACE)
	{
		debug
		(
			"\tbaseId=%3d"
			" baseSizeInterval.min=%2d"
			" baseSizeInterval.max=%2d"
			" *CurrentTurn=%3d"
			" initialTurn=%3d"
			" baseAge=%3d"
			" currentFactionStatisticalMineralIntake2=%7.2f"
			" currentFactionStatisticalBudgetIntake2=%7.2f"
			" baseSize=%2d"
			" baseStatisticalSize=%5.2f"
			" baseSizeProportionalCoefficient=%5.2f"
			" baseMineralIntake2=%2d"
			" baseStatisticalMineralIntake2=%5.2f"
			" baseMineralProportionalCoefficient=%5.2f"
			" baseBudgetIntake2=%2d"
			" baseStatisticalBudgetIntake2=%5.2f"
			" baseBudgetProportionalCoefficient=%5.2f"
			"\n"
			, baseId
			, baseSizeInterval.min
			, baseSizeInterval.max
			, *CurrentTurn
			, initialTurn
			, baseAge
			, currentFactionStatisticalMineralIntake2
			, currentFactionStatisticalBudgetIntake2
			, baseSize
			, baseStatisticalSize
			, baseSizeProportionalCoefficient
			, baseMineralIntake2
			, baseStatisticalMineralIntake2
			, baseMineralProportionalCoefficient
			, baseBudgetIntake2
			, baseStatisticalBudgetIntake2
			, baseBudgetProportionalCoefficient
		);
	}

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);

		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBaseSize = getBaseStatisticalSize(futureBaseAge) * baseSizeProportionalCoefficient;

		if (futureBaseSize <= baseSizeInterval.min)
			continue;

		double populationProportion = (std::min(futureBaseSize, (double)baseSizeInterval.max) - (double)baseSizeInterval.min) / futureBaseSize;

		double futureBaseMineralIntake2 = getBaseStatisticalMineralIntake2(futureBaseAge) * baseMineralProportionalCoefficient;
		double futureBaseBudgetIntake2 = getBaseStatisticalBudgetIntake2(futureBaseAge) * baseBudgetProportionalCoefficient;

		double futureBaseMineralIntake2ExtraPopulation = futureBaseMineralIntake2 * populationProportion;
		double futureBaseBudgetIntake2ExtraPopulation = futureBaseBudgetIntake2 * populationProportion;

		double futureBaseMineralWeight = futureBaseMineralIntake2ExtraPopulation * futureMineralDecayCoefficient;
		double futureBaseBudgetWeight = futureBaseBudgetIntake2ExtraPopulation * futureBudgetDecayCoefficient;

		double turnGain = getResourceScore(futureBaseMineralWeight, futureBaseBudgetWeight);
		gain += turnGain;

		if (TRACE)
		{
			debug
			(
				"\t\tfutureTurn=%3d"
				" futureBaseAge=%3d"
				" futureFactionStatisticalMineralIntake2=%7.2f"
				" futureFactionStatisticalBudgetIntake2=%7.2f"
				" futureMineralDecayCoefficient=%5.2f"
				" futureBudgetDecayCoefficient=%5.2f"
				" futureBaseSize=%5.2f"
				" populationProportion=%5.2f"
				" futureBaseMineralIntake2=%7.2f"
				" futureBaseBudgetIntake2=%7.2f"
				" futureBaseMineralIntake2ExtraPopulation=%7.2f"
				" futureBaseBudgetIntake2ExtraPopulation=%7.2f"
				" futureBaseMineralWeight=%7.2f"
				" futureBaseBudgetWeight=%7.2f"
				" turnGain=%7.2f"
				" gain=%7.2f"
				"\n"
				, futureTurn
				, futureBaseAge
				, futureFactionStatisticalMineralIntake2
				, futureFactionStatisticalBudgetIntake2
				, futureMineralDecayCoefficient
				, futureBudgetDecayCoefficient
				, futureBaseSize
				, populationProportion
				, futureBaseMineralIntake2
				, futureBaseBudgetIntake2
				, futureBaseMineralIntake2ExtraPopulation
				, futureBaseBudgetIntake2ExtraPopulation
				, futureBaseMineralWeight
				, futureBaseBudgetWeight
				, turnGain
				, gain
			);
		}

	}

	if (TRACE)
	{
		debug
		(
			"\tgain=%7.2f"
			"\n"
			, gain
		);
	}

	return gain;

}

/*
Estimates base extra population gain.
Contributing population: (minSize, maxSize].
*/
double getBasePopulationGrowthGain(int baseId, int delay, double proportionalGrowthIncrease)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	BASE *base = getBase(baseId);
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);
	int initialTurn = *CurrentTurn + delay;

	// base proportional coefficients

	int baseMineralIntake2 = base->mineral_intake_2;
	double baseStatisticalMineralIntake2 = getBaseStatisticalMineralIntake2(baseId);
	double baseMineralProportionalCoefficient = (double)baseMineralIntake2 / baseStatisticalMineralIntake2;

	int baseBudgetIntake2 = base->economy_total + base->psych_total + base->labs_total;
	double baseStatisticalBudgetIntake2 = getBaseStatisticalBudgetIntake2(baseId);
	double baseBudgetProportionalCoefficient = (double)baseBudgetIntake2 / baseStatisticalBudgetIntake2;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		double futureBaseAge1 = (double)(baseAge + delay) + (double)(futureTurn - initialTurn);
		double futureBaseAge2 = (double)(baseAge + delay) + (double)(futureTurn - initialTurn) * proportionalGrowthIncrease;

		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);

		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBaseMineralIntake2Difference =
			+ getBaseStatisticalMineralIntake2(futureBaseAge2) * baseMineralProportionalCoefficient
			- getBaseStatisticalMineralIntake2(futureBaseAge1) * baseMineralProportionalCoefficient
		;
		double futureBaseBudgetIntake2Difference =
			+ getBaseStatisticalBudgetIntake2(futureBaseAge2) * baseBudgetProportionalCoefficient
			- getBaseStatisticalBudgetIntake2(futureBaseAge1) * baseBudgetProportionalCoefficient
		;

		double futureBaseMineralIntake2ExtraPopulation = futureBaseMineralIntake2Difference;
		double futureBaseBudgetIntake2ExtraPopulation = futureBaseBudgetIntake2Difference;

		double futureBaseMineralWeight = futureBaseMineralIntake2ExtraPopulation * futureMineralDecayCoefficient;
		double futureBaseBudgetWeight = futureBaseBudgetIntake2ExtraPopulation * futureBudgetDecayCoefficient;

		gain += getResourceScore(futureBaseMineralWeight, futureBaseBudgetWeight);

	}

	return gain;

}

/*
Estimates proportional mineral intake gain.
*/
double getBaseProportionalMineralIntakeGain(int baseId, int delay, double proportion)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	BASE *base = getBase(baseId);

	int initialTurn = *CurrentTurn + delay;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);

	// base proportional coefficients

	int baseMineralIntake = base->mineral_intake;
	double baseStatisticalMineralIntake = getBaseStatisticalMineralIntake(baseId);
	double baseMineralProportionalCoefficient = (double)baseMineralIntake / baseStatisticalMineralIntake;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;

		double futureBaseMineralIntake = getBaseStatisticalMineralIntake(futureBaseAge) * baseMineralProportionalCoefficient;
		double futureBaseMineralIntakeProportion = futureBaseMineralIntake * proportion;
		double futureBaseMineralWeight = futureBaseMineralIntakeProportion * futureMineralDecayCoefficient;

		gain += getResourceScore(futureBaseMineralWeight, 0);

	}

	return gain;

}

/*
Estimates proportional budget intake gain.
*/
double getBaseProportionalBudgetIntakeGain(int baseId, int delay, double proportion)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	int initialTurn = *CurrentTurn + delay;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);

	// base proportional coefficients

	Budget baseBudgetIntakeByType = getBaseBudgetIntake(baseId);
	int baseBudgetIntake = baseBudgetIntakeByType.economy + baseBudgetIntakeByType.psych + baseBudgetIntakeByType.labs;
	double baseStatisticalBudgetIntake = getBaseStatisticalBudgetIntake(baseId);
	double baseBudgetProportionalCoefficient = (double)baseBudgetIntake / baseStatisticalBudgetIntake;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBaseBudgetIntake = getBaseStatisticalBudgetIntake(futureBaseAge) * baseBudgetProportionalCoefficient;
		double futureBaseBudgetIntakeProportion = futureBaseBudgetIntake * proportion;
		double futureBaseBudgetWeight = futureBaseBudgetIntakeProportion * futureBudgetDecayCoefficient;

		gain += getResourceScore(0, futureBaseBudgetWeight);

	}

	return gain;

}

/*
Estimates proportional mineral intake gain.
*/
double getBaseProportionalMineralIntake2Gain(int baseId, int delay, double proportion)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	BASE *base = getBase(baseId);

	int initialTurn = *CurrentTurn + delay;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);

	// base proportional coefficients

	int baseMineralIntake2 = base->mineral_intake_2;
	double baseStatisticalMineralIntake2 = getBaseStatisticalMineralIntake2(baseId);
	double baseMineralProportionalCoefficient = (double)baseMineralIntake2 / baseStatisticalMineralIntake2;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;

		double futureBaseMineralIntake2 = getBaseStatisticalMineralIntake2(futureBaseAge) * baseMineralProportionalCoefficient;
		double futureBaseMineralIntake2Proportion = futureBaseMineralIntake2 * proportion;
		double futureBaseMineralWeight = futureBaseMineralIntake2Proportion * futureMineralDecayCoefficient;

		gain += getResourceScore(futureBaseMineralWeight, 0);

	}

	return gain;

}

/*
Estimates proportional budget intake gain.
*/
double getBaseProportionalBudgetIntake2Gain(int baseId, int delay, double proportion)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	BASE *base = getBase(baseId);

	int initialTurn = *CurrentTurn + delay;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);

	// base proportional coefficients

	int baseBudgetIntake2 = base->economy_total + base->psych_total + base->labs_total;
	double baseStatisticalBudgetIntake2 = getBaseStatisticalBudgetIntake2(baseId);
	double baseBudgetProportionalCoefficient = (double)baseBudgetIntake2 / baseStatisticalBudgetIntake2;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBaseBudgetIntake2 = getBaseStatisticalBudgetIntake2(futureBaseAge) * baseBudgetProportionalCoefficient;
		double futureBaseBudgetIntake2Proportion = futureBaseBudgetIntake2 * proportion;
		double futureBaseBudgetWeight = futureBaseBudgetIntake2Proportion * futureBudgetDecayCoefficient;

		gain += getResourceScore(0, futureBaseBudgetWeight);

	}

	return gain;

}

//=======================================================
// production item helper functions
//=======================================================

/*
Calculates number of extra police units can be added to the base.
*/
int getBasePoliceExtraCapacity(int baseId)
{
	BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
	
	// get base police allowed
	
	int policeAllowed = getBasePoliceAllowed(baseId);
	
	// no police allowed
	
	if (policeAllowed == 0)
		return 0;
		
	// count number of police units in base
	
	int policePresent = 0;
	
	for (int vehicleId : baseInfo.garrison)
	{
		// count only police2x if available
		
		if (aiData.police2xUnitAvailable && !isInfantryDefensivePolice2xVehicle(vehicleId))
			continue;
			
		// count police
		
		policePresent++;
		
	}
	
	return std::max(0, policeAllowed - policePresent);
	
}

double getItemPriority(int item, double gain)
{
	double cost = item >= 0 ? Units[item].cost : Facility[-item].cost;
	return gain / cost;
}

double getBaseColonyUnitGain(int baseId, int unitId, double travelTime, double buildSiteScore)
{
	// depopulation
	
	double depopulationIncome = -getBaseCitizenIncome(baseId);
	double depopulationGain = getGainIncome(depopulationIncome);
	
	// support
	
	double supportIncome = getResourceScore(-getUnitSupport(unitId), 0);
	double supportGain = getGainTimeInterval(supportIncome, 0, travelTime);
	
	// new base
	
	double delayedMeanNewBaseGain = getGainDelay(meanNewBaseGain, travelTime);
	double newBaseGain = delayedMeanNewBaseGain * buildSiteScore;
	
	// summarize
	
	return depopulationGain + supportGain + newBaseGain;
	
}

/**
Computes estimated police provided gain.
*/
double getBasePoliceGain(int baseId, bool police2x)
{
	// police allowed
	
	int policeAllowed = getBasePoliceAllowed(baseId);
	
	if (policeAllowed <= 0)
		return 0.0;
	
	// police power
	
	int policePower = getBasePolicePower(baseId, police2x);
	
	// basic base gain
	
	Resource oldBaseIntake2 = getBaseResourceIntake2(baseId);
	double oldBaseGain = getBaseGain(baseId, oldBaseIntake2);
	
	// extra worker base gain
	
	Resource newBaseIntake2 =
		Resource::combine
		(
			oldBaseIntake2,
			{
				aiData.averageWorkerNutrientIntake2,
				aiData.averageWorkerMineralIntake2,
				aiData.averageWorkerEnergyIntake2,
 			}
		)
	;
	double newBaseGain = getBaseGain(baseId, newBaseIntake2);
	
	// extra worker gain
	
	double extraWorkerGain = std::max(0.0, newBaseGain - oldBaseGain);
	
	// police gain
	
	double policeGain = (double)policePower * extraWorkerGain;
	return policeGain;
	
}

