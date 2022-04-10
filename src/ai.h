#pragma once

#include <string>
#include <vector>
#include <set>
#include "terranx.h"
#include "terranx_types.h"
#include "terranx_enums.h"

struct IdIntValue
{
	int id;
	int value;
};

struct IdDoubleValue
{
	int id;
	double value;
	
};

struct VehicleLocation
{
	int vehicleId;
	MAP *location;
};

struct VehicleLocationTravelTime
{
	int vehicleId;
	MAP *location;
	int travelTime;
};

int aiFactionUpkeep(const int factionId);
void __cdecl modified_enemy_units_check(int factionId);
void strategy();
void populateAIData();
void analyzeGeography();
void populateGlobalVariables();
void evaluateBaseBorderDistances();
void evaluateBaseNativeDefenseDemands();
void evaluateBasePoliceRequests();
void evaluateBaseDefense();
void populateWarzones();
void designUnits();
void proposeMultiplePrototypes(int factionId, std::vector<int> chassisIds, std::vector<int> weaponIds, std::vector<int> armorIds, std::vector<int> abilitiesSets, int reactorId, int plan, char *name);
void checkAndProposePrototype(int factionId, int chassisId, int weaponId, int armorId, int abilities, int reactorId, int plan, char *name);
void obsoletePrototypes(int factionId, std::set<int> chassisIds, std::set<int> weaponIds, std::set<int> armorIds, std::set<int> abilityFlagsSet, std::set<int> reactors);

// --------------------------------------------------
// helper functions
// --------------------------------------------------

VEH *getVehicleByAIId(int aiId);
MAP *getClosestPod(int vehicleId);
int getNearestFactionBaseRange(int factionId, int x, int y);
int getNearestOtherFactionBaseRange(int factionId, int x, int y);
int getNearestBaseId(int x, int y, std::set<int> baseIds);
int getFactionBestOffenseValue(int factionId);
int getFactionBestDefenseValue(int factionId);
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
int getPathDistance(int srcX, int srcY, int dstX, int dstY, int unitId, int factionId);
int getVehiclePathDistance(int vehicleId, int x, int y);
int getPathMovementCost(int srcX, int srcY, int dstX, int dstY, int unitId, int factionId, bool excludeDestination);
int getVehiclePathMovementCost(int vehicleId, int x, int y, bool excludeDestination);
int getPathTravelTime(int sourceX, int sourceY, int destinationX, int destinationY, int unitId, int factionId);
int getVehiclePathTravelTime(int vehicleId, int x, int y);
std::vector<VehicleLocation> matchVehiclesToUnrankedLocations(std::vector<int> vehicleIds, std::vector<MAP *> locations);
std::vector<VehicleLocation> matchVehiclesToRankedLocations(std::vector<int> vehicleIds, std::vector<MAP *> locations);
bool compareIdDoubleValueAscending(const IdDoubleValue &a, const IdDoubleValue &b);
bool compareIdDoubleValueDescending(const IdDoubleValue &a, const IdDoubleValue &b);
bool canUnitAttack(int attackerUnitId, int defenderUnitId);
bool canVehicleAttack(int attackerVehicleId, int defenderVehicleId);
double getRelativeUnitStrength(int attackerUnitId, int defenderUnitId);
double getVehiclePsiOffenseModifier(int vehicleId);
double getVehiclePsiDefenseModifier(int vehicleId);
double getVehicleConventionalOffenseModifier(int vehicleId);
double getVehicleConventionalDefenseModifier(int vehicleId);
double getVehicleToBaseTravelTimeCoefficient(int vehicleId, int baseId);
double getVehicleToBaseDepthCoefficient(int vehicleId, int baseId);

