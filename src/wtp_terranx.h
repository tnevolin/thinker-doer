#pragma once

#include "path.h"

typedef void	(__cdecl *fp_0void				)();
typedef void	(__cdecl *fp_1void				)(int);
typedef void	(__cdecl *fp_2void				)(int, int);
typedef void	(__cdecl *fp_3void				)(int, int, int);
typedef void	(__cdecl *fp_4void				)(int, int, int, int);
typedef void	(__cdecl *fp_5void				)(int, int, int, int, int);
typedef void	(__cdecl *fp_6void				)(int, int, int, int, int, int);
typedef void	(__cdecl *fp_7void				)(int, int, int, int, int, int, int);
typedef int		(__cdecl *fp_0int				)();
typedef int		(__cdecl *fp_7intstr			)(int, int, int, int, int, int, int, const char*);
typedef char *	(__cdecl *fp_str_void			)();
typedef char *	(__cdecl *fp_str_void			)();
typedef void	(__cdecl *fp_2int_void			)(int, int);
typedef void	(__cdecl *fp_3int_void			)(int, int, int);
typedef int		(__cdecl *fp_int_char_int		)(char *, int);
typedef void	(__cdecl *fp_void_charp_int		)(char *, int);
typedef char *	(__cdecl *fp_charp_int_charp_int)(int, char *, int);

// turn control factions moved

extern int *TurnFactionsMoved;

// global constants

extern const int NO_SYNC;
extern const char* scriptTxtID;

// offsets

extern const int TABLE_next_cell_count;
extern const int *TABLE_next_cell_x;
extern const int *TABLE_next_cell_y;

extern const int TABLE_square_block_radius_count;
extern const int *TABLE_square_block_radius;
extern const int TABLE_square_block_radius_base_internal;
extern const int TABLE_square_block_radius_base_external;
extern const int *TABLE_square_offset_x;
extern const int *TABLE_square_offset_y;

// Path

extern FPath_find PATH_find;
extern int *PATH_flags;

// battle computation display variables

extern int *tx_battle_compute_attacker_effect_count;
extern int *tx_battle_compute_defender_effect_count;
extern char (*tx_battle_compute_attacker_effect_labels)[0x4][0x50];
extern char (*tx_battle_compute_defender_effect_labels)[0x4][0x50];
extern int (*tx_battle_compute_attacker_effect_values)[0x4];
extern int (*tx_battle_compute_defender_effect_values)[0x4];

// labels pointer

extern char ***tx_labels;
extern const int LABEL_OFFSET_PLANET;
extern const int LABEL_OFFSET_AIRTOAIR;
extern const int LABEL_OFFSET_SENSOR;
extern const int LABEL_OFFSET_TERRAIN;
extern const int LABEL_OFFSET_ROCKY;
extern const int LABEL_OFFSET_FOREST;
extern const int LABEL_OFFSET_FUNGUS;
extern const int LABEL_OFFSET_ROAD_ATTACK;
extern const int LABEL_OFFSET_BASE;
extern const int LABEL_OFFSET_PERIMETER;
extern const int LABEL_OFFSET_TACHYON;
extern const int LABEL_OFFSET_TRACKING;
extern const char *LABEL_NAVAL_YARD;
extern const char *LABEL_AEROSPACE_COMPLEX;
extern const char *LABEL_WEAPON;
extern const char *LABEL_ARMOR;

extern char *g_strTEMP;
extern int *g_PROBE_FACT_TARGET;
extern int *g_UNK_ATTACK_FLAGS;
extern byte *g_SOCIALWIN;
extern int *current_attacker;
extern int *current_defender;
extern int *net_income;
extern int *tech_per_turn;

// yield rules

