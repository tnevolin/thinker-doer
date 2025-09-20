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

#pragma once

#ifdef BUILD_REL
    #define MOD_VERSION "Thinker Mod v5.1 - The Will to Power mod v389"
#else
    #define MOD_VERSION "Thinker Mod develop build"
#endif

#ifdef BUILD_DEBUG
    #define MOD_DATE __DATE__ " " __TIME__
    #define DEBUG 1
    #define debug(...) fprintf(debug_log, __VA_ARGS__);
    #define debugw(...) { fprintf(debug_log, __VA_ARGS__); \
        fflush(debug_log); }
    #define debug_ver(...) if (conf.debug_verbose) { fprintf(debug_log, __VA_ARGS__); }
    #define flushlog() fflush(debug_log);
#else
    #define MOD_DATE __DATE__
    #define DEBUG 0
    #define NDEBUG /* Disable assertions */
    #define debug(...) /* Nothing */
    #define debugw(...) /* Nothing */
    #define debug_ver(...) /* Nothing */
    #define flushlog()
    #ifdef __GNUC__
    #pragma GCC diagnostic ignored "-Wunused-variable"
    #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
    #endif
#endif

#ifdef __GNUC__
    #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
    #pragma GCC diagnostic ignored "-Wchar-subscripts"
    #pragma GCC diagnostic ignored "-Wattributes"
    #pragma GCC diagnostic error "-Wreturn-type"
#else
    #define UNUSED(x) UNUSED_ ## x
#endif

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <windows.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include <psapi.h>
#include <set>
#include <list>
#include <queue>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <unordered_map>

#define DLL_EXPORT extern "C" __declspec(dllexport)
#define GameAppName "Alpha Centauri"
#define GameIniFile ".\\Alpha Centauri.ini"
#define ModAppName "thinker"
#define ModIniFile ".\\thinker.ini"

