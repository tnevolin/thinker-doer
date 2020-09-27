#pragma once

#include "main.h"
#include "engine.h"

struct MAP_INFO
{
	int x;
	int y;
	MAP *tile;
};

/*

Function parameter naming conventions

Faction ID is always written as "faction". It is also written with a number suffix
(faction1, faction2, ...) if there are multiple factions handled in the same function.

Generic "id" is used to denote any parameter for base, unit, or vehicle IDs.
It is assumed the meaning of the parameter is clear from the function context.

If there multiple non-faction ID parameters, the first one is written with a full name
"base_id" etc. and the second can be "id" if the meaning is clear from the context.

*/

const int offset[][2] = {
    {1,-1},{2,0},{1,1},{0,2},{-1,1},{-2,0},{-1,-1},{0,-2}
};

const int offset_tbl[][2] = {
    {1,-1},{2,0},{1,1},{0,2},{-1,1},{-2,0},{-1,-1},{0,-2},
    {2,-2},{2,2},{-2,2},{-2,-2},{1,-3},{3,-1},{3,1},{1,3},
    {-1,3},{-3,1},{-3,-1},{-1,-3},{4,0},{-4,0},{0,4},{0,-4},
    {1,-5},{2,-4},{3,-3},{4,-2},{5,-1},{5,1},{4,2},{3,3},
    {2,4},{1,5},{-1,5},{-2,4},{-3,3},{-4,2},{-5,1},{-5,-1},
    {-4,-2},{-3,-3},{-2,-4},{-1,-5},{0,6},{6,0},{0,-6},{-6,0},
    {0,-8},{1,-7},{2,-6},{3,-5},{4,-4},{5,-3},{6,-2},{7,-1},
    {8,0},{7,1},{6,2},{5,3},{4,4},{3,5},{2,6},{1,7},
    {0,8},{-1,7},{-2,6},{-3,5},{-4,4},{-5,3},{-6,2},{-7,1},
    {-8,0},{-7,-1},{-6,-2},{-5,-3},{-4,-4},{-3,-5},{-2,-6},{-1,-7},
};

/*
This array is used for both adjacent tiles and base radius tiles.
*/
int const ADJACENT_TILE_OFFSET_COUNT = 9;
int const BASE_TILE_OFFSET_COUNT = 21;
int const BASE_TILE_OFFSETS[BASE_TILE_OFFSET_COUNT][2] =
{
	{+0,+0},
	{+1,-1},
	{+2,+0},
	{+1,+1},
	{+0,+2},
	{-1,+1},
	{-2,+0},
	{-1,-1},
	{+0,-2},
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
};

char* prod_name(int prod);
int mineral_cost(int factionId, int itemId, int baseId);
bool has_tech(int faction, int tech);
bool has_ability(int faction, int abl);
bool has_chassis(int faction, int chs);
bool has_weapon(int faction, int wpn);
bool has_terra(int faction, int act, bool ocean);
bool has_project(int faction, int id);
bool has_facility(int base_id, int id);
bool can_build(int base_id, int id);
bool can_build_unit(int faction, int id);
bool is_human(int faction);
bool ai_faction(int faction);
bool ai_enabled(int faction);
bool at_war(int faction1, int faction2);
bool un_charter();
int prod_count(int faction, int id, int skip);
int find_hq(int faction);
int veh_triad(int id);
int veh_speed(int id);
int veh_speed_without_roads(int id);
int unit_chassis_speed(int id);
int veh_chassis_speed(int id);
int unit_triad(int id);
int unit_speed(int id);
int best_armor(int faction, bool cheap);
int best_weapon(int faction);
int best_reactor(int faction);
int offense_value(UNIT* u);
int defense_value(UNIT* u);
int random(int n);
double random_double(double scale);
int map_hash(int x, int y);
double lerp(double a, double b, double t);
int wrap(int a);
int map_range(int x1, int y1, int x2, int y2);
int point_range(const Points& S, int x, int y);
double mean_range(const Points& S, int x, int y);
MAP* mapsq(int x, int y);
int unit_in_tile(MAP* sq);
int set_move_to(int id, int x, int y);
int set_road_to(int id, int x, int y);
int set_move_road_tube_to(int id, int x, int y);
int set_action(int id, int act, char icon);
int set_convoy(int id, int res);
int veh_skip(int id);
bool at_target(VEH* veh);
bool is_ocean(MAP* sq);
bool is_ocean_shelf(MAP* sq);
bool is_ocean_deep(MAP* sq);
bool is_sea_base(int id);
bool workable_tile(int x, int y, int faction);
bool has_defenders(int x, int y, int faction);
int nearby_items(int x, int y, int range, uint32_t item);
int bases_in_range(int x, int y, int range);
int nearby_tiles(int x, int y, int type, int limit);
int coast_tiles(int x, int y);
int spawn_veh(int unit_id, int faction, int x, int y, int base_id);
char* parse_str(char* buf, int len, const char* s1, const char* s2, const char* s3, const char* s4);
void check_zeros(int* ptr, int len);
void print_map(int x, int y);
void print_veh(int id);
void print_base(int id);
int map_rainfall(MAP *tile);
int map_level(MAP *tile);
int map_elevation(MAP *tile);
int map_rockiness(MAP *tile);
bool map_base(MAP *tile);
bool map_has_item(MAP *tile, int item);
bool map_has_landmark(MAP *tile, int landmark);

