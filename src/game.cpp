#include "game.h"

// external variables definitions

char* prod_name(int prod) {
    if (prod >= 0) {
        return (char*)&(Units[prod].name);
    } else {
        return (char*)(Facility[-prod].name);
    }
}

int mineral_cost(int faction, int prod) {
    if (prod >= 0) {
        return Units[prod].cost * cost_factor(faction, 1, -1);
    } else {
        return Facility[-prod].cost * cost_factor(faction, 1, -1);
    }
}

bool has_tech(int faction, int tech) {
    assert(valid_player(faction));
    if (tech == TECH_None) {
        return true;
    }
    return tech >= 0 && TechOwners[tech] & (1 << faction);
}

bool has_ability(int faction, int abl) {
    assert(abl >= 0 && abl <= ABL_ID_ALGO_ENHANCEMENT);
    return has_tech(faction, Ability[abl].preq_tech);
}

bool has_chassis(int faction, int chs) {
    return has_tech(faction, Chassis[chs].preq_tech);
}

bool has_weapon(int faction, int wpn) {
    return has_tech(faction, Weapon[wpn].preq_tech);
}

bool has_terra(int faction, int act, bool ocean) {
    int preq_tech = (ocean ? Terraform[act].preq_tech_sea : Terraform[act].preq_tech);
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
    int i = SecretProjects[id-PROJECT_ID_FIRST];
    return i >= 0 && (faction < 0 || Bases[i].faction_id == faction);
}

bool has_facility(int base_id, int id) {
    assert(base_id >= 0 && id > 0 && id <= FAC_EMPTY_SP_64);
    if (id >= PROJECT_ID_FIRST) {
        return SecretProjects[id-PROJECT_ID_FIRST] == base_id;
    }
    int faction = Bases[base_id].faction_id;
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
    return Bases[base_id].facilities_built[id/8] & (1 << (id % 8));
}

