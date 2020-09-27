
#include "list"
#include "move.h"
#include "ai.h"

typedef int PMTable[MAPSZ*2][MAPSZ];
/*

Priority Management Tables contain values calculated for each map square
to guide the AI in its move planning.

The tables contain only ephemeral values: they are recalculated each turn
for every active faction, and the values are never saved with the save games.

For example, pm_former counter is decremented whenever a former starts
work on a given square to prevent too many of them converging on the same spot.
It is also used as a shortcut to determine if a square has a friendly base
in a 2-tile radius, because formers are only allowed to work on squares which
are reachable by a friendly base worker.

Non-combat units controlled by Thinker (formers, colony pods, land-based crawlers)
use pm_safety table to decide if there are enemy units too close to them.
PM_SAFE defines the threshold below which the units will attempt to flee
to the square chosen by escape_move.

*/

PMTable pm_former;
PMTable pm_safety;
PMTable pm_roads;
PMTable pm_enemy;
PMTable pm_enemy_near;
bool build_tubes = false;

void adjust_value(PMTable tbl, int x, int y, int range, int value) {
    for (int i=-range*2; i<=range*2; i++) {
        for (int j=-range*2 + abs(i); j<=range*2 - abs(i); j+=2) {
            int x2 = wrap(x + i);
            int y2 = y + j;
            if (mapsq(x2, y2)) {
                tbl[x2][y2] += value;
            }
        }
    }
}

/*
Adjusts value but stays in region if requested.
*/
void adjust_value_within_region(PMTable tbl, int x, int y, int range, int value, bool stayInRegion)
{
	// staying in region is not required

	if (!stayInRegion)
		return adjust_value(tbl, x, y, range, value);

	// get map at location

	MAP *tile = getMapTile(x, y);

	if (tile == NULL)
		return;

	// iterate nearby tiles

    for (int i=-range*2; i<=range*2; i++) {
        for (int j=-range*2 + abs(i); j<=range*2 - abs(i); j+=2) {
            int x2 = wrap(x + i);
            int y2 = y + j;

            // get map at nearby location

            MAP *nearbyTile = getMapTile(x2, y2);

            if (nearbyTile == NULL)
				continue;

			// verify nearby location is in same region

			if (nearbyTile->region != tile->region)
				continue;

			tbl[x2][y2] += value;

        }

    }

}

bool other_in_tile(int faction, MAP* sq) {
    int u = unit_in_tile(sq);
    return (u >= 0 && u != faction);
}

bool non_ally_in_tile(int faction, MAP* sq) {
    int u = unit_in_tile(sq);
    return (u >= 0 && u != faction && ~tx_factions[faction].diplo_status[u] & DIPLO_PACT);
}

HOOK_API int enemy_move(int id) {
    assert(id >= 0 && id < UNITS);
    VEH* veh = &tx_vehicles[id];
    int faction = veh->faction_id;

    // kick sea explorers off land port

    if (isHealthySeaExplorerInLandPort(id))
	{
		return kickSeaExplorerFromLandPort(id);
	}

    if (ai_enabled(faction)) {
        int w = tx_units[veh->proto_id].weapon_type;
        if (w == WPN_COLONY_MODULE) {
            return colony_move(id);
        } else if (w == WPN_SUPPLY_TRANSPORT) {
            return crawler_move(id);
        } else if (w == WPN_TERRAFORMING_UNIT) {

        	// select terraforming algorithm

        	if (conf.ai_useWTPAlgorithms)
			{
				// WTP
				return enemyMoveFormer(id);
			}
			else
			{
				// Thinker
				return former_move(id);
			}

        } else if (w == WPN_TROOP_TRANSPORT && veh_triad(id) == TRIAD_SEA) {
            return trans_move(id);
        } else if (w <= WPN_PSI_ATTACK && veh_triad(id) == TRIAD_LAND) {

        	// select combat algorithm

        	if (conf.ai_useWTPAlgorithms)
			{
				// WTP
				return enemyMoveCombat(id);
			}
			else
			{
				// Thinker
				return combat_move(id);
			}

        }
        else
		{
        	// select combat algorithm

        	if (conf.ai_useWTPAlgorithms)
			{
				// WTP
				return enemyMoveCombat(id);
			}
			else
			{
				// Thinker
			}

		}

    }
    return tx_enemy_move(id);
}

HOOK_API int log_veh_kill(int UNUSED(ptr), int id, int UNUSED(owner), int UNUSED(unit_id)) {
    /* This logging function is called whenever a vehicle is removed/killed for any reason. */
    VEH* veh = &tx_vehicles[id];
    if (ai_enabled(*active_faction) && at_war(*active_faction, veh->faction_id)) {
        assert(veh->x >= 0 && veh->y >= 0);
        adjust_value(pm_enemy_near, veh->x, veh->y, 3, -1);
        pm_enemy[veh->x][veh->y]--;
    }
    return 0;
}

void adjust_route(PMTable tbl, const TileSearch& ts, int value) {
    int i = 0;
    int j = ts.cur;
    while (j >= 0 && ++i < 50) {
        auto p = ts.newtiles[j];
        j = p.prev;
        tbl[p.x][p.y] += value;
    }
}

int route_distance(PMTable tbl, int x1, int y1, int x2, int y2) {
    Points visited;
    std::list<PathNode> items;
    items.push_back({x1, y1, 0, 0});
    int limit = max(8, map_range(x1, y1, x2, y2) * 2);
    int i = 0;

    while (items.size() > 0 && ++i < 120) {
        PathNode cur = items.front();
        items.pop_front();
        if (cur.x == x2 && cur.y == y2 && cur.dist <= limit) {
            debug("route_distance %d %d -> %d %d = %d %d\n", x1, y1, x2, y2, i, cur.dist);
            return cur.dist;
        }
        for (const int* t : offset) {
            int rx = wrap(cur.x + t[0]);
            int ry = cur.y + t[1];
            if (mapsq(rx, ry) && tbl[rx][ry] > 0 && !visited.count({rx, ry})) {
                items.push_back({rx, ry, cur.dist + 1, 0});
                visited.insert({rx, ry});
            }
        }
    }
    return -1;
}

