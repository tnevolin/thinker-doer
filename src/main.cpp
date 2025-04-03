
/*
 * Thinker - AI improvement mod for Sid Meier's Alpha Centauri.
 * https://github.com/induktio/thinker/
 *
 * Thinker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) version 3 of the GPL.
 *
 * Thinker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Thinker.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "main.h"
#include "lib/ini.h"

FILE* debug_log = NULL;
Config conf;
AIPlans plans[MaxPlayerNum];
set_str_t movedlabels;
map_str_t musiclabels;


int option_handler(void* user, const char* section, const char* name, const char* value) {
    #define MATCH(n) strcmp(name, n) == 0
    char buf[INI_MAX_LINE] = {};
    Config* cf = (Config*)user;
    strncpy(buf, value, INI_MAX_LINE);
    buf[INI_MAX_LINE-1] = '\0';

    if (strcmp(section, "thinker") != 0 && strcmp(section, "wtp") != 0) {
        return opt_handle_error(section, name);
    } else if (MATCH("DirectDraw")) {
        cf->directdraw = atoi(value);
    } else if (MATCH("DisableOpeningMovie")) {
        cf->disable_opening_movie = atoi(value);
    } else if (MATCH("video_mode")) {
        cf->video_mode = clamp(atoi(value), 0, 2);
    } else if (MATCH("window_width")) {
        cf->window_width = atoi(value);
    } else if (MATCH("window_height")) {
        cf->window_height = atoi(value);
    } else if (MATCH("smac_only")) {
        cf->smac_only = atoi(value);
    } else if (MATCH("smooth_scrolling")) {
        cf->smooth_scrolling = atoi(value);
    } else if (MATCH("scroll_area")) {
        cf->scroll_area = max(0, atoi(value));
    } else if (MATCH("auto_minimise")) {
        cf->auto_minimise = atoi(value);
    } else if (MATCH("render_base_info")) {
        cf->render_base_info = atoi(value);
    } else if (MATCH("render_high_detail")) {
        cf->render_high_detail = atoi(value);
    } else if (MATCH("autosave_interval")) {
        cf->autosave_interval = atoi(value);
    } else if (MATCH("warn_on_former_replace")) {
        cf->warn_on_former_replace = atoi(value);
    } else if (MATCH("manage_player_bases")) {
        cf->manage_player_bases = atoi(value);
    } else if (MATCH("manage_player_units")) {
        cf->manage_player_units = atoi(value);
    } else if (MATCH("render_probe_labels")) {
        cf->render_probe_labels = atoi(value);
    } else if (MATCH("foreign_treaty_popup")) {
        cf->foreign_treaty_popup = atoi(value);
    } else if (MATCH("editor_free_units")) {
        cf->editor_free_units = atoi(value);
    } else if (MATCH("new_base_names")) {
        cf->new_base_names = atoi(value);
    } else if (MATCH("new_unit_names")) {
        cf->new_unit_names = atoi(value);
    } else if (MATCH("player_colony_pods")) {
        cf->player_colony_pods = atoi(value);
    } else if (MATCH("computer_colony_pods")) {
        cf->computer_colony_pods = atoi(value);
    } else if (MATCH("player_formers")) {
        cf->player_formers = atoi(value);
    } else if (MATCH("computer_formers")) {
        cf->computer_formers = atoi(value);
    } else if (MATCH("player_satellites")) {
        opt_list_parse(cf->player_satellites, buf, 3, 0);
    } else if (MATCH("computer_satellites")) {
        opt_list_parse(cf->computer_satellites, buf, 3, 0);
    } else if (MATCH("design_units")) {
        cf->design_units = atoi(value);
    } else if (MATCH("factions_enabled")) {
        cf->factions_enabled = atoi(value);
    } else if (MATCH("social_ai")) {
        cf->social_ai = atoi(value);
    } else if (MATCH("social_ai_bias")) {
        cf->social_ai_bias = clamp(atoi(value), 0, 1000);
    } else if (MATCH("tech_balance")) {
        cf->tech_balance = atoi(value);
    } else if (MATCH("base_hurry")) {
        cf->base_hurry = atoi(value);
    } else if (MATCH("base_spacing")) {
        cf->base_spacing = clamp(atoi(value), 2, 8);
    } else if (MATCH("base_nearby_limit")) {
        cf->base_nearby_limit = atoi(value);
    } else if (MATCH("expansion_limit")) {
        cf->expansion_limit = atoi(value);
    } else if (MATCH("expansion_autoscale")) {
        cf->expansion_autoscale = atoi(value);
    } else if (MATCH("limit_project_start")) {
        cf->limit_project_start = atoi(value);
    } else if (MATCH("max_satellites")) {
        cf->max_satellites = max(0, atoi(value));
    } else if (MATCH("new_world_builder")) {
        cf->new_world_builder = atoi(value);
    } else if (MATCH("world_continents")) {
        cf->world_continents = atoi(value);
    } else if (MATCH("world_polar_caps")) {
        cf->world_polar_caps = atoi(value);
    } else if (MATCH("world_hills_mod")) {
        cf->world_hills_mod = clamp(atoi(value), 0, 100);
    } else if (MATCH("world_ocean_mod")) {
        cf->world_ocean_mod = clamp(atoi(value), 0, 100);
    } else if (MATCH("world_islands_mod")) {
        cf->world_islands_mod = atoi(value);
    } else if (MATCH("world_mirror_x")) {
        cf->world_mirror_x = atoi(value);
    } else if (MATCH("world_mirror_y")) {
        cf->world_mirror_y = atoi(value);
    } else if (MATCH("modified_landmarks")) {
        cf->modified_landmarks = atoi(value);
    } else if (MATCH("world_sea_levels")) {
        opt_list_parse(cf->world_sea_levels, buf, 3, 0);
    } else if (MATCH("time_warp_mod")) {
        cf->time_warp_mod = atoi(value);
    } else if (MATCH("time_warp_techs")) {
        cf->time_warp_techs = atoi(value);
    } else if (MATCH("time_warp_projects")) {
        cf->time_warp_projects = atoi(value);
    } else if (MATCH("time_warp_start_turn")) {
        cf->time_warp_start_turn = clamp(atoi(value), 0, 500);
    } else if (MATCH("faction_placement")) {
        cf->faction_placement = atoi(value);
    } else if (MATCH("nutrient_bonus")) {
        cf->nutrient_bonus = atoi(value);
    } else if (MATCH("rare_supply_pods")) {
        cf->rare_supply_pods = atoi(value);
    } else if (MATCH("simple_cost_factor")) {
        cf->simple_cost_factor = atoi(value);
    } else if (MATCH("revised_tech_cost")) {
        cf->revised_tech_cost = atoi(value);
    } else if (MATCH("tech_stagnate_rate")) {
        cf->tech_stagnate_rate = max(1, atoi(value));
    } else if (MATCH("fast_fungus_movement")) {
        cf->fast_fungus_movement = atoi(value);
    } else if (MATCH("magtube_movement_rate")) {
        cf->magtube_movement_rate = atoi(value);
    } else if (MATCH("chopper_attack_rate")) {
        cf->chopper_attack_rate = atoi(value);
    } else if (MATCH("base_psych")) {
        cf->base_psych = atoi(value);
    } else if (MATCH("nerve_staple")) {
        cf->nerve_staple = atoi(value);
    } else if (MATCH("nerve_staple_mod")) {
        cf->nerve_staple_mod = atoi(value);
    } else if (MATCH("delay_drone_riots")) {
        cf->delay_drone_riots = atoi(value);
    } else if (MATCH("skip_drone_revolts")) {
        cf->skip_drone_revolts = atoi(value);
    } else if (MATCH("activate_skipped_units")) {
        cf->activate_skipped_units = atoi(value);
    } else if (MATCH("counter_espionage")) {
        cf->counter_espionage = atoi(value);
    } else if (MATCH("ignore_reactor_power")) {
        cf->ignore_reactor_power = atoi(value);
    } else if (MATCH("long_range_artillery")) {
        cf->long_range_artillery = atoi(value);
    } else if (MATCH("modify_upgrade_cost")) {
        cf->modify_upgrade_cost = atoi(value);
    } else if (MATCH("modify_unit_support")) {
        cf->modify_unit_support = atoi(value);
    } else if (MATCH("skip_default_balance")) {
        cf->skip_default_balance = atoi(value);
    } else if (MATCH("early_research_start")) {
        cf->early_research_start = atoi(value);
    } else if (MATCH("facility_capture_fix")) {
        cf->facility_capture_fix = atoi(value);
    } else if (MATCH("territory_border_fix")) {
        cf->territory_border_fix = atoi(value);
    } else if (MATCH("auto_relocate_hq")) {
        cf->auto_relocate_hq = atoi(value);
    } else if (MATCH("simple_hurry_cost")) {
        cf->simple_hurry_cost = atoi(value);
    } else if (MATCH("eco_damage_fix")) {
        cf->eco_damage_fix = atoi(value);
    } else if (MATCH("clean_minerals")) {
        cf->clean_minerals = clamp(atoi(value), 0, 127);
    } else if (MATCH("biology_lab_bonus")) {
        cf->biology_lab_bonus = clamp(atoi(value), 0, 127);
    } else if (MATCH("spawn_fungal_towers")) {
        cf->spawn_fungal_towers = atoi(value);
    } else if (MATCH("spawn_spore_launchers")) {
        cf->spawn_spore_launchers = atoi(value);
    } else if (MATCH("spawn_sealurks")) {
        cf->spawn_sealurks = atoi(value);
    } else if (MATCH("spawn_battle_ogres")) {
        cf->spawn_battle_ogres = atoi(value);
    } else if (MATCH("planetpearls")) {
        cf->planetpearls = atoi(value);
    } else if (MATCH("event_perihelion")) {
        cf->event_perihelion = atoi(value);
    } else if (MATCH("event_sunspots")) {
        cf->event_sunspots = clamp(atoi(value), 0, 100);
    } else if (MATCH("event_market_crash")) {
        cf->event_market_crash = atoi(value);
    } else if (MATCH("aquatic_bonus_minerals")) {
        cf->aquatic_bonus_minerals = atoi(value);
    } else if (MATCH("alien_guaranteed_techs")) {
        cf->alien_guaranteed_techs = atoi(value);
    } else if (MATCH("alien_early_start")) {
        cf->alien_early_start = atoi(value);
    } else if (MATCH("cult_early_start")) {
        cf->cult_early_start = atoi(value);
    } else if (MATCH("normal_elite_moves")) {
        cf->normal_elite_moves = atoi(value);
    } else if (MATCH("native_elite_moves")) {
        cf->native_elite_moves = atoi(value);
    } else if (MATCH("native_weak_until_turn")) {
        cf->native_weak_until_turn = clamp(atoi(value), 0, 127);
    } else if (MATCH("native_lifecycle_levels")) {
        opt_list_parse(cf->native_lifecycle_levels, buf, 6, 0);
    } else if (MATCH("facility_defense_bonus")) {
        opt_list_parse(cf->facility_defense_bonus, buf, 4, 0);
    } else if (MATCH("neural_amplifier_bonus")) {
        cf->neural_amplifier_bonus = clamp(atoi(value), 0, 1000);
    } else if (MATCH("dream_twister_bonus")) {
        cf->dream_twister_bonus = clamp(atoi(value), 0, 1000);
    } else if (MATCH("fungal_tower_bonus")) {
        cf->fungal_tower_bonus = clamp(atoi(value), 0, 1000);
    } else if (MATCH("planet_defense_bonus")) {
        cf->planet_defense_bonus = atoi(value);
    } else if (MATCH("sensor_defense_ocean")) {
        cf->sensor_defense_ocean = atoi(value);
    } else if (MATCH("collateral_damage_value")) {
        cf->collateral_damage_value = clamp(atoi(value), 0, 127);
    } else if (MATCH("cost_factor")) {
        opt_list_parse(CostRatios, buf, MaxDiffNum, 1);
    } else if (MATCH("tech_cost_factor")) {
        opt_list_parse(cf->tech_cost_factor, buf, MaxDiffNum, 1);
    } else if (MATCH("content_pop_player")) {
        opt_list_parse(cf->content_pop_player, buf, MaxDiffNum, 0);
    } else if (MATCH("content_pop_computer")) {
        opt_list_parse(cf->content_pop_computer, buf, MaxDiffNum, 0);
    } else if (MATCH("repair_minimal")) {
        cf->repair_minimal = clamp(atoi(value), 0, 10);
    } else if (MATCH("repair_fungus")) {
        cf->repair_fungus = clamp(atoi(value), 0, 10);
    } else if (MATCH("repair_friendly")) {
        cf->repair_friendly = clamp(atoi(value), 0, 10);
    } else if (MATCH("repair_airbase")) {
        cf->repair_airbase = clamp(atoi(value), 0, 10);
    } else if (MATCH("repair_bunker")) {
        cf->repair_bunker = clamp(atoi(value), 0, 10);
    } else if (MATCH("repair_base")) {
        cf->repair_base = clamp(atoi(value), 0, 10);
    } else if (MATCH("repair_base_native")) {
        cf->repair_base_native = clamp(atoi(value), 0, 10);
    } else if (MATCH("repair_base_facility")) {
        cf->repair_base_facility = clamp(atoi(value), 0, 10);
    } else if (MATCH("repair_nano_factory")) {
        cf->repair_nano_factory = clamp(atoi(value), 0, 10);
    } else if (MATCH("repair_battle_ogre")) {
        cf->repair_battle_ogre = clamp(atoi(value), 0, 10);
    } else if (MATCH("minimal_popups")) {
        if (DEBUG) {
            cf->minimal_popups = atoi(value);
            cf->debug_verbose = !atoi(value);
        }
    } else if (MATCH("skip_faction")) {
        if (atoi(value) > 0) {
            cf->skip_random_factions |= 1 << (atoi(value) - 1);
        }
    } else if (MATCH("crater")) {
        cf->landmarks.crater = max(0, atoi(value));
    } else if (MATCH("volcano")) {
        cf->landmarks.volcano = max(0, atoi(value));
    } else if (MATCH("jungle")) {
        cf->landmarks.jungle = max(0, atoi(value));
    } else if (MATCH("uranium")) {
        cf->landmarks.uranium = max(0, atoi(value));
    } else if (MATCH("sargasso")) {
        cf->landmarks.sargasso = max(0, atoi(value));
    } else if (MATCH("ruins")) {
        cf->landmarks.ruins = max(0, atoi(value));
    } else if (MATCH("dunes")) {
        cf->landmarks.dunes = max(0, atoi(value));
    } else if (MATCH("fresh")) {
        cf->landmarks.fresh = max(0, atoi(value));
    } else if (MATCH("mesa")) {
        cf->landmarks.mesa = max(0, atoi(value));
    } else if (MATCH("canyon")) {
        cf->landmarks.canyon = max(0, atoi(value));
    } else if (MATCH("geothermal")) {
        cf->landmarks.geothermal = max(0, atoi(value));
    } else if (MATCH("ridge")) {
        cf->landmarks.ridge = max(0, atoi(value));
    } else if (MATCH("borehole")) {
        cf->landmarks.borehole = max(0, atoi(value));
    } else if (MATCH("nexus")) {
        cf->landmarks.nexus = max(0, atoi(value));
    } else if (MATCH("unity")) {
        cf->landmarks.unity = max(0, atoi(value));
    } else if (MATCH("fossil")) {
        cf->landmarks.fossil = max(0, atoi(value));
    } else if (MATCH("label_pop_size")) {
        parse_format_args(label_pop_size, value, 4, StrBufLen);
    } else if (MATCH("label_pop_boom")) {
        parse_format_args(label_pop_boom, value, 0, StrBufLen);
    } else if (MATCH("label_nerve_staple")) {
        parse_format_args(label_nerve_staple, value, 1, StrBufLen);
    } else if (MATCH("label_psych_effect")) {
        parse_format_args(label_psych_effect, value, 2, StrBufLen);
    } else if (MATCH("label_captured_base")) {
        parse_format_args(label_captured_base, value, 1, StrBufLen);
    } else if (MATCH("label_stockpile_energy")) {
        parse_format_args(label_stockpile_energy, value, 1, StrBufLen);
    } else if (MATCH("label_sat_nutrient")) {
        parse_format_args(label_sat_nutrient, value, 1, StrBufLen);
    } else if (MATCH("label_sat_mineral")) {
        parse_format_args(label_sat_mineral, value, 1, StrBufLen);
    } else if (MATCH("label_sat_energy")) {
        parse_format_args(label_sat_energy, value, 1, StrBufLen);
    } else if (MATCH("label_eco_damage")) {
        parse_format_args(label_eco_damage, value, 2, StrBufLen);
    } else if (MATCH("label_base_surplus")) {
        parse_format_args(label_base_surplus, value, 3, StrBufLen);
    } else if (MATCH("label_unit_reactor")) {
        int len = strlen(buf);
        int j = 0;
        int k = 0;
        for (int i = 0; i < len && i < StrBufLen && k < 4; i++) {
            bool last = i == len - 1;
            if (buf[i] == ',' || last) {
                strncpy(label_unit_reactor[k], buf+j, i-j+last);
                label_unit_reactor[k][i-j+last] = '\0';
                j = i + 1;
                k++;
            }
        }
    } else if (MATCH("script_label")) {
        char* p = strupr(strstrip(buf));
        debug("script_label %s\n", p);
        movedlabels.insert(p);
    } else if (MATCH("music_label")) {
        char *p, *s, *k, *v;
        if ((p = strtok_r(buf, ",", &s)) != NULL) {
            k = strstrip(p);
            if ((p = strtok_r(NULL, ",", &s)) != NULL) {
                v = strstrip(p);
                if (strlen(k) && strlen(v)) {
                    debug("music_label %s = %s\n", k, v);
                    musiclabels[k] = v;
                }
            }
        }
    }
    // [WTP] configuratoin begin
    else if (MATCH("alternative_weapon_icon_selection_algorithm")) {
        cf->alternative_weapon_icon_selection_algorithm = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("alternative_prototype_cost_formula")) {
        cf->alternative_prototype_cost_formula = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("reactor_cost_factors")) {
		opt_list_parse(cf->reactor_cost_factors, buf, 4, 0);
    }
    else if (MATCH("hurry_minimal_minerals")) {
        cf->hurry_minimal_minerals = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("flat_hurry_cost")) {
        cf->flat_hurry_cost = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("flat_hurry_cost_multiplier_unit")) {
        cf->flat_hurry_cost_multiplier_unit = std::max(1, atoi(value));
    }
    else if (MATCH("flat_hurry_cost_multiplier_facility")) {
        cf->flat_hurry_cost_multiplier_facility = std::max(1, atoi(value));
    }
    else if (MATCH("flat_hurry_cost_multiplier_project")) {
        cf->flat_hurry_cost_multiplier_project = std::max(1, atoi(value));
    }
	else if (MATCH("collateral_damage_defender_reactor")) {
		cf->collateral_damage_defender_reactor = (atoi(value) == 0 ? false : true);
	}
    else if (MATCH("sea_territory_distance_same_as_land")) {
        cf->sea_territory_distance_same_as_land = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("alternative_artillery_damage")) {
        cf->alternative_artillery_damage = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("disable_home_base_morale_effect")) {
        cf->disable_home_base_morale_effect = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("disable_current_base_morale_effect")) {
        cf->disable_current_base_morale_effect = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("default_morale_very_green")) {
        cf->default_morale_very_green = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("cloning_vats_disable_impunities"))
    {
        cf->cloning_vats_disable_impunities = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("cloning_vats_se_growth"))
    {
        cf->cloning_vats_se_growth = std::max(0, std::min(2, atoi(value)));
    }
    else if (MATCH("se_growth_rating_min"))
    {
        cf->se_growth_rating_min = std::min(0, atoi(value));
    }
    else if (MATCH("se_growth_rating_max"))
    {
        cf->se_growth_rating_max = std::max(0, std::min(9, atoi(value)));
    }
    else if (MATCH("pop_boom_requires_golden_age"))
    {
        cf->pop_boom_requires_golden_age = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("pop_boom_requires_growth_rating"))
    {
        cf->pop_boom_requires_growth_rating = atoi(value);
    }
    else if (MATCH("ocean_depth_multiplier"))
    {
        cf->ocean_depth_multiplier = atof(value);
    }
    else if (MATCH("pts_new_base_size_less_average"))
    {
        cf->pts_new_base_size_less_average = std::max(0, atoi(value));
    }
    else if (MATCH("hsa_does_not_kill_probe"))
    {
        cf->hsa_does_not_kill_probe = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("condenser_and_enricher_do_not_multiply_nutrients"))
    {
        cf->condenser_and_enricher_do_not_multiply_nutrients = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("alternative_tech_cost"))
    {
        cf->alternative_tech_cost = (atoi(value) == 0 ? false : true);
    }
//    else if (MATCH("fix_mineral_contribution"))
//    {
//        cf->fix_mineral_contribution = (atoi(value) == 0 ? false : true);
//    }
    else if (MATCH("modified_probe_risks"))
    {
        cf->modified_probe_risks = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("probe_risk_procure_research_data"))
    {
        cf->probe_risk_procure_research_data = std::max(0, atoi(value));
    }
    else if (MATCH("probe_risk_introduce_genetic_plague"))
    {
        cf->probe_risk_introduce_genetic_plague = std::max(0, atoi(value));
    }
    else if (MATCH("sensor_offense"))
    {
        cf->sensor_offense = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("sensor_offense_ocean"))
    {
        cf->sensor_offense_ocean = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("break_treaty_before_fight"))
    {
        cf->break_treaty_before_fight = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("habitation_facility_growth_bonus"))
    {
        cf->habitation_facility_growth_bonus = atoi(value);
    }
    else if (MATCH("unit_upgrade_ignores_movement"))
    {
        cf->unit_upgrade_ignores_movement = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("ai_base_allowed_fungus_rocky"))
    {
        cf->ai_base_allowed_fungus_rocky = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("silent_vendetta_warning"))
    {
        cf->silent_vendetta_warning = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("design_cost_in_rows"))
    {
        cf->design_cost_in_rows = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("carry_over_nutrients"))
    {
        cf->carry_over_nutrients = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("carry_over_minerals"))
    {
        cf->carry_over_minerals = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("subversion_allow_stacked_units"))
    {
        cf->subversion_allow_stacked_units = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("alternative_subversion_and_mind_control"))
    {
        cf->alternative_subversion_and_mind_control = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("alternative_subversion_and_mind_control_scale"))
    {
        cf->alternative_subversion_and_mind_control_scale = std::max(0.0, atof(value));
    }
    else if (MATCH("alternative_subversion_unit_cost_multiplier"))
    {
        cf->alternative_subversion_unit_cost_multiplier = atof(value);
    }
    else if (MATCH("alternative_mind_control_nutrient_cost"))
    {
        cf->alternative_mind_control_nutrient_cost = atof(value);
    }
    else if (MATCH("alternative_mind_control_mineral_cost"))
    {
        cf->alternative_mind_control_mineral_cost = atof(value);
    }
    else if (MATCH("alternative_mind_control_energy_cost"))
    {
        cf->alternative_mind_control_energy_cost = atof(value);
    }
    else if (MATCH("alternative_mind_control_facility_cost_multiplier"))
    {
        cf->alternative_mind_control_facility_cost_multiplier = atof(value);
    }
    else if (MATCH("alternative_mind_control_project_cost_multiplier"))
    {
        cf->alternative_mind_control_project_cost_multiplier = atof(value);
    }
    else if (MATCH("alternative_mind_control_happiness_power_base"))
    {
        cf->alternative_mind_control_happiness_power_base = atof(value);
    }
    else if (MATCH("capture_base_destroys_facilities"))
    {
        cf->capture_base_destroys_facilities = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("alternative_support"))
    {
        cf->alternative_support = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("instant_completion_fixed_minerals"))
    {
        cf->instant_completion_fixed_minerals = std::min(50, std::max(0, atoi(value)));
    }
    else if (MATCH("sensor_indestructible"))
    {
        cf->sensor_indestructible = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("artillery_duel_uses_bonuses"))
    {
        cf->artillery_duel_uses_bonuses = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("probe_combat_uses_bonuses"))
    {
        cf->probe_combat_uses_bonuses = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("disable_vanilla_base_hurry"))
    {
        cf->disable_vanilla_base_hurry = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("science_projects_alternative_labs_bonus"))
    {
        cf->science_projects_alternative_labs_bonus = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("science_projects_supercollider_labs_bonus"))
    {
        cf->science_projects_supercollider_labs_bonus = atoi(value);
    }
    else if (MATCH("science_projects_theoryofeverything_labs_bonus"))
    {
        cf->science_projects_theoryofeverything_labs_bonus = atoi(value);
    }
    else if (MATCH("science_projects_universaltranslator_labs_bonus"))
    {
        cf->science_projects_universaltranslator_labs_bonus = atoi(value);
    }
    else if (MATCH("disengagement_from_stack"))
    {
        cf->disengagement_from_stack = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("eco_damage_alternative_industry_effect_reduction_formula"))
    {
        cf->eco_damage_alternative_industry_effect_reduction_formula = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("pressure_dome_minerals"))
    {
        cf->pressure_dome_minerals = std::max(0, atoi(value));
    }
    else if (MATCH("pressure_dome_recycling_tanks_bonus"))
    {
        cf->pressure_dome_recycling_tanks_bonus = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("tech_trade_likeability"))
    {
        cf->tech_trade_likeability = std::min(0x7F, std::max(0, atoi(value)));
    }
    else if (MATCH("conventional_power_psi_percentage"))
    {
        cf->conventional_power_psi_percentage = atoi(value);
    }
    else if (MATCH("zoc_enabled"))
    {
        cf->zoc_enabled = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("aaa_range"))
    {
        cf->aaa_range = atoi(value);
    }
    else if (MATCH("base_psych_improved"))
    {
        cf->base_psych_improved = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("base_psych_cost"))
    {
        cf->base_psych_cost = atoi(value);
    }
    else if (MATCH("base_psych_remove_drone_soft"))
    {
        cf->base_psych_remove_drone_soft = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("base_psych_specialist_content"))
    {
        cf->base_psych_specialist_content = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("base_psych_economy_conversion_ratio"))
    {
        cf->base_psych_economy_conversion_ratio = atoi(value);
    }
    else if (MATCH("base_psych_facility_extra_power"))
    {
        cf->base_psych_facility_extra_power = atoi(value);
    }
    else if (MATCH("base_psych_police_extra_power"))
    {
        cf->base_psych_police_extra_power = atoi(value);
    }
    else if (MATCH("facility_terraforming_ecodamage_halved"))
    {
        cf->facility_terraforming_ecodamage_halved = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("tree_farm_yield_bonus_forest"))
    {
        opt_list_parse(cf->tree_farm_yield_bonus_forest, buf, 3, 0);
    }
    else if (MATCH("hybrid_forest_yield_bonus_forest"))
    {
        opt_list_parse(cf->hybrid_forest_yield_bonus_forest, buf, 3, 0);
    }
    else if (MATCH("recycling_tanks_mineral_multiplier"))
    {
        cf->recycling_tanks_mineral_multiplier = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("recycling_tanks_mineral_bonus_mining_platform"))
    {
        cf->recycling_tanks_mineral_bonus_mining_platform = atoi(value);
    }
    else if (MATCH("recycling_tanks_mineral_bonus_rocky_mine"))
    {
        cf->recycling_tanks_mineral_bonus_rocky_mine = atoi(value);
    }
    else if (MATCH("genejack_factory_mineral_multiplier"))
    {
        cf->genejack_factory_mineral_multiplier = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("genejack_factory_mineral_bonus_mining_platform"))
    {
        cf->genejack_factory_mineral_bonus_mining_platform = atoi(value);
    }
    else if (MATCH("genejack_factory_mineral_bonus_rocky_mine"))
    {
        cf->genejack_factory_mineral_bonus_rocky_mine = atoi(value);
    }
    else if (MATCH("energy_multipliers_tree_farm"))
    {
        opt_list_parse(cf->energy_multipliers_tree_farm, buf, 3, 0);
    }
    else if (MATCH("energy_multipliers_hybrid_forest"))
    {
        opt_list_parse(cf->energy_multipliers_hybrid_forest, buf, 3, 0);
    }
    else if (MATCH("energy_multipliers_centauri_preserve"))
    {
        opt_list_parse(cf->energy_multipliers_centauri_preserve, buf, 3, 0);
    }
    else if (MATCH("energy_multipliers_temple_of_planet"))
    {
        opt_list_parse(cf->energy_multipliers_temple_of_planet, buf, 3, 0);
    }
    else if (MATCH("echelon_mirror_bonus"))
    {
        opt_list_parse(cf->echelon_mirror_bonus, buf, 2, 0);
    }
    else if (MATCH("echelon_mirror_ecodamage"))
    {
        cf->echelon_mirror_ecodamage = atoi(value);
    }
    else if (MATCH("base_inefficiency_alternative"))
    {
        cf->base_inefficiency_alternative = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("drone_riot_intensifies"))
    {
        cf->drone_riot_intensifies = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("se_morale_excess_combat_bonus"))
    {
        cf->se_morale_excess_combat_bonus = atof(value);
    }
    else if (MATCH("se_police_excess_industry_bonus"))
    {
        cf->se_police_excess_industry_bonus = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("isle_of_deep_offense_bonus"))
    {
        cf->isle_of_deep_offense_bonus = atoi(value);
    }
    else if (MATCH("isle_of_deep_defense_bonus"))
    {
        cf->isle_of_deep_defense_bonus = atoi(value);
    }
    else if (MATCH("facility_yield_bonus_biology_lab"))
    {
        opt_list_parse(cf->facility_yield_bonus_biology_lab, buf, 3, 0);
    }
    else if (MATCH("worker_algorithm_enable_alternative"))
    {
        cf->worker_algorithm_enable_alternative = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("worker_algorithm_energy_weight"))
    {
        cf->worker_algorithm_energy_weight = atof(value);
    }
    else if (MATCH("worker_algorithm_growth_multiplier"))
    {
        cf->worker_algorithm_growth_multiplier = atof(value);
    }
    else if (MATCH("worker_algorithm_minimal_nutrient_surplus"))
    {
        cf->worker_algorithm_minimal_nutrient_surplus = atoi(value);
    }
    else if (MATCH("worker_algorithm_minimal_mineral_surplus"))
    {
        cf->worker_algorithm_minimal_mineral_surplus = atoi(value);
    }
    else if (MATCH("worker_algorithm_minimal_energy_surplus"))
    {
        cf->worker_algorithm_minimal_energy_surplus = atoi(value);
    }
    else if (MATCH("worker_algorithm_nutrient_preference"))
    {
        cf->worker_algorithm_nutrient_preference = atof(value);
    }
    else if (MATCH("worker_algorithm_mineral_preference"))
    {
        cf->worker_algorithm_mineral_preference = atof(value);
    }
    else if (MATCH("worker_algorithm_energy_preference"))
    {
        cf->worker_algorithm_energy_preference = atof(value);
    }
    else if (MATCH("diplomacy_improvement_chance_human_faction_revenge"))
    {
        cf->diplomacy_improvement_chance_human_faction_revenge = atoi(value);
    }
    else if (MATCH("diplomacy_improvement_chance_human_major_atrocity"))
    {
        cf->diplomacy_improvement_chance_human_major_atrocity = atoi(value);
    }
    else if (MATCH("diplomacy_improvement_chance_ai_faction_revenge"))
    {
        cf->diplomacy_improvement_chance_ai_faction_revenge = atoi(value);
    }
    else if (MATCH("diplomacy_improvement_chance_ai_major_atrocity"))
    {
        cf->diplomacy_improvement_chance_ai_major_atrocity = atoi(value);
    }
    else if (MATCH("diplomacy_disable_altering_rainfall_patterns_penalty"))
    {
        cf->diplomacy_disable_altering_rainfall_patterns_penalty = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("diplomacy_aggressiveness_effect_interval"))
    {
        cf->diplomacy_aggressiveness_effect_interval = atoi(value);
    }
    else if (MATCH("diplomacy_se_choice_effect_interval"))
    {
        cf->diplomacy_se_choice_effect_interval = atoi(value);
    }
    else if (MATCH("diplomacy_probe_action_vendetta_global_friction"))
    {
        cf->diplomacy_probe_action_vendetta_global_friction = atoi(value);
    }
    else if (MATCH("bunker_ignores_terrain"))
    {
        cf->bunker_ignores_terrain = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("bunker_bonus_surface"))
    {
        cf->bunker_bonus_surface = atoi(value);
    }
    else if (MATCH("bunker_bonus_aerial"))
    {
        cf->bunker_bonus_aerial = atoi(value);
    }
    else if (MATCH("display_numeric_mood"))
    {
        cf->display_numeric_mood = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("planetary_datalinks_faction_count"))
    {
        cf->planetary_datalinks_faction_count = atoi(value);
    }
    else if (MATCH("ai_useWTPAlgorithms"))
    {
        cf->ai_useWTPAlgorithms = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp_enabled_factions"))
    {
    	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
		{
			cf->wtp_enabled_factions[factionId] = (value[factionId - 1] == '0' ? false : true);
		}
    }
    else if (MATCH("ai_resource_score_nutrient"))
    {
        cf->ai_resource_score_nutrient = atof(value);
    }
    else if (MATCH("ai_resource_score_mineral"))
    {
        cf->ai_resource_score_mineral = atof(value);
    }
    else if (MATCH("ai_resource_score_energy"))
    {
        cf->ai_resource_score_energy = atof(value);
    }
    else if (MATCH("ai_faction_mineral_a0"))
    {
        cf->ai_faction_mineral_a0 = atof(value);
    }
    else if (MATCH("ai_faction_mineral_a1"))
    {
        cf->ai_faction_mineral_a1 = atof(value);
    }
    else if (MATCH("ai_faction_mineral_a2"))
    {
        cf->ai_faction_mineral_a2 = atof(value);
    }
    else if (MATCH("ai_faction_budget_a0"))
    {
        cf->ai_faction_budget_a0 = atof(value);
    }
    else if (MATCH("ai_faction_budget_a1"))
    {
        cf->ai_faction_budget_a1 = atof(value);
    }
    else if (MATCH("ai_faction_budget_a2"))
    {
        cf->ai_faction_budget_a2 = atof(value);
    }
    else if (MATCH("ai_faction_budget_a3"))
    {
        cf->ai_faction_budget_a3 = atof(value);
    }
    else if (MATCH("ai_statistics_base_mineral_intake_sqrt_a"))
    {
        cf->ai_statistics_base_mineral_intake_sqrt_a = atof(value);
    }
    else if (MATCH("ai_statistics_base_mineral_intake_sqrt_b"))
    {
        cf->ai_statistics_base_mineral_intake_sqrt_b = atof(value);
    }
    else if (MATCH("ai_statistics_base_mineral_intake_sqrt_c"))
    {
        cf->ai_statistics_base_mineral_intake_sqrt_c = atof(value);
    }
    else if (MATCH("ai_base_mineral_intake_a"))
    {
        cf->ai_base_mineral_intake_a = atof(value);
    }
    else if (MATCH("ai_base_mineral_intake_b"))
    {
        cf->ai_base_mineral_intake_b = atof(value);
    }
    else if (MATCH("ai_base_mineral_intake_c"))
    {
        cf->ai_base_mineral_intake_c = atof(value);
    }
    else if (MATCH("ai_base_mineral_intake2_a"))
    {
        cf->ai_base_mineral_intake2_a = atof(value);
    }
    else if (MATCH("ai_base_mineral_intake2_b"))
    {
        cf->ai_base_mineral_intake2_b = atof(value);
    }
    else if (MATCH("ai_base_mineral_multiplier_a"))
    {
        cf->ai_base_mineral_multiplier_a = atof(value);
    }
    else if (MATCH("ai_base_mineral_multiplier_b"))
    {
        cf->ai_base_mineral_multiplier_b = atof(value);
    }
    else if (MATCH("ai_base_mineral_multiplier_c"))
    {
        cf->ai_base_mineral_multiplier_c = atof(value);
    }
    else if (MATCH("ai_base_budget_intake_a"))
    {
        cf->ai_base_budget_intake_a = atof(value);
    }
    else if (MATCH("ai_base_budget_intake_b"))
    {
        cf->ai_base_budget_intake_b = atof(value);
    }
    else if (MATCH("ai_base_budget_intake2_a"))
    {
        cf->ai_base_budget_intake2_a = atof(value);
    }
    else if (MATCH("ai_base_budget_intake2_b"))
    {
        cf->ai_base_budget_intake2_b = atof(value);
    }
    else if (MATCH("ai_base_budget_intake2_c"))
    {
        cf->ai_base_budget_intake2_c = atof(value);
    }
    else if (MATCH("ai_base_budget_multiplier_a"))
    {
        cf->ai_base_budget_multiplier_a = atof(value);
    }
    else if (MATCH("ai_base_budget_multiplier_b"))
    {
        cf->ai_base_budget_multiplier_b = atof(value);
    }
    else if (MATCH("ai_base_budget_multiplier_c"))
    {
        cf->ai_base_budget_multiplier_c = atof(value);
    }
    else if (MATCH("ai_worker_count_a"))
    {
        cf->ai_worker_count_a = atof(value);
    }
    else if (MATCH("ai_worker_count_b"))
    {
        cf->ai_worker_count_b = atof(value);
    }
    else if (MATCH("ai_worker_count_c"))
    {
        cf->ai_worker_count_c = atof(value);
    }
    else if (MATCH("ai_base_size_a"))
    {
        cf->ai_base_size_a = atof(value);
    }
    else if (MATCH("ai_base_size_b"))
    {
        cf->ai_base_size_b = atof(value);
    }
    else if (MATCH("ai_base_size_c"))
    {
        cf->ai_base_size_c = atof(value);
    }
    else if (MATCH("ai_statistics_base_growth_a"))
    {
        cf->ai_statistics_base_growth_a = atof(value);
    }
    else if (MATCH("ai_statistics_base_growth_b"))
    {
        cf->ai_statistics_base_growth_b = atof(value);
    }
    else if (MATCH("ai_citizen_income"))
    {
        cf->ai_citizen_income = atof(value);
    }
    else if (MATCH("ai_production_priority_coefficient"))
    {
        cf->ai_production_priority_coefficient = atof(value);
    }
    else if (MATCH("ai_development_scale_min"))
    {
        cf->ai_development_scale_min = atof(value);
    }
    else if (MATCH("ai_development_scale_max"))
    {
        cf->ai_development_scale_max = atof(value);
    }
    else if (MATCH("ai_development_scale_max_turn"))
    {
        cf->ai_development_scale_max_turn = atof(value);
    }
    else if (MATCH("ai_production_vanilla_priority_unit"))
    {
        cf->ai_production_vanilla_priority_unit = atof(value);
    }
    else if (MATCH("ai_production_vanilla_priority_facility"))
    {
        cf->ai_production_vanilla_priority_facility = atof(value);
    }
    else if (MATCH("ai_production_vanilla_priority_project"))
    {
        cf->ai_production_vanilla_priority_project = atof(value);
    }
    else if (MATCH("ai_production_project_mineral_surplus_fraction"))
    {
        cf->ai_production_project_mineral_surplus_fraction = atof(value);
    }
    else if (MATCH("ai_production_priority_facility_psych"))
    {
        cf->ai_production_priority_facility_psych = atof(value);
    }
    else if (MATCH("ai_production_threat_coefficient_human"))
    {
        cf->ai_production_threat_coefficient_human = atof(value);
    }
    else if (MATCH("ai_production_threat_coefficient_vendetta"))
    {
        cf->ai_production_threat_coefficient_vendetta = atof(value);
    }
    else if (MATCH("ai_production_threat_coefficient_pact"))
    {
        cf->ai_production_threat_coefficient_pact = atof(value);
    }
    else if (MATCH("ai_production_threat_coefficient_treaty"))
    {
        cf->ai_production_threat_coefficient_treaty = atof(value);
    }
    else if (MATCH("ai_production_threat_coefficient_other"))
    {
        cf->ai_production_threat_coefficient_other = atof(value);
    }
    else if (MATCH("ai_production_pod_bonus"))
    {
        cf->ai_production_pod_bonus = atof(value);
    }
    else if (MATCH("ai_production_pod_popping_priority"))
    {
        cf->ai_production_pod_popping_priority = atof(value);
    }
    else if (MATCH("ai_production_expansion_priority"))
    {
        cf->ai_production_expansion_priority = atof(value);
    }
    else if (MATCH("ai_production_expansion_priority_bonus"))
    {
        cf->ai_production_expansion_priority_bonus = atof(value);
    }
    else if (MATCH("ai_production_expansion_priority_bonus_base_scale"))
    {
        cf->ai_production_expansion_priority_bonus_base_scale = atof(value);
    }
    else if (MATCH("ai_production_combat_unit_turns_limit"))
    {
        cf->ai_production_combat_unit_turns_limit = atoi(value);
    }
    else if (MATCH("ai_production_unit_min_mineral_surplus"))
    {
        cf->ai_production_unit_min_mineral_surplus = atoi(value);
    }
    else if (MATCH("ai_production_improvement_coverage_land"))
    {
        cf->ai_production_improvement_coverage_land = atof(value);
    }
    else if (MATCH("ai_production_improvement_coverage_ocean"))
    {
        cf->ai_production_improvement_coverage_ocean = atof(value);
    }
    else if (MATCH("ai_production_improvement_priority"))
    {
        cf->ai_production_improvement_priority = atof(value);
    }
    else if (MATCH("ai_production_naval_yard_defense_priority"))
    {
        cf->ai_production_naval_yard_defense_priority = atof(value);
    }
    else if (MATCH("ai_production_aerospace_complex_defense_priority"))
    {
        cf->ai_production_aerospace_complex_defense_priority = atof(value);
    }
    else if (MATCH("ai_production_inflation"))
    {
        cf->ai_production_inflation = atof(value);
    }
    else if (MATCH("ai_production_income_interest_rate"))
    {
        cf->ai_production_income_interest_rate = atof(value);
    }
    else if (MATCH("ai_production_transport_priority"))
    {
        cf->ai_production_transport_priority = atof(value);
    }
    else if (MATCH("ai_production_transport_seats_per_sea_base"))
    {
        cf->ai_production_transport_seats_per_sea_base = atof(value);
    }
    else if (MATCH("ai_production_support_ratio"))
    {
        cf->ai_production_support_ratio = atof(value);
    }
    else if (MATCH("ai_production_base_probe_priority"))
    {
        cf->ai_production_base_probe_priority = atof(value);
    }
    else if (MATCH("ai_production_base_protection_priority"))
    {
        cf->ai_production_base_protection_priority = atof(value);
    }
    else if (MATCH("ai_production_combat_priority"))
    {
        cf->ai_production_combat_priority = atof(value);
    }
    else if (MATCH("ai_production_alien_combat_priority"))
    {
        cf->ai_production_alien_combat_priority = atof(value);
    }
    else if (MATCH("ai_production_global_combat_superiority_land"))
    {
        cf->ai_production_global_combat_superiority_land = atof(value);
    }
    else if (MATCH("ai_production_global_combat_superiority_sea"))
    {
        cf->ai_production_global_combat_superiority_sea = atof(value);
    }
    else if (MATCH("ai_production_combat_unit_proportions"))
    {
		char *token;
		const char *delimiter = ",";
		token = strtok((char *)value, delimiter);
		for(int i = 0; i < 3 && token != nullptr; i++, token = strtok(nullptr, delimiter))
		{
			cf->ai_production_combat_unit_proportions[i] = atof(token);
		}
    }
    else if (MATCH("ai_production_current_project_priority_build_time"))
    {
		cf->ai_production_current_project_priority_build_time = atof(value);
    }
    else if (MATCH("ai_production_defensive_facility_threat_threshold"))
    {
		cf->ai_production_defensive_facility_threat_threshold = atof(value);
    }
    else if (MATCH("ai_production_priority_police"))
    {
        cf->ai_production_priority_police = atof(value);
    }
    else if (MATCH("ai_production_survival_effect_a"))
    {
        cf->ai_production_survival_effect_a = atof(value);
    }
    else if (MATCH("ai_production_survival_effect_b"))
    {
        cf->ai_production_survival_effect_b = atof(value);
    }
    else if (MATCH("ai_production_survival_effect_c"))
    {
        cf->ai_production_survival_effect_c = atof(value);
    }
    else if (MATCH("ai_production_survival_effect_d"))
    {
        cf->ai_production_survival_effect_d = atof(value);
    }
    else if (MATCH("ai_expansion_weight_deep"))
    {
        cf->ai_expansion_weight_deep = atof(value);
    }
    else if (MATCH("ai_mapstat_ocean"))
    {
        cf->ai_mapstat_ocean = atof(value);
    }
    else if (MATCH("ai_mapstat_rocky"))
    {
        cf->ai_mapstat_rocky = atof(value);
    }
    else if (MATCH("ai_mapstat_rainfall"))
    {
        cf->ai_mapstat_rainfall = atof(value);
    }
    else if (MATCH("ai_mapstat_rockiness"))
    {
        cf->ai_mapstat_rockiness = atof(value);
    }
    else if (MATCH("ai_mapstat_elevation"))
    {
        cf->ai_mapstat_elevation = atof(value);
    }
    else if (MATCH("ai_expansion_travel_time_multiplier"))
    {
        cf->ai_expansion_travel_time_multiplier = atof(value);
    }
    else if (MATCH("ai_expansion_coastal_base"))
    {
        cf->ai_expansion_coastal_base = atof(value);
    }
    else if (MATCH("ai_expansion_ocean_connection_base"))
    {
        cf->ai_expansion_ocean_connection_base = atof(value);
    }
    else if (MATCH("ai_expansion_radius_overlap_ignored"))
    {
        cf->ai_expansion_radius_overlap_ignored = atoi(value);
    }
    else if (MATCH("ai_expansion_radius_overlap_coefficient"))
    {
        cf->ai_expansion_radius_overlap_coefficient = atof(value);
    }
    else if (MATCH("ai_expansion_internal_border_connection_coefficient"))
    {
        cf->ai_expansion_internal_border_connection_coefficient = atof(value);
    }
    else if (MATCH("ai_expansion_external_border_connection_coefficient"))
    {
        cf->ai_expansion_external_border_connection_coefficient = atof(value);
    }
    else if (MATCH("ai_expansion_land_use_coefficient"))
    {
        cf->ai_expansion_land_use_coefficient = atof(value);
    }
    else if (MATCH("ai_expansion_placement_coefficient"))
    {
        cf->ai_expansion_placement_coefficient = atof(value);
    }
    else if (MATCH("ai_terraforming_nutrientWeight"))
    {
        cf->ai_terraforming_nutrientWeight = atof(value);
    }
    else if (MATCH("ai_terraforming_mineralWeight"))
    {
        cf->ai_terraforming_mineralWeight = atof(value);
    }
    else if (MATCH("ai_terraforming_energyWeight"))
    {
        cf->ai_terraforming_energyWeight = atof(value);
    }
    else if (MATCH("ai_terraforming_completion_bonus"))
    {
        cf->ai_terraforming_completion_bonus = atof(value);
    }
    else if (MATCH("ai_terraforming_land_rocky_tile_threshold"))
    {
        cf->ai_terraforming_land_rocky_tile_threshold = atof(value);
    }
    else if (MATCH("ai_terraforming_travel_time_multiplier"))
    {
        cf->ai_terraforming_travel_time_multiplier = atoi(value);
    }
    else if (MATCH("ai_terraforming_networkConnectionValue"))
    {
        cf->ai_terraforming_networkConnectionValue = atof(value);
    }
    else if (MATCH("ai_terraforming_networkImprovementValue"))
    {
        cf->ai_terraforming_networkImprovementValue = atof(value);
    }
    else if (MATCH("ai_terraforming_networkBaseExtensionValue"))
    {
        cf->ai_terraforming_networkBaseExtensionValue = atof(value);
    }
    else if (MATCH("ai_terraforming_networkWildExtensionValue"))
    {
        cf->ai_terraforming_networkWildExtensionValue = atof(value);
    }
    else if (MATCH("ai_terraforming_networkDensityThreshold"))
    {
        cf->ai_terraforming_networkDensityThreshold = atof(value);
    }
    else if (MATCH("ai_terraforming_nearbyForestKelpPenalty"))
    {
        cf->ai_terraforming_nearbyForestKelpPenalty = atof(value);
    }
    else if (MATCH("ai_terraforming_fitnessMultiplier"))
    {
        cf->ai_terraforming_fitnessMultiplier = atof(value);
    }
    else if (MATCH("ai_terraforming_baseNutrientThresholdRatio"))
    {
        cf->ai_terraforming_baseNutrientThresholdRatio = atof(value);
    }
    else if (MATCH("ai_terraforming_baseNutrientCostMultiplier"))
    {
        cf->ai_terraforming_baseNutrientCostMultiplier = atof(value);
    }
    else if (MATCH("ai_terraforming_baseMineralThresholdRatio"))
    {
        cf->ai_terraforming_baseMineralThresholdRatio = atof(value);
    }
    else if (MATCH("ai_terraforming_baseMineralCostMultiplier"))
    {
        cf->ai_terraforming_baseMineralCostMultiplier = atof(value);
    }
    else if (MATCH("ai_terraforming_raiseLandPayoffTime"))
    {
        cf->ai_terraforming_raiseLandPayoffTime = atof(value);
    }
    else if (MATCH("ai_terraforming_sensorValue"))
    {
        cf->ai_terraforming_sensorValue = atof(value);
    }
    else if (MATCH("ai_terraforming_sensorBorderRange"))
    {
        cf->ai_terraforming_sensorBorderRange = atof(value);
    }
    else if (MATCH("ai_terraforming_sensorShoreRange"))
    {
        cf->ai_terraforming_sensorShoreRange = atof(value);
    }
    else if (MATCH("ai_terraforming_bunkerValue"))
    {
        cf->ai_terraforming_bunkerValue = atof(value);
    }
    else if (MATCH("ai_terraforming_bunkerBorderRange"))
    {
        cf->ai_terraforming_bunkerBorderRange = atof(value);
    }
    else if (MATCH("ai_terraforming_landBridgeValue"))
    {
        cf->ai_terraforming_landBridgeValue = atof(value);
    }
    else if (MATCH("ai_terraforming_landBridgeRangeScale"))
    {
        cf->ai_terraforming_landBridgeRangeScale = atof(value);
    }
    else if (MATCH("ai_combat_base_threat_coefficient"))
    {
        cf->ai_combat_base_threat_coefficient = atof(value);
    }
    else if (MATCH("ai_combat_base_threat_range"))
    {
        cf->ai_combat_base_threat_range = atof(value);
    }
    else if (MATCH("ai_base_threat_travel_time_scale"))
    {
        cf->ai_base_threat_travel_time_scale = atof(value);
    }
    else if (MATCH("ai_combat_travel_time_scale"))
    {
        cf->ai_combat_travel_time_scale = atof(value);
    }
    else if (MATCH("ai_combat_travel_time_scale_base_protection"))
    {
        cf->ai_combat_travel_time_scale_base_protection = atof(value);
    }
    else if (MATCH("ai_combat_priority_escape"))
    {
        cf->ai_combat_priority_escape = atof(value);
    }
    else if (MATCH("ai_combat_priority_repair"))
    {
        cf->ai_combat_priority_repair = atof(value);
    }
    else if (MATCH("ai_combat_priority_repair_partial"))
    {
        cf->ai_combat_priority_repair_partial = atof(value);
    }
    else if (MATCH("ai_combat_priority_monolith_promotion"))
    {
        cf->ai_combat_priority_monolith_promotion = atof(value);
    }
    else if (MATCH("ai_combat_priority_field_healing"))
    {
        cf->ai_combat_priority_field_healing = atof(value);
    }
    else if (MATCH("ai_combat_priority_base_protection"))
    {
        cf->ai_combat_priority_base_protection = atof(value);
    }
    else if (MATCH("ai_combat_priority_base_healing"))
    {
        cf->ai_combat_priority_base_healing = atof(value);
    }
    else if (MATCH("ai_combat_attack_priority_base"))
    {
        cf->ai_combat_attack_priority_base = atof(value);
    }
    else if (MATCH("ai_combat_attack_bonus_alien_mind_worms"))
    {
        cf->ai_combat_attack_bonus_alien_mind_worms = atof(value);
    }
    else if (MATCH("ai_combat_attack_bonus_alien_spore_launcher"))
    {
        cf->ai_combat_attack_bonus_alien_spore_launcher = atof(value);
    }
    else if (MATCH("ai_combat_attack_bonus_alien_fungal_tower"))
    {
        cf->ai_combat_attack_bonus_alien_fungal_tower = atof(value);
    }
    else if (MATCH("ai_combat_attack_bonus_hostile"))
    {
        cf->ai_combat_attack_bonus_hostile = atof(value);
    }
    else if (MATCH("ai_combat_priority_pod"))
    {
        cf->ai_combat_priority_pod = atof(value);
    }
    else if (MATCH("ai_combat_base_protection_superiority"))
    {
        cf->ai_combat_base_protection_superiority = atof(value);
    }
    else if (MATCH("ai_combat_field_attack_base_proximity_scale"))
    {
        cf->ai_combat_field_attack_base_proximity_scale = atoi(value);
    }
    else if (MATCH("ai_combat_field_attack_superiority_required"))
    {
        cf->ai_combat_field_attack_superiority_required = atof(value);
    }
    else if (MATCH("ai_combat_field_attack_superiority_desired"))
    {
        cf->ai_combat_field_attack_superiority_desired = atof(value);
    }
    else if (MATCH("ai_combat_base_attack_superiority_required"))
    {
        cf->ai_combat_base_attack_superiority_required = atof(value);
    }
    else if (MATCH("ai_combat_base_attack_superiority_desired"))
    {
        cf->ai_combat_base_attack_superiority_desired = atof(value);
    }
    else if (MATCH("ai_combat_strength_increase_value"))
    {
        cf->ai_combat_strength_increase_value = atof(value);
    }
    else if (MATCH("ai_combat_winning_probability_a"))
    {
        cf->ai_combat_winning_probability_a = atof(value);
    }
    else if (MATCH("ai_combat_winning_probability_b"))
    {
        cf->ai_combat_winning_probability_b = atof(value);
    }
    else if (MATCH("ai_combat_sea_base_threat_coefficient"))
    {
        cf->ai_combat_sea_base_threat_coefficient = atof(value);
    }
    // =WTP= configuratoin end
    else {
        return opt_handle_error(section, name);
    }
    return 1;
}

int opt_handle_error(const char* section, const char* name) {
    static bool unknown_option = false;
    char msg[1024] = {};
    if (!unknown_option) {
        snprintf(msg, sizeof(msg),
            "Unknown configuration option detected in thinker.ini.\n"
            "Game might not work as intended.\n"
            "Header: %s\n"
            "Option: %s\n",
            section, name);
        MessageBoxA(0, msg, MOD_VERSION, MB_OK | MB_ICONWARNING);
    }
    unknown_option = true;
    return 0;
}

int opt_list_parse(int* ptr, char* buf, int len, int min_val) {
    const char *d=",";
    char *s, *p;
    p = strtok_r(buf, d, &s);
    for (int i = 0; i < len && p != NULL; i++, p = strtok_r(NULL, d, &s)) {
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
        } else if (wcscmp(argv[i], L"-native") == 0) {
            cf->video_mode = VM_Native;
        } else if (wcscmp(argv[i], L"-screen") == 0) {
            cf->video_mode = VM_Custom;
        } else if (wcscmp(argv[i], L"-windowed") == 0) {
            cf->video_mode = VM_Window;
        }
    }
    return 1;
}

DLL_EXPORT DWORD ThinkerModule() {
    return 0;
}

DLL_EXPORT BOOL APIENTRY DllMain(HINSTANCE UNUSED(hinstDLL), DWORD fdwReason, LPVOID UNUSED(lpvReserved)) {
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            if (DEBUG && !(debug_log = fopen("debug.txt", "w"))) {
                MessageBoxA(0, "Error while opening debug.txt file.",
                    MOD_VERSION, MB_OK | MB_ICONSTOP);
                exit(EXIT_FAILURE);
            }
            if (ini_parse("thinker.ini", option_handler, &conf) < 0) {
                MessageBoxA(0, "Error while opening thinker.ini file.",
                    MOD_VERSION, MB_OK | MB_ICONSTOP);
                exit(EXIT_FAILURE);
            }
			
			// [WTP]
			// parse thinker user file
			
			if (FileExists("thinker_user.ini"))
			{
				if (ini_parse("thinker_user.ini", option_handler, &conf) < 0)
				{
					MessageBoxA(0, "Error while opening thinker_user.ini file.", MOD_VERSION, MB_OK | MB_ICONSTOP);
					exit(EXIT_FAILURE);
				}
			}
			
            if (!cmd_parse(&conf) || !patch_setup(&conf)) {
                MessageBoxA(0, "Error while loading the game.",
                    MOD_VERSION, MB_OK | MB_ICONSTOP);
                exit(EXIT_FAILURE);
            }
            *EngineVersion = MOD_VERSION;
            *EngineDate = MOD_DATE;
            random_reseed(GetTickCount());
            debug("random_reseed %u\n", random_state());
            flushlog();
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



