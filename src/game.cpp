
#include "game.h"

#include "wtp_mod.h"

static uint32_t custom_game_rules = 0;
static uint32_t custom_more_rules = 0;


bool un_charter() {
    return ProposalPassCount[PROP_REPEAL_UN_CHARTER]
        <= ProposalPassCount[PROP_REINSTATE_UN_CHARTER];
}

bool global_trade_pact() {
    return ProposalPassCount[PROP_REPEAL_GLOBAL_TRADE_PACT]
        < ProposalPassCount[PROP_GLOBAL_TRADE_PACT];
}

bool victory_done() {
    return (*GameState & STATE_GAME_DONE);
}

bool full_game_turn() {
    return !(*GameState & STATE_GAME_DONE) || (*GameState & STATE_FINAL_SCORE_DONE);
}

bool voice_of_planet() {
    // Even destroyed Voice of Planet allows building Ascent to Transcendence
    return project_base(FAC_VOICE_OF_PLANET) != SP_Unbuilt;
}

bool valid_player(int faction_id) {
    return faction_id > 0 && faction_id < MaxPlayerNum;
}

bool valid_triad(int triad) {
    return (triad == TRIAD_LAND || triad == TRIAD_SEA || triad == TRIAD_AIR);
}

char* label_get(size_t index) {
    return (TextLabels->labels)[index];
}

char* __cdecl parse_set(int faction_id) {
    *gender_default = MFactions[faction_id].noun_gender;
    *plurality_default = MFactions[faction_id].is_noun_plural;
    return MFactions[faction_id].noun_faction;
}

int __cdecl parse_num(size_t index, int value) {
    if (index > 9) {
        return 3;
    }
    ParseNumTable[index] = value;
    return 0;
}

/*
This function is the preferred, more generic version to be used instead of parse_say.
*/
int __cdecl parse_says(size_t index, const char* src, int gender, int plural) {
   if (!src || index > 9) {
       return 3;
   }
   if (gender < 0) {
       gender = *gender_default;
   }
   ParseStrGender[index] = gender;
   if (plural < 0) {
       plural = *plurality_default;
   }
   ParseStrPlurality[index] = plural;
   strcpy_n(ParseStrBuffer[index].str, StrBufLen, src);
   return 0;
}

int __cdecl game_year(int turn) {
    return Rules->normal_start_year + turn;
}

int __cdecl in_box(int x, int y, RECT* rc) {
    return x >= rc->left && x < rc->right
        && y >= rc->top && y < rc->bottom;
}

/*
Calculate the offset and bitmask for the specified input.
*/
void __cdecl bitmask(uint32_t input, uint32_t* offset, uint32_t* mask) {
    *offset = input / 8;
    *mask = 1 << (input & 7);
}

void init_world_config() {
    ThinkerVars->game_time_spent = 0;
    /*
    Adjust Special Scenario Rules if any are selected from the mod menu.
    This also overrides settings for any scenarions unless all custom options are left empty.
    */
    if ((custom_game_rules || custom_more_rules) && !*MultiplayerActive) {
        *GameRules = (*GameRules & GAME_RULES_MASK) | custom_game_rules;
        *GameMoreRules = (*GameMoreRules & GAME_MRULES_MASK) | custom_more_rules;
        if (*GameRules & RULES_SCN_NO_NATIVE_LIFE) {
            for (int i = *VehCount - 1; i >= 0; --i) {
                if (!Vehs[i].faction_id && Vehs[i].unit_id == BSC_FUNGAL_TOWER) {
                    kill(i);
                }
            }
        }
    }
}

void show_rules_menu() {
    *GameRules = (*GameRules & GAME_RULES_MASK) | custom_game_rules;
    *GameMoreRules = (*GameMoreRules & GAME_MRULES_MASK) | custom_more_rules;
    Console_editor_scen_rules(MapWin);
    custom_game_rules = *GameRules & ~GAME_RULES_MASK;
    custom_more_rules = *GameMoreRules & ~GAME_MRULES_MASK;
}

