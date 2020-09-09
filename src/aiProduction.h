#ifndef __AIPRODUCTION_H__
#define __AIPRODUCTION_H__

#include "main.h"
#include "game.h"

const int MAX_RANGE = 500;
const double MIN_THREAT_TURNS = 4.0;
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
void calculateNativeProtectionDemand();
void calculateExpansionDemand();
void calculateConventionalDefenseDemand();
void setProductionChoices();
double getFactionConventionalOffenseMultiplier(int id);
double getFactionConventionalDefenseMultiplier(int id);
int findRangeToNearestOwnBase(int x, int y, int region);
int suggestBaseProduction(int id, bool baseProductionDone, int choice);
void addProductionDemand(int id, bool immediate, int item, double priority);
int selectBestNativeDefensePrototype(int factionId);
int findStrongestNativeDefensePrototype(int factionId);

#endif // __AIPRODUCTION_H__
