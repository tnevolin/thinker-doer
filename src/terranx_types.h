#pragma once
#pragma pack(push, 1)

struct CChassis;
extern CChassis* Chassis;
struct UNIT;
extern UNIT* Units;
struct VEH;
extern VEH* Vehicles;

struct BASE {
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
    int production_id_last;					// +0x0044
    int eco_damage;							// +0x0048
    int queue_size;							// +0x004C
    int queue_items[10];                    // +0x0050
    int worked_tiles;                       // +0x0078
    int specialist_total;					// +0x007C
    int specialist_unk_1;					// +0x0080
    /*
    Specialist types (R_Citizen, 4 bits per id) for the first 16 specialists in the base.
    These are assigned in base_yield and base_energy and chosen by best_specialist.
    */
    int specialist_types[2];				// +0x0084
    char facilities_built[12];				// +0x008C
    int mineral_surplus_final;				// +0x0098
    int minerals_accumulated_2;				// +0x009C
    int pad_1;								// +0x00A0
    int pad_2;								// +0x00A4
    int pad_3;								// +0x00A8
    int pad_4;								// +0x00AC
    int nutrient_intake;					// +0x00B0
    int mineral_intake;						// +0x00B4
    int energy_intake;						// +0x00B8
    int unused_intake;
    int nutrient_intake_2;                  // +0x00C0
    int mineral_intake_multiplied;			// +0x00C4
    int energy_intake_2;					// +0x00C8
    int unused_intake_2;					// +0x00CC
    int nutrient_surplus;                   // +0x00D0
    int mineral_surplus;                    // +0x00D4
    int energy_surplus;                     // +0x00D8
    int unused_surplus;                     // +0x00DC
    int nutrient_inefficiency;				// +0x00E0
    int mineral_inefficiency;				// +0x00E4
    int energy_inefficiency;				// +0x00E8
    int unused_inefficiency;				// +0x00EC
    int nutrient_consumption;				// +0x00F0
    int mineral_consumption;				// +0x00F4
    int energy_consumption;					// +0x00F8
    int unused_consumption;					// +0x00FC
    int economy_total;						// +0x0100
    int psych_total;						// +0x0104
    int labs_total;							// +0x0108
    int unk_2;
    short autoforward_land_base_id;			// +0x0110
    short autoforward_sea_base_id;
    short autoforward_air_base_id;			// +0x0114
    short pad_5;
    int talent_total;						// +0x0118
    int drone_total;						// +0x011C
    int superdrone_total;					// +0x0120
    int random_event_turns;
    int nerve_staple_count;
    int pad_6;
    int pad_7;
};

struct MAP {
    byte level;				// 0x0000
    byte altitude;			// 0x0001
    /*
    flags & 0xF0:
    AI colonization priority returned by world_site()
    This is written to the map struct by site_set()
    0 = unknown/not available, 1..15 higher numbers indicate a better location.
    When AltGr+4 debug overlay is enabled, MapWin_gen_terrain_poly renders
    this number on land tiles where value > 0.
    flags & 0xF:
    Faction ID of the unit occupying this tile. 0xF = unoccupied.
    Sometimes faction ID of a deleted unit persists on the tile.
    */
    byte flags;				// 0x0002
    /*
    The game keeps track of disjoint land/water areas and assigns each of them an ID number
    which is used to index the [128] planning variable arrays in Faction struct.
    Valid ranges: 1-62 (land), 65-126 (sea).
    */
    byte region;
    byte visibility;
    byte rocks;
    byte unk_1;
    char owner;
    int items;
    short landmarks;
    byte unk_2; // 0x40 = set_dirty()
    byte art_ref_id;
    int visible_items[7];

    bool is_visible(int faction) {
        return visibility & (1 << faction);
    }
    bool is_base() {
        return items & TERRA_BASE_IN_TILE;
    }
    bool is_base_radius() {
        return items & TERRA_BASE_RADIUS;
    }
    bool is_base_bunker() {
        return items & (TERRA_BUNKER | TERRA_BASE_IN_TILE);
    }
};

