#pragma once

#include "main.h"
#include "lib/FastNoiseLite.h"

const int StartBonusCount = 2;
const int StartBonusTiles = 25;

MAP* mapsq(int x, int y);
int wrap(int x);
int map_range(int x1, int y1, int x2, int y2);
int vector_dist(int x1, int y1, int x2, int y2);
int min_range(const Points& S, int x, int y);
int min_vector(const Points& S, int x, int y);
double avg_range(const Points& S, int x, int y);
bool is_ocean(MAP* sq);
bool is_ocean(BASE* base);
bool is_ocean_shelf(MAP* sq);
bool is_shore_level(MAP* sq);
bool map_is_flat();

bool __cdecl bad_reg(int region);
int __cdecl region_at(int x, int y);
int __cdecl base_at(int x, int y);
int __cdecl x_dist(int x1, int x2);
uint32_t __cdecl code_at(int x, int y);
void __cdecl code_set(int x, int y, int code);
bool __cdecl near_landmark(int x, int y);
void __cdecl mod_map_wipe();
int __cdecl mod_hex_cost(int unit_id, int faction_id, int x1, int y1, int x2, int y2, int toggle);
int __cdecl mod_bonus_at(int x, int y);
int __cdecl mod_goody_at(int x, int y);
int __cdecl mod_base_find3(int x, int y, int faction1, int region, int faction2, int faction3);
int __cdecl mod_whose_territory(int faction_id, int x, int y, int* base_id, int ignore_comm);
int total_yield(int x, int y, int faction);
int fungus_yield(int faction, ResType res_type);
int item_yield(int x, int y, int faction, int bonus, MapItem item);
void world_generate(uint32_t seed);
void console_world_generate(uint32_t seed);
void __cdecl find_start(int faction, int* tx, int* ty);
void __cdecl mod_world_build();
void __cdecl mod_time_warp();

