
#include <iostream>
#include <iomanip>
#include <sstream>
#include <numeric>
#include "wtp_terranx_enum.h"
#include "wtp_terranx.h"
#include "wtp_game.h"
#include "wtp_mod.h"
#include "wtp_aiTask.h"

ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> RANGE_TILES;

// Location

bool operator==(const Location &o1, const Location &o2)
{
	return o1.x == o2.x && o1.y == o2.y;
}
bool operator!=(const Location &o1, const Location &o2)
{
	return o1.x != o2.x || o1.y != o2.y;
}

std::string getLocationString(Location location)
{
	std::ostringstream ss;
	
	ss
		<< "("
		<< std::dec << std::setw(3) << std::setfill(' ') << location.x
		<< ","
		<< std::dec << std::setw(3) << std::setfill(' ') << location.y
		<< ")"
	;
	
    return ss.str();
    
}

std::string getLocationString(int tileIndex)
{
	// allow returning for incorrect index
	
	if (!(tileIndex >= 0 && tileIndex < *MapAreaTiles))
		return "-nullptr-";
	
	return getLocationString(getLocation(tileIndex));
	
}

std::string getLocationString(MAP *tile)
{
	// allow returning for nullptr
	
	if (tile == nullptr)
		return "-nullptr-";
	
	return getLocationString(getLocation(tile));
	
}

// =======================================================
// tile offsets
// =======================================================

/*
Returns index for offset or -1 if not found.
*/
int getOffsetIndex(int dx, int dy)
{
	// wrap dx

	if (*MapToggleFlat == 0 && abs(dx) > *MapHalfX)
	{
		if (dx > 0)
		{
			dx -= *MapAreaX;
		}
		else
		{
			dx += *MapAreaX;
		}
	}

	// verify offset has same parity

	if ((dx & 1) != (dy & 1))
		return -1;

	// verify offset is in range

	if (abs(dx) + abs(dy) > 8)
		return -1;

	// search offset

	for (int offsetIndex = 0; offsetIndex < RANGE_OFFSET_COUNT[8]; offsetIndex++)
	{
		if (OFFSETS[offsetIndex][0] == dx && OFFSETS[offsetIndex][1] == dy)
		{
			return offsetIndex;
		}

	}

	// not found

	return -1;

}

/*
Returns index for offset between two coordinates.
*/
int getOffsetIndex(int x1, int y1, int x2, int y2)
{
	int dx = x2 - x1;
	int dy = y2 - y1;

	// wrap dx

	if (*MapToggleFlat == 0 && abs(dx) > *MapHalfX)
	{
		if (dx > 0)
		{
			dx -= *MapAreaX;
		}
		else
		{
			dx += *MapAreaX;
		}
	}

	// return offset index

	return getOffsetIndex(dx, dy);

}

/*
Returns index for offset between two tiles.
*/
int getOffsetIndex(MAP *tile1, MAP *tile2)
{
	assert(isOnMap(tile1));
	assert(isOnMap(tile2));
	
	return getOffsetIndex(getX(tile1), getY(tile1), getX(tile2), getY(tile2));
	
}

// =======================================================
// MAP conversions
// =======================================================

/**
Checks if location is on map (valid location).
*/
bool isOnMap(MAP *tile)
{
	return tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles;
}

/**
Checks if location is on map (valid location).
*/
bool isOnMap(int x, int y)
{
	return (((x ^ y) & 0x1) == 0x0 && (x >= 0 && x < *MapAreaX) && (y >= 0 && y < *MapAreaY));
}

/**
Computes location by map tile index.
*/
Location getLocation(int tileIndex)
{
	assert(tileIndex >= 0 && tileIndex < *MapAreaTiles);
	
	int y = tileIndex / (*MapHalfX);
	int x = (tileIndex % (*MapHalfX)) * 2 + (y % 2);
	
	return {x, y};
	
}

/**
Computes location by map tile.
*/
Location getLocation(MAP *tile)
{
	assert(isOnMap(tile));
	
	int mapIndex = tile - *MapTiles;
	int y = mapIndex / (*MapHalfX);
	int x = (mapIndex % (*MapHalfX)) * 2 + (y % 2);
	
	return {x, y};
	
}

/**
Computes location coordinates by map tile index.
*/
int getX(int tileIndex)
{
	assert(tileIndex >= 0 && tileIndex < *MapAreaTiles);
	return (tileIndex % (*MapHalfX)) * 2 + ((tileIndex / (*MapHalfX)) % 2);
}
int getY(int tileIndex)
{
	assert(tileIndex >= 0 && tileIndex < *MapAreaTiles);
	return tileIndex / (*MapHalfX);
}

/**
Computes location coordinates by map tile.
*/
int getX(MAP const *tile)
{
	return getX(tile - *MapTiles);
}
int getY(MAP const *tile)
{
	return getY(tile - *MapTiles);
}

/**
Computes map tile index by location coordinates.
*/
int getMapTileIndex(int x, int y)
{
	assert(isOnMap(x, y));
	
	return ((*MapHalfX) * y + x / 2);
	
}

/**
Computes map tile by location coordinates.
*/
MAP *getMapTile(int x, int y)
{
	if (!isOnMap(x, y))
		return nullptr;
	
	return *MapTiles + ((*MapHalfX) * y + x / 2);
	
}

/**
Computes location difference for two map tiles.
*/
Location getDelta(MAP *tile1, MAP *tile2)
{
	Location location1 = getLocation(tile1);
	Location location2 = getLocation(tile2);
	
	int dx = location2.x - location1.x;
	int dy = location2.y - location1.y;
	
	// wrap x coordinate for cylindric map
	
	if (!*MapToggleFlat)
	{
		if (dx < -*MapHalfX)
		{
			dx += *MapAreaX;
		}
		else if (dx > +*MapHalfX)
		{
			dx -= *MapAreaX;
		}
	}
	
	return {dx, dy};
	
}

/**
Computes location absolute difference for two map tiles.
*/
Location getAbsoluteDelta(MAP *tile1, MAP *tile2)
{
	Location location1 = getLocation(tile1);
	Location location2 = getLocation(tile2);
	
	int dx = abs(location2.x - location1.x);
	int dy = abs(location2.y - location1.y);
	
	// wrap x coordinate for cylindric map
	
	if (!*MapToggleFlat && dx > *MapHalfX)
	{
		dx = *MapAreaX - dx;
	}
	
	return {dx, dy};
	
}

/*
Converts diagonal coordinates to rectangular.
shift == true: shift everything to make them non-negative
*/
Location getRectangularCoordinates(Location diagonal, bool shift)
{
	int x = (+ diagonal.x - diagonal.y) / 2;
	int y = (+ diagonal.x + diagonal.y) / 2;
	if (shift) {x += *MapAreaY / 2;}
	return {x, y};
}
Location getRectangularCoordinates(Location diagonal)
{
	return getRectangularCoordinates(diagonal, false);
}
Location getRectangularCoordinates(MAP *tile, bool shift)
{
	return getRectangularCoordinates(getLocation(tile), shift);
}

/*
Converts rectangular coordinates to diagonal.
*/

Location getDiagonalCoordinates(Location rectangular)
{
	int x = (+ rectangular.x + rectangular.y);
	int y = (- rectangular.x + rectangular.y);
	return {x, y};
}

// =======================================================
// MAP conversions - end
// =======================================================

// =======================================================
// iterating surrounding locations - end
// =======================================================

/**
Returns complete angle array with adjacent tiles.
Tiles not on the map are nullptr.
*/
std::vector<MapAngle> const getAdjacentMapAngles(MAP *tile)
{
	std::vector<MapAngle> adjacentMapAngles;
	
	for (int angle = 0; angle < ANGLE_COUNT; angle++)
	{
		MAP *adjacentTile = getTileByAngle(tile, angle);
		
		if (adjacentTile == nullptr)
			continue;
		
		adjacentMapAngles.push_back({adjacentTile, angle});
		
	}
	
	return adjacentMapAngles;
	
}

/**
Returns valid adjacent tile indexes.
*/
std::vector<int> const getAdjacentTileIndexes(int tileIndex)
{
	int x = getX(tileIndex);
	int y = getY(tileIndex);
	
	std::vector<int> adjacentTileIndexes;
	
	for (int angle = 0; angle < TABLE_next_cell_count; angle++)
	{
		int offsetX = TABLE_next_cell_x[angle];
		int offsetY = TABLE_next_cell_y[angle];
		
		int nextCellX = wrap(x + offsetX);
		int nextCellY = y + offsetY;
		
		if (!isOnMap(nextCellX, nextCellY))
			continue;
		
		adjacentTileIndexes.push_back(getMapTileIndex(nextCellX, nextCellY));
		
	}
	
	return adjacentTileIndexes;
	
}

/**
Returns valid adjacent tiles.
*/
std::vector<MAP *> const getAdjacentTiles(MAP *tile)
{
	Location location = getLocation(tile);
	int x = location.x;
	int y = location.y;
	
	std::vector<MAP *> adjacentTiles;
	
	for (int angle = 0; angle < TABLE_next_cell_count; angle++)
	{
		int offsetX = TABLE_next_cell_x[angle];
		int offsetY = TABLE_next_cell_y[angle];
		
		int nextCellX = wrap(x + offsetX);
		int nextCellY = y + offsetY;
		
		if (!isOnMap(nextCellX, nextCellY))
			continue;
		
		adjacentTiles.push_back(getMapTile(nextCellX, nextCellY));
		
	}
	
	return adjacentTiles;
	
}

/*
Returns tiles sharing a side with given tile.
*/
std::vector<MAP *> getSideTiles(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	std::vector<MAP *> tiles;

	for (int angle = 0; angle < TABLE_next_cell_count; angle += 2)
	{
		int offsetX = TABLE_next_cell_x[angle];
		int offsetY = TABLE_next_cell_y[angle];

		int nextCellX = wrap(x + offsetX);
		int nextCellY = y + offsetY;

		if (!isOnMap(nextCellX, nextCellY))
			continue;

		tiles.push_back(getMapTile(nextCellX, nextCellY));

	}

	return tiles;

}

/**
Returns tiles from beginIndex (inclusive) to endIndex (exclusive).
*/
ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> const &getSquareOffsetTiles(MAP *center, int beginIndex, int endIndex)
{
	assert(isOnMap(center));
	
	int x = getX(center);
	int y = getY(center);
	
	RANGE_TILES.clear();
	
	for (int index = beginIndex; index < endIndex; index++)
	{
		int offsetX = TABLE_square_offset_x[index];
		int offsetY = TABLE_square_offset_y[index];
		
		int cellX = wrap(x + offsetX);
		int cellY = y + offsetY;
		
		if (!isOnMap(cellX, cellY))
			continue;
		
		RANGE_TILES.push_back(getMapTile(cellX, cellY));
		
	}
	
	return RANGE_TILES;
	
}

/**
Returns tiles within given index from given center.
Uses vanilla TABLE_square_offset.
*/
ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> const &getSquareBlockTiles(MAP *center, int minRadius, int maxRadius)
{
	assert(isOnMap(center));
	assert(minRadius >= 0 && minRadius < TABLE_square_block_radius_count);
	assert(maxRadius >= 0 && maxRadius < TABLE_square_block_radius_count);
	
	int x = getX(center);
	int y = getY(center);
	
	int minIndex = (minRadius == 0 ? 0 : TABLE_square_block_radius[minRadius - 1]);
	int maxIndex = TABLE_square_block_radius[maxRadius];
	
	RANGE_TILES.clear();
	
	for (int index = minIndex; index < maxIndex; index++)
	{
		int offsetX = TABLE_square_offset_x[index];
		int offsetY = TABLE_square_offset_y[index];
		
		int cellX = wrap(x + offsetX);
		int cellY = y + offsetY;
		
		if (!isOnMap(cellX, cellY))
			continue;
		
		RANGE_TILES.push_back(getMapTile(cellX, cellY));
		
	}
	
	return RANGE_TILES;
	
}

/**
Returns tiles within given radius range from given center.
Uses vanilla TABLE_square_offset.
minRadius = 0-8, inclusive
maxRadius = 0-8, inclusive
*/
ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> const &getSquareBlockRadiusTiles(MAP *center, int minRadius, int maxRadius)
{
	assert(isOnMap(center));
	assert(minRadius >= 0 && minRadius < TABLE_square_block_radius_count);
	assert(maxRadius >= 0 && maxRadius < TABLE_square_block_radius_count);
	
	int x = getX(center);
	int y = getY(center);
	
	int minIndex = (minRadius == 0 ? 0 : TABLE_square_block_radius[minRadius - 1]);
	int maxIndex = TABLE_square_block_radius[maxRadius];
	
	RANGE_TILES.clear();
	
	for (int index = minIndex; index < maxIndex; index++)
	{
		int offsetX = TABLE_square_offset_x[index];
		int offsetY = TABLE_square_offset_y[index];
		
		int cellX = wrap(x + offsetX);
		int cellY = y + offsetY;
		
		if (!isOnMap(cellX, cellY))
			continue;
		
		RANGE_TILES.push_back(getMapTile(cellX, cellY));
		
	}
	
	return RANGE_TILES;
	
}

/*
Returns tiles withing given range from given center.
Returns reference to global variable. Not safe for nested use.
*/
ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> const &getRangeTiles(MAP *tile, int range, bool includeCenter)
{
//	Profiling::start("- getRangeTiles");
	
	RANGE_TILES.clear();
	
	if (!(range >= 0 && range < MAX_RANGE))
	{
		debug("ERROR: assert failed: (range >= 0 && range < MAX_RANGE)\n");flushlog();
		return RANGE_TILES;
	}
	
	int squareBlockRange = std::min(TABLE_square_block_radius_count - 1, range);
	
	// populate range < TABLE_square_block_radius_count
	
	getSquareBlockRadiusTiles(tile, (includeCenter ? 0 : 1), squareBlockRange);
	
	// use normal computation
	
	int x = getX(tile);
	int y = getY(tile);
	
	for (int ringRange = squareBlockRange + 1; ringRange <= range; ringRange++)
	{
		int dMax = 2 * ringRange;
		
		for (int i = 0; i < dMax; i++)
		{
			int x0 = wrap(x - dMax + i);
			int y0 = y + 0 - i;
			
			if (isOnMap(x0, y0))
			{
				MAP *tile0 = getMapTile(x0, y0);
				RANGE_TILES.push_back(tile0);
			}
			
			int x1 = wrap(x + 0 + i);
			int y1 = y - dMax + i;
			
			if (isOnMap(x1, y1))
			{
				MAP *tile1 = getMapTile(x1, y1);
				RANGE_TILES.push_back(tile1);
			}
			
			int x2 = wrap(x + dMax - i);
			int y2 = y + 0 + i;
			
			if (isOnMap(x2, y2))
			{
				MAP *tile2 = getMapTile(x2, y2);
				RANGE_TILES.push_back(tile2);
			}
			
			int x3 = wrap(x + 0 - i);
			int y3 = y + dMax - i;
			
			if (isOnMap(x3, y3))
			{
				MAP *tile3 = getMapTile(x3, y3);
				RANGE_TILES.push_back(tile3);
			}
			
		}
		
	}
	
//	Profiling::stop("- getRangeTiles");
	return RANGE_TILES;
	
}

/*
Returns next range tile by given tile.
*/
bool nextRangeTile(MAP *center, int maxRange, bool includeCenter, RangeTile &rangeTile)
{
//	Profiling::start("- getNextRangeTile");
	
	int centerIndex = center - *MapTiles;
	int centerY = centerIndex / (*MapHalfX);
	int centerX = (centerIndex % (*MapHalfX)) * 2 + (centerY % 2);
	
	if (rangeTile.tile == nullptr)
	{
		rangeTile.dx = 0;
		rangeTile.dy = 0;
		rangeTile.tile = center;
		
		if (includeCenter)
		{
			return true;
		}
		
	}
	
	int x;
	int y;
	bool onMap = false;
	while (!onMap)
	{
		if (rangeTile.tile == center)
		{
			rangeTile.dx = +0;
			rangeTile.dy = -2;
		}
		else if (rangeTile.dx >= 0 && rangeTile.dy < 0)
		{
			rangeTile.dx++;
			rangeTile.dy++;
		}
		else if (rangeTile.dx > 0 && rangeTile.dy >= 0)
		{
			rangeTile.dx--;
			rangeTile.dy++;
		}
		else if (rangeTile.dx <= 0 && rangeTile.dy > 0)
		{
			rangeTile.dx--;
			rangeTile.dy--;
		}
		else if (rangeTile.dx < 0 && rangeTile.dy <= 0)
		{
			rangeTile.dx++;
			rangeTile.dy--;
		}
		
		// advance to next range
		
		if (rangeTile.dx == 0 && rangeTile.dy < 0)
		{
			rangeTile.dy -= 2;
			
			// verify max range
			
			if ((-rangeTile.dy) / 2 > maxRange)
			{
				return false;
			}
			
		}
		
		x = wrap(centerX + rangeTile.dx);
		y = centerY + rangeTile.dy;
		onMap = x >= 0 && x < *MapAreaX && y >= 0 && y < *MapAreaY && ((x + y) & 1) == 0;
		
	}
	
	int rangeTileIndex = (x + *MapAreaX * y) / 2;
	rangeTile.tile = *MapTiles + rangeTileIndex;
	
	return true;
	
}

std::vector<MAP *> getBaseOffsetTiles(int x, int y, int offsetBegin, int offsetEnd)
{
	std::vector<MAP *> baseOffsetLocations;

	for (int offset = offsetBegin; offset < offsetEnd; offset++)
	{
		MAP *baseOffsetLocation = getMapTile(wrap(x + BASE_TILE_OFFSETS[offset][0]), y + BASE_TILE_OFFSETS[offset][1]);

		if (baseOffsetLocation == nullptr)
			continue;

		baseOffsetLocations.push_back(baseOffsetLocation);

	}

	return baseOffsetLocations;

}

std::vector<MAP *> getBaseOffsetTiles(MAP *tile, int offsetBegin, int offsetEnd)
{
	assert(isOnMap(tile));
	return getBaseOffsetTiles(getX(tile), getY(tile), offsetBegin, offsetEnd);
}

std::vector<MAP *> getBaseAdjacentTiles(int x, int y, bool includeCenter)
{
	assert(isOnMap(x, y));
	return getBaseOffsetTiles(x, y, (includeCenter ? 0 : 1), OFFSET_COUNT_ADJACENT);
}

std::vector<MAP *> getBaseAdjacentTiles(MAP *tile, bool includeCenter)
{
	assert(isOnMap(tile));
	return getBaseAdjacentTiles(getX(tile), getY(tile), includeCenter);
}

std::vector<MAP *> getBaseRadiusTiles(int x, int y, bool includeCenter)
{
	assert(isOnMap(x, y));
	return getBaseOffsetTiles(x, y, (includeCenter ? 0 : 1), OFFSET_COUNT_RADIUS);
}

std::vector<MAP *> getBaseRadiusTiles(MAP *tile, bool includeCenter)
{
	assert(isOnMap(tile));
	return getBaseRadiusTiles(getX(tile), getY(tile), includeCenter);
}

ArrayVector<MAP *, MAX_RANGE_TILE_COUNT> const &getBaseExternalRadiusTiles(MAP *center)
{
	return getSquareOffsetTiles(center, TABLE_square_block_radius_base_internal, TABLE_square_block_radius_base_external);
}

/*
Returns tiles outside of base radius with sharing side with radius.
*/
std::vector<MAP *> getBaseRadiusSideTiles(MAP *tile)
{
	return getBaseOffsetTiles(tile, OFFSET_COUNT_RADIUS, OFFSET_COUNT_RADIUS_SIDE);
}

/*
Returns tiles outside of base radius with sharing side with radius.
*/
std::vector<MAP *> getBaseRadiusAdjacentTiles(MAP *tile)
{
	return getBaseOffsetTiles(tile, OFFSET_COUNT_RADIUS, OFFSET_COUNT_RADIUS_CORNER);
}

// =======================================================
// iterating surrounding locations - end
// =======================================================

bool has_armor(int factionId, int armorId)
{
	return isFactionHasTech(factionId, Armor[armorId].preq_tech);
}

bool has_reactor(int factionId, int reactor)
{
	return isFactionHasTech(factionId, Reactor[reactor - 1].preq_tech);
}

/*
Calculates mineral cost for given item at base.
Base cost modifying facilites (Skunkworks and Brood Pits) are taken into account.
*/
int getBaseMineralCost(int baseId, int item)
{
	assert(baseId >= 0 && baseId < *BaseCount);

	BASE *base = &(Bases[baseId]);

	int mineralCost = (item >= 0 ? tx_veh_cost(item, baseId, 0) : Facility[-item].cost) * mod_cost_factor(base->faction_id, RSC_MINERAL, -1);

	return mineralCost;

}

/**
Calculates cost for given item at base.
Base cost modifying facilites (Skunkworks and Brood Pits) are taken into account.
*/
int getBaseItemCost(int baseId, int item)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	
	BASE *base = getBase(baseId);
	
	int itemCost;
	
	if (item == - FAC_STOCKPILE_ENERGY)
	{
		itemCost = base->mineral_surplus;
	}
	else if (item >= 0)
	{
		itemCost = tx_veh_cost(item, baseId, 0) / mod_cost_factor(base->faction_id, RSC_MINERAL, -1);
	}
	else
	{
		itemCost = Facility[-item].cost;
	}
	
	return itemCost;
	
}

int veh_triad(int id) {
    return unit_triad(Vehicles[id].unit_id);
}

int veh_speed_without_roads(int id) {
    return getVehicleSpeed(id) / Rules->move_rate_roads;
}

int unit_chassis_speed(int id)
{
	return Chassis[Units[id].chassis_id].speed;
}

int veh_chassis_speed(int id)
{
	VEH *vehicle = &Vehicles[id];
	UNIT *unit = &Units[vehicle->unit_id];
	CChassis *chassis = &Chassis[unit->chassis_id];
	return chassis->speed;
}

int unit_triad(int id) {
    int triad = Chassis[Units[id].chassis_id].triad;
    assert(triad == TRIAD_LAND || triad == TRIAD_SEA || triad == TRIAD_AIR);
    return triad;
}

double random_double(double scale) {
    return scale * ((double)rand() / (double)(RAND_MAX + 1));
}

bool isSeaBase(int baseId) {
    MAP* sq = mapsq(Bases[baseId].x, Bases[baseId].y);
    return is_ocean(sq);
}

BASE *vehicle_home_base(VEH *vehicle) {
    return (vehicle->home_base_id >= 0 ? &Bases[vehicle->home_base_id] : NULL);
}

MAP *base_square(BASE *base) {
    return mapsq(base->x, base->y);
}

bool unit_has_ability(int id, int ability) {
    return Units[id].ability_flags & ability;
}

bool vehicle_has_ability(int vehicleId, int ability) {
    return Units[Vehicles[vehicleId].unit_id].ability_flags & ability;
}

int map_rainfall(MAP *tile) {
	int rainfall;
	if (tile->climate & TILE_MOIST) {
		rainfall = 1;
	}
	else if (tile->climate & TILE_RAINY) {
		rainfall = 2;
	}
	else {
		rainfall = 0;
	}
	return rainfall;
}

int map_level(MAP *tile) {
	return tile->climate >> 5;
}

int map_elevation(MAP *tile) {
	return map_level(tile) - ALT_SHORE_LINE;
}

int map_rockiness(MAP *tile) {
	return ((tile->val3 & TILE_ROCKY) ? 2 : ((tile->val3 & TILE_ROLLING) ? 1 : 0));
}

/*
Safe check for tile having base.
NULL pointer returns false.
*/
bool map_base(MAP *tile) {
	return (tile && (tile->items & BIT_BASE_IN_TILE));
}

/*
Safe check for tile having item.
NULL pointer returns false.
*/
bool map_has_item(MAP *tile, uint32_t item) {
	return (tile && (tile->items & item));
}

