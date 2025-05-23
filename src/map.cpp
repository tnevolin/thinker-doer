
#include "map.h"


static Points spawns;
static Points natives;
static Points goodtiles;


MAP* mapsq(int x, int y) {
    if (x >= 0 && y >= 0 && x < *MapAreaX && y < *MapAreaY && !((x + y)&1)) {
        return &((*MapTiles)[ (x + *MapAreaX * y)/2 ]);
    } else {
        return NULL;
    }
}

int wrap(int x) {
    if (!map_is_flat()) {
        if (x >= 0) {
            if (x >= *MapAreaX) {
                x -= *MapAreaX;
            }
        } else {
            x += *MapAreaX;
        }
    }
    return x;
}

int map_range(int x1, int y1, int x2, int y2) {
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    if (!map_is_flat() && dx > *MapHalfX) {
        dx = *MapAreaX - dx;
    }
    return (dx + dy)/2;
}

int vector_dist(int x1, int y1, int x2, int y2) {
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    if (!map_is_flat() && dx > *MapHalfX) {
        dx = *MapAreaX - dx;
    }
    return max(dx, dy) - ((((dx + dy) / 2) - min(dx, dy) + 1) / 2);
}

int min_range(const Points& S, int x, int y) {
    int z = MaxMapDist;
    for (auto& p : S) {
        z = min(z, map_range(x, y, p.x, p.y));
    }
    return z;
}

int min_vector(const Points& S, int x, int y) {
    int z = MaxMapDist;
    for (auto& p : S) {
        z = min(z, vector_dist(x, y, p.x, p.y));
    }
    return z;
}

double avg_range(const Points& S, int x, int y) {
    int n = 0;
    int sum = 0;
    for (auto& p : S) {
        sum += map_range(x, y, p.x, p.y);
        n++;
    }
    return (n > 0 ? (1.0*sum)/n : 0);
}

bool is_ocean(MAP* sq) {
    return (!sq || (sq->climate >> 5) < ALT_SHORE_LINE);
}

bool is_ocean(BASE* base) {
    return is_ocean(mapsq(base->x, base->y));
}

bool is_ocean_shelf(MAP* sq) {
    return (sq && (sq->climate >> 5) == ALT_OCEAN_SHELF);
}

bool is_shore_level(MAP* sq) {
    return (sq && (sq->climate >> 5) == ALT_SHORE_LINE);
}

bool map_is_flat() {
    return *MapToggleFlat & 1;
}

bool adjacent_region(int x, int y, int owner, int threshold, bool ocean) {
    assert(mapsq(x, y));
    for (auto& m : iterate_tiles(x, y, 1, 9)) {
        if (is_ocean(m.sq) != ocean) {
            continue;
        }
        if ((owner < 0 || m.sq->owner < 0 || m.sq->owner == owner) && !bad_reg(m.sq->region)
        && Continents[m.sq->region].tile_count >= threshold) {
            return true;
        }
    }
    return false;
}

int clear_overlay(int UNUSED(x), int UNUSED(y)) {
    return 0;
}

void refresh_overlay(std::function<int(int, int)> tile_value) {
    if (*GameState & STATE_OMNISCIENT_VIEW && MapWin->iWhatToDrawFlags & MAPWIN_DRAW_GOALS) {
        for (int y = 0; y < *MapAreaY; y++) {
            for (int x = y&1; x < *MapAreaX; x+=2) {
                mapdata[{x, y}].overlay = tile_value(x, y);
            }
        }
        draw_map(1);
    }
}

int __cdecl is_coast(int x, int y, bool is_base_radius) {
    int radius = is_base_radius ? 21 : 9;
    for (int i = 1; i < radius; i++) {
        int x2 = wrap(x + TableOffsetX[i]);
        int y2 = y + TableOffsetY[i];
        MAP* sq = mapsq(x2, y2);
        if (sq && is_ocean(sq)) {
            return true;
        }
    }
    return false;
}

int __cdecl is_port(int base_id, bool is_base_radius) {
    return is_coast(Bases[base_id].x, Bases[base_id].y, is_base_radius);
}

/*
Validate region bounds. Bad regions include: 0, 63, 64, 127, 128.
*/
int __cdecl bad_reg(int region) {
    return (region & RegionBounds) == RegionBounds || !(region & RegionBounds);
}

int __cdecl region_at(int x, int y) {
    MAP* sq = mapsq(x, y);
    return sq ? sq->region : 0;
}

int __cdecl base_at(int x, int y) {
    MAP* sq = mapsq(x, y);
    if (sq && sq->is_base()) {
        if (*BaseCount <= 0) {
            // Removed unnecessary debug code from here
            rebuild_base_bits();
            return -1;
        }
        for (int i = 0; i < *BaseCount; ++i) {
            if (Bases[i].x == x && Bases[i].y == y) {
                return i;
            }
        }
        assert(0);
    }
    return -1;
}

int __cdecl x_dist(int x1, int x2) {
    int dist = abs(x1 - x2);
    if (!map_is_flat() && dist > *MapHalfX) {
        dist = *MapAreaX - dist;
    }
    return dist;
}

uint32_t __cdecl bit_at(int x, int y) {
    MAP* sq = mapsq(x, y);
    return sq ? sq->items : 0;
}

void __cdecl bit_put(int x, int y, uint32_t items) {
    MAP* sq = mapsq(x, y);
    sq->items = items;
}

void __cdecl bit_set(int x, int y, uint32_t items, bool add) {
    MAP* sq = mapsq(x, y);
    if (add) {
        sq->items |= items;
    } else {
        sq->items &= ~items;
    }
}

uint32_t __cdecl bit2_at(int x, int y) {
    MAP* sq = mapsq(x, y);
    return (sq ? *((uint32_t*)&sq->landmarks) : 0);
}

void __cdecl bit2_set(int x, int y, uint32_t items, bool add) {
    MAP* sq = mapsq(x, y);
    if (add) {
        *((uint32_t*)&sq->landmarks) |= items;
    } else {
        *((uint32_t*)&sq->landmarks) &= ~items;
    }
}

uint32_t __cdecl code_at(int x, int y) {
    MAP* sq = mapsq(x, y);
    return (sq ? sq->art_ref_id : 0);
}

void __cdecl code_set(int x, int y, int code) {
    MAP* sq = mapsq(x, y);
    if (sq) {
        sq->art_ref_id = code;
        *GameDrawState |= 4;
    }
}

/*
First Manifold Nexus tile must also be visible to the owner for the effect to take place.
*/
int __cdecl has_temple(int faction_id) {
    for (int y = 0; y < *MapAreaY; y++) {
        for (int x = y&1; x < *MapAreaX; x += 2) {
            MAP* sq = mapsq(x, y);
            if (sq && sq->landmarks & LM_NEXUS
            && sq->art_ref_id == 0 && sq->owner == faction_id
            && sq->visibility & (1 << faction_id)) {
                return true;
            }
        }
    }
    return false;
}

int __cdecl near_landmark(int x, int y) {
    for (int i = 0; i < TableRange[8]; i++) {
        int x2 = wrap(x + TableOffsetX[i]);
        int y2 = y + TableOffsetY[i];
        if (code_at(x2, y2)) {
            return true;
        }
    }
    return false;
}

/*
Reset the map to a blank state. Original doesn't wipe unk_1 and owner fields.
This is simplified by zeroing all fields first and then setting specific fields.
*/
void __cdecl mod_map_wipe() {
    *MapSeaLevel = 0;
    *MapSeaLevelCouncil = 0;
    *MapLandmarkCount = 0;
    *MapRandomSeed = random(0x7FFF) + 1;
    memset(*MapTiles, 0, *MapAreaTiles * sizeof(MAP));
    for (int i = 0; i < *MapAreaTiles; i++) {
        (*MapTiles)[i].climate = 0x20;
        (*MapTiles)[i].contour = 20;
        (*MapTiles)[i].val2 = 0xF;
        (*MapTiles)[i].owner = -1;
    }
    reset_state();
    mapdata.clear();
    mapnodes.clear();
}

int __cdecl resource_yield(BaseResType type, int faction_id, int base_id, int x, int y) {
    if (type == RSC_NUTRIENT) {
        return mod_crop_yield(faction_id, base_id, x, y, 0);
    } else if (type == RSC_MINERAL) {
        return mod_mine_yield(faction_id, base_id, x, y, 0);
    } else if (type == RSC_ENERGY) {
        return mod_energy_yield(faction_id, base_id, x, y, 0);
    } else if (type == RSC_UNUSED) {
        *BaseTerraformReduce = 0;
    }
    return 0;
}