extern int *BIT_OCEAN_SQ_NUTRIENT;
extern int *BIT_OCEAN_SQ_MINERALS;
extern int *BIT_OCEAN_SQ_ENERGY;
extern int *BIT_OCEAN_SQ_PSI;
extern int *BIT_BASE_SQ_NUTRIENT;
extern int *BIT_BASE_SQ_MINERALS;
extern int *BIT_BASE_SQ_ENERGY;
extern int *BIT_BASE_SQ_PSI;
extern int *BIT_BONUS_SQ_NUTRIENT;
extern int *BIT_BONUS_SQ_MINERALS;
extern int *BIT_BONUS_SQ_ENERGY;
extern int *BIT_BONUS_SQ_PSI;
extern int *BIT_FOREST_SQ_NUTRIENT;
extern int *BIT_FOREST_SQ_MINERALS;
extern int *BIT_FOREST_SQ_ENERGY;
extern int *BIT_FOREST_SQ_PSI;
extern int *BIT_RECYCLING_TANKS_NUTRIENT;
extern int *BIT_RECYCLING_TANKS_MINERALS;
extern int *BIT_RECYCLING_TANKS_ENERGY;
extern int *BIT_RECYCLING_TANKS_PSI;
extern int *BIT_IMPROVED_LAND_NUTRIENT;
extern int *BIT_IMPROVED_LAND_MINERALS;
extern int *BIT_IMPROVED_SEA_NUTRIENT;
extern int *BIT_IMPROVED_SEA_MINERALS;
extern int *BIT_IMPROVED_SEA_ENERGY;
extern int *BIT_IMPROVED_SEA_PSI;
extern int *BIT_MONOLITH_NUTRIENT;
extern int *BIT_MONOLITH_MINERALS;
extern int *BIT_MONOLITH_ENERGY;
extern int *BIT_MONOLITH_PSI;
extern int *BIT_BOREHOLE_SQ_NUTRIENT;
extern int *BIT_BOREHOLE_SQ_MINERALS;
extern int *BIT_BOREHOLE_SQ_ENERGY;
extern int *BIT_BOREHOLE_SQ_PSI;

extern int *CURRENT_BASE_DRONES_UNMODIFIED;
extern int *CURRENT_BASE_DRONES_PSYCH;
extern int *CURRENT_BASE_DRONES_FACILITIES;
extern int *CURRENT_BASE_DRONES_POLICE;
extern int *CURRENT_BASE_DRONES_PROJECTS;
extern int *CURRENT_BASE_SDRONES_UNMODIFIED;
extern int *CURRENT_BASE_SDRONES_PSYCH;
extern int *CURRENT_BASE_SDRONES_FACILITIES;
extern int *CURRENT_BASE_SDRONES_POLICE;
extern int *CURRENT_BASE_SDRONES_PROJECTS;
extern int *CURRENT_BASE_TALENTS_UNMODIFIED;
extern int *CURRENT_BASE_TALENTS_PSYCH;
extern int *CURRENT_BASE_TALENTS_FACILITIES;
extern int *CURRENT_BASE_TALENTS_POLICE;
extern int *CURRENT_BASE_TALENTS_PROJECTS;

extern int *alphax_tgl_probe_steal_tech;

extern fp_0int tx_read_basic_rules;
extern fp_6int tx_create_prototype;
extern fp_3int tx_upgrade_cost;
extern fp_1int tx_strlen;
extern fp_2int_void tx_strcat;
extern fp_5int tx_tile_yield;
extern fp_0int tx_base_mechanics_production;
extern fp_5int tx_set_se_on_dialog_close;
extern fp_7int tx_hex_cost;
extern fp_3int tx_terrain_avail;
extern fp_0void tx_world_climate;
extern fp_0void tx_world_rivers;
extern fp_0void tx_world_rainfall;
extern fp_4void tx_go_to;
extern fp_int_char_int tx_say_base;
extern fp_4int tx_probe;
extern fp_3int tx_set_facility;
extern fp_charp_int_charp_int tx_itoa;
extern fp_0void tx_base_nutrient;
extern fp_5int tx_social_calc;
extern fp_0int tx_base_production;
extern fp_3int tx_has_fac;
extern fp_1int tx_black_market;
extern fp_3void tx_setup_player;
extern fp_0void tx_balance;
extern fp_3int tx_order_veh;
extern fp_3int tx_veh_cost;
extern fp_1void tx_base_first;
extern fp_1int tx_action;
extern fp_2void tx_parse_string;
extern fp_1void tx_kill;
extern fp_1int tx_spying;
extern fp_0int tx_text_get;
extern fp_4void tx_set_treaty;
extern fp_3int tx_veh_drop;
extern fp_7void tx_battle_fight_2;
extern fp_3int tx_break_treaty;
extern fp_2void tx_act_of_aggression;
extern fp_2void tx_tech_research;
extern fp_3void tx_bitmask;
extern fp_3void tx_action_move;
extern fp_1int tx_veh_cargo;
extern fp_3int tx_zoc_move;
extern fp_4int base_prod_choice;
extern fp_2void base_prod_change;
extern fp_1void enemy_units_check;
extern fp_1int unit_speed;
extern fp_2void energy_compute;
extern fp_2int tx_can_arty;
extern fp_5void clip;
extern fp_5int mineral_yield;
extern tc_2int font_width;
extern tc_4int buffer_box;
extern tc_3int buffer_fill3;
extern tc_5int buffer_write_l;

