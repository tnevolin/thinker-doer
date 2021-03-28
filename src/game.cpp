#include "game.h"

// external variables definitions

char* prod_name(int prod) {
    if (prod >= 0)
        return (char*)&(tx_units[prod].name);
    else
        return (char*)(tx_facility[-prod].name);
}

/*
Calculates mineral cost for given faction, production item, and possibly base.
If base is not negative then its cost modifying facilites are taken into account: Skunkworks and Brood Pits.
*/
int mineral_cost(int baseId, int itemId)
{
	BASE *base = &(tx_bases[baseId]);

	return (itemId >= 0 ? tx_veh_cost(itemId, baseId, 0) : tx_facility[-itemId].cost) * tx_cost_factor(base->faction_id, 1, -1);

}

bool has_tech(int faction, int tech) {
    assert(faction > 0 && faction < 8);
    if (tech == TECH_None)
        return true;
    return tech >= 0 && tx_tech_discovered[tech] & (1 << faction);
}

bool has_ability(int faction, int abl) {
    return has_tech(faction, tx_ability[abl].preq_tech);
}

bool has_chassis(int faction, int chs) {
    return has_tech(faction, tx_chassis[chs].preq_tech);
}

bool has_weapon(int faction, int wpn) {
    return has_tech(faction, tx_weapon[wpn].preq_tech);
}

bool has_terra(int faction, int act, bool ocean) {
    int preq_tech = (ocean ? tx_terraform[act].preq_tech_sea : tx_terraform[act].preq_tech);
    if ((act == FORMER_RAISE_LAND || act == FORMER_LOWER_LAND)
    && *game_rules & RULES_SCN_NO_TERRAFORMING) {
        return false;
    }
    if (act >= FORMER_CONDENSER && act <= FORMER_LOWER_LAND
    && has_project(faction, FAC_WEATHER_PARADIGM)) {
        return preq_tech != TECH_Disable;
    }
    return has_tech(faction, preq_tech);
}

bool has_project(int faction, int id) {
    /* If faction_id is negative, check if anyone has built the project. */
    assert(faction != 0 && id >= PROJECT_ID_FIRST);
    int i = tx_secret_projects[id-PROJECT_ID_FIRST];
    return i >= 0 && (faction < 0 || tx_bases[i].faction_id == faction);
}

bool has_facility(int base_id, int id) {
    assert(base_id >= 0 && id > 0 && id <= FAC_EMPTY_SP_64);
    if (id >= PROJECT_ID_FIRST) {
        return tx_secret_projects[id-PROJECT_ID_FIRST] == base_id;
    }
    int faction = tx_bases[base_id].faction_id;
    const int freebies[][2] = {
        {FAC_COMMAND_CENTER, FAC_COMMAND_NEXUS},
        {FAC_NAVAL_YARD, FAC_MARITIME_CONTROL_CENTER},
        {FAC_ENERGY_BANK, FAC_PLANETARY_ENERGY_GRID},
        {FAC_PERIMETER_DEFENSE, FAC_CITIZENS_DEFENSE_FORCE},
        {FAC_AEROSPACE_COMPLEX, FAC_CLOUDBASE_ACADEMY},
        {FAC_BIOENHANCEMENT_CENTER, FAC_CYBORG_FACTORY},
        {FAC_QUANTUM_CONVERTER, FAC_SINGULARITY_INDUCTOR},
    };
    for (const int* p : freebies) {
        if (p[0] == id && has_project(faction, p[1])) {
            return true;
        }
    }
    return tx_bases[base_id].facilities_built[id/8] & (1 << (id % 8));
}

bool can_build(int base_id, int id) {
    assert(base_id >= 0 && id > 0 && id <= FAC_EMPTY_SP_64);
    BASE* base = &tx_bases[base_id];
    int faction = base->faction_id;
    Faction* f = &tx_factions[faction];
    if (id == FAC_HEADQUARTERS && find_hq(faction) >= 0) {
        return false;
    }
    if (id == FAC_RECYCLING_TANKS && has_facility(base_id, FAC_PRESSURE_DOME)) {
        return false;
    }
    if (id == FAC_HOLOGRAM_THEATRE && (has_project(faction, FAC_VIRTUAL_WORLD)
    || !has_facility(base_id, FAC_RECREATION_COMMONS))) {
        return false;
    }
    if ((id == FAC_RECREATION_COMMONS || id == FAC_HOLOGRAM_THEATRE)
    && has_project(faction, FAC_TELEPATHIC_MATRIX)) {
        return false;
    }
    if ((id == FAC_HAB_COMPLEX || id == FAC_HABITATION_DOME) && base->nutrient_surplus < 2) {
        return false;
    }
    if (id >= PROJECT_ID_FIRST && id <= PROJECT_ID_LAST) {
        if (tx_secret_projects[id-PROJECT_ID_FIRST] != PROJECT_UNBUILT
        || *game_rules & RULES_SCN_NO_BUILDING_SP) {
            return false;
        }
    }
    if (id == FAC_VOICE_OF_PLANET && ~*game_rules & RULES_VICTORY_TRANSCENDENCE) {
        return false;
    }
    if (id == FAC_ASCENT_TO_TRANSCENDENCE) {
        return has_project(-1, FAC_VOICE_OF_PLANET)
            && !has_project(-1, FAC_ASCENT_TO_TRANSCENDENCE)
            && *game_rules & RULES_VICTORY_TRANSCENDENCE;
    }
    if (id == FAC_SUBSPACE_GENERATOR) {
        if (base->pop_size < tx_basic->base_size_subspace_gen)
            return false;
        int n = 0;
        for (int i=0; i<*total_num_bases; i++) {
            BASE* b = &tx_bases[i];
            if (b->faction_id == faction && has_facility(i, FAC_SUBSPACE_GENERATOR)
            && b->pop_size >= tx_basic->base_size_subspace_gen
            && ++n >= tx_basic->subspace_gen_req)
                return false;
        }
    }
    if (id >= FAC_SKY_HYDRO_LAB && id <= FAC_ORBITAL_DEFENSE_POD) {
        int n = prod_count(faction, -id, base_id);
        if ((id == FAC_SKY_HYDRO_LAB && f->satellites_nutrient + n >= conf.max_sat)
        || (id == FAC_ORBITAL_POWER_TRANS && f->satellites_energy + n >= conf.max_sat)
        || (id == FAC_NESSUS_MINING_STATION && f->satellites_mineral + n >= conf.max_sat)
        || (id == FAC_ORBITAL_DEFENSE_POD && f->satellites_ODP + n >= conf.max_sat)) {
            return false;
        }
    }
    /* Rare special case if the game engine reaches the global unit limit. */
    if (id == FAC_STOCKPILE_ENERGY) {
        return (*current_turn + base_id) % 4 > 0 || !can_build_unit(faction, -1);
    }
    return has_tech(faction, tx_facility[id].preq_tech) && !has_facility(base_id, id);
}

bool can_build_unit(int faction, int id) {
    assert(faction > 0 && faction < 8 && id >= -1);
    UNIT* u = &tx_units[id];
    if (id >= 0 && id < 64 && u->preq_tech != TECH_None && !has_tech(faction, u->preq_tech)) {
        return false;
    }
    return *total_num_vehicles < 2000;
}

bool is_human(int faction) {
    return *human_players & (1 << faction);
}

bool ai_faction(int faction) {
    /* Exclude native life since Thinker AI routines don't apply to them. */
    return faction > 0 && !(*human_players & (1 << faction));
}

bool ai_enabled(int faction) {
    return ai_faction(faction) && faction <= conf.factions_enabled;
}

bool at_war(int faction1, int faction2) {
    if (faction1 == faction2 || faction1 < 0 || faction2 < 0)
        return false;
    return !faction1 || !faction2
        || tx_factions[faction1].diplo_status[faction2] & DIPLO_VENDETTA;
}

