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
int bestFormerChassisIds[3];
int bestFormerSpeeds[3];
robin_hood::unordered_flat_map<int, BaseProductionInfo> baseProductionInfos;
robin_hood::unordered_flat_map<int, double> weakestEnemyBaseProtection;

double const LAND_ARTILLERY_SATURATION_RATIO = 0.20;
double const INFANTRY_DEFENSIVE_SATURATION_RATIO = 0.75;
double globalLandArtillerySaturationCoefficient;
double globalInfantryDefensiveSaturationCoefficient;

double techStealGain;

// sea transport parameters by sea cluster

robin_hood::unordered_flat_map<int, double> seaTransportDemands;
robin_hood::unordered_flat_map<int, int> bestSeaTransportProductionBaseId;

// military power in count of existing and potential military units cost
robin_hood::unordered_flat_map<int, double> militaryPowers;

int selectedProject = -1;

// currently processing base production demand

std::vector<ProductionDemand> productionDemands;
ProductionDemand *currentBaseProductionDemand;

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
	Profiling::start("productionStrategy", "strategy");
	
	// set global statistics
	
	Profiling::start("set global statistics", "productionStrategy");
	meanNewBaseGain = getNewBaseGain();
	landColonyGain = getLandColonyGain();
	Profiling::stop("set global statistics");
	
	// base production infos
	
	Profiling::start("base production infos", "productionStrategy");
	for (int baseId : aiData.baseIds)
	{
		baseProductionInfos.emplace(baseId, BaseProductionInfo());
		baseProductionInfos.at(baseId).baseGain = getBaseGain(baseId);
		baseProductionInfos.at(baseId).extraPoliceGains.at(0) = getBasePoliceGain(baseId, false);
		baseProductionInfos.at(baseId).extraPoliceGains.at(1) = getBasePoliceGain(baseId, true);
	}
	Profiling::stop("base production infos");
	
	// evaluate global demands
	// populate global variables
	
	populateFactionProductionData();
	evaluateGlobalColonyDemand();
	evaluateGlobalSeaTransportDemand();
	
	// set production
	
	initializeProductionDemands();
	suggestGlobalProduction();
	suggestBaseProductions();
	applyBaseProductions();
	
	// hurry protective unit production
	
	hurryProtectiveUnit();
	
	Profiling::stop("productionStrategy");
	
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
	Profiling::start("populateFactionProductionData", "productionStrategy");
	
	// global parameters
	
	techStealGain = getTechStealGain();
	
	// fomer speed
	
	std::fill(std::begin(bestFormerSpeeds), std::begin(bestFormerSpeeds) + 3, 0);
	std::fill(std::begin(bestFormerChassisIds), std::begin(bestFormerChassisIds) + 3, -1);
	for (int unitId : aiData.formerUnitIds)
	{
		int chassisId = Units[unitId].chassis_id;
		CChassis chassis = Chassis[Units[unitId].chassis_id];
		if (chassis.speed > bestFormerSpeeds[chassis.triad])
		{
			bestFormerChassisIds[chassis.triad] = chassisId;
			bestFormerSpeeds[chassis.triad] = chassis.speed;
		}
	}
	
	// landArtillery and landDefenders saturation
	
	int landOffensiveVehicleCount = 0;
	int landArtilleryVehicleCount = 0;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// land
		
		if (vehicle->triad() != TRIAD_LAND)
			continue;
		
		// offensive
		
		if (!isOffensiveVehicle(vehicleId))
			continue;
		
		// land offensive count
		
		landOffensiveVehicleCount++;
		
		// artillery
		
		if (!isArtilleryVehicle(vehicleId))
			continue;
		
		// land artillery count
		
		landArtilleryVehicleCount++;
		
	}
	
	double landArtilleryRatio = landOffensiveVehicleCount == 0 ? 0.0 : (double)landArtilleryVehicleCount / (double)landOffensiveVehicleCount;
	globalLandArtillerySaturationCoefficient = landArtilleryRatio <= LAND_ARTILLERY_SATURATION_RATIO ? 1.0 : (1.0 - landArtilleryRatio) / (1.0 - LAND_ARTILLERY_SATURATION_RATIO);
	
	int vehicleCount = 0;
	int infantryDefensiveVehicleCount = 0;
	
	for (int vehicleId : aiData.combatVehicleIds)
	{
		// vehicle count
		
		vehicleCount++;
		
		// infantry defensive
		
		if (!isInfantryDefensiveVehicle(vehicleId))
			continue;
		
		// infantry defensive count
		
		infantryDefensiveVehicleCount++;
		
	}
	
	double infantryDefensiveRatio = vehicleCount == 0 ? 0.0 : (double)infantryDefensiveVehicleCount / (double)vehicleCount;
	globalInfantryDefensiveSaturationCoefficient = infantryDefensiveRatio <= INFANTRY_DEFENSIVE_SATURATION_RATIO ? 1.0 : (1.0 - infantryDefensiveRatio) / (1.0 - INFANTRY_DEFENSIVE_SATURATION_RATIO);
	debug("globalInfantryDefensiveSaturationCoefficient=%5.2f\n", globalInfantryDefensiveSaturationCoefficient);
	
	Profiling::stop("populateFactionProductionData");
	
}

/**
Evaluates need for new colonies based on current inefficiency.
Every new colony creates more drones and, thus, is less effective in larger empire.
*/
void evaluateGlobalColonyDemand()
{
	Profiling::start("evaluateGlobalColonyDemand", "productionStrategy");
	
	debug("evaluateGlobalColonyDemand - %s\n", MFactions[aiFactionId].noun_faction);
	
	Faction *faction = getFaction(aiFactionId);
	
	// set default global colony demand multiplier
	
	globalColonyDemand = 1.0;
	
	// do not compute if there are no bases to produce anything
	
	if (aiData.baseIds.size() == 0)
	{
		Profiling::stop("evaluateGlobalColonyDemand");
		return;
	}
	
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
	
	Profiling::stop("evaluateGlobalColonyDemand");
	
}

void evaluateGlobalSeaTransportDemand()
{
	Profiling::start("evaluateGlobalSeaTransportDemand", "productionStrategy");
	
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
			MAP *baseTile = getBaseMapTile(baseId);
			int baseSeaCluster = getBaseSeaCluster(baseTile);
			
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
	
//	if (DEBUG)
//	{
//		for (robin_hood::pair<int, double> seaTransportDemandEntry : seaTransportDemands)
//		{
//			int cluster = seaTransportDemandEntry.first;
//			double demand = seaTransportDemandEntry.second;
//			
//			debug("%2d %5.2f\n", cluster, demand);
//			
//		}
//		
//	}
	
	Profiling::stop("evaluateGlobalSeaTransportDemand");
	
}

void initializeProductionDemands()
{
	Profiling::start("initializeProductionDemands", "productionStrategy");
	
	productionDemands.clear();
	
	for (int baseId : aiData.baseIds)
	{
		productionDemands.emplace_back();
		productionDemands.back().initialize(baseId);
	}
	
	Profiling::stop("initializeProductionDemands");
	
}

void suggestGlobalProduction()
{
	Profiling::start("suggestGlobalProduction", "productionStrategy");
	
    debug("\nsuggestGlobalProduction - %s\n\n", MFactions[*CurrentFaction].noun_faction);
    
    evaluateHeadquarters();
    evaluateProject();
	
    debug("\n");
	
	Profiling::stop("suggestGlobalProduction");
	
}

void suggestBaseProductions()
{
	Profiling::start("suggestBaseProductions", "productionStrategy");
	
    debug("\nsuggestBaseProductions - %s\n\n", MFactions[*CurrentFaction].noun_faction);
	
    for (ProductionDemand &productionDemand : productionDemands)
	{
		currentBaseProductionDemand = &productionDemand;
		suggestBaseProduction();
	}
	
    debug("\n");
	
	Profiling::stop("suggestBaseProductions");
	
}

