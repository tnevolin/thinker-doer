#pragma once

#include "main.h"
#include <vector>
#include <set>
#include <map>
#include "robin_hood.h"
#include "engine.h"

struct MapValue
{
	MAP *tile = nullptr;
	int value = -1;

	MapValue(MAP *_tile, int _value);

};

struct Location
{
	int x;
	int y;
};
bool operator==(const Location &o1, const Location &o2);
bool operator!=(const Location &o1, const Location &o2);

struct RepairInfo
{
	bool full;
	int damage;
	int time;
};

struct Budget
{
	int economy;
	int psych;
	int labs;
};

// =======================================================
// tile offsets
// =======================================================

const int RANGE_OFFSET_COUNT[] = {1, 9, 25, 49, 81, 121, 169, 225, 289};
const int ANGLE_COUNT = RANGE_OFFSET_COUNT[1] - RANGE_OFFSET_COUNT[0];
const int OFFSETS[289][2] =
{
	{  0,  0},
	{  1, -1},
	{  2,  0},
	{  1,  1},
	{  0,  2},
	{ -1,  1},
	{ -2,  0},
	{ -1, -1},
	{  0, -2},
	{  2, -2},
	{  2,  2},
	{ -2,  2},
	{ -2, -2},
	{  1, -3},
	{  3, -1},
	{  3,  1},
	{  1,  3},
	{ -1,  3},
	{ -3,  1},
	{ -3, -1},
	{ -1, -3},
	{  4,  0},
	{ -4,  0},
	{  0,  4},
	{  0, -4},
	{  1, -5},
	{  2, -4},
	{  3, -3},
	{  4, -2},
	{  5, -1},
	{  5,  1},
	{  4,  2},
	{  3,  3},
	{  2,  4},
	{  1,  5},
	{ -1,  5},
	{ -2,  4},
	{ -3,  3},
	{ -4,  2},
	{ -5,  1},
	{ -5, -1},
	{ -4, -2},
	{ -3, -3},
	{ -2, -4},
	{ -1, -5},
	{  0,  6},
	{  6,  0},
	{  0, -6},
	{ -6,  0},
	{  0, -8},
	{  1, -7},
	{  2, -6},
	{  3, -5},
	{  4, -4},
	{  5, -3},
	{  6, -2},
	{  7, -1},
	{  8,  0},
	{  7,  1},
	{  6,  2},
	{  5,  3},
	{  4,  4},
	{  3,  5},
	{  2,  6},
	{  1,  7},
	{  0,  8},
	{ -1,  7},
	{ -2,  6},
	{ -3,  5},
	{ -4,  4},
	{ -5,  3},
	{ -6,  2},
	{ -7,  1},
	{ -8,  0},
	{ -7, -1},
	{ -6, -2},
	{ -5, -3},
	{ -4, -4},
	{ -3, -5},
	{ -2, -6},
	{ -1, -7},
	{  0,-10},
	{  1, -9},
	{  2, -8},
	{  3, -7},
	{  4, -6},
	{  5, -5},
	{  6, -4},
	{  7, -3},
	{  8, -2},
	{  9, -1},
	{ 10,  0},
	{  9,  1},
	{  8,  2},
	{  7,  3},
	{  6,  4},
	{  5,  5},
	{  4,  6},
	{  3,  7},
	{  2,  8},
	{  1,  9},
	{  0, 10},
	{ -1,  9},
	{ -2,  8},
	{ -3,  7},
	{ -4,  6},
	{ -5,  5},
	{ -6,  4},
	{ -7,  3},
	{ -8,  2},
	{ -9,  1},
	{-10,  0},
	{ -9, -1},
	{ -8, -2},
	{ -7, -3},
	{ -6, -4},
	{ -5, -5},
	{ -4, -6},
	{ -3, -7},
	{ -2, -8},
	{ -1, -9},
	{  0,-12},
	{  1,-11},
	{  2,-10},
	{  3, -9},
	{  4, -8},
	{  5, -7},
	{  6, -6},
	{  7, -5},
	{  8, -4},
	{  9, -3},
	{ 10, -2},
	{ 11, -1},
	{ 12,  0},
	{ 11,  1},
	{ 10,  2},
	{  9,  3},
	{  8,  4},
	{  7,  5},
	{  6,  6},
	{  5,  7},
	{  4,  8},
	{  3,  9},
	{  2, 10},
	{  1, 11},
	{  0, 12},
	{ -1, 11},
	{ -2, 10},
	{ -3,  9},
	{ -4,  8},
	{ -5,  7},
	{ -6,  6},
	{ -7,  5},
	{ -8,  4},
	{ -9,  3},
	{-10,  2},
	{-11,  1},
	{-12,  0},
	{-11, -1},
	{-10, -2},
	{ -9, -3},
	{ -8, -4},
	{ -7, -5},
	{ -6, -6},
	{ -5, -7},
	{ -4, -8},
	{ -3, -9},
	{ -2,-10},
	{ -1,-11},
	{  0,-14},
	{  1,-13},
	{  2,-12},
	{  3,-11},
	{  4,-10},
	{  5, -9},
	{  6, -8},
	{  7, -7},
	{  8, -6},
	{  9, -5},
	{ 10, -4},
	{ 11, -3},
	{ 12, -2},
	{ 13, -1},
	{ 14,  0},
	{ 13,  1},
	{ 12,  2},
	{ 11,  3},
	{ 10,  4},
	{  9,  5},
	{  8,  6},
	{  7,  7},
	{  6,  8},
	{  5,  9},
	{  4, 10},
	{  3, 11},
	{  2, 12},
	{  1, 13},
	{  0, 14},
	{ -1, 13},
	{ -2, 12},
	{ -3, 11},
	{ -4, 10},
	{ -5,  9},
	{ -6,  8},
	{ -7,  7},
	{ -8,  6},
	{ -9,  5},
	{-10,  4},
	{-11,  3},
	{-12,  2},
	{-13,  1},
	{-14,  0},
	{-13, -1},
	{-12, -2},
	{-11, -3},
	{-10, -4},
	{ -9, -5},
	{ -8, -6},
	{ -7, -7},
	{ -6, -8},
	{ -5, -9},
	{ -4,-10},
	{ -3,-11},
	{ -2,-12},
	{ -1,-13},
	{  0,-16},
	{  1,-15},
	{  2,-14},
	{  3,-13},
	{  4,-12},
	{  5,-11},
	{  6,-10},
	{  7, -9},
	{  8, -8},
	{  9, -7},
	{ 10, -6},
	{ 11, -5},
	{ 12, -4},
	{ 13, -3},
	{ 14, -2},
	{ 15, -1},
	{ 16,  0},
	{ 15,  1},
	{ 14,  2},
	{ 13,  3},
	{ 12,  4},
	{ 11,  5},
	{ 10,  6},
	{  9,  7},
	{  8,  8},
	{  7,  9},
	{  6, 10},
	{  5, 11},
	{  4, 12},
	{  3, 13},
	{  2, 14},
	{  1, 15},
	{  0, 16},
	{ -1, 15},
	{ -2, 14},
	{ -3, 13},
	{ -4, 12},
	{ -5, 11},
	{ -6, 10},
	{ -7,  9},
	{ -8,  8},
	{ -9,  7},
	{-10,  6},
	{-11,  5},
	{-12,  4},
	{-13,  3},
	{-14,  2},
	{-15,  1},
	{-16,  0},
	{-15, -1},
	{-14, -2},
	{-13, -3},
	{-12, -4},
	{-11, -5},
	{-10, -6},
	{ -9, -7},
	{ -8, -8},
	{ -7, -9},
	{ -6,-10},
	{ -5,-11},
	{ -4,-12},
	{ -3,-13},
	{ -2,-14},
	{ -1,-15},
}
;

