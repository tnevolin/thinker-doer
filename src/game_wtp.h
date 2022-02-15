#pragma once

#include "main.h"
#include <vector>
#include <set>
#include <set>
#include "terranx_types.h"

struct MAP_STATE
{
    byte climate;
    byte rocks;
    int items;
};

// =======================================================
// iterating surrounding locations structures
// =======================================================

int const BASE_OFFSET_COUNT_CENTER = 1;
int const BASE_OFFSET_COUNT_ADJACENT = 9;
int const BASE_OFFSET_COUNT_RADIUS = 21;
int const BASE_OFFSET_COUNT_RADIUS_ADJACENT = 37;

int const BASE_TILE_OFFSETS[BASE_OFFSET_COUNT_RADIUS_ADJACENT][2] =
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
	// base radius adjacent
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
};

// =======================================================
// iterating surrounding locations structures - end
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
int getMapIndexByCoordinates(int x, int y);
int getMapIndexByPointer(MAP *tile);
int getX(int mapIndex);
int getY(int mapIndex);
int getX(MAP *tile);
int getY(MAP *tile);
MAP *getMapTile(int mapIndex);
MAP *getMapTile(int x, int y);

// =======================================================
// MAP conversions - end
// =======================================================

// =======================================================
// iterating surrounding locations
// =======================================================

std::vector<MAP *> getBaseOffsetTiles(int x, int y, int offsetBegin, int offsetEnd);
std::vector<MAP *> getBaseOffsetTiles(MAP *tile, int offsetBegin, int offsetEnd);
std::vector<MAP *> getBaseAdjacentTiles(int x, int y, bool includeCenter);
std::vector<MAP *> getBaseAdjacentTiles(MAP *tile, bool includeCenter);
std::vector<MAP *> getBaseRadiusTiles(int x, int y, bool includeCenter);
std::vector<MAP *> getBaseRadiusTiles(MAP *tile, bool includeCenter);
std::vector<MAP *> getBaseRadiusAdjacentTiles(int x, int y, bool includeCenter);
std::vector<MAP *> getBaseRadiusAdjacentTiles(MAP *tile, bool includeCenter);

std::vector<MAP *> getEqualRangeTiles(int x, int y, int range);
std::vector<MAP *> getEqualRangeTiles(MAP *tile, int range);
std::vector<MAP *> getRangeTiles(int x, int y, int range, bool includeCenter);
std::vector<MAP *> getRangeTiles(MAP *tile, int range, bool includeCenter);
std::vector<MAP *> getAdjacentTiles(int x, int y, bool includeCenter);
std::vector<MAP *> getAdjacentTiles(MAP *tile, bool includeCenter);

// =======================================================
// iterating surrounding locations - end
// =======================================================