void init_save_game(int faction_id) {
    MFaction* m = &MFactions[faction_id];
    if (!faction_id) {
        return;
    }
    if (!*CurrentTurn) {
        memset(&m->thinker_probe_lost, 0, 20);
    }
    m->thinker_unused[0] = 0;
    m->thinker_unused[1] = 0;
    m->thinker_unused[2] = 0;
    /*
    COMMFREQ = Gets an extra comm frequency (another faction to talk to) at beginning of game.
    */
    if (m->rule_flags & RFLAG_COMMFREQ && *CurrentTurn == 1) {
        std::set<int> plrs;
        for (int i = 1; i < MaxPlayerNum; i++) {
            if (faction_id != i && is_alive(i)) {
                plrs.insert({i});
            }
        }
        if (plrs.size()) {
            int plr_id = pick_random(plrs);
            net_set_treaty(faction_id, plr_id, DIPLO_COMMLINK, 1, 0);
        }
    }
    /*
    Remove invalid prototypes from the savegame.
    This also attempts to repair invalid vehicle stacks to prevent game crashes.
    Stack iterators should never contain infinite loops or vehicles from multiple tiles.
    */
    for (int i = 0; i < MaxProtoFactionNum; i++) {
        int unit_id = faction_id*MaxProtoFactionNum + i;
        UNIT* u = &Units[unit_id];
        if (strlen(u->name) >= MaxProtoNameLen
        || u->chassis_id < CHS_INFANTRY
        || u->chassis_id > CHS_MISSILE) {
            for (int j = *VehCount-1; j >= 0; j--) {
                if (Vehs[j].unit_id == unit_id) {
                    veh_kill(j);
                }
            }
            for (int j = 0; j < *BaseCount; j++) {
                if (Bases[j].queue_items[0] == unit_id) {
                    Bases[j].queue_items[0] = -FAC_STOCKPILE_ENERGY;
                }
            }
            memset(u, 0, sizeof(UNIT));
        }
        if (u->is_active()) {
            if (u->weapon_mode() == WMODE_SUPPLY && u->plan != PLAN_SUPPLY) {
                print_unit(unit_id);
                u->plan = PLAN_SUPPLY;
            } else if (u->weapon_mode() == WMODE_COLONY && u->plan != PLAN_COLONY) {
                print_unit(unit_id);
                u->plan = PLAN_COLONY;
            } else if (u->weapon_mode() == WMODE_TERRAFORM && u->plan != PLAN_TERRAFORM) {
                print_unit(unit_id);
                u->plan = PLAN_TERRAFORM;
            } else if (u->weapon_mode() == WMODE_PROBE && u->plan != PLAN_PROBE) {
                print_unit(unit_id);
                u->plan = PLAN_PROBE;
            }
        }
    }
    for (int i = 0; i < *VehCount; i++) {
        VEH* veh = &Vehs[i];
        int prev_id = veh->prev_veh_id_stack;
        int next_id = veh->next_veh_id_stack;
        bool adjust = false;

        if (prev_id == i || (prev_id >= 0 && (Vehs[prev_id].x != veh->x || Vehs[prev_id].y != veh->y))) {
            print_veh_stack(veh->x, veh->y);
            veh->prev_veh_id_stack = -1;
            adjust = true;
        }
        if (next_id == i || (next_id >= 0 && (Vehs[next_id].x != veh->x || Vehs[next_id].y != veh->y))) {
            print_veh_stack(veh->x, veh->y);
            veh->next_veh_id_stack = -1;
            adjust = true;
        }
        if (adjust) {
            stack_fix(i);
        }
    }
}