bool un_charter() {
    return *un_charter_repeals <= *un_charter_reinstates;
}

int prod_count(int faction, int id, int skip) {
    int n = 0;
    for (int i=0; i<*total_num_bases; i++) {
        BASE* base = &tx_bases[i];
        if (base->faction_id == faction && base->queue_items[0] == id && i != skip) {
            n++;
        }
    }
    return n;
}

int find_hq(int faction) {
    for(int i=0; i<*total_num_bases; i++) {
        BASE* base = &tx_bases[i];
        if (base->faction_id == faction && has_facility(i, FAC_HEADQUARTERS)) {
            return i;
        }
    }
    return -1;
}

int veh_triad(int id) {
    return unit_triad(tx_vehicles[id].proto_id);
}

int veh_speed(int id) {
    return tx_veh_speed(id, 0);
}

int veh_speed_without_roads(int id) {
    return tx_veh_speed(id, 0) / tx_basic->mov_rate_along_roads;
}

int unit_chassis_speed(int id)
{
	return tx_chassis[tx_units[id].chassis_type].speed;
}

int veh_chassis_speed(int id)
{
	VEH *vehicle = &tx_vehicles[id];
	UNIT *unit = &tx_units[vehicle->proto_id];
	R_Chassis *chassis = &tx_chassis[unit->chassis_type];
	return chassis->speed;
}

int unit_triad(int id) {
    int triad = tx_chassis[tx_units[id].chassis_type].triad;
    assert(triad == TRIAD_LAND || triad == TRIAD_SEA || triad == TRIAD_AIR);
    return triad;
}

int unit_speed(int id) {
    return tx_chassis[tx_units[id].chassis_type].speed;
}

int best_armor(int faction, bool cheap) {
    int ci = ARM_NO_ARMOR;
    int cv = 4;
    for (int i=ARM_SYNTHMETAL_ARMOR; i<=ARM_RESONANCE_8_ARMOR; i++) {
        if (has_tech(faction, tx_defense[i].preq_tech)) {
            int val = tx_defense[i].defense_value;
            int cost = tx_defense[i].cost;
            if (cheap && (cost > 6 || cost > val))
                continue;
            int iv = val * (i >= ARM_PULSE_3_ARMOR ? 5 : 4);
            if (iv > cv) {
                cv = iv;
                ci = i;
            }
        }
    }
    return ci;
}

int best_weapon(int faction) {
    int ci = WPN_HAND_WEAPONS;
    int cv = 4;
    for (int i=WPN_LASER; i<=WPN_STRING_DISRUPTOR; i++) {
        if (has_tech(faction, tx_weapon[i].preq_tech)) {
            int iv = tx_weapon[i].offense_value *
                (i == WPN_RESONANCE_LASER || i == WPN_RESONANCE_BOLT ? 5 : 4);
            if (iv > cv) {
                cv = iv;
                ci = i;
            }
        }
    }
    return ci;
}

int best_reactor(int faction) {
    for (const int r : {REC_SINGULARITY, REC_QUANTUM, REC_FUSION}) {
        if (has_tech(faction, tx_reactor[r - 1].preq_tech)) {
            return r;
        }
    }
    return REC_FISSION;
}

int offense_value(UNIT* u) {
    if (u->weapon_type == WPN_CONVENTIONAL_PAYLOAD)
        return tx_factions[*active_faction].best_weapon_value * u->reactor_type;
    return tx_weapon[u->weapon_type].offense_value * u->reactor_type;
}

int defense_value(UNIT* u) {
    return tx_defense[u->armor_type].defense_value * u->reactor_type;
}

int random(int n) {
    return (n >= 1 ? rand() % n : 0);
}

double random_double(double scale) {
    return scale * ((double)rand() / (double)(RAND_MAX + 1));
}

int map_hash(int x, int y) {
    return ((*map_random_seed ^ x) * 127) ^ (y * 179);
}

double lerp(double a, double b, double t) {
    return a + t*(b-a);
}

int wrap(int a) {
    if (!*map_toggle_flat)
        return (a < 0 ? a + *map_axis_x : a % *map_axis_x);
    else
        return a;
}

int map_range(int x1, int y1, int x2, int y2) {
    int xd = abs(x1-x2);
    int yd = abs(y1-y2);
    if (!*map_toggle_flat && xd > *map_axis_x/2)
        xd = *map_axis_x - xd;
    return (xd + yd)/2;
}

int point_range(const Points& S, int x, int y) {
    int z = MAPSZ;
    for (auto& p : S) {
        z = min(z, map_range(x, y, p.first, p.second));
    }
    return z;
}

double mean_range(const Points& S, int x, int y) {
    int n = 0;
    int sum = 0;
    for (auto& p : S) {
        sum += map_range(x, y, p.first, p.second);
        n++;
    }
    return (n > 0 ? (double)sum/n : 0);
}

MAP* mapsq(int x, int y) {
    assert((x + y)%2 == 0);
    if (x >= 0 && y >= 0 && x < *map_axis_x && y < *map_axis_y) {
        int i = x/2 + (*map_half_x) * y;
        return &((*tx_map_ptr)[i]);
    } else {
        return NULL;
    }
}

int unit_in_tile(MAP* sq) {
    if (!sq || (sq->flags & 0xf) == 0xf)
        return -1;
    return sq->flags & 0xf;
}

int set_move_to(int id, int x, int y) {
    VEH* veh = &tx_vehicles[id];
    veh->waypoint_1_x = x;
    veh->waypoint_1_y = y;
    veh->move_status = ORDER_MOVE_TO;
    veh->status_icon = 'G';
    return SYNC;
}

int set_road_to(int id, int x, int y) {
    VEH* veh = &tx_vehicles[id];
    veh->waypoint_1_x = x;
    veh->waypoint_1_y = y;
    veh->move_status = ORDER_ROAD_TO;
    veh->status_icon = 'R';
    return SYNC;
}

