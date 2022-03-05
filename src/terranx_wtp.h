#pragma once

#include "terranx.h"
#include "path.h"

// global constants

extern const int NO_SYNC;
extern const char* scriptTxtID;

// Path

extern CPath* PATH;
extern FPath_find PATH_find;

// battle computation display variables

extern int *tx_battle_compute_attacker_effect_count;
extern int *tx_battle_compute_defender_effect_count;
extern char (*tx_battle_compute_attacker_effect_labels)[0x4][0x50];
extern char (*tx_battle_compute_defender_effect_labels)[0x4][0x50];
extern int (*tx_battle_compute_attacker_effect_values)[0x4];
extern int (*tx_battle_compute_defender_effect_values)[0x4];

// labels pointer

extern char ***tx_labels;
const int LABEL_OFFSET_PLANET = 0x271;
const int LABEL_OFFSET_AIRTOAIR = 0x1C1;
const int LABEL_OFFSET_SENSOR = 0x265;
const int LABEL_OFFSET_TERRAIN = 0x14B;
const int LABEL_OFFSET_ROCKY = 0x152;
const int LABEL_OFFSET_FOREST = 0x123;
const int LABEL_OFFSET_FUNGUS = 0x5B;
const int LABEL_OFFSET_ROAD_ATTACK = 0x25E;

extern char *g_strTEMP;
extern int *current_base_growth_rate;
extern int *g_PROBE_FACT_TARGET;
extern int *g_UNK_ATTACK_FLAGS;
extern byte *g_SOCIALWIN;
extern int *TERRA_OCEAN_SQ_NUTRIENT;
extern int *TERRA_OCEAN_SQ_MINERALS;
extern int *TERRA_OCEAN_SQ_ENERGY;
extern int *TERRA_OCEAN_SQ_PSI;
extern int *TERRA_BASE_SQ_NUTRIENT;
extern int *TERRA_BASE_SQ_MINERALS;
extern int *TERRA_BASE_SQ_ENERGY;
extern int *TERRA_BASE_SQ_PSI;
extern int *TERRA_BONUS_SQ_NUTRIENT;
extern int *TERRA_BONUS_SQ_MINERALS;
extern int *TERRA_BONUS_SQ_ENERGY;
extern int *TERRA_BONUS_SQ_PSI;
extern int *TERRA_FOREST_SQ_NUTRIENT;
extern int *TERRA_FOREST_SQ_MINERALS;
extern int *TERRA_FOREST_SQ_ENERGY;
extern int *TERRA_FOREST_SQ_PSI;
extern int *TERRA_RECYCLING_TANKS_NUTRIENT;
extern int *TERRA_RECYCLING_TANKS_MINERALS;
extern int *TERRA_RECYCLING_TANKS_ENERGY;
extern int *TERRA_RECYCLING_TANKS_PSI;
extern int *TERRA_IMPROVED_LAND_NUTRIENT;
extern int *TERRA_IMPROVED_LAND_MINERALS;
extern int *TERRA_IMPROVED_SEA_NUTRIENT;
extern int *TERRA_IMPROVED_SEA_MINERALS;
extern int *TERRA_IMPROVED_SEA_ENERGY;
extern int *TERRA_IMPROVED_SEA_PSI;
extern int *TERRA_MONOLITH_NUTRIENT;
extern int *TERRA_MONOLITH_MINERALS;
extern int *TERRA_MONOLITH_ENERGY;
extern int *TERRA_MONOLITH_PSI;
extern int *TERRA_BOREHOLE_SQ_NUTRIENT;
extern int *TERRA_BOREHOLE_SQ_MINERALS;
extern int *TERRA_BOREHOLE_SQ_ENERGY;
extern int *TERRA_BOREHOLE_SQ_PSI;

extern fp_void_charp_int *say_orders;
extern fp_1void *say_orders2;

extern fp_0int *tx_read_basic_rules;
extern fp_5int *tx_proto_cost;
extern fp_6int *tx_create_prototype;
extern fp_3int *tx_upgrade_cost;
extern fp_2int_void *tx_strcat;
extern fp_6int *tx_base_find3;
extern fp_5int *tx_tile_yield;
extern fp_0int *tx_base_mechanics_production;
extern fp_5int *tx_set_se_on_dialog_close;
extern fp_7int *tx_hex_cost;
extern fp_3int *tx_terrain_avail;
extern fp_0void *tx_world_climate;
extern fp_0void *tx_world_rivers;
extern fp_0void *tx_world_rainfall;
extern fp_4void *tx_go_to;
extern fp_int_char_int *tx_say_base;
extern fp_3int *tx_base_init;
extern fp_4int *tx_probe;
extern fp_3int *tx_set_facility;
extern fp_charp_int_charp_int *tx_itoa;
extern fp_0void *tx_base_nutrient;
extern fp_5int *tx_social_calc;
extern fp_0int *tx_base_production;
extern fp_3int *tx_has_fac;
extern fp_1int *tx_black_market;
extern fp_3void *tx_setup_player;
extern fp_0void *tx_balance;
extern fp_0void *tx_world_build;
extern fp_3int *tx_order_veh;
extern fp_3int *tx_veh_cost;
extern fp_1void *tx_base_first;
extern fp_1int *tx_action;
extern fp_2void *tx_parse_string;
extern fp_1void *tx_kill;
extern fp_1int *tx_spying;
extern fp_0int *tx_text_get;
extern fp_4void *tx_set_treaty;
extern fp_3int *tx_veh_drop;
extern fp_7void *tx_battle_fight_2;
extern fp_3int *tx_break_treaty;
extern fp_2void *tx_act_of_aggression;
extern fp_2void *tx_tech_research;
extern fp_3void *tx_bitmask;
extern fp_3void *tx_action_move;
extern fp_1int *tx_veh_cargo;
extern fp_3int *tx_zoc_move;
extern fp_4int* base_prod_choice;
extern fp_4int *tech_pick;
extern fp_0void *reset_territory;
extern fp_3int *break_treaty;
extern fp_2int *base_making;
extern fp_3int *mind_control;
extern fp_3int *veh_put;
extern fp_3int *morale_mod;
extern fp_2void *base_prod_change;
extern fp_0void *base_hurry;
extern fp_2int *base_on_sea;
extern fp_3int *base_find2;
extern fp_1void *enemy_units_check;
extern fp_2int *veh_who;
extern fp_2int *pact_withdraw;
extern fp_2int *base_find;
extern fp_5int *whose_territory;
extern fp_1int* unit_speed;
extern fp_1int* base_upkeep;
extern fp_0int* base_ecology;
extern fp_0void *base_minerals;
extern fp_0void *base_yield;

