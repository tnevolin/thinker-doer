#include "terranx_wtp.h"

// global constants

const int NO_SYNC = 1;
const char* scriptTxtID = "SCRIPT";

// Path

CPath* PATH = (CPath*)0x945B00;
FPath_find PATH_find = (FPath_find)0x59A530;

// battle computation display variables

int *tx_battle_compute_attacker_effect_count = (int *)0x915614;
int *tx_battle_compute_defender_effect_count = (int *)0x915618;
char (*tx_battle_compute_attacker_effect_labels)[0x4][0x50] = (char (*)[0x4][0x50])0x90F554;
char (*tx_battle_compute_defender_effect_labels)[0x4][0x50] = (char (*)[0x4][0x50])0x90F694;
int (*tx_battle_compute_attacker_effect_values)[0x4] = (int (*)[0x4])0x9155F0;
int (*tx_battle_compute_defender_effect_values)[0x4] = (int (*)[0x4])0x915600;

// labels pointer

char ***tx_labels = (char ***)0x009B90F8;
const int LABEL_OFFSET_PLANET = 0x271;
const int LABEL_OFFSET_AIRTOAIR = 0x1C1;
const int LABEL_OFFSET_SENSOR = 0x265;
const int LABEL_OFFSET_TERRAIN = 0x14B;
const int LABEL_OFFSET_ROCKY = 0x152;
const int LABEL_OFFSET_FOREST = 0x123;
const int LABEL_OFFSET_FUNGUS = 0x5B;
const int LABEL_OFFSET_ROAD_ATTACK = 0x25E;
const int LABEL_OFFSET_BASE = 0x14C;
const int LABEL_OFFSET_PERIMETER = 0x162;
const int LABEL_OFFSET_TACHYON = 0x165;
const char *LABEL_NAVAL_YARD = "Naval Yard";
const char *LABEL_AEROSPACE_COMPLEX = "AeroComplex";

char *g_strTEMP = (char *)0x009B86A0;
char *SPACE = (char *)0x00682820;
char *PARENTHESES_LEFT = (char *)0x00682E9C;
char *PARENTHESES_RIGHT = (char *)0x00682E98;
int *current_base_growth_rate = (int *)0x0090E918;
int *g_PROBE_FACT_TARGET = (int *)0x00945B34;
int *g_UNK_ATTACK_FLAGS = (int *)0x0093E904;
byte *g_SOCIALWIN = (byte *)0x008A6270;
int *current_attacker = (int *)0x00689F28;
int *current_defender = (int *)0x00689F2C;

// yield rules

int *TERRA_OCEAN_SQ_NUTRIENT = (int *)0x00945F50;
int *TERRA_OCEAN_SQ_MINERALS = (int *)0x00945F54;
int *TERRA_OCEAN_SQ_ENERGY = (int *)0x00945F58;
int *TERRA_OCEAN_SQ_PSI = (int *)0x00945F5C;
int *TERRA_BASE_SQ_NUTRIENT = (int *)0x00945F60;
int *TERRA_BASE_SQ_MINERALS = (int *)0x00945F64;
int *TERRA_BASE_SQ_ENERGY = (int *)0x00945F68;
int *TERRA_BASE_SQ_PSI = (int *)0x00945F6C;
int *TERRA_BONUS_SQ_NUTRIENT = (int *)0x00945F70;
int *TERRA_BONUS_SQ_MINERALS = (int *)0x00945F74;
int *TERRA_BONUS_SQ_ENERGY = (int *)0x00945F78;
int *TERRA_BONUS_SQ_PSI = (int *)0x00945F7C;
int *TERRA_FOREST_SQ_NUTRIENT = (int *)0x00945F80;
int *TERRA_FOREST_SQ_MINERALS = (int *)0x00945F84;
int *TERRA_FOREST_SQ_ENERGY = (int *)0x00945F88;
int *TERRA_FOREST_SQ_PSI = (int *)0x00945F8C;
int *TERRA_RECYCLING_TANKS_NUTRIENT = (int *)0x00945F90;
int *TERRA_RECYCLING_TANKS_MINERALS = (int *)0x00945F94;
int *TERRA_RECYCLING_TANKS_ENERGY = (int *)0x00945F98;
int *TERRA_RECYCLING_TANKS_PSI = (int *)0x00945F9C;
int *TERRA_IMPROVED_LAND_NUTRIENT = (int *)0x00945FA0;
int *TERRA_IMPROVED_LAND_MINERALS = (int *)0x00945FA4;
int *TERRA_IMPROVED_SEA_NUTRIENT = (int *)0x00945FB0;
int *TERRA_IMPROVED_SEA_MINERALS = (int *)0x00945FB4;
int *TERRA_IMPROVED_SEA_ENERGY = (int *)0x00945FB8;
int *TERRA_IMPROVED_SEA_PSI = (int *)0x00945FBC;
int *TERRA_MONOLITH_NUTRIENT = (int *)0x00945FC0;
int *TERRA_MONOLITH_MINERALS = (int *)0x00945FC4;
int *TERRA_MONOLITH_ENERGY = (int *)0x00945FC8;
int *TERRA_MONOLITH_PSI = (int *)0x00945FCC;
int *TERRA_BOREHOLE_SQ_NUTRIENT = (int *)0x00945FD0;
int *TERRA_BOREHOLE_SQ_MINERALS = (int *)0x00945FD4;
int *TERRA_BOREHOLE_SQ_ENERGY = (int *)0x00945FD8;
int *TERRA_BOREHOLE_SQ_PSI = (int *)0x00945FDC;

