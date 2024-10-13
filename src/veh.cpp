
#include "veh.h"

static TileSearch ts;


bool __cdecl can_arty(int unit_id, bool allow_sea_arty) {
    UNIT& u = Units[unit_id];
    if ((Weapon[u.weapon_id].offense_value <= 0 // PSI + non-combat
    || Armor[u.armor_id].defense_value < 0) // PSI
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

bool __cdecl has_abil(int unit_id, VehAblFlag ability) {
    int faction_id = unit_id / MaxProtoFactionNum;
    // workaround fix for legacy base_build that may incorrectly use negative unit_id
    if (unit_id < 0) {
        return false;
    }
    if (Units[unit_id].ability_flags & ability) {
        return true;
    }
    if (!faction_id) {
        return false; // skip basic prototypes from #UNITS
    }
    if (is_alien(faction_id) && ability == ABL_DEEP_RADAR) {
        return true; // Caretakers + Usurpers > "Deep Radar" ability for all units
    }
    // Pirates > "Marine Detachment" ability for combat sea units with Adaptive Doctrine
    for (int i = 0; i < MFactions[faction_id].faction_bonus_count; i++) {
        if (MFactions[faction_id].faction_bonus_id[i] == FCB_FREEABIL_PREQ) {
            int abil_bonus_id = MFactions[faction_id].faction_bonus_val1[i];
            if (has_tech(Ability[abil_bonus_id].preq_tech, faction_id)
            && (ability & (1 << abil_bonus_id))) {
                return true;
            }
        }
    }
    // Nethack Terminus allows all probe teams to act as they have "Algorithmic Enhancement".
    // Manual implies fusion reactor or better is required for this but has_abil does not check it.
    if (ability == ABL_ALGO_ENHANCEMENT && Units[unit_id].is_probe()
    && has_project(FAC_NETHACK_TERMINUS, faction_id)) {
        return true;
    }
    return false;
}

bool has_ability(int faction, VehAbl abl, VehChassis chs, VehWeapon wpn) {
    int F = Ability[abl].flags;
    int triad = Chassis[chs].triad;
    bool is_combat = Weapon[wpn].offense_value != 0;
    bool is_former = Weapon[wpn].mode == WMODE_TERRAFORMER;
    bool is_probe = Weapon[wpn].mode == WMODE_INFOWAR;

    if ((triad == TRIAD_LAND && ~F & AFLAG_ALLOWED_LAND_UNIT)
    || (triad == TRIAD_SEA && ~F & AFLAG_ALLOWED_SEA_UNIT)
    || (triad == TRIAD_AIR && ~F & AFLAG_ALLOWED_AIR_UNIT)) {
        return false;
    }
    if ((~F & AFLAG_ALLOWED_COMBAT_UNIT && is_combat)
    || (~F & AFLAG_ALLOWED_TERRAFORM_UNIT && is_former)
    || (~F & AFLAG_ALLOWED_NONCOMBAT_UNIT && !is_combat && !is_former)) {
        return false;
    }
    if ((F & AFLAG_NOT_ALLOWED_PROBE_TEAM && is_probe)
    || (F & AFLAG_ONLY_PROBE_TEAM && !is_probe)) {
        return false;
    }
    if (F & AFLAG_TRANSPORT_ONLY_UNIT && Weapon[wpn].mode != WMODE_TRANSPORT) {
        return false;
    }
    if (F & AFLAG_NOT_ALLOWED_FAST_UNIT && Chassis[chs].speed > 1) {
        return false;
    }
    if (F & AFLAG_NOT_ALLOWED_PSI_UNIT && Weapon[wpn].offense_value < 0) {
        return false;
    }
    return has_tech(Ability[abl].preq_tech, faction);
}

/*
Renamed from veh_at. This version never calls rebuild_vehicle_bits on failed searches.
*/
int __cdecl veh_stack(int x, int y) {
    MAP* sq = mapsq(x, y);
    if (!sq || !sq->veh_in_tile()) {
        return -1; // invalid or empty map tile
    }
    for (int veh_id = 0; veh_id < *VehCount; veh_id++) {
        if (Vehs[veh_id].x == x && Vehs[veh_id].y == y) {
            return veh_top(veh_id);
        }
    }
    return -1;
}

/*
Renamed from speed_proto. Calculate the road move speed for specified prototype.
*/
int __cdecl proto_speed(int unit_id) {
    if (unit_id == BSC_FUNGAL_TOWER) {
        return 0; // cannot move
    }
    UNIT* u = &Units[unit_id];
    int speed_val = u->speed();
    int triad = u->triad();
    int weapon_id = Units[unit_id].weapon_id;

    if (triad == TRIAD_AIR) {
        speed_val += Units[unit_id].reactor_id * 2;
    }
    if (Weapon[weapon_id].mode == WMODE_TRANSPORT) {
        speed_val--;
    }
    if (has_abil(unit_id, ABL_SLOW)) {
        speed_val--;
    }
    if (has_abil(unit_id, ABL_ANTIGRAV_STRUTS)) {
        speed_val += (triad == TRIAD_AIR) ? Units[unit_id].reactor_id * 2 : 1;
    }
    if (triad == TRIAD_AIR) {
        if (has_abil(unit_id, ABL_FUEL_NANOCELLS)) {
            speed_val += 2;
        }
        if (has_project(FAC_CLOUDBASE_ACADEMY, unit_id / MaxProtoFactionNum)) {
            speed_val += 2; // bonus from Aerospace Complex
        }
        if (has_abil(unit_id, ABL_AIR_SUPERIORITY)) {
            // generally -20% to -25%, in some cases higher due to lossy division rounding
            speed_val = (speed_val * 4) / 5;
        }
        if (Weapon[weapon_id].mode == WMODE_TRANSPORT) {
            speed_val /= 2; // 2nd penalty for air transports: -50%
        }
    }
    speed_val = clamp(speed_val, 1, 99) * Rules->move_rate_roads;
    assert(speed_val == speed_proto(unit_id));
    return speed_val;
}

/*
Renamed from speed. Calculate the speed of a unit on roads taking into consideration
various factors. Parameter skip_morale should be normally set to false and
seems to be only set to true for certain combat calculations in battle_fight.
Fix: due to limited size of moves_spent field, speeds over 255 are not allowed.
*/
int __cdecl veh_speed(int veh_id, bool skip_morale) {
    int unit_id = Vehs[veh_id].unit_id;
    UNIT* u = &Units[unit_id];
    bool normal_unit = unit_id >= MaxProtoFactionNum || u->offense_value() >= 0;
    if (unit_id == BSC_FUNGAL_TOWER) {
        return 0; // cannot move
    }
    // Maximum speed is rounded down to nearest full movement point
    int max_speed = 255 - (255 % Rules->move_rate_roads);
    int speed_val = proto_speed(unit_id);
    int triad = u->triad();
    if (triad == TRIAD_SEA && has_project(FAC_MARITIME_CONTROL_CENTER, Vehs[veh_id].faction_id)) {
        speed_val += Rules->move_rate_roads * 2;
    }
    if (!skip_morale && mod_morale_veh(veh_id, true, 0) == MORALE_ELITE
    && ((normal_unit && conf.normal_elite_moves > 0)
    || (!normal_unit && conf.native_elite_moves > 0))) {
        speed_val += Rules->move_rate_roads;
    }
    if (Vehs[veh_id].damage_taken && triad != TRIAD_AIR) {
        int moves = speed_val / Rules->move_rate_roads;
        int reactor_val;
        if (u->plan == PLAN_ALIEN_ARTIFACT) {
            reactor_val = 1;
            speed_val = 1;
        } else {
            reactor_val = clamp((int)u->reactor_id, 1, 100) * 10;
            speed_val = clamp(reactor_val, 1, 99);
        }
        speed_val = (moves * clamp(reactor_val - Vehs[veh_id].damage_taken, 0, 9999)
            + speed_val - 1) / speed_val;
        speed_val = clamp(speed_val, (triad == TRIAD_SEA) ? 2 : 1, 999) * Rules->move_rate_roads;
    }
    return min(max_speed, speed_val);
}

/*
Original version assigned cargo capacity on Spore Launchers but it seems this feature is not used.
*/
int __cdecl veh_cargo(int veh_id) {
    VEH* v = &Vehicles[veh_id];
    UNIT* u = &Units[v->unit_id];
    if (u->carry_capacity > 0 && veh_id < MaxProtoFactionNum
    && Weapon[u->weapon_id].offense_value < 0) {
        return v->morale + 1;
    }
    return u->carry_capacity;
}

/*
Calculates cost of the prototype based on various factors. Optimized logic flow from
the original without any differences to the final calculation.
*/
int __cdecl mod_proto_cost(VehChassis chassis_id, VehWeapon weapon_id,
VehArmor armor_id, VehAblFlag ability, VehReactor reactor_id) {
    uint8_t weap_cost = Weapon[weapon_id].cost;
    // PB check: moved to start vs after 1st triad checks in original > no difference in logic
    if (Chassis[chassis_id].missile && Weapon[weapon_id].offense_value >= 99) {
        return weap_cost;
    }
    uint8_t triad = Chassis[chassis_id].triad;
    uint32_t armor_cost = Armor[armor_id].cost;
    uint32_t speed_cost = Chassis[chassis_id].cost;
    int abil_modifier = 0;
    int flags_modifier = 0;
    if (ability) {
        for (int i = 0; i < MaxAbilityNum; i++) {
            if ((1 << i) & ability) {
                if (abil_modifier) {
                    abil_modifier++; // Increased cost with more than one ability
                }
                int factor = Ability[i].cost;
                if (factor > 0) { // 1+ = Straight Cost; 25% increase per unit of cost
                    abil_modifier += factor;
                } else {
                    switch (factor) {
                      case 0: // None
                      default:
                        break;
                        // Increases w/ ratio of weapon to armor: 0, 1, or 2. Rounded DOWN.
                        // Never higher than 2.
                      case -1:
                        // fixed potential crash: will never trigger in vanilla but could with mods
                        if (armor_cost) {
                            abil_modifier += clamp(weap_cost / armor_cost, 0u, 2u);
                        }
                        break;
                      case -2: // Increases w/ weapon value
                        abil_modifier += weap_cost - 1;
                        break;
                      case -3: // Increases w/ armor value
                        abil_modifier += armor_cost - 1;
                        break;
                      case -4: // Increases w/ speed value
                        abil_modifier += speed_cost - 1;
                        break;
                      case -5: // Increases w/ weapon+armor value
                        abil_modifier += weap_cost + armor_cost - 2;
                        break;
                      case -6: // Increases w/ weapon+speed value
                        abil_modifier += weap_cost + speed_cost - 2;
                        break;
                      case -7: // Increases w/ armor+speed value
                        abil_modifier += armor_cost + speed_cost - 2;
                        break;
                    }
                }
                // 010000000000 - Cost increased for land units; Deep Radar
                // Shifted flag check into main ability loop rather than its
                // own loop at 1st triad checks
                if (Ability[i].flags & AFLAG_COST_INC_LAND_UNIT && triad == TRIAD_LAND) {
                    // separate variable keeps logic same (two abilities, both with cost 0,
                    // one with cost increase flag will trigger above "if (abil_modifier)" if
                    // this is directly abil_modifier++)
                    flags_modifier++;
                }
            }
        }
        abil_modifier += flags_modifier; // adding here keeps logic consistent after optimization
    }
    if (triad == TRIAD_SEA) {
        armor_cost /= 2;
        speed_cost += reactor_id;
    } else if (triad == TRIAD_AIR) {
        if (armor_cost > 1) {
            armor_cost *= reactor_id * 2;
        }
        speed_cost += reactor_id * 2;
    } // TRIAD_LAND ability flag check moved into ability loop above
    uint32_t combat_mod = armor_cost / 2 + 1;
    if (combat_mod < weap_cost) { // which ever is greater
        combat_mod = weap_cost;
    }
    int proto_mod;
    // shifted this check to top vs at end > no difference in logic
    if (combat_mod == 1 && armor_cost == 1 && speed_cost == 1 && reactor_id == REC_FISSION) {
        proto_mod = 1;
    } else {
        // (2 << n) == 2^(n + 1) ; (2 << n) / 2 == 2 ^ n;
        // will crash if reactor_id is 0xFF > divide by zero; not putting in error checking
        // since this is unlikely even with modding however noting for future
        proto_mod = ((speed_cost + armor_cost) * combat_mod + ((2 << reactor_id) / 2))
            / (2 << reactor_id);
        if (speed_cost == 1) {
            proto_mod = (proto_mod / 2) + 1;
        }
        if (weap_cost > 1 && Armor[armor_id].cost > 1) {
            proto_mod++;
            // moved inside nested check vs separate triad checks below > no difference in logic
            if (triad == TRIAD_LAND && speed_cost > 1) {
                proto_mod++;
            }
        }
        // excludes sea probes
        if (triad == TRIAD_SEA && Weapon[weapon_id].mode != WMODE_INFOWAR) {
            proto_mod = (proto_mod + 1) / 2;
        } else if (triad == TRIAD_AIR) {
            proto_mod /= (Weapon[weapon_id].mode > WMODE_MISSILE) ? 2 : 4; // Non-combat : Combat
        }
        int reactor_mod = (reactor_id * 3 + 1) / 2;
        if (proto_mod < reactor_mod) { // which ever is greater
            proto_mod = reactor_mod;
        }
    }
    int cost = (proto_mod * (abil_modifier + 4) + 2) / 4;
//    debug("proto_cost %d %d %d %d %d cost: %d\n",
//        chassis_id, weapon_id, armor_id, ability, reactor_id, cost);
    assert(cost == proto_cost(chassis_id, weapon_id, armor_id, ability, reactor_id));
    return cost;
}

/*
Calculates the base cost of the specified unit (without prototype modifiers).
*/
int __cdecl mod_base_cost(int unit_id) {
    return mod_proto_cost(
        (VehChassis)Units[unit_id].chassis_id,
        (VehWeapon)Units[unit_id].weapon_id,
        (VehArmor)Units[unit_id].armor_id,
        ABL_NONE,
        (VehReactor)Units[unit_id].reactor_id);
}

/*
Calculate the specified prototype's mineral row cost to build. Optional output parameter
whether there is an associated 1st time prototype cost (true) or just the base (false).
Note that has_proto_cost is treated as int32_t variable in the game engine.
*/
int __cdecl mod_veh_cost(int unit_id, int base_id, int* has_proto_cost) {
    int cost = Units[unit_id].cost;
    if (base_id >= 0 && unit_id < MaxProtoFactionNum // Fix: added base_id bounds check
    && (Units[unit_id].offense_value() < 0 || unit_id == BSC_SPORE_LAUNCHER)
    && has_fac_built(FAC_BROOD_PIT, base_id)) {
        cost = (cost * 3) / 4; // Decrease the cost of alien units by 25%
    }
    if (Units[unit_id].plan == PLAN_COLONIZATION && base_id >= 0) {
        cost = clamp(cost, 1, 999);
    }
    int proto_cost_first = 0;
    if (unit_id >= MaxProtoFactionNum && !Units[unit_id].is_prototyped()) {
        proto_cost_first = (base_id >= 0 && has_fac_built(FAC_SKUNKWORKS, base_id))
            ? 0 : (prototype_factor(unit_id) * mod_base_cost(unit_id) + 50) / 100; // moved checks up
        cost += proto_cost_first;
    }
    if (has_proto_cost) {
        *has_proto_cost = proto_cost_first != 0;
    }
    assert(cost == veh_cost(unit_id, base_id, 0));
    return cost;
}

/*
Calculate upgrade cost between two prototypes in mineral rows.
*/
int __cdecl mod_upgrade_cost(int faction, int new_unit_id, int old_unit_id) {
    UNIT* old_unit = &Units[old_unit_id];
    UNIT* new_unit = &Units[new_unit_id];
    int cost;

    if (conf.modify_upgrade_cost) {
        if (new_unit->is_supply()) {
            cost = max((int)new_unit->cost, 4*max(1, new_unit->cost - old_unit->cost));
        } else {
            cost = max((int)new_unit->cost, 2*max(1, new_unit->cost - old_unit->cost));
        }
        if (new_unit_id >= MaxProtoFactionNum && !new_unit->is_prototyped()) {
            cost *= 2;
        }
        if (has_project(FAC_NANO_FACTORY, faction)) {
            cost /= 2;
        }
    } else {
        int modifier = new_unit->cost;
        if (new_unit_id >= MaxProtoFactionNum && !new_unit->is_prototyped()) {
            modifier = ((new_unit->cost + 1) / 2 + new_unit->cost)
                * ((new_unit->cost + 1) / 2 + new_unit->cost + 1);
        }
        cost = max(0, new_unit->speed() - old_unit->speed())
            + max(0, new_unit->armor_cost() - old_unit->armor_cost())
            + max(0, new_unit->weapon_cost() - old_unit->weapon_cost())
            + modifier;
        if (has_project(FAC_NANO_FACTORY, faction)) {
            cost /= 2;
        }
        assert(cost == upgrade_cost(faction, new_unit_id, old_unit_id));
    }
    return cost;
}

int __cdecl mod_veh_init(int unit_id, int faction, int x, int y) {
    int veh_id = veh_init(unit_id, faction, x, y);
    if (veh_id >= 0) {
        Vehicles[veh_id].home_base_id = -1;
        // Set these flags to disable any non-Thinker unit automation.
        Vehicles[veh_id].state |= VSTATE_UNK_40000;
        Vehicles[veh_id].state &= ~VSTATE_UNK_2000;
    }
    return veh_id;
}

int __cdecl mod_veh_kill(int veh_id) {
    VEH* veh = &Vehicles[veh_id];
    debug("disband %2d %2d %s\n", veh->x, veh->y, veh->name());
    veh_kill(veh_id);
    return VEH_SKIP;
}

/*
Skip vehicle turn by adjusting spent moves to maximum available moves.
*/
int __cdecl mod_veh_skip(int veh_id) {
    VEH* veh = &Vehicles[veh_id];
    int moves = veh_speed(veh_id, 0);

    if (is_human(veh->faction_id)) {
        if (conf.activate_skipped_units) {
            if (!veh->moves_spent && veh->order < ORDER_FARM && !(veh->state & VSTATE_HAS_MOVED)) {
                veh->flags |= VFLAG_FULL_MOVE_SKIPPED;
            } else if (veh->triad() != TRIAD_AIR) {
                // veh_skip may get called twice on aircraft units
                veh->flags &= ~VFLAG_FULL_MOVE_SKIPPED;
            }
        }
    } else if (veh->faction_id > 0) {
        veh->waypoint_1_x = veh->x;
        veh->waypoint_1_y = veh->y;
        veh->status_icon = '-';
        if (veh->need_monolith()) {
            MAP* sq = mapsq(veh->x, veh->y);
            if (sq && sq->items & BIT_MONOLITH) {
                monolith(veh_id);
            }
        }
    }
    veh->moves_spent = moves;
    return moves;
}

int __cdecl mod_veh_wake(int veh_id) {
    VEH* veh = &Vehicles[veh_id];
    if (veh->order >= ORDER_FARM && veh->order < ORDER_MOVE_TO && !(veh->state & VSTATE_CRAWLING)) {
        veh->moves_spent = veh_speed(veh_id, 0) - Rules->move_rate_roads;
        if (veh->terraforming_turns) {
            int turns = veh->terraforming_turns - contribution(veh_id, veh->order - 4);
            veh->terraforming_turns = max(0, turns);
        }
    }
    if (veh->state & VSTATE_ON_ALERT && !(veh->state & VSTATE_HAS_MOVED) && veh->order_auto_type == ORDERA_ON_ALERT) {
        veh->moves_spent = 0;
    }
    /*
    Formers are a special case since they might not move but can build items immediately.
    When formers build something, VSTATE_HAS_MOVED should be set to prevent them from moving twice per turn.
    */
    if (conf.activate_skipped_units) {
        if (veh->flags & VFLAG_FULL_MOVE_SKIPPED && !(veh->state & VSTATE_HAS_MOVED)) {
            veh->moves_spent = 0;
        }
        veh->flags &= ~VFLAG_FULL_MOVE_SKIPPED;
    }
    veh->order = ORDER_NONE;
    veh->state &= ~(VSTATE_UNK_8000000|VSTATE_UNK_2000000|VSTATE_UNK_1000000|VSTATE_EXPLORE|VSTATE_ON_ALERT);
    return veh_id;
}

int __cdecl find_return_base(int veh_id) {
    VEH* veh = &Vehs[veh_id];
    MAP* sq;
    debug("find_return_base %2d %2d %s\n", veh->x, veh->y, veh->name());
    ts.init(veh->x, veh->y, veh->triad() == TRIAD_SEA ? TS_SEA_AND_SHORE : veh->triad());
    while ((sq = ts.get_next()) != NULL) {
        if (sq->is_base() && sq->owner == veh->faction_id) {
            return base_at(ts.rx, ts.ry);
        }
    }
    int region = region_at(veh->x, veh->y);
    int min_dist = INT_MAX;
    int base_id = -1;
    bool sea = region > MaxRegionLandNum;
    for (int i = 0; i < *BaseCount; i++) {
        BASE* base = &Bases[i];
        if (base->faction_id == veh->faction_id) {
            int base_region = region_at(base->x, base->y);
            int dist = map_range(base->x, base->y, veh->x, veh->y)
                * ((base_region > MaxRegionLandNum) == sea ? 1 : 4)
                * (base_region == region ? 1 : 8);
            if (dist < min_dist) {
                base_id = i;
                min_dist = dist;
            }
        }
    }
    return base_id;
}

int __cdecl probe_return_base(int UNUSED(x), int UNUSED(y), int veh_id) {
    return find_return_base(veh_id);
}

void parse_abl_name(char* buf, const char* name, bool reduce) {
    const int len = min(strlen(name), (size_t)MaxProtoNameLen);
    // Skip empty abbreviation names (Deep Radar)
    if (!len) {
        return;
    }
    for (int i = 0; i < len; i++) {
        if (i >= 3 && reduce && (len >= 8 || !isalpha(name[i]))) {
            strncat(buf, name, i+1);
            if (!isspace(name[i]) && name[i] != '-') {
                strncat(buf, " ", 2);
            }
            return;
        }
    }
    strncat(buf, name, len);
    if (!isspace(name[len-1]) && name[len-1] != '-') {
        strncat(buf, " ", 2);
    }
}

void parse_wpn_name(char* buf, VehWeapon wpn, bool reduce) {
    const char* name = Weapon[wpn].name_short;
    const int len = min(strlen(name), (size_t)MaxProtoNameLen);
    if (reduce) {
        for (int i = 0; i < len; i++) {
            if (isspace(name[i])) {
                strncat(buf, name, i+1);
                return;
            }
        }
    }
    if (reduce && strlen(name) >= 10) {
        strncat(buf, name, 8);
        strncat(buf, " ", 2);
        return;
    }
    strncat(buf, name, MaxProtoNameLen);
    strncat(buf, " ", 2);
}

void parse_arm_name(char* buf, VehArmor arm, bool reduce) {
    const char* name = Armor[arm].name_short;
    if (reduce && strlen(name) >= 8) {
        strncat(buf, name, 4);
        strncat(buf, " ", 2);
        return;
    }
    strncat(buf, name, MaxProtoNameLen);
    strncat(buf, " ", 2);
}

void parse_chs_name(char* buf, const char* name1, const char* name2) {
    // Convert Speeder to Rover (short form)
    if (strlen(name1) >= 7 && strlen(name1) > strlen(name2) && strlen(name2)) {
        strncat(buf, name2, MaxProtoNameLen);
        strncat(buf, " ", 2);
        return;
    }
    strncat(buf, name1, MaxProtoNameLen);
    strncat(buf, " ", 2);
}

void parse_chs_name(char* buf, const char* name) {
    strncat(buf, name, MaxProtoNameLen);
    strncat(buf, " ", 2);
}

int __cdecl mod_name_proto(char* name, int unit_id, int faction_id,
VehChassis chs, VehWeapon wpn, VehArmor arm, VehAblFlag abls, VehReactor rec) {
    char buf[256];
    bool noncombat = Weapon[wpn].mode == WMODE_CONVOY
        || Weapon[wpn].mode == WMODE_INFOWAR
        || Weapon[wpn].mode == WMODE_TERRAFORMER
        || Weapon[wpn].mode == WMODE_TRANSPORT
        || Weapon[wpn].mode == WMODE_COLONIST;
    bool combat = Weapon[wpn].offense_value != 0
        && Armor[arm].defense_value != 0;
    bool psi_arm = Armor[arm].defense_value < 0;
    bool psi_wpn = Weapon[wpn].offense_value < 0;
    int wpn_v = (psi_wpn ? 2 : Weapon[wpn].offense_value);
    int arm_v = (psi_arm ? 2 : Armor[arm].defense_value);
    int triad = Chassis[chs].triad;
    int spd_v = Chassis[chs].speed;
    bool arty = abls & ABL_ARTILLERY;
    bool marine = abls & ABL_AMPHIBIOUS;
    bool intercept = abls & ABL_AIR_SUPERIORITY && triad == TRIAD_AIR;
    bool garrison = combat && !arty && !marine && wpn_v < arm_v && spd_v < 2;
    uint32_t prefix_abls = abls & ~(ABL_ARTILLERY | ABL_AMPHIBIOUS | ABL_NERVE_GAS
        | (intercept ? ABL_AIR_SUPERIORITY : 0)); // SAM for ground units
    const char* mk_label = (*TextLabels)[919]; // Mk (reactors)

    if (!conf.new_unit_names || (!combat && !noncombat) || Chassis[chs].missile) {
        if (unit_id < 0) {
            if (name) {
                name[0] = '\0';
            }
            return 0;
        }
        return name_proto(name, unit_id, faction_id, chs, wpn, arm, abls, rec);
    }
    for (int i = 0; i < 2; i++) {
        buf[0] = '\0';
        if (abls & ABL_NERVE_GAS) {
            parse_abl_name(buf, Ability[ABL_ID_NERVE_GAS].abbreviation, i > 0);
        }
        for (int j = 0; j < MaxAbilityNum; j++) {
            if (prefix_abls & (1 << j)) {
                parse_abl_name(buf, Ability[j].abbreviation, i > 0);
            }
        }
        if (arm_v != 1 && !(combat && !psi_arm && wpn_v >= 4*arm_v)) {
            parse_arm_name(buf, arm, i > 0 || abls || spd_v > 1 || wpn_v >= arm_v
                || (noncombat && strlen(Weapon[wpn].name_short) >= 10));
        }
        if (combat) {
            if (!garrison) {
                if (triad == TRIAD_LAND && Weapon[wpn].offense_value == 1) {
                    strncat(buf, (*TextLabels)[146], 16); // Scout
                    strncat(buf, " ", 2);
                } else if (triad == TRIAD_LAND && Weapon[wpn].offense_value == 2 && spd_v > 1) {
                    strncat(buf, (*TextLabels)[182], 16); // Recon
                    strncat(buf, " ", 2);
                } else {
                    parse_wpn_name(buf, wpn, i > 0);
                }
            }
            if ((!arty && !marine && !intercept) || (arty && spd_v > 1)) {
                int value = wpn_v - arm_v;
                if (value <= -1) { // Defensive
                    if (arm_v >= 8 && wpn_v*2 >= arm_v) {
                        parse_chs_name(buf, Chassis[chs].defsv_name_lrg);
                    } else if (wpn_v >= 2) {
                        parse_chs_name(buf, Chassis[chs].defsv1_name);
                    } else {
                        parse_chs_name(buf, Chassis[chs].defsv2_name);
                    }
                } else if (value >= 1) { // Offensive
                    if (wpn_v >= 8 && arm_v*2 >= wpn_v) {
                        parse_chs_name(buf, Chassis[chs].offsv_name_lrg);
                    } else if (triad != TRIAD_AIR && arm_v >= 2) {
                        parse_chs_name(buf, Chassis[chs].offsv1_name);
                    } else if (triad == TRIAD_AIR && !((wpn_v + arm_v) & 4)) {
                        parse_chs_name(buf, Chassis[chs].offsv1_name);
                    } else {
                        parse_chs_name(buf, Chassis[chs].offsv2_name);
                    }
                } else if (triad != LAND || spd_v == 1) { // Scout units
                    parse_chs_name(buf, Chassis[chs].offsv1_name);
                } else {
                    parse_chs_name(buf, Chassis[chs].offsv2_name);
                }
            }
            if (arty) {
                if ((wpn_v + arm_v) & 4) {
                    strncat(buf, (*TextLabels)[543], 16); // Artillery
                } else {
                    strncat(buf, (*TextLabels)[544], 16); // Battery
                }
                strncat(buf, " ", 2);
            }
            if (marine) {
                if ((wpn_v + arm_v) & 4) {
                    strncat(buf, (*TextLabels)[614], 16); // Marines
                } else {
                    strncat(buf, (*TextLabels)[615], 16); // Invaders
                }
                strncat(buf, " ", 2);
            }
            if (intercept) {
                if ((wpn_v + arm_v) & 4) {
                    parse_chs_name(buf, Chassis[chs].defsv1_name);
                } else {
                    parse_chs_name(buf, Chassis[chs].defsv2_name);
                }
            }
        } else {
            if (spd_v > 1) {
                parse_chs_name(buf, Chassis[chs].offsv1_name, Chassis[chs].offsv2_name);
            }
            parse_wpn_name(buf, wpn, i > 0);
        }
        if (Weapon[wpn].mode != WMODE_INFOWAR && rec > REC_FISSION) {
            if (strlen(buf) + strlen(mk_label) + 3 <= MaxProtoNameLen) {
                snprintf(buf, MaxProtoNameLen, "%s%s%d", buf, mk_label, rec);
            } else if (i == 0) {
                continue;
            }
        }
        strtrail(buf);
        if (strlen(buf) + 4 <= MaxProtoNameLen) {
            break;
        }
    }
    strncpy(name, buf, MaxProtoNameLen);
    name[MaxProtoNameLen - 1] = '\0';
    return 0;
}

VehArmor best_armor(int faction, bool cheap) {
    int ci = ARM_NO_ARMOR;
    int cv = 0;
    for (int i = ARM_NO_ARMOR; i <= ARM_RESONANCE_8_ARMOR; i++) {
        if (has_tech(Armor[i].preq_tech, faction)) {
            int val = Armor[i].defense_value;
            int cost = Armor[i].cost;
            if (cheap && (cost > 5 || cost > val)) {
                continue;
            }
            int iv = val * (i >= ARM_PULSE_3_ARMOR ? 5 : 4);
            if (iv > cv) {
                cv = iv;
                ci = i;
            }
        }
    }
    return (VehArmor)ci;
}

VehWeapon best_weapon(int faction) {
    int ci = WPN_HAND_WEAPONS;
    int cv = 0;
    for (int i = WPN_HAND_WEAPONS; i <= WPN_STRING_DISRUPTOR; i++) {
        if (has_tech(Weapon[i].preq_tech, faction)) {
            int iv = Weapon[i].offense_value *
                (i == WPN_RESONANCE_LASER || i == WPN_RESONANCE_BOLT ? 5 : 4);
            if (iv > cv) {
                cv = iv;
                ci = i;
            }
        }
    }
    return (VehWeapon)ci;
}

VehReactor best_reactor(int faction) {
    for (VehReactor r : {REC_SINGULARITY, REC_QUANTUM, REC_FUSION}) {
        if (has_tech(Reactor[r - 1].preq_tech, faction)) {
            return r;
        }
    }
    return REC_FISSION;
}

int offense_value(int unit_id) {
    assert(unit_id >= 0 && unit_id < MaxProtoNum);
    UNIT* u = &Units[unit_id];
    int w = (conf.ignore_reactor_power ? (int)REC_FISSION : u->reactor_id);
    if (u->is_missile() && !u->is_planet_buster()) {
        // Conv missiles might have nominally low attack rating
        int faction = unit_id / MaxProtoFactionNum;
        int max_value = max(1, (int)Weapon[best_weapon(faction)].offense_value);
        return max_value * w;
    }
    return Weapon[u->weapon_id].offense_value * w;
}

int defense_value(int unit_id) {
    assert(unit_id >= 0 && unit_id < MaxProtoNum);
    UNIT* u = &Units[unit_id];
    int w = (conf.ignore_reactor_power ? (int)REC_FISSION : u->reactor_id);
    return Armor[u->armor_id].defense_value * w;
}

int set_move_to(int veh_id, int x, int y) {
    VEH* veh = &Vehicles[veh_id];
    debug("set_move_to %2d %2d -> %2d %2d %s\n", veh->x, veh->y, x, y, veh->name());
    veh->waypoint_1_x = x;
    veh->waypoint_1_y = y;
    veh->order = ORDER_MOVE_TO;
    veh->status_icon = 'G';
    if (veh->is_former()) {
        veh->terraforming_turns = 0;
    }
    mapnodes.erase({x, y, NODE_PATROL});
    mapnodes.erase({x, y, NODE_COMBAT_PATROL});
    if (veh->x == x && veh->y == y) {
        return mod_veh_skip(veh_id);
    }
    pm_target[x][y]++;
    return VEH_SYNC;
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
    veh->order = ORDER_ROAD_TO;
    veh->status_icon = 'R';
    return VEH_SYNC;
}

int set_action(int veh_id, int act, char icon) {
    VEH* veh = &Vehicles[veh_id];
    if (act == ORDER_THERMAL_BOREHOLE) {
        mapnodes.insert({veh->x, veh->y, NODE_BOREHOLE});
    } else if (act == ORDER_TERRAFORM_UP) {
        mapnodes.insert({veh->x, veh->y, NODE_RAISE_LAND});
    } else if (act == ORDER_SENSOR_ARRAY) {
        mapnodes.insert({veh->x, veh->y, NODE_SENSOR_ARRAY});
    }
    veh->order = act;
    veh->status_icon = icon;
    veh->state &= ~VSTATE_UNK_10000;
    return VEH_SYNC;
}

int set_convoy(int veh_id, ResType res) {
    VEH* veh = &Vehicles[veh_id];
    mapnodes.insert({veh->x, veh->y, NODE_CONVOY});
    veh->order_auto_type = res-1;
    veh->order = ORDER_CONVOY;
    veh->status_icon = 'C';
    return veh_skip(veh_id);
}

int set_board_to(int veh_id, int trans_veh_id) {
    VEH* veh = &Vehicles[veh_id];
    VEH* v2 = &Vehicles[trans_veh_id];
    assert(veh_id != trans_veh_id);
    assert(veh->x == v2->x && veh->y == v2->y);
    assert(veh_cargo(trans_veh_id) > 0);
    veh->order = ORDER_SENTRY_BOARD;
    veh->waypoint_1_x = trans_veh_id;
    veh->waypoint_1_y = 0;
    veh->status_icon = 'L';
    debug("set_board_to %2d %2d %s -> %s\n", veh->x, veh->y, veh->name(), v2->name());
    return VEH_SYNC;
}

int cargo_loaded(int veh_id) {
    int n=0;
    for (int i=0; i < *VehCount; i++) {
        VEH* veh = &Vehicles[i];
        if (veh->order == ORDER_SENTRY_BOARD && veh->waypoint_1_x == veh_id) {
            n++;
        }
    }
    return n;
}