void applyBaseProductions()
{
	Profiling::start("applyBaseProductions", "productionStrategy");
	
    debug("\napplyBaseProductions - %s\n\n", MFactions[*CurrentFaction].noun_faction);
	
    for (ProductionDemand &productionDemand : productionDemands)
	{
		int baseId = productionDemand.baseId;
		BASE* base = productionDemand.base;
		
		debug("applyBaseProduction - %s\n", base->name);
		
		// current production choice
		
		int currentChoice = base->queue_items[0];
		int choice = currentChoice;
		
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
		// facility
		else if (choice < 0 && -choice < FAC_STOCKPILE_ENERGY)
		{
			// only not managed facilities
			
			if (MANAGED_FACILITIES.count(-choice) == 0)
			{
				vanillaPriority = conf.ai_production_vanilla_priority_facility;
			}
			
		}
//		// project
//		else if (choice < 0 && -choice >= SP_ID_First && -choice <= SP_ID_Last)
//		{
//			// leave vanilla project be
//			
//			vanillaPriority = conf.ai_production_vanilla_priority_project;
//			
//		}
		
		// drop priority for impossible colony
		
		if (choice >= 0 && isColonyUnit(choice) && !canBaseProduceColony(baseId))
		{
			vanillaPriority = 0.0;
		}
		
		// WTP
		
		int wtpChoice = productionDemand.item;
		double wtpPriority = productionDemand.priority;
		
//		debug
//		(
//			"production selection\n"
//			"\t%-10s(%5.2f) %s\n"
//			"\t%-10s(%5.2f) %s\n"
//			, "vanilla" , vanillaPriority, prod_name(choice)
//			, "wtp", wtpPriority, prod_name(wtpChoice)
//		);
		
		// select production based on priority
		
		if (wtpPriority >= vanillaPriority)
		{
			choice = wtpChoice;
		}
		
		debug("=> %-25s vanillaPriority=%5.2f wtpPriority=%5.2f\n", prod_name(choice), vanillaPriority, wtpPriority);
		
		// set base production if changed
		
		if (choice != currentChoice)
		{
			base_prod_change(baseId, choice);
		}
		
		debug("\n");
		
	}
	
    debug("\n");
	
	Profiling::stop("applyBaseProductions");
	
}

void evaluateHeadquarters()
{
	debug("- evaluateHeadquarters\n");
	
	int facilityId = FAC_HEADQUARTERS;
	
	// currentBudget
	
	int currentBudget = 0;
	
	for (ProductionDemand &otherProductionDemand : productionDemands)
	{
		int otherBaseId = otherProductionDemand.baseId;
		BASE *otherBase = otherProductionDemand.base;
		
		Profiling::start("- evaluateHeadquarters - computeBase");
		computeBase(otherBaseId, false);
		Profiling::stop("- evaluateHeadquarters - computeBase");
		
		int budget = otherBase->economy_total + otherBase->psych_total + otherBase->labs_total;
		currentBudget += budget;
		
	}
	
	// current HQ base
	
	int hqBaseId = -1;
	
	for (int baseId : aiData.baseIds)
	{
		if (isBaseHasFacility(baseId, facilityId))
		{
			hqBaseId = baseId;
			setBaseFacility(baseId, facilityId, false);
		}
		
	}
	
	// best HQ location
	
	ProductionDemand *bestHQProductionDemand = nullptr;
	int bestHQProductionDemandBudget = currentBudget;
	
	for (ProductionDemand &productionDemand : productionDemands)
	{
		int baseId = productionDemand.baseId;
		
		int totalBudget = 0;
		
		setBaseFacility(baseId, facilityId, true);
		
		for (ProductionDemand &otherProductionDemand : productionDemands)
		{
			int otherBaseId = otherProductionDemand.baseId;
			BASE *otherBase = otherProductionDemand.base;
			
			Profiling::start("- evaluateHeadquarters - computeBase");
			computeBase(otherBaseId, false);
			Profiling::stop("- evaluateHeadquarters - computeBase");
			
			int budget = otherBase->economy_total + otherBase->psych_total + otherBase->labs_total;
			totalBudget += budget;
			
		}
		
		setBaseFacility(baseId, facilityId, false);
		
		if (totalBudget > bestHQProductionDemandBudget)
		{
			bestHQProductionDemand = &productionDemand;
			bestHQProductionDemandBudget = totalBudget;
		}
		
	}
	
	// restore HQ
	
	if (hqBaseId != -1)
	{
		setBaseFacility(hqBaseId, facilityId, true);
	}
	
	// no relocation base
	
	if (bestHQProductionDemand == nullptr)
		return;
	
	// add demand
	
	int budgetImprovement = bestHQProductionDemandBudget - currentBudget;
	double income = getResourceScore(0.0, (double)budgetImprovement);
	double gain = getGainIncome(income);
	double priority = getItemPriority(-facilityId, gain);
	bestHQProductionDemand->addItemPriority(-facilityId, priority);
	
	debug
	(
		"\t%-25s"
		" priority=%5.2f   |"
		" income=%5.2f"
		" gain=%5.2f"
		"\n"
		, bestHQProductionDemand->base->name
		, priority
		, income
		, gain
	)
	;
	
}