/*
Send unit to destination building a road/tube on a way if applicable.
*/
int set_move_road_tube_to(int id, int x, int y)
{
    VEH *veh = &tx_vehicles[id];
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

int set_action(int id, int act, char icon) {
    VEH* veh = &tx_vehicles[id];
    if (act == ORDER_THERMAL_BOREHOLE)
        boreholes.insert({veh->x, veh->y});
    veh->move_status = act;
    veh->status_icon = icon;
    veh->state &= 0xFFFEFFFF;
    return SYNC;
}

int set_convoy(int id, int res) {
    VEH* veh = &tx_vehicles[id];
    convoys.insert({veh->x, veh->y});
    veh->type_crawling = res-1;
    veh->move_status = ORDER_CONVOY;
    veh->status_icon = 'C';
    return tx_veh_skip(id);
}

int veh_skip(int id) {
    VEH* veh = &tx_vehicles[id];
    veh->waypoint_1_x = veh->x;
    veh->waypoint_1_y = veh->y;
    veh->status_icon = '-';
    if (veh->damage_taken) {
        MAP* sq = mapsq(veh->x, veh->y);
        if (sq && sq->items & TERRA_MONOLITH) {
            tx_monolith(id);
        }
    }
    return tx_veh_skip(id);
}

bool at_target(VEH* veh) {
    return veh->move_status == ORDER_NONE || (veh->waypoint_1_x < 0 && veh->waypoint_1_y < 0)
        || (veh->x == veh->waypoint_1_x && veh->y == veh->waypoint_1_y);
}

bool is_ocean(MAP* sq) {
    return (!sq || (sq->level >> 5) < LEVEL_SHORE_LINE);
}

bool is_ocean_shelf(MAP* sq) {
    return (sq && (sq->level >> 5) == LEVEL_OCEAN_SHELF);
}

bool is_ocean_deep(MAP* sq) {
    return (sq && (sq->level >> 5) <= LEVEL_OCEAN);
}

bool is_sea_base(int id) {
    MAP* sq = mapsq(tx_bases[id].x, tx_bases[id].y);
    return is_ocean(sq);
}

bool workable_tile(int x, int y, int faction) {
    assert(faction > 0 && faction < 8);
    MAP* sq = mapsq(x, y);
    if (!sq || ~sq->items & TERRA_BASE_RADIUS) {
        return false;
    }
    for (int i=0; i<20; i++) {
        sq = mapsq(wrap(x + offset_tbl[i][0]), y + offset_tbl[i][1]);
        if (sq && sq->owner == faction && sq->items & TERRA_BASE_IN_TILE) {
            return true;
        }
    }
    return false;
}

bool has_defenders(int x, int y, int faction) {
    assert(faction > 0 && faction < 8);
    for (int i=0; i<*total_num_vehicles; i++) {
        VEH* veh = &tx_vehicles[i];
        UNIT* u = &tx_units[veh->proto_id];
        if (veh->faction_id == faction && veh->x == x && veh->y == y
        && u->weapon_type <= WPN_PSI_ATTACK && unit_triad(veh->proto_id) == TRIAD_LAND) {
            return true;
        }
    }
    return false;
}

int nearby_items(int x, int y, int range, uint32_t item) {
    MAP* sq;
    int n = 0;
    for (int i=-range*2; i<=range*2; i++) {
        for (int j=-range*2 + abs(i); j<=range*2 - abs(i); j+=2) {
            sq = mapsq(wrap(x + i), y + j);
            if (sq && sq->items & item) {
                n++;
            }
        }
    }
    return n;
}

int bases_in_range(int x, int y, int range) {
    return nearby_items(x, y, range, TERRA_BASE_IN_TILE);
}

int nearby_tiles(int x, int y, int type, int limit) {
    MAP* sq;
    int n = 0;
    TileSearch ts;
    ts.init(x, y, type, 2);
    while (n < limit && (sq = ts.get_next()) != NULL) {
        n++;
    }
    return n;
}

int coast_tiles(int x, int y) {
    MAP* sq;
    int n = 0;
    for (const int* t : offset) {
        sq = mapsq(wrap(x + t[0]), y + t[1]);
        if (sq && is_ocean(sq)) {
            n++;
        }
    }
    return n;
}

int spawn_veh(int unit_id, int faction, int x, int y, int base_id) {
    int id = tx_veh_init(unit_id, faction, x, y);
    if (id >= 0) {
        tx_vehicles[id].home_base_id = base_id;
        // Set these flags to disable any non-Thinker unit automation.
        tx_vehicles[id].state |= VSTATE_UNK_40000;
        tx_vehicles[id].state &= ~VSTATE_UNK_2000;
    }
    return id;
}

char* parse_str(char* buf, int len, const char* s1, const char* s2, const char* s3, const char* s4) {
    buf[0] = '\0';
    strcat_s(buf, len, s1);
    strcat_s(buf, len, s2);
    strcat_s(buf, len, s3);
    strcat_s(buf, len, s4);
    return (strlen(buf) > 0 ? buf : NULL);
}

/*
For debugging purposes only, check if the address range is unused.
*/
void check_zeros(int* ptr, int len) {
    char buf[100];
    if (DEBUG && !(*(byte*)ptr == 0 && memcmp((byte*)ptr, (byte*)ptr + 1, len - 1) == 0)) {
        snprintf(buf, sizeof(buf), "Non-zero values detected at: 0x%06x", (int)ptr);
        MessageBoxA(0, buf, "Debug notice", MB_OK | MB_ICONINFORMATION);
        int* p = ptr;
        for (int i=0; i*sizeof(int) < (unsigned)len; i++, p++) {
            debug("LOC %08x %d: %08x\n", (int)p, i, *p);
        }
    }
}

void print_map(int x, int y) {
    MAP* m = mapsq(x, y);
    debug("MAP %2d %2d | %2d %d | %02x %02x %02x | %02x %02x %02x | %02x %02x %02x | %08x %08x\n",
        x, y, m->owner, tx_bonus_at(x, y), m->level, m->altitude, m->rocks,
        m->flags, m->region, m->visibility, m->unk_1, m->unk_2, m->art_ref_id,
        m->items, m->landmarks);
}

void print_veh(int id) {
    VEH* v = &tx_vehicles[id];
    debug("VEH %22s %3d %3d %d | %08x %04x %02x | %2d %3d | %2d %2d %2d %2d | %2d %2d %d %d %d %d\n",
        tx_units[v->proto_id].name, v->proto_id, id, v->faction_id,
        v->state, v->flags, v->visibility, v->move_status, v->status_icon,
        v->x, v->y, v->waypoint_1_x, v->waypoint_1_y,
        v->morale, v->damage_taken, v->iter_count, v->unk_1, v->probe_action, v->probe_sabotage_id);
}

void print_base(int id) {
    BASE* base = &tx_bases[id];
    int prod = base->queue_items[0];
    debug("[ turn: %d faction: %d base: %2d x: %2d y: %2d "\
        "pop: %d tal: %d dro: %d mins: %2d acc: %2d | %08x | %3d %s | %s ]\n",
        *current_turn, base->faction_id, id, base->x, base->y,
        base->pop_size, base->talent_total, base->drone_total,
        base->mineral_surplus, base->minerals_accumulated,
        base->status_flags, prod, prod_name(prod), (char*)&(base->name));
}

void TileSearch::init(int x, int y, int tp) {
    assert(tp == TRIAD_LAND || tp == TRIAD_SEA || tp == TRIAD_AIR
        || tp == NEAR_ROADS || tp == TERRITORY_LAND);
    head = 0;
    tail = 0;
    items = 0;
    roads = 0;
    owner = 0;
    y_skip = 0;
    type = tp;
    oldtiles.clear();
    sq = mapsq(x, y);
    if (sq) {
        items++;
        tail = 1;
        owner = sq->owner;
        newtiles[0] = {x, y, 0, -1};
        oldtiles.insert({x, y});
        if (!is_ocean(sq)) {
            if (sq->items & (TERRA_ROAD | TERRA_BASE_IN_TILE))
                roads |= TERRA_ROAD;
            if (sq->items & (TERRA_RIVER | TERRA_BASE_IN_TILE))
                roads |= TERRA_RIVER;
        }
    }
}

void TileSearch::init(int x, int y, int tp, int skip) {
    init(x, y, tp);
    y_skip = skip;
}

/*
Traverse current search path and check for zones of control.
*/
bool TileSearch::has_zoc(int faction) {
    int zoc = 0;
    int i = 0;
    int j = cur;
    while (j >= 0 && i++ < 20) {
        auto p = newtiles[j];
        if (tx_zoc_any(p.x, p.y, faction) && ++zoc > 1)
            return true;
        j = p.prev;
    }
    return false;
}

/*
Implement a breadth-first search of adjacent map tiles to iterate possible movement paths.
Pathnodes are also used to keep track of the route to reach the current destination.
*/
MAP* TileSearch::get_next() {
    while (items > 0) {
        bool first = oldtiles.size() == 1;
        cur = head;
        rx = newtiles[head].x;
        ry = newtiles[head].y;
        dist = newtiles[head].dist;
        head = (head + 1) % QSIZE;
        items--;
        if (!(sq = mapsq(rx, ry)))
            continue;
        bool skip = (type == TRIAD_LAND && is_ocean(sq)) ||
                    (type == TRIAD_SEA && !is_ocean(sq)) ||
                    (type == NEAR_ROADS && (is_ocean(sq) || !(sq->items & roads))) ||
                    (type == TERRITORY_LAND && (is_ocean(sq) || sq->owner != owner));
        if (!first && skip) {
            if (type == NEAR_ROADS && !is_ocean(sq))
                return sq;
            continue;
        }
        for (const int* t : offset) {
            int x2 = wrap(rx + t[0]);
            int y2 = ry + t[1];
            if (y2 >= y_skip && y2 < *map_axis_y - y_skip
            && items < QSIZE && !oldtiles.count({x2, y2})) {
                newtiles[tail] = {x2, y2, dist+1, cur};
                tail = (tail + 1) % QSIZE;
                items++;
                oldtiles.insert({x2, y2});
            }
        }
        if (!first) {
            return sq;
        }
    }
    return NULL;
}

BASE *vehicle_home_base(VEH *vehicle) {
    return (vehicle->home_base_id >= 0 ? &tx_bases[vehicle->home_base_id] : NULL);
}

MAP *base_square(BASE *base) {
    return mapsq(base->x, base->y);
}

bool unit_has_ability(int id, int ability) {
    return tx_units[id].ability_flags & ability;
}

bool vehicle_has_ability(int vehicleId, int ability) {
    return tx_units[tx_vehicles[vehicleId].proto_id].ability_flags & ability;
}

bool vehicle_has_ability(VEH *vehicle, int ability) {
    return tx_units[vehicle->proto_id].ability_flags & ability;
}

int map_rainfall(MAP *tile) {
	int rainfall;
	if (tile->level & TILE_MOIST) {
		rainfall = 1;
	}
	else if (tile->level & TILE_RAINY) {
		rainfall = 2;
	}
	else {
		rainfall = 0;
	}
	return rainfall;
}

int map_level(MAP *tile) {
	return tile->level >> 5;
}

int map_elevation(MAP *tile) {
	return map_level(tile) - LEVEL_SHORE_LINE;
}

int map_rockiness(MAP *tile) {
	return ((tile->rocks & TILE_ROCKY) ? 2 : ((tile->rocks & TILE_ROLLING) ? 1 : 0));
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
bool map_has_landmark(MAP *tile, int landmark) {
	return (tile && (tile->landmarks & landmark));
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
		tx_bases[base_id].facilities_built[facility_id/8] |= (1 << (facility_id % 8));
	}
	else
	{
		tx_bases[base_id].facilities_built[facility_id/8] &= ~(1 << (facility_id % 8));
	}

}

/*
Checks if faction has tech to build a facility.
*/
bool has_facility_tech(int faction_id, int facility_id) {
	return has_tech(faction_id, tx_facility[facility_id].preq_tech);
}

/*
Counts base doctors, empaths, and transcends combined.
*/
int getDoctors(int id)
{
	BASE *base = &(tx_bases[id]);

	int doctors = 0;

	for (int i = 0; i < min(16, base->specialist_total); i++)
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
	return tx_bases[baseId].queue_items[0];
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
	BASE *base = &(tx_bases[baseId]);

	return (base->minerals_accumulated <= tx_basic->retool_exemption);

}

bool isBaseBuildingProjectBeyondRetoolingExemption(int baseId)
{
	BASE *base = &(tx_bases[baseId]);
	int item = base->queue_items[0];

	return (item >= -PROJECT_ID_LAST && item <= -PROJECT_ID_FIRST && base->minerals_accumulated > tx_basic->retool_exemption);

}

int getBaseBuildingItemCost(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	int item = getBaseBuildingItem(baseId);
	return (item >= 0 ? tx_veh_cost(item, baseId, 0) : tx_facility[-item].cost) * tx_cost_factor(base->faction_id, 1, -1);
}

/*
Returns SE MORALE attack bonus/penalty.
*/
int getSEMoraleAttack(int factionId)
{
	Faction *faction = &(tx_factions[factionId]);

	int factionSEMoraleAttack;

	if (faction->SE_morale_pending <= -2)
	{
		factionSEMoraleAttack = max(-4, faction->SE_morale_pending) + 1;
	}
	else if (faction->SE_morale_pending <= +1)
	{
		factionSEMoraleAttack = faction->SE_morale_pending;
	}
	else
	{
		factionSEMoraleAttack = min(+4, faction->SE_morale_pending) - 1;
	}

	return factionSEMoraleAttack;

}

/*
Returns SE MORALE defense bonus/penalty.
*/
int getSEMoraleDefense(int factionId)
{
	Faction *faction = &(tx_factions[factionId]);

	int factionSEMoraleDefense;

	if (faction->SE_morale_pending <= -2)
	{
		factionSEMoraleDefense = max(-4, faction->SE_morale_pending) + 1;
	}
	else
	{
		factionSEMoraleDefense = min(3, faction->SE_morale_pending);
	}

	return factionSEMoraleDefense;

}

/*
Returns vehicle morale on attack.
*/
int getMoraleAttack(int id)
{
	VEH *vehicle = &(tx_vehicles[id]);
	int factionId = vehicle->faction_id;

	return max(0, min(6, vehicle->morale + getSEMoraleAttack(factionId)));

}

/*
Returns vehicle morale on defense.
*/
int getMoraleDefense(int id)
{
	VEH *vehicle = &(tx_vehicles[id]);
	int factionId = vehicle->faction_id;

	return max(0, min(6, vehicle->morale + getSEMoraleDefense(factionId)));

}

/*
Returns vehicle morale modifier on attack.
*/
double getMoraleModifierAttack(int id)
{
	// get vehicle morale

	int morale = getMoraleAttack(id);

	// calculate modifier

	return 1.0 + 0.125 * (double)(morale - 2);

}

/*
Returns vehicle morale modifier on defense.
*/
double getMoraleModifierDefense(int id)
{
	// get vehicle morale

	int morale = getMoraleDefense(id);

	// calculate modifier

	return 1.0 + 0.125 * (double)(morale - 2);

}

double getVehiclePsiAttackStrength(int id)
{
	VEH *vehicle = &(tx_vehicles[id]);
	int triad = veh_triad(id);
	Faction *faction = &(tx_factions[vehicle->faction_id]);

	// get psi attack strength

	double psiCombatStrength;

	switch (triad)
	{
	case TRIAD_LAND:
		psiCombatStrength = (double)tx_basic->psi_combat_land_numerator / (double)tx_basic->psi_combat_land_denominator;
		break;
	case TRIAD_SEA:
		psiCombatStrength = (double)tx_basic->psi_combat_sea_numerator / (double)tx_basic->psi_combat_sea_denominator;
		break;
	case TRIAD_AIR:
		psiCombatStrength = (double)tx_basic->psi_combat_air_numerator / (double)tx_basic->psi_combat_air_denominator;
		break;
	default:
		return 1.0;
	}

	// get vehicle lifecycle modifier

	int lifecycle = vehicle->morale;
	double lifecycleModifier = 1.0 + 0.125 * (double)(lifecycle - 2);

	// get faction PLANET rating modifier

	int planet = faction->SE_planet_pending;
	double planetModifier = 1.0 + tx_basic->combat_psi_bonus_per_PLANET * planet;

	// calculate modifier

	return psiCombatStrength * lifecycleModifier * planetModifier;

}

double getVehiclePsiDefenseStrength(int id)
{
	VEH *vehicle = &(tx_vehicles[id]);
	Faction *faction = &(tx_factions[vehicle->faction_id]);

	// get psi defense strength

	double psiCombatStrength = 1.0;

	// get vehicle lifecycle modifier

	int lifecycle = vehicle->morale;
	double lifecycleModifier = 1.0 + 0.125 * (double)(lifecycle - 2);

	// get faction PLANET rating modifier

	int planet = faction->SE_planet_pending;
	double planetModifier = (conf.planet_combat_bonus_on_defense ? 1.0 + tx_basic->combat_psi_bonus_per_PLANET * planet : 1.0);

	// calculate modifier

	return psiCombatStrength * lifecycleModifier * planetModifier;

}

/*
Returns new vehicle morale modifier on attack.
*/
double getNewVehicleMoraleModifierAttack(int factionId, double averageFacilityMoraleBoost)
{
	Faction *faction = &(tx_factions[factionId]);

	// get new vehicle morale

	double morale = max(0.0, min(6.0, 0.0 + averageFacilityMoraleBoost * (faction->SE_morale_pending <= -2 ? 0.5 : 1.0) + (double)getSEMoraleAttack(factionId)));

	// calculate modifier

	return 1.0 + 0.125 * (double)(morale - 2);

}

/*
Returns new vehicle morale modifier on defense.
*/
double getNewVehicleMoraleModifierDefense(int factionId, double averageFacilityMoraleBoost)
{
	Faction *faction = &(tx_factions[factionId]);

	// get new vehicle morale

	double morale = max(0.0, min(6.0, 0.0 + averageFacilityMoraleBoost * (faction->SE_morale_pending <= -2 ? 0.5 : 1.0) + (double)getSEMoraleDefense(factionId)));

	// calculate modifier

	return 1.0 + 0.125 * (double)(morale - 2);

}

double getSEPlanetModifierAttack(int factionId)
{
	Faction *faction = &(tx_factions[factionId]);

	return 1.0 + (double)(tx_basic->combat_psi_bonus_per_PLANET * faction->SE_planet_pending) / 100.0;

}

double getSEPlanetModifierDefense(int factionId)
{
	Faction *faction = &(tx_factions[factionId]);

	if (conf.planet_combat_bonus_on_defense)
	{
		return 1.0 + (double)(tx_basic->combat_psi_bonus_per_PLANET * faction->SE_planet_pending) / 100.0;
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
		psiCombatBaseOdds = (double)tx_basic->psi_combat_land_numerator / (double)tx_basic->psi_combat_land_denominator;
		break;
	case TRIAD_SEA:
		psiCombatBaseOdds = (double)tx_basic->psi_combat_sea_numerator / (double)tx_basic->psi_combat_sea_denominator;
		break;
	case TRIAD_AIR:
		psiCombatBaseOdds = (double)tx_basic->psi_combat_air_numerator / (double)tx_basic->psi_combat_air_denominator;
		break;
	default:
		psiCombatBaseOdds = 1.0;
	}

	return psiCombatBaseOdds;

}

bool isCombatUnit(int id)
{
	return id >= 0 && tx_units[id].weapon_type <= WPN_PSI_ATTACK;
}

bool isCombatVehicle(int id)
{
	VEH *vehicle = &(tx_vehicles[id]);

	return tx_units[vehicle->proto_id].weapon_type <= WPN_PSI_ATTACK;
}

/*
Calculates average maximal damage unit delivers to enemy in psi combat when attacking.
*/
double calculatePsiDamageAttack(int id, int enemyId)
{
	VEH *vehicle = &(tx_vehicles[id]);
	VEH *enemyVehicle = &(tx_vehicles[enemyId]);

	// calculate base damage (vehicle attacking)

	double damage =
		1.0
		*
		getPsiCombatBaseOdds(veh_triad(id))
		*
		(
			getMoraleModifierAttack(id)
			*
			getSEPlanetModifierAttack(vehicle->faction_id)
		)
		/
		(
			getMoraleModifierDefense(enemyId)
			*
			getSEPlanetModifierDefense(enemyVehicle->faction_id)
		)
		*
		(double)(10 - vehicle->damage_taken)
	;

	// attacker empath increases damage

	if (vehicle_has_ability(vehicle, ABL_EMPATH))
	{
		damage *= (1 + (double)tx_basic->combat_bonus_empath_song_vs_psi / 100.0);
	}

	// defender trance decreases damage

	if (vehicle_has_ability(enemyVehicle, ABL_TRANCE))
	{
		damage /= (1 + (double)tx_basic->combat_bonus_trance_vs_psi / 100.0);
	}

	return damage;

}

/*
Calculates average maximal damage unit delivers to enemy in psi combat when defending.
*/
double calculatePsiDamageDefense(int id, int enemyId)
{
	VEH *vehicle = &(tx_vehicles[id]);
	VEH *enemyVehicle = &(tx_vehicles[enemyId]);

	// calculate base damage (vehicle defending)

	double damage =
		1.0
		/
		getPsiCombatBaseOdds(veh_triad(enemyId))
		*
		(
			getMoraleModifierDefense(id)
			*
			getSEPlanetModifierDefense(vehicle->faction_id)
		)
		/
		(
			getMoraleModifierAttack(enemyId)
			*
			getSEPlanetModifierAttack(enemyVehicle->faction_id)
		)
		*
		(double)(10 - vehicle->damage_taken)
	;

	// attacker empath decreases damage

	if (vehicle_has_ability(enemyVehicle, ABL_EMPATH))
	{
		damage /= (1 + (double)tx_basic->combat_bonus_empath_song_vs_psi / 100.0);
	}

	// defender trance increases damage

	if (vehicle_has_ability(vehicle, ABL_TRANCE))
	{
		damage *= (1 + (double)tx_basic->combat_bonus_trance_vs_psi / 100.0);
	}

	return damage;

}

/*
Calculates average maximal damage unit delivers to average native when attacking.
*/
double calculateNativeDamageAttack(int id)
{
	VEH *vehicle = &(tx_vehicles[id]);

	// calculate base damage (vehicle attacking)

	double damage =
		1.0
		*
		getPsiCombatBaseOdds(veh_triad(id))
		*
		(
			getMoraleModifierAttack(id)
			*
			getSEPlanetModifierAttack(vehicle->faction_id)
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
		damage *= (1 + (double)tx_basic->combat_bonus_empath_song_vs_psi / 100.0);
	}

	return damage;

}

/*
Calculates average maximal damage unit delivers to average native when defending.
*/
double calculateNativeDamageDefense(int id)
{
	VEH *vehicle = &(tx_vehicles[id]);

	// calculate base damage (vehicle defending)

	double damage =
		1.0
		/
		getPsiCombatBaseOdds(veh_triad(id)) // assuming native has same triad as defender
		*
		(
			getMoraleModifierDefense(id)
			*
			getSEPlanetModifierDefense(vehicle->faction_id)
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
		damage *= (1 + (double)tx_basic->combat_bonus_trance_vs_psi / 100.0);
	}

	return damage;

}

void setVehicleOrder(int id, int order)
{
	VEH *vehicle = &(tx_vehicles[id]);

    vehicle->move_status = order;
    vehicle->status_icon = veh_status_icon[order];

}

MAP *getMapTile(int x, int y)
{
	// ignore impossible combinations

	if ((x + y)%2 != 0)
		return NULL;

	// return map tile with wrapped x if needed

	return mapsq(wrap(x), y);

}

MAP *getBaseMapTile(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	return getMapTile(base->x, base->y);

}

MAP *getVehicleMapTile(int vehicleId)
{
	VEH *vehicle = &(tx_vehicles[vehicleId]);

	return getMapTile(vehicle->x, vehicle->y);

}

bool isImprovedTile(int x, int y)
{
	MAP *tile = getMapTile(x, y);

	return (tile->items & (TERRA_ROAD | TERRA_MAGTUBE | TERRA_MINE | TERRA_SOLAR | TERRA_FARM | TERRA_SOIL_ENR | TERRA_FOREST | TERRA_CONDENSER | TERRA_ECH_MIRROR | TERRA_THERMAL_BORE | TERRA_SENSOR)) != 0;

}

bool isVehicleSupply(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_SUPPLY_TRANSPORT);
}

