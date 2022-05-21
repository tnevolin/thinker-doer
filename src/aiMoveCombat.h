#pragma once

#include "main.h"
#include "game.h"
#include "game_wtp.h"

const int MAX_PODPOP_DISTANCE = 10;

struct VehicleRange
{
	int id;
	int range;
};

void moveCombatStrategy();
void movePolice2x();
void moveProtector();
void moveAlienProtector();
void moveScoutStrategy();
void moveLandScoutStrategy();
void moveSeaScoutStrategy();
void nativeCombatStrategy();
void fightStrategy();
void popPods();
void popLandPods();
void popSeaPods();
void synchronizeAttack();
MAP *getNearestUnexploredConnectedOceanRegionLocation(int factionId, int initialLocationX, int initialLocationY);
double getVehicleConventionalOffenseValue(int vehicleId);
double getVehicleConventionalDefenseValue(int vehicleId);
double getVehiclePsiOffenseValue(int vehicleId);
double getVehiclePsiDefenseValue(int vehicleId);
int getRangeToNearestActiveFactionBase(int x, int y);
MAP *getNearestMonolith(int x, int y, int triad);
MAP *getNearestBase(int vehicleId);
bool isInWarZone(int vehicleId);
std::vector<int> getReachableEnemies(int vehicleId);
bool isVehicleAvailable(int vehicleId, bool notAvailableInWarzone);
bool isVehicleAvailable(int vehicleId);
void findSureKill(int vehicleId);
int getVehicleProtectionRange(int vehicleId);

