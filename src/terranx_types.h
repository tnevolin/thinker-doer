#ifndef __TERRANX_TYPES_H__
#define __TERRANX_TYPES_H__

#pragma pack(push, 1)
struct BASE
{
    short x;                                // +0x0000
    short y;                                // +0x0002
    char faction_id;                        // +0x0004
    char faction_id_former;                 // +0x0005
    char pop_size;                          // +0x0006
    char assimilation_turns_left;           // +0x0007
    char nerve_staple_turns_left;           // +0x0008
    char ai_plan_status;                    // +0x0009
    char factions_spotted_flags;            // +0x000A
    char factions_pop_size_intel[8];        // +0x000B
    char name[25];                          // +0x0013
    short unk_x_coord;                      // +0x002C
    short unk_y_coord;                      // +0x002E
    int status_flags;                       // +0x0030
    int event_flags;                        // +0x0034
    int governor_flags;                     // +0x0038
    int nutrients_accumulated;              // +0x003C
    int minerals_accumulated;               // +0x0040
    int production_id_last;
    int eco_damage;
    int queue_size;
    int queue_items[10];                    // +0x0050
    int worked_tiles;                       // +0x0078
    int specialist_total;
    int pad3;
    int pad4;
    int pad5;                               // +0x0088
    char facilities_built[12];              // +0x008C
    int mineral_surplus_final;              // +0x0098
    int minerals_accumulated_2;             // +0x009C
    int pad6;                               // +0x00A0
    int pad7;
    int pad8;
    int pad9;
    int nutrient_intake;                    // +0x00B0
    int mineral_intake;
    int energy_intake;
    int unused_intake;
    int nutrient_intake_2;                  // +0x00C0
    int mineral_intake_2;
    int energy_intake_2;
    int unused_intake_2;
    int nutrient_surplus;                   // +0x00D0
    int mineral_surplus;                    // +0x00D4
    int energy_surplus;                     // +0x00D8
    int unused_surplus;                     // +0x00DC
    int nutrient_inefficiency;
    int mineral_inefficiency;
    int energy_inefficiency;
    int unused_inefficiency;
    int nutrient_consumption;
    int mineral_consumption;
    int energy_consumption;                 // +0xD4
    int unused_consumption;                 // +0xD8
    int economy_total;
    int psych_total;
    int labs_total;
    int unk10;
    short autoforward_land_base_id;
    short autoforward_sea_base_id;
    short autoforward_air_base_id;
    short pad10;
    int talent_total;
    int drone_total;
    int superdrone_total;
    int random_event_turns;
    int nerve_staple_count;
    int pad11;
    int pad12;
};

struct UNIT
{
    char name[32];                  //0x0000
    int ability_flags;              //0x0020
    char chassis_type;              //0x0024
    char weapon_type;               //0x0025
    char armor_type;                //0x0026
    char reactor_type;              //0x0027
    char carry_capacity;            //0x0028
    char cost;                      //0x0029
    char unit_plan;                 //0x002A
    char unk0; // this is used      //0x002B
    char factions_retired;          //0x002C
    char factions;                  //0x002D
    char icon_offset;               //0x002E
    char unk1; // not used          //0x002F
    short unit_flags;               //0x0030
    short preq_tech;                //0x0032
};

struct VEH
{
    short x;                        //0x0000
    short y;                        //0x0002
    int flags_1;                    //0x0004
    short flags_2;                  //0x0008
    short proto_id;                 //0x000A
    short pad_0;
    char faction_id;
    char year_end_lurking;
    char damage_taken;
    char move_status;
    char waypoint_count;
    char patrol_current_point;
    short waypoint_1_x;
    short waypoint_2_x;
    short waypoint_3_x;
    short waypoint_4_x;
    short waypoint_1_y;
    short waypoint_2_y;
    short waypoint_3_y;
    short waypoint_4_y;
    char morale;
    char terraforming_turns;
    char type_crawling;
    byte visibility;
    char road_moves_spent;
    char unk5;
    char iter_count;
    char status_icon;
    char unk8;
    char unk9;
    short home_base_id;
    short next_unit_id_stack;
    short prev_unit_id_stack;
};