/*
Safe check for tile having landmark.
NULL pointer returns false.
*/
bool map_has_landmark(MAP *tile, int landmark)
{
	// null pointer
	
	if (tile == nullptr)
		return false;
	
	// ladmark should be present
	
	if ((tile->landmarks & landmark) == 0)
		return false;
	
	// special conditions
	
	switch (landmark)
	{
	case LM_CRATER:
		return (tile->code_at() < 9);
		break;
	
	case LM_VOLCANO:
		return (tile->code_at() < 9);
		break;
	
	case LM_JUNGLE:
		return (!is_ocean(tile));
		break;
	
	case LM_URANIUM:
		return true;
		break;
	
	case LM_FRESH:
		return (is_ocean(tile));
		break;
	
	case LM_CANYON:
		return true;
		break;
	
	case LM_GEOTHERMAL:
		return true;
		break;
	
	case LM_RIDGE:
		return true;
		break;
	
	case LM_FOSSIL:
		return (tile->code_at() < 6);
		break;
	
	}
	
	return true;
	
}

int isBonusAt(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	return bonus_at(x, y) != RES_NONE;
	
}

int getNutrientBonus(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	int bonus = 0;
	
	// resource
	
	if (bonus_at(x, y) == 1)
	{
		bonus += ResInfo->bonus_sq.nutrient;
	}
	
	// ladmarks
	
	if
	(
		map_has_landmark(tile, LM_JUNGLE)
		||
		map_has_landmark(tile, LM_FRESH)
	)
	{
		bonus += 1;
	}
	
	return bonus;
	
}

int getMineralBonus(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	int bonus = 0;
	
	// resource
	
	if (bonus_at(x, y) == 2)
	{
		bonus += ResInfo->bonus_sq.mineral;
	}
	
	// ladmarks
	
	if
	(
		map_has_landmark(tile, LM_CRATER)
		||
		map_has_landmark(tile, LM_VOLCANO)
		||
		map_has_landmark(tile, LM_CANYON)
		||
		map_has_landmark(tile, LM_FOSSIL)
	)
	{
		bonus += 1;
	}
	
	return bonus;
	
}

int getEnergyBonus(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	int bonus = 0;
	
	// resource
	
	if (bonus_at(x, y) == 3)
	{
		bonus += ResInfo->bonus_sq.energy;
	}
	
	// ladmarks
	
	if
	(
		map_has_landmark(tile, LM_VOLCANO)
		||
		map_has_landmark(tile, LM_GEOTHERMAL)
		||
		map_has_landmark(tile, LM_URANIUM)
	)
	{
		bonus += 1;
	}
	
	return bonus;
	
}

bool isLandmarkBonus(MAP *tile)
{
	assert(isOnMap(tile));
	
	return
		map_has_landmark(tile, LM_JUNGLE)
		||
		map_has_landmark(tile, LM_FRESH)
		||
		map_has_landmark(tile, LM_CRATER)
		||
		map_has_landmark(tile, LM_VOLCANO)
		||
		map_has_landmark(tile, LM_CANYON)
		||
		map_has_landmark(tile, LM_FOSSIL)
		||
		map_has_landmark(tile, LM_GEOTHERMAL)
		||
		map_has_landmark(tile, LM_URANIUM)
	;
	
}

/*
Reads vehicle order string.
*/
const char *readOrder(int id) {
	g_strTEMP[0] = '\x0';
	say_orders2(id);
	return g_strTEMP;
}

/*
Checks base has facility.
*/
bool isBaseHasFacility(int base_id, int facility_id)
{
	return has_facility((FacilityId)facility_id, base_id);
}

/*
adds/removes base regular facility (not project)
*/
void setBaseFacility(int base_id, int facility_id, bool add)
{
    assert(base_id >= 0 && base_id < *BaseCount);
    assert(facility_id > 0 && facility_id <= FAC_EMPTY_FACILITY_64);

    if (add)
	{
		Bases[base_id].facilities_built[facility_id/8] |= (1 << (facility_id % 8));
	}
	else
	{
		Bases[base_id].facilities_built[facility_id/8] &= ~(1 << (facility_id % 8));
	}

}

/*
Checks if faction has tech to build a facility.
*/
bool has_facility_tech(int faction_id, int facility_id) {
	return isFactionHasTech(faction_id, Facility[facility_id].preq_tech);
}

/*
Counts base doctors, empaths, and transcends combined.
*/
int getBaseDoctors(int id)
{
	BASE *base = &(Bases[id]);

	int doctors = 0;

	for (int i = 0; i < std::min(16, base->specialist_total); i++)
	{
		int specialistType = (base->specialist_types[i/8] >> (4 * (i%8))) & 0xF;

		switch (specialistType)
		{
		case 1:
		case 4:
		case 6:
			doctors++;
			break;
		}

	}

	return doctors;

}

/*
Finds psych generated by best available doctor.
*/
int getDoctorSpecialistType(int factionId)
{
	int bestDoctorSpecialistType = -1;
	int bestDoctorSpecialistTypePsychBonus = 0;

	for (int specialistType = 0; specialistType < 7; specialistType++)
	{
		CCitizen *citizen = getCitizen(specialistType);

		// uncovered

		if (!isFactionHasTech(factionId, citizen->preq_tech))
			continue;

		// not obsolete

		if (isFactionHasTech(factionId, citizen->obsol_tech))
			continue;

		// get psych bonus

		int specialistPsychBonus = citizen->psych_bonus;

		// update best

		if (specialistPsychBonus > bestDoctorSpecialistTypePsychBonus)
		{
			bestDoctorSpecialistType = specialistType;
			bestDoctorSpecialistTypePsychBonus = specialistPsychBonus;
		}

	}

	return bestDoctorSpecialistType;

}

int getBaseBuildingItem(int baseId)
{
	return Bases[baseId].queue_items[0];
}

bool isBaseBuildingUnit(int baseId)
{
	int item = getBaseBuildingItem(baseId);
	return (item >= 0);
}

bool isBaseBuildingFacility(int baseId)
{
	int item = getBaseBuildingItem(baseId);
	return (item > -SP_ID_First && item < 0);
}

bool isBaseBuildingProject(int baseId)
{
	int item = getBaseBuildingItem(baseId);
	return (item >= -SP_ID_Last && item <= -SP_ID_First);
}

bool isBaseProductionWithinRetoolingExemption(int baseId)
{
	BASE *base = &(Bases[baseId]);

	return (base->minerals_accumulated <= Rules->retool_exemption);

}

int getBaseBuildingItemMineralCost(int baseId)
{
	BASE *base = &(Bases[baseId]);

	int item = getBaseBuildingItem(baseId);
	return (item >= 0 ? tx_veh_cost(item, baseId, 0) : Facility[-item].cost) * mod_cost_factor(base->faction_id, RSC_MINERAL, -1);
}

double getUnitPsiOffenseStrength(int factionId, int unitId)
{
	Faction *faction = getFaction(factionId);
	UNIT *unit = &(Units[unitId]);
	int triad = unit->triad();
	
	// initial value
	
	double psiOffenseStrength = (double)Rules->psi_combat_ratio_atk[triad] / (double)Rules->psi_combat_ratio_def[triad];
	
	// faction PLANET rating modifier
	
	psiOffenseStrength *= getPercentageBonusMultiplier(Rules->combat_psi_bonus_per_planet * faction->SE_planet_pending);
	
	// abilities modifier
	
	if (isUnitHasAbility(unitId, ABL_EMPATH))
	{
		psiOffenseStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_empath_song_vs_psi);
	}
	
	// return strength
	
	return psiOffenseStrength;
	
}

double getUnitPsiDefenseStrength(int factionId, int unitId)
{
	Faction *faction = getFaction(factionId);
	
	// initial value
	
	double psiDefenseStrength = 1.0;
	
	// faction PLANET rating modifier
	
	if (conf.planet_defense_bonus)
	{
		psiDefenseStrength *= getPercentageBonusMultiplier(Rules->combat_psi_bonus_per_planet * faction->SE_planet_pending);
	}
	
	// abilities modifier
	
	if (isUnitHasAbility(unitId, ABL_TRANCE))
	{
		psiDefenseStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi);
	}
	
	// return strength
	
	return psiDefenseStrength;
	
}

double getUnitConOffenseStrength(int unitId)
{
	UNIT *unit = &(Units[unitId]);

	return (double)Weapon[unit->weapon_id].offense_value;

}

double getUnitConDefenseStrength(int unitId)
{
	UNIT *unit = &(Units[unitId]);

	return (double)Armor[unit->armor_id].defense_value;

}

double getUnitConArtilleryDuelStrength(int unitId)
{
	return (double)(Weapon[Units[unitId].weapon_id].offense_value);
}

/*
Calculates psi offense strength for new vehicle built at base.
*/
double getNewUnitPsiOffenseStrength(int unitId, int baseId)
{
	BASE *base = &(Bases[baseId]);
	int factionId = base->faction_id;
	
	double psiOffenseStrength = getUnitPsiOffenseStrength(factionId, unitId);
	double moraleMultiplier = getMoraleMultiplier(getNewVehicleMorale(unitId, baseId));
	
	return psiOffenseStrength * moraleMultiplier;
	
}

double getNewUnitPsiDefenseStrength(int unitId, int baseId)
{
	BASE *base = &(Bases[baseId]);
	int factionId = base->faction_id;
	
	double psiDefenseStrength = getUnitPsiDefenseStrength(factionId, unitId);
	double moraleMultiplier = getMoraleMultiplier(getNewVehicleMorale(unitId, baseId));
	
	return psiDefenseStrength * moraleMultiplier;
	
}

double getNewUnitConOffenseStrength(int unitId, int baseId)
{
	double conOffenseStrength = getUnitConOffenseStrength(unitId);
	double moraleMultiplier = getMoraleMultiplier(getNewVehicleMorale(unitId, baseId));

	return conOffenseStrength * moraleMultiplier;
	
}

double getNewUnitConDefenseStrength(int unitId, int baseId)
{
	double conDefenseStrength = getUnitConDefenseStrength(unitId);
	double moraleMultiplier = getMoraleMultiplier(getNewVehicleMorale(unitId, baseId));
	
	return conDefenseStrength * moraleMultiplier;
	
}

double getVehiclePsiOffenseStrength(int vehicleId, bool ignoreDamage)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	
	// unit psi offense strength
	
	double psiOffenseStrength = getUnitPsiOffenseStrength(factionId, unitId);
	
	// morale modifier
	
	psiOffenseStrength *= getVehicleMoraleMultiplier(vehicleId);
	
	// damage modifier
	
	if (!ignoreDamage)
	{
		psiOffenseStrength *= getVehicleRelativePower(vehicleId);
	}
	
	// return strength
	
	return psiOffenseStrength;
	
}

double getVehiclePsiDefenseStrength(int vehicleId, bool ignoreDamage)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	
	// psi defense strength
	
	double psiDefenseStrength = getUnitPsiDefenseStrength(factionId, unitId);
	
	// morale modifier
	
	psiDefenseStrength *= getVehicleMoraleMultiplier(vehicleId);
	
	// damage modifier
	
	if (!ignoreDamage)
	{
		psiDefenseStrength *= getVehicleRelativePower(vehicleId);
	}
	
	// return strength
	
	return psiDefenseStrength;
	
}

double getVehicleConOffenseStrength(int vehicleId, bool ignoreDamage)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// unit with psi weapon does not initiate conventional offense

	if (Weapon[vehicle->weapon_type()].offense_value < 0)
		return 0.0;

	// strength

	double conventionalOffenseStrength = (double)vehicle->offense_value();

	// morale modifier

	conventionalOffenseStrength *= getVehicleMoraleMultiplier(vehicleId);

	// damage modifier
	
	if (!ignoreDamage)
	{
		conventionalOffenseStrength *= getVehicleRelativePower(vehicleId);
	}
	
	// return strength

	return conventionalOffenseStrength;

}

double getVehicleConDefenseStrength(int vehicleId, bool ignoreDamage)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// unit with psi armor does not initiate conventional defense

	if (Armor[vehicle->armor_type()].defense_value < 0)
		return 0.0;

	// conventional defense strength

	double conventionalDefenseStrength = (double)vehicle->defense_value();

	// morale modifier

	conventionalDefenseStrength *= getVehicleMoraleMultiplier(vehicleId);

	if (!ignoreDamage)
	{
		conventionalDefenseStrength *= getVehicleRelativePower(vehicleId);
	}
	
	// return strength

	return conventionalDefenseStrength;

}

/*
Computes total damage vehicle can deliver in conventional offense.
*/
double getVehicleConOffensePower(int vehicleId)
{
	return getVehicleConOffenseStrength(vehicleId) * getVehicleRelativePower(vehicleId);
}

/*
Computes total damage vehicle can deliver in conventional defense.
*/
double getVehicleConDefensePower(int vehicleId)
{
	return getVehicleConDefenseStrength(vehicleId) * getVehicleRelativePower(vehicleId);
}

double getFactionSEPlanetOffenseModifier(int factionId)
{
	Faction *faction = &(Factions[factionId]);

	return getPercentageBonusMultiplier(Rules->combat_psi_bonus_per_planet * faction->SE_planet_pending);

}

double getFactionSEPlanetDefenseModifier(int factionId)
{
	Faction *faction = &(Factions[factionId]);

	if (conf.planet_defense_bonus)
	{
		return getPercentageBonusMultiplier(Rules->combat_psi_bonus_per_planet * faction->SE_planet_pending);
	}
	else
	{
		return 1.0;
	}

}

double getPsiCombatBaseOdds(int triad)
{
	return (double)Rules->psi_combat_ratio_atk[triad] / (double)Rules->psi_combat_ratio_def[triad];
}

bool isCombatUnit(int unitId)
{
	assert(unitId >= 0);
	
	// fungal tower is considered non combat unit as it does not attack
	return getUnitOffenseValue(unitId) != 0 && unitId != BSC_FUNGAL_TOWER;
	
}

bool isCombatVehicle(int vehicleId)
{
	assert(vehicleId >= 0);
	
	return isCombatUnit(getVehicle(vehicleId)->unit_id);
	
}

bool isUtilityVehicle(int vehicleId)
{
	assert(vehicleId >= 0 && vehicleId < *VehCount);
	return isColonyVehicle(vehicleId) || isFormerVehicle(vehicleId);
}

bool isOgreUnit(int unitId)
{
	switch (unitId)
	{
	case BSC_BATTLE_OGRE_MK1:
	case BSC_BATTLE_OGRE_MK2:
	case BSC_BATTLE_OGRE_MK3:
		return true;
	default:
		return false;
	}
}

bool isOgreVehicle(int vehicleId)
{
	assert(vehicleId >= 0 && vehicleId < *VehCount);
	return isOgreUnit(Vehicles[vehicleId].unit_id);
}

/*
Calculates average maximal damage vehicle delivers to enemy in psi combat when attacking.
*/
double calculatePsiDamageAttack(int vehicleId, int enemyVehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);

	// calculate base damage (vehicle attacking)

	double damage =
		1.0 * getPsiCombatBaseOdds(vehicle->triad())
		*
		(
			getVehicleMoraleMultiplier(vehicleId)
			*
			getFactionSEPlanetOffenseModifier(vehicle->faction_id)
		)
		/
		(
			getVehicleMoraleMultiplier(enemyVehicleId)
			*
			getFactionSEPlanetDefenseModifier(enemyVehicle->faction_id)
		)
		*
		(double)(10 * vehicle->reactor_type() - vehicle->damage_taken) / (double)vehicle->reactor_type()
	;

	// attacker empath increases damage

	if (vehicle_has_ability(vehicleId, ABL_EMPATH))
	{
		damage *= (1 + (double)Rules->combat_bonus_empath_song_vs_psi / 100.0);
	}

	// defender trance decreases damage

	if (vehicle_has_ability(enemyVehicleId, ABL_TRANCE))
	{
		damage /= (1 + (double)Rules->combat_bonus_trance_vs_psi / 100.0);
	}

	return damage;

}

/*
Calculates average maximal damage unit delivers to enemy in psi combat when defending.
*/
double calculatePsiDamageDefense(int vehicleId, int enemyVehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	VEH *enemyVehicle = &(Vehicles[enemyVehicleId]);

	// calculate base damage (vehicle defending)

	double damage =
		1.0 / getPsiCombatBaseOdds(enemyVehicle->triad())
		*
		(
			getVehicleMoraleMultiplier(vehicleId)
			*
			getFactionSEPlanetDefenseModifier(vehicle->faction_id)
		)
		/
		(
			getVehicleMoraleMultiplier(enemyVehicleId)
			*
			getFactionSEPlanetOffenseModifier(enemyVehicle->faction_id)
		)
		*
		(double)(10 - vehicle->damage_taken)
	;

	// attacker empath decreases damage

	if (vehicle_has_ability(enemyVehicleId, ABL_EMPATH))
	{
		damage /= (1 + (double)Rules->combat_bonus_empath_song_vs_psi / 100.0);
	}

	// defender trance increases damage

	if (vehicle_has_ability(vehicleId, ABL_TRANCE))
	{
		damage *= (1 + (double)Rules->combat_bonus_trance_vs_psi / 100.0);
	}

	return damage;

}

VehOrder getVehicleOrder(int vehicleId)
{
    return (VehOrder)getVehicle(vehicleId)->order;
}

void setVehicleOrder(int id, int order)
{
	VEH *vehicle = &(Vehicles[id]);

    vehicle->order = order;
    vehicle->status_icon = VehStatusIcon[order];

    // clear flag for hold to be able to heal

    if (order == ORDER_HOLD)
	{
		vehicle->state = 0;
	}

}

int getBaseMapTileIndex(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	return getMapTileIndex(base->x, base->y);
	
}

MAP *getBaseMapTile(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	return getMapTile(base->x, base->y);
	
}

int getVehicleMapTileIndex(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	return getMapTileIndex(vehicle->x, vehicle->y);
	
}

MAP *getVehicleMapTile(int vehicleId)
{
	if (!(vehicleId >= 0 && vehicleId < *VehCount))
		return nullptr;
	
	VEH &vehicle = Vehicles[vehicleId];
	
	return getMapTile(vehicle.x, vehicle.y);
	
}

/*
Verifies if tile has significant improvement.
*/
bool isImprovedTile(MAP *tile)
{
	return (tile->items & (BIT_MINE | BIT_SOLAR | BIT_FOREST | BIT_CONDENSER | BIT_ECH_MIRROR | BIT_THERMAL_BORE | BIT_SENSOR | BIT_MONOLITH)) != 0;
}
bool isImprovedTile(int x, int y)
{
	return isImprovedTile(getMapTile(x, y));
}

bool isSupplyUnit(int unitId)
{
	return Units[unitId].weapon_id == WPN_SUPPLY_TRANSPORT;
}
bool isSupplyVehicle(int vehicleId)
{
	return isSupplyUnit(Vehicles[vehicleId].unit_id);
}

bool isColonyUnit(int id)
{
	return (id >= 0 && Units[id].weapon_id == WPN_COLONY_MODULE);
}

bool isArtifactUnit(int unitId)
{
	return (Units[unitId].weapon_id == WPN_ALIEN_ARTIFACT);
}

bool isArtifactVehicle(int id)
{
	return (Units[Vehicles[id].unit_id].weapon_id == WPN_ALIEN_ARTIFACT);
}

bool isColonyVehicle(int id)
{
	return (Units[Vehicles[id].unit_id].weapon_id == WPN_COLONY_MODULE);
}

bool isFormerUnit(int unitId)
{
	return (Units[unitId].weapon_id == WPN_TERRAFORMING_UNIT);
}

bool isFormerVehicle(int vehicleId)
{
	return isFormerVehicle(&(Vehicles[vehicleId]));
}
bool isFormerVehicle(VEH *vehicle)
{
	return (Units[vehicle->unit_id].weapon_id == WPN_TERRAFORMING_UNIT);
}

bool isConvoyUnit(int unitId)
{
	return (Units[unitId].weapon_id == WPN_SUPPLY_TRANSPORT);
}
bool isConvoyVehicle(int vehicleId)
{
	return isConvoyUnit(Vehicles[vehicleId].unit_id);
}

bool isTransportVehicle(VEH *vehicle)
{
	return (Units[vehicle->unit_id].weapon_id == WPN_TROOP_TRANSPORT);
}

bool isTransportUnit(int unitId)
{
	return (Units[unitId].weapon_id == WPN_TROOP_TRANSPORT);
}

bool isTransportVehicle(int vehicleId)
{
	return (Units[Vehicles[vehicleId].unit_id].weapon_id == WPN_TROOP_TRANSPORT);
}

bool isSeaTransportUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);
	return (unit->weapon_id == WPN_TROOP_TRANSPORT && unit->triad() == TRIAD_SEA);
}

bool isSeaTransportVehicle(int vehicleId)
{
	return isSeaTransportUnit(getVehicle(vehicleId)->unit_id);
}

bool isVehicleProbe(VEH *vehicle)
{
	return (Units[vehicle->unit_id].weapon_id == WPN_PROBE_TEAM);
}

bool isVehicleIdle(int vehicleId)
{
	return (Vehicles[vehicleId].order == ORDER_NONE);
}

bool isMeleeUnit(int unitId)
{
	return isCombatUnit(unitId) && !isLandArtilleryUnit(unitId) && unitId != BSC_FUNGAL_TOWER;
}

bool isMeleeVehicle(int vehicleId)
{
	return isMeleeUnit(getVehicle(vehicleId)->unit_id);
}

bool isArtilleryUnit(int unitId)
{
	return isCombatUnit(unitId) && can_arty(unitId, true);
}

bool isArtilleryVehicle(int vehicleId)
{
	return isArtilleryUnit(getVehicle(vehicleId)->unit_id);
}

/**
Artillery vehicle not on transport.
*/
bool isActiveArtilleryVehicle(int vehicleId)
{
	return isArtilleryUnit(getVehicle(vehicleId)->unit_id) && !isVehicleOnTransport(vehicleId);
}

bool isLandArtilleryUnit(int unitId)
{
	return (getUnit(unitId)->triad() == TRIAD_LAND && isArtilleryUnit(unitId));
}

bool isLandArtilleryVehicle(int vehicleId)
{
	return isLandArtilleryUnit(getVehicle(vehicleId)->unit_id);
}

/**
Computes base with optional worked tiles reset.
Base compute may not pick optimal worker placement without worked tile reset.
*/
void computeBase(int baseId, bool resetWorkedTiles, MAP *excludedTile)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	
	Profiling::start("- computeBase");
	
	if (resetWorkedTiles)
	{
		Bases[baseId].worked_tiles = 0;
		Bases[baseId].specialist_total = 0;
		Bases[baseId].specialist_adjust = 0;
	}
	
	set_base(baseId);
	baseComputeExcludedTile = excludedTile;
	base_compute(1);
	baseComputeExcludedTile = nullptr;
	
	Profiling::stop("- computeBase");	
	
}

/*
Returns base tile region plus all ocean regions this base is connected to for coastal bases.
*/
robin_hood::unordered_flat_set<int> getBaseConnectedRegions(int id)
{
	robin_hood::unordered_flat_set<int> baseConnectedRegions;

	BASE *base = &(Bases[id]);

	// get base tile

	MAP *baseTile = getMapTile(base->x, base->y);

	// get and store base tile region

	baseConnectedRegions.insert(baseTile->region);

	// get and store base adjacent ocean regions for land base

	if (!is_ocean(baseTile))
	{
		for (MAP *tile : getBaseAdjacentTiles(base->x, base->y, false))
		{
			// skip land tiles

			if (!is_ocean(tile))
				continue;

			// get and store tile region

			baseConnectedRegions.insert(tile->region);

		}

	}

	return baseConnectedRegions;

}

/*
Returns base tile ocean regions it can issue ships to.
*/
robin_hood::unordered_flat_set<int> getBaseConnectedOceanRegions(int baseId)
{
	BASE *base = &(Bases[baseId]);
	MAP *baseLocations = getBaseMapTile(baseId);

	robin_hood::unordered_flat_set<int> baseConnectedOceanRegions;

	if (is_ocean(baseLocations))
	{
		baseConnectedOceanRegions.insert(baseLocations->region);
	}
	else
	{
		for (MAP *tile : getBaseAdjacentTiles(base->x, base->y, false))
		{
			if (!is_ocean(tile))
				continue;

			baseConnectedOceanRegions.insert(tile->region);

		}

	}

	return baseConnectedOceanRegions;

}

