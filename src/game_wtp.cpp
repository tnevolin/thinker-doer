#include "game_wtp.h"
#include "terranx_wtp.h"
#include "wtp.h"

// =======================================================
// MAP conversions
// =======================================================

bool isOnMap(int x, int y)
{
	return (((x + y) & 0x1) == 0 && (x >= 0 && y >= 0 && x < *map_axis_x && y < *map_axis_y));
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

// =======================================================
// MAP conversions - end
// =======================================================

// =======================================================
// iterating surrounding locations - end
// =======================================================

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
	return getBaseOffsetTiles(x, y, (includeCenter ? 0 : 1), BASE_OFFSET_COUNT_ADJACENT);
}

std::vector<MAP *> getBaseAdjacentTiles(MAP *tile, bool includeCenter)
{
	return getBaseAdjacentTiles(getX(tile), getY(tile), includeCenter);
}

std::vector<MAP *> getBaseRadiusTiles(int x, int y, bool includeCenter)
{
	return getBaseOffsetTiles(x, y, (includeCenter ? 0 : 1), BASE_OFFSET_COUNT_RADIUS);
}

std::vector<MAP *> getBaseRadiusTiles(MAP *tile, bool includeCenter)
{
	return getBaseRadiusTiles(getX(tile), getY(tile), includeCenter);
}

std::vector<MAP *> getBaseRadiusAdjacentTiles(int x, int y, bool includeCenter)
{
	return getBaseOffsetTiles(x, y, (includeCenter ? 0 : 1), BASE_OFFSET_COUNT_RADIUS_ADJACENT);
}

std::vector<MAP *> getBaseRadiusAdjacentTiles(MAP *tile, bool includeCenter)
{
	return getBaseRadiusAdjacentTiles(getX(tile), getY(tile), includeCenter);
}

