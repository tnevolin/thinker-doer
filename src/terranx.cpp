
#include "terranx.h"


const char** tx_version = (const char**)0x691870;
const char** tx_date = (const char**)0x691874;

byte* tx_tech_discovered = (byte*)0x9A6670;
int* tx_secret_projects = (int*)0x9A6514;
int* tx_cost_ratios = (int*)0x689378;
short (*tx_faction_rankings)[8] = (short (*)[8])(0x9A68AC);
MetaFaction* tx_metafactions = (MetaFaction*)0x946A50;
Faction* tx_factions = (Faction*)0x96C9E0;
BASE* tx_bases = (BASE*)0x97D040;
UNIT* tx_units = (UNIT*)0x9AB868;
VEH* tx_vehicles = (VEH*)0x952828;
MAP** tx_map_ptr = (MAP**)0x94A30C;

R_Basic* tx_basic = (R_Basic*)0x949738;
R_Tech* tx_techs = (R_Tech*)0x94F358;
R_Social* tx_social = (R_Social*)0x94B000;
R_Facility* tx_facility = (R_Facility*)0x9A4B68;
R_Ability* tx_ability = (R_Ability*)0x9AB538;
R_Chassis* tx_chassis = (R_Chassis*)0x94A330;
R_Citizen* tx_citizen = (R_Citizen*)0x946020;
R_Defense* tx_defense = (R_Defense*)0x94F278;
R_Reactor* tx_reactor = (R_Reactor*)0x9527F8;
R_Resource* tx_resource = (R_Resource*)0x945F50;
R_Terraform* tx_terraform = (R_Terraform*)0x691878;
R_Weapon* tx_weapon = (R_Weapon*)0x94AE60;

// battle computation display variables
int *tx_battle_compute_attacker_effect_count = (int *)0x915614;
char (*tx_battle_compute_attacker_effect_labels)[0x4][0x50] = (char (*)[0x4][0x50])0x90F554;
int (*tx_battle_compute_attacker_effect_values)[0x4] = (int (*)[0x4])0x9155F0;
int *tx_battle_compute_defender_effect_count = (int *)0x915618;
char (*tx_battle_compute_defender_effect_labels)[0x4][0x50] = (char (*)[0x4][0x50])0x90F694;
int (*tx_battle_compute_defender_effect_values)[0x4] = (int (*)[0x4])0x915600;

// labels pointer
char ***tx_labels = (char ***)0x009B90F8;

fp_7intstr* tx_propose_proto = (fp_7intstr*)0x580860;
fp_4int* tx_veh_init = (fp_4int*)0x5C03D0;
fp_1int* tx_veh_skip = (fp_1int*)0x5C1D20;
fp_2int* tx_veh_at = (fp_2int*)0x5BFE90;
fp_2int* tx_veh_speed = (fp_2int*)0x5C1540;
fp_3int* tx_zoc_any = (fp_3int*)0x5C89F0;
fp_3int* tx_best_defender = (fp_3int*)0x5044D0;
fp_5int* tx_battle_compute = (fp_5int*)0x501DA0;
fp_6int* tx_battle_kill = (fp_6int*)0x505D80;
fp_1void* tx_enemy_units_check = (fp_1void*)0x00579510;
fp_1int* tx_enemy_move = (fp_1int*)0x56B5B0;
fp_1int* tx_monolith = (fp_1int*)0x57A050;
/*
Builds base.
(vehicle id, ?)
*/
fp_2int* tx_action_build = (fp_2int*)0x4C96E0;
/*
Sets vehicle move_action to given terraform action and returns number of terraforming turns - 1.
(vehicle id, terrafroming action, 0 = return terraforming turns only or 1 = also set vehicle move_action)
*/
fp_3int* tx_action_terraform = (fp_3int*)0x004C9B00;
/*
Calculates cost of raising land in energy credits.
(tile x, tile y, faction id)
*/
fp_3int* tx_terraform_cost = (fp_3int*)0x4C9420;
fp_2int* tx_bonus_at = (fp_2int*)0x592030;
fp_2int* tx_goody_at = (fp_2int*)0x592140;
fp_3int* tx_cost_factor = (fp_3int*)0x4E4430;
fp_3int* tx_site_set = (fp_3int*)0x591B50;
fp_3int* tx_world_site = (fp_3int*)0x5C4FD0;
fp_1int* tx_set_base = (fp_1int*)0x4E39D0;
fp_1int* tx_base_compute = (fp_1int*)0x4EC3B0;
fp_4int* tx_base_prod_choices = (fp_4int*)0x4F81A0;
fp_0int* tx_turn_upkeep = (fp_0int*)0x5258C0;
fp_1int* tx_faction_upkeep = (fp_1int*)0x527290;
fp_3int* tx_tech_val = (fp_3int*)0x5BCBE0;
fp_6int* tx_social_ai = (fp_6int*)0x5B4790;
fp_1int* tx_social_set = (fp_1int*)0x5B4600;
fp_1int* tx_pop_goal = (fp_1int*)0x4EF090;
fp_1int* tx_consider_designs = (fp_1int*)0x581260;
fp_1int* tx_action_staple = (fp_1int*)0x4CA7F0;
fp_1int* tx_tech_rate = (fp_1int*)0x5BE6B0;
fp_1int* tx_tech_selection = (fp_1int*)0x5BE380;