/*
Evaluates unit defense effectiveness based on defense value and cost.
*/
double evaluateUnitConDefenseEffectiveness(int id)
{
	UNIT *unit = &(Units[id]);
	int cost = unit->cost;
	int defenseValue = Armor[unit->armor_id].defense_value;

	// inflate cost due to support assuming 40 turns of service

	if (!unit_has_ability(id, ABL_CLEAN_REACTOR))
	{
		cost += 4;
	}

	return (double)defenseValue / (double)cost;

}

/*
Evaluates unit offense effectiveness based on offense value and cost.
*/
double evaluateUnitConOffenseEffectiveness(int id)
{
	UNIT *unit = &(Units[id]);
	int cost = unit->cost;
	int offenseValue = Weapon[unit->armor_id].offense_value;

	// inflate cost due to support assuming 40 turns of service

	if (!unit_has_ability(id, ABL_CLEAN_REACTOR))
	{
		cost += 4;
	}

	return (double)offenseValue / (double)cost;

}

/*
Evaluates unit defense effectiveness based on defense value and cost.
*/
double evaluateUnitPsiDefenseEffectiveness(int id)
{
	UNIT *unit = &(Units[id]);

	// value

	double defenseValue = 1.0;

	if (unit_has_ability(id, ABL_TRANCE))
	{
		defenseValue *= 1.0 + (double)Rules->combat_bonus_trance_vs_psi / 100.0;
	}

	// cost

	int cost = unit->cost;

	// inflate cost due to support assuming 40 turns of service

	if (!unit_has_ability(id, ABL_CLEAN_REACTOR))
	{
		cost += 4;
	}

	return defenseValue / (double)cost;

}

/*
Evaluates unit offense effectiveness based on offense value and cost.
*/
double evaluateUnitPsiOffenseEffectiveness(int id)
{
	UNIT *unit = &(Units[id]);

	// value

	double offenseValue = 1.0;

	if (unit_has_ability(id, ABL_EMPATH))
	{
		offenseValue *= 1.0 + (double)Rules->combat_bonus_trance_vs_psi / 100.0;
	}

	// cost

	int cost = unit->cost;

	// inflate cost due to support assuming 40 turns of service

	if (!unit_has_ability(id, ABL_CLEAN_REACTOR))
	{
		cost += 4;
	}

	return offenseValue / (double)cost;

}

/*
extendedTriad: 0 = land, 1 = sea, 2 = air, 3 = psi, 4 = probe
*/
double getBaseDefenseMultiplier(int baseId, int extendedTriad)
{
	int defenseBonus = 0;
	
	switch (extendedTriad)
	{
	case 4:
		defenseBonus = conf.probe_combat_uses_bonuses ? Rules->combat_bonus_intrinsic_base_def : 0;
		break;
	case 3:
		defenseBonus = Rules->combat_bonus_intrinsic_base_def;
		break;
	case 2:
	case 1:
	case 0:
		{
			bool firstLevelDefense = has_facility(TRIAD_DEFENSIVE_FACILITIES[extendedTriad], baseId);
			bool secondLevelDefense = has_facility(FAC_TACHYON_FIELD, baseId);
			
			if (!firstLevelDefense && !secondLevelDefense)
			{
				defenseBonus = Rules->combat_bonus_intrinsic_base_def;
			}
			else
			{
				if (firstLevelDefense)
				{
					defenseBonus += conf.facility_defense_bonus[extendedTriad];
				}
				if (secondLevelDefense)
				{
					defenseBonus += conf.facility_defense_bonus[3];
				}
				
			}
			
		}
		break;
		
	}
		
	return getPercentageBonusMultiplier(defenseBonus);
	
}

double getBaseDefenseMultiplier(int baseId, int attackerUnitId, int defenderUnitId)
{
	UNIT *attackerUnit = getUnit(attackerUnitId);
	int attackerUnitTriad = attackerUnit->triad();
	
	return getBaseDefenseMultiplier(baseId, isPsiCombat(attackerUnitId, defenderUnitId) ? 3 : attackerUnitTriad);
	
}

int estimateBaseItemProductionTime(int baseId, int item)
{
	BASE *base = getBase(baseId);

	return divideIntegerRoundUp(getBaseMineralCost(baseId, item), base->mineral_surplus);

}

/*
Estimates turns to complete current base production assuming all parameters stays as is.
*/
int estimateBaseProductionTurnsToComplete(int id)
{
	BASE *base = &(Bases[id]);

	return ((getBaseMineralCost(id, base->queue_items[0]) - base->minerals_accumulated) + (base->mineral_surplus - 1)) / base->mineral_surplus;

}

MAP *getBaseWorkerTile(int baseId, int workerNumber)
{
	assert(workerNumber >=0 && workerNumber < 21);
	
	BASE const &base = Bases[baseId];
	
	int dx = OFFSETS[workerNumber][0];
	int dy = OFFSETS[workerNumber][1];
	
	int x = wrap(base.x + dx);
	int y = base.y + dy;
	
	return isOnMap(x, y) ? getMapTile(x, y) : nullptr;
	
}

/*
Returns all valid base radius map tiles around base.
if startFromCenter == true then also return central tile as first element
*/
std::vector<MAP *> getBaseWorkableTiles(int baseId, bool startWithCenter)
{
	BASE *base = &(Bases[baseId]);

	return getBaseRadiusTiles(base->x, base->y, startWithCenter);

}

std::vector<MAP *> getBaseWorkedTiles(int baseId)
{
	BASE *base = &(Bases[baseId]);

	std::vector<MAP *> baseWorkedTiles;

	for (int offsetIndex = 1; offsetIndex < OFFSET_COUNT_RADIUS; offsetIndex++)
	{
		if (base->worked_tiles & (0x1 << offsetIndex))
		{
			MAP *workedTile = getMapTile(wrap(base->x + BASE_TILE_OFFSETS[offsetIndex][0]), base->y + BASE_TILE_OFFSETS[offsetIndex][1]);

			if (workedTile == NULL)
				continue;

			baseWorkedTiles.push_back(workedTile);

		}

	}

	return baseWorkedTiles;

}

int getBaseWorkerCount(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	int workerCount = 0;
	
	for (int offsetIndex = 1; offsetIndex < OFFSET_COUNT_RADIUS; offsetIndex++)
	{
		if ((base->worked_tiles & (0x1 << offsetIndex)) != 0)
		{
			workerCount++;
		}
		
	}
	
	return workerCount;
	
}

bool isBaseWorkedTile(int baseId, int x, int y)
{
	if (!(baseId >= 0 && baseId < *BaseCount && isOnMap(x, y)))
		return false;

	BASE *base = &(Bases[baseId]);

	for (int offset = 0; offset < OFFSET_COUNT_RADIUS; offset++)
	{
		int baseRadiusTileX = wrap(base->x + BASE_TILE_OFFSETS[offset][0]);
		int baseRadiusTileY = base->y + BASE_TILE_OFFSETS[offset][1];

		if (baseRadiusTileX == x && baseRadiusTileY == y)
		{
			if ((base->worked_tiles & (0x1 << offset)) == 0)
			{
				return false;
			}
			else
			{
				return true;
			}

		}

	}

	return false;

}

bool isBaseWorkedTile(int baseId, MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	return isBaseWorkedTile(baseId, x, y);
	
}

/**
Returns faction available units.
*/
std::vector<int> getFactionUnitIds(int factionId, bool includeObsolete, bool includeNotPrototyped)
{
	std::vector<int> factionUnitIds;
	
	// alien units are always the same
	
	if (factionId == 0)
		return ALIEN_UNITS;
	
	// iterate faction units
	
    for (int slot = 0; slot < 2 * MaxProtoFactionNum; slot++)
	{
        int unitId = getUnitIdBySlot(factionId, slot);
		
		// skip not available
		
		if (!isAvailableUnit(unitId, factionId, includeObsolete))
			continue;
		
		// do not include not prototyped if not requested
		
		if (!includeNotPrototyped && !isPrototypedUnit(unitId))
			continue;
		
		// add unit
		
		factionUnitIds.push_back(unitId);
		
	}
	
	return factionUnitIds;
	
}

/*
Returns fastest former unit.
*/
int getFactionFastestFormerUnitId(int factionId)
{
	int fastestFormerUnitId = BSC_FORMERS;
	int fastestFormerUnitChassisSpeed = 1;

    for (int unitId : getFactionUnitIds(factionId, false, false))
	{
        UNIT *unit = getUnit(unitId);
        int unitChassisSpeed = unit->speed();

		// former

		if (!isFormerUnit(unitId))
			continue;

		if (unitChassisSpeed > fastestFormerUnitChassisSpeed)
		{
			fastestFormerUnitId = unitId;
			fastestFormerUnitChassisSpeed = unitChassisSpeed;
		}

	}

	return fastestFormerUnitId;

}

/*
Determines if vehicle is a native land predefined unit.
*/
bool isVehicleNativeLand(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int unitId = vehicle->unit_id;

	return
		unitId == BSC_MIND_WORMS
		||
		unitId == BSC_SPORE_LAUNCHER
	;

}

bool isBaseBuildingColony(int baseId)
{
	BASE *base = &(Bases[baseId]);
	int item = base->queue_items[0];

	return (item >= 0 && isColonyUnit(item));

}

/*
Returns base population size limit without this facility.
Returns -1 if given facility is not a population limit facility.
*/
int getFacilityPopulationLimit(int factionId, int facilityId)
{
	MFaction *MFaction = &(MFactions[factionId]);

	int populationLimit;

	// basic population limit for facility

	switch (facilityId)
	{
	case FAC_HAB_COMPLEX:
		populationLimit = Rules->pop_limit_wo_hab_complex;
		break;
	case FAC_HABITATION_DOME:
		populationLimit = Rules->pop_limit_wo_hab_dome;
		break;
	default:
		return -1;
	}

	// faction penalty

    populationLimit -= MFaction->rule_population;

    // Ascetic Virtue

    if (isFactionHasProject(factionId, FAC_ASCETIC_VIRTUES))
	{
		populationLimit += 2;
	}

    return populationLimit;

}

/*
Checks if base can build facility.
*/
bool isBaseFacilityAvailable(int baseId, int facilityId)
{
	BASE *base = &(Bases[baseId]);

	// already built

	if (isBaseHasFacility(baseId, facilityId))
		return false;

	// tech available?

	if (!has_facility_tech(base->faction_id, facilityId))
		return false;

	// special cases

	switch (facilityId)
	{
	case FAC_TACHYON_FIELD:
		if (!isBaseHasFacility(baseId, FAC_PERIMETER_DEFENSE))
			return false;
		break;
	case FAC_HOLOGRAM_THEATRE:
		if (!isBaseHasFacility(baseId, FAC_RECREATION_COMMONS))
			return false;
		break;
	case FAC_HYBRID_FOREST:
		if (!isBaseHasFacility(baseId, FAC_TREE_FARM))
			return false;
		break;
	case FAC_QUANTUM_LAB:
		if (!isBaseHasFacility(baseId, FAC_FUSION_LAB))
			return false;
		break;
	case FAC_NANOHOSPITAL:
		if (!isBaseHasFacility(baseId, FAC_RESEARCH_HOSPITAL))
			return false;
		break;
	case FAC_GENEJACK_FACTORY:
		if (!isBaseHasFacility(baseId, FAC_RECYCLING_TANKS))
			return false;
		break;
	case FAC_ROBOTIC_ASSEMBLY_PLANT:
		if (!isBaseHasFacility(baseId, FAC_GENEJACK_FACTORY))
			return false;
		break;
	case FAC_QUANTUM_CONVERTER:
		if (!isBaseHasFacility(baseId, FAC_ROBOTIC_ASSEMBLY_PLANT))
			return false;
		break;
	case FAC_NANOREPLICATOR:
		if (!isBaseHasFacility(baseId, FAC_QUANTUM_CONVERTER))
			return false;
		break;
	case FAC_HABITATION_DOME:
		if (!isBaseHasFacility(baseId, FAC_HAB_COMPLEX))
			return false;
		break;
	case FAC_TEMPLE_OF_PLANET:
		if (!isBaseHasFacility(baseId, FAC_CENTAURI_PRESERVE))
			return false;
		break;
	case FAC_SKY_HYDRO_LAB:
    case FAC_NESSUS_MINING_STATION:
    case FAC_ORBITAL_POWER_TRANS:
    case FAC_ORBITAL_DEFENSE_POD:
		if (!isBaseHasFacility(baseId, FAC_AEROSPACE_COMPLEX))
			return false;
		break;
	}

	return true;

}

/*
Checks if base is connected to region.
Coastal bases can be connected to multiple ocean regions.
*/
bool isBaseConnectedToRegion(int baseId, int region)
{
	BASE *base = &(Bases[baseId]);

	return isTileConnectedToRegion(base->x, base->y, region);

}

/*
Checks if tile is connected to region.
Coastal tiles can be connected to multiple ocean regions.
*/
bool isTileConnectedToRegion(int x, int y, int region)
{
	MAP *tile = getMapTile(x, y);

	if (tile->region == region)
		return true;

	if (!is_ocean(tile))
	{
		for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
		{
			if (is_ocean(adjacentTile) && adjacentTile->region == region)
				return true;

		}

	}

	return false;

}

std::vector<int> getRegionBases(int factionId, int region)
{
	std::vector<int> regionBases;

	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		// only this faction

		if (base->faction_id != factionId)
			continue;

		// only connected to this region

		if (!isBaseConnectedToRegion(baseId, region))
			continue;

		regionBases.push_back(baseId);

	}

	return regionBases;

}

/*
Finds combat units at base.
*/
std::vector<int> getBaseGarrison(int baseId)
{
	BASE *base = &(Bases[baseId]);

	std::vector<int> baseGarrison;

	int topStackVehicleId = veh_at(base->x, base->y);

	if (topStackVehicleId == -1)
		return baseGarrison;

	for (int vehicleId : getStackVehicleIds(topStackVehicleId))
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// only this faction units

		if (vehicle->faction_id != base->faction_id)
			continue;

		// only in this base

		if (!(vehicle->x == base->x && vehicle->y == base->y))
			continue;

		// combat

		if (!isCombatVehicle(vehicleId))
			continue;

		baseGarrison.push_back(vehicleId);

	}

	return baseGarrison;

}

/*
Finds infantry defender combat units at base.
*/
std::vector<int> getBaseInfantryDefenderGarrison(int baseId)
{
	BASE *base = &(Bases[baseId]);
	MAP *baseTile = getBaseMapTile(baseId);

	std::vector<int> garrison;

	for (int vehicleId : getTileVehicleIds(baseTile))
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// only this faction units

		if (vehicle->faction_id != base->faction_id)
			continue;

		// combat

		if (!isCombatVehicle(vehicleId))
			continue;

		// infantry defender

		if (!isInfantryDefensiveVehicle(vehicleId))
			continue;

		garrison.push_back(vehicleId);

	}

	return garrison;

}

std::vector<int> getFactionBases(int factionId)
{
	std::vector<int> factionBases;

	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		if (base->faction_id != factionId)
			continue;

		factionBases.push_back(baseId);

	}

	return factionBases;

}

/*
Estimates unit odds in base defense against land native attack.
This doesn't account for vehicle morale.
*/
double estimateUnitBaseLandNativeProtection(int unitId, int factionId, bool ocean)
{
	// basic defense odds against land native attack

	double nativeProtection = 1.0 / getPsiCombatBaseOdds(ocean ? TRIAD_SEA : TRIAD_LAND);

	// add base defense bonus

	nativeProtection *= 1.0 + (double)Rules->combat_bonus_intrinsic_base_def / 100.0;

	// add faction defense bonus

	nativeProtection *= getFactionDefenseMultiplier(factionId);

	// add PLANET

	nativeProtection *= getFactionSEPlanetDefenseModifier(factionId);

	// add trance

	if (unit_has_ability(unitId, ABL_TRANCE))
	{
		nativeProtection *= 1.0 + (double)Rules->combat_bonus_trance_vs_psi / 100.0;
	}

	// correction for native base attack penalty until turn 50

	if (*CurrentTurn < 50)
	{
		nativeProtection *= 2;
	}

	return nativeProtection;

}

/*
Estimates vehicle odds in base defense against native attack.
This accounts for vehicle morale.
*/
double getVehicleBaseNativeProtection(int baseId, int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *baseTile = getBaseMapTile(baseId);

	// unit native protection

	double nativeProtection = estimateUnitBaseLandNativeProtection(Vehicles[vehicleId].unit_id, vehicle->faction_id, is_ocean(baseTile));

	// add morale

	nativeProtection *= getVehicleMoraleMultiplier(vehicleId);

	return nativeProtection;

}

double getFactionOffenseMultiplier(int factionId)
{
	double factionOffenseMultiplier = 1.0;

	MFaction *MFaction = &(MFactions[factionId]);

	for (int bonusIndex = 0; bonusIndex < MFaction->faction_bonus_count; bonusIndex++)
	{
		int bonusId = MFaction->faction_bonus_id[bonusIndex];

		if (bonusId == RULE_OFFENSE)
		{
			factionOffenseMultiplier *= ((double)MFaction->faction_bonus_val1[bonusIndex] / 100.0);
		}

	}

	return factionOffenseMultiplier;

}

double getFactionDefenseMultiplier(int factionId)
{
	double factionDefenseMultiplier = 1.0;

	MFaction *MFaction = &(MFactions[factionId]);

	for (int bonusIndex = 0; bonusIndex < MFaction->faction_bonus_count; bonusIndex++)
	{
		int bonusId = MFaction->faction_bonus_id[bonusIndex];

		if (bonusId == RULE_DEFENSE)
		{
			factionDefenseMultiplier *= ((double)MFaction->faction_bonus_val1[bonusIndex] / 100.0);
		}

	}

	return factionDefenseMultiplier;

}

double getFactionFanaticBonusMultiplier(int factionId)
{
	double factionOffenseMultiplier = 1.0;

	MFaction *MFaction = &(MFactions[factionId]);

	if (MFaction->rule_flags & RFLAG_FANATIC)
	{
		factionOffenseMultiplier *= (1.0 + (double)Rules->combat_bonus_fanatic / 100.0);
	}

	return factionOffenseMultiplier;

}

/*
Calculates average number of native vehicles destroyed in psi combat when defending at base.
Assuming vehicle will heal in full at base.
*/
double getVehicleBaseNativeProtectionPotential(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// calculate base damage (vehicle defending)

	double protectionPotential = 1.0 / getPsiCombatBaseOdds(TRIAD_LAND) * getVehicleMoraleMultiplier(vehicleId) * getFactionSEPlanetDefenseModifier(vehicle->faction_id) * (1.0 + (double)Rules->combat_bonus_intrinsic_base_def / 100.0);

	// defender trance increases combat protectionPotential

	if (vehicle_has_ability(vehicleId, ABL_TRANCE))
	{
		protectionPotential *= (1 + (double)Rules->combat_bonus_trance_vs_psi / 100.0);
	}

	// double potential before turn 50

	if (*CurrentTurn < 50)
	{
		protectionPotential *= 2;
	}

	// adjustment to native base lifecycle

	protectionPotential /= (1.0 + (double)(*CurrentTurn / 50 - 2) * 0.125);

	return protectionPotential;

}

/*
Calculates average number of native vehicles destroyed in psi combat when defending at base per cost.
Assuming vehicle will heal in full at base.
*/
double getVehicleBaseNativeProtectionEfficiency(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// calculate protection potential

	double protectionPotential = getVehicleBaseNativeProtectionPotential(vehicleId);

	// calculate cost

	int cost = Units[vehicle->unit_id].cost;

	// inflate cost due to support assuming 40 turns of service

	if (!unit_has_ability(vehicleId, ABL_CLEAN_REACTOR))
	{
		cost += 4;
	}

	return protectionPotential / (double)cost;

}

int getBasePoliceRating(int baseId)
{
	BASE *base = &(Bases[baseId]);
	Faction *faction = &(Factions[base->faction_id]);

	int policeRating = faction->SE_police_pending;

	if (isBaseHasFacility(baseId, FAC_BROOD_PIT))
	{
		policeRating = std::min(3, policeRating + 2);
	}

	return policeRating;

}

int getBasePolicePower(int baseId, bool police2x)
{
	return (getBasePoliceRating(baseId) == 3 ? 2 : 1) + (police2x ? 1 : 0);
}

/**
Calculates number of allowed police units per base POLICE rating.
*/
int getBasePoliceAllowed(int baseId)
{
	int policeAllowed;

	switch (getBasePoliceRating(baseId))
	{
	case -5:
	case -4:
	case -3:
	case -2:
		policeAllowed = 0;
		break;
	case -1:
	case +0:
		policeAllowed = 1;
		break;
	case +1:
		policeAllowed = 2;
		break;
	case +2:
	case +3:
		policeAllowed = 3;
		break;
	default:
		policeAllowed = 0;
	}

	return policeAllowed;

}

/**
Counts number of police (or 2x) vehicles at base.
*/
int getBasePoliceCount(int baseId, bool police2x)
{
	BASE *base = getBase(baseId);
	MAP *baseTile = getBaseMapTile(baseId);
	
	int policeCount = 0;
	
	for (int vehicleId : getTileVehicleIds(baseTile))
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// same faction
		
		if (vehicle->faction_id != base->faction_id)
			continue;
		
		// combat
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		// police2x if requested
		
		if (police2x && !isPolice2xVehicle(vehicleId))
			continue;
		
		// accumulate
		
		policeCount++;
		
	}
	
	return policeCount;
}

char *getVehicleUnitName(int vehicleId)
{
	return Units[Vehicles[vehicleId].unit_id].name;
}

int getVehicleUnitPlan(int vehicleId)
{
	return (int)Units[vehicleId].plan;
}

/*
Estimates base native protection in number of native units killed.
*/
double getBaseNativeProtection(int baseId)
{
	double baseNativeProtection = 0.0;

	for (int vehicleId : getBaseGarrison(baseId))
	{
		// combat vehicles only

		if (!isCombatVehicle(vehicleId))
			continue;

		baseNativeProtection += getVehicleBaseNativeProtection(baseId, vehicleId);

	}

	return baseNativeProtection;

}

/*
Check if given tile is an explored edge for faction.
*/
bool isExploredEdge(int factionId, int x, int y)
{
	MAP *tile = getMapTile(x, y);

	if (tile == NULL)
		return false;

	// tile itself should be visible

	if (~tile->visibility & (0x1 << factionId))
		return false;

	for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
	{
		// if at least one adjacent tile is not visible - we are at edge

		if (~adjacentTile->visibility & (0x1 << factionId))
			return true;

	}

	return false;

}

bool isVehicleExploring(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return vehicle->state & VSTATE_EXPLORE;

}

bool isVehicleCanHealAtThisLocation(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehilceMapTile = getVehicleMapTile(vehicleId);

	// calculate vehicle field damage healing threshold

	int fieldDamageHealingThreshold = Units[vehicle->unit_id].reactor_id * 2;

	return
		(vehicle->damage_taken > fieldDamageHealingThreshold)
		||
		(map_has_item(vehilceMapTile, BIT_BASE_IN_TILE) && (vehicle->damage_taken > 0))
	;

}

/*
Returns all ocean regions this location is adjacent to.
*/
robin_hood::unordered_flat_set<int> getAdjacentOceanRegions(int x, int y)
{
	debug("getAdjacentOceanRegions: x=%d, y=%d\n", x, y);

	robin_hood::unordered_flat_set<int> adjacentOceanRegions;

	MAP *tile = getMapTile(x, y);

	if (tile == NULL)
		return adjacentOceanRegions;

	if (is_ocean(tile))
	{
		debug("\tocean\n");

		// ocean base is adjacent to one ocean region

		adjacentOceanRegions.insert(tile->region);

		debug("\t\tregion=%d\n", tile->region);

	}
	else
	{
		debug("\tland\n");

		// iterate adjacent tiles

		for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
		{
			debug("\t\tadjacentTile\n");

			if (is_ocean(adjacentTile))
			{
				debug("\t\tocean\n");

				adjacentOceanRegions.insert(adjacentTile->region);

			}

		}

	}

	debug("\n");

	return adjacentOceanRegions;

}