int __cdecl mod_crop_yield(int faction_id, int base_id, int x, int y, int flag) {
    MAP* sq = mapsq(x, y);
    if (!sq) {
        return 0;
    }
    int value = 0;
    int alt = sq->alt_level();
    bool bonus_landmark = false;
    bool bonus_nutrient = bonus_at(x, y) == RES_NUTRIENT;
    bool is_base = sq->is_base();
    int planet = Factions[faction_id].SE_planet_pending;

    if ((alt >= ALT_SHORE_LINE && sq->landmarks & LM_JUNGLE && !(sq->art_ref_id & 0x80))
    || (alt < ALT_SHORE_LINE && sq->landmarks & LM_FRESH)) {
        bonus_landmark = true;
    }
    if (is_base) {
        value = ResInfo->base_sq_nutrient;
        if (bonus_nutrient )
            value = 2 * ResInfo->base_sq_nutrient;
        if (bonus_landmark) {
            value++;
        }
        if (has_fac_built(FAC_PRESSURE_DOME, base_id)
        || has_fac_built(FAC_RECYCLING_TANKS, base_id)) {
            value += ResInfo->recycling_tanks_energy;
        }
		// [WTP]
		// Recycling Tanks population bonus
		if (conf.recycling_tanks_population_bonus && has_fac_built(FAC_RECYCLING_TANKS, base_id))
		{
			// every second out of three citizens contributes nutrient
			value += (Bases[base_id].pop_size + 1) / 3;
        }
    }
    else if (sq->items & BIT_THERMAL_BORE) {
        value = ResInfo->borehole_sq_nutrient;
        if (bonus_nutrient) {
            value += ResInfo->bonus_sq_nutrient;
        }
    }
    else if (sq->items & BIT_MONOLITH) {
        value = ResInfo->monolith_nutrient;
        if (bonus_nutrient) {
            value += ResInfo->bonus_sq_nutrient;
        }
        if (has_project(FAC_MANIFOLD_HARMONICS, faction_id)) {
			// [WTP]
			// shift Manifold Harmonics minimal planet rating
            value += ManifoldHarmonicsBonus[clamp(planet - conf.manifold_harmonics_min_planet_rating + 1, 0, 4)][0];
        }
    }
    else if (sq->items & BIT_FUNGUS && !flag && alt >= ALT_OCEAN_SHELF) {
        int fungus_val = clamp(planet, -3, 0) + Factions[faction_id].tech_fungus_nutrient;
        value = clamp(fungus_val, 0, 99);
        if (has_project(FAC_MANIFOLD_HARMONICS, faction_id)) {
			// [WTP]
			// shift Manifold Harmonics minimal planet rating
            value += ManifoldHarmonicsBonus[clamp(planet - conf.manifold_harmonics_min_planet_rating + 1, 0, 4)][0];
        }
		
		// [WTP]
		// Biology Lab fungus bonus
		
		if (base_id >=0 && has_facility(FAC_BIOLOGY_LAB, base_id)) {
			value += conf.facility_yield_bonus_biology_lab[0];
		}
		
		// [WTP]
		// fungus receives terrain bonuses
		
		if (conf.fungus_terrain_bonus)
		{
			if (bonus_nutrient) {
				value += ResInfo->bonus_sq_nutrient;
			}
            if (bonus_landmark)
                value++;
		}
		
    } else {
        if (alt < ALT_SHORE_LINE) {
            value = ResInfo->ocean_sq_nutrient;
            if (bonus_nutrient) {
                value = ResInfo->bonus_sq_nutrient + ResInfo->ocean_sq_nutrient;
            }
            if (bonus_landmark)
                value++;
            if (alt == ALT_OCEAN_SHELF || *ExpansionEnabled) {
                if (sq->items & BIT_FARM) {
                    value += ResInfo->improved_sea_nutrient;
                    if (has_fac_built(FAC_AQUAFARM, base_id)) {
                        value++;
                    }
                }
                if (sq->items & BIT_MINE && value > 1) {
                    value += Rules->nutrient_effect_mine_sq;
                }
            }
        } else {
            if (sq->items & BIT_FOREST) {
                value = ResInfo->forest_sq_nutrient
                    + (bonus_nutrient ? ResInfo->bonus_sq_nutrient : 0);
                if (has_fac_built(FAC_TREE_FARM, base_id)) {
                    value += conf.tree_farm_yield_bonus_forest[0];
                }
                if (has_fac_built(FAC_HYBRID_FOREST, base_id)) {
                    value += conf.hybrid_forest_yield_bonus_forest[0];
                }
                
                if (bonus_landmark) {
                    value++;
                }
            } else {
                if (sq->is_rocky()) {
                    value = 0;
                } else {
                    value = (sq->is_rainy() ? 2 : (sq->is_moist() ? 1 : 0));
                }
                if (bonus_nutrient) {
                    value += ResInfo->bonus_sq_nutrient;
                }
                if (bonus_landmark) {
                    value++;
                }
                if (value < 0) {
                    value = 0;
                }
                if (sq->items & BIT_FARM && !sq->is_rocky()) {
                    value += ResInfo->improved_land_nutrient;
                }
                if (sq->items & BIT_MINE && value > 1) {
                    value += Rules->nutrient_effect_mine_sq;
                }
            }
        }
        if (sq->items & BIT_SOIL_ENRICHER) {
			
			// [WTP]
			
			if (conf.condenser_and_enricher_do_not_multiply_nutrients)
			{
				// enricher does not multiply nutrients and instead adds 1
				value += 1;
			}
			else
			{
				// vanilla default: 50% increase
				value += value / 2;
			}
			
        }
        if (sq->items & BIT_CONDENSER) {
			
			// [WTP]
			
			if (conf.condenser_and_enricher_do_not_multiply_nutrients)
			{
				// condenser does not multiply nutrients
			}
			else
			{
				// vanilla default: 50% increase
				value += value / 2;
			}
			
        }
        if (value > 2 && !bonus_nutrient && !(sq->items & BIT_CONDENSER)
        && (faction_id < 0 || !has_tech(Rules->tech_preq_allow_3_nutrients_sq, faction_id))) {
            *BaseTerraformReduce += (value - 2);
            value = 2;
        }
    }
    if (base_id >= 0) {
        if (Bases[base_id].event_flags & BEVENT_BUMPER) {
            value++;
        }
        if (Bases[base_id].event_flags & BEVENT_FAMINE && value) {
            value--;
        }
    }
    
    // [WTP]
    // not like vanilla anymore
//    assert(value == crop_yield(faction_id, base_id, x, y, flag));

    return value;
}

int __cdecl mod_mine_yield(int faction_id, int base_id, int x, int y, int flag) {
    MAP* sq = mapsq(x, y);
    if (!sq) {
        return 0;
    }
    bool has_limit = true;
    bool bonus_landmark = false;
    bool bonus_mineral = bonus_at(x, y) == RES_MINERAL;
    bool is_base = sq->is_base();
    int alt = sq->alt_level();
    int planet = Factions[faction_id].SE_planet_pending;

    if ((sq->landmarks & LM_CRATER && sq->art_ref_id < 9)
    || (sq->landmarks & LM_VOLCANO && sq->art_ref_id < 9)
    || (sq->landmarks & LM_FOSSIL && sq->art_ref_id < 6)
    || (sq->landmarks & LM_CANYON)) {
        bonus_landmark = !(sq->art_ref_id & 0x80);
    }
    int value = bonus_landmark + (bonus_mineral ? ResInfo->bonus_sq_mineral : 0);

    if (is_base) {
        value += ResInfo->base_sq_mineral;
        if (has_fac_built(FAC_PRESSURE_DOME, base_id)
        || has_fac_built(FAC_RECYCLING_TANKS, base_id)) {
            value += ResInfo->recycling_tanks_mineral;
        }
		// [WTP]
		// Recycling Tanks population bonus
		if (conf.recycling_tanks_population_bonus && has_fac_built(FAC_RECYCLING_TANKS, base_id))
		{
			// every third out of three citizens contributes mineral
			value += (Bases[base_id].pop_size + 0) / 3;
        }
        has_limit = false;
    } else {
        if (sq->items & BIT_MONOLITH) {
            value = ResInfo->monolith_mineral;
            if (bonus_mineral) {
                value += ResInfo->bonus_sq_mineral;
            }
            if (has_project(FAC_MANIFOLD_HARMONICS, faction_id)) {
				// [WTP]
				// shift Manifold Harmonics minimal planet rating
				value += ManifoldHarmonicsBonus[clamp(planet - conf.manifold_harmonics_min_planet_rating + 1, 0, 4)][1];
            }
            has_limit = false;
        }
        else if (sq->items & BIT_THERMAL_BORE) {
            value = ResInfo->borehole_sq_mineral;
            if (bonus_mineral) {
                value += ResInfo->bonus_sq_mineral;
            }
        }
        else if (sq->items & BIT_FUNGUS && alt >= ALT_OCEAN_SHELF) {
            int fungus_val = clamp(planet, -3, 0) + Factions[faction_id].tech_fungus_mineral;
            value = clamp(fungus_val, 0, 99);
            if (has_project(FAC_MANIFOLD_HARMONICS, faction_id)) {
				// [WTP]
				// shift Manifold Harmonics minimal planet rating
				value += ManifoldHarmonicsBonus[clamp(planet - conf.manifold_harmonics_min_planet_rating + 1, 0, 4)][1];
            }
            has_limit = false;
			
			// [WTP]
			// Biology Lab fungus bonus
			
			if (base_id >= 0 && has_facility(FAC_BIOLOGY_LAB, base_id)) {
				value += conf.facility_yield_bonus_biology_lab[1];
			}
			
			// [WTP]
			// fungus receives terrain bonuses
			
			if (conf.fungus_terrain_bonus)
			{
				if (bonus_mineral) {
					value += ResInfo->bonus_sq_mineral;
				}
			}
			
        }
        else if (alt >= ALT_SHORE_LINE) {
            int modifier = sq->val3 >> 6;
            if (sq->items & BIT_FOREST) {
                value += ResInfo->forest_sq_mineral;
                
				// [WTP]
				// facilities custom bonus
				
                if (has_fac_built(FAC_TREE_FARM, base_id)) {
                    value += conf.tree_farm_yield_bonus_forest[1];
                }
                if (has_fac_built(FAC_HYBRID_FOREST, base_id)) {
                    value += conf.hybrid_forest_yield_bonus_forest[1];
                }
                
            } else if (!(sq->items & BIT_MINE) && !flag) {
                value += (modifier > 0);
            } else {
                value += modifier;
                if (!modifier) {
                    modifier = 1;
                }
                if (bonus_mineral || bonus_landmark) {
                    modifier++;
                }
                if (!(sq->items & BIT_ROAD) && !flag
                && modifier > Rules->limit_mineral_inc_for_mine_wo_road) {
                    *BaseTerraformReduce += (modifier - Rules->limit_mineral_inc_for_mine_wo_road);
                    modifier = Rules->limit_mineral_inc_for_mine_wo_road;
                }
                value += modifier;
                
                // [WTP]
                // Recycling Tank mineral bonus
                
                if (base_id >=0 && has_facility(FAC_RECYCLING_TANKS, base_id))
				{
					if (sq->is_rocky())
					{
						value += conf.recycling_tanks_mineral_bonus_rocky_mine;
					}
					else
					{
						value += conf.recycling_tanks_mineral_bonus_regular_mine;
					}
				}
				
                if (base_id >=0 && has_facility(FAC_GENEJACK_FACTORY, base_id) && sq->is_rocky())
				{
					value += conf.genejack_factory_mineral_bonus_rocky_mine;
				}
				
            }
        }
        else {
            value += ResInfo->ocean_sq_mineral;
            if (alt == ALT_OCEAN_SHELF && MFactions[faction_id].is_aquatic()
            && *ExpansionEnabled && conf.aquatic_bonus_minerals) {
                value++;
            }
            if (alt == ALT_OCEAN_SHELF || *ExpansionEnabled) {
                if (sq->items & BIT_MINE || flag) {
                    value += ResInfo->improved_sea_mineral;
                    if (has_tech(Rules->tech_preq_mining_platform_bonus, faction_id) ) {
                        value++;
                    }
                    if (has_fac_built(FAC_SUBSEA_TRUNKLINE, base_id)) {
                        value++;
                    }
                    
					// [WTP]
					// facility bonus (mining platform)
					
					if (base_id >=0 && has_facility(FAC_RECYCLING_TANKS, base_id))
					{
						value += conf.recycling_tanks_mineral_bonus_mining_platform;
					}
					
					if (base_id >=0 && has_facility(FAC_GENEJACK_FACTORY, base_id))
					{
						value += conf.genejack_factory_mineral_bonus_mining_platform;
					}
					
                }
            }
        }
    }
    if (has_limit && value > 2 && !bonus_mineral
    && !has_tech(Rules->tech_preq_allow_3_minerals_sq, faction_id)) {
        *BaseTerraformReduce += (value - 2);
        value = 2;
    }
    if (base_id >= 0) {
        if (Bases[base_id].event_flags & BEVENT_INDUSTRY) {
            value++;
        }
        if (Bases[base_id].event_flags & BEVENT_BUST && value) {
            value--;
        }
    }
    
    // [WTP]
    // assertion is incorrect for aquatic faction base
//    // Original function can return inconsistent sea mineral output when base_id is not set
//    assert((base_id < 0 && alt == ALT_OCEAN_SHELF && MFactions[faction_id].is_aquatic())
//        || (value == mine_yield(faction_id, base_id, x, y, flag)));
	
	// [WTP]
	// Pressure Dome minerals
	
    if (is_base && base_id >= 0 && has_fac_built(FAC_PRESSURE_DOME, base_id))
	{
		value += conf.pressure_dome_minerals;
	}
	
    return value;
}