bool isColonyUnit(int id)
{
	return (id >= 0 && tx_units[id].weapon_type == WPN_COLONY_MODULE);
}

bool isVehicleColony(int id)
{
	return (tx_units[tx_vehicles[id].proto_id].weapon_type == WPN_COLONY_MODULE);
}

bool isFormerUnit(int unitId)
{
	return (tx_units[unitId].weapon_type == WPN_TERRAFORMING_UNIT);
}

bool isVehicleFormer(int vehicleId)
{
	return isVehicleFormer(&(tx_vehicles[vehicleId]));
}
bool isVehicleFormer(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_TERRAFORMING_UNIT);
}

bool isVehicleTransport(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_TROOP_TRANSPORT);
}

bool isVehicleProbe(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_PROBE_TEAM);
}

void computeBase(int baseId)
{
	tx_set_base(baseId);
	tx_base_compute(1);

}

/*
Returns base tile region plus all ocean regions this base is connected to for coastal bases.
*/
std::set<int> getBaseConnectedRegions(int id)
{
	std::set<int> baseConnectedRegions;

	BASE *base = &(tx_bases[id]);

	// get base tile

	MAP *baseTile = getMapTile(base->x, base->y);

	// get and store base tile region

	baseConnectedRegions.insert(baseTile->region);

	// get and store base adjacent ocean regions for land base

	if (!is_ocean(baseTile))
	{
		for (MAP *tile : getAdjacentTiles(base->x, base->y, false))
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
	BASE *base = &(tx_bases[baseId]);
	MAP *baseLocations = getBaseMapTile(baseId);

	std::set<int> baseConnectedOceanRegions;

	if (is_ocean(baseLocations))
	{
		baseConnectedOceanRegions.insert(baseLocations->region);
	}
	else
	{
		for (MAP *tile : getAdjacentTiles(base->x, base->y, false))
		{
			if (!is_ocean(tile))
				continue;

			baseConnectedOceanRegions.insert(tile->region);

		}

	}

	return baseConnectedOceanRegions;

}

bool isOceanRegion(int region)
{
	return region >= 64;
}

/*
Evaluates unit defense effectiveness based on defense value and cost.
*/
double evaluateUnitConventionalDefenseEffectiveness(int id)
{
	UNIT *unit = &(tx_units[id]);
	int cost = unit->cost;
	int defenseValue = tx_defense[unit->armor_type].defense_value;

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
	UNIT *unit = &(tx_units[id]);
	int cost = unit->cost;
	int offenseValue = tx_weapon[unit->armor_type].offense_value;

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
	UNIT *unit = &(tx_units[id]);

	// value

	double defenseValue = 1.0;

	if (unit_has_ability(id, ABL_TRANCE))
	{
		defenseValue *= 1.0 + (double)tx_basic->combat_bonus_trance_vs_psi / 100.0;
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
	UNIT *unit = &(tx_units[id]);

	// value

	double offenseValue = 1.0;

	if (unit_has_ability(id, ABL_EMPATH))
	{
		offenseValue *= 1.0 + (double)tx_basic->combat_bonus_trance_vs_psi / 100.0;
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
*/
double getBaseDefenseMultiplier(int id, int triad, bool countDefensiveStructures, bool countTerritoryBonus)
{
	double baseDefenseMultiplier;

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

	if (countDefensiveStructures && (firstLevelDefense || secondLevelDefense))
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
		baseDefenseMultiplier = 1.0 + (double)tx_basic->combat_bonus_intrinsic_base_def / 100.0;
	}

	if (countTerritoryBonus)
	{
		baseDefenseMultiplier *= 1.0 + (double)conf.combat_bonus_territory / 100.0;
	}

	return baseDefenseMultiplier;

}

int getUnitOffenseValue(int id)
{
	return tx_weapon[tx_units[id].weapon_type].offense_value;
}

int getUnitDefenseValue(int id)
{
	return tx_defense[tx_units[id].armor_type].defense_value;
}

int getVehicleOffenseValue(int id)
{
	return getUnitOffenseValue(tx_vehicles[id].proto_id);
}

int getVehicleDefenseValue(int id)
{
	return getUnitDefenseValue(tx_vehicles[id].proto_id);
}

/*
Estimates turns to complete current base production assuming all parameters stays as is.
*/
int estimateBaseProductionTurnsToComplete(int id)
{
	BASE *base = &(tx_bases[id]);

	return ((mineral_cost(id, base->queue_items[0]) - base->minerals_accumulated) + (base->mineral_surplus - 1)) / base->mineral_surplus;

}

/*
Returns all valid adjacent map tiles around given point.
if startFromCenter == true then also return central tile as first element
*/
std::vector<MAP *> getAdjacentTiles(int x, int y, bool startWithCenter)
{
	std::vector<MAP *> adjacentTiles;

	for (int offsetIndex = (startWithCenter ? 0 : 1); offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
	{
		MAP *tile = getMapTile(wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]), y + BASE_TILE_OFFSETS[offsetIndex][1]);

		if (tile == NULL)
			continue;

		adjacentTiles.push_back(tile);

	}

	return adjacentTiles;

}

/*
Returns all valid adjacent map tile infos around given point.
if startFromCenter == true then also return central tile as first element
*/
std::vector<MAP_INFO> getAdjacentTileInfos(int x, int y, bool startWithCenter)
{
	std::vector<MAP_INFO> adjacentTileInfos;

	for (int offsetIndex = (startWithCenter ? 0 : 1); offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
	{
		int tileX = wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]);
		int tileY = y + BASE_TILE_OFFSETS[offsetIndex][1];
		MAP *tile = getMapTile(tileX, tileY);

		if (tile == NULL)
			continue;

		adjacentTileInfos.push_back({tileX, tileY, tile});

	}

	return adjacentTileInfos;

}

/*
Returns all valid base radius map tiles around given point.
if startFromCenter == true then also return central tile as first element
*/
std::vector<MAP *> getBaseRadiusTiles(int x, int y, bool startWithCenter)
{
	std::vector<MAP *> baseRadiusTiles;

	for (int offsetIndex = (startWithCenter ? 0 : 1); offsetIndex < BASE_TILE_OFFSET_COUNT; offsetIndex++)
	{
		MAP *tile = getMapTile(wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]), y + BASE_TILE_OFFSETS[offsetIndex][1]);

		if (tile == NULL)
			continue;

		baseRadiusTiles.push_back(tile);

	}

	return baseRadiusTiles;

}

