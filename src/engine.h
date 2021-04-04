#pragma once

#include "main.h"

#define THINKER_HEADER (short)0xACAC

// =WTP=
// need to use it in other places
extern const char* ScriptTxtID;

void init_save_game(int faction);
HOOK_API int mod_faction_upkeep(int faction);
HOOK_API int mod_base_find3(int x, int y, int faction1, int region, int faction2, int faction3);

