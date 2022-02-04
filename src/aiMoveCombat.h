#pragma once

#include "main.h"
#include "game.h"
#include "game_wtp.h"

struct COMBAT_ORDER
{
	int x = -1;
	int y = -1;
	int enemyAIId = -1;
	int order = -1;
	bool kill = false;
};

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
void populateCombatLists();
bool compareDefenderVehicleRanges(const VehicleRange &vehicleRange1, const VehicleRange &vehicleRange2);
void baseProtection();
void moveScoutStrategy();
void moveLandScoutStrategy();
void moveSeaScoutStrategy();
void nativeCombatStrategy();
void factionCombatStrategy();
void fightStrategy();
void popPods();
void popLandPods();
void popOceanPods();
int compareVehicleValue(VEHICLE_VALUE o1, VEHICLE_VALUE o2);
int moveCombat(int id);
int applyCombatOrder(int id, COMBAT_ORDER *combatOrder);
int applyMoveOrder(int id, COMBAT_ORDER *combatOrder);
int applyAttackOrder(int id, COMBAT_ORDER *combatOrder);
void setMoveOrder(int vehicleId, int x, int y, int order);
void setAttackOrder(int vehicleId, int enemyVehicleId);
void setKillOrder(int vehicleId);
int processSeaExplorer(int vehicleId);
bool isHealthySeaExplorerInLandPort(int vehicleId);
int kickSeaExplorerFromLandPort(int vehicleId);
int killVehicle(int vehicleId);
int moveVehicle(int vehicleId, int x, int y);
MAP_INFO getNearestUnexploredConnectedOceanRegionTile(int factionId, int initialLocationX, int initialLocationY);
double getVehicleConventionalOffenseValue(int vehicleId);
double getVehicleConventionalDefenseValue(int vehicleId);
double getVehiclePsiOffenseValue(int vehicleId);
double getVehiclePsiDefenseValue(int vehicleId);
int getRangeToNearestActiveFactionBase(int x, int y);
Location getNearestMonolithLocation(int x, int y, int triad);
Location getNearestBaseLocation(int x, int y, int triad);
bool isInWarZone(int vehicleId);
void countTransportableVehicles();
std::vector<int> getReachableEnemies(int vehicleId);
bool isVehicleAvailable(int vehicleId);

