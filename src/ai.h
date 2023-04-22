#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include "aiData.h"
#include "terranx.h"
#include "terranx_types.h"
#include "terranx_enums.h"

const int MAX_SAFE_LOCATION_SEARCH_RANGE = 10;
const int STACK_MAX_BASE_RANGE = 20;
const int STACK_MAX_COUNT = 20;
const int MAX_PROXIMITY_RANGE = 4;

// base movement impediment effect range and coefficient
const int BASE_MOVEMENT_IMPEDIMENT_MAX_RANGE = 3;
const double BASE_TRAVEL_IMPEDIMENT_COEFFICIENT = 2.0;
// number of moves to compute around the base
const double BASE_TRAVEL_TIME_PRECOMPUTATION_LIMIT = 10.0;

//// forward declarations
//
//struct StrengthType;
//struct Force;
//struct ATTACK_TYPE;
//
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

/*
Information about defend economic effect and remaining health.
*/
struct VehicleDefendInfo
{
	double economicEffect;
	double relativeDamage;
};

const int BASE_DEFENSE_TYPE_COUNT = 4;
enum BASE_DEFENSE_TYPE
{
	BD_LAND,
	BD_SEA,
	BD_AIR,
	BD_NATIVE,
};

int aiFactionUpkeep(const int factionId);
void __cdecl modified_enemy_units_check(int factionId);
void strategy();

void executeTasks();
void populateAIData();
void populateGlobalVariables();
void populateGeographyData();
void populateFactionData();
void populateMovementData();
void analyzeGeography();
void populateFactionInfos();
void populateBases();
void populateOurBases();
void populateBaseBorderDistances();
void populateFactionCombatModifiers();
void populateUnits();
void populateMaxMineralSurplus();
void populateFactionAirbases();
void populateVehicles();
void populateNetworkCoverage();
void populateShipyardsAndSeaTransports();
void populateReachableAssociations();
void populateWarzones();
void populateBaseEnemyMovementCosts();
void populateEnemyStacks();
void populateEmptyEnemyBaseTiles();
void populateEnemyBaseOceanAssociations();
void populateEnemyBaseOceanAssociations();

void populateEnemyMovementInfos();
void populateMovementInfos();
void populateTransfers();
void populateLandClusters();

void evaluateBasePoliceRequests();
void evaluateBaseDefense();

void populateEnemyOffenseThreat();
void designUnits();
void proposeMultiplePrototypes(int factionId, std::vector<int> chassisIds, std::vector<int> weaponIds, std::vector<int> armorIds, std::vector<std::vector<int>> abilitiesSets, int reactor, int plan, char *name);
void checkAndProposePrototype(int factionId, int chassisId, int weaponId, int armorId, std::vector<int> abilityIds, int reactor, int plan, char *name);
void obsoletePrototypes(int factionId, std::set<int> chassisIds, std::set<int> weaponIds, std::set<int> armorIds, std::set<int> abilityFlagsSet, std::set<int> reactors);

// --------------------------------------------------
// helper functions
// --------------------------------------------------