int __cdecl mod_energy_yield(int faction_id, int base_id, int x, int y, int flag) {
    MAP* sq = mapsq(x, y);
    if (!sq) {
        return 0;
    }
    bool is_fungus = false;
    bool has_limit = true;
    bool bonus_energy = bonus_at(x, y) == RES_ENERGY;
    bool is_base = sq->is_base();
    int economy = Factions[faction_id].SE_economy_pending
        + (base_id >= 0 && Bases[base_id].golden_age_active() ? 1 : 0);
    int planet = Factions[faction_id].SE_planet_pending;
    int alt = sq->alt_level();
    int value = 0;

    if (is_base) {
        bool is_hq = has_fac_built(FAC_HEADQUARTERS, base_id);
        if (has_fac_built(FAC_PRESSURE_DOME, base_id)
        || has_fac_built(FAC_RECYCLING_TANKS, base_id)) {
            value += ResInfo->recycling_tanks_energy;
        }
		// [WTP]
		// Recycling Tanks population bonus
		if (conf.recycling_tanks_population_bonus && has_fac_built(FAC_RECYCLING_TANKS, base_id))
		{
			// every first out of three citizens contributes nutrient
			value += (Bases[base_id].pop_size + 2) / 3;
        }
        if (economy > 4) {
            value += 4;
        } else if (economy > 3) {
            value += 2;
        } else if (economy >= 0) {
            value += (economy == 1); // +1 each square is added later
        } else {
            value += economy;
            if (!is_hq || economy < -1 || Factions[faction_id].base_count == 1) {
                value++;
            }
        }
        value += 1 + is_hq;
        if (*GovernorFaction == faction_id) {
            value++;
        }
        has_limit = false;
    }
    // Monolith takes priority over any fungus values on the same tile.
    // Modify the game to not apply 2 resource yield restrictions on monolith energy.
    // This limit does not apply on nutrients/minerals produced by monoliths.
    else if (sq->items & BIT_MONOLITH) {
        value = ResInfo->monolith_energy;
        if (has_project(FAC_MANIFOLD_HARMONICS, faction_id)) {
			// [WTP]
			// shift Manifold Harmonics minimal planet rating
			value += ManifoldHarmonicsBonus[clamp(planet - conf.manifold_harmonics_min_planet_rating + 1, 0, 4)][2];
        }
        has_limit = false;
    }
    else if (sq->items & BIT_THERMAL_BORE) {
        value = ResInfo->borehole_sq_energy;
    }
    else if (sq->items & BIT_FUNGUS && alt >= ALT_OCEAN_SHELF) {
        int fungus_val = clamp(planet, -3, 0) + Factions[faction_id].tech_fungus_energy;
        value = clamp(fungus_val, 0, 99);
        if (has_project(FAC_MANIFOLD_HARMONICS, faction_id)) {
			// [WTP]
			// shift Manifold Harmonics minimal planet rating
			value += ManifoldHarmonicsBonus[clamp(planet - conf.manifold_harmonics_min_planet_rating + 1, 0, 4)][2];
        }
        is_fungus = true;
        has_limit = false;
		
		// [WTP]
		// Biology Lab fungus bonus
		
		if (base_id >= 0 && has_facility(FAC_BIOLOGY_LAB, base_id)) {
			value += conf.facility_yield_bonus_biology_lab[2];
		}
		
    }
    else if (alt < ALT_SHORE_LINE) {
        if (alt == ALT_OCEAN_SHELF || *ExpansionEnabled) {
            value = ResInfo->ocean_sq_energy;
            if (sq->items & BIT_SOLAR || flag) {
                value += ResInfo->improved_sea_energy;
                if (has_fac_built(FAC_THERMOCLINE_TRANSDUCER, base_id)) {
                    value++;
                }
            }
        }
        if (sq->items & BIT_FOREST) {
            value--;
        }
    }
    else if (sq->items & BIT_FOREST) {
        value = ResInfo->forest_sq_energy;
                
		// [WTP]
		// facilities custom bonus
		
//        if (has_fac_built(FAC_HYBRID_FOREST, base_id)) {
//            value++;
//        }
		if (has_fac_built(FAC_TREE_FARM, base_id)) {
			value += conf.tree_farm_yield_bonus_forest[2];
		}
		if (has_fac_built(FAC_HYBRID_FOREST, base_id)) {
			value += conf.hybrid_forest_yield_bonus_forest[2];
		}
		
    }
    else {
        if (sq->items & (BIT_ECH_MIRROR|BIT_SOLAR) || flag) {
            value += 1 + max(0, alt - ALT_SHORE_LINE);
        }
        // Fix crash issue by adding null pointer check for CurrentBase
        if (sq->items & BIT_SOLAR && *CurrentBase) {
			
			// [WTP]
			// custom Echelon Mirror bonus
			
//            for (auto& m : iterate_tiles(x, y, 1, 9)) {
//                if (m.sq->items & BIT_ECH_MIRROR
//                && m.sq->owner == (*CurrentBase)->faction_id) {
//                    value++;
//                }
//            }
			int echelonMirrorBonus = 0;
            for (auto& m : iterate_tiles(x, y, 1, 9)) {
                if (m.sq->items & BIT_ECH_MIRROR
                && m.sq->owner == (*CurrentBase)->faction_id) {
                    echelonMirrorBonus++;
                }
            }
            echelonMirrorBonus = echelonMirrorBonus * conf.echelon_mirror_bonus[0] / conf.echelon_mirror_bonus[1];
            value += echelonMirrorBonus;
			
        }
    }
    // [WTP]
    // fungus receives all bonuses
    if (!is_fungus || conf.fungus_terrain_bonus) {
        if (sq->items & BIT_RIVER && alt >= ALT_SHORE_LINE) {
            value++;
        }
        if (bonus_energy) {
            value += ResInfo->bonus_sq_energy;
        }
        if ((sq->landmarks & LM_VOLCANO && sq->art_ref_id < 9)
        || sq->landmarks & (LM_URANIUM|LM_GEOTHERMAL|LM_RIDGE)) {
            value += !(sq->art_ref_id & 0x80);
        }
        if (base_id >= 0 && project_base(FAC_MERCHANT_EXCHANGE) == base_id) {
            value++;
        }
        if (economy >= 2) {
            value++;
        }
        if (is_base) {
            value = clamp(value, 0, 99);
            *BaseTerraformEnergy = value;
        }
    }
    if (has_limit && value > 2 && !bonus_energy
    && !has_tech(Rules->tech_preq_allow_3_energy_sq, faction_id)) {
        value = 2;
        *BaseTerraformReduce += (value - 2);
    }
    if (base_id >= 0) {
        if (Bases[base_id].event_flags & BEVENT_HEAT_WAVE) {
            value++;
        }
        if (Bases[base_id].event_flags & BEVENT_CLOUD_COVER && value) {
            value--;
        }
    }
    if (*DustCloudDuration && value) {
        value--;
    }
    bool solar_flares;
    if (*ControlUpkeepA) {
        solar_flares = *SolarFlaresEvent & 2;
    } else {
        solar_flares = *SolarFlaresEvent & 1;
    }
    value = (solar_flares ? 3 * value : value);

    // [WTP]
    // assertion is not correct anymore
//    // Original function can return inconsistent base output when economy is between 3 and 4
//    assert((is_base && economy >= 3 && economy <= 4)
//        || (!is_base && sq->items & BIT_MONOLITH && !has_tech(Rules->tech_preq_allow_3_energy_sq, faction_id))
//        || (value == energy_yield(faction_id, base_id, x, y, flag)));
	
    return value;
}