// read basic rules
fp_0int *tx_read_basic_rules = (fp_0int* )0x00585170;

// calculates prototype cost
fp_5int *tx_proto_cost = (fp_5int* )0x005A5A60;

// creates prototype
fp_6int *tx_create_prototype = (fp_6int* )0x005A5D40;

// calculates upgrade cost
fp_3int *tx_upgrade_cost = (fp_3int* )0x004EFD50;

// concatenates strings
fp_2int_void *tx_strcat = (fp_2int_void* )0x00645470;

// base_find3
fp_6int *tx_base_find3 = (fp_6int* )0x004E3D50;

// calculate tile yield
fp_5int *tx_tile_yield = (fp_5int* )0x004E7DC0;

// base production
fp_0int *tx_base_production_choice = (fp_0int* )0x004F07E0;

// set SE on dialog close
fp_5int *tx_set_se_on_dialog_close = (fp_5int* )0x005B4210;

// hex cost
fp_7int *tx_hex_cost = (fp_7int* )0x00593510;

// checks if terraforming action is available
// terraforming action, sea flag, returns 0 unless action is advanced terraforming and The Weather Paradigm is owned
fp_3int *tx_terrain_avail = (fp_3int *)0x005BAB40;

// recalculates world climate
fp_0void *tx_world_climate = (fp_0void *)0x005C5A30;

// recalculates world rivers
fp_0void *tx_world_rivers = (fp_0void *)0x005C38B0;

// recalculates world rainfall
fp_0void *tx_world_rainfall = (fp_0void *)0x005C4470;

// send vehicle to destination
fp_4void *tx_go_to = (fp_4void *)0x00560AD0;

// copies base name to string buffer
fp_int_char_int *tx_say_base = (fp_int_char_int *)0x004E3A00;

// initializes base
fp_3int *tx_base_init = (fp_3int *)0x004E4B80;

// probe action
fp_4int *tx_probe = (fp_4int *)0x0059F120;

// set facility in base
fp_3int *tx_set_facility = (fp_3int *)0x004E48B0;

// itoa
fp_charp_int_charp_int *tx_itoa = (fp_charp_int_charp_int *)0x0064FC88;

// base_nutrient
fp_0void *tx_base_nutrient = (fp_0void *)0x004E9B70;

// set SE on dialog close
fp_5int *tx_social_calc = (fp_5int *)0x005B4210;

// computes base minerals
fp_3int *tx_has_fac = (fp_3int *)0x00421670;

// computes inefficiency based on given energy intake
fp_1int *tx_black_market = (fp_1int *)0x004EA1F0;

// sets up player
fp_3void *tx_setup_player = (fp_3void *)0x005B0E00;

// balances game by giving extra colony
fp_0void *tx_balance = (fp_0void *)0x005B0420;

// map initialization
fp_0void *tx_world_build = (fp_0void *)0x005C86E0;

// don't exactly know
fp_3int *tx_order_veh = (fp_3int *)0x005947C0;

// calculates unit production cost at base
// arg1 = unit ID
// arg2 = base ID, if not negative - Brood Pit and Skunkworks modify the cost when present
// arg3 = address where to store flag if unit cost is higher than base cost
fp_3int *tx_veh_cost = (fp_3int *)0x005C1850;

// set first time base production
fp_1void *tx_base_first = (fp_1void *)0x004E4AA0;

// executes action on vehicle
fp_1int *tx_action = (fp_1int *)0x004CF740;

// string formatting
fp_2void *tx_parse_string = (fp_2void *)0x00625880;

// kills vehicle
fp_1void *tx_kill = (fp_1void *)0x005C0B00;

// determing if g_CURRENT_PC_FACT_ID has infiltration to given faction
fp_1int *tx_spying = (fp_1int *)0x0055BC00;

// reads the line of text from file
fp_0int *tx_text_get = (fp_0int *)0x005FD570;

// manipulates diplo_status flags
fp_4void *tx_set_treaty = (fp_4void *)0x0055BB30;

// places vehicle at tile
fp_3int *tx_veh_drop = (fp_3int *)0x005C0080;

// combat computation stage
fp_7void *tx_battle_fight_2 = (fp_7void *)0x00506AF0;

// breaks treaty
fp_3int *tx_break_treaty = (fp_3int *)0x0055F770;

// act of aggression
fp_2void *tx_act_of_aggression = (fp_2void *)0x0050C2E0;