struct MAP
{
    byte level;             // 0x0000
    byte altitude;          // 0x0001
    byte flags;             // 0x0002
    // continent/sea id
    byte body_id;           // 0x0003
    byte visibility;        // 0x0004
    byte rocks;             // 0x0005
    byte unk_1;             // 0x0006
    char owner;             // 0x0007
    int items;              // 0x0008
    short landmarks;        // 0x000C
    byte unk_2;
    byte art_ref_id;
    int visible_items[7];
};

struct FactMeta
{
    int is_leader_female;           // 0x0000
    char filename[24];              // 0x0004
    char search_key[24];            // 0x001C
    char name_leader[24];           // 0x0034
    char title_leader[24];          // 0x004C
    char adj_leader[128];           // 0x0064
    char adj_insult_leader[128];    // 0x00E4
    char adj_faction[128];          // 0x0164
    char adj_insult_faction[128];   // 0x01E4
    char pad_1[128];                // 0x0264
    char noun_faction[24];          // 0x02E4
    int noun_gender;                // 0x02FC
    int is_noun_plural;             // 0x0300
    char adj_name_faction[128];     // 0x0304
    char formal_name_faction[40];   // 0x0384
    char insult_leader[24];         // 0x03AC
    char desc_name_faction[24];     // 0x03C4
    char assistant_name[24];        // 0x03DC
    char scientist_name[24];        // 0x03F4
    char assistant_city[24];        // 0x040C
    char pad_2[176];                // 0x0424
    int rule_tech_selected;         // 0x04D4
    int rule_morale;
    int rule_research;
    int rule_drone;                 // 0x04E0
    int rule_talent;
    int rule_energy;
    int rule_interest;
    int rule_population;            // 0x04F0
    int rule_hurry;
    int rule_techcost;
    int rule_psi;
    int rule_sharetech;             // 0x0500
    int rule_commerce;
    int rule_flags;                 // 0x0508
    int faction_bonus_count;
    int faction_bonus_id[8];        // 0x0510
    int faction_bonus_val1[8];      // 0x0530
    int faction_bonus_val2[8];      // 0x0550
    int AI_fight;                   // 0x0570
    int AI_growth;
    int AI_tech;
    int AI_wealth;
    int AI_power;
    int soc_priority_category;
    int soc_opposition_category;
    int soc_priority_model;
    int soc_opposition_model;
    int soc_priority_effect;
    int soc_opposition_effect;
};

struct Goal
{
    short type;
    short unk_1;
    int x;
    int y;
    int unk_2;
};

