
#include "terranx.h"

const char** engine_version = (const char**)0x691870;
const char** engine_date = (const char**)0x691874;
const char* last_save_path = (const char*)0x9A66C9;
BASE** current_base_ptr = (BASE**)0x90EA30;
int* current_base_id = (int*)0x689370;
int* game_preferences = (int*)0x9A6490;
int* game_more_preferences = (int*)0x9A6494;
int* game_state = (int*)0x9A64C0;
int* game_rules = (int*)0x9A649C;
int* diff_level = (int*)0x9A64C4;
int* smacx_enabled = (int*)0x9A6488;
int* human_players = (int*)0x9A64E8;
int* current_turn = (int*)0x9A64D4;
int* active_faction = (int*)0x9A6820;
int* total_num_bases = (int*)0x9A64CC;
int* total_num_vehicles = (int*)0x9A64C8;
int* map_random_seed = (int*)0x949878;
int* map_toggle_flat = (int*)0x94988C;
int* map_area_tiles = (int*)0x949884;
int* map_area_sq_root = (int*)0x949888;
int* map_axis_x = (int*)0x949870;
int* map_axis_y = (int*)0x949874;
int* map_half_x = (int*)0x68FAF0;
int* climate_future_change = (int*)0x9A67D8;
int* un_charter_repeals = (int*)0x9A6638;
int* un_charter_reinstates = (int*)0x9A663C;
int* gender_default = (int*)0x9BBFEC;
int* plurality_default = (int*)0x9BBFF0;
int* current_player_faction = (int*)0x939284;
int* multiplayer_active = (int*)0x93F660;
int* pbem_active = (int*)0x93A95C;
int* sunspot_duration = (int*)0x9A6800;
int* diplo_active_faction = (int*)0x93F7CC;
int* diplo_current_friction = (int*)0x93FA74;
int* diplo_opponent_faction = (int*)0x8A4164;
int* base_find_dist = (int*)0x90EA04;
int* veh_attack_flags = (int*)0x93E904;
int* game_not_started = (int*)0x68F21C;
int* screen_width = (int*)0x9B7B1C;
int* screen_height = (int*)0x9B7B20;

int* dword_915620 = (int*)0x915620;
int* dword_9B2068 = (int*)0x9B2068;
int* dword_9B7AE4 = (int*)0x9B7AE4;
int* dword_93A934 = (int*)0x93A934;
int* dword_945B18 = (int*)0x945B18;
int* dword_945B1C = (int*)0x945B1C;

int16_t (*FactionRankings)[8] = (int16_t (*)[8])(0x9A68AC);
uint8_t* TechOwners = (uint8_t*)0x9A6670;
int* SecretProjects = (int*)0x9A6514;
int* CostRatios = (int*)0x689378;
MFaction* MFactions = (MFaction*)0x946A50;
Faction* Factions = (Faction*)0x96C9E0;
BASE* Bases = (BASE*)0x97D040;
UNIT* Units = (UNIT*)0x9AB868;
VEH* Vehicles = (VEH*)0x952828;
MAP** MapPtr = (MAP**)0x94A30C;

// Rules parsed from alphax.txt
CRules*     Rules     = (CRules*)0x949738;
CTech*      Tech      = (CTech*)0x94F358;
CSocial*    Social    = (CSocial*)0x94B000;
CFacility*  Facility  = (CFacility*)0x9A4B68;
CAbility*   Ability   = (CAbility*)0x9AB538;
CChassis*   Chassis   = (CChassis*)0x94A330;
CCitizen*   Citizen   = (CCitizen*)0x946020;
CDefense*   Armor     = (CDefense*)0x94F278;
CReactor*   Reactor   = (CReactor*)0x9527F8;
CResource*  Resource  = (CResource*)0x945F50;
CTerraform* Terraform = (CTerraform*)0x691878;
CWeapon*    Weapon    = (CWeapon*)0x94AE60;

Fbattle_fight_1 battle_fight_1 = (Fbattle_fight_1)0x506A60;
Fpropose_proto propose_proto = (Fpropose_proto)0x580860;
Faction_airdrop action_airdrop = (Faction_airdrop)0x4CC360;
Faction_destroy action_destroy = (Faction_destroy)0x4CAA50;
Fhas_abil has_abil = (Fhas_abil)0x5BF1F0;
Fparse_says parse_says = (Fparse_says)0x625EC0;
Fpopp popp = (Fpopp)0x48C0A0;
Fhex_cost hex_cost = (Fhex_cost)0x593510;