void move_upkeep(int faction) {
    if (!ai_enabled(faction)) {
        return;
    }
    int enemymask = 1;
    convoys.clear();
    boreholes.clear();
    needferry.clear();
    memset(pm_former, 0, sizeof(pm_former));
    memset(pm_safety, 0, sizeof(pm_safety));
    memset(pm_roads, 0, sizeof(pm_roads));
    memset(pm_enemy, 0, sizeof(pm_enemy));
    memset(pm_enemy_near, 0, sizeof(pm_enemy_near));
    build_tubes = has_terra(faction, FORMER_MAGTUBE, 0);

    for (int i=1; i<8; i++) {
        enemymask |= (at_war(faction, i) ? 1 : 0) << i;
    }
    for (int i=0; i<*total_num_vehicles; i++) {
        VEH* veh = &tx_vehicles[i];
        UNIT* u = &tx_units[veh->proto_id];
        auto xy = mp(veh->x, veh->y);
        if (veh->move_status == FORMER_THERMAL_BORE+4) {
            boreholes.insert(xy);
        }
        if ((1 << veh->faction_id) & enemymask) {
            int val = (u->weapon_type <= WPN_PSI_ATTACK && veh->proto_id != BSC_FUNGAL_TOWER ? -100 : 0);
            adjust_value_within_region(pm_safety, veh->x, veh->y, 1, val, veh_triad(i) != TRIAD_AIR);
            if (val == -100) {
                int w = (u->chassis_type == CHS_INFANTRY ? 2 : 1);
                adjust_value_within_region(pm_safety, veh->x, veh->y, 4-w, val/(w*4), veh_triad(i) != TRIAD_AIR);
            }
            adjust_value_within_region(pm_enemy_near, veh->x, veh->y, 3, 1, veh_triad(i) != TRIAD_AIR);
            pm_enemy[veh->x][veh->y]++;
        } else if (veh->faction_id == faction) {
            if (u->weapon_type <= WPN_PSI_ATTACK) {
                adjust_value_within_region(pm_safety, veh->x, veh->y, 1, 40, veh_triad(i) != TRIAD_AIR);
                pm_safety[veh->x][veh->y] += 60;
            }
            if ((u->weapon_type == WPN_COLONY_MODULE || u->weapon_type == WPN_TERRAFORMING_UNIT)
            && veh_triad(i) == TRIAD_LAND) {
                MAP* sq = mapsq(veh->x, veh->y);
                if (sq && is_ocean(sq) && sq->items & TERRA_BASE_IN_TILE
                && coast_tiles(veh->x, veh->y) < 8) {
                    needferry.insert(xy);
                }
            }
        }
    }
    MAP* sq;
    TileSearch ts;
    for (int y=0; y < *map_axis_y; y++) {
        for (int x=y&1; x < *map_axis_x; x += 2) {
            sq = mapsq(x, y);
            if (sq->owner == faction && ((!build_tubes && sq->items & TERRA_ROAD)
            || (build_tubes && sq->items & TERRA_MAGTUBE))) {
                pm_roads[x][y]++;
            }
        }
    }
    for (int i=0; i<*total_num_bases; i++) {
        BASE* base = &tx_bases[i];
        if (base->faction_id == faction) {
            adjust_value(pm_former, base->x, base->y, 2, base->pop_size);
            pm_safety[base->x][base->y] += 10000;

            if (!is_sea_base(i)) {
                int j = 0;
                int k = 0;
                ts.init(base->x, base->y, TERRITORY_LAND);
                while (++j < 150 && k < 5 && (sq = ts.get_next()) != NULL) {
                    if (sq->items & TERRA_BASE_IN_TILE) {
                        if (route_distance(pm_roads, base->x, base->y, ts.rx, ts.ry) < 0) {
                            debug("route_add %d %d -> %d %d\n", base->x, base->y, ts.rx, ts.ry);
                            adjust_route(pm_roads, ts, 1);
                        }
                        k++;
                    }
                }
            }
        }
    }
}

bool need_formers(int x, int y, int faction) {
    assert(faction > 0 && faction < 8);
    MAP* sq;
    int k = 0;
    for (int i=0; i<20; i++) {
        int x2 = wrap(x + offset_tbl[i][0]);
        int y2 = y + offset_tbl[i][1];
        sq = mapsq(x2, y2);
        if (sq && sq->owner == faction && select_item(x2, y2, faction, sq) != -1 && ++k > 3) {
            return true;
        }
    }
    return false;
}

bool has_transport(int x, int y, int faction) {
    assert(faction > 0 && faction < 8);
    for (int i=0; i<*total_num_vehicles; i++) {
        VEH* veh = &tx_vehicles[i];
        UNIT* u = &tx_units[veh->proto_id];
        if (veh->faction_id == faction && veh->x == x && veh->y == y
        && tx_weapon[u->weapon_type].mode == WMODE_TRANSPORT) {
            return true;
        }
    }
    return false;
}

bool non_combat_move(int x, int y, int faction, int triad) {
    MAP* sq = mapsq(x, y);
    int other = unit_in_tile(sq);
    if (x < 0 || y < 0 || !sq || (triad != TRIAD_AIR && is_ocean(sq) != (triad == TRIAD_SEA))) {
        return false;
    }
    if (sq->owner > 0 && sq->owner != faction) {
        return false;
    }
    if (other < 0 || other == faction) {
        return pm_safety[x][y] >= PM_NEAR_SAFE;
    }
    return false;
}

