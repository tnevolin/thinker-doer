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

#pragma once

#ifdef BUILD_REL
    #define MOD_VERSION "The Will to Power mod - version 233 (Thinker Mod v2.5)"
#else
    #define MOD_VERSION "The Will to Power mod - development"
#endif

#ifdef BUILD_DEBUG
    #define MOD_DATE __DATE__ " " __TIME__
    #define DEBUG 1
    #define debug(...) fprintf(debug_log, __VA_ARGS__);
#else
    #define MOD_DATE __DATE__
    #define DEBUG 0
    #define NDEBUG /* Disable assertions */
    #define debug(...) /* Nothing */
    #ifdef __GNUC__
    #pragma GCC diagnostic ignored "-Wunused-variable"
    #pragma GCC diagnostic ignored "-Wunused-but-set-variable"
    #endif
#endif

#ifdef __GNUC__
    #define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
    #pragma GCC diagnostic ignored "-Wchar-subscripts"
    #pragma GCC diagnostic ignored "-Wparentheses"
#else
    #define UNUSED(x) UNUSED_ ## x
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include <psapi.h>
#include <algorithm>
#include <set>
#include <list>
#include <string>
#include <vector>
#include "terranx.h"

#define DLL_EXPORT extern "C" __declspec(dllexport)
#define HOOK_API extern "C"
// =WTP=
// macros cause confution with original std:: functions
//#define std::min(x, y) std::min(x, y)
//#define std::max(x, y) std::max(x, y)