static int __cdecl base_hex_cost(int unit_id, int faction_id, int x1, int y1, int x2, int y2, bool toggle) {
    MAP* sq_dst = mapsq(x2, y2);
    uint32_t bit_dst = (sq_dst ? sq_dst->items : 0);
    if (is_ocean(sq_dst)) {
        if (bit_dst & BIT_FUNGUS
        && sq_dst->alt_level() == ALT_OCEAN_SHELF
        && Units[unit_id].triad() == TRIAD_SEA
        && unit_id != BSC_SEALURK // Bug fix
        && unit_id != BSC_ISLE_OF_THE_DEEP
        && !has_project(FAC_XENOEMPATHY_DOME, faction_id)) {
            return Rules->move_rate_roads * 3;
        }
        return Rules->move_rate_roads;
    }
    MAP* sq_src = mapsq(x1, y1);
    uint32_t bit_src = (sq_src ? sq_src->items : 0);
    if (is_ocean(sq_src)) {
        return Rules->move_rate_roads;
    }
    if (unit_id >= 0 && Units[unit_id].triad() != TRIAD_LAND) {
        return Rules->move_rate_roads;
    }
    // Land only conditions
    if (bit_src & (BIT_MAGTUBE | BIT_BASE_IN_TILE) && bit_dst & (BIT_MAGTUBE | BIT_BASE_IN_TILE)
    && faction_id) {
        return 0;
    }
    if ((bit_src & (BIT_ROAD | BIT_BASE_IN_TILE) || (bit_src & BIT_FUNGUS && faction_id > 0
    && has_project(FAC_XENOEMPATHY_DOME, faction_id))) && bit_dst & (BIT_ROAD | BIT_BASE_IN_TILE)
    && faction_id) {
        return 1;
    }
    if (faction_id >= 0 && (has_project(FAC_XENOEMPATHY_DOME, faction_id) || !faction_id
    || unit_id == BSC_MIND_WORMS || unit_id == BSC_SPORE_LAUNCHER) && bit_dst & BIT_FUNGUS) {
        return 1;
    }
    if (bit_src & BIT_RIVER && bit_dst & BIT_RIVER && x_dist(x1, x2) == 1
    && abs(y1 - y2) == 1 && faction_id) {
        return 1;
    }
    if (Units[unit_id].chassis_id == CHS_HOVERTANK
    || has_abil(unit_id, ABL_ANTIGRAV_STRUTS)) {
        return Rules->move_rate_roads;
    }
    int cost = Rules->move_rate_roads;
    if (sq_dst->is_rocky() && !toggle) {
        cost += Rules->move_rate_roads;
    }
    if (bit_dst & BIT_FOREST && !toggle) {
        cost += Rules->move_rate_roads;
    }
    if (faction_id && bit_dst & BIT_FUNGUS && (unit_id >= MaxProtoFactionNum
    || Units[unit_id].offense_value() >= 0)) {
        int plan = Units[unit_id].plan;
        if (plan != PLAN_TERRAFORM && plan != PLAN_ARTIFACT
        && Factions[faction_id].SE_planet <= 0) {
            return cost + Rules->move_rate_roads * 2;
        }
        int value = proto_speed(unit_id);
        if (cost <= value) {
            return value;
        }
    }
    return cost;
}

int __cdecl mod_hex_cost(int unit_id, int faction_id, int x1, int y1, int x2, int y2, int toggle) {
    int value = base_hex_cost(unit_id, faction_id, x1, y1, x2, y2, toggle);
    MAP* sq_a = mapsq(x1, y1);
    MAP* sq_b = mapsq(x2, y2);

//    if (DEBUG && sq_a && sq_b) {
//        assert(value == hex_cost(unit_id, faction_id, x1, y1, x2, y2, toggle));
//    }
    if (conf.magtube_movement_rate > 0 && Units[unit_id].triad() == TRIAD_LAND) {
        if (!is_ocean(sq_a) && !is_ocean(sq_b)) {
            if (sq_a->items & (BIT_BASE_IN_TILE | BIT_MAGTUBE)
            && sq_b->items & (BIT_BASE_IN_TILE | BIT_MAGTUBE)) {
                value = 1;
            } else if (value == 1) { // Moving along a road
                value = conf.road_movement_rate;
            }
        }
    }
    if (conf.fast_fungus_movement > 0 && Units[unit_id].triad() != TRIAD_AIR) {
        if (!is_ocean(sq_b) && sq_b->is_fungus()) {
            value = min(max(proto_speed(unit_id), Rules->move_rate_roads), value);
        }
    }
    return value;
}

/*
Determine if the tile has a resource bonus.
Return Value: 0 (no bonus), 1 (nutrient), 2 (mineral), 3 (energy)
*/
int __cdecl mod_bonus_at(int x, int y) {
    MAP* sq = mapsq(x, y);
    uint32_t bit = sq->items;
    uint32_t alt = sq->alt_level();
    bool has_rsc_bonus = bit & BIT_BONUS_RES;
    if (!has_rsc_bonus && (!*MapRandomSeed || (alt >= ALT_SHORE_LINE
    && !conf.rare_supply_pods && !(*GameRules & RULES_NO_UNITY_SCATTERING)))) {
        return 0;
    }
    int avg = (x + y) >> 1;
    x -= avg;
    int chk = (avg & 3) + 4 * (x & 3);
    if (!has_rsc_bonus && chk != ((*MapRandomSeed + (-5 * (avg >> 2)) - 3 * (x >> 2)) & 0xF)) {
        return 0;
    }
    if (alt < ALT_OCEAN_SHELF) {
        return 0;
    }
    int ret = (alt < ALT_SHORE_LINE && !conf.rare_supply_pods) ? chk % 3 + 1 : (chk % 5) & 3;
    if (!ret || bit & BIT_NUTRIENT_RES) {
        if (bit & BIT_ENERGY_RES) {
            return 3; // energy
        }
        return ((bit & BIT_MINERAL_RES) != 0) + 1; // nutrient or mineral
    }
    return ret;
}

/*
Determine if the tile has a supply pod and if so what type.
Return Value: 0 (no supply pod), 1 (standard supply pod), 2 (unity pod?)
*/
int __cdecl mod_goody_at(int x, int y) {
    MAP* sq = mapsq(x, y);
    uint32_t bit = sq->items;
    uint32_t alt = sq->alt_level();
    if (bit & (BIT_SUPPLY_REMOVE | BIT_MONOLITH)) {
        return 0; // nothing, supply pod already opened or monolith
    }
    if (*GameRules & RULES_NO_UNITY_SCATTERING) {
        return (bit & (BIT_UNK_4000000 | BIT_UNK_8000000)) ? 2 : 0; // ?
    }
    if (bit & BIT_SUPPLY_POD) {
        return 1; // supply pod
    }
    if (!*MapRandomSeed) {
        return 0; // nothing
    }
    int avg = (x + y) >> 1;
    int x_diff = x - avg;
    int cmp = (avg & 3) + 4 * (x_diff & 3);
    if (!conf.rare_supply_pods && alt >= ALT_SHORE_LINE) {
        if (cmp == ((-5 * (avg >> 2) - 3 * (x_diff >> 2) + *MapRandomSeed) & 0xF)) {
            return 2;
        }
    }
    return cmp == ((11 * (avg / 4) + 61 * (x_diff / 4) + *MapRandomSeed + 8) & 0x1F); // 0 or 1
}

/*
This version adds support for modified territory borders (earlier bases claim tiles first).
*/
int __cdecl mod_base_find3(int x, int y, int faction1, int region, int faction2, int faction3) {
    int dist = 9999;
    int result = -1;
    bool border_fix = conf.territory_border_fix && region >= MaxRegionNum/2;

    for (int i = 0; i < *BaseCount; i++) {
        BASE* base = &Bases[i];
        MAP* bsq = mapsq(base->x, base->y);

        if (bsq && (region < 0 || bsq->region == region || border_fix)) {
            if ((faction1 < 0 && (faction2 < 0 || faction2 != base->faction_id))
            || (faction1 == base->faction_id)
            || (faction2 == -2 && Factions[faction1].diplo_status[base->faction_id] & DIPLO_PACT)
            || (faction2 >= 0 && faction2 == base->faction_id)) {
                if (faction3 < 0 || base->faction_id == faction3 || base->visibility & (1 << faction3)) {
                    int val = vector_dist(x, y, base->x, base->y);
                    if (conf.territory_border_fix ? val < dist : val <= dist) {
                        dist = val;
                        result = i;
                    }
                }
            }
        }
    }
    if (DEBUG && !conf.territory_border_fix) {
        int res = base_find3(x, y, faction1, region, faction2, faction3);
        debug("base_find3 x: %2d y: %2d r: %2d %2d %2d %2d %2d %4d\n",
              x, y, region, faction1, faction2, faction3, result, dist);
        assert(res == result);
        assert(*BaseFindDist == dist);
    }
    *BaseFindDist = 9999;
    if (result >= 0) {
        *BaseFindDist = dist;
    }
    return result;
}

int __cdecl mod_whose_territory(int faction_id, int x, int y, int* base_id, int ignore_comm) {
    MAP* sq = mapsq(x, y);
    if (!sq || sq->owner < 0) { // Fix: invalid coordinates return -1 (no owner)
        return -1;
    }
    if (faction_id != sq->owner) {
        if (!ignore_comm
        && !(*GameState & STATE_OMNISCIENT_VIEW)
        && (!has_treaty(faction_id, sq->owner, DIPLO_COMMLINK)
        || !has_treaty(faction_id, sq->owner, DIPLO_UNK_8000000))) {
            return -1;
        }
        if (base_id) {
            if (conf.territory_border_fix) {
                *base_id = mod_base_find3(x, y, -1, sq->region, -1, -1);
            } else {
                *base_id = base_find3(x, y, -1, sq->region, -1, -1);
            }
        }
    }
    return sq->owner;
}

int total_yield(int x, int y, int faction) {
    return mod_crop_yield(faction, -1, x, y, 0)
        + mod_mine_yield(faction, -1, x, y, 0)
        + mod_energy_yield(faction, -1, x, y, 0);
}

int fungus_yield(int faction, ResType res_type) {
    Faction* f = &Factions[faction];
    int p = clamp(f->SE_planet_pending, -3, 0);
    int N = clamp(f->tech_fungus_nutrient + p, 0, 99);
    int M = clamp(f->tech_fungus_mineral + p, 0, 99);
    int E = clamp(f->tech_fungus_energy + p + (f->SE_economy_pending >= 2), 0, 99);

    if (has_project(FAC_MANIFOLD_HARMONICS, faction)) {
		// [WTP]
		// shift Manifold Harmonics minimal planet rating
        int m = clamp(f->SE_planet_pending - conf.manifold_harmonics_min_planet_rating + 1, 0, 4);
        N += ManifoldHarmonicsBonus[m][0];
        M += ManifoldHarmonicsBonus[m][1];
        E += ManifoldHarmonicsBonus[m][2];
    }
    if (res_type == RES_NUTRIENT) {
        return N;
    }
    if (res_type == RES_MINERAL) {
        return M;
    }
    if (res_type == RES_ENERGY) {
        return E;
    }
    return N+M+E;
}

