#pragma once

#include "main.h"
#include <vector>
#include <set>
#include <map>
#include "robin_hood.h"
#include "engine.h"

// first level trial based defensive structure facilities
FacilityId const TRIAD_DEFENSIVE_FACILITIES[3] = {FAC_PERIMETER_DEFENSE, FAC_NAVAL_YARD, FAC_AEROSPACE_COMPLEX};

int const MAX_RANGE = 40;
int const MAX_RANGE_TILE_COUNT = (1 + 2 * MAX_RANGE) * (1 + 2 * MAX_RANGE);
int const MAX_REACHABLE_LOCATION_RANGE = 40;
int const MAX_REACHABLE_LOCATION_COUNT = (1 + 2 * MAX_REACHABLE_LOCATION_RANGE) * (1 + 2 * MAX_REACHABLE_LOCATION_RANGE);

/// alien units
const std::vector<int> ALIEN_UNITS
{
    BSC_MIND_WORMS,
    BSC_ISLE_OF_THE_DEEP,
    BSC_LOCUSTS_OF_CHIRON,
    BSC_SEALURK,
    BSC_SPORE_LAUNCHER,
    BSC_FUNGAL_TOWER,
};

/// surface type (land or sea)
enum SurfaceType
{
	ST_LAND,
	ST_SEA,
};
const int SurfaceTypeCount = ST_SEA + 1;

enum ENGAGEMENT_MODE
{
	EM_MELEE,
	EM_ARTILLERY,
};
int const ENGAGEMENT_MODE_COUNT = EM_ARTILLERY + 1;

enum COMBAT_MODE
{
	CM_MELEE,
	CM_ARTILLERY_DUEL,
	CM_BOMBARDMENT,
};
const int COMBAT_MODE_COUNT = CM_BOMBARDMENT + 1;

enum COMBAT_TYPE
{
	CT_CON = 0,
	CT_PSI = 1,
};
const int COMBAT_TYPE_COUNT = CT_PSI + 1;

enum MovementType
{
	MT_AIR,
	MT_SEA,
	MT_SEA_NATIVE,
	MT_LAND,
	MT_LAND_NATIVE,
	MT_LAND_EASY,
	MT_LAND_HOVER,
	MT_LAND_NATIVE_HOVER,
};
size_t const MOVEMENT_TYPE_COUNT = MT_LAND_NATIVE_HOVER + 1;
size_t const SEA_MOVEMENT_TYPE_FIRST = MT_SEA;
size_t const SEA_MOVEMENT_TYPE_LAST = MT_SEA_NATIVE;
size_t const SEA_MOVEMENT_TYPE_COUNT = SEA_MOVEMENT_TYPE_LAST - SEA_MOVEMENT_TYPE_FIRST + 1;
size_t const LAND_MOVEMENT_TYPE_FIRST = MT_LAND;
size_t const LAND_MOVEMENT_TYPE_LAST = MT_LAND_NATIVE_HOVER;
size_t const LAND_MOVEMENT_TYPE_COUNT = LAND_MOVEMENT_TYPE_LAST - LAND_MOVEMENT_TYPE_FIRST + 1;
// basic land movement ignores hovering
size_t const BASIC_LAND_MOVEMENT_TYPE_FIRST = MT_LAND;
size_t const BASIC_LAND_MOVEMENT_TYPE_LAST = MT_LAND_NATIVE;
size_t const BASIC_LAND_MOVEMENT_TYPE_COUNT = BASIC_LAND_MOVEMENT_TYPE_LAST - BASIC_LAND_MOVEMENT_TYPE_FIRST + 1;
std::array<MovementType, SEA_MOVEMENT_TYPE_COUNT> const SEA_MOVEMENT_TYPES {MT_SEA, MT_SEA_NATIVE};
std::array<MovementType, LAND_MOVEMENT_TYPE_COUNT> const LAND_MOVEMENT_TYPES {MT_LAND, MT_LAND_NATIVE, MT_LAND_EASY, MT_LAND_HOVER, MT_LAND_NATIVE_HOVER};
std::array<MovementType, BASIC_LAND_MOVEMENT_TYPE_COUNT> const BASIC_LAND_MOVEMENT_TYPES {MT_LAND, MT_LAND_NATIVE};

