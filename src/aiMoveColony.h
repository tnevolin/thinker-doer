#pragma once

#include "game_wtp.h"

const int MAX_EXPANSION_RANGE = 10;

void moveColonyStrategy();
void analyzeBasePlacementSites();
double getBuildSiteRangeScore(MAP *tile);
double getBuildSitePlacementScore(MAP *tile);
double getTileQualityScore(MAP *tile);
bool isValidBuildSite(MAP *tile, int factionId);
bool isValidWorkSite(MAP *tile, int factionId);

