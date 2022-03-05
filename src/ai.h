#pragma once

#include <string>
#include <vector>
#include <set>
#include "terranx.h"
#include "terranx_types.h"
#include "terranx_enums.h"

int aiFactionUpkeep(const int factionId);
void __cdecl modified_enemy_units_check(int factionId);
void strategy();
void populateAIData();
void analyzeGeography();
void populateGlobalVariables();
void populateBaseExposures();
void evaluateBaseExposures();
void evaluateBaseNativeDefenseDemands();
void evaluateDefenseDemand();
void populateWarzones();
void populateBasesAndTiles();
void designUnits();
void proposeMultiplePrototypes(int factionId, std::vector<int> chassisIds, std::vector<int> weaponIds, std::vector<int> armorIds, std::vector<int> abilitiesSets, int reactorId, int plan, char *name);
void checkAndProposePrototype(int factionId, int chassisId, int weaponId, int armorId, int abilities, int reactorId, int plan, char *name);
void obsoletePrototypes(int factionId, std::set<int> chassisIds, std::set<int> weaponIds, std::set<int> armorIds, std::set<int> abilityFlagsSet, std::set<int> reactors);

// --------------------------------------------------
// helper functions
// --------------------------------------------------

VEH *getVehicleByAIId(int aiId);
MAP *getNearestPod(int vehicleId);
int getNearestFactionBaseRange(int factionId, int x, int y);
int getNearestOtherFactionBaseRange(int factionId, int x, int y);
int getNearestBaseId(int x, int y, std::set<int> baseIds);
int getFactionBestPrototypedWeaponOffenseValue(int factionId);
int getFactionBestPrototypedArmorDefenseValue(int factionId);
double evaluateCombatStrength(double offenseValue, double defenseValue);
double evaluateOffenseStrength(double offenseValue);
double evaluateDefenseStrength(double DefenseValue);
bool isVehicleThreatenedByEnemyInField(int vehicleId);
bool isDestinationReachable(int vehicleId, int x, int y, bool immediatelyReachable);
int getRangeToNearestFactionAirbase(int x, int y, int factionId);
int getNearestEnemyBaseDistance(int baseId);
double getConventionalCombatBonusMultiplier(int attackerUnitId, int defenderUnitId);
std::string getAbilitiesString(int unitType);
bool isWithinAlienArtilleryRange(int vehicleId);
bool isUseWtpAlgorithms(int factionId);
int getCoastalBaseOceanAssociation(MAP *tile, int factionId);
bool isOceanBaseTile(MAP *tile, int factionId);
int getAssociation(MAP *tile, int factionId);
int getAssociation(int region, int factionId);
std::set<int> *getConnections(MAP *tile, int factionId);
std::set<int> *getConnections(int region, int factionId);
bool isSameAssociation(MAP *tile1, MAP *tile2, int factionId);
bool isSameAssociation(int extendedRegion1, MAP *tile2, int factionId);
bool isImmediatelyReachableAssociation(int association, int factionId);
bool isPotentiallyReachableAssociation(int association, int factionId);
int getExtendedRegion(MAP *tile);
bool isPolarRegion(MAP *tile);
int getVehicleAssociation(int vehicleId);
int getOceanAssociation(MAP *tile, int factionId);
int getBaseOceanAssociation(int baseId);
bool isOceanAssociationCoast(MAP *tile, int oceanAssociation, int factionId);
bool isOceanAssociationCoast(int x, int y, int oceanAssociation, int factionId);
int getMaxBaseSize(int factionId);
std::set<int> getBaseOceanRegions(int baseId);
int getPathDistance(int sourceX, int sourceY, int destinationX, int destinationY, int unitId, int factionId);
int getPathTravelTime(int sourceX, int sourceY, int destinationX, int destinationY, int unitId, int factionId);

