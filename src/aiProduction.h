#pragma once

#include "main.h"
#include "game.h"
#include "aiData.h"

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

const std::set<int> MANAGED_UNIT_TYPES
{
	WPN_COLONY_MODULE,
	WPN_TERRAFORMING_UNIT,
	WPN_TROOP_TRANSPORT,
}
;

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
int findAttackerUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findDefenderUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findMixedUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int findArtilleryUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);
int selectCombatUnit(int baseId, int targetBaseId);
void evaluateTransportDemand();
int findTransportUnit();
int getFirstUnbuiltAvailableFacilityFromList(int baseId, std::vector<int> facilityIds);
double getResourceScore(double minerals, double energy);
double getIntakeGain(double score, double delay);
double getIncomeGain(double score, double delay);
double getGrowthGain(double score, double delay);
double getNormalizedGain(double developmentRate);
double getBaseIncome(int baseId);
double getBasePopulationGrowth(int baseId);
int getBaseEconomyIntake(int baseId);
int getBasePsychIntake(int baseId);
int getBaseLabsIntake(int baseId);
int getBaseTotalMineral(int baseId);
int getBaseTotalEnergy(int baseId);
std::vector<int> getProtectionCounterUnits(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength);