// array structures
template<class T, int N> class ArrayVector : public std::array<T,N>
{
private:
	int size = 0;
public:
	void clear()
	{
		size = 0;
	}
	void push_back(T element)
	{
		if (size < N) this->at(size++) = element;
	}
	typename std::array<T,N>::const_iterator const begin() const
	{
		return this->cbegin();
	}
	typename std::array<T,N>::const_iterator const end() const
	{
		return this->cbegin() + size;
	}
};
template<class T, int N> class ArrayView
{
private:
	std::array<T,N> const *a;
	int b;
	int e;
public:
	void set(std::array<T,N> const *_a, int _b, int _e)
	{
		a = _a;
		b = _b;
		e = _e;
	}
	typename std::array<T,N>::const_iterator const begin() const
	{
		return a->cbegin() + b;
	}
	typename std::array<T,N>::const_iterator const end() const
	{
		return a->cbegin() + e;
	}
};

/// combination of angle and tile
struct MapAngle
{
	MAP *tile;
	int angle;
};

/**
coordinate location
*/
struct Location
{
	int x;
	int y;
	
	int minAbs() { return std::min(std::abs(x), std::abs(y)); }
	int maxAbs() { return std::max(std::abs(x), std::abs(y)); }
	int absDiff() { return abs(abs(x) - abs(y)); }
	
	Location(int _x, int _y)
	: x(_x), y(_y)
	{}
	Location()
	: Location(-1, -1)
	{}
	
	bool isValid()
	{
		return x != -1 && y != -1;
	}
	
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

struct Resource
{
	double nutrient;
	double mineral;
	double energy;
	
	Resource(double _nutrient, double _mineral, double _energy)
	: nutrient(_nutrient), mineral(_mineral), energy(_energy)
	{}
	
	Resource()
	: Resource(0.0, 0.0, 0.0)
	{}
	
	static Resource combine(Resource const &o1, Resource const &o2)
	{
		return Resource(o1.nutrient + o2.nutrient, o1.mineral + o2.mineral, o1.energy + o2.energy);
	}
	
};

struct Income
{
	double income;
	double incomeGrowth;
	