void __cdecl mod_turn_upkeep() {
    debug("turn_upkeep %d bases: %d vehs: %d\n", (*CurrentTurn)+1, *BaseCount, *VehCount);
    snprintf(ThinkerVars->build_date, 12, MOD_DATE);
    if (*CurrentTurn == 0) {
        init_world_config();
    }
    if (DEBUG) {
        if (conf.debug_mode) {
            *GameState |= STATE_DEBUG_MODE;
            *GamePreferences |= PREF_ADV_FAST_BATTLE_RESOLUTION;
        } else {
            *GameState &= ~STATE_DEBUG_MODE;
        }
    }
    // Original game turn upkeep starts here
    for (int veh_id = *VehCount - 1; veh_id >= 0; --veh_id) {
        VEH* veh = &Vehs[veh_id];
        if (veh->triad() == TRIAD_AIR && veh->range() && veh_speed(veh_id, 0) - veh->moves_spent > 0) {
            MAP* sq = mapsq(veh->x, veh->y);
            if (sq && sq->base_who() < 0 && !(sq->items & BIT_AIRBASE)
            && !mod_stack_check(veh_id, 6, ABL_CARRIER, -1, -1)) {
                // Fix bug here that prevented the turn from advancing
                // while any needlejet in flight has moves left.
                if (veh->movement_turns < veh->range() - 1) {
                    ++veh->movement_turns;
                }
                veh->state |= VSTATE_HAS_MOVED;
                int dmg_val;
                if (veh->range() != 1 || veh->chassis_type() != CHS_COPTER) {
                    dmg_val = veh->cur_hitpoints();
                } else {
                    dmg_val = 3 * veh->reactor_type();
                }
                veh->damage_taken = clamp(veh->damage_taken + dmg_val, 0, 255);
                if (veh->max_hitpoints() - veh->damage_taken <= 0) {
                    kill(veh_id);
                }
            }
        }
    }
    *dword_90DB84 = 0; // action_terraform
    rankings(1);
    *CurrentMissionYear = game_year(++(*CurrentTurn));
    MapWin_main_caption();

    if (*ClimateFutureChange) {
        int val = *ClimateValueA + *ClimateValueC;
        *ClimateValueC += *ClimateValueA;
        if (*ClimateValueC >= *ClimateValueB) {
            *ClimateValueC = val - *ClimateValueB;
            int alt_val = (*ClimateFutureChange > 0 ? 1 : ((*ClimateFutureChange >= 0) - 1));
            *ClimateFutureChange -= alt_val;
            *MapSeaLevel += alt_val;
            world_climate();
            draw_map(1);
            if (alt_val > 0) {
                popp(ScriptFile, "SEARISING", 0, "searis_sm.pcx", 0);
            } else {
                popp(ScriptFile, "SEAFALLING", 0, "seafal_sm.pcx", 0);
            }
        }
    }
    alien_fauna();
    do_fungal_towers();

    if ((*CurrentTurn == 4 && !game_randv(4))
    || (*CurrentTurn == 5 && !game_randv(3))
    || (*CurrentTurn == 6 && !game_randv(2))
    || *CurrentTurn >= 7) {
        bool aliens_arrive = false;
        for (int i = 1; i < MaxPlayerNum; i++) {
            Faction& plr = Factions[i];
            if (is_alien(i) || (!plr.base_count && !_strcmpi(MFactions[i].filename, "FUNGBOY"))) {
                int flags = plr.player_flags | PFLAG_MULTI_TECH_ACHIEVED;
                if (flags == -1 && plr.diff_level == -1 && !is_human(i) && !is_alive(i)) {
                    if (is_alien(i)) {
                        aliens_arrive = true;
                    }
                    set_alive(i, true);
                    mod_setup_player(i, -282, 0);
                }
            }
        }
        if (aliens_arrive) {
            popp(ScriptFile, "ALIENSARRIVE", 0, "al_land_sm.pcx", 0);
            interlude(21, 0, 1, 0);
        }
    }
    int caretake_id = 0;
    for (int i = 1; i < MaxPlayerNum; i++) {
        if (is_alive(i) && !_strcmpi(MFactions[i].filename, "CARETAKE")) { // Added is_alive check
            caretake_id = i;
        }
    }
    if (caretake_id) {
        for (int i = 0; i < *BaseCount; i++) {
            int faction_id = Bases[i].faction_id;
            if (faction_id != caretake_id
            && Bases[i].queue_items[0] == -FAC_ASCENT_TO_TRANSCENDENCE
            && !has_treaty(caretake_id, faction_id, DIPLO_VENDETTA)) {
                if (has_treaty(faction_id, caretake_id, DIPLO_PACT)) {
                    pact_ends(faction_id, caretake_id);
                }
                treaty_on(caretake_id, faction_id, DIPLO_VENDETTA|DIPLO_COMMLINK);
                set_treaty(caretake_id, faction_id, DIPLO_UNK_40, 1);
                Factions[faction_id].diplo_spoke[caretake_id] = *CurrentTurn;
                if (faction_id == *CurrentPlayerFaction) {
                    X_pops4("NOTRANSCEND", 0x100000, FactionPortraits[caretake_id], (int)sub_5398E0);
                }
            }
        }
    }
    for (int i = 1; i < MaxPlayerNum; i++) {
        for (int j = 1; j < MaxPlayerNum; j++) {
            if (i != j && is_alien(i) && is_alien(j)) {
                set_treaty(i, j, DIPLO_UNK_80000000|DIPLO_UNK_40000000|DIPLO_UNK_8000000|\
                    DIPLO_MAJOR_ATROCITY_VICTIM|DIPLO_ATROCITY_VICTIM|DIPLO_UNK_800|\
                    DIPLO_SHALL_BETRAY|DIPLO_WANT_REVENGE|DIPLO_VENDETTA|DIPLO_COMMLINK, 1);
                set_agenda(i, j, AGENDA_PERMANENT|AGENDA_UNK_800|AGENDA_UNK_400|AGENDA_UNK_20|\
                    AGENDA_FIGHT_TO_DEATH|AGENDA_UNK_4|AGENDA_UNK_1, 1);
                Factions[i].diplo_spoke[j] = *CurrentTurn;
            }
        }
    }
    random_events(0);
    if (*CurrentTurn == 75 && has_tech(Facility[FAC_BIOLOGY_LAB].preq_tech, *CurrentPlayerFaction)) {
        interlude(1, 0, 1, 0);
    }
    if (voice_of_planet()) {
        Faction& plr = Factions[*CurrentPlayerFaction];
        MFaction& m_plr = MFactions[*CurrentPlayerFaction];
        if (!(plr.player_flags & PFLAG_UNK_2000)) {
            plr.player_flags |= PFLAG_UNK_2000;
            *plurality_default = 0;
            *gender_default = m_plr.is_leader_female;
            parse_says(0, m_plr.title_leader, -1, -1);
            parse_says(1, m_plr.name_leader, -1, -1);
            popp(ScriptFile, "ASCENT", 0, "asctran_sm.pcx", 0);
        }
    }
    if ((*CurrentTurn & 3) == 1) {
        reset_territory();
    }
    do_all_non_input();
}

