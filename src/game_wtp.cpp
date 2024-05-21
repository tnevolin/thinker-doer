#include "game_wtp.h"
#include "terranx_wtp.h"
#include "wtp.h"

// MapValue

MapValue::MapValue(MAP *_tile, int _value)
: tile{_tile}, value{_value}
{
	
}
	
// Location

bool operator==(const Location &o1, const Location &o2)
{
	return o1.x == o2.x && o1.y == o2.y;
}
bool operator!=(const Location &o1, const Location &o2)
{
	return o1.x != o2.x || o1.y != o2.y;
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

	if (*map_toggle_flat == 0 && abs(dx) > *map_half_x)
	{
		if (dx > 0)
		{
			dx -= *map_axis_x;
		}
		else
		{
			dx += *map_axis_x;
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

	if (*map_toggle_flat == 0 && abs(dx) > *map_half_x)
	{
		if (dx > 0)
		{
			dx -= *map_axis_x;
		}
		else
		{
			dx += *map_axis_x;
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
	assert(tile1 != nullptr);
	assert(tile2 != nullptr);

	return getOffsetIndex(getX(tile1), getY(tile1), getX(tile2), getY(tile2));

}

// =======================================================
// MAP conversions
// =======================================================

bool isOnMap(int x, int y)
{
	return (((x ^ y) & 0x1) == 0x0 && (x >= 0 && x < *map_axis_x) && (y >= 0 && y < *map_axis_y));
}

bool isValidTile(MAP *tile)
{
	if (tile == nullptr)
		return false;

	int mapIndex = (tile - (*MapPtr));

	if (!(mapIndex >= 0 && mapIndex < *map_area_tiles))
		return false;

	return true;

}

/*
coordinates -> map index
*/

int getMapIndexByCoordinates(int x, int y)
{
	if (!isOnMap(x, y))
		return -1;

	return x / 2 + (*map_half_x) * y;

}

/*
tile -> map index
*/

int getMapIndexByPointer(MAP *tile)
{
	if (tile == nullptr)
		return -1;

	int mapIndex = (tile - (*MapPtr));

	if (!(mapIndex >= 0 && mapIndex < *map_area_tiles))
		return -1;

	return mapIndex;

}

/*
map index -> coordinates
*/

int getX(int mapIndex)
{
	if (!(mapIndex >= 0 && mapIndex < *map_area_tiles))
		return -1;
	
	return (mapIndex % (*map_half_x)) * 2 + (mapIndex / (*map_half_x) % 2);
	
}

int getY(int mapIndex)
{
	if (!(mapIndex >= 0 && mapIndex < *map_area_tiles))
		return -1;

	return mapIndex / (*map_half_x);

}

/*
map tile -> coordinates
*/

int getX(MAP *tile)
{
	return getX(getMapIndexByPointer(tile));
}

int getY(MAP *tile)
{
	return getY(getMapIndexByPointer(tile));
}

Location getLocation(MAP *tile)
{
	return {getX(tile), getY(tile)};
}

/*
map index -> tile
*/

MAP *getMapTile(int mapIndex)
{
	if (!(mapIndex >= 0 && mapIndex < *map_area_tiles))
		return nullptr;

	return &((*MapPtr)[mapIndex]);

}

/*
coordinates -> tile
*/

MAP *getMapTile(int x, int y)
{
	return getMapTile(getMapIndexByCoordinates(x, y));

}

/*
map tiles -> coordinate difference
*/

Location getDelta(MAP *tile1, MAP *tile2)
{
	int dx = getX(tile2) - getX(tile1);
	int dy = getY(tile2) - getY(tile1);
	
	if (dx < -*map_half_x)
	{
		dx += *map_axis_x;
	}
	else if (dx > +*map_half_x)
	{
		dx -= *map_axis_x;
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
	if (shift) {x += *map_axis_y / 2;}
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

std::vector<MAP *> getAdjacentTiles(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	
	std::vector<MAP *> tiles;
	
	for (int angle = 0; angle < TABLE_next_cell_count; angle++)
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

/*
Returns tiles sharing a side with given tile.
*/
std::vector<MAP *> getSideTiles(MAP *tile)
{
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
Returns square block radius tile by index.
*/
MAP * getSquareBlockRadiusTile(MAP *center, int index)
{
	int x = getX(center);
	int y = getY(center);
	
	int offsetX = TABLE_square_offset_x[index];
	int offsetY = TABLE_square_offset_y[index];
	
	int cellX = wrap(x + offsetX);
	int cellY = y + offsetY;
	
	MAP *tile = (isOnMap(cellX, cellY) ? getMapTile(cellX, cellY) : nullptr);
	
	return tile;
	
}

/**
Returns square block radius tiles from beginIndex (inclusive) to endIndex (exclusive).
*/
std::vector<MAP *> getSquareBlockRadiusTiles(MAP *center, int beginIndex, int endIndex)
{
	int x = getX(center);
	int y = getY(center);
	
	std::vector<MAP *> tiles;
	
	for (int index = beginIndex; index < endIndex; index++)
	{
		int offsetX = TABLE_square_offset_x[index];
		int offsetY = TABLE_square_offset_y[index];
		
		int cellX = wrap(x + offsetX);
		int cellY = y + offsetY;
		
		if (!isOnMap(cellX, cellY))
			continue;
		
		tiles.push_back(getMapTile(cellX, cellY));
		
	}
	
	return tiles;
	
}

/*
Returns tiles within given radius range from given center.
Uses vanilla TABLE_square_offset.
minRadius = 0-8, inclusive
maxRadius = 0-8, inclusive
*/
std::vector<MAP *> getSquareBlockTiles(MAP *center, int minRadius, int maxRadius)
{
	assert(minRadius >= 0 && minRadius < TABLE_square_block_radius_count);
	assert(maxRadius >= 0 && maxRadius < TABLE_square_block_radius_count);
	
	int x = getX(center);
	int y = getY(center);
	
	int minIndex = (minRadius == 0 ? 0 : TABLE_square_block_radius[minRadius - 1]);
	int maxIndex = TABLE_square_block_radius[maxRadius];
	
	std::vector<MAP *> tiles;
	
	for (int index = minIndex; index < maxIndex; index++)
	{
		int offsetX = TABLE_square_offset_x[index];
		int offsetY = TABLE_square_offset_y[index];
		
		int cellX = wrap(x + offsetX);
		int cellY = y + offsetY;
		
		if (!isOnMap(cellX, cellY))
			continue;
		
		tiles.push_back(getMapTile(cellX, cellY));
		
	}
	
	return tiles;
	
}

/*
Returns tile equally ranged from given center.
*/
std::vector<MAP *> getEqualRangeTiles(MAP *tile, int range)
{
	assert(range >= 0);
	
	// use vanilla table for range < TABLE_square_block_radius_count
	
	if (range < TABLE_square_block_radius_count)
	{
		return getSquareBlockTiles(tile, range, range);
	}
	
	// use normal computation
	
	int x = getX(tile);
	int y = getY(tile);
	
	std::vector<MAP *> tiles;
	
	if (range == 0)
	{
		tiles.push_back(tile);
		return tiles;
	}
	
	int dMax = 2 * range;
	
	for (int i = 0; i < dMax; i++)
	{
		MAP *tile0 = getMapTile(wrap(x - dMax + i), y + 0 - i);
		if (tile0 != nullptr)
		{
			tiles.push_back(tile0);
		}
		
		MAP *tile1 = getMapTile(wrap(x + 0 + i), y - dMax + i);
		if (tile1 != nullptr)
		{
			tiles.push_back(tile1);
		}
		
		MAP *tile2 = getMapTile(wrap(x + dMax - i), y + 0 + i);
		if (tile2 != nullptr)
		{
			tiles.push_back(tile2);
		}
		
		MAP *tile3 = getMapTile(wrap(x + 0 - i), y + dMax - i);
		if (tile3 != nullptr)
		{
			tiles.push_back(tile3);
		}
		
	}
	
	return tiles;
	
}

/*
Returns tiles withing given range from given center.
*/
std::vector<MAP *> getRangeTiles(MAP *tile, int range, bool includeCenter)
{
	assert(range >= 0);
	
	// use vanilla table for range < TABLE_square_block_radius_count
	
	if (range < TABLE_square_block_radius_count)
	{
		return getSquareBlockTiles(tile, (includeCenter ? 0 : 1), range);
	}
	
	// use normal computation
	
	std::vector<MAP *> tiles;
	
	if (includeCenter)
	{
		tiles.push_back(tile);
	}
	
	for (int ringRange = 1; ringRange <= range; ringRange++)
	{
		std::vector<MAP *> equalRangeTiles = getEqualRangeTiles(tile, ringRange);
		tiles.insert(tiles.end(), equalRangeTiles.begin(), equalRangeTiles.end());
	}
	
	return tiles;
	
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
	return getBaseOffsetTiles(getX(tile), getY(tile), offsetBegin, offsetEnd);
}

std::vector<MAP *> getBaseAdjacentTiles(int x, int y, bool includeCenter)
{
	return getBaseOffsetTiles(x, y, (includeCenter ? 0 : 1), OFFSET_COUNT_ADJACENT);
}

std::vector<MAP *> getBaseAdjacentTiles(MAP *tile, bool includeCenter)
{
	return getBaseAdjacentTiles(getX(tile), getY(tile), includeCenter);
}

std::vector<MAP *> getBaseRadiusTiles(int x, int y, bool includeCenter)
{
	return getBaseOffsetTiles(x, y, (includeCenter ? 0 : 1), OFFSET_COUNT_RADIUS);
}

std::vector<MAP *> getBaseRadiusTiles(MAP *tile, bool includeCenter)
{
	return getBaseRadiusTiles(getX(tile), getY(tile), includeCenter);
}

std::vector<MAP *> getBaseExternalRadiusTiles(MAP *center)
{
	return getSquareBlockRadiusTiles(center, TABLE_square_block_radius_base_internal, TABLE_square_block_radius_base_external);
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

std::vector<MAP *> getMeleeAttackPositions(MAP *target)
{
	return getRangeTiles(target, 1, false);
}
std::vector<MAP *> getArtilleryAttackPositions(MAP *target)
{
	return getRangeTiles(target, Rules->artillery_max_rng, false);
}

// =======================================================
// iterating surrounding locations - end
// =======================================================

bool has_armor(int factionId, int armorId)
{
	return has_tech(factionId, Armor[armorId].preq_tech);
}

bool has_reactor(int factionId, int reactor)
{
	return has_tech(factionId, Reactor[reactor - 1].preq_tech);
}

/*
Calculates mineral cost for given item at base.
Base cost modifying facilites (Skunkworks and Brood Pits) are taken into account.
*/
int getBaseMineralCost(int baseId, int item)
{
	assert(baseId >= 0 && baseId < *total_num_bases);

	BASE *base = &(Bases[baseId]);

	int mineralCost = (item >= 0 ? tx_veh_cost(item, baseId, 0) : Facility[-item].cost) * cost_factor(base->faction_id, 1, -1);

	return mineralCost;

}
/*
Calculates cost for given item at base.
Base cost modifying facilites (Skunkworks and Brood Pits) are taken into account.
*/
int getBaseItemCost(int baseId, int item)
{
	BASE *base = getBase(baseId);
	return getBaseMineralCost(baseId, item) / cost_factor(base->faction_id, 1, -1);
}

int veh_triad(int id) {
    return unit_triad(Vehicles[id].unit_id);
}

int veh_speed_without_roads(int id) {
    return mod_veh_speed(id) / Rules->mov_rate_along_roads;
}

int unit_chassis_speed(int id)
{
	return Chassis[Units[id].chassis_type].speed;
}

int veh_chassis_speed(int id)
{
	VEH *vehicle = &Vehicles[id];
	UNIT *unit = &Units[vehicle->unit_id];
	CChassis *chassis = &Chassis[unit->chassis_type];
	return chassis->speed;
}

int unit_triad(int id) {
    int triad = Chassis[Units[id].chassis_type].triad;
    assert(triad == TRIAD_LAND || triad == TRIAD_SEA || triad == TRIAD_AIR);
    return triad;
}

double random_double(double scale) {
    return scale * ((double)rand() / (double)(RAND_MAX + 1));
}

/*
Send unit to destination building a road/tube on a way if applicable.
*/
int set_move_road_tube_to(int id, int x, int y)
{
    VEH *veh = &Vehicles[id];
    int triad = veh_triad(id);

    // set way points

    veh->waypoint_1_x = x;
    veh->waypoint_1_y = y;

    // set connection type

    int moveStatus;

    switch (triad)
    {
	// land unit
	case TRIAD_LAND:
		// build road or tube to destination depending on existing technology
		moveStatus = (has_terra(veh->faction_id, TERRA_MAGTUBE, 0) ? ORDER_MAGTUBE_TO : ORDER_ROAD_TO);
		break;
	// sea or air unit
	default:
		// go to destination
		moveStatus = ORDER_MOVE_TO;
		break;
    }

    // set vehicle status and icon

    veh->move_status = moveStatus;
    veh->status_icon = veh_status_icon[moveStatus];
    veh->waypoint_1_x = x;
    veh->waypoint_1_y = y;

    return SYNC;

}

bool is_sea_base(int id) {
    MAP* sq = mapsq(Bases[id].x, Bases[id].y);
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

bool vehicle_has_ability(VEH *vehicle, int ability) {
    return Units[vehicle->unit_id].ability_flags & ability;
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
	return (tile && (tile->items & TERRA_BASE_IN_TILE));
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

	// high bit in art_ref_id should not be set

	if ((tile->art_ref_id & 0x80) != 0)
		return false;

	// special conditions

	switch (landmark)
	{
	case LM_CRATER:
		return (tile->art_ref_id < 9);
		break;

	case LM_VOLCANO:
		return (tile->art_ref_id < 9);
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
		return (tile->art_ref_id < 6);
		break;

	}

	return true;

}

int getNutrientBonus(MAP *tile)
{
	if (tile == nullptr)
		return 0;

	int x = getX(tile);
	int y = getY(tile);

	int bonus = 0;

	// resource

	if (bonus_at(x, y) == 1)
	{
		bonus += *TERRA_BONUS_SQ_NUTRIENT;
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
	if (tile == nullptr)
		return 0;

	int x = getX(tile);
	int y = getY(tile);

	int bonus = 0;

	// resource

	if (bonus_at(x, y) == 2)
	{
		bonus += *TERRA_BONUS_SQ_MINERALS;
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
	if (tile == nullptr)
		return 0;

	int x = getX(tile);
	int y = getY(tile);

	int bonus = 0;

	// resource

	if (bonus_at(x, y) == 3)
	{
		bonus += *TERRA_BONUS_SQ_NUTRIENT;
	}

	// ladmarks

	if
	(
		map_has_landmark(tile, LM_JUNGLE)
		||
		map_has_landmark(tile, LM_GEOTHERMAL)
	)
	{
		bonus += 1;
	}

	return bonus;

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
adds/removes base regular facility (not project)
*/
void setBaseFacility(int base_id, int facility_id, bool add)
{
    assert(base_id >= 0 && base_id < *total_num_bases);
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
	return has_tech(faction_id, Facility[facility_id].preq_tech);
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

int getDoctorQuelledDrones(int id)
{
	// collect total psych generated by doctors

	int doctorGeneratedPsych = getBaseDoctors(id) * 2;

	// calculate psych multiplier

	double psychMultipler = 1.0;

	if(has_facility(id, FAC_HOLOGRAM_THEATRE))
	{
		psychMultipler += 0.5;
	}
	if(has_facility(id, FAC_TREE_FARM))
	{
		psychMultipler += 0.5;
	}
	if(has_facility(id, FAC_HYBRID_FOREST))
	{
		psychMultipler += 0.5;
	}
	if(has_facility(id, FAC_RESEARCH_HOSPITAL))
	{
		psychMultipler += 0.25;
	}
	if(has_facility(id, FAC_NANOHOSPITAL))
	{
		psychMultipler += 0.25;
	}

	// calculate total psych output from doctors

	double doctorPsychOutput = psychMultipler * (double)doctorGeneratedPsych;

	// calculate quelled drones from doctors

	int doctorQuelledDrones = (int)floor(doctorPsychOutput / 2.0);

	return doctorQuelledDrones;

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

		if (!has_tech(factionId, citizen->preq_tech))
			continue;

		// not obsolete

		if (has_tech(factionId, citizen->obsol_tech))
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
	return (item > -PROJECT_ID_FIRST && item < 0);
}

bool isBaseBuildingProject(int baseId)
{
	int item = getBaseBuildingItem(baseId);
	return (item >= -PROJECT_ID_LAST && item <= -PROJECT_ID_FIRST);
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
	return (item >= 0 ? tx_veh_cost(item, baseId, 0) : Facility[-item].cost) * cost_factor(base->faction_id, 1, -1);
}

double getUnitPsiOffenseStrength(int unitId)
{
	UNIT *unit = &(Units[unitId]);
	int triad = unit->triad();

	double psiOffenseStrength;

	switch (triad)
	{
	case TRIAD_LAND:
		psiOffenseStrength = (double)Rules->psi_combat_land_numerator / (double)Rules->psi_combat_land_denominator;
		break;
	case TRIAD_SEA:
		psiOffenseStrength = (double)Rules->psi_combat_sea_numerator / (double)Rules->psi_combat_sea_denominator;
		break;
	case TRIAD_AIR:
		psiOffenseStrength = (double)Rules->psi_combat_air_numerator / (double)Rules->psi_combat_air_denominator;
		break;
	default:
		return 1.0;
	}

	// return strength

	return psiOffenseStrength;

}

double getUnitPsiDefenseStrength(int unitId)
{
	UNIT *unit = &(Units[unitId]);
	int triad = unit->triad();

	// psi defense strength

	double psiDefenseStrength;

	switch (triad)
	{
	case TRIAD_LAND:
		psiDefenseStrength = 1.0;
		break;
	case TRIAD_SEA:
		psiDefenseStrength = 1.0;
		break;
	case TRIAD_AIR:
		psiDefenseStrength = 1.0;
		break;
	default:
		return 1.0;
	}

	// return strength

	return psiDefenseStrength;

}

double getUnitConventionalOffenseStrength(int unitId)
{
	UNIT *unit = &(Units[unitId]);

	return (double)Weapon[unit->weapon_type].offense_value;

}

double getUnitConventionalDefenseStrength(int unitId)
{
	UNIT *unit = &(Units[unitId]);

	return (double)Armor[unit->armor_type].defense_value;

}

/*
Calculates psi offense strength for new vehicle built at base.
*/
double getNewUnitPsiOffenseStrength(int unitId, int baseId)
{
	BASE *base = &(Bases[baseId]);

	double psiOffenseStrength = getUnitPsiOffenseStrength(unitId);

	// morale modifier

	psiOffenseStrength *= getMoraleMultiplier(getNewVehicleMorale(unitId, baseId));

	// faction PLANET rating modifier

	psiOffenseStrength *= getFactionSEPlanetOffenseModifier(base->faction_id);

	// abilities modifier

	if (unit_has_ability(unitId, ABL_EMPATH))
	{
		psiOffenseStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_empath_song_vs_psi);
	}

	// return strength

	return psiOffenseStrength;

}

double getNewUnitPsiDefenseStrength(int unitId, int baseId)
{
	BASE *base = &(Bases[baseId]);

	// psi defense strength

	double psiDefenseStrength = getUnitPsiDefenseStrength(unitId);

	// morale modifier

	psiDefenseStrength *= getMoraleMultiplier(getNewVehicleMorale(unitId, baseId));

	// faction PLANET rating modifier

	if (conf.planet_combat_bonus_on_defense)
	{
		psiDefenseStrength *= getFactionSEPlanetDefenseModifier(base->faction_id);
	}

	// abilities modifier

	if (unit_has_ability(unitId, ABL_TRANCE))
	{
		psiDefenseStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi);
	}

	// return strength

	return psiDefenseStrength;

}

double getNewUnitConventionalOffenseStrength(int unitId, int baseId)
{
	// strength

	double conventionalOffenseStrength = getUnitConventionalOffenseStrength(unitId);

	// morale modifier

	conventionalOffenseStrength *= getMoraleMultiplier(getNewVehicleMorale(unitId, baseId));

	// return strength

	return conventionalOffenseStrength;

}

double getNewUnitConventionalDefenseStrength(int unitId, int baseId)
{
	// conventional defense strength

	double conventionalDefenseStrength = getUnitConventionalDefenseStrength(unitId);

	// morale modifier

	conventionalDefenseStrength *= getMoraleMultiplier(getNewVehicleMorale(unitId, baseId));

	// return strength

	return conventionalDefenseStrength;

}

double getVehiclePsiOffenseStrength(int id)
{
	VEH *vehicle = &(Vehicles[id]);
	Faction *faction = &(Factions[vehicle->faction_id]);
	
	// psi offense strength
	
	double psiOffenseStrength = getUnitPsiOffenseStrength(vehicle->unit_id);
	
	// morale modifier
	
	psiOffenseStrength *= getVehicleMoraleMultiplier(id);
	
	// faction PLANET rating modifier
	
	psiOffenseStrength *= getPercentageBonusMultiplier(Rules->combat_psi_bonus_per_PLANET * faction->SE_planet_pending);
	
	// abilities modifier
	
	if (isVehicleHasAbility(id, ABL_EMPATH))
	{
		psiOffenseStrength *= 1.0 + (double)Rules->combat_bonus_empath_song_vs_psi / 100.0;
	}
	
	// return strength
	
	return psiOffenseStrength;
	
}

double getVehiclePsiDefenseStrength(int id)
{
	VEH *vehicle = &(Vehicles[id]);

	// psi defense strength

	double psiDefenseStrength = getUnitPsiDefenseStrength(vehicle->unit_id);

	// morale modifier

	psiDefenseStrength *= getVehicleMoraleMultiplier(id);

	// return strength

	return psiDefenseStrength;

}

double getVehicleConventionalOffenseStrength(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// unit with psi weapon does not initiate conventional offense

	if (Weapon[vehicle->weapon_type()].offense_value < 0)
		return 0.0;

	// strength

	double conventionalOffenseStrength = (double)vehicle->offense_value();

	// morale modifier

	conventionalOffenseStrength *= getVehicleMoraleMultiplier(vehicleId);

	// return strength

	return conventionalOffenseStrength;

}

double getVehicleConventionalDefenseStrength(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// unit with psi armor does not initiate conventional defense

	if (Armor[vehicle->armor_type()].defense_value < 0)
		return 0.0;

	// conventional defense strength

	double conventionalDefenseStrength = (double)vehicle->defense_value();

	// morale modifier

	conventionalDefenseStrength *= getVehicleMoraleMultiplier(vehicleId);

	// return strength

	return conventionalDefenseStrength;

}

/*
Computes total damage vehicle can deliver in psi offense.
*/
double getVehiclePsiOffensePower(int vehicleId)
{
	return getVehiclePsiOffenseStrength(vehicleId) * getVehicleRelativePower(vehicleId);
}

/*
Computes total damage vehicle can deliver in psi defense.
*/
double getVehiclePsiDefensePower(int vehicleId)
{
	return getVehiclePsiDefenseStrength(vehicleId) * getVehicleRelativePower(vehicleId);
}

/*
Computes total damage vehicle can deliver in conventional offense.
*/
double getVehicleConventionalOffensePower(int vehicleId)
{
	return getVehicleConventionalOffenseStrength(vehicleId) * getVehicleRelativePower(vehicleId);
}

/*
Computes total damage vehicle can deliver in conventional defense.
*/
double getVehicleConventionalDefensePower(int vehicleId)
{
	return getVehicleConventionalDefenseStrength(vehicleId) * getVehicleRelativePower(vehicleId);
}

double getFactionSEPlanetOffenseModifier(int factionId)
{
	Faction *faction = &(Factions[factionId]);

	return getPercentageBonusMultiplier(Rules->combat_psi_bonus_per_PLANET * faction->SE_planet_pending);

}

double getFactionSEPlanetDefenseModifier(int factionId)
{
	Faction *faction = &(Factions[factionId]);

	if (conf.planet_combat_bonus_on_defense)
	{
		return getPercentageBonusMultiplier(Rules->combat_psi_bonus_per_PLANET * faction->SE_planet_pending);
	}
	else
	{
		return 1.0;
	}

}

double getPsiCombatBaseOdds(int triad)
{
	double psiCombatBaseOdds;

	switch (triad)
	{
	case TRIAD_LAND:
		psiCombatBaseOdds = (double)Rules->psi_combat_land_numerator / (double)Rules->psi_combat_land_denominator;
		break;
	case TRIAD_SEA:
		psiCombatBaseOdds = (double)Rules->psi_combat_sea_numerator / (double)Rules->psi_combat_sea_denominator;
		break;
	case TRIAD_AIR:
		psiCombatBaseOdds = (double)Rules->psi_combat_air_numerator / (double)Rules->psi_combat_air_denominator;
		break;
	default:
		psiCombatBaseOdds = 1.0;
	}

	return psiCombatBaseOdds;

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
	assert(vehicleId >= 0 && vehicleId < *total_num_vehicles);
	return isColonyVehicle(vehicleId) || isFormerVehicle(vehicleId);
}

bool isOgreVehicle(int vehicleId)
{
	assert(vehicleId >= 0 && vehicleId < *total_num_vehicles);
	VEH *vehicle = getVehicle(vehicleId);
	switch (vehicle->unit_id)
	{
	case BSC_BATTLE_OGRE_MK1:
	case BSC_BATTLE_OGRE_MK2:
	case BSC_BATTLE_OGRE_MK3:
		return true;
	default:
		return false;
	}
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

	if (vehicle_has_ability(vehicle, ABL_EMPATH))
	{
		damage *= (1 + (double)Rules->combat_bonus_empath_song_vs_psi / 100.0);
	}

	// defender trance decreases damage

	if (vehicle_has_ability(enemyVehicle, ABL_TRANCE))
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

	if (vehicle_has_ability(enemyVehicle, ABL_EMPATH))
	{
		damage /= (1 + (double)Rules->combat_bonus_empath_song_vs_psi / 100.0);
	}

	// defender trance increases damage

	if (vehicle_has_ability(vehicle, ABL_TRANCE))
	{
		damage *= (1 + (double)Rules->combat_bonus_trance_vs_psi / 100.0);
	}

	return damage;

}

/*
Calculates average maximal damage unit delivers to average native when attacking.
*/
double calculateNativeDamageAttack(int id)
{
	VEH *vehicle = &(Vehicles[id]);

	// calculate base damage (vehicle attacking)

	double damage =
		1.0
		*
		getPsiCombatBaseOdds(veh_triad(id))
		*
		(
			getVehicleMoraleMultiplier(id)
			*
			getFactionSEPlanetOffenseModifier(vehicle->faction_id)
		)
		/
		(
			1.0 // average native without morale modifier
			*
			1.0 // natives do not have PLANET rating
		)
		*
		(double)(10 - vehicle->damage_taken)
	;

	// attacker empath increases damage

	if (vehicle_has_ability(vehicle, ABL_EMPATH))
	{
		damage *= (1 + (double)Rules->combat_bonus_empath_song_vs_psi / 100.0);
	}

	return damage;

}

/*
Calculates average maximal damage unit delivers to average native when defending.
*/
double calculateNativeDamageDefense(int id)
{
	VEH *vehicle = &(Vehicles[id]);

	// calculate base damage (vehicle defending)

	double damage =
		1.0
		/
		getPsiCombatBaseOdds(veh_triad(id)) // assuming native has same triad as defender
		*
		(
			getVehicleMoraleMultiplier(id)
			*
			getFactionSEPlanetDefenseModifier(vehicle->faction_id)
		)
		/
		(
			1.0 // average native without morale modifier
			*
			1.0 // natives do not have PLANET rating
		)
		*
		(double)(10 - vehicle->damage_taken)
	;

	// defender trance increases damage

	if (vehicle_has_ability(vehicle, ABL_TRANCE))
	{
		damage *= (1 + (double)Rules->combat_bonus_trance_vs_psi / 100.0);
	}

	return damage;

}

void setVehicleOrder(int id, int order)
{
	VEH *vehicle = &(Vehicles[id]);
	
    vehicle->move_status = order;
    vehicle->status_icon = veh_status_icon[order];
    
    // clear flag for hold to be able to heal
    
    if (order == ORDER_HOLD)
	{
		vehicle->state = 0;
	}
	
}

MAP *getBaseMapTile(int baseId)
{
	BASE *base = &(Bases[baseId]);

	return getMapTile(base->x, base->y);

}

MAP *getVehicleMapTile(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return getMapTile(vehicle->x, vehicle->y);

}

/*
Verifies if tile has significant improvement.
*/
bool isImprovedTile(MAP *tile)
{
	return (tile->items & (TERRA_MINE | TERRA_SOLAR | TERRA_FOREST | TERRA_CONDENSER | TERRA_ECH_MIRROR | TERRA_THERMAL_BORE | TERRA_SENSOR | TERRA_MONOLITH)) != 0;
}
bool isImprovedTile(int x, int y)
{
	return isImprovedTile(getMapTile(x, y));
}

bool isSupplyVehicle(VEH *vehicle)
{
	return (Units[vehicle->unit_id].weapon_type == WPN_SUPPLY_TRANSPORT);
}

bool isColonyUnit(int id)
{
	return (id >= 0 && Units[id].weapon_type == WPN_COLONY_MODULE);
}

bool isArtifactUnit(int unitId)
{
	return (Units[unitId].weapon_type == WPN_ALIEN_ARTIFACT);
}

bool isArtifactVehicle(int id)
{
	return (Units[Vehicles[id].unit_id].weapon_type == WPN_ALIEN_ARTIFACT);
}

bool isColonyVehicle(int id)
{
	return (Units[Vehicles[id].unit_id].weapon_type == WPN_COLONY_MODULE);
}

bool isFormerUnit(int unitId)
{
	return (Units[unitId].weapon_type == WPN_TERRAFORMING_UNIT);
}

bool isFormerVehicle(int vehicleId)
{
	return isFormerVehicle(&(Vehicles[vehicleId]));
}
bool isFormerVehicle(VEH *vehicle)
{
	return (Units[vehicle->unit_id].weapon_type == WPN_TERRAFORMING_UNIT);
}

bool isTransportVehicle(VEH *vehicle)
{
	return (Units[vehicle->unit_id].weapon_type == WPN_TROOP_TRANSPORT);
}

bool isTransportUnit(int unitId)
{
	return (Units[unitId].weapon_type == WPN_TROOP_TRANSPORT);
}

bool isTransportVehicle(int vehicleId)
{
	return (Units[Vehicles[vehicleId].unit_id].weapon_type == WPN_TROOP_TRANSPORT);
}

bool isSeaTransportUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);
	return (unit->weapon_type == WPN_TROOP_TRANSPORT && unit->triad() == TRIAD_SEA);
}

bool isSeaTransportVehicle(int vehicleId)
{
	return isSeaTransportUnit(getVehicle(vehicleId)->unit_id);
}

bool isVehicleProbe(VEH *vehicle)
{
	return (Units[vehicle->unit_id].weapon_type == WPN_PROBE_TEAM);
}

bool isVehicleIdle(int vehicleId)
{
	return (Vehicles[vehicleId].move_status == ORDER_NONE);
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

bool isLandArtilleryUnit(int unitId)
{
	return (getUnit(unitId)->triad() == TRIAD_LAND && isArtilleryUnit(unitId));
}

bool isLandArtilleryVehicle(int vehicleId)
{
	return isLandArtilleryUnit(getVehicle(vehicleId)->unit_id);
}

/*
Computes base with optional worked tiles reset.
Base compute may not pick optimal worker placement without worked tile reset.
*/
void computeBase(int baseId, bool resetWorkedTiles)
{
	if (resetWorkedTiles)
	{
		Bases[baseId].worked_tiles = 0;
	}

	set_base(baseId);
	base_compute(1);

}

/*
Computes base and tries to adjust number of doctors to avoid drones.
*/
void computeBaseDoctors(int baseId)
{
	BASE *base = getBase(baseId);

//	computeBase(baseId, true);
//	removeBaseDoctors(baseId);
//	base_doctors();
//
	removeBaseDoctors(baseId);
	do
	{
		int doctors1 = getBaseDoctors(baseId);
		computeBase(baseId, true);
		int doctors2 = getBaseDoctors(baseId);

		// computeBase reset doctors count

		if (doctors2 < doctors1)
			break;

		// exit condition

		if (base->talent_total >= base->drone_total)
			break;

		if (!addBaseDoctor(baseId))
			break;

	}
	while (true);

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

bool isLandRegion(int region)
{
	// convert extended region back to region

	if (region >= MaxRegionNum)
	{
		int mapIndex = region - MaxRegionNum;
		MAP *tile = getMapTile(mapIndex);
		region = tile->region;
	}

	return region < MaxRegionNum/2;

}

bool isOceanRegion(int region)
{
	// convert extended region back to region

	if (region >= MaxRegionNum)
	{
		int mapIndex = region - MaxRegionNum;
		MAP *tile = getMapTile(mapIndex);
		region = tile->region;
	}

	return region >= MaxRegionNum/2;

}

/*
Evaluates unit defense effectiveness based on defense value and cost.
*/
double evaluateUnitConventionalDefenseEffectiveness(int id)
{
	UNIT *unit = &(Units[id]);
	int cost = unit->cost;
	int defenseValue = Armor[unit->armor_type].defense_value;

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
double evaluateUnitConventionalOffenseEffectiveness(int id)
{
	UNIT *unit = &(Units[id]);
	int cost = unit->cost;
	int offenseValue = Weapon[unit->armor_type].offense_value;

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
Calculates base defense multiplier taking different factors into account.
*/
double getBaseDefenseMultiplier(int id, int attackerUnitId, int defenderUnitId)
{
	UNIT *attackerUnit = getUnit(attackerUnitId);
	int attackerUnitTriad = attackerUnit->triad();
	
	double baseDefenseMultiplier;
	
	if (isPsiCombat(attackerUnitId, defenderUnitId))
	{
		baseDefenseMultiplier = getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def);
	}
	else
	{
		bool firstLevelDefense = false;
		switch (attackerUnitTriad)
		{
		case TRIAD_LAND:
			firstLevelDefense = has_facility(id, FAC_PERIMETER_DEFENSE);
			break;
		case TRIAD_SEA:
			firstLevelDefense = has_facility(id, FAC_NAVAL_YARD);
			break;
		case TRIAD_AIR:
			firstLevelDefense = has_facility(id, FAC_AEROSPACE_COMPLEX);
			break;
		}

		bool secondLevelDefense = has_facility(id, FAC_TACHYON_FIELD);

		if (firstLevelDefense || secondLevelDefense)
		{
			int defenseMultiplier = 2;

			if (firstLevelDefense)
			{
				defenseMultiplier = conf.perimeter_defense_multiplier;
			}

			if (secondLevelDefense)
			{
				defenseMultiplier += conf.tachyon_field_bonus;
			}

			baseDefenseMultiplier = (double)defenseMultiplier / 2.0;

		}
		else
		{
			baseDefenseMultiplier = getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def);
		}

	}

	return baseDefenseMultiplier;

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

bool isBaseWorkedTile(int baseId, int x, int y)
{
	if (!(baseId >= 0 && baseId < *total_num_bases && isOnMap(x, y)))
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
	int tileMapIndex = getMapIndexByPointer(tile);
	int x = getX(tileMapIndex);
	int y = getY(tileMapIndex);

	return isBaseWorkedTile(baseId, x, y);

}

/*
Summarize base garrizon defense value for all conventional combat units with defense value >= 2.
*/
int getBaseConventionalDefenseValue(int baseId)
{
	BASE *base = &(Bases[baseId]);

	int baseConventionalDefenseValue = 0;

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// own vehicles only

		if (vehicle->faction_id != base->faction_id)
			continue;

		// combat vehicles only

		if (!isCombatVehicle(vehicleId))
			continue;

		// get defense value

		int defenseValue = Armor[Units[vehicle->unit_id].armor_type].defense_value;

		// defense value >= 2 (conventional unit)

		if (defenseValue < 2)
			continue;

		baseConventionalDefenseValue += defenseValue;

	}

	return baseConventionalDefenseValue;

}

/*
Returns faction available units.
*/
std::vector<int> getFactionUnitIds(int factionId, bool includeObsolete, bool includePrototype)
{
	std::vector<int> factionUnitIds;

	// max slot is MaxProtoFactionNum for aliens and twice as that for normal factions

	int maxSlot = (factionId == 0 ? MaxProtoFactionNum : 2 * MaxProtoFactionNum);

	// iterate faction prototypes space

    for (int slot = 0; slot < maxSlot; slot++)
	{
        int unitId = getUnitIdBySlot(factionId, slot);
        UNIT *unit = &(Units[unitId]);

		// skip not available

		if (!isAvailableUnit(unitId, factionId, includeObsolete))
			continue;

		// do not include not prototyped if not requested

		if (unitId >= MaxProtoFactionNum && !(unit->unit_flags & UNIT_PROTOTYPED) && !includePrototype)
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

    if (isProjectBuilt(factionId, FAC_ASCETIC_VIRTUES))
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

	if (has_facility(baseId, facilityId))
		return false;

	// tech available?

	if (!has_facility_tech(base->faction_id, facilityId))
		return false;

	// special cases

	switch (facilityId)
	{
	case FAC_TACHYON_FIELD:
		if (!has_facility(baseId, FAC_PERIMETER_DEFENSE))
			return false;
		break;
	case FAC_HOLOGRAM_THEATRE:
		if (!has_facility(baseId, FAC_RECREATION_COMMONS))
			return false;
		break;
	case FAC_HYBRID_FOREST:
		if (!has_facility(baseId, FAC_TREE_FARM))
			return false;
		break;
	case FAC_QUANTUM_LAB:
		if (!has_facility(baseId, FAC_FUSION_LAB))
			return false;
		break;
	case FAC_NANOHOSPITAL:
		if (!has_facility(baseId, FAC_RESEARCH_HOSPITAL))
			return false;
		break;
	case FAC_GENEJACK_FACTORY:
		if (!has_facility(baseId, FAC_RECYCLING_TANKS))
			return false;
		break;
	case FAC_ROBOTIC_ASSEMBLY_PLANT:
		if (!has_facility(baseId, FAC_GENEJACK_FACTORY))
			return false;
		break;
	case FAC_QUANTUM_CONVERTER:
		if (!has_facility(baseId, FAC_ROBOTIC_ASSEMBLY_PLANT))
			return false;
		break;
	case FAC_NANOREPLICATOR:
		if (!has_facility(baseId, FAC_QUANTUM_CONVERTER))
			return false;
		break;
	case FAC_HABITATION_DOME:
		if (!has_facility(baseId, FAC_HAB_COMPLEX))
			return false;
		break;
	case FAC_TEMPLE_OF_PLANET:
		if (!has_facility(baseId, FAC_CENTAURI_PRESERVE))
			return false;
		break;
	case FAC_SKY_HYDRO_LAB:
    case FAC_NESSUS_MINING_STATION:
    case FAC_ORBITAL_POWER_TRANS:
    case FAC_ORBITAL_DEFENSE_POD:
		if (!has_facility(baseId, FAC_AEROSPACE_COMPLEX))
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

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
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
Returns this faction vehicles in given region of the same triad as this region.
*/
std::vector<int> getRegionSurfaceVehicles(int factionId, int region, bool includeStationed)
{
	std::vector<int> regionVehicles;

	bool ocean = isOceanRegion(region);
	int triad = (ocean ? TRIAD_SEA : TRIAD_LAND);

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);

		// only this faction

		if (vehicle->faction_id != factionId)
			continue;

		// only in this region

		if (map_has_item(vehicleLocation, TERRA_BASE_IN_TILE))
		{
			// only if garrison included

			if (!includeStationed)
				continue;

			// base should be connected to this region

			if (!isTileConnectedToRegion(vehicle->x, vehicle->y, region))
				continue;

		}
		else
		{
			// vehilce not in base in this region

			if (vehicleLocation->region != region)
				continue;

		}

		// only matching triad

		if (veh_triad(vehicleId) != triad)
			continue;

		regionVehicles.push_back(vehicleId);

	}

	return regionVehicles;

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

	for (int vehicleId : getStackVehicles(topStackVehicleId))
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

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
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

	if (*current_turn < 50)
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

	double nativeProtection = estimateUnitBaseLandNativeProtection(Vehicles[vehicleId].unit_id, vehicle->faction_id, isOceanRegion(baseTile->region));

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

		if (bonusId == FCB_OFFENSE)
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

		if (bonusId == FCB_DEFENSE)
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

	if (MFaction->rule_flags & FACT_FANATIC)
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

	if (*current_turn < 50)
	{
		protectionPotential *= 2;
	}

	// adjustment to native base lifecycle

	protectionPotential /= (1.0 + (double)(*current_turn / 50 - 2) * 0.125);

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

	if (has_facility(baseId, FAC_BROOD_PIT))
	{
		policeRating = std::min(3, policeRating + 2);
	}

	return policeRating;

}

int getBasePoliceEffect(int baseId)
{
	return (getBasePoliceRating(baseId) == 3 ? 2 : 1);
}

/*
Calculates number of allowed police units per SE POLICE rating.
Counts Brood Pit in base.
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

char *getVehicleUnitName(int vehicleId)
{
	return Units[Vehicles[vehicleId].unit_id].name;
}

int getVehicleUnitPlan(int vehicleId)
{
	return (int)Units[vehicleId].unit_plan;
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

bool isBaseHasAccessToWater(int baseId)
{
    BASE* base = &(Bases[baseId]);

    for (MAP *tile : getBaseAdjacentTiles(base->x, base->y, true))
	{
		if (is_ocean(tile))
			return true;

	}

    return false;

}

bool isBaseCanBuildShips(int baseId)
{
    BASE* base = &(Bases[baseId]);

    if (!has_chassis(base->faction_id, CHS_FOIL))
		return false;

	return isBaseHasAccessToWater(baseId);

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

	int fieldDamageHealingThreshold = Units[vehicle->unit_id].reactor_type * 2;

	return
		(vehicle->damage_taken > fieldDamageHealingThreshold)
		||
		(map_has_item(vehilceMapTile, TERRA_BASE_IN_TILE) && (vehicle->damage_taken > 0))
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

		for (int baseId = 0; baseId < *total_num_bases; baseId++)
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

bool isMapTileVisibleToFaction(MAP *tile, int factionId)
{
	return (tile->visibility & (0x1 << factionId) != 0);
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

std::vector<int> getStackVehicles(int vehicleId)
{
	std::vector<int> stackedVehicleIds;

	// get top of the stack vehicle

	int topStackVehicleId = vehicleId;
	while (Vehicles[topStackVehicleId].prev_unit_id_stack != -1)
	{
		topStackVehicleId = Vehicles[topStackVehicleId].prev_unit_id_stack;
	}

	// collect all vehicles in the stack

	for (int stackedVehicleId = topStackVehicleId; stackedVehicleId != -1; stackedVehicleId = Vehicles[stackedVehicleId].next_unit_id_stack)
	{
		stackedVehicleIds.push_back(stackedVehicleId);
	}

	return stackedVehicleIds;

}

std::vector<int> getTileVehicleIds(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);

	int vehicleAt = veh_at(x, y);
	
	if (vehicleAt == -1)
	{
		return std::vector<int>();
	}
	else
	{
		return getStackVehicles(vehicleAt);
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

	set_action(id, action + 4, veh_status_icon[action + 4]);

}

/*
nutrient yield calculation
*/
HOOK_API int mod_nutrient_yield(int factionId, int baseId, int x, int y, int tf)
{
	// call original function

    int value = crop_yield(factionId, baseId, x, y, tf);

    // get map tile

    MAP* tile = getMapTile(x, y);

    // bad tile - should not happen, though

    if (!tile)
		return value;

	// apply condenser and enricher fix if configured

	if (conf.condenser_and_enricher_do_not_multiply_nutrients)
	{
		// condenser does not multiply nutrients

		if (tile->items & TERRA_CONDENSER)
		{
			value = (value * 2 + 2) / 3;
		}

		// enricher does not multiply nutrients

		if (tile->items & TERRA_SOIL_ENR)
		{
			value = (value * 2 + 2) / 3;
		}

		// enricher adds 1 nutrient

		if (tile->items & TERRA_SOIL_ENR)
		{
			value++;
		}

	}

    return value;

}

/*
mineral yield calculation
*/
HOOK_API int mod_mineral_yield(int factionId, int baseId, int x, int y, int tf)
{
	// call original function

    int value = mineral_yield(factionId, baseId, x, y, tf);

    // add minerals for Pressure dome if configured

    if (conf.pressure_dome_minerals > 0)
	{
		if (baseId >= 0 && baseId < *total_num_bases)
		{
			BASE *base = &(Bases[baseId]);

			if (x == base->x && y == base->y)
			{
				if (has_facility(baseId, FAC_PRESSURE_DOME))
				{
					value += conf.pressure_dome_minerals;
				}

			}

		}

	}

    return value;

}

/*
energy yield calculation
*/
HOOK_API int mod_energy_yield(int factionId, int baseId, int x, int y, int tf)
{
	// call original function

    int value = energy_yield(factionId, baseId, x, y, tf);

    return value;

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

	for (int stackedVehicleId : getStackVehicles(vehicleId))
	{
		VEH *stackedVehicle = &(Vehicles[stackedVehicleId]);

		// exclude self

		if (stackedVehicleId == vehicleId)
			continue;

		// get loaded vehicles

		if (stackedVehicle->move_status == ORDER_SENTRY_BOARD && stackedVehicle->waypoint_1_x == vehicleId)
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
	VEH *vehicle = &(Vehicles[vehicleId]);
	return vehicle->x == x && vehicle->y == y;
}

bool isVehicleAtLocation(int vehicleId, MAP *tile)
{
	return isVehicleAtLocation(vehicleId, getX(tile), getY(tile));
}

std::vector<int> getFactionLocationVehicleIds(int factionId, MAP *tile)
{
	std::vector<int> vehicleIds;

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
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

    if (base->status_flags & BASE_HURRY_PRODUCTION)
		return;

	debug("\tminerals = %d, cost = %d\n", minerals, cost);

	// apply hurry parameters

    Faction* faction = &(Factions[base->faction_id]);
    base->minerals_accumulated += minerals;
    faction->energy_credits -= cost;
    base->status_flags |= BASE_HURRY_PRODUCTION;

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
	
	if
	(
		// sentry
		vehicle->move_status == ORDER_SENTRY_BOARD
		&&
		// on transport
		vehicle->waypoint_1_y == 0
		&&
		vehicle->waypoint_1_x >= 0
	)
	{
		return vehicle->waypoint_1_x;
	}
	
	return -1;
	
}

int setMoveTo(int vehicleId, MAP *destination)
{
    VEH *vehicle = getVehicle(vehicleId);
    MAP *vehicleTile = getVehicleMapTile(vehicleId);
    int vehicleSpeed1 = (mod_veh_speed(vehicleId) == Rules->mov_rate_along_roads ? 1 : 0);
	int x = getX(destination);
	int y = getY(destination);
	bool vehicleTileOcean = is_ocean(vehicleTile);
	bool destinationOcean = is_ocean(destination);
	
    debug("setMoveTo (%3d,%3d) -> (%3d,%3d)\n", vehicle->x, vehicle->y, x, y);
    
    vehicle->waypoint_1_x = x;
    vehicle->waypoint_1_y = y;
    vehicle->move_status = ORDER_MOVE_TO;
    vehicle->status_icon = 'G';
    vehicle->terraforming_turns = 0;

    // vanilla bug fix for adjacent tile move
    
    int vehicleTileRangeToDestination = getRange(vehicleTile, destination);
    
    if (vehicle->triad() == TRIAD_LAND && !vehicleTileOcean && !destinationOcean && vehicleTileRangeToDestination == 1)
	{
		int factionId = vehicle->faction_id;
		int directMoveHexCost = getHexCost(vehicle->unit_id, vehicle->faction_id, vehicle->x, vehicle->y, x, y, vehicleSpeed1);
		bool vehicleTileZoc = isZoc(factionId, vehicleTile);
		bool destinationZoc = isZoc(factionId, destination);
		
		// set direct move to infinite cost if zoc
		
		if (vehicleTileZoc && destinationZoc)
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
			bool adjacentTileZoc = isZoc(factionId, adjacentTile);
			
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
			
			if (vehicleTileZoc && adjacentTileZoc)
				continue;
			
			// compute cost
			
			int hexCost1 = getHexCost(vehicle->unit_id, vehicle->faction_id, vehicle->x, vehicle->y, adjacentTileX, adjacentTileY, vehicleSpeed1);
			int hexCost2 = getHexCost(vehicle->unit_id, vehicle->faction_id, adjacentTileX, adjacentTileY, x, y, vehicleSpeed1);
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
			vehicle->waypoint_1_x = getX(waypoint);
			vehicle->waypoint_1_y = getY(waypoint);
			vehicle->waypoint_2_x = x;
			vehicle->waypoint_2_y = y;
			debug("setWaypoint (%3d,%3d)\n", getX(waypoint), getY(waypoint));
		}
		
	}
	
    return SYNC;
	
}

int setMoveTo(int vehicleId, const std::vector<MAP *> &waypoints)
{
    VEH* vehicle = &(Vehicles[vehicleId]);
	
    debug("setMoveTo (%3d,%3d) -> waypoints\n", vehicle->x, vehicle->y);
	
	setVehicleWaypoints(vehicleId, waypoints);
    vehicle->move_status = ORDER_MOVE_TO;
    vehicle->status_icon = 'G';
    vehicle->terraforming_turns = 0;

    return SYNC;

}

/*
Checks if territory belongs to nobody, us or ally.
*/
bool isFriendlyTerritory(int factionId, MAP* tile)
{
	return tile->owner == -1 || tile->owner == factionId || has_pact(factionId, tile->owner);
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

	return unit->weapon_type == WPN_HAND_WEAPONS && unit->armor_type == ARM_NO_ARMOR;

}

bool isScoutVehicle(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);

	return unit->weapon_type == WPN_HAND_WEAPONS && unit->armor_type == ARM_NO_ARMOR;

}

/*
Checks if location is targetted by any vehicle.
*/
bool isTargettedLocation(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (vehicle->move_status == ORDER_MOVE_TO && vehicle->x == x && vehicle->y == y)
			return true;

	}

	return false;

}

/*
Checks if location is targetted by faction vehicle.
*/
bool isFactionTargettedLocation(MAP *tile, int factionId)
{
	int x = getX(tile);
	int y = getY(tile);

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// only given faction

		if (vehicle->faction_id != factionId)
			continue;

		if (vehicle->move_status == ORDER_MOVE_TO && vehicle->x == x && vehicle->y == y)
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

	if (*current_turn < 45)
	{
		morale = 0;
	}
	else if (*current_turn < 90)
	{
		morale = 1;
	}
	else if (*current_turn < 170)
	{
		morale = 2;
	}
	else if (*current_turn < 250)
	{
		morale = 3;
	}
	else if (*current_turn < 330)
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
Returns vehicle morale.
*/
int getVehicleMorale(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// get basis vehicle morale

	int morale = vehicle->morale;

	// faction MORALE bonus for regular units

	if (!isNativeVehicle(vehicleId))
	{
		morale += getFactionSEMoraleBonus(vehicle->faction_id);
	}

	// return morale clipped by possible range

	return std::max((int)MORALE_VERY_GREEN, std::min((int)MORALE_ELITE, morale));

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
		if (isProjectBuilt(base->faction_id, FAC_XENOEMPATHY_DOME))
		{
			morale++;
		}

		if (isProjectBuilt(base->faction_id, FAC_PHOLUS_MUTAGEN))
		{
			morale++;
		}

		if (isProjectBuilt(base->faction_id, FAC_VOICE_OF_PLANET))
		{
			morale++;
		}

		if (has_facility(baseId, FAC_CENTAURI_PRESERVE))
		{
			morale++;
		}

		if (has_facility(baseId, FAC_TEMPLE_OF_PLANET))
		{
			morale++;
		}

		if (has_facility(baseId, FAC_BIOLOGY_LAB))
		{
			morale++;
		}

		if (has_facility(baseId, FAC_BIOENHANCEMENT_CENTER))
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

double getBasePsiDefenseMultiplier()
{
	return (1.0 + (double)Rules->combat_bonus_intrinsic_base_def / 100.0);
}

bool isWithinFriendlySensorRange(int factionId, int x, int y)
{
	MAP *centralTile = getMapTile(x, y);

	// should be in own or neutral territory

	if (!(centralTile->owner == -1 || centralTile->owner == factionId))
		return false;

	// find real sensors

	for (int dx = -4; dx <= +4; dx++)
	{
		for (int dy = -(4 - abs(dx)); dy <= +(4 - abs(dx)); dy += 2)
		{
			MAP *tile = getMapTile(wrap(x + dx), y + dy);
			if (tile == NULL)
				continue;

			if (tile->owner != factionId)
				continue;

			if (map_has_item(tile, TERRA_SENSOR))
				return true;

		}
	}

	// find bases with survey pod

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		if (base->faction_id != factionId)
			continue;

		if (map_range(x, y, base->x, base->y) > 2)
			continue;

		if (has_facility(baseId, FAC_GEOSYNC_SURVEY_POD))
			return true;

	}

	// not found

	return false;

}

int getRegionPodCount(int x, int y, int range, int region)
{
	int regionPodCount = 0;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);

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

/*
Return true if vehicle order == ORDER_SENTRY_BOARD and damage > 0.
*/
bool isVehicleHealing(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	return vehicle->move_status == ORDER_SENTRY_BOARD && vehicle->damage_taken > 0;
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

		if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE) && isTileConnectedToRegion(vehicle->x, vehicle->y, region))
		{
			vehicleInRegion = true;
		}

	}

	return vehicleInRegion;

}

bool isProbeVehicle(int vehicleId)
{
	return (Units[Vehicles[vehicleId].unit_id].weapon_type == WPN_PROBE_TEAM);
}

/*
Computes relative attacker/defender strength.
*/
double battleCompute(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat)
{
	VEH *attackerVehicle = &(Vehicles[attackerVehicleId]);
	VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);
	MAP *defenderVehicleTile = getMapTile(defenderVehicleId);

	// set flags

	int flags = 0x00;

	if
	(
		longRangeCombat
		&&
		(
			attackerVehicle->triad() == TRIAD_LAND && can_arty(attackerVehicle->unit_id, 1)
			||
			attackerVehicle->triad() == TRIAD_SEA && can_arty(attackerVehicle->unit_id, 1) && defenderVehicle->triad() == TRIAD_LAND && !is_ocean(defenderVehicleTile)
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
		attackerVehicle->triad() == TRIAD_AIR && Chassis[Units[attackerVehicle->unit_id].chassis_type].missile == 0
		&&
		attackerVehicle->offense_value() > 0 && attackerVehicle->defense_value() > 0
		&&
		vehicle_has_ability(defenderVehicleId, ABL_AIR_SUPERIORITY) && Chassis[Units[attackerVehicle->unit_id].chassis_type].missile == 0
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

	mod_battle_compute(attackerVehicleId, defenderVehicleId, (int)&attackerOffenseValue, (int)&defenderStrength, flags);

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
	assert(attackerVehicleId >= 0 && attackerVehicleId < *total_num_vehicles);
	assert(defenderVehicleId >= 0 && defenderVehicleId < *total_num_vehicles);
	
	// find best defender
	
	int bestDefenderVehicleId = mod_best_defender(defenderVehicleId, attackerVehicleId, (int)longRangeCombat);
	
	// compute relative strength against best defender
	
	return battleCompute(attackerVehicleId, bestDefenderVehicleId, longRangeCombat);
	
}

int getVehicleSpeedWithoutRoads(int id)
{
	return mod_veh_speed(id) / Rules->mov_rate_along_roads;
}

int getFactionBestWeapon(int factionId)
{
	int bestWeapon = WPN_HAND_WEAPONS;
	int bestWeaponOffenseValue = 1;

	for (int weaponId = WPN_LASER; weaponId <= WPN_STRING_DISRUPTOR; weaponId++)
	{
		if (has_tech(factionId, Weapon[weaponId].preq_tech))
		{
			if (Weapon[weaponId].offense_value > bestWeaponOffenseValue)
			{
				bestWeapon = weaponId;
				bestWeaponOffenseValue = Weapon[weaponId].offense_value;
			}

		}

	}

	return bestWeapon;

}

/*
Searches for faction best weapon not exceeding limit.
*/
int getFactionBestWeapon(int factionId, int limit)
{
	int bestWeapon = WPN_HAND_WEAPONS;
	int bestWeaponOffenseValue = 1;

	for (int weaponId = WPN_LASER; weaponId <= WPN_STRING_DISRUPTOR; weaponId++)
	{
		if (has_tech(factionId, Weapon[weaponId].preq_tech))
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

	return bestWeapon;

}

int getFactionBestArmor(int factionId)
{
	int bestArmor = ARM_NO_ARMOR;
	int bestArmorDefenseValue = 1;

	for (int armorId = ARM_SYNTHMETAL_ARMOR; armorId <= ARM_RESONANCE_8_ARMOR; armorId++)
	{
		if (has_tech(factionId, Armor[armorId].preq_tech))
		{
			if (Armor[armorId].defense_value > bestArmorDefenseValue)
			{
				bestArmor = armorId;
				bestArmorDefenseValue = Armor[armorId].defense_value;
			}

		}

	}

	return bestArmor;

}

/*
Searches for faction best armor not exceeding limit.
*/
int getFactionBestArmor(int factionId, int limit)
{
	int bestArmor = ARM_NO_ARMOR;
	int bestArmorDefenseValue = 1;

	for (int armorId = ARM_SYNTHMETAL_ARMOR; armorId <= ARM_RESONANCE_8_ARMOR; armorId++)
	{
		if (has_tech(factionId, Armor[armorId].preq_tech))
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

	return bestArmor;

}

bool isSensorBonusApplied(int factionId, int x, int y, bool attacker)
{
	MAP *tile = getMapTile(x, y);

	// check if within friendly sensor range

	if (!isWithinFriendlySensorRange(factionId, x, y))
		return false;

	// check if at sea and sensor is applicable at sea

	if (is_ocean(tile) && !conf.combat_bonus_sensor_ocean)
		return false;

	// check if attacker and sensor is applicable for attack

	if (attacker && !conf.combat_bonus_sensor_offense)
		return false;

	// otherwise return true

	return true;

}

/*
Finds base in tile.
Returns base id if found. Otherwise, -1.
*/
int getBaseIdAt(int x, int y)
{
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		if (base->x == x && base->y == y)
			return baseId;

	}

	return -1;

}

double getSensorOffenseMultiplier(int factionId, int x, int y)
{
	MAP *tile = getMapTile(x, y);

	return
		(
			isWithinFriendlySensorRange(factionId, x, y)
			&&
			conf.combat_bonus_sensor_offense
			&&
			(!isOceanRegion(tile->region) || conf.combat_bonus_sensor_ocean)
		)
		?
		getPercentageBonusMultiplier(Rules->combat_defend_sensor)
		:
		1.0
	;

}

double getSensorDefenseMultiplier(int factionId, int x, int y)
{
	MAP *tile = getMapTile(x, y);

	return
		(
			isWithinFriendlySensorRange(factionId, x, y)
			&&
			(!isOceanRegion(tile->region) || conf.combat_bonus_sensor_ocean)
		)
		?
		getPercentageBonusMultiplier(Rules->combat_defend_sensor)
		:
		1.0
	;

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

/*
Checks if building mine at this location grants +1 mineral bonus.
*/
bool isMineBonus(int x, int y)
{
	MAP *tile = getMapTile(x, y);

	return
	(
		bonus_at(x, y) == RES_MINERAL
		||
		(map_has_landmark(tile, LM_VOLCANO) && tile->art_ref_id < 9)
		||
		(map_has_landmark(tile, LM_CRATER) && tile->art_ref_id < 9)
		||
		(map_has_landmark(tile, LM_FOSSIL) && tile->art_ref_id < 6)
		||
		map_has_landmark(tile, LM_CANYON)
	)
	;

}

std::vector<int> selectVehicles(const VehicleFilter filter)
{
	std::vector<int> vehicleIds;

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
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
	for (int stackedVehicleId : getStackVehicles(vehicleId))
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
	int x = getX(tile);
	int y = getY(tile);
	
	int vehicleIdAtTile = veh_at(x, y);
	
	if (vehicleIdAtTile == -1)
		return false;
	
	for (int vehicleId : getStackVehicles(vehicleIdAtTile))
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
	int x = getX(tile);
	int y = getY(tile);
	
	int vehicleIdAtTile = veh_at(x, y);
	
	if (vehicleIdAtTile == -1)
		return false;
	
	for (int vehicleId : getStackVehicles(vehicleIdAtTile))
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

/*
Checks if tile is blocked.
*/
bool isBlocked(int factionId, MAP *tile)
{
	if (tile == NULL)
		return false;
	
	// search for unfriendly unit in given tile
	
	int tileOwner = veh_who(getX(tile), getY(tile));
	
	if (tileOwner != -1 && tileOwner != factionId && !has_pact(factionId, tileOwner))
		return true;
	
	return false;
	
}

/*
Checks if tile is in ZOC.
*/
bool isZoc(int factionId, MAP *tile)
{
	if (tile == NULL)
		return false;
	
	// no ZOC on ocean
	
	if (is_ocean(tile))
		return false;
	
	// no ZOC on friendly unit
	
	int tileOwner = veh_who(getX(tile), getY(tile));
	if (tileOwner != -1 && isFriendly(factionId, tileOwner))
		return false;
	
	// no ZOC on base
	
	if (isBaseAt(tile))
		return false;
	
	// search for unfriendly unit in adjacent tiles
	
	for (MAP *adjacentTile : getAdjacentTiles(tile))
	{
		// vehicle on ocean does not excert ZOC
		
		if (is_ocean(adjacentTile))
			continue;
		
		int adjacentTileOwner = veh_who(getX(adjacentTile), getY(adjacentTile));
		
		if (adjacentTileOwner != -1 && adjacentTileOwner != factionId && !has_pact(factionId, adjacentTileOwner))
			return true;
		
	}
	
	return false;
	
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
		conf.ignore_reactor_power_in_combat
	;

	// calculate attacker and defender power

	int attackerPower = 10 * (ignoreReactor ? 1 : attackerVehicle->reactor_type()) - attackerVehicle->damage_taken / (ignoreReactor ? attackerVehicle->reactor_type() : 1);
	int defenderPower = 10 * (ignoreReactor ? 1 : defenderVehicle->reactor_type()) - defenderVehicle->damage_taken / (ignoreReactor ? defenderVehicle->reactor_type() : 1);

	// calculate odds

	return relativeStrength * ((double)attackerPower / (double)defenderPower);

}

double getBattleOddsAt(int attackerVehicleId, int defenderVehicleId, bool longRangeCombat, MAP *battleTile)
{
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
	assert(attackerVehicleId >= 0 && attackerVehicleId < *total_num_vehicles);
	assert(defenderVehicleId >= 0 && defenderVehicleId < *total_num_vehicles);
	
	// find best defender
	
	int bestDefenderVehicleId = mod_best_defender(defenderVehicleId, attackerVehicleId, (int)longRangeCombat);
	
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
	return Weapon[Units[unitId].weapon_type].offense_value;
}

int getUnitDefenseValue(int unitId)
{
	return Armor[Units[unitId].armor_type].defense_value;
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

	if (*current_turn < 45)
	{
		morale = 0;
	}
	else if (*current_turn < 90)
	{
		morale = 1;
	}
	else if (*current_turn < 170)
	{
		morale = 2;
	}
	else if (*current_turn < 250)
	{
		morale = 3;
	}
	else if (*current_turn < 330)
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

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int mapX = getX(mapIndex);
		int mapY = getY(mapIndex);
		MAP *tile = getMapTile(mapIndex);

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
	tx_kill(vehicleId);
}

void board(int vehicleId)
{
	VEH* vehicle = getVehicle(vehicleId);
	
	// boarded already
	
	int transportVehicleId = getVehicleTransportId(vehicleId);
	
	if (transportVehicleId != -1)
		return;
	
	// find available transport in same tile
	
	for (int tileVehicleId : getStackVehicles(vehicleId))
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
	vehicle->waypoint_1_y = 0;
	vehicle->waypoint_1_x = transportVehicleId;
	vehicle->state |= VSTATE_IN_TRANSPORT;
	
}

void unboard(int vehicleId)
{
	VEH* vehicle = &(Vehicles[vehicleId]);

	setVehicleOrder(vehicleId, ORDER_NONE);
	vehicle->waypoint_1_y = 0;
	vehicle->waypoint_1_x = -1;
	vehicle->state &= ~VSTATE_IN_TRANSPORT;

}

int getTransportUsedCapacity(int transportVehicleId)
{
	assert(transportVehicleId >= 0 && transportVehicleId < *total_num_vehicles);

	int transportUsedCapacity = 0;

	// get top of the stack vehicle

	int topStackVehicleId = transportVehicleId;
	while (Vehicles[topStackVehicleId].prev_unit_id_stack != -1)
	{
		topStackVehicleId = Vehicles[topStackVehicleId].prev_unit_id_stack;
	}

	// count vehicles loaded on this transport

	for (int vehicleId = topStackVehicleId; vehicleId != -1; vehicleId = Vehicles[vehicleId].next_unit_id_stack)
	{
		// skip transport itself

		if (vehicleId == transportVehicleId)
			continue;

		// increment used capcity for vehicle on this transport

		if (getVehicleTransportId(vehicleId) == transportVehicleId)
		{
			transportUsedCapacity++;
		}

	}

	return transportUsedCapacity;

}

int getTransportRemainingCapacity(int transportVehicleId)
{
	assert(transportVehicleId >= 0 && transportVehicleId < *total_num_vehicles);

	return tx_veh_cargo(transportVehicleId) - getTransportUsedCapacity(transportVehicleId);

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

	int currentBaseId = *current_base_id;

	// set base pointers

	set_base(baseId);

	// compute base nutrients

	tx_base_nutrient();

	// restore current base pointers

	set_base(currentBaseId);

	// return computed GROWTH rate

	return *current_base_growth_rate;

}

void accumulateMapIntValue(robin_hood::unordered_flat_map<int, int> *m, int key, int value)
{
	if (m->count(key) == 0)
	{
		m->insert({key, value});
	}
	else
	{
		m->at(key) += value;
	}

}

int getHexCost(int unitId, int factionId, int fromX, int fromY, int toX, int toY, int speed1)
{
	return mod_hex_cost(unitId, factionId, fromX, fromY, toX, toY, speed1);
}
int getHexCost(int unitId, int factionId, MAP *fromTile, MAP *toTile, int speed1)
{
	return mod_hex_cost(unitId, factionId, getX(fromTile), getY(fromTile), getX(toTile), getY(toTile), speed1);
}

int getVehicleHexCost(int vehicleId, int fromX, int fromY, int toX, int toY)
{
	VEH *vehicle = getVehicle(vehicleId);
	return mod_hex_cost(vehicle->unit_id, vehicle->faction_id, fromX, fromY, toX, toY, (getVehicleSpeed(vehicleId) == Rules->mov_rate_along_roads ? 1 : 0));
}

int getSeaHexCost(MAP *tile)
{
	return (map_has_item(tile, TERRA_FUNGUS) ? 3 : 1) * Rules->mov_rate_along_roads;
}

/*
Returns true if two factions has commlink.
*/
bool isCommlink(int factionId1, int factionId2)
{
	return factionId1 != factionId2 && factionId1 > 0 && factionId2 > 0 && (getFaction(factionId1)->diplo_status[factionId2] & DIPLO_COMMLINK);
}

/*
Returns true if two factions are at war.
Also account for natives are always at war with other factions.
*/
bool isVendetta(int factionId1, int factionId2)
{
	return factionId1 == 0 || factionId2 == 0 || at_war(factionId1, factionId2);
}

/*
Retruns true if two factions have pact.
*/
bool isPact(int factionId1, int factionId2)
{
	return has_pact(factionId1, factionId2);
}

/*
Retruns true if same faction or pact.
*/
bool isFriendly(int factionId1, int factionId2)
{
	return factionId1 == factionId2 || has_pact(factionId1, factionId2);
}

/*
Retruns true if war.
*/
bool isHostile(int factionId1, int factionId2)
{
	return isVendetta(factionId1, factionId2);
}

/*
Retruns true if not same, not pact, not war.
*/
bool isNeutral(int factionId1, int factionId2)
{
	return !isFriendly(factionId1, factionId2) && !isHostile(factionId1, factionId2);
}

/*
Checks if faction at war with any other normal faction.
*/
bool isVendettaWithAny(int factionId)
{
	for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
	{
		if (otherFactionId == factionId)
			continue;

		if (isVendetta(factionId, otherFactionId))
			return true;

	}

	return false;

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
int getVehicleMaxConventionalHP(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	int opponentFirePower = (conf.ignore_reactor_power_in_combat ? vehicle->reactor_type() : 1);
	return (10 * vehicle->reactor_type()) / opponentFirePower;

}

/*
Returns vehicle current hit points in conventional combat.
*/
int getVehicleCurrentConventionalHP(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	int opponentFirePower = (conf.ignore_reactor_power_in_combat ? vehicle->reactor_type() : 1);
	return (10 * vehicle->reactor_type() - vehicle->damage_taken / (opponentFirePower + 1)) / opponentFirePower;

}

/*
Returns unit slot by unitId.
*/
int getUnitSlotById(int unitId)
{
	assert(unitId >= 0 && unitId < MaxProtoNum);
	return (unitId < MaxProtoFactionNum ? unitId : MaxProtoFactionNum + unitId % MaxProtoFactionNum);
}
/*
Returns unitId by faction and slot number.
*/
int getUnitIdBySlot(int factionId, int slot)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	assert(slot >= 0 && slot < 2 * MaxProtoFactionNum);
	assert(factionId != 0 || slot < MaxProtoFactionNum);
	return (slot < MaxProtoFactionNum ? slot : MaxProtoFactionNum * (factionId - 1) + slot);
}
/*
Generates unit index by factionId and unitId.
Unit index is unique identifier defining both unit faction and id.
*/
int getUnitIndexByUnitId(int factionId, int unitId)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	return (2 * MaxProtoFactionNum) * factionId + getUnitSlotById(unitId);
}
/*
Extracts factionId from unitIndex.
*/
int getFactionIdByUnitIndex(int unitIndex)
{
	assert(unitIndex >= 0 && unitIndex < MaxProtoFactionNum || unitIndex >= (2 * MaxProtoFactionNum) && unitIndex < (2 * MaxProtoFactionNum) * MaxPlayerNum);
	return unitIndex / (2 * MaxProtoFactionNum);
}
/*
Extracts unitId from unitIndex.
*/
int getUnitIdByUnitIndex(int unitIndex)
{
	assert(unitIndex >= 0 && unitIndex < MaxProtoFactionNum || unitIndex >= (2 * MaxProtoFactionNum) && unitIndex < (2 * MaxProtoFactionNum) * MaxPlayerNum);
	return getUnitIdBySlot(getFactionIdByUnitIndex(unitIndex), unitIndex % (2 * MaxProtoFactionNum));
}

/*
Determines number of drones could be quelled by police.
*/
int getBasePoliceDrones(int baseId)
{
	// compute base

	computeBase(baseId, false);

	// get drones currently quelled by police

	int quelledDrones = *CURRENT_BASE_DRONES_FACILITIES - *CURRENT_BASE_DRONES_POLICE;

	// get drones left after projects applied

	int leftDrones = *CURRENT_BASE_DRONES_PROJECTS;

	// get doctors

	int doctors = getBaseDoctors(baseId);

	// calculate potential number of drones can be quelled with police

	int policeDrones = quelledDrones + leftDrones + doctors;

	return policeDrones;

}

/*
Returns true if reactor is ignored in conventional combat.
*/
bool isReactorIgnoredInConventionalCombat()
{
	return (conf.ignore_reactor_power || conf.ignore_reactor_power_in_combat);
}

/*
Returns true if reactor is ignored in combat.
*/
bool isReactorIgnoredInCombat(bool psiCombat)
{
	return psiCombat || isReactorIgnoredInConventionalCombat();
}

bool isInfantryUnit(int unitId)
{
	return getUnit(unitId)->chassis_type == CHS_INFANTRY;
}

bool isInfantryVehicle(int vehicleId)
{
	return isInfantryUnit(getVehicle(vehicleId)->unit_id);
}

bool isInfantryPolice2xUnit(int unitId, int factionId)
{
	UNIT *unit = &(Units[unitId]);
	bool wormPolice = isFactionSpecial(factionId, FACT_WORMPOLICE);

	return
		(
			isCombatUnit(unitId) && unit->chassis_type == CHS_INFANTRY && isUnitHasAbility(unitId, ABL_POLICE_2X)
		)
		||
		(
			wormPolice && unitId == BSC_MIND_WORMS
		)
	;

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
	return (Weapon[Units[attackerUnitId].weapon_type].offense_value < 0 || Armor[Units[defenderUnitId].armor_type].defense_value < 0);
}

double getAlienTurnStrengthModifier()
{
	return (*current_turn <= conf.aliens_fight_half_strength_unit_turn ? 0.5 : 1.0);
}

bool isProjectBuilt(int factionId, int projectFacilityId)
{
	assert(projectFacilityId >= PROJECT_ID_FIRST && projectFacilityId <= PROJECT_ID_LAST);

	// get projectId

	int projectId = projectFacilityId - PROJECT_ID_FIRST;

	// get project base

	int projectBaseId = SecretProjects[projectId];

	// check project is built

	if (projectBaseId == -1)
		return false;

	// get project base

	BASE *base = getBase(projectBaseId);

	// return true if base belongs to faction

	return base->faction_id == factionId;

}

bool isProjectAvailable(int factionId, int projectFacilityId)
{
	assert(projectFacilityId >= PROJECT_ID_FIRST && projectFacilityId <= PROJECT_ID_LAST);

	// get projectId

	int projectId = projectFacilityId - PROJECT_ID_FIRST;

	// check project is already built

	int projectBaseId = SecretProjects[projectId];

	// check project is built

	if (projectBaseId >= 0)
		return false;

	// check faction did not unlocked project

	if (!has_tech(factionId, Facility[projectFacilityId].preq_tech))
		return false;

	// all conditions met

	return true;

}

int getFactionMaintenance(int factionId)
{
	return Factions[factionId].maintenance;
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

	return vehicle->move_status == ORDER_SENTRY_BOARD && vehicle->waypoint_1_x == -1 && vehicle->waypoint_1_y == 0;

}

bool isVehicleOnAlert(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return vehicle->move_status == ORDER_HOLD && vehicle->waypoint_1_x == -1 && vehicle->waypoint_1_y == 0 && vehicle->waypoint_2_x == vehicle->x && vehicle->waypoint_2_y == vehicle->y;

}

bool isVehicleOnHold(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return vehicle->move_status == ORDER_HOLD && vehicle->waypoint_1_x == -1 && vehicle->waypoint_1_y == 0 && vehicle->waypoint_2_x == -1 && vehicle->waypoint_2_y == -1;

}

bool isVehicleOnHold10Turns(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	return vehicle->move_status == ORDER_HOLD && vehicle->waypoint_1_x == -1 && vehicle->waypoint_1_y == 10 && vehicle->waypoint_2_x == -1 && vehicle->waypoint_2_y == -1;

}

void setVehicleOnSentry(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	vehicle->move_status = ORDER_SENTRY_BOARD;
	vehicle->waypoint_1_x = -1;
	vehicle->waypoint_1_y = 0;

}

void setVehicleOnAlert(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	vehicle->move_status = ORDER_HOLD;
	vehicle->waypoint_1_x = -1;
	vehicle->waypoint_1_y = 0;
	vehicle->waypoint_2_x = vehicle->x;
	vehicle->waypoint_2_y = vehicle->y;

}

void setVehicleOnHold(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	vehicle->move_status = ORDER_HOLD;
	vehicle->waypoint_1_x = -1;
	vehicle->waypoint_1_y = 0;
	vehicle->waypoint_2_x = -1;
	vehicle->waypoint_2_y = -1;

}

void setVehicleOnHold10Turns(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	vehicle->move_status = ORDER_HOLD;
	vehicle->waypoint_1_x = -1;
	vehicle->waypoint_1_y = 10;
	vehicle->waypoint_2_x = -1;
	vehicle->waypoint_2_y = -1;

}

/*
Calculates correct difference between two x coordinates.
*/
int getDx(int x1, int x2)
{
	int Dx = x2 - x1;

	if (!*map_toggle_flat)
	{
		if (abs(Dx) > *map_half_x)
		{
			if (Dx > 0)
			{
				Dx -= *map_axis_x;
			}
			else
			{
				Dx += *map_axis_x;
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
	if (vehicleId >= 0 && vehicleId < *total_num_vehicles)
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
	return (baseId >= 0 && baseId < *total_num_bases ? &(Bases[baseId]) : nullptr);
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

	return has_facility(baseId, regularRepairFacility);

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
	return getChassis(getUnit(unitId)->chassis_type);
}

CChassis *getVehicleChassis(int vehicleId)
{
	return getChassis(getUnit(getVehicle(vehicleId)->unit_id)->chassis_type);
}

bool isNeedlejetUnit(int unitId)
{
	return getUnit(unitId)->chassis_type == CHS_NEEDLEJET;
}

bool isNeedlejetVehicle(int vehicleId)
{
	return isNeedlejetUnit(getVehicle(vehicleId)->unit_id);
}

bool isNeedlejetUnitInFlight(int unitId, MAP *tile)
{
	return isNeedlejetUnit(unitId) && !isAirbaseAt(tile);
}

bool isNeedlejetVehicleInFlight(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return isNeedlejetUnitInFlight(vehicle->unit_id, vehicleTile);
}

void longRangeFire(int vehicleId, int offsetIndex)
{
	battle_fight_1(vehicleId, offsetIndex, 1, 1, 0);
}
void longRangeFire(int vehicleId, MAP *attackLocation)
{
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
		map_has_item(tile, TERRA_BASE_IN_TILE | TERRA_BUNKER | TERRA_MONOLITH)
		||
		isNativeVehicle(vehicleId) && map_has_item(tile, TERRA_FUNGUS)
		||
		isProjectBuilt(vehicle->faction_id, FAC_NANO_FACTORY)
	;
	
}

RepairInfo getVehicleRepairInfo(int vehicleId, MAP *tile)
{
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
	
	if (map_has_item(tile, TERRA_MONOLITH))
	{
		fullRepair = true;
	}
	
	// native in fungus
	
	if (native && map_has_item(tile, TERRA_FUNGUS))
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
	
	if (isProjectBuilt(vehicle->faction_id, FAC_NANO_FACTORY))
	{
		fullRepair = true;
		repairRate += conf.repair_nano_factory;
	}
	
	// check location
	
	if (map_has_item(tile, TERRA_BASE_IN_TILE))
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
	else if (triad == TRIAD_LAND && map_has_item(tile, TERRA_BUNKER) && !is_ocean(tile))
	{
		fullRepair = true;
		repairRate += conf.repair_bunker;
	}
	else if (triad == TRIAD_AIR && map_has_item(tile, TERRA_AIRBASE))
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
	
	int repairTime = (map_has_item(tile, TERRA_MONOLITH) ? 0 : divideIntegerRoundUp(repairDamage, repairRate));
	
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

/*
Checks if unit is a prototype.
*/
bool isPrototypeUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);

	return (unitId >= MaxProtoFactionNum && !(unit->unit_flags & UNIT_PROTOTYPED));

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

		if (!has_tech(factionId, unit->preq_tech))
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

double getBaseSupportRatio(int baseId)
{
	BASE *base = getBase(baseId);

	if (base->mineral_intake_2 <= 0)
		return 0.0;

	return (double)(base->mineral_intake_2 - base->mineral_surplus) / (double)base->mineral_intake_2;

}

bool isAtArtilleryDamageLimit(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	bool vehicleTileOcean = is_ocean(vehicleTile);
	
	int damagePercentageLimit;
	
	if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE | TERRA_BUNKER))
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
	CChassis *chassis = getChassis(unit->chassis_type);

	int maxRebaseTurns;

	switch (unit->chassis_type)
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

/*
Checks whether it is allowed to build base here without breaking treaty.
territory: unclaimed, own, unknown, hostile
*/
bool isAllowedBaseLocation(int factionId, MAP *tile)
{
	return tile->owner == -1 || tile->owner == factionId || !isCommlink(tile->owner, factionId) || isVendetta(tile->owner, factionId);
}

/*
Checks if land has rough terrain bonus.
*/
bool isRoughTerrain(MAP *tile)
{
	if (is_ocean(tile))
		return false;
	
	if (map_has_item(tile, TERRA_BASE_IN_TILE))
		return false;
	
	if (map_has_item(tile, TERRA_FOREST | TERRA_FUNGUS) || map_rockiness(tile) == 2)
		return true;
	
	return false;
	
}

/*
Checks if land has open terrain bonus.
*/
bool isOpenTerrain(MAP *tile)
{
	if (is_ocean(tile))
		return false;
	
	if (map_has_item(tile, TERRA_BASE_IN_TILE))
		return false;
	
	if (map_has_item(tile, TERRA_FOREST | TERRA_FUNGUS) || map_rockiness(tile) == 2)
		return false;
	
	return true;
	
}

bool isMobileUnit(int unitId)
{
	switch (getUnit(unitId)->chassis_type)
	{
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		return true;
	default:
		return false;
	}
}

/*
Computes melee attack strength multiplier taking all bonuses/penalties into account.
exact location: whether combat happens at this exact location or nearby
*/
double getUnitMeleeAttackStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation)
{
	int x = getX(tile);
	int y = getY(tile);
	
	double attackStrengthMultiplier = 1.0;
	
	// sensor
	
	attackStrengthMultiplier *= getSensorOffenseMultiplier(attackerFactionId, x, y);
	attackStrengthMultiplier /= getSensorOffenseMultiplier(defenderFactionId, x, y);
	
	// other modifiers are difficult to guess at not exact location
	
	if (!exactLocation)
		return attackStrengthMultiplier;
	
	// base and terrain
	
	int baseId = base_at(x, y);
	if (baseId != -1)
	{
		// base
		
		attackStrengthMultiplier /= getBaseDefenseMultiplier(baseId, attackerUnitId, defenderUnitId);
		
		if (isInfantryUnit(attackerUnitId))
		{
			// infantry vs. base
			
			attackStrengthMultiplier *= getPercentageBonusMultiplier(Rules->combat_infantry_vs_base);
			
		}
		
		if (isMobileUnit(attackerUnitId))
		{
			// mobile vs. base
			
			attackStrengthMultiplier /= getPercentageBonusMultiplier(Rules->combat_mobile_def_in_rough);
			
		}
		
	}
	else
	{
		if (isNativeUnit(attackerUnitId) && !isNativeUnit(defenderUnitId) && map_has_item(tile, TERRA_FUNGUS))
		{
			// native vs. regular in fungus gets 50% bonus
			
			attackStrengthMultiplier *= getPercentageBonusMultiplier(50);
			
		}
		
		if (!isNativeUnit(attackerUnitId) && !isNativeUnit(defenderUnitId) && map_has_item(tile, TERRA_FUNGUS))
		{
			// regular vs. regular in fungus gets 50% penalty
			
			attackStrengthMultiplier /= getPercentageBonusMultiplier(50);
			
		}
		
		if (!isPsiCombat(attackerUnitId, defenderUnitId) && isRoughTerrain(tile))
		{
			// conventional combat in rough gets 50% penalty
			
			attackStrengthMultiplier /= getPercentageBonusMultiplier(50);
			
		}
		
		if (!isPsiCombat(attackerUnitId, defenderUnitId) && isMobileUnit(attackerUnitId) && isRoughTerrain(tile))
		{
			// mobile vs. rough penalty
			
			attackStrengthMultiplier /= getPercentageBonusMultiplier(Rules->combat_mobile_def_in_rough);
			
		}
		
		if (!isPsiCombat(attackerUnitId, defenderUnitId) && isMobileUnit(attackerUnitId) && !isRoughTerrain(tile))
		{
			// mobile in open bonus
			
			attackStrengthMultiplier *= getPercentageBonusMultiplier(Rules->combat_mobile_open_ground);
			
		}
		
	}
	
	return attackStrengthMultiplier;
	
}

/*
Computes melee attack strength multiplier taking all bonuses/penalties into account.
exact location: whether combat happens at this exact location or nearby
*/
double getUnitArtilleryAttackStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation)
{
	int x = getX(tile);
	int y = getY(tile);
	
	double attackStrengthMultiplier = 1.0;
	
	// sensor
	
	attackStrengthMultiplier *= getSensorOffenseMultiplier(attackerFactionId, x, y);
	attackStrengthMultiplier /= getSensorOffenseMultiplier(defenderFactionId, x, y);
	
	// other modifiers are difficult to guess at not exact location
	
	if (!exactLocation)
		return attackStrengthMultiplier;
	
	// base and terrain
	
	int baseId = base_at(x, y);
	if (baseId != -1)
	{
		// base defense
		
		attackStrengthMultiplier /= getBaseDefenseMultiplier(baseId, attackerUnitId, defenderUnitId);
		
	}
	
	return attackStrengthMultiplier;
	
}

/*
Checks whether tile is an airbase (friendly base or airbase) for this factionId.
*/
bool isAtAirbase(int vehicleId)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return map_has_item(vehicleTile, TERRA_BASE_IN_TILE | TERRA_AIRBASE);
}

bool isUnitCanAttackNeedlejet(int unitId)
{
	return unit_has_ability(unitId, ABL_AIR_SUPERIORITY);
}

bool isUnitCanAttackLowAir(int unitId)
{
	return isMeleeUnit(unitId) || isArtilleryUnit(unitId) && unit_has_ability(unitId, ABL_AIR_SUPERIORITY);
}

int getClosestHostileBaseRange(int factionId, MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	
	int closestHostileBaseRange = INT_MAX;
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
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

/*
Returns unit speed not accounting for elite extra movement bonus.
*/
int getUnitSpeed(int unitId)
{
	return unit_speed(unitId);
}

/*
Returns vehicle speed accounting for elite extra movement bonus.
*/
int getVehicleSpeed(int vehicleId)
{
	return mod_veh_speed(vehicleId);
}

double getEuclidianDistance(int x1, int y1, int x2, int y2)
{
	int dx = x2 - x1;
	int dy = y2 - y1;
	
	if (!*map_toggle_flat)
	{
		if (dx < -*map_half_x)
		{
			dx += *map_axis_x;
		}
		else if (dx > *map_half_x)
		{
			dx -= *map_axis_x;
		}
	}
	
	return sqrt(dx * dx + dy * dy);
	
}

int getVehicleUnitCost(int vehicleId)
{
	return getUnit(getVehicle(vehicleId)->unit_id)->cost;
}

int getVehicleRemainingMovement(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	return getVehicleSpeed(vehicleId) - vehicle->road_moves_spent;
}

/*
Determines if vehicle can move between tiles.
*/
bool isAllowedMove(int vehicleId, MAP *srcTile, MAP *dstTile)
{
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
			
			if (map_has_item(srcTile, TERRA_BASE_IN_TILE))
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
			
			if (isVehicleIgnoreZOC(vehicleId))
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

bool isUnitIgnoreZOC(int unitId)
{
	UNIT *unit = getUnit(unitId);
	
	if (unit->triad() != TRIAD_LAND)
		return true;
	
	if (unit->unit_plan == PLAN_INFO_WARFARE)
		return true;
	
	if (unit_has_ability(unitId, ABL_CLOAKED))
		return true;
	
	return false;
	
}

bool isVehicleIgnoreZOC(int vehicleId)
{
	return isUnitIgnoreZOC(getVehicle(vehicleId)->unit_id);
}

int getBaseAt(MAP *tile)
{
	return base_at(getX(tile), getY(tile));
}

bool isBaseAt(MAP *tile)
{
	return (map_has_item(tile, TERRA_BASE_IN_TILE));
}

bool isFactionBaseAt(MAP *tile, int factionId)
{
	return (map_has_item(tile, TERRA_BASE_IN_TILE) && tile->owner == factionId);
}

bool isFriendlyBaseAt(MAP *tile, int factionId)
{
	return (map_has_item(tile, TERRA_BASE_IN_TILE) && isFriendlyTerritory(factionId, tile));
}

bool isHostileBaseAt(MAP *tile, int factionId)
{
	return (map_has_item(tile, TERRA_BASE_IN_TILE) && isHostileTerritory(factionId, tile));
}

bool isNeutralBaseAt(MAP *tile, int factionId)
{
	return (map_has_item(tile, TERRA_BASE_IN_TILE) && isNeutralTerritory(factionId, tile));
}

bool isUnfriendlyBaseAt(MAP *tile, int factionId)
{
	return (map_has_item(tile, TERRA_BASE_IN_TILE) && !isFriendlyTerritory(factionId, tile));
}

bool isEmptyHostileBaseAt(MAP *tile, int factionId)
{
	return isHostileBaseAt(tile, factionId) && !isVehicleAt(tile);
}

bool isEmptyNeutralBaseAt(MAP *tile, int factionId)
{
	return isNeutralBaseAt(tile, factionId) && !isVehicleAt(tile);
}

bool isBunkerAt(MAP *tile)
{
	return (map_has_item(tile, TERRA_BUNKER));
}

bool isVehicleAt(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	
	return veh_at(x, y) != -1;
	
}

bool isHostileVehicleAt(MAP *tile, int factionId)
{
	int x = getX(tile);
	int y = getY(tile);
	
	int vehicleId = veh_at(x, y);
	
	if (vehicleId == -1)
		return false;
	
	return isHostile(factionId, getVehicle(vehicleId)->faction_id);
	
}

bool isUnfriendlyVehicleAt(MAP *tile, int factionId)
{
	int x = getX(tile);
	int y = getY(tile);
	
	int vehicleId = veh_at(x, y);
	
	if (vehicleId == -1)
		return false;
	
	return !isFriendly(factionId, getVehicle(vehicleId)->faction_id);
	
}

bool isHostileSurfaceVehicleAt(MAP *tile, int factionId)
{
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
Tile may have base/bunker/airbase.
*/
bool isUnitCanMeleeAttackAtTile(int attackerUnitId, int defenderUnitId, MAP *tile)
{
	UNIT *attackerUnit = getUnit(attackerUnitId);
	int attackerTriad = attackerUnit->triad();
	bool tileOcean = is_ocean(tile);
	
	// melee unit
	
	if (!isMeleeUnit(attackerUnitId))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	
	if (isNeedlejetUnitInFlight(defenderUnitId, tile) && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// check movement
	
	bool allowed = true;
	
	switch (attackerTriad)
	{
	case TRIAD_SEA:
		if (!tileOcean && attackerUnitId != BSC_SEALURK)
		{
			// sea vehicle cannot attack enemy on land (except sealurk)
			allowed = false;
		}
		break;
		
	case TRIAD_LAND:
		if (tileOcean)
		{
			if (isUnitHasAbility(attackerUnitId, ABL_AMPHIBIOUS))
			{
				if (!map_has_item(tile, TERRA_BASE_IN_TILE))
				{
					// land amphibious vehicle cannot attack enemy on *open* ocean (not a sea base)
					allowed = false;
				}
			}
			else
			{
				// land non-amphibious vehicle cannot attack enemy on ocean
				allowed = false;
			}
			
		}
		break;
		
	}
	
	return allowed;
	
}

/*
Checks if unit can attack enemy from given tile.
Assuming defender is not at base/bunker/airbase.
*/
bool isUnitCanMeleeAttackFromTile(int attackerUnitId, int defenderUnitId, MAP */*tile*/)
{
	UNIT *attackerUnit = getUnit(attackerUnitId);
	UNIT *defenderUnit = getUnit(defenderUnitId);
	int attackerTriad = attackerUnit->triad();
	int defenderTriad = defenderUnit->triad();
	
	// melee unit
	
	if (!isMeleeUnit(attackerUnitId))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	
	if (isNeedlejetUnit(defenderUnitId) && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// check movement
	
	bool allowed = true;
	
	switch (attackerTriad)
	{
	case TRIAD_SEA:
		if (defenderTriad == TRIAD_LAND && attackerUnitId != BSC_SEALURK)
		{
			// sea unit cannot attack land unit outside of base (except sealurk)
			allowed = false;
		}
		break;
		
	case TRIAD_LAND:
		if (defenderTriad == TRIAD_SEA)
		{
			// land unit cannot attack sea unit outside of base
			allowed = false;
		}
		break;
		
	}
	
	return allowed;
	
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

/*
Checks if vehicle can generally attack enemy at tile.
*/
bool isVehicleCanMeleeAttackTile(int vehicleId, MAP *target)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	bool targetOcean = is_ocean(target);
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return false;
	
	// check movement
	
	bool allowed = true;
	
	switch (triad)
	{
	case TRIAD_SEA:
		if (!targetOcean)
		{
			if (vehicle->unit_id != BSC_SEALURK)
			{
				// sea vehicle cannot attack enemy on land (except sealurk)
				allowed = false;
			}
			
		}
		break;
		
	case TRIAD_LAND:
		if (targetOcean)
		{
			if (isVehicleHasAbility(vehicleId, ABL_AMPHIBIOUS))
			{
				if (!map_has_item(target, TERRA_BASE_IN_TILE))
				{
					// land amphibious vehicle cannot attack enemy on *open* ocean (not a sea base)
					allowed = false;
				}
				
			}
			else
			{
				// land non-amphibious vehicle cannot attack enemy on ocean
				allowed = false;
			}
			
		}
		break;
		
	}
	
	return allowed;
	
}

/*
Checks if vehicle can attack enemy from given tile.
*/
bool isVehicleCanMeleeAttackFromTile(int vehicleId, MAP *from)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	bool fromOcean = is_ocean(from);
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return false;
	
	// check movement
	
	bool allowed = true;
	
	switch (triad)
	{
	case TRIAD_LAND:
		if (fromOcean)
		{
			if (!isVehicleHasAbility(vehicleId, ABL_AMPHIBIOUS))
			{
				// non-amphibious land unit cannot attack from ocean
				allowed = false;
			}
			
		}
		break;
		
	}
	
	return allowed;
	
}

/*
Checks if vehicle can generally attack enemy at tile.
*/
bool isVehicleCanArtilleryAttackTile(int vehicleId, MAP */*target*/)
{
	// artillery vehicle
	
	if (!isArtilleryVehicle(vehicleId))
		return false;
	
	// otherwise artillery attack is allowed
	
	return true;
	
}

/*
Checks if vehicle can attack enemy from given tile.
*/
bool isVehicleCanArtilleryAttackFromTile(int vehicleId, MAP *from)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	bool fromOcean = is_ocean(from);
	
	// artillery vehicle
	
	if (!isArtilleryVehicle(vehicleId))
		return false;
	
	// check movement
	
	bool allowed = true;
	
	switch (triad)
	{
	case TRIAD_LAND:
		if (fromOcean)
		{
			// land vehicle cannot artillery attack from ocean
			allowed = false;
		}
		break;
		
	}
	
	return allowed;
	
}

bool isAirbaseAt(MAP *tile)
{
	return map_has_item(tile, TERRA_BASE_IN_TILE | TERRA_AIRBASE);
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
		vehicle->waypoint_1_x = getX(waypoints.at(0));
		vehicle->waypoint_1_y = getY(waypoints.at(0));
	}
	if (waypoints.size() >= 2)
	{
		vehicle->waypoint_2_x = getX(waypoints.at(1));
		vehicle->waypoint_2_y = getY(waypoints.at(1));
	}
	if (waypoints.size() >= 3)
	{
		vehicle->waypoint_3_x = getX(waypoints.at(2));
		vehicle->waypoint_3_y = getY(waypoints.at(2));
	}
	if (waypoints.size() >= 4)
	{
		vehicle->waypoint_4_x = getX(waypoints.at(3));
		vehicle->waypoint_4_y = getY(waypoints.at(3));
	}
	
}

bool isHoveringLandUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);
	return unit->chassis_type == CHS_HOVERTANK || isUnitHasAbility(unitId, ABL_ANTIGRAV_STRUTS);
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
	
	if (has_facility(baseId, FAC_RECYCLING_TANKS))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_GENEJACK_FACTORY))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_ROBOTIC_ASSEMBLY_PLANT))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_QUANTUM_CONVERTER))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_NANOREPLICATOR))
	{
		multiplierNumerator += 2;
	}
	
	return (double)multiplierNumerator / (double)multiplierDenominator;
	
}

double getBaseEconomyMultiplier(int baseId)
{
	int multiplierNumerator = 4;
	int multiplierDenominator = 4;
	
	if (has_facility(baseId, FAC_ENERGY_BANK))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_TREE_FARM))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_HYBRID_FOREST))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_FUSION_LAB))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_QUANTUM_LAB))
	{
		multiplierNumerator += 2;
	}
	
	return (double)multiplierNumerator / (double)multiplierDenominator;
	
}

