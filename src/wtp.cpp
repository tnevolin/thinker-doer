#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include "wtp.h"
#include "main.h"
#include "patch.h"
#include "engine.h"

/**
Combat calculation placeholder.
All custom combat calculation goes here.
*/
HOOK_API int read_basic_rules()
{
    // run original code

    int value = tx_read_basic_rules();

    // update rules

    if (value == 0)
    {
        if (conf.tube_movement_rate_multiplier > 0)
        {
            // set road movement cost
            conf.road_movement_cost = conf.tube_movement_rate_multiplier;
            // set tube movement rate
            tx_basic->mov_rate_along_roads *= conf.tube_movement_rate_multiplier;

        }

    }

    return value;

}

/**
Combat calculation placeholder.
All custom combat calculation goes here.
*/
HOOK_API void battle_compute(int attacker_vehicle_id, int defender_vehicle_id, int attacker_strength_pointer, int defender_strength_pointer, int flags)
{
    debug
    (
        "battle_compute(attacker_vehicle_id=%d, defender_vehicle_id=%d, attacker_strength_pointer=%d, defender_strength_pointer=%d, flags=%d)\n",
        attacker_vehicle_id,
        defender_vehicle_id,
        attacker_strength_pointer,
        defender_strength_pointer,
        flags
    )
    ;

    // run original function

    tx_battle_compute(attacker_vehicle_id, defender_vehicle_id, attacker_strength_pointer, defender_strength_pointer, flags);

    debug
    (
        "attacker_strength=%d, defender_strength=%d\n",
        *(int *)attacker_strength_pointer,
        *(int *)defender_strength_pointer
    )
    ;

    // apply_planet_combat_bonus_on_defense

    if (conf.apply_planet_combat_bonus_on_defense)
    {
        // get attacker/defender vehicles

        VEH attacker_vehicle = tx_vehicles[attacker_vehicle_id];
        VEH defender_vehicle = tx_vehicles[defender_vehicle_id];

        // get attacker/defender units

        UNIT attacker_unit = tx_units[attacker_vehicle.proto_id];
        UNIT defender_unit = tx_units[defender_vehicle.proto_id];

        // get attacker/defender weapon/armor id

        R_Weapon attacker_weapon = tx_weapon[attacker_unit.weapon_type];
        R_Defense defender_armor = tx_defense[defender_unit.armor_type];

        // get attacker/defender weapon/armor value

        int attacker_weapon_value = attacker_weapon.offense_value;
        int defender_armor_value = defender_armor.defense_value;

        // check if it is a psi combat

        if (attacker_weapon_value < 0 || defender_armor_value < 0)
        {
            // get defender faction id

            int defender_faction_id = tx_vehicles[defender_vehicle_id].faction_id;

            if (defender_faction_id != 0)
            {
                // get defender planet rating

                int defender_planet_rating = tx_factions[defender_faction_id].SE_planet;

                if (defender_planet_rating != 0)
                {
                    // calculate defender psi combat bonus

                    int defender_psi_combat_bonus = tx_basic->combat_psi_bonus_per_PLANET * defender_planet_rating;

                    // calculate "Planet" label pointer

                    char *label_planet = *(*tx_labels + LABEL_OFFSET_PLANET);

                    // add effect description

                    if (*tx_battle_compute_defender_effect_count < 4)
                    {
                        strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], label_planet);
                        (*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = defender_psi_combat_bonus;

                        (*tx_battle_compute_defender_effect_count)++;

                    }

                    // modify defender strength

                    *(int *)defender_strength_pointer = (int)round((double)(*(int *)defender_strength_pointer) * (100.0 + (double)defender_psi_combat_bonus) / 100.0);

                }

            }

        }

    }

    // territory_combat_bonus

    if (conf.territory_combat_bonus != 0)
    {
        // get attacker/defender vehicle

        VEH *attacker_vehicle = &tx_vehicles[attacker_vehicle_id];
        VEH *defender_vehicle = &tx_vehicles[defender_vehicle_id];

        // get attacker/defender owner

        int attacker_faction_id = attacker_vehicle->faction_id;
        int defender_faction_id = defender_vehicle->faction_id;

        // apply territory defense bonus for non alien combat only

        if (attacker_faction_id != 0 && defender_faction_id != 0)
        {
            // get defender location

            MAP *defender_location = mapsq(defender_vehicle->x, defender_vehicle->y);

            // get defender territory owner

            int defender_location_owner = (int)defender_location->owner;

            // verify defender is in attacker's territory

            if (defender_location_owner == attacker_faction_id)
            {
                // territory attack bonus

                int attacker_territory_bonus = conf.territory_combat_bonus;

                // "Territory:" label

                const char label_territory[] = "Territory:";

                // add effect description

                if (*tx_battle_compute_attacker_effect_count < 4)
                {
                    strcpy((*tx_battle_compute_attacker_effect_labels)[*tx_battle_compute_attacker_effect_count], label_territory);
                    (*tx_battle_compute_attacker_effect_values)[*tx_battle_compute_attacker_effect_count] = attacker_territory_bonus;

                    (*tx_battle_compute_attacker_effect_count)++;

                }

                // modify attacker strength

                *(int *)attacker_strength_pointer = (int)round((double)(*(int *)attacker_strength_pointer) * (100.0 + (double)attacker_territory_bonus) / 100.0);

            }

            // verify defender is in defender's territory

            if (defender_location_owner == defender_faction_id)
            {
                // territory defense bonus

                int defender_territory_bonus = conf.territory_combat_bonus;

                // "Territory:" label

                const char label_territory[] = "Territory:";

                // add effect description

                if (*tx_battle_compute_defender_effect_count < 4)
                {
                    strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], label_territory);
                    (*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = defender_territory_bonus;

                    (*tx_battle_compute_defender_effect_count)++;

                }

                // modify defender strength

                *(int *)defender_strength_pointer = (int)round((double)(*(int *)defender_strength_pointer) * (100.0 + (double)defender_territory_bonus) / 100.0);

            }

        }

    }

    // adjust summary lines to the bottom

    while (*tx_battle_compute_attacker_effect_count < 4)
    {
        (*tx_battle_compute_attacker_effect_labels)[*tx_battle_compute_attacker_effect_count][0] = '\x0';
        (*tx_battle_compute_attacker_effect_values)[*tx_battle_compute_attacker_effect_count] = 0;

        (*tx_battle_compute_attacker_effect_count)++;

    }

    while (*tx_battle_compute_defender_effect_count < 4)
    {
        (*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count][0] = '\x0';
        (*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = 0;

        (*tx_battle_compute_defender_effect_count)++;

    }

}