/*
Returns all ocean regions this location is connected to.
*/
robin_hood::unordered_flat_set<int> getConnectedOceanRegions(int factionId, int x, int y)
{
	debug("getConnectedOceanRegions: factionId=%d, x=%d, y=%d\n", factionId, x, y);

	robin_hood::unordered_flat_set<int> connectedOceanRegions;

	MAP *tile = getMapTile(x, y);

	if (tile == NULL)
		return connectedOceanRegions;

	// add adjacent tiles regions first

	robin_hood::unordered_flat_set<int> adjacentOceanRegions = getAdjacentOceanRegions(x, y);
	connectedOceanRegions.insert(adjacentOceanRegions.begin(), adjacentOceanRegions.end());

	// iterate own bases

	while (true)
	{
		size_t currentRegionSize = connectedOceanRegions.size();
		debug("\tcurrentRegionSize=%d\n", currentRegionSize);

		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);

			debug("\t\t%-25s factionId=%d, base->faction_id=%d, is_ocean(baseTile)=%d\n", Bases[baseId].name, factionId, base->faction_id, is_ocean(baseTile));

			// only own and allied bases

			if (base->faction_id != factionId)
				continue;

			// only land bases

			if (is_ocean(baseTile))
				continue;

			// get this base adjacent ocean regions

			robin_hood::unordered_flat_set<int> baseAdjacentOceanRegions = getAdjacentOceanRegions(base->x, base->y);

			// check for adjacent regions intersection

			for (int baseAdjacentOceanRegion : baseAdjacentOceanRegions)
			{
				debug("\t\t\tbaseAdjacentOceanRegion=%d\n", baseAdjacentOceanRegion);
				if (connectedOceanRegions.count(baseAdjacentOceanRegion) != 0)
				{
					debug("\t\t\t\tintersect=1\n");

					// union region sets

					connectedOceanRegions.insert(baseAdjacentOceanRegions.begin(), baseAdjacentOceanRegions.end());

					break;

				}

			}

		}

		if (connectedOceanRegions.size() == currentRegionSize)
			break;

	}

	debug("\n");

	return connectedOceanRegions;

}

void setMapTileVisibleToFaction(MAP *tile, int factionId)
{
	tile->visibility |= (0x1 << factionId);
}

/*
Verifies that faction1 and faction2 has given diplo_status.
*/
bool isDiploStatus(int faction1Id, int faction2Id, int diploStatus)
{
	return
		faction1Id != faction2Id && faction1Id >= 0 && faction2Id >= 0 && faction1Id < 8 && faction2Id < 8
		&&
		Factions[faction1Id].diplo_status[faction2Id] & diploStatus
	;
}

void setDiploStatus(int faction1Id, int faction2Id, int diploStatus, bool on)
{
	if (on)
	{
		Factions[faction1Id].diplo_status[faction2Id] |= diploStatus;
	}
	else
	{
		Factions[faction1Id].diplo_status[faction2Id] &= ~diploStatus;
	}

}

/*
Calculates amount of minerals to complete production in the base.
*/
int getRemainingMinerals(int baseId)
{
	BASE *base = &(Bases[baseId]);

	return std::max(0, getBaseMineralCost(baseId, base->queue_items[0]) - base->minerals_accumulated);

}

std::vector<int> getStackVehicleIds(int vehicleId)
{
	std::vector<int> stackedVehicleIds;

	// get top of the stack vehicle

	int topStackVehicleId = vehicleId;
	while (Vehicles[topStackVehicleId].prev_veh_id_stack != -1)
	{
		topStackVehicleId = Vehicles[topStackVehicleId].prev_veh_id_stack;
	}

	// collect all vehicles in the stack

	for (int stackedVehicleId = topStackVehicleId; stackedVehicleId != -1; stackedVehicleId = Vehicles[stackedVehicleId].next_veh_id_stack)
	{
		stackedVehicleIds.push_back(stackedVehicleId);
	}

	return stackedVehicleIds;

}

std::vector<int> getTileVehicleIds(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	int vehicleAt = veh_at(x, y);

	if (vehicleAt == -1)
	{
		return std::vector<int>();
	}
	else
	{
		return getStackVehicleIds(vehicleAt);
	}

}

void setTerraformingAction(int id, int action)
{
	VEH *vehicle = &(Vehicles[id]);

	// subtract raise/lower land cost

	if (action == FORMER_RAISE_LAND || action == FORMER_LOWER_LAND)
	{
		Factions[vehicle->faction_id].energy_credits -= terraform_cost(vehicle->x, vehicle->y, vehicle->faction_id);
	}

	// set action

	set_action(id, action + 4, VehStatusIcon[action + 4]);

}

/*
Checks if tile is a land coast.
*/
bool isCoast(MAP *tile)
{
	if (is_ocean(tile))
		return false;

	for (MAP *adjacentTile : getAdjacentTiles(tile))
	{
		if (is_ocean(adjacentTile))
			return true;
	}

	return false;

}

bool isOceanRegionCoast(int x, int y, int oceanRegion)
{
	MAP *tile = getMapTile(x, y);

	if (is_ocean(tile))
		return false;

	for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
	{
		if (is_ocean(adjacentTile) && adjacentTile->region == oceanRegion)
			return true;
	}

	return false;

}

/*
Returns vehicle in the same stack and attached for give transport.
*/
std::vector<int> getLoadedVehicleIds(int vehicleId)
{
	std::vector<int> loadedVehicleIds;

	// select those in stack attached to this one

	for (int stackedVehicleId : getStackVehicleIds(vehicleId))
	{
		VEH *stackedVehicle = &(Vehicles[stackedVehicleId]);

		// exclude self

		if (stackedVehicleId == vehicleId)
			continue;

		// get loaded vehicles

		if (stackedVehicle->order == ORDER_SENTRY_BOARD && stackedVehicle->waypoint_x[0] == vehicleId)
		{
			loadedVehicleIds.push_back(stackedVehicleId);
		}

	}

	return loadedVehicleIds;

}

bool is_ocean_deep(MAP* sq) {
    return (sq && (sq->climate >> 5) <= ALT_OCEAN);
}

bool isVehicleAtLocation(int vehicleId, int x, int y)
{
	assert(isOnMap(x, y));
	
	VEH *vehicle = &(Vehicles[vehicleId]);
	return vehicle->x == x && vehicle->y == y;
	
}

bool isVehicleAtLocation(int vehicleId, MAP *tile)
{
	assert(isOnMap(tile));
	return isVehicleAtLocation(vehicleId, getX(tile), getY(tile));
}

std::vector<int> getFactionLocationVehicleIds(int factionId, MAP *tile)
{
	std::vector<int> vehicleIds;

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if
		(
			vehicle->faction_id == factionId
			&&
			isVehicleAtLocation(vehicleId, tile)
		)
		{
			vehicleIds.push_back(vehicleId);
		}

	}

	return vehicleIds;

}

/*
Hurries base production and subtracts the cost from reserves.
*/
void hurryProduction(BASE* base, int minerals, int cost)
{
	debug("hurryProduction - %s\n", base->name);

	// hurry once only

    if (base->state_flags & BSTATE_HURRY_PRODUCTION)
		return;

	debug("\tminerals = %d, cost = %d\n", minerals, cost);

	// apply hurry parameters

    Faction* faction = &(Factions[base->faction_id]);
    base->minerals_accumulated += minerals;
    faction->energy_credits -= cost;
    base->state_flags |= BSTATE_HURRY_PRODUCTION;

}

bool isVehicleOnTransport(int vehicleId)
{
	return getVehicleTransportId(vehicleId) != -1;
}
bool isVehicleOnSeaTransport(int vehicleId)
{
	int transportVehicleId = getVehicleTransportId(vehicleId);
	return transportVehicleId != -1 && isSeaTransportVehicle(transportVehicleId);
}

bool isLandVehicleOnTransport(int vehicleId)
{
	return getVehicle(vehicleId)->triad() == TRIAD_LAND && isVehicleOnTransport(vehicleId);
}

int getVehicleTransportId(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	int transportId = -1;
	
	if
	(
		// in transport flag is set
		(vehicle->state & VSTATE_IN_TRANSPORT) != 0
		&&
		// sentry
		vehicle->order == ORDER_SENTRY_BOARD
		&&
		// on transport
		vehicle->waypoint_y[0] == 0
		&&
		// transportId is set
		vehicle->waypoint_x[0] >= 0
	)
	{
		transportId = vehicle->waypoint_x[0];
		
		// check transport is actually a transport
		
		if (!isTransportVehicle(transportId))
		{
			unboard(vehicleId);
			transportId = -1;
		}
		
	}
	
	return transportId;
	
}

int setMoveTo(int vehicleId, MAP *destination)
{
	assert(isOnMap(destination));
	
    VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
    MAP *vehicleTile = getVehicleMapTile(vehicleId);
    int vehicleSpeed1 = (getVehicleSpeed(vehicleId) == 1 ? 1 : 0);
	int x = getX(destination);
	int y = getY(destination);
	bool vehicleTileOcean = is_ocean(vehicleTile);
	bool destinationOcean = is_ocean(destination);
	
    debug("setMoveTo %s -> %s\n", getLocationString({vehicle->x, vehicle->y}).c_str(), getLocationString(destination).c_str());
	
    vehicle->waypoint_x[0] = x;
    vehicle->waypoint_y[0] = y;
    vehicle->order = ORDER_MOVE_TO;
    vehicle->status_icon = 'G';
    vehicle->movement_turns = 0;
	
    // vanilla bug fix for adjacent tile move
	
    int vehicleTileRangeToDestination = getRange(vehicleTile, destination);
    
    int destinationVehicleFactionId = veh_who(x, y);
    bool destinationBlocked = destinationVehicleFactionId == -1 ? false : !isFriendly(factionId, destinationVehicleFactionId);
	
    if (vehicle->triad() == TRIAD_LAND && !vehicleTileOcean && !destinationOcean && vehicleTileRangeToDestination == 1 && !destinationBlocked)
	{
		int directMoveHexCost = mod_hex_cost(vehicle->unit_id, vehicle->faction_id, vehicle->x, vehicle->y, x, y, vehicleSpeed1);
		bool destinationZoc = isZoc(factionId, vehicleTile, destination);
		
		// set direct move to infinite cost if zoc
		
		if (destinationZoc)
		{
			directMoveHexCost = INT_MAX;
		}
		
		// search optimal waypoint
		
		MAP *waypoint = nullptr;
		int waypointMoveHexCost = directMoveHexCost;
		
		for (MAP *adjacentTile : getAdjacentTiles(vehicleTile))
		{
			bool adjacentTileOcean = is_ocean(adjacentTile);
			int adjacentTileRangeToDestination = getRange(adjacentTile, destination);
			int adjacentTileX = getX(adjacentTile);
			int adjacentTileY = getY(adjacentTile);
			bool adjacentTileBlocked = isBlocked(factionId, adjacentTile);
			bool adjacentTileZoc = isZoc(factionId, vehicleTile, adjacentTile);
			
			// land
			
			if (adjacentTileOcean)
				continue;
			
			// adjacent tile should be also adjacent to destination
			
			if (adjacentTileRangeToDestination != 1)
				continue;
			
			// not blocked
			
			if (adjacentTileBlocked)
				continue;
			
			// not zoc
			
			if (adjacentTileZoc)
				continue;
			
			// compute cost
			
			int hexCost1 = mod_hex_cost(vehicle->unit_id, vehicle->faction_id, vehicle->x, vehicle->y, adjacentTileX, adjacentTileY, vehicleSpeed1);
			int hexCost2 = mod_hex_cost(vehicle->unit_id, vehicle->faction_id, adjacentTileX, adjacentTileY, x, y, vehicleSpeed1);
			int hexCost = hexCost1 + hexCost2;
			
			if (hexCost >= directMoveHexCost)
				continue;
			
			if (hexCost < waypointMoveHexCost)
			{
				waypoint = adjacentTile;
				waypointMoveHexCost = hexCost;
			}
			
		}
		
		if (waypoint != nullptr)
		{
			vehicle->waypoint_x[0] = getX(waypoint);
			vehicle->waypoint_y[0] = getY(waypoint);
			vehicle->waypoint_x[1] = x;
			vehicle->waypoint_y[1] = y;
			vehicle->waypoint_count = 1;
			debug("setWaypoint %s\n", getLocationString(waypoint).c_str());
		}
		
	}
	
    return EM_SYNC;
	
}

int setMoveTo(int vehicleId, const std::vector<MAP *> &waypoints)
{
    VEH* vehicle = &(Vehicles[vehicleId]);

    debug("setMoveTo %s -> waypoints\n", getLocationString({vehicle->x, vehicle->y}).c_str());

	setVehicleWaypoints(vehicleId, waypoints);
    vehicle->order = ORDER_MOVE_TO;
    vehicle->status_icon = 'G';
    vehicle->movement_turns = 0;

    return EM_SYNC;

}

/*
Checks if territory belongs to nobody, us or ally.
*/
bool isFriendlyTerritory(int factionId, MAP* tile)
{
	return tile->owner == -1 || tile->owner == factionId || has_pact(factionId, tile->owner);
}

/**
Checks if territory belongs to unfriendly owner.
*/
bool isUnfriendlyTerritory(int factionId, MAP* tile)
{
	return tile->owner != -1 && tile->owner != factionId && !has_pact(factionId, tile->owner);
}

/*
Checks if territory belongs to neutral faction.
*/
bool isNeutralTerritory(int factionId, MAP* tile)
{
	return tile->owner != -1 && tile->owner != factionId && !isPact(tile->owner, factionId) && !isVendetta(tile->owner, factionId);
}

/*
Checks if territory belongs to hostile faction.
*/
bool isHostileTerritory(int factionId, MAP* tile)
{
	return tile->owner != -1 && tile->owner != factionId && isVendetta(tile->owner, factionId);
}

bool isUnitHasAbility(int unitId, int ability)
{
	return unit_has_ability(unitId, ability);
}

bool isVehicleHasAbility(int vehicleId, int ability)
{
	return isUnitHasAbility(Vehicles[vehicleId].unit_id, ability);
}

bool isScoutUnit(int unitId)
{
	UNIT *unit = &(Units[unitId]);

	return unit->weapon_id == WPN_HAND_WEAPONS && unit->armor_id == ARM_NO_ARMOR;

}

bool isScoutVehicle(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);

	return unit->weapon_id == WPN_HAND_WEAPONS && unit->armor_id == ARM_NO_ARMOR;

}

bool isPodPoppingUnit(int unitId)
{
	return Units[unitId].triad() != TRIAD_AIR && (isScoutUnit(unitId) || isNativeUnit(unitId) || isOgreUnit(unitId));
}

bool isPodPoppingVehicle(int vehicleId)
{
	return isPodPoppingUnit(Vehicles[vehicleId].unit_id);
}

/*
Checks if location is targetted by any vehicle.
*/
bool isTargettedLocation(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (vehicle->order == ORDER_MOVE_TO && vehicle->x == x && vehicle->y == y)
			return true;

	}

	return false;

}

/*
Checks if location is targetted by faction vehicle.
*/
bool isFactionTargettedLocation(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// only given faction

		if (vehicle->faction_id != factionId)
			continue;

		if (vehicle->order == ORDER_MOVE_TO && vehicle->x == x && vehicle->y == y)
			return true;

	}

	return false;

}

/*
Computes newborn native psi attack strength.
*/
double getNativePsiAttackStrength(int triad)
{
	// get base odds

	double baseOdds = getPsiCombatBaseOdds(triad);

	// get morale

	int morale;

	if (*CurrentTurn < 45)
	{
		morale = 0;
	}
	else if (*CurrentTurn < 90)
	{
		morale = 1;
	}
	else if (*CurrentTurn < 170)
	{
		morale = 2;
	}
	else if (*CurrentTurn < 250)
	{
		morale = 3;
	}
	else if (*CurrentTurn < 330)
	{
		morale = 4;
	}
	else
	{
		morale = 5;
	}

	// compute strength

	return baseOdds * getMoraleMultiplier(morale);

}

double getMoraleMultiplier(int morale)
{
	return (1.0 + 0.125 * (morale - 2));
}

/*
Returns SE MORALE bonus.
*/
int getFactionSEMoraleBonus(int factionId)
{
	Faction *faction = &(Factions[factionId]);
	int factionSEMoraleRating = std::max(-4, std::min(4, faction->SE_morale));

	int factionSEMoraleBonus = 0;

	switch (factionSEMoraleRating)
	{
	case -4:
		factionSEMoraleBonus = -3;
		break;
	case -3:
		factionSEMoraleBonus = -2;
		break;
	case -2:
		factionSEMoraleBonus = -1;
		break;
	case -1:
		factionSEMoraleBonus = -1;
		break;
	case 0:
		factionSEMoraleBonus = 0;
		break;
	case 1:
		factionSEMoraleBonus = 1;
		break;
	case 2:
		factionSEMoraleBonus = 1;
		break;
	case 3:
		factionSEMoraleBonus = 2;
		break;
	case 4:
		factionSEMoraleBonus = 3;
		break;
	}

	return factionSEMoraleBonus;

}

/*
Returns vehicle morale with all default flags.
*/
int getVehicleMorale(int vehicleId)
{
	return mod_morale_veh(vehicleId, 0, 0);
}

/*
Returns new vehicle morale built at base.
*/
int getNewVehicleMorale(int unitId, int baseId)
{
	UNIT *unit = &(Units[unitId]);
	BASE *base = &(Bases[baseId]);

	// get default unit morale

	int morale = (conf.default_morale_very_green ? MORALE_VERY_GREEN : MORALE_GREEN);

	// modify morale for trained unit

	if (unit_has_ability(unitId, ABL_TRAINED))
	{
		morale++;
	}

	// modify morale based on morale facilities

	if (isNativeUnit(unitId))
	{
		if (isFactionHasProject(base->faction_id, FAC_XENOEMPATHY_DOME))
		{
			morale++;
		}

		if (isFactionHasProject(base->faction_id, FAC_PHOLUS_MUTAGEN))
		{
			morale++;
		}

		if (isFactionHasProject(base->faction_id, FAC_VOICE_OF_PLANET))
		{
			morale++;
		}

		if (isBaseHasFacility(baseId, FAC_CENTAURI_PRESERVE))
		{
			morale++;
		}

		if (isBaseHasFacility(baseId, FAC_TEMPLE_OF_PLANET))
		{
			morale++;
		}

		if (isBaseHasFacility(baseId, FAC_BIOLOGY_LAB))
		{
			morale++;
		}

		if (isBaseHasFacility(baseId, FAC_BIOENHANCEMENT_CENTER))
		{
			morale++;
		}

	}
	else
	{
		morale += morale_mod(baseId, base->faction_id, unit->triad());
	}

	// faction MORALE bonus for regular units

	if (!isNativeUnit(unitId))
	{
		morale += getFactionSEMoraleBonus(base->faction_id);
	}

	// return morale clipped by possible range

	return std::max((int)MORALE_VERY_GREEN, std::min((int)MORALE_ELITE, morale));

}

/*
Returns vehicle morale modifier.
*/
double getVehicleMoraleMultiplier(int vehicleId)
{
	return getMoraleMultiplier(getVehicleMorale(vehicleId));
}

double getBaseIntrinsicPsiDefenseMultiplier()
{
	return getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def);
}

bool isWithinFriendlySensorRange(int factionId, MAP *tile)
{
	Profiling::start("- isWithinFriendlySensorRange");
	
	// real sensors
	
	for (MAP *rangeTile : getRangeTiles(tile, 2, true))
	{
		// own or neutral territory
		
		if (!(rangeTile->owner == -1 || rangeTile->owner == factionId))
			continue;
		
		if (map_has_item(rangeTile, BIT_SENSOR))
		{
			Profiling::stop("- isWithinFriendlySensorRange");
			return true;
		}
		
	}
	
	// find bases with survey pod
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		if (base->faction_id != factionId)
			continue;
		
		if (getRange(baseTile, tile) > 2)
			continue;
		
		if (isBaseHasFacility(baseId, FAC_GEOSYNC_SURVEY_POD))
		{
			Profiling::stop("- isWithinFriendlySensorRange");
			return true;
		}
		
	}
	
	// not found
	
	Profiling::stop("- isWithinFriendlySensorRange");
	return false;
	
}

bool isOffenseSensorBonusApplicable(int factionId, MAP *tile)
{
	// offense bonus applies to the surface
	
	if (!conf.sensor_offense)
		return false;
	
	if (tile->is_sea() && !conf.sensor_offense_ocean)
		return false;
	
	return isWithinFriendlySensorRange(factionId, tile);
	
}
bool isDefenseSensorBonusApplicable(int factionId, MAP *tile)
{
	// defense bonus applies to the surface
	
	if (tile->is_sea() && !conf.sensor_defense_ocean)
		return false;
	
	return isWithinFriendlySensorRange(factionId, tile);
	
}

int getRegionPodCount(int x, int y, int range, int region)
{
	assert(isOnMap(x, y));
	
	int regionPodCount = 0;
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		// within range
		
		if (map_range(x, y, getX(tile), getY(tile)) > range)
			continue;
		
		// within region if given
		
		if (region >= 0 && tile->region != region)
			continue;
		
		// count pods
		
		if (mod_goody_at(getX(tile), getY(tile)) != 0)
		{
			regionPodCount++;
		}
		
	}
	
	return regionPodCount;
	
}

MAP *getNearbyItemLocation(int x, int y, int range, int item)
{
	MAP *nearbyItemLocation = nullptr;

	for (int dx = -(2 * range); dx <= (2 * range); dx++)
	{
		for (int dy = -(2 * range - abs(x)); dx <= +(2 * range - abs(x)); dy += 2)
		{
			int tileX = wrap(x + dx);
			int tileY = y + dy;

			MAP *tile = getMapTile(tileX, tileY);
			if (tile == NULL)
				continue;

			if (map_has_item(tile, item))
			{
				nearbyItemLocation = getMapTile(tileX, tileY);
				return nearbyItemLocation;
			}

		}

	}

	return nearbyItemLocation;

}

bool isMapRangeHasItem(int x, int y, int range, uint32_t item)
{
	for (MAP *tile : getRangeTiles(getMapTile(x, y), range, true))
	{
		if (map_has_item(tile, item))
			return true;
	}
	
	return false;
	
}

/*
Return true if vehicle order == ORDER_SENTRY_BOARD and damage > 0.
*/
bool isVehicleHealing(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	return vehicle->order == ORDER_SENTRY_BOARD && vehicle->damage_taken > 0;
}

/*
Finds if vehicle is in region including sea units in ports.
Air units are assumed to be in all regions at once.
*/
bool isVehicleInRegion(int vehicleId, int region)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int triad = vehicle->triad();

	bool vehicleInRegion = false;

	if (triad == TRIAD_AIR)
	{
		// air units are in all regions at once

		vehicleInRegion = true;

	}
	else if (triad == TRIAD_LAND)
	{
		// land units same region only

		if (vehicleTile->region == region)
		{
			vehicleInRegion = true;
		}

	}
	else if (triad == TRIAD_SEA)
	{
		// sea units same region

		if (vehicleTile->region == region)
		{
			vehicleInRegion = true;
		}

		// sea units in ports

		if (map_has_item(vehicleTile, BIT_BASE_IN_TILE) && isTileConnectedToRegion(vehicle->x, vehicle->y, region))
		{
			vehicleInRegion = true;
		}

	}

	return vehicleInRegion;

}

bool isProbeUnit(int unitId)
{
	return (Units[unitId].weapon_id == WPN_PROBE_TEAM);
}

bool isProbeVehicle(int vehicleId)
{
	return isProbeUnit(Vehicles[vehicleId].unit_id);
}