int item_yield(int x, int y, int faction, int bonus, MapItem item) {
    MAP* sq = mapsq(x, y);
    if (!sq) {
        return 0;
    }
    int N = 0, M = 0, E = 0;
    if (item != BIT_FUNGUS) {
        if (bonus == RES_NUTRIENT) {
            N += ResInfo->bonus_sq_nutrient;
        }
        if (bonus == RES_MINERAL) {
            M += ResInfo->bonus_sq_mineral;
        }
        if (bonus == RES_ENERGY) {
            E += ResInfo->bonus_sq_energy;
        }
        if (sq->items & BIT_RIVER && !is_ocean(sq)) {
            E++;
        }
        if (sq->landmarks & LM_CRATER && sq->art_ref_id < 9) { M++; }
        if (sq->landmarks & LM_VOLCANO && sq->art_ref_id < 9) { M++; E++; }
        if (sq->landmarks & LM_JUNGLE) { N++; }
        if (sq->landmarks & LM_URANIUM) { E++; }
        if (sq->landmarks & LM_FRESH && is_ocean(sq)) { N++; }
        if (sq->landmarks & LM_GEOTHERMAL) { E++; }
        if (sq->landmarks & LM_RIDGE) { E++; }
        if (sq->landmarks & LM_CANYON) { M++; }
        if (sq->landmarks & LM_FOSSIL && sq->art_ref_id < 6) { M++; }
        if (Factions[faction].SE_economy_pending >= 2) {
            E++;
        }
    }
    if (item == BIT_FOREST) {
        N += ResInfo->forest_sq_nutrient;
        M += ResInfo->forest_sq_mineral;
        E += ResInfo->forest_sq_energy;
    }
    else if (item == BIT_THERMAL_BORE) {
        N += ResInfo->borehole_sq_nutrient;
        M += ResInfo->borehole_sq_mineral;
        E += ResInfo->borehole_sq_energy;
        N -= (N > 0);
    }
    else if (item == BIT_FUNGUS) {
        N = fungus_yield(faction, RES_NUTRIENT);
        M = fungus_yield(faction, RES_MINERAL);
        E = fungus_yield(faction, RES_ENERGY);
    }
    else if (item == BIT_FARM && is_ocean(sq)) {
        N += ResInfo->ocean_sq_nutrient + ResInfo->improved_sea_nutrient;
        M += ResInfo->ocean_sq_mineral;
        E += ResInfo->ocean_sq_energy;
        if (sq->items & BIT_MINE) {
            M += ResInfo->improved_sea_mineral
                + has_tech(Rules->tech_preq_mining_platform_bonus, faction);
            N -= (N > 0);
        }
        if (sq->items & BIT_SOLAR) {
            E += ResInfo->improved_sea_energy;
        }
    }
    else {
        assert(0);
    }
    if (N > 2 && bonus != RES_NUTRIENT
    && !has_tech(Rules->tech_preq_allow_3_nutrients_sq, faction)) {
        N = 2;
    }
    if (M > 2 && bonus != RES_MINERAL
    && !has_tech(Rules->tech_preq_allow_3_minerals_sq, faction)) {
        M = 2;
    }
    if (E > 2 && bonus != RES_ENERGY
    && !has_tech(Rules->tech_preq_allow_3_energy_sq, faction)) {
        E = 2;
    }
    if (conf.debug_verbose) {
        debug("item_yield %2d %2d %08X faction: %d bonus: %d planet: %d N: %d M: %d E: %d prev: %d\n",
        x, y, item, faction, bonus, Factions[faction].SE_planet_pending,
        N, M, E, total_yield(x, y, faction));
    }
    return N+M+E;
}

void process_map(int faction, int k) {
    spawns.clear();
    natives.clear();
    goodtiles.clear();
    /*
    This value defines how many tiles an area needs to have before it's
    considered sufficiently large to be a faction starting location.
    Map area square root values: Tiny = 33, Standard = 56, Huge = 90
    */
    int limit = clamp(*MapAreaTiles, 1024, 8192) / 40;
    size_t land_area = 0;
    MAP* sq;

    for (int y = 0; y < *MapAreaY; y++) {
        for (int x = y&1; x < *MapAreaX; x+=2) {
            if (y < k || y >= *MapAreaY - k || !(sq = mapsq(x, y))) {
                continue;
            }
            // LM_FRESH landmark is ignored for this check
            if (!is_ocean(sq) && !sq->is_rocky() && !sq->is_fungus()
            && sq->is_land_region() && !(sq->landmarks & ~LM_FRESH)
            && Continents[sq->region].tile_count >= limit) {
                goodtiles.insert({x, y});
            }
            if (sq->is_land_region()) {
                land_area++;
            }
        }
    }
    for (int i = 0; i < *VehCount; i++) {
        VEH* v = &Vehicles[i];
        if (v->faction_id == 0) {
            natives.insert({v->x, v->y});
        }
        if (v->faction_id != 0 && v->faction_id != faction) {
            spawns.insert({v->x, v->y});
        }
    }
    if (goodtiles.size() * 3 < land_area) {
        goodtiles.clear();
    }
    debug("process_map x: %d y: %d sqrt: %d tiles: %d good: %d\n",
        *MapAreaX, *MapAreaY, *MapAreaSqRoot, *MapAreaTiles, goodtiles.size());
}

bool valid_start(int faction, int iter, int x, int y) {
    MAP* sq = mapsq(x, y);
    bool aquatic = MFactions[faction].is_aquatic();
    int limit = max((*MapAreaTiles < 1600 ? 5 : 7), 8 - iter/80);
    int min_sc = 80 - iter/4;
    int sea = 0;
    int sc = 0;
    int xd = 0;
    int yd = 0;

    if (!sq || sq->items & BIT_BASE_DISALLOWED || (sq->is_rocky() && !is_ocean(sq))) {
        return false;
    }
    // LM_FRESH landmark is incorrectly used on some maps
    if (aquatic != is_ocean(sq) || (sq->landmarks & ~LM_FRESH && iter < 150)) {
        return false;
    }
    if ((goody_at(x, y) > 0 || bonus_at(x, y) > 0) && iter < 200) {
        return false;
    }
    if (min_range(natives, x, y) < max(4, 8 - iter/32)) {
        return false;
    }
    if (min_range(spawns, x, y) < max(limit, *MapAreaSqRoot/4 + 8 - iter/8)) {
        return false;
    }
    if (aquatic) {
        if (Continents[sq->region].tile_count < *MapAreaTiles / (16 << (iter/32))) {
            return false;
        }
    } else {
        for (int i = 2; i < 20; i+=2) {
            if (is_ocean(mapsq(wrap(x-i), y)) || is_ocean(mapsq(wrap(x+i), y))) {
                break;
            }
            xd++;
        }
        for (int i = 2; i < 20; i+=2) {
            if (is_ocean(mapsq(x, y-i)) || is_ocean(mapsq(x, y+i))) {
                break;
            }
            yd++;
        }
    }
    for (auto& m : iterate_tiles(x, y, 0, 45)) {
        int bonus = bonus_at(m.x, m.y);
        if (is_ocean(m.sq)) {
            if (!is_ocean_shelf(m.sq)) {
                sc--;
            } else if (aquatic) {
                sc += 4;
            }
            sea++;
        } else {
            sc += (m.sq->is_rainy_or_moist() ? 3 : 1);
            if (m.sq->items & BIT_RIVER) {
                sc += 5;
            }
            if (m.sq->items & BIT_FOREST) {
                sc += 4;
            }
            if (m.sq->is_rolling()) {
                sc += 1;
            }
        }
        if (bonus != RES_NONE) {
            sc += (m.i < 25 ? 15 : 10);
        }
        if (goody_at(m.x, m.y) > 0) {
            sc += 15;
        }
        if (m.sq->items & BIT_FUNGUS) {
            sc -= (m.i <= 20 ? 4 : 2) * (is_ocean(m.sq) ? 1 : 2);
        }
        if (m.sq->items & BIT_MONOLITH) {
            sc += 8;
        }
    }
    debug("find_score %d %d x: %3d y: %3d xd: %d yd: %d min: %d score: %d\n",
        faction, iter, x, y, xd, yd, min_sc, sc);

    if (!aquatic && iter < 100) { // Avoid spawns without sufficient land nearby
        if (sea > 20) {
            return false;
        }
        if (max(xd, yd) < 4) {
            return false;
        }
    }
    return sc >= min_sc;
}

