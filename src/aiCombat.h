#pragma once

#include "main.h"
#include "game.h"
#include "game_wtp.h"

struct COMBAT_ORDER
{
	int x = -1;
	int y = -1;
	bool hold = false;
	int enemyAIId = -1;
};

struct VEHICLE_VALUE
{
	int id;
	double value;
	double damage;
};

void aiCombatStrategy();
void populateCombatLists();
void aiNativeCombatStrategy();
void popPods();
void attackNativeArtillery(int enemyVehicleId);
void attackNativeTower(int enemyVehicleId);
void attackVehicle(int enemyVehicleId);
int compareVehicleValue(VEHICLE_VALUE o1, VEHICLE_VALUE o2);
int moveCombat(int id);
int applyCombatOrder(int id, COMBAT_ORDER *combatOrder);
int applyMoveOrder(int id, COMBAT_ORDER *combatOrder);
int applyAttackOrder(int id, COMBAT_ORDER *combatOrder);
void setMoveOrder(int id, int x, int y, bool hold);
int processSeaExplorer(int vehicleId);
bool isHealthySeaExplorerInLandPort(int vehicleId);
int kickSeaExplorerFromLandPort(int vehicleId);
int killVehicle(int vehicleId);
int moveVehicle(int vehicleId, int x, int y);
MAP_INFO getNearestUnexploredConnectedOceanRegionTile(int factionId, int initialLocationX, int initialLocationY);
double getVehicleMoraleModifier(int vehicleId);
double getVehicleConventionalOffenseValue(int vehicleId);
double getVehicleConventionalDefenseValue(int vehicleId);
double getVehiclePsiOffenseValue(int vehicleId);
double getVehiclePsiDefenseValue(int vehicleId);
double getThreatLevel();
int getRangeToNearestActiveFactionBase(int x, int y);

