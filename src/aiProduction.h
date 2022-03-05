#pragma once

#include "main.h"
#include "game.h"

const int GAME_DURATION = 350;
const double MAX_THREAT_TURNS = 10.0;
const double ARTILLERY_OFFENSIVE_VALUE_KOEFFICIENT = 0.5;

struct PRODUCTION_DEMAND
{
	int baseId;
	BASE *base;
	int item;
	double priority;
};

struct PRODUCTION_CHOICE
{
	int item;
	bool urgent;
};

enum UNIT_TYPE
{
	UT_DEFENSE = 0,
};

const int WTP_MANAGED_FACILITIES[] =
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
};

const int WTP_MANAGED_UNIT_PLANS[] =
{
    PLAN_RECONNAISANCE,
    PLAN_COLONIZATION,
    PLAN_TERRAFORMING,
};

enum COMBAT_UNIT_CLASS
{
	UC_NOT_COMBAT,
	UC_INFANTRY_DEFENDER,
	UC_INFANTRY_ATTACKER,
	UC_LAND_FAST_ATTACKER,
	UC_LAND_ARTILLERY,
	UC_SHIP,
	UC_BOMBER,
	UC_INTERCEPTOR,
};

const double COMBAT_UNIT_CLASS_WEIGHTS[] =
{
	0.0,
	1.0,
	0.5,
	0.5,
	1.0,
	1.0,
	0.5,
	0.1,
};

void productionStrategy();
void setupProductionVariables();
void evaluateGlobalBaseDemand();
void evaluateGlobalFormerDemand();
//HOOK_API int modifiedBaseProductionChoice(int baseId, int a2, int a3, int a4);
void setProduction();
int aiSuggestBaseProduction(int baseId, int choice);
bool evaluateMandatoryDefenseDemand();
void evaluateFacilitiesDemand();
void evaluatePopulationLimitFacilitiesDemand();
void evaluatePsychFacilitiesDemand();
void evaluateEcoDamageFacilitiesDemand();
void evaluateMultiplyingFacilitiesDemand();
void evaluateDefensiveFacilitiesDemand();
void evaluateMilitaryFacilitiesDemand();
void evaluatePoliceDemand();
void evaluateLandFormerDemand();
void evaluateSeaFormerDemand();
void evaluateLandColonyDemand();
void evaluateSeaColonyDemand();
void evaluateAlienDefenseDemand();
void evaluateLandAlienDefenseDemand();
void evaluateSeaAlienDefenseDemand();
void evaluateAlienHuntingDemand();
void evaluateLandAlienHuntingDemand();
void evaluatePodPoppingDemand();
void evaluateLandPodPoppingDemand();
void evaluateSeaPodPoppingDemand();
void evaluatePrototypingDemand();
void evaluateCombatDemand();
void addProductionDemand(int item, double priority);
int findNativeDefenderUnit();
int findNativeAttackerUnit(bool ocean);
int findRegularDefenderUnit(int factionId);
int findFastAttackerUnit(int factionId);
int findScoutUnit(int factionId, int triad);
int findColonyUnit(int factionId, int triad);
int findOptimalColonyUnit(int factionId, int triad, int mineralSurplus);
bool canBaseProduceColony(int baseId);
int findFormerUnit(int factionId, int triad);
std::vector<int> getRegionBases(int factionId, int region);
int getRegionBasesMaxMineralSurplus(int factionId, int region);
int getRegionBasesMaxPopulationSize(int factionId, int region);
int calculateRegionSurfaceUnitTypeCount(int factionId, int region, int weaponType);
int calculateUnitTypeCount(int baseId, int weaponType, int triad, int excludedBaseId);
bool isBaseNeedPsych(int baseId);
int findPoliceUnit(int factionId);
bool isMilitaryItem(int item);
int thinker_base_prod_choice(int id, int v1, int v2, int v3);
int findBestAntiNativeUnitId(int baseId, int targetBaseId, int opponentTriad);
int findBestAntiRegularUnitId(int baseId, int targetBaseId, int opponentTriad);
int selectCombatUnit(int baseId, int targetBaseId);
void evaluateTransportDemand();
int findTransportUnit();
int getUnitClass(int unitId);
int getFirstUnbuiltFacilityFromList(int baseId, std::vector<int> facilityIds);
double getResourceScore(double minerals, double energy);
double getIntakeGain(double score, double delay);
double getIncomeGain(double score, double delay);
double getGrowthGain(double score, double delay);
double getNormalizedGain(double developmentRate);
double getBaseIncome(int baseId);
double getBasePopulationGrowthRate(int baseId);
int getBasePsychIntake(int baseId);
int getBaseEconomyIntake(int baseId);
int getBaseLabsIntake(int baseId);

