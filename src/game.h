#pragma once

#include "main.h"

// Always enabled settings for random maps started from the main menu
// Select only those settings that are not set in Special Scenario Rules
const uint32_t GAME_RULES_MASK = 0x7808FFFF;
const uint32_t GAME_MRULES_MASK = 0xFFFFFFF0;

bool un_charter();
bool global_trade_pact();
bool victory_done();
bool voice_of_planet();
bool valid_player(int faction_id);
bool valid_triad(int triad);
char* label_get(size_t index);
char* __cdecl parse_set(int faction_id);
int __cdecl parse_num(size_t index, int value);
int __cdecl parse_says(size_t index, const char* src, int gender, int plural);
int __cdecl game_start_turn();
int __cdecl game_year(int n);
int __cdecl in_box(int x, int y, RECT* rc);
void __cdecl bitmask(uint32_t input, uint32_t* offset, uint32_t* mask);

void show_rules_menu();
void init_world_config();
void init_save_game(int faction_id);
void __cdecl mod_load_map_daemon(int a1);
void __cdecl mod_load_daemon(int a1, int a2);
void __cdecl mod_auto_save();
int __cdecl mod_replay_base(int event, int x, int y, int faction_id);
int __cdecl mod_turn_upkeep();
int __cdecl mod_faction_upkeep(int faction_id);
void __cdecl mod_repair_phase(int faction_id);
void __cdecl mod_production_phase(int faction_id);
void __cdecl mod_name_base(int faction_id, char* name, bool save_offset, bool water);
int __cdecl load_music_strcmpi(const char* active, const char* label);

template<typename T, typename... Args>
int net_show(T format, Args... vals) {
    char buf[StrBufLen];
    snprintf(buf, StrBufLen, format, vals...);
    parse_says(0, buf, -1, -1);
    return NetMsg_pop(NetMsg, "GENERIC", 5000, 0, 0);
}

