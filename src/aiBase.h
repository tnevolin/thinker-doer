#ifndef __AIBASE_H__
#define __AIBASE_H__

#include "main.h"
#include "game.h"

struct BASE_WEIGHT
{
	BASE *base;
	double weight;
};

void factionHurryProduction(int factionId);
void hurryProductionPartially(BASE *base, int allowance);

#endif // __AIBASE_H__
