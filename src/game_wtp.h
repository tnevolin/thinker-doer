#pragma once

#include "main.h"
#include <vector>
#include <unordered_set>
#include <set>
#include "terranx_types.h"

struct Location
{
	int x;
	int y;

	Location()
	{
		this->x = -1;
		this->y = -1;
	}

	Location(int newX, int newY)
	{
		this->x = newX;
		this->y = newY;
	}

	Location(const Location &location)
	{
		this->x = location.x;
		this->y = location.y;
	}

	void set(int newX, int newY)
	{
		this->x = newX;
		this->y = newY;
	}

	void set(Location newLocation)
	{
		this->x = newLocation.x;
		this->y = newLocation.y;
	}
	
	bool operator ==(const Location &location) const
	{
		return this->x == location.x && this->y == location.y;
	}

};

namespace std
{
	template <>
		class hash<Location>
		{
			public:
				size_t operator()(const Location &location) const
				{
					return std::hash<int>()(location.x) ^ std::hash<int>()(location.y);
				}
		
		};
}

struct MAP_INFO
{
	int x;
	int y;
	MAP *tile;

	MAP_INFO() : tile(NULL) {}

	MAP_INFO(int argument_x, int argument_y, MAP *argument_tile)
	{
		this->x = argument_x;
		this->y = argument_y;
		this->tile = argument_tile;
	}

};

struct MAP_STATE
{
    byte climate;
    byte rocks;
    int items;
};

/*
This array is used for both adjacent tiles and base radius tiles.
*/
int const ADJACENT_TILE_OFFSET_COUNT = 9;
int const BASE_RADIUS_TILE_OFFSET_COUNT = 21;
int const BASE_RADIUS_BORDERED_TILE_OFFSET_COUNT = 37;
int const BASE_TILE_OFFSETS[BASE_RADIUS_BORDERED_TILE_OFFSET_COUNT][2] =
{
	// center
	{+0,+0},
	// adjacent tiles
	{+1,-1},
	{+2,+0},
	{+1,+1},
	{+0,+2},
	{-1,+1},
	{-2,+0},
	{-1,-1},
	{+0,-2},
	// base radius outer tiles
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
	// base radius bordered tiles
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
int getMapIndex(int x, int y);
int getMapIndex(Location location);
int getMapIndex(MAP *tile);
int getX(int mapIndex);
int getY(int mapIndex);
Location getLocation(int mapIndex);
Location getLocation(MAP *tile);
MAP *getMapTile(int mapIndex);
MAP *getMapTile(int x, int y);
MAP *getMapTile(Location location);

// =======================================================
// MAP conversions - end
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
bool isUnitColony(int id);
bool isVehicleArtifact(int id);
bool isColonyVehicle(int id);
bool isUnitFormer(int unitId);
bool isFormerVehicle(int vehicleId);
bool isFormerVehicle(VEH *vehicle);
bool isVehicleTransport(VEH *vehicle);
bool isTransportUnit(int unitId);
bool isVehicleTransport(int vehicleId);
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
std::vector<MAP *> getAdjacentTiles(int x, int y, bool startWithCenter);
std::vector<MAP_INFO> getAdjacentTileInfos(int x, int y, bool startWithCenter);
std::vector<MAP *> getBaseRadiusTiles(int x, int y, bool startWithCenter);
std::vector<MAP *> getBaseWorkableTiles(int baseId, bool startWithCenter);
std::vector<MAP_INFO> getBaseRadiusTileInfos(int x, int y, bool startWithCenter);
int getFriendlyIntersectedBaseRadiusTileCount(int factionId, int x, int y);
int getFriendlyLandBorderedBaseRadiusTileCount(int factionId, int x, int y);
std::vector<MAP *> getBaseWorkedTiles(int baseId);
std::vector<MAP *> getBaseWorkedTiles(BASE *base);
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
int getVehicleUnitPlan(int vehicleId);
int getBasePoliceUnitCount(int baseId);
double getBaseNativeProtection(int baseId);
bool isBaseHasAccessToWater(int baseId);
bool isBaseCanBuildShips(int baseId);
bool isExploredEdge(int factionId, int x, int y);
MAP_INFO getAdjacentOceanTileInfo(int x, int y);
bool isVehicleExploring(int vehicleId);
bool isVehicleCanHealAtThisLocation(int vehicleId);
std::unordered_set<int> getAdjacentOceanRegions(int x, int y);
std::unordered_set<int> getConnectedOceanRegions(int factionId, int x, int y);
bool isMapTileVisibleToFaction(MAP *tile, int factionId);
bool isDiploStatus(int faction1Id, int faction2Id, int diploStatus);
void setDiploStatus(int faction1Id, int faction2Id, int diploStatus, bool on);
int getRemainingMinerals(int baseId);
std::vector<int> getStackedVehicleIds(int vehicleId);
void setTerraformingAction(int vehicleId, int action);
double getImprovedTileWeightedYield(MAP_INFO *tileInfo, int terraformingActionsCount, int terraformingActions[], double nutrientWeight, double mineralWeight, double energyWeight);
void getMapState(MAP_INFO *mapInfo, MAP_STATE *mapState);
void setMapState(MAP_INFO *mapInfo, MAP_STATE *mapState);
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
bool isVehicleAtLocation(int vehicleId, Location location);
std::vector<int> getFactionLocationVehicleIds(int factionId, Location location);
Location getAdjacentRegionLocation(int x, int y, int region);
bool isValidLocation(Location location);
void hurryProduction(BASE* base, int minerals, int cost);
MAP* getMapTile(int mapTileIndex);
int getMapIndex(MAP *tile);
Location getMapIndexLocation(int mapIndex);
int getLocationMapIndex(Location location);
int getLocationMapIndex(int x, int y);
bool isLandVehicleOnSeaTransport(int vehicleId);
int getLandVehicleSeaTransportVehicleId(int vehicleId);
int setMoveTo(int vehicleId, Location location);
int setMoveTo(int vehicleId, int x, int y);
bool isFriendlyTerritory(int factionId, MAP* tile);
bool isVehicleHasAbility(int vehicleId, int abilityId);
bool isScoutUnit(int unitId);
bool isScoutVehicle(int vehicleId);
bool isTargettedLocation(Location location);
bool isFactionTargettedLocation(Location location, int factionId);
double getNativePsiAttackStrength(int triad);
double getMoraleModifier(int morale);
int getFactionSEMoraleBonus(int factionId);
int getVehicleMorale(int vehicleId, bool defendAtBase);
int getNewVehicleMorale(int unitId, int baseId, bool defendAtBase);
double getVehicleMoraleModifier(int vehicleId, bool defendAtBase);
double getBasePsiDefenseMultiplier();
bool isWithinFriendlySensorRange(int factionId, int x, int y);
std::vector<Location> getRegionPodLocations(int region);
int getRegionPodCount(int x, int y, int range, int region);
Location getNearbyItemLocation(int x, int y, int range, int item);
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
Location getTileLocation(MAP *tile);
bool isZoc(int factionId, int x, int y);
double getBattleOdds(int attackerVehicleId, int defenderVehicleId);
int getUnitWeaponOffenseValue(int unitId);
int getUnitArmorDefenseValue(int unitId);
int getVehicleWeaponOffenseValue(int vehicleId);
int getVehicleArmorDefenseValue(int vehicleId);
bool factionHasSpecial(int factionId, int special);
bool factionHasBonus(int factionId, int bonusId);
double getAlienMoraleModifier();
Location getNearestLandTerritory(int x, int y, int factionId);

