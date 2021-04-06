#include "terranx_wtp.h"

// global constants

const int NO_SYNC = 1;
const char* scriptTxtID = "SCRIPT";

// battle computation display variables
int *tx_battle_compute_attacker_effect_count = (int *)0x915614;
char (*tx_battle_compute_attacker_effect_labels)[0x4][0x50] = (char (*)[0x4][0x50])0x90F554;
int (*tx_battle_compute_attacker_effect_values)[0x4] = (int (*)[0x4])0x9155F0;
int *tx_battle_compute_defender_effect_count = (int *)0x915618;
char (*tx_battle_compute_defender_effect_labels)[0x4][0x50] = (char (*)[0x4][0x50])0x90F694;
int (*tx_battle_compute_defender_effect_values)[0x4] = (int (*)[0x4])0x915600;

// labels pointer
char ***tx_labels = (char ***)0x009B90F8;

char *g_strTEMP = (char *)0x009B86A0;
int *current_base_growth_rate = (int *)0x0090E918;
int *g_PROBE_FACT_TARGET = (int *)0x00945B34;
int *g_UNK_ATTACK_FLAGS = (int *)0x0093E904;
byte *g_SOCIALWIN = (byte *)0x008A6270;

fp_void_charp_int *say_orders = (fp_void_charp_int *)0x4B43E0;
fp_1void *say_orders2 = (fp_1void *)0x004B4970;

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
fp_0int *tx_base_production = (fp_0int* )0x004F07E0;

// set SE on dialog close
fp_5int *tx_set_se_on_dialog_close = (fp_5int* )0x005B4210;

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

fp_4int* base_prod_choice = (fp_4int*)0x004F81A0;

