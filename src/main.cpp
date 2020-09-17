#include <iostream>
#include <string>
#include <stdio.h>

#include "main.h"
#include "patch.h"
#include "game.h"
#include "move.h"
#include "tech.h"
#include "lib/ini.h"
#include "wtp.h"
#include "aiProduction.h"

std::string MOVE_STATUS[] =
{
	"ORDER_NONE",              //  -
	"ORDER_SENTRY_BOARD",      // (L)
	"ORDER_HOLD",              // (H); Hold (set 1st waypoint (-1, 0)), Hold 10 (-1, 10), On Alert
	"ORDER_CONVOY",            // (O)
	"ORDER_FARM",              // (f)
	"ORDER_SOIL_ENRICHER",     // (f)
	"ORDER_MINE",              // (M)
	"ORDER_SOLAR_COLLECTOR",   // (S)
	"ORDER_PLANT_FOREST",      // (F)
	"ORDER_ROAD",              // (R)
	"ORDER_MAGTUBE",          // (R)
	"ORDER_BUNKER",           // (K)
	"ORDER_AIRBASE",          // (.)
	"ORDER_SENSOR_ARRAY",     // (O)
	"ORDER_REMOVE_FUNGUS",    // (F)
	"ORDER_PLANT_FUNGUS",     // (F)
	"ORDER_CONDENSER",        // (N)
	"ORDER_ECHELON_MIRROR",   // (E)
	"ORDER_THERMAL_BOREHOLE", // (B)
	"ORDER_DRILL_AQUIFIER",   // (Q)
	"ORDER_TERRAFORM_UP",     // (])
	"ORDER_TERRAFORM_DOWN",   // ([)
	"ORDER_TERRAFORM_LEVEL",  // (_)
	"ORDER_PLACE_MONOLITH",   // (?)
	"ORDER_MOVE_TO",          // (G); Move unit to here, Go to Base, Group go to, Patrol
	"ORDER_MOVE",             // (>); Only used in a few places, seems to be buggy mechanic
	"ORDER_EXPLORE",          // (/); not set via shortcut, AI related?
	"ORDER_ROADS_TO",         // (r)
	"ORDER_MAGTUBE_TO",       // (t)
}
;

FILE* debug_log = NULL;
Config conf;
AIPlans plans[8];

Points convoys;
Points boreholes;
Points needferry;

int handler(void* user, const char* section, const char* name, const char* value) {
    Config* cf = (Config*)user;
    char buf[250];
    strcpy_s(buf, sizeof(buf), value);
    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("thinker", "free_formers")) {
        cf->free_formers = atoi(value);
    } else if (MATCH("thinker", "free_colony_pods")) {
        cf->free_colony_pods = atoi(value);
    } else if (MATCH("thinker", "satellites_nutrient")) {
        cf->satellites_nutrient = max(0, atoi(value));
    } else if (MATCH("thinker", "satellites_mineral")) {
        cf->satellites_mineral = max(0, atoi(value));
    } else if (MATCH("thinker", "satellites_energy")) {
        cf->satellites_energy = max(0, atoi(value));
    } else if (MATCH("thinker", "design_units")) {
        cf->design_units = atoi(value);
    } else if (MATCH("thinker", "factions_enabled")) {
        cf->factions_enabled = atoi(value);
    } else if (MATCH("thinker", "social_ai")) {
        cf->social_ai = atoi(value);
    } else if (MATCH("thinker", "tech_balance")) {
        cf->tech_balance = atoi(value);
    } else if (MATCH("thinker", "hurry_items")) {
        cf->hurry_items = atoi(value);
    } else if (MATCH("thinker", "expansion_factor")) {
        cf->expansion_factor = max(0, atoi(value)) / 100.0;
    } else if (MATCH("thinker", "limit_project_start")) {
        cf->limit_project_start = atoi(value);
    } else if (MATCH("thinker", "max_satellites")) {
        cf->max_sat = atoi(value);
    } else if (MATCH("thinker", "smac_only")) {
        cf->smac_only = atoi(value);
    } else if (MATCH("thinker", "faction_placement")) {
        cf->faction_placement = atoi(value);
    } else if (MATCH("thinker", "nutrient_bonus")) {
        cf->nutrient_bonus = atoi(value);
    } else if (MATCH("thinker", "revised_tech_cost")) {
        cf->revised_tech_cost = atoi(value);
    } else if (MATCH("thinker", "auto_relocate_hq")) {
        cf->auto_relocate_hq = atoi(value);
    } else if (MATCH("thinker", "eco_damage_fix")) {
        cf->eco_damage_fix = atoi(value);
    } else if (MATCH("thinker", "collateral_damage_value")) {
        cf->collateral_damage_value = atoi(value);
    } else if (MATCH("thinker", "disable_planetpearls")) {
        cf->disable_planetpearls = atoi(value);
    } else if (MATCH("thinker", "disable_aquatic_bonus_minerals")) {
        cf->disable_aquatic_bonus_minerals = max(0, atoi(value));
    } else if (MATCH("thinker", "cost_factor")) {
        opt_list_parse(tx_cost_ratios, buf, 6, 1);
    } else if (MATCH("thinker", "content_pop_player")) {
        opt_list_parse(conf.content_pop_player, buf, 6, 0);
        conf.patch_content_pop = 1;
    } else if (MATCH("thinker", "content_pop_computer")) {
        opt_list_parse(conf.content_pop_computer, buf, 6, 0);
        conf.patch_content_pop = 1;
    }
    // WtP configuration parameters
    else if (MATCH("wtp", "disable_alien_guaranteed_technologies")) {
        cf->disable_alien_guaranteed_technologies = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_weapon_icon_selection_algorithm")) {
        cf->alternative_weapon_icon_selection_algorithm = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "ignore_reactor_power_in_combat")) {
        cf->ignore_reactor_power_in_combat = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_prototype_cost_formula")) {
        cf->alternative_prototype_cost_formula = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "reactor_cost_factor_0")) {
        cf->reactor_cost_factors[0] = atoi(value);
    }
    else if (MATCH("wtp", "reactor_cost_factor_1")) {
        cf->reactor_cost_factors[1] = atoi(value);
    }
    else if (MATCH("wtp", "reactor_cost_factor_2")) {
        cf->reactor_cost_factors[2] = atoi(value);
    }
    else if (MATCH("wtp", "reactor_cost_factor_3")) {
        cf->reactor_cost_factors[3] = atoi(value);
    }
    else if (MATCH("wtp", "disable_hurry_penalty_threshold")) {
        cf->disable_hurry_penalty_threshold = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_unit_hurry_formula")) {
        cf->alternative_unit_hurry_formula = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_upgrade_cost_formula")) {
        cf->alternative_upgrade_cost_formula = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_base_defensive_structure_bonuses")) {
        cf->alternative_base_defensive_structure_bonuses = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "perimeter_defense_multiplier")) {
        cf->perimeter_defense_multiplier = atoi(value);
    }
    else if (MATCH("wtp", "tachyon_field_bonus")) {
        cf->tachyon_field_bonus = atoi(value);
    }
    else if (MATCH("wtp", "collateral_damage_defender_reactor")) {
        cf->collateral_damage_defender_reactor = (atoi(value) == 0 ? false : true);
    }
// integrated into Thinker
//    else if (MATCH("wtp", "collateral_damage_value")) {
//        cf->collateral_damage_value = atoi(value);
//    }
// integrated into Thinker
//    else if (MATCH("wtp", "disable_aquatic_bonus_minerals")) {
//        cf->disable_aquatic_bonus_minerals = (atoi(value) == 0 ? false : true);
//    }
    else if (MATCH("wtp", "repair_minimal")) {
        cf->repair_minimal = min(10, max(0, atoi(value)));
    }
    else if (MATCH("wtp", "repair_fungus")) {
        cf->repair_fungus = min(10, max(0, atoi(value)));
    }
    else if (MATCH("wtp", "repair_friendly")) {
        cf->repair_friendly = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "repair_airbase")) {
        cf->repair_airbase = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "repair_bunker")) {
        cf->repair_bunker = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "repair_base")) {
        cf->repair_base = min(10, max(0, atoi(value)));
    }
    else if (MATCH("wtp", "repair_base_native")) {
        cf->repair_base_native = min(10, max(0, atoi(value)));
    }
    else if (MATCH("wtp", "repair_base_facility")) {
        cf->repair_base_facility = min(10, max(0, atoi(value)));
    }
    else if (MATCH("wtp", "repair_nano_factory")) {
        cf->repair_nano_factory = min(10, max(0, atoi(value)));
    }
    else if (MATCH("wtp", "alternative_combat_mechanics")) {
        cf->alternative_combat_mechanics = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_combat_mechanics_loss_divider")) {
        cf->alternative_combat_mechanics_loss_divider = max(1.0, atof(value));
    }