/**
Composes combat effect value percentage.
Overwrites zero with empty string.
*/
HOOK_API void battle_compute_compose_value_percentage(int output_string_pointer, int input_string_pointer)
{
    debug
    (
        "battle_compute_compose_value_percentage:input(output_string=%s, input_string=%s)\n",
        (char *)output_string_pointer,
        (char *)input_string_pointer
    )
    ;

    // call original function

    tx_strcat(output_string_pointer, input_string_pointer);

    debug
    (
        "battle_compute_compose_value_percentage:output(output_string=%s, input_string=%s)\n",
        (char *)output_string_pointer,
        (char *)input_string_pointer
    )
    ;

    // replace zero with empty string

    if (strcmp((char *)output_string_pointer, " +0%") == 0)
    {
        ((char *)output_string_pointer)[0] = '\x0';

    }

    debug
    (
        "battle_compute_compose_value_percentage:corrected(output_string=%s, input_string=%s)\n",
        (char *)output_string_pointer,
        (char *)input_string_pointer
    )
    ;

}

/**
Prototype cost calculation.

1. Calculate module and weapon/armor reactor modified costs.
module reactor modified cost = item cost * (Fission reactor value / 100)
weapon/armor reactor modified cost = item cost * (reactor value / 100)

2. Select primary and secondary item.
primary item = the one with higher reactor modified cost
secondary item = the one with lower reactor modified cost

3. Calculate primary item cost and secondary item shifted cost
primary item cost = item cost * (corresponding reactor value / 100)
secondary item shifted cost = (item cost - 1) * (corresponding reactor value / 100)

4. Calculate unit base cost.
unit base cost = [primary item cost + secondary item shifted cost / 2] * chassis cost / 2

5. Round.

6. Multiply by ability cost factor.
ability bytes 0-3 is cost factor

7. Add ability flat cost
ability bytes 4-7 is flat cost

8. Round.

*/
HOOK_API int proto_cost(int chassis_id, int weapon_id, int armor_id, int abilities, int reactor_level)
{
    // calculate reactor cost factors

    double fission_reactor_cost_factor = (double)conf.reactor_cost_factors[0] / 100.0;
    double reactor_cost_factor = (double)conf.reactor_cost_factors[reactor_level - 1] / 100.0;

    // determine whether we have module or weapon

    bool module = (tx_weapon[weapon_id].mode >= WMODE_TRANSPORT);

    // get component costs

    double weapon_cost = (double)tx_weapon[weapon_id].cost;
    double armor_cost = (double)tx_defense[armor_id].cost;
    double chassis_cost = (double)tx_chassis[chassis_id].cost;

    // calculate items reactor modified costs

    double weapon_reactor_modified_cost = weapon_cost * (module ? fission_reactor_cost_factor : reactor_cost_factor);
    double armor_reactor_modified_cost = weapon_cost * reactor_cost_factor;

    // select primary item and secondary item shifted costs

    double primary_item_cost;
    double secondary_item_shifted_cost;

    if (weapon_reactor_modified_cost >= armor_reactor_modified_cost)
	{
		primary_item_cost = weapon_reactor_modified_cost;
		secondary_item_shifted_cost = (armor_cost - 1) * reactor_cost_factor;
	}
	else
	{
		primary_item_cost = armor_reactor_modified_cost;
		secondary_item_shifted_cost = (weapon_cost - 1) * (module ? fission_reactor_cost_factor : reactor_cost_factor);
	}

    // set minimal cost to reactor level (this is checked in some other places so we should do this here to avoid any conflicts)

    int minimal_cost = reactor_level;

    // calculate base unit cost without abilities

    int base_cost = (int)round((primary_item_cost + secondary_item_shifted_cost / 2) * chassis_cost / 2);

    // get abilities cost modifications

    double abilities_cost_factor = 0;
    int abilities_cost_addition = 0;
    for (int ability_id = 0; ability_id < 32; ability_id++)
    {
        if (((abilities >> ability_id) & 0x1) == 0x1)
        {
            int ability_cost = tx_ability[ability_id].cost;

            // take ability cost factor bits: 0-3

            int ability_cost_factor = (ability_cost & 0xF);

            if (ability_cost_factor > 0)
            {
                abilities_cost_factor += (double)ability_cost_factor;

            }

            // take ability cost addition bits: 4-7

            int ability_cost_addition = ((ability_cost >> 4) & 0xF);

            if (ability_cost_addition > 0)
            {
                abilities_cost_addition += ability_cost_addition;

            }

        }

    }

    // calculate final cost

    int cost =
        max
        (
            // minimal cost
            minimal_cost,
            (int)
            round
            (
				// base cost
				base_cost
                *
                // abilities cost factor
                (1.0 + 0.25 * abilities_cost_factor)
            )
            +
            // abilities flat cost
            abilities_cost_addition
        )
    ;

    return cost;

}

HOOK_API int upgrade_cost(int faction_id, int new_unit_id, int old_unit_id)
{
    // get old unit cost

    int old_unit_cost = tx_units[old_unit_id].cost;

    // get new unit cost

    int new_unit_cost = tx_units[new_unit_id].cost;

    // double new unit cost if not prototyped

    if ((tx_units[new_unit_id].unit_flags & 0x4) == 0)
    {
        new_unit_cost *= 2;
    }

    // calculate cost difference

    int cost_difference = new_unit_cost - old_unit_cost;

    // calculate base upgrade cost
    // x4 if positive
    // x2 if negative

    int upgrade_cost = cost_difference * (cost_difference >= 0 ? 4 : 2);

    // halve upgrade cost if positive and faction possesses The Nano Factory

    if (upgrade_cost > 0)
    {
        // get The Nano Factory base id

        int nano_factory_base_id = tx_secret_projects[0x16];

        // get The Nano Factory faction id

        if (nano_factory_base_id >= 0)
        {
            if (tx_bases[nano_factory_base_id].faction_id == faction_id)
            {
                upgrade_cost /= 2;

            }

        }

    }

    return upgrade_cost;

}