struct MFaction {
    int is_leader_female;
    char filename[24];
    char search_key[24];
    char name_leader[24];
    char title_leader[24];
    char adj_leader[128];
    char adj_insult_leader[128];
    char adj_faction[128];
    char adj_insult_faction[128];
    /*
    Thinker-specific save game variables.
    */
    short thinker_header;
    char thinker_flags;
    char thinker_tech_id;
    int thinker_tech_cost;
    /*
    Exponentially weighted moving average of distances to nearest enemy bases.
    This is updated while choosing base production, and it is used as a
    general heuristic to determine the level of threat from other factions.
    When no enemies are present, the range is capped at 40.
    */
    float thinker_enemy_range;
    char pad_1[116];
    /*
    End of block
    */
    char noun_faction[24];
    int noun_gender;
    int is_noun_plural;
    char adj_name_faction[128];
    char formal_name_faction[40];
    char insult_leader[24];
    char desc_name_faction[24];
    char assistant_name[24];
    char scientist_name[24];
    char assistant_city[24];
    char pad_2[176];
    int rule_tech_selected;
    int rule_morale;
    int rule_research;
    int rule_drone;                 // 0x04E0
    int rule_talent;
    int rule_energy;
    int rule_interest;
    int rule_population;            // 0x04F0
    int rule_hurry;					// 0x04F4
    int rule_techcost;
    int rule_psi;
    int rule_sharetech;             // 0x0500
    int rule_commerce;
    int rule_flags;                 // 0x0508
    int faction_bonus_count;		// 0x050C
    int faction_bonus_id[8];        // 0x0510
    int faction_bonus_val1[8];      // 0x0530
    int faction_bonus_val2[8];      // 0x0550
    int AI_fight;                   // 0x0570
    int AI_growth;					// 0x0574
    int AI_tech;					// 0x0578
    int AI_wealth;					// 0x057C
    int AI_power;					// 0x0580
    int soc_priority_category;		// 0x0584
    int soc_opposition_category;	// 0x0588
    int soc_priority_model;			// 0x058C
    int soc_opposition_model;		// 0X0590
    int soc_priority_effect;
    int soc_opposition_effect;
};

struct Goal {
    short type;
    short priority;
    int x;
    int y;
    int base_id;
};