int getVehicleIdByAIId(int aiId);
VEH *getVehicleByAIId(int aiId);
MAP *getClosestPod(int vehicleId);
int getNearestAIFactionBaseRange(int x, int y);
int getNearestBaseRange(MAP *tile, std::vector<int> baseIds, bool sameRealm);
int getFactionBestOffenseValue(int factionId);
int getFactionBestDefenseValue(int factionId);
double evaluateCombatStrength(double offenseValue, double defenseValue);
double evaluateOffenseStrength(double offenseValue);
double evaluateDefenseStrength(double DefenseValue);
bool isVehicleThreatenedByEnemyInField(int vehicleId);
bool isUnitDestinationReachable(int factionId, int unitId, MAP *origin, MAP *destination, bool immediatelyReachable);
bool isUnitDestinationArtilleryReachable(int factionId, int unitId, MAP *origin, MAP *destination, bool immediatelyReachable);
bool isVehicleDestinationReachable(int vehicleId, MAP *destination, bool immediatelyReachable);
bool isVehicleDestinationArtilleryReachable(int vehicleId, MAP *destination, bool immediatelyReachable);
int getNearestEnemyBaseDistance(int baseId);
double getConventionalCombatBonusMultiplier(int attackerUnitId, int defenderUnitId);
std::string getAbilitiesString(int unitType);
bool isWithinAlienArtilleryRange(int vehicleId);
bool isWtpEnabledFaction(int factionId);
int getCoastalBaseOceanAssociation(MAP *tile, int factionId);
int getExtendedRegionAssociation(int extendedRegion, int factionId);
int getAssociation(MAP *tile, int factionId);
std::set<int> &getAssociationConnections(int association, int factionId);
std::set<int> &getTileAssociationConnections(MAP *tile, int factionId);
bool isConnected(int association1, int association2, int factionId);
bool isConnected(MAP *tile1, MAP *tile2, int factionId);
bool isSameAssociation(MAP *tile1, MAP *tile2, int factionId);
bool isSameLandAssociation(MAP *tile1, MAP *tile2, int factionId);
bool isSameOceanAssociation(MAP *tile1, MAP *tile2, int factionId);
bool isImmediatelyReachableAssociation(int association, int factionId);
bool isImmediatelyReachable(MAP *tile, int factionId);
bool isPotentiallyReachableAssociation(int association, int factionId);
bool isPotentiallyReachable(MAP *tile, int factionId);
int getExtendedRegion(MAP *tile);
bool isPolarRegion(MAP *tile);
bool getEdgeRange(MAP *tile);
bool isEdgeTile(MAP *tile);
int getVehicleAssociation(int vehicleId);
int getLandAssociation(MAP *tile, int factionId);
int getOceanAssociation(MAP *tile, int factionId);
int getSurfaceAssociation(MAP *tile, int factionId, bool ocean);
int getBaseLandAssociation(int baseId);
int getBaseOceanAssociation(int baseId);
bool isOceanAssociationCoast(MAP *tile, int oceanAssociation, int factionId);
bool isOceanAssociationCoast(int x, int y, int oceanAssociation, int factionId);
int getMaxBaseSize(int factionId);
std::set<int> getBaseOceanRegions(int baseId);
int getPathMovementCost(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile);
int getPathTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed);
int getUnitTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed);
int getGravshipUnitTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed);
int getAirRangedUnitTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed);
int getSeaUnitTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed);
int getLandUnitTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed);
int getVehicleTravelTime(int vehicleId, MAP *origin, MAP *target, bool ignoreHostile, int action);
int getVehicleTravelTime(int vehicleId, MAP *destination, bool ignoreHostile, int action);
int getVehicleTravelTime(int vehicleId, MAP *destination);
int getVehicleMeleeAttackTravelTime(int vehicleId, MAP *target, bool ignoreHostile);
int getVehicleArtilleryAttackTravelTime(int vehicleId, MAP *attackTarget, bool ignoreHostile);
int getVehicleTravelTimeByTaskType(int vehicleId, MAP *target, TaskType taskType);
std::vector<VehicleLocation> matchVehiclesToUnrankedLocations(std::vector<int> vehicleIds, std::vector<MAP *> locations);
std::vector<VehicleLocation> matchVehiclesToRankedLocations(std::vector<int> vehicleIds, std::vector<MAP *> locations);
bool compareIdIntValueAscending(const IdIntValue &a, const IdIntValue &b);
bool compareIdIntValueDescending(const IdIntValue &a, const IdIntValue &b);
bool compareIdDoubleValueAscending(const IdDoubleValue &a, const IdDoubleValue &b);
bool compareIdDoubleValueDescending(const IdDoubleValue &a, const IdDoubleValue &b);
double getMeleeRelativeUnitStrength(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId);
double getArtilleryDuelRelativeUnitStrength(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId);
double getBombardmentRelativeUnitStrength(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId);
double getUnitBombardmentDamage(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId);
double getVehicleBombardmentDamage(int attackerVehicleId, int defenderVehicleId);
double getVehiclePsiOffensePowerModifier(int vehicleId);
double getVehiclePsiDefenseModifier(int vehicleId);
double getVehicleConventionalOffenseModifier(int vehicleId);
double getVehicleConventionalDefenseModifier(int vehicleId);
MAP *getSafeLocation(int vehicleId);
MAP *getSafeLocation(int vehicleId, int baseRange);
MapValue findClosestItemLocation(int vehicleId, terrain_flags item, int maxSearchRange, bool avoidWarzone);
double getAverageSensorMultiplier(MAP *tile);
MAP *getNearestAirbase(int factionId, MAP *tile);
int getNearestAirbaseRange(int factionId, MAP *tile);
bool isOffensiveUnit(int unitId);
bool isOffensiveVehicle(int vehicleId);
double getExponentialCoefficient(double scale, double travelTime);
void modifiedAllocatePsych(int faction);
int getBaseItemBuildTime(int baseId, int item, bool accumulatedMinerals);
int getBaseItemBuildTime(int baseId, int item);
double getStatisticalBaseSize(double age);
double getBaseEstimatedSize(double currentSize, double currentAge, double futureAge);
double getBaseStatisticalWorkerCount(double age);
double getBaseStatisticalProportionalWorkerCountIncrease(double currentAge, double futureAge);
int getBaseProjectedSize(int baseId, int turns);
int getBaseGrowthTime(int baseId, int targetSize);
int getBaseOptimalDoctors(int baseId);
double getResourceScore(double minerals, double energy);
void populatePathInfoParameters();
void populateHexCosts();
void setBaseFoundationYear(int baseId);
int getBaseAge(int baseId);
double getBaseSizeGrowth(int baseId);
double getBonusDelayEffectCoefficient(double delay);
double getBuildTimeEffectCoefficient(double delay);
int getUnitGroup(int unitId, bool airbase);
double getVehicleStrenghtMultiplier(int vehicleId);
double getUnitMeleeAttackEffect(int ownFactionId, int ownUnitId, int foeVehicleId, MAP *tile);
double getUnitArtilleryDuelAttackEffect(int ownFactionId, int ownUnitId, int foeVehicleId, MAP *tile);
double getUnitBombardmentAttackEffect(int ownFactionId, int ownUnitId, int foeVehicleId, MAP *tile);
double getUnitBaseProtectionEffect(int ownFactionId, int ownUnitId, int foeFactionId, int foeUnitId, MAP *tile);
double getTileDefenseMultiplier(MAP *tile, int factionId);
double getHarmonicMean(std::vector<std::vector<double>> parameters);
bool compareMovementActions(MovementAction &o1, MovementAction &o2);
std::vector<MovementAction> getVehicleReachableLocations(int vehicleId);
std::vector<MovementAction> getVehicleMeleeAttackLocations(int vehicleId);
std::vector<MovementAction> getVehicleArtilleryAttackPositions(int vehicleId);
double getVehicleTileDanger(int vehicleId, MAP *tile);
bool isVehicleAllowedMove(int vehicleId, MAP *from, MAP *to, bool ignoreEnemy);
std::vector<MAP *> getVehicleThreatenedTiles(int vehicleId);
std::vector<int> &getAssociationSeaTransports(int association, int factionId);
std::vector<int> &getAssociationShipyards(int association, int factionId);
void assignVehiclesToTransports();
void joinAssociations(std::map<int, int> &associations, std::map<int, std::set<int>> &connections);
int getBestSeaCombatSpeed(int factionId);
int getSeaTransportUnitId(int oceanAssociation, int factionId, bool existing);
int getSeaTransportChassisSpeed(int oceanAssociation, int factionId, bool existing);
int getBestSeaTransportSpeed(int factionId);
MAP *getClosestTargetLocation(int factionId, MAP *origin, MAP *target, int proximity, int triad, bool ignoreHostile);
void disbandOrversupportedVehicles(int factionId);
void storeBaseSetProductionItems();
MAP *getVehicleMeleeAttackPosition(int vehicleId, MAP *target, bool ignoreHostile);
MAP *getVehicleArtilleryAttackPosition(int vehicleId, MAP *target, bool ignoreHostile);
bool isUnitCanCaptureBase(int factionId, int unitId, MAP *origin, MAP *baseTile);
bool isVehicleCanCaptureBase(int vehicleId, MAP *baseTile);
bool isUnitCanMeleeAttackStack(int factionId, int unitId, MAP *origin, MAP *enemyStackTile, EnemyStackInfo &enemyStackInfo);
bool isUnitCanArtilleryAttackStack(int factionId, int unitId, MAP *origin, MAP *enemyStackTile, EnemyStackInfo &enemyStackInfo);
double getVehicleDestructionPriority(int vehicleId);
bool isBaseSetProductionProject(int baseId);
int getUnitTrueCost(int unitId, bool combat);
double getVehicleApproachChance(int baseId, int vehicleId);
double getBaseTravelImpediment(int range, bool oceanBase);