/*
Returns all valid base radius map tile infos around given point.
if startFromCenter == true then also return central tile as first element
*/
std::vector<MAP_INFO> getBaseRadiusTileInfos(int x, int y, bool startWithCenter)
{
	std::vector<MAP_INFO> baseRadiusTileInfos;

	for (int offsetIndex = (startWithCenter ? 0 : 1); offsetIndex < BASE_TILE_OFFSET_COUNT; offsetIndex++)
	{
		int tileX = wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]);
		int tileY = y + BASE_TILE_OFFSETS[offsetIndex][1];
		MAP *tile = getMapTile(tileX, tileY);

		if (tile == NULL)
			continue;

		baseRadiusTileInfos.push_back({tileX, tileY, tile});

	}

	return baseRadiusTileInfos;

}

std::vector<MAP *> getBaseWorkedTiles(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	std::vector<MAP *> baseWorkedTiles;

	for (int offsetIndex = 1; offsetIndex < BASE_TILE_OFFSET_COUNT; offsetIndex++)
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

	for (int offsetIndex = 1; offsetIndex < BASE_TILE_OFFSET_COUNT; offsetIndex++)
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

/*
Summarize base garrizon defense value for all conventional combat units with defense value >= 2.
*/
int getBaseConventionalDefenseValue(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	int baseConventionalDefenseValue = 0;

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(tx_vehicles[vehicleId]);

		// own vehicles only

		if (vehicle->faction_id != base->faction_id)
			continue;

		// combat vehicles only

		if (!isCombatVehicle(vehicleId))
			continue;

		// get defense value

		int defenseValue = tx_defense[tx_units[vehicle->proto_id].armor_type].defense_value;

		// defense value >= 2 (conventional unit)

		if (defenseValue < 2)
			continue;

		baseConventionalDefenseValue += defenseValue;

	}

	return baseConventionalDefenseValue;

}