double getBaseLabsMultiplier(int baseId)
{
	int multiplierNumerator = 4;
	int multiplierDenominator = 4;
	
	if (has_facility(baseId, FAC_NETWORK_NODE))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_RESEARCH_HOSPITAL))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_NANOHOSPITAL))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_FUSION_LAB))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_QUANTUM_LAB))
	{
		multiplierNumerator += 2;
	}
	
	return (double)multiplierNumerator / (double)multiplierDenominator;
	
}

double getBasePsychMultiplier(int baseId)
{
	int multiplierNumerator = 4;
	int multiplierDenominator = 4;
	
	if (has_facility(baseId, FAC_HOLOGRAM_THEATRE))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_TREE_FARM))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_HYBRID_FOREST))
	{
		multiplierNumerator += 2;
	}
	if (has_facility(baseId, FAC_RESEARCH_HOSPITAL))
	{
		multiplierNumerator += 1;
	}
	if (has_facility(baseId, FAC_NANOHOSPITAL))
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
	assert(vehicleId >= 0 && vehicleId < *total_num_vehicles);
	assert(to >= *MapPtr && to < *MapPtr + *map_area_tiles);
	
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

int getRange(int tile1Index, int tile2Index)
{
	assert(tile1Index >= 0 && tile1Index < *map_area_tiles);
	assert(tile2Index >= 0 && tile2Index < *map_area_tiles);
	
	int y1 = tile1Index / (*map_half_x);
	int x1 = (tile1Index % (*map_half_x)) * 2 + (y1 % 2);
	int y2 = tile2Index / (*map_half_x);
	int x2 = (tile2Index % (*map_half_x)) * 2 + (y2 % 2);
	
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    
	if (*map_toggle_flat && dx > *map_half_x)
	{
		dx = *map_axis_x - dx;
	}
	
	return (dx + dy) / 2;
	
}