/**
Multipurpose combat roll function.
Determines which side wins this round. Returns 1 for attacker, 0 for defender.
*/
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
{
    debug("combat_roll(attacker_strength=%d, defender_strength=%d, attacker_vehicle_offset=%d, defender_vehicle_offset=%d, attacker_initial_power=%d, defender_initial_power=%d, *attacker_won_last_round=%d)\n",
        attacker_strength,
        defender_strength,
        attacker_vehicle_offset,
        defender_vehicle_offset,
        attacker_initial_power,
        defender_initial_power,
        *attacker_won_last_round
    )
    ;

    // normalize strength values

    if (attacker_strength < 1) attacker_strength = 1;
    if (defender_strength < 1) defender_strength = 1;

    // calculate outcome

    int attacker_wins;

    if (!conf.alternative_combat_mechanics)
    {
        attacker_wins =
            standard_combat_mechanics_combat_roll
            (
                attacker_strength,
                defender_strength
            )
        ;

    }
    else
    {
        attacker_wins =
            alternative_combat_mechanics_combat_roll
            (
                attacker_strength,
                defender_strength,
                attacker_vehicle_offset,
                defender_vehicle_offset,
                attacker_initial_power,
                defender_initial_power,
                attacker_won_last_round
            )
        ;

    }

    // return outcome

    return attacker_wins;

}

/**
Standard combat mechanics.
Determines which side wins this round. Returns 1 for attacker, 0 for defender.
*/
int standard_combat_mechanics_combat_roll
(
    int attacker_strength,
    int defender_strength
)
{
    debug("standard_combat_mechanics_combat_roll(attacker_strength=%d, defender_strength=%d)\n",
        attacker_strength,
        defender_strength
    )
    ;

    // generate random roll

    int random_roll = random(attacker_strength + defender_strength);
    debug("\trandom_roll=%d\n", random_roll);

    // calculate outcome

    int attacker_wins = (random_roll < attacker_strength ? 1 : 0);
    debug("\tattacker_wins=%d\n", attacker_wins);

    return attacker_wins;

}

/**
Alternative combat mechanics.
Determines which side wins this round. Returns 1 for attacker, 0 for defender.
*/
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
{
    debug("alternative_combat_mechanics_combat_roll(attacker_strength=%d, defender_strength=%d, attacker_initial_power=%d, defender_initial_power=%d, *attacker_won_last_round=%d)\n",
        attacker_strength,
        defender_strength,
        attacker_initial_power,
        defender_initial_power,
        *attacker_won_last_round
    )
    ;

    // calculate probabilities

    double p, q, pA, qA, pD, qD;

    alternative_combat_mechanics_probabilities
    (
        attacker_strength,
        defender_strength,
        &p, &q, &pA, &qA, &pD, &qD
    )
    ;

    // determine whether we are on first round

    VEH attacker_vehicle = tx_vehicles[attacker_vehicle_offset / 0x34];
    VEH defender_vehicle = tx_vehicles[defender_vehicle_offset / 0x34];

    UNIT attacker_unit = tx_units[attacker_vehicle.proto_id];
    UNIT defender_unit = tx_units[defender_vehicle.proto_id];

    int attacker_power = attacker_unit.reactor_type * 0xA - attacker_vehicle.damage_taken;
    int defender_power = defender_unit.reactor_type * 0xA - defender_vehicle.damage_taken;

    bool first_round = (attacker_power == attacker_initial_power) && (defender_power == defender_initial_power);
    debug("\tfirst_round=%d\n", (first_round ? 1 : 0));

    // determine effective p

    double effective_p = (first_round ? p : (*attacker_won_last_round == 1 ? pA : pD));
    debug("\teffective_p=%f\n", effective_p);

    // generate random roll

    double random_roll = (double)rand() / (double)RAND_MAX;
    debug("\trandom_roll=%f\n", random_roll);

    // calculate outcome

    int attacker_wins = (random_roll < effective_p ? 1 : 0);
    debug("\tattacker_wins=%d\n", attacker_wins);

    // set round winner

    *attacker_won_last_round = attacker_wins;

    return attacker_wins;

}

/**
Alternative combat mechanics.
Calculates probabilities.
*/
void alternative_combat_mechanics_probabilities
(
    int attacker_strength,
    int defender_strength,
    double *p, double *q, double *pA, double *qA, double *pD, double *qD
)
{
    // determine first round probability

    *p = (double)attacker_strength / ((double)attacker_strength + (double)defender_strength);
    *q = 1 - *p;

    // determine following rounds probabilities

    *qA = *q / conf.alternative_combat_mechanics_loss_divider;
    *pA = 1 - *qA;
    *pD = *p / conf.alternative_combat_mechanics_loss_divider;
    *qD = 1 - *pD;

}

