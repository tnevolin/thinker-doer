#pragma once

#include <memory>
#include "game_wtp.h"
#include "aiTransport.h"

void __cdecl modified_enemy_units_check(int factionId);
void aiStrategy(int factionId);
void analyzeGeography();
void setSharedOceanRegions();
void populateGlobalVariables();
void populateBaseExposures();
void evaluateBaseExposures();
void evaluateBaseNativeDefenseDemands();
void evaluateDefenseDemand();
void designUnits();
void proposeMultiplePrototypes(int factionId, std::vector<int> chassisIds, std::vector<int> weaponIds, std::vector<int> armorIds, std::vector<int> abilitiesSets, int reactorId, int plan, char *name);
void checkAndProposePrototype(int factionId, int chassisId, int weaponId, int armorId, int abilities, int reactorId, int plan, char *name);
void moveStrategy();
void updateGlobalVariables();
void moveAllStrategy();
void healStrategy();
int moveVehicle(const int vehicleId);

// ==================================================
// enemy_move entry point
// ==================================================

int __cdecl modified_enemy_move(const int vehicleId);