int const OFFSET_COUNT_CENTER = 1;
int const OFFSET_COUNT_ADJACENT = 9;
int const OFFSET_COUNT_RADIUS = 21;
int const OFFSET_COUNT_RADIUS_SIDE = 37;
int const OFFSET_COUNT_RADIUS_CORNER = 45;

int const BASE_TILE_OFFSETS[OFFSET_COUNT_RADIUS_CORNER][2] =
{
	// center
	{+0,+0},
	// adjacent
	{+1,-1},
	{+2,+0},
	{+1,+1},
	{+0,+2},
	{-1,+1},
	{-2,+0},
	{-1,-1},
	{+0,-2},
	// base radius outer
	{+2,-2},
	{+2,+2},
	{-2,+2},
	{-2,-2},
	{+1,-3},
	{+3,-1},
	{+3,+1},
	{+1,+3},
	{-1,+3},
	{-3,+1},
	{-3,-1},
	{-1,-3},
	// base radius side
	{+3,-3},
	{+3,+3},
	{-3,+3},
	{-3,-3},
	{-0,-4},
	{+4,+0},
	{+0,+4},
	{-4,-0},
	{+2,-4},
	{+4,-2},
	{+4,+2},
	{+2,+4},
	{-2,+4},
	{-4,+2},
	{-4,-2},
	{-2,-4},
	// base radius adjacent
	{+5,-1},
	{+5,+1},
	{+1,+5},
	{-1,+5},
	{-5,+1},
	{-5,-1},
	{-1,-5},
	{+1,-5},
};