/**
Calculates odds.
*/
HOOK_API void calculate_odds
(
    int attacker_vehicle_offset,
    int defender_vehicle_offset,
    int attacker_strength,
    int defender_strength,
    int *attacker_odd,
    int *defender_odd
)
{
    debug("calculate_odds(attacker_vehicle_offset=%d, defender_vehicle_offset=%d, attacker_strength=%d, defender_strength=%d)\n",
        attacker_vehicle_offset,
        defender_vehicle_offset,
        attacker_strength,
        defender_strength
    )
    ;

    // get attacker and defender vehicles

    VEH attacker_vehicle = tx_vehicles[attacker_vehicle_offset / sizeof(VEH)];
    VEH defender_vehicle = tx_vehicles[defender_vehicle_offset / sizeof(VEH)];

    // get attacker and defender units

    UNIT attacker_unit = tx_units[attacker_vehicle.proto_id];
    UNIT defender_unit = tx_units[defender_vehicle.proto_id];

    // calculate attacker and defender power
    // artifact gets 1 HP regardless of reactor

    int attacker_power = (attacker_unit.unit_plan == PLAN_ALIEN_ARTIFACT ? 1 : attacker_unit.reactor_type * 10 - attacker_vehicle.damage_taken);
    int defender_power = (defender_unit.unit_plan == PLAN_ALIEN_ARTIFACT ? 1 : defender_unit.reactor_type * 10 - defender_vehicle.damage_taken);

    // determine if we are ignoring reactor power

    bool ignore_reactor_power =
        conf.ignore_reactor_power_in_combat
        ||
        tx_weapon[attacker_unit.weapon_type].offense_value < 0
        ||
        tx_defense[defender_unit.armor_type].defense_value < 0
    ;

    // calculate firepower

    int attacker_fp =
        (ignore_reactor_power ? defender_unit.reactor_type : 1)
    ;
    int defender_fp =
        (ignore_reactor_power ? attacker_unit.reactor_type : 1)
    ;

    // calculate HP

    int attacker_hp = (int)ceil((double)attacker_power / defender_fp);
    int defender_hp = (int)ceil((double)defender_power / attacker_fp);

    // calculate attacker winning probability

    double attacker_winning_probability;

    if (defender_hp <= 0)
    {
        attacker_winning_probability = 1.0;

    }
    else if (attacker_hp <= 0)
    {
        attacker_winning_probability = 0.0;

    }
    else
    {
        if (!conf.alternative_combat_mechanics)
        {
            attacker_winning_probability =
                standard_combat_mechanics_calculate_attacker_winning_probability
                (
                    attacker_strength,
                    defender_strength,
                    attacker_hp,
                    defender_hp
                )
            ;

        }
        else
        {
            attacker_winning_probability =
                alternative_combat_mechanics_calculate_attacker_winning_probability
                (
                    attacker_strength,
                    defender_strength,
                    attacker_hp,
                    defender_hp
                )
            ;

        }

    }

    /*
    // calculate odds

    double odds = 1.0 / (1.0 / attacker_winning_probability - 1.0);

    bool attacker_advantage = (odds >= 1.0);
    double normalized_odds = (attacker_advantage ? odds : 1.0 / odds);

    // find good odds ratio

    int best_numerator = 0;
    int best_denominator = 0;
    double best_difference = -1.0;

    for (int denominator = 0x1; denominator <= 0xA; denominator++)
    {
        int numerator = (int)round(normalized_odds * denominator);
        double odds_ratio = (double)numerator / (double)denominator;
        double difference = abs(odds_ratio - normalized_odds);

        if (best_difference == -1 || difference < best_difference)
        {
            best_difference = difference;
            best_numerator = numerator;
            best_denominator = denominator;

        }

    }

    // set odds

    if (attacker_advantage)
    {
        *attacker_odd = best_numerator;
        *defender_odd = best_denominator;

    }
    else
    {
        *attacker_odd = best_denominator;
        *defender_odd = best_numerator;

    }
    */

    // set attacker odd to attacker winning probability percentage
    // set defender odd to 1 to disable fraction simplification

    *attacker_odd = (int)round(attacker_winning_probability * 100.0);
    *defender_odd = 1;

}

/**
Standard combat mechanics.
Calculates attacker winning probability.
*/
double standard_combat_mechanics_calculate_attacker_winning_probability
(
    int attacker_strength,
    int defender_strength,
    int attacker_hp,
    int defender_hp
)
{
    debug("standard_combat_mechanics_calculate_attacker_winning_probability(attacker_strength=%d, defender_strength=%d, attacker_hp=%d, defender_hp=%d)\n",
        attacker_strength,
        defender_strength,
        attacker_hp,
        defender_hp
    )
    ;

    // determine round probability

    double p = (double)attacker_strength / ((double)attacker_strength + (double)defender_strength);
    double q = 1 - p;

    // calculate attacker winning probability

    double attacker_winning_probability = 0.0;
    double p1 = pow(p, defender_hp);

    for (int alp = 0; alp < attacker_hp; alp++)
    {
        double q1 = pow(q, alp);

        double c = binomial_koefficient(defender_hp - 1 + alp, alp);

        attacker_winning_probability += p1 * q1 * c;

    }

    debug("p(round)=%f, p(battle)=%f\n", p, attacker_winning_probability);

    return attacker_winning_probability;

}

double binomial_koefficient(int n, int k)
{
    double c = 1.0;

    for (int i = 0; i < k; i++)
    {
        c *= (double)(n - i) / (double)(k - i);

    }

    return c;

}

/**
Alternative combat mechanics.
Calculates attacker winning probability.
*/
double alternative_combat_mechanics_calculate_attacker_winning_probability
(
    int attacker_strength,
    int defender_strength,
    int attacker_hp,
    int defender_hp
)
{
    debug("alternative_combat_mechanics_calculate_attacker_winning_probability(attacker_strength=%d, defender_strength=%d, attacker_hp=%d, defender_hp=%d)\n",
        attacker_strength,
        defender_strength,
        attacker_hp,
        defender_hp
    )
    ;

    double attacker_winning_probability;

    // cannot calculate too long tree for HP > 10
    // return 0.5 instead
    if (attacker_hp > 10 || defender_hp > 10)
    {
        attacker_winning_probability = 0.5;

    }
    else if (attacker_hp <= 0 && defender_hp <= 0)
    {
        attacker_winning_probability = 0.5;

    }
    else if (defender_hp <= 0)
    {
        attacker_winning_probability = 1.0;

    }
    else if (attacker_hp <= 0)
    {
        attacker_winning_probability = 0.0;

    }
    else
    {
        // determine first round probability

        double p = (double)attacker_strength / ((double)attacker_strength + (double)defender_strength);
        double q = 1 - p;

        // determine following rounds probabilities

        double qA = q / conf.alternative_combat_mechanics_loss_divider;
        double pA = 1 - qA;
        double pD = p / conf.alternative_combat_mechanics_loss_divider;
        double qD = 1 - pD;

        debug("p=%f, q=%f, pA=%f, qA=%f, pD=%f, qD=%f\n", p, q, pA, qA, pD, qD);

        // calculate attacker winning probability

        attacker_winning_probability =
            // attacker wins first round
            p
            *
            alternative_combat_mechanics_calculate_attacker_winning_probability_following_rounds
            (
                true,
                pA, qA, pD, qD,
                attacker_hp,
                defender_hp - 1
            )
            +
            // defender wins first round
            q
            *
            alternative_combat_mechanics_calculate_attacker_winning_probability_following_rounds
            (
                false,
                pA, qA, pD, qD,
                attacker_hp - 1,
                defender_hp
            )
        ;

    }

    debug("attacker_winning_probability=%f\n", attacker_winning_probability);

    return attacker_winning_probability;

}