int getRange(MAP *origin, MAP *destination)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	return map_range(getX(origin), getY(origin), getX(destination), getY(destination));
}

/*
Returns exact Euclidian distance between points squared.
*/
double getVectorDistanceSquared(MAP *origin, MAP *destination)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	Location delta = getDelta(origin, destination);
	
	return (double)(delta.x * delta.x + delta.y * delta.y) / 2.0;
}

/*
Returns exact Euclidian distance between points.
*/
double getVectorDistance(MAP *origin, MAP *destination)
{
	return sqrt(getVectorDistanceSquared(origin, destination));
}

/*
Returns exact cosine of angle between two segments.
*/
double getVectorAngleCos(MAP *vertex, MAP *point1, MAP *point2)
{
	assert(vertex >= *MapPtr && vertex < *MapPtr + *map_area_tiles);
	assert(point1 >= *MapPtr && point1 < *MapPtr + *map_area_tiles);
	assert(point2 >= *MapPtr && point2 < *MapPtr + *map_area_tiles);
	
	double h2 = getVectorDistanceSquared(point1, point2);
	double ca2 = getVectorDistanceSquared(vertex, point1);
	double cb2 = getVectorDistanceSquared(vertex, point2);
	
	return ((ca2 + cb2) - h2) / (2.0 * sqrt(ca2 * cb2));
	
}