/*
Original file headers use TERRAN (savegames) and TERRANMAP (maps without game state).
When unit count exceeds previous limit 2048, this extended version will use a modified
file header for savegames to prevent these files from being opened on the original game.
*/
int __cdecl save_daemon_header(const char* header, FILE* file) {
    if (*VehCount > MaxVehNum && !strcmp(header, "TERRAN")) {
        return header_write("TERRAE", file);
    }
    return header_write(header, file);
}

int __cdecl load_daemon_strcmp(const char* value, const char* header) {
    if (conf.modify_unit_limit && !strcmp(header, "TERRAN")) {
        // Both original and extended version allowed
        return strcmp(value, "TERRAN") && strcmp(value, "TERRAE");
    }
    return strcmp(value, header);
}

int __cdecl mod_load_map_daemon(const char* name) {
    // Another map selected from various dialogs
    reset_state();
    return load_map_daemon(name);
}

int __cdecl mod_load_daemon(const char* name, int flag) {
    // Another savegame opened from selection dialog
    reset_state();
    return load_daemon(name, flag);
}

void __cdecl mod_auto_save() {
    if ((!*PbemActive || *MultiplayerActive)
    && (!(*GameRules & RULES_IRONMAN) || *GameState & STATE_SCENARIO_EDITOR)) {
        if (conf.autosave_interval > 0 && !(*CurrentTurn % conf.autosave_interval)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "saves/auto/Autosave_%d.sav", game_year(*CurrentTurn));
            save_daemon(buf);
        }
    }
}

/*
Store base related events for the endgame replay screen.
0 = create base, 1 = change base owner, 2 = kill base.
*/
int __cdecl mod_replay_base(int event, int x, int y, int faction_id) {
    assert(mapsq(x, y) && event >= 0 && event <= 2);
    debug("replay_base %d %d %d %d %d %d\n",
        *ReplayEventSize, *CurrentTurn, event, faction_id, x, y);
    if (*ReplayEventSize >= 0 && *ReplayEventSize < 8192) {
        ReplayEvent* p = &ReplayEvents[*ReplayEventSize/8];
        p->event = event;
        p->faction_id = faction_id;
        p->turn = *CurrentTurn;
        p->x = x;
        p->y = y;
        *ReplayEventSize += 8;
    }
    return *ReplayEventSize;
}

