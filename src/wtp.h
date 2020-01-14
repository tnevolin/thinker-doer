#ifndef __PROTOTYPE_H__
#define __PROTOTYPE_H__

#include "main.h"
#include "game.h"

HOOK_API void battle_compute(int attacker_vehicle_id, int defender_vehicle_id, int attacker_strength_pointer, int defender_strength_pointer, int flags);

HOOK_API void battle_compute_compose_value_percentage(int output_string_pointer, int input_string_pointer);

HOOK_API int proto_cost(int chassisTypeId, int weaponTypeId, int armorTypeId, int abilities, int reactorTypeId);

HOOK_API int upgrade_cost(int faction_id, int new_unit_id, int old_unit_id);

HOOK_API int combat_roll
(
    int attacker_strength,
    int defender_strength,
    int attacker_vehicle_offset,
    int defender_vehicle_offset,
    int attacker_initial_power,
    int defender_initial_power,
    int *attacker_won_last_round
)
;

int standard_combat_mechanics_combat_roll
(
    int attacker_strength,
    int defender_strength
)
;

int alternative_combat_mechanics_combat_roll
(
    int attacker_strength,
    int defender_strength,
    int attacker_vehicle_offset,
    int defender_vehicle_offset,
    int attacker_initial_power,
    int defender_initial_power,
    int *attacker_won_last_round
)
;

void alternative_combat_mechanics_probabilities
(
    int attacker_strength,
    int defender_strength,
    double *p, double *q, double *pA, double *qA, double *pD, double *qD
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

double standard_combat_mechanics_calculate_attacker_winning_probability
(
    int attacker_strength,
    int defender_strength,
    int attacker_power,
    int defender_power
)
;

double binomial_koefficient(int n, int k);

double alternative_combat_mechanics_calculate_attacker_winning_probability
(
    int attacker_strength,
    int defender_strength,
    int attacker_power,
    int defender_power
)
;

double alternative_combat_mechanics_calculate_attacker_winning_probability_following_rounds
(
    bool attacker_won,
    double pA, double qA, double pD, double qD,
    int attacker_hp,
    int defender_hp
)
;

#endif // __PROTOTYPE_H__

