#ifndef __AIPRODUCTION_H__
#define __AIPRODUCTION_H__

#include "main.h"
#include "game.h"

const int MAX_RANGE = 500;
const double MIN_THREAT_TURNS = 4.0;
const double ARTILLERY_OFFENSIVE_VALUE_KOEFFICIENT = 0.5;

enum UNIT_TYPE
{
	UT_DEFENSE = 0,
};

struct FACTION_INFO
{
	double conventionalOffenseMultiplier;
	double conventionalDefenseMultiplier;
	double threatKoefficient;
};

void aiProductionStrategy();
void populateProducitonLists();
void calculateConventionalDefenseDemand();
double getFactionConventionalOffenseMultiplier(int id);
double getFactionConventionalDefenseMultiplier(int id);
int findRangeToNearestOwnBase(int x, int y, int region);
int suggestBaseProduction(int id, int choice);

#endif // __AIPRODUCTION_H__