/*
Returns tile equally ranged from given center.
*/
std::vector<MAP *> getEqualRangeTiles(int x, int y, int range)
{
	std::vector<MAP *> tiles;
	
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
std::vector<MAP *> getEqualRangeTiles(MAP *tile, int range)
{
	return getEqualRangeTiles(getX(tile), getY(tile), range);
}

/*
Returns tiles withing given range from given center.
*/
std::vector<MAP *> getRangeTiles(int x, int y, int range, bool includeCenter)
{
	std::vector<MAP *> tiles;
	
	if (includeCenter)
	{
		tiles.push_back(getMapTile(x, y));
	}
	
	for (int ringRange = 1; ringRange <= range; ringRange++)
	{
		std::vector<MAP *> equalRangeTiles = getEqualRangeTiles(x, y, ringRange);
		tiles.insert(tiles.end(), equalRangeTiles.begin(), equalRangeTiles.end());
		
	}
	
	return tiles;
	
}
std::vector<MAP *> getRangeTiles(MAP *tile, int range, bool includeCenter)
{
	return getRangeTiles(getX(tile), getY(tile), range, includeCenter);
}

/*
Returns tiles withing range 1 from given center.
*/
std::vector<MAP *> getAdjacentTiles(int x, int y, bool includeCenter)
{
	return getRangeTiles(x, y, 1, includeCenter);
}
std::vector<MAP *> getAdjacentTiles(MAP *tile, bool includeCenter)
{
	return getAdjacentTiles(getX(tile), getY(tile), includeCenter);
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
	if (baseId < 0)
		return 0;
	
	BASE *base = &(Bases[baseId]);

	int mineralCost = (item >= 0 ? tx_veh_cost(item, baseId, 0) : Facility[-item].cost) * cost_factor(base->faction_id, 1, -1);

	return mineralCost;

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
bool map_has_item(MAP *tile, unsigned int item) {
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
void setBaseFacility(int base_id, int facility_id, bool add) {
    assert(base_id >= 0 && facility_id > 0 && facility_id <= FAC_EMPTY_SP_64);

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
int getDoctors(int id)
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

	int doctorGeneratedPsych = getDoctors(id) * 2;

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
		psiOffenseStrength = sqrt((double)Rules->psi_combat_land_numerator / (double)Rules->psi_combat_land_denominator);
		break;
	case TRIAD_SEA:
		psiOffenseStrength = sqrt((double)Rules->psi_combat_sea_numerator / (double)Rules->psi_combat_sea_denominator);
		break;
	case TRIAD_AIR:
		psiOffenseStrength = sqrt((double)Rules->psi_combat_air_numerator / (double)Rules->psi_combat_air_denominator);
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
		psiDefenseStrength = sqrt((double)Rules->psi_combat_land_denominator / (double)Rules->psi_combat_land_numerator);
		break;
	case TRIAD_SEA:
		psiDefenseStrength = sqrt((double)Rules->psi_combat_sea_denominator / (double)Rules->psi_combat_sea_numerator);
		break;
	case TRIAD_AIR:
		psiDefenseStrength = sqrt((double)Rules->psi_combat_air_denominator / (double)Rules->psi_combat_air_numerator);
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

	psiOffenseStrength *= getMoraleModifier(getNewVehicleMorale(unitId, baseId, false));

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

	psiDefenseStrength *= getMoraleModifier(getNewVehicleMorale(unitId, baseId, false));

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
	
	conventionalOffenseStrength *= getMoraleModifier(getNewVehicleMorale(unitId, baseId, false));
	
	// return strength
	
	return conventionalOffenseStrength;
	
}

double getNewUnitConventionalDefenseStrength(int unitId, int baseId)
{
	// conventional defense strength
	
	double conventionalDefenseStrength = getUnitConventionalDefenseStrength(unitId);
	
	// morale modifier
	
	conventionalDefenseStrength *= getMoraleModifier(getNewVehicleMorale(unitId, baseId, false));
	
	// return strength
	
	return conventionalDefenseStrength;
	
}

double getVehiclePsiOffenseStrength(int id)
{
	VEH *vehicle = &(Vehicles[id]);
	int triad = veh_triad(id);
	Faction *faction = &(Factions[vehicle->faction_id]);

	// psi attack strength

	double psiOffenseStrength;

	switch (triad)
	{
	case TRIAD_LAND:
		psiOffenseStrength = sqrt((double)Rules->psi_combat_land_numerator / (double)Rules->psi_combat_land_denominator);
		break;
	case TRIAD_SEA:
		psiOffenseStrength = sqrt((double)Rules->psi_combat_sea_numerator / (double)Rules->psi_combat_sea_denominator);
		break;
	case TRIAD_AIR:
		psiOffenseStrength = sqrt((double)Rules->psi_combat_air_numerator / (double)Rules->psi_combat_air_denominator);
		break;
	default:
		return 1.0;
	}

	// morale modifier

	psiOffenseStrength *= getVehicleMoraleModifier(id, false);

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

	psiDefenseStrength *= getVehicleMoraleModifier(id, false);

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

	conventionalOffenseStrength *= getVehicleMoraleModifier(vehicleId, false);

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

	conventionalDefenseStrength *= getVehicleMoraleModifier(vehicleId, false);

	// return strength
	
	return conventionalDefenseStrength;

}

/*
Computes total damage vehicle can deliver in psi offense.
*/
double getVehiclePsiOffense(int vehicleId)
{
	return getVehiclePsiOffenseStrength(vehicleId) * (double)getVehicleHitPoints(vehicleId, true);
}

/*
Computes total damage vehicle can deliver in psi defense.
*/
double getVehiclePsiDefense(int vehicleId)
{
	return getVehiclePsiDefenseStrength(vehicleId) * (double)getVehicleHitPoints(vehicleId, true);
}

/*
Computes total damage vehicle can deliver in conventional offense.
*/
double getVehicleConventionalOffense(int vehicleId)
{
	return getVehicleConventionalOffenseStrength(vehicleId) * (double)getVehicleHitPoints(vehicleId, false);
}

/*
Computes total damage vehicle can deliver in conventional defense.
*/
double getVehicleConventionalDefense(int vehicleId)
{
	return getVehicleConventionalDefenseStrength(vehicleId) * (double)getVehicleHitPoints(vehicleId, false);
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

bool isCombatUnit(int id)
{
	assert(id >= 0);
	
	return Weapon[Units[id].weapon_type].offense_value != 0;
	
}

bool isCombatVehicle(int id)
{
	assert(id >= 0);
	
	return Weapon[Units[Vehicles[id].unit_id].weapon_type].offense_value != 0;
	
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
			getVehicleMoraleModifier(vehicleId, false)
			*
			getFactionSEPlanetOffenseModifier(vehicle->faction_id)
		)
		/
		(
			getVehicleMoraleModifier(enemyVehicleId, false)
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
			getVehicleMoraleModifier(vehicleId, false)
			*
			getFactionSEPlanetDefenseModifier(vehicle->faction_id)
		)
		/
		(
			getVehicleMoraleModifier(enemyVehicleId, false)
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
			getVehicleMoraleModifier(id, false)
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
			getVehicleMoraleModifier(id, false)
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

bool isSeaTransportVehicle(int vehicleId)
{
	return (Units[Vehicles[vehicleId].unit_id].weapon_type == WPN_TROOP_TRANSPORT && veh_triad(vehicleId) == TRIAD_SEA);
}

bool isVehicleProbe(VEH *vehicle)
{
	return (Units[vehicle->unit_id].weapon_type == WPN_PROBE_TEAM);
}

bool isVehicleIdle(int vehicleId)
{
	return (Vehicles[vehicleId].move_status == ORDER_NONE);
}

bool isLandArtilleryVehicle(int vehicleId)
{
	return (veh_triad(vehicleId) == TRIAD_LAND && isVehicleHasAbility(vehicleId, ABL_ARTILLERY));
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
Returns base tile region plus all ocean regions this base is connected to for coastal bases.
*/
std::set<int> getBaseConnectedRegions(int id)
{
	std::set<int> baseConnectedRegions;

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
std::set<int> getBaseConnectedOceanRegions(int baseId)
{
	BASE *base = &(Bases[baseId]);
	MAP *baseLocations = getBaseMapTile(baseId);

	std::set<int> baseConnectedOceanRegions;

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
Calculates base defense multiplier different factors taken into account.
If triad is non negative it defines which defensive structures to apply.
Otherwise, only base intrinsic defense is applied.
*/
double getBaseDefenseMultiplier(int id, int triad)
{
	assert(triad < 0 || (triad >= TRIAD_LAND && triad <= TRIAD_AIR));
	
	double baseDefenseMultiplier;
	
	if (triad < 0)
	{
		baseDefenseMultiplier = getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def);
	}
	else
	{
		bool firstLevelDefense = false;
		switch (triad)
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
			baseDefenseMultiplier = 2.0;

			if (firstLevelDefense)
			{
				baseDefenseMultiplier = conf.perimeter_defense_multiplier;
			}

			if (secondLevelDefense)
			{
				baseDefenseMultiplier += conf.tachyon_field_bonus;
			}

			baseDefenseMultiplier /= 2.0;

		}
		else
		{
			baseDefenseMultiplier = getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def);
		}
		
	}
	
	return baseDefenseMultiplier;

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
Estimates turns to complete given item assuming no switching loss.
*/
int estimateBaseProductionTurnsToCompleteItem(int baseId, int item)
{
	BASE *base = &(Bases[baseId]);
	
	int mineralCost = getBaseMineralCost(baseId, item);
	
	if (mineralCost <= base->minerals_accumulated)
		return 1;
	
	if (base->mineral_surplus <= 0)
		return 9999;
	
	return ((getBaseMineralCost(baseId, item) - base->minerals_accumulated) + (base->mineral_surplus - 1)) / base->mineral_surplus;

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

	for (int offsetIndex = 1; offsetIndex < BASE_OFFSET_COUNT_RADIUS; offsetIndex++)
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

std::vector<MAP *> getBaseWorkedTiles(BASE *base)
{
	std::vector<MAP *> baseWorkedTiles;

	for (int offsetIndex = 1; offsetIndex < BASE_OFFSET_COUNT_RADIUS; offsetIndex++)
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

	for (int offset = 0; offset < BASE_OFFSET_COUNT_RADIUS; offset++)
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
std::vector<int> getFactionUnitIds(int factionId, bool includeNotPrototyped)
{
	std::vector<int> factionUnitIds;

	// only real factions

	if (!(factionId >= 1 && factionId <= 7))
		return factionUnitIds;

	// iterate faction prototypes space

    for (int unitIndex = 0; unitIndex < 128; unitIndex++)
	{
        int unitId = (unitIndex < 64 ? unitIndex : (factionId - 1) * 64 + unitIndex);
        UNIT *unit = &(Units[unitId]);

		// skip not enabled

		if (unitId < 64 && !has_tech(factionId, unit->preq_tech))
			continue;

        // skip empty

        if (strlen(unit->name) == 0)
			continue;
		
		// skip obsolete
		
		if ((unit->obsolete_factions & (0x1 << factionId)) != 0)
			continue;

		// do not include not prototyped if not requested

		if (unitId >= 64 && !includeNotPrototyped && !(unit->unit_flags & UNIT_PROTOTYPED))
			continue;

		// add unit

		factionUnitIds.push_back(unitId);

	}

	return factionUnitIds;

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

int getProjectedBasePopulation(int baseId, int turns)
{
	BASE *base = &(Bases[baseId]);

	int nutrientCostFactor = cost_factor(base->faction_id, 0, baseId);
	int nutrientsAccumulated = base->nutrients_accumulated + base->nutrient_surplus * turns;
	int projectedBasePopulation = base->pop_size;

	while (nutrientsAccumulated >= nutrientCostFactor * (projectedBasePopulation + 1))
	{
		nutrientsAccumulated -= nutrientCostFactor * (projectedBasePopulation + 1);
		projectedBasePopulation++;
	}

	return projectedBasePopulation;

}

/*
Returns faction bases population limit until given facility is built.
Retruns 0 if given facility is not a population limit facility.
*/
int getFactionFacilityPopulationLimit(int factionId, int facilityId)
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
		return 0;
	}

	// faction penalty

    populationLimit -= MFaction->rule_population;

    // Ascetic Virtue

    if (has_project(factionId, FAC_ASCETIC_VIRTUES))
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

	nativeProtection *= getVehicleMoraleModifier(vehicleId, true);

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

	double protectionPotential = 1.0 / getPsiCombatBaseOdds(TRIAD_LAND) * getVehicleMoraleModifier(vehicleId, true) * getFactionSEPlanetDefenseModifier(vehicle->faction_id) * (1.0 + (double)Rules->combat_bonus_intrinsic_base_def / 100.0);

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

	int sePoliceRating = faction->SE_police_pending;
	
	if (has_facility(baseId, FAC_BROOD_PIT))
	{
		sePoliceRating = std::min(3, sePoliceRating + 2);
	}
	
	return sePoliceRating;
	
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
std::set<int> getAdjacentOceanRegions(int x, int y)
{
	debug("getAdjacentOceanRegions: x=%d, y=%d\n", x, y);

	std::set<int> adjacentOceanRegions;

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
std::set<int> getConnectedOceanRegions(int factionId, int x, int y)
{
	debug("getConnectedOceanRegions: factionId=%d, x=%d, y=%d\n", factionId, x, y);

	std::set<int> connectedOceanRegions;

	MAP *tile = getMapTile(x, y);

	if (tile == NULL)
		return connectedOceanRegions;

	// add adjacent tiles regions first

	std::set<int> adjacentOceanRegions = getAdjacentOceanRegions(x, y);
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

			std::set<int> baseAdjacentOceanRegions = getAdjacentOceanRegions(base->x, base->y);

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

std::vector<int> getTileVehicles(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	
	int vehicleAt = veh_at(x, y);
	
	if (vehicleAt != -1)
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

MAP_STATE getMapState(MAP *tile)
{
	return {tile->climate, tile->val3, tile->items};
}

void setMapState(MAP *tile, MAP_STATE *mapState)
{
	tile->climate = mapState->climate;
	tile->val3 = mapState->rocks;
	tile->items = mapState->items;

}

void copyMapState(MAP_STATE *destinationMapState, MAP_STATE *sourceMapState)
{
	destinationMapState->climate = sourceMapState->climate;
	destinationMapState->rocks = sourceMapState->rocks;
	destinationMapState->items = sourceMapState->items;

}

void generateTerraformingChange(MAP_STATE *mapState, int action)
{
	// level terrain changes rockiness
	if (action == FORMER_LEVEL_TERRAIN)
	{
		if (mapState->rocks & TILE_ROCKY)
		{
			mapState->rocks &= ~TILE_ROCKY;
			mapState->rocks |= TILE_ROLLING;
		}
		else if (mapState->rocks & TILE_ROLLING)
		{
			mapState->rocks &= ~TILE_ROLLING;
		}

	}
	// other actions change items
	else
	{
		// special cases
		if (action == FORMER_AQUIFER)
		{
			mapState->items |= (TERRA_RIVER_SRC | TERRA_RIVER);
		}
		// regular cases
		else
		{
			// remove items

			mapState->items &= ~Terraform[action].bit_incompatible;

			// add items

			mapState->items |= Terraform[action].bit;

		}

	}

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

bool isCoast(int x, int y)
{
	MAP *tile = getMapTile(x, y);

	if (is_ocean(tile))
		return false;

	for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
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

int getVehicleTransportId(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	if
	(
		// sentry
		vehicle->move_status == ORDER_SENTRY_BOARD
		&&
		// on sea transport
		vehicle->waypoint_1_y == 0
		&&
		vehicle->waypoint_1_x >= 0
	)
	{
		return vehicle->waypoint_1_x;
	}
	
	return -1;
		
}

int setMoveTo(int vehicleId, MAP *tile)
{
	return setMoveTo(vehicleId, getX(tile), getY(tile));
}
	
int setMoveTo(int vehicleId, int x, int y)
{
    VEH* vehicle = &(Vehicles[vehicleId]);
    
    debug("setMoveTo (%3d,%3d) -> (%3d,%3d)\n", vehicle->x, vehicle->y, x, y);
    
    vehicle->waypoint_1_x = x;
    vehicle->waypoint_1_y = y;
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
	
	return baseOdds * getMoraleModifier(morale);
	
}

double getMoraleModifier(int morale)
{
	return (1.0 + 0.125 * (morale - 2));
}

/*
Returns SE MORALE bonus.
*/
int getFactionSEMoraleBonus(int factionId, bool defendAtBase)
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
		factionSEMoraleBonus = (defendAtBase ? 2 : 1);
		break;
	case 3:
		factionSEMoraleBonus = (defendAtBase ? 3 : 2);
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
int getVehicleMorale(int vehicleId, bool defendAtBase)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// get basis vehicle morale
	
	int morale = vehicle->morale;
	
	// faction MORALE bonus for regular units
	
	if (!isNativeVehicle(vehicleId))
	{
		morale += getFactionSEMoraleBonus(vehicle->faction_id, defendAtBase);
	}
	
	// return morale limited by edge values

	return std::max((int)MORALE_VERY_GREEN, std::min((int)MORALE_ELITE, morale));

}

/*
Returns new vehicle morale built at base.
*/
int getNewVehicleMorale(int unitId, int baseId, bool defendAtBase)
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
		if (has_project(base->faction_id, FAC_XENOEMPATHY_DOME))
		{
			morale++;
		}
		
		if (has_project(base->faction_id, FAC_PHOLUS_MUTAGEN))
		{
			morale++;
		}
		
		if (has_project(base->faction_id, FAC_VOICE_OF_PLANET))
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
		morale += getFactionSEMoraleBonus(base->faction_id, defendAtBase);
	}
	
	// return morale limited by edge values

	return std::max((int)MORALE_VERY_GREEN, std::min((int)MORALE_ELITE, morale));

}

/*
Returns vehicle morale modifier.
*/
double getVehicleMoraleModifier(int vehicleId, bool defendAtBase)
{
	return getMoraleModifier(getVehicleMorale(vehicleId, defendAtBase));
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
		
		if (goody_at(getX(tile), getY(tile)) != 0)
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
double battleCompute(int attackerVehicleId, int defenderVehicleId)
{
	VEH *attackerVehicle = &(Vehicles[attackerVehicleId]);
	VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);
	MAP *defenderVehicleTile = getMapTile(defenderVehicleId);
	
	// set flags
	
	int flags = 0x00;
	
	if
	(
		attackerVehicle->triad() == TRIAD_LAND && can_arty(attackerVehicle->unit_id, 1)
		||
		attackerVehicle->triad() == TRIAD_SEA && can_arty(attackerVehicle->unit_id, 1) && defenderVehicle->triad() == TRIAD_LAND && !is_ocean(defenderVehicleTile)
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
	
	int attackerStrength;
	int defenderStrength;
	
	battle_compute(attackerVehicleId, defenderVehicleId, (int)&attackerStrength, (int)&defenderStrength, flags);
	
	// calculate relative strength
	
	double relativeStrength = (double)attackerStrength / (double)defenderStrength;
	
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
Return true if unit is a prototype.
*/
bool isPrototypeUnit(int unitId)
{
	return (unitId >= 64 && !(Units[unitId].unit_flags & UNIT_PROTOTYPED));
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
int getBaseIdInTile(int x, int y)
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
	return
		unitId == BSC_MIND_WORMS
		||
		unitId == BSC_ISLE_OF_THE_DEEP
		||
		unitId == BSC_LOCUSTS_OF_CHIRON
		||
		unitId == BSC_SPORE_LAUNCHER
		||
		unitId == BSC_SEALURK
		||
		unitId == BSC_FUNGAL_TOWER
	;

}

/*
Determines if vehicle is a native predefined unit.
*/
bool isNativeVehicle(int vehicleId)
{
	return isNativeUnit(Vehicles[vehicleId].unit_id);
}

double getPercentageBonusMultiplier(int percentageBonus)
{
	return 1.0 + (double)percentageBonus / 100.0;
}

int getPercentageBonusModifiedValue(int value, int percentageBonus)
{
	return value * (100 + percentageBonus) / 100;
}

int getPercentageSubstituteBonusModifiedValue(int value, int oldPercentageBonus, int newPercentageBonus)
{
	return value * (100 + newPercentageBonus) / (100 + oldPercentageBonus);
}

/*
Returns unit cost accounting for roughly 40 turns of maintenance.
*/
int getUnitMaintenanceCost(int unitId)
{
	return Units[unitId].cost + 4;
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

int countPodsInBaseRadius(int x, int y)
{
	int podCount = 0;
	
	for (MAP *tile : getBaseRadiusTiles(x, y, true))
	{
		if ((goody_at(getX(tile), getY(tile)) != 0))
		{
			podCount++;
		}
	}
	
	return podCount;
	
}

/*
Checks if tile is in ZOC.
*/
bool isZoc(int factionId, int x, int y)
{
	MAP *tile = getMapTile(x, y);
	
	if (tile == NULL)
		return false;
	
	// no ZOC on ocean
	
	if (is_ocean(tile))
		return false;
	
	// search for unfriendly unit in adjacent tiles
	
	for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
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
double getBattleOdds(int attackerVehicleId, int defenderVehicleId)
{
	VEH *attackerVehicle = &(Vehicles[attackerVehicleId]);
	VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);
	
	// calculate relative strength
	
	double relativeStrength = battleCompute(attackerVehicleId, defenderVehicleId);
	
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

/*
Calculates average attacker damage for destroying defender taking damage and reactor into account.
*/
double getAttackerDamage(int attackerVehicleId, int defenderVehicleId)
{
	VEH *attackerVehicle = &(Vehicles[attackerVehicleId]);
	VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);
	
	// calculate relative strength
	
	double relativeStrength = battleCompute(attackerVehicleId, defenderVehicleId);
	
	// check whether battle ignores reactor
	
	bool ignoreReactor =
		Weapon[attackerVehicle->weapon_type()].offense_value < 0 || Armor[defenderVehicle->armor_type()].defense_value < 0
		||
		conf.ignore_reactor_power_in_combat
	;
	
	// calculate attacker and defender HP
	
//	int attackerHP = 10 * (ignoreReactor ? 1 : attackerVehicle->reactor_type()) - attackerVehicle->damage_taken / (ignoreReactor ? attackerVehicle->reactor_type() : 1);
	int defenderHP = 10 * (ignoreReactor ? 1 : defenderVehicle->reactor_type()) - defenderVehicle->damage_taken / (ignoreReactor ? defenderVehicle->reactor_type() : 1);
	
	// calculate attacker lost HP
	
	double attackerLostHP = (double)defenderHP / relativeStrength;
	
	// calculate damage
	
	double attackerDamage = attackerLostHP * (double)(ignoreReactor ? attackerVehicle->reactor_type() : 1);
	
	return attackerDamage;
	
}

/*
Calculates average attacker damage for destroying whole stack.
*/
double getAttackerStackDamage(int attackerVehicleId, int defenderVehicleId)
{
	double attackerStackDamage = 0.0;
	
	for (int defenderStackVehicleId : getStackVehicles(defenderVehicleId))
	{
		attackerStackDamage += getAttackerDamage(attackerVehicleId, defenderStackVehicleId);
	}
	
	return attackerStackDamage;
	
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
double getAlienMoraleModifier()
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
	
	return getMoraleModifier(morale);
	
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

int getRangeToNearestFactionBase(int x, int y, int factionId)
{
	int minRange = INT_MAX;
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		
		// faction base
		
		if (base->faction_id != factionId)
			continue;
		
		// calculate range
		
		int range = map_range(x, y, base->x, base->y);
		
		// update min range
		
		minRange = std::min(minRange, range);
		
	}
	
	return minRange;
	
}

int getRangeToNearestFactionColony(int x, int y, int factionId)
{
	int minRange = INT_MAX;
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// faction vehicle
		
		if (vehicle->faction_id != factionId)
			continue;
		
		// colony
		
		if (!isColonyVehicle(vehicleId))
			continue;
		
		// calculate range
		
		int range = map_range(x, y, vehicle->x, vehicle->y);
		
		// update min range
		
		minRange = std::min(minRange, range);
		
	}
	
	return minRange;
	
}

/*
Kills vehicle properly with transport check.
*/
void killVehicle(int vehicleId)
{
	tx_kill(vehicleId);
}

void board(int vehicleId, int transportVehicleId)
{
	VEH* vehicle = &(Vehicles[vehicleId]);
	VEH* transportVehicle = &(Vehicles[transportVehicleId]);
	
	// cannot board on itself
	
	if (vehicleId == transportVehicleId)
		return;
	
	// board at same location
	
	if (!(vehicle->x == transportVehicle->x && vehicle->y == transportVehicle->y))
		return;
	
	// do not board on transport with no remaining capacity
	
	if (getTransportRemainingCapacity(transportVehicleId) <= 0)
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

int getVehicleHitPoints(int vehicleId, bool psiCombat)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	int power = getVehiclePower(vehicleId);
	
	return (psiCombat || conf.ignore_reactor_power_in_combat ? power : (power + (vehicle->reactor_type() - 1)) / vehicle->reactor_type());
	
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

void accumulateMapIntValue(std::map<int, int> *m, int key, int value)
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

int getHexCost(int unitId, int factionId, int fromX, int fromY, int toX, int toY)
{
	return mod_hex_cost(unitId, factionId, fromX, fromY, toX, toY, (unit_speed(unitId) == Rules->mov_rate_along_roads ? 1 : 0));
}

/*
Returns true if two factions are at war.
Also account for natives are always at war with other factions.
*/
bool isWar(int factionId1, int factionId2)
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
Returns faction unit slot number.
*/
int getUnitSlotByUnitId(int unitId)
{
	assert(unitId >= 0 && unitId < MaxProtoNum);
	return (unitId < MaxProtoFactionNum ? unitId : MaxProtoFactionNum + unitId / MaxProtoFactionNum);
}

/*
Returns unitId by faction slot number.
*/
int getUnitIdByUnitSlot(int factionId, int unitSlot)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	assert(unitSlot >= 0 && unitSlot < 2 * MaxProtoFactionNum);
	assert(factionId != 0 || unitSlot < MaxProtoFactionNum);
	return (unitSlot < MaxProtoFactionNum ? unitSlot : MaxProtoFactionNum * (factionId - 1) + unitSlot);
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
	
	int doctors = getDoctors(baseId);
	
	// calculate potential number of drones can be quelled with police
	
	int policeDrones = quelledDrones + leftDrones + doctors;
	
	return policeDrones;
	
}

/*
Returns true if reactor is ignored in conventional combat.
*/
bool isReactorIgnored()
{
	return (conf.ignore_reactor_power || conf.ignore_reactor_power_in_combat);
}

bool isInfantryPolice2xVehicle(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);
	bool wormPolice = isFactionSpecial(vehicle->faction_id, FACT_WORMPOLICE);
	
	return unit->chassis_type == CHS_INFANTRY && (isVehicleHasAbility(vehicleId, ABL_POLICE_2X) || (wormPolice && vehicle->unit_id == BSC_MIND_WORMS));
	
}

bool isInfantryDefensiveVehicle(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);
	int weaponOffenseValue = Weapon[unit->weapon_type].offense_value;
	int armorDefenseValue = Armor[unit->armor_type].defense_value;
	
	return unit->chassis_type == CHS_INFANTRY && (weaponOffenseValue <= (armorDefenseValue + 1) / 2);
	
}

int getUnitIndex(int unitId)
{
	return (unitId < MaxProtoFactionNum ? unitId : MaxProtoFactionNum + unitId % MaxProtoFactionNum);
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

