#pragma once

struct BASE_WEIGHT
{
	int baseId;
	double weight;
};

void considerHurryingProduction(int factionId);
void hurryProductionPartially(int baseId, int allowance);