void evaluateProject()
{
	debug("evaluateProject - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// find project
	
	int cheapestProjectFacilityId = -1;
	int cheapestProjectFacilityCost = INT_MAX;
	
	for (int projectFacilityId = SP_ID_First; projectFacilityId <= SP_ID_Last; projectFacilityId++)
	{
		// exclude not available project
		
		if (!mod_facility_avail((FacilityId)projectFacilityId, aiFactionId, -1, 0))
			continue;
		
		// project cost
		
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
	
	if (cheapestProjectFacilityId == -1)
		return;
	
	int selectedProjectFacilityId = cheapestProjectFacilityId;
	CFacility *selectedProjectFacility = getFacility(selectedProjectFacilityId);
	
	// max threat
	
	double maxThreat = 2.0;
	for (ProductionDemand &productionDemand : productionDemands)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(productionDemand.baseId);
		
		double threat = baseInfo.combatData.assailantWeight;
		maxThreat = std::max(maxThreat, threat);
		
	}

	// check project value
	
	ProductionDemand *bestProductionDemand = nullptr;
	double bestBaseValue = 0.0;
	int bestBaseBuildTime = 0;
	
	for (ProductionDemand &productionDemand : productionDemands)
	{
		int baseId = productionDemand.baseId;
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		
		// turns to completion
		
		int buildTime = getBaseItemBuildTime(baseId, -selectedProjectFacilityId, false);
		double buildTimeCoefficient = 1.0 / (double)std::max(1, buildTime);
		
		// threat coefficient
		
		double threat = baseInfo.combatData.assailantWeight;
		double threatCoefficient = 1.0 - threat / maxThreat;
		
		// value
		
		double value = buildTimeCoefficient * threatCoefficient;
		
		if (value > bestBaseValue)
		{
			bestProductionDemand = &productionDemand;
			bestBaseValue = value;
			bestBaseBuildTime = buildTime;
		}
		
	}
	
	if (bestProductionDemand == nullptr)
	{
		debug("\tmost suitable base not found\n");
		return;
	}
	
	// add project priority
	
	double buildTimeCoefficient = conf.ai_production_current_project_priority_build_time / (double)std::max(1, bestBaseBuildTime);
	double priority = buildTimeCoefficient;
	
	bestProductionDemand->addItemPriority(-selectedProjectFacilityId, priority);
	
	debug
	(
		"\t%-30s %-25s ai_production_current_project_priority_build_time=%5.2f bestBaseBuildTime=%2d priority=%5.2f\n"
		, selectedProjectFacility->name
		, bestProductionDemand->base->name
		, conf.ai_production_current_project_priority_build_time
		, bestBaseBuildTime
		, priority
	);
	
}

void suggestBaseProduction()
{
	Profiling::start("suggestBaseProduction", "suggestBaseProductions");
	
	debug("suggestBaseProduction - %s\n", currentBaseProductionDemand->base->name);
	
	// evaluate economical items
	
	evaluateFacilities();
	evaluateExpansionUnits();
	evaluateTerraformUnits();
	evaluateConvoyUnits();
	
	// evaluate combat units
	
	evaluateDefensiveProbeUnits();
	evaluatePodPoppingUnits();
	evaluateBaseDefenseUnits();
	evaluateBunkerDefenseUnits();
	evaluateTerritoryProtectionUnits();
	evaluateEnemyBaseAssaultUnits();
	
	// evaluate transport
	// should be after unit evaluations to substitute transport as needed
	
	evaluateSeaTransport();
	
	debug("\n");
	
	Profiling::stop("suggestBaseProduction");
	
}

void evaluateFacilities()
{
	Profiling::start("evaluateFacilities", "suggestBaseProduction");
	
	debug("evaluateFacilities\n");
	
	evaluatePressureDome();
	evaluateStockpileEnergy();
	evaluatePsychFacilitiesRemoval();
	evaluatePsychFacilities();
	evaluateRecyclingTanks();
	evaluateIncomeFacilities();
	evaluateMineralMultiplyingFacilities();
	evaluatePopulationLimitFacilities();
	evaluateMilitaryFacilities();
	evaluatePrototypingFacilities();

	Profiling::stop("evaluateFacilities");
	
}

void evaluatePressureDome()
{
	debug("- evaluatePressureDome\n");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	BASE *base = productionDemand.base;
	
	int facilityId = FAC_PRESSURE_DOME;
	
	if (*ClimateFutureChange <= 0 || !is_shore_level(mapsq(base->x, base->y)))
		return;
	
	// fixed extremely high priority
	double priority = 1000.0;
	
	productionDemand.addItemPriority(-facilityId, priority);
	
	debug
	(
		"\t%-32s"
		" priority=%5.2f   |"
		"\n"
		, getFacility(facilityId)->name
		, priority
	)
	;
	
}

void evaluateStockpileEnergy()
{
	debug("- evaluateStockpileEnergy\n");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
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

/**
Evaluates base income facilities removal.
*/
void evaluatePsychFacilitiesRemoval()
{
	debug("- evaluatePsychFacilitiesRemoval\n");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	
	// facilityIds
	
	int facilityIds[] = {FAC_PUNISHMENT_SPHERE, FAC_RECREATION_COMMONS, FAC_HOLOGRAM_THEATRE, FAC_PARADISE_GARDEN, };
	
	for (int facilityId : facilityIds)
	{
		// evaluate regular psych facility removal only when PS is present
		
		if (!has_facility(FAC_PUNISHMENT_SPHERE, baseId) && facilityId != FAC_PUNISHMENT_SPHERE)
			continue;
		
		// facility should be present
		
		if (!isBaseHasFacility(baseId, facilityId))
			continue;
		
		// gain
		
		double gain = getFacilityGain(baseId, facilityId, false);
		
		debug
		(
			"\t%-32s"
			" gain=%5.2f"
			"\n"
			, getFacility(facilityId)->name
			, gain
		);
		
		// remove if positive gain
		
		if (gain >= 0)
		{
			setBaseFacility(baseId, facilityId, false);
			debug("\t\tremoved\n");
		}
		
	}
	
}

void evaluatePsychFacilities()
{
	debug("- evaluatePsychFacilities\n");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	
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
		
		// gain
		
		double gain = getFacilityGain(baseId, facilityId, true);
		double priority = conf.ai_production_priority_facility_psych * getItemPriority(-facilityId, gain);
		
		// add demand
		
		productionDemand.addItemPriority(-facilityId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f"
			" gain=%5.2f"
			" ai_production_priority_facility_psych=%5.2f"
			"\n"
			, getFacility(facilityId)->name
			, priority
			, gain
			, conf.ai_production_priority_facility_psych
		);
		
	}
	
}

void evaluateRecyclingTanks()
{
	debug("- evaluateRecyclingTanks\n");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	
	// facilityIds
	
	const std::vector<int> facilityIds
	{
		FAC_RECYCLING_TANKS,
	};
	
	for (int facilityId : facilityIds)
	{
		if (!isBaseCanBuildFacility(baseId, facilityId))
			continue;
		
		// gain
		
		double gain = getFacilityGain(baseId, facilityId, true);
		double priority = getItemPriority(-facilityId, gain);
		
		// add demand
		
		productionDemand.addItemPriority(-facilityId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f"
			" gain=%5.2f"
			"\n"
			, getFacility(facilityId)->name
			, priority
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
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
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
		
		double facilityGain = getFacilityGain(baseId, facilityId, true);
		
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
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	
	// facilityIds
	
	const std::vector<int> facilityIds
	{
		FAC_GENEJACK_FACTORY, FAC_ROBOTIC_ASSEMBLY_PLANT, FAC_NANOREPLICATOR, FAC_QUANTUM_CONVERTER,
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
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
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
		
		int growthToLimitTime = getBaseTurnsToPopulation(baseId, populationLimit + 1);
		int facilityBuildTime = getBaseItemBuildTime(baseId, -facilityId, false);
		
		if (growthToLimitTime <= facilityBuildTime)
		{
			// mandatory limit facility order
			
			double priority = INF;
			productionDemand.addItemPriority(-facilityId, priority);
			
			debug
			(
				"\t%-32s"
				" priority=%5.2f   |"
				" populationLimit=%2d"
				" growthToLimitTime=%2d"
				" facilityBuildTime=%2d"
				" - mandatory order"
				"\n"
				, getFacility(facilityId)->name
				, priority
				, populationLimit
				, growthToLimitTime
				, facilityBuildTime
			);
			
			return;
		}
		
		// gain from building a facility
		
		double facilityGain = getFacilityGain(baseId, facilityId, true);
		
		// gain from population limit lifted
		
		double populationLimitIncome = getResourceScore(base->nutrient_surplus, 0, 0);
		double populationLimitUpkeep = getResourceScore(0.0, -Facility[facilityId].maint);
		double populationLimitGain = getGainDelay(getGainIncome(populationLimitIncome + populationLimitUpkeep), growthToLimitTime);
		
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
			" growthToLimitTime=%2d"
			" facilityGain=%5.2f"
			" populationLimitIncome=%5.2f"
			" populationLimitUpkeep=%5.2f"
			" populationLimitGain=%5.2f"
			" gain=%5.2f"
			"\n"
			, getFacility(facilityId)->name
			, priority
			, populationLimit
			, growthToLimitTime
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
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	bool ocean = is_ocean(getBaseMapTile(baseId));
	TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
	
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
			
			for (int vehicleId : baseTileInfo.playerVehicleIds)
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
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
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

void evaluateDefensiveProbeUnits()
{
	Profiling::start("evaluateDefensiveProbeUnits", "suggestBaseProduction");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	debug("evaluateDefensiveProbeUnits\n");
	
	// base new probe morale multiplier
	
	double newProbeMoraleMultiplier = has_facility(FAC_COVERT_OPS_CENTER, baseId) ? 1.25 : 1.00;
	
	// scan combat units for protection
	
	for (int unitId : aiFactionInfo->buildableUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		
		// defensive probe
		
		if (!(isInfantryUnit(unitId) && isProbeUnit(unitId)))
			continue;
		
		// exclude those base cannot produce
		
		if (!isBaseCanBuildUnit(baseId, unitId))
			continue;
		
		// seek for best target base gain
		
		double bestGain = 0.0;
		
		for (int targetBaseId : aiData.baseIds)
		{
			MAP *targetBaseTile = getBaseMapTile(targetBaseId);
			BaseInfo &targetBaseInfo = aiData.getBaseInfo(targetBaseId);
			BaseProbeData &targetBaseProbeData = targetBaseInfo.probeData;
			
			if (targetBaseProbeData.isSatisfied(false))
				continue;
			
			double travelTime = getUnitApproachTime(aiFactionId, unitId, baseTile, targetBaseTile);
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// probe
			
			double combatEffect = newProbeMoraleMultiplier;
			double combatEffectCoefficient = getCombatEffectCoefficient(combatEffect);
			double unitProbeGain = techStealGain * combatEffectCoefficient;
			double probeGain = unitProbeGain * travelTimeCoefficient;
			
			double upkeep = getResourceScore(-getUnitSupport(unitId), 0.0);
			double upkeepGain = getGainIncome(upkeep);
			
			// combined
			
			double gain =
				+ probeGain
				+ upkeepGain
			;
			
			bestGain = std::max(bestGain, gain);
			
			debug
			(
				"\t\t%-32s"
				" %-25s"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				" combatEffect=%5.2f"
				" combatEffectCoefficient=%5.2f"
				" unitProbeGain=%5.2f"
				" probeGain=%5.2f"
				" upkeepGain=%5.2f"
				" gain=%7.2f"
				"\n"
				, unit->name
				, Bases[targetBaseId].name
				, travelTime
				, travelTimeCoefficient
				, combatEffect
				, combatEffectCoefficient
				, unitProbeGain
				, probeGain
				, upkeepGain
				, gain
			);
			
		}
		
		// priority
		
		double rawPriority = getItemPriority(unitId, bestGain);
		double priority =
			conf.ai_production_base_probe_priority
			* rawPriority
		;
		
		// add production
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" ai_production_base_probe_priority=%5.2f"
			" bestGain=%5.2f"
			" rawPriority=%5.2f"
			"\n"
			, Units[unitId].name
			, priority
			, conf.ai_production_base_probe_priority
			, bestGain
			, rawPriority
		);
		
	}
	
	Profiling::stop("evaluateDefensiveProbeUnits");
	
}

void evaluateExpansionUnits()
{
	Profiling::start("evaluateExpansionUnits", "suggestBaseProduction");
	
	debug("evaluateExpansionUnits\n");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	MAP *baseTile = getBaseMapTile(baseId);
	
	// verify base can build a colony
	
	if (!canBaseProduceColony(baseId))
	{
		debug("\tbase cannot build a colony\n");
		Profiling::stop("evaluateExpansionUnits");
		return;
	}
	
	// population limit coefficient
	
	int populationLimit = getBasePopulationLimit(baseId);
	double populationLimitCoefficient = 1.00;
	
	if (base->pop_size >= populationLimit)
	{
		populationLimitCoefficient = 1.50;
	}
	else if (base->pop_size >= populationLimit - 1)
	{
		populationLimitCoefficient = 1.25;
	}
	
	// citizen loss gain
	
	double citizenIncome = getBaseCitizenIncome(baseId);
	double citizenLossGain = 0.75 * getGainIncome(-citizenIncome);
	
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
			
//		debug("\t[%3d] %-32s\n", unitId, unit->name);
		
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
			
			double travelTime = getUnitApproachTime(aiFactionId, unitId, baseTile, tile);
			
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
			
//			debug
//			(
//				"\t\t%s"
//				" travelTime=%7.2f"
//				" buildSiteBaseGain=%5.2f"
//				" buildSiteBuildGain=%5.2f"
//				"\n"
//				, getLocationString(tile).c_str()
//				, travelTime
//				, buildSiteBaseGain
//				, buildSiteBuildGain
//			);
			
		}
		
		// combine with citizen loss gain
		
		double gain = bestBuildSiteGain + citizenLossGain;
		
		// priority
		
		double rawPriority = getItemPriority(unitId, gain);
		double priority =
			conf.ai_production_expansion_priority
			* globalColonyDemand
			* populationLimitCoefficient
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
			" ai_production_expansion_priority=%5.2f"
			" globalColonyDemand=%5.2f"
			" populationLimitCoefficient=%5.2f"
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
			, populationLimitCoefficient
			, rawPriority
		);
		
	}
	
	Profiling::stop("evaluateExpansionUnits");
	
}

void evaluateTerraformUnits()
{
	struct TerraformingGain
	{
		double time;
		double income;
		double gain;
		bool operator<(TerraformingGain const &other) const
		{
			return this->gain > other.gain;
		}
	};

	Profiling::start("evaluateTerraformUnits", "suggestBaseProduction");
	
	debug("evaluateTerraformUnits\n");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	int baseSeaCluster = getBaseSeaCluster(baseTile);
	
	// supported formers
	
	int supportedFormerCounts[3] = {0, 0, 0};
	for (int vehicleId : aiData.formerVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		
		if (vehicle->home_base_id != baseId)
			continue;
		
		supportedFormerCounts[triad]++;
		
	}
	
	// existing formers
	
	int existingFormerCounts[3] = {0, 0, 0};
	for (int vehicleId : aiData.formerVehicleIds)
	{
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int chassisId = Units[Vehs[vehicleId].unit_id].chassis_id;
		CChassis &chassis = Chassis[chassisId];
		int triad = chassis.triad;
		int speed = chassis.speed;
		
		// same cluster
		
		switch (triad)
		{
		case TRIAD_AIR:
			if (!isSameAirCluster(chassisId, speed, baseTile, vehicleTile))
				continue;
			break;
		case TRIAD_SEA:
			if (!isSameSeaCluster(baseSeaCluster, vehicleTile))
				continue;
			break;
		case TRIAD_LAND:
			if (!isSameLandTransportedCluster(baseTile, vehicleTile))
				continue;
			break;
		}
		
		existingFormerCounts[triad]++;
		
	}
	
	// extra former gain
	
	double extraFormerGains[3] = {0.0, 0.0, 0.0};
	for (Triad triad : {TRIAD_AIR, TRIAD_SEA, TRIAD_LAND})
	{
		// for sea former base should have access to water
		
		if (triad == TRIAD_SEA && !(isBaseAccessesWater(baseId) && baseSeaCluster >= 0))
			continue;
		
		// variable set
		
		if (bestFormerChassisIds[triad] == -1 || bestFormerSpeeds[triad] == 0)
			continue;
		
		// average travel time and terraforming gains
		
		HarmonicSummaryStatistics distanceHarmonicSummary;
		std::vector<TerraformingGain> terraformingGains;
		for (FormerRequest &formerRequest : aiData.production.formerRequests)
		{
			TileInfo &tileInfo = aiData.getTileInfo(formerRequest.tile);
			
			// sensible terraforming time
			
			if (formerRequest.terraformingTime <= 0.0)
				continue;
			
			// compatible surface
			
			if ((triad == TRIAD_LAND && !tileInfo.land) || (triad == TRIAD_SEA && !tileInfo.ocean))
				continue;
			
			// same cluster
			
			switch (triad)
			{
			case TRIAD_AIR:
				if (!isSameAirCluster(bestFormerChassisIds[TRIAD_AIR], bestFormerSpeeds[TRIAD_AIR], baseTile, formerRequest.tile))
					continue;
				break;
			case TRIAD_SEA:
				if (!isSameSeaCluster(baseSeaCluster, formerRequest.tile))
					continue;
				break;
			case TRIAD_LAND:
				if (!isSameLandTransportedCluster(baseTile, formerRequest.tile))
					continue;
				break;
			}
			
			// distance
			
			double distance = (double)getRange(baseTile, formerRequest.tile);
			distanceHarmonicSummary.add(distance);
			
			// terraforming gain
			
			terraformingGains.push_back({formerRequest.terraformingTime, formerRequest.income, 0.0});
			
		}
		double averageDistance = distanceHarmonicSummary.getHarmonicMean();
		double averateTravelTime = averageDistance / bestFormerSpeeds[triad];
		
		// update terraforming gains
		
		for (TerraformingGain &terraformingGain : terraformingGains)
		{
			terraformingGain.time += averateTravelTime;
			terraformingGain.gain = terraformingGain.income / terraformingGain.time;
		}
		
		// sort terraforming gains
		
		std::sort(terraformingGains.begin(), terraformingGains.end());
		
		// extra former gain
		
		double existingFormerCount = (double)existingFormerCounts[triad];
		double existingFormerAccumulatedTime = 0.0;
		double existingFormerTotalGain = 0.0;
		double extraFormerCount = existingFormerCount + 1.0;
		double extraFormerAccumulatedTime = 0.0;
		double extraFormerTotalGain = 0.0;
		for (TerraformingGain &terraformingGain : terraformingGains)
		{
			if (existingFormerCount > 0.0)
			{
				existingFormerAccumulatedTime += terraformingGain.time / existingFormerCount;
				existingFormerTotalGain += getGainDelay(getGainIncome(terraformingGain.income), existingFormerAccumulatedTime);
			}
			extraFormerAccumulatedTime += terraformingGain.time / extraFormerCount;
			extraFormerTotalGain += getGainDelay(getGainIncome(terraformingGain.income), extraFormerAccumulatedTime);
//debug
//(
//	"\tterraformingGain.time=%5.2f"
//	" terraformingGain.income=%5.2f"
//	" existingFormerAccumulatedTime=%5.2f"
//	" existingFormerGain=%5.2f"
//	" extraFormerAccumulatedTime=%5.2f"
//	" extraFormerGain=%5.2f"
//	" delta=%5.2f"
//	"\n"
//	, terraformingGain.time
//	, terraformingGain.income
//	, existingFormerAccumulatedTime
//	, getGainDelay(getGainIncome(terraformingGain.income), existingFormerAccumulatedTime)
//	, extraFormerAccumulatedTime
//	, getGainDelay(getGainIncome(terraformingGain.income), extraFormerAccumulatedTime)
//	, getGainDelay(getGainIncome(terraformingGain.income), extraFormerAccumulatedTime) - getGainDelay(getGainIncome(terraformingGain.income), existingFormerAccumulatedTime)
//);
		}
		extraFormerGains[triad] = extraFormerTotalGain - existingFormerTotalGain;
		
	}
debug("extraFormerGains= %5.2f %5.2f %5.2f\n", extraFormerGains[0], extraFormerGains[1], extraFormerGains[2]);
	
	// process available former units
	
	for (int unitId : aiData.formerUnitIds)
	{
		UNIT &unit = Units[unitId];
		int triad = unit.triad();
		
		// best unit speed
		
		if (unit.chassis_id != bestFormerChassisIds[triad])
			continue;
		
		// for sea former base should have access to water
		
		if (triad == TRIAD_SEA && !(isBaseAccessesWater(baseId) && baseSeaCluster >= 0))
			continue;
		
		// shoud have some gain
		
		if (extraFormerGains[triad] == 0.0)
			continue;
		
		// early base restriction
		// no more than one former of each triad
		
		if (*CurrentTurn < 40 && supportedFormerCounts[triad] >= 1)
			continue;
		
		// unit priority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		
		if (unitPriorityCoefficient <= 0.0)
			continue;
		
		// terraforming gain
		
		double terraformingGain = extraFormerGains[triad];
		
		// upkeep gain
		
		double upkeepIncome = getResourceScore(-(double)getUnitSupport(unitId), 0.0);
		double upkeepGain = getGainIncome(upkeepIncome);
		
		// gain
		
		double gain = terraformingGain + upkeepGain;
		
		// priority
		
		double improvementPriority = 0.0;
		switch (triad)
		{
		case TRIAD_AIR:
			improvementPriority = 0.5 * (conf.ai_production_improvement_priority_sea + conf.ai_production_improvement_priority_land);
			break;
		case TRIAD_SEA:
			improvementPriority = conf.ai_production_improvement_priority_sea;
			break;
		case TRIAD_LAND:
			improvementPriority = conf.ai_production_improvement_priority_land;
			break;
		}
		
		double rawPriority = getItemPriority(unitId, gain);
		double priority =
			improvementPriority
			* unitPriorityCoefficient
			* rawPriority
		;
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" terraformingGain=%5.2f"
			" upkeepGain=%5.2f"
			" gain=%5.2f"
			" rawPriority=%5.2f"
			" improvementPriority=%5.2f"
			" unitPriorityCoefficient=%5.2f"
			"\n"
			, unit.name
			, priority
			, terraformingGain
			, upkeepGain
			, gain
			, rawPriority
			, improvementPriority
			, unitPriorityCoefficient
		);
		
	}
	
	Profiling::stop("evaluateTerraformUnits");
	
}