void count_bases(int x, int y, int faction, int triad, int* area, int* bases) {
    assert(triad == TRIAD_LAND || triad == TRIAD_SEA || triad == TRIAD_AIR);
    MAP* sq;
    *area = 0;
    *bases = 0;
    int i = 0;
    TileSearch ts;
    ts.init(x, y, triad, 2);
    while (++i < 160 && (sq = ts.get_next()) != NULL) {
        if (sq->items & TERRA_BASE_IN_TILE) {
            (*bases)++;
        } else if (can_build_base(ts.rx, ts.ry, faction, triad)) {
            (*area)++;
        }
    }
    debug("count_bases %d %d %d %d %d\n", x, y, triad, *area, *bases);
}

bool has_base_sites(int x, int y, int faction, int triad) {
    int area;
    int bases;
    count_bases(x, y, faction, triad, &area, &bases);
    return area > bases+4;
}

bool need_heals(VEH* veh) {
    return veh->damage_taken > 2 && (veh->proto_id < BSC_BATTLE_OGRE_MK1 || veh->proto_id > BSC_BATTLE_OGRE_MK3);
}

bool defend_tile(VEH* veh, MAP* sq) {
    UNIT* u = &tx_units[veh->proto_id];
    if (!sq) {
        return true;
    } else if (need_heals(veh) && sq->items & (TERRA_BASE_IN_TILE | TERRA_MONOLITH)) {
        return true;
    } else if (~sq->items & TERRA_BASE_IN_TILE || u->weapon_type <= WPN_PSI_ATTACK) {
        return false;
    }
    int n = 0;
    for (const int* t : offset) {
        int x2 = wrap(veh->x + t[0]);
        int y2 = veh->y + t[1];
        if (mapsq(x2, y2) && pm_safety[x2][y2] < PM_SAFE) {
            n++;
        }
    }
    return n > 3;
}

int escape_score(int x, int y, int range, VEH* veh, MAP* sq) {
    return pm_safety[x][y] - range*120
        + (sq->items & TERRA_MONOLITH && need_heals(veh) ? 1000 : 0)
        + (sq->items & TERRA_BUNKER ? 500 : 0)
        + (sq->items & TERRA_FOREST ? 100 : 0)
        + (sq->rocks & TILE_ROCKY ? 100 : 0);
}

int escape_move(int id) {
    VEH* veh = &tx_vehicles[id];
    MAP* sq = mapsq(veh->x, veh->y);
    if (defend_tile(veh, sq))
        return veh_skip(id);
    int faction = veh->faction_id;
    int tscore = pm_safety[veh->x][veh->y];
    int i = 0;
    int tx = -1;
    int ty = -1;
    TileSearch ts;
    ts.init(veh->x, veh->y, veh_triad(id));
    while (++i < (tscore < 500 ? 80 : 25) && (sq = ts.get_next()) != NULL) {
        int score = escape_score(ts.rx, ts.ry, ts.dist-1, veh, sq);
        if (score > tscore && !non_ally_in_tile(faction, sq) && !ts.has_zoc(faction)) {
            tx = ts.rx;
            ty = ts.ry;
            tscore = score;
            debug("escape_score %d %d -> %d %d | %d %d %d\n",
                veh->x, veh->y, ts.rx, ts.ry, i, ts.dist, score);
        }
    }
    if (tx >= 0) {
        debug("escape_move  %d %d -> %d %d | %d %d\n",
            veh->x, veh->y, tx, ty, pm_safety[veh->x][veh->y], tscore);
        return set_move_to(id, tx, ty);
    }
    if (tx_units[veh->proto_id].weapon_type <= WPN_PSI_ATTACK)
        return tx_enemy_move(id);
    return veh_skip(id);
}

int trans_move(int id) {
    VEH* veh = &tx_vehicles[id];
    int faction = veh->faction_id;
    int triad = veh_triad(id);
    auto xy = mp(veh->x, veh->y);
    if (needferry.count(xy)) {
        needferry.erase(xy);
        return veh_skip(id);
    }
    if (needferry.size() > 0 && triad == TRIAD_SEA && at_target(veh)) {
        MAP* sq;
        int i = 0;
        TileSearch ts;
        ts.init(veh->x, veh->y, TRIAD_SEA);
        while (i++ < 120 && (sq = ts.get_next()) != NULL) {
            xy = mp(ts.rx, ts.ry);
            if (needferry.count(xy) && non_combat_move(ts.rx, ts.ry, faction, triad)) {
                needferry.erase(xy);
                debug("trans_move %d %d -> %d %d\n", veh->x, veh->y, ts.rx, ts.ry);
                return set_move_to(id, ts.rx, ts.ry);
            }
        }
    }
    return tx_enemy_move(id);
}

int want_convoy(int faction, int x, int y, MAP* sq) {
    if (sq && sq->owner == faction && !convoys.count({x, y})
    && !(sq->items & BASE_DISALLOWED)) {
        int bonus = tx_bonus_at(x, y);
        if (bonus == RES_ENERGY)
            return RES_NONE;
        else if (sq->items & (TERRA_CONDENSER | TERRA_SOIL_ENR))
            return RES_NUTRIENT;
        else if (bonus == RES_NUTRIENT)
            return RES_NONE;
        else if (sq->items & TERRA_MINE && sq->rocks & TILE_ROCKY)
            return RES_MINERAL;
        else if (sq->items & TERRA_FOREST && ~sq->items & TERRA_RIVER)
            return RES_MINERAL;
    }
    return RES_NONE;
}