struct Faction {
    int player_flags;																						// 0x0000
    int ranking; // 0 (lowest) to 7 (highest)																// 0x0004
    int diff_level;																							// 0x0008
    int base_name_offset; // Keep track which base names have been used										// 0x000C
    int base_sea_name_offset; // Keep track which sea base names have been used								// 0x0010
    int last_base_turn; // Turn for last built, captured or acquired (drone riot) base						// 0x0014
    int diplo_status[8]; // Contains all formal treaties													// 0x0018
    int diplo_agenda[8];
    int diplo_friction[8];
    int diplo_spoke[8]; // Turn for the last player-to-AI communication; -1 for never
    int diplo_merc[8]; // Possibly higher values indicate willingness for deal-making
    char diplo_patience[8]; // AI-to-player
    int sanction_turns; // Turns left for economic sanctions imposed by other factions for atrocities
    int loan_balance[8]; // Loan balance remaining this faction owes another to be paid over term
    int loan_payment[8]; // The per turn payment amount this faction owes another faction
    int unk_1[8]; // unused
    int integrity_blemishes;
    int global_reputation;
    int diplo_gifts[8]; // ? Gifts and bribes we have received
    int diplo_wrongs[8]; // Number of times we double crossed this faction
    int diplo_betrayed[8]; // Number of times double crossed by this faction
    int diplo_unk_3[8]; // ? combat related
    int diplo_unk_4[8]; // ? combat related
    int traded_maps; // bitfield of other factions that have traded maps with faction
    int base_governor_adv; // default advanced Governor settings
    int atrocities; // count committed by faction
    int major_atrocities; // count committed by faction
    int subvert_total; // ? probe: mind control base (+4) / subvert unit (+1) total
    int diplo_subvert[8]; // ? probe: mind control base (+4) / subvert unit (+1) per faction
    int diplo_stolen_techs[8]; // probe: successfully procured research data (tech/map) per faction
    int energy_credits;				// 0x0220
    int energy_cost;				// 0x0224
    int SE_Politics_pending;        // 0x0228
    int SE_Economics_pending;		// 0x022C
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
    int SE_growth_pending;			// 0x0264
    int SE_planet_pending;
    int SE_probe_pending;
    int SE_industry_pending;        // 0x0270
    int SE_research_pending;        // 0x0274
    int SE_economy;					// 0x0278
    int SE_effic;
    int SE_support;                 // 0x0280
    int SE_talent;
    int SE_morale;                  // 0x0288
    int SE_police;					// 0x028C
    int SE_growth;                  // 0x0290
    int SE_planet;					// 0x0294
    int SE_probe;					// 0x0298
    int SE_industry;                // 0x029C
    int SE_research;                // 0x02A0
    int SE_economy_2;				// 0x02A4
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
    int SE_industry_base;			// 0x02F4
    int SE_research_base;
    int unk_13;
    int unk_14;
    int tech_commerce_bonus; // Increases commerce income
    int turn_commerce_income;
    int unk_17;
    int unk_18;                     // 0x0310
    int tech_fungus_nutrient;		// 0x0314
    int tech_fungus_mineral;		// 0x0318
    int tech_fungus_energy;			// 0x031C
    int unk_22;                     // 0x0320
    int SE_alloc_psych;				// 0x0324
    int SE_alloc_labs;				// 0x0328
    int unk_25;
    int unk_26[11]; // unused		// 0x0330
    int tech_ranking; // Twice the number of techs discovered
    int unk_27;						// 0x0360
    int ODP_deployed;
    int theory_of_everything;		// 0x0368
    char tech_trade_source[92];		// 0x036C
    int tech_accumulated;			// 0x03C8
    int tech_research_id;			// 0x03CC
    int tech_cost;					// 0x03D0
    int earned_techs_saved;			// 0x03D4
    int net_random_event;			// 0x03D8
    int AI_fight;
    int AI_growth;
    int AI_tech;
    int AI_wealth;
    int AI_power;
    int target_x;
    int target_y;
    int unk_28;
    int council_call_turn;
    int unk_29[11]; // Council related
    int unk_30[11]; // Council related
    byte facility_announced[4]; // bitfield - used to determine one time play of fac audio blurb
    int unk_32;
    int unk_33;
    int clean_minerals_modifier; // Starts from zero and increases by one after each fungal pop.
    int unk_36;
    int unk_37;
    char saved_queue_name[8][24];	// 0x0470
    int saved_queue_size[8];		// 0x0530
    int saved_queue_items[8][10];	// 0x0550
    int unk_40[8];					// 0x0690
    int unk_41[40];					// 0x06B0
    int unk_42[32];					// 0x06D8
    int unk_43[8];					// 0x0758
    int unk_44;						// 0x0778
    int unk_45;						// 0x077C
    int unk_46;						// 0x0780
    int unk_47;
    int nutrient_surplus_total;
    int labs_total;
    int satellites_nutrient;		// 0x0790
    int satellites_mineral;			// 0x0794
    int satellites_energy;			// 0x0798
    int satellites_ODP;
    int best_weapon_value;			// 0x07A0
    int best_psi_land_offense;
    int best_psi_land_defense;
    int best_armor_value;
    int best_land_speed;			// 0x07B0
    int enemy_best_weapon_value; // Enemy refers here to any non-pact faction
    int enemy_best_armor_value;
    int enemy_best_land_speed;
    int enemy_best_psi_land_offense;// 0x07C0
    int enemy_best_psi_land_defense;
    int unk_64;
    int unk_65;
    int unk_66;						// 0x07D0
    int unk_67;
    int unk_68;
    int unk_69;
    byte units_active[512];			// 0x07E0
    byte units_queue[512];			// 0x09E0
    short units_lost[512];			// 0x0BE0
    int total_combat_units;			// 0x0DE0
    int current_num_bases;			// 0x0DE4
    int mil_strength_1;				// 0x0DE8
    int mil_strength_2;				// 0x0DEC
    int pop_total;					// 0x0DF0
    int unk_70;						// 0x0DF4
    int planet_busters;				// 0x0DF8
    int unk_71;						// 0x0DFC
    int unk_72;						// 0x0E00
    /*
    AI planning variables that relate to faction units in specific disjoint land/water areas.
    All of these are indexed by the region value in MAP struct.
    */
    short region_total_combat_units[128];
    byte region_total_bases[128];
    byte region_total_offensive_units[128];
    short region_force_rating[128]; // Combined offensive/morale rating of all units in the area
    short region_flags[128]; // Movement planning flags
    short region_territory_tiles[128];
    short region_visible_tiles[128];
    short region_good_tiles[128];
    short region_unk_5[128]; // Unknown reset_territory/enemy_move counter
    byte region_unk_6[128]; // Unknown enemy_strategy state
    byte region_territory_goodies[128];
    byte region_base_plan[128]; // visible in map UI with omni view + debug mode under base name
    /* End of block */
    Goal goals[75];
    Goal sites[25];
    int unk_92;
    int unk_93;
    int unk_94;
    int unk_95;
    int unk_96;
    int unk_97;
    int unk_98;
    int unk_99;
    int unk_100[8];
    int corner_market_turn;
    int corner_market_active;
    int unk_101;
    int unk_102;
    int unk_103;
    int player_flags_ext;
    int unk_105;
    int unk_106;
    int unk_107;
    int unk_108;
    /*
    Thinker variables in the old save game format.
    */
    short old_thinker_header;
    char old_thinker_flags;
    char old_thinker_tech_id;
    int old_thinker_tech_cost;
    float old_thinker_enemy_range;
    int padding[7];
};