struct Faction
{
    int diplo_flags;                // 0x0000
    int ranking;
    int diff_level;
    int unk_0;
    int unk_1;                      // 0x0010
    int tutorial_more_bases;
    int diplo_status[8];            // 0x0018
    int diplo_agenda[8];            // 0x0038
    int diplo_friction_1[8];        // 0x0058
    int diplo_turn_check[8];        // 0x0078
    int diplo_treaty_turns[8];      // 0x0098
    char diplo_friction_2[8];       // 0x00B8
    int sanction_turns;             // 0x00C0
    int loan_years[8];              // 0x00C4
    int loan_payment[8];            // 0x00E4
    char gap_104[32];               // 0x0104
    int minor_atrocities;           // 0x0124
    int global_reputation;
    int diplo_unk1[8];              // 0x012C
    int diplo_unk2[8];              // 0x014C
    int diplo_backstabs[8];         // 0x016C
    int diplo_unk3[8];              // 0x018C
    int diplo_unk4[8];              // 0x01AC
    int map_trade_done;             // 0x01CC
    int governor_def_flags;         // 0x01D0
    int unk_2;
    int major_atrocities;
    int unk_3;
    int unk_4[8];                   // 0x01E0
    int unk_5[8];                   // 0x0200
    int energy_credits;             // 0x0220
    int energy_cost;
    int SE_Politics_pending;        // 0x0222
    int SE_Economics_pending;
    int SE_Values_pending;          // 0x0230
    int SE_Future_pending;
    int SE_Politics;                // 0x0238
    int SE_Economics;
    int SE_Values;                  // 0x0240
    int SE_Future;
    int SE_upheaval_cost_paid;
    int SE_economy_pending;
    int SE_effic_pending;           // 0x0250
    int SE_support_pending;
    int SE_talent_pending;
    int SE_morale_pending;
    int SE_police_pending;          // 0x0260
    int SE_growth_pending;
    int SE_planet_pending;
    int SE_probe_pending;
    int SE_industry_pending;        // 0x0270
    int SE_research_pending;
    int SE_economy;
    int SE_effic;
    int SE_support;                 // 0x0280
    int SE_talent;
    int SE_morale;                  // 0x0288
    int SE_police;
    int SE_growth;                  // 0x0290
    int SE_planet;
    int SE_probe;
    int SE_industry;                // 0x029C
    int SE_research;                // 0x02A0
    int SE_economy_2;
    int SE_effic_2;
    int SE_support_2;
    int SE_talent_2;                // 0x02B0
    int SE_morale_2;
    int SE_police_2;
    int SE_growth_2;
    int SE_planet_2;                // 0x02C0
    int SE_probe_2;
    int SE_industry_2;              // 0x02C8
    int SE_research_2;
    int SE_economy_base;            // 0x02D0
    int SE_effic_base;
    int SE_support_base;
    int SE_talent_base;
    int SE_morale_base;             // 0x02E0
    int SE_police_base;
    int SE_growth_base;
    int SE_planet_base;
    int SE_probe_base;              // 0x02F0
    int SE_industry_base;
    int SE_research_base;
    int unk_13;
    int unk_14;                     // 0x0300
    int tech_commerce_bonus;
    int unk_16;
    int unk_17;
    int unk_18;                     // 0x0310
    int tech_fungus_nutrient;
    int tech_fungus_mineral;
    int tech_fungus_energy;
    int unk_22;                     // 0x0320
    int SE_alloc_psych;
    int SE_alloc_labs;
    int unk_25;
    char gap_330[44];               // 0x0330
    int tech_ranking;               // 0x035C
    int unk_26;                     // 0x0360
    int ODP_deployed;               // 0x0364
    int theory_of_everything;       // 0x0368
    char tech_trade_source[92];     // 0x036C
    int tech_accumulated;           // 0x03C8
    int tech_research_id;           // 0x03CC
    int tech_cost;                  // 0x03D0
    int earned_techs_saved;
    int net_random_event;
    int AI_fight;
    int AI_growth;                  // 0x03E0
    int AI_tech;
    int AI_wealth;
    int AI_power;
    int target_x;                   // 0x03F0
    int target_y;                   // 0x03F4
    int unk_28;                     // 0x03F8
    int council_call_turn;          // 0x03FC
    int unk_29[11];                 // 0x0400
    int unk_30[11];
    int unk_31;
    int unk_32;
    char unk_33;
    char unk_34;
    char gap_462[2];
    int unk_35;
    int unk_36;
    int unk_37;
    char gap_470[192];
    int saved_queue_size[8];
    int saved_queue_items[78];
    int unk_38;
    char unk_39[4];
    int unk_40;
    char unk_41[12];
    int unk_42;
    char unk_43[12];
    int unk_44;
    char gap_6B4[80];
    int unk_45;
    char gap_708[200];
    int unk_46;
    char gap_7D4[32];
    int unk_47;
    int unk_48;
    int unk_49;
    int unk_50;
    int labs_total;
    int satellites_nutrient;
    int satellites_mineral;
    int satellites_energy;
    int satellites_ODP;
    int best_weapon_value;
    int unk_55;
    int unk_56;
    int best_armor_value;
    int unk_58;
    int unk_59;
    int unk_60;
    int unk_61;
    int unk_62;
    int unk_63;
    int unk_64;
    int unk_65;
    int unk_66;
    int unk_67;
    int unk_68;
    char unk_69[4];
    byte units_active[512];
    byte units_queue[512];
    short units_lost[512];
    int total_mil_units;
    int current_num_bases;
    int mil_strength_1;
    int mil_strength_2;
    int pop_total;
    int unk_70;
    int planet_busters;
    int unk_71;
    int unk_72;
    short unk_73[128];
    char unk_74[128];
    char unk_75[128];
    short unk_76[128];
    short unk_77[128];
    short unk_78[128];
    short unk_79[128];
    short unk_80[128];
    short unk_81[128];
    char unk_82[128];
    char unk_83[128];
    char unk_84[128];
    Goal goals_1[75];
    Goal goals_2[25];
    int unk_93;
    int unk_94;
    int unk_95;
    int unk_96;
    int unk_97;
    int unk_98;
    int unk_99;
    char gap_2058[4];
    int unk_100[8];
    int corner_market_turn;
    int corner_market_active;
    int unk_101;
    int unk_102;
    int unk_103;
    int unk_104;
    int unk_105;
    int unk_106;
    int unk_107;
    int unk_108;
    /*
    Thinker-specific save game variables.
    The game engine overwrites these locations during the endgame score calculation.
    */
    short thinker_header;
    char thinker_flags;
    char thinker_tech_id;
    int thinker_tech_cost;
    int unk_111;
    int unk_112;
    int unk_113;
    int unk_114;
    int unk_115;
    int unk_116;
    int unk_117;
    int unk_118;
};