double alternative_combat_mechanics_calculate_attacker_winning_probability_following_rounds
(
    bool attacker_won,
    double pA, double qA, double pD, double qD,
    int attacker_hp,
    int defender_hp
)
{
    double attacker_winning_probability;

    if (attacker_hp <= 0 && defender_hp <= 0)
    {
        attacker_winning_probability = 0.5;

    }
    else if (defender_hp <= 0)
    {
        attacker_winning_probability = 1.0;

    }
    else if (attacker_hp <= 0)
    {
        attacker_winning_probability = 0.0;

    }
    else
    {
        attacker_winning_probability =
            // attacker wins round
            (attacker_won ? pA : pD)
            *
            alternative_combat_mechanics_calculate_attacker_winning_probability_following_rounds
            (
                true,
                pA, qA, pD, qD,
                attacker_hp,
                defender_hp - 1
            )
            +
            // defender wins round
            (attacker_won ? qA : qD)
            *
            alternative_combat_mechanics_calculate_attacker_winning_probability_following_rounds
            (
                false,
                pA, qA, pD, qD,
                attacker_hp - 1,
                defender_hp
            )
        ;

    }

    return attacker_winning_probability;

}

/**
For sea squares pretend they are on the same continent as closest reachable coastal base
if such base is also closer than any other sea base in the same sea.
*/
HOOK_API int calculate_distance_to_nearest_base(int x, int y, int unknown_1, int region, int unknown_2, int unknown_3)
{
    debug
    (
        "calculate_distance_to_nearest_base(x=%d, y=%d, region=%d)\n",
        x,
        y,
        region
    )
    ;

    bool closest_reachable_base_found = false;
    bool closest_reachable_base_is_coastal = false;
    int closest_reachable_base_distance = 9999;
    BASE *closest_reachable_base = NULL;
    MAP *closest_reachable_base_square = NULL;

    int saved_coastal_base_region = -1;

    // check if given square is at sea

    if (region >= 0x41)
    {
        // loop through bases

        for (int base_id = 0; base_id < *total_num_bases; base_id++)
        {
            // get base

            BASE *base = &tx_bases[base_id];

            // get base map square

            MAP *base_map_square = mapsq(base->x, base->y);

            // get base map square body id

            int base_map_square_region = base_map_square->region;

            // check if it is the same body
            if (base_map_square_region == region)
            {
                // calculate base distance

                int base_distance = map_distance(x, y, base->x, base->y);

                // improve distance if smaller

                if (!closest_reachable_base_found || base_distance < closest_reachable_base_distance)
                {
                    closest_reachable_base_found = true;
                    closest_reachable_base_is_coastal = false;
                    closest_reachable_base_distance = base_distance;
                    closest_reachable_base = base;
                    closest_reachable_base_square = base_map_square;

                }

            }
            // otherwise, check it it is a land base
            else if (base_map_square_region < 0x41)
            {
                // iterate over nearby base squares

                for (int dx = -2; dx <= 2; dx++)
                {
                    for (int dy = -(2 - abs(dx)); dy <= (2 - abs(dx)); dy += 2)
                    {
                        int near_x = base->x + dx;
                        int near_y = base->y + dy;

                        // exclude squares beyond the map

                        if (near_y < 0 || near_y >= *map_axis_y)
                            continue;

                        if (*map_toggle_flat && (near_x < 0 || near_x >= *map_axis_x))
                            continue;

                        // wrap x on cylinder map

                        if (!*map_toggle_flat && near_x < 0)
                        {
                            near_x += *map_axis_x;

                        }

                        if (!*map_toggle_flat && near_x >= *map_axis_x)
                        {
                            near_x -= *map_axis_x;

                        }

                        // get near map square

                        MAP *near_map_square = mapsq(near_x, near_y);

                        // get near map square body id

                        int near_map_square_region = near_map_square->region;

                        // check if it is a matching body id

                        if (near_map_square_region == region)
                        {
                            // calculate base distance

                            int base_distance = map_distance(x, y, base->x, base->y);

                            // improve distance if smaller

                            if (!closest_reachable_base_found || base_distance < closest_reachable_base_distance)
                            {
                                closest_reachable_base_found = true;
                                closest_reachable_base_is_coastal = true;
                                closest_reachable_base_distance = base_distance;
                                closest_reachable_base = base;
                                closest_reachable_base_square = base_map_square;

                            }

                        }

                    }

                }

            }

        }

        debug
        (
            "closest_reachable_base_found=%d, closest_reachable_base_is_coastal=%d, closest_reachable_base_distance=%d\n",
            closest_reachable_base_found,
            closest_reachable_base_is_coastal,
            closest_reachable_base_distance
        )
        ;

        // if closest coastal base is found - pretend it is placed on same body

        if (closest_reachable_base_found && closest_reachable_base_is_coastal)
        {
            saved_coastal_base_region = closest_reachable_base_square->region;

            debug
            (
                "Before emulation: closest_reachable_base->x=%d, closest_reachable_base->y=%d, closest_reachable_base_square->region=%d\n",
                closest_reachable_base->x,
                closest_reachable_base->y,
                closest_reachable_base_square->region
            )
            ;

            closest_reachable_base_square->region = region;

            debug
            (
                "After emulation: closest_reachable_base_square->region=%d\n",
                closest_reachable_base_square->region
            )
            ;

        }

    }

    // run original function

    int nearest_base_faction_id = tx_calculate_distance_to_nearest_base(x, y, unknown_1, region, unknown_2, unknown_3);

    // revert coastal base map square body to original

    if (closest_reachable_base_found && closest_reachable_base_is_coastal)
    {
        debug
        (
            "Before restoration: closest_reachable_base_square->region=%d\n",
            closest_reachable_base_square->region
        )
        ;

        closest_reachable_base_square->region = saved_coastal_base_region;

        debug
        (
            "After restoration: closest_reachable_base_square->region=%d\n",
            closest_reachable_base_square->region
        )
        ;

    }

    // return nearest base faction id

    return nearest_base_faction_id;

}

/**
Calculates map distance as in vanilla.
*/
int map_distance(int x1, int y1, int x2, int y2)
{
    int xd = abs(x1-x2);
    int yd = abs(y1-y2);
    if (!*map_toggle_flat && xd > *map_axis_x/2)
        xd = *map_axis_x - xd;

    return ((xd + yd) + abs(xd - yd) / 2) / 2;

}

