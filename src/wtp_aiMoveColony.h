#pragma once

#include <float.h>

#include "main.h"
#include "engine.h"
#include "game.h"
#include "wtp_game.h"

const int MAX_EXPANSION_RANGE = 10;

const int PLACEMENT_CLOSE_BASE_RANGE = 9;
const int PLACEMENT_CLOSE_TILE_RANGE = PLACEMENT_CLOSE_BASE_RANGE - 2;
const double PLACEMENT_COMBINED_DISTANCE[] = {6.0, 10.0};
const double PLACEMENT_ANGLE_SIN[] = {+0.0, +1.0};

int const PLACEMENT_CONGESTION_PROXIMITY_MAX = 17;
double const PLACEMENT_CONGESTION[PLACEMENT_CONGESTION_PROXIMITY_MAX + 1] =
{
	0.0,	//  0
	0.0,	//  1
	0.0,	//  2
	0.0,	//  3
	0.0,	//  4
	0.0,	//  5
	0.0,	//  6
	0.0,	//  7
	0.0,	//  8
	0.0,	//  9
	0.0,	// 10
	0.0,	// 11
	1.0,	// 12
	1.0,	// 13
	0.7,	// 14
	0.4,	// 15
	0.0,	// 16
	0.0,	// 17
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
double getBuildSiteOverlapScore(MAP *buildSite);
int getTileBaseOverlapCount(MAP *tile);
double getBuildSiteConnectionScore(MAP *buildSite);
double getBuildSiteLandUseScore(MAP *buildSite);
double getEffectiveYieldScore(double nutrient, double mineral, double energy);
double getColonyTravelTimeCoefficient(double travelTime);
void populateConcaveTiles();

