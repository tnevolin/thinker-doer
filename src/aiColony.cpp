#include "aiColony.h"

double getBasePositionScore(int factionId, int x, int y)
{
	MAP *tile = getMapTile(x, y);
	bool oceanBase = is_ocean(tile);

	// collect tile yield scores

	std::vector<double> tileScores;

	for (MAP_INFO tileInfo : getBaseRadiusTileInfos(x, y, false))
	{
		// ignore claimed tiles

		if (tileInfo.tile->owner != -1 && tileInfo.tile->owner != factionId)
			continue;

		// ignore not owned land tiles for ocean base

		if (oceanBase && !is_ocean(tileInfo.tile) && tileInfo.tile->owner != factionId)
			continue;

		// collect yield score

		tileScores.push_back(getBaseRadiusTileScore(tileInfo));

	}

	// summarize top scores

	double score = 0.0;

	double weight = 1.0;
	double weightMultiplier = 0.95;
	for (double tileScore : tileScores)
	{
		score += weight * tileScore;
		weight *= weightMultiplier;
	}

	// discourage border radius intersection

	score -= 0.25 * (double)getFriendlyIntersectedBaseRadiusTileCount(factionId, x, y);

	// encourage land usage

	score += 2.0 * (double)getFriendlyLandBorderedBaseRadiusTileCount(factionId, x, y);

	// prefer coast

	if (isCoast(x, y))
	{
		score += 2.0;
	}

	// return score

	return score;

}

double getBaseRadiusTileScore(MAP_INFO tileInfo)
{
	double score = 0.0;

	double nutrientWeight = 1.0;
	double mineralWeight = 1.0;
	double energyWeight = 0.5;

	// calculate farm + mine score

	int farmMineTerraformingActionsCount = 2;
	int farmMineTerraformingActions[] = {FORMER_FARM, FORMER_MINE};

	double farmMineWeightedYield = getImprovedTileWeightedYield(&tileInfo, farmMineTerraformingActionsCount, farmMineTerraformingActions, nutrientWeight, mineralWeight, energyWeight);

	// calculate farm + collector score

	int farmSolarTerraformingActionsCount = 2;
	int farmSolarTerraformingActions[] = {FORMER_FARM, FORMER_SOLAR};

	double farmSolarWeightedYield = getImprovedTileWeightedYield(&tileInfo, farmSolarTerraformingActionsCount, farmSolarTerraformingActions, nutrientWeight, mineralWeight, energyWeight);

	// add best yield score

	score += max(farmMineWeightedYield, farmSolarWeightedYield);

	// discourage base radius overlap

	if (map_has_item(tileInfo.tile, TERRA_BASE_RADIUS))
	{
		score /= 2.0;
	}

	// return score

	return score;

}