/*
Returns faction available prototypes.
*/
std::vector<int> getFactionPrototypes(int factionId, bool includeNotPrototyped)
{
	std::vector<int> factionPrototypes;

	// only real factions

	if (!(factionId >= 1 && factionId <= 7))
		return factionPrototypes;

	// iterate faction prototypes space

    for (int unitIndex = 0; unitIndex < 128; unitIndex++)
	{
        int unitId = (unitIndex < 64 ? unitIndex : (factionId - 1) * 64 + unitIndex);
        UNIT *unit = &(tx_units[unitId]);

		// skip not enabled

		if (unitId < 64 && !has_tech(factionId, unit->preq_tech))
			continue;

        // skip empty

        if (strlen(unit->name) == 0)
			continue;

		// do not include not prototyped if not requested

		if (unitId >= 64 && !includeNotPrototyped && !(unit->unit_flags & UNIT_PROTOTYPED))
			continue;

		// add prototype

		factionPrototypes.push_back(unitId);

	}

	return factionPrototypes;

}

/*
Determines if vehicle is a native land predefined unit.
*/
bool isNativeLandVehicle(int vehicleId)
{
	VEH *vehicle = &(tx_vehicles[vehicleId]);
	int unitId = vehicle->proto_id;

	return
		unitId == BSC_MIND_WORMS
		||
		unitId == BSC_SPORE_LAUNCHER
	;

}

