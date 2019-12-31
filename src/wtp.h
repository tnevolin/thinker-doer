#ifndef __PROTOTYPE_H__
#define __PROTOTYPE_H__

#include "main.h"
#include "game.h"

HOOK_API int proto_cost(int chassisTypeId, int weaponTypeId, int armorTypeId, int abilities, int reactorTypeId);

HOOK_API int upgrade_cost(int faction_id, int new_unit_id, int old_unit_id);

HOOK_API int combat_roll
(
    int attacker_vehicle_offset,
    int defender_vehicle_offset,
    int attacker_initial_health,
    int defender_initial_health,
    int attacker_strength,
    int defender_strength,
    int attacker_firepower,
    int defender_firepower
)
;

HOOK_API void calculate_odds
(
    int attacker_vehicle_offset,
    int defender_vehicle_offset,
    int attacker_weapon_strength,
    int defender_armor_strength,
    int *attacker_odd,
    int *defender_odd
)
;

double binomial_koefficient(int n, int k);

#endif // __PROTOTYPE_H__