/**
Verifies two locations are within the base radius of each other.
*/
bool isWithinBaseRadius(int x1, int y1, int x2, int y2)
{
	return map_distance(x1, y1, x2, y2) <= 2;

}

/**
Rolls artillery damage.
*/
HOOK_API int roll_artillery_damage(int attacker_strength, int defender_strength, int attacker_firepower)
{
    debug
    (
        "roll_artillery_damage(attacker_strength=%d, defender_strength=%d, attacker_firepower=%d)\n",
        attacker_strength,
        defender_strength,
        attacker_firepower
    )
    ;

    int artillery_damage = 0;

    // compare attacker and defender strength

    if (attacker_strength >= defender_strength)
    {
        // replicate vanilla algorithm

        int max_damage = attacker_strength / defender_strength;

        artillery_damage = random(max_damage + 1);

    }
    else
    {
        int random_roll = random(attacker_strength + defender_strength);

        artillery_damage = (random_roll < attacker_strength ? 1 : 0);

    }

    // multiply attacker strength by firepower

    artillery_damage *= attacker_firepower;

    debug
    (
        "artillery_damage=%d)\n",
        artillery_damage
    )
    ;

    return artillery_damage;

}

/**
nutrient yield calculation
*/
HOOK_API int mod_nutrient_yield(int faction_id, int base, int x, int y, int tf)
{
    int value = crop_yield(faction_id, base, x, y, tf);

    MAP* sq = mapsq(x, y);

    // condenser does not multiply nutrients
    if (sq && (sq->items & TERRA_CONDENSER))
    {
        value = (value * 2 + 2) / 3;
    }

    // enricher does not multiply nutrients and instead adds 1
    if (sq && (sq->items & TERRA_SOIL_ENR))
    {
        value = (value * 2 + 2) / 3;
        value++;
    }

    return value;

}

/**
mineral yield calculation
*/
HOOK_API int mod_mineral_yield(int faction_id, int base, int x, int y, int tf)
{
    int value = mineral_yield(faction_id, base, x, y, tf);

    return value;

}

/**
energy yield calculation
*/
HOOK_API int mod_energy_yield(int faction_id, int base, int x, int y, int tf)
{
    int value = energy_yield(faction_id, base, x, y, tf);

    return value;

}

/**
SE accumulated resource adjustment
*/
HOOK_API int se_accumulated_resource_adjustment(int a1, int a2, int faction_id, int a4, int a5)
{
    // get faction

    Faction* faction = &tx_factions[faction_id];

    // get old ratings

    int SE_growth_pending_old = faction->SE_growth_pending;
    int SE_industry_pending_old = faction->SE_industry_pending;

    // execute original code

    // use modified function instead of original code
//    int returnValue = tx_set_se_on_dialog_close(a1, a2, faction_id, a4, a5);
    int returnValue = modifiedSocialCalc(a1, a2, faction_id, a4, a5);

    // get new ratings

    int SE_growth_pending_new = faction->SE_growth_pending;
    int SE_industry_pending_new = faction->SE_industry_pending;

    debug
    (
        "se_accumulated_resource_adjustment: faction_id=%d, SE_growth: %+d -> %+d, SE_industry: %+d -> %+d\n",
        faction_id,
        SE_growth_pending_old,
        SE_growth_pending_new,
        SE_industry_pending_old,
        SE_industry_pending_new
    )
    ;

    // adjust nutrients

    if (SE_growth_pending_new != SE_growth_pending_old)
    {
        int row_size_old = 10 - SE_growth_pending_old;
        int row_size_new = 10 - SE_growth_pending_new;

        double conversion_ratio = (double)row_size_new / (double)row_size_old;

        for (int i = 0; i < *total_num_bases; i++)
        {
            BASE* base = &tx_bases[i];

            if (base->faction_id == faction_id)
            {
                base->nutrients_accumulated = (int)round((double)base->nutrients_accumulated * conversion_ratio);
            }

        }

    }

    // adjust minerals

    if (SE_industry_pending_new != SE_industry_pending_old)
    {
        int row_size_old = 10 - SE_industry_pending_old;
        int row_size_new = 10 - SE_industry_pending_new;

        double conversion_ratio = (double)row_size_new / (double)row_size_old;

        for (int i = 0; i < *total_num_bases; i++)
        {
            BASE* base = &tx_bases[i];

            if (base->faction_id == faction_id)
            {
                base->minerals_accumulated = (int)round((double)base->minerals_accumulated * conversion_ratio);
            }

        }

    }

    fflush(debug_log);

    return returnValue;

}

/**
hex cost
*/
HOOK_API int hex_cost(int unit_id, int faction_id, int from_x, int from_y, int to_x, int to_y, int a7)
{
    // execute original code

    int value = tx_hex_cost(unit_id, faction_id, from_x, from_y, to_x, to_y, a7);

    // correct road/tube movement rate

    if (conf.tube_movement_rate_multiplier > 0)
    {
        MAP* square_from = mapsq(from_x, from_y);
        MAP* square_to = mapsq(to_x, to_y);

        if (square_from && square_to)
        {
            // set movement cost along tube to 1
            // base implies tube in its tile
            if
            (
                (square_from->items & (TERRA_BASE_IN_TILE | TERRA_MAGTUBE))
                &&
                (square_to->items & (TERRA_BASE_IN_TILE | TERRA_MAGTUBE))
            )
            {
                value = 1;

            }
            // set road movement cost
            else if (value == 1)
            {
                value = conf.road_movement_cost;

            }

        }

    }

    return value;

}

/**
Calculates tech level recursively.
*/
int wtp_tech_level(int id) {
    if (id < 0 || id > TECH_TranT)
    {
        return 0;
    }
    else
    {
        int v1 = wtp_tech_level(tx_techs[id].preq_tech1);
        int v2 = wtp_tech_level(tx_techs[id].preq_tech2);

        return max(v1, v2) + 1;
    }
}

