#pragma once

#include <float.h>
#include "game_wtp.h"

const int MAX_EXPANSION_RANGE = 10;

const int PLACEMENT_MAX_RANGE = 8;
const int PLACEMENT_QUALITY[PLACEMENT_MAX_RANGE + 1][PLACEMENT_MAX_RANGE + 1] =
	{
		{
			-9,	// 0, 0
			-9,	// 0, 1
			-9,	// 0, 2
			-4,	// 0, 3
			+0,	// 0, 4
			+2,	// 0, 5
			-4,	// 0, 6
			-3,	// 0, 7
			+0,	// 0, 8
		},
		{
			-9,	// 1, 0
			-9,	// 1, 1
			-9,	// 1, 2
			-3,	// 1, 3
			+4,	// 1, 4
			+1,	// 1, 5
			-4,	// 1, 6
			-2,	// 1, 7
			+0,	// 1, 8
		},
		{
			-9,	// 2, 0
			-9,	// 2, 1
			-4,	// 2, 2
			-3,	// 2, 3
			+4,	// 2, 4
			+0,	// 2, 5
			-3,	// 2, 6
			+0,	// 2, 7
			+0,	// 2, 8
		},
		{
			-9,	// 3, 0
			-9,	// 3, 1
			-9,	// 3, 2
			+4,	// 3, 3
			+2,	// 3, 4
			-2,	// 3, 5
			+0,	// 3, 6
			+0,	// 3, 7
			+0,	// 3, 8
		},
		{
			-9,	// 4, 0
			-9,	// 4, 1
			-9,	// 4, 2
			-9,	// 4, 3
			-4,	// 4, 4
			-4,	// 4, 5
			-2,	// 4, 6
			+0,	// 4, 7
			+0,	// 4, 8
		},
		{
			-9,	// 5, 0
			-9,	// 5, 1
			-9,	// 5, 2
			-9,	// 5, 3
			-9,	// 5, 4
			-4,	// 5, 5
			+0,	// 5, 6
			+0,	// 5, 7
			+0,	// 5, 8
		},
		{
			-9,	// 6, 0
			-9,	// 6, 1
			-9,	// 6, 2
			-9,	// 6, 3
			-9,	// 6, 4
			-9,	// 6, 5
			-2,	// 6, 6
			+0,	// 6, 7
			+0,	// 6, 8
		},
		{
			-9,	// 7, 0
			-9,	// 7, 1
			-9,	// 7, 2
			-9,	// 7, 3
			-9,	// 7, 4
			-9,	// 7, 5
			-9,	// 7, 6
			+0,	// 7, 7
			+0,	// 7, 8
		},
		{
			-9,	// 8, 0
			-9,	// 8, 1
			-9,	// 8, 2
			-9,	// 8, 3
			-9,	// 8, 4
			-9,	// 8, 5
			-9,	// 8, 6
			-9,	// 8, 7
			+0,	// 8, 8
		},
	}
;

struct ExpansionBaseInfo
{
};

struct ExpansionTileInfo
{
	bool accessible = false;
	bool immediatelyReachable = false;
	// expansion
	int expansionRange = -1;
	double yieldScore = 0.0;
	double buildScore = 0.0;
};

// expansion data
extern std::vector<MAP *> buildSites;
// expansion data operations
void setupExpansionData();
// access expansion data arrays
ExpansionBaseInfo *getExpansionBaseInfo(int baseId);
ExpansionTileInfo *getExpansionTileInfo(int mapIndex);
ExpansionTileInfo *getExpansionTileInfo(int x, int y);
ExpansionTileInfo *getExpansionTileInfo(MAP *tile);

// strategy
void moveColonyStrategy();
void analyzeBasePlacementSites();
double getBuildSiteYieldScore(MAP *tile);
double getBuildSiteTravelTimeScore(double travelTime);
double getBuildSitePlacementScore(MAP *tile);
bool isValidBuildSite(MAP *tile, int factionId);
bool isValidWorkSite(MAP *tile, int factionId);
bool isWithinExpansionRangeSameAssociation(int x, int y, int expansionRange);
bool isWithinExpansionRange(int x, int y, int expansionRange);
int getBaseRadiusTouchCount(int x, int y, int factionId);
int getBaseRadiusOverlapCount(int x, int y, int factionId);
int getBuildSiteNearestBaseRange(MAP *tile);
int getNearestColonyRange(MAP *tile);
int getExpansionRange(MAP *tile);
double estimateTravelTime(MAP *src, MAP *dst, int unitId);
double estimateVehicleTravelTime(int vehicleId, MAP *destination);
double getMinimalYieldScore();
double getSeaSquareFutureYieldScore();
double getTileFutureYieldScore(MAP *tile);
double getTerraformingOptionFutureYieldScore(MAP *tile, MAP_STATE *currentMapState, MAP_STATE *improvedMapState);
int getNearestEnemyBaseRange(MAP *tile);
double getYieldScore(double nutrient, double mineral, double energy);
std::vector<MAP *> getUnavailableBuildSites(MAP *buildSite);