int crawler_move(int id) {
    VEH* veh = &tx_vehicles[id];
    MAP* sq = mapsq(veh->x, veh->y);
    if (!sq || veh->home_base_id < 0)
        return veh_skip(id);
    if (!at_target(veh))
        return SYNC;
    BASE* base = &tx_bases[veh->home_base_id];
    int res = want_convoy(veh->faction_id, veh->x, veh->y, sq);
    bool prefer_min = base->nutrient_surplus > 5;
    int v1 = 0;
    if (res) {
        v1 = (sq->items & TERRA_FOREST ? 1 : 2);
    }
    if (res && v1 > 1) {
        return set_convoy(id, res);
    }
    int i = 0;
    int tx = -1;
    int ty = -1;
    TileSearch ts;
    ts.init(veh->x, veh->y, TRIAD_LAND);

    while (++i < 50 && (sq = ts.get_next()) != NULL) {
        int other = unit_in_tile(sq);
        if (other > 0 && other != veh->faction_id) {
            debug("convoy_skip %d %d %d %d %d %d\n", veh->x, veh->y,
                ts.rx, ts.ry, veh->faction_id, other);
            continue;
        }
        res = want_convoy(veh->faction_id, ts.rx, ts.ry, sq);
        if (res && pm_safety[ts.rx][ts.ry] >= PM_SAFE) {
            int v2 = (sq->items & TERRA_FOREST ? 1 : 2);
            if (prefer_min && res == RES_MINERAL && v2 > 1) {
                return set_move_to(id, ts.rx, ts.ry);
            } else if (!prefer_min && res == RES_NUTRIENT) {
                return set_move_to(id, ts.rx, ts.ry);
            }
            if (res == RES_MINERAL && v2 > v1) {
                tx = ts.rx;
                ty = ts.ry;
                v1 = v2;
            }
        }
    }
    if (tx >= 0)
        return set_move_to(id, tx, ty);
    if (v1 > 0)
        return set_convoy(id, RES_MINERAL);
    return veh_skip(id);
}

bool safe_path(const TileSearch& ts, int faction) {
    int i = 0;
    const PathNode* node = &ts.newtiles[ts.cur];

    while (node->dist > 0 && ++i < 20) {
        assert(node->prev >= 0);
        MAP* sq = mapsq(node->x, node->y);
        if (pm_safety[node->x][node->y] < PM_SAFE) {
            debug("path_unsafe %d %d %d\n", faction, node->x, node->y);
            return false;
        }
        if (sq && sq->owner > 0 && sq->owner != faction
        && ~tx_factions[faction].diplo_status[sq->owner] & DIPLO_PACT) {
            debug("path_foreign %d %d %d\n", faction, node->x, node->y);
            return false;
        }
        node = &ts.newtiles[node->prev];
    }
    return true;
}

bool valid_colony_path(int id) {
    VEH* veh = &tx_vehicles[id];
    int faction = veh->faction_id;
    int tx = veh->waypoint_1_x;
    int ty = veh->waypoint_1_y;

    if (at_target(veh) || !can_build_base(tx, ty, faction, veh_triad(id))) {
        return false;
    }
    if (~veh->state & VSTATE_UNK_40000 && (veh->state & VSTATE_UNK_2000
    || map_range(veh->x, veh->y, tx, ty) > 10)) {
        return false;
    }
    return true;
}

bool can_build_base(int x, int y, int faction, int triad) {
    if ((x + y)%2) // Invalid map coordinates
        return false;
    MAP* sq = mapsq(x, y);
    if (!sq || y < 3 || y >= *map_axis_y-3 || (~sq->landmarks & LM_JUNGLE && pm_former[x][y] > 0))
        return false;
    if ((sq->rocks & TILE_ROCKY && !is_ocean(sq)) || (sq->items & (BASE_DISALLOWED | IMP_ADVANCED)))
        return false;
    if (sq->owner > 0 && faction != sq->owner && !at_war(faction, sq->owner))
        return false;
    if (non_ally_in_tile(faction, sq))
        return false;
    if (sq->landmarks & LM_VOLCANO && sq->art_ref_id == 0)
        return false;
    if (sq->landmarks & LM_JUNGLE && !is_ocean(sq) && *current_turn > 60) {
        if (bases_in_range(x, y, 1) > 0 || bases_in_range(x, y, 2) > 1) {
            return false;
        }
    } else if (bases_in_range(x, y, 2) > 0) {
        return false;
    }
    if (triad != TRIAD_SEA && !is_ocean(sq)) {
        return true;
    } else if ((triad == TRIAD_SEA || triad == TRIAD_AIR) && is_ocean_shelf(sq)) {
        return true;
    }
    return false;
}

int base_tile_score(int x1, int y1, int range, int triad) {
    const int priority[][2] = {
        {TERRA_RIVER, 1},
        {TERRA_FUNGUS, -2},
        {TERRA_FARM, 2},
        {TERRA_FOREST, 2},
        {TERRA_MONOLITH, 4},
    };
    MAP* sq = mapsq(x1, y1);
    int score = (!is_ocean(sq) && sq->items & TERRA_RIVER ? 4 : 0);
    int land = 0;
    range *= (triad == TRIAD_LAND ? 3 : 1);

    for (int i=0; i<20; i++) {
        int x2 = wrap(x1 + offset_tbl[i][0]);
        int y2 = y1 + offset_tbl[i][1];
        sq = mapsq(x2, y2);
        if (sq) {
            int items = sq->items;
            score += (tx_bonus_at(x2, y2) ? 6 : 0);
            if (sq->landmarks && !(sq->landmarks & (LM_DUNES | LM_SARGASSO | LM_UNITY))) {
                score += (sq->landmarks & LM_JUNGLE ? 3 : 2);
            }
            if (i < 8) { // Only adjacent tiles
                if (triad == TRIAD_SEA && !is_ocean(sq)
                && nearby_tiles(x2, y2, TRIAD_LAND, 20) >= 20 && ++land < 3) {
                    score += (sq->owner < 1 ? 20 : 3);
                }
                if (is_ocean_shelf(sq)) {
                    score += (triad == TRIAD_SEA ? 3 : 2);
                }
            }
            if (!is_ocean(sq)) {
                if (sq->level & TILE_RAINY) {
                    score += 2;
                }
                if (sq->level & TILE_MOIST && sq->rocks & TILE_ROLLING) {
                    score += 2;
                }
            }
            for (const int* p : priority) if (items & p[0]) score += p[1];
        }
    }
    return score - range + random(4) + min(0, pm_safety[x1][y1]);
}

