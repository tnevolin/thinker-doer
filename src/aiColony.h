#pragma once

#include "game_wtp.h"

int moveColony(int vehicleId);
int moveLandColony(int vehicleId);
int moveSeaColony(int vehicleId);
int moveLandColonyOnLand(int vehicleId);
int moveSeaColonyOnOcean(int vehicleId);
double getBuildLocationScore(int colonyVehicleId, int x, int y);
double getBuildLocationPlacementScore(int factionId, int x, int y);
double getBaseRadiusTileScore(MAP_INFO tileInfo);
Location findLandBaseBuildLocation(int colonyVehicleId);
Location findOceanBaseBuildLocation(int colonyVehicleId);
Location findTransportLandBaseBuildLocation(int transportVehicleId);
bool isValidBuildSite(int factionId, int triad, int x, int y);
bool isUnclaimedBuildSite(int factionId, int x, int y, int colonyVehicleId);

