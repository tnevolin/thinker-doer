#pragma once

#include "main.h"
#include "game.h"

#define BASE_DISALLOWED (TERRA_BASE_IN_TILE | TERRA_MONOLITH | TERRA_FUNGUS | TERRA_THERMAL_BORE)

#define IMP_SIMPLE (TERRA_FARM | TERRA_MINE | TERRA_FOREST)
#define IMP_ADVANCED (TERRA_CONDENSER | TERRA_THERMAL_BORE)

#define PM_SAFE -20
#define PM_NEAR_SAFE -40

typedef int PMTable[MAPSZ*2][MAPSZ];
/*
    Priority Management Tables contain values calculated for each map square
    to guide the AI in its move planning.

    The tables contain only ephemeral values: they are recalculated each turn
    for every active faction, and the values are never saved with the save games.

    For example, pm_former counter is decremented whenever a former starts
    work on a given square to prevent too many of them converging on the same spot.
    It is also used as a shortcut to determine if a square has a friendly base
    in a 2-tile radius, because formers are only allowed to work on squares which
    are reachable by a friendly base worker.

    Non-combat units controlled by Thinker use pm_safety table to decide if
    there are enemy units too close to them. PM_SAFE defines the threshold below
    which the units will attempt to flee to the square chosen by escape_move.
*/

void adjust_value(PMTable tbl, int x, int y, int range, int value);
void adjust_value_within_region(PMTable tbl, int x, int y, int range, int value, bool stayInRegion);
HOOK_API int enemy_move(int id);
HOOK_API int log_veh_kill(int a, int b, int c, int d);
void move_upkeep(int faction);
bool need_formers(int x, int y, int faction);
bool has_transport(int x, int y, int faction);
bool non_combat_move(int x, int y, int faction, int triad);
bool can_build_base(int x, int y, int faction, int triad);
bool has_base_sites(int x, int y, int faction, int triad);
int select_item(int x, int y, int faction, MAP* sq);
int crawler_move(int id);
int colony_move(int id);
int former_move(int id);
int trans_move(int id);
int combat_move(int id);
bool defend_tile(VEH* veh, MAP* sq);
int select_item(int x, int y, int fac, MAP* sq);
int escape_move(int id);
bool other_in_tile(int fac, MAP* sq);
int former_tile_score(int x1, int y1, int x2, int y2, int fac, MAP* sq);
bool isSafe(int x, int y);
double base_tile_score(int factionId, int x1, int y1, int range, int triad);

