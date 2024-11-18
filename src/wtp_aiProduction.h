#pragma once

#include "main.h"
#include "game.h"
#include "wtp_aiData.h"

const int GAME_DURATION = 350;
const double MAX_THREAT_TURNS = 10.0;
const double ARTILLERY_OFFENSIVE_VALUE_KOEFFICIENT = 0.5;
const int LAST_TURN = 400;
const int BASE_FUTURE_SPAN = 100;
// psych 50% multiplier bonus to base resource growth (more workers)
const double PSYCH_MULTIPLIER_RESOURCE_GROWTH_INCREASE = 0.25;
double const MIN_COMBAT_DEMAND_SUPERIORITY = 2.0;
double const MAX_COMBAT_DEMAND_SUPERIORITY = 3.0;

struct BaseProductionInfo
{
	double baseGain;
	std::array<double, 2> extraPoliceGains;
};

struct ProductionDemand
{
	int baseId;
	BASE *base;
	int baseSeaCluster;
	
	int item;
	double priority;
	
	double baseGain;
	double baseCitizenGain;
	double baseWorkerGain;
	
	void initialize(int baseId);
	void addItemPriority(int item, double priority);
	
};

struct PRODUCTION_CHOICE
{
	int item;
	bool urgent;
};

const robin_hood::unordered_flat_set<int> MANAGED_UNIT_TYPES
{
	WPN_COLONY_MODULE,
	WPN_TERRAFORMING_UNIT,
	WPN_TROOP_TRANSPORT,
}
;

const robin_hood::unordered_flat_set<int> MANAGED_FACILITIES
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

struct Interval
{
	int min = 0;
	int max = 0;
	Interval(int _min, int _max): min{_min}, max{_max} {}
};

struct SurfacePodData
{
	int scanRange;
	int podCount;
	double averagePodDistance;
	double totalConsumptionRate;
	double factionConsumptionRate;
	double factionConsumptionGain;
	
};


// production strategy

void productionStrategy();
void populateFactionProductionData();
void evaluateGlobalColonyDemand();
void evaluateGlobalSeaTransportDemand();
void setProduction();

// base item selection

void suggestBaseProduction(int baseId);

void evaluateFacilities();
void evaluateStockpileEnergy();
void evaluateHeadquarters();
void evaluatePsychFacilitiesRemoval();
void evaluatePsychFacilities();
void evaluateIncomeFacilities();
void evaluateMineralMultiplyingFacilities();
void evaluatePopulationLimitFacilities();
void evaluateMilitaryFacilities();
void evaluatePrototypingFacilities();

void evaluateExpansionUnits();
void evaluateTerraformingUnits();
void evaluateCrawlingUnits();
void evaluatePodPoppingUnits();
void evaluateBaseDefenseUnits();
void evaluateTerritoryProtectionUnits();
void evaluateEnemyBaseAssaultUnits();
void evaluateSeaTransport();
void evaluateProject();
int findAntiNativeDefenderUnit(bool ocean);
int findNativeAttackerUnit(bool ocean);
int findScoutUnit(int triad);
bool canBaseProduceColony(int baseId);
int findFormerUnit(int triad);
std::vector<int> getRegionBases(int factionId, int region);
int getRegionBasesMaxMineralSurplus(int factionId, int region);
int getRegionBasesMaxPopulationSize(int factionId, int region);
int calculateUnitTypeCount(int baseId, int weaponType, int triad, int excludedBaseId);
bool isMilitaryItem(int item);
int findAttackerUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findDefenderUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findMixedUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findArtilleryUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int selectProtectionUnit(int baseId, int targetBaseId);
int selectAlienProtectorUnit();
int findTransportUnit();
bool isBaseCanBuildUnit(int baseId, int unitId);
bool isBaseCanBuildFacility(int baseId, int facilityId);
int getFirstAvailableFacility(int baseId, std::vector<int> facilityIds);
std::vector<int> getProtectionCounterUnits(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
double getUnitPriorityCoefficient(int baseId, int unitId);
int findInfantryPoliceUnit(bool first);
void selectProject();
void assignProject();
void hurryProtectiveUnit();

//=======================================================
// estimated income functions
//=======================================================

double getFacilityGain(int baseId, int facilityId, bool build, bool includeMaintenance);
double getDevelopmentScore(int turn);
double getDevelopmentScoreGrowth(int turn);
double getDevelopmentScale();
double getRawProductionPriority(double gain, double cost);
double getProductionPriority(double gain, double cost);
double getProductionConstantIncomeGain(double score, double buildTime, double bonusDelay);
double getProductionLinearIncomeGain(double score, double buildTime, double bonusDelay);
double getMoraleProportionalMineralBonus(std::array<int,4> levels);
BaseMetric getBaseMetric(int baseId);
double getDemand(double required, double provided);

//=======================================================
// statistical estimates
//=======================================================

double getFactionStatisticalMineralIntake2(double turn);
double getFactionStatisticalMineralIntake2Growth(double turn);
double getFactionStatisticalBudgetIntake2(double turn);
double getFactionStatisticalBudgetIntake2Growth(double turn);
double getBaseStatisticalSize(double age);
double getBaseStatisticalMineralIntake(double age);
double getBaseStatisticalMineralIntake2(double age);
double getBaseStatisticalMineralMultiplier(double age);
double getBaseStatisticalBudgetIntake(double age);
double getBaseStatisticalBudgetIntake2(double age);
double getBaseStatisticalBudgetMultiplier(double age);
double getFlatMineralIntakeGain(int delay, double value);
double getFlatBudgetIntakeGain(int delay, double value);
double getBasePopulationGain(int baseId, const Interval &baseSizeInterval);
double getBasePopulationGrowthGain(int baseId, int delay, double proportionalGrowthIncrease);
double getBaseProportionalMineralIntakeGain(int baseId, int delay, double proportion);
double getBaseProportionalBudgetIntakeGain(int baseId, int delay, double proportion);
double getBaseProportionalMineralIntake2Gain(int baseId, int delay, double proportion);
double getBaseProportionalBudgetIntake2Gain(int baseId, int delay, double proportion);

//=======================================================
// production item helper functions
//=======================================================

int getBasePoliceExtraCapacity(int baseId);
double getItemPriority(int item, double gain);
double getBaseColonyUnitGain(int baseId, int unitId, double travelTime, double buildSiteScore);
double getBasePoliceGain(int baseId, bool police2x);

