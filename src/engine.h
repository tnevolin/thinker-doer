#pragma once

#include "main.h"

#define THINKER_HEADER (short)0xACAC

void init_save_game(int faction);
int __cdecl mod_faction_upkeep(int faction);
int __cdecl mod_base_find3(int x, int y, int faction1, int region, int faction2, int faction3);
void __cdecl mod_name_base(int faction, char* name, bool save_offset, bool water);
int __cdecl mod_best_defender(int defenderVehicleId, int attackerVehicleId, int bombardment);
int __cdecl battle_fight_parse_num(int index, int value);