void apply_nutrient_bonus(int faction, int* x, int* y) {
    MAP* sq;
    Points addon;
    Points places;
    Points allpods;
    Points rivers;
    bool aquatic = MFactions[faction].is_aquatic();
    int adjust = (aquatic ? 0 : 8);
    int nutrient = 0;
    int num = 0;

    // Bonus resources are placed adjacent to each other only on diagonals
    for (auto& m : iterate_tiles(*x, *y, 0, 80)) {
        int bonus = bonus_at(m.x, m.y);
        int goody = goody_at(m.x, m.y);
        if (aquatic == is_ocean(m.sq) && m.sq->alt_level() >= ALT_OCEAN_SHELF
        && (m.i < 25 || (m.i < 45 && nutrient + (int)places.size()/4 < conf.nutrient_bonus))) {
            if (bonus == RES_NUTRIENT) {
                if (nutrient < conf.nutrient_bonus && m.sq->items & BIT_FUNGUS) {
                    m.sq->items &= ~BIT_FUNGUS;
                }
                if (m.sq->is_rocky()) {
                    rocky_set(m.x, m.y, LEVEL_ROLLING);
                }
                synch_bit(m.x, m.y, faction);
                nutrient++;
            } else if (!goody && bonus == RES_NONE
            && adjust > 0 && m.sq->items & BIT_RIVER) {
                if (m.i == 0) {
                    adjust = 0;
                }
                if (m.i > 0 && m.i <= adjust
                && m.sq->region == region_at(*x, *y)
                && can_build_base(m.x, m.y, faction, TRIAD_LAND)
                && min_range(spawns, m.x, m.y) >= 8) {
                    rivers.insert({m.x, m.y});
                }
            }
        }
        if (bonus || goody) {
            allpods.insert({m.x, m.y});
        }
        else if (m.i < 45 && (int)places.size()/4 < conf.nutrient_bonus
        && aquatic == is_ocean(m.sq) && m.sq->alt_level() >= ALT_OCEAN_SHELF
        && !(m.sq->items & (BIT_SUPPLY_REMOVE | BIT_MONOLITH))) {
            places.insert({m.x, m.y});
        }
    }
    while (places.size() > 0 && nutrient + (int)addon.size() < conf.nutrient_bonus) {
        auto t = pick_random(places);
        bool found = false;
        for (auto& p : allpods) {
            if (wrap(abs(p.x - t.x)) == 1 && abs(p.y - t.y) == 1) {
                found = true; break;
            }
        }
        if (!found) {
            addon.insert({t.x, t.y});
            allpods.insert({t.x, t.y});
        }
        places.erase(t);
    }
    for (auto& t : addon) {
        sq = mapsq(t.x, t.y);
        sq->items &= ~(BIT_FUNGUS | BIT_MINERAL_RES | BIT_ENERGY_RES);
        sq->items |= (BIT_SUPPLY_REMOVE | BIT_BONUS_RES | BIT_NUTRIENT_RES);
        if (sq->is_rocky()) {
            rocky_set(t.x, t.y, LEVEL_ROLLING);
        }
        synch_bit(t.x, t.y, faction);
        num++;
    }
    // Adjust position to adjacent river if currently not on river
    if (adjust > 0 && rivers.size() > 0) {
        auto t = pick_random(rivers);
        *x = t.x;
        *y = t.y;
    }
}

void __cdecl find_start(int faction, int* tx, int* ty) {
    bool aquatic = MFactions[faction].is_aquatic();
    int x = 0;
    int y = 0;
    int i = 0;
    int k = (*MapAreaY < 80 ? 4 : 8);
    process_map(faction, k/2);

    while (++i <= 500) {
        if (!aquatic && goodtiles.size() > 0 && i <= 200) {
            auto t = pick_random(goodtiles);
            y = t.y;
            x = t.x;
        } else {
            y = (random(*MapAreaY - k*2) + k);
            x = (random(*MapAreaX) & (~1)) + (y&1);
        }
        debug("find_iter  %d %d x: %3d y: %3d\n", faction, i, x, y);
        if (valid_start(faction, i, x, y)) {
            if (conf.nutrient_bonus > 0) {
                apply_nutrient_bonus(faction, &x, &y);
            }
            // No unity scattering can normally spawn pods at two tile range from the start
            if (*GameRules & RULES_NO_UNITY_SCATTERING && conf.rare_supply_pods > 1) {
                for (auto& m : iterate_tiles(x, y, 0, 25)) {
                    m.sq->items |= BIT_SUPPLY_REMOVE;
                }
            }
            *tx = x;
            *ty = y;
            break;
        }
    }
    site_set(*tx, *ty, world_site(*tx, *ty, 0));
    debug("find_start %d %d x: %3d y: %3d range: %d\n", faction, i, *tx, *ty, min_range(spawns, *tx, *ty));
    flushlog();
}

bool locate_landmark(int* x, int* y, bool ocean) {
    int attempts = 0;
    if (!mapsq(*x, *y)) {
        do {
            if (*MapAreaY > 17) {
                *y = random(*MapAreaY - 16) + 8;
            } else {
                *y = 0;
            }
            if (*MapAreaX > 1) {
                *x = (random(*MapAreaX) & (~1)) + (*y&1);
            } else {
                *x = 0;
            }
            if (++attempts >= 1000) {
                return false;
            }
        } while (is_ocean(mapsq(*x, *y)) != ocean || near_landmark(*x, *y));
    }
    return true;
}

void __cdecl mod_world_monsoon(int x, int y) {
    MAP* sq;
    world_rainfall();
    if (!conf.modified_landmarks) {
        int attempts = 0;
        int land_count;
        if (!mapsq(x, y)) {
            do {
                y = *MapAreaY / 2 + random(4) - 2;
                x = (random(*MapAreaX) & (~1)) + (y&1);
                land_count = 0;
                for (int i = 0; i < TableRange[5]; i++) {
                    int x2 = wrap(x + TableOffsetX[i]);
                    int y2 = y + TableOffsetY[i];
                    if (!is_ocean(mapsq(x2, y2))) {
                        land_count++;
                    }
                }
                if (++attempts >= 1000) {
                    return;
                }
                sq = mapsq(x, y);
            } while (is_ocean(sq) || !is_coast(x, y, true)
            || land_count < 40 || !sq->is_rainy() || near_landmark(x, y));
        }
        for (int i = 0; i < TableRange[5]; i++) {
            int x2 = wrap(x + TableOffsetX[i]);
            int y2 = y + TableOffsetY[i];
            sq = mapsq(x2, y2);
            if (sq && abs(TableOffsetY[i]) <= 8) {
                if (i < 21 && is_ocean(sq)) {
                    world_alt_set(x2, y2, ALT_SHORE_LINE, true);
                }
                bit2_set(x2, y2, LM_JUNGLE, true);
                code_set(x2, y2, i);
            }
        }
        new_landmark(x, y, (int)Natural[__builtin_ctz(LM_JUNGLE)].name);
        return;
    }
    struct TileInfo {
        uint8_t valid;
        uint8_t valid_near;
        uint8_t sea;
        uint8_t sea_near;
    };
    std::unordered_map<Point, TileInfo> tiles;

    int i = 0, j = 0, x2 = 0, y2 = 0, num = 0;
    const int y_a = *MapAreaY * 5/16;
    const int y_b = *MapAreaY * 11/16;
    const int limit = max(1024, *MapAreaTiles) * (3 + *MapCloudCover) / 120;

    for (y = 0; y < *MapAreaY; y++) {
        for (x = y&1; x < *MapAreaX; x+=2) {
            if (!(sq = mapsq(x, y))) {
                continue;
            }
            tiles[{x, y}].sea = sq->alt_level() < ALT_SHORE_LINE
                && Continents[sq->region].tile_count > 15;
            tiles[{x, y}].valid = !sq->landmarks
                && (sq->alt_level() == ALT_SHORE_LINE || sq->alt_level() == ALT_ONE_ABOVE_SEA);
        }
    }
    for (y = 0; y < *MapAreaY; y++) {
        for (x = y&1; x < *MapAreaX; x+=2) {
            if (tiles[{x, y}].valid) {
                for (auto& p : iterate_tiles(x, y, 0, 45)) {
                    if (tiles[{p.x, p.y}].valid) {
                        tiles[{x, y}].valid_near++;
                    }
                    if (tiles[{p.x, p.y}].sea) {
                        tiles[{x, y}].sea_near++;
                    }
                }
            }
        }
    }
    int loc_all = 0;
    int loc_cur = random(4);
    for (i = 0; i < 512 && num < limit; i++) {
        if (loc_all == 15) {
            loc_all = 0;
            loc_cur = random(4);
        }
        if (i % 16 == 0 || loc_all & (1 << loc_cur)) {
            while (loc_all & (1 << loc_cur)) {
                loc_cur = random(4);
            }
        }
        y = (random(y_b - y_a) + y_a);
        x = wrap(((random(*MapAreaX / 4) + loc_cur * *MapAreaX / 4) &~1) + (y&1));
        if (!tiles[{x, y}].valid
        || tiles[{x, y}].sea_near < 8 - i/16
        || tiles[{x, y}].valid_near < 16 - i/32) {
            continue;
        }
        int goal = num + max(21, limit/4);
        for (j = 0; j < 32 && num < goal && num < limit; j++) {
            if (j % 4 == 0) {
                y2 = y;
                x2 = x;
            }
            y2 = y2 + random(8);
            x2 = wrap(((x2 + random(8)) &~1) + (y2&1));
            if (y2 >= y_a && y2 <= y_b
            && tiles[{x2, y2}].valid
            && tiles[{x2, y2}].valid_near > 8 - i/32) {
                for (auto& p : iterate_tiles(x2, y2, 0, 21)) {
                    if (tiles[{p.x, p.y}].valid && !(p.sq->landmarks & LM_JUNGLE)) {
                        assert(!is_ocean(p.sq));
                        bit2_set(p.x, p.y, LM_JUNGLE, 1);
                        code_set(p.x, p.y, num % 121);
                        loc_all |= (1 << loc_cur);
                        num++;
                    }
                }
            }
        }
    }
}

void __cdecl mod_world_borehole(int x, int y) {
    if (!locate_landmark(&x, &y, false)) {
        return;
    }
    // Replace inconsistent timeGetTime() values used for seeding
    uint32_t seed = random(256);
    int val0 = (seed / 8) % 4;
    int val1 = 8;
    int val2 = ((seed % 8) / 3) + 5;
    int val3 = 3 - ((seed % 8) % 3);
    int val4 = -1;

    if (val0 & 2) {
        val2--;
        val3--;
        if (val0 & 1) {
            val1 = 8;
            val2++;
            val3++;
        } else {
            val1 = 6;
            val2--;
            val3--;
        }
        val1 = (val1 + 8) % 8 + 1;
        val2 = (val2 + 8) % 8 + 1;
        val3 = (val3 + 8) % 8 + 1;
    }
    if (conf.modified_landmarks) {
        val1 = 2;
        val2 = 4;
        val3 = 6;
        val4 = 8;
    }
    for (int i = 0; i < 9; i++) {
        int x2 = wrap(x + TableOffsetX[i]);
        int y2 = y + TableOffsetY[i];
        if (mapsq(x2, y2)) {
            world_alt_set(x2, y2, ALT_SHORE_LINE, true);
            bit_set(x2, y2, BIT_SUPPLY_REMOVE, true);
            if (i == val1 || i == val2 || i == val3 || i == val4) {
                bit_set(x2, y2, BIT_THERMAL_BORE, true);
                bit2_set(x2, y2, LM_BOREHOLE, true);
                code_set(x2, y2, i);
            }
        }
    }
    for (auto offset : {val1, val2, val3, val4}) {
        if (offset > 0) {
            int x2 = wrap(x + TableOffsetX[offset]);
            int y2 = y + TableOffsetY[offset];
            for (int i = 1; i < 9; i++) {
                int x3 = wrap(x2 + TableOffsetX[i]);
                int y3 = y2 + TableOffsetY[i];
                if (mapsq(x3, y3) && is_ocean(mapsq(x3, y3))) {
                    world_alt_set(x3, y3, ALT_SHORE_LINE, true);
                }
            }
        }
    }
    bit2_set(x, y, LM_BOREHOLE, true);
    new_landmark(x, y, (int)Natural[__builtin_ctz(LM_BOREHOLE)].name);
}

