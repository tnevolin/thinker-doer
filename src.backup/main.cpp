/*
 * Thinker - AI improvement mod for Sid Meier's Alpha Centauri.
 * https://github.com/induktio/thinker/
 *
 * Thinker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
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

bool unknown_option = false;
FILE* debug_log = NULL;
Config conf;
NodeSet mapnodes;
AIPlans plans[MaxPlayerNum];


int handler(void* user, const char* section, const char* name, const char* value) {
    Config* cf = (Config*)user;
    char buf[INI_MAX_LINE+2] = {};
    strncpy(buf, value, INI_MAX_LINE);
    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    if (MATCH("thinker", "DirectDraw")) {
        cf->directdraw = atoi(value);
    } else if (MATCH("thinker", "DisableOpeningMovie")) {
        cf->disable_opening_movie = atoi(value);
    } else if (MATCH("thinker", "cpu_idle_fix")) {
        cf->cpu_idle_fix = atoi(value);
    } else if (MATCH("thinker", "smooth_scrolling")) {
        cf->smooth_scrolling = atoi(value);
    } else if (MATCH("thinker", "scroll_area")) {
        cf->scroll_area = std::max(0, atoi(value));
    } else if (MATCH("thinker", "world_map_labels")) {
        cf->world_map_labels = atoi(value);
    } else if (MATCH("thinker", "new_base_names")) {
        cf->new_base_names = atoi(value);
    } else if (MATCH("thinker", "windowed")) {
        cf->windowed = atoi(value);
    } else if (MATCH("thinker", "window_width")) {
        cf->window_width = std::max(800, atoi(value));
    } else if (MATCH("thinker", "window_height")) {
        cf->window_height = std::max(600, atoi(value));
    } else if (MATCH("thinker", "smac_only")) {
        cf->smac_only = atoi(value);
    } else if (MATCH("thinker", "player_free_units")) {
        cf->player_free_units = atoi(value);
    } else if (MATCH("thinker", "free_formers")) {
        cf->free_formers = std::max(0, atoi(value));
    } else if (MATCH("thinker", "free_colony_pods")) {
        cf->free_colony_pods = std::max(0, atoi(value));
    } else if (MATCH("thinker", "satellites_nutrient")) {
        cf->satellites_nutrient = std::max(0, atoi(value));
    } else if (MATCH("thinker", "satellites_mineral")) {
        cf->satellites_mineral = std::max(0, atoi(value));
    } else if (MATCH("thinker", "satellites_energy")) {
        cf->satellites_energy = std::max(0, atoi(value));
    } else if (MATCH("thinker", "design_units")) {
        cf->design_units = atoi(value);
    } else if (MATCH("thinker", "factions_enabled")) {
        cf->factions_enabled = atoi(value);
    } else if (MATCH("thinker", "social_ai")) {
        cf->social_ai = atoi(value);
    } else if (MATCH("thinker", "social_ai_bias")) {
        cf->social_ai = std::min(1000, std::max(0, atoi(value)));
    } else if (MATCH("thinker", "tech_balance")) {
        cf->tech_balance = atoi(value);
    } else if (MATCH("thinker", "hurry_items")) {
        cf->hurry_items = atoi(value);
    } else if (MATCH("thinker", "base_spacing")) {
        cf->base_spacing = std::min(8, std::max(2, atoi(value)));
    } else if (MATCH("thinker", "base_nearby_limit")) {
        cf->base_nearby_limit = atoi(value);
    } else if (MATCH("thinker", "expansion_factor")) {
        cf->expansion_factor = std::max(0, atoi(value));
    } else if (MATCH("thinker", "expansion_autoscale")) {
        cf->expansion_autoscale = atoi(value);
    } else if (MATCH("thinker", "limit_project_start")) {
        cf->limit_project_start = atoi(value);
    } else if (MATCH("thinker", "max_satellites")) {
        cf->max_satellites = atoi(value);
    } else if (MATCH("thinker", "faction_placement")) {
        cf->faction_placement = atoi(value);
    } else if (MATCH("thinker", "nutrient_bonus")) {
        cf->nutrient_bonus = atoi(value);
    } else if (MATCH("thinker", "rare_supply_pods")) {
        cf->rare_supply_pods = atoi(value);
    } else if (MATCH("thinker", "revised_tech_cost")) {
        cf->revised_tech_cost = atoi(value);
    } else if (MATCH("thinker", "auto_relocate_hq")) {
        cf->auto_relocate_hq = atoi(value);
    } else if (MATCH("thinker", "ignore_reactor_power")) {
        cf->ignore_reactor_power = atoi(value);
    } else if (MATCH("thinker", "territory_border_fix")) {
        cf->territory_border_fix = atoi(value);
    } else if (MATCH("thinker", "eco_damage_fix")) {
        cf->eco_damage_fix = atoi(value);
    } else if (MATCH("thinker", "clean_minerals")) {
        cf->clean_minerals = std::max(0, atoi(value));
    } else if (MATCH("thinker", "spawn_fungal_towers")) {
        cf->spawn_fungal_towers = atoi(value);
    } else if (MATCH("thinker", "spawn_spore_launchers")) {
        cf->spawn_spore_launchers = atoi(value);
    } else if (MATCH("thinker", "spawn_sealurks")) {
        cf->spawn_sealurks = atoi(value);
    } else if (MATCH("thinker", "spawn_battle_ogres")) {
        cf->spawn_battle_ogres = atoi(value);
    } else if (MATCH("thinker", "collateral_damage_value")) {
        cf->collateral_damage_value = std::min(127, std::max(0, atoi(value)));
    } else if (MATCH("thinker", "disable_planetpearls")) {
        cf->disable_planetpearls = atoi(value);
    } else if (MATCH("thinker", "disable_aquatic_bonus_minerals")) {
        cf->disable_aquatic_bonus_minerals = atoi(value);
    } else if (MATCH("thinker", "disable_alien_guaranteed_techs")) {
        cf->disable_alien_guaranteed_techs = atoi(value);
    } else if (MATCH("thinker", "cost_factor")) {
        opt_list_parse(CostRatios, buf, MaxDiffNum, 1);
    } else if (MATCH("thinker", "tech_cost_factor")) {
        opt_list_parse(TechCostRatios, buf, MaxDiffNum, 1);
    } else if (MATCH("thinker", "content_pop_player")) {
        opt_list_parse(cf->content_pop_player, buf, 6, 0);
        cf->patch_content_pop = 1;
    } else if (MATCH("thinker", "content_pop_computer")) {
        opt_list_parse(cf->content_pop_computer, buf, 6, 0);
        cf->patch_content_pop = 1;
    } else if (MATCH("thinker", "repair_minimal")) {
        cf->repair_minimal = std::min(10, std::max(0, atoi(value)));
    } else if (MATCH("thinker", "repair_fungus")) {
        cf->repair_fungus = std::min(10, std::max(0, atoi(value)));
    } else if (MATCH("thinker", "repair_friendly")) {
        cf->repair_friendly = !!atoi(value);
    } else if (MATCH("thinker", "repair_airbase")) {
        cf->repair_airbase = !!atoi(value);
    } else if (MATCH("thinker", "repair_bunker")) {
        cf->repair_bunker = !!atoi(value);
    } else if (MATCH("thinker", "repair_base")) {
        cf->repair_base = std::min(10, std::max(0, atoi(value)));
    } else if (MATCH("thinker", "repair_base_native")) {
        cf->repair_base_native = std::min(10, std::max(0, atoi(value)));
    } else if (MATCH("thinker", "repair_base_facility")) {
        cf->repair_base_facility = std::min(10, std::max(0, atoi(value)));
    } else if (MATCH("thinker", "repair_nano_factory")) {
        cf->repair_nano_factory = std::min(10, std::max(0, atoi(value)));
    }
    // =WTP= configuratoin begin
    else if (MATCH("wtp", "disable_alien_guaranteed_technologies")) {
        cf->disable_alien_guaranteed_technologies = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_weapon_icon_selection_algorithm")) {
        cf->alternative_weapon_icon_selection_algorithm = (atoi(value) == 0 ? false : true);
    }
    // implemented in Thinker?
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
    else if (MATCH("wtp", "flat_hurry_cost")) {
        cf->flat_hurry_cost = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "flat_hurry_cost_multiplier_unit")) {
        cf->flat_hurry_cost_multiplier_unit = std::max(1, atoi(value));
    }
    else if (MATCH("wtp", "flat_hurry_cost_multiplier_facility")) {
        cf->flat_hurry_cost_multiplier_facility = std::max(1, atoi(value));
    }
    else if (MATCH("wtp", "flat_hurry_cost_multiplier_project")) {
        cf->flat_hurry_cost_multiplier_project = std::max(1, atoi(value));
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
//	else if (MATCH("wtp", "collateral_damage_value")) {
//		cf->collateral_damage_value = atoi(value);
//	}
	// integrated into Thinker
//	else if (MATCH("wtp", "disable_aquatic_bonus_minerals")) {
//		cf->disable_aquatic_bonus_minerals = (atoi(value) == 0 ? false : true);
//	}
	// integrated into Thinker
//	else if (MATCH("wtp", "repair_minimal")) {
//		cf->repair_minimal = std::min(10, std::max(0, atoi(value)));
//	}
//	else if (MATCH("wtp", "repair_fungus")) {
//		cf->repair_fungus = std::min(10, std::max(0, atoi(value)));
//	}
//	else if (MATCH("wtp", "repair_friendly")) {
//		cf->repair_friendly = (atoi(value) == 0 ? false : true);
//	}
//	else if (MATCH("wtp", "repair_airbase")) {
//		cf->repair_airbase = (atoi(value) == 0 ? false : true);
//	}
//	else if (MATCH("wtp", "repair_bunker")) {
//		cf->repair_bunker = (atoi(value) == 0 ? false : true);
//	}
//	else if (MATCH("wtp", "repair_base")) {
//		cf->repair_base = std::min(10, std::max(0, atoi(value)));
//	}
//	else if (MATCH("wtp", "repair_base_native")) {
//		cf->repair_base_native = std::min(10, std::max(0, atoi(value)));
//	}
//	else if (MATCH("wtp", "repair_base_facility")) {
//		cf->repair_base_facility = std::min(10, std::max(0, atoi(value)));
//	}
//	else if (MATCH("wtp", "repair_nano_factory")) {
//		cf->repair_nano_factory = std::min(10, std::max(0, atoi(value)));
//	}
    else if (MATCH("wtp", "alternative_combat_mechanics")) {
        cf->alternative_combat_mechanics = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "alternative_combat_mechanics_loss_divisor")) {
        cf->alternative_combat_mechanics_loss_divisor = std::max(1.0, atof(value));
    }
	// integrated into Thinker
//	else if (MATCH("wtp", "disable_planetpearls")) {
//		cf->disable_planetpearls = (atoi(value) == 0 ? false : true);
//	}
    else if (MATCH("wtp", "planet_combat_bonus_on_defense")) {
        cf->planet_combat_bonus_on_defense = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "sea_territory_distance_same_as_land")) {
        cf->sea_territory_distance_same_as_land = (atoi(value) == 0 ? false : true);
    }
    // integrated into Thinker
//	else if (MATCH("wtp", "coastal_territory_distance_same_as_sea")) {
//		cf->coastal_territory_distance_same_as_sea = (atoi(value) == 0 ? false : true);
//	}
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
        cf->cloning_vats_se_growth = std::max(0, std::min(2, atoi(value)));
    }
    else if (MATCH("wtp", "se_growth_rating_min"))
    {
        cf->se_growth_rating_min = std::min(0, atoi(value));
    }
    else if (MATCH("wtp", "se_growth_rating_cap"))
    {
        cf->se_growth_rating_cap = std::max(0, std::min(9, atoi(value)));
    }
    else if (MATCH("wtp", "recycling_tanks_mineral_multiplier"))
    {
        cf->recycling_tanks_mineral_multiplier = (atoi(value) == 0 ? false : true);
    }
    // integrated into Thinker
//	else if (MATCH("wtp", "free_minerals"))
//	{
//		cf->free_minerals = std::max(0, atoi(value));
//	}
    else if (MATCH("wtp", "native_life_generator_constant"))
    {
        cf->native_life_generator_constant = std::max(0, std::min(255, atoi(value)));
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
        cf->pts_new_base_size_less_average = std::max(0, atoi(value));
    }
    else if (MATCH("wtp", "biology_lab_research_bonus"))
    {
        cf->biology_lab_research_bonus = std::max(0, atoi(value));
    }
    else if (MATCH("wtp", "hsa_does_not_kill_probe"))
    {
        cf->hsa_does_not_kill_probe = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "condenser_and_enricher_do_not_multiply_nutrients"))
    {
        cf->condenser_and_enricher_do_not_multiply_nutrients = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "tech_cost_scale"))
    {
        cf->tech_cost_scale = atof(value);
    }
    else if (MATCH("wtp", "flat_extra_prototype_cost"))
    {
        cf->flat_extra_prototype_cost = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "fix_mineral_contribution"))
    {
        cf->fix_mineral_contribution = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "fix_former_wake"))
    {
        cf->fix_former_wake = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "infiltration_expire"))
    {
        cf->infiltration_expire = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "infiltration_devices"))
    {
        cf->infiltration_devices = std::max(1, atoi(value));
    }
    else if (MATCH("wtp", "human_infiltration_device_lifetime_base"))
    {
        cf->human_infiltration_device_lifetime_base = std::max(1.0, atof(value));
    }
    else if (MATCH("wtp", "human_infiltration_device_lifetime_probe_effect"))
    {
        cf->human_infiltration_device_lifetime_probe_effect = atof(value);
    }
    else if (MATCH("wtp", "computer_infiltration_device_lifetime_base"))
    {
        cf->computer_infiltration_device_lifetime_base = std::max(1.0, atof(value));
    }
    else if (MATCH("wtp", "computer_infiltration_device_lifetime_probe_effect"))
    {
        cf->computer_infiltration_device_lifetime_probe_effect = atof(value);
    }
    else if (MATCH("wtp", "modified_probe_action_risks"))
    {
        cf->modified_probe_action_risks = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "probe_action_risk_genetic_plague"))
    {
        cf->probe_action_risk_genetic_plague = std::max(0, atoi(value));
    }
    else if (MATCH("wtp", "combat_bonus_sensor_ocean"))
    {
        cf->combat_bonus_sensor_ocean = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "combat_bonus_sensor_offense"))
    {
        cf->combat_bonus_sensor_offense = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "break_treaty_before_fight"))
    {
        cf->break_treaty_before_fight = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "compact_effect_icons"))
    {
        cf->compact_effect_icons = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "se_research_bonus_percentage"))
    {
        cf->se_research_bonus_percentage = std::max(0, std::min(100, atoi(value)));
    }
    else if (MATCH("wtp", "remove_fungal_tower_defense_bonus"))
    {
        cf->remove_fungal_tower_defense_bonus = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "aliens_fight_half_strength_unit_turn"))
    {
        cf->aliens_fight_half_strength_unit_turn = std::max(1, atoi(value));
    }
    else if (MATCH("wtp", "habitation_facility_disable_explicit_population_limit"))
    {
        cf->habitation_facility_disable_explicit_population_limit = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "habitation_facility_absent_growth_penalty"))
    {
        cf->habitation_facility_absent_growth_penalty = std::max(0, atoi(value));
    }
    else if (MATCH("wtp", "habitation_facility_present_growth_bonus_max"))
    {
        cf->habitation_facility_present_growth_bonus_max = std::max(0, atoi(value));
    }
    else if (MATCH("wtp", "unit_upgrade_ignores_movement"))
    {
        cf->unit_upgrade_ignores_movement = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "group_terraforming"))
    {
        cf->group_terraforming = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "ai_base_allowed_fungus_rocky"))
    {
        cf->ai_base_allowed_fungus_rocky = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "ai_useWTPAlgorithms"))
    {
        cf->ai_useWTPAlgorithms = (atoi(value) == 0 ? false : true);
    }
    else if (MATCH("wtp", "ai_production_vanilla_priority_unit"))
    {
        cf->ai_production_vanilla_priority_unit = atof(value);
    }
    else if (MATCH("wtp", "ai_production_vanilla_priority_project"))
    {
        cf->ai_production_vanilla_priority_project = atof(value);
    }
    else if (MATCH("wtp", "ai_production_vanilla_priority_facility"))
    {
        cf->ai_production_vanilla_priority_facility = atof(value);
    }
    else if (MATCH("wtp", "ai_production_thinker_priority"))
    {
        cf->ai_production_thinker_priority = atof(value);
    }
    else if (MATCH("wtp", "ai_production_threat_coefficient_vendetta"))
    {
        cf->ai_production_threat_coefficient_vendetta = atof(value);
    }
    else if (MATCH("wtp", "ai_production_threat_coefficient_pact"))
    {
        cf->ai_production_threat_coefficient_pact = atof(value);
    }
    else if (MATCH("wtp", "ai_production_threat_coefficient_treaty"))
    {
        cf->ai_production_threat_coefficient_treaty = atof(value);
    }
    else if (MATCH("wtp", "ai_production_threat_coefficient_other"))
    {
        cf->ai_production_threat_coefficient_other = atof(value);
    }
    else if (MATCH("wtp", "ai_production_min_native_protection"))
    {
        cf->ai_production_min_native_protection = atof(value);
    }
    else if (MATCH("wtp", "ai_production_max_native_protection"))
    {
        cf->ai_production_max_native_protection = atof(value);
    }
    else if (MATCH("wtp", "ai_production_native_protection_priority"))
    {
        cf->ai_production_native_protection_priority = atof(value);
    }
    else if (MATCH("wtp", "ai_production_max_unpopulated_range"))
    {
        cf->ai_production_max_unpopulated_range = atoi(value);
    }
    else if (MATCH("wtp", "ai_production_expansion_coverage"))
    {
        cf->ai_production_expansion_coverage = atof(value);
    }
    else if (MATCH("wtp", "ai_production_ocean_expansion_coverage"))
    {
        cf->ai_production_ocean_expansion_coverage = atof(value);
    }
    else if (MATCH("wtp", "ai_production_expansion_priority"))
    {
        cf->ai_production_expansion_priority = atof(value);
    }
    else if (MATCH("wtp", "ai_production_combat_unit_turns_limit"))
    {
        cf->ai_production_combat_unit_turns_limit = atoi(value);
    }
    else if (MATCH("wtp", "ai_production_facility_priority_penalty"))
    {
        cf->ai_production_facility_priority_penalty = atof(value);
    }
    else if (MATCH("wtp", "ai_production_combat_unit_min_mineral_surplus"))
    {
        cf->ai_production_combat_unit_min_mineral_surplus = atoi(value);
    }
    else if (MATCH("wtp", "ai_production_exploration_coverage"))
    {
        cf->ai_production_exploration_coverage = atof(value);
    }
    else if (MATCH("wtp", "ai_production_improvement_coverage"))
    {
        cf->ai_production_improvement_coverage = atof(value);
    }
    else if (MATCH("wtp", "ai_production_improvement_coverage"))
    {
        cf->ai_production_improvement_coverage = atof(value);
    }
    else if (MATCH("wtp", "ai_production_population_projection_turns"))
    {
        cf->ai_production_population_projection_turns = atoi(value);
    }
    else if (MATCH("wtp", "ai_production_threat_level_threshold"))
    {
        cf->ai_production_threat_level_threshold = atof(value);
    }
    else if (MATCH("wtp", "ai_production_combat_choice_priority"))
    {
        cf->ai_production_combat_choice_priority = atof(value);
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
    else if (MATCH("wtp", "ai_terraforming_sensorValue"))
    {
        cf->ai_terraforming_sensorValue = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_sensorBorderRange"))
    {
        cf->ai_terraforming_sensorBorderRange = atof(value);
    }
    else if (MATCH("wtp", "ai_terraforming_sensorShoreRange"))
    {
        cf->ai_terraforming_sensorShoreRange = atof(value);
    }
    // =WTP= configuratoin end
    else {
        for (int i=0; i<16; i++) {
            if (MATCH("thinker", landmark_params[i])) {
                cf->landmarks &= ~((atoi(value) ? 0 : 1) << i);
                return 1;
            }
        }
        unknown_option = true;

        // =WTP=
        // display unknown option
        char message[100];
        sprintf(message, "Unknown configuration option in thinker.ini.\n[%s]\n%s", section, name);
		MessageBoxA(0, message, MOD_VERSION, MB_OK | MB_ICONWARNING);

        return 0;  /* unknown section/name, error */
    }
    return 1;
}