bool has_armor(int factionId, int armorId);
bool has_reactor(int factionId, int reactorId);
int getBaseMineralCost(int baseId, int itemId);
int veh_triad(int id);
int map_rainfall(MAP *tile);
int map_level(MAP *tile);
int map_elevation(MAP *tile);
int map_rockiness(MAP *tile);
bool map_base(MAP *tile);
bool map_has_item(MAP *tile, unsigned int item);
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
void setBaseFacility(int base_id, int facility_id, bool add);
bool has_facility_tech(int faction_id, int facility_id);
int getDoctors(int id);
int getDoctorQuelledDrones(int id);
int getBaseBuildingItem(int baseId);
bool isBaseBuildingUnit(int baseId);
bool isBaseBuildingFacility(int baseId);
bool isBaseBuildingProject(int baseId);
bool isBaseProductionWithinRetoolingExemption(int baseId);
int getBaseBuildingItemCost(int baseId);
double getUnitPsiOffenseStrength(int id, int baseId);
double getUnitPsiDefenseStrength(int id, int baseId, bool defendAtBase);
double getUnitConventionalOffenseStrength(int id, int baseId);
double getUnitConventionalDefenseStrength(int id, int baseId, bool defendAtBase);
double getVehiclePsiOffenseStrength(int id);
double getVehiclePsiDefenseStrength(int id, bool defendAtBase);
double getVehicleConventionalOffenseStrength(int vehicleId);
double getVehicleConventionalDefenseStrength(int vehicleId, bool defendAtBase);
double getFactionSEPlanetAttackModifier(int factionId);
double getFactionSEPlanetDefenseModifier(int factionId);
double getPsiCombatBaseOdds(int triad);
bool isCombatUnit(int id);
bool isCombatVehicle(int id);
double calculatePsiDamageAttack(int id, int enemyId);
double calculatePsiDamageDefense(int id, int enemyId);
double calculateNativeDamageAttack(int id);
double calculateNativeDamageDefense(int id);
void setVehicleOrder(int id, int order);
MAP *getBaseMapTile(int baseId);
MAP *getVehicleMapTile(int vehicleId);
bool isImprovedTile(int x, int y);
bool isVehicleSupply(VEH *vehicle);
bool isColonyUnit(int id);
bool isArtifactVehicle(int id);
bool isColonyVehicle(int id);
bool isUnitFormer(int unitId);
bool isFormerVehicle(int vehicleId);
bool isFormerVehicle(VEH *vehicle);
bool isTransportVehicle(VEH *vehicle);
bool isTransportUnit(int unitId);
bool isTransportVehicle(int vehicleId);
bool isSeaTransportVehicle(int vehicleId);
bool isVehicleProbe(VEH *vehicle);
bool isVehicleIdle(int vehicleId);
void computeBase(int baseId);
std::set<int> getBaseConnectedRegions(int id);
std::set<int> getBaseConnectedOceanRegions(int baseId);
bool isLandRegion(int region);
bool isOceanRegion(int region);
double evaluateUnitConventionalDefenseEffectiveness(int id);
double evaluateUnitConventionalOffenseEffectiveness(int id);
double evaluateUnitPsiDefenseEffectiveness(int id);
double evaluateUnitPsiOffenseEffectiveness(int id);
double getBaseDefenseMultiplier(int id, int triad);
int getUnitOffenseValue(int id);
int getUnitDefenseValue(int id);
int getVehicleOffenseValue(int id);
int getVehicleDefenseValue(int id);
int estimateBaseProductionTurnsToComplete(int id);
std::vector<MAP *> getBaseWorkableTiles(int baseId, bool startWithCenter);
int getBaseRadiusTileOverlapCount(int x, int y);
int getFriendlyLandBorderedBaseRadiusTileCount(int factionId, int x, int y);
std::vector<MAP *> getBaseWorkedTiles(int baseId);
std::vector<MAP *> getBaseWorkedTiles(BASE *base);
bool isBaseWorkedTile(int baseId, int x, int y);
bool isBaseWorkedTile(int baseId, MAP *tile);
int getBaseConventionalDefenseValue(int baseId);
std::vector<int> getFactionPrototypes(int factionId, bool includeNotPrototyped);
bool isVehicleNativeLand(int vehicleId);
bool isBaseBuildingColony(int baseId);
int projectBasePopulation(int baseId, int turns);
int getFactionFacilityPopulationLimit(int factionId, int facilityId);
bool isBaseFacilityAvailable(int baseId, int facilityId);
bool isBaseConnectedToRegion(int baseId, int region);
bool isTileConnectedToRegion(int x, int y, int region);
std::vector<int> getRegionBases(int factionId, int region);
std::vector<int> getRegionSurfaceVehicles(int factionId, int region, bool includeStationed);
std::vector<int> getBaseGarrison(int baseId);
std::vector<int> getFactionBases(int factionId);
double estimateUnitBaseLandNativeProtection(int unitId, int factionId, bool ocean);
double getVehicleBaseNativeProtection(int baseId, int vehicleId);
double getFactionOffenseMultiplier(int factionId);
double getFactionDefenseMultiplier(int factionId);
double getFactionFanaticBonusMultiplier(int factionId);
double getVehicleBaseNativeProtectionPotential(int vehicleId);
double getVehicleBaseNativeProtectionEfficiency(int vehicleId);
int getAllowedPolice(int factionId);
int getBasePolice(int baseId);
int getVehicleUnitPlan(int vehicleId);
int getBasePoliceUnitCount(int baseId);
double getBaseNativeProtection(int baseId);
bool isBaseHasAccessToWater(int baseId);
bool isBaseCanBuildShips(int baseId);
bool isExploredEdge(int factionId, int x, int y);
bool isVehicleExploring(int vehicleId);
bool isVehicleCanHealAtThisLocation(int vehicleId);
std::set<int> getAdjacentOceanRegions(int x, int y);
std::set<int> getConnectedOceanRegions(int factionId, int x, int y);
bool isMapTileVisibleToFaction(MAP *tile, int factionId);
bool isDiploStatus(int faction1Id, int faction2Id, int diploStatus);
void setDiploStatus(int faction1Id, int faction2Id, int diploStatus, bool on);
int getRemainingMinerals(int baseId);
std::vector<int> getStackedVehicleIds(int vehicleId);
void setTerraformingAction(int vehicleId, int action);
void getMapState(MAP *tile, MAP_STATE *mapState);
void setMapState(MAP *tile, MAP_STATE *mapState);
void copyMapState(MAP_STATE *destinationMapState, MAP_STATE *sourceMapState);
void generateTerraformingChange(MAP_STATE *mapState, int action);
HOOK_API int mod_nutrient_yield(int faction_id, int a2, int x, int y, int a5);
HOOK_API int mod_mineral_yield(int faction_id, int a2, int x, int y, int a5);
HOOK_API int mod_energy_yield(int faction_id, int a2, int x, int y, int a5);
bool isCoast(int x, int y);
bool isOceanRegionCoast(int x, int y, int oceanRegion);
std::vector<int> getLoadedVehicleIds(int vehicleId);
bool is_ocean_deep(MAP* sq);
bool isVehicleAtLocation(int vehicleId, int x, int y);
bool isVehicleAtLocation(int vehicleId, MAP *tile);
std::vector<int> getFactionLocationVehicleIds(int factionId, MAP *tile);
void hurryProduction(BASE* base, int minerals, int cost);
bool isLandVehicleOnSeaTransport(int vehicleId);
int getLandVehicleSeaTransportVehicleId(int vehicleId);
int setMoveTo(int vehicleId, MAP *tile);
int setMoveTo(int vehicleId, int x, int y);
bool isFriendlyTerritory(int factionId, MAP* tile);
bool isVehicleHasAbility(int vehicleId, int abilityId);
bool isScoutUnit(int unitId);
bool isScoutVehicle(int vehicleId);
bool isTargettedLocation(MAP *tile);
bool isFactionTargettedLocation(MAP *tile, int factionId);
double getNativePsiAttackStrength(int triad);
double getMoraleModifier(int morale);
int getFactionSEMoraleBonus(int factionId);
int getVehicleMorale(int vehicleId, bool defendAtBase);
int getNewVehicleMorale(int unitId, int baseId, bool defendAtBase);
double getVehicleMoraleModifier(int vehicleId, bool defendAtBase);
double getBasePsiDefenseMultiplier();
bool isWithinFriendlySensorRange(int factionId, int x, int y);
int getRegionPodCount(int x, int y, int range, int region);
MAP *getNearbyItemLocation(int x, int y, int range, int item);
bool isVehicleHealing(int vehicleId);
bool isVehicleInRegion(int vehicleId, int region);
bool isProbeVehicle(int vehicleId);
double battleCompute(int attackerVehicleId, int defenderVehicleId);
int getVehicleSpeedWithoutRoads(int id);
int getFactionBestWeapon(int factionId);
int getFactionBestArmor(int factionId);
bool isPrototypeUnit(int unitId);
bool isSensorBonusApplied(int factionId, int x, int y, bool attacker);
int getBaseIdInTile(int x, int y);
double getSensorOffenseMultiplier(int factionId, int x, int y);
double getSensorDefenseMultiplier(int factionId, int x, int y);
bool isNativeUnit(int unitId);
bool isNativeVehicle(int vehicleId);
double getPercentageBonusMultiplier(int percentageBonus);
int getPercentageBonusModifiedValue(int value, int percentageBonus);
int getPercentageSubstituteBonusModifiedValue(int value, int oldPercentageBonus, int newPercentageBonus);
int getUnitMaintenanceCost(int unitId);
bool isMineBonus(int x, int y);
std::vector<int> selectVehicles(const VehicleFilter filter);
bool isNextToRegion(int x, int y, int region);
int getSeaTransportInStack(int vehicleId);
bool isSeaTransportInStack(int vehicleId);
int countPodsInBaseRadius(int x, int y);
bool isZoc(int factionId, int x, int y);
double getBattleOdds(int attackerVehicleId, int defenderVehicleId);
int getUnitWeaponOffenseValue(int unitId);
int getUnitArmorDefenseValue(int unitId);
int getVehicleWeaponOffenseValue(int vehicleId);
int getVehicleArmorDefenseValue(int vehicleId);
bool factionHasSpecial(int factionId, int special);
bool factionHasBonus(int factionId, int bonusId);
double getAlienMoraleModifier();
MAP *getNearestLandTerritory(int x, int y, int factionId);
int getRangeToNearestFactionBase(int x, int y, int factionId);
int getRangeToNearestFactionColony(int x, int y, int factionId);
void killVehicle(int vehicleId);