bool isBaseBuildingColony(int baseId)
{
	BASE *base = &(tx_bases[baseId]);
	int item = base->queue_items[0];

	return (item >= 0 && isColonyUnit(item));

}

int projectBasePopulation(int baseId, int turns)
{
	BASE *base = &(tx_bases[baseId]);

	int nutrientCostFactor = tx_cost_factor(base->faction_id, 0, baseId);
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
	MetaFaction *metaFaction = &(tx_metafactions[factionId]);

	int populationLimit;

	// basic population limit for facility

	switch (facilityId)
	{
	case FAC_HAB_COMPLEX:
		populationLimit = tx_basic->pop_limit_wo_hab_complex;
		break;
	case FAC_HABITATION_DOME:
		populationLimit = tx_basic->pop_limit_wo_hab_dome;
		break;
	default:
		return 0;
	}

	// faction penalty

    populationLimit -= metaFaction->rule_population;

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
	BASE *base = &(tx_bases[baseId]);

	// already built

	if (has_facility(baseId, facilityId))
		return false;

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
	BASE *base = &(tx_bases[baseId]);

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
		for (MAP *adjacentTile : getAdjacentTiles(x, y, false))
		{
			if (is_ocean(adjacentTile) && adjacentTile->region == region)
				return true;

		}

	}

	return false;

}

int getXCoordinateByMapIndex(int mapIndex)
{
	return (mapIndex % (*map_half_x)) * 2 + (mapIndex / (*map_half_x) % 2);
}

int getYCoordinateByMapIndex(int mapIndex)
{
	return mapIndex / (*map_half_x);
}