int getOffsetIndex(int dx, int dy);
int getOffsetIndex(int x1, int y1, int x2, int y2);
int getOffsetIndex(MAP *tile1, MAP *tile2);

// =======================================================
// tile offsets - end
// =======================================================

struct VehicleFilter
{
	int factionId = -1;
	int triad = -1;
	int weaponType = -1;
};

// =======================================================
// MAP conversions
// =======================================================

bool isOnMap(int x, int y);
bool isValidTile(MAP *tile);
int getMapIndexByCoordinates(int x, int y);
int getMapIndexByPointer(MAP *tile);
int getX(int mapIndex);
int getY(int mapIndex);
int getX(MAP *tile);
int getY(MAP *tile);
Location getLocation(MAP *tile);
MAP *getMapTile(int mapIndex);
MAP *getMapTile(int x, int y);
Location getDelta(MAP *tile1, MAP *tile2);
Location getRectangularCoordinates(Location diagonal, bool shift);
Location getRectangularCoordinates(Location diagonal);
Location getRectangularCoordinates(MAP *tile, bool shift);
Location getDiagonalCoordinates(Location rectangular);

// =======================================================
// MAP conversions - end
// =======================================================

// =======================================================
// iterating surrounding locations
// =======================================================

std::vector<MAP *> getAdjacentTiles(MAP *tile);
std::vector<MAP *> getSideTiles(MAP *tile);
MAP * getSquareBlockRadiusTile(MAP *center, int index);
std::vector<MAP *> getSquareBlockRadiusTiles(MAP *center, int beginIndex, int endIndex);

std::vector<MAP *> getSquareBlockTiles(MAP *center, int minRadius, int maxRadius);
std::vector<MAP *> getEqualRangeTiles(MAP *tile, int range);
std::vector<MAP *> getRangeTiles(MAP *tile, int range, bool includeCenter);

std::vector<MAP *> getBaseOffsetTiles(int x, int y, int offsetBegin, int offsetEnd);
std::vector<MAP *> getBaseOffsetTiles(MAP *tile, int offsetBegin, int offsetEnd);
std::vector<MAP *> getBaseAdjacentTiles(int x, int y, bool includeCenter);
std::vector<MAP *> getBaseAdjacentTiles(MAP *tile, bool includeCenter);
std::vector<MAP *> getBaseRadiusTiles(int x, int y, bool includeCenter);
std::vector<MAP *> getBaseRadiusTiles(MAP *tile, bool includeCenter);
std::vector<MAP *> getBaseExternalRadiusTiles(MAP *tile);
std::vector<MAP *> getBaseRadiusSideTiles(MAP *tile);
std::vector<MAP *> getBaseRadiusAdjacentTiles(MAP *tile);

std::vector<MAP *> getMeleeAttackPositions(MAP *target);
std::vector<MAP *> getArtilleryAttackPositions(MAP *target);

// =======================================================
// iterating surrounding locations - end
// =======================================================