struct PathNode {
    int x;
    int y;
    int dist;
    int prev;
};

class TileSearch {
    int type;
    int head;
    int tail;
    int roads;
    int items;
    int y_skip;
    int owner;
    MAP* sq;
    public:
    int rx, ry, dist, cur;
    PathNode newtiles[QSIZE];
    Points oldtiles;
    void init(int, int, int);
    void init(int, int, int, int);
    bool has_zoc(int);
    MAP* get_next();
};

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
bool isBaseBuildingProject(int baseId);
bool isBaseProductionWithinRetoolingExemption(int baseId);
bool isBaseBuildingProjectBeyondRetoolingExemption(int baseId);
int getBaseBuildingItemCost(int baseId);
int getSEMoraleAttack(int factionId);
int getSEMoraleDefense(int factionId);
int getMoraleAttack(int id);
int getMoraleDefense(int id);
double getMoraleModifierAttack(int id);
double getMoraleModifierDefense(int id);
double getVehiclePsiAttackStrength(int id);
double getVehiclePsiDefenseStrength(int id);
double getNewVehicleMoraleModifierAttack(int factionId, double averageFacilityMoraleBoost);
double getNewVehicleMoraleModifierDefense(int factionId, double averageFacilityMoraleBoost);
double getSEPlanetModifierAttack(int factionId);
double getSEPlanetModifierDefense(int factionId);
double getPsiCombatBaseOdds(int triad);
bool isCombatUnit(int id);
bool isCombatVehicle(int id);
double calculatePsiDamageAttack(int id, int enemyId);
double calculatePsiDamageDefense(int id, int enemyId);
double calculateNativeDamageAttack(int id);
double calculateNativeDamageDefense(int id);
void setVehicleOrder(int id, int order);
MAP *getMapTile(int x, int y);
MAP *getBaseMapTile(int baseId);
MAP *getVehicleMapTile(int vehicleId);
bool isImprovedTile(int x, int y);
bool isVehicleSupply(VEH *vehicle);
bool isColonyUnit(int id);
bool isVehicleColony(int id);
bool isFormerUnit(int unitId);
bool isVehicleFormer(int vehicleId);
bool isVehicleFormer(VEH *vehicle);
bool isVehicleTransport(VEH *vehicle);
bool isVehicleProbe(VEH *vehicle);
void computeBase(int baseId);
std::set<int> getBaseConnectedRegions(int id);
std::set<int> getBaseConnectedOceanRegions(int baseId);
bool isOceanRegion(int region);
double evaluateUnitConventionalDefenseEffectiveness(int id);
double evaluateUnitConventionalOffenseEffectiveness(int id);
double evaluateUnitPsiDefenseEffectiveness(int id);
double evaluateUnitPsiOffenseEffectiveness(int id);
double getBaseDefenseMultiplier(int id, int triad, bool countDefensiveStructures, bool countTerritoryBonus);
int getUnitOffenseValue(int id);
int getUnitDefenseValue(int id);
int getVehicleOffenseValue(int id);
int getVehicleDefenseValue(int id);
int estimateBaseProductionTurnsToComplete(int id);
std::vector<MAP *> getAdjacentTiles(int x, int y, bool startWithCenter);
std::vector<MAP_INFO> getAdjacentTileInfos(int x, int y, bool startWithCenter);
std::vector<MAP *> getBaseRadiusTiles(int x, int y, bool startWithCenter);
std::vector<MAP_INFO> getBaseRadiusTileInfos(int x, int y, bool startWithCenter);
std::vector<MAP *> getBaseWorkedTiles(int baseId);
std::vector<MAP *> getBaseWorkedTiles(BASE *base);
int getBaseConventionalDefenseValue(int baseId);
std::vector<int> getFactionPrototypes(int factionId, bool includeNotPrototyped);
bool isNativeLandVehicle(int vehicleId);
bool isBaseBuildingColony(int baseId);
int projectBasePopulation(int baseId, int turns);
int getFactionFacilityPopulationLimit(int factionId, int facilityId);
bool isBaseFacilityAvailable(int baseId, int facilityId);
bool isBaseConnectedToRegion(int baseId, int region);
bool isTileConnectedToRegion(int x, int y, int region);
int getXCoordinateByMapIndex(int mapIndex);
int getYCoordinateByMapIndex(int mapIndex);
std::vector<int> getRegionBases(int factionId, int region);
std::vector<int> getRegionSurfaceVehicles(int factionId, int region, bool includeStationed);
std::vector<int> getBaseGarrison(int baseId);
std::vector<int> getFactionBases(int factionId);
double estimateUnitBaseLandNativeProtection(int factionId, int unitId);
double estimateVehicleBaseLandNativeProtection(int factionId, int vehicleId);
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