int getAdjacentTileIndex(int tileIndex, int angle)
{
	assert(tileIndex >= 0 && tileIndex < *map_area_tiles);
	assert(angle >= 0 && angle < TABLE_next_cell_count);
	
	int x = getX(tileIndex);
	int y = getY(tileIndex);
	
	int nx = wrap(x + TABLE_next_cell_x[angle]);
	int ny = y + TABLE_next_cell_y[angle];
	
	if (!isOnMap(nx, ny))
		return -1;
	
	return getMapIndexByCoordinates(nx, ny);
	
}

MAP *getTileByAngle(MAP *tile, int angle)
{
	assert(tile >= *MapPtr && tile < *MapPtr + *map_area_tiles);
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
	assert(tile >= *MapPtr && tile < *MapPtr + *map_area_tiles);
	assert(anotherTile >= *MapPtr && anotherTile < *MapPtr + *map_area_tiles);
	
	int tileX = getX(tile);
	int tileY = getY(tile);
	int anotherTileX = getX(anotherTile);
	int anotherTileY = getY(anotherTile);
	
	int dx = anotherTileX - tileX;
	int dy = anotherTileY - tileY;
	
	if (dx < -*map_half_x)
	{
		dx += *map_axis_x;
	}
	else if (dx > +*map_half_x)
	{
		dx -= *map_axis_x;
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
	return mod_goody_at(getX(tile), getY(tile)) != 0;
}

std::vector<int> getTransportPassengers(int transportVehicleId)
{
	std::vector<int> passengerVehicleIds;
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		if (getVehicleTransportId(vehicleId) == transportVehicleId)
		{
			passengerVehicleIds.push_back(vehicleId);
		}
		
	}
	
	return passengerVehicleIds;
	
}

