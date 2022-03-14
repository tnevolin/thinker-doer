#pragma once

#include "main.h"
#include "game.h"
#include "game_wtp.h"

const int MAX_PODPOP_DISTANCE = 10;

//struct COMBAT_ORDER
//{
//	int x = -1;
//	int y = -1;
//	int enemyAIId = -1;
//	int order = -1;
//	bool kill = false;
//};
//
struct VEHICLE_VALUE
{
	int id;
	double value;
	double damage;
};

struct VehicleRange
{
	int id;
	int range;
};

void moveCombatStrategy();
//void populateCombatLists();
bool compareDefenderVehicleRanges(const VehicleRange &vehicleRange1, const VehicleRange &vehicleRange2);
void baseNativeProtection();
void baseFactionProtection();
void moveScoutStrategy();
void moveLandScoutStrategy();
void moveSeaScoutStrategy();
void nativeCombatStrategy();
//void factionCombatStrategy();
void fightStrategy();
void popPods();
void popLandPods();
void popSeaPods();
int compareVehicleValue(VEHICLE_VALUE o1, VEHICLE_VALUE o2);
//int moveCombat(int id);
//int applyCombatOrder(int id, COMBAT_ORDER *combatOrder);
//void setMoveOrder(int vehicleId, int x, int y, int order);
//void setAttackOrder(int vehicleId, int enemyVehicleId);
//void setKillOrder(int vehicleId);
//int moveVehicle(int vehicleId, int x, int y);
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
bool isVehicleAvailable(int vehicleId, bool notInWarzone);