/*
Computes relative attacker/defender strength.
*/
double battleCompute(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat)
{
	VEH *attackerVehicle = &(Vehicles[attackerVehicleId]);
	VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);
	MAP *defenderVehicleTile = getVehicleMapTile(defenderVehicleId);

	// set flags

	int flags = 0x00;

	if
	(
		longRangeCombat
		&&
		(
			(attackerVehicle->triad() == TRIAD_LAND && can_arty(attackerVehicle->unit_id, 1))
			||
			(attackerVehicle->triad() == TRIAD_SEA && can_arty(attackerVehicle->unit_id, 1) && defenderVehicle->triad() == TRIAD_LAND && !is_ocean(defenderVehicleTile))
		)
	)
	{
		flags |= 0x01;

		if (can_arty(defenderVehicle->unit_id, 1))
		{
			flags |= 0x02;
		}

	}

	if
	(
		attackerVehicle->triad() == TRIAD_AIR && Chassis[Units[attackerVehicle->unit_id].chassis_id].missile == 0
		&&
		attackerVehicle->offense_value() > 0 && attackerVehicle->defense_value() > 0
		&&
		vehicle_has_ability(defenderVehicleId, ABL_AIR_SUPERIORITY) && Chassis[Units[attackerVehicle->unit_id].chassis_id].missile == 0
		&&
		defenderVehicle->offense_value() > 0
	)
	{
		flags |= 0x0A;

		if (defenderVehicle->triad() == TRIAD_AIR)
		{
			flags |= 0x10;
		}

	}

	// compute battle

	int attackerOffenseValue;
	int defenderStrength;

	wtp_mod_battle_compute(attackerVehicleId, defenderVehicleId, &attackerOffenseValue, &defenderStrength, flags);

	// calculate relative strength

	double relativeStrength = (double)attackerOffenseValue / (double)defenderStrength;

	// adjust for aliens to fight at half strength

	if (attackerVehicle->faction_id == 0)
	{
		relativeStrength *= getAlienTurnStrengthModifier();
	}
	if (defenderVehicle->faction_id == 0)
	{
		relativeStrength /= getAlienTurnStrengthModifier();
	}

	// return relative strength

	return relativeStrength;

}

/*
Computes relative attacker/defender strength against best defender in stack.
*/
double battleComputeStack(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat)
{
	assert(attackerVehicleId >= 0 && attackerVehicleId < *VehCount);
	assert(defenderVehicleId >= 0 && defenderVehicleId < *VehCount);

	// find best defender

	int bestDefenderVehicleId = mod_best_defender(defenderVehicleId, attackerVehicleId, (int)longRangeCombat);

	// compute relative strength against best defender

	return battleCompute(attackerVehicleId, bestDefenderVehicleId, longRangeCombat);

}

int getVehicleSpeedWithoutRoads(int id)
{
	return getVehicleSpeed(id);
}

VehWeapon getFactionBestWeapon(int factionId)
{
	int bestWeapon = WPN_HAND_WEAPONS;
	int bestWeaponOffenseValue = 1;
	
	for (int weaponId = WPN_LASER; weaponId <= WPN_STRING_DISRUPTOR; weaponId++)
	{
		if (isFactionHasTech(factionId, Weapon[weaponId].preq_tech))
		{
			if (Weapon[weaponId].offense_value > bestWeaponOffenseValue)
			{
				bestWeapon = weaponId;
				bestWeaponOffenseValue = Weapon[weaponId].offense_value;
			}
			
		}
		
	}
	
	return (VehWeapon)bestWeapon;
	
}

/*
Searches for faction best weapon not exceeding limit.
*/
VehWeapon getFactionBestWeapon(int factionId, int limit)
{
	int bestWeapon = WPN_HAND_WEAPONS;
	int bestWeaponOffenseValue = 1;
	
	for (int weaponId = WPN_LASER; weaponId <= WPN_STRING_DISRUPTOR; weaponId++)
	{
		if (isFactionHasTech(factionId, Weapon[weaponId].preq_tech))
		{
			if (Weapon[weaponId].offense_value > limit)
				continue;
			
			if (Weapon[weaponId].offense_value > bestWeaponOffenseValue)
			{
				bestWeapon = weaponId;
				bestWeaponOffenseValue = Weapon[weaponId].offense_value;
			}
			
		}
		
	}
	
	return (VehWeapon)bestWeapon;
	
}

VehArmor getFactionBestArmor(int factionId)
{
	int bestArmor = ARM_NO_ARMOR;
	int bestArmorDefenseValue = 1;
	
	for (int armorId = ARM_SYNTHMETAL_ARMOR; armorId <= ARM_RESONANCE_8_ARMOR; armorId++)
	{
		if (isFactionHasTech(factionId, Armor[armorId].preq_tech))
		{
			if (Armor[armorId].defense_value > bestArmorDefenseValue)
			{
				bestArmor = armorId;
				bestArmorDefenseValue = Armor[armorId].defense_value;
			}
			
		}
		
	}
	
	return (VehArmor)bestArmor;
	
}

/*
Searches for faction best armor not exceeding limit.
*/
VehArmor getFactionBestArmor(int factionId, int limit)
{
	int bestArmor = ARM_NO_ARMOR;
	int bestArmorDefenseValue = 1;
	
	for (int armorId = ARM_SYNTHMETAL_ARMOR; armorId <= ARM_RESONANCE_8_ARMOR; armorId++)
	{
		if (isFactionHasTech(factionId, Armor[armorId].preq_tech))
		{
			if (Armor[armorId].defense_value > limit)
				continue;
			
			if (Armor[armorId].defense_value > bestArmorDefenseValue)
			{
				bestArmor = armorId;
				bestArmorDefenseValue = Armor[armorId].defense_value;
			}
			
		}
		
	}
	
	return (VehArmor)bestArmor;
	
}

/*
Finds base in tile.
Returns base id if found. Otherwise, -1.
*/
int getBaseIdAt(int x, int y)
{
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		if (base->x == x && base->y == y)
			return baseId;

	}

	return -1;

}

/*
Determines if unit is a native predefined unit.
*/
bool isNativeUnit(int unitId)
{
	return unitId < 0x40 && getUnitOffenseValue(unitId) < 0;
}

/*
Determines if vehicle is a native predefined unit.
*/
bool isNativeVehicle(int vehicleId)
{
	return isNativeUnit(getVehicle(vehicleId)->unit_id);
}

double getPercentageBonusMultiplier(int percentageBonus)
{
	return 1.0 + (double)percentageBonus / 100.0;
}

/**
Checks if building mine at this location grants +1 mineral bonus.
*/
bool isMineBonus(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	return
	(
		bonus_at(x, y) == RES_MINERAL
		||
		(map_has_landmark(tile, LM_VOLCANO) && tile->code_at() < 9)
		||
		(map_has_landmark(tile, LM_CRATER) && tile->code_at() < 9)
		||
		(map_has_landmark(tile, LM_FOSSIL) && tile->code_at() < 6)
		||
		map_has_landmark(tile, LM_CANYON)
	)
	;
	
}

std::vector<int> selectVehicles(const VehicleFilter filter)
{
	std::vector<int> vehicleIds;

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (filter.factionId >= 0 && vehicle->faction_id != filter.factionId)
			continue;

		if (filter.triad >= 0 && vehicle->triad() != filter.triad)
			continue;

		if (filter.weaponType >= 0 && vehicle->weapon_type() != filter.weaponType)
			continue;

		vehicleIds.push_back(vehicleId);

	}

	return vehicleIds;

}

/*
Checks if next to region
*/
bool isNextToRegion(int x, int y, int region)
{
	for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
	{
		if (adjacentTile->region == region)
			return true;
	}

	return false;

}

int getSeaTransportInStack(int vehicleId)
{
	for (int stackedVehicleId : getStackVehicleIds(vehicleId))
	{
		VEH *stackedVehicle = &(Vehicles[stackedVehicleId]);

		if (stackedVehicle->weapon_type() == WPN_TROOP_TRANSPORT)
			return stackedVehicleId;

	}

	return -1;

}

bool isSeaTransportInStack(int vehicleId)
{
	return getSeaTransportInStack(vehicleId) != -1;
}

/*
Checks if there is sea transport in tile.
*/
bool isSeaTransportAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	int vehicleIdAtTile = veh_at(x, y);

	if (vehicleIdAtTile == -1)
		return false;

	for (int vehicleId : getStackVehicleIds(vehicleIdAtTile))
	{
		VEH *vehicle = getVehicle(vehicleId);

		// own

		if (vehicle->faction_id != factionId)
			continue;

		// sea transport

		if (!isSeaTransportVehicle(vehicleId))
			continue;

		// all check passed

		return true;

	}

	return false;

}

/*
Checks if there is available sea transport in tile.
*/
bool isAvailableSeaTransportAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	int vehicleIdAtTile = veh_at(x, y);

	if (vehicleIdAtTile == -1)
		return false;

	for (int vehicleId : getStackVehicleIds(vehicleIdAtTile))
	{
		VEH *vehicle = getVehicle(vehicleId);

		// own

		if (vehicle->faction_id != factionId)
			continue;

		// sea transport

		if (!isSeaTransportVehicle(vehicleId))
			continue;

		// available

		if (getTransportRemainingCapacity(vehicleId) <= 0)
			continue;

		// all check passed

		return true;

	}

	return false;

}

int countPodsInBaseRadius(int x, int y)
{
	assert(isOnMap(x, y));
	
	int podCount = 0;

	for (MAP *tile : getBaseRadiusTiles(x, y, true))
	{
		if ((mod_goody_at(getX(tile), getY(tile)) != 0))
		{
			podCount++;
		}
	}

	return podCount;

}

bool isBlocked(int factionId, MAP *tile)
{
	assert(isOnMap(tile));
	
	// search for unfriendly unit in given tile
	
	int tileOwner = veh_who(getX(tile), getY(tile));
	
	if (tileOwner != -1 && tileOwner != factionId && !has_pact(factionId, tileOwner))
		return true;
	
	return false;
	
}

bool isZoc(int factionId, MAP *fromTile, MAP *toTile)
{
	assert(isOnMap(fromTile));
	assert(isOnMap(toTile));
	
	// no ZOC on ocean
	
	if (is_ocean(fromTile) || is_ocean(toTile))
		return false;
	
	// friendly unit allows entry
	
	int toTileOwner = veh_who(getX(toTile), getY(toTile));
	if (toTileOwner != -1 && isFriendly(factionId, toTileOwner))
		return false;
	
	// no ZOC on base
	
	if (isBaseAt(fromTile) || isBaseAt(toTile))
		return false;
	
	// search for unfriendly unit in adjacent tiles
	
	bool fromZoc = false;
	bool toZoc = false;
	
	for (MAP *adjacentTile : getAdjacentTiles(fromTile))
	{
		// vehicle on ocean does not excert ZOC
		
		if (is_ocean(adjacentTile))
			continue;
		
		int adjacentTileOwner = veh_who(getX(adjacentTile), getY(adjacentTile));
		
		if (adjacentTileOwner != -1 && adjacentTileOwner != factionId && !has_pact(factionId, adjacentTileOwner))
		{
			fromZoc = true;
			break;
		}
		
	}
	
	for (MAP *adjacentTile : getAdjacentTiles(toTile))
	{
		// vehicle on ocean does not excert ZOC
		
		if (is_ocean(adjacentTile))
			continue;
		
		int adjacentTileOwner = veh_who(getX(adjacentTile), getY(adjacentTile));
		
		if (adjacentTileOwner != -1 && adjacentTileOwner != factionId && !has_pact(factionId, adjacentTileOwner))
		{
			toZoc = true;
			break;
		}
		
	}
	
	return fromZoc && toZoc;
	
}

/*
Calculates relative attacker odds (= relative strength * relative power) taking damage and reactor into account.
*/
double getBattleOdds(int attackerVehicleId, int defenderVehicleId, int longRangeCombat)
{
	VEH *attackerVehicle = &(Vehicles[attackerVehicleId]);
	VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);

	// calculate relative strength

	double relativeStrength = battleCompute(attackerVehicleId, defenderVehicleId, longRangeCombat);

	// check whether battle ignores reactor

	bool ignoreReactor =
		Weapon[attackerVehicle->weapon_type()].offense_value < 0 || Armor[defenderVehicle->armor_type()].defense_value < 0
		||
		conf.ignore_reactor_power
	;

	// calculate attacker and defender power

	int attackerPower = 10 * (ignoreReactor ? 1 : attackerVehicle->reactor_type()) - attackerVehicle->damage_taken / (ignoreReactor ? attackerVehicle->reactor_type() : 1);
	int defenderPower = 10 * (ignoreReactor ? 1 : defenderVehicle->reactor_type()) - defenderVehicle->damage_taken / (ignoreReactor ? defenderVehicle->reactor_type() : 1);

	// calculate odds

	return relativeStrength * ((double)attackerPower / (double)defenderPower);

}

double getBattleOddsAt(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat, MAP *battleTile)
{
	assert(isOnMap(battleTile));
	
	VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);
	int battleTileX = getX(battleTile);
	int battleTileY = getY(battleTile);

	// store current defender location

	int currentDefenderVehicleX = defenderVehicle->x;
	int currentDefenderVehicleY = defenderVehicle->y;

	// set defender battle location

	defenderVehicle->x = battleTileX;
	defenderVehicle->y = battleTileY;

	// compute battle

	double result = getBattleOdds(attackerVehicleId, defenderVehicleId, longRangeCombat);

	// restore defender original location

	defenderVehicle->x = currentDefenderVehicleX;
	defenderVehicle->y = currentDefenderVehicleY;

	// return result

	return result;

}

/*
Calculates relative attacker odds fighting best defender in stack.
*/
double getBattleStackOdds(int attackerVehicleId, int defenderVehicleId, int longRangeCombat)
{
	assert(attackerVehicleId >= 0 && attackerVehicleId < *VehCount);
	assert(defenderVehicleId >= 0 && defenderVehicleId < *VehCount);

	// find best defender

	int bestDefenderVehicleId = mod_best_defender(defenderVehicleId, attackerVehicleId, longRangeCombat);

	// compute relative strength against best defender

	return getBattleOdds(attackerVehicleId, bestDefenderVehicleId, longRangeCombat);

}

/*
Calculates average attack effect.
*/
double getAttackEffect(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat)
{
	VEH *attackerVehicle = &(Vehicles[attackerVehicleId]);
	VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);

	// calculate relative strength

	double relativeStrength = battleCompute(attackerVehicleId, defenderVehicleId, longRangeCombat);

	// calculate attacker and defender HP

	bool psiCombat = isPsiCombat(attackerVehicle->unit_id, defenderVehicle->unit_id);

	int attackerHP = getVehicleHitPoints(attackerVehicleId, psiCombat);
	int defenderHP = getVehicleHitPoints(defenderVehicleId, psiCombat);

	// calculate effect

	double attackEffect = relativeStrength * (double)attackerHP / (double)defenderHP;

	return attackEffect;

}

/*
Estimates how many enemies vehicle can destroy on average attacking a stack.
*/
double getStackAttackEffect(int attackerVehicleId, std::vector<int> defenderVehicleIds, bool longRangeCombat)
{
	// verify attacker is combat vehicle

	if (!isCombatVehicle(attackerVehicleId))
		return 0.0;

	// verify attacker vehicle is capable of given combat type fight

	if (longRangeCombat)
	{
		if (!isArtilleryVehicle(attackerVehicleId))
			return 0.0;
	}
	else
	{
		if (isLandArtilleryVehicle(attackerVehicleId))
			return 0.0;
	}

	// calculate total loss

	int alienAttackCount = 0;
	double alienAttackLoss = 0.0;
	int normalAttackCount = 0;
	double normalAttackLossSum = 0.0;
	double flatAttackEffect = 0.0;

	for (int defenderVehicleId : defenderVehicleIds)
	{
		VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);

		// get attack effect

		double attackEffect = getAttackEffect(attackerVehicleId, defenderVehicleId, longRangeCombat);

		// zero attack effect means enemy is indistructible for this vehicle

		if (attackEffect <= 0.0)
			return 0.0;

		if (longRangeCombat && !isArtilleryVehicle(defenderVehicleId))
		{
			// cannot inflict more arillery damage if at the limit

			if (isAtArtilleryDamageLimit(defenderVehicleId))
				continue;

			// accumulate flat effect

			flatAttackEffect += attackEffect;

		}
		else
		{
			// calculate attacker loss

			double attackLoss = 1.0 / attackEffect;

			if (defenderVehicle->faction_id == 0)
			{
				alienAttackCount = 1;
				alienAttackLoss = std::max(alienAttackLoss, attackLoss);
			}
			else
			{
				normalAttackCount++;
				normalAttackLossSum += attackLoss;
			}

		}

	}

	// calculate direct attack effect

	double directAttackEffect;

	if ((alienAttackCount + normalAttackCount) <= 0 || (alienAttackLoss + normalAttackLossSum) <= 0.0)
	{
		directAttackEffect = 0.0;
	}
	else
	{
		directAttackEffect = 1.0 / ((alienAttackLoss + normalAttackLossSum) / (alienAttackCount + normalAttackCount));
	}

	// summarize combined effect

	double stackAttackEffect = directAttackEffect + flatAttackEffect;

	// return value

	return stackAttackEffect;

}

int getUnitOffenseValue(int unitId)
{
	return Weapon[Units[unitId].weapon_id].offense_value;
}

int getUnitDefenseValue(int unitId)
{
	return Armor[Units[unitId].armor_id].defense_value;
}

int getVehicleOffenseValue(int vehicleId)
{
	return getUnitOffenseValue(Vehicles[vehicleId].unit_id);
}

int getVehicleDefenseValue(int vehicleId)
{
	return getUnitDefenseValue(Vehicles[vehicleId].unit_id);
}

bool isFactionSpecial(int factionId, int special)
{
    return MFactions[factionId].rule_flags & special;
}

bool isFactionHasBonus(int factionId, int bonusId)
{
	MFaction *metaFaction = &(MFactions[factionId]);

	for (int bonusIndex = 0; bonusIndex < metaFaction->faction_bonus_count; bonusIndex++)
	{
		if (metaFaction->faction_bonus_id[bonusIndex] == bonusId)
			return true;

	}

	return false;

}

/*
Computes newborn native psi attack strength.
*/
double getAlienMoraleMultiplier()
{
	// get morale

	int morale;

	if (*CurrentTurn < 45)
	{
		morale = 0;
	}
	else if (*CurrentTurn < 90)
	{
		morale = 1;
	}
	else if (*CurrentTurn < 170)
	{
		morale = 2;
	}
	else if (*CurrentTurn < 250)
	{
		morale = 3;
	}
	else if (*CurrentTurn < 330)
	{
		morale = 4;
	}
	else
	{
		morale = 5;
	}

	// compute morale modifier

	return getMoraleMultiplier(morale);

}

/*
Finds nearest land location owned by faction.
*/
MAP *getNearestLandTerritory(int x, int y, int factionId)
{
	MAP *nearestTerritory = nullptr;
	int minRange = INT_MAX;
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int mapX = getX(tile);
		int mapY = getY(tile);
		
		// land
		
		if (is_ocean(tile))
			continue;
		
		// faction owned
		
		if (tile->owner != factionId)
			continue;
		
		// get range
		
		int range = map_range(x, y, mapX, mapY);
		
		// update best spot
		
		if (range < minRange)
		{
			nearestTerritory = tile;
			minRange = range;
		}
		
	}
	
	return nearestTerritory;
	
}

/*
Kills vehicle properly with transport check.
*/
void killVehicle(int vehicleId)
{
	kill(vehicleId);
}

/**
Checks if vehicle is boarded to transport.
*/
bool isVehicleOnTransport(int vehicleId, int transportVehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	return vehicle->order == ORDER_SENTRY_BOARD && vehicle->waypoint_y[0] == 0 && vehicle->waypoint_x[0] == transportVehicleId && (vehicle->state & VSTATE_IN_TRANSPORT) != 0;
}

/**
Boards vehicle a transport.
*/
void board(int vehicleId, int transportVehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);
	
	// check if vehicle is boarded to this transport already
	
	if (isVehicleOnTransport(vehicleId, transportVehicleId))
		return;
	
	// vehicle should be land vehicle
	
	if (vehicle->triad() != TRIAD_LAND)
		return;
	
	// transport should be transport
	
	if (!isTransportVehicle(transportVehicleId))
		return;
	
	// vehicle and transport should be at the same tile
	
	if (vehicleTile != transportVehicleTile)
		return;
	
	// transport should have capacity
	
	if (!isTransportHasCapacity(transportVehicleId))
		return;
	
	// board vehicle on transport
	
	setVehicleOrder(vehicleId, ORDER_SENTRY_BOARD);
	vehicle->waypoint_y[0] = 0;
	vehicle->waypoint_x[0] = transportVehicleId;
	vehicle->state |= VSTATE_IN_TRANSPORT;
	
}

void board(int vehicleId)
{
	VEH* vehicle = getVehicle(vehicleId);

	// boarded already

	int transportVehicleId = getVehicleTransportId(vehicleId);

	if (transportVehicleId != -1)
		return;

	// find available transport in same tile

	for (int tileVehicleId : getStackVehicleIds(vehicleId))
	{
		// transport

		if (!isTransportVehicle(tileVehicleId))
			continue;

		// available

		if (getTransportRemainingCapacity(tileVehicleId) == 0)
			continue;

		// found

		transportVehicleId = tileVehicleId;
		break;

	}

	// not found

	if (transportVehicleId == -1)
		return;

	// board

	setVehicleOrder(vehicleId, ORDER_SENTRY_BOARD);
	vehicle->waypoint_y[0] = 0;
	vehicle->waypoint_x[0] = transportVehicleId;
	vehicle->state |= VSTATE_IN_TRANSPORT;

}

void unboard(int vehicleId)
{
	VEH* vehicle = &(Vehicles[vehicleId]);

	setVehicleOrder(vehicleId, ORDER_NONE);
	vehicle->waypoint_y[0] = 0;
	vehicle->waypoint_x[0] = -1;
	vehicle->state &= ~VSTATE_IN_TRANSPORT;

}

int getTransportCapacity(int transportVehicleId)
{
	assert(transportVehicleId >= 0 && transportVehicleId < *VehCount);
	return tx_veh_cargo(transportVehicleId);
}

int getTransportUsedCapacity(int transportVehicleId)
{
	assert(transportVehicleId >= 0 && transportVehicleId < *VehCount);
	
	int transportUsedCapacity = 0;
	
	// iterate vehicles
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		// get transportId
		
		int transportId = getVehicleTransportId(vehicleId);
		
		// assigned to this transport
		
		if (transportId != transportVehicleId)
			continue;
		
		// increment used capacity
		
		transportUsedCapacity++;
		
	}
	
	return transportUsedCapacity;
	
}

int getTransportRemainingCapacity(int transportVehicleId)
{
	assert(transportVehicleId >= 0 && transportVehicleId < *VehCount);
	return tx_veh_cargo(transportVehicleId) - getTransportUsedCapacity(transportVehicleId);
}

bool isTransportHasCapacity(int transportVehicleId)
{
	return getTransportRemainingCapacity(transportVehicleId) > 0;
}

/*
Returns vehicle max power.
*/
int getVehicleMaxPower(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return 10 * vehicle->reactor_type();

}

/*
Returns vehicle current power.
*/
int getVehiclePower(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return 10 * vehicle->reactor_type() - vehicle->damage_taken;

}

double getVehicleRelativeDamage(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return (double)vehicle->damage_taken / (double)(10 * vehicle->reactor_type());

}

double getVehicleRelativePower(int vehicleId)
{
	return 1.0 - getVehicleRelativeDamage(vehicleId);
}

int getVehicleHitPoints(int vehicleId, bool psiCombat)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	int power = getVehiclePower(vehicleId);

	return (isReactorIgnoredInCombat(psiCombat) ? (power + (vehicle->reactor_type() - 1)) / vehicle->reactor_type() : power);

}

/*
Computes base GROWTH rate.
*/
int getBaseGrowthRate(int baseId)
{
	// store current base pointers

	int currentBaseId = *CurrentBaseID;

	// set base pointers

	set_base(baseId);

	// compute base nutrients

	tx_base_nutrient();

	// restore current base pointers

	set_base(currentBaseId);

	// return computed GROWTH rate

	return *BaseGrowthRate;

}