// current base psych breakdown

int *CURRENT_BASE_DRONES_UNMODIFIED = (int *)0x0090E920;
int *CURRENT_BASE_DRONES_PSYCH = (int *)0x0090E924;
int *CURRENT_BASE_DRONES_FACILITIES = (int *)0x0090E928;
int *CURRENT_BASE_DRONES_POLICE = (int *)0x0090E92C;
int *CURRENT_BASE_DRONES_PROJECTS = (int *)0x0090E930;
int *CURRENT_BASE_SDRONES_UNMODIFIED = (int *)0x0090E94C;
int *CURRENT_BASE_SDRONES_PSYCH = (int *)0x0090E950;
int *CURRENT_BASE_SDRONES_FACILITIES = (int *)0x0090E954;
int *CURRENT_BASE_SDRONES_POLICE = (int *)0x0090E958;
int *CURRENT_BASE_SDRONES_PROJECTS = (int *)0x0090E95C;
int *CURRENT_BASE_TALENTS_UNMODIFIED = (int *)0x0090E984;
int *CURRENT_BASE_TALENTS_PSYCH = (int *)0x0090E988;
int *CURRENT_BASE_TALENTS_FACILITIES = (int *)0x0090E98C;
int *CURRENT_BASE_TALENTS_POLICE = (int *)0x0090E990;
int *CURRENT_BASE_TALENTS_PROJECTS = (int *)0x0090E994;

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

// propose base production
fp_4int *base_prod_choice = (fp_4int*)0x004F81A0;

// select technology to research
fp_4int *tech_pick = (fp_4int *)0x005BEB70;

// put territory borders on a map
fp_0void *reset_territory = (fp_0void *)0x00523DD0;

// breaks treaty
fp_3int *break_treaty = (fp_3int *)0x0055F770;

// base production change
fp_2int *base_making = (fp_2int *)0x004E4700;

// mind control cost
fp_3int *mind_control = (fp_3int *)0x0059EA80;

// put vehicle on target tile
fp_3int *veh_put = (fp_3int *)0x005A59B0;

// base conventional morale modifier
fp_3int *morale_mod = (fp_3int *)0x004E6400;

// updates base production
fp_2void *base_prod_change = (fp_2void *)0x004E5A60;

// base hurr routine
fp_0void *base_hurry = (fp_0void *)0x004F3FE0;

// checks if base is on sea region
fp_2int *base_on_sea = (fp_2int *)0x0050DE50;

// finds nearest base for given faction
fp_3int *base_find2 = (fp_3int *)0x004E3C60;

// enemy unit movement entry point
fp_1void *enemy_units_check = (fp_1void *)0x00579510;

// vehicle owner occupying tile
fp_2int *veh_who = (fp_2int *)0x00500250;

// vehicle withdrawal after breaking pact
fp_2int *pact_withdraw = (fp_2int *)0x0053C370;

// finds nearest base to location
fp_2int *base_find = (fp_2int *)0x004E3B80;

// checks territory
fp_5int *whose_territory = (fp_5int *)0x004E3EF0;

// unit speed
fp_1int *unit_speed = (fp_1int *)0x005C13B0;

// base upkeep
fp_1int *base_upkeep = (fp_1int *)0x004F79C0;

// base ecology
fp_0int *base_ecology = (fp_0int *)0x004F67F0;

// base minerals
fp_0void *base_minerals = (fp_0void *)0x004E9CB0;

// base yield
fp_0void *base_yield = (fp_0void *)0x004E80B0;

// draw a fireball
fp_3void *boom = (fp_3void *)0x00504AA0;