void __cdecl mod_world_fossil(int x, int y) {
    if (!locate_landmark(&x, &y, true)) {
        return;
    }
    for (int i = 0; i < 6; i++) {
        int x2 = wrap(x + TableOffsetX[i]);
        int y2 = y + TableOffsetY[i];
        MAP* sq = mapsq(x2, y2);
        if (sq && is_ocean(sq)) {
            world_alt_set(x2, y2, ALT_OCEAN, true);
            if (conf.modified_landmarks) {
                world_alt_set(x2, y2, ALT_OCEAN_SHELF, true);
            }
            bit2_set(x2, y2, LM_FOSSIL, true);
            code_set(x2, y2, i);
        }
    }
    new_landmark(x, y, (int)Natural[__builtin_ctz(LM_FOSSIL)].name);
}

float world_fractal(FastNoiseLite& noise, int x, int y) {
    float val = 1.0f * noise.GetNoise(3.0f*x, 2.0f*y)
        + 0.3f * (2 + *MapErosiveForces) * noise.GetNoise(-6.0f*x, -4.0f*y);
    if (x < 8 && *MapAreaX >= 32 && !map_is_flat()) {
        val = x/8.0f * val + (1.0f - x/8.0f) * world_fractal(noise, *MapAreaX + x, y);
    }
    return val;
}

void console_world_generate(uint32_t seed) {
    if (*GameState & STATE_SCENARIO_EDITOR && *GameState & STATE_OMNISCIENT_VIEW) {
        *VehCount = 0;
        *BaseCount = 0;
        MapWin->fUnitNotViewMode = 0;
        MapWin->iUnit = -1;
        *GameState |= STATE_UNK_4;
        *GameState &= ~STATE_OMNISCIENT_VIEW;
        world_generate(seed);
        *GameState |= STATE_OMNISCIENT_VIEW;
        draw_map(1);
        GraphicWin_redraw(WorldWin);
    }
}

void __cdecl mod_world_build() {
    static uint32_t seed = random_state();
    if (!conf.new_world_builder) {
        ThinkerVars->map_random_value = 0;
        world_build();
        
        // [WTP]
		// modify ocean depth
		
		if (conf.ocean_depth_multiplier != 1.0)
		{
			debug("modify ocean depth %d\n", *MapAreaTiles);
			
			for (int i = 0; i < *MapAreaTiles; i++)
			{
				MAP *tile = &((*MapTiles)[i]);
				
				// ocean
				
				if (!is_ocean(tile))
					continue;
				
				// old level and altitude
				
				int oldLevel = tile->climate & 0xE0;
				int oldLevelIndex = oldLevel >> 5;
				int oldAltitude = tile->contour;
				int oldAltitudeM = 50 * (tile->contour - 60);
				int oldAltitudeLevelIndex = oldAltitude / 20;
				
				if (oldAltitudeLevelIndex != oldLevelIndex)
				{
					debug("\toldAltitudeLevelIndex and oldLevelIndex do not match.\n");
				}
				
				// new level and altitude
				
				int newAltitudeM = (int)floor(conf.ocean_depth_multiplier * (double)oldAltitudeM);
				int newAltitude = newAltitudeM / 50 + 60;
				int newAltitudeLevelIndex = newAltitude / 20;
				int newLevelIndex = newAltitudeLevelIndex;
				int newLevel = newLevelIndex << 5;
				
				// update map
				
				tile->climate = (tile->climate & ~0xE0) | newLevel;
				tile->contour = newAltitude;
				
				debug("[%4d] %d, %3d, %5d -> %d, %3d, %5d\n", i, oldLevelIndex, oldAltitude, oldAltitudeM, newLevelIndex, newAltitude, newAltitudeM);
				
			}
			
			debug("\n");
			
		}
		
        return;
    }
    seed += pair_hash(seed, GetTickCount());
    world_generate(seed);
}

void world_generate(uint32_t seed) {
    if (DEBUG) {
        *GameState |= STATE_DEBUG_MODE;
    }
    MAP* sq;
    mod_map_wipe();
    ThinkerVars->map_random_value = seed;
    FastNoiseLite noise;
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2S);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetSeed(seed);
    random_reseed(seed);

    memcpy(AltNatural, AltNaturalDefault, 0x2Cu);
    *WorldAddTemperature = 1;
    *WorldSkipTerritory = 1;
    /*
    MapSizePlanet (this should not be unset, but checked regardless)
    MapOceanCoverage
    MapLandCoverage
    MapErosiveForces
    MapPlanetaryOrbit
    MapCloudCover
    MapNativeLifeForms
    */
    for (int i = 0; i < 7; i++) {
        if (MapSizePlanet[i] < 0) {
            MapSizePlanet[i] = random(3);
        }
        if (i > 0) {
            MapSizePlanet[i] = clamp(MapSizePlanet[i], 0, 2);
        }
    }
    if (*GameState & STATE_OMNISCIENT_VIEW) {
        MapWin_clear_terrain(MapWin);
        draw_map(1);
    }
    game_srand(seed ^ 0xffff); // For game_rand function, terrain detail and landmark placement
    *MapRandomSeed = (seed % 0x7fff) + 1; // Must be non-zero, supply pod placement

    Points conts;
    uint32_t continents = clamp(*MapAreaSqRoot / 12, 4, 20);
    int levels[256] = {};
    int x = 0, y = 0, i = 0;

    if (conf.world_continents && *MapAreaY >= 32) {
        while (++i <= 200 && conts.size() < continents) {
            y = (random(*MapAreaY - 16) + 8);
            x = (random(*MapAreaX) &~1) + (y&1);
            if (i & 1 && min_vector(conts, x, y) <= *MapAreaSqRoot/6) {
                conts.insert({x, y});
            }
            if (~i & 1 && min_vector(conts, x, y) >= *MapAreaSqRoot/3) {
                conts.insert({x, y});
            }
        }
    }

    const float L = AltNatural[ALT_SHORE_LINE]; // Default shoreline altitude = 45
    const float Wmid = (50 - conf.world_sea_levels[*MapOceanCoverage])*0.01f;
    const float Wsea = 0.1f + 0.01f * conf.world_ocean_mod;
    const float Wland = 0.1f + 0.01f * conf.world_hills_mod + 0.25f * (*MapErosiveForces);
    const float Wdist = 1.0f / clamp(*MapAreaSqRoot * 0.1f, 1.0f, 15.0f);

    for (y = 0; y < *MapAreaY; y++) {
        float Wcaps = 1.0f - min(1.0f, (min(y, *MapAreaY - y) / (max(1.0f, *MapAreaY * 0.2f))));

        for (x = y&1; x < *MapAreaX; x+=2) {
            float Wcont = 1.5f - clamp(min_vector(conts, x, y) * Wdist, 0.75f, 1.5f);
            float value = world_fractal(noise, x, y) + Wmid - 0.5f*Wcaps;
            if (value > 0) {
                value = value * Wland;
            } else {
                value = value * Wsea;
            }
            if (conf.world_continents && value < 0.2f) {
                value += Wcont * Wland;
//                if (DEBUG) pm_overlay[x][y] = (int)(10*Wcont);
            }
            sq = mapsq(x, y);
            sq->contour = clamp((int)(L + L*value), 0, 255);
        }
    }
    if (conf.world_mirror_x) {
        const int ky = 2 - (*MapAreaY & 1);
        for (y = 0; y < *MapAreaY/2; y++) {
            for (x = y&1; x < *MapAreaX; x+=2) {
                MAP* src = mapsq(x, y);
                MAP* tgt = mapsq(x, *MapAreaY - y - ky);
                tgt->contour = src->contour;
            }
        }
    }
    if (conf.world_mirror_y) {
        const int kx = 2 - (*MapAreaX & 1);
        for (y = 0; y < *MapAreaY; y++) {
            for (x = y&1; x < *MapAreaX/2; x+=2) {
                MAP* src = mapsq(x, y);
                MAP* tgt = mapsq(*MapAreaX - x - kx, y);
                tgt->contour = src->contour;
            }
        }
    }
    for (y = 0; y < *MapAreaY; y++) {
        for (x = y&1; x < *MapAreaX; x+=2) {
            sq = mapsq(x, y);
            levels[sq->contour]++;
        }
    }
    int level_sum = 0;
    int level_mod = 0;
    for (i = 0; i < 256; i++) {
        level_sum += levels[i];
        if (level_sum >= *MapAreaTiles * conf.world_sea_levels[*MapOceanCoverage] / 100) {
            level_mod = AltNatural[ALT_SHORE_LINE] - i;
            break;
        }
    }
    for (y = 0; y < *MapAreaY; y++) {
        for (x = y&1; x < *MapAreaX; x+=2) {
            sq = mapsq(x, y);
            sq->contour = clamp(sq->contour + level_mod, 0, 255);
        }
    }
    debug("world_build seed: %10u size: %d x: %d y: %d "\
    "ocean: %d erosion: %d cloud: %d sea_level: %d level_mod: %d\n",
    seed, *MapSizePlanet, *MapAreaX, *MapAreaY, *MapOceanCoverage,
    *MapErosiveForces, *MapCloudCover, conf.world_sea_levels[*MapOceanCoverage], level_mod);

    if (conf.world_polar_caps) {
        world_polar_caps();
    }
    world_linearize_contours();
    world_shorelines();
    Path_continents(Paths);
    Points bridges;

    for (y = 3; y < *MapAreaY - 3; y++) {
        for (x = y&1; x < *MapAreaX; x+=2) {
            sq = mapsq(x, y);
            if (is_ocean(sq)) {
                continue;
            }
            uint64_t sea = 0;
            int land_count = Continents[sq->region].tile_count;
            if (conf.world_islands_mod > 0) {
                if (land_count < conf.world_islands_mod && land_count < *MapAreaTiles/8) {
                    bridges.insert({x, y});
                    if (DEBUG) mapdata[{x, y}].overlay = -1;
                    continue;
                }
            }
            for (auto& m : iterate_tiles(x, y, 1, 13)) {
                if (is_ocean(m.sq)) {
                    if (Continents[m.sq->region].tile_count >= 30) {
                        sea |= 1 << (m.sq->region & 0x3f);
                    }
                }
            }
            if (__builtin_popcount(sea) > 1) {
                if (land_count > *MapAreaTiles/8 || pair_hash(seed^(x/8), y/8) & 1) {
                    bridges.insert({x, y});
                    if (DEBUG) mapdata[{x, y}].overlay = -2;
                }
            }
        }
    }
    for (auto& p : bridges) {
        world_alt_set(p.x, p.y, ALT_OCEAN, 1);
        world_alt_set(p.x, p.y, ALT_OCEAN_SHELF, 1);
    }
    world_temperature();
    world_riverbeds();
    world_fungus();
    Path_continents(Paths);

    LMConfig lm;
    memcpy(&lm, &conf.landmarks, sizeof(lm));

    for (i = 0; i < 16; i++) {
        if (conf.world_mirror_x || conf.world_mirror_y) {
            break;
        }
        if (lm.jungle > 0) {
            mod_world_monsoon(-1, -1);
            lm.jungle--;
        }
        if (lm.crater > 0) {
            world_crater(-1, -1);
            lm.crater--;
        }
        if (lm.volcano > 0) {
            world_volcano(-1, -1, 0);
            lm.volcano--;
        }
        if (lm.mesa > 0) {
            world_mesa(-1, -1);
            lm.mesa--;
        }
        if (lm.ridge > 0) {
            world_ridge(-1, -1);
            lm.ridge--;
        }
        if (lm.uranium > 0) {
            world_diamond(-1, -1);
            lm.uranium--;
        }
        if (lm.ruins > 0) {
            world_ruin(-1, -1);
            lm.ruins--;
        }
        /*
        Unity Wreckage and Fossil Field Ridge are always expansion only content.
        Manifold Nexus and Borehole Cluster were also added to vanilla SMAC
        in the patches, even though those landmarks are technically expansion content.
        */
        if (*ExpansionEnabled) {
            if (lm.unity > 0) {
                world_unity(-1, -1);
                lm.unity--;
            }
            if (lm.fossil > 0) {
                mod_world_fossil(-1, -1);
                lm.fossil--;
            }
        }
        if (lm.canyon > 0) {
            world_canyon_nessus(-1, -1);
            lm.canyon--;
        }
        if (lm.nexus > 0) {
            world_temple(-1, -1);
            lm.nexus--;
        }
        if (lm.borehole > 0) {
            mod_world_borehole(-1, -1);
            lm.borehole--;
        }
        if (lm.sargasso > 0) {
            world_sargasso(-1, -1);
            lm.sargasso--;
        }
        if (lm.dunes > 0) {
            world_dune(-1, -1);
            lm.dunes--;
        }
        if (lm.fresh > 0) {
            world_fresh(-1, -1);
            lm.fresh--;
        }
        if (lm.geothermal > 0) {
            world_geothermal(-1, -1);
            lm.geothermal--;
        }
    }
    fixup_landmarks();
    *WorldAddTemperature = 0;
    *WorldSkipTerritory = 0; // If this flag is false, reset_territory is run in world_climate
    world_climate(); // Run Path::continents
    world_rocky();

    if (!*GameHalted) {
        MapWin_clear_terrain(MapWin);
    }
    flushlog();
}