std::vector<int> getRegionBases(int factionId, int region)
{
	std::vector<int> regionBases;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(tx_bases[baseId]);

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
		VEH *vehicle = &(tx_vehicles[vehicleId]);
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
Finds base own stationed units.
*/
std::vector<int> getBaseGarrison(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	std::vector<int> baseGarrison;

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(tx_vehicles[vehicleId]);

		// only this faction units

		if (vehicle->faction_id != base->faction_id)
			continue;

		// only in this base

		if (!(vehicle->x == base->x && vehicle->y == base->y))
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
		BASE *base = &(tx_bases[baseId]);

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
double estimateUnitBaseLandNativeProtection(int factionId, int unitId)
{
	// basic defense odds against land native attack

	double nativeProtection = 1.0 / getPsiCombatBaseOdds(TRIAD_LAND);
	debug("nativeProtection=%f\n", nativeProtection);

	// add base defense bonus

	nativeProtection *= 1.0 + (double)tx_basic->combat_bonus_intrinsic_base_def / 100.0;
	debug("nativeProtection=%f\n", nativeProtection);

	// add faction defense bonus

	nativeProtection *= getFactionDefenseMultiplier(factionId);
	debug("nativeProtection=%f\n", nativeProtection);

	// add PLANET

	nativeProtection *= getSEPlanetModifierDefense(factionId);
	debug("nativeProtection=%f\n", nativeProtection);

	// add trance

	if (unit_has_ability(unitId, ABL_TRANCE))
	{
		nativeProtection *= 1.0 + (double)tx_basic->combat_bonus_trance_vs_psi / 100.0;
	}
	debug("nativeProtection=%f\n", nativeProtection);

	// correction for native base attack penalty until turn 50

	if (*current_turn < 50)
	{
		nativeProtection *= 2;
	}
	debug("nativeProtection=%f\n", nativeProtection);

	return nativeProtection;

}

/*
Estimates vehicle odds in base defense agains land native attack.
This accounts for vehicle morale.
*/
double estimateVehicleBaseLandNativeProtection(int factionId, int vehicleId)
{
	// unit native protection

	double nativeProtection = estimateUnitBaseLandNativeProtection(factionId, tx_vehicles[vehicleId].proto_id);

	// add morale

	nativeProtection *= getMoraleModifierDefense(vehicleId);
	debug("nativeProtection=%f\n", nativeProtection);

	return nativeProtection;

}

double getFactionOffenseMultiplier(int factionId)
{
	double factionOffenseMultiplier = 1.0;

	MetaFaction *metaFaction = &(tx_metafactions[factionId]);

	for (int bonusIndex = 0; bonusIndex < metaFaction->faction_bonus_count; bonusIndex++)
	{
		int bonusId = metaFaction->faction_bonus_id[bonusIndex];

		if (bonusId == FCB_OFFENSE)
		{
			factionOffenseMultiplier *= ((double)metaFaction->faction_bonus_val1[bonusIndex] / 100.0);
		}

	}

	return factionOffenseMultiplier;

}

double getFactionDefenseMultiplier(int factionId)
{
	double factionDefenseMultiplier = 1.0;

	MetaFaction *metaFaction = &(tx_metafactions[factionId]);

	for (int bonusIndex = 0; bonusIndex < metaFaction->faction_bonus_count; bonusIndex++)
	{
		int bonusId = metaFaction->faction_bonus_id[bonusIndex];

		if (bonusId == FCB_DEFENSE)
		{
			factionDefenseMultiplier *= ((double)metaFaction->faction_bonus_val1[bonusIndex] / 100.0);
		}

	}

	return factionDefenseMultiplier;

}

double getFactionFanaticBonusMultiplier(int factionId)
{
	double factionOffenseMultiplier = 1.0;

	MetaFaction *metaFaction = &(tx_metafactions[factionId]);

	if (metaFaction->rule_flags & FACT_FANATIC)
	{
		factionOffenseMultiplier *= (1.0 + (double)tx_basic->combat_bonus_fanatic / 100.0);
	}

	return factionOffenseMultiplier;

}

/*
Calculates average number of native vehicles destroyed in psi combat when defending at base.
Assuming vehicle will heal in full at base.
*/
double getVehicleBaseNativeProtectionPotential(int vehicleId)
{
	VEH *vehicle = &(tx_vehicles[vehicleId]);

	// calculate base damage (vehicle defending)

	double protectionPotential = 1.0 / getPsiCombatBaseOdds(TRIAD_LAND) * getMoraleModifierDefense(vehicleId) * getSEPlanetModifierDefense(vehicle->faction_id) * (1.0 + (double)tx_basic->combat_bonus_intrinsic_base_def / 100.0);

	// defender trance increases combat protectionPotential

	if (vehicle_has_ability(vehicleId, ABL_TRANCE))
	{
		protectionPotential *= (1 + (double)tx_basic->combat_bonus_trance_vs_psi / 100.0);
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
	VEH *vehicle = &(tx_vehicles[vehicleId]);

	// calculate protection potential

	double protectionPotential = getVehicleBaseNativeProtectionPotential(vehicleId);

	// calculate cost

	int cost = tx_units[vehicle->proto_id].cost;

	// inflate cost due to support assuming 40 turns of service

	if (!unit_has_ability(vehicleId, ABL_CLEAN_REACTOR))
	{
		cost += 4;
	}

	return protectionPotential / (double)cost;

}

/*
Calculates number of allowed police units per SE POLICE rating.
*/
int getAllowedPolice(int factionId)
{
	Faction *faction = &(tx_factions[factionId]);

	int sePoliceRating = faction->SE_police_pending;

	int allowedPolice;

	switch (sePoliceRating)
	{
	case -5:
	case -4:
	case -3:
	case -2:
		allowedPolice = 0;
		break;
	case -1:
	case +0:
		allowedPolice = 1;
		break;
	case +1:
		allowedPolice = 2;
		break;
	case +2:
	case +3:
		allowedPolice = 3;
		break;
	default:
		allowedPolice = 0;
	}

	return allowedPolice;

}

int getVehicleUnitPlan(int vehicleId)
{
	return (int)tx_units[vehicleId].unit_plan;
}

/*
Counts all police units in the base.
Police unit = combat unit.
*/
int getBasePoliceUnitCount(int baseId)
{
	int basePoliceUnitCount = 0;

	for (int vehicleId : getBaseGarrison(baseId))
	{
		// only combat units

		if (!isCombatVehicle(vehicleId))
			continue;

		basePoliceUnitCount++;

	}

	return basePoliceUnitCount;

}

/*
Estimates base native protection in number of native units killed.
*/
double getBaseNativeProtection(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	double baseNativeProtection = 0.0;

	for (int vehicleId : getBaseGarrison(baseId))
	{
		// combat vehicles only

		if (!isCombatVehicle(vehicleId))
			continue;

		baseNativeProtection += estimateVehicleBaseLandNativeProtection(base->faction_id, vehicleId);

	}

	return baseNativeProtection;

}

bool isBaseHasAccessToWater(int baseId)
{
    BASE* base = &(tx_bases[baseId]);

    for (MAP *tile : getAdjacentTiles(base->x, base->y, true))
	{
		if (is_ocean(tile))
			return true;

	}

    return false;

}

bool isBaseCanBuildShips(int baseId)
{
    BASE* base = &(tx_bases[baseId]);

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

	for (MAP_INFO adjacentTileInfo : getAdjacentTileInfos(x, y, false))
	{
		MAP *adjacentTile = adjacentTileInfo.tile;

		// if at least one adjacent tile is not visible - we are at edge

		if (~adjacentTile->visibility & (0x1 << factionId))
			return true;

	}

	return false;

}

MAP_INFO getAdjacentOceanTileInfo(int x, int y)
{
	MAP_INFO adjacentOceanTileInfo;

	for (int adjacentTileIndex = 1; adjacentTileIndex < ADJACENT_TILE_OFFSET_COUNT; adjacentTileIndex++)
	{
		int adjacentTileX = wrap(x + BASE_TILE_OFFSETS[adjacentTileIndex][0]);
		int adjacentTileY = y + BASE_TILE_OFFSETS[adjacentTileIndex][1];
		MAP *adjacentTile = getMapTile(adjacentTileX, adjacentTileY);

		if (adjacentTile == NULL)
			continue;

		if (is_ocean(adjacentTile))
		{
			adjacentOceanTileInfo.x = adjacentTileX;
			adjacentOceanTileInfo.y = adjacentTileY;
			adjacentOceanTileInfo.tile = adjacentTile;

			break;

		}

	}

	return adjacentOceanTileInfo;

}

bool isVehicleExploring(int vehicleId)
{
	VEH *vehicle = &(tx_vehicles[vehicleId]);

	return vehicle->state & VSTATE_EXPLORE;

}

bool isVehicleCanHealAtThisLocation(int vehicleId)
{
	VEH *vehicle = &(tx_vehicles[vehicleId]);
	MAP *vehilceMapTile = getVehicleMapTile(vehicleId);

	// calculate vehicle field damage healing threshold

	int fieldDamageHealingThreshold = tx_units[vehicle->proto_id].reactor_type * 2;

	return
		(vehicle->damage_taken > fieldDamageHealingThreshold)
		||
		(map_has_item(vehilceMapTile, TERRA_BASE_IN_TILE) && (vehicle->damage_taken > 0))
	;

}

/*
Returns all ocean regions this location is adjacent to.
*/
std::unordered_set<int> getAdjacentOceanRegions(int x, int y)
{
	debug("getAdjacentOceanRegions: x=%d, y=%d\n", x, y);

	std::unordered_set<int> adjacentOceanRegions;

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

		for (MAP *adjacentTile : getAdjacentTiles(x, y, false))
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
std::unordered_set<int> getConnectedOceanRegions(int factionId, int x, int y)
{
	debug("getConnectedOceanRegions: factionId=%d, x=%d, y=%d\n", factionId, x, y);

	std::unordered_set<int> connectedOceanRegions;

	MAP *tile = getMapTile(x, y);

	if (tile == NULL)
		return connectedOceanRegions;

	// add adjacent tiles regions first

	std::unordered_set<int> adjacentOceanRegions = getAdjacentOceanRegions(x, y);
	connectedOceanRegions.insert(adjacentOceanRegions.begin(), adjacentOceanRegions.end());

	// iterate own bases

	while (true)
	{
		size_t currentRegionSize = connectedOceanRegions.size();
		debug("\tcurrentRegionSize=%d\n", currentRegionSize);

		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = &(tx_bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);

			debug("\t\t%-25s factionId=%d, base->faction_id=%d, is_ocean(baseTile)=%d\n", tx_bases[baseId].name, factionId, base->faction_id, is_ocean(baseTile));

			// only own and allied bases

			if (base->faction_id != factionId)
				continue;

			// only land bases

			if (is_ocean(baseTile))
				continue;

			// get this base adjacent ocean regions

			std::unordered_set<int> baseAdjacentOceanRegions = getAdjacentOceanRegions(base->x, base->y);

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
	return (tile->visibility & (0x1 << factionId));

}

/*
Verifies that faction1 and faction2 has given diplo_status.
*/
bool isDiploStatus(int faction1Id, int faction2Id, int diploStatus)
{
	return (tx_factions[faction1Id].diplo_status[faction2Id] & diploStatus);
}

void setDiploStatus(int faction1Id, int faction2Id, int diploStatus, bool on)
{
	if (on)
	{
		tx_factions[faction1Id].diplo_status[faction2Id] |= diploStatus;
	}
	else
	{
		tx_factions[faction1Id].diplo_status[faction2Id] &= ~diploStatus;
	}

}

/*
Calculates amount of minerals to complete production in the base.
*/
int getRemainingMinerals(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	return max(0, mineral_cost(baseId, base->queue_items[0]) - base->minerals_accumulated);

}

std::vector<int> getStackedVehicleIds(int vehicleId)
{
	std::vector<int> stackedVehicleIds;

	// get top of the stack vehicle

	int topStackVehicleId = vehicleId;
	while (tx_vehicles[topStackVehicleId].prev_unit_id_stack != -1)
	{
		topStackVehicleId = tx_vehicles[topStackVehicleId].prev_unit_id_stack;
	}

	// collect all vehicles in the stack

	for (int stackedVehicleId = topStackVehicleId; stackedVehicleId != -1; stackedVehicleId = tx_vehicles[stackedVehicleId].next_unit_id_stack)
	{
		stackedVehicleIds.push_back(stackedVehicleId);
	}

	return stackedVehicleIds;

}

/*
Searches for friendly sensor in given range.
*/
bool isInRangeOfFriendlySensor(int x, int y, int range, int factionId)
{
	bool sensor = false;

	for (int dx = -(2 * range); dx <= (2 * range); dx++)
	{
		for (int dy = -((2 * range) - abs(dx)); dy <= +((2 * range) - abs(dx)); dy += 2)
		{
			MAP *tile = getMapTile(x + dx, y + dy);

			if (tile == NULL)
				continue;

			// return true for friendly sensor

			if (tile->owner == factionId && map_has_item(tile, TERRA_SENSOR))
				sensor = true;

		}

	}

	return sensor;

}

