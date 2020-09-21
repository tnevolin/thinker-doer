#ifndef __AIHURRY_H__
#define __AIHURRY_H__

#include "main.h"
#include "game.h"

struct BASE_WEIGHT
{
	int baseId;
	double weight;
};

void factionHurryProduction(int factionId);
void hurryProductionPartially(int baseId, int allowance);

#endif // __AIHURRY_H__