int colony_move(int id) {
    VEH* veh = &tx_vehicles[id];
    MAP* sq = mapsq(veh->x, veh->y);
    int faction = veh->faction_id;
    int triad = veh_triad(id);
    if (defend_tile(veh, sq) || veh->iter_count > 7) {
        return veh_skip(id);
    }
    if (pm_safety[veh->x][veh->y] < PM_SAFE) {
        return escape_move(id);
    }
    if ((at_target(veh) || triad == TRIAD_LAND) && can_build_base(veh->x, veh->y, faction, triad)) {
        tx_action_build(id, 0);
        return SYNC;
    }
    if (is_ocean(sq) && triad == TRIAD_LAND) {
        if (!has_transport(veh->x, veh->y, faction)) {
            needferry.insert({veh->x, veh->y});
            return veh_skip(id);
        }
        for (const int* t : offset) {
            int x2 = wrap(veh->x + t[0]);
            int y2 = veh->y + t[1];
            if (non_combat_move(x2, y2, faction, triad) && has_base_sites(x2, y2, faction, TRIAD_LAND)) {
                debug("colony_trans %d %d -> %d %d\n", veh->x, veh->y, x2, y2);
                return set_move_to(id, x2, y2);
            }
        }
    }
    if (!valid_colony_path(id)) {
        int i = 0;
        int k = 0;
        int tscore = INT_MIN;
        int tx = -1;
        int ty = -1;
        int limit = (!((*current_turn + id) % 8) ? 500 : 200);
        TileSearch ts;
        ts.init(veh->x, veh->y, triad, 2);
        while (++i < limit && k < 30 && (sq = ts.get_next()) != NULL) {
            if (!can_build_base(ts.rx, ts.ry, faction, triad) || !safe_path(ts, faction))
                continue;
            int score = base_tile_score(ts.rx, ts.ry, ts.dist, triad);
            k++;
            if (score > tscore) {
                tx = ts.rx;
                ty = ts.ry;
                tscore = score;
            }
        }
        if (tx >= 0) {
            debug("colony_move %d %d -> %d %d | %d %d | %d\n", veh->x, veh->y, tx, ty, faction, id, tscore);
            adjust_value(pm_former, tx, ty, 1, 1);
            // Set these flags to disable any non-Thinker unit automation.
            veh->state |= VSTATE_UNK_40000;
            veh->state &= ~VSTATE_UNK_2000;
            return set_move_to(id, tx, ty);
        }
    }
    if (!at_target(veh)) {
        return SYNC;
    }
    if (triad == TRIAD_LAND) {
        return tx_enemy_move(id);
    }
    return veh_skip(id);
}

bool can_bridge(int x, int y, int faction, MAP* sq) {
    if (is_ocean(sq) || !has_terra(faction, FORMER_RAISE_LAND, 0))
        return false;
    if (coast_tiles(x, y) < 3)
        return false;
    const int range = 4;
    TileSearch ts;
    ts.init(x, y, TRIAD_LAND);
    int k = 0;
    while (k++ < 120 && ts.get_next() != NULL);

    int n = 0;
    for (int i=-range*2; i<=range*2; i++) {
        for (int j=-range*2 + abs(i); j<=range*2 - abs(i); j+=2) {
            int x2 = wrap(x + i);
            int y2 = y + j;
            sq = mapsq(x2, y2);
            if (sq && y2 > 0 && y2 < *map_axis_y-1 && !is_ocean(sq)
            && !ts.oldtiles.count({x2, y2})) {
                n++;
            }
        }
    }
    debug("bridge %d %d %d %d\n", x, y, k, n);
    return n > 4;
}

bool can_borehole(int x, int y, int faction, int bonus, MAP* sq) {
    if (!has_terra(faction, FORMER_THERMAL_BORE, is_ocean(sq)))
        return false;
    if (!sq || sq->items & (BASE_DISALLOWED | IMP_ADVANCED) || bonus == RES_NUTRIENT)
        return false;
    // Planet factions should build boreholes only in reduced numbers.
    if (tx_factions[faction].SE_planet_base > 0 && (map_hash(x, y) % 101 > 14))
        return false;
    if (bonus == RES_NONE && sq->rocks & TILE_ROLLING)
        return false;
    if (pm_former[x][y] < 4 && !boreholes.count({x, y}))
        return false;
    int level = sq->level >> 5;
    for (const int* t : offset) {
        int x2 = wrap(x + t[0]);
        int y2 = y + t[1];
        sq = mapsq(x2, y2);
        if (!sq || sq->items & TERRA_THERMAL_BORE || boreholes.count({x2, y2}))
            return false;
        int level2 = sq->level >> 5;
        if (level2 < level && level2 > LEVEL_OCEAN_SHELF)
            return false;
    }
    return true;
}

bool can_farm(int x, int y, int faction, int bonus, bool has_nut, MAP* sq) {
    bool rain = sq->level & (TILE_RAINY | TILE_MOIST);
    if (!has_terra(faction, FORMER_FARM, is_ocean(sq)))
        return false;
    if (bonus == RES_NUTRIENT && (has_nut || (rain && sq->rocks & TILE_ROLLING)))
        return true;
    if (bonus == RES_ENERGY || bonus == RES_MINERAL)
        return false;
    if (sq->landmarks & LM_JUNGLE && !has_nut)
        return false;
    if (nearby_items(x, y, 1, TERRA_FOREST) < (sq->items & TERRA_FOREST ? 4 : 1))
        return false;
    if (!has_nut && nearby_items(x, y, 1, TERRA_FARM | TERRA_SOLAR) > 2)
        return false;
    if (rain && sq->rocks & TILE_ROLLING)
        return nearby_items(x, y, 1, TERRA_FARM | TERRA_CONDENSER) < 3;
    return (has_nut && !(x % 2) && !(y % 2) && !(abs(x-y) % 4));
}

