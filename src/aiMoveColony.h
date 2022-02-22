#pragma once

#include "game_wtp.h"

const int MAX_EXPANSION_RANGE = 10;

struct ExpansionBaseInfo
{
};

struct ExpansionTileInfo
{
	bool accessible = false;
	bool immediatelyReachable = false;
	// expansion
	int expansionRange = -1;
	double qualityScore = 0.0;
	double buildScore = 0.0;
};

// expansion data operations
void setupExpansionData();
void cleanupExpansionData();
// access expansion data arrays
ExpansionBaseInfo *getExpansionBaseInfo(int baseId);
ExpansionTileInfo *getExpansionTileInfo(int mapIndex);
ExpansionTileInfo *getExpansionTileInfo(int x, int y);
ExpansionTileInfo *getExpansionTileInfo(MAP *tile);
// strategy
void moveColonyStrategy();
void analyzeBasePlacementSites();
double getBuildSiteYieldScore(MAP *tile);
double getBuildSiteRangeScore(MAP *tile);
double getBuildSitePlaceScore(MAP *tile);
double getTileQualityScore(MAP *tile);
bool isValidBuildSite(MAP *tile, int factionId);
bool isValidWorkSite(MAP *tile, int factionId);
bool isWithinExpansionRangeSameAssociation(int x, int y, int expansionRange);
bool isWithinExpansionRange(int x, int y, int expansionRange);
int getBaseRadiusTouchCount(int x, int y, int factionId);
int getBaseRadiusOverlapCount(int x, int y, int factionId);
int getNearestBaseRange(MAP *tile);
int getNearestColonyRange(MAP *tile);
int getExpansionRange(MAP *tile);