struct R_Basic
{
    int mov_rate_along_roads;
    int nutrient_intake_req_citizen;
    int max_airdrop_rng_wo_orbital_insert;
    int artillery_max_rng;
    int artillery_dmg_numerator;
    int artillery_dmg_denominator;
    int nutrient_cost_multi;
    int mineral_cost_multi;
    int rules_tech_discovery_rate;
    int limit_mineral_inc_for_mine_wo_road;
    int nutrient_effect_mine_sq;
    int min_base_size_specialists;
    int drones_induced_genejack_factory;
    int pop_limit_wo_hab_complex;
    int pop_limit_wo_hab_dome;
    int tech_preq_improv_fungus;
    int tech_preq_ease_fungus_mov;
    int tech_preq_build_road_fungus;
    int tech_preq_allow_2_spec_abil;
    int tech_preq_allow_3_nutrients_sq;
    int tech_preq_allow_3_minerals_sq;
    int tech_preq_allow_3_energy_sq;
    int extra_cost_prototype_land;
    int extra_cost_prototype_sea;
    int extra_cost_prototype_air;
    int psi_combat_land_numerator;
    int psi_combat_sea_numerator;
    int psi_combat_air_numerator;
    int psi_combat_land_denominator;
    int psi_combat_sea_denominator;
    int psi_combat_air_denominator;
    int starting_energy_reserve;
    int combat_bonus_intrinsic_base_def;
    int combat_bonus_atk_road;
    int combat_bonus_atk_higher_elevation;
    int combat_penalty_atk_lwr_elevation;
    int tech_preq_orb_insert_wo_space;
    int min_turns_between_councils;
    int minerals_harvesting_forest;
    int territory_max_dist_base;
    int turns_corner_global_energy_market;
    int tech_preq_mining_platform_bonus;
    int tech_preq_economic_victory;
    int combat_penalty_atk_airdrop;
    int combat_bonus_fanatic;
    int combat_land_vs_sea_artillery;
    int combat_artillery_bonus_altitude;
    int combat_mobile_open_ground;
    int combat_mobile_def_in_rough;
    int combat_bonus_trance_vs_psi;
    int combat_bonus_empath_song_vs_psi;
    int combat_infantry_vs_base;
    int combat_penalty_air_supr_vs_ground;
    int combat_bonus_air_supr_vs_air;
    int combat_penalty_non_combat_vs_combat;
    int combat_comm_jammer_vs_mobile;
    int combat_bonus_vs_ship_port;
    int combat_AAA_bonus_vs_air;
    int combat_defend_sensor;
    int combat_psi_bonus_per_PLANET;
    int retool_strictness;
    int retool_penalty_prod_change;
    int retool_exemption;
    int tgl_probe_steal_tech;
    int tgl_humans_always_contact_tcp;
    int tgl_humans_always_contact_pbem;
    int max_dmg_percent_arty_base_bunker;
    int max_dmg_percent_arty_open;
    int max_dmg_percent_arty_sea;
    int freq_global_warming_numerator;
    int freq_global_warming_denominator;
    int normal_start_year;
    int normal_ending_year_lowest_3_diff;
    int normal_ending_year_highest_3_diff;
    int tgl_oblit_base_atrocity;
    int base_size_subspace_gen;
    int subspace_gen_req;
};