/**
Factions have commlink.
*/
bool isCommlink(int factionId1, int factionId2)
{
	assert(factionId1 >= 0 && factionId1 < MaxPlayerNum);
	assert(factionId2 >= 0 && factionId2 < MaxPlayerNum);
	
	return factionId1 != factionId2 && factionId1 != 0 && factionId2 != 0 && (getFaction(factionId1)->diplo_status[factionId2] & DIPLO_COMMLINK);
	
}

/**
Factions have vendetta.
Aliens are always at war with other factions.
*/
bool isVendetta(int factionId1, int factionId2)
{
	assert(factionId1 >= 0 && factionId1 < MaxPlayerNum);
	assert(factionId2 >= 0 && factionId2 < MaxPlayerNum);
	
	return factionId1 != factionId2 && (factionId1 == 0 || factionId2 == 0 || at_war(factionId1, factionId2));
	
}

/**
Factions have pact.
*/
bool isPact(int factionId1, int factionId2)
{
	assert(factionId1 >= 0 && factionId1 < MaxPlayerNum);
	assert(factionId2 >= 0 && factionId2 < MaxPlayerNum);
	
	return factionId1 != factionId2 && factionId1 != 0 && factionId2 != 0 && has_pact(factionId1, factionId2);
	
}

/**
Factions have treaty.
*/
bool isTreaty(int factionId1, int factionId2)
{
	assert(factionId1 >= 0 && factionId1 < MaxPlayerNum);
	assert(factionId2 >= 0 && factionId2 < MaxPlayerNum);
	
	return factionId1 != factionId2 && factionId1 != 0 && factionId2 != 0 && (getFaction(factionId1)->diplo_status[factionId2] & DIPLO_TREATY);
	
}

/**
Factions can collocate each other vehicles.
Any faction is friendly to itself.
*/
bool isFriendly(int factionId1, int factionId2)
{
	assert(factionId1 >= 0 && factionId1 < MaxPlayerNum);
	assert(factionId2 >= 0 && factionId2 < MaxPlayerNum);
	
	return factionId1 == factionId2 || isPact(factionId1, factionId2);
	
}

/**
Factions can NOT collocate each other vehicles.
*/
bool isUnfriendly(int factionId1, int factionId2)
{
	return factionId1 != factionId2 && !isPact(factionId1, factionId2);
}

/**
Factions have vendetta.
Aliens are always at war with other factions.
*/
bool isHostile(int factionId1, int factionId2)
{
	return isVendetta(factionId1, factionId2);
}

/**
Factions do not have pact and do not have vendetta.
*/
bool isNeutral(int factionId1, int factionId2)
{
	return factionId1 != factionId2 && !isFriendly(factionId1, factionId2) && !isHostile(factionId1, factionId2);
}

/*
Returns vehicle max hit points in psi combat.
*/
int getVehicleMaxPsiHP(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	int opponentFirePower = vehicle->reactor_type();
	return (10 * vehicle->reactor_type()) / opponentFirePower;

}

/*
Returns vehicle current hit points in psi combat.
*/
int getVehicleCurrentPsiHP(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	int opponentFirePower = vehicle->reactor_type();
	return (10 * vehicle->reactor_type() - vehicle->damage_taken / (opponentFirePower + 1)) / opponentFirePower;

}

/*
Returns vehicle max hit points in conventional combat.
*/
int getVehicleMaxConHP(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	int opponentFirePower = (conf.ignore_reactor_power ? vehicle->reactor_type() : 1);
	return (10 * vehicle->reactor_type()) / opponentFirePower;

}

/*
Returns vehicle current hit points in conventional combat.
*/
int getVehicleCurrentConHP(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	int opponentFirePower = (conf.ignore_reactor_power ? vehicle->reactor_type() : 1);
	return (10 * vehicle->reactor_type() - vehicle->damage_taken / (opponentFirePower + 1)) / opponentFirePower;

}

/**
Returns unit slot number by unitId.
Slot number is the unique unit index across all faction units.
*/
int getUnitSlotById(int unitId)
{
	assert(unitId >= 0 && unitId < MaxProtoNum);
	return (unitId < MaxProtoFactionNum ? unitId : MaxProtoFactionNum + unitId % MaxProtoFactionNum);
}

/**
Returns unitId by faction and slot number.
*/
int getUnitIdBySlot(int factionId, int slot)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	assert(slot >= 0 && slot < 2 * MaxProtoFactionNum);
	assert(factionId != 0 || slot < MaxProtoFactionNum);
	return (slot < MaxProtoFactionNum ? slot : MaxProtoFactionNum * (factionId - 1) + slot);
}

/**
Returns unit index by unitId.
Unit index is the unique unit index across all factions and units.
*/
int getUnitIndex(int factionId, int unitId)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	assert(unitId >= 0 && unitId < MaxProtoNum);
	return (2 * MaxProtoFactionNum) * factionId + getUnitSlotById(unitId);
}

/**
Extracts factionId from unitIndex.
*/
int getFactionIdByUnitIndex(int unitIndex)
{
	assert((unitIndex >= 0 && unitIndex < MaxProtoFactionNum) || (unitIndex >= (2 * MaxProtoFactionNum) && unitIndex < (2 * MaxProtoFactionNum) * MaxPlayerNum));
	return unitIndex / (2 * MaxProtoFactionNum);
}

/**
Extracts unitId from unitIndex.
*/
int getUnitIdByUnitIndex(int unitIndex)
{
	assert((unitIndex >= 0 && unitIndex < MaxProtoFactionNum) || (unitIndex >= (2 * MaxProtoFactionNum) && unitIndex < (2 * MaxProtoFactionNum) * MaxPlayerNum));
	return getUnitIdBySlot(getFactionIdByUnitIndex(unitIndex), unitIndex % (2 * MaxProtoFactionNum));
}

/**
Retrieves number of drones currently suppressed by police.
*/
int getBasePoliceSuppressedDrones(int baseId)
{
	// compute base
	
	Profiling::start("- getBasePoliceSuppressedDrones - computeBase");
	computeBase(baseId, false);
	Profiling::stop("- getBasePoliceSuppressedDrones - computeBase");
	
	// get drones currently suppressed by police
	
	return *CURRENT_BASE_DRONES_FACILITIES - *CURRENT_BASE_DRONES_POLICE;
	
}

/*
Returns true if reactor is ignored in combat.
*/
bool isReactorIgnoredInCombat(bool psiCombat)
{
	return psiCombat || conf.ignore_reactor_power;
}

bool isInfantryUnit(int unitId)
{
	return getUnit(unitId)->chassis_id == CHS_INFANTRY;
}

bool isInfantryVehicle(int vehicleId)
{
	return isInfantryUnit(getVehicle(vehicleId)->unit_id);
}

bool isPolice2xUnit(int unitId, int factionId)
{
	bool wormPolice = isFactionSpecial(factionId, RFLAG_WORMPOLICE);
	return (isCombatUnit(unitId) && isUnitHasAbility(unitId, ABL_POLICE_2X)) || (wormPolice && (unitId == BSC_MIND_WORMS || unitId == BSC_SPORE_LAUNCHER));
}

bool isPolice2xVehicle(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	return isPolice2xUnit(vehicle->unit_id, vehicle->faction_id);
}

bool isInfantryPolice2xUnit(int unitId, int factionId)
{
	return isInfantryUnit(unitId) && isPolice2xUnit(unitId, factionId);
}

bool isInfantryPolice2xVehicle(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	return isInfantryPolice2xUnit(vehicle->unit_id, vehicle->faction_id);
}

/*
Determines combat type between units.
*/
bool isPsiCombat(int attackerUnitId, int defenderUnitId)
{
	return (Weapon[Units[attackerUnitId].weapon_id].offense_value < 0 || Armor[Units[defenderUnitId].armor_id].defense_value < 0);
}

double getAlienTurnStrengthModifier()
{
	return (*CurrentTurn <= conf.native_weak_until_turn ? 0.5 : 1.0);
}

int getFactionMaintenance(int factionId)
{
	return Factions[factionId].facility_maint_total;
}

int getFactionNetIncome(int factionId)
{
	energy_compute(factionId, 0);
	return *net_income;
}

int getFactionGrossIncome(int factionId)
{
	return getFactionNetIncome(factionId) + getFactionMaintenance(factionId);
}

double getFactionTechPerTurn(int factionId)
{
	energy_compute(factionId, 0);
	return (double)*tech_per_turn / 100.0;
}

bool isVehicleOnSentry(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return vehicle->order == ORDER_SENTRY_BOARD && vehicle->waypoint_x[0] == -1 && vehicle->waypoint_y[0] == 0;

}

bool isVehicleOnAlert(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return vehicle->order == ORDER_HOLD && vehicle->waypoint_x[0] == -1 && vehicle->waypoint_y[0] == 0 && vehicle->waypoint_x[1] == vehicle->x && vehicle->waypoint_y[1] == vehicle->y;

}

bool isVehicleOnHold(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return vehicle->order == ORDER_HOLD && vehicle->waypoint_x[0] == -1 && vehicle->waypoint_y[0] == 0 && vehicle->waypoint_x[1] == -1 && vehicle->waypoint_y[1] == -1;

}

bool isVehicleOnHold10Turns(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return vehicle->order == ORDER_HOLD && vehicle->waypoint_x[0] == -1 && vehicle->waypoint_y[0] == 10 && vehicle->waypoint_x[1] == -1 && vehicle->waypoint_y[1] == -1;

}

void setVehicleOnSentry(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	vehicle->order = ORDER_SENTRY_BOARD;
	vehicle->waypoint_x[0] = -1;
	vehicle->waypoint_y[0] = 0;

}

void setVehicleOnAlert(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	vehicle->order = ORDER_HOLD;
	vehicle->waypoint_x[0] = -1;
	vehicle->waypoint_y[0] = 0;
	vehicle->waypoint_x[1] = vehicle->x;
	vehicle->waypoint_y[1] = vehicle->y;

}

void setVehicleOnHold(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	vehicle->order = ORDER_HOLD;
	vehicle->waypoint_x[0] = -1;
	vehicle->waypoint_y[0] = 0;
	vehicle->waypoint_x[1] = -1;
	vehicle->waypoint_y[1] = -1;

}

void setVehicleOnHold10Turns(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	vehicle->order = ORDER_HOLD;
	vehicle->waypoint_x[0] = -1;
	vehicle->waypoint_y[0] = 10;
	vehicle->waypoint_x[1] = -1;
	vehicle->waypoint_y[1] = -1;

}

/*
Calculates correct difference between two x coordinates.
*/
int getDx(int x1, int x2)
{
	int Dx = x2 - x1;

	if (!*MapToggleFlat)
	{
		if (abs(Dx) > *MapHalfX)
		{
			if (Dx > 0)
			{
				Dx -= *MapAreaX;
			}
			else
			{
				Dx += *MapAreaX;
			}
		}
	}

	return Dx;

}

/*
Calculates correct difference between two y coordinates.
*/
int getDy(int y1, int y2)
{
	return y2 - y1;
}

int getAbilityProportionalCost(int abilityId)
{
	int abilityCost = Ability[abilityId].cost;
	int abilityProportionalCost = (abilityCost & 0xF);
	return abilityProportionalCost;
}

int getAbilityFlatCost(int abilityId)
{
	int abilityCost = Ability[abilityId].cost;
	int abilityFlatCost = ((abilityCost >> 4) & 0xF);
	return abilityFlatCost;
}

UNIT *getUnit(int unitId)
{
	return &(Units[unitId]);
}

VEH *getVehicle(int vehicleId)
{
	if (vehicleId >= 0 && vehicleId < *VehCount)
	{
		return &(Vehicles[vehicleId]);
	}
	else
	{
		return nullptr;
	}
}

BASE *getBase(int baseId)
{
	return (baseId >= 0 && baseId < *BaseCount ? &(Bases[baseId]) : nullptr);
}

bool isBaseHasNativeRepairFacility(int baseId)
{
	return breed_mod(baseId, getBase(baseId)->faction_id) > 0;
}

bool isBaseHasRegularRepairFacility(int baseId, int triad)
{
	int regularRepairFacility;

	switch (triad)
	{
	case TRIAD_LAND:
		regularRepairFacility = FAC_COMMAND_CENTER;
		break;
	case TRIAD_SEA:
		regularRepairFacility = FAC_NAVAL_YARD;
		break;
	case TRIAD_AIR:
		regularRepairFacility = FAC_AEROSPACE_COMPLEX;
		break;
	default:
		return false;
	}

	return isBaseHasFacility(baseId, regularRepairFacility);

}

MFaction *getMFaction(int factionId)
{
	return &(MFactions[factionId]);
}

Faction *getFaction(int factionId)
{
	return &(Factions[factionId]);
}

CChassis *getChassis(int chassisId)
{
	return &(Chassis[chassisId]);
}

CChassis *getUnitChassis(int unitId)
{
	return getChassis(getUnit(unitId)->chassis_id);
}

CChassis *getVehicleChassis(int vehicleId)
{
	return getChassis(getUnit(getVehicle(vehicleId)->unit_id)->chassis_id);
}

bool isNeedlejetUnit(int unitId)
{
	return getUnit(unitId)->chassis_id == CHS_NEEDLEJET;
}

bool isNeedlejetVehicle(int vehicleId)
{
	return isNeedlejetUnit(getVehicle(vehicleId)->unit_id);
}

void longRangeFire(int vehicleId, int offsetIndex)
{
	battle_fight_1(vehicleId, offsetIndex, 1, 1, 0);
}
void longRangeFire(int vehicleId, MAP *attackLocation)
{
	assert(isOnMap(attackLocation));
	
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int x = getX(attackLocation);
	int y = getY(attackLocation);

	// check range

	int range = map_range(vehicle->x, vehicle->y, x, y);

	if (range > Rules->artillery_max_rng)
		return;

	// get offsetIndex

	int offsetIndex = getOffsetIndex(vehicleTile, attackLocation);

	// long range fire

	longRangeFire(vehicleId, offsetIndex);

}

bool isVehicleFullRepair(int vehicleId, MAP *tile)
{
	VEH *vehicle = getVehicle(vehicleId);

	return
		map_has_item(tile, BIT_BASE_IN_TILE | BIT_BUNKER | BIT_MONOLITH)
		||
		(isNativeVehicle(vehicleId) && map_has_item(tile, BIT_FUNGUS))
		||
		isFactionHasProject(vehicle->faction_id, FAC_NANO_FACTORY)
	;

}

RepairInfo getVehicleRepairInfo(int vehicleId, MAP *tile)
{
	assert(isOnMap(tile));
	
	VEH *vehicle = getVehicle(vehicleId);
	int triad = vehicle->triad();
	bool native = isNativeVehicle(vehicleId);

	// battle ogres are not repairable

	if (isOgreVehicle(vehicleId))
	{
		return {false, 0, 0};
	}

	// repair damage

	int damage = divideIntegerRoundUp(vehicle->damage_taken, vehicle->reactor_type());

	// conditions

	bool fullRepair = false;
	int repairRate = conf.repair_minimal;

	// monolith

	if (map_has_item(tile, BIT_MONOLITH))
	{
		fullRepair = true;
	}

	// native in fungus

	if (native && map_has_item(tile, BIT_FUNGUS))
	{
		fullRepair = true;
		repairRate = conf.repair_fungus;
	}

	// friendly territory

	if (tile->owner == vehicle->faction_id)
	{
		repairRate += conf.repair_friendly;
	}

	// nano factory

	if (isFactionHasProject(vehicle->faction_id, FAC_NANO_FACTORY))
	{
		fullRepair = true;
		repairRate += conf.repair_nano_factory;
	}

	// check location

	if (map_has_item(tile, BIT_BASE_IN_TILE))
	{
		int baseId = getBaseAt(tile);

		fullRepair = true;
		repairRate += conf.repair_base;

		if (native)
		{
			if (isBaseHasNativeRepairFacility(baseId))
			{
				repairRate += conf.repair_base_native;
			}
		}
		else
		{
			if (isBaseHasRegularRepairFacility(baseId, triad))
			{
				repairRate += conf.repair_base_facility;
			}
		}

	}
	else if (triad == TRIAD_LAND && map_has_item(tile, BIT_BUNKER) && !is_ocean(tile))
	{
		fullRepair = true;
		repairRate += conf.repair_bunker;
	}
	else if (triad == TRIAD_AIR && map_has_item(tile, BIT_AIRBASE))
	{
		fullRepair = true;
		repairRate += conf.repair_airbase;
	}

	// repairRate should not be zero

	if (repairRate == 0)
	{
		debug("[ERROR] Repair rate is zero.");
		exit(1);
	}

	// update repair damage

	int repairDamage = std::max(0, (fullRepair ? damage : damage - 2));

	// repair time

	int repairTime = (map_has_item(tile, BIT_MONOLITH) ? 0 : divideIntegerRoundUp(repairDamage, repairRate));

	// return repair object

	return {fullRepair, repairDamage, repairTime};

}

/*
Checks if unit is obsolete.
*/
bool isObsoleteUnit(int unitId, int factionId)
{
	UNIT *unit = getUnit(unitId);

	return ((unit->obsolete_factions & (0x1 << factionId)) != 0);

}

/**
Checks if unit is prototyped.
*/
bool isPrototypedUnit(int unitId)
{
	return unitId < MaxProtoFactionNum || Units[unitId].is_prototyped();
}

/*
Checks if unit is active.
*/
bool isActiveUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);

	return (unit->unit_flags & UNIT_ACTIVE);

}

/*
Checks if unit is listed in faction workshop.
*/
bool isAvailableUnit(int unitId, int factionId, bool includeObsolete)
{
	UNIT *unit = getUnit(unitId);

	if (unitId < MaxProtoFactionNum)
	{
		// predefined unit has to be discovered to become available

		if (!isFactionHasTech(factionId, unit->preq_tech))
			return false;

	}

	// unit has to be active and not obsolete

	return isActiveUnit(unitId) && (includeObsolete || !isObsoleteUnit(unitId, factionId));

}

CAbility *getAbility(int abilityId)
{
	assert(abilityId >= ABL_ID_SUPER_TERRAFORMER && abilityId <= ABL_ID_ALGO_ENHANCEMENT);
	return &(Ability[abilityId]);
}

void obsoleteUnit(int unitId, int factionId)
{
	UNIT *unit = getUnit(unitId);

	unit->obsolete_factions |= (0x1 << factionId);

}

void reinstateUnit(int unitId, int factionId)
{
	UNIT *unit = getUnit(unitId);

	unit->obsolete_factions &= ~(0x1 << factionId);

}

int divideIntegerRoundUp(int divident, int divisor)
{
	return (divisor <= 0 ? INT_MAX : (divident + (divisor - 1)) / divisor);
}

CFacility *getFacility(int facilityId)
{
	return &(Facility[facilityId]);
}

bool isAtArtilleryDamageLimit(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	bool vehicleTileOcean = is_ocean(vehicleTile);

	int damagePercentageLimit;

	if (map_has_item(vehicleTile, BIT_BASE_IN_TILE | BIT_BUNKER))
	{
		damagePercentageLimit = Rules->max_dmg_percent_arty_base_bunker;
	}
	else
	{
		damagePercentageLimit = (!vehicleTileOcean ? Rules->max_dmg_percent_arty_open : Rules->max_dmg_percent_arty_sea);
	}

	int damageLimit = (10 * vehicle->reactor_type()) * damagePercentageLimit / 100;

	return (vehicle->damage_taken >= damageLimit);

}

CCitizen *getCitizen(int specialistType)
{
	return &(Citizen[specialistType]);
}

/*
Sets base specialist at given index to given type.
Does not change total number of specialists.
*/
void setBaseSpecialist(int baseId, int specialistIndex, int specialistType)
{
	BASE *base = getBase(baseId);

	specialistType &= 0xF;
	int specialistArrayIndex = specialistIndex / 8;
	int specialistPosition = (specialistIndex % 8) * 4;

	base->specialist_types[specialistArrayIndex] &= ~(0xF << specialistPosition);
	base->specialist_types[specialistArrayIndex] |= (specialistType << specialistPosition);

}

/*
Attempts to remove base doctor and returns true if successful.
*/
bool removeBaseDoctor(int baseId)
{
	BASE *base = getBase(baseId);
	int doctors = getBaseDoctors(baseId);

	// no doctors

	if (doctors == 0)
		return false;

	// remove doctor

	base->specialist_total--;

	return true;

}

/*
Remove all base doctors.
*/
void removeBaseDoctors(int baseId)
{
	while (removeBaseDoctor(baseId)){}
}

/*
Attempts to add base doctor and returns true if successful.
*/
bool addBaseDoctor(int baseId)
{
	BASE *base = getBase(baseId);
	int doctors = getBaseDoctors(baseId);

	// all citizens are doctors

	if (doctors == base->pop_size)
		return false;

	// specialist limit is reached

	if (doctors == 16)
		return false;

	// find best doctor

	int doctorSpecialistType = getDoctorSpecialistType(base->faction_id);

	// add doctor

	if (base->specialist_total < base->pop_size)
	{
		// add another doctor specialist

		setBaseSpecialist(baseId, base->specialist_total, doctorSpecialistType);
		base->specialist_total++;

	}
	else
	{
		// turn one existing non-doctor specialist to doctor

		int specialistIndex = (base->specialist_total - 1) - doctors;
		setBaseSpecialist(baseId, specialistIndex, doctorSpecialistType);

	}

	return true;

}

/*
Returns max number of turns for air vehicle to stay in air until rebasing.
*/
int getVehicleMaxRebaseTurns(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	UNIT *unit = getUnit(vehicle->unit_id);
	CChassis *chassis = getChassis(unit->chassis_id);

	int maxRebaseTurns;

	switch (unit->chassis_id)
	{
	case CHS_NEEDLEJET:
	case CHS_MISSILE:
		maxRebaseTurns = chassis->range;
		break;
	case CHS_COPTER:
		maxRebaseTurns = 4;
		break;
	default:
		maxRebaseTurns = -1;
	}

	return maxRebaseTurns;

}

CTerraform *getTerraform(int terraformType)
{
	return &(Terraform[terraformType]);
}

/*
Returns computer base b-drones.
*/
int getBaseBDrones(int baseId)
{
	BASE *base = getBase(baseId);

	set_base(baseId);
	base_psych();

	return (*CURRENT_BASE_DRONES_UNMODIFIED - std::max(0, base->pop_size - 3)) + *CURRENT_BASE_SDRONES_UNMODIFIED;

}

/**
Checks whether it is allowed to build base here without breaking treaty.
territory: unclaimed, own
Even though game allows building on "no commlink" or "vendetta" territory, this is excluded for simplicity.
*/
bool isAllowedBaseLocation(int factionId, MAP *tile)
{
	return tile->owner == -1 || tile->owner == factionId;
}

bool isMobileUnit(int unitId)
{
	switch (getUnit(unitId)->chassis_id)
	{
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		return true;
	default:
		return false;
	}
}

/*
Checks whether tile is an airbase (friendly base or airbase) for this factionId.
*/
bool isAtAirbase(int vehicleId)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return map_has_item(vehicleTile, BIT_BASE_IN_TILE | BIT_AIRBASE);
}

bool isUnitCanAttackNeedlejet(int unitId)
{
	return unit_has_ability(unitId, ABL_AIR_SUPERIORITY);
}

bool isUnitCanAttackLowAir(int unitId)
{
	return isMeleeUnit(unitId) || (isArtilleryUnit(unitId) && unit_has_ability(unitId, ABL_AIR_SUPERIORITY));
}

int getClosestHostileBaseRange(int factionId, MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	int closestHostileBaseRange = INT_MAX;

	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);

		// not this faction

		if (base->faction_id == factionId)
			continue;

		// hostile to this faction

		if (!isVendetta(base->faction_id, factionId))
			continue;

		// range

		int range = map_range(x, y, base->x, base->y);

		// update best

		closestHostileBaseRange = std::min(closestHostileBaseRange, range);

	}

	return closestHostileBaseRange;

}

double getEuclidianDistance(int x1, int y1, int x2, int y2)
{
	assert(isOnMap(x1, y1));
	assert(isOnMap(x2, y2));
	
	int dx = x2 - x1;
	int dy = y2 - y1;

	if (!*MapToggleFlat)
	{
		if (dx < -*MapHalfX)
		{
			dx += *MapAreaX;
		}
		else if (dx > *MapHalfX)
		{
			dx -= *MapAreaX;
		}
	}

	return sqrt(0.5 * (dx * dx + dy * dy));

}