/**
Calculates tech cost.
cost grows cubic from the beginning then linear.
S  = 20                                     // fixed shift (first tech cost)
C  = 0.02                                   // cubic coefficient
B  = 80 * (<map area> / <normal map area>)  // linear slope
x0 = SQRT(B / (3 * C))                      // break point
A  = C * x0 ^ 3 - B * x0                    // linear intercept
x  = (<level> - 1) * 7

cost = S +
1.
when x < x0 then
C * x ^ 3
2.
else
A + B * x
*/
int wtp_tech_cost(int fac, int tech) {
    assert(fac >= 0 && fac < 8);
    MetaFaction* m = &tx_metafactions[fac];
    int level = 1;

    if (tech >= 0) {
        level = wtp_tech_level(tech);
    }

    double S = 20.0;
    double C = 0.02;
    double B = 80 * (*map_area_tiles / 3200.0);
    double x0 = sqrt(B / (3 * C));
    double A = C * x0 * x0 * x0 - B * x0;
    double x = (level - 1) * 7;

    double base = S + (x < x0 ? C * x * x * x : A + B * x);

    double cost =
        base
        * (double)m->rule_techcost / 100.0
        * (*game_rules & RULES_TECH_STAGNATION ? 1.5 : 1.0)
        * (double)tx_basic->rules_tech_discovery_rate / 100.0
    ;

    double dw;

    if (is_human(fac)) {
        dw = 1.0 + 0.1 * (10.0 - tx_cost_ratios[*diff_level]);
    }
    else {
        dw = 1.0;
    }

    cost *= dw;

    debug("tech_cost %d %d | %8.4f %8.4f %8.4f %d %d %s\n", *current_turn, fac,
        base, dw, cost, level, tech, (tech >= 0 ? tx_techs[tech].name : NULL));

    return max(2, (int)cost);

}

HOOK_API int sayBase(char *buffer, int id)
{
	// execute original code

	int returnValue = tx_say_base(buffer, id);

	// get base

	BASE *base = &(tx_bases[id]);

	// get base population limit

	int populationLimit = getBasePopulationLimit(base);

	// generate symbol

	char symbol[10] = "   <P>";

	if (populationLimit != -1 && base->pop_size >= populationLimit)
	{
		strcat(buffer, symbol);
	}

	// return original value

	return returnValue;

}

bool isBaseFacilityBuilt(BASE *base, int facilityId)
{
	return (base->facilities_built[facilityId / 8]) & (1 << (facilityId % 8));
}

int getBasePopulationLimit(BASE *base)
{
    int pop_rule = tx_metafactions[base->faction_id].rule_population;
    int hab_complex_limit = tx_basic->pop_limit_wo_hab_complex - pop_rule;
    int hab_dome_limit = tx_basic->pop_limit_wo_hab_dome - pop_rule;

    bool habitationComplexBuilt = isBaseFacilityBuilt(base, FAC_HAB_COMPLEX);
    bool habitationDomeBuilt = isBaseFacilityBuilt(base, FAC_HABITATION_DOME);

    if (habitationDomeBuilt)
	{
		return -1;
	}
	else if (habitationComplexBuilt)
	{
		return hab_dome_limit;
	}
	else
	{
		return hab_complex_limit;
	}

}

/**
Checks if we need to refit to population growth facility instead.
*/
int refitToGrowthFacility(int id, BASE *base, int choice)
{
	// growth facility is already chosen

	if (choice == -FAC_HAB_COMPLEX || choice == -FAC_HABITATION_DOME)
		return choice;

	// get next growth facility

	int nextAvailableGrowthFacility = getnextAvailableGrowthFacility(base);

	// skip if no next growth facility is available

	if (nextAvailableGrowthFacility == -1)
		return choice;

	// get base population limit

	int populationLimit = getBasePopulationLimit(base);

	// don't refit if there is no limit
	// this is kinda redundant since we have already asserted there is a growth facility to built

	if (populationLimit == -1)
		return choice;

	// don't refit if 4 people below limit
	// this is to prevent some low mineral bases from building it at size 1

	if (base->pop_size <= (populationLimit - 4))
		return choice;

	// calculate turns to reach the limit

	double turnsToLimit = (double)(((populationLimit + 1 - base->pop_size) * (base->pop_size + 1) * tx_cost_factor(base->faction_id, 0, id)) - base->nutrients_accumulated) / (double)base->nutrient_surplus;

	// calculate turns to build current production and growth facility

	double turnsToBuild = (double)(mineral_cost(base->faction_id, choice) + mineral_cost(base->faction_id, -nextAvailableGrowthFacility) - base->minerals_accumulated) / (double)base->mineral_surplus;

	// return growth facility if we cannot build current production until population limit is reached

	if (turnsToBuild >= turnsToLimit)
	{
		return -nextAvailableGrowthFacility;
	}
	else
	{
		return choice;
	}

}

/**
Return next unbuilt growth facility or -1 if there is no next available facility.
*/
int getnextAvailableGrowthFacility(BASE *base)
{
	if (!isBaseFacilityBuilt(base, FAC_HAB_COMPLEX))
	{
		return (has_tech(base->faction_id, tx_facility[FAC_HAB_COMPLEX].preq_tech) ? FAC_HAB_COMPLEX : -1);
	}
	else if (!isBaseFacilityBuilt(base, FAC_HABITATION_DOME))
	{
		return (has_tech(base->faction_id, tx_facility[FAC_HABITATION_DOME].preq_tech) ? FAC_HABITATION_DOME : -1);
	}
	else
	{
		return -1;
	}

}