void evaluateConvoyUnits()
{
	Profiling::start("evaluateConvoyUnits", "suggestBaseProduction");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	debug("evaluateConvoyUnits\n");
	
	// process available supply units
	
	for (int unitId : aiFactionInfo->buildableUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		
		// crawler
		
		if (!isSupplyUnit(unitId))
			continue;
		
		// for sea crawler base should have access to water
		
		if (triad == TRIAD_SEA && !isBaseAccessesWater(baseId))
			continue;
		
		// unit priority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		
		if (unitPriorityCoefficient <= 0.0)
			continue;
		
		// iterate convoy requests
		
		double bestGain = 0.0;
		for (ConvoyRequest const &convoyRequest : aiData.production.convoyRequests)
		{
			// matching surface
			
			if ((is_ocean(convoyRequest.tile) && triad == TRIAD_LAND) || (!is_ocean(convoyRequest.tile) && triad == TRIAD_SEA))
				continue;
			
			// reachable
			
			if (!isUnitDestinationReachable(unitId, baseTile, convoyRequest.tile))
				continue;
			
			// travelTime
			
			double travelTime = getUnitApproachTime(aiFactionId, unitId, baseTile, convoyRequest.tile);
			if (travelTime == INF)
				continue;
			
			// gain
			
			double gain = getGainDelay(convoyRequest.gain, travelTime);
			
			bestGain = std::max(bestGain, gain);
			
		}
		
		// gain
		
		double incomeGain = bestGain;
		double upkeepGain = getGainIncome(getResourceScore(-getUnitSupport(unitId), 0.0));
		double gain = incomeGain + upkeepGain;
		
		// priority
		
		double priority = unitPriorityCoefficient * getItemPriority(unitId, gain);;
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" unitPriorityCoefficient=%5.2f"
			" incomeGain=%5.2f"
			" upkeepGain=%5.2f"
			" gain=%5.2f"
			"\n"
			, unit->name
			, priority
			, unitPriorityCoefficient
			, incomeGain
			, upkeepGain
			, gain
		);
		
	}
	
	Profiling::stop("evaluateConvoyUnits");
	
}

