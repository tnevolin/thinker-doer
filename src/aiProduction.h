#ifndef __AIPRODUCTION_H__
#define __AIPRODUCTION_H__

#include "main.h"
#include "game.h"

const double MAX_THREAT_TURNS = 10.0;
const double ARTILLERY_OFFENSIVE_VALUE_KOEFFICIENT = 0.5;

struct PRODUCTION_PRIORITY
{
	int item;
	double priority;
};

struct PRODUCTION_DEMAND
{
	bool immediate;
	int immediateItem;
	double immediatePriority;
	std::vector<PRODUCTION_PRIORITY> regular;
};

struct PRODUCTION_CHOICE
{
	int item;
	bool immediate;
};

enum UNIT_TYPE
{
	UT_DEFENSE = 0,
};

void aiProductionStrategy();
void populateProducitonLists();
void evaluateExplorationDemand();
void evaluateLandImprovementDemand();
void evaluateExpansionDemand();
void evaluateNativeProtectionDemand();
void evaluateFactionProtectionDemand();
void setProductionChoices();
int findNearestOwnBaseId(int x, int y, int region);
int suggestBaseProduction(int id, bool productionDone, int choice);
void addProductionDemand(int id, bool immediate, int item, double priority);
int selectBestNativeDefensePrototype(int factionId);
int findStrongestNativeDefensePrototype(int factionId);
int findDefenderPrototype();
int findFastAttackerUnit();
bool undesirableProduction(int id);
int findScoutUnit(int triad);
int findColonyUnit(int triad);
int findOptimalColonyUnit(int triad, int mineralSurplus, double infantryTurnsToDestination);

#endif // __AIPRODUCTION_H__
