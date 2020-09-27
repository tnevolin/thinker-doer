#ifndef __AICOMBAT_H__
#define __AICOMBAT_H__

#include "main.h"

struct COMBAT_ORDER
{
	int defendX = -1;
	int defendY = -1;
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
void attackNativeArtillery(int vehicleId);
int compareVehicleValue(VEHICLE_VALUE o1, VEHICLE_VALUE o2);
int enemyMoveCombat(int id);
int applyCombatOrder(int id, COMBAT_ORDER *combatOrder);
int applyDefendOrder(int id, int x, int y);
int applyAttackOrder(int id, COMBAT_ORDER *combatOrder);
void setDefendOrder(int id, int x, int y);
bool isHealthySeaExplorerInLandPort(int vehicleId);
int kickSeaExplorerFromLandPort(int vehicleId);

#endif // __AICOMBAT_H__