void evaluatePodPoppingUnits()
{
	Profiling::start("evaluatePodPoppingUnits", "suggestBaseProduction");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
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
		
		for (MAP *tile : aiData.pods)
		{
			// within range
			
			if (getRange(baseTile, tile) > surfacePodData.scanRange)
				continue;
			
			// matching surface
			
			if (is_ocean(tile) != surface)
				continue;
			
			// same cluster
			
			if ((surface == 0 && !isSameLandTransportedCluster(baseTile, tile)) || (surface == 1 && !isSameSeaCluster(baseTile, tile)))
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
		int offenseValue = unit->offense_value();
		int defenseValue = unit->defense_value();
		
		// surface triad
		
		if (triad == TRIAD_AIR)
			continue;
		
		// fastest triad chassis
		
		if (unit->speed() < aiFactionInfo->fastestTriadChassisIds.at(triad))
			continue;
		
		// best weapon and armor or psi
		
		if (!((offenseValue < 0 || offenseValue >= aiFactionInfo->maxConOffenseValue) && (defenseValue < 0 || defenseValue >= std::min(aiFactionInfo->maxConOffenseValue, aiFactionInfo->maxConDefenseValue))))
			continue;
		
		// pod data
		
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
		{
			Profiling::stop("evaluatePodPoppingUnits");
			return;
		}
		
		// consumption rate
		
		int unitSpeed = getUnitSpeed(aiFactionId, unitId);
		
		if (unitSpeed <= 0)
			continue;
		
		double unitTravelTime = 0.5 * surfacePodData.averagePodDistance / (double)unitSpeed;
		double unitConsumptionInterval = unitTravelTime + (unitSpeed == 1 ? 2.0 : 1.0); // for average repair time
		double unitConsumptionRate = 1.0 / unitConsumptionInterval;
		
		double unitTotalConsumptionRate = surfacePodData.totalConsumptionRate + unitConsumptionRate;
		double unitFactionConsumptionRate = surfacePodData.factionConsumptionRate + unitConsumptionRate;
		
		double unitConsumptionTime = surfacePodData.podCount / unitTotalConsumptionRate;
		double unitFactionConsumptionIncome = conf.ai_production_pod_bonus * unitFactionConsumptionRate;
		double unitFactionConsumptionGain = getGainTimeInterval(getGainIncome(unitFactionConsumptionIncome), 0.0, unitConsumptionTime);
		
		double factionConsumptionGainIcrease = unitFactionConsumptionGain - surfacePodData.factionConsumptionGain;
		double podPoppingGain = factionConsumptionGainIcrease;
		
		// upkeep
		
		double upkeepGain = getGainIncome(getResourceScore(-getUnitSupport(unitId), 0.0));
		
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
			" surface=%2d"
			" podCount=%2d"
			" totalConsumptionRate=%5.2f"
			" factionConsumptionRate=%5.2f"
			" unitConsumptionRate=%5.2f"
			" factionConsumptionGain=%5.2f"
			" unitFactionConsumptionGain=%5.2f"
			" factionConsumptionGainIcrease=%5.2f"
			" podPoppingGain=%5.2f"
			" upkeepGain=%5.2f"
			" gain=%5.2f"
			" rawPriority=%5.2f"
			"\n"
			, Units[unitId].name
			, priority
			, conf.ai_production_pod_popping_priority
			, unitPriorityCoefficient
			, surface
			, surfacePodData.podCount
			, surfacePodData.totalConsumptionRate
			, surfacePodData.factionConsumptionRate
			, unitConsumptionRate
			, surfacePodData.factionConsumptionGain
			, unitFactionConsumptionGain
			, factionConsumptionGainIcrease
			, podPoppingGain
			, upkeepGain
			, gain
			, rawPriority
		);
		
	}
	
	Profiling::stop("evaluatePodPoppingUnits");
	
}

