#pragma once

#include <float.h>
#include "wtp_game.h"
#include "wtp_aiMoveFormer.h"

const int MAX_EXPANSION_RANGE = 10;

const int PLACEMENT_CLOSE_BASE_RANGE = 9;
const int PLACEMENT_CLOSE_TILE_RANGE = PLACEMENT_CLOSE_BASE_RANGE - 2;
const double PLACEMENT_COMBINED_DISTANCE[] = {6.0, 10.0};
const double PLACEMENT_ANGLE_SIN[] = {+0.0, +1.0};

struct PlacementConfiguration
{
	double multiplier;
	std::vector<std::array<int, 2>> gaps;
};
const int PLACEMENT_CONFIGURATION_MAX_RANGE = 7;
PlacementConfiguration const PLACEMENT_CONFIGURATIONS[PLACEMENT_CONFIGURATION_MAX_RANGE + 1][PLACEMENT_CONFIGURATION_MAX_RANGE + 1] =
{
	{	// 0
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
	},
	{	// 1
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
	},
	{	// 2
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
	},
	{	// 3
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
	},
	{	// 4
		{0.1, {{+2,+2},{+2,-2},}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
		{1.0, {{+2,+2},}},
		{0.0, {}},
		{0.0, {}},
	},
	{	// 5
		{0.0, {}},
		{0.1, {{+2,+2},{+3,-1},}},
		{0.3, {{+2,+2},{+3, 0},}},
		{0.5, {{+2,+2},{+3,+1},}},
		{1.0, {{+2,+2},{+3,+2},}},
		{1.0, {{+2,+2},{+3,+2},{+2,+3},{+3,+3},}},
		{0.0, {}},
		{0.0, {}},
	},
	{	// 6
		{1.0, {{+3,+1},{+3, 0},{+3,-1},}},
		{1.0, {{+3,+1},{+3, 0},}},
		{1.0, {{+3,+1},}},
		{0.3, {{+2,+2},{+3,+2},}},
		{0.5, {{+2,+2},{+3,+2},{+4,+2},}},
		{0.3, {{+2,+2},{+3,+2},{+4,+2},{+2,+3},{+3,+3},{+4,+3},}},
		{0.1, {{+2,+2},{+3,+2},{+4,+2},{+2,+3},{+3,+3},{+4,+3},{+2,+4},{+3,+4},{+4,+4},}},
		{0.0, {}},
	},
	{	// 7
		{0.5, {{+2,+1},{+3,+1},{+2, 0},{+3, 0},{+2,-1},{+3,-1},}},
		{0.5, {{+2,+1},{+3,+1},{+2, 0},{+3, 0},}},
		{0.3, {{+3,+1},{+3,+2},}},
		{0.1, {{+2,+2},{+3,+2},{+4,+2},{+3,+1},{+4,+1},{+5,+1},}},
		{0.1, {{+2,+2},{+3,+2},{+4,+2},{+5,+2},}},
		{0.0, {}},
		{0.0, {}},
		{0.0, {}},
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
double getBasePlacementLandUse(MAP *buildSite);
int getBasePlacementRadiusOverlap(MAP *tile);
double getEffectiveYieldScore(double nutrient, double mineral, double energy);
double getColonyTravelTimeCoefficient(double travelTime);

