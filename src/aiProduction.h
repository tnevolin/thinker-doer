#ifndef __AIPRODUCTION_H__
#define __AIPRODUCTION_H__

#include "main.h"
#include "game.h"

const double MAX_THREAT_TURNS = 10.0;
const double ARTILLERY_OFFENSIVE_VALUE_KOEFFICIENT = 0.5;

struct PRODUCTION_DEMAND
{
	int item;
	bool urgent;
	double priority;
	double sumPriorities;
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

void aiProductionStrategy();
void populateProducitonLists();
void evaluateFacilitiesDemand();
void evaluateBasePopulationLimitFacilitiesDemand(int baseId);
void evaluateBasePsychFacilitiesDemand(int baseId);
void evaluateBaseMultiplyingFacilitiesDemand(int baseId);
void evaluateLandImprovementDemand();
void evaluateExpansionDemand();
void evaluateExplorationDemand();
void evaluateNativeProtectionDemand();
void evaluateFactionProtectionDemand();
void setProductionChoices();
int findNearestOwnBaseId(int x, int y, int region);
HOOK_API int suggestBaseProduction(int baseId, int a2, int a3, int a4);
void addProductionDemand(int baseId, int item, bool immediate, double priority);
int selectBestNativeDefensePrototype(int factionId);
int findStrongestNativeDefensePrototype(int factionId);
int findConventionalDefenderUnit(int factionId);
int findFastAttackerUnit();
int findScoutUnit(int triad);
int findColonyUnit(int triad);
int findOptimalColonyUnit(int triad, int mineralSurplus, double infantryTurnsToDestination);
bool canBaseProduceColony(int baseId, int colonyUnitId);
int findFormerUnit(int triad);

#endif // __AIPRODUCTION_H__
