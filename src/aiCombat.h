#ifndef __AICOMBAT_H__
#define __AICOMBAT_H__

#include "main.h"

struct COMBAT_ORDER
{
	int enemyAIId;
};

struct VEHICLE_DAMAGE_VALUE
{
	int id;
	double damage;
	double value;
};

void aiCombatStrategy();
void populateCombatLists();
void aiNativeCombatStrategy();
void attackNativeArtillery(int vehicleId);
int compareVehicleDamageValue(VEHICLE_DAMAGE_VALUE o1, VEHICLE_DAMAGE_VALUE o2);
int enemyMoveCombat(int id);
int applyCombatOrder(int id, COMBAT_ORDER *combatOrder);
int applyAttackOrder(int id, COMBAT_ORDER *combatOrder);

#endif // __AICOMBAT_H__

