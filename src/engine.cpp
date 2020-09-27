
#include "engine.h"
#include "game.h"
#include "tech.h"
#include "move.h"
#include "wtp.h"
#include "ai.h"
#include "aiHurry.h"

const char* ScriptTxtID = "SCRIPT";

BASE** current_base_ptr = (BASE**)0x90EA30;
int* current_base_id = (int*)0x689370;
int* game_settings = (int*)0x9A6490;
int* game_state = (int*)0x9A64C0;
int* game_rules = (int*)0x9A649C;
int* diff_level = (int*)0x9A64C4;
int* smacx_enabled = (int*)0x9A6488;
int* human_players = (int*)0x9A64E8;
int* current_turn = (int*)0x9A64D4;
int* active_faction = (int*)0x9A6820;
int* total_num_bases = (int*)0x9A64CC;
int* total_num_vehicles = (int*)0x9A64C8;
int* map_random_seed = (int*)0x949878;
int* map_toggle_flat = (int*)0x94988C;
int* map_area_tiles = (int*)0x949884;
int* map_area_sq_root = (int*)0x949888;
int* map_axis_x = (int*)0x949870;
int* map_axis_y = (int*)0x949874;
int* map_half_x = (int*)0x68FAF0;
int* climate_future_change = (int*)0x9A67D8;
int* un_charter_repeals = (int*)0x9A6638;
int* un_charter_reinstates = (int*)0x9A663C;
int* gender_default = (int*)0x9BBFEC;
int* plurality_default = (int*)0x9BBFF0;
int* current_player_faction = (int*)0x939284;
int* multiplayer_active = (int*)0x93F660;
int* pbem_active = (int*)0x93A95C;
int* sunspot_duration = (int*)0x9A6800;
int* diplo_active_faction = (int*)0x93F7CC;
int* diplo_current_friction = (int*)0x93FA74;
int* diplo_opponent_faction = (int*)0x8A4164;
char *g_strTEMP = (char *)0x009B86A0;
int *current_base_growth_rate = (int *)0x0090E918;

fp_1int* social_upkeep = (fp_1int*)0x5B44D0;
fp_1int* repair_phase = (fp_1int*)0x526030;
fp_1int* production_phase = (fp_1int*)0x526E70;
fp_1int* allocate_energy = (fp_1int*)0x5267B0;
fp_1int* enemy_diplomacy = (fp_1int*)0x55F930;
fp_1int* enemy_strategy = (fp_1int*)0x561080;
fp_1int* corner_market = (fp_1int*)0x59EE50;
fp_1int* call_council = (fp_1int*)0x52C880;
fp_2int* eliminate_player = (fp_2int*)0x5B3380;
fp_2int* can_call_council = (fp_2int*)0x52C670;
fp_void* do_all_non_input = (fp_void*)0x5FCB20;
fp_void* auto_save = (fp_void*)0x5ABD20;
fp_2int* parse_num = (fp_2int*)0x625E30;
fp_icii* parse_says = (fp_icii*)0x625EC0;
fp_ccici* popp = (fp_ccici*)0x48C0A0;
fp_3int* capture_base = (fp_3int*)0x50C510;
fp_1int* base_kill = (fp_1int*)0x4E5250;
fp_5int* crop_yield = (fp_5int*)0x4E6E50;
fp_5int* mineral_yield = (fp_5int*)0x4E7310;
fp_5int* energy_yield = (fp_5int*)0x4E7750;
fp_6int* base_draw = (fp_6int*)0x55AF20;
fp_3int* draw_tile = (fp_3int*)0x46AF40;
tc_2int* font_width = (tc_2int*)0x619280;
tc_4int* buffer_box = (tc_4int*)0x5E3203;
tc_3int* buffer_fill3 = (tc_3int*)0x5DFCD0;
tc_5int* buffer_write_l = (tc_5int*)0x5DCEA0;
fp_void_charp_int *say_orders = (fp_void_charp_int *)0x4B43E0;
fp_1void *say_orders2 = (fp_1void *)0x004B4970;

bool victory_done() {
    // TODO: Check for scenario victory conditions
    return *game_state & (STATE_VICTORY_CONQUER | STATE_VICTORY_DIPLOMATIC | STATE_VICTORY_ECONOMIC)
        || has_project(-1, FAC_ASCENT_TO_TRANSCENDENCE);
}