#ifdef BUILD_DEBUG
#ifdef assert
#undef assert
#define assert(_Expression) \
((!!(_Expression)) \
|| (fprintf(debug_log, "Assertion Failed: %s %s %d\n", #_Expression, __FILE__, __LINE__) \
&& (_assert(#_Expression, __FILE__, __LINE__), 0)))
#endif
#endif

const bool DEF = true;
const bool ATT = false;

const int MaxMapDist = 1024;
const int MaxNaturalNum = 16;
const int MaxLandmarkNum = 64;
const int MaxRegionNum = 128;
const int MaxRegionLandNum = 64;
const int RegionBounds = 63;

const int MaxDiffNum = 6;
const int MaxPlayerNum = 8;
const int MaxGoalsNum = 75;
const int MaxSitesNum = 25;
const int MaxBaseNum = 512;
const int MaxVehNum = 2048;
const int MaxProtoNum = 512;
const int MaxProtoFactionNum = 64;
const int MaxBaseNameLen = 25;
const int MaxProtoNameLen = 32;
const int MaxBasePopSize = 127;
const int MaxBaseSpecNum = 16;

const int MaxTechnologyNum = 89;
const int MaxChassisNum = 9;
const int MaxWeaponNum = 26;
const int MaxArmorNum = 14;
const int MaxReactorNum = 4;
const int MaxAbilityNum = 29;
const int MaxMoraleNum = 7;
const int MaxDefenseModeNum = 3;
const int MaxOffenseModeNum = 3;
const int MaxOrderNum = 30;
const int MaxPlanNum = 15;
const int MaxTriadNum = 3;

const int MaxResourceInfoNum = 9;
const int MaxTimeControlNum = 6;
const int MaxCompassNum = 8;
const int MaxResourceNum = 4;
const int MaxEnergyNum = 3;

const int MaxMandateNum = 4;
const int MaxFacilityNum = 64; // 0 slot unused
const int MaxSecretProjectNum = 64;
const int MaxTerrainNum = 20;
const int MaxSocialCatNum = 4;
const int MaxSocialModelNum = 4;
const int MaxSocialEffectNum = 11;
const int MaxSpecialistNum = 7;
const int MaxCitizenNum = 10;
const int MaxMoodNum = 9;
const int MaxReputeNum = 8;
const int MaxMightNum = 7;
const int MaxBonusNum = 8;
const int MaxBonusNameNum = 41;
const int MaxProposalNum = 11;

const int StrBufLen = 256;
const int LineBufLen = 128;
const int MaxEnemyRange = 50;

enum VideoMode {
    VM_Native = 0,
    VM_Custom = 1,
    VM_Window = 2,
};

struct LMConfig {
    int crater = 1;
    int volcano = 1;
    int jungle = 1;
    int uranium = 1;
    int sargasso = 1;
    int ruins = 1;
    int dunes = 1;
    int fresh = 1;
    int mesa = 1;
    int canyon = 0;
    int geothermal = 1;
    int ridge = 1;
    int borehole = 1;
    int nexus = 1;
    int unity = 1;
    int fossil = 1;
};

/*
Config parsed from thinker.ini. Alpha Centauri.ini related options
can be set negative values to use the defaults from Alpha Centauri.ini.
*/
struct Config {
    int video_mode = VM_Native;
    int window_width = 1024;
    int window_height = 768;
    int minimised = 0; // internal variable
    int video_player = 2; // internal variable
    int playing_movie = 0;  // internal variable
    int screen_width = 1024; // internal variable
    int screen_height = 768; // internal variable
    int directdraw = 0;
    int disable_opening_movie = -1;
    int smac_only = 0;
    int smooth_scrolling = 0;
    int scroll_area = 40;
    int auto_minimise = 0;
    int render_base_info = 1;
    int render_high_detail = 1; // unlisted option
    int autosave_interval = 1;
    int warn_on_former_replace = 1;
    int manage_player_bases = 0;
    int manage_player_units = 0;
    int render_probe_labels = 1;
    int foreign_treaty_popup = 0;
    int editor_free_units = 1;
    int new_base_names = 1;
    int new_unit_names = 1;
    int spawn_free_units[9] = {0,0,1,0,0,1,1,0,1};
    int player_colony_pods = 0;
    int computer_colony_pods = 0;
    int player_formers = 0;
    int computer_formers = 0;
    int player_satellites[3] = {0,0,0};
    int computer_satellites[3] = {0,0,0};
    int design_units = 1;
    int factions_enabled = 7;
    int social_ai = 1;
    int social_ai_bias = 10;
    int tech_balance = 0;
    int base_hurry = 0;
    int base_spacing = 3;
    int base_nearby_limit = -1;
    int expansion_limit = 100;
    int expansion_autoscale = 0;
    int limit_project_start = 0;
    int max_satellites = 20;
    int new_world_builder = 1;
    int world_sea_levels[3] = {46,58,70};
    int world_hills_mod = 40;
    int world_ocean_mod = 40;
    int world_islands_mod = 16;
    int world_continents = 0;
    int world_polar_caps = 1;
    int world_mirror_x = 0;
    int world_mirror_y = 0;
    int modified_landmarks = 0;
    int time_warp_mod = 1;
    int time_warp_techs = 5;
    int time_warp_projects = 1;
    int time_warp_start_turn = 40;
    int faction_placement = 1;
    int nutrient_bonus = 0;
    int rare_supply_pods = 0;
    int simple_cost_factor = 0;
    int revised_tech_cost = 1;
    int tech_cost_factor[MaxDiffNum] = {124,116,108,100,84,76};
    int tech_rate_modifier = 100; // internal variable
    int tech_stagnate_rate = 200;
    int fast_fungus_movement = 0;
    int magtube_movement_rate = 0;
    int road_movement_rate = 1; // internal variable
    int max_movement_rate = 255; // internal variable
    int chopper_attack_rate = 1;
    int base_psych = 1;
    int nerve_staple = 2;
    int nerve_staple_mod = -10;
    int delay_drone_riots = 0;
    int activate_skipped_units = 1; // unlisted option
    int counter_espionage = 0;
    int ignore_reactor_power = 0;
    int long_range_artillery = 0;
    int modify_upgrade_cost = 0;
    int modify_unit_support = 0;
    int modify_unit_limit = 0;
    int max_veh_num = MaxVehNum; // internal variable
    int skip_default_balance = 1; // unlisted option
    int early_research_start = 1; // unlisted option
    int facility_capture_fix = 1; // unlisted option
    int territory_border_fix = 1;
    int auto_relocate_hq = 1;
    int simple_hurry_cost = 1;
    int eco_damage_fix = 1;
    int clean_minerals = 16;
    int biology_lab_bonus = 2;
    int spawn_fungal_towers = 1;
    int spawn_spore_launchers = 1;
    int spawn_sealurks = 1;
    int spawn_battle_ogres = 1;
    int planetpearls = 1;
    int event_perihelion = 1;
    int event_sunspots = 10;
    int event_market_crash = 1;
    int altitude_limit = 6; // internal variable
    int tile_output_limit[3] = {2,2,2};
    int soil_improve_value = 0;
    int aquatic_bonus_minerals = 1;
    int alien_guaranteed_techs = 1;
    int alien_early_start = 0;
    int cult_early_start = 0;
    int normal_elite_moves = 1;
    int native_elite_moves = 0;
    int native_weak_until_turn = -1;
    int native_lifecycle_levels[6] = {40,80,120,160,200,240};
    int facility_defense_bonus[4] = {100,100,100,100};
    int neural_amplifier_bonus = 50;
    int dream_twister_bonus = 50;
    int fungal_tower_bonus = 50;
    int planet_defense_bonus = 0;
    int sensor_defense_ocean = 0;
    int collateral_damage_value = 3;
    int content_pop_player[MaxDiffNum] = {6,5,4,3,2,1};
    int content_pop_computer[MaxDiffNum] = {3,3,3,3,3,3};
    int unit_support_bonus[MaxDiffNum] = {0,0,0,0,0,0};
    int repair_minimal = 1;
    int repair_fungus = 2;
    int repair_friendly = 1;
    int repair_airbase = 1;
    int repair_bunker = 1;
    int repair_base = 1;
    int repair_base_native = 10;
    int repair_base_facility = 10;
    int repair_nano_factory = 10;
    int repair_battle_ogre = 0;
    LMConfig landmarks;
    int minimal_popups = 0; // unlisted option
    int diplo_patience = 0; // internal variable
    int skip_random_factions = 0; // internal variable
    int faction_file_count = 14; // internal variable
    int reduced_mode = 0; // internal variable
    int debug_mode = DEBUG; // internal variable
    int debug_verbose = DEBUG; // internal variable
    
	//  [WTP]
    bool alternative_weapon_icon_selection_algorithm = false;
    // implemented in Thinker?
    bool alternative_prototype_cost_formula = false;
    int reactor_cost_factors[4];
    bool hurry_minimal_minerals = false;
    bool flat_hurry_cost = false;
    int flat_hurry_cost_multiplier_unit = 1;
    int flat_hurry_cost_multiplier_facility = 1;
    int flat_hurry_cost_multiplier_project = 1;
    bool collateral_damage_defender_reactor = false;
    bool uniform_promotions = true; // internal configuration
    bool very_green_no_defense_bonus = true; // internal configuration
    bool sea_territory_distance_same_as_land = false;
    bool alternative_artillery_damage = false;
    bool disable_home_base_morale_effect = false;
    bool disable_current_base_morale_effect = false;
    bool default_morale_very_green = false;
    int road_movement_cost = 0;
    bool cloning_vats_disable_impunities = false;
    int cloning_vats_se_growth = 0;
    int se_growth_rating_min = -2;
    int se_growth_rating_max = 5;
    bool pop_boom_requires_golden_age = false;
    int pop_boom_requires_growth_rating = 6;
    double ocean_depth_multiplier = 1.0;
    int pts_new_base_size_less_average = 2;
    bool hsa_does_not_kill_probe = false;
    bool condenser_and_enricher_do_not_multiply_nutrients = false;
    bool alternative_tech_cost = false;
    bool fix_mineral_contribution = true; // internal setting
    bool modified_probe_risks = false;
    int probe_risk_procure_research_data = 0;
    int probe_risk_introduce_genetic_plague = 0;
    bool sensor_offense = false;
    bool sensor_offense_ocean = false;
    bool break_treaty_before_fight = false;
    int habitation_facility_growth_bonus = 0;
    bool unit_upgrade_ignores_movement = false;
    bool ai_base_allowed_fungus_rocky = false;
    bool silent_vendetta_warning = false;
    bool design_cost_in_rows = false;
    bool carry_over_nutrients = false;
    bool carry_over_minerals = false;
    bool subversion_allow_stacked_units = false;
    bool alternative_subversion_and_mind_control = false;
    double alternative_subversion_and_mind_control_scale;
    double alternative_subversion_unit_cost_multiplier;
	double alternative_mind_control_nutrient_cost;
	double alternative_mind_control_mineral_cost;
	double alternative_mind_control_energy_cost;
	double alternative_mind_control_facility_cost_multiplier;
	double alternative_mind_control_project_cost_multiplier;
	double alternative_mind_control_happiness_power_base;
	bool capture_base_destroys_facilities;
	bool alternative_support = false;
	int instant_completion_fixed_minerals;
	bool sensor_indestructible;
	bool artillery_duel_uses_bonuses;
	bool probe_combat_uses_bonuses;
	bool disable_vanilla_base_hurry;
	bool science_projects_alternative_labs_bonus;
	int science_projects_supercollider_labs_bonus;
	int science_projects_theoryofeverything_labs_bonus;
	int science_projects_universaltranslator_labs_bonus;
	bool disengagement_from_stack;
	int pressure_dome_minerals = 0;
	bool pressure_dome_recycling_tanks_bonus = 1;
	int tech_trade_likeability = 0x12;
	int conventional_power_psi_percentage = 0;
	bool zoc_enabled = 1;
	int aaa_range = -1;
	bool base_psych_improved = false;
	int base_psych_cost = 2;
	int base_psych_economy_conversion_ratio = 0;
	int base_psych_facility_extra_power = 0;
	int base_psych_police_extra_power = 0;
    bool facility_terraforming_ecodamage_halved = false;
    int tree_farm_yield_bonus_forest[3] = {0,0,0};
    int hybrid_forest_yield_bonus_forest[3] = {0,0,0};
    bool recycling_tanks_mineral_multiplier = true;
    bool recycling_tanks_population_bonus;
    bool genejack_factory_mineral_multiplier = true;
    int genejack_factory_mineral_bonus_mining_platform = 0;
    int genejack_factory_mineral_bonus_regular_mine = 0;
    int genejack_factory_mineral_bonus_rocky_mine = 0;
    int energy_multipliers_tree_farm[3] = {0,0,0};
    int energy_multipliers_hybrid_forest[3] = {0,0,0};
    int energy_multipliers_centauri_preserve[3] = {0,0,0};
    int energy_multipliers_temple_of_planet[3] = {0,0,0};
    int echelon_mirror_bonus = 1;
    bool base_inefficiency_alternative = false;
    bool drone_riot_intensifies = true;
    int se_morale_combat_bonus = 0;
    int se_police_excess_industry_bonus = 0;
    int isle_of_deep_offense_bonus = 0;
    int isle_of_deep_defense_bonus = 0;
    int facility_yield_bonus_biology_lab[3] = {0,0,0};
    bool worker_algorithm_enable_alternative = false;
    double worker_algorithm_energy_weight = 0.0;
    double worker_algorithm_growth_multiplier = 0.0;
    int worker_algorithm_minimal_nutrient_surplus = 0;
    int worker_algorithm_minimal_mineral_surplus = 0;
    int worker_algorithm_minimal_energy_surplus = 0;
    double worker_algorithm_nutrient_preference = 0.0;
    double worker_algorithm_mineral_preference = 0.0;
    double worker_algorithm_energy_preference = 0.0;
    int diplomacy_improvement_chance_human_faction_revenge = 0;
    int diplomacy_improvement_chance_human_major_atrocity = 0;
    int diplomacy_improvement_chance_ai_faction_revenge = 0;
    int diplomacy_improvement_chance_ai_major_atrocity = 0;
    int diplomacy_aggressiveness_effect_interval = 1;
    int diplomacy_se_choice_effect_interval = 2;
    int diplomacy_probe_action_vendetta_global_friction = 2;
    bool bunker_ignores_terrain = false;
    int bunker_bonus_surface = 50;
    int bunker_bonus_aerial = 0;
    bool display_numeric_mood = false;
    int manifold_harmonics_min_planet_rating = 0;
    bool fungus_terrain_bonus = false;
    int promotion_probability_denominator = 1;
    // AI configurations
    bool ai_useWTPAlgorithms;
    bool wtp_enabled_factions[MaxPlayerNum];
    double ai_resource_score_nutrient;
    double ai_resource_score_mineral;
    double ai_resource_score_energy;
    double ai_faction_mineral_a0;
    double ai_faction_mineral_a1;
    double ai_faction_mineral_a2;
    double ai_faction_budget_a0;
    double ai_faction_budget_a1;
    double ai_faction_budget_a2;
    double ai_faction_budget_a3;
    double ai_statistics_base_mineral_intake_sqrt_a;
    double ai_statistics_base_mineral_intake_sqrt_b;
    double ai_statistics_base_mineral_intake_sqrt_c;
    double ai_base_mineral_intake_a;
    double ai_base_mineral_intake_b;
    double ai_base_mineral_intake_c;
    double ai_base_mineral_intake2_a;
    double ai_base_mineral_intake2_b;
    double ai_base_mineral_multiplier_a;
    double ai_base_mineral_multiplier_b;
    double ai_base_mineral_multiplier_c;
    double ai_base_budget_intake_a;
    double ai_base_budget_intake_b;
    double ai_base_budget_intake2_a;
    double ai_base_budget_intake2_b;
    double ai_base_budget_intake2_c;
    double ai_base_budget_multiplier_a;
    double ai_base_budget_multiplier_b;
    double ai_base_budget_multiplier_c;
    double ai_worker_count_a;
    double ai_worker_count_b;
    double ai_worker_count_c;
    double ai_base_size_a;
    double ai_base_size_b;
    double ai_base_size_c;
    double ai_statistics_base_growth_a;
    double ai_statistics_base_growth_b;
    double ai_citizen_income;
    double ai_production_priority_coefficient;
    double ai_development_scale_min;
    double ai_development_scale_max;
    double ai_development_scale_max_turn;
    double ai_production_vanilla_priority_unit;
    double ai_production_vanilla_priority_facility;
    double ai_production_vanilla_priority_project;
    double ai_production_project_mineral_surplus_fraction;
    double ai_production_priority_facility_psych;
    double ai_production_threat_coefficient_human;
    double ai_production_threat_coefficient_vendetta;
    double ai_production_threat_coefficient_pact;
    double ai_production_threat_coefficient_treaty;
    double ai_production_threat_coefficient_other;
    double ai_production_pod_bonus;
    double ai_production_pod_popping_priority;
    double ai_production_expansion_priority;
    int ai_production_combat_unit_turns_limit;
    int ai_production_unit_min_mineral_surplus;
    double ai_production_improvement_priority_sea;
    double ai_production_improvement_priority_land;
    double ai_production_naval_yard_defense_priority;
    double ai_production_aerospace_complex_defense_priority;
    double ai_production_inflation;
    double ai_production_income_interest_rate;
    double ai_production_transport_priority;
    double ai_production_transport_seats_per_sea_base;
    double ai_production_support_ratio;
    double ai_production_base_probe_priority;
    double ai_production_base_protection_priority;
    double ai_production_combat_priority;
    double ai_production_alien_combat_priority;
    double ai_production_priority_police;
    double ai_production_survival_effect_a;
    double ai_production_survival_effect_b;
    double ai_production_survival_effect_c;
    double ai_production_survival_effect_d;
    double ai_expansion_weight_deep;
    double ai_mapstat_ocean;
    double ai_mapstat_rocky;
    double ai_mapstat_rainfall;
    double ai_mapstat_rockiness;
    double ai_mapstat_elevation;
    double ai_expansion_travel_time_scale;
    double ai_expansion_coastal_base_large;
    double ai_expansion_coastal_base_small;
    double ai_expansion_ocean_connection_base;
    double ai_expansion_coastal_base_small_land;
    int ai_expansion_radius_overlap_ignored;
    double ai_expansion_radius_overlap_coefficient;
    double ai_expansion_internal_border_connection_coefficient;
    double ai_expansion_external_border_connection_coefficient;
    double ai_expansion_land_use_coefficient;
    double ai_expansion_placement_coefficient;
    double ai_terraforming_nutrientWeight;
    double ai_terraforming_mineralWeight;
	double ai_terraforming_energyWeight;
	double ai_terraforming_completion_bonus;
	double ai_terraforming_land_rocky_tile_threshold;
	int ai_terraforming_travel_time_multiplier;
	double ai_terraforming_networkConnectionValue;
	double ai_terraforming_networkImprovementValue;
	double ai_terraforming_networkBaseExtensionValue;
	double ai_terraforming_networkWildExtensionValue;
	double ai_terraforming_networkDensityThreshold;
	double ai_terraforming_nearbyForestKelpPenalty;
	double ai_terraforming_fitnessMultiplier;
	double ai_terraforming_baseNutrientThresholdRatio;
	double ai_terraforming_baseNutrientCostMultiplier;
	double ai_terraforming_baseMineralThresholdRatio;
	double ai_terraforming_baseMineralCostMultiplier;
    double ai_terraforming_raiseLandPayoffTime;
    double ai_terraforming_sensorValue;
    double ai_terraforming_sensorBorderRange;
    double ai_terraforming_sensorShoreRange;
    double ai_terraforming_bunkerValue;
    double ai_terraforming_bunkerBorderRange;
    double ai_terraforming_landBridgeValue;
    double ai_terraforming_landBridgeRangeScale;
    double ai_combat_base_threat_coefficient;
    double ai_combat_base_threat_range;
    double ai_base_threat_travel_time_scale;
    double ai_combat_travel_time_scale;
    double ai_combat_travel_time_scale_base_protection;
    double ai_combat_priority_escape;
    double ai_combat_priority_repair;
    double ai_combat_priority_repair_partial;
    double ai_combat_priority_monolith_promotion;
    double ai_combat_priority_field_healing;
    double ai_combat_priority_base_protection;
    double ai_combat_priority_base_healing;
    double ai_combat_attack_priority_base;
    double ai_combat_attack_bonus_alien_mind_worms;
    double ai_combat_attack_bonus_alien_spore_launcher;
    double ai_combat_attack_bonus_alien_fungal_tower;
    double ai_combat_attack_bonus_hostile;
    double ai_combat_priority_pod;
    double ai_combat_base_protection_superiority;
    int ai_combat_field_attack_base_proximity_scale;
    double ai_combat_field_attack_superiority_required;
    double ai_combat_field_attack_superiority_desired;
    double ai_combat_base_attack_superiority_required;
    double ai_combat_base_attack_superiority_desired;
    double ai_combat_strength_increase_value;
    double ai_combat_winning_probability_a;
    double ai_combat_winning_probability_b;
    double ai_combat_sea_base_threat_coefficient;
    double ai_production_global_combat_superiority_land;
    double ai_production_global_combat_superiority_sea;
    double ai_production_combat_unit_proportions[3];
    double ai_production_current_project_priority_build_time;
    double ai_production_defensive_facility_threat_threshold;

};

/*
AIPlans contains several general purpose variables for AI decision-making
that are recalculated each turn. These values are not stored in the save game.
*/
struct AIPlans {
    int main_region = -1;
    int main_region_x = -1;
    int main_region_y = -1;
    int main_sea_region = -1;
    int target_land_region = 0;
    int prioritize_naval = 0;
    int naval_scout_x = -1;
    int naval_scout_y = -1;
    int naval_airbase_x = -1;
    int naval_airbase_y = -1;
    int naval_start_x = -1;
    int naval_start_y = -1;
    int naval_end_x = -1;
    int naval_end_y = -1;
    int naval_beach_x = -1;
    int naval_beach_y = -1;
    int defend_weights = 0;
    int land_combat_units = 0;
    int sea_combat_units = 0;
    int air_combat_units = 0;
    int probe_units = 0;
    int missile_units = 0;
    int transport_units = 0;
    int unknown_factions = 0;
    int contacted_factions = 0;
    int enemy_factions = 0;
    int build_tubes = 0;
    /*
    Amount of minerals a base needs to produce before it is allowed to build secret projects.
    All faction-owned bases are ranked each turn based on the surplus mineral production,
    and only the top third are selected for project building.
    */
    int project_limit = 5;
    int median_limit = 5;
    int energy_limit = 15;
    /*
    PSI combat units are only selected for production if this score is higher than zero.
    Higher values will make the prototype picker choose these units more often.
    */
    int psi_score = 0;
    int keep_fungus = 0;
    int plant_fungus = 0;
    int satellite_goal = 0;
    int enemy_odp = 0;
    int enemy_sat = 0;
    int mil_strength = 0;
    int defense_modifier = 0;
    float enemy_base_range = 0;
    float enemy_mil_factor = 0;
    int enemy_bases = 0;
    int captured_bases = 0;
};

#include "engine.h"
#include "config.h"
#include "strings.h"
#include "faction.h"
#include "random.h"
#include "patch.h"
#include "base.h"
#include "build.h"
#include "game.h"
#include "gui.h"
#include "gui_dialog.h"
#include "veh.h"
#include "veh_turn.h"
#include "veh_combat.h"
#include "net.h"
#include "map.h"
#include "mapgen.h"
#include "probe.h"
#include "path.h"
#include "plan.h"
#include "goal.h"
#include "move.h"
#include "tech.h"
#include "test.h"
#include "debug.h"

extern FILE* debug_log;
extern Config conf;
extern AIPlans plans[MaxPlayerNum];
extern set_str_t movedlabels;
extern map_str_t musiclabels;

DLL_EXPORT DWORD ThinkerModule();
bool FileExists(const char* path);
void exit_fail(int32_t addr);
void exit_fail();
int opt_handle_error(const char* section, const char* name);
int opt_list_parse(int32_t* dst, char* src, int num, int min_val);