bool can_build(int base_id, int id) {
    assert(base_id >= 0 && id > 0 && id <= FAC_EMPTY_SP_64);
    BASE* base = &Bases[base_id];
    int faction = base->faction_id;
    Faction* f = &Factions[faction];
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
        if (SecretProjects[id-PROJECT_ID_FIRST] != PROJECT_UNBUILT
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
        if (base->pop_size < Rules->base_size_subspace_gen) {
            return false;
        }
        int n = 0;
        for (int i=0; i<*total_num_bases; i++) {
            BASE* b = &Bases[i];
            if (b->faction_id == faction && has_facility(i, FAC_SUBSPACE_GENERATOR)
            && b->pop_size >= Rules->base_size_subspace_gen
            && ++n >= Rules->subspace_gen_req) {
                return false;
            }
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
    return has_tech(faction, Facility[id].preq_tech) && !has_facility(base_id, id);
}

bool can_build_unit(int faction, int id) {
    assert(valid_player(faction) && id >= -1);
    UNIT* u = &Units[id];
    if (id >= 0 && id < MaxProtoFactionNum && u->preq_tech != TECH_None
    && !has_tech(faction, u->preq_tech)) {
        return false;
    }
    return *total_num_vehicles < MaxVehNum * 15 / 16;
}

bool is_human(int faction) {
    return *human_players & (1 << faction);
}

bool is_alien(int faction) {
    return MFactions[faction].rule_flags & FACT_ALIEN;
}

bool ai_enabled(int faction) {
    /* Exclude native life since Thinker AI routines don't apply to them. */
    return faction > 0 && !(*human_players & (1 << faction))
        && faction <= conf.factions_enabled;
}

bool at_war(int faction1, int faction2) {
    return faction1 != faction2 && faction1 >= 0 && faction2 >= 0
        && Factions[faction1].diplo_status[faction2] & DIPLO_VENDETTA;
}

bool has_pact(int faction1, int faction2) {
    return faction1 > 0 && faction2 > 0
        && Factions[faction1].diplo_status[faction2] & DIPLO_PACT;
}

bool un_charter() {
    return *un_charter_repeals <= *un_charter_reinstates;
}

bool valid_player(int faction) {
    return faction > 0 && faction < MaxPlayerNum;
}

bool valid_triad(int triad) {
    return (triad == TRIAD_LAND || triad == TRIAD_SEA || triad == TRIAD_AIR);
}

int prod_count(int faction, int id, int skip) {
    assert(valid_player(faction));
    int n = 0;
    for (int i=0; i<*total_num_bases; i++) {
        BASE* base = &Bases[i];
        if (base->faction_id == faction && base->queue_items[0] == id && i != skip) {
            n++;
        }
    }
    return n;
}

int facility_count(int faction, int facility) {
    assert(valid_player(faction) && facility >= 0);
    int n = 0;
    for (int i=0; i<*total_num_bases; i++) {
        BASE* base = &Bases[i];
        if (base->faction_id == faction && has_facility(i, facility)) {
            n++;
        }
    }
    return n;
}

int find_hq(int faction) {
    for(int i=0; i<*total_num_bases; i++) {
        BASE* base = &Bases[i];
        if (base->faction_id == faction && has_facility(i, FAC_HEADQUARTERS)) {
            return i;
        }
    }
    return -1;
}

int manifold_nexus_owner() {
    for (int y=0; y < *map_axis_y; y++) {
        for (int x=y&1; x < *map_axis_x; x += 2) {
            MAP* sq = mapsq(x, y);
            /* First Manifold Nexus tile must also be visible to the owner. */
            if (sq && sq->landmarks & LM_NEXUS && sq->art_ref_id == 0) {
                return (sq->owner >= 0 && sq->is_visible(sq->owner) ? sq->owner : -1);
            }
        }
    }
    return -1;
}

int mod_veh_speed(int veh_id) {
    return veh_speed(veh_id, 0);
}

int best_armor(int faction, bool cheap) {
    int ci = ARM_NO_ARMOR;
    int cv = 4;
    for (int i=ARM_SYNTHMETAL_ARMOR; i<=ARM_RESONANCE_8_ARMOR; i++) {
        if (has_tech(faction, Armor[i].preq_tech)) {
            int val = Armor[i].defense_value;
            int cost = Armor[i].cost;
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
        if (has_tech(faction, Weapon[i].preq_tech)) {
            int iv = Weapon[i].offense_value *
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
        if (has_tech(faction, Reactor[r - 1].preq_tech)) {
            return r;
        }
    }
    return REC_FISSION;
}

int offense_value(UNIT* u) {
    int w = (conf.ignore_reactor_power ? (int)REC_FISSION : u->reactor_type);
    if (u->weapon_type == WPN_CONVENTIONAL_PAYLOAD) {
        return Factions[*active_faction].best_weapon_value * w * 4/5;
    }
    return Weapon[u->weapon_type].offense_value * w;
}

int defense_value(UNIT* u) {
    int w = (conf.ignore_reactor_power ? (int)REC_FISSION : u->reactor_type);
    return Armor[u->armor_type].defense_value * w;
}

int faction_might(int faction) {
    Faction* f = &Factions[faction];
    return max(1, f->mil_strength_1 + f->mil_strength_2 + f->pop_total);
}

double expansion_ratio(int faction) {
    int bases = 0;
    for (int i=1; i < MaxPlayerNum && conf.expansion_autoscale > 0; i++) {
        if (is_human(i)) {
            bases = Factions[i].current_num_bases;
            break;
        }
    }
    double limit = max(1.0 * bases, (pow(*map_half_x * *map_axis_y, 0.4) *
        min(1.0, max(0.4, *map_area_sq_root / 56.0)) * conf.expansion_factor / 100.0));
    return (limit > 0 ? Factions[faction].current_num_bases / limit : 1);
}

int random(int n) {
    return (n > 0 ? rand() % n : 0);
}

int map_hash(int x, int y) {
    return ((*map_random_seed ^ x) * 127) ^ (y * 179);
}

int wrap(int a) {
    if (!*map_toggle_flat) {
        return (a < 0 ? a + *map_axis_x : a % *map_axis_x);
    } else {
        return a;
    }
}

int map_range(int x1, int y1, int x2, int y2) {
    int xd = abs(x1-x2);
    int yd = abs(y1-y2);
    if (!*map_toggle_flat & 1 && xd > *map_axis_x/2) {
        xd = *map_axis_x - xd;
    }
    return (xd + yd)/2;
}

int vector_dist(int x1, int y1, int x2, int y2) {
    int dx = abs(x1 - x2);
    int dy = abs(y1 - y2);
    if (!(*map_toggle_flat & 1 || dx <= *map_half_x)) {
        dx = *map_axis_x - dx;
    }
    return max(dx, dy) - ((((dx + dy) / 2) - min(dx, dy) + 1) / 2);
}

int min_range(const Points& S, int x, int y) {
    int z = MaxMapW;
    for (auto& p : S) {
        z = min(z, map_range(x, y, p.x, p.y));
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
    return (n > 0 ? (double)sum/n : 0);
}

MAP* mapsq(int x, int y) {
    assert(!((x + y)&1));
    if (x >= 0 && y >= 0 && x < *map_axis_x && y < *map_axis_y && !((x + y)&1)) {
        return &((*MapPtr)[ x/2 + (*map_half_x) * y ]);
    } else {
        return NULL;
    }
}

int unit_in_tile(MAP* sq) {
    if (!sq || (sq->flags & 0xf) == 0xf) {
        return -1;
    }
    return sq->flags & 0xf;
}

int set_move_to(int veh_id, int x, int y) {
    VEH* veh = &Vehicles[veh_id];
    debug("set_move_to %2d %2d -> %2d %2d %s\n", veh->x, veh->y, x, y, veh->name());
    veh->waypoint_1_x = x;
    veh->waypoint_1_y = y;
    veh->move_status = ORDER_MOVE_TO;
    veh->status_icon = 'G';
    veh->terraforming_turns = 0;
    mapnodes.erase({x, y, NODE_PATROL});
    if (veh->x == x && veh->y == y) {
        return mod_veh_skip(veh_id);
    }
    return SYNC;
}

int set_move_next(int veh_id, int offset) {
    VEH* veh = &Vehicles[veh_id];
    int x = wrap(veh->x + BaseOffsetX[offset]);
    int y = veh->y + BaseOffsetY[offset];
    return set_move_to(veh_id, x, y);
}

int set_road_to(int veh_id, int x, int y) {
    VEH* veh = &Vehicles[veh_id];
    veh->waypoint_1_x = x;
    veh->waypoint_1_y = y;
    veh->move_status = ORDER_ROAD_TO;
    veh->status_icon = 'R';
    return SYNC;
}

int set_action(int veh_id, int act, char icon) {
    VEH* veh = &Vehicles[veh_id];
    if (act == ORDER_THERMAL_BOREHOLE) {
        mapnodes.insert({veh->x, veh->y, NODE_BOREHOLE});
    } else if (act == ORDER_TERRAFORM_UP) {
        mapnodes.insert({veh->x, veh->y, NODE_RAISE_LAND});
    }
    veh->move_status = act;
    veh->status_icon = icon;
    veh->state &= ~VSTATE_UNK_10000;
    return SYNC;
}

int set_convoy(int veh_id, int res) {
    VEH* veh = &Vehicles[veh_id];
    mapnodes.insert({veh->x, veh->y, NODE_CONVOY});
    veh->type_crawling = res-1;
    veh->move_status = ORDER_CONVOY;
    veh->status_icon = 'C';
    return veh_skip(veh_id);
}

int set_board_to(int veh_id, int trans_veh_id) {
    VEH* veh = &Vehicles[veh_id];
    VEH* v2 = &Vehicles[trans_veh_id];
    assert(veh_id != trans_veh_id);
    assert(veh->x == v2->x && veh->y == v2->y);
    assert(cargo_capacity(trans_veh_id) > 0);
    veh->move_status = ORDER_SENTRY_BOARD;
    veh->waypoint_1_x = trans_veh_id;
    veh->waypoint_1_y = 0;
    veh->status_icon = 'L';
    debug("set_board_to %2d %2d %s -> %s\n", veh->x, veh->y, veh->name(), v2->name());
    return SYNC;
}

int cargo_loaded(int veh_id) {
    int n=0;
    for (int i=0; i<*total_num_vehicles; i++) {
        VEH* veh = &Vehicles[i];
        if (veh->move_status == ORDER_SENTRY_BOARD && veh->waypoint_1_x == veh_id) {
            n++;
        }
    }
    return n;
}

/*
Original Offset: 005C1760
*/
int cargo_capacity(int veh_id) {
    VEH* v = &Vehicles[veh_id];
    UNIT* u = &Units[v->unit_id];
    if (u->carry_capacity > 0 && veh_id < MaxProtoFactionNum 
    && Weapon[u->weapon_type].offense_value < 0) {
        return v->morale + 1;
    }
    return u->carry_capacity;
}

int mod_veh_skip(int veh_id) {
    VEH* veh = &Vehicles[veh_id];
    veh->waypoint_1_x = veh->x;
    veh->waypoint_1_y = veh->y;
    veh->status_icon = '-';
    if (veh->damage_taken) {
        MAP* sq = mapsq(veh->x, veh->y);
        if (sq && sq->items & TERRA_MONOLITH) {
            monolith(veh_id);
        }
    }
    return veh_skip(veh_id);
}

bool at_target(VEH* veh) {
    return veh->move_status == ORDER_NONE || (veh->waypoint_1_x < 0 && veh->waypoint_1_y < 0)
        || (veh->x == veh->waypoint_1_x && veh->y == veh->waypoint_1_y);
}

bool is_ocean(MAP* sq) {
    return (!sq || (sq->level >> 5) < LEVEL_SHORE_LINE);
}

bool is_ocean(BASE* base) {
    return is_ocean(mapsq(base->x, base->y));
}

bool is_ocean_shelf(MAP* sq) {
    return (sq && (sq->level >> 5) == LEVEL_OCEAN_SHELF);
}

bool is_shore_level(MAP* sq) {
    return (sq && (sq->level >> 5) == LEVEL_SHORE_LINE);
}

bool has_defenders(int x, int y, int faction) {
    assert(valid_player(faction));
    for (int i=0; i<*total_num_vehicles; i++) {
        VEH* veh = &Vehicles[i];
        UNIT* u = &Units[veh->unit_id];
        if (veh->faction_id == faction && veh->x == x && veh->y == y
        && u->weapon_type <= WPN_PSI_ATTACK && veh->triad() == TRIAD_LAND) {
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
    for (const int* t : NearbyTiles) {
        sq = mapsq(wrap(x + t[0]), y + t[1]);
        if (sq && is_ocean(sq)) {
            n++;
        }
    }
    return n;
}

int set_base_facility(int base_id, int facility_id, bool add) {
    assert(base_id >= 0 && facility_id > 0 && facility_id <= FAC_EMPTY_FACILITY_64);
    if (add) {
        Bases[base_id].facilities_built[facility_id/8] |= (1 << (facility_id % 8));
    } else {
        Bases[base_id].facilities_built[facility_id/8] &= ~(1 << (facility_id % 8));
    }
    return 0;
}

int spawn_veh(int unit_id, int faction, int x, int y, int base_id) {
    int id = veh_init(unit_id, faction, x, y);
    if (id >= 0) {
        Vehicles[id].home_base_id = base_id;
        // Set these flags to disable any non-Thinker unit automation.
        Vehicles[id].state |= VSTATE_UNK_40000;
        Vehicles[id].state &= ~VSTATE_UNK_2000;
    }
    return id;
}

char* parse_str(char* buf, int len, const char* s1, const char* s2, const char* s3, const char* s4) {
    buf[0] = '\0';
    strncat(buf, s1, len);
    strncat(buf, s2, len);
    strncat(buf, s3, len);
    strncat(buf, s4, len);
    // strlen count does not include the first null character.
    if (strlen(buf) > 0 && strlen(buf) < (size_t)len) {
        return buf;
    }
    return NULL;
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
    debug("MAP %2d %2d owner: %2d bonus: %d level: %02x alt: %02x rocks: %02x "\
          "flags: %02x reg: %02x vis: %02x unk1: %02x unk2: %02x art: %02x items: %08x lm: %08x\n",
        x, y, m->owner, bonus_at(x, y), m->level, m->altitude, m->rocks,
        m->flags, m->region, m->visibility, m->unk_1, m->unk_2, m->art_ref_id,
        m->items, m->landmarks);
}

void print_veh(int id) {
    VEH* v = &Vehicles[id];
    int speed = veh_speed(id, 0);
    debug("VEH %24s %3d %3d %d order: %2d %c %3d %3d -> %3d %3d moves: %2d speed: %2d damage: %d "
          "state: %08x flags: %04x vis: %02x mor: %d iter: %d angle: %d\n",
        Units[v->unit_id].name, v->unit_id, id, v->faction_id,
        v->move_status, (v->status_icon ? v->status_icon : ' '),
        v->x, v->y, v->waypoint_1_x, v->waypoint_1_y, speed - v->road_moves_spent, speed,
        v->damage_taken, v->state, v->flags, v->visibility, v->morale,
        v->iter_count, v->rotate_angle);
}

void print_base(int id) {
    BASE* base = &Bases[id];
    int prod = base->queue_items[0];
    debug("[ turn: %d faction: %d base: %2d x: %2d y: %2d "\
        "pop: %d tal: %d dro: %d min: %2d acc: %2d | %08x | %3d %s | %s ]\n",
        *current_turn, base->faction_id, id, base->x, base->y,
        base->pop_size, base->talent_total, base->drone_total,
        base->mineral_surplus, base->minerals_accumulated,
        base->status_flags, prod, prod_name(prod), (char*)&(base->name));
}

/*
Original Offset: 005C0DB0
*/
bool __cdecl can_arty(int unit_id, bool allow_sea_arty) {
    UNIT& u = Units[unit_id];
    if ((Weapon[u.weapon_type].offense_value <= 0 // PSI + non-combat
    || Armor[u.armor_type].defense_value < 0) // PSI
    && unit_id != BSC_SPORE_LAUNCHER) { // Spore Launcher exception
        return false;
    }
    if (u.triad() == TRIAD_SEA) {
        return allow_sea_arty;
    }
    if (u.triad() == TRIAD_AIR) {
        return false;
    }
    return has_abil(unit_id, ABL_ARTILLERY); // TRIAD_LAND
}

/*
Goal types that are entirely redundant for Thinker AIs.
*/
bool ignore_goal(int type) {
    return type == AI_GOAL_COLONIZE || type == AI_GOAL_TERRAFORM_LAND
        || type == AI_GOAL_TERRAFORM_WATER || type == AI_GOAL_CONDENSER
        || type == AI_GOAL_THERMAL_BOREHOLE || type == AI_GOAL_ECHELON_MIRROR
        || type == AI_GOAL_SENSOR_ARRAY || type == AI_GOAL_DEFEND;
}

/*
Original Offset: 0x579A30
*/
void __cdecl add_goal(int faction, int type, int priority, int x, int y, int base_id) {
    if (!mapsq(x, y)) {
        return;
    }
    if (ai_enabled(faction) && type < 200) {
        return;
    }
    debug("add_goal %d type: %3d pr: %2d x: %3d y: %3d base: %d\n",
        faction, type, priority, x, y, base_id);

    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal& goal = Factions[faction].goals[i];
        if (goal.x == x && goal.y == y && goal.type == type) {
            if (goal.priority <= priority) {
                goal.priority = (int16_t)priority;
            }
            return;
        }
    }
    int goal_score = 0, goal_id = -1;
    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal& goal = Factions[faction].goals[i];

        if (goal.type < 0 || goal.priority < priority) {
            int cmp = goal.type >= 0 ? 0 : 1000;
            if (!cmp) {
                cmp = goal.priority > 0 ? 20 - goal.priority : goal.priority + 100;
            }
            if (cmp > goal_score) {
                goal_score = cmp;
                goal_id = i;
            }
        }
    }
    if (goal_id >= 0) {
        Goal& goal = Factions[faction].goals[goal_id];
        goal.type = (int16_t)type;
        goal.priority = (int16_t)priority;
        goal.x = x;
        goal.y = y;
        goal.base_id = base_id;
    }
}

/*
Original Offset: 0x579D80
*/
void __cdecl wipe_goals(int faction) {
    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal& goal = Factions[faction].goals[i];
        // Mod: instead of always deleting decrement priority each turn
        if (goal.priority > 0) {
            goal.priority--;
        }
        if (goal.priority <= 0) {
            goal.type = AI_GOAL_UNUSED;
        }
    }
    for (int i = 0; i < MaxSitesNum; i++) {
        Goal& site = Factions[faction].sites[i];
        int16_t type = site.type;
        if (type >= 0) {
            add_goal(faction, type, site.priority, site.x, site.y, -1);
        }
    }
}

int has_goal(int faction, int type, int x, int y) {
    assert(faction < MaxPlayerNum && mapsq(x, y));
    for (int i = 0; i < MaxGoalsNum; i++) {
        Goal& goal = Factions[faction].goals[i];
        if (goal.priority > 0 && goal.x == x && goal.y == y && goal.type == type) {
            return goal.priority;
        }
    }
    return 0;
}

std::vector<MapTile> iterate_tiles(int x, int y, int start_index, int end_index) {
    std::vector<MapTile> tiles;
    assert(start_index < end_index && (unsigned)end_index <= sizeof(TableOffsetX)/sizeof(int));

    for (int i = start_index; i < end_index; i++) {
        int x2 = wrap(x + TableOffsetX[i]);
        int y2 = y + TableOffsetY[i];
        MAP* sq = mapsq(x2, y2);
        if (sq) {
            tiles.push_back({x2, y2, sq});
        }
    }
    return tiles;
}

void TileSearch::reset() {
    type = 0;
    head = 0;
    tail = 0;
    owner = 0;
    roads = 0;
    y_skip = 0;
    oldtiles.clear();
}

void TileSearch::add_start(int x, int y) {
    if (tail < QueueSize/2 && (sq = mapsq(x, y)) && !oldtiles.count({x, y})) {
        owner = sq->owner;
        paths[tail] = {x, y, 0, -1};
        oldtiles.insert({x, y});
        if (!is_ocean(sq)) {
            if (sq->items & (TERRA_ROAD | TERRA_BASE_IN_TILE)) {
                roads |= TERRA_ROAD | TERRA_BASE_IN_TILE;
            }
            if (sq->items & (TERRA_RIVER | TERRA_BASE_IN_TILE)) {
                roads |= TERRA_RIVER | TERRA_BASE_IN_TILE;
            }
        }
        tail++;
    }
}

void TileSearch::init(int x, int y, int tp) {
    assert(tp >= TS_TRIAD_LAND && tp <= MaxTileSearchType);
    reset();
    type = tp;
    add_start(x, y);
}

void TileSearch::init(int x, int y, int tp, int skip) {
    init(x, y, tp);
    y_skip = skip;
}

void TileSearch::init(const PointList& points, int tp, int skip) {
    reset();
    type = tp;
    y_skip = skip;
    for (auto& p : points) {
        add_start(p.x, p.y);
    }
}

PointList& TileSearch::get_route(PointList& pp) {
    pp.clear();
    int i = 0;
    int j = cur;
    while (j >= 0 && ++i < PathLimit) {
        auto p = paths[j];
        j = p.prev;
        pp.push_front({p.x, p.y});
    }
    return pp;
}

/*
Traverse current search path and check for zones of control.
*/
bool TileSearch::has_zoc(int faction) {
    int zoc = 0;
    int i = 0;
    int j = cur;
    while (j >= 0 && ++i < PathLimit) {
        auto p = paths[j];
        if (zoc_any(p.x, p.y, faction) && ++zoc > 1) {
            return true;
        }
        j = p.prev;
    }
    return false;
}

/*
Implement a breadth-first search of adjacent map tiles to iterate possible movement paths.
Pathnodes are also used to keep track of the route to reach the current destination.
Returns first tile if instructed.
*/
MAP* TileSearch::get_next() {
    while (head < tail) {
        bool first = paths[head].dist == 0;
        rx = paths[head].x;
        ry = paths[head].y;
        dist = paths[head].dist;
        cur = head;
        head++;
        if (!(sq = mapsq(rx, ry))) {
            continue;
        }
        bool skip = (type == TS_TRIAD_LAND && is_ocean(sq)) ||
                    (type == TS_TRIAD_SEA && !is_ocean(sq)) ||
                    (type == TS_NEAR_ROADS && (is_ocean(sq) || !(sq->items & roads))) ||
                    (type == TS_TERRITORY_LAND && (is_ocean(sq) || sq->owner != owner)) ||
                    (type == TS_TERRITORY_BORDERS && (is_ocean(sq) || sq->owner != owner)) ||
                    (type == TS_SEA_AND_SHORE && !is_ocean(sq));
        if (!first && skip) {
            if ((type == TS_NEAR_ROADS && !is_ocean(sq))
            || type == TS_TERRITORY_BORDERS
            || type == TS_SEA_AND_SHORE) {
                return sq;
            }
            continue;
        }
        for (const int* t : NearbyTiles) {
            int x2 = wrap(rx + t[0]);
            int y2 = ry + t[1];
            if (y2 >= y_skip && y2 < *map_axis_y - y_skip
            && tail < QueueSize && dist < PathLimit
            && !oldtiles.count({x2, y2})) {
                paths[tail] = {x2, y2, dist+1, cur};
                tail++;
                oldtiles.insert({x2, y2});
            }
        }
        if (returnFirstTile || !first)
		{
            return sq;
        }
    }
    return NULL;
}

/*
Does not return first tile by default.
*/
MAP* TileSearch::get_next()
{
	return get_next(false);
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

bool isVehicleIdle(int vehicleId)
{
	return (tx_vehicles[vehicleId].move_status == ORDER_NONE);
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

	for (int offsetIndex = (startWithCenter ? 0 : 1); offsetIndex < BASE_RADIUS_TILE_OFFSET_COUNT; offsetIndex++)
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

	for (int offsetIndex = (startWithCenter ? 0 : 1); offsetIndex < BASE_RADIUS_TILE_OFFSET_COUNT; offsetIndex++)
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

/*
Returns all tiles within base radius belonging to friendly base radiuses.
*/
int getFriendlyIntersectedBaseRadiusTileCount(int factionId, int x, int y)
{
	int friendlyIntersectedBaseRadiusTileCount = 0;

	for (int offsetIndex = 0; offsetIndex < BASE_RADIUS_TILE_OFFSET_COUNT; offsetIndex++)
	{
		MAP *tile = getMapTile(wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]), y + BASE_TILE_OFFSETS[offsetIndex][1]);

		if (tile == NULL)
			continue;

		// add friendly intersected base radius

		if (tile->owner == factionId && map_has_item(tile, TERRA_BASE_RADIUS))
		{
			friendlyIntersectedBaseRadiusTileCount++;
		}

	}

	return friendlyIntersectedBaseRadiusTileCount;

}

/*
Returns all land tiles side adjacent to base radius belonging to friendly base radiuses.
*/
int getFriendlyLandBorderedBaseRadiusTileCount(int factionId, int x, int y)
{
	int friendlyLandBorderedBaseRadiusTileCount = 0;

	for (int offsetIndex = BASE_RADIUS_TILE_OFFSET_COUNT; offsetIndex < BASE_RADIUS_BORDERED_TILE_OFFSET_COUNT; offsetIndex++)
	{
		MAP *tile = getMapTile(wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]), y + BASE_TILE_OFFSETS[offsetIndex][1]);

		if (tile == NULL)
			continue;

		// add friendly bordered base radius

		if (!is_ocean(tile) && tile->owner == factionId && map_has_item(tile, TERRA_BASE_RADIUS))
		{
			friendlyLandBorderedBaseRadiusTileCount++;
		}

	}

	return friendlyLandBorderedBaseRadiusTileCount;

}

std::vector<MAP *> getBaseWorkedTiles(int baseId)
{
	BASE *base = &(tx_bases[baseId]);

	std::vector<MAP *> baseWorkedTiles;

	for (int offsetIndex = 1; offsetIndex < BASE_RADIUS_TILE_OFFSET_COUNT; offsetIndex++)
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

	for (int offsetIndex = 1; offsetIndex < BASE_RADIUS_TILE_OFFSET_COUNT; offsetIndex++)
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

void setTerraformingAction(int id, int action)
{
	VEH *vehicle = &(tx_vehicles[id]);

	// subtract raise/lower land cost

	if (action == FORMER_RAISE_LAND || action == FORMER_LOWER_LAND)
	{
		tx_factions[vehicle->faction_id].energy_credits -= tx_terraform_cost(vehicle->x, vehicle->y, vehicle->faction_id);
	}

	// set action

	set_action(id, action + 4, veh_status_icon[action + 4]);

}

double getImprovedTileWeightedYield(MAP_INFO *tileInfo, int terraformingActionsCount, int terraformingActions[], double nutrientWeight, double mineralWeight, double energyWeight)
{
	int x = tileInfo->x;
	int y = tileInfo->y;

	// populate current map states

	MAP_STATE currentMapState;

	getMapState(tileInfo, &currentMapState);

	// populate improved map state

	MAP_STATE improvedMapState;

	copyMapState(&improvedMapState, &currentMapState);

	generateTerraformingChange(&improvedMapState, FORMER_REMOVE_FUNGUS);
	for (int i = 0; i < terraformingActionsCount; i++)
	{
		generateTerraformingChange(&improvedMapState, terraformingActions[i]);
	}

	// apply changes

	setMapState(tileInfo, &improvedMapState);

	// gather improved yield

	int nutrientYield = mod_nutrient_yield(0, -1, x, y, 0);
	int mineralYield = mod_mineral_yield(0, -1, x, y, 0);
	int energyYield = mod_energy_yield(0, -1, x, y, 0);

	// restore original state

	setMapState(tileInfo, &currentMapState);

	// return weighted yield

	return nutrientWeight * (double)nutrientYield + mineralWeight * (double)mineralYield + energyWeight * (double)energyYield;

}

void getMapState(MAP_INFO *mapInfo, MAP_STATE *mapState)
{
	MAP *tile = mapInfo->tile;

	mapState->climate = tile->level;
	mapState->rocks = tile->rocks;
	mapState->items = tile->items;

}

void setMapState(MAP_INFO *mapInfo, MAP_STATE *mapState)
{
	MAP *tile = mapInfo->tile;

	tile->level = mapState->climate;
	tile->rocks = mapState->rocks;
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

			mapState->items &= ~tx_terraform[action].removed_items_flag;

			// add items

			mapState->items |= tx_terraform[action].added_items_flag;

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
Checks whether given tile is closest to this colony than to any other friendly colony.
*/
bool isRightfulBuildSite(int x, int y, int vehicleId)
{
	MAP *tile = getMapTile(x, y);
	VEH *vehicle = &(tx_vehicles[vehicleId]);
	MAP *vehicleLocation = getVehicleMapTile(vehicleId);
	int triad = veh_triad(vehicleId);
	int factionId = vehicle->faction_id;
	int vehicleRange = map_range(vehicle->x, vehicle->y, x, y);

	// vehicle should be a colony

	if (!isVehicleColony(vehicleId))
		return false;

	// site should be in same region

	if (triad != TRIAD_AIR && !(triad == TRIAD_SEA && map_has_item(vehicleLocation, TERRA_BASE_IN_TILE)) && vehicleLocation->region != tile->region)
		return false;

	// get nearest friendly colony range

	int nearestFriendlyColonyRange = INT_MAX;

	for (int otherVehicleId = 0; otherVehicleId < *total_num_vehicles; otherVehicleId++)
	{
		VEH *otherVehicle = &(tx_vehicles[otherVehicleId]);
		MAP *otherVehicleLocation = getVehicleMapTile(otherVehicleId);
		int otherVehicleTriad = veh_triad(otherVehicleId);

		// skip current vehicle

		if (otherVehicleId == vehicleId)
			continue;

		// only this faction

		if (otherVehicle->faction_id != factionId)
			continue;

		// only colony

		if (!isVehicleColony(otherVehicleId))
			continue;

		// only able to reach destination

		if (otherVehicleTriad != TRIAD_AIR && !(otherVehicleTriad == TRIAD_SEA && map_has_item(otherVehicleLocation, TERRA_BASE_IN_TILE)) && otherVehicleLocation->region != tile->region)
			continue;

		// calculate range

		int otherVehicleRange = map_range(otherVehicle->x, otherVehicle->y, x, y);

		// add 1 for vehicles with bigger id

		if (otherVehicleId > vehicleId)
		{
			otherVehicleRange++;
		}

		// update nearest range

		nearestFriendlyColonyRange = min(nearestFriendlyColonyRange, otherVehicleRange);

	}

	// check rightfullness

	return vehicleRange < nearestFriendlyColonyRange;

}

bool isCoast(int x, int y)
{
	MAP *tile = getMapTile(x, y);

	if (is_ocean(tile))
		return false;

	for (MAP *adjacentTile : getAdjacentTiles(x, y, false))
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

	for (MAP *adjacentTile : getAdjacentTiles(x, y, false))
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

	for (int stackedVehicleId : getStackedVehicleIds(vehicleId))
	{
		VEH *stackedVehicle = &(tx_vehicles[stackedVehicleId]);

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