struct R_Resource
{
    int ocean_sq_nutrient;
    int ocean_sq_mineral;
    int ocean_sq_energy;
    int pad_0;
    int base_sq_nutrient;
    int base_sq_mineral;
    int base_sq_energy;
    int pad_1;
    int bonus_sq_nutrient;
    int bonus_sq_mineral;
    int bonus_sq_energy;
    int pad_2;
    int forest_sq_nutrient;
    int forest_sq_mineral;
    int forest_sq_energy;
    int pad_3;
    int recycling_tanks_nutrient;
    int recycling_tanks_mineral;
    int recycling_tanks_energy;
    int pad_4;
    int improved_land_nutrient;
    int improved_land_mineral;
    int improved_land_energy;
    int pad_5;
    int improved_sea_nutrient;
    int improved_sea_mineral;
    int improved_sea_energy;
    int pad_6;
    int monolith_nutrient;
    int monolith_mineral;
    int monolith_energy;
    int pad_7;
    int borehole_sq_nutrient;
    int borehole_sq_mineral;
    int borehole_sq_energy;
    int pad_8;
};

struct R_Social
{
    char* field_name;
    int   soc_preq_tech[4];
    char* soc_name[4];
    int   effects[4][11];
};

struct R_Facility
{
    char* name;
    char* effect;
    int pad;
    int cost;
    int maint;
    int preq_tech;
    int free;
    int AI_fight;
    int AI_growth;
    int AI_tech;
    int AI_wealth;
    int AI_power;
};

struct R_Tech
{
    int flags;
    char* name;
    char short_name[12]; // technology short name that is used as reference in other places (prerequisites, etc.)
    int AI_growth;
    int AI_tech;
    int AI_wealth;
    int AI_power;
    int preq_tech1;
    int preq_tech2;
};

struct R_Ability
{
    char* name;
    char* description;
    char* abbreviation;
    int cost;
    int padding;
    int flags;
    int preq_tech;
};

struct R_Chassis
{
    char* offsv1_name;
    char* offsv2_name;
    char* offsv_name_lrg;
    char* defsv1_name;
    char* defsv2_name;
    char* defsv_name_lrg;
    int offsv1_gender;
    int offsv2_gender;
    int offsv_gender_lrg;
    int defsv1_gender;
    int defsv2_gender;
    int defsv_gender_lrg;
    int offsv1_is_plural;
    int offsv2_is_plural;
    int offsv_is_plural_lrg;
    int defsv1_is_plural;
    int defsv2_is_plural;
    int defsv_is_plural_lrg;
    char speed;
    char triad;
    char range;
    char cargo;
    char cost;
    char missile;
    char sprite_flag_x_coord[8];
    char sprite_flag_y_coord[8];
    char sprite_unk1_x_coord[8];
    char sprite_unk1_y_coord[8];
    char sprite_unk2_x_coord[8];
    char sprite_unk2_y_coord[8];
    char sprite_unk3_x_coord[8];
    char sprite_unk3_y_coord[8];
    short preq_tech;
};

struct R_Citizen
{
    char* singular_name;
    char* plural_name;
    int preq_tech;
    int obsol_tech;
    int ops_bonus;
    int psych_bonus;
    int research_bonus;
};

struct R_Defense
{
    char* name;
    char* name_short;
    char defense_value;
    char mode;
    char cost;
    char padding1;
    short preq_tech;
    short padding2;
};

struct R_Reactor
{
    char* name;
    char* name_short;
    short preq_tech;
    short cost_factor; // Renamed from padding. Will be used to store cost factor.
};

struct R_Terraform
{
    char* name;
    char* name_sea;
    int preq_tech;
    int preq_tech_sea;
    int flag;
    int flag_sea;
    int rate;
    char* shortcuts;
};

struct R_Weapon
{
    char* name;
    char* name_short;
    char offense_value;
    char icon;
    char mode;
    char cost;
    short preq_tech;
    short padding;
};

#pragma pack(pop)

#endif // __TERRANX_TYPES_H__