double getVehicleMaxRelativeBombardmentDamage(int vehicleId)
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

double getVehicleRemainingRelativeBombardmentDamage(int vehicleId)
{
	double maxRelativeBombardmentDamage = getVehicleMaxRelativeBombardmentDamage(vehicleId);
	double relativeDamage = getVehicleRelativeDamage(vehicleId);
	
	return std::max(0.0, maxRelativeBombardmentDamage - relativeDamage);
	
}

int getBasePopulationLimit(int baseId)
{
	BASE *base = getBase(baseId);
	
	int populationLimit;
	
	if (!has_facility(baseId, FAC_HAB_COMPLEX))
	{
		populationLimit = getFacilityPopulationLimit(base->faction_id, FAC_HAB_COMPLEX);
	}
	else if (!has_facility(baseId, FAC_HABITATION_DOME))
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
    assert(factionId >= 0 && factionId < MaxPlayerNum);
    assert(facilityId >= PROJECT_ID_FIRST && factionId <= PROJECT_ID_LAST);
    
    if (factionId == 0)
		return false;
	
	return has_project(factionId, facilityId);
	
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

/*
Calculates how much population base receives in one turn.
*/
double getBasePopulationGrowth(int baseId)
{
	BASE *base = &(Bases[baseId]);
	int requiredNutrients = (base->pop_size + 1) * cost_factor(base->faction_id, 0, baseId);
	return 1.0 / (double)(divideIntegerRoundUp(requiredNutrients, base->nutrient_surplus) + 1);
}

int getMovementRateAlongTube()
{
	return (conf.tube_movement_rate_multiplier == 0 ? 0 : 1);
}

int getMovementRateAlongRoad()
{
	return (conf.tube_movement_rate_multiplier == 0 ? 1 : conf.tube_movement_rate_multiplier);
}

bool isInfantryDefensiveUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);
	int offenseValue = getUnitOffenseValue(unitId);
	int defenseValue = getUnitDefenseValue(unitId);
	
	// combat
	
	if (!isCombatUnit(unitId))
		return false;
	
	// infantry
	
	if (unit->chassis_type != CHS_INFANTRY)
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
	return isInfantryDefensiveUnit(getVehicle(vehicleId)->unit_id);
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

bool isUnitRequireSupport(int unitId)
{
	UNIT *unit = getUnit(unitId);
	return unit->unit_plan <= (conf.supply_convoy_and_info_warfare_require_support ? 11 : 9) && !isUnitHasAbility(unitId, ABL_CLEAN_REACTOR);
}