bool can_fungus(int x, int y, int faction, int bonus, MAP* sq) {
    if (sq->items & (TERRA_BASE_IN_TILE | TERRA_MONOLITH | IMP_ADVANCED))
        return false;
    return !can_borehole(x, y, faction, bonus, sq) && nearby_items(x, y, 1, TERRA_FUNGUS) < 5;
}

bool can_road(int x, int y, int faction, MAP* sq) {
    assert(faction > 0 && faction < 8);
    if (is_ocean(sq) || !has_terra(faction, FORMER_ROAD, 0)
    || sq->items & (TERRA_ROAD | TERRA_BASE_IN_TILE))
        return false;
    if (~sq->items & TERRA_BASE_RADIUS && pm_roads[x][y] < 1)
        return false;
    if (sq->items & TERRA_FUNGUS && (!has_tech(faction, tx_basic->tech_preq_build_road_fungus)
    || (!build_tubes && has_project(faction, FAC_XENOEMPATHY_DOME))))
        return false;
    if (pm_roads[x][y] > 0 || sq->items & (TERRA_FARM | TERRA_MINE | TERRA_CONDENSER | TERRA_THERMAL_BORE))
        return true;
    int i = 0;
    int r[] = {0,0,0,0,0,0,0,0};
    for (const int* t : offset) {
        sq = mapsq(wrap(x + t[0]), y + t[1]);
        if (!is_ocean(sq) && sq->owner == faction) {
            if (sq->items & (TERRA_ROAD | TERRA_BASE_IN_TILE)) {
                r[i] = 1;
            }
        }
        i++;
    }
    // Determine if we should connect roads on opposite sides of the tile
    if ((r[0] && r[4] && !r[2] && !r[6])
    || (r[2] && r[6] && !r[0] && !r[4])
    || (r[1] && r[5] && !((r[2] && r[4]) || (r[0] && r[6])))
    || (r[3] && r[7] && !((r[0] && r[2]) || (r[4] && r[6])))) {
        return true;
    }
    return false;
}

bool can_magtube(int x, int y, int faction, MAP* sq) {
    assert(faction > 0 && faction < 8);
    if (is_ocean(sq) || pm_roads[x][y] < 1 || sq->items & (TERRA_MAGTUBE | TERRA_BASE_IN_TILE)) {
        return false;
    }
    return has_terra(faction, FORMER_MAGTUBE, 0) && sq->items & TERRA_ROAD
        && (~sq->items & TERRA_FUNGUS || has_tech(faction, tx_basic->tech_preq_build_road_fungus));
}

bool can_sensor(int x, int y, int faction, MAP* sq) {
    assert(faction > 0 && faction < 8);
    return has_terra(faction, FORMER_SENSOR, is_ocean(sq))
        && ~sq->items & TERRA_SENSOR
        && !nearby_items(x, y, 2, TERRA_SENSOR)
        && (~sq->items & TERRA_FUNGUS || has_tech(faction, tx_basic->tech_preq_improv_fungus))
        && *current_turn > random(20);
}

int select_item(int x, int y, int faction, MAP* sq) {
    int items = sq->items;
    bool sea = is_ocean(sq);
    if (items & TERRA_BASE_IN_TILE || (sea && !is_ocean_shelf(sq))
    || (sq->landmarks & LM_VOLCANO && sq->art_ref_id == 0)) {
        return -1;
    }
    int bonus = tx_bonus_at(x, y);
    bool rocky_sq = sq->rocks & TILE_ROCKY;
    bool has_eco = has_terra(faction, FORMER_CONDENSER, 0);
    bool has_min = has_tech(faction, tx_basic->tech_preq_allow_3_minerals_sq);
    bool has_nut = has_tech(faction, tx_basic->tech_preq_allow_3_nutrients_sq);
    bool road = can_road(x, y, faction, sq);

    if (can_magtube(x, y, faction, sq)) {
        return FORMER_MAGTUBE;
    }
    if (items & TERRA_FUNGUS) {
        if (plans[faction].keep_fungus && can_fungus(x, y, faction, bonus, sq)) {
            if (road)
                return FORMER_ROAD;
            else if (can_sensor(x, y, faction, sq))
                return FORMER_SENSOR;
            return -1;
        }
        return FORMER_REMOVE_FUNGUS;
    }
    if (can_bridge(x, y, faction, sq)) {
        int cost = tx_terraform_cost(x, y, faction);
        if (cost < tx_factions[faction].energy_credits/10) {
            return FORMER_RAISE_LAND;
        }
    }
    if (~items & TERRA_BASE_RADIUS || items & (TERRA_MONOLITH | TERRA_FUNGUS)) {
        return (road ? FORMER_ROAD : -1);
    }
    if (road) {
        return FORMER_ROAD;
    }
    if (plans[faction].plant_fungus && can_fungus(x, y, faction, bonus, sq)) {
        return FORMER_PLANT_FUNGUS;
    }
    if (sea) {
        bool improved = items & (TERRA_MINE | TERRA_SOLAR | TERRA_SENSOR);
        if (~items & TERRA_FARM && has_terra(faction, FORMER_FARM, sea))
            return FORMER_FARM;
        else if (!improved && bonus == RES_NONE && can_sensor(x, y, faction, sq))
            return FORMER_SENSOR;
        else if (!improved && has_terra(faction, FORMER_SOLAR, sea) && bonus != RES_MINERAL
        && (bonus == RES_NUTRIENT || nearby_items(x, y, 1, TERRA_SOLAR)+1 < nearby_items(x, y, 1, TERRA_MINE)))
            return FORMER_SOLAR;
        else if (!improved && has_terra(faction, FORMER_MINE, sea))
            return FORMER_MINE;
        return -1;
    }
    if ((has_min || bonus != RES_NONE) && can_borehole(x, y, faction, bonus, sq))
        return FORMER_THERMAL_BORE;
    if (items & TERRA_THERMAL_BORE)
        return -1;
    if (rocky_sq && ~items & TERRA_MINE && (has_min || bonus == RES_MINERAL)
    && has_terra(faction, FORMER_MINE, sea))
        return FORMER_MINE;
    if (rocky_sq && (bonus == RES_NUTRIENT || sq->landmarks & LM_JUNGLE)
    && has_terra(faction, FORMER_LEVEL_TERRAIN, sea))
        return FORMER_LEVEL_TERRAIN;
    if (!rocky_sq && can_farm(x, y, faction, bonus, has_nut, sq)) {
        bool improved = items & (TERRA_CONDENSER | TERRA_SOLAR);
        if (!has_nut) {
            if (!improved && sq->level & TILE_RAINY && has_terra(faction, FORMER_SOLAR, sea))
                return FORMER_SOLAR;
            else if (~items & TERRA_FARM && sq->level & TILE_MOIST && sq->rocks & TILE_ROLLING)
                return FORMER_FARM;
        }
        if (has_nut || bonus == RES_NUTRIENT) {
            if (~items & TERRA_FARM)
                return FORMER_FARM;
            else if (~items & TERRA_CONDENSER && has_eco)
                return FORMER_CONDENSER;
            else if (~items & TERRA_SOIL_ENR && has_terra(faction, FORMER_SOIL_ENR, sea))
                return FORMER_SOIL_ENR;
            else if (!has_eco && !improved && has_terra(faction, FORMER_SOLAR, sea)
            && (bonus != RES_NONE || items & TERRA_RIVER))
                return FORMER_SOLAR;
        }
    } else if (!rocky_sq && !(items & IMP_ADVANCED)) {
        if (can_sensor(x, y, faction, sq))
            return FORMER_SENSOR;
        else if (~items & TERRA_FOREST && has_terra(faction, FORMER_FOREST, sea))
            return FORMER_FOREST;
    }
    return -1;
}