void __cdecl mod_faction_upkeep(int faction_id) {
    Faction* f = &Factions[faction_id];
    MFaction* m = &MFactions[faction_id];
    debug("faction_upkeep %d %d\n", *CurrentTurn, faction_id);

    init_save_game(faction_id);
    plans_upkeep(faction_id);
    reset_netmsg_status();
    *ControlUpkeepA = 1;
    social_upkeep(faction_id);
    do_all_non_input();
    mod_repair_phase(faction_id);
    do_all_non_input();
    mod_production_phase(faction_id);
    do_all_non_input();
    if (full_game_turn()) {
        allocate_energy(faction_id);
        do_all_non_input();
        enemy_diplomacy(faction_id);
        do_all_non_input();
        enemy_strategy(faction_id);
        do_all_non_input();
        /*
        Thinker-specific AI planning routines.
        Note that move_upkeep is only updated after all the production is done,
        so that the movement code can utilize up-to-date priority maps.
        This means we cannot use move_upkeep maps or variables in production phase.
        */
        mod_social_ai(faction_id, -1, -1, -1, -1, 0);
                
        // [WTP]
        // social AI
        wtp_mod_social_ai(faction_id);
        
        probe_upkeep(faction_id);
        move_upkeep(faction_id, (is_human(faction_id) ? UM_Player : UM_Full));
        do_all_non_input();

        if (!is_human(faction_id) && *GameRules & RULES_VICTORY_ECONOMIC
        && has_tech(Rules->tech_preq_economic_victory, faction_id)) {
            int cost = corner_market(faction_id);
            if (!victory_done() && f->corner_market_active <= 0 && f->energy_credits > cost) {
                f->corner_market_turn = *CurrentTurn + Rules->turns_corner_global_energy_market;
                f->corner_market_active = cost;
                f->energy_credits -= cost;

                *gender_default = m->noun_gender;
                *plurality_default = 0;
                parse_says(0, m->title_leader, -1, -1);
                parse_says(1, m->name_leader, -1, -1);
                *plurality_default = m->is_noun_plural;
                parse_says(2, m->noun_faction, -1, -1);
                parse_says(3, m->adj_name_faction, -1, -1);
                ParseNumTable[0] = game_year(f->corner_market_turn);
                popp(ScriptFile, "CORNERWARNING", 0, "econwin_sm.pcx", 0);
            }
        }
    }
    for (int i = 0; i < *BaseCount; i++) {
        BASE* base = &Bases[i];
        if (base->faction_id == faction_id) {
            base->state_flags &= ~(BSTATE_UNK_1 | BSTATE_HURRY_PRODUCTION);
        }
    }
    f->energy_credits -= f->hurry_cost_total;
    f->hurry_cost_total = 0;
    if (f->energy_credits < 0) {
        f->energy_credits = 0;
    }
    if (!f->base_count && !has_active_veh(faction_id, PLAN_COLONY)) {
        mod_eliminate_player(faction_id, 0);
    }
    if (f->base_count && f->tech_research_id < 0 && *NetUpkeepState != 1
    && !(*GameRules & RULES_SCN_NO_TECH_ADVANCES)) {
        f->tech_research_id = mod_tech_selection(faction_id);
    }
    *ControlUpkeepA = 0;
    Paths->xDst = -1;
    Paths->yDst = -1;

    if (full_game_turn()) {
        if (faction_id == MapWin->cOwner
        && !(*GameState & (STATE_COUNCIL_HAS_CONVENED | STATE_DISPLAYED_COUNCIL_AVAIL_MSG))
        && can_call_council(faction_id, 0) && !(*GameState & STATE_GAME_DONE)) {
            *GameState |= STATE_DISPLAYED_COUNCIL_AVAIL_MSG;
            popp(ScriptFile, "COUNCILOPEN", 0, "council_sm.pcx", 0);
        }
        if (!is_human(faction_id)) {
            call_council(faction_id);
        }
    }
    if (!*MultiplayerActive && *GamePreferences & PREF_BSC_AUTOSAVE_EACH_TURN
    && faction_id == MapWin->cOwner) {
        auto_save();
    }
    flushlog();
}