	Income() : income(0.0), incomeGrowth(0.0) {}
	Income(double _income, double _incomeGrowth) : income(_income), incomeGrowth(_incomeGrowth) {}
	
};

struct RangeTile
{
	int dx;
	int dy;
	MAP *tile = nullptr;
};

class ArithmeticSummaryStatistics
{
private:
	double sumWeight = 0.0;
	double sumWeightValue = 0.0;
public:
	void clear()
	{
		this->sumWeight = 0.0;
		this->sumWeightValue = 0.0;
	}
	void add(double value, double weight = 1.0)
	{
		assert(weight >= 0.0);
		sumWeight += weight;
		sumWeightValue += weight * value;
	}
	double getWeight()
	{
		return sumWeight;
	}
	double getArithmeticSum()
	{
		return sumWeightValue;
	}
	double getArithmeticMean()
	{
		return sumWeight <= 0.0 ? 0.0 : sumWeightValue / sumWeight;
	}
};

class HarmonicSummaryStatistics
{
private:
	double sumWeight = 0.0;
	double sumWeightValue = 0.0;
public:
	void clear()
	{
		this->sumWeight = 0.0;
		this->sumWeightValue = 0.0;
	}
	void add(double value, double weight = 1.0)
	{
		assert(weight >= 0.0);
		assert(value > 0.0);
		sumWeight += weight;
		sumWeightValue += weight * 1.0 / value;
	}
	double getWeight()
	{
		return sumWeight;
	}
	double getHarmonicSum()
	{
		return sumWeightValue;
	}
	double getHarmonicMean()
	{
		return sumWeight <= 0.0 || sumWeightValue <= 0.0 ? 0.0 : 1.0 / (sumWeightValue / sumWeight);
	}
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

std::string getLocationString(Location location);
std::string getLocationString(int tileIndex);
std::string getLocationString(MAP *tile);

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

bool isOnMap(MAP *tile);
bool isOnMap(int x, int y);
Location getLocation(int tileIndex);
Location getLocation(MAP *tile);
int getX(int tileIndex);
int getY(int tileIndex);
int getX(MAP const *tile);
int getY(MAP const *tile);
int getMapTileIndex(int x, int y);
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

std::vector<MapAngle> const getAdjacentMapAngles(MAP *tile);
std::vector<int> const getAdjacentTileIndexes(int tileIndex);
std::vector<MAP *> const getAdjacentTiles(MAP *tile);
std::vector<MAP *> getSideTiles(MAP *tile);

ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> const &getSquareOffsetTiles(MAP *center, int beginIndex, int endIndex);
ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> const &getSquareBlockRadiusTiles(MAP *center, int minRadius, int maxRadius);
ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> const &getRangeTiles(MAP *tile, int range, bool includeCenter);
bool nextRangeTile(MAP *center, int maxRange, bool includeCenter, RangeTile &rangeTile);

std::vector<MAP *> getBaseOffsetTiles(int x, int y, int offsetBegin, int offsetEnd);
std::vector<MAP *> getBaseOffsetTiles(MAP *tile, int offsetBegin, int offsetEnd);
std::vector<MAP *> getBaseAdjacentTiles(int x, int y, bool includeCenter);
std::vector<MAP *> getBaseAdjacentTiles(MAP *tile, bool includeCenter);
std::vector<MAP *> getBaseRadiusTiles(int x, int y, bool includeCenter);
std::vector<MAP *> getBaseRadiusTiles(MAP *tile, bool includeCenter);
ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> const &getBaseExternalRadiusTiles(MAP *tile);
std::vector<MAP *> getBaseRadiusSideTiles(MAP *tile);
std::vector<MAP *> getBaseRadiusAdjacentTiles(MAP *tile);

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
int isBonusAt(MAP *tile);
int getNutrientBonus(MAP *tile);
int getMineralBonus(MAP *tile);
int getEnergyBonus(MAP *tile);
bool isLandmarkBonus(MAP *tile);
int veh_speed_without_roads(int id);
int unit_chassis_speed(int id);
int veh_chassis_speed(int id);
int unit_triad(int id);
double random_double(double scale);
bool isSeaBase(int baseId);
BASE *vehicle_home_base(VEH *vehicle);
MAP *base_square(BASE *base);
bool unit_has_ability(int id, int ability);
bool vehicle_has_ability(int vehicleId, int ability);
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
double getUnitPsiOffenseStrength(int factionId, int unitId);
double getUnitPsiDefenseStrength(int factionId, int unitId);
double getUnitConOffenseStrength(int unitId);
double getUnitConDefenseStrength(int unitId);
double getUnitConArtilleryDuelStrength(int unitId);
double getNewUnitPsiOffenseStrength(int id, int baseId);
double getNewUnitPsiDefenseStrength(int id, int baseId);
double getNewUnitConOffenseStrength(int id, int baseId);
double getNewUnitConDefenseStrength(int id, int baseId);
double getVehiclePsiOffenseStrength(int vehicleId, bool ignoreDamage = false);
double getVehiclePsiDefenseStrength(int vehicleId, bool ignoreDamage = false);
double getVehicleConOffenseStrength(int vehicleId, bool ignoreDamage = false);
double getVehicleConDefenseStrength(int vehicleId, bool ignoreDamage = false);
double getVehicleConArtilleryDuelStrength(int vehicleId);
double getVehicleConOffensePower(int vehicleId);
double getVehicleConDefensePower(int vehicleId);
double getFactionSEPlanetOffenseModifier(int factionId);
double getFactionSEPlanetDefenseModifier(int factionId);
double getPsiCombatBaseOdds(int triad);
bool isCombatUnit(int unitId);
bool isCombatVehicle(int vehicleId);
bool isUtilityVehicle(int vehicleId);
bool isOgreUnit(int unitId);
bool isOgreVehicle(int vehicleId);
double calculatePsiDamageAttack(int id, int enemyId);
double calculatePsiDamageDefense(int id, int enemyId);
VehOrder getVehicleOrder(int vehicleId);
void setVehicleOrder(int vehicleId, int order);
int getBaseMapTileIndex(int baseId);
MAP *getBaseMapTile(int baseId);
int getVehicleMapTileIndex(int vehicleId);
MAP *getVehicleMapTile(int vehicleId);
bool isImprovedTile(MAP *tile);
bool isImprovedTile(int x, int y);
bool isSupplyUnit(int unitId);
bool isSupplyVehicle(int vehicleId);
bool isColonyUnit(int id);
bool isArtifactUnit(int unitId);
bool isArtifactVehicle(int id);
bool isColonyVehicle(int id);
bool isFormerUnit(int unitId);
bool isFormerVehicle(int vehicleId);
bool isFormerVehicle(VEH *vehicle);
bool isConvoyUnit(int unitId);
bool isConvoyVehicle(int vehicleId);
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
bool isActiveArtilleryVehicle(int vehicleId);
bool isLandArtilleryUnit(int unitId);
bool isLandArtilleryVehicle(int vehicleId);
void computeBase(int baseId, bool resetWorkedTiles);
robin_hood::unordered_flat_set<int> getBaseConnectedRegions(int id);
robin_hood::unordered_flat_set<int> getBaseConnectedOceanRegions(int baseId);
double evaluateUnitConDefenseEffectiveness(int id);
double evaluateUnitConOffenseEffectiveness(int id);
double evaluateUnitPsiDefenseEffectiveness(int id);
double evaluateUnitPsiOffenseEffectiveness(int id);
double getBaseDefenseMultiplier(int baseId, int extendedTriad);
double getBaseDefenseMultiplier(int baseId, int attackerUnitId, int defenderUnitId);
int estimateBaseItemProductionTime(int baseId, int item);
int estimateBaseProductionTurnsToComplete(int id);
MAP *getBaseWorkerTile(int baseId, int workerNumber);
std::vector<MAP *> getBaseWorkableTiles(int baseId, bool startWithCenter);
std::vector<MAP *> getBaseWorkedTiles(int baseId);
int getBaseWorkerCount(int baseId);
bool isBaseWorkedTile(int baseId, int x, int y);
bool isBaseWorkedTile(int baseId, MAP *tile);
std::vector<int> getFactionUnitIds(int factionId, bool includeObsolete, bool includeNotPrototyped);
int getFactionFastestFormerUnitId(int factionId);
bool isVehicleNativeLand(int vehicleId);
bool isBaseBuildingColony(int baseId);
int getFacilityPopulationLimit(int factionId, int facilityId);
bool isBaseFacilityAvailable(int baseId, int facilityId);
bool isBaseConnectedToRegion(int baseId, int region);
bool isTileConnectedToRegion(int x, int y, int region);
std::vector<int> getRegionBases(int factionId, int region);
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
int getBasePolicePower(int baseId, bool police2x);
int getBasePoliceAllowed(int baseId);
int getBasePoliceCount(int baseId, bool police2x);
char *getVehicleUnitName(int vehicleId);
int getVehicleUnitPlan(int vehicleId);
double getBaseNativeProtection(int baseId);
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
bool isUnfriendlyTerritory(int factionId, MAP* tile);
bool isNeutralTerritory(int factionId, MAP* tile);
bool isHostileTerritory(int factionId, MAP* tile);
bool isUnitHasAbility(int unitId, int ability);
bool isVehicleHasAbility(int vehicleId, int ability);
bool isScoutUnit(int unitId);
bool isScoutVehicle(int vehicleId);
bool isPodPoppingUnit(int unitId);
bool isPodPoppingVehicle(int vehicleId);
bool isTargettedLocation(MAP *tile);
bool isFactionTargettedLocation(MAP *tile, int factionId);
double getNativePsiAttackStrength(int triad);
double getMoraleMultiplier(int morale);
int getFactionSEMoraleBonus(int factionId);
int getVehicleMorale(int vehicleId);
int getNewVehicleMorale(int unitId, int baseId);
double getVehicleMoraleMultiplier(int vehicleId);
double getBaseIntrinsicPsiDefenseMultiplier();
bool isWithinFriendlySensorRange(int factionId, MAP *tile);
bool isOffenseSensorBonusApplicable(int factionId, MAP *tile);
bool isDefenseSensorBonusApplicable(int factionId, MAP *tile);
int getRegionPodCount(int x, int y, int range, int region);
MAP *getNearbyItemLocation(int x, int y, int range, int item);
bool isMapRangeHasItem(int x, int y, int range, uint32_t item);
bool isVehicleHealing(int vehicleId);
bool isVehicleInRegion(int vehicleId, int region);
bool isProbeUnit(int unitId);
bool isProbeVehicle(int vehicleId);
double battleCompute(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat);
double battleComputeStack(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat);
int getVehicleSpeedWithoutRoads(int id);
VehWeapon getFactionBestWeapon(int factionId);
VehWeapon getFactionBestWeapon(int factionId, int limit);
VehArmor getFactionBestArmor(int factionId);
VehArmor getFactionBestArmor(int factionId, int limit);
int getBaseIdAt(int x, int y);
bool isNativeUnit(int unitId);
bool isNativeVehicle(int vehicleId);
double getPercentageBonusMultiplier(int percentageBonus);
bool isMineBonus(MAP *tile);
std::vector<int> selectVehicles(const VehicleFilter filter);
bool isNextToRegion(int x, int y, int region);
int getSeaTransportInStack(int vehicleId);
bool isSeaTransportInStack(int vehicleId);
bool isSeaTransportAt(MAP *tile, int factionId);
bool isAvailableSeaTransportAt(MAP *tile, int factionId);
int countPodsInBaseRadius(int x, int y);
bool isBlocked(int factionId, MAP *tile);
bool isZoc(int factionId, MAP *fromTile, MAP *toTile);
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
bool isVehicleOnTransport(int vehicleId, int transportVehicleId);
void board(int vehicleId, int transportVehicleId);
void board(int vehicleId);
void unboard(int vehicleId);
int getTransportCapacity(int transportVehicleId);
int getTransportUsedCapacity(int transportVehicleId);
int getTransportRemainingCapacity(int transportVehicleId);
bool isTransportHasCapacity(int transportVehicleId);
int getVehicleMaxPower(int vehicleId);
int getVehiclePower(int vehicleId);
double getVehicleRelativeDamage(int vehicleId);
double getVehicleRelativePower(int vehicleId);
int getVehicleHitPoints(int vehicleId, bool psiCombat);
int getBaseGrowthRate(int baseId);
bool isCommlink(int factionId1, int factionId2);
bool isVendetta(int factionId1, int factionId2);
bool isPact(int factionId1, int factionId2);
bool isTreaty(int factionId1, int factionId2);
bool isFriendly(int factionId1, int factionId2);
bool isUnfriendly(int factionId1, int factionId2);
bool isHostile(int factionId1, int factionId2);
bool isNeutral(int factionId1, int factionId2);
int getVehicleMaxPsiHP(int vehicleId);
int getVehicleCurrentPsiHP(int vehicleId);
int getVehicleMaxConHP(int vehicleId);
int getVehicleCurrentConHP(int vehicleId);
int getUnitSlotById(int unitId);
int getUnitIdBySlot(int factionId, int slot);
int getUnitIndex(int factionId, int unitId);
int getFactionIdByUnitIndex(int unitIndex);
int getUnitIdByUnitIndex(int unitIndex);
int getBasePoliceSuppressedDrones(int baseId);
bool isReactorIgnoredInConventionalCombat();
bool isReactorIgnoredInCombat(bool psiCombat);
bool isInfantryUnit(int unitId);
bool isInfantryVehicle(int vehicleId);
bool isPolice2xUnit(int unitId, int factionId);
bool isPolice2xVehicle(int vehicleId);
bool isInfantryPolice2xUnit(int unitId, int factionId);
bool isInfantryPolice2xVehicle(int vehicleId);
bool isPsiCombat(int attackerUnitId, int defenderUnitId);
double getAlienTurnStrengthModifier();
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
void longRangeFire(int vehicleId, int offsetIndex);
void longRangeFire(int vehicleId, MAP *attackLocation);
RepairInfo getVehicleRepairInfo(int vehicleId, MAP *tile);
bool isObsoleteUnit(int unitId, int factionId);
bool isPrototypedUnit(int unitId);
bool isActiveUnit(int unitId);
bool isAvailableUnit(int unitId, int factionId, bool includeObsolete);
CAbility *getAbility(int abilityId);
void obsoleteUnit(int unitId, int factionId);
void reinstateUnit(int unitId, int factionId);
int divideIntegerRoundUp(int divident, int divisor);
CFacility *getFacility(int facilityId);
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
bool isMobileUnit(int unitId);
bool isAtAirbase(int vehicleId);
int getClosestHostileBaseRange(int factionId, MAP *tile);
double getEuclidianDistance(int x1, int y1, int x2, int y2);
int getVehicleUnitCost(int vehicleId);
int getVehicleRemainingMovement(int vehicleId);
bool isAllowedMove(int vehicleId, MAP *srcTile, MAP *dstTile);
bool isZocAffectedUnit(int unitId);
bool isZocAffectedVehicle(int vehicleId);
bool isZocIgnoringUnit(int unitId);
bool isZocIgnoringVehicle(int vehicleId);
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
bool isUnitCanInitiateArtilleryDuel(int attackerUnitId, int defenderUnitId);
bool isUnitCanInitiateBombardment(int attackerUnitId, int defenderUnitId);
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
double getBaseEnergyEfficiencyCoefficient(int baseId);
double getBaseEnergyMultiplier(int baseId);
double getBaseEconomyMultiplier(int baseId);
double getBaseLabsMultiplier(int baseId);
double getBasePsychMultiplier(int baseId);
bool isLandVechileMoveAllowed(int vehicleId, MAP *from, MAP *to);
int getRange(int x1, int y1, int x2, int y2);
int getRange(int tile1Index, int tile2Index);
int getRange(MAP const *tile1, MAP const *tile2);
int getVectorDist(int x1, int y1, int x2, int y2);
int getVectorDist(int tile1Index, int tile2Index);
int getVectorDist(MAP const *tile1, MAP const *tile2);
int getProximity(int x1, int y1, int x2, int y2);
double getEuqlideanDistanceSquared(MAP *origin, MAP *destination);
double getEuqlideanDistance(MAP *origin, MAP *destination);
double getDiagonalDistance(MAP *tile1, MAP *tile2);
int getDiagonalDistanceDoubled(MAP *tile1, MAP *tile2);
Location getLocationByAngle(int x, int y, int angle);
int getTileIndexByAngle(int tileIndex, int angle);
MAP *getTileByAngle(MAP *tile, int angle);
int getAngleByTile(MAP *tile, MAP *anotherTile);
bool isPodAt(MAP *tile);
std::vector<int> getTransportPassengers(int transportVehicleId);
double getLocationBombardmentMaxRelativeDamage(MAP *tile);
double getVehicleBombardmentMaxRelativeDamage(int vehicleId);
double getVehicleBombardmentRemainingRelativeDamage(int vehicleId);
int getBasePopulationLimit(int baseId);
int getBaseRadiusLayer(MAP *baseTile, MAP *tile);
bool isWithinBaseRadius(MAP *baseTile, MAP *tile);
std::vector<MAP *> getFartherBaseRadiusTiles(MAP *baseTile, MAP *tile);
bool isFactionHasProject(int factionId, int facilityId);
Budget getBaseBudgetIntake(int baseId);
Budget getBaseBudgetIntake2(int baseId);
double getBasePopulationGrowth(int baseId);
double getBasePopulationGrowthIncrease(int baseId, double nutrientSurplusIncrease);
double getBasePopulationGrowthRate(int baseId);
int getBaseTurnsToPopulation(int baseId, int population);
int getMovementRateAlongTube();
int getMovementRateAlongRoad();
bool isInfantryDefensiveUnit(int unitId);
bool isInfantryDefensiveVehicle(int vehicleId);
bool isInfantryDefensivePolice2xUnit(int unitId, int factionId);
bool isInfantryDefensivePolice2xVehicle(int vehicleId);
bool isBaseDefenderVehicle(int vehicleId);
bool isInterceptorUnit(int unitId);
bool isInterceptorVehicle(int vehicleId);
int getUnitSupport(int unitId);
bool getVehicleSupport(int vehicleId);
bool isUnitRequiresSupport(int unitId);
bool isVehicleRequiresSupport(int vehicleId);
bool isFactionHasAbility(int faction, VehAbl abl);
bool isFactionHasTech(int factionId, int tech);
int getFactionTechCount(int factionId);
bool isUnprotectedArtifact(int vehicleId);
double average(std::vector<double> v);
bool isBattleOgreUnit(int unitId);
bool isBattleOgreVehicle(int unitId);
int getUnitMoveRate(int factionId, int unitId);
int getUnitSpeed(int factionId, int unitId);
int getVehicleMoveRate(int vehicleId);
int getVehicleSpeed(int vehicleId);
bool isTileAccessesWater(MAP *tile);
bool isBaseAccessesWater(int baseId);
bool isBaseCanBuildShip(int baseId);
bool isUnitZocRestricted(int unitId);
bool isVehicleZocRestricted(int vehicleId);
std::vector<int> getBaseFacilities(int baseId);
std::vector<int> getBaseProjects(int baseId);
Resource getBaseWorkerResourceIntake(int baseId, MAP *tile);
Resource getBaseResourceIntake2(int baseId);
bool isLandRegion(MAP *tile, bool includePolar = false);
bool isSeaRegion(MAP *tile, bool includePolar = false);
bool isPolarRegion(MAP *tile);
int getBaseNextUnitSupport(int baseId, int unitId);
int getBaseMoraleModifier(int baseId, int extendedTriad);
bool isFactionCanBuildAirOffense(int factionId);
bool isFactionCanBuildAirDefense(int factionId);
bool isFactionCanBuildMelee(int factionId, int type, int triad);
bool isRoughTerrain(MAP *tile);
bool isOpenTerrain(MAP *tile);
std::set<int> getBaseSeaRegions(int baseId);
int getClosestNotOwnedTileRange(int factionId, MAP *tile, int minRadius, int maxRadius);
int getClosestNotOwnedOrSeaTileRange(int factionId, MAP *tile, int minRadius, int maxRadius);
int getUnitOffenseExtendedTriad(int unitId, int enemyUnitId);