int former_tile_score(int x1, int y1, int x2, int y2, int faction, MAP* sq) {
    const int priority[][2] = {
        {TERRA_RIVER, 2},
        {TERRA_FARM, -3},
        {TERRA_SOLAR, -2},
        {TERRA_FOREST, -4},
        {TERRA_MINE, -4},
        {TERRA_CONDENSER, -4},
        {TERRA_SOIL_ENR, -4},
        {TERRA_THERMAL_BORE, -8},
    };
    int bonus = tx_bonus_at(x2, y2);
    int range = map_range(x1, y1, x2, y2);
    int items = sq->items;
    int score = (sq->landmarks ? 3 : 0);

    if (bonus && !(items & IMP_ADVANCED)) {
        bool bh = (bonus == RES_MINERAL || bonus == RES_ENERGY) && can_borehole(x2, y2, faction, bonus, sq);
        int w = (bh || !(items & IMP_SIMPLE) ? 5 : 1);
        score += w * (bonus == RES_NUTRIENT ? 3 : 2);
    }
    for (const int* p : priority) {
        if (items & p[0]) {
            score += p[1];
        }
    }
    if (items & TERRA_FUNGUS) {
        score += (items & IMP_ADVANCED ? 20 : 0);
        score += (plans[faction].keep_fungus ? -10 : -2);
        score += (plans[faction].plant_fungus && (items & TERRA_ROAD) ? -8 : 0);
    }
    if (items & (TERRA_FOREST | TERRA_SENSOR) && can_road(x2, y2, faction, sq)) {
        score += 8;
    }
    if (pm_roads[x2][y2] > 0 && (~items & TERRA_ROAD || (build_tubes && ~items & TERRA_MAGTUBE))) {
        score += 12;
    }
    return score - range/2 + min(8, pm_former[x2][y2]) + min(0, pm_safety[x2][y2]);
}

int former_move(int id) {
    VEH* veh = &tx_vehicles[id];
    int faction = veh->faction_id;
    int triad = veh_triad(id);
    int x = veh->x;
    int y = veh->y;
    MAP* sq = mapsq(x, y);
    bool safe = pm_safety[x][y] >= PM_SAFE;
    int item;
    if (!sq || sq->owner != faction) {
        return tx_enemy_move(id);
    }
    if (defend_tile(veh, sq)) {
        return veh_skip(id);
    }
    debug
    (
        "former parameters: x=%d, y=%d, safe=%d, move_status=%s\n",
        veh->x,
        veh->y,
        safe,
        MOVE_STATUS[veh->move_status].c_str()
    )
    ;

    if (safe || (veh->move_status >= 16 && veh->move_status < 22))
    {
        if
        (
            (veh->move_status >= 4 && veh->move_status < 24)
            ||
            (triad != TRIAD_LAND && !at_target(veh))
        )
        {
            return SYNC;
        }
        item = select_item(x, y, faction, sq);
        if (item >= 0) {
            int cost = 0;
            pm_former[x][y] -= 2;
            if (item == FORMER_RAISE_LAND) {
                cost = tx_terraform_cost(x, y, faction);
                tx_factions[faction].energy_credits -= cost;
            }
            debug("former_action %d %d | %d %d | %d %s\n",
                x, y, faction, id, cost, tx_terraform[item].name);
            return set_action(id, item+4, *tx_terraform[item].shortcuts);
        }
    } else if (!safe) {
        return escape_move(id);
    }
    int i = 0;
    int limit = (triad == TRIAD_LAND ? 50 : 100);
    int tscore = INT_MIN;
    int action = -1;
    int tx = -1;
    int ty = -1;
    int bx = x;
    int by = y;
    if (veh->home_base_id >= 0) {
        bx = tx_bases[veh->home_base_id].x;
        by = tx_bases[veh->home_base_id].y;
    }
    TileSearch ts;
    ts.init(x, y, triad);

    while (++i < limit && (sq = ts.get_next()) != NULL) {
        if (sq->owner != faction || sq->items & TERRA_BASE_IN_TILE
        || pm_safety[ts.rx][ts.ry] < PM_SAFE
        || (pm_former[ts.rx][ts.ry] < 1 && pm_roads[ts.rx][ts.ry] < 1)
        || other_in_tile(faction, sq))
            continue;
        int score = former_tile_score(bx, by, ts.rx, ts.ry, faction, sq);
        if (score > tscore && (item = select_item(ts.rx, ts.ry, faction, sq)) != -1) {
            tx = ts.rx;
            ty = ts.ry;
            tscore = score;
            action = item;
        }
    }
    if (tx >= 0) {
        pm_former[tx][ty] -= 2;
        debug("former_move %d %d -> %d %d | %d %d | %d %s\n",
            x, y, tx, ty, faction, id, tscore, (action >= 0 ? tx_terraform[action].name : ""));
        return set_move_to(id, tx, ty);
    } else if (!random(4)) {
        return set_move_to(id, bx, by);
    }
    debug("former_skip %d %d | %d %d\n", x, y, faction, id);
    return veh_skip(id);
}