int opt_list_parse(int* ptr, char* buf, int len, int min_val) {
    const char *d=",";
    char *s, *p;
    p = strtok_r(buf, d, &s);
    for (int i=0; i<len && p != NULL; i++, p = strtok_r(NULL, d, &s)) {
        ptr[i] = std::max(min_val, atoi(p));
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
        } else if (wcscmp(argv[i], L"-windowed") == 0) {
            cf->windowed = 1;
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
            if (unknown_option) {
                MessageBoxA(0, "Unknown configuration option detected in thinker.ini.\n"\
                    "Game might not work as intended.", MOD_VERSION, MB_OK | MB_ICONWARNING);
            }
            srand(time(0));
            *engine_version = MOD_VERSION;
            *engine_date = MOD_DATE;
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

HOOK_API int mod_base_production(int id, int v1, int v2, int v3) {
    assert(id >= 0 && id < *total_num_bases);
    BASE* base = &Bases[id];
    int prod = base->queue_items[0];
    int faction = base->faction_id;
    int choice = 0;
    print_base(id);

    if (is_human(faction)) {
        debug("skipping human base\n");
        choice = base_prod_choices(id, v1, v2, v3);
    } else if (!ai_enabled(faction)) {
        debug("skipping computer base\n");
        choice = base_prod_choices(id, v1, v2, v3);
    } else {
        set_base(id);
        base_compute(1);
        if ((choice = need_psych(id)) != 0 && choice != prod) {
            debug("BUILD PSYCH\n");
        } else if (base->status_flags & BASE_PRODUCTION_DONE) {
            choice = select_prod(id);
            base->status_flags &= ~BASE_PRODUCTION_DONE;
        } else if (prod >= 0 && !can_build_unit(faction, prod)) {
            debug("BUILD FACILITY\n");
            choice = find_facility(id);
        } else if (prod < 0 && !can_build(id, abs(prod))) {
            debug("BUILD CHANGE\n");
            if (base->minerals_accumulated > Rules->retool_exemption) {
                choice = find_facility(id);
            } else {
                choice = select_prod(id);
            }
        } else if (need_defense(id)) {
            debug("BUILD DEFENSE\n");
            choice = find_proto(id, TRIAD_LAND, COMBAT, DEF);
        } else {
            consider_hurry(id);
            debug("BUILD OLD\n");
            choice = prod;
        }
        debug("choice: %d %s\n", choice, prod_name(choice));
    }
    fflush(debug_log);
    return choice;
}

HOOK_API int mod_turn_upkeep() {
    turn_upkeep();
    debug("turn_upkeep %d bases: %d vehicles: %d\n",
        *current_turn, *total_num_bases, *total_num_vehicles);

    if (DEBUG) {
        if (conf.debug_mode) {
            *game_state |= STATE_DEBUG_MODE;
        } else {
            *game_state &= ~STATE_DEBUG_MODE;
        }
    }
    return 0;
}

int plans_upkeep(int faction) {
    const int i = faction;
    if (!faction || is_human(faction)) {
        return 0;
    }
    /*
    Remove bugged prototypes from the savegame.
    */
    for (int j=0; j < MaxProtoFactionNum; j++) {
        int id = faction*MaxProtoFactionNum + j;
        UNIT* u = &Units[id];
        if (strlen(u->name) >= MaxProtoNameLen
        || u->chassis_type < CHS_INFANTRY
        || u->chassis_type > CHS_MISSILE) {
            for (int k = *total_num_vehicles-1; k >= 0; k--) {
                if (Vehicles[k].unit_id == id) {
                    veh_kill(k);
                }
            }
            for (int k=0; k < *total_num_bases; k++) {
                if (Bases[k].queue_items[0] == id) {
                    Bases[k].queue_items[0] = -FAC_STOCKPILE_ENERGY;
                }
            }
            memset(u, 0, sizeof(UNIT));
        }
    }

    if (!Factions[faction].current_num_bases) {
        return 0;
    }
    if (conf.design_units) {
        int rec = best_reactor(i);
        int wpn = best_weapon(i);
        int arm = best_armor(i, false);
        int arm2 = best_armor(i, true);
        int chs = has_chassis(i, CHS_HOVERTANK) ? CHS_HOVERTANK : 
            (has_chassis(i, CHS_SPEEDER) ? CHS_SPEEDER : CHS_INFANTRY);
        bool twoabl = has_tech(i, Rules->tech_preq_allow_2_spec_abil);
        char buf[200];

        if (has_weapon(i, WPN_PROBE_TEAM)) {
            int algo = has_ability(i, ABL_ID_ALGO_ENHANCEMENT) ? ABL_ALGO_ENHANCEMENT : 0;
            if (has_chassis(i, CHS_FOIL)) {
                int ship = (rec >= REC_FUSION && has_chassis(i, CHS_CRUISER) ? CHS_CRUISER : CHS_FOIL);
                char* name = parse_str(buf, MaxProtoNameLen,
                    Armor[(rec >= REC_FUSION ? arm : ARM_NO_ARMOR)].name_short,
                    " ", Chassis[ship].offsv1_name, " Probe");
                propose_proto(i, ship, WPN_PROBE_TEAM,
                    (rec >= REC_FUSION ? arm : ARM_NO_ARMOR), algo, rec, PLAN_INFO_WARFARE, name);
            }
            if (arm != ARM_NO_ARMOR && rec >= REC_FUSION) {
                char* name = parse_str(buf, MaxProtoNameLen, Armor[arm].name_short, " ",
                    Chassis[chs].offsv1_name, " Probe");
                propose_proto(i, chs, WPN_PROBE_TEAM, arm, algo, rec, PLAN_INFO_WARFARE, name);
            }
        }
        if (has_ability(i, ABL_ID_AAA) && arm != ARM_NO_ARMOR) {
            int abls = ABL_AAA;
            if (twoabl) {
                abls |= (has_ability(i, ABL_ID_COMM_JAMMER) ? ABL_COMM_JAMMER :
                    (has_ability(i, ABL_ID_TRANCE) ? ABL_TRANCE : 0));
            }
            propose_proto(i, CHS_INFANTRY, WPN_HAND_WEAPONS, arm, abls, rec, PLAN_DEFENSIVE, NULL);
        }
        if (has_ability(i, ABL_ID_TRANCE) && !has_ability(i, ABL_ID_AAA)) {
            propose_proto(i, CHS_INFANTRY, WPN_HAND_WEAPONS, arm, ABL_TRANCE, rec, PLAN_DEFENSIVE, NULL);
        }
        if (has_chassis(i, CHS_NEEDLEJET) && has_ability(i, ABL_ID_AIR_SUPERIORITY)) {
            propose_proto(i, CHS_NEEDLEJET, wpn, ARM_NO_ARMOR, ABL_AIR_SUPERIORITY | ABL_DEEP_RADAR,
                rec, PLAN_AIR_SUPERIORITY, NULL);
        }
        if (has_weapon(i, WPN_TERRAFORMING_UNIT) && rec >= REC_FUSION) {
            bool grav = has_chassis(i, CHS_GRAVSHIP);
            int abls = has_ability(i, ABL_ID_SUPER_TERRAFORMER ? ABL_SUPER_TERRAFORMER : 0) |
                (twoabl && !grav && has_ability(i, ABL_ID_FUNGICIDAL) ? ABL_FUNGICIDAL : 0);
            propose_proto(i, (grav ? CHS_GRAVSHIP : chs), WPN_TERRAFORMING_UNIT, ARM_NO_ARMOR, abls,
                REC_FUSION, PLAN_TERRAFORMING, NULL);
        }
        if (has_weapon(i, WPN_SUPPLY_TRANSPORT) && rec >= REC_FUSION && arm2 != ARM_NO_ARMOR) {
            char* name = parse_str(buf, MaxProtoNameLen, Reactor[REC_FUSION-1].name_short, " ",
                Armor[arm2].name_short, " Supply");
            propose_proto(i, CHS_INFANTRY, WPN_SUPPLY_TRANSPORT, arm2, 0, REC_FUSION, PLAN_DEFENSIVE, name);
        }
    }

    if (ai_enabled(i)) {
        int minerals[MaxBaseNum];
        int population[MaxBaseNum];
        Faction* f = &Factions[i];
        MFaction* m = &MFactions[i];

        plans[i].unknown_factions = 0;
        plans[i].contacted_factions = 0;
        plans[i].enemy_factions = 0;
        plans[i].enemy_bases = 0;
        plans[i].diplo_flags = 0;

        for (int j=1; j < MaxPlayerNum; j++) {
            int diplo = f->diplo_status[j];
            if (i != j && Factions[j].current_num_bases > 0) {
                plans[i].diplo_flags |= diplo;
                if (diplo & DIPLO_COMMLINK) {
                    plans[i].contacted_factions++;
                } else {
                    plans[i].unknown_factions++;
                }
                if (at_war(i, j)) {
                    plans[i].enemy_factions++;
                }
            }
        }
        int n = 0;
        for (int j=0; j < *total_num_bases; j++) {
            BASE* base = &Bases[j];
            if (base->faction_id == i) {
                population[n] = base->pop_size;
                minerals[n++] = base->mineral_surplus;
            } else if (base->faction_id_former == i && at_war(i, base->faction_id)) {
                plans[i].enemy_bases += (is_human(base->faction_id) ? 4 : 1);
            }
        }
        std::sort(minerals, minerals+n);
        std::sort(population, population+n);
        if (f->current_num_bases >= 20) {
            plans[i].proj_limit = std::max(5, minerals[n*3/4]);
        } else {
            plans[i].proj_limit = std::max(5, minerals[n*2/3]);
        }
        plans[i].satellites_goal = std::min(conf.max_satellites, population[n*3/4]);
        plans[i].need_police = (f->SE_police > -2 && f->SE_police < 3
            && !has_project(i, FAC_TELEPATHIC_MATRIX));

        int psi = std::max(-4, 2 - f->best_weapon_value);
        if (has_project(i, FAC_NEURAL_AMPLIFIER)) {
            psi += 2;
        }
        if (has_project(i, FAC_DREAM_TWISTER)) {
            psi += 2;
        }
        if (has_project(i, FAC_PHOLUS_MUTAGEN)) {
            psi++;
        }
        if (has_project(i, FAC_XENOEMPATHY_DOME)) {
            psi++;
        }
        if (MFactions[i].rule_flags & FACT_WORMPOLICE && plans[i].need_police) {
            psi += 2;
        }
        psi += std::min(2, std::max(-2, (f->enemy_best_weapon_value - f->best_weapon_value)/2));
        psi += MFactions[i].rule_psi/10 + 2*f->SE_planet;
        plans[i].psi_score = psi;

        const int manifold[][3] = {{0,0,0}, {0,1,0}, {1,1,0}, {1,1,1}, {1,2,1}};
        int p = (has_project(i, FAC_MANIFOLD_HARMONICS) ? std::min(4, std::max(0, f->SE_planet + 1)) : 0);
        int dn = manifold[p][0] + f->tech_fungus_nutrient - Resource->forest_sq_nutrient;
        int dm = manifold[p][1] + f->tech_fungus_mineral - Resource->forest_sq_mineral;
        int de = manifold[p][2] + f->tech_fungus_energy - Resource->forest_sq_energy;
        bool terra_fungus = has_terra(i, FORMER_PLANT_FUNGUS, LAND)
            && (has_tech(i, Rules->tech_preq_ease_fungus_mov) || has_project(i, FAC_XENOEMPATHY_DOME));

        plans[i].keep_fungus = (dn+dm+de > 0);
        if (dn+dm+de > 0 && terra_fungus) {
            plans[i].plant_fungus = dn+dm+de;
        } else {
            plans[i].plant_fungus = 0;
        }
        debug("plans_upkeep %d %d proj_limit: %2d sat_goal: %2d psi: %2d keep_fungus: %d "\
              "plant_fungus: %d enemy_bases: %d enemy_range: %.4f\n",
              *current_turn, i, plans[i].proj_limit, plans[i].satellites_goal,
              psi, plans[i].keep_fungus, plans[i].plant_fungus,
              plans[i].enemy_bases, m->thinker_enemy_range);
    }
    return 0;
}

int project_score(int faction, int proj) {
    CFacility* p = &Facility[proj];
    Faction* f = &Factions[faction];
    return random(3) + f->AI_fight * p->AI_fight
        + (f->AI_growth+1) * p->AI_growth + (f->AI_power+1) * p->AI_power
        + (f->AI_tech+1) * p->AI_tech + (f->AI_wealth+1) * p->AI_wealth;
}

bool redundant_project(int faction, int proj) {
    Faction* f = &Factions[faction];
    if (proj == FAC_PLANETARY_DATALINKS) {
        int n = 0;
        for (int i=0; i < MaxPlayerNum; i++) {
            if (Factions[i].current_num_bases > 0) {
                n++;
            }
        }
        return n < 4;
    }
    if (proj == FAC_CITIZENS_DEFENSE_FORCE) {
        return Facility[FAC_CITIZENS_DEFENSE_FORCE].maint == 0
            && facility_count(faction, FAC_CITIZENS_DEFENSE_FORCE) > f->current_num_bases/2 + 2;
    }
    if (proj == FAC_MARITIME_CONTROL_CENTER) {
        int n = 0;
        for (int i=0; i<*total_num_vehicles; i++) {
            VEH* veh = &Vehicles[i];
            if (veh->faction_id == faction && veh->triad() == TRIAD_SEA) {
                n++;
            }
        }
        return n < f->current_num_bases/4 && facility_count(faction, FAC_NAVAL_YARD) < 4;
    }
    if (proj == FAC_HUNTER_SEEKER_ALGO) {
        return f->SE_probe >= 3;
    }
    if (proj == FAC_LIVING_REFINERY) {
        return f->SE_support >= 3;
    }
    return false;
}

int find_project(int id) {
    BASE* base = &Bases[id];
    int faction = base->faction_id;
    Faction* f = &Factions[faction];
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
        if (Bases[i].faction_id == faction) {
            int t = Bases[i].queue_items[0];
            if (t <= -PROJECT_ID_FIRST || t == -FAC_SUBSPACE_GENERATOR) {
                projs++;
            } else if (t == -FAC_SKUNKWORKS) {
                works++;
            } else if (t >= 0 && Units[t].weapon_type == WPN_PLANET_BUSTER) {
                nukes++;
            }
        }
    }
    if (build_nukes && nukes < nuke_limit && nukes < bases/8) {
        int best = 0;
        for(int i=0; i<64; i++) {
            int unit_id = faction*64 + i;
            UNIT* u = &Units[unit_id];
            if (u->weapon_type == WPN_PLANET_BUSTER && strlen(u->name) > 0
            && offense_value(u) > offense_value(&Units[best])) {
                debug("find_project %d %d %s\n", faction, unit_id, (char*)Units[unit_id].name);
                best = unit_id;
            }
        }
        if (best) {
            if (~Units[best].unit_flags & UNIT_PROTOTYPED && can_build(id, FAC_SKUNKWORKS)) {
                if (works < 2)
                    return -FAC_SKUNKWORKS;
            } else {
                return best;
            }
        }
    }
    if (projs+nukes < (build_nukes ? 4 : 3) && projs+nukes < bases/4) {
        bool alien = is_alien(faction);
        if (alien && can_build(id, FAC_SUBSPACE_GENERATOR)) {
            return -FAC_SUBSPACE_GENERATOR;
        }
        int score = INT_MIN;
        int choice = 0;
        for (int i=PROJECT_ID_FIRST; i<=PROJECT_ID_LAST; i++) {
            int tech = Facility[i].preq_tech;
            if (alien && (i == FAC_ASCENT_TO_TRANSCENDENCE || i == FAC_VOICE_OF_PLANET)) {
                continue;
            }
            if (conf.limit_project_start > *diff_level && i != FAC_ASCENT_TO_TRANSCENDENCE
            && tech >= 0 && !(TechOwners[tech] & *human_players)) {
                continue;
            }
            if (can_build(id, i) && prod_count(faction, -i, id) < similar_limit
            && (similar_limit > 1 || !redundant_project(faction, i))) {
                int sc = project_score(faction, i);
                choice = (sc > score ? i : choice);
                score = std::max(score, sc);
                debug("find_project %d %d %d %s\n", faction, i, sc, Facility[i].name);
            }
        }
        return (projs > 0 || score > 2 ? -choice : 0);
    }
    return 0;
}

bool relocate_hq(int id) {
    BASE* base = &Bases[id];
    int faction = base->faction_id;
    Faction* f = &Factions[faction];
    int bases = f->current_num_bases;
    int n = 0;
    int k = 0;

    if (!has_tech(faction, Facility[FAC_HEADQUARTERS].preq_tech)
    || base->mineral_surplus < (bases < 6 ? 4 : plans[faction].proj_limit)) {
        return false;
    }
    for (int i=0; i<*total_num_bases; i++) {
        BASE* b = &Bases[i];
        int t = b->queue_items[0];
        if (b->faction_id == faction) {
            if (i != id) {
                if (i < id) {
                    k++;
                }
                if (map_range(base->x, base->y, b->x, b->y) < 7) {
                    n++;
                }
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
    return random(100) < 30.0 * (1.0 - 1.0 * k / bases) + 30.0 * std::min(15, n) / std::min(15, bases);
}

bool can_build_ships(int id) {
    BASE* b = &Bases[id];
    int k = *map_area_sq_root + 20;
    return (has_chassis(b->faction_id, CHS_FOIL) || has_chassis(b->faction_id, CHS_CRUISER))
        && nearby_tiles(b->x, b->y, TRIAD_SEA, k) >= k;
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
        && Chassis[u1->chassis_type].speed <= Chassis[u2->chassis_type].speed
        && (Chassis[u2->chassis_type].triad != TRIAD_AIR || u1->chassis_type == u2->chassis_type)
        && !((u1->ability_flags & u2->ability_flags) ^ u1->ability_flags));
    if (val) {
        debug("unit_is_better %s -> %s\n", u1->name, u2->name);
    }
    return val;
}

int unit_score(int id, int faction, int cfactor, int minerals, int accumulated, bool defend) {
    assert(valid_player(faction) && id >= 0 && cfactor > 0);
    const int abls[][2] = {
        {ABL_AAA, 4},
        {ABL_AIR_SUPERIORITY, 2},
        {ABL_ALGO_ENHANCEMENT, 5},
        {ABL_AMPHIBIOUS, -4},
        {ABL_ARTILLERY, -2},
        {ABL_DROP_POD, 3},
        {ABL_EMPATH, 2},
        {ABL_TRANCE, 3},
        {ABL_SLOW, -4},
        {ABL_TRAINED, 2},
        {ABL_COMM_JAMMER, 3},
        {ABL_CLEAN_REACTOR, 2},
        {ABL_ANTIGRAV_STRUTS, 3},
        {ABL_BLINK_DISPLACER, 3},
        {ABL_DEEP_PRESSURE_HULL, 2},
        {ABL_SUPER_TERRAFORMER, 8},
    };
    UNIT* u = &Units[id];
    int v = 18 * (defend ? defense_value(u) : offense_value(u));
    if (v < 0) {
        v = (defend ? Factions[faction].best_armor_value : Factions[faction].best_weapon_value)
            * (conf.ignore_reactor_power ? REC_FISSION : best_reactor(faction))
            + plans[faction].psi_score * 12;
    }
    if (u->triad() != TRIAD_AIR) {
        v += (defend ? 12 : 32) * u->speed();
    }
    if (u->ability_flags & ABL_POLICE_2X && plans[faction].need_police) {
        v += 20;
    }
    for (const int* i : abls) {
        if (u->ability_flags & i[0]) {
            v += 8 * i[1];
        }
    }
    int turns = std::max(0, u->cost * cfactor - accumulated) / std::max(2, minerals);
    int score = v - turns * (u->weapon_type == WPN_COLONY_MODULE ? 6 : 3)
        * (std::max(3, 9 - *current_turn/10) + (minerals < 6 ? 2 : 0));
    debug("unit_score %s cfactor: %d minerals: %d cost: %d turns: %d score: %d\n",
        u->name, cfactor, minerals, u->cost, turns, score);
    return score;
}

/*
Find the best prototype for base production when weighted against cost given the triad
and type constraints. For any combat-capable unit, mode is set to COMBAT (= 0).
*/
int find_proto(int base_id, int triad, int mode, bool defend) {
    assert(base_id >= 0 && base_id < *total_num_bases);
    assert(mode >= COMBAT && mode <= WMODE_INFOWAR);
    assert(valid_triad(triad));
    BASE* b = &Bases[base_id];
    int faction = b->faction_id;
    int basic = BSC_SCOUT_PATROL;
    debug("find_proto faction: %d triad: %d mode: %d defend: %d\n", faction, triad, mode, defend);

    if (mode == WMODE_COLONIST) {
        basic = (triad == TRIAD_SEA ? BSC_SEA_ESCAPE_POD : BSC_COLONY_POD);
    } else if (mode == WMODE_TERRAFORMER) {
        basic = (triad == TRIAD_SEA ? BSC_SEA_FORMERS : BSC_FORMERS);
    } else if (mode == WMODE_CONVOY) {
        basic = BSC_SUPPLY_CRAWLER;
    } else if (mode == WMODE_TRANSPORT) {
        basic = BSC_TRANSPORT_FOIL;
    } else if (mode == WMODE_INFOWAR) {
        basic = BSC_PROBE_TEAM;
    }
    int best = basic;
    int cfactor = cost_factor(faction, 1, -1);
    int best_val = unit_score(
        best, faction, cfactor, b->mineral_surplus, b->minerals_accumulated, defend);
    if (Units[best].triad() != triad) {
        best_val -= 40;
    }
    for (int i=0; i < 2*MaxProtoFactionNum; i++) {
        int id = (i < MaxProtoFactionNum ? i : (faction-1)*MaxProtoFactionNum + i);
        UNIT* u = &Units[id];
        if (u->unit_flags & UNIT_ACTIVE && strlen(u->name) > 0 && u->triad() == triad && id != best) {
            if (id < MaxProtoFactionNum && !has_tech(faction, u->preq_tech)) {
                continue;
            }
            if ((mode && Weapon[u->weapon_type].mode != mode)
            || (!mode && Weapon[u->weapon_type].offense_value == 0)
            || (!mode && defend && u->chassis_type != CHS_INFANTRY)
            || (u->weapon_type == WPN_PSI_ATTACK && plans[faction].psi_score < 1)
            || u->weapon_type == WPN_PLANET_BUSTER
            || !(mode != COMBAT || best == basic || (defend == (offense_value(u) < defense_value(u))))) {
                continue;
            }
            int val = unit_score(
                id, faction, cfactor, b->mineral_surplus, b->minerals_accumulated, defend);
            if (unit_is_better(&Units[best], u) || random(100) > 50 + best_val - val) {
                best = id;
                best_val = val;
                debug("===> %s\n", Units[best].name);
            }
        }
    }
    return best;
}

int need_defense(int id) {
    BASE* b = &Bases[id];
    int prod = b->queue_items[0];
    /* Do not interrupt secret project building. */
    return !has_defenders(b->x, b->y, b->faction_id)
        && (Rules->retool_strictness == 0 || (prod < 0 && prod > -FAC_HUMAN_GENOME_PROJ));
}

int need_psych(int id) {
    BASE* b = &Bases[id];
    int faction = b->faction_id;
    int unit = unit_in_tile(mapsq(b->x, b->y));
    Faction* f = &Factions[faction];
    if (unit != faction || b->nerve_staple_turns_left > 0 || has_project(faction, FAC_TELEPATHIC_MATRIX))
        return 0;
    if (b->drone_total > b->talent_total) {
        if (b->nerve_staple_count < 3 && !un_charter() && f->SE_police >= 0 && b->pop_size >= 4
        && b->faction_id_former == faction) {
            action_staple(id);
            return 0;
        }
        if (can_build(id, FAC_RECREATION_COMMONS))
            return -FAC_RECREATION_COMMONS;
        if (has_project(faction, FAC_VIRTUAL_WORLD) && can_build(id, FAC_NETWORK_NODE))
            return -FAC_NETWORK_NODE;
        if (!b->assimilation_turns_left && b->pop_size >= 6 && b->energy_surplus >= 12
        && can_build(id, FAC_HOLOGRAM_THEATRE))
            return -FAC_HOLOGRAM_THEATRE;
    }
    return 0;
}

int hurry_item(BASE* b, int mins, int cost) {
    Faction* f = &Factions[b->faction_id];
    f->energy_credits -= cost;
    b->minerals_accumulated += mins;
    b->status_flags |= BASE_HURRY_PRODUCTION;
    debug("hurry_item %d %d %d %d %d %s %s\n", *current_turn, b->faction_id,
        mins, cost, f->energy_credits, b->name, prod_name(b->queue_items[0]));
    return 1;
}

int consider_hurry(int id) {
    BASE* b = &Bases[id];
    int faction = b->faction_id;
    int t = b->queue_items[0];
    Faction* f = &Factions[faction];
    MFaction* m = &MFactions[faction];

    if (!(conf.hurry_items && t >= -FAC_ORBITAL_DEFENSE_POD && ~b->status_flags & BASE_HURRY_PRODUCTION))
        return 0;
    bool cheap = b->minerals_accumulated >= Rules->retool_exemption;
    int reserve = 20 + std::min(900, std::max(0, f->current_num_bases * std::min(30, (*current_turn - 20)/5)));
    int mins = mineral_cost(faction, t) - b->minerals_accumulated;
    int cost = (t < 0 ? 2*mins : mins*mins/20 + 2*mins) * (cheap ? 1 : 2) * m->rule_hurry / 100;
    int turns = (int)ceil(mins / std::max(0.01, 1.0 * b->mineral_surplus));

    if (!(cheap && mins > 0 && cost > 0 && f->energy_credits - cost > reserve))
        return 0;
    if (b->drone_total > b->talent_total && t < 0 && t == need_psych(id))
        return hurry_item(b, mins, cost);
    if (t < 0 && turns > 1) {
        if (t == -FAC_RECYCLING_TANKS || t == -FAC_PRESSURE_DOME
        || t == -FAC_RECREATION_COMMONS || t == -FAC_TREE_FARM
        || t == -FAC_HEADQUARTERS)
            return hurry_item(b, mins, cost);
        if (t == -FAC_CHILDREN_CRECHE && f->SE_growth >= 2)
            return hurry_item(b, mins, cost);
        if (t == -FAC_PERIMETER_DEFENSE && m->thinker_enemy_range < 15)
            return hurry_item(b, mins, cost);
        if (t == -FAC_AEROSPACE_COMPLEX && has_tech(faction, TECH_Orbital))
            return hurry_item(b, mins, cost);
    }
    if (t >= 0 && turns > 1) {
        if (m->thinker_enemy_range < 15 && Units[t].weapon_type <= WPN_PSI_ATTACK
        && !has_defenders(b->x, b->y, faction)) {
            return hurry_item(b, mins, cost);
        }
        if (Units[t].weapon_type == WPN_TERRAFORMING_UNIT && b->pop_size < 3) {
            return hurry_item(b, mins, cost);
        }
    }
    return 0;
}

int find_facility(int id) {
    const int build_order[][2] = {
        {FAC_RECREATION_COMMONS, 0},
        {FAC_CHILDREN_CRECHE, 0},
        {FAC_PERIMETER_DEFENSE, 2},
        {FAC_GENEJACK_FACTORY, 0},
        {FAC_NETWORK_NODE, 1},
        {FAC_AEROSPACE_COMPLEX, 0},
        {FAC_TREE_FARM, 0},
        {FAC_HAB_COMPLEX, 0},
        {FAC_COMMAND_CENTER, 2},
        {FAC_GEOSYNC_SURVEY_POD, 2},
        {FAC_FLECHETTE_DEFENSE_SYS, 2},
        {FAC_HABITATION_DOME, 0},
        {FAC_FUSION_LAB, 1},
        {FAC_ENERGY_BANK, 1},
        {FAC_RESEARCH_HOSPITAL, 1},
        {FAC_TACHYON_FIELD, 4},
        {FAC_QUANTUM_LAB, 5},
        {FAC_NANOHOSPITAL, 5},
    };
    BASE* base = &Bases[id];
    int faction = base->faction_id;
    int proj;
    int minerals = base->mineral_surplus;
    int extra = base->minerals_accumulated/10;
    int pop_rule = MFactions[faction].rule_population;
    int hab_complex_limit = Rules->pop_limit_wo_hab_complex - pop_rule;
    int hab_dome_limit = Rules->pop_limit_wo_hab_dome - pop_rule;
    Faction* f = &Factions[faction];
    MFaction* m = &MFactions[faction];
    bool sea_base = is_ocean(base);
    bool core_base = minerals+extra >= plans[faction].proj_limit;
    bool can_build_units = can_build_unit(faction, -1);

    if (base->drone_total > 0 && can_build(id, FAC_RECREATION_COMMONS))
        return -FAC_RECREATION_COMMONS;
    if (can_build(id, FAC_RECYCLING_TANKS))
        return -FAC_RECYCLING_TANKS;
    if (base->pop_size >= hab_complex_limit && can_build(id, FAC_HAB_COMPLEX))
        return -FAC_HAB_COMPLEX;
    if (core_base && (proj = find_project(id)) != 0) {
        return proj;
    }
    if (core_base && has_facility(id, FAC_AEROSPACE_COMPLEX)) {
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
        CFacility* fc = &Facility[c];
        /* Check if we have sufficient base energy for multiplier facilities. */
        if (t[1] & 1 && base->energy_surplus < 2*fc->maint + fc->cost/(f->AI_tech ? 2 : 1))
            continue;
        /* Avoid building combat-related facilities in peacetime. */
        if (t[1] & 2 && m->thinker_enemy_range > 40 - std::min(12, 3*fc->maint))
            continue;
        /* Build these facilities only if the global unit limit is reached. */
        if (t[1] & 4 && can_build_units)
            continue;
        if (c == FAC_TREE_FARM && sea_base && base->energy_surplus < 2*fc->maint + fc->cost)
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
    int probes = 0;
    for (int i=0; i<*total_num_vehicles; i++) {
        VEH* veh = &Vehicles[i];
        if (veh->faction_id == faction && veh->home_base_id == id
        && Units[veh->unit_id].weapon_type == WPN_PROBE_TEAM) {
            probes++;
        }
    }
    return select_combat(id, probes, sea_base, can_build_ships(id), false);
}

int select_colony(int id, int pods, bool build_ships) {
    BASE* base = &Bases[id];
    int faction = base->faction_id;
    bool land = has_base_sites(base->x, base->y, faction, TRIAD_LAND);
    bool sea = has_base_sites(base->x, base->y, faction, TRIAD_SEA);
    int limit = (*current_turn < 60 || (!random(3) && (land || (build_ships && sea))) ? 2 : 1);

    if (pods >= limit) {
        return -1;
    }
    if (is_ocean(base)) {
        for (const auto& m : iterate_tiles(base->x, base->y, 1, 9)) {
            if (land && non_combat_move(m.x, m.y, faction, TRIAD_LAND)) {
                if (m.sq->owner < 1 || !random(6)) {
                    return TRIAD_LAND;
                }
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

bool need_scouts(int x, int y, int faction, int scouts) {
    MAP* sq = mapsq(x, y);
    Faction* f = &Factions[faction];
    if (is_ocean(sq)) {
        return false;
    }
    int score = (*current_turn < 40 ? 6 : 3) - 3*scouts
        + std::min(8, (f->region_territory_goodies[sq->region]
        / std::max(1, (int)f->region_total_bases[sq->region])));
    return random(16) < score;
}

int select_combat(int base_id, int probes, bool sea_base, bool build_ships, bool def_land) {
    BASE* base = &Bases[base_id];
    int faction = base->faction_id;
    Faction* f = &Factions[faction];
    bool reserve = base->mineral_surplus >= base->mineral_intake/2;

    int w1 = 4*plans[faction].aircraft < f->current_num_bases ? 2 : 5;
    int w2 = 4*plans[faction].transports < f->current_num_bases ? 2 : 5;
    int w3 = plans[faction].prioritize_naval ? 2 : 5;

    if (has_weapon(faction, WPN_PROBE_TEAM) && (!random(probes + 4) || !reserve)) {
        return find_proto(base_id, (build_ships ? TRIAD_SEA : TRIAD_LAND), WMODE_INFOWAR, DEF);

    } else if (has_chassis(faction, CHS_NEEDLEJET) && f->SE_police >= -3 && !random(w1)) {
        return find_proto(base_id, TRIAD_AIR, COMBAT, ATT);

    } else if (build_ships && (sea_base || (!def_land && !random(w3)))) {
        return find_proto(base_id, TRIAD_SEA, (!random(w2) ? WMODE_TRANSPORT : COMBAT), ATT);
    }
    return find_proto(base_id, TRIAD_LAND, COMBAT, (!random(4) ? DEF : ATT));
}

int select_prod(int id) {
    BASE* base = &Bases[id];
    MAP* sq1 = mapsq(base->x, base->y);
    int faction = base->faction_id;
    int minerals = base->mineral_surplus;
    Faction* f = &Factions[faction];
    MFaction* m = &MFactions[faction];
    AIPlans* p = &plans[faction];

    int reserve = std::max(2, base->mineral_intake / 2);
    bool has_formers = has_weapon(faction, WPN_TERRAFORMING_UNIT);
    bool has_supply = has_weapon(faction, WPN_SUPPLY_TRANSPORT);
    bool build_ships = can_build_ships(id);
    bool sea_base = is_ocean(base);
    int might = faction_might(faction);
    int transports = 0;
    int landprobes = 0;
    int seaprobes = 0;
    int defenders = 0;
    int crawlers = 0;
    int formers = 0;
    int scouts = 0;
    int pods = 0;
    int enemyrange = 40;
    double enemymil = 0;

    for (int i=1; i < MaxPlayerNum; i++) {
        if (i == faction || ~f->diplo_status[i] & DIPLO_COMMLINK) {
            continue;
        }
        double mil = std::min(5.0, (is_human(faction) ? 2.0 : 1.0) * faction_might(i) / might);
        if (at_war(faction, i)) {
            enemymil = std::max(enemymil, 1.0 * mil);
        } else if (!has_pact(faction, i)) {
            enemymil = std::max(enemymil, 0.3 * mil);
        }
    }
    for (int i=0; i < *total_num_bases; i++) {
        BASE* b = &Bases[i];
        if (at_war(faction, b->faction_id)) {
            int range = map_range(base->x, base->y, b->x, b->y);
            MAP* sq2 = mapsq(b->x, b->y);
            if (sq1 && sq2 && sq1->region != sq2->region) {
                range = 3*range/2;
            }
            enemyrange = std::min(enemyrange, range);
        }
    }
    for (int i=0; i < *total_num_vehicles; i++) {
        VEH* veh = &Vehicles[i];
        UNIT* u = &Units[veh->unit_id];
        if (veh->faction_id != faction) {
            continue;
        }
        if (veh->home_base_id == id) {
            if (u->weapon_type == WPN_TERRAFORMING_UNIT) {
                formers++;
            } else if (u->weapon_type == WPN_COLONY_MODULE) {
                pods++;
            } else if (u->weapon_type == WPN_PROBE_TEAM) {
                if (veh->triad() == TRIAD_LAND) {
                    landprobes++;
                } else {
                    seaprobes++;
                }
            } else if (u->weapon_type == WPN_SUPPLY_TRANSPORT) {
                crawlers++;
                if (veh->move_status != ORDER_CONVOY) {
                    has_supply = false;
                }
            } else if (u->weapon_type == WPN_TROOP_TRANSPORT) {
                transports++;
            }
        }
        if (veh->is_combat_unit() && veh->triad() == TRIAD_LAND) {
            if (map_range(base->x, base->y, veh->x, veh->y) <= 1) {
                defenders++;
            }
            if (veh->home_base_id == id) {
                scouts++;
            }
        }
    }
    bool build_pods = ~*game_rules & RULES_SCN_NO_COLONY_PODS
        && (base->pop_size > 1 || base->nutrient_surplus > 1)
        && pods < 2 && expansion_ratio(faction) < 1.0
        && *total_num_bases < MaxBaseNum * 19 / 20;
    m->thinker_enemy_range = (enemyrange + 9 * m->thinker_enemy_range)/10;

    double w1 = std::min(1.0, std::max(0.5, 1.0 * minerals / p->proj_limit));
    double w2 = 2.0 * enemymil / (m->thinker_enemy_range * 0.1 + 0.1) + 0.5 * p->enemy_bases
        + std::min(1.0, f->current_num_bases / 24.0) * (f->AI_fight * 0.2 + 0.8);
    double threat = 1 - (1 / (1 + std::max(0.0, w1 * w2)));

    debug("select_prod %d %d %2d %2d def: %d frm: %d prb: %d crw: %d pods: %d expand: %d "\
        "scouts: %d min: %2d res: %2d limit: %2d range: %2d mil: %.4f threat: %.4f\n",
        *current_turn, faction, base->x, base->y,
        defenders, formers, landprobes+seaprobes, crawlers, pods, build_pods,
        scouts, minerals, reserve, p->proj_limit, enemyrange, enemymil, threat);

    if (minerals > 2 && defenders < 1 || need_scouts(base->x, base->y, faction, scouts)) {
        return find_proto(id, TRIAD_LAND, COMBAT, DEF);

    } else if (*climate_future_change > 0 && is_shore_level(mapsq(base->x, base->y))
    && can_build(id, FAC_PRESSURE_DOME)) {
            return -FAC_PRESSURE_DOME;

    } else if (!conf.auto_relocate_hq && relocate_hq(id)) {
        return -FAC_HEADQUARTERS;

    } else if (minerals > reserve && random(100) < (int)(100.0 * threat)) {
        return select_combat(id, landprobes+seaprobes, sea_base, build_ships,
            (defenders < 2 && enemyrange < 12));

    } else if (has_formers && formers < (base->pop_size < (sea_base ? 4 : 3) ? 1 : 2)
    && (need_formers(base->x, base->y, faction) || (!formers && !random(6)))) {
        if (minerals >= 8 && has_chassis(faction, CHS_GRAVSHIP)) {
            int unit = find_proto(id, TRIAD_AIR, WMODE_TERRAFORMER, DEF);
            if (unit >= 0 && Units[unit].triad() == TRIAD_AIR) {
                return unit;
            }
        }
        if (sea_base || (build_ships && formers == 1 && !random(3))) {
            return find_proto(id, TRIAD_SEA, WMODE_TERRAFORMER, DEF);
        } else {
            return find_proto(id, TRIAD_LAND, WMODE_TERRAFORMER, DEF);
        }

    } else if (build_ships && has_weapon(faction, WPN_PROBE_TEAM)
    && ocean_shoreline(base->x, base->y) && !random(seaprobes > 0 ? 6 : 3)
    && p->unknown_factions > 1 && p->contacted_factions < 2) {
        return find_proto(id, TRIAD_SEA, WMODE_INFOWAR, DEF);

    } else {
        int crawl_target = 1 + std::min(base->pop_size/4,
            (minerals >= p->proj_limit ? 2 : 1));

        if (minerals >= 12 && base->eco_damage > 0 && can_build(id, FAC_TREE_FARM)
        && Factions[faction].SE_planet_base >= 0) {
            return -FAC_TREE_FARM;

        } else if (has_supply && !sea_base && crawlers < crawl_target) {
            return find_proto(id, TRIAD_LAND, WMODE_CONVOY, DEF);

        } else if (build_ships && !transports && mapnodes.count({base->x, base->y, NODE_NEED_FERRY})) {
            return find_proto(id, TRIAD_SEA, WMODE_TRANSPORT, DEF);

        } else if (build_pods && !can_build(id, FAC_RECYCLING_TANKS)) {
            int tr = select_colony(id, pods, build_ships);
            if (tr == TRIAD_LAND || tr == TRIAD_SEA) {
                return find_proto(id, tr, WMODE_COLONIST, DEF);
            }
        }
        return find_facility(id);
    }
}

int social_score(int faction, int sf, int sm, int range, bool pop_boom, bool has_nexus,
int robust, int immunity, int impunity, int penalty) {
    enum {ECO, EFF, SUP, TAL, MOR, POL, GRW, PLA, PRO, IND, RES};
    Faction* f = &Factions[faction];
    MFaction* m = &MFactions[faction];
    double base_ratio = std::min(1.0, f->current_num_bases / std::min(40.0, *map_area_sq_root * 0.5));
    int morale = (has_project(faction, FAC_COMMAND_NEXUS) ? 2 : 0)
        + (has_project(faction, FAC_CYBORG_FACTORY) ? 2 : 0);
    int sc = 0;
    int vals[11];

    if ((&f->SE_Politics)[sf] == sm) {
        /* Evaluate the current active social model. */
        memcpy(vals, &f->SE_economy, sizeof(vals));
    } else {
        /* Take the faction base social values and apply all modifiers. */
        memcpy(vals, &f->SE_economy_base, sizeof(vals));

        for (int i=0; i<4; i++) {
            int j = (sf == i ? sm : (&f->SE_Politics)[i]);
            for (int k=0; k<11; k++) {
                int val = Social[i].effects[j][k];
                if ((1 << (i*4 + j)) & impunity) {
                    val = std::max(0, val);
                } else if ((1 << (i*4 + j)) & penalty) {
                    val = val * (val < 0 ? 2 : 1);
                }
                vals[k] += val;
            }
        }
        if (has_project(faction, FAC_ASCETIC_VIRTUES)) {
            vals[POL] += 1;
        }
        if (has_project(faction, FAC_LIVING_REFINERY)) {
            vals[SUP] += 2;
        }
        if (has_nexus) {
            if (is_alien(faction)) {
                vals[RES]++;
            }
            vals[PLA]++;
        }
        for (int k=0; k<11; k++) {
            if ((1 << k) & immunity) {
                vals[k] = std::max(0, vals[k]);
            } else if ((1 << k) & robust && vals[k] < 0) {
                vals[k] /= 2;
            }
        }
    }
    if (m->soc_priority_category >= 0 && m->soc_priority_model >= 0) {
        if (sf == m->soc_priority_category) {
            if (sm == m->soc_priority_model) {
                sc += conf.social_ai_bias;
            } else if (sm != SOCIAL_M_FRONTIER) {
                sc -= conf.social_ai_bias;
            }
        } else {
            if ((&f->SE_Politics)[m->soc_priority_category] == m->soc_priority_model) {
                sc += conf.social_ai_bias;
            } else if ((&f->SE_Politics)[m->soc_priority_category] != SOCIAL_M_FRONTIER) {
                sc -= conf.social_ai_bias;
            }
        }
    }
    if (vals[ECO] >= 2) {
        sc += (vals[ECO] >= 4 ? 16 : 12);
    }
    if (vals[EFF] < -2) {
        sc -= (vals[EFF] < -3 ? 20 : 14);
    }
    if (vals[SUP] < -3) {
        sc -= 10;
    }
    if (vals[SUP] < 0) {
        sc -= (int)((1.0 - base_ratio) * 10.0);
    }
    if (vals[MOR] >= 1 && vals[MOR] + morale >= 4) {
        sc += 10;
    }
    if (vals[PRO] >= 3 && !has_project(faction, FAC_HUNTER_SEEKER_ALGO)) {
        sc += 4 * std::max(0, 4 - range/8);
    }
    sc += std::max(2, 2 + 4*f->AI_wealth + 3*f->AI_tech - f->AI_fight)
        * std::min(5, std::max(-3, vals[ECO]));
    sc += std::max(2, 2*(f->AI_wealth + f->AI_tech) - f->AI_fight + (int)(4*base_ratio))
        * (std::min(6, vals[EFF]) + (vals[EFF] >= 3 ? 2 : 0));
    sc += std::max(3, 3 + 2*f->AI_power + 2*f->AI_fight)
        * std::min(3, std::max(-4, vals[SUP]));
    sc += std::max(2, 4 - range/8 + 2*f->AI_power + 2*f->AI_fight)
        * std::min(4, std::max(-4, vals[MOR]));
    if (!has_project(faction, FAC_TELEPATHIC_MATRIX)) {
        sc += (vals[POL] < 0 ? 2 : 4) * std::min(3, std::max(-5, vals[POL]));
        if (vals[POL] < -2) {
            sc -= (vals[POL] < -3 ? 4 : 2) * std::max(0, 4 - range/8)
                * (has_chassis(faction, CHS_NEEDLEJET) ? 2 : 1);
        }
        if (has_project(faction, FAC_LONGEVITY_VACCINE) && sf == SOCIAL_C_ECONOMICS) {
            sc += (sm == SOCIAL_M_PLANNED ? 10 : 0);
            sc += (sm == SOCIAL_M_SIMPLE || sm == SOCIAL_M_GREEN ? 5 : 0);
        }
        sc += std::min(5, std::max(-5, vals[TAL])) * 3;
    }
    if (!has_project(faction, FAC_CLONING_VATS)) {
        if (pop_boom && vals[GRW] >= 4) {
            sc += 20;
        }
        if (vals[GRW] < -2) {
            sc -= 10;
        }
        sc += (pop_boom ? 6 : 3) * std::min(6, std::max(-3, vals[GRW]));
    }
    sc += std::max(2, (f->SE_planet_base > 0 ? 5 : 2) + m->rule_psi/10
        + (has_project(faction, FAC_MANIFOLD_HARMONICS) ? 6 : 0)) * std::min(3, std::max(-3, vals[PLA]));
    sc += std::max(2, 4 - range/8 + 2*f->AI_power + 2*f->AI_fight) * std::min(3, std::max(-2, vals[PRO]));
    sc += 8 * std::min(8 - *diff_level, std::max(-3, vals[IND]));
    sc += std::max(2, 3 + 4*f->AI_tech + 2*(f->AI_wealth - f->AI_fight))
        * std::min(5, std::max(-5, vals[RES]));

    debug("social_score %d %d %d %d %s\n", faction, sf, sm, sc, Social[sf].soc_name[sm]);
    return sc;
}

HOOK_API int mod_social_ai(int faction, int v1, int v2, int v3, int v4, int v5) {
    Faction* f = &Factions[faction];
    MFaction* m = &MFactions[faction];
    bool pop_boom = 0;
    int want_pop = 0;
    int pop_total = 0;
    int robust = 0;
    int immunity = 0;
    int impunity = 0;
    int penalty = 0;

    if (!conf.social_ai || !ai_enabled(faction)) {
        return social_ai(faction, v1, v2, v3, v4, v5);
    }
    if (f->SE_upheaval_cost_paid > 0) {
        social_set(faction);
        return 0;
    }
    int range = (int)(m->thinker_enemy_range / (1.0 + 0.1 * std::min(4, plans[faction].enemy_bases)));
    bool has_nexus = (manifold_nexus_owner() == faction);
    assert(!memcmp(&f->SE_Politics, &f->SE_Politics_pending, 16));

    for (int i=0; i<m->faction_bonus_count; i++) {
        if (m->faction_bonus_id[i] == FCB_ROBUST) {
            robust |= (1 << m->faction_bonus_val1[i]);
        }
        if (m->faction_bonus_id[i] == FCB_IMMUNITY) {
            immunity |= (1 << m->faction_bonus_val1[i]);
        }
        if (m->faction_bonus_id[i] == FCB_IMPUNITY) {
            impunity |= (1 << (m->faction_bonus_val1[i] * 4 + m->faction_bonus_val2[i]));
        }
        if (m->faction_bonus_id[i] == FCB_PENALTY) {
            penalty |= (1 << (m->faction_bonus_val1[i] * 4 + m->faction_bonus_val2[i]));
        }
    }
    if (has_project(faction, FAC_NETWORK_BACKBONE)) {
        /* Cybernetic */
        impunity |= (1 << (4*SOCIAL_C_FUTURE + SOCIAL_M_CYBERNETIC));
    }
    if (has_project(faction, FAC_CLONING_VATS)) {
        /* Power & Thought Control */
        impunity |= (1 << (4*SOCIAL_C_VALUES + SOCIAL_M_POWER))
            | (1 << (4*SOCIAL_C_FUTURE + SOCIAL_M_THOUGHT_CONTROL));

    } else if (has_tech(faction, Facility[FAC_CHILDREN_CRECHE].preq_tech)) {
        for (int i=0; i<*total_num_bases; i++) {
            BASE* b = &Bases[i];
            if (b->faction_id == faction) {
                want_pop += (pop_goal(i) - b->pop_size)
                    * (b->nutrient_surplus > 1 && has_facility(i, FAC_CHILDREN_CRECHE) ? 4 : 1);
                pop_total += b->pop_size;
            }
        }
        if (pop_total > 0) {
            pop_boom = ((f->SE_growth < 4 ? 1 : 2) * want_pop) >= pop_total;
        }
    }
    debug("social_params %d %d %8s range: %2d has_nexus: %d pop_boom: %d want_pop: %3d pop_total: %3d "\
        "robust: %04x immunity: %04x impunity: %04x penalty: %04x\n", *current_turn, faction, m->filename,
        range, has_nexus, pop_boom, want_pop, pop_total, robust, immunity, impunity, penalty);
    int score_diff = 1 + map_hash(*current_turn, faction) % 6;
    int sf = -1;
    int sm2 = -1;

    for (int i=0; i<4; i++) {
        int sm1 = (&f->SE_Politics)[i];
        int sc1 = social_score(faction, i, sm1, range, pop_boom, has_nexus, robust, immunity, impunity, penalty);

        for (int j=0; j<4; j++) {
            if (j == sm1 || !has_tech(faction, Social[i].soc_preq_tech[j]) ||
            (i == m->soc_opposition_category && j == m->soc_opposition_model)) {
                continue;
            }
            int sc2 = social_score(faction, i, j, range, pop_boom, has_nexus, robust, immunity, impunity, penalty);
            if (sc2 - sc1 > score_diff) {
                sf = i;
                sm2 = j;
                score_diff = sc2 - sc1;
            }
        }
    }
    int cost = (is_alien(faction) ? 36 : 24);
    if (sf >= 0 && f->energy_credits > cost) {
        int sm1 = (&f->SE_Politics)[sf];
        (&f->SE_Politics_pending)[sf] = sm2;
        f->energy_credits -= cost;
        f->SE_upheaval_cost_paid += cost;
        debug("social_change %d %d %8s cost: %2d score_diff: %2d %s -> %s\n",
            *current_turn, faction, m->filename,
            cost, score_diff, Social[sf].soc_name[sm1], Social[sf].soc_name[sm2]);
    }
    social_set(faction);
    consider_designs(faction);
    return 0;
}