void evaluateBaseDefenseUnits()
{
	Profiling::start("evaluateBaseDefenseUnits", "suggestBaseProduction");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
	
	debug("evaluateBaseDefenseUnits\n");
	
	// scan combat units for protection
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int defenseValue = unit->defense_value();
		
		// defensive
		
		if (!isInfantryDefensiveUnit(unitId))
			continue;
		
		// best armor or psi
		
		if (baseTileInfo.playerVehicleIds.size() == 0)
		{
			if (unitId != BSC_SCOUT_PATROL)
				continue;
		}
		else
		{
			if (!(defenseValue < 0 || unit->defense_value() >= aiFactionInfo->maxConDefenseValue))
				continue;
		}
			
		// exclude those base cannot produce
		
		if (!isBaseCanBuildUnit(baseId, unitId))
			continue;
		
		// saturation ratio
		
		double infantryDefensiveSaturationCoefficient = (isInfantryDefensiveUnit(unitId) && !isOffensiveUnit(unitId, aiFactionId)) ? globalInfantryDefensiveSaturationCoefficient : 1.0;
		
		// seek for best target base gain
		
		double bestGain = 0.0;
		
		for (int targetBaseId : aiData.baseIds)
		{
			MAP *targetBaseTile = getBaseMapTile(targetBaseId);
			BaseInfo &targetBaseInfo = aiData.baseInfos.at(targetBaseId);
			BasePoliceData &targetBasePoliceData = targetBaseInfo.policeData;
			CombatData &targetCombatData = targetBaseInfo.combatData;
			
			double travelTime = getUnitApproachTime(aiFactionId, unitId, baseTile, targetBaseTile);
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// police
			
			double unitPoliceGain = targetBasePoliceData.isSatisfied(isPolice2xUnit(unitId, aiFactionId)) ? 0.0 : targetBasePoliceData.getUnitPoliceGain(unitId, aiFactionId);
			double policeGain = conf.ai_production_priority_police * getGainDelay(unitPoliceGain, travelTime);
			
			// protection
			
			double combatEffect = targetCombatData.getProtectorUnitEffect(aiFactionId, unitId);
			double survivalEffect = getSurvivalEffect(combatEffect);
			double unitProtectionGain = targetCombatData.isSufficientProtect() ? 0.0 : aiFactionInfo->averageBaseGain * survivalEffect;
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
			* infantryDefensiveSaturationCoefficient
			* rawPriority
		;
		
		// add production
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" ai_production_base_protection_priority=%5.2f"
			" infantryDefensiveSaturationCoefficient=%5.2f"
			" bestGain=%5.2f"
			" rawPriority=%5.2f"
			"\n"
			, Units[unitId].name
			, priority
			, conf.ai_production_base_protection_priority
			, infantryDefensiveSaturationCoefficient
			, bestGain
			, rawPriority
		);
		
	}
	
	Profiling::stop("evaluateBaseDefenseUnits");
	
}

