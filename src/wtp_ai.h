#pragma once

#include "main.h"
#include "engine.h"
#include "wtp_ai_game.h"

void aiFactionUpkeep(const int factionId);
void __cdecl wtp_mod_enemy_turn(int factionId);
void strategy(bool computer);
void executeTasks();

void populateAIData();
void populateGlobalVariables();
void populateTileInfos();
void populateTileInfoBaseRanges();
void populateRegionAreas();
void populatePlayerBaseIds();
void populatePlayerBaseRanges();
void populateFactionInfos();
void populateBaseInfos();

void populatePlayerGlobalVariables();
void populatePlayerFactionIncome();
void populateUnits();
void populateMaxMineralSurplus();
void populateVehicles();
void populateDangerZones();
void populateEnemyStacks();
void populateEmptyEnemyBaseTiles();
void populateBasePoliceData();

void populateEnemyBaseInfos();
void populateCombatEffects();
void populateEnemyBaseCaptureGains();
void populateEnemyBaseProtectorWeights();

void evaluateEnemyStacks();
void evaluateBaseDefense();
void evaluateDefense(MAP *tile, CombatData &combatData, double targetGain);
void evaluateBaseProbeDefense();

void designUnits();
void proposeMultiplePrototypes(int factionId, std::vector<VehChassis> chassisIds, std::vector<VehWeapon> weaponIds, std::vector<VehArmor> armorIds, std::vector<std::vector<VehAbl>> potentialAbilityIdSets, VehReactor reactor, VehPlan plan, char const *name);
void checkAndProposePrototype(int factionId, VehChassis chassisId, VehWeapon weaponId, VehArmor armorId, std::vector<VehAbl> abilityIdSet, VehReactor reactor, VehPlan plan, char const *name);

void vehicleKill(int vehicleId);

