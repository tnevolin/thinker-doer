#pragma once

typedef unsigned char byte;

#include "terranx_enums.h"
#include "terranx_types.h"

extern const char** tx_version;
extern const char** tx_date;

extern byte* tx_tech_discovered;
extern int* tx_secret_projects;
extern int* tx_cost_ratios;
extern short (*tx_faction_rankings)[8];
extern MetaFaction* tx_metafactions;
extern Faction* tx_factions;
extern BASE* tx_bases;
extern UNIT* tx_units;
extern VEH* tx_vehicles;
extern MAP** tx_map_ptr;

extern R_Basic* tx_basic;
extern R_Tech* tx_techs;
extern R_Social* tx_social;
extern R_Facility* tx_facility;
extern R_Ability* tx_ability;
extern R_Chassis* tx_chassis;
extern R_Citizen* tx_citizen;
extern R_Defense* tx_defense;
extern R_Reactor* tx_reactor;
extern R_Resource* tx_resource;
extern R_Terraform* tx_terraform;
extern R_Weapon* tx_weapon;

// battle computation display variables
extern int *tx_battle_compute_attacker_effect_count;
extern char (*tx_battle_compute_attacker_effect_labels)[0x4][0x50];
extern int (*tx_battle_compute_attacker_effect_values)[0x4];
extern int *tx_battle_compute_defender_effect_count;
extern char (*tx_battle_compute_defender_effect_labels)[0x4][0x50];
extern int (*tx_battle_compute_defender_effect_values)[0x4];

// labels pointer
extern char ***tx_labels;

typedef void __cdecl fp_0void();
typedef void __cdecl fp_1void(int);
typedef void __cdecl fp_2void(int, int);
typedef void __cdecl fp_3void(int, int, int);
typedef void __cdecl fp_4void(int, int, int, int);
typedef int __cdecl fp_void();
typedef int __cdecl fp_0int();
typedef int __cdecl fp_1int(int);
typedef int __cdecl fp_2int(int, int);
typedef int __cdecl fp_3int(int, int, int);
typedef int __cdecl fp_4int(int, int, int, int);
typedef int __cdecl fp_5int(int, int, int, int, int);
typedef int __cdecl fp_6int(int, int, int, int, int, int);
typedef int __cdecl fp_7int(int, int, int, int, int, int, int);
typedef int __cdecl fp_7intstr(int, int, int, int, int, int, int, const char*);
typedef char* __cdecl fp_str_void();
typedef char* __cdecl fp_str_void();
typedef void __cdecl fp_2int_void(int, int);
typedef void __cdecl fp_3int_void(int, int, int);
typedef int __cdecl fp_int_char_int(char *, int);
typedef void __cdecl fp_void_charp_int(char *, int);
typedef char *__cdecl fp_charp_int_charp_int(int, char *, int);

// params: faction, chassis, module, armor, specials, reactor, unit_plan, name
extern fp_7intstr* tx_propose_proto;

// params: prototype_id, faction, x, y
extern fp_4int* tx_veh_init;

extern fp_1int* tx_veh_skip;
extern fp_2int* tx_veh_at;
extern fp_2int* tx_veh_speed;
extern fp_3int* tx_zoc_any;
extern fp_3int* tx_best_defender;
extern fp_5int* tx_battle_compute;
extern fp_6int* tx_battle_kill;
extern fp_1void* tx_enemy_units_check;
extern fp_1int* tx_enemy_move;
extern fp_1int* tx_monolith;
extern fp_2int* tx_action_build;
extern fp_3int* tx_action_terraform;
extern fp_3int* tx_terraform_cost;
extern fp_2int* tx_bonus_at;
extern fp_2int* tx_goody_at;
extern fp_3int* tx_cost_factor;
extern fp_3int* tx_site_set;
extern fp_3int* tx_world_site;
extern fp_1int* tx_set_base;
extern fp_1int* tx_base_compute;
extern fp_4int* tx_base_prod_choices;
extern fp_0int* tx_turn_upkeep;
extern fp_1int* tx_faction_upkeep;
extern fp_3int* tx_tech_val;
extern fp_6int* tx_social_ai;
extern fp_1int* tx_social_set;
extern fp_1int* tx_pop_goal;
extern fp_1int* tx_consider_designs;
extern fp_1int* tx_action_staple;
extern fp_1int* tx_tech_rate;
extern fp_1int* tx_tech_selection;
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