void __cdecl mod_repair_phase(int faction_id) {
    Faction* f = &Factions[faction_id];
    f->ODP_deployed = 0;
    Points tiles;

    for (int veh_id = 0; veh_id < *VehCount; veh_id++) {
        VEH* veh = &Vehs[veh_id];
        if (veh->faction_id != faction_id) {
            continue;
        }
        MAP* sq = mapsq(veh->x, veh->y);
        const bool at_base = sq && sq->is_base();
        const int triad = veh->triad();
        veh->iter_count = 0;
        veh->moves_spent = 0;
        veh->flags &= ~VFLAG_UNK_1000;
        veh->state &= ~(VSTATE_WORKING|VSTATE_UNK_2000|VSTATE_UNK_2);

        if (conf.activate_skipped_units) {
            veh->flags &= ~VFLAG_FULL_MOVE_SKIPPED;
        }
        if (sq && !at_base) {
            if (veh->faction_id == MapWin->cOwner || veh->is_visible(MapWin->cOwner)) {
                tiles.insert({veh->x, veh->y});
            }
        }
        if ( !((*CurrentTurn + veh_id) & 3) ) {
            veh->state &= ~VSTATE_UNK_800;
            if (veh->flags & VFLAG_UNK_2) {
                veh->flags &= ~VFLAG_UNK_2;
            } else {
                veh->flags &= ~VFLAG_UNK_1;
            }
        }
        if (veh->order == ORDER_SENTRY_BOARD || veh->order == ORDER_HOLD) {
            if (veh->waypoint_y[0]) {
                if (!--veh->waypoint_y[0]) {
                    veh->order = ORDER_NONE;
                }
            }
        }
        if (veh->state & VSTATE_UNK_8) {
            if (at_base
            || (triad == TRIAD_LAND && sq->is_bunker())
            || (triad == TRIAD_AIR && sq->is_airbase())) {
                veh->state &= ~VSTATE_UNK_8;
            }
        }
        if (veh->unit_id == BSC_FUNGAL_TOWER) {
            veh->faction_id = 0;
        }
        if (!sq || !veh->damage_taken
        || (veh->is_battle_ogre() && conf.repair_battle_ogre <= 0)
        || !(veh->unit_id == BSC_FUNGAL_TOWER || !(veh->state & VSTATE_HAS_MOVED))) {
            continue;
        }
        int value = conf.repair_minimal;
        int base_id = base_at(veh->x, veh->y);

        if (veh->unit_id != BSC_FUNGAL_TOWER) {
            if (veh->is_native_unit() && sq->is_fungus()) {
                assert(sq->alt_level() >= ALT_OCEAN_SHELF);
                value = conf.repair_fungus;
            }
            if (conf.repair_friendly > 0
            && mod_whose_territory(faction_id, veh->x, veh->y, 0, 0) == faction_id) {
                value += conf.repair_friendly;
            }
            if (triad == TRIAD_AIR && sq->items & BIT_AIRBASE) {
                value += conf.repair_airbase;
            }
            if (triad == TRIAD_LAND && sq->items & BIT_BUNKER) {
                value += conf.repair_bunker;
            }
        }
        if (base_id >= 0 && !Bases[base_id].drone_riots_active()) {
            value += conf.repair_base;
            // Fix: consider secret projects built by the base owner instead of the veh owner
            if (!veh->is_native_unit()) {
                if ((triad == TRIAD_LAND && has_facility(FAC_COMMAND_CENTER, base_id))
                || (triad == TRIAD_SEA && has_facility(FAC_NAVAL_YARD, base_id))
                || (triad == TRIAD_AIR && has_facility(FAC_AEROSPACE_COMPLEX, base_id))) {
                    value += conf.repair_base_facility;
                }
            } else if (breed_mod(base_id, faction_id)) {
                value += conf.repair_base_native;
            }
        }
        bool repair_abl = false;
        if (triad == TRIAD_LAND) {
            if ((!veh_cargo(veh_id) && veh->order == ORDER_SENTRY_BOARD) || is_ocean(sq)) {
                int iter_id = veh_top(veh_id);
                while (iter_id >= 0) {
                    if (iter_id != veh_id && has_abil(Vehs[iter_id].unit_id, ABL_REPAIR)) {
                        repair_abl = true;
                    }
                    iter_id = Vehs[iter_id].next_veh_id_stack;
                }
                if (repair_abl) {
                    value *= 2;
                }
            }
        }
        if (has_project(FAC_NANO_FACTORY, faction_id)) {
            value += conf.repair_nano_factory;
        }
        if (veh->is_battle_ogre()) {
             value = min(value, conf.repair_battle_ogre);
        }
        int repair_limit = 0;
        int repair_value = value * veh->reactor_type();

        if (faction_id && !repair_abl && base_id < 0 && !has_project(FAC_NANO_FACTORY, faction_id)) {
            repair_limit = 2 * veh->reactor_type();
            if (sq->is_fungus()) {
                assert(sq->alt_level() >= ALT_OCEAN_SHELF);
                if (veh->is_native_unit()) {
                    repair_limit = 0;
                }
                if (has_project(FAC_XENOEMPATHY_DOME, faction_id)) {
                    repair_limit = 0;
                    repair_value += veh->reactor_type();
                }
            }
        }
        int damage = clamp(veh->damage_taken - repair_value, repair_limit, 255);
        if (damage < veh->damage_taken) {
            veh->damage_taken = damage;
            if (damage <= repair_limit && is_human(faction_id)
            && veh->order == ORDER_SENTRY_BOARD
            && (triad != TRIAD_LAND || !is_ocean(sq))) {
                veh->order = ORDER_NONE;
            }
        }
    }
    for (auto& p : tiles) {
        draw_tile(p.x, p.y, -1);
    }
    do_all_draws();
}

