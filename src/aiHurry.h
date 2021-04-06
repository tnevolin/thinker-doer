#pragma once

#include "main.h"
#include "game.h"

struct BASE_WEIGHT
{
	int baseId;
	double weight;
};

void considerHurryingProduction(int factionId);
void hurryProductionPartially(int baseId, int allowance);