/*
Original Offset: 005C89A0
*/
HOOK_API int game_year(int n) {
    return tx_basic->normal_start_year + n;
}

/*
Original Offset: 00527290
*/
HOOK_API int faction_upkeep(int faction) {
    Faction* f = &tx_factions[faction];
    MetaFaction* m = &tx_metafactions[faction];

    debug("faction_upkeep %d %d:%-25s\n", *current_turn, faction, tx_metafactions[faction].noun_faction);
    fflush(debug_log);

    init_save_game(faction);

    // distributing support across bases
	// affects normal factions only

	if (faction != 0)
	{
		distributeSupport(faction);
	}

    *(int*)0x93A934 = 1;
    social_upkeep(faction);
    do_all_non_input();
    repair_phase(faction);
    do_all_non_input();

	// consider hurrying production in all bases
	// affects AI factions only
	if (faction != 0 && !is_human(faction))
	{
		factionHurryProduction(faction);
	}

    production_phase(faction);

    do_all_non_input();
    if (!(*game_state & STATE_UNK_1) || *game_state & STATE_UNK_8) {
        allocate_energy(faction);
        do_all_non_input();
        enemy_diplomacy(faction);
        do_all_non_input();
        enemy_strategy(faction);
        do_all_non_input();

        /*
        Thinker-specific AI planning routines.
        If the new social_ai is disabled from the config old version gets called instead.
        Player factions always skip the new social_ai function.
        */
        debug("%-25s SE_planet_pending=%d\n", tx_metafactions[faction].noun_faction, tx_factions[faction].SE_planet_pending);
        social_ai(faction, -1, -1, -1, -1, 0);
        debug("%-25s SE_planet_pending=%d\n", tx_metafactions[faction].noun_faction, tx_factions[faction].SE_planet_pending);
        move_upkeep(faction);

		// WTP AI unit movement algorithms
		// affects AI factions only

		if (faction != 0 && !is_human(faction) && conf.ai_useWTPAlgorithms)
		{
			aiStrategy(faction);
		}

        do_all_non_input();

        if (!is_human(faction)) {
            int cost = corner_market(faction);
            if (!victory_done() && f->energy_credits > cost && f->corner_market_active < 1
            && has_tech(faction, tx_basic->tech_preq_economic_victory)
            && *game_rules & RULES_VICTORY_ECONOMIC) {
                f->corner_market_turn = *current_turn + tx_basic->turns_corner_global_energy_market;
                f->corner_market_active = cost;
                f->energy_credits -= cost;

                gender_default = &m->noun_gender;
                plurality_default = 0;
                parse_says(0, m->title_leader, -1, -1);
                parse_says(1, m->name_leader, -1, -1);
                plurality_default = &m->is_noun_plural;
                parse_says(2, m->noun_faction, -1, -1);
                parse_says(3, m->adj_name_faction, -1, -1);
                parse_num(0, game_year(f->corner_market_turn));
                popp(ScriptTxtID, "CORNERWARNING", 0, "econwin_sm.pcx", 0);
            }
        }
    }
    for (int i=0; i<*total_num_bases; i++) {
        BASE* base = &tx_bases[i];
        if (base->faction_id == faction) {
            base->status_flags &= ~(BASE_UNK_1 | BASE_HURRY_PRODUCTION);
        }
    }
    f->energy_credits -= f->energy_cost;
    f->energy_cost = 0;
    if (f->energy_credits < 0) {
        f->energy_credits = 0;
    }
    if (!f->current_num_bases && !f->units_active[BSC_COLONY_POD] && !f->units_active[BSC_SEA_ESCAPE_POD]) {
        eliminate_player(faction, 0);
    }
    *(int*)0x93A934 = 0;
    *(int*)0x945B18 = -1;
    *(int*)0x945B1C = -1;

    if (!(*game_state & STATE_UNK_1) || *game_state & STATE_UNK_8) {
        if (faction == *current_player_faction && !(*game_state & (STATE_UNK_10000 | STATE_UNK_4000))
        && can_call_council(faction, 0) && !(*game_state & STATE_UNK_1)) {
            *game_state |= STATE_UNK_4000;
            popp(ScriptTxtID, "COUNCILOPEN", 0, "council_sm.pcx", 0);
        }
        if (!is_human(faction)) {
            call_council(faction);
        }
    }
    if (!*multiplayer_active && *game_settings & 2 && faction == *current_player_faction) {
        auto_save();
    }
    fflush(debug_log);
    return 0;
}