void __cdecl mod_production_phase(int faction_id) {
    Faction* f = &Factions[faction_id];
    MFaction* m = &MFactions[faction_id];
    debug("production_phase %d %d\n", *CurrentTurn, faction_id);
    f->best_mineral_output = 0;
    f->energy_surplus_total = 0;
    f->facility_maint_total = 0;
    f->turn_commerce_income = 0;
    mod_tech_effects(faction_id);

    for (int i = 1; i < MaxPlayerNum; i++) {
        if (faction_id != i && is_alive(faction_id) && is_alive(i)) {
            assert(f->loan_balance[i] >= 0);
            if (f->loan_balance[i] && !Factions[i].sanction_turns) {
                assert(f->loan_payment[i] > 0);
                if (at_war(faction_id, i)) {
                    f->loan_balance[i] += f->loan_payment[i];
                } else {
                    int payment = clamp(f->energy_credits, 0, f->loan_payment[i]);
                    Factions[i].energy_credits += payment;
                    f->energy_credits -= payment;
                    f->loan_balance[i] -= payment;
                    if (payment < f->loan_payment[i]) {
                        f->loan_balance[i] += f->loan_payment[i] - payment;
                    }
                }
            }
        }
    }

    f->player_flags &= ~PFLAG_SELF_AWARE_COLONY_LOST_MAINT;
    *dword_93A958 = f->energy_credits;
    /*
    Reset all fields listed below.
    int32_t unk_40[8];
    int32_t unk_41[40];
    int32_t unk_42[32];
    int32_t unk_43[9];
    int32_t unk_45;
    int32_t unk_46;
    int32_t unk_47;
    */
    assert((int)&f->unk_40 + 0x170 == (int)&f->nutrient_surplus_total);
    memset(&f->unk_40, 0, 0x170);

    f->nutrient_surplus_total = 0;
    f->labs_total = 0;

    if (*BaseCount > 0) {
        for (int base_id = 0; base_id < *BaseCount; base_id++) {
            if (Bases[base_id].faction_id == faction_id) {
                if (mod_base_upkeep(base_id)) {
                    base_id--; // Base was removed for some reason
                }
                do_all_non_input();
            }
        }
    }
    /*
    Apply the original AI facility maintenance discounts
    These modifiers are not displayed in budget screens, they are just applied here
    */
    if (!is_human(faction_id) && *DiffLevel >= DIFF_THINKER) {
        if (f->facility_maint_total) {
            int value;
            if (*DiffLevel == DIFF_THINKER) {
                value = f->facility_maint_total / 3;
                f->energy_credits += value;
            } else {
                value = f->facility_maint_total * 2 / 3;
                f->energy_credits += value;
            }
            f->facility_maint_total -= value;
        }
    }
    /*
    INTEREST = Energy reserves interest.
    Non-zero = constant percentage per turn (including negative)
    Zero     = +1/base each turn
    */
    if (m->rule_flags & RFLAG_INTEREST) {
        int rule_interest = m->rule_interest;
        if (rule_interest) {
            f->energy_credits += f->energy_credits * rule_interest / 100;
        } else {
            for (int base_id = 0; base_id < *BaseCount; base_id++) {
                if (Bases[base_id].faction_id == faction_id) {
                    f->energy_credits++;
                    f->energy_surplus_total++;
                }
            }
        }
    }
    f->unk_18 = f->energy_surplus_total;
    f->unk_17 = f->energy_surplus_total + f->turn_commerce_income - f->facility_maint_total;

    if (*CurrentTurn == 1) {
        int techs = m->rule_tech_selected;
        *SkipTechScreenA = 1;
        while (--techs >= 0) {
            tech_advance(faction_id);
        }
        *SkipTechScreenA = 0;
    }
    if (full_game_turn()) {
        if (f->sanction_turns) {
            f->sanction_turns--;
            if (!f->sanction_turns && faction_id == MapWin->cOwner) {
                if (!is_alien(faction_id)) {
                    popp(ScriptFile, "SANCTIONSEND", 0, "council_sm.pcx", 0);
                } else {
                    popp(ScriptFile, "SANCTIONSENDALIEN", 0, "Alopdir.pcx", 0);
                }
            }
        }
    }
}

uint32_t offset_next(int32_t faction, uint32_t position, uint32_t amount) {
    if (!position) {
        return 0;
    }
    uint32_t loop = 0;
    uint32_t offset = ((*MapRandomSeed + faction) & 0xFE) | 1;
    do {
        if (offset & 1) {
            offset ^= 0x170;
        }
        offset >>= 1;
    } while (offset >= amount || ++loop != position);
    return offset;
}