// integrated into Thinker
//    else if (MATCH("wtp", "disable_planetpearls")) {
//        cf->disable_planetpearls = (atoi(value) == 0 ? false : true);
//    }
    else if (MATCH("wtp", "planet_combat_bonus_on_defense")) {
        cf->planet_combat_bonus_on_defense = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "sea_territory_distance_same_as_land")) {
        cf->sea_territory_distance_same_as_land = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "coastal_territory_distance_same_as_sea")) {
        cf->coastal_territory_distance_same_as_sea = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_artillery_damage")) {
        cf->alternative_artillery_damage = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "disable_home_base_cc_morale_bonus")) {
        cf->disable_home_base_cc_morale_bonus = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "disable_current_base_cc_morale_bonus")) {
        cf->disable_current_base_cc_morale_bonus = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "default_morale_very_green")) {
        cf->default_morale_very_green = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "combat_bonus_territory")) {
        cf->combat_bonus_territory = atoi(value);
    }
    else if (MATCH("wtp", "tube_movement_rate_multiplier")) {
        cf->tube_movement_rate_multiplier = atoi(value);
    }
    else if (MATCH("wtp", "project_contribution_threshold"))
    {
        cf->project_contribution_threshold = atoi(value);
    }
    else if (MATCH("wtp", "project_contribution_proportion"))
    {
        cf->project_contribution_proportion = atof(value);
    }
    else if (MATCH("wtp", "cloning_vats_disable_impunities"))
    {
        cf->cloning_vats_disable_impunities = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "cloning_vats_se_growth"))
    {
        cf->cloning_vats_se_growth = max(0, min(2, atoi(value)));
    }
    else if (MATCH("wtp", "se_growth_rating_cap"))
    {
        cf->se_growth_rating_cap = max(0, min(9, atoi(value)));
    }
    else if (MATCH("wtp", "recycling_tanks_mineral_multiplier"))
    {
        cf->recycling_tanks_mineral_multiplier = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "free_minerals"))
    {
        cf->free_minerals = max(0, atoi(value));
    }
    else if (MATCH("wtp", "native_life_generator_constant"))
    {
        cf->native_life_generator_constant = max(0, min(255, atoi(value)));
    }
    else if (MATCH("wtp", "native_life_generator_multiplier"))
    {
        cf->native_life_generator_multiplier = atoi(value);
    }
    else if (MATCH("wtp", "native_life_generator_more_sea_creatures"))
    {
        cf->native_life_generator_more_sea_creatures = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "native_disable_sudden_death"))
    {
        cf->native_disable_sudden_death = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_inefficiency"))
    {
        cf->alternative_inefficiency = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "unit_home_base_reassignment_production_threshold"))
    {
        cf->unit_home_base_reassignment_production_threshold = atoi(value);
    }
    else if (MATCH("wtp", "ocean_depth_multiplier"))
    {
        cf->ocean_depth_multiplier = atof(value);
    }
    else if (MATCH("wtp", "zoc_regular_army_sneaking_disabled"))
    {
        cf->zoc_regular_army_sneaking_disabled = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "pts_new_base_size_less_average"))
    {
        cf->pts_new_base_size_less_average = max(0, atoi(value));
    }
    else if (MATCH("wtp", "biology_lab_research_bonus"))
    {
        cf->biology_lab_research_bonus = max(0, atoi(value));
    }
    else if (MATCH("wtp", "hsa_does_not_kill_probe"))
    {
        cf->hsa_does_not_kill_probe = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "ai_useWTPAlgorithms"))
    {
        cf->ai_useWTPAlgorithms = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "ai_production_threat_coefficient_vendetta"))
    {
        cf->ai_production_threat_coefficient_vendetta = atof(value);
    }
    else if (MATCH("wtp", "ai_production_threat_coefficient_truce"))
    {
        cf->ai_production_threat_coefficient_truce = atof(value);
    }
    else if (MATCH("wtp", "ai_production_threat_coefficient_treaty"))
    {
        cf->ai_production_threat_coefficient_treaty = atof(value);
    }
    else if (MATCH("wtp", "ai_production_threat_coefficient_pact"))
    {
        cf->ai_production_threat_coefficient_pact = atof(value);
    }
    else if (MATCH("wtp", "ai_production_min_native_protection"))
    {
        cf->ai_production_min_native_protection = atof(value);
    }
    else if (MATCH("wtp", "ai_production_max_native_protection"))
    {
        cf->ai_production_max_native_protection = atof(value);
    }
    else if (MATCH("wtp", "ai_production_native_protection_priority_multiplier"))
    {
        cf->ai_production_native_protection_priority_multiplier = atof(value);
    }
    else if (MATCH("wtp", "ai_production_max_unpopulated_range"))
    {
        cf->ai_production_max_unpopulated_range = atoi(value);
    }
    else if (MATCH("wtp", "ai_production_expansion_coverage"))
    {
        cf->ai_production_expansion_coverage = atof(value);
    }
    else if (MATCH("wtp", "ai_production_colony_priority"))
    {
        cf->ai_production_colony_priority = atof(value);
    }
    else if (MATCH("wtp", "ai_production_unit_turns_limit"))
    {
        cf->ai_production_unit_turns_limit = atoi(value);
    }
    else if (MATCH("wtp", "ai_production_payoff_turns"))
    {
        cf->ai_production_payoff_turns = atoi(value);
    }
    else if (MATCH("wtp", "ai_production_combat_unit_min_mineral_surplus"))
    {
        cf->ai_production_combat_unit_min_mineral_surplus = atoi(value);
    }
    else if (MATCH("wtp", "ai_production_exploration_coverage"))
    {
        cf->ai_production_exploration_coverage = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_nutrientWeight"))
    {
        cf->ai_terraforming_nutrientWeight = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_mineralWeight"))
    {
        cf->ai_terraforming_mineralWeight = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_energyWeight"))
    {
        cf->ai_terraforming_energyWeight = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_landDistanceScale"))
    {
        cf->ai_terraforming_landDistanceScale = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_waterDistanceScale"))
    {
        cf->ai_terraforming_waterDistanceScale = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_networkConnectionValue"))
    {
        cf->ai_terraforming_networkConnectionValue = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_networkImprovementValue"))
    {
        cf->ai_terraforming_networkImprovementValue = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_networkBaseExtensionValue"))
    {
        cf->ai_terraforming_networkBaseExtensionValue = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_networkWildExtensionValue"))
    {
        cf->ai_terraforming_networkWildExtensionValue = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_networkCoverageThreshold"))
    {
        cf->ai_terraforming_networkCoverageThreshold = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_nearbyForestKelpPenalty"))
    {
        cf->ai_terraforming_nearbyForestKelpPenalty = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_rankMultiplier"))
    {
        cf->ai_terraforming_rankMultiplier = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_exclusivityMultiplier"))
    {
        cf->ai_terraforming_exclusivityMultiplier = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_baseNutrientThresholdRatio"))
    {
        cf->ai_terraforming_baseNutrientThresholdRatio = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_baseNutrientDemandMultiplier"))
    {
        cf->ai_terraforming_baseNutrientDemandMultiplier = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_baseMineralThresholdRatio"))
    {
        cf->ai_terraforming_baseMineralThresholdRatio = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_baseMineralDemandMultiplier"))
    {
        cf->ai_terraforming_baseMineralDemandMultiplier = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_raiseLandPayoffTime"))
    {
        cf->ai_terraforming_raiseLandPayoffTime = atof(value);
    }
    // Thinker default case
    else {
        for (int i=0; i<16; i++) {
            if (MATCH("thinker", lm_params[i])) {
                cf->landmarks &= ~((atoi(value) ? 0 : 1) << i);
                return 1;
            }
        }
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

int opt_list_parse(int* ptr, char* buf, int len, int min_val) {
    const char *d=",";
    char *s, *p;
    p = strtok_r(buf, d, &s);
    for (int i=0; i<len && p != NULL; i++, p = strtok_r(NULL, d, &s)) {
        ptr[i] = max(min_val, atoi(p));
    }
    return 1;
}

int cmd_parse(Config* cf) {
    int argc;
    LPWSTR* argv;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i=1; i<argc; i++) {
        if (wcscmp(argv[i], L"-smac") == 0) {
            cf->smac_only = 1;
        }
    }
    return 1;
}

DLL_EXPORT BOOL APIENTRY DllMain(HINSTANCE UNUSED(hinstDLL), DWORD fdwReason, LPVOID UNUSED(lpvReserved)) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            if (DEBUG && !(debug_log = fopen("debug.txt", "w"))) {
                return FALSE;
            }
            if (ini_parse("thinker.ini", handler, &conf) < 0) {
                MessageBoxA(0, "Error while opening thinker.ini file.",
                    MOD_VERSION, MB_OK | MB_ICONSTOP);
                exit(EXIT_FAILURE);
            }
            if (!cmd_parse(&conf) || !patch_setup(&conf)) {
                return FALSE;
            }
            srand(time(0));
            *tx_version = MOD_VERSION;
            *tx_date = MOD_DATE;
            break;

        case DLL_PROCESS_DETACH:
            fclose(debug_log);
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}

DLL_EXPORT int ThinkerDecide() {
    return 0;
}

HOOK_API int base_production(int id, int v1, int v2, int v3) {
    assert(id >= 0 && id < *total_num_bases);
    BASE* base = &tx_bases[id];
    int prod = base->queue_items[0];
    int faction = base->faction_id;
    int choice = 0;
    print_base(id);

    // do not suggest production for human factions

    if (is_human(faction))
	{
        debug("skipping human base\n");
        return base->queue_items[0];
    }

    // do not override production choice for not AI enabled factions

	if (!ai_enabled(faction))
	{
        debug("skipping computer base\n");
        return tx_base_prod_choices(id, v1, v2, v3);
    }

    // store production done flag for further use

    bool productionDone = (base->status_flags & BASE_PRODUCTION_DONE);

	tx_set_base(id);
	tx_base_compute(1);
	if ((choice = need_psych(id)) != 0 && choice != prod) {
		debug("BUILD PSYCH\n");
	} else if (base->status_flags & BASE_PRODUCTION_DONE || prod == -FAC_STOCKPILE_ENERGY) {

		choice = select_prod(id);

		base->status_flags &= ~BASE_PRODUCTION_DONE;

	} else if (prod >= 0 && !can_build_unit(faction, prod)) {
		debug("BUILD FACILITY\n");
		choice = find_facility(id);
	} else if (prod < 0 && !can_build(id, abs(prod))) {
		debug("BUILD CHANGE\n");
		if (base->minerals_accumulated > tx_basic->retool_exemption) {
			choice = find_facility(id);
		} else {
			choice = select_prod(id);
		}
	} else if (need_defense(id)) {
		debug("BUILD DEFENSE\n");
		choice = find_proto(id, TRIAD_LAND, COMBAT, DEF);
	} else {
		// hurrying is considered globally after production phase
//		consider_hurry(id);
		debug("BUILD OLD\n");
		choice = prod;
	}
	debug("choice: %d %s\n", choice, prod_name(choice));

    // change choice to growth facility if needed

    choice = refitToGrowthFacility(id, base, choice);

    fflush(debug_log);

	// [WTP] production choice override

	choice = suggestBaseProduction(id, productionDone, choice);

    return choice;

}

HOOK_API int turn_upkeep() {
    tx_turn_upkeep();
    debug("turn_upkeep %d bases: %d vehicles: %d\n",
        *current_turn, *total_num_bases, *total_num_vehicles);

    for (int i=1; i<8 && conf.design_units; i++) {
        if (is_human(i) || !tx_factions[i].current_num_bases)
            continue;
        int rec = best_reactor(i);
        int wpn = best_weapon(i);
        int arm = best_armor(i, false);
        int arm2 = best_armor(i, true);
        int chs = has_chassis(i, CHS_HOVERTANK) ? CHS_HOVERTANK : CHS_SPEEDER;
        bool twoabl = has_tech(i, tx_basic->tech_preq_allow_2_spec_abil);
        char buf[200];

        if (has_weapon(i, WPN_PROBE_TEAM)) {
            int algo = has_ability(i, ABL_ID_ALGO_ENHANCEMENT) ? ABL_ALGO_ENHANCEMENT : 0;
            if (has_chassis(i, CHS_FOIL)) {
                int ship = (rec >= REC_FUSION && has_chassis(i, CHS_CRUISER) ? CHS_CRUISER : CHS_FOIL);
                tx_propose_proto(i, ship, WPN_PROBE_TEAM, (rec >= REC_FUSION ? arm : 0), algo,
                rec, PLAN_INFO_WARFARE, (ship == CHS_FOIL ? "Foil Probe Team" : "Cruiser Probe Team"));
            }
            if (arm != ARM_NO_ARMOR && rec >= REC_FUSION) {
                char* name = parse_str(buf, 32, tx_reactor[rec-1].name_short, " ",
                    tx_defense[arm].name_short, " Probe Team");
                tx_propose_proto(i, chs, WPN_PROBE_TEAM, arm, algo, rec, PLAN_INFO_WARFARE, name);
            }
        }
        if (has_ability(i, ABL_ID_AAA) && arm != ARM_NO_ARMOR) {
            int abls = ABL_AAA;
            if (twoabl) {
                abls |= (has_ability(i, ABL_ID_COMM_JAMMER) ? ABL_COMM_JAMMER :
                    (has_ability(i, ABL_ID_TRANCE) ? ABL_TRANCE : 0));
            }
            tx_propose_proto(i, CHS_INFANTRY, WPN_HAND_WEAPONS, arm, abls, rec, PLAN_DEFENSIVE, NULL);
        }
        if (has_ability(i, ABL_ID_TRANCE) && !has_ability(i, ABL_ID_AAA)) {
            tx_propose_proto(i, CHS_INFANTRY, WPN_HAND_WEAPONS, arm, ABL_TRANCE, rec, PLAN_DEFENSIVE, NULL);
        }
        if (has_chassis(i, CHS_NEEDLEJET) && has_ability(i, ABL_ID_AIR_SUPERIORITY)) {
            tx_propose_proto(i, CHS_NEEDLEJET, wpn, ARM_NO_ARMOR, ABL_AIR_SUPERIORITY | ABL_DEEP_RADAR,
            rec, PLAN_AIR_SUPERIORITY, NULL);
        }
        if (has_weapon(i, WPN_TERRAFORMING_UNIT) && rec >= REC_FUSION) {
            bool grav = has_chassis(i, CHS_GRAVSHIP);
            int abls = has_ability(i, ABL_ID_SUPER_TERRAFORMER ? ABL_SUPER_TERRAFORMER : 0) |
                (twoabl && !grav && has_ability(i, ABL_ID_FUNGICIDAL) ? ABL_FUNGICIDAL : 0);
            tx_propose_proto(i, (grav ? CHS_GRAVSHIP : chs), WPN_TERRAFORMING_UNIT, ARM_NO_ARMOR, abls,
            REC_FUSION, PLAN_TERRAFORMING, NULL);
        }
        if (has_weapon(i, WPN_SUPPLY_TRANSPORT) && rec >= REC_FUSION && arm2 != ARM_NO_ARMOR) {
            char* name = parse_str(buf, 32, tx_reactor[REC_FUSION-1].name_short, " ",
                tx_defense[arm2].name_short, " Supply");
            tx_propose_proto(i, CHS_INFANTRY, WPN_SUPPLY_TRANSPORT, arm2, 0, REC_FUSION, PLAN_DEFENSIVE, name);
        }
    }

    // Creation of free vehicles is moved to modifiedSetupPlayer

//    if (*current_turn == 1) {
//
//		// give colony and former bonuses to human too
////        int bonus = ~(*human_players) & 0xfe;
//        int bonus = ~(0) & 0xfe;
//
//        for (int i=0; i<*total_num_vehicles; i++) {
//            VEH* veh = &tx_vehicles[i];
//            if (1 << veh->faction_id & bonus) {
//                bonus &= ~(1 << veh->faction_id);
//                MAP* sq = mapsq(veh->x, veh->y);
//                int former = (is_ocean(sq) ? BSC_SEA_FORMERS : BSC_FORMERS);
//                int colony = (is_ocean(sq) ? BSC_SEA_ESCAPE_POD : BSC_COLONY_POD);
//                for (int j=0; j<conf.free_formers; j++) {
//                    spawn_veh(former, veh->faction_id, veh->x, veh->y, -1);
//                }
//                for (int j=0; j<conf.free_colony_pods; j++) {
//				spawn_veh(colony, veh->faction_id, veh->x, veh->y, -1);
//                }
//            }
//        }
//        for (int i=1; i<8; i++) {
//            if (!is_human(i)) {
//                tx_factions[i].satellites_nutrient = conf.satellites_nutrient;
//                tx_factions[i].satellites_mineral = conf.satellites_mineral;
//                tx_factions[i].satellites_energy = conf.satellites_energy;
//            }
//        }
//    }

    if (DEBUG) {
        *game_state |= STATE_DEBUG_MODE;
    }
    int minerals[BASES];
    for (int i=1; i<8; i++) {
        Faction* f = &tx_factions[i];
        R_Resource* r = tx_resource;
        if (is_human(i) || !f->current_num_bases)
            continue;
        plans[i].enemy_bases = 0;
        plans[i].diplo_flags = 0;
        for (int j=1; j<8; j++) {
            int st = f->diplo_status[j];
            if (i != j && st & DIPLO_COMMLINK) {
                plans[i].diplo_flags |= st;
            }
        }
        int n = 0;
        for (int j=0; j<*total_num_bases; j++) {
            BASE* base = &tx_bases[j];
            if (base->faction_id == i) {
                minerals[n++] = base->mineral_surplus;
            } else if (base->faction_id_former == i && at_war(i, base->faction_id)) {
                plans[i].enemy_bases += (is_human(base->faction_id) ? 4 : 1);
            }
        }
        std::sort(minerals, minerals+n);
        plans[i].proj_limit = max(5, minerals[n*2/3]);
        plans[i].need_police = (f->SE_police > -2 && f->SE_police < 3
            && !has_project(i, FAC_TELEPATHIC_MATRIX));

        int psi = max(-4, 2 - f->best_weapon_value);
        if (has_project(i, FAC_NEURAL_AMPLIFIER))
            psi += 2;
        if (has_project(i, FAC_DREAM_TWISTER))
            psi += 2;
        if (has_project(i, FAC_PHOLUS_MUTAGEN))
            psi++;
        if (has_project(i, FAC_XENOEMPATHY_DOME))
            psi++;
        if (tx_metafactions[i].rule_flags & FACT_WORMPOLICE && plans[i].need_police)
            psi += 2;
        psi += min(2, max(-2, (f->enemy_best_weapon_value - f->best_weapon_value)/2));
        psi += tx_metafactions[i].rule_psi/10 + 2*f->SE_planet;
        plans[i].psi_score = psi;

        const int manifold[][3] = {{0,0,0}, {0,1,0}, {1,1,0}, {1,1,1}, {1,2,1}};
        int p = (has_project(i, FAC_MANIFOLD_HARMONICS) ? min(4, max(0, f->SE_planet + 1)) : 0);
        int dn = manifold[p][0] + f->tech_fungus_nutrient - r->forest_sq_nutrient;
        int dm = manifold[p][1] + f->tech_fungus_mineral - r->forest_sq_mineral;
        int de = manifold[p][2] + f->tech_fungus_energy - r->forest_sq_energy;
        bool terra_fungus = has_terra(i, FORMER_PLANT_FUNGUS, 0)
            && (has_tech(i, tx_basic->tech_preq_ease_fungus_mov) || has_project(i, FAC_XENOEMPATHY_DOME));

        plans[i].keep_fungus = (dn+dm+de > 0);
        if (dn+dm+de > 0 && terra_fungus) {
            plans[i].plant_fungus = dn+dm+de;
        } else {
            plans[i].plant_fungus = 0;
        }
        debug("turn_values %d %d proj_limit: %d psi: %d keep_fungus: %d "\
            "plant_fungus: %d enemy_bases: %d enemy_range: %.4f\n",
            *current_turn, i, plans[i].proj_limit, psi, plans[i].keep_fungus, plans[i].plant_fungus,
            plans[i].enemy_bases, f->thinker_enemy_range);
    }

    return 0;
}

int project_score(int faction, int proj) {
    R_Facility* p = &tx_facility[proj];
    Faction* f = &tx_factions[faction];
    return random(3) + f->AI_fight * p->AI_fight
        + (f->AI_growth+1) * p->AI_growth + (f->AI_power+1) * p->AI_power
        + (f->AI_tech+1) * p->AI_tech + (f->AI_wealth+1) * p->AI_wealth;
}

int find_project(int id) {
    BASE* base = &tx_bases[id];
    int faction = base->faction_id;
    Faction* f = &tx_factions[faction];
    int bases = f->current_num_bases;
    int nuke_limit = (f->planet_busters < f->AI_fight + 2 ? 1 : 0);
    int similar_limit = (base->minerals_accumulated > 80 ? 2 : 1);
    int projs = 0;
    int nukes = 0;
    int works = 0;

    bool build_nukes = has_weapon(faction, WPN_PLANET_BUSTER) &&
        (!un_charter() || plans[faction].diplo_flags & DIPLO_ATROCITY_VICTIM ||
        (f->AI_fight > 0 && f->AI_power > 0));

    for (int i=0; i<*total_num_bases; i++) {
        if (tx_bases[i].faction_id == faction) {
            int t = tx_bases[i].queue_items[0];
            if (t <= -PROJECT_ID_FIRST || t == -FAC_SUBSPACE_GENERATOR) {
                projs++;
            } else if (t == -FAC_SKUNKWORKS) {
                works++;
            } else if (t >= 0 && tx_units[t].weapon_type == WPN_PLANET_BUSTER) {
                nukes++;
            }
        }
    }
    if (build_nukes && nukes < nuke_limit && nukes < bases/8) {
        int best = 0;
        for(int i=0; i<64; i++) {
            int unit_id = faction*64 + i;
            UNIT* u = &tx_units[unit_id];
            if (u->weapon_type == WPN_PLANET_BUSTER && strlen(u->name) > 0
            && offense_value(u) > offense_value(&tx_units[best])) {
                debug("find_project %d %d %s\n", faction, unit_id, (char*)tx_units[unit_id].name);
                best = unit_id;
            }
        }
        if (best) {
            if (~tx_units[best].unit_flags & UNIT_PROTOTYPED && can_build(id, FAC_SKUNKWORKS)) {
                if (works < 2)
                    return -FAC_SKUNKWORKS;
            } else {
                return best;
            }
        }
    }
    if (projs+nukes < (build_nukes ? 4 : 3) && projs+nukes < bases/4) {
        bool alien = tx_metafactions[faction].rule_flags & FACT_ALIEN;
        if (alien && can_build(id, FAC_SUBSPACE_GENERATOR)) {
            return -FAC_SUBSPACE_GENERATOR;
        }
        int score = INT_MIN;
        int choice = 0;
        for (int i=PROJECT_ID_FIRST; i<=PROJECT_ID_LAST; i++) {
            int tech = tx_facility[i].preq_tech;
            if (alien && (i == FAC_ASCENT_TO_TRANSCENDENCE || i == FAC_VOICE_OF_PLANET)) {
                continue;
            }
            if (conf.limit_project_start > *diff_level && i != FAC_ASCENT_TO_TRANSCENDENCE
            && tech >= 0 && !(tx_tech_discovered[tech] & *human_players)) {
                continue;
            }
            if (can_build(id, i) && prod_count(faction, -i, id) < similar_limit) {
                int sc = project_score(faction, i);
                choice = (sc > score ? i : choice);
                score = max(score, sc);
                debug("find_project %d %d %d %s\n", faction, i, sc, tx_facility[i].name);
            }
        }
        return (projs > 0 || score > 2 ? -choice : 0);
    }
    return 0;
}

bool relocate_hq(int id) {
    BASE* base = &tx_bases[id];
    int faction = base->faction_id;
    Faction* f = &tx_factions[faction];
    int bases = f->current_num_bases;
    int n = 0;
    int k = 0;

    if (!has_tech(faction, tx_facility[FAC_HEADQUARTERS].preq_tech)
    || base->mineral_surplus < (bases < 6 ? 4 : plans[faction].proj_limit)) {
        return false;
    }
    for (int i=0; i<*total_num_bases; i++) {
        BASE* b = &tx_bases[i];
        int t = b->queue_items[0];
        if (b->faction_id == faction) {
            if (i != id) {
                if (i < id)
                    k++;
                if (map_range(base->x, base->y, b->x, b->y) < 7)
                    n++;
            }
            if (t == -FAC_HEADQUARTERS || has_facility(i, FAC_HEADQUARTERS)) {
                return false;
            }
        }
    }
    /*
    Base IDs are assigned in the foundation order, so this pseudorandom selection tends
    towards bases that were founded at an earlier date and have many neighboring friendly bases.
    */
    return random(100) < 30.0 * (1.0 - 1.0 * k / bases) + 30.0 * min(15, n) / min(15, bases);
}

bool can_build_ships(int id) {
    BASE* b = &tx_bases[id];
    int k = *map_area_sq_root + 20;
    return has_chassis(b->faction_id, CHS_FOIL) && nearby_tiles(b->x, b->y, TRIAD_SEA, k) >= k;
}

/*
Return true if unit2 is strictly better than unit1 in all circumstances (non PSI).
Disable random chance in prototype choices in these instances.
*/
bool unit_is_better(UNIT* u1, UNIT* u2) {
    bool val = (u1->cost >= u2->cost
        && offense_value(u1) >= 0
        && offense_value(u1) <= offense_value(u2)
        && defense_value(u1) <= defense_value(u2)
        && tx_chassis[u1->chassis_type].speed <= tx_chassis[u2->chassis_type].speed
        && (tx_chassis[u2->chassis_type].triad != TRIAD_AIR || u1->chassis_type == u2->chassis_type)
        && !((u1->ability_flags & u2->ability_flags) ^ u1->ability_flags));
    if (val) {
        debug("unit_is_better %s -> %s\n", u1->name, u2->name);
    }
    return val;
}

int unit_score(int id, int faction, int cfactor, int minerals, bool def) {
    const int abls[][2] = {
        {ABL_AAA, 4},
        {ABL_AIR_SUPERIORITY, 2},
        {ABL_ALGO_ENHANCEMENT, 5},
        {ABL_AMPHIBIOUS, -4},
        {ABL_ARTILLERY, -2},
        {ABL_DROP_POD, 2},
        {ABL_EMPATH, 2},
        {ABL_TRANCE, 3},
        {ABL_TRAINED, 2},
        {ABL_COMM_JAMMER, 3},
        {ABL_ANTIGRAV_STRUTS, 3},
        {ABL_BLINK_DISPLACER, 3},
        {ABL_DEEP_PRESSURE_HULL, 4},
        {ABL_SUPER_TERRAFORMER, 8},
    };
    UNIT* u = &tx_units[id];
    int v = 18 * (def ? defense_value(u) : offense_value(u));
    if (v < 0) {
        v = (def ? tx_factions[faction].best_armor_value : tx_factions[faction].best_weapon_value)
            * best_reactor(faction) + plans[faction].psi_score * 12;
    }
    if (unit_triad(id) == TRIAD_LAND) {
        v += (def ? 10 : 18) * unit_speed(id);
    }
    if (u->ability_flags & ABL_POLICE_2X && plans[faction].need_police) {
        v += 20;
    }
    for (const int* i : abls) {
        if (u->ability_flags & i[0]) {
            v += 8 * i[1];
        }
    }
    int turns = u->cost * cfactor / max(2, minerals);
    int score = v - turns * (u->weapon_type == WPN_COLONY_MODULE ? 18 : 9);
    debug("unit_score %s cfactor: %d minerals: %d cost: %d turns: %d score: %d\n",
        u->name, cfactor, minerals, u->cost, turns, score);
    return score;
}

/*
Find the best prototype for base production when weighted against cost given the triad
and type constraints. For any combat-capable unit, mode is set to COMBAT (= 0).
*/
int find_proto(int base_id, int triad, int mode, bool defend) {

	BASE *base = &(tx_bases[base_id]);

    assert(base_id >= 0 && base_id < *total_num_bases);
    assert(mode >= COMBAT && mode <= WMODE_INFOWAR);
    assert(triad == TRIAD_LAND || triad == TRIAD_SEA || triad == TRIAD_AIR);
    BASE* b = &tx_bases[base_id];
    int faction = b->faction_id;
    int basic = BSC_SCOUT_PATROL;
    debug("find_proto faction: %d triad: %d mode: %d def: %d\n", faction, triad, mode, defend);
    if (mode == WMODE_COLONIST)
        basic = (triad == TRIAD_SEA ? BSC_SEA_ESCAPE_POD : BSC_COLONY_POD);
    else if (mode == WMODE_TERRAFORMER)
        basic = (triad == TRIAD_SEA ? BSC_SEA_FORMERS : BSC_FORMERS);
    else if (mode == WMODE_CONVOY)
        basic = BSC_SUPPLY_CRAWLER;
    else if (mode == WMODE_TRANSPORT)
        basic = BSC_TRANSPORT_FOIL;
    else if (mode == WMODE_INFOWAR)
        basic = BSC_PROBE_TEAM;
    int best = basic;
    int cfactor = tx_cost_factor(faction, 1, -1);
    int minerals = b->mineral_surplus;
    int best_val = unit_score(best, faction, cfactor, minerals, defend);

	// calculate cost limit for unit

	int unitCostLimit = base->mineral_surplus * conf.ai_production_unit_turns_limit / tx_cost_factor(faction, 1, -1);

    for (int i=0; i < 128; i++) {
        int id = (i < 64 ? i : (faction-1)*64 + i);
        UNIT* u = &tx_units[id];

        // exclude too costly units

        if (u->cost > unitCostLimit)
			continue;

        if (strlen(u->name) > 0 && unit_triad(id) == triad && id != best) {
            if (id < 64 && !has_tech(faction, u->preq_tech))
                continue;
            if ((mode && tx_weapon[u->weapon_type].mode != mode)
            || (!mode && tx_weapon[u->weapon_type].offense_value == 0)
            || (!mode && defend && u->chassis_type != CHS_INFANTRY)
            || (u->weapon_type == WPN_PSI_ATTACK && plans[faction].psi_score < 1)
            || u->weapon_type == WPN_PLANET_BUSTER
            || !(mode || best == basic || (defend == (offense_value(u) < defense_value(u)))))
                continue;
            int val = unit_score(id, faction, cfactor, minerals, defend);
            if (unit_is_better(&tx_units[best], u) || random(100) > 50 + best_val - val) {
                best = id;
                best_val = val;
                debug("===> %s\n", tx_units[best].name);
            }
        }
    }
    return best;
}

int need_defense(int id) {
    BASE* b = &tx_bases[id];
    int prod = b->queue_items[0];
    /* Do not interrupt secret project building. */
    return !has_defenders(b->x, b->y, b->faction_id)
        && prod < 0 && prod > -FAC_HUMAN_GENOME_PROJ;
}

int need_psych(int id) {
    BASE* b = &tx_bases[id];
    int faction = b->faction_id;
    int unit = unit_in_tile(mapsq(b->x, b->y));
    Faction* f = &tx_factions[faction];

    if (unit != faction || b->nerve_staple_turns_left > 0 || has_project(faction, FAC_TELEPATHIC_MATRIX))
	{
        return 0;
	}

	// get base doctors

	int doctors = getDoctors(id);

	// do not build psych facility until there are drones or doctors

	if (b->drone_total == 0 && doctors == 0)
		return 0;

	// calculate current production time and extra population grown by that time
	// this extra population also will be added to drones to determine need for psych facility

	int mineralCost = mineral_cost(faction, b->queue_items[0]);
	int productionTime = (mineralCost - b->minerals_accumulated) / max(1, b->mineral_surplus) + 1;
	int nutrientProduced = b->nutrients_accumulated + b->nutrient_surplus * productionTime;
	int nutrientBoxSize = (b->pop_size + 1) * tx_cost_factor(faction, 0, id);
	int extraPopulation = nutrientProduced / nutrientBoxSize;

	// consider building psych facility if there are doctors or drones will outnumber talents soon

    if (doctors > 0 || (b->drone_total + extraPopulation) > b->talent_total) {
        if (b->nerve_staple_count < 3 && !un_charter() && f->SE_police >= 0 && b->pop_size >= 4
        && b->faction_id_former == faction) {
            tx_action_staple(id);
            return 0;
        }
        if (can_build(id, FAC_RECREATION_COMMONS))
            return -FAC_RECREATION_COMMONS;
        if (has_project(faction, FAC_VIRTUAL_WORLD) && can_build(id, FAC_NETWORK_NODE))
            return -FAC_NETWORK_NODE;
        if (/*!b->assimilation_turns_left && b->pop_size >= 6 && b->energy_surplus >= 12
        && */can_build(id, FAC_HOLOGRAM_THEATRE))
            return -FAC_HOLOGRAM_THEATRE;
    }
    return 0;
}

int hurry_item(BASE* b, int mins, int cost) {
    Faction* f = &tx_factions[b->faction_id];
    f->energy_credits -= cost;
    b->minerals_accumulated += mins;
    b->status_flags |= BASE_HURRY_PRODUCTION;
    debug("hurry_item %d %d %d %d %d %s %s\n", *current_turn, b->faction_id,
        mins, cost, f->energy_credits, b->name, prod_name(b->queue_items[0]));
    return 1;
}

int consider_hurry(int id) {
    BASE* b = &tx_bases[id];
    int faction = b->faction_id;
    int t = b->queue_items[0];
    Faction* f = &tx_factions[faction];
    MetaFaction* m = &tx_metafactions[faction];

    // applicability condition
    if
    (
        !
        (
            conf.hurry_items
            &&
            // [WtP] allow hurrying everything
            (
                t >= -FAC_ORBITAL_DEFENSE_POD
                ||
                (t >= -PROJECT_ID_LAST && t <= -PROJECT_ID_FIRST)
            )
            &&
            (~b->status_flags & BASE_HURRY_PRODUCTION)
         )
    )
        return 0;

    // determine hurry penalty threshold and hurry cost
    bool cheap;
    // projects
    if (t >= -PROJECT_ID_LAST && t <= -PROJECT_ID_FIRST)
    {
        // no threshold
        if (conf.disable_hurry_penalty_threshold)
        {
            cheap = true;
        }
        // vanilla default threshold
        else
        {
            int rowMinerals = 10 - max(-5, min(5, f->SE_industry));
            cheap = b->minerals_accumulated >= 4 * rowMinerals;
        }
    }
    // facilities and units
    else
    {
        // no threshold
        if (conf.disable_hurry_penalty_threshold)
        {
            cheap = true;
        }
        // vanilla default threshold
        else
        {
            cheap = b->minerals_accumulated >= tx_basic->retool_exemption;
        }
    }

    int reserve = 20 + min(900, max(0, f->current_num_bases * min(30, (*current_turn - 20)/5)));
    int mins = mineral_cost(faction, t) - b->minerals_accumulated;
    int turns = (int)ceil(mins / max(0.01, 1.0 * b->mineral_surplus));

    // hurry cost calculation
    int cost;
    // facility
    if (t >= -FAC_ORBITAL_DEFENSE_POD && t < 0)
    {
        cost = (cheap ? 1 : 2) * 2 * mins;
    }
    // project
    else if (t >= -PROJECT_ID_LAST && t <= -PROJECT_ID_FIRST)
    {
        cost = (cheap ? 1 : 2) * 4 * mins;
    }
    // units
    else if (t >= 0)
    {
        // alternative formula
        if (conf.alternative_unit_hurry_formula)
        {
            cost = (cheap ? 1 : 2) * 4 * mins;
        }
        // vanilla default formula
        else
        {
            cost = (mins * mins / 20 + 2 * mins) * (cheap ? 1 : 2);
        }
    }
    // unsupported item type
    else
    {
        return 0;
    }
    cost = cost * m->rule_hurry / 100;

    // VoP doubles hurry cost

    if (tx_secret_projects[FAC_VOICE_OF_PLANET - PROJECT_ID_FIRST] > PROJECT_UNBUILT)
	{
		cost *= 2;
	}

    // hurry only cheap cases
    if (!(cheap && mins > 0 && cost > 0 && f->energy_credits - cost > reserve))
        return 0;

    if (b->drone_total > b->talent_total && t < 0 && t == need_psych(id))
        return hurry_item(b, mins, cost);
    if (t < 0 && turns > 1) {
        if (t == -FAC_RECYCLING_TANKS || t == -FAC_PRESSURE_DOME
        || t == -FAC_RECREATION_COMMONS || t == -FAC_HEADQUARTERS)
            return hurry_item(b, mins, cost);
        if (t == -FAC_CHILDREN_CRECHE && f->SE_growth >= 2)
            return hurry_item(b, mins, cost);
        if (t == -FAC_PERIMETER_DEFENSE && f->thinker_enemy_range < 15)
            return hurry_item(b, mins, cost);
        if (t == -FAC_AEROSPACE_COMPLEX && has_tech(faction, TECH_Orbital))
            return hurry_item(b, mins, cost);
        // [WtP] hurry SP always
        if (t >= -PROJECT_ID_LAST && t <= -PROJECT_ID_FIRST)
            return hurry_item(b, mins, cost);
    }
    if (t >= 0 && turns > 1) {
        if (f->thinker_enemy_range < 15 && tx_units[t].weapon_type <= WPN_PSI_ATTACK
        && !has_defenders(b->x, b->y, faction)) {
            return hurry_item(b, mins, cost);
        }
        if (tx_units[t].weapon_type == WPN_TERRAFORMING_UNIT && b->pop_size < 3) {
            return hurry_item(b, mins, cost);
        }
    }
    return 0;
}

int find_facility(int id)
{
    const int sumbergeProtectionFacilites[] =
    {
        FAC_PRESSURE_DOME,
    };

    const int defenseFacilites[] =
    {
        FAC_PERIMETER_DEFENSE,
        FAC_TACHYON_FIELD,
    };

    const int populationLimitFacilites[] =
    {
        FAC_HAB_COMPLEX,
        FAC_HABITATION_DOME,
    };

    const int psychFacilityIds[] =
    {
        FAC_RECREATION_COMMONS,
        FAC_HOLOGRAM_THEATRE,
        FAC_PARADISE_GARDEN,
    };

    const int growthFacilityIds[] =
    {
        FAC_CHILDREN_CRECHE,
    };

    const int mineralFacilityIds[] =
    {
        FAC_RECYCLING_TANKS,
        FAC_GENEJACK_FACTORY,
        FAC_ROBOTIC_ASSEMBLY_PLANT,
        FAC_QUANTUM_CONVERTER,
        FAC_NANOREPLICATOR,
    };

    // bitmask: 1 = labs, 2 = economy, 4 = psych
    const int energyFacilityIds[][2] =
    {
        {FAC_NETWORK_NODE, 1},
        {FAC_ENERGY_BANK, 2},
        {FAC_HOLOGRAM_THEATRE, 4},
        {FAC_FUSION_LAB, 3},
        {FAC_QUANTUM_LAB, 3},
        {FAC_RESEARCH_HOSPITAL, 5},
        {FAC_NANOHOSPITAL, 5},
        {FAC_TREE_FARM, 6},
        {FAC_HYBRID_FOREST, 6},
    };

    const int fixedLabsFacilityIds[][2] =
    {
        {FAC_BIOLOGY_LAB, 4},
    };

    // TODO refactor these too
    const int build_order[][2] = {
        {FAC_AEROSPACE_COMPLEX, 0},
        {FAC_COMMAND_CENTER, 2},
        {FAC_GEOSYNC_SURVEY_POD, 2},
        {FAC_FLECHETTE_DEFENSE_SYS, 2},
        {FAC_TACHYON_FIELD, 4},
    };

    BASE* base = &tx_bases[id];
    int faction = base->faction_id;
    int proj;
    int minerals = base->mineral_surplus;
    int extra = base->minerals_accumulated/10;
    int pop_rule = tx_metafactions[faction].rule_population;
    int hab_complex_limit = tx_basic->pop_limit_wo_hab_complex - pop_rule;
    int hab_dome_limit = tx_basic->pop_limit_wo_hab_dome - pop_rule;
    int popuationLimits[] = {hab_complex_limit, hab_dome_limit};
    Faction* f = &tx_factions[faction];
    bool sea_base = is_sea_base(id);
    bool core_base = minerals+extra >= plans[faction].proj_limit;
    bool can_build_units = can_build_unit(faction, -1);
	int doctors = getDoctors(id);

	// submerge protection

    if (*climate_future_change > 0)
	{
        MAP* sq = mapsq(base->x, base->y);
        if (sq && (sq->level >> 5) == LEVEL_SHORE_LINE)
		{
			for (int sumbergeProtectionFacilityId : sumbergeProtectionFacilites)
			{
				if (can_build(id, sumbergeProtectionFacilityId))
					return -sumbergeProtectionFacilityId;

			}

		}

    }

    // defense

	for (int defenseFacilityId : defenseFacilites)
	{
		if (can_build(id, defenseFacilityId))
		{
			if (f->thinker_enemy_range < 40 - min(12, 3 * tx_facility[defenseFacilityId].maint))
				return -defenseFacilityId;
		}

	}

	// population limit

	for (int i = 0; i < 2; i++)
	{
		int populationLimitFacilityId = populationLimitFacilites[i];
		int populationLimit = popuationLimits[i];

		if (can_build(id, populationLimitFacilityId))
		{
			// project population in as much turns as allowed to build most expensive unit

			int populationSizeProjection = base->pop_size + (base->nutrients_accumulated + base->nutrient_surplus * conf.ai_production_unit_turns_limit) / ((base->pop_size + 1) * tx_cost_factor(faction, 0, id));

			// build facility if population projection is above population limit

			if (populationSizeProjection > populationLimit)
			{
				if (can_build(id, populationLimitFacilityId))
				{
					return -populationLimitFacilityId;
				}

			}

		}

	}

	// psych

	if (base->drone_total > base->talent_total || doctors > 0)
	{
		for (int psychFacilityId : psychFacilityIds)
		{
			if (can_build(id, psychFacilityId))
				return -psychFacilityId;

		}

	}

	// growth

	for (int growthFacilityId : growthFacilityIds)
	{
		if (can_build(id, growthFacilityId))
			return -growthFacilityId;

	}

	// mineral

	for (int mineralFacilityId : mineralFacilityIds)
	{
		if (can_build(id, mineralFacilityId))
		{
			// calculate break even turns

			int payoffTurns = mineral_cost(faction, -mineralFacilityId) / max(1,(base->mineral_intake / 2 - tx_facility[mineralFacilityId].maint / 2));

			// build facility if it breaks even soon enough

			if (payoffTurns <= conf.ai_production_payoff_turns)
				return -mineralFacilityId;

		}

	}

	// project

    if (core_base && (proj = find_project(id)) != 0)
	{
        return proj;
    }

	// energy

	for (const int *energyFacilityStruct : energyFacilityIds)
	{
		int energyFacilityId = energyFacilityStruct[0];
		int energyFacilityFocus = energyFacilityStruct[1];

		if (can_build(id, energyFacilityId))
		{
			// calculate focus effect

			int effect = 0;

			if (energyFacilityFocus & 0x1)
			{
				effect += base->labs_total;
			}
			if (energyFacilityFocus & 0x2)
			{
				effect += base->economy_total;
			}
			if (energyFacilityFocus & 0x3)
			{
				effect += base->psych_total;
			}

			// calculate break even turns

			int payoffTurns = mineral_cost(faction, -energyFacilityId) / max(1, (effect / 2 / 2 - tx_facility[energyFacilityId].maint / 2));

			// build facility if it breaks even soon enough

			if (payoffTurns <= conf.ai_production_payoff_turns)
				return -energyFacilityId;

		}

	}

	// fixedLabs

	for (const int *fixedLabsFacilityStruct : fixedLabsFacilityIds)
	{
		int fixedLabsFacilityId = fixedLabsFacilityStruct[0];
		int fixedLabsFacilityBonus = fixedLabsFacilityStruct[1];

		if (can_build(id, fixedLabsFacilityId))
		{
			// calculate break even turns

			int payoffTurns = mineral_cost(faction, -fixedLabsFacilityId) / max(1, (fixedLabsFacilityBonus / 2 - tx_facility[fixedLabsFacilityId].maint / 2));

			// build facility if it breaks even soon enough

			if (payoffTurns <= conf.ai_production_payoff_turns)
				return -fixedLabsFacilityId;

		}

	}

	// satellites

    if (core_base && has_facility(id, FAC_AEROSPACE_COMPLEX))
	{
        if (can_build(id, FAC_ORBITAL_DEFENSE_POD))
            return -FAC_ORBITAL_DEFENSE_POD;
        if (can_build(id, FAC_NESSUS_MINING_STATION))
            return -FAC_NESSUS_MINING_STATION;
        if (can_build(id, FAC_ORBITAL_POWER_TRANS))
            return -FAC_ORBITAL_POWER_TRANS;
        if (can_build(id, FAC_SKY_HYDRO_LAB))
            return -FAC_SKY_HYDRO_LAB;
    }

    for (const int* t : build_order) {
        int c = t[0];
        R_Facility* fc = &tx_facility[c];
        /* Check if we have sufficient base energy for multiplier facilities. */
        if (t[1] & 1 && base->energy_surplus < 2*fc->maint + fc->cost/(f->AI_tech ? 2 : 1))
            continue;
        /* Avoid building combat-related facilities in peacetime. */
        if (t[1] & 2 && f->thinker_enemy_range > 40 - min(12, 3*fc->maint))
            continue;
        /* Build these facilities only if the global unit limit is reached. */
        if (t[1] & 4 && can_build_units)
            continue;
        if (c == FAC_COMMAND_CENTER && (sea_base || !core_base || f->SE_morale < 0))
            continue;
        if (c == FAC_GENEJACK_FACTORY && base->mineral_intake < 16)
            continue;
        if (c == FAC_HAB_COMPLEX && base->pop_size+1 < hab_complex_limit)
            continue;
        if (c == FAC_HABITATION_DOME && base->pop_size < hab_dome_limit)
            continue;
        /* Place survey pods only randomly on some bases to reduce maintenance. */
        if ((c == FAC_GEOSYNC_SURVEY_POD || c == FAC_FLECHETTE_DEFENSE_SYS)
        && (minerals*2 < fc->cost*3 || (map_hash(base->x, base->y) % 101 > 25)))
            continue;
        if (can_build(id, c)) {
            return -1*c;
        }
    }

    if (!can_build_units) {
        return -FAC_STOCKPILE_ENERGY;
    }

    debug("BUILD OFFENSE\n");

    bool build_ships = can_build_ships(id) && (sea_base || !random(3));

    return select_combat(id, sea_base, build_ships, 0, 0);

}

int select_colony(int id, int pods, bool build_ships) {
    BASE* base = &tx_bases[id];
    int faction = base->faction_id;
    bool land = has_base_sites(base->x, base->y, faction, TRIAD_LAND);
    bool sea = has_base_sites(base->x, base->y, faction, TRIAD_SEA);
    int limit = (*current_turn < 60 || (!random(3) && (land || (build_ships && sea))) ? 2 : 1);

    if (pods >= limit) {
        return -1;
    }
    if (is_sea_base(id)) {
        for (const int* t : offset) {
            int x2 = wrap(base->x + t[0]);
            int y2 = base->y + t[1];
            if (land && non_combat_move(x2, y2, faction, TRIAD_LAND) && !random(6)) {
                return TRIAD_LAND;
            }
        }
        if (sea) {
            return TRIAD_SEA;
        }
    } else {
        if (build_ships && sea && (!land || (*current_turn > 50 && !random(3)))) {
            return TRIAD_SEA;
        } else if (land) {
            return TRIAD_LAND;
        }
    }
    return -1;
}

int select_combat(int id, bool sea_base, bool build_ships, int probes, int def_land) {
    BASE* base = &tx_bases[id];
    int faction = base->faction_id;
    Faction* f = &tx_factions[faction];
    bool reserve = base->mineral_surplus >= base->mineral_intake/2;

    if (has_weapon(faction, WPN_PROBE_TEAM) && (!random(probes ? 6 : 3) || !reserve)) {
        return find_proto(id, (build_ships ? TRIAD_SEA : TRIAD_LAND), WMODE_INFOWAR, DEF);

    } else if (has_chassis(faction, CHS_NEEDLEJET) && f->SE_police >= -3 && !random(3)) {
        return find_proto(id, TRIAD_AIR, COMBAT, ATT);

    } else if (build_ships && (sea_base || (!def_land && !random(3)))) {
        return find_proto(id, TRIAD_SEA, (!random(3) ? WMODE_TRANSPORT : COMBAT), ATT);
    }
    return find_proto(id, TRIAD_LAND, COMBAT, (!random(4) ? DEF : ATT));
}

int faction_might(int faction) {
    Faction* f = &tx_factions[faction];
    return max(1, f->mil_strength_1 + f->mil_strength_2 + f->pop_total) * (is_human(faction) ? 2 : 1);
}

double expansion_ratio(Faction* f) {
    return f->current_num_bases / (pow(*map_half_x * *map_axis_y, 0.4) *
        min(1.0, max(0.4, *map_area_sq_root / 56.0)) * conf.expansion_factor);
}

int select_prod(int id) {
    BASE* base = &tx_bases[id];
    int faction = base->faction_id;
    int minerals = base->mineral_surplus;
    Faction* f = &tx_factions[faction];
    AIPlans* p = &plans[faction];

    int might = faction_might(faction);
    int transports = 0;
    int defenders = 0;
    int crawlers = 0;
    int formers = 0;
    int probes = 0;
    int pods = 0;
    int enemymask = 1;
    int enemyrange = 40;
    double enemymil = 0;

    for (int i=1; i<8; i++) {
        if (i==faction || ~f->diplo_status[i] & DIPLO_COMMLINK)
            continue;
        double mil = min(5.0, 1.0 * faction_might(i) / might);
        if (f->diplo_status[i] & DIPLO_VENDETTA) {
            enemymask |= (1 << i);
            enemymil = max(enemymil, 1.0 * mil);
        } else if (~f->diplo_status[i] & DIPLO_PACT) {
            enemymil = max(enemymil, 0.3 * mil);
        }
    }
    for (int i=0; i<*total_num_bases; i++) {
        BASE* b = &tx_bases[i];
        if ((1 << b->faction_id) & enemymask) {
            int range = map_range(base->x, base->y, b->x, b->y);
            MAP* sq1 = mapsq(base->x, base->y);
            MAP* sq2 = mapsq(b->x, b->y);
            if (sq1 && sq2 && sq1->region != sq2->region) {
                range = range*3/2;
            }
            enemyrange = min(enemyrange, range);
        }
    }
    for (int i=0; i<*total_num_vehicles; i++) {
        VEH* veh = &tx_vehicles[i];
        UNIT* u = &tx_units[veh->proto_id];
        if (veh->faction_id != faction)
            continue;
        if (veh->home_base_id == id) {
            if (u->weapon_type == WPN_TERRAFORMING_UNIT)
                formers++;
            else if (u->weapon_type == WPN_COLONY_MODULE)
                pods++;
            else if (u->weapon_type == WPN_PROBE_TEAM)
                probes++;
            else if (u->weapon_type == WPN_SUPPLY_TRANSPORT)
                crawlers += (veh->move_status == ORDER_CONVOY ? 1 : 5);
            else if (u->weapon_type == WPN_TROOP_TRANSPORT)
                transports++;
        }
        int range = map_range(base->x, base->y, veh->x, veh->y);
        if (range <= 1) {
            if (u->weapon_type <= WPN_PSI_ATTACK && unit_triad(veh->proto_id) == TRIAD_LAND) {
                defenders++;
            }
        }
    }
    int reserve = max(2, base->mineral_intake / 2);
    double base_ratio = expansion_ratio(f);
    bool has_formers = has_weapon(faction, WPN_TERRAFORMING_UNIT);
    bool has_supply = has_weapon(faction, WPN_SUPPLY_TRANSPORT);
    bool build_ships = can_build_ships(id);
    bool build_pods = ~*game_rules & RULES_SCN_NO_COLONY_PODS
        && (base->pop_size > 1 || base->nutrient_surplus > 1)
        && pods < 2 && *total_num_bases < 500 && base_ratio < 1.0;
    bool sea_base = is_sea_base(id);
    f->thinker_enemy_range = (enemyrange + 9 * f->thinker_enemy_range)/10;

    double w1 = min(1.0, max(0.5, 1.0 * minerals / p->proj_limit));
    double w2 = 2.0 * enemymil / (f->thinker_enemy_range * 0.1 + 0.1) + 0.5 * p->enemy_bases
        + min(1.0, base_ratio) * (f->AI_fight * 0.2 + 0.8);
    double threat = 1 - (1 / (1 + max(0.0, w1 * w2)));
    int def_target = (*current_turn < 50 && !sea_base && !random(3) ? 2 : 1);

    debug("select_prod %d %d %2d %2d | %d %d %d %d %d %d | %2d %2d %2d | %2d %.4f %.4f %.4f %.4f\n",
    *current_turn, faction, base->x, base->y,
    defenders, formers, pods, probes, crawlers, build_pods,
    minerals, reserve, p->proj_limit, enemyrange, enemymil, w1, w2, threat);

    if (defenders < def_target && minerals > 2) {
        if (def_target < 2 && (*current_turn > 30 || enemyrange < 15))
            return find_proto(id, TRIAD_LAND, COMBAT, DEF);
        else
            return BSC_SCOUT_PATROL;
    } else if (!conf.auto_relocate_hq && relocate_hq(id)) {
        return -FAC_HEADQUARTERS;
    } else if (minerals > reserve && random(100) < (int)(100.0 * threat)) {
        return select_combat(id, sea_base, build_ships, probes, (defenders < 2 && enemyrange < 12));
    } else if (has_formers && formers < (base->pop_size < (sea_base ? 4 : 3) ? 1 : 2)
    && (need_formers(base->x, base->y, faction) || (!formers && id & 1))) {
        if (base->mineral_surplus >= 8 && has_chassis(faction, CHS_GRAVSHIP)) {
            int unit = find_proto(id, TRIAD_AIR, WMODE_TERRAFORMER, DEF);
            if (unit_triad(unit) == TRIAD_AIR) {
                return unit;
            }
        }
        if (sea_base || (build_ships && formers == 1 && !random(3)))
            return find_proto(id, TRIAD_SEA, WMODE_TERRAFORMER, DEF);
        else
            return find_proto(id, TRIAD_LAND, WMODE_TERRAFORMER, DEF);
    } else {
        int crawl_target = 1 + min(base->pop_size/3,
            (base->mineral_surplus >= p->proj_limit ? 2 : 1));
        if (has_supply && !sea_base && crawlers < crawl_target) {
            return find_proto(id, TRIAD_LAND, WMODE_CONVOY, DEF);
        } else if (build_ships && !transports && needferry.count({base->x, base->y})) {
            return find_proto(id, TRIAD_SEA, WMODE_TRANSPORT, DEF);
        } else if (build_pods && !can_build(id, FAC_RECYCLING_TANKS)) {
        	// do not build colony in base size 1
        	if (base->pop_size >= 2)
			{
				int tr = select_colony(id, pods, build_ships);
				if (tr == TRIAD_LAND || tr == TRIAD_SEA) {
					return find_proto(id, tr, WMODE_COLONIST, DEF);
				}
			}
        }
        return find_facility(id);
    }
}

int soc_effect(int val, bool immune, bool penalty) {
    if (immune)
        return max(0, val);
    if (penalty)
        return (val < 0 ? val*2 : val);
    return val;
}

int social_score(int faction, int sf, int sm2, int pop_boom, int range, int immunity, int impunity, int penalty) {
    enum {ECO, EFF, SUP, TAL, MOR, POL, GRW, PLA, PRO, IND, RES};
    Faction* f = &tx_factions[faction];
    MetaFaction* m = &tx_metafactions[faction];
    R_Social* s = &tx_social[sf];
    double bw = min(1.0, f->current_num_bases / min(40.0, *map_area_sq_root * 0.5));
    int morale = (has_project(faction, FAC_COMMAND_NEXUS) ? 2 : 0)
        + (has_project(faction, FAC_CYBORG_FACTORY) ? 2 : 0);
    int pr_sf = m->soc_priority_category;
    int pr_sm = m->soc_priority_model;
    int sm1 = (&f->SE_Politics)[sf];
    int sc = 0;
    int vals[11];
    memcpy(vals, &f->SE_economy, sizeof(vals));

    if (sm1 != sm2) {
        for (int i=0; i<11; i++) {
            bool im1 = (1 << i) & immunity || (1 << (sf*4 + sm1)) & impunity;
            bool im2 = (1 << i) & immunity || (1 << (sf*4 + sm2)) & impunity;
            bool pn1 = (1 << (sf*4 + sm1)) & penalty;
            bool pn2 = (1 << (sf*4 + sm2)) & penalty;
            vals[i] -= soc_effect(s->effects[sm1][i], im1, pn1);
            vals[i] += soc_effect(s->effects[sm2][i], im2, pn2);
        }
    }
    if (pr_sf >= 0 && pr_sm >= 0) {
        if ((sf != pr_sf && (&f->SE_Politics)[pr_sf] == pr_sm) || (sf == pr_sf && sm2 == pr_sm)) {
            sc += 14;
        }
    }
    if (vals[ECO] >= 2)
        sc += (vals[ECO] >= 4 ? 16 : 12);
    if (vals[EFF] < -2)
        sc -= (vals[EFF] < -3 ? 20 : 14);
    if (vals[SUP] < -3)
        sc -= 10;
    if (vals[SUP] < 0)
        sc -= (int)((1 - bw) * 10);
    if (vals[MOR] >= 1 && vals[MOR] + morale >= 4)
        sc += 10;
    if (vals[PRO] >= 3 && !has_project(faction, FAC_HUNTER_SEEKER_ALGO))
        sc += 4 * max(0, 4 - range/8);

    sc += max(2, 2 + 4*f->AI_wealth + 3*f->AI_tech - f->AI_fight)
        * min(5, max(-3, vals[ECO]));
    sc += max(2, 2*(f->AI_wealth + f->AI_tech) - f->AI_fight + (int)(4*bw))
        * (min(6, vals[EFF]) + (vals[EFF] >= 3 ? 2 : 0));
    sc += max(3, 3 + 2*f->AI_power + 2*f->AI_fight)
        * min(3, max(-4, vals[SUP]));
    sc += max(2, 4 - range/8 + 2*f->AI_power + 2*f->AI_fight)
        * min(4, max(-4, vals[MOR]));
    if (!has_project(faction, FAC_TELEPATHIC_MATRIX)) {
        sc += (vals[POL] < 0 ? 2 : 4) * min(3, max(-5, vals[POL]));
        if (vals[POL] < -2) {
            sc -= (vals[POL] < -3 ? 4 : 2) * max(0, 4 - range/8)
                * (has_chassis(faction, CHS_NEEDLEJET) ? 2 : 1);
        }
        if (has_project(faction, FAC_LONGEVITY_VACCINE)) {
            sc += (sf == 1 && sm2 == 2 ? 10 : 0);
            sc += (sf == 1 && (sm2 == 0 || sm2 == 3) ? 5 : 0);
        }
    }
    if (!has_project(faction, FAC_CLONING_VATS)) {
        if (pop_boom && vals[GRW] >= 4)
            sc += 20;
        if (vals[GRW] < -2)
            sc -= 10;
        sc += (pop_boom ? 6 : 3) * min(6, max(-3, vals[GRW]));
    }
    sc += max(2, (f->SE_planet_base > 0 ? 5 : 2) + m->rule_psi/10
        + (has_project(faction, FAC_MANIFOLD_HARMONICS) ? 6 : 0)) * min(3, max(-3, vals[PLA]));
    sc += max(2, 4 - range/8 + 2*f->AI_power + 2*f->AI_fight) * min(3, max(-2, vals[PRO]));
    sc += 8 * min(8 - *diff_level, max(-3, vals[IND]));
    sc += max(2, 3 + 4*f->AI_tech + 2*(f->AI_wealth - f->AI_fight))
        * min(5, max(-5, vals[RES]));

    debug("social_score %d %d %d %d %s\n", faction, sf, sm2, sc, s->soc_name[sm2]);
    return sc;
}

HOOK_API int social_ai(int faction, int v1, int v2, int v3, int v4, int v5) {
    Faction* f = &tx_factions[faction];
    MetaFaction* m = &tx_metafactions[faction];
    R_Social* s = tx_social;
    int range = (int)(f->thinker_enemy_range / (1.0 + 0.1 * min(4, plans[faction].enemy_bases)));
    int pop_boom = 0;
    int want_pop = 0;
    int pop_total = 0;
    int immunity = 0;
    int impunity = 0;
    int penalty = 0;

    if (!conf.social_ai || !ai_enabled(faction)) {
        return tx_social_ai(faction, v1, v2, v3, v4, v5);
    }
    if (f->SE_upheaval_cost_paid > 0) {
        tx_social_set(faction);
        return 0;
    }
    assert(!memcmp(&f->SE_Politics, &f->SE_Politics_pending, 16));
    for (int i=0; i<m->faction_bonus_count; i++) {
        if (m->faction_bonus_id[i] == FCB_IMMUNITY)
            immunity |= (1 << m->faction_bonus_val1[i]);
        if (m->faction_bonus_id[i] == FCB_IMPUNITY)
            impunity |= (1 << (m->faction_bonus_val1[i] * 4 + m->faction_bonus_val2[i]));
        if (m->faction_bonus_id[i] == FCB_PENALTY)
            penalty |= (1 << (m->faction_bonus_val1[i] * 4 + m->faction_bonus_val2[i]));
    }
    if (has_project(faction, FAC_NETWORK_BACKBONE)) {
        /* Cybernetic */
        impunity |= (1 << (4*3 + 1));
    }
    if (has_project(faction, FAC_CLONING_VATS)) {
        /* Power & Thought Control */
        impunity |= (1 << (4*2 + 1)) | (1 << (4*3 + 3));
    } else if (has_tech(faction, tx_facility[FAC_CHILDREN_CRECHE].preq_tech)) {
        for (int i=0; i<*total_num_bases; i++) {
            BASE* b = &tx_bases[i];
            if (b->faction_id == faction) {
                want_pop += (tx_pop_goal(i) - b->pop_size)
                    * (b->nutrient_surplus > 1 && has_facility(i, FAC_CHILDREN_CRECHE) ? 4 : 1);
                pop_total += 4 * b->pop_size;
            }
        }
        if (pop_total > 0) {
            pop_boom = ((f->SE_growth < 4 ? 4 : 8) * want_pop) >= pop_total;
        }
    }
    debug("social_params %d %d %8s pop_boom: %d want_pop: %3d pop_total: %3d range: %2d "\
        "immunity: %04x impunity: %04x penalty: %04x\n", *current_turn, faction, m->filename,
        pop_boom, want_pop, pop_total, range, immunity, impunity, penalty);
    int score_diff = 1 + random(6);
    int sf = -1;
    int sm2 = -1;

    for (int i=0; i<4; i++) {
        int sm1 = (&f->SE_Politics)[i];
        int sc1 = social_score(faction, i, sm1, pop_boom, range, immunity, impunity, penalty);

        for (int j=0; j<4; j++) {
            if (j == sm1 || !has_tech(faction, s[i].soc_preq_tech[j]) ||
            (i == m->soc_opposition_category && j == m->soc_opposition_model))
                continue;
            int sc2 = social_score(faction, i, j, pop_boom, range, immunity, impunity, penalty);
            if (sc2 - sc1 > score_diff) {
                sf = i;
                sm2 = j;
                score_diff = sc2 - sc1;
            }
        }
    }
    int cost = (m->rule_flags & FACT_ALIEN ? 36 : 24);
    if (sf >= 0 && f->energy_credits > cost) {
        int sm1 = (&f->SE_Politics)[sf];
        (&f->SE_Politics_pending)[sf] = sm2;
        f->energy_credits -= cost;
        f->SE_upheaval_cost_paid += cost;
        debug("social_change %d %d %8s cost: %2d score_diff: %2d %s -> %s\n",
            *current_turn, faction, m->filename,
            cost, score_diff, s[sf].soc_name[sm1], s[sf].soc_name[sm2]);
    }
    tx_social_set(faction);
    tx_consider_designs(faction);
    return 0;
}