int getVehicleUnitCost(int vehicleId)
{
	return getUnit(getVehicle(vehicleId)->unit_id)->cost;
}

int getVehicleRemainingMovement(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	return getVehicleMoveRate(vehicleId) - vehicle->moves_spent;
}

/*
Determines if vehicle can move between tiles.
*/
bool isAllowedMove(int vehicleId, MAP *srcTile, MAP *dstTile)
{
	assert(isOnMap(srcTile));
	assert(isOnMap(dstTile));
	
	VEH *vehicle = getVehicle(vehicleId);
	UNIT *unit = getUnit(vehicle->unit_id);
	int triad = unit->triad();
	int srcX = getX(srcTile);
	int srcY = getY(srcTile);
	int dstX = getX(dstTile);
	int dstY = getY(dstTile);
	bool srcOcean = is_ocean(srcTile);
	bool dstOcean = is_ocean(dstTile);

	// cannot enter unfriendly base

	if (isUnfriendlyBaseAt(dstTile, vehicle->faction_id))
		return false;

	// cannot enter tile with unfriendly units

	int dstTileVehicleId = veh_at(dstX, dstY);
	if (dstTileVehicleId != -1 && !isFriendly(vehicle->faction_id, getVehicle(dstTileVehicleId)->faction_id))
		return false;

	// check triad specific conditions

	bool allowed = false;

	switch (triad)
	{
	case TRIAD_LAND:

		if (!srcOcean && !dstOcean)
		{
			// land to land

			// moves from base or into friendly base

			if (map_has_item(srcTile, BIT_BASE_IN_TILE))
			{
				allowed = true;
				break;
			}

			if (isFriendlyBaseAt(dstTile, vehicle->faction_id))
			{
				allowed = true;
				break;
			}

			// ignores ZOC

			if (isZocIgnoringVehicle(vehicleId))
			{
				allowed = true;
				break;
			}

			// ZOC

			if (zoc_move(srcX, srcY, vehicle->faction_id) && zoc_move(dstX, dstY, vehicle->faction_id))
			{
				allowed = false;
				break;
			}

			// otherwise allowed

			allowed = true;
			break;

		}
		else
		{
			allowed = isLandVechileMoveAllowed(vehicleId, srcTile, dstTile);
		}

		break;

	case TRIAD_SEA:

		if
		(
			(!srcOcean && !isBaseAt(srcTile))
			||
			(!dstOcean && !isFriendlyBaseAt(dstTile, vehicle->faction_id))
		)
		{
			// sea vechile is not allowed to be on land unless in port

			allowed = false;

		}
		else
		{
			// otherwise allowed

			allowed = true;

		}

		break;

	case TRIAD_AIR:

		// air vehicle is not restricted by surface or zoc

		allowed = true;

		break;

	}

	return allowed;

}

bool isZocAffectedUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);
	
	if (unit->triad() != TRIAD_LAND)
		return false;
	
	if (unit->plan == PLAN_PROBE || unit->plan == PLAN_ARTIFACT)
		return false;
	
	if (unit_has_ability(unitId, ABL_CLOAKED))
		return false;
	
	return true;
	
}

bool isZocAffectedVehicle(int vehicleId)
{
	return isZocAffectedUnit(getVehicle(vehicleId)->unit_id);
}

bool isZocIgnoringUnit(int unitId)
{
	return !isZocAffectedUnit(unitId);
}

bool isZocIgnoringVehicle(int vehicleId)
{
	return isZocIgnoringUnit(getVehicle(vehicleId)->unit_id);
}

int getBaseAt(MAP *tile)
{
	assert(isOnMap(tile));
	return base_at(getX(tile), getY(tile));
}

bool isBaseAt(MAP *tile)
{
	assert(isOnMap(tile));
	return (map_has_item(tile, BIT_BASE_IN_TILE));
}

bool isFactionBaseAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	return (map_has_item(tile, BIT_BASE_IN_TILE) && tile->owner == factionId);
}

bool isFriendlyBaseAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	return (map_has_item(tile, BIT_BASE_IN_TILE) && isFriendlyTerritory(factionId, tile));
}

bool isHostileBaseAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	return (map_has_item(tile, BIT_BASE_IN_TILE) && isHostileTerritory(factionId, tile));
}

bool isNeutralBaseAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	return (map_has_item(tile, BIT_BASE_IN_TILE) && isNeutralTerritory(factionId, tile));
}

bool isUnfriendlyBaseAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	return (map_has_item(tile, BIT_BASE_IN_TILE) && !isFriendlyTerritory(factionId, tile));
}

bool isEmptyHostileBaseAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	return isHostileBaseAt(tile, factionId) && !isVehicleAt(tile);
}

bool isEmptyNeutralBaseAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	return isNeutralBaseAt(tile, factionId) && !isVehicleAt(tile);
}

bool isBunkerAt(MAP *tile)
{
	assert(isOnMap(tile));
	return (map_has_item(tile, BIT_BUNKER));
}

bool isVehicleAt(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	return veh_at(x, y) != -1;

}

bool isHostileVehicleAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	int vehicleId = veh_at(x, y);

	if (vehicleId == -1)
		return false;

	return isHostile(factionId, getVehicle(vehicleId)->faction_id);

}

bool isUnfriendlyVehicleAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	int vehicleId = veh_at(x, y);

	if (vehicleId == -1)
		return false;

	return !isFriendly(factionId, getVehicle(vehicleId)->faction_id);

}

bool isHostileSurfaceVehicleAt(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	
	bool airbase = isAirbaseAt(tile);

	for (int tileVehicleId : getTileVehicleIds(tile))
	{
		VEH *tileVehicle = getVehicle(tileVehicleId);

		// hostile

		if (!isHostile(factionId, tileVehicle->faction_id))
			continue;

		// land, sea or air at airbase

		if (tileVehicle->triad() != TRIAD_AIR || airbase)
			return true;

	}

	return false;

}

/*
Checks if unit can generally attack enemy at tile.
*/
bool isUnitCanInitiateArtilleryDuel(int attackerUnitId, int defenderUnitId)
{
	return (isArtilleryUnit(attackerUnitId) && isSurfaceUnit(defenderUnitId) && isArtilleryUnit(defenderUnitId));
}

/*
Checks if unit can bombard enemy from given tile.
*/
bool isUnitCanInitiateBombardment(int attackerUnitId, int defenderUnitId)
{
	return (isArtilleryUnit(attackerUnitId) && isSurfaceUnit(defenderUnitId) && !isArtilleryUnit(defenderUnitId));
}

bool isAirbaseAt(MAP *tile)
{
	return map_has_item(tile, BIT_BASE_IN_TILE | BIT_AIRBASE);
}

bool isArtilleryAt(MAP *tile)
{
	for (int tileVehicleId : getTileVehicleIds(tile))
	{
		if (isArtilleryVehicle(tileVehicleId))
			return true;

	}

	return false;

}

std::vector<int> getTileSurfaceVehicleIds(MAP *tile)
{
	assert(isOnMap(tile));
	
	std::vector<int> surfaceVehicleIds;

	for (int tileVehicleId : getTileVehicleIds(tile))
	{
		if (isSurfaceVehicle(tileVehicleId))
		{
			surfaceVehicleIds.push_back(tileVehicleId);
		}

	}

	return surfaceVehicleIds;

}

bool isSurfaceUnit(int unitId)
{
	return (getUnit(unitId)->triad() != TRIAD_AIR);
}

bool isSurfaceVehicle(int vehicleId)
{
	return (getVehicle(vehicleId)->triad() != TRIAD_AIR || isAtAirbase(vehicleId));
}

bool isBombardmentUnit(int unitId)
{
	return (getUnit(unitId)->triad() != TRIAD_AIR);
}

bool isBombardmentVehicle(int vehicleId)
{
	return isSurfaceVehicle(vehicleId) && !isArtilleryVehicle(vehicleId);
}

void setVehicleWaypoints(int vehicleId, const std::vector<MAP *> &waypoints)
{
	VEH *vehicle = getVehicle(vehicleId);

	if (waypoints.size() >= 1)
	{
		vehicle->waypoint_x[0] = getX(waypoints.at(0));
		vehicle->waypoint_y[0] = getY(waypoints.at(0));
	}
	if (waypoints.size() >= 2)
	{
		vehicle->waypoint_x[1] = getX(waypoints.at(1));
		vehicle->waypoint_y[1] = getY(waypoints.at(1));
	}
	if (waypoints.size() >= 3)
	{
		vehicle->waypoint_x[2] = getX(waypoints.at(2));
		vehicle->waypoint_y[2] = getY(waypoints.at(2));
	}
	if (waypoints.size() >= 4)
	{
		vehicle->waypoint_x[3] = getX(waypoints.at(3));
		vehicle->waypoint_y[3] = getY(waypoints.at(3));
	}

}

bool isHoveringLandUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);
	return unit->chassis_id == CHS_HOVERTANK || isUnitHasAbility(unitId, ABL_ANTIGRAV_STRUTS);
}

bool isHoveringLandVehicle(int vehicleId)
{
	return isHoveringLandUnit(getVehicle(vehicleId)->unit_id);
}

bool isEasyFungusEnteringLandUnit(int unitId)
{
	return isFormerUnit(unitId) || isArtifactUnit(unitId);
}

bool isEasyFungusEnteringVehicle(int vehicleId)
{
	return isEasyFungusEnteringLandUnit(getVehicle(vehicleId)->unit_id);
}

double getBaseMineralMultiplier(int baseId)
{
	int multiplierNumerator = 4;
	int multiplierDenominator = 4;

	if (isBaseHasFacility(baseId, FAC_RECYCLING_TANKS))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_GENEJACK_FACTORY))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_ROBOTIC_ASSEMBLY_PLANT))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_QUANTUM_CONVERTER))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_NANOREPLICATOR))
	{
		multiplierNumerator += 2;
	}

	return (double)multiplierNumerator / (double)multiplierDenominator;

}

/**
Returns base energy efficiency coefficient.
*/
double getBaseEnergyEfficiencyCoefficient(int baseId)
{
	BASE *base = getBase(baseId);
	
	return base->energy_intake <= 0 ? 1.0 : 1.0 - (double)base->energy_inefficiency / (double)base->energy_intake;
	
}

/**
Computes energy yield multiplier conversion to budget (economy + labs).
*/
double getBaseEnergyMultiplier(int baseId)
{
	BASE *base = getBase(baseId);
	Faction *faction = getFaction(base->faction_id);
	
	double economyAllocation = (double)(10 - faction->SE_alloc_labs + faction->SE_alloc_psych) / 10.0;
	double economyMultiplier = getBaseEconomyMultiplier(baseId);
	double labsAllocation = (double)(faction->SE_alloc_labs) / 10.0;
	double labsMultiplier = getBaseLabsMultiplier(baseId);
	
	double energyMultiplier = economyAllocation * economyMultiplier + labsAllocation * labsMultiplier;
	
	return energyMultiplier;
	
}

double getBaseEconomyMultiplier(int baseId)
{
	int multiplierNumerator = 4;
	int multiplierDenominator = 4;

	if (isBaseHasFacility(baseId, FAC_ENERGY_BANK))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_TREE_FARM))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_HYBRID_FOREST))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_FUSION_LAB))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_QUANTUM_LAB))
	{
		multiplierNumerator += 2;
	}

	return (double)multiplierNumerator / (double)multiplierDenominator;

}

double getBaseLabsMultiplier(int baseId)
{
	int multiplierNumerator = 4;
	int multiplierDenominator = 4;

	if (isBaseHasFacility(baseId, FAC_NETWORK_NODE))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_RESEARCH_HOSPITAL))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_NANOHOSPITAL))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_FUSION_LAB))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_QUANTUM_LAB))
	{
		multiplierNumerator += 2;
	}

	return (double)multiplierNumerator / (double)multiplierDenominator;

}

double getBasePsychMultiplier(int baseId)
{
	int multiplierNumerator = 4;
	int multiplierDenominator = 4;

	if (isBaseHasFacility(baseId, FAC_HOLOGRAM_THEATRE))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_TREE_FARM))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_HYBRID_FOREST))
	{
		multiplierNumerator += 2;
	}
	if (isBaseHasFacility(baseId, FAC_RESEARCH_HOSPITAL))
	{
		multiplierNumerator += 1;
	}
	if (isBaseHasFacility(baseId, FAC_NANOHOSPITAL))
	{
		multiplierNumerator += 1;
	}

	return (double)multiplierNumerator / (double)multiplierDenominator;

}

/*
Checks if land vehicle can move.
*/
bool isLandVechileMoveAllowed(int vehicleId, MAP *from, MAP *to)
{
	assert(isOnMap(from));
	assert(isOnMap(to));
	assert(vehicleId >= 0 && vehicleId < *VehCount);

	VEH *vehicle = getVehicle(vehicleId);
	bool fromOcean = is_ocean(from);
	bool toOcean = is_ocean(to);

	if (vehicle->triad() != TRIAD_LAND)
		return false;

	if (toOcean) // to ocean
	{
		if (isVehicleHasAbility(vehicleId, ABL_AMPHIBIOUS))
		{
			if (isAvailableSeaTransportAt(to, vehicle->faction_id) || isBaseAt(to))
				return true;
		}
		else
		{
			if (isAvailableSeaTransportAt(to, vehicle->faction_id) || isFriendlyBaseAt(to, vehicle->faction_id))
				return true;
		}

		return false;

	}
	else if (fromOcean) // from ocean to land
	{
		if (isVehicleHasAbility(vehicleId, ABL_AMPHIBIOUS))
		{
			if (isSeaTransportAt(from, vehicle->faction_id))
				return true;
		}
		else
		{
			if (isSeaTransportAt(from, vehicle->faction_id) && !isUnfriendlyVehicleAt(to, vehicle->faction_id))
				return true;
		}

		return false;

	}
	else // land to land
	{
		return true;
	}

}

int getRange(int x1, int y1, int x2, int y2)
{
	assert(isOnMap(x1, y1));
	assert(isOnMap(x2, y2));
	
	return map_range(x1, y1, x2, y2);
	
}
int getRange(int tile1Index, int tile2Index)
{
	assert(tile1Index >= 0 && tile1Index < *MapAreaTiles);
	assert(tile2Index >= 0 && tile2Index < *MapAreaTiles);
	
	int y1 = tile1Index / (*MapHalfX);
	int x1 = (tile1Index % (*MapHalfX)) * 2 + (y1 % 2);
	int y2 = tile2Index / (*MapHalfX);
	int x2 = (tile2Index % (*MapHalfX)) * 2 + (y2 % 2);
	
	return getRange(x1, y1, x2, y2);
	
}
int getRange(MAP const *tile1, MAP const *tile2)
{
	return getRange(getX(tile1), getY(tile1), getX(tile2), getY(tile2));
}

int getVectorDist(int x1, int y1, int x2, int y2)
{
	assert(isOnMap(x1, y1));
	assert(isOnMap(x2, y2));
	
	return vector_dist(x1, y1, x2, y2);
	
}
int getVectorDist(int tile1Index, int tile2Index)
{
	assert(tile1Index >= 0 && tile1Index < *MapAreaTiles);
	assert(tile2Index >= 0 && tile2Index < *MapAreaTiles);
	
	int y1 = tile1Index / (*MapHalfX);
	int x1 = (tile1Index % (*MapHalfX)) * 2 + (y1 % 2);
	int y2 = tile2Index / (*MapHalfX);
	int x2 = (tile2Index % (*MapHalfX)) * 2 + (y2 % 2);
	
	return getVectorDist(x1, y1, x2, y2);
	
}
int getVectorDist(MAP const *tile1, MAP const *tile2)
{
	assert(tile1 >= *MapTiles && tile1 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	return getVectorDist(getX(tile1), getY(tile1), getX(tile2), getY(tile2));
	
}

/*
Returns doubled vector distance to turn halves into wholes.
*/
int getProximity(int x1, int y1, int x2, int y2)
{
	assert(isOnMap(x1, y1));
	assert(isOnMap(x2, y2));
	
	int dx = abs(x1 - x2);
	int dy = abs(y1 - y2);
	if (!map_is_flat() && dx > *MapHalfX)
	{
		dx = *MapAreaX - dx;
	}
	
    return (min(dx, dy) + 3 * max(dx, dy)) / 2;
    
}

/*
Returns exact Euclidian distance between points squared.
*/
double getEuqlideanDistanceSquared(MAP *origin, MAP *destination)
{
	assert(isOnMap(origin));
	assert(isOnMap(destination));

	Location delta = getDelta(origin, destination);

	return (double)(delta.x * delta.x + delta.y * delta.y) / 2.0;
}

/*
Returns exact Euclidian distance between points.
*/
double getEuqlideanDistance(MAP *origin, MAP *destination)
{
	assert(isOnMap(origin));
	assert(isOnMap(destination));
	return sqrt(getEuqlideanDistanceSquared(origin, destination));
}

/**
Computes distance between two tiles.
Diagonal step counts as 1.5.
*/
double getDiagonalDistance(MAP *tile1, MAP *tile2)
{
	assert(isOnMap(tile1));
	assert(isOnMap(tile2));
	Location delta = getAbsoluteDelta(tile1, tile2);
	return (double)delta.minAbs() + 1.5 * (double)delta.absDiff() / 2.0;
}

/**
Computes double distance between two tiles.
Diagonal step counts as 1.5.
*/
int getDiagonalDistanceDoubled(MAP *tile1, MAP *tile2)
{
	assert(isOnMap(tile1));
	assert(isOnMap(tile2));
	Location delta = getAbsoluteDelta(tile1, tile2);
	return 2 * delta.minAbs() + 3 * delta.absDiff() / 2;
}

Location getLocationByAngle(int x, int y, int angle)
{
	assert(isOnMap(x, y));
	assert(angle >= 0 && angle < TABLE_next_cell_count);
	
	int nx = wrap(x + TABLE_next_cell_x[angle]);
	int ny = y + TABLE_next_cell_y[angle];
	
	if (!isOnMap(nx, ny))
		return Location();
	
	return Location(nx, ny);
	
}

int getTileIndexByAngle(int tileIndex, int angle)
{
	assert(tileIndex >= 0 && tileIndex < *MapAreaTiles);
	assert(angle >= 0 && angle < TABLE_next_cell_count);
	
	int x = getX(tileIndex);
	int y = getY(tileIndex);
	
	int nx = wrap(x + TABLE_next_cell_x[angle]);
	int ny = y + TABLE_next_cell_y[angle];
	
	if (!isOnMap(nx, ny))
		return -1;
	
	return getMapTileIndex(nx, ny);
	
}

MAP *getTileByAngle(MAP *tile, int angle)
{
	assert(isOnMap(tile));
	assert(angle >= 0 && angle < TABLE_next_cell_count);
	
	int x = getX(tile);
	int y = getY(tile);
	
	int nx = wrap(x + TABLE_next_cell_x[angle]);
	int ny = y + TABLE_next_cell_y[angle];
	
	if (!isOnMap(nx, ny))
		return nullptr;
	
	return getMapTile(nx, ny);
	
}

int getAngleByTile(MAP *tile, MAP *anotherTile)
{
	assert(isOnMap(tile));
	assert(anotherTile >= *MapTiles && anotherTile < *MapTiles + *MapAreaTiles);

	int tileX = getX(tile);
	int tileY = getY(tile);
	int anotherTileX = getX(anotherTile);
	int anotherTileY = getY(anotherTile);

	int dx = anotherTileX - tileX;
	int dy = anotherTileY - tileY;

	if (dx < -*MapHalfX)
	{
		dx += *MapAreaX;
	}
	else if (dx > +*MapHalfX)
	{
		dx -= *MapAreaX;
	}

	// not adjacent

	if (abs(dx) + abs(dy) != 2)
		return -1;

	int angle = -1;

	switch (dx)
	{
	case -2:
		angle = 5;
		break;
	case +2:
		angle = 1;
		break;
	case -1:
		switch (dy)
		{
		case -1:
			angle = 6;
			break;
		case +1:
			angle = 4;
			break;
		}
		break;
	case +1:
		switch (dy)
		{
		case -1:
			angle = 0;
			break;
		case +1:
			angle = 2;
			break;
		}
		break;
	case 0:
		switch (dy)
		{
		case -2:
			angle = 7;
			break;
		case +2:
			angle = 3;
			break;
		}
		break;
	}

	return angle;

}

bool isPodAt(MAP *tile)
{
	assert(isOnMap(tile));
	return mod_goody_at(getX(tile), getY(tile)) != 0;
}

std::vector<int> getTransportPassengers(int transportVehicleId)
{
	std::vector<int> passengerVehicleIds;

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		if (getVehicleTransportId(vehicleId) == transportVehicleId)
		{
			passengerVehicleIds.push_back(vehicleId);
		}

	}

	return passengerVehicleIds;

}

/**
Computes max relative bombardment damage vehicles can receive at this location.
*/
double getLocationBombardmentMaxRelativeDamage(MAP *tile)
{
	int maxBombardmentDamagePercentage = 100;
	
	if (isBaseAt(tile) || isBunkerAt(tile))
	{
		maxBombardmentDamagePercentage = Rules->max_dmg_percent_arty_base_bunker;
	}
	else if (is_ocean(tile))
	{
		maxBombardmentDamagePercentage = Rules->max_dmg_percent_arty_sea;
	}
	else
	{
		maxBombardmentDamagePercentage = Rules->max_dmg_percent_arty_open;
	}
	
	return getPercentageBonusMultiplier(-maxBombardmentDamagePercentage);
	
}

double getVehicleBombardmentMaxRelativeDamage(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	bool vehicleTileOcean = is_ocean(vehicleTile);
	int vehicleMaxPower = getVehicleMaxPower(vehicleId);

	int maxBombardmentDamagePercentage = 100;

	if (isBaseAt(vehicleTile) || isBunkerAt(vehicleTile))
	{
		maxBombardmentDamagePercentage = Rules->max_dmg_percent_arty_base_bunker;
	}
	else
	{
		switch (vehicle->triad())
		{
		case TRIAD_AIR:
			maxBombardmentDamagePercentage = (isAirbaseAt(vehicleTile) ? Rules->max_dmg_percent_arty_sea : 0);
			break;
		case TRIAD_SEA:
			maxBombardmentDamagePercentage = Rules->max_dmg_percent_arty_sea;
			break;
		case TRIAD_LAND:
			maxBombardmentDamagePercentage = (vehicleTileOcean ? Rules->max_dmg_percent_arty_sea : Rules->max_dmg_percent_arty_open);
			break;
		}
	}

	int maxDamage = vehicleMaxPower * maxBombardmentDamagePercentage / 100;

	return (double)maxDamage / (double)vehicleMaxPower;

}

double getVehicleBombardmentRemainingRelativeDamage(int vehicleId)
{
	double maxRelativeBombardmentDamage = getVehicleBombardmentMaxRelativeDamage(vehicleId);
	double relativeDamage = getVehicleRelativeDamage(vehicleId);
	
	return std::max(0.0, maxRelativeBombardmentDamage - relativeDamage);
	
}

int getBasePopulationLimit(int baseId)
{
	BASE *base = getBase(baseId);

	int populationLimit;

	if (!isBaseHasFacility(baseId, FAC_HAB_COMPLEX))
	{
		populationLimit = getFacilityPopulationLimit(base->faction_id, FAC_HAB_COMPLEX);
	}
	else if (!isBaseHasFacility(baseId, FAC_HABITATION_DOME))
	{
		populationLimit = getFacilityPopulationLimit(base->faction_id, FAC_HABITATION_DOME);
	}
	else
	{
		populationLimit = INT_MAX;
	}

	return populationLimit;

}

/*
Determines number of side moves from tile2 to base range with center at tile1.
Returns 0 if already within base range.
*/
int getBaseRadiusLayer(MAP *baseTile, MAP *tile)
{
	Location delta = getDelta(baseTile, tile);
	Location rectangularDelta = getRectangularCoordinates(delta);
	int maxAbsRectangularDelta = std::max(abs(rectangularDelta.x), abs(rectangularDelta.y));
	int minAbsRectangularDelta = std::min(abs(rectangularDelta.x), abs(rectangularDelta.y));
	return std::max(0, maxAbsRectangularDelta - 2) + std::max(0, minAbsRectangularDelta - 1);
}