/*
Generate a base name. Uses additional base names from basenames folder.
When faction base names are used, creates additional variations from basenames/generic.txt.
First land/sea base always uses the first available name from land/sea names list.
Vanilla name_base chooses sea base names in a sequential non-random order (this version is random).
*/
void __cdecl mod_name_base(int faction_id, char* name, bool save_offset, bool sea_base) {
    if (!conf.new_base_names) {
        return name_base(faction_id, name, save_offset, sea_base);
    }
    Faction& f = Factions[faction_id];
    uint32_t offset = f.base_name_offset;
    uint32_t offset_sea = f.base_sea_name_offset;
    const int buf_size = 256;
    char file_name_1[buf_size];
    char file_name_2[buf_size];
    set_str_t all_names;
    vec_str_t sea_names;
    vec_str_t land_names;
    snprintf(file_name_1, buf_size, "%s.txt", MFactions[faction_id].filename);
    snprintf(file_name_2, buf_size, "basenames\\%s.txt", MFactions[faction_id].filename);
    strlwr(file_name_1);
    strlwr(file_name_2);

    for (int i = 0; i < *BaseCount; i++) {
        if (Bases[i].faction_id == faction_id) {
            all_names.insert(Bases[i].name);
        }
    }
    if (sea_base) {
        reader_path(sea_names, file_name_1, "#WATERBASES", MaxBaseNameLen);
        reader_path(sea_names, file_name_2, "#WATERBASES", MaxBaseNameLen);

        if (sea_names.size() > 0 && offset_sea < sea_names.size()) {
            for (uint32_t i = 0; i < sea_names.size(); i++) {
                uint32_t seed = offset_next(faction_id, offset_sea + i, sea_names.size());
                if (seed < sea_names.size()) {
                    vec_str_t::const_iterator it(sea_names.begin());
                    std::advance(it, seed);
                    if (!has_item(all_names, it->c_str())) {
                        strcpy_n(name, MaxBaseNameLen, it->c_str());
                        if (save_offset) {
                            f.base_sea_name_offset++;
                        }
                        return;
                    }
                }
            }
        }
    }
    land_names.clear();
    reader_path(land_names, file_name_1, "#BASES", MaxBaseNameLen);
    reader_path(land_names, file_name_2, "#BASES", MaxBaseNameLen);

    if (save_offset) {
        f.base_name_offset++;
    }
    if (land_names.size() > 0 && offset < land_names.size()) {
        for (uint32_t i = 0; i < land_names.size(); i++) {
            uint32_t seed = offset_next(faction_id, offset + i, land_names.size());
            if (seed < land_names.size()) {
                vec_str_t::const_iterator it(land_names.begin());
                std::advance(it, seed);
                if (!has_item(all_names, it->c_str())) {
                    strcpy_n(name, MaxBaseNameLen, it->c_str());
                    return;
                }
            }
        }
    }
    for (int i = 0; i < *BaseCount; i++) {
        all_names.insert(Bases[i].name);
    }
    uint32_t x = 0;
    uint32_t a = 0;
    uint32_t b = 0;

    land_names.clear();
    sea_names.clear();
    reader_path(land_names, "basenames\\generic.txt", "#BASESA", MaxBaseNameLen);
    if (land_names.size() > 0) {
        reader_path(sea_names, "basenames\\generic.txt", "#BASESB", MaxBaseNameLen);
    } else {
        reader_path(land_names, "basename.txt", "#GENERIC", MaxBaseNameLen);
    }

    for (int i = 0; i < 2*MaxBaseNum && land_names.size() > 0; i++) {
        x = pair_hash(faction_id + MaxPlayerNum*f.base_name_offset, *MapRandomSeed + i);
        a = ((x & 0xffff) * land_names.size()) >> 16;
        name[0] = '\0';

        if (sea_names.size() > 0) {
            b = ((x >> 16) * sea_names.size()) >> 16;
            snprintf(name, MaxBaseNameLen, "%s %s", land_names[a].c_str(), sea_names[b].c_str());
        } else {
            snprintf(name, MaxBaseNameLen, "%s", land_names[a].c_str());
        }
        if (strlen(name) >= 2 && !all_names.count(name)) {
            return;
        }
    }
    for (int i = *BaseCount; i <= 2*MaxBaseNum; i++) {
        name[0] = '\0';
        snprintf(name, MaxBaseNameLen, "Sector %d", i);
        if (!all_names.count(name)) {
            return;
        }
    }
}

/*
Game engine uses these defaults for ambient music tracks.
This patches load_music comparison function such that it maps
the user config to any available music tracks.
If none of the config values match current faction, aset1.amb is used.

DEFAULT, aset1.amb
GAIANS, gset1.amb
HIVE, mset1.amb
UNIV, uset2.amb
MORGAN, mset1.amb
SPARTANS, sset1.amb
BELIEVE, bset1.amb
PEACE, gset1.amb
*/

int __cdecl load_music_strcmpi(const char* active, const char* label)
{
    bool fallback = true;
    char lookup[StrBufLen] = {};

    for (const auto& pair : musiclabels) {
        if (!strcmpi(pair.first.c_str(), active)) {
            if (!strcmpi(pair.second.c_str(), "gset1.amb")) {
                strncpy(lookup, "gaians", StrBufLen);
            } else if (!strcmpi(pair.second.c_str(), "mset1.amb")) {
                strncpy(lookup, "hive", StrBufLen);
            } else if (!strcmpi(pair.second.c_str(), "uset2.amb")) {
                strncpy(lookup, "univ", StrBufLen);
            } else if (!strcmpi(pair.second.c_str(), "sset1.amb")) {
                strncpy(lookup, "spartans", StrBufLen);
            } else if (!strcmpi(pair.second.c_str(), "bset1.amb")) {
                strncpy(lookup, "believe", StrBufLen);
            } else {
                break; // aset1.amb or incorrect filename is used
            }
            fallback = false;
            break;
        }
    }
    debug("load_music %d %s %s %s\n", fallback, active, label, lookup);
    flushlog();
    if (fallback) {
        return 1; // Skip all other choices and select original fallback default
    }
    return strcmpi(label, lookup);
}


