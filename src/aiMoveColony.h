#pragma once

#include "game_wtp.h"

void moveColonyStrategy();
void moveLandColonyStrategy(int vehicleId);
void moveSeaColonyStrategy(int vehicleId);
double getBuildLocationScore(int colonyVehicleId, int x, int y);
double getBuildLocationPlacementScore(int factionId, int x, int y);
double getBaseRadiusTileScore(MAP_INFO *tileInfo);
Location findLandBaseBuildLocation(int colonyVehicleId);
Location findOceanBaseBuildLocation(int colonyVehicleId);
bool isValidBuildSite(int factionId, int triad, int x, int y);
bool isUnclaimedBuildSite(int factionId, int x, int y, int colonyVehicleId);