#ifdef BUILD_DEBUG
#ifdef assert
#undef assert
#define assert(_Expression) \
((!!(_Expression)) \
|| (fprintf(debug_log, "Assertion Failed: %s %s %d\n", #_Expression, __FILE__, __LINE__) \
&& (_assert(#_Expression, __FILE__, __LINE__), 0)))
#endif
#endif

const int COMBAT = 0;
const int SYNC = 0;
const bool DEF = true;
const bool ATT = false;
const bool SEA = true;
const bool LAND = false;

const int MaxMapW = 512;
const int MaxMapH = 256;
const int MaxRegionNum = 128;

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

const int MaxTechnologyNum = 89;
const int MaxChassisNum = 9;
const int MaxWeaponNum = 26;
const int MaxArmorNum = 14;
const int MaxReactorNum = 4;
const int MaxAbilityNum = 29;

const int MaxFacilityNum = 134; // 0 slot unused
const int MaxSecretProjectNum = 64;
const int MaxSocialCatNum = 4;
const int MaxSocialModelNum = 4;
const int MaxSocialEffectNum = 11;

/*
Config parsed from thinker.ini. Alpha Centauri.ini related options
can be set negative values to use the defaults from Alpha Centauri.ini.
*/
struct Config {
    int directdraw = 0;
    int disable_opening_movie = 1;
    int cpu_idle_fix = 1;
    int smooth_scrolling = 0;
    int scroll_area = 40;
    int world_map_labels = 1;
    int new_base_names = 0;
    int windowed = 0;
    int window_width = 1024;
    int window_height = 768;
    int smac_only = 0;
    int player_free_units = 0;
    int free_formers = 0;
    int free_colony_pods = 0;
    int satellites_nutrient = 0;
    int satellites_mineral = 0;
    int satellites_energy = 0;
    int design_units = 1;
    int factions_enabled = 7;
    int social_ai = 1;
    int social_ai_bias = 10;
    int tech_balance = 0;
    int hurry_items = 0;
    int base_spacing = 3;
    int base_nearby_limit = -1;
    int expansion_limit = 100;
    int expansion_autoscale = 0;
    int limit_project_start = 3;
    int max_satellites = 20;
    int faction_placement = 1;
    int nutrient_bonus = 0;
    int rare_supply_pods = 0;
    int landmarks = 0xffff;
    int revised_tech_cost = 1;
    int auto_relocate_hq = 1;
    int ignore_reactor_power = 0;
    int territory_border_fix = 1;
    int eco_damage_fix = 1;
    int clean_minerals = 16;
    int spawn_fungal_towers = 1;
    int spawn_spore_launchers = 1;
    int spawn_sealurks = 1;
    int spawn_battle_ogres = 1;
    int collateral_damage_value = 3;
    int disable_planetpearls = 0;
    int disable_aquatic_bonus_minerals = 0;
    int disable_alien_guaranteed_techs = 0;
    int patch_content_pop = 0;
    int content_pop_player[MaxDiffNum] = {6,5,4,3,2,1};
    int content_pop_computer[MaxDiffNum] = {3,3,3,3,3,3};
    int repair_minimal = 1;
    int repair_fungus = 2;
    int repair_friendly = true;
    int repair_airbase = true;
    int repair_bunker = true;
    int repair_base = 1;
    int repair_base_native = 10;
    int repair_base_facility = 10;
    int repair_nano_factory = 10;
    int debug_mode = DEBUG;
//  =WTP=
    bool disable_alien_guaranteed_technologies = false;
    bool alternative_weapon_icon_selection_algorithm = false;
    // implemented in Thinker?
	bool ignore_reactor_power_in_combat = false;
    bool alternative_prototype_cost_formula = false;
    int reactor_cost_factors[4];
    bool flat_hurry_cost = false;
    int flat_hurry_cost_multiplier_unit = 1;
    int flat_hurry_cost_multiplier_facility = 1;
    int flat_hurry_cost_multiplier_project = 1;
    bool alternative_upgrade_cost_formula = false;
    bool alternative_base_defensive_structure_bonuses = false;
    int perimeter_defense_multiplier = 0;
    int tachyon_field_bonus = 0;
    bool collateral_damage_defender_reactor = false;
	// integrated into Thinker
//	int collateral_damage_value = 3;
	// integrated into Thinker
//	bool disable_aquatic_bonus_minerals = false;
	// integrated into Thinker
//	int repair_minimal = 1;
//	int repair_fungus = 2;
//	bool repair_friendly = true;
//	bool repair_airbase = true;
//	bool repair_bunker = true;
//	int repair_base = 1;
//	int repair_base_native = 10;
//	int repair_base_facility = 10;
//	int repair_nano_factory = 10;
    bool alternative_combat_mechanics = false;
    double alternative_combat_mechanics_loss_divisor = 1.0;
	// integrated into Thinker
//	bool disable_planetpearls = false;
    bool uniform_promotions = true; // internal configuration
    bool very_green_no_defense_bonus = true; // internal configuration
    bool planet_combat_bonus_on_defense = false;
    bool sea_territory_distance_same_as_land = false;
    // integrated into Thinker
//	bool coastal_territory_distance_same_as_sea = false;
    bool alternative_artillery_damage = false;
    bool disable_home_base_cc_morale_bonus = false;
    bool disable_current_base_cc_morale_bonus = false;
    bool default_morale_very_green = false;
    int combat_bonus_territory = 0;
    int tube_movement_rate_multiplier = 0;
    int road_movement_cost = 0;
    int project_contribution_threshold = 0;
    double project_contribution_proportion = 0.0;
    bool cloning_vats_disable_impunities = false;
    int cloning_vats_se_growth = 0;
    int se_growth_rating_min = -2;
    int se_growth_rating_cap = 5;
    bool recycling_tanks_mineral_multiplier = false;
    // integrated into Thinker
//	int free_minerals = 16;
    int native_life_generator_constant = 2;
    int native_life_generator_multiplier = 2;
    bool native_life_generator_more_sea_creatures = false;
    bool native_disable_sudden_death = false;
    bool alternative_inefficiency = false;
    double ocean_depth_multiplier = 1.0;
    bool zoc_regular_army_sneaking_disabled = false;
    int pts_new_base_size_less_average = 2;
    int biology_lab_research_bonus = 2;
    bool hsa_does_not_kill_probe = false;
    bool condenser_and_enricher_do_not_multiply_nutrients = false;
    bool alternative_tech_cost = false;
    double tech_cost_scale = 1.0;
    bool flat_extra_prototype_cost = false;
    bool fix_mineral_contribution = false;
    bool fix_former_wake = false;
    bool infiltration_expire = false;
    int infiltration_devices = 1;
    double human_infiltration_device_lifetime_base = 1.0;
    double human_infiltration_device_lifetime_probe_effect = 0.0;
    double computer_infiltration_device_lifetime_base = 1.0;
    double computer_infiltration_device_lifetime_probe_effect = 0.0;
    bool modified_probe_action_risks = false;
    int probe_action_risk_genetic_plague = 0;
    bool combat_bonus_sensor_ocean = false;
    bool combat_bonus_sensor_offense = false;
    bool break_treaty_before_fight = false;
    bool compact_effect_icons = false;
    int se_research_bonus_percentage = 10;
    bool remove_fungal_tower_defense_bonus = false;
    int aliens_fight_half_strength_unit_turn = 15;
    bool habitation_facility_disable_explicit_population_limit = false;
    int habitation_facility_absent_growth_penalty = 0;
    int habitation_facility_present_growth_bonus_max = 0;
    bool unit_upgrade_ignores_movement = false;
    bool group_terraforming = false;
    bool ai_base_allowed_fungus_rocky = false;
    bool interceptor_scramble_fix = false;
    int right_of_passage_agreement = 0;
    bool scorched_earth = false;
    double orbital_yield_limit = 1.0;
    bool silent_vendetta_warning = false;
    bool design_cost_in_rows = false;
    int energy_market_crash_numerator = 4;
    bool carry_over_minerals = false;
    // AI configurations
    bool ai_useWTPAlgorithms;
    double ai_production_vanilla_priority_unit;
    double ai_production_vanilla_priority_project;
    double ai_production_vanilla_priority_facility;
    double ai_production_thinker_priority;
    double ai_production_military_priority;
    double ai_production_military_priority_minimal;
    double ai_production_threat_coefficient_human;
    double ai_production_threat_coefficient_vendetta;
    double ai_production_threat_coefficient_pact;
    double ai_production_threat_coefficient_treaty;
    double ai_production_threat_coefficient_other;
    double ai_production_min_native_protection;
    double ai_production_max_native_protection;
    double ai_production_native_protection_priority;
    int ai_production_max_unpopulated_range;
    double ai_production_expansion_coverage;
    double ai_production_ocean_expansion_coverage;
    double ai_production_expansion_priority;
    int ai_production_combat_unit_turns_limit;
    double ai_production_facility_priority_penalty;
    int ai_production_combat_unit_min_mineral_surplus;
    double ai_production_exploration_coverage;
    double ai_production_improvement_coverage;
    int ai_production_population_projection_turns;
    double ai_production_threat_level_threshold;
    double ai_terraforming_nutrientWeight;
    double ai_terraforming_mineralWeight;
	double ai_terraforming_energyWeight;
	double ai_terraforming_landDistanceScale;
	double ai_terraforming_waterDistanceScale;
	double ai_terraforming_networkConnectionValue;
	double ai_terraforming_networkImprovementValue;
	double ai_terraforming_networkBaseExtensionValue;
	double ai_terraforming_networkWildExtensionValue;
	double ai_terraforming_networkCoverageThreshold;
	double ai_terraforming_nearbyForestKelpPenalty;
	double ai_terraforming_rankMultiplier;
	double ai_terraforming_exclusivityMultiplier;
	double ai_terraforming_baseNutrientThresholdRatio;
	double ai_terraforming_baseNutrientDemandMultiplier;
	double ai_terraforming_baseMineralThresholdRatio;
	double ai_terraforming_baseMineralDemandMultiplier;
    double ai_terraforming_raiseLandPayoffTime;
    double ai_terraforming_sensorValue;
    double ai_terraforming_sensorBorderRange;
    double ai_terraforming_sensorShoreRange;

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
    int naval_score = INT_MIN;
    int naval_airbase_x = -1;
    int naval_airbase_y = -1;
    int naval_start_x = -1;
    int naval_start_y = -1;
    int naval_end_x = -1;
    int naval_end_y = -1;
    int naval_beach_x = -1;
    int naval_beach_y = -1;
    int combat_units = 0;
    int aircraft = 0;
    int transports = 0;
    int unknown_factions = 0;
    int contacted_factions = 0;
    int enemy_factions = 0;
    int diplo_flags = 0;
    /*
    Amount of minerals a base needs to produce before it is allowed to build secret projects.
    All faction-owned bases are ranked each turn based on the surplus mineral production,
    and only the top third are selected for project building.
    */
    int proj_limit = 5;
    /*
    PSI combat units are only selected for production if this score is higher than zero.
    Higher values will make the prototype picker choose these units more often.
    */
    int psi_score = 0;
    int need_police = 1;
    int keep_fungus = 0;
    int plant_fungus = 0;
    int satellites_goal = 0;
    /*
    Number of our bases captured by another faction we're currently at war with.
    Important heuristic in threat calculation.
    */
    int enemy_bases = 0;
};

enum crawl_resource_types {
    RES_NONE = 0,
    RES_NUTRIENT = 1,
    RES_MINERAL = 2,
    RES_ENERGY = 3,
};

enum nodeset_types {
    NODE_CONVOY = 1,
    NODE_BOREHOLE = 2,
    NODE_NEED_FERRY = 3,
    NODE_RAISE_LAND = 4,
    NODE_GOAL_RAISE_LAND = 5,
    NODE_GOAL_NAVAL_START = 6,
    NODE_GOAL_NAVAL_BEACH = 7,
    NODE_GOAL_NAVAL_END = 8,
    NODE_PATROL = 9,
    NODE_BASE_SITE = 10,
};

struct MapTile {
    int x;
    int y;
    MAP* sq;
};

struct Point {
    int x;
    int y;
};

struct PointComp {
    bool operator()(const Point& p1, const Point& p2) const
    {
        return p1.x < p2.x || (p1.x == p2.x && p1.y < p2.y);
    }
};

struct MapNode {
    int x;
    int y;
    int type;
};

struct NodeComp {
    bool operator()(const MapNode& p1, const MapNode& p2) const
    {
        return p1.x < p2.x || (p1.x == p2.x && (
            p1.y < p2.y || (p1.y == p2.y && (p1.type < p2.type))
        ));
    }
};

typedef std::set<MapNode,NodeComp> NodeSet;
typedef std::set<Point,PointComp> Points;
typedef std::list<Point> PointList;

#include "patch.h"
#include "path.h"
#include "gui.h"
#include "game.h"
#include "move.h"
#include "tech.h"
#include "engine.h"
#include "test.h"

extern FILE* debug_log;
extern Config conf;
extern NodeSet mapnodes;
extern AIPlans plans[MaxPlayerNum];

DLL_EXPORT int ThinkerDecide();
HOOK_API int mod_turn_upkeep();
HOOK_API int mod_base_production(int id, int v1, int v2, int v3);
HOOK_API int mod_social_ai(int faction, int v1, int v2, int v3, int v4, int v5);

int plans_upkeep(int faction);
int need_defense(int id);
int need_psych(int id);
int consider_hurry(int id);
int find_project(int id);
int find_facility(int id);
int select_production(int id);
int find_proto(int base_id, int triad, int mode, bool defend);
int select_combat(int base_id, int probes, bool sea_base, bool build_ships, bool def_land);
int opt_list_parse(int* ptr, char* buf, int len, int min_val);



