#pragma once

#include "main.h"
#include "game.h"
#include "aiData.h"

const int GAME_DURATION = 350;
const double MAX_THREAT_TURNS = 10.0;
const double ARTILLERY_OFFENSIVE_VALUE_KOEFFICIENT = 0.5;
const int LAST_TURN = 400;
const int BASE_FUTURE_SPAN = 100;

struct ItemPriority
{
	int item = -FAC_STOCKPILE_ENERGY;
	double priority = 0.0;
};

struct ProductionDemand
{
	int baseId;
	BASE *base;
	double colonyCoefficient;
	
	std::map<int, double> priorities;
	
	void initialize(int baseId);
	void add(int item, double priority, bool upkeep);
	double getItemPriority(int item);
	ItemPriority get();
	
};

struct PRODUCTION_CHOICE
{
	int item;
	bool urgent;
};

const std::set<int> MANAGED_UNIT_TYPES
{
	WPN_COLONY_MODULE,
	WPN_TERRAFORMING_UNIT,
	WPN_TROOP_TRANSPORT,
}
;

const std::set<int> MANAGED_FACILITIES
{
    FAC_HEADQUARTERS,
    FAC_CHILDREN_CRECHE,
    FAC_RECYCLING_TANKS,
    FAC_PERIMETER_DEFENSE,
    FAC_TACHYON_FIELD,
    FAC_RECREATION_COMMONS,
    FAC_ENERGY_BANK,
    FAC_NETWORK_NODE,
    FAC_BIOLOGY_LAB,
    FAC_SKUNKWORKS,
    FAC_HOLOGRAM_THEATRE,
    FAC_PARADISE_GARDEN,
    FAC_TREE_FARM,
    FAC_HYBRID_FOREST,
    FAC_FUSION_LAB,
    FAC_QUANTUM_LAB,
    FAC_RESEARCH_HOSPITAL,
    FAC_NANOHOSPITAL,
    FAC_ROBOTIC_ASSEMBLY_PLANT,
    FAC_NANOREPLICATOR,
    FAC_QUANTUM_CONVERTER,
    FAC_GENEJACK_FACTORY,
    FAC_PUNISHMENT_SPHERE,
    FAC_HAB_COMPLEX,
    FAC_HABITATION_DOME,
//    FAC_PRESSURE_DOME,
    FAC_COMMAND_CENTER,
    FAC_NAVAL_YARD,
    FAC_AEROSPACE_COMPLEX,
    FAC_BIOENHANCEMENT_CENTER,
    FAC_CENTAURI_PRESERVE,
    FAC_TEMPLE_OF_PLANET,
//    FAC_PSI_GATE,
//    FAC_COVERT_OPS_CENTER,
//    FAC_BROOD_PIT,
    FAC_AQUAFARM,
    FAC_SUBSEA_TRUNKLINE,
    FAC_THERMOCLINE_TRANSDUCER,
//    FAC_FLECHETTE_DEFENSE_SYS,
//    FAC_SUBSPACE_GENERATOR,
//    FAC_GEOSYNC_SURVEY_POD,
//    FAC_SKY_HYDRO_LAB,
//    FAC_NESSUS_MINING_STATION,
//    FAC_ORBITAL_POWER_TRANS,
//    FAC_ORBITAL_DEFENSE_POD,
    FAC_STOCKPILE_ENERGY,
}
;

struct BaseMetric
{
	int nutrient = 0;
	int mineral = 0;
	int economy = 0;
	int psych = 0;
	int labs = 0;
};