int best_defender(int x, int y, int faction, int att_id) {
    int id = tx_veh_at(x, y);
    if (id < 0 || ~tx_vehicles[id].visibility & (1 << faction))
        return -1;
    return tx_best_defender(id, att_id, 0);
}

int combat_value(int id, int power, int moves, int mov_rate) {
    VEH* veh = &tx_vehicles[id];
    return max(1, power * (10 - veh->damage_taken) * min(moves, mov_rate) / mov_rate);
}

double battle_eval(int id1, int id2, int moves, int mov_rate) {
    int s1;
    int s2;
    tx_battle_compute(id1, id2, (int)&s1, (int)&s2, 0);
    int v1 = combat_value(id1, s1, moves, mov_rate);
    int v2 = combat_value(id2, s2, mov_rate, mov_rate);
    return 1.0 * v1/v2;
}

int combat_move(int id) {
    VEH* veh = &tx_vehicles[id];
    MAP* sq = mapsq(veh->x, veh->y);
    UNIT* u = &tx_units[veh->proto_id];
    int faction = veh->faction_id;
    if (is_ocean(sq) || u->ability_flags & ABL_ARTILLERY)
        return tx_enemy_move(id);
    if (pm_enemy_near[veh->x][veh->y] < 1 || veh->iter_count > 7) {
        if (need_heals(veh))
            return escape_move(id);
        return tx_enemy_move(id);
    }
    bool at_base = sq->items & TERRA_BASE_IN_TILE;
    int mov_rate = tx_basic->mov_rate_along_roads;
    int moves = veh_speed(id) - veh->road_moves_spent;
    int max_dist = moves;
    int defenders = 0;

    if (at_base) {
        for (int i=0; i<*total_num_vehicles; i++) {
            VEH* v = &tx_vehicles[i];
            if (veh->x == v->x && veh->y == v->y && v->damage_taken < 5
            && tx_units[v->proto_id].weapon_type <= WPN_PSI_ATTACK) {
                defenders++;
            }
        }
        max_dist = (defenders > 1 ? moves : 1);
    }
    int i = 0;
    int tx = -1;
    int ty = -1;
    int id2 = -1;
    double odds = (at_base ? 1.5 - 0.1 * min(3, defenders) : 1.1);
    TileSearch ts;
    ts.init(veh->x, veh->y, NEAR_ROADS);

    while (i++ < 50 && (sq = ts.get_next()) != NULL && ts.dist <= max_dist) {
        bool to_base = sq->items & TERRA_BASE_IN_TILE;
        int fac2 = unit_in_tile(sq);
        assert(veh->x != ts.rx || veh->y != ts.ry);
        assert(map_range(veh->x, veh->y, ts.rx, ts.ry) <= ts.dist);
        assert((at_base && to_base) < ts.dist);

        if (at_war(faction, fac2) && (id2 = best_defender(ts.rx, ts.ry, faction, id)) != -1) {
            VEH* veh2 = &tx_vehicles[id2];
            UNIT* u2 = &tx_units[veh2->proto_id];
            max_dist = min(ts.dist, max_dist);
            if (ts.dist > 1 && ~veh2->visibility & (1 << faction))
                continue;
            if (!to_base && veh_triad(id2) == TRIAD_AIR && ~u->ability_flags & ABL_AIR_SUPERIORITY)
                continue;
            double v1 = battle_eval(id, id2, moves - ts.dist + 1, mov_rate);
            double v2 = (sq->owner == faction ? 0.1 : 0.0)
                + (to_base ? 0.0 : 0.05 * (pm_enemy[ts.rx][ts.ry]))
                + min(12, abs(offense_value(u2)))*(u2->chassis_type == CHS_INFANTRY ? 0.008 : 0.02);
            double v3 = min(1.6, v1) + min(0.5, max(-0.5, v2));

            debug("attack_odds %2d %2d -> %2d %2d | %d %d %d %d | %d %d %.4f %.4f %.4f %.4f | %d %d %s | %d %d %s\n",
                veh->x, veh->y, ts.rx, ts.ry, ts.dist, max_dist, at_base, to_base,
                pm_enemy[ts.rx][ts.ry], pm_safety[ts.rx][ts.ry], v1, v2, v3, odds,
                id, faction, u->name, id2, fac2, u2->name);
            if (v3 > odds) {
                tx = ts.rx;
                ty = ts.ry;
                odds = v3;
            }
        } else if (to_base && at_war(faction, sq->owner) && pm_enemy[ts.rx][ts.ry] < 1) {
            tx = ts.rx;
            ty = ts.ry;
            break;
        }
    }
    if (tx >= 0) {
        debug("attack_set  %2d %2d -> %2d %2d\n", veh->x, veh->y, tx, ty);
        return set_move_to(id, tx, ty);
    }
    if (need_heals(veh)) {
        return escape_move(id);
    }
    return tx_enemy_move(id);
}

bool isSafe(int x, int y)
{
	return pm_safety[x][y] >= PM_SAFE;
}