struct CRules {
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

struct CResource {
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

struct CSocial {
    char* field_name;
    int   soc_preq_tech[4];
    char* soc_name[4];
    int   effects[4][11];
};

struct CFacility {
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

struct CTech {
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

struct CAbility {
    char* name;
    char* description;
    char* abbreviation;
    int cost;
    int unk_1;
    int flags;
    short preq_tech;
    short pad;
};

struct CChassis {
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

struct CCitizen {
    char* singular_name;
    char* plural_name;
    int preq_tech;
    int obsol_tech;
    int ops_bonus;
    int psych_bonus;
    int research_bonus;
};

struct CDefense {
    char* name;
    char* name_short;
    char defense_value;
    char mode;
    char cost;
    char padding1;
    short preq_tech;
    short padding2;
};

struct CReactor {
    char* name;
    char* name_short;
    short preq_tech;
    short cost_factor; // Renamed from padding. Will be used to store cost factor.
};

struct CTerraform {
    char* name;
    char* name_sea;
    int preq_tech;
    int preq_tech_sea;
    int added_items_flag;
    int removed_items_flag;
    int rate;
    char* shortcuts;
};

struct CWeapon {
    char* name;
    char* name_short;
    char offense_value;
    char icon;
    char mode;
    char cost;
    short preq_tech;
    short padding;
};

struct UNIT {
    char name[32];
    int ability_flags;
    char chassis_type;
    char weapon_type;
    char armor_type;
    char reactor_type;
    char carry_capacity;
    char cost;
    char unit_plan;
    char unk_1; // some kind of internal prototype category?
    char obsolete_factions;// faction bitfield of those who marked this prototype obsolete
    char combat_factions; // faction bitfield for those that have seen this unit in combat (atk/def)
    char icon_offset;
    char pad_1; // unused
    short unit_flags;
    short preq_tech;

    int triad() {
        return Chassis[(int)chassis_type].triad;
    }
    int speed() {
        return Chassis[(int)chassis_type].speed;
    }
};

struct VEH {
    short x;
    short y;
    int state;
    short flags;
    short unit_id;
    short pad_0; // unused
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
    char rotate_angle;
    char iter_count;
    char status_icon;
    char probe_action;
    char probe_sabotage_id;
    short home_base_id;
    short next_unit_id_stack;
    short prev_unit_id_stack;

    const char* name() {
        return Units[unit_id].name;
    }
    int triad() {
        return Chassis[(int)Units[unit_id].chassis_type].triad;
    }
    int weapon_type() {
        return Units[unit_id].weapon_type;
    }
    bool is_combat_unit() {
        return Units[unit_id].weapon_type <= WPN_PSI_ATTACK && unit_id != BSC_FUNGAL_TOWER;
    }
    bool is_probe() {
        return Units[unit_id].weapon_type == WPN_PROBE_TEAM;
    }
    bool is_visible(int faction) {
        return visibility & (1 << faction);
    }
};

#pragma pack(pop)

