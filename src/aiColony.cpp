#include <algorithm>
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

	// summarize top scores from top down

	std::sort(tileScores.begin(), tileScores.end());
	std::reverse(tileScores.begin(), tileScores.end());

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

	score += 1.0 * (double)getFriendlyLandBorderedBaseRadiusTileCount(factionId, x, y);

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

	// select best weighted yield

	double bestWeightedYield = 0.0;

	if (map_rockiness(tileInfo.tile) == 2)
	{
		// mine

		{
			int actionsCount = 1;
			int actions[] = {FORMER_MINE};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

		// level, farm, mine

		{
			int actionsCount = 3;
			int actions[] = {FORMER_LEVEL_TERRAIN, FORMER_FARM, FORMER_MINE};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

		// level, farm, solar

		{
			int actionsCount = 3;
			int actions[] = {FORMER_LEVEL_TERRAIN, FORMER_FARM, FORMER_SOLAR};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

	}
	else
	{
		// farm, mine

		{
			int actionsCount = 2;
			int actions[] = {FORMER_FARM, FORMER_MINE};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

		// farm, solar

		{
			int actionsCount = 2;
			int actions[] = {FORMER_FARM, FORMER_SOLAR};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

	}

	// add best weighted yield to score

	score += bestWeightedYield;

	// discourage base radius overlap

	if (map_has_item(tileInfo.tile, TERRA_BASE_RADIUS))
	{
		score /= 2.0;
	}

	// return score

	return score;

}