void evaluateBunkerDefenseUnits()
{
	Profiling::start("evaluateBunkerDefenseUnits", "suggestBaseProduction");
	
	debug("evaluateBunkerDefenseUnits\n");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
	
	// scan combat units for protection
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int defenseValue = unit->defense_value();
		
		// defensive
		
		if (!isInfantryDefensiveUnit(unitId))
			continue;
		
		// best armor or psi
		
		if (baseTileInfo.playerVehicleIds.size() == 0)
		{
			if (unitId != BSC_SCOUT_PATROL)
				continue;
		}
		else
		{
			if (!(defenseValue < 0 || unit->defense_value() >= aiFactionInfo->maxConDefenseValue))
				continue;
		}
			
		// exclude those base cannot produce
		
		if (!isBaseCanBuildUnit(baseId, unitId))
			continue;
		
		// saturation ratio
		
		double infantryDefensiveSaturationCoefficient = (isInfantryDefensiveUnit(unitId) && !isOffensiveUnit(unitId, aiFactionId)) ? globalInfantryDefensiveSaturationCoefficient : 1.0;
		
		// seek for best target base gain
		
		double bestGain = 0.0;
		
		for (robin_hood::pair<MAP *, CombatData> &bunkerCombatDataEntry : aiData.bunkerCombatDatas)
		{
			MAP *targetBunkerTile = bunkerCombatDataEntry.first;
			CombatData &targetBunkerCombatData = bunkerCombatDataEntry.second;
			
			double travelTime = getUnitApproachTime(aiFactionId, unitId, baseTile, targetBunkerTile);
			if (travelTime == INF)
				continue;
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
			
			// protection
			// 0.5 of average base gain
			
			double combatEffect = targetBunkerCombatData.getProtectorUnitEffect(aiFactionId, unitId);
			double survivalEffect = getSurvivalEffect(combatEffect);
			double unitProtectionGain = targetBunkerCombatData.isSufficientProtect() ? 0.0 : 0.5 * aiFactionInfo->averageBaseGain * survivalEffect;
			double protectionGain = unitProtectionGain * travelTimeCoefficient;
			
			double upkeep = getResourceScore(-getBaseNextUnitSupport(baseId, unitId), 0);
			double upkeepGain = getGainIncome(upkeep);
			
			// combined
			
			double gain =
				+ protectionGain
				+ upkeepGain
			;
			
			bestGain = std::max(bestGain, gain);
			
//			debug
//			(
//				"\t\t%-32s"
//				" %-25s"
//				" travelTime=%7.2f"
//				" travelTimeCoefficient=%5.2f"
//				" ai_production_priority_police=%5.2f"
//				" unitPoliceGain=%5.2f"
//				" policeGain=%5.2f"
//				" ai_production_base_protection_priority=%5.2f"
//				" combatEffect=%5.2f"
//				" survivalEffect=%5.2f"
//				" unitProtectionGain=%5.2f"
//				" protectionGain=%5.2f"
//				" upkeepGain=%5.2f"
//				" gain=%7.2f"
//				"\n"
//				, unit->name
//				, Bases[targetBaseId].name
//				, travelTime
//				, travelTimeCoefficient
//				, conf.ai_production_priority_police
//				, unitPoliceGain
//				, policeGain
//				, conf.ai_production_base_protection_priority
//				, combatEffect
//				, survivalEffect
//				, unitProtectionGain
//				, protectionGain
//				, upkeepGain
//				, gain
//			);
			
		}
		
		// priority
		
		double rawPriority = getItemPriority(unitId, bestGain);
		double priority =
			conf.ai_production_base_protection_priority
			* infantryDefensiveSaturationCoefficient
			* rawPriority
		;
		
		// add production
		
		productionDemand.addItemPriority(unitId, priority);
		
		debug
		(
			"\t%-32s"
			" priority=%5.2f   |"
			" ai_production_base_protection_priority=%5.2f"
			" infantryDefensiveSaturationCoefficient=%5.2f"
			" bestGain=%5.2f"
			" rawPriority=%5.2f"
			"\n"
			, Units[unitId].name
			, priority
			, conf.ai_production_base_protection_priority
			, infantryDefensiveSaturationCoefficient
			, bestGain
			, rawPriority
		);
		
	}
	
	Profiling::stop("evaluateBunkerDefenseUnits");
	
}

void evaluateTerritoryProtectionUnits()
{
	Profiling::start("evaluateTerritoryProtectionUnits", "suggestBaseProduction");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	debug("evaluateTerritoryProtectionUnits\n");
	
	// scan combat units for protection
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
//		int offenseValue = unit->offense_value();
//		int defenseValue = unit->defense_value();
		
//		// best weapon and armor or psi
//		
//		if (!((offenseValue < 0 || offenseValue >= aiFactionInfo->maxConOffenseValue) && (defenseValue < 0 || defenseValue >= std::min(aiFactionInfo->maxConOffenseValue, aiFactionInfo->maxConDefenseValue))))
//			continue;
//		
//		// fastest conventional chassis
//		
//		if (offenseValue > 0 && unit->chassis_id != aiFactionInfo->fastestTriadChassisIds.at(unit->triad()))
//			continue;
//		
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
			// enemy
			
			if (!enemyStackInfo->hostile)
				continue;
			
			// not sufficiently targeted
			
			if (!enemyStackInfo->isSufficient(false))
				continue;
			
			// ship and artillery do not attack tower
			
			if (isArtilleryUnit(unitId) && enemyStackInfo->alienFungalTower)
				continue;
			
			// get attack position
			
			MapDoubleValue position(nullptr, INF);
			bool direct = true;
			
			if (position.tile == nullptr && enemyStackInfo->isUnitCanMeleeAttackStack(unitId))
			{
				position = getMeleeAttackPosition(unitId, baseTile, enemyStackInfo->tile);
				direct = true;
			}
			if (position.tile == nullptr && enemyStackInfo->isUnitCanArtilleryAttackStack(unitId))
			{
				position = getArtilleryAttackPosition(unitId, baseTile, enemyStackInfo->tile);
				direct = enemyStackInfo->artillery;
			}
			
			if (position.tile == nullptr)
				continue;
			
			// travel time
			
			double travelTime = position.value;
//			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
			
			// gain
			
			double combatEffect;
			double combatValue;
			
			if (direct)
			{
				combatEffect = enemyStackInfo->getUnitDirectEffect(unitId);
				double winningProbability = getWinningProbability(combatEffect);
				double defenderDestructionGain = winningProbability * enemyStackInfo->destructionGain;
				double attackerDestructionGain = (1.0 - winningProbability) * aiData.unitDestructionGains.at(unitId);
				combatValue = defenderDestructionGain - attackerDestructionGain;
			}
			else
			{
				combatEffect = enemyStackInfo->getUnitBombardmentEffect(unitId);
				double defenderDestructionGain = combatEffect * enemyStackInfo->destructionGain;
				double attackerDestructionGain = 0.0;
				combatValue = defenderDestructionGain - attackerDestructionGain;
			}
			
			double attackGain = getGainDelay(getGainBonus(combatValue), travelTime);
			
			double upkeep = getResourceScore(-getUnitSupport(unitId), 0);
			double upkeepGain = getGainIncome(upkeep);
			
			double gain =
				+ attackGain
				+ upkeepGain
			;
			
			bestGain = std::max(bestGain, gain);
			
			debug
			(
				"\t\t%-32s"
				" -> %s"
				" travelTime=%7.2f"
				" combatEffect=%5.2f"
				" attackGain=%5.2f"
				" upkeepGain=%5.2f"
				" gain=%7.2f"
				"\n"
				, unit->name
				, getLocationString(enemyStackInfo->tile).c_str()
				, travelTime
				, combatEffect
				, attackGain
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
	
	Profiling::stop("evaluateTerritoryProtectionUnits");
	
}

void evaluateEnemyBaseAssaultUnits()
{
	Profiling::start("evaluateEnemyBaseAssaultUnits", "suggestBaseProduction");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	
	debug("evaluateEnemyBaseAssaultUnits\n");
	
	// scan combat units
	
	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int offenseValue = unit->offense_value();
		int defenseValue = unit->defense_value();
		
		// best weapon and armor or psi
		
		if (!((offenseValue < 0 || offenseValue >= aiFactionInfo->maxConOffenseValue) && (defenseValue < 0 || defenseValue >= std::min(aiFactionInfo->maxConOffenseValue, aiFactionInfo->maxConDefenseValue))))
			continue;
		
		// exclude those base cannot produce
		
		if (!isBaseCanBuildUnit(baseId, unitId))
			continue;
		
		// unit priority
		
		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId, unitId);
		
		if (unitPriorityCoefficient <= 0.0)
		{
			Profiling::stop("evaluateEnemyBaseAssaultUnits");
			return;
		}
		
		debug("\t%-32s\n", unit->name);
		
		// landArtillerySaturationCoefficient
		
		double landArtillerySaturationCoefficient = isLandArtilleryUnit(unitId) ? globalLandArtillerySaturationCoefficient : 1.0;
		
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
			
			// exclude player base or friendly base
			
			if (isFriendly(aiFactionId, enemyBase->faction_id))
				continue;
			
			// travel time
			
			double travelTime = getUnitApproachTime(aiFactionId, unitId, baseTile, enemyBaseTile);
			
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
			
//			debug
//			(
//				"\t\t%-25s"
//				" travelTime=%7.2f"
//				" assaultEffect=%5.2f"
//				" productionRatio=%5.2f"
//				" costRatio=%5.2f"
//				" adjustedEffect=%5.2f"
//				" adjustedSuperiority=%5.2f"
//				" enemyBaseCaptureGain=%5.2f"
//				" adjustedEnemyBaseCaptureGain=%5.2f"
//				" incomeGain=%5.2f"
//				" closestPlayerBaseRange=%2d"
//				" rangeCoefficient=%5.2f"
//				" captureGain=%5.2f"
//				" upkeepGain=%5.2f"
//				" gain=%5.2f"
//				"\n"
//				, enemyBase->name
//				, travelTime
//				, assaultEffect
//				, productionRatio
//				, costRatio
//				, adjustedEffect
//				, adjustedSuperiority
//				, enemyBaseCaptureGain
//				, adjustedEnemyBaseCaptureGain
//				, incomeGain
//				, enemyBaseInfo.closestPlayerBaseRange
//				, rangeCoefficient
//				, captureGain
//				, upkeepGain
//				, gain
//			);
			
		}
		
		// priority
		
		double rawPriority = getItemPriority(unitId, bestGain);
		double priority =
			unitPriorityCoefficient
			* landArtillerySaturationCoefficient
			* rawPriority
		;
		
		// add production
		
		productionDemand.addItemPriority(unitId, priority);
		
