#pragma once

#include "game_wtp.h"

double getBuildLocationScore(int factionId, int x, int y, int range, int speed, int **pm_safety);
double getBuildLocationPlacementScore(int factionId, int x, int y);
double getBaseRadiusTileScore(MAP_INFO tileInfo);
Location findTransportLandBaseBuildLocation(int transportVehicleId);

