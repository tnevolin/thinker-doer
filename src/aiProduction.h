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

HOOK_API int suggestBaseProduction(int baseId, int a2, int a3, int a4);
int aiSuggestBaseProduction(int baseId, int choice);
void evaluateFacilitiesDemand();
void evaluateBasePopulationLimitFacilitiesDemand();
void evaluateBasePsychFacilitiesDemand();
void evaluateBaseMultiplyingFacilitiesDemand();
void evaluateLandImprovementDemand();
void evaluateExpansionDemand();
void evaluateExplorationDemand();
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
int calculateRegionSurfaceUnitTypeCount(int factionId, int region, int weaponType);

#endif // __AIPRODUCTION_H__
