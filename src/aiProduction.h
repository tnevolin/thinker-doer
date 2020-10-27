#ifndef __AIPRODUCTION_H__
#define __AIPRODUCTION_H__

#include "main.h"
#include "game.h"

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

HOOK_API int suggestBaseProduction(int baseId, int a2, int a3, int a4);
int aiSuggestBaseProduction(int baseId, int choice);
void evaluateFacilitiesDemand();
void evaluateBasePopulationLimitFacilitiesDemand();
void evaluateBasePsychFacilitiesDemand();
void evaluateBaseMultiplyingFacilitiesDemand();
void evaluateLandImprovementDemand();
void evaluateLandExpansionDemand();
void evaluateOceanExpansionDemand();
void evaluateExplorationDemand();
void evaluatePoliceDemand();
void evaluateNativeProtectionDemand();
void evaluateFactionProtectionDemand();
void addProductionDemand(int item, double priority);
int selectBestNativeDefensePrototype(int factionId);
int findStrongestNativeDefensePrototype(int factionId);
int findConventionalDefenderUnit(int factionId);
int findFastAttackerUnit(int factionId);
int findScoutUnit(int factionId, int triad);
int findColonyUnit(int factionId, int triad);
int findOptimalColonyUnit(int factionId, int triad, int mineralSurplus, double infantryTurnsToDestination);
bool canBaseProduceColony(int baseId, int colonyUnitId);
int findFormerUnit(int factionId, int triad);
std::vector<int> getRegionBases(int factionId, int region);
int getRegionBasesMaxMineralSurplus(int factionId, int region);
int getRegionBasesMaxPopulationSize(int factionId, int region);
int getMaxBaseSize(int factionId);
int calculateRegionSurfaceUnitTypeCount(int factionId, int region, int weaponType);
int calculateUnitTypeCount(int baseId, int weaponType, int triad, int excludedBaseId);
bool isBaseNeedPsych(int baseId);
int findPoliceUnit(int factionId);
HOOK_API void modifiedBaseFirst(int baseId);
int getNearestFactionBaseRange(int factionId, int x, int y);

#endif // __AIPRODUCTION_H__