fp_4int* veh_init = (fp_4int*)0x5C03D0;
fp_1int* veh_skip = (fp_1int*)0x5C1D20;
fp_2int* veh_at = (fp_2int*)0x5BFE90;
fp_2int* veh_speed = (fp_2int*)0x5C1540;
fp_3int* zoc_any = (fp_3int*)0x5C89F0;
fp_1int* monolith = (fp_1int*)0x57A050;
fp_2int* action_build = (fp_2int*)0x4C96E0;
fp_3int* action_terraform = (fp_3int*)0x4C9B00;
fp_3int* terraform_cost = (fp_3int*)0x4C9420;
fp_2int* bonus_at = (fp_2int*)0x592030;
fp_2int* goody_at = (fp_2int*)0x592140;
fp_3int* cost_factor = (fp_3int*)0x4E4430;
fp_3int* site_set = (fp_3int*)0x591B50;
fp_3int* world_site = (fp_3int*)0x5C4FD0;
fp_1int* set_base = (fp_1int*)0x4E39D0;
fp_1int* base_compute = (fp_1int*)0x4EC3B0;
fp_4int* base_prod_choices = (fp_4int*)0x4F81A0;
fp_void* turn_upkeep = (fp_void*)0x5258C0;
fp_1int* faction_upkeep = (fp_1int*)0x527290;
fp_1int* action_staple = (fp_1int*)0x4CA7F0;
fp_1int* social_upkeep = (fp_1int*)0x5B44D0;
fp_1int* repair_phase = (fp_1int*)0x526030;
fp_1int* production_phase = (fp_1int*)0x526E70;
fp_1int* allocate_energy = (fp_1int*)0x5267B0;
fp_1int* enemy_diplomacy = (fp_1int*)0x55F930;
fp_1int* enemy_strategy = (fp_1int*)0x561080;
fp_1int* corner_market = (fp_1int*)0x59EE50;
fp_1int* call_council = (fp_1int*)0x52C880;
fp_3int* setup_player = (fp_3int*)0x5B0E00;
fp_2int* eliminate_player = (fp_2int*)0x5B3380;
fp_2int* can_call_council = (fp_2int*)0x52C670;
fp_void* do_all_non_input = (fp_void*)0x5FCB20;
fp_void* auto_save = (fp_void*)0x5ABD20;
fp_2int* parse_num = (fp_2int*)0x625E30;
fp_3int* capture_base = (fp_3int*)0x50C510;
fp_1int* base_kill = (fp_1int*)0x4E5250;
fp_5int* crop_yield = (fp_5int*)0x4E6E50;
fp_6int* base_draw = (fp_6int*)0x55AF20;
fp_6int* base_find3 = (fp_6int*)0x4E3D50;
fp_3int* draw_tile = (fp_3int*)0x46AF40;
tc_2int* font_width = (tc_2int*)0x619280;
tc_4int* buffer_box = (tc_4int*)0x5E3203;
tc_3int* buffer_fill3 = (tc_3int*)0x5DFCD0;
tc_5int* buffer_write_l = (tc_5int*)0x5DCEA0;
fp_6int* social_ai = (fp_6int*)0x5B4790;
fp_1int* social_set = (fp_1int*)0x5B4600;
fp_1int* pop_goal = (fp_1int*)0x4EF090;
fp_1int* consider_designs = (fp_1int*)0x581260;
fp_3int* tech_val = (fp_3int*)0x5BCBE0;
fp_1int* tech_rate = (fp_1int*)0x5BE6B0;
fp_1int* tech_selection = (fp_1int*)0x5BE380;
fp_1int* enemy_move = (fp_1int*)0x56B5B0;
fp_3int* best_defender = (fp_3int*)0x5044D0;
fp_5int* battle_compute = (fp_5int*)0x501DA0;
fp_6int* battle_kill = (fp_6int*)0x505D80;
fp_7int* battle_fight_2 = (fp_7int*)0x506AF0;
fp_void* draw_cursor = (fp_void*)0x46AE00;
fp_1int* veh_kill = (fp_1int*)0x5C08C0;
fp_1int* veh_wake = (fp_1int*)0x5C1D70;
fp_1int* stack_fix = (fp_1int*)0x5B8E10;
fp_2int* stack_veh = (fp_2int*)0x5B8EE0;

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

// tech research
fp_2void *tx_tech_research = (fp_2void *)0x005BE940;

// bitmask
fp_3void *tx_bitmask = (fp_3void *)0x0050BA00;

// action move
fp_3void *tx_action_move = (fp_3void *)0x004CD090;

// how many units can this vehicle carry
fp_1int *tx_veh_cargo = (fp_1int *)0x005C1760;

// ZOC check
fp_3int *tx_zoc_move = (fp_3int *)0x005C8D40;