/**
Directs portion of bases production toward project construction.
If faction builds more than one project production is directed to the base with smallest id.
*/
void contributeToProject(int factionId)
{
	MetaFaction *metaFaction = &(tx_metafactions[factionId]);

	debug("\n");
	debug("PROJECT CONTRIBUTION: %-24s\n", metaFaction->noun_faction);

	// select project base and other faction bases

	BASE *projectBase = NULL;
	std::vector<BASE *> contributingBases;

	int project = -1;
	int projectCost = 0;
	int requiredMinerals = 0;

	for (int id = 0; id < *total_num_bases; id++)
	{
		BASE *base = &(tx_bases[id]);

		// ignore not own bases

		if (base->faction_id != factionId)
			continue;

		// select project base

		if (projectBase == NULL && isBaseBuildingProject(base))
		{
			projectBase = base;

			int item = getBaseBuildingItem(base);
			project = -item;
			projectCost = mineral_cost(factionId, item);
			requiredMinerals = getBaseBuildingItemCost(factionId, base) - base->minerals_accumulated;

			continue;

		}

		// collect other bases

		contributingBases.push_back(base);

	}

	// do nothing if there is no project base

	if (projectBase == NULL)
		return;

	debug("projectBase: %-25s\n", projectBase->name);

	// collect contribution

	int totalContribution = 0;

	// process contributing bases

	for
	(
		std::vector<BASE *>::iterator contributingBasesIterator = contributingBases.begin();
		contributingBasesIterator != contributingBases.end();
		contributingBasesIterator++
	)
	{
		BASE *base = *contributingBasesIterator;

		// stop cycle if we already have enough

		if (totalContribution >= requiredMinerals)
			break;

		// check if base mineral surplus is above threshold

		if (base->mineral_surplus <= conf.project_contribution_threshold)
			continue;

		// calculate base contribution
		// it cannot take more than accumulated minerals

		int baseContribution = (int)floor(conf.project_contribution_proportion * (base->mineral_surplus - conf.project_contribution_threshold));
		baseContribution = min(base->minerals_accumulated, baseContribution);
		baseContribution = min(requiredMinerals - totalContribution, baseContribution);

		// contribute

		debug("\t%-25s -%4d, %4d -> %4d\n", base->name, baseContribution, base->minerals_accumulated, base->minerals_accumulated - baseContribution);

		base->minerals_accumulated -= baseContribution;
		totalContribution += baseContribution;

	}

	// apply contribution

	debug("->\n");
	debug("\t%-25s +%4d, %4d -> %4d\n", projectBase->name, totalContribution, projectBase->minerals_accumulated, projectBase->minerals_accumulated + totalContribution);

	projectBase->minerals_accumulated += totalContribution;

	// display popup

	int percentageCompleted = projectBase->minerals_accumulated * 100 / projectCost;

	parse_says(0, metaFaction->noun_faction, -1, -1);
	parse_says(1, tx_facility[project].name, -1, -1);
	parse_num(2, percentageCompleted);
	popp(ScriptTxtID, "PROJECTREPORT", 0, "secproj_sm.pcx", 0);

	debug("\n");
    fflush(debug_log);

}

/**
Modifies base init computation.
*/
HOOK_API int baseInit(int factionId, int x, int y)
{
	// execute original code

	int id = tx_base_init(factionId, x, y);

	// return immediatelly if base wasn't created

	if (id < 0)
		return id;

	// get base

	BASE *base = &(tx_bases[id]);

	// The Planetary Transit System fix
	// new population comes from existing bases rather than from thin air

	if (has_project(factionId, FAC_PLANETARY_TRANS_SYS) && base->pop_size == 3)
	{
		// transit two people

		for (int transit = 0; transit < 2; transit++)
		{
			// search base with highest population more than 1

			int highestPopulation = 0;
			BASE *highestPopulationBase = NULL;

			for (int i = 0; i < *total_num_bases; i++)
			{
				BASE *otherBase = &(tx_bases[i]);

				// exclude self

				if (i == id)
					continue;

				// exclude not own bases

				if (otherBase->faction_id != factionId)
					continue;

				// exclude bases size 3 or less

				if (otherBase->pop_size <= 3)
					continue;

				// update best base

				if (otherBase->pop_size > highestPopulation)
				{
					highestPopulation = otherBase->pop_size;
					highestPopulationBase = otherBase;
				}

			}

			// highest population base found
			if (highestPopulationBase != NULL)
			{
				// citizen left for new base

				highestPopulationBase->pop_size--;

			}
			else
			{
				// no one can come - don't add people to new base

				base->pop_size--;

			}

		}

	}

	// return base id

	return id;

}

/**
Wraps display help ability functionality.
This expands single number packed ability value (proportional + flat) into human readable text.
*/
HOOK_API char *getAbilityCostText(int cost, char *destination, int radix)
{
	// execute original code

	char *output = tx_itoa(cost, destination, radix);

	// clear default output

	output[0] = '\x0';

	// append help text

	if (cost == 0)
	{
		strcat(output, "Free");
	}
	else
	{
		// cost factor

		int costFactor = (cost & 0xF);
		int costFactorPercentage = 25 * costFactor;

		if (costFactor > 0)
		{
			sprintf(output + strlen(output), "Increases unit cost by %d %%. ", costFactorPercentage);
		}

		// flat cost

		int flatCost = (cost >> 4);

		if (flatCost > 0)
		{
			sprintf(output + strlen(output), "Increases unit cost by %d mineral rows. ", flatCost);
		}

	}

	// return original value

	return output;

}

/**
Wraps social_calc.
This is initially for CV GROWTH effect.
*/
HOOK_API int modifiedSocialCalc(int seSelectionsPointer, int seRatingsPointer, int factionId, int ignored4, int seChoiceEffectOnly)
{
	// execute original code

	int value = tx_social_calc(seSelectionsPointer, seRatingsPointer, factionId, ignored4, seChoiceEffectOnly);

	// CV changes GROWTH rate if applicable

	if (conf.cloning_vats_se_growth != 0)
	{
		if (has_project(factionId, FAC_CLONING_VATS))
		{
			// calculate GROWTH rating output offset and create pointer to it

			int *seGrowthRating = (int *)seRatingsPointer + (SE_GROWTH - SE_ECONOMY);

			// modify GROWTH rating

			*seGrowthRating += conf.cloning_vats_se_growth;

		}

	}

	// return original value

	return value;

}

void correctcGrowthTurnsForPopulationBoom(int destinationStringPointer, int sourceStringPointer)
{
    // call original function

    tx_strcat(destinationStringPointer, sourceStringPointer);

	// get current base

	int id = *current_base_id;
	BASE *base = *current_base_ptr;

	// get current base faction

	Faction *faction = &(tx_factions[base->faction_id]);

    // calculate base GROWTH rating

    int growthRating = faction->SE_growth_pending;

    if (has_facility(id, FAC_CHILDREN_CRECHE))
	{
		growthRating += 2;
	}

	if (base->status_flags & BASE_GOLDEN_AGE_ACTIVE)
	{
		growthRating += 2;
	}

    // modify output for population boom

    if (growthRating > conf.se_growth_rating_cap)
	{
		// set growth turns text offset

		char *growthTurnsText = (char *)destinationStringPointer + 8;

		// rewrtie growth turns text

		strcpy(growthTurnsText, "POP BOOM !");

	}

}

