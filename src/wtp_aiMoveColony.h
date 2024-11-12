#pragma once

#include <float.h>
#include "wtp_game.h"
#include "wtp_aiMoveFormer.h"

const int MAX_EXPANSION_RANGE = 10;

const int PLACEMENT_CLOSE_BASE_RANGE = 9;
const int PLACEMENT_CLOSE_TILE_RANGE = PLACEMENT_CLOSE_BASE_RANGE - 2;
const double PLACEMENT_COMBINED_DISTANCE[] = {6.0, 10.0};
const double PLACEMENT_ANGLE_SIN[] = {+0.0, +1.0};

// how good is to place new base relative to existing one
const int PLACEMENT_MAX_RANGE = 8;
const int PLACEMENT_QUALITY[PLACEMENT_MAX_RANGE + 1][PLACEMENT_MAX_RANGE + 1] =
	{
		{
			-9,	// 0, 0
			-9,	// 0, 1
			-9,	// 0, 2
			-1,	// 0, 3
			 0,	// 0, 4
			+2,	// 0, 5
			-2,	// 0, 6
			-2,	// 0, 7
			-2,	// 0, 8
		},
		{
			-9,	// 1, 0
			-9,	// 1, 1
			-9,	// 1, 2
			-1,	// 1, 3
			+2,	// 1, 4
			+2,	// 1, 5
			-2,	// 1, 6
			-2,	// 1, 7
			-1,	// 1, 8
		},
		{
			-9,	// 2, 0
			-9,	// 2, 1
			-9,	// 2, 2
			 0,	// 2, 3
			+2,	// 2, 4
			-1,	// 2, 5
			-2,	// 2, 6
			-2,	// 2, 7
			-1,	// 2, 8
		},
		{
			-9,	// 3, 0
			-9,	// 3, 1
			-9,	// 3, 2
			+2,	// 3, 3
			+2,	// 3, 4
			-2,	// 3, 5
			-2,	// 3, 6
			-1,	// 3, 7
			 0,	// 3, 8
		},
		{
			-9,	// 4, 0
			-9,	// 4, 1
			-9,	// 4, 2
			-9,	// 4, 3
			-2,	// 4, 4
			-2,	// 4, 5
			-2,	// 4, 6
			-1,	// 4, 7
			 0,	// 4, 8
		},
		{
			-9,	// 5, 0
			-9,	// 5, 1
			-9,	// 5, 2
			-9,	// 5, 3
			-9,	// 5, 4
			-2,	// 5, 5
			-2,	// 5, 6
			-1,	// 5, 7
			 0,	// 5, 8
		},
		{
			-9,	// 6, 0
			-9,	// 6, 1
			-9,	// 6, 2
			-9,	// 6, 3
			-9,	// 6, 4
			-9,	// 6, 5
			-2,	// 6, 6
			-1,	// 6, 7
			 0,	// 6, 8
		},
		{
			-9,	// 7, 0
			-9,	// 7, 1
			-9,	// 7, 2
			-9,	// 7, 3
			-9,	// 7, 4
			-9,	// 7, 5
			-9,	// 7, 6
			 0,	// 7, 7
			 0,	// 7, 8
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
			 0,	// 8, 8
		},
	}
;

struct YieldInfo
{
	MAP *tile;
	double score;
	double nutrientSurplus;
	double resourceScore;
};

struct TileExpansionInfo
{
	// expansion
	bool expansionRange = false;
	bool validBuildSite = false;
	bool validWorkTile = false;
	bool worked = false;
	Resource averageYield;
//	double relativeYieldScore = 0.0;
	double buildSiteBaseGain = 0.0;
	double buildSitePlacementScore = 0.0;
//	double buildSiteYieldScore = 0.0;
	double buildSiteScore = 0.0;
};

// expansion data (accessible from other modules)
extern std::vector<MAP *> buildSites;
extern std::vector<MAP *> availableBuildSites;
// access expansion data arrays
TileExpansionInfo &getTileExpansionInfo(MAP *tile);

// strategy
void moveColonyStrategy();
void populateExpansionData();
void analyzeBasePlacementSites();
double getBuildSiteBaseGain(MAP *buildSite);
//double getBuildSiteYieldScore(MAP *tile);
double getBuildSitePlacementScore(MAP *tile);
bool isValidBuildSite(MAP *tile, int factionId);
bool isValidWorkTile(MAP *baseTile, MAP *workTile);
int getBuildSiteNearestBaseRange(MAP *tile);
int getNearestColonyRange(MAP *tile);
int getExpansionRange(MAP *tile);
Resource getAverageTileYield(MAP *tile);
int getNearestEnemyBaseRange(MAP *tile);
std::vector<MAP *> getUnavailableBuildSites(MAP *buildSite);
double getBasePlacementLandUse(MAP *tile);
int getBasePlacementRadiusOverlap(MAP *tile);
double getEffectiveYieldScore(double nutrient, double mineral, double energy);
double getColonyTravelTimeCoefficient(double travelTime);