//		debug
//		(
//			"\t%-32s"
//			" priority=%5.2f   |"
//			" unitPriorityCoefficient=%5.2f"
//			" landArtillerySaturationCoefficient=%5.2f"
//			" bestGain=%5.2f"
//			" rawPriority=%5.2f"
//			"\n"
//			, Units[unitId].name
//			, priority
//			, unitPriorityCoefficient
//			, landArtillerySaturationCoefficient
//			, bestGain
//			, rawPriority
//		);
		
	}
	
	Profiling::stop("evaluateEnemyBaseAssaultUnits");
	
}

void evaluateSeaTransport()
{
	Profiling::start("evaluateSeaTransport", "suggestBaseProduction");
	
	debug("evaluateSeaTransport\n");
	
	ProductionDemand &productionDemand = *currentBaseProductionDemand;
	int baseId = productionDemand.baseId;
	int baseSeaCluster = productionDemand.baseSeaCluster;
	
	// base should have access to water
	
	if (baseSeaCluster == -1)
	{
		Profiling::stop("evaluateSeaTransport");
		return;
	}
	
	// find transport unit
	
	int unitId = aiFactionInfo->bestSeaTransportUnitId;
	
	if (unitId == -1)
	{
		debug("\tno seaTransport unit\n");
		Profiling::stop("evaluateSeaTransport");
		return;
	}
	
	double seaTransportDemand = seaTransportDemands[baseSeaCluster];
	
	if (seaTransportDemand <= 0.0)
	{
		debug("seaTransportDemand <= 0.0\n");
		Profiling::stop("evaluateSeaTransport");
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
	
	Profiling::stop("evaluateSeaTransport");
	
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
	
	// set projected
	
	base->pop_size = projectedSize - 1;
	Profiling::start("- canBaseProduceColony - computeBase");
	computeBase(baseId, true);
	Profiling::stop("- canBaseProduceColony - computeBase");
	
	int projectedMineralSurplus = base->mineral_surplus;
	
	// restore
	
	aiData.resetBase(baseId);
	
	// check surplus is not below zero
	
	if (projectedMineralSurplus < 0)
		return false;
	
	// all checks passed
	
	return true;
	
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
		VEH *vehicle = getVehicle(vehicleId);

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

/*
Checks if base can build unit.
*/
bool isBaseCanBuildUnit(int baseId, int unitId)
{
	BASE *base = getBase(baseId);
	UNIT *unit = getUnit(unitId);
	
	MAP *baseTile = getBaseMapTile(baseId);
	int baseSeaCluster = getBaseSeaCluster(baseTile);
	
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
	
	MAP *baseTile = getBaseMapTile(baseId);
	int baseSeaCluster = getBaseSeaCluster(baseTile);
	
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

void hurryProtectiveUnit()
{
	Profiling::start("hurryProtectiveUnit", "productionStrategy");
	
	debug("hurryProtectiveUnit - %s\n", getMFaction(aiFactionId)->noun_faction);

	int mostVulnerableBaseId = -1;
	double mostVulnerableBaseRelativeNotProvidedPresentEffect = 0.50;

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

		if (baseInfo.combatData.assailantWeight <= 0.0)
			continue;

		double relativeNotProvidedPresentEffect = std::min(1.0, baseInfo.combatData.remainingAssailantWeight / baseInfo.combatData.assailantWeight);

		debug("\t\tremainingEffect=%3d\n", item);

		if (relativeNotProvidedPresentEffect > mostVulnerableBaseRelativeNotProvidedPresentEffect)
		{
			mostVulnerableBaseId = baseId;
			mostVulnerableBaseRelativeNotProvidedPresentEffect = relativeNotProvidedPresentEffect;
		}

	}

	if (mostVulnerableBaseId == -1)
	{
		Profiling::stop("hurryProtectiveUnit");
		return;
	}

	debug
	(
		"\tmostVulnerableBase=%-25s"
		" mostVulnerableBaseRelativeNotProvidedPresentEffect=%5.2f"
		"\n"
		, getBase(mostVulnerableBaseId)->name
		, mostVulnerableBaseRelativeNotProvidedPresentEffect
	);

	int item = getBaseBuildingItem(mostVulnerableBaseId);

	if (item < 0 || !isInfantryDefensiveUnit(item))
	{
		debug("\t\tnot infantry devensive\n");
		Profiling::stop("hurryProtectiveUnit");
		return;
	}

	// hurry production

	int spendPool = getFaction(aiFactionId)->energy_credits;

	hurryProductionPartially(mostVulnerableBaseId, spendPool);

	debug("\t%-25s spendPool=%4d\n", getBase(mostVulnerableBaseId)->name, spendPool);

	Profiling::stop("hurryProtectiveUnit");
	
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
double getFacilityGain(int baseId, int facilityId, bool build)
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
	Profiling::start("- getFacilityGain - computeBase");
	computeBase(baseId, true);
	Profiling::stop("- getFacilityGain - computeBase");
	
	double newPopulationGrowth = getBasePopulationGrowth(baseId);
	double newIncome = getBaseIncome(baseId);
	int newEcoDamage = std::min(100, base->eco_damage);
	
	// restore base
	
	aiData.resetBase(baseId);
	
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
	
	double maintenanceIncome = (build ? +1 : -1) * getResourceScore(0.0, -Facility[facilityId].maint);
	double maintenanceIncomeGain = getGainIncome(maintenanceIncome);
	gain += maintenanceIncomeGain;
	
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
	TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
	
	// get base police allowed
	
	int policeAllowed = getBasePoliceAllowed(baseId);
	
	// no police allowed
	
	if (policeAllowed == 0)
		return 0;
		
	// count number of police units in base
	
	int policePresent = 0;
	
	for (int vehicleId : baseTileInfo.playerVehicleIds)
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

double getTechStealGain()
{
	int maxLevelTechId = -1;
	int maxTechLevel = 0;
	for (int techId = 0; techId < TECH_TranT; techId++)
	{
		if (!has_tech(techId, aiFactionId))
			continue;
		
		int techLevel = wtp_tech_level(techId);
		
		if (techLevel > maxTechLevel)
		{
			maxLevelTechId = techId;
			maxTechLevel = std::max(maxTechLevel, techLevel);
		}
		
	}
	
	if (maxLevelTechId == -1)
		return 0.0;
	
	double techCost = wtp_tech_cost(aiFactionId, maxLevelTechId);
	double gain = getGainBonus(techCost);
	
	// not our gain but other faction gain
	// divided by remaining faction count
	
	return gain / 6.0;
	
}

