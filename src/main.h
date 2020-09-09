#pragma once

#ifdef BUILD_REL
    #define MOD_VERSION "The Will to Power mod - version 74"
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
#else
    #define UNUSED(x) UNUSED_ ## x
#endif

#define DLL_EXPORT extern "C" __declspec(dllexport)
#define HOOK_API extern "C"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include <limits.h>
#include <time.h>
#include <math.h>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "terranx.h"

typedef std::set<std::pair<int,int>> Points;
#define mp(x, y) std::make_pair(x, y)
#define min(x, y) std::min(x, y)
#define max(x, y) std::max(x, y)

static_assert(sizeof(struct R_Basic) == 308, "");
static_assert(sizeof(struct R_Social) == 212, "");
static_assert(sizeof(struct R_Facility) == 48, "");
static_assert(sizeof(struct R_Tech) == 44, "");
static_assert(sizeof(struct MetaFaction) == 1436, "");
static_assert(sizeof(struct Faction) == 8396, "");
static_assert(sizeof(struct BASE) == 308, "");
static_assert(sizeof(struct UNIT) == 52, "");
static_assert(sizeof(struct VEH) == 52, "");
static_assert(sizeof(struct MAP) == 44, "");

#define MAPSZ 256
#define QSIZE 512
#define BASES 512
#define UNITS 2048
#define COMBAT 0
#define SYNC 0
#define NO_SYNC 1
#define NEAR_ROADS 3
#define TERRITORY_LAND 4
#define RES_NONE 0
#define RES_NUTRIENT 1
#define RES_MINERAL 2
#define RES_ENERGY 3
#define ATT false
#define DEF true

struct Config {
    int free_formers = 0;
    int free_colony_pods = 0;
    int satellites_nutrient = 0;
    int satellites_mineral = 0;
    int satellites_energy = 0;
    int design_units = 1;
    int factions_enabled = 7;
    int social_ai = 1;
    int tech_balance = 0;
    int hurry_items = 1;
    double expansion_factor = 1;
    int limit_project_start = 3;
    int max_sat = 10;
    int smac_only = 0;
    int faction_placement = 1;
    int nutrient_bonus = 1;
    int landmarks = 0xffff;
    int revised_tech_cost = 1;
    int auto_relocate_hq = 1;
    int eco_damage_fix = 1;
    int collateral_damage_value = 3;
    int disable_planetpearls = 0;
    int disable_aquatic_bonus_minerals = 0;
    int patch_content_pop = 0;
    int content_pop_player[6] = {6,5,4,3,2,1};
    int content_pop_computer[6] = {3,3,3,3,3,3};
//  SMACX The Will to Power Mod
    bool disable_alien_guaranteed_technologies = false;
    bool alternative_weapon_icon_selection_algorithm = false;
    bool ignore_reactor_power_in_combat = false;
    bool alternative_prototype_cost_formula = false;
    int reactor_cost_factors[4];
    bool disable_hurry_penalty_threshold = false;
    bool alternative_unit_hurry_formula = false;
    bool alternative_upgrade_cost_formula = false;
    bool alternative_base_defensive_structure_bonuses = false;
    int perimeter_defense_multiplier = 0;
    int tachyon_field_bonus = 0;
    bool collateral_damage_defender_reactor = false;
// integrated into Thinker
//    int collateral_damage_value = 3;
// integrated into Thinker
//    bool disable_aquatic_bonus_minerals = false;
    int repair_minimal = 1;
    int repair_fungus = 2;
    bool repair_friendly = true;
    bool repair_airbase = true;
    bool repair_bunker = true;
    int repair_base = 1;
    int repair_base_native = 10;
    int repair_base_facility = 10;
    int repair_nano_factory = 10;
    bool alternative_combat_mechanics = false;
    double alternative_combat_mechanics_loss_divider = 1.0;
// integrated into Thinker
//    bool disable_planetpearls = false;
    bool uniform_promotions = true; // internal configuration
    bool very_green_no_defense_bonus = true; // internal configuration
    bool planet_combat_bonus_on_defense = false;
    bool sea_territory_distance_same_as_land = false;
    bool coastal_territory_distance_same_as_sea = false;
    bool alternative_artillery_damage = false;
    bool disable_home_base_cc_morale_bonus = false;
    bool disable_current_base_cc_morale_bonus = false;
    bool default_morale_very_green = false;
    int territory_combat_bonus = 0;
    int tube_movement_rate_multiplier = 0;
    int road_movement_cost = 0;
    int project_contribution_threshold = 0;
    double project_contribution_proportion = 0.0;
    bool cloning_vats_disable_impunities = false;
    int cloning_vats_se_growth = 0;
    int se_growth_rating_cap = 5;
    bool recycling_tanks_mineral_multiplier = false;
    int free_minerals = 16;
    int native_life_generator_constant = 2;
    int native_life_generator_multiplier = 2;
    bool native_life_generator_more_sea_creatures = false;
    bool native_disable_sudden_death = false;
    bool alternative_inefficiency = false;
    int unit_home_base_reassignment_production_threshold = 0;
    double ocean_depth_multiplier = 1.0;
    bool zoc_regular_army_sneaking_disabled = false;
    bool ai_useWTPAlgorithms;
    double ai_production_min_native_protection;
    double ai_production_max_native_protection;
    double ai_production_native_protection_priority_multiplier;
    int ai_production_max_unpopulated_range;
    int ai_production_min_unpopulated_tiles;
    double ai_production_colony_priority;
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

};

/*
    AIPlans contains several general purpose variables for AI decision-making
    that are recalculated each turn. These values are not stored in the save game.
*/
struct AIPlans {
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
    /*
    Number of our bases captured by another faction we're currently at war with.
    Important heuristic in threat calculation.
    */
    int enemy_bases = 0;
};

extern std::string MOVE_STATUS[];
extern FILE* debug_log;
extern Config conf;
extern AIPlans plans[8];
extern Points convoys;
extern Points boreholes;
extern Points needferry;

DLL_EXPORT int ThinkerDecide();
HOOK_API int turn_upkeep();
HOOK_API int base_production(int id, int v1, int v2, int v3);
HOOK_API int social_ai(int faction, int v1, int v2, int v3, int v4, int v5);

int need_defense(int id);
int need_psych(int id);
int consider_hurry(int id);
int hurry_item(BASE* b, int mins, int cost);
int find_project(int id);
int find_facility(int id);
int select_prod(int id);
int find_proto(int base_id, int triad, int mode, bool defend);
int select_combat(int id, bool sea_base, bool build_ships, int probes, int def_land);
int opt_list_parse(int* ptr, char* buf, int len, int min_val);