void set_project_owner(FacilityId item_id, int faction_id) {
    for (int i = 0; i < *BaseCount; i++) {
        if (Bases[i].faction_id == faction_id) {
            SecretProjects[item_id - SP_ID_First] = i;
            break;
        }
    }
}

void __cdecl mod_time_warp() {
    const uint32_t BIT_SKIP_BONUS = (BIT_BASE_IN_TILE | BIT_VEH_IN_TILE
        | BIT_THERMAL_BORE | BIT_MONOLITH | BIT_MINE | BIT_SOLAR);
    const FacilityId projects[] = {
        FAC_CITIZENS_DEFENSE_FORCE,
        FAC_COMMAND_NEXUS,
        FAC_HUMAN_GENOME_PROJECT,
        FAC_MERCHANT_EXCHANGE,
        FAC_PLANETARY_TRANSIT_SYSTEM,
        FAC_VIRTUAL_WORLD,
        FAC_WEATHER_PARADIGM,
    };
    std::set<int32_t> selected = {};
    std::set<int32_t> choices = {};
    const int num = clamp(*MapAreaTiles / 3200 + 2, 2, 5);
    /*
    Aquatic factions always spawn with an extra Unity Gunship.
    Alien factions spawn with an extra Colony Pod and Battle Ogre Mk1.
    These units are added in setup_player regardless of other conditions.
    */

    if (conf.time_warp_mod && !*MultiplayerActive) {
        *SkipTechScreenB = 1;
        for (int i = 1; i < MaxPlayerNum; i++) {
            bool ocean = MFactions[i].is_aquatic();
            bool alien = MFactions[i].is_alien();
            for (int j = 0; j < conf.time_warp_techs; j++) {
                tech_advance(i);
            }
            Factions[i].energy_credits += num * 50;
            consider_designs(i);

            int base_id = find_hq(i);
            if (base_id >= 0) {
                BASE* base = &Bases[base_id];
                base->pop_size = 4;
                if (!has_fac_built(FAC_PRESSURE_DOME, base_id)) {
                    set_fac(FAC_RECYCLING_TANKS, base_id, 1);
                }
                set_fac(FAC_RECREATION_COMMONS, base_id, 1);
                set_fac(FAC_PERIMETER_DEFENSE, base_id, 1);
                set_fac(FAC_NETWORK_NODE, base_id, 1);

                for (int j = 0; j < num; j++) {
                    if (ocean) {
                        mod_veh_init(j&1 ? BSC_UNITY_GUNSHIP : BSC_TRANSPORT_FOIL, i, base->x, base->y);
                    } else {
                        mod_veh_init(j&1 ? BSC_UNITY_ROVER : BSC_SCOUT_PATROL, i, base->x, base->y);
                    }
                }
                for (int j = 0; j < num - ocean - alien; j++) {
                    mod_veh_init(ocean ? BSC_SEA_ESCAPE_POD : BSC_COLONY_POD, i, base->x, base->y);
                }
                for (int j = 0; j < num; j++) {
                    mod_veh_init(ocean ? BSC_SEA_FORMERS : BSC_FORMERS, i, base->x, base->y);
                }
                int fixed = 0;
                int added = 0;
                for (auto& m : iterate_tiles(base->x, base->y, 0, 81)) {
                    if ((base->x != m.x || base->y != m.y)
                    && (m.sq->owner < 0 || m.sq->owner == i)
                    && !(m.sq->landmarks & LM_VOLCANO)
                    && !(m.sq->items & BIT_SKIP_BONUS)) {
                        // Improve resources/remove fungus near start
                        int bonus = bonus_at(m.x, m.y);
                        if (!goody_at(m.x, m.y) && !m.sq->is_rocky() && added < 5 && !random(4)) {
                            if (!is_ocean(m.sq)) {
                                m.sq->items &= ~TerraformRules[FORMER_FOREST][1];
                                m.sq->items &= ~BIT_FUNGUS;
                                m.sq->items |= BIT_FOREST;
                                added++;
                            } else if (is_ocean_shelf(m.sq)) {
                                m.sq->items &= ~TerraformRules[FORMER_FARM][1];
                                m.sq->items &= ~BIT_FUNGUS;
                                m.sq->items |= BIT_FARM;
                                added++;
                            }
                        }
                        if (m.sq->items & BIT_FUNGUS && bonus != RES_NONE && fixed < 4) {
                            m.sq->items &= ~BIT_FUNGUS;
                            fixed++;
                        }
                        if (bonus == RES_NUTRIENT && m.sq->is_rocky()) {
                            rocky_set(m.x, m.y, LEVEL_ROLLING);
                        }
                    }
                    m.sq->visibility |= (1 << i);
                    synch_bit(m.x, m.y, i);
                }
                // Set initial production, turn advances once before player turn begins
                if (!base->nerve_staple_turns_left) {
                    ++base->nerve_staple_turns_left;
                }
                set_base(base_id);
                base_compute(1);
                base_change(base_id, select_build(base_id));
            }
        }
        *SkipTechScreenB = 0;
        *CurrentTurn = conf.time_warp_start_turn;
    } else {
        time_warp();
    }
    init_world_config();
    if (!conf.time_warp_projects || *MultiplayerActive) {
        return;
    }
    for (const FacilityId item_id : projects) {
        choices.clear();
        for (int i = 1; i < MaxPlayerNum; i++) {
            if (selected.count(i)
            || (item_id == FAC_COMMAND_NEXUS && has_free_facility(FAC_COMMAND_CENTER, i))
            || (item_id == FAC_CITIZENS_DEFENSE_FORCE && has_free_facility(FAC_PERIMETER_DEFENSE, i))) {
                // skip faction
            } else {
                choices.insert(i);
            }
        }
        if (!choices.size()) {
            for (int i = 1; i < MaxPlayerNum; i++) {
                if (!selected.count(i)) {
                    choices.insert(i);
                }
            }
        }
        if (choices.size()) {
            int32_t choice = pick_random(choices);
            set_project_owner(item_id, choice);
            selected.insert(choice);
            debug("time_warp %s %s\n", MFactions[choice].filename, Facility[item_id].name);
        } else {
            assert(0);
        }
    }
}