bool has_armor(int factionId, int armorId);
bool has_reactor(int factionId, int reactorId);
int getBaseMineralCost(int baseId, int item);
int getBaseItemCost(int baseId, int item);
int veh_triad(int id);
int map_rainfall(MAP *tile);
int map_level(MAP *tile);
int map_elevation(MAP *tile);
int map_rockiness(MAP *tile);
bool map_base(MAP *tile);
bool map_has_item(MAP *tile, uint32_t item);
bool map_has_landmark(MAP *tile, int landmark);
int getNutrientBonus(MAP *tile);
int getMineralBonus(MAP *tile);
int getEnergyBonus(MAP *tile);
int veh_speed_without_roads(int id);
int unit_chassis_speed(int id);
int veh_chassis_speed(int id);
int unit_triad(int id);
double random_double(double scale);
bool is_sea_base(int id);
BASE *vehicle_home_base(VEH *vehicle);
MAP *base_square(BASE *base);
bool unit_has_ability(int id, int ability);
bool vehicle_has_ability(int vehicleId, int ability);
bool vehicle_has_ability(VEH *vehicle, int ability);
const char *readOrder(int id);
bool isBaseHasFacility(int base_id, int facility_id);
void setBaseFacility(int base_id, int facility_id, bool add);
bool has_facility_tech(int faction_id, int facility_id);
int getBaseDoctors(int id);
int getDoctorSpecialistType(int factionId);
int getBaseBuildingItem(int baseId);
bool isBaseBuildingUnit(int baseId);
bool isBaseBuildingFacility(int baseId);
bool isBaseBuildingProject(int baseId);
bool isBaseProductionWithinRetoolingExemption(int baseId);
int getBaseBuildingItemMineralCost(int baseId);
double getUnitPsiOffenseStrength(int unitId);
double getUnitPsiDefenseStrength(int unitId);
double getUnitConventionalOffenseStrength(int unitId);
double getUnitConventionalDefenseStrength(int unitId);
double getNewUnitPsiOffenseStrength(int id, int baseId);
double getNewUnitPsiDefenseStrength(int id, int baseId);
double getNewUnitConventionalOffenseStrength(int id, int baseId);
double getNewUnitConventionalDefenseStrength(int id, int baseId);
double getVehiclePsiOffenseStrength(int id);
double getVehiclePsiDefenseStrength(int id);
double getVehicleConventionalOffenseStrength(int vehicleId);
double getVehicleConventionalDefenseStrength(int vehicleId);
double getVehicleConventionalArtilleryDuelStrength(int vehicleId);
double getVehiclePsiOffensePower(int vehicleId);
double getVehiclePsiDefensePower(int vehicleId);
double getVehicleConventionalOffensePower(int vehicleId);
double getVehicleConventionalDefensePower(int vehicleId);
double getFactionSEPlanetOffenseModifier(int factionId);
double getFactionSEPlanetDefenseModifier(int factionId);
double getPsiCombatBaseOdds(int triad);
bool isCombatUnit(int unitId);
bool isCombatVehicle(int vehicleId);
bool isUtilityVehicle(int vehicleId);
bool isOgreVehicle(int vehicleId);
double calculatePsiDamageAttack(int id, int enemyId);
double calculatePsiDamageDefense(int id, int enemyId);
double calculateNativeDamageAttack(int id);
double calculateNativeDamageDefense(int id);
void setVehicleOrder(int id, int order);
MAP *getBaseMapTile(int baseId);
MAP *getVehicleMapTile(int vehicleId);
bool isImprovedTile(MAP *tile);
bool isImprovedTile(int x, int y);
bool isSupplyVehicle(VEH *vehicle);
bool isColonyUnit(int id);
bool isArtifactUnit(int unitId);
bool isArtifactVehicle(int id);
bool isColonyVehicle(int id);
bool isFormerUnit(int unitId);
bool isFormerVehicle(int vehicleId);
bool isFormerVehicle(VEH *vehicle);
bool isTransportVehicle(VEH *vehicle);
bool isTransportUnit(int unitId);
bool isTransportVehicle(int vehicleId);
bool isSeaTransportUnit(int unitId);
bool isSeaTransportVehicle(int vehicleId);
bool isVehicleProbe(VEH *vehicle);
bool isVehicleIdle(int vehicleId);
bool isMeleeUnit(int unitId);
bool isMeleeVehicle(int vehicleId);
bool isArtilleryUnit(int unitId);
bool isArtilleryVehicle(int vehicleId);
bool isLandArtilleryUnit(int unitId);
bool isLandArtilleryVehicle(int vehicleId);
void computeBase(int baseId, bool resetWorkedTiles);
void computeBaseDoctors(int baseId);
robin_hood::unordered_flat_set<int> getBaseConnectedRegions(int id);
robin_hood::unordered_flat_set<int> getBaseConnectedOceanRegions(int baseId);
bool isLandRegion(int region);
bool isOceanRegion(int region);
double evaluateUnitConventionalDefenseEffectiveness(int id);
double evaluateUnitConventionalOffenseEffectiveness(int id);
double evaluateUnitPsiDefenseEffectiveness(int id);
double evaluateUnitPsiOffenseEffectiveness(int id);
double getBaseDefenseMultiplier(int id, int attackerUnitId, int defenderUnitId);
int estimateBaseItemProductionTime(int baseId, int item);
int estimateBaseProductionTurnsToComplete(int id);
std::vector<MAP *> getBaseWorkableTiles(int baseId, bool startWithCenter);
std::vector<MAP *> getBaseWorkedTiles(int baseId);
bool isBaseWorkedTile(int baseId, int x, int y);
bool isBaseWorkedTile(int baseId, MAP *tile);
int getBaseConventionalDefenseValue(int baseId);
std::vector<int> getFactionUnitIds(int factionId, bool includeObsolete, bool includePrototype);
int getFactionFastestFormerUnitId(int factionId);
bool isVehicleNativeLand(int vehicleId);
bool isBaseBuildingColony(int baseId);
int getFacilityPopulationLimit(int factionId, int facilityId);
bool isBaseFacilityAvailable(int baseId, int facilityId);
bool isBaseConnectedToRegion(int baseId, int region);
bool isTileConnectedToRegion(int x, int y, int region);
std::vector<int> getRegionBases(int factionId, int region);
std::vector<int> getRegionSurfaceVehicles(int factionId, int region, bool includeStationed);
std::vector<int> getBaseGarrison(int baseId);
std::vector<int> getBaseInfantryDefenderGarrison(int baseId);
std::vector<int> getFactionBases(int factionId);
double estimateUnitBaseLandNativeProtection(int unitId, int factionId, bool ocean);
double getVehicleBaseNativeProtection(int baseId, int vehicleId);
double getFactionOffenseMultiplier(int factionId);
double getFactionDefenseMultiplier(int factionId);
double getFactionFanaticBonusMultiplier(int factionId);
double getVehicleBaseNativeProtectionPotential(int vehicleId);
double getVehicleBaseNativeProtectionEfficiency(int vehicleId);
int getBasePoliceRating(int baseId);
int getBasePoliceEffect(int baseId);
int getBasePoliceAllowed(int baseId);
char *getVehicleUnitName(int vehicleId);
int getVehicleUnitPlan(int vehicleId);
double getBaseNativeProtection(int baseId);
bool isBaseHasAccessToWater(int baseId);
bool isBaseCanBuildShips(int baseId);
bool isExploredEdge(int factionId, int x, int y);
bool isVehicleExploring(int vehicleId);
bool isVehicleCanHealAtThisLocation(int vehicleId);
robin_hood::unordered_flat_set<int> getAdjacentOceanRegions(int x, int y);
robin_hood::unordered_flat_set<int> getConnectedOceanRegions(int factionId, int x, int y);
void setMapTileVisibleToFaction(MAP *tile, int factionId);
bool isDiploStatus(int faction1Id, int faction2Id, int diploStatus);
void setDiploStatus(int faction1Id, int faction2Id, int diploStatus, bool on);
int getRemainingMinerals(int baseId);
std::vector<int> getStackVehicles(int vehicleId);
std::vector<int> getTileVehicleIds(MAP *tile);
void setTerraformingAction(int vehicleId, int action);
__cdecl int mod_nutrient_yield(int faction_id, int a2, int x, int y, int a5);
__cdecl int mod_mineral_yield(int faction_id, int a2, int x, int y, int a5);
__cdecl int mod_energy_yield(int faction_id, int a2, int x, int y, int a5);
bool isCoast(MAP *tile);
bool isOceanRegionCoast(int x, int y, int oceanRegion);
std::vector<int> getLoadedVehicleIds(int vehicleId);
bool is_ocean_deep(MAP* sq);
bool isVehicleAtLocation(int vehicleId, int x, int y);
bool isVehicleAtLocation(int vehicleId, MAP *tile);
std::vector<int> getFactionLocationVehicleIds(int factionId, MAP *tile);
void hurryProduction(BASE* base, int minerals, int cost);
bool isVehicleOnTransport(int vehicleId);
bool isVehicleOnSeaTransport(int vehicleId);
bool isLandVehicleOnTransport(int vehicleId);
int getVehicleTransportId(int vehicleId);
int setMoveTo(int vehicleId, MAP *destination);
int setMoveTo(int vehicleId, const std::vector<MAP *> &waypoints);
bool isFriendlyTerritory(int factionId, MAP* tile);
bool isNeutralTerritory(int factionId, MAP* tile);
bool isHostileTerritory(int factionId, MAP* tile);
bool isUnitHasAbility(int unitId, int ability);
bool isVehicleHasAbility(int vehicleId, int ability);
bool isScoutUnit(int unitId);
bool isScoutVehicle(int vehicleId);
bool isTargettedLocation(MAP *tile);
bool isFactionTargettedLocation(MAP *tile, int factionId);
double getNativePsiAttackStrength(int triad);
double getMoraleMultiplier(int morale);
int getFactionSEMoraleBonus(int factionId);
int getVehicleMorale(int vehicleId);
int getNewVehicleMorale(int unitId, int baseId);
double getVehicleMoraleMultiplier(int vehicleId);
double getBasePsiDefenseMultiplier();
bool isWithinFriendlySensorRange(int factionId, int x, int y);
int getRegionPodCount(int x, int y, int range, int region);
MAP *getNearbyItemLocation(int x, int y, int range, int item);
bool isVehicleHealing(int vehicleId);
bool isVehicleInRegion(int vehicleId, int region);
bool isProbeVehicle(int vehicleId);
double battleCompute(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat);
double battleComputeStack(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat);
int getVehicleSpeedWithoutRoads(int id);
int getFactionBestWeapon(int factionId);
int getFactionBestWeapon(int factionId, int limit);
int getFactionBestArmor(int factionId);
int getFactionBestArmor(int factionId, int limit);
bool isSensorBonusApplied(int factionId, int x, int y, bool attacker);
int getBaseIdAt(int x, int y);
double getSensorOffenseMultiplier(int factionId, int x, int y);
double getSensorDefenseMultiplier(int factionId, int x, int y);
bool isNativeUnit(int unitId);
bool isNativeVehicle(int vehicleId);
double getPercentageBonusMultiplier(int percentageBonus);
bool isMineBonus(int x, int y);
std::vector<int> selectVehicles(const VehicleFilter filter);
bool isNextToRegion(int x, int y, int region);
int getSeaTransportInStack(int vehicleId);
bool isSeaTransportInStack(int vehicleId);
bool isSeaTransportAt(MAP *tile, int factionId);
bool isAvailableSeaTransportAt(MAP *tile, int factionId);
int countPodsInBaseRadius(int x, int y);
bool isBlocked(int factionId, MAP *tile);
bool isZoc(int factionId, MAP *tile);
double getBattleOdds(int attackerVehicleId, int defenderVehicleId, int longRangeFire);
double getBattleOddsAt(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat, MAP *battleTile);
double getBattleStackOdds(int attackerVehicleId, int defenderVehicleId, int longRangeCombat);
double getAttackEffect(int attackerVehicleId, int defenderVehicleId);
double getStackAttackEffect(int attackerVehicleId, std::vector<int> defenderVehicleIds, bool longRangeCombat);
int getUnitOffenseValue(int unitId);
int getUnitDefenseValue(int unitId);
int getVehicleOffenseValue(int vehicleId);
int getVehicleDefenseValue(int vehicleId);
bool isFactionSpecial(int factionId, int special);
bool isFactionHasBonus(int factionId, int bonusId);
double getAlienMoraleMultiplier();
MAP *getNearestLandTerritory(int x, int y, int factionId);
void killVehicle(int vehicleId);
void board(int vehicleId);
void unboard(int vehicleId);
int getTransportUsedCapacity(int transportVehicleId);
int getTransportRemainingCapacity(int transportVehicleId);
int getVehicleMaxPower(int vehicleId);
int getVehiclePower(int vehicleId);
double getVehicleRelativeDamage(int vehicleId);
double getVehicleRelativePower(int vehicleId);
int getVehicleHitPoints(int vehicleId, bool psiCombat);
int getBaseGrowthRate(int baseId);
void accumulateMapIntValue(robin_hood::unordered_flat_map<int, int> *m, int key, int value);
int getHexCost(int unitId, int factionId, int fromX, int fromY, int toX, int toY, int speed1);
int getHexCost(int unitId, int factionId, MAP *fromTile, MAP *toTile, int speed1);
int getVehicleHexCost(int vehicleId, int fromX, int fromY, int toX, int toY);
int getSeaHexCost(MAP *tile);
bool isCommlink(int factionId1, int factionId2);
bool isVendetta(int factionId1, int factionId2);
bool isPact(int factionId1, int factionId2);
bool isFriendly(int factionId1, int factionId2);
bool isHostile(int factionId1, int factionId2);
bool isNeutral(int factionId1, int factionId2);
bool isVendettaWithAny(int factionId);
int getVehicleMaxPsiHP(int vehicleId);
int getVehicleCurrentPsiHP(int vehicleId);
int getVehicleMaxConventionalHP(int vehicleId);
int getVehicleCurrentConventionalHP(int vehicleId);
int getUnitSlotById(int unitId);
int getUnitIdBySlot(int factionId, int slot);
int getUnitIndexByUnitId(int factionId, int unitId);
int getFactionIdByUnitIndex(int unitIndex);
int getUnitIdByUnitIndex(int unitIndex);
int getBasePoliceDrones(int baseId);
bool isReactorIgnoredInConventionalCombat();
bool isReactorIgnoredInCombat(bool psiCombat);
bool isInfantryUnit(int unitId);
bool isInfantryVehicle(int vehicleId);
bool isInfantryPolice2xUnit(int unitId, int factionId);
bool isInfantryPolice2xVehicle(int vehicleId);
bool isPsiCombat(int attackerUnitId, int defenderUnitId);
double getAlienTurnStrengthModifier();
bool isProjectBuilt(int factionId, int projectFacilityId);
bool isProjectAvailable(int factionId, int projectFacilityId);
int getFactionMaintenance(int factionId);
int getFactionNetIncome(int factionId);
int getFactionGrossIncome(int factionId);
double getFactionTechPerTurn(int factionId);
bool isVehicleOnSentry(int vehicleId);
bool isVehicleOnAlert(int vehicleId);
bool isVehicleOnHold(int vehicleId);
bool isVehicleOnHold10Turns(int vehicleId);
void setVehicleOnSentry(int vehicleId);
void setVehicleOnAlert(int vehicleId);
void setVehicleOnHold(int vehicleId);
void setVehicleOnHold10Turns(int vehicleId);
int getDx(int x1, int x2);
int getDy(int y1, int y2);
int getAbilityProportionalCost(int abilityId);
int getAbilityFlatCost(int abilityId);
UNIT *getUnit(int unitId);
VEH *getVehicle(int vehicleId);
BASE *getBase(int baseId);
bool isBaseHasNativeRepairFacility(int baseId);
bool isBaseHasRegularRepairFacility(int baseId, int triad);
MFaction *getMFaction(int factionId);
Faction *getFaction(int factionId);
CChassis *getChassis(int chassisId);
CChassis *getUnitChassis(int unitId);
CChassis *getVehicleChassis(int vehicleId);
bool isNeedlejetUnit(int unitId);
bool isNeedlejetVehicle(int vehicleId);
bool isNeedlejetUnitInFlight(int unitId, MAP *tile);
bool isNeedlejetVehicleInFlight(int vehicleId);
void longRangeFire(int vehicleId, int offsetIndex);
void longRangeFire(int vehicleId, MAP *attackLocation);
RepairInfo getVehicleRepairInfo(int vehicleId, MAP *tile);
bool isObsoleteUnit(int unitId, int factionId);
bool isPrototypeUnit(int unitId);
bool isActiveUnit(int unitId);
bool isAvailableUnit(int unitId, int factionId, bool includeObsolete);
CAbility *getAbility(int abilityId);
void obsoleteUnit(int unitId, int factionId);
void reinstateUnit(int unitId, int factionId);
int divideIntegerRoundUp(int divident, int divisor);
CFacility *getFacility(int facilityId);
double getBaseSupportRatio(int baseId);
bool isAtArtilleryDamageLimit(int vehicleId);
CCitizen *getCitizen(int specialistType);
void setBaseSpecialist(int baseId, int specialistIndex, int specialistType);
bool removeBaseDoctor(int baseId);
void removeBaseDoctors(int baseId);
bool addBaseDoctor(int baseId);
int getVehicleMaxRebaseTurns(int vehicleId);
CTerraform *getTerraform(int terraformType);
int getBaseBDrones(int baseId);
bool isAllowedBaseLocation(int factionId, MAP *tile);
bool isRoughTerrain(MAP *tile);
bool isOpenTerrain(MAP *tile);
bool isMobileUnit(int unitId);
double getUnitMeleeAttackStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation);
double getUnitArtilleryAttackStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation);
bool isAtAirbase(int vehicleId);
int getClosestHostileBaseRange(int factionId, MAP *tile);
int getUnitSpeed(int unitId);
int getVehicleSpeed(int vehicleId);
double getEuclidianDistance(int x1, int y1, int x2, int y2);
int getVehicleUnitCost(int vehicleId);
int getVehicleRemainingMovement(int vehicleId);
bool isAllowedMove(int vehicleId, MAP *srcTile, MAP *dstTile);
int getVehicleHexCost(int vehicleId, int hexCost);
bool isUnitIgnoreZOC(int unitId);
bool isVehicleIgnoreZOC(int vehicleId);
int getBaseAt(MAP *tile);
bool isBaseAt(MAP *tile);
bool isFactionBaseAt(MAP *tile, int factionId);
bool isFriendlyBaseAt(MAP *tile, int factionId);
bool isHostileBaseAt(MAP *tile, int factionId);
bool isNeutralBaseAt(MAP *tile, int factionId);
bool isUnfriendlyBaseAt(MAP *tile, int factionId);
bool isEmptyHostileBaseAt(MAP *tile, int factionId);
bool isEmptyNeutralBaseAt(MAP *tile, int factionId);
bool isBunkerAt(MAP *tile);
bool isVehicleAt(MAP *tile);
bool isHostileVehicleAt(MAP *tile, int factionId);
bool isUnfriendlyVehicleAt(MAP *tile, int factionId);
bool isHostileSurfaceVehicleAt(MAP *tile, int factionId);
bool isUnitCanMeleeAttackAtTile(int attackerUnitId, int defenderUnitId, MAP *tile);
bool isUnitCanMeleeAttackFromTile(int attackerUnitId, int defenderUnitId, MAP *tile);
bool isUnitCanInitiateArtilleryDuel(int attackerUnitId, int defenderUnitId);
bool isUnitCanInitiateBombardment(int attackerUnitId, int defenderUnitId);
bool isVehicleCanMeleeAttackTile(int vehicleId, MAP *target);
bool isVehicleCanMeleeAttackFromTile(int vehicleId, MAP *from);
bool isVehicleCanArtilleryAttackTile(int vehicleId, MAP *target);
bool isVehicleCanArtilleryAttackFromTile(int vehicleId, MAP *from);
bool isAirbaseAt(MAP *tile);
bool isArtilleryAt(MAP *tile);
std::vector<int> getTileSurfaceVehicleIds(MAP *tile);
bool isSurfaceUnit(int unitId);
bool isSurfaceVehicle(int vehicleId);
bool isBombardmentUnit(int unitId);
bool isBombardmentVehicle(int vehicleId);
void setVehicleWaypoints(int vehicleId, const std::vector<MAP *> &waypoints);
bool isHoveringLandUnit(int unitId);
bool isHoveringLandVehicle(int vehicleId);
bool isEasyFungusEnteringLandUnit(int unitId);
bool isEasyFungusEnteringVehicle(int vehicleId);
double getBaseMineralMultiplier(int baseId);
double getBaseEconomyMultiplier(int baseId);
double getBaseLabsMultiplier(int baseId);
double getBasePsychMultiplier(int baseId);
bool isLandVechileMoveAllowed(int vehicleId, MAP *from, MAP *to);
int getRange(MAP *origin, MAP *destination);
int getRange(int tile1Index, int tile2Index);
double getVectorDistanceSquared(MAP *origin, MAP *destination);
double getVectorDistance(MAP *origin, MAP *destination);
double getVectorAngleCos(MAP *vertex, MAP *point1, MAP *point2);
int getAdjacentTileIndex(int tileIndex, int angle);
MAP *getTileByAngle(MAP *tile, int angle);
int getAngleByTile(MAP *tile, MAP *anotherTile);
bool isPodAt(MAP *tile);
std::vector<int> getTransportPassengers(int transportVehicleId);
double getVehicleMaxRelativeBombardmentDamage(int vehicleId);
double getVehicleRemainingRelativeBombardmentDamage(int vehicleId);
int getBasePopulationLimit(int baseId);
int getBaseRadiusLayer(MAP *baseTile, MAP *tile);
bool isWithinBaseRadius(MAP *baseTile, MAP *tile);
std::vector<MAP *> getFartherBaseRadiusTiles(MAP *baseTile, MAP *tile);
bool isFactionHasProject(int factionId, int facilityId);
Budget getBaseBudgetIntake(int baseId);
double getBasePopulationGrowth(int baseId);
int getMovementRateAlongTube();
int getMovementRateAlongRoad();
bool isInfantryDefensiveUnit(int unitId);
bool isInfantryDefensiveVehicle(int vehicleId);
bool isInfantryDefensivePolice2xUnit(int unitId, int factionId);
bool isInfantryDefensivePolice2xVehicle(int vehicleId);
bool isUnitRequireSupport(int unitId);
bool isFactionHasAbility(int faction, VehAbl abl);