void productionStrategy();
void evaluateGlobalBaseDemand();
void evaluateGlobalFormerDemand();
void evaluateGlobalProtectionDemand();
void evaluateGlobalLandCombatDemand();
void evaluateGlobalTerritoryLandCombatDemand();
void evaluateGlobalSeaCombatDemand();
void evaluateGlobalLandAlienCombatDemand();
//HOOK_API int modifiedBaseProductionChoice(int baseId, int a2, int a3, int a4);
void setProduction();
void suggestBaseProduction(int baseId);
void evaluateMandatoryDefenseDemand();
void evaluateFacilities();
void evaluateHeadquarters();
void evaluatePopulationLimitFacilities();
void evaluatePunishmentSphere();
void evaluatePunishmentSphereRemoval();
void evaluatePsychFacilities();
void evaluatePsychFacilitiesRemoval();
void evaluateSimpleFacilities();
void evaluateBiologyLab();
void evaluateBroodPit();
void evaluateDefensiveFacilities();
void evaluateMilitaryFacilities();
void evaluatePrototypingFacilities();
void evaluatePolice();
void evaluateLandFormerDemand();
void evaluateSeaFormerDemand();
void evaluateColony();
void evaluatePodPoppingDemand();
void evaluateLandPodPoppingDemand();
void evaluateSeaPodPoppingDemand();
void evaluateProtectionDemand();
void evaluateLandCombatDemand();
void evaluateSeaCombatDemand();
void evaluateLandAlienCombatDemand();
void evaluateProjectDemand();
int findAntiNativeDefenderUnit(bool ocean);
int findNativeAttackerUnit(bool ocean);
int findScoutUnit(int triad);
bool canBaseProduceColony(int baseId);
int findFormerUnit(int triad);
std::vector<int> getRegionBases(int factionId, int region);
int getRegionBasesMaxMineralSurplus(int factionId, int region);
int getRegionBasesMaxPopulationSize(int factionId, int region);
int calculateRegionSurfaceUnitTypeCount(int factionId, int region, int weaponType);
int calculateUnitTypeCount(int baseId, int weaponType, int triad, int excludedBaseId);
bool isMilitaryItem(int item);
int findAttackerUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findDefenderUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findMixedUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findArtilleryUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findPrototypeUnit();
int selectProtectionUnit(int baseId, int targetBaseId);
int selectCombatUnit(int baseId, bool ocean);
int selectAlienProtectorUnit();
void evaluateTransportDemand();
int findTransportUnit();
int getFirstAvailableFacility(int baseId, std::vector<int> facilityIds);
double getBaseIncome(int baseId);
double getBaseWorkerRelativeIncome(int baseId);
double getBaseWorkerIncome(int baseId);
double getBaseWorkerNutrientIntake(int baseId);
double getCitizenIncome();
double getBaseRelativeIncomeGrowth(int baseId);
double getBaseIncomeGrowth(int baseId);
double getCitizenIncomeGrowth(int baseId);
std::vector<int> getProtectionCounterUnits(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
double getUnitPriorityCoefficient(int baseId);
int findInfantryPoliceUnit(bool first);
void evaluateGlobalPoliceDemand();
void selectProject();
void assignProject();
void hurryProtectiveUnit();
double getItemProductionPriority(int baseId, int item, int delay, double bonus, double income, double incomeGrowth);
double getItemProductionPriority(int baseId, int item, int delay, double income, double incomeGrowth);
double getEmulatedFacilityProductionPriority(int baseId, int facilityId);
double getEmulatedPoliceProductionPriority(int baseId, int unitId, int policeCount);
double getComputedProductionPriority
(
	int baseId,
	int item,
	int delay,
	int currentWorkerCount,
	double currentIncome,
	double currentIncomeGrowth,
	double currentRelativeIncomeGrowth,
	double currentNutrientIntake,
	double workerRelativeIncome,
	double workerNutrientIntake,
	double itemIncome,
	int projectedSize,
	int oldWorkerCount,
	int oldEcoDamage,
	int newWorkerCount,
	int newEcoDamage
);
double getBaseFacilityIncome(int baseId, int facilityId);
double getDevelopmentScore(double t);
double getDevelopmentScoreTangent(double t);
double getDevelopmentScale();
double getBaseSize(double age);
double getNewBaseGain(int delay);
double getColonyRawProductionPriority();
double getGain(int delay, double bonus, double income, double incomeGrowth);
double getIncomeGain(int delay, double income, double incomeGrowth);
double getRawProductionPriority(double gain, double cost);
double getProductionPriority(double gain, double cost);
double getBaseGrowthScale(int baseId);
double getProductionConstantIncomeGain(double score, double buildTime, double bonusDelay);
double getProductionLinearIncomeGain(double score, double buildTime, double bonusDelay);
double getProductionProportionalIncomeGain(double score, int buildTime, int bonusDelay);
double getPsychIncomeGrowth(int baseId, double psychGrowth);
double getBaseMoraleBonusScore(int baseId, std::vector<int> levels);
BaseMetric getBaseMetric(int baseId);
BaseMetric getFacilityImprovement(int baseId, int facilityId);
int getFacilityWorkerCountEffect(int baseId, int facilityId);
double getDemand(double required, double provided);