bool isWithinBaseRadius(MAP *baseTile, MAP *tile)
{
	return getBaseRadiusLayer(baseTile, tile) == 0;
}

/*
Returns one or two tiles closer to base range than given one.
*/
std::vector<MAP *> getFartherBaseRadiusTiles(MAP *baseTile, MAP *tile)
{
	std::vector<MAP *> fartherBaseRadiusTiles;

	int tileBaseRadiusLayer = getBaseRadiusLayer(baseTile, tile);

	if (tileBaseRadiusLayer == 0)
		return fartherBaseRadiusTiles;

	for (MAP *sideTile : getSideTiles(tile))
	{
		int sideTileBaseRadiusLayer = getBaseRadiusLayer(baseTile, sideTile);

		if (sideTileBaseRadiusLayer > tileBaseRadiusLayer)
		{
			fartherBaseRadiusTiles.push_back(sideTile);
		}

	}

	return fartherBaseRadiusTiles;

}

bool isFactionHasProject(int factionId, int facilityId)
{
	return has_project((FacilityId)facilityId, factionId);
}

Budget getBaseBudgetIntake(int baseId)
{
	BASE *base = getBase(baseId);
	Faction *faction = getFaction(base->faction_id);
	
	int allocationEconomy = 10 - faction->SE_alloc_psych - faction->SE_alloc_labs;
	int allocationPsych = faction->SE_alloc_psych;
	
	int economyIntake = (base->energy_surplus * allocationEconomy + 4) / 10;
	int psychIntake = (base->energy_surplus * allocationPsych + 4) / 10;
	int labsIntake = base->energy_surplus - economyIntake - psychIntake;
	
	return {economyIntake, psychIntake, labsIntake};
	
}

Budget getBaseBudgetIntake2(int baseId)
{
	BASE *base = getBase(baseId);
	return {base->economy_total, base->psych_total, base->labs_total};
}

/**
Calculates how much population growths a turn.
*/
double getBasePopulationGrowth(int baseId)
{
	BASE *base = getBase(baseId);
	int requiredNutrients = (base->pop_size + 1) * mod_cost_factor(base->faction_id, RSC_NUTRIENT, baseId);
	return 1.0 / ((double)requiredNutrients / (double)std::max(1, base->nutrient_surplus) + 1);
}

/**
Calculates how much more population growths a turn with given nutrient increase.
*/
double getBasePopulationGrowthIncrease(int baseId, double nutrientSurplusIncrease)
{
	BASE *base = getBase(baseId);
	int requiredNutrients = (base->pop_size + 1) * mod_cost_factor(base->faction_id, RSC_NUTRIENT, baseId);
	return 1.0 / ((double)requiredNutrients / nutrientSurplusIncrease + 1);
}

/*
Calculates how much population growths a turn. RELATIVE to current base size.
*/
double getBasePopulationGrowthRate(int baseId)
{
	BASE *base = getBase(baseId);
	return getBasePopulationGrowth(baseId) / (double)base->pop_size;
}

int getBaseTurnsToPopulation(int baseId, int population)
{
	BASE *base = getBase(baseId);
	
	if (base->pop_size >= population)
		return 0.0;
	
	int nutrientCostFactor = mod_cost_factor(base->faction_id, RSC_NUTRIENT, baseId);
	int requiredNutrients = nutrientCostFactor * (population - base->pop_size) * (population + base->pop_size + 1) / 2;
	int remainingNutrients = requiredNutrients - base->nutrients_accumulated;
	return remainingNutrients / std::max(1, base->nutrient_surplus);
	
}

int getMovementRateAlongTube()
{
	return (conf.magtube_movement_rate == 0 ? 0 : 1);
}

int getMovementRateAlongRoad()
{
	return (conf.magtube_movement_rate == 0 ? 1 : conf.magtube_movement_rate);
}

bool isInfantryDefensiveUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);
	int offenseValue = getUnitOffenseValue(unitId);
	int defenseValue = getUnitDefenseValue(unitId);

	// battle ogre is considred defensive unit
	
	if (isBattleOgreUnit(unitId))
		return true;
	
	// combat

	if (!isCombatUnit(unitId))
		return false;

	// infantry

	if (unit->chassis_id != CHS_INFANTRY)
		return false;

	// psi defense makes it defensive unit

	if (defenseValue < 0)
		return true;

	// psi offense and conventional defense makes it NOT defensive unit

	if (offenseValue < 0)
		return false;
	
	// conventional offense should not be more than conventional offense

	return offenseValue <= defenseValue;

}
bool isInfantryDefensiveVehicle(int vehicleId)
{
	return isInfantryDefensiveUnit(Vehicles[vehicleId].unit_id);
}

bool isInfantryDefensivePolice2xUnit(int unitId, int factionId)
{
	return isInfantryPolice2xUnit(unitId, factionId) && isInfantryDefensiveUnit(unitId);
}

bool isInfantryDefensivePolice2xVehicle(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	return isInfantryDefensivePolice2xUnit(vehicle->unit_id, vehicle->faction_id);
}

bool isBaseDefenderVehicle(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return isInfantryDefensiveVehicle(vehicleId) && isBaseAt(vehicleTile) && vehicle->order == ORDER_HOLD;
}

bool isUnitRequiresSupport(int unitId)
{
	const int support_type = (conf.modify_unit_support == 1 ? PLAN_SUPPLY : (conf.modify_unit_support == 2 ? PLAN_PROBE : PLAN_TERRAFORM));
	return Units[unitId].plan <= support_type && !isUnitHasAbility(unitId, ABL_CLEAN_REACTOR);
}

bool isVehicleRequiresSupport(int vehicleId)
{
	return isUnitRequiresSupport(Vehicles[vehicleId].unit_id);
}

bool isInterceptorUnit(int unitId)
{
	return Chassis[Units[unitId].chassis_id].triad == TRIAD_AIR && isUnitHasAbility(unitId, ABL_AIR_SUPERIORITY);
}
bool isInterceptorVehicle(int vehicleId)
{
	return isInterceptorUnit(Vehicles[vehicleId].unit_id);
}

int getUnitSupport(int unitId)
{
	return isUnitRequiresSupport(unitId) ? 1 : 0;
}

bool getVehicleSupport(int vehicleId)
{
	return isUnitRequiresSupport(Vehicles[vehicleId].unit_id);
}

bool isFactionHasAbility(int faction, VehAbl abl)
{
	assert(abl >= 0 && abl <= ABL_ID_ALGO_ENHANCEMENT);
	return isFactionHasTech(faction, Ability[abl].preq_tech);
}

bool isFactionHasTech(int factionId, int tech)
{
	return has_tech(tech, factionId);
}

int getFactionTechCount(int factionId)
{
	int count = 0;
	
	for (int techId = TECH_Biogen; techId <= TECH_TranT; techId++)
	{
		if (isFactionHasTech(factionId, techId))
		{
			count++;
		}
	}
	
    return count;
    
}

bool isUnprotectedArtifact(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	
	// artifact
	
	if (vehicle->unit_id != BSC_ALIEN_ARTIFACT)
		return false;
	
	// not at base
	
	if (map_has_item(getVehicleMapTile(vehicleId), BIT_BASE_IN_TILE))
		return false;
	
	for (int stackVehicleId : getStackVehicleIds(vehicleId))
	{
		VEH *stackVehicle = getVehicle(stackVehicleId);
		
		// ignore artifact
		
		if (stackVehicle->unit_id == BSC_ALIEN_ARTIFACT)
			continue;
		
		// stack is protected
		
		return false;
		
	}
	
	return true;
	
}

/**
Computes average of double values in given vector.
Returns 0.0 for empty vector.
*/
double average(std::vector<double> v)
{
	return v.empty() ? 0.0 : std::accumulate(v.begin(), v.end(), 0.0) / (double)v.size();
}

bool isBattleOgreUnit(int unitId)
{
	return unitId == BSC_BATTLE_OGRE_MK1 || unitId == BSC_BATTLE_OGRE_MK2 || unitId == BSC_BATTLE_OGRE_MK3;
}

bool isBattleOgreVehicle(int vehicleId)
{
	return isBattleOgreUnit(Vehicles[vehicleId].unit_id);
}

/**
Computes unit move rate along the road/tube.
+ Maritime Control Center
*/
int getUnitMoveRate(int factionId, int unitId)
{
	UNIT *unit = getUnit(unitId);
    int triad = unit->triad();
    
	// fungal tower does not move
	
	if (unitId == BSC_FUNGAL_TOWER)
	{
		return 0;
	}
	
	int unitMoveRate = speed_proto(unitId);

    if (triad == TRIAD_SEA && isFactionHasProject(factionId, FAC_MARITIME_CONTROL_CENTER))
	{
		unitMoveRate += 2 * Rules->move_rate_roads;
	}
	
    return unitMoveRate;
    
}

/**
Computes unit speed (not move rate along the road/tube !!!).
+ Maritime Control Center
*/
int getUnitSpeed(int factionId, int unitId)
{
	return getUnitMoveRate(factionId, unitId) / Rules->move_rate_roads;
}

/**
Computes vehicle unit move rate along the road/tube.
+ elite movement bonus
+ damage
*/
int getVehicleMoveRate(int vehicleId)
{
	return veh_speed(vehicleId, false);
}

/**
Computes vehicle unit speed (not move rate along the road/tube !!!)
+ elite movement bonus
+ damage
*/
int getVehicleSpeed(int vehicleId)
{
	return getVehicleMoveRate(vehicleId) / Rules->move_rate_roads;
}

bool isTileAccessesWater(MAP *tile)
{
	// ocean
	
	if (is_ocean(tile))
		return true;
	
	for (MAP *adjacentTile : getAdjacentTiles(tile))
	{
		if (is_ocean(adjacentTile))
			return true;
	}
	
	return false;
	
}

bool isBaseAccessesWater(int baseId)
{
	return isTileAccessesWater(getBaseMapTile(baseId));
}

bool isBaseCanBuildShip(int baseId)
{
	BASE *base = getBase(baseId);
	int factionId = base->faction_id;
	
	// no tech
	
	if (!isFactionHasTech(factionId, getChassis(CHS_FOIL)->preq_tech))
		return false;
	
	// can build if accesses water
	
	return isBaseAccessesWater(baseId);
	
}

bool isUnitZocRestricted(int unitId)
{
	UNIT *unit = getUnit(unitId);
	
	bool zocRestricted = false;
	
	switch (unit->triad())
	{
	case TRIAD_LAND:
		
		// land unit is restricted by default
		
		zocRestricted = true;
		
		// artifact and probe are not restricted
		
		switch (unit->weapon_id)
		{
		case WPN_ALIEN_ARTIFACT:
		case WPN_PROBE_TEAM:
			
			zocRestricted = false;
			
			break;
			
		}
		
		// cloaked is not restricted
		
		if (isUnitHasAbility(unitId, ABL_CLOAKED))
		{
			zocRestricted = false;
		}
		
		break;
		
	default:
		
		// non land unit is not restricted
		
		zocRestricted = false;
		
	}
	
	return zocRestricted;
	
}

bool isVehicleZocRestricted(int vehicleId)
{
	return isUnitZocRestricted(Vehicles[vehicleId].unit_id);
}

/**
Gets all base regular facilities.
*/
std::vector<int> getBaseFacilities(int baseId)
{
	std::vector<int> facilities;
	
	for (int facilityId = Fac_ID_First; facilityId <= Fac_ID_Last; facilityId++)
	{
		if (isBaseHasFacility(baseId, facilityId))
		{
			facilities.push_back(facilityId);
		}
		
	}
	
	return facilities;
	
}

/**
Gets all base projects.
*/
std::vector<int> getBaseProjects(int baseId)
{
	std::vector<int> projects;
	
	for (int facilityId = SP_ID_First; facilityId <= SP_ID_Last; facilityId++)
	{
		if (isBaseHasFacility(baseId, facilityId))
		{
			projects.push_back(facilityId);
		}
		
	}
	
	return projects;
	
}

Resource getBaseWorkerResourceIntake(int baseId, MAP *tile)
{
	assert(isOnMap(tile));
	
	BASE *base = getBase(baseId);
	int factionId = base->faction_id;
	int x = getX(tile);
	int y = getY(tile);
	
	return
		{
			(double)mod_crop_yield(factionId, baseId, x, y, 0) - (double)Rules->nutrient_intake_req_citizen,
			(double)mod_mine_yield(factionId, baseId, x, y, 0),
			(double)mod_energy_yield(factionId, baseId, x, y, 0),
		}
	;
	
}

Resource getBaseResourceIntake2(int baseId)
{
	BASE *base = getBase(baseId);
	
	return {(double)base->nutrient_surplus, (double)base->mineral_intake_2, (double)(base->economy_total + base->labs_total)};
	
}

bool isLandRegion(MAP *tile, bool includePolar)
{
	return /*tile->region >= 0 && */tile->region <= (includePolar ? 63 : 62);
}

bool isSeaRegion(MAP *tile, bool includePolar)
{
	return tile->region >= 64 && tile->region <= (includePolar ? 127 : 126);
}

bool isPolarRegion(MAP *tile)
{
	return tile->region == 63 || tile->region == 127;
}

int getBaseNextUnitSupport(int baseId, int unitId)
{
	BASE *base = &(Bases[baseId]);
	Faction *faction = getFaction(base->faction_id);
	
	// check if unit requires no support
	
	if (!isUnitRequiresSupport(unitId))
		return 0;
	
	// base supported vehicles
	
	int supportRequiredVehicleCount = 0;
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		if (Vehicles[vehicleId].home_base_id == baseId && isVehicleRequiresSupport(vehicleId))
		{
			supportRequiredVehicleCount++;
		}
		
	}
	
	// support
	
	int nextUnitSupport;
	
	if (conf.alternative_support)
	{
		switch (faction->SE_support_pending)
		{
		case -4:
			nextUnitSupport = 1;
			break;
		case -3:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 1 ? 0 : 1;
			break;
		case -2:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 2 ? 0 : 1;
			break;
		case -1:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 3 ? 0 : 1;
			break;
		case +0:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 4 ? 0 : 1;
			break;
		case +1:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 5 ? 0 : 1;
			break;
		case +2:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 6 ? 0 : 1;
			break;
		case +3:
			nextUnitSupport = ((supportRequiredVehicleCount + 1) <= std::max(8, (int)base->pop_size)) ? 0 : 1;
			break;
		default:
			nextUnitSupport = 0;
		}
		
	}
	else
	{
		switch (faction->SE_support_pending)
		{
		case -4:
			nextUnitSupport = 2;
			break;
		case -3:
			nextUnitSupport = 1;
			break;
		case -2:
		case -1:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 1 ? 0 : 1;
			break;
		case +0:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 2 ? 0 : 1;
			break;
		case +1:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 3 ? 0 : 1;
			break;
		case +2:
			nextUnitSupport = (supportRequiredVehicleCount + 1) <= 4 ? 0 : 1;
			break;
		case +3:
			nextUnitSupport = ((supportRequiredVehicleCount + 1) <= std::max(4, (int)base->pop_size)) ? 0 : 1;
			break;
		default:
			nextUnitSupport = 0;
		}
		
	}
	
	return nextUnitSupport;
	
}

int getBaseMoraleModifier(int baseId, int extendedTriad)
{
	BASE *base = getBase(baseId);
	
	int moraleModifier = 0;
	
	if (extendedTriad == 3)
	{
		if (isFactionHasProject(base->faction_id, FAC_XENOEMPATHY_DOME))
		{
			moraleModifier++;
		}
		
		if (isFactionHasProject(base->faction_id, FAC_PHOLUS_MUTAGEN))
		{
			moraleModifier++;
		}
		
		if (isFactionHasProject(base->faction_id, FAC_VOICE_OF_PLANET))
		{
			moraleModifier++;
		}
		
		if (isBaseHasFacility(baseId, FAC_CENTAURI_PRESERVE))
		{
			moraleModifier++;
		}
		
		if (isBaseHasFacility(baseId, FAC_TEMPLE_OF_PLANET))
		{
			moraleModifier++;
		}
		
		if (isBaseHasFacility(baseId, FAC_BIOLOGY_LAB))
		{
			moraleModifier++;
		}
		
		if (isBaseHasFacility(baseId, FAC_BIOENHANCEMENT_CENTER))
		{
			moraleModifier++;
		}
		
	}
	else
	{
		moraleModifier = morale_mod(baseId, base->faction_id, extendedTriad);
	}
	
	return moraleModifier;
	
}

bool isFactionCanBuildAirOffense(int factionId)
{
	return
		isFactionHasTech(factionId, Chassis[CHS_NEEDLEJET].preq_tech)
		||
		isFactionHasTech(factionId, Chassis[CHS_COPTER].preq_tech)
		||
		isFactionHasTech(factionId, Chassis[CHS_MISSILE].preq_tech)
		||
		isFactionHasTech(factionId, Chassis[CHS_GRAVSHIP].preq_tech)
	;
}

bool isFactionCanBuildAirDefense(int factionId)
{
	return
		isFactionHasTech(factionId, Chassis[CHS_NEEDLEJET].preq_tech)
		||
		isFactionHasTech(factionId, Chassis[CHS_COPTER].preq_tech)
		||
		isFactionHasTech(factionId, Chassis[CHS_GRAVSHIP].preq_tech)
	;
}

bool isFactionCanBuildMelee(int factionId, int type, int triad)
{
	if (factionId == 0)
		return false;
	
	bool canBuild = false;
	
	switch (type)
	{
	case 0:
		switch (triad)
		{
		case TRIAD_AIR:
			canBuild = isFactionHasTech(factionId, Units[BSC_LOCUSTS_OF_CHIRON].preq_tech);
			break;
		case TRIAD_SEA:
			canBuild =
				isFactionHasTech(factionId, Units[BSC_ISLE_OF_THE_DEEP].preq_tech)
				||
				isFactionHasTech(factionId, Units[BSC_SEALURK].preq_tech)
			;
			break;
		case TRIAD_LAND:
			canBuild = isFactionHasTech(factionId, Units[BSC_MIND_WORMS].preq_tech);
			break;
		}
		break;
	case 1:
		switch (triad)
		{
		case TRIAD_AIR:
			canBuild =
				isFactionHasTech(factionId, Chassis[CHS_NEEDLEJET].preq_tech)
				||
				isFactionHasTech(factionId, Chassis[CHS_COPTER].preq_tech)
				||
				isFactionHasTech(factionId, Chassis[CHS_MISSILE].preq_tech)
				||
				isFactionHasTech(factionId, Chassis[CHS_GRAVSHIP].preq_tech)
			;
			break;
		case TRIAD_SEA:
			canBuild =
				isFactionHasTech(factionId, Chassis[CHS_FOIL].preq_tech)
				||
				isFactionHasTech(factionId, Chassis[CHS_CRUISER].preq_tech)
			;
			break;
		case TRIAD_LAND:
			canBuild =
				isFactionHasTech(factionId, Chassis[CHS_INFANTRY].preq_tech)
				||
				isFactionHasTech(factionId, Chassis[CHS_SPEEDER].preq_tech)
				||
				isFactionHasTech(factionId, Chassis[CHS_HOVERTANK].preq_tech)
			;
			break;
		}
		break;
	}
	
	return canBuild;
	
}

bool isRoughTerrain(MAP *tile)
{
	return !map_has_item(tile, BIT_BASE_IN_TILE) && !is_ocean(tile) && (map_has_item(tile, BIT_FUNGUS | BIT_FOREST) || map_rockiness(tile) == 2);
}

bool isOpenTerrain(MAP *tile)
{
	return !map_has_item(tile, BIT_BASE_IN_TILE) && !is_ocean(tile) && !(map_has_item(tile, BIT_FUNGUS | BIT_FOREST) || map_rockiness(tile) == 2);
}

/*
Returns non polar sea regions base accesses.
*/
std::set<int> getBaseSeaRegions(int baseId)
{
	MAP *baseTile = getBaseMapTile(baseId);
	
	std::set<int> baseSeaRegions;
	
	if (is_ocean(baseTile))
	{
		if (!baseTile->is_pole_tile())
		{
			baseSeaRegions.insert(baseTile->region);
		}
		
	}
	else
	{
		for (MAP *adjacentTile : getAdjacentTiles(baseTile))
		{
			if (is_ocean(adjacentTile) && !adjacentTile->is_pole_tile())
			{
				baseSeaRegions.insert(adjacentTile->region);
			}
			
		}
		
	}
	
	return baseSeaRegions;
	
}

/**
Returns range to first not faction owned tile.
Uses vanilla TABLE_square_offset.
minRadius = 0-8, inclusive
maxRadius = 0-8, inclusive
*/
int getClosestNotOwnedTileRange(int factionId, MAP *tile, int minRadius, int maxRadius)
{
	assert(tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles);
	assert(minRadius >= 0 && minRadius < TABLE_square_block_radius_count);
	assert(maxRadius >= 0 && maxRadius < TABLE_square_block_radius_count);
	
	int x = getX(tile);
	int y = getY(tile);
	
	int closestNotOwnedTileRange = -1;
	
	for (int radius = minRadius; radius <= maxRadius; radius++)
	{
		int minIndex = (radius == 0 ? 0 : TABLE_square_block_radius[radius - 1]);
		int maxIndex = TABLE_square_block_radius[radius];
		
		for (int index = minIndex; index < maxIndex; index++)
		{
			int offsetX = TABLE_square_offset_x[index];
			int offsetY = TABLE_square_offset_y[index];
			
			int cellX = wrap(x + offsetX);
			int cellY = y + offsetY;
			
			if (!isOnMap(cellX, cellY))
				continue;
			
			MAP *tile = getMapTile(cellX, cellY);
			if (tile->owner != factionId)
			{
				closestNotOwnedTileRange = radius;
				break;
			}
			
		}
		
		if (closestNotOwnedTileRange != -1)
			break;
		
	}
	
	return closestNotOwnedTileRange;
	
}

/**
Returns range to first not faction owned or sea tile.
Uses vanilla TABLE_square_offset.
minRadius = 0-8, inclusive
maxRadius = 0-8, inclusive
*/
int getClosestNotOwnedOrSeaTileRange(int factionId, MAP *tile, int minRadius, int maxRadius)
{
	assert(tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles);
	assert(minRadius >= 0 && minRadius < TABLE_square_block_radius_count);
	assert(maxRadius >= 0 && maxRadius < TABLE_square_block_radius_count);
	
	// land
	
	if (!tile->is_land())
		return -1;
	
	int x = getX(tile);
	int y = getY(tile);
	
	int closestNotOwnedOrSeaTileRange = -1;
	
	for (int radius = minRadius; radius <= maxRadius; radius++)
	{
		int minIndex = (radius == 0 ? 0 : TABLE_square_block_radius[radius - 1]);
		int maxIndex = TABLE_square_block_radius[radius];
		
		for (int index = minIndex; index < maxIndex; index++)
		{
			int offsetX = TABLE_square_offset_x[index];
			int offsetY = TABLE_square_offset_y[index];
			
			int cellX = wrap(x + offsetX);
			int cellY = y + offsetY;
			
			if (!isOnMap(cellX, cellY))
				continue;
			
			MAP *tile = getMapTile(cellX, cellY);
			if (tile->owner != factionId || tile->is_sea())
			{
				closestNotOwnedOrSeaTileRange = radius;
				break;
			}
			
		}
		
		if (closestNotOwnedOrSeaTileRange != -1)
			break;
		
	}
	
	return closestNotOwnedOrSeaTileRange;
	
}

int getUnitOffenseExtendedTriad(int unitId, int enemyUnitId)
{
	UNIT &unit = Units[unitId];
	UNIT &enemyUnit = Units[enemyUnitId];
	return Weapon[unit.weapon_id].offense_value < 0 || Armor[enemyUnit.armor_id].defense_value < 0 ? 3 : Chassis[unit.chassis_id].triad;
}

