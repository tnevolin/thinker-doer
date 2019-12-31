#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "wtp.h"

/**
Prototype cost calculation.

Select primary and secondary weapon/module/armor items
primary item = one with higher true cost
secondary item = one with lower true cost

Calculate unit base cost
unit base cost = primary item cost + secondary item cost / 2 - 1

Multiply by reactor cost factor
reactor cost factor = reactor cost / Fission reactor cost

Multiply by chassis cost factor
chassis cost factor = chassis cost / 2

Multiply by ability factor or add ability flat cost
ability bytes 0-3 is unit cost factor
ability bytes 4-7 is unit cost flat addition

Special rules:
Colony foil/cruiser costs same as infantry/speeder.
Former foil/cruiser costs same as infantry/speeder.
Supply foil/cruiser costs same as infantry/speeder.

*/
HOOK_API int proto_cost(int chassis_id, int weapon_id, int armor_id, int abilities, int reactor_level)
{
    // Special rules

    if
    (
        tx_weapon[weapon_id].mode == WMODE_COLONIST
        ||
        tx_weapon[weapon_id].mode == WMODE_TERRAFORMER
        ||
        tx_weapon[weapon_id].mode == WMODE_CONVOY
    )
    {
        switch(chassis_id)
        {
        case CHS_FOIL:
            chassis_id = CHS_INFANTRY;
            break;

        case CHS_CRUISER:
            chassis_id = CHS_SPEEDER;
            break;

        }

    }

    // get Fission reactor cost factor

    double fission_reactor_cost_factor = (double)conf.reactor_cost_factors[REC_FISSION - 1];

    // get reactor cost factor

    double reactor_cost_factor = (double)conf.reactor_cost_factors[reactor_level - 1];

    // set minimal cost to reactor level (this is checked in some other places so we should do this here to avoid any conflicts)

    int minimal_cost = reactor_level;

    // get pieces cost

    double chassis_cost = (double)tx_chassis[chassis_id].cost;
    double weapon_cost = (double)tx_weapon[weapon_id].cost;
    double armor_cost = (double)tx_defense[armor_id].cost;

    // select primary and secondary item cost

    double primary_item_cost = max(weapon_cost, armor_cost);
    double secondary_item_cost = min(weapon_cost, armor_cost);

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

    int cost =
        max
        (
            // minimal cost
            minimal_cost,
            (int)
            round
            (
                (
                    // primary item cost
                    primary_item_cost
                    +
                    // secondary item cost scaled and halved
                    (secondary_item_cost - 1.0) / 2.0
                )
                *
                // chassis cost factor
                (chassis_cost / 2.0)
                *
                // abilities cost factor
                (1.0 + 0.25 * abilities_cost_factor)
                *
                // reactor cost factor
                (reactor_cost_factor / fission_reactor_cost_factor)
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
1. Determines which side wins this round. Returns 1 for attacker, 0 for defender.
2. Sets attacker and defender firepower.
*/
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
{
    debug("combat_roll(attacker_strength=%d, defender_strength=%d, attacker_vehicle_offset=%d, defender_vehicle_offset=%d, *attacker_firepower, *defender_firepower)\n",
        attacker_strength,
        defender_strength,
        attacker_vehicle_offset,
        defender_vehicle_offset
    )
    ;

    // get attacker and defender units

    UNIT attacker_unit = tx_units[tx_vehicles[attacker_vehicle_offset / sizeof(VEH)].proto_id];
    UNIT defender_unit = tx_units[tx_vehicles[defender_vehicle_offset / sizeof(VEH)].proto_id];

    // ATTACKER FIREPOWER

    // calculate attacker multiplied firepower

    int attacker_multiplied_firepower = attacker_firepower * conf.firepower_multiplier;
    debug("\tattacker_multiplied_firepower=%d\n", attacker_multiplied_firepower);

    // calculate attacker random firepower

    int attacker_random_firepower = (conf.random_firepower ? (rand() % attacker_multiplied_firepower) + 1 : attacker_multiplied_firepower);
    debug("\tattacker_random_firepower=%d\n", attacker_random_firepower);

    // DEFENDER FIREPOWER

    // calculate defender multiplied firepower

    int defender_multiplied_firepower = defender_firepower * conf.firepower_multiplier;
    debug("\tdefender_multiplied_firepower=%d\n", defender_multiplied_firepower);

    // calculate defender random firepower

    int defender_random_firepower = (conf.random_firepower ? (rand() % defender_multiplied_firepower) + 1 : defender_multiplied_firepower);
    debug("\tdefender_random_firepower=%d\n", defender_random_firepower);

    // ROUND OUTCOME

    // normalize strength values

    if (attacker_strength < 1) attacker_strength = 1;
    if (defender_strength < 1) defender_strength = 1;

    // generate random number

    int random_integer = rand();
    debug("\trandom_integer=%d\n", random_integer);

    int random_roll = random_integer % (attacker_strength + defender_strength);
    debug("\trandom_roll=%d\n", random_roll);

    // calculate outcome

    int attacker_wins = (random_roll < attacker_strength ? 1 : 0);
    debug("\tattacker_wins=%d\n", attacker_wins);

    // return outcome

    return attacker_wins;

}

/**
Calculate display odds.
*/
HOOK_API void calculate_odds
(
    int attacker_vehicle_offset,
    int defender_vehicle_offset,
    int attacker_weapon_strength,
    int defender_armor_strength,
    int *attacker_odd,
    int *defender_odd
)
{
    debug("calculate_odds(attacker_vehicle_offset=%d, defender_vehicle_offset=%d, attacker_weapon_strength=%d, defender_armor_strength=%d)\n",
        attacker_vehicle_offset,
        defender_vehicle_offset,
        attacker_weapon_strength,
        defender_armor_strength
    )
    ;

    // get attacker and defender vehicles

    VEH attacker_vehicle = tx_vehicles[attacker_vehicle_offset / sizeof(VEH)];
    VEH defender_vehicle = tx_vehicles[defender_vehicle_offset / sizeof(VEH)];

    // get attacker and defender units

    UNIT attacker_unit = tx_units[attacker_vehicle.proto_id];
    UNIT defender_unit = tx_units[defender_vehicle.proto_id];

    // get attacker and defender HP
    // artifact gets 1 HP regardless of reactor

    int attacker_hp = (attacker_unit.unit_plan == PLAN_ALIEN_ARTIFACT ? 1 : attacker_unit.reactor_type * 10 - attacker_vehicle.damage_taken);
    int defender_hp = (defender_unit.unit_plan == PLAN_ALIEN_ARTIFACT ? 1 : defender_unit.reactor_type * 10 - defender_vehicle.damage_taken);

    // determine if we are ignoring reactor power

    bool ignore_reactor_power =
        conf.ignore_reactor_power_in_combat
        ||
        tx_weapon[attacker_unit.weapon_type].offense_value < 0
        ||
        tx_defense[defender_unit.armor_type].defense_value < 0
    ;

    // calculate max firepower

    int attacker_max_fp =
        (ignore_reactor_power ? defender_unit.reactor_type : 1)
        *
        conf.firepower_multiplier
    ;
    int defender_max_fp =
        (ignore_reactor_power ? attacker_unit.reactor_type : 1)
        *
        conf.firepower_multiplier
    ;

    // calculate firepower

    double attacker_fp = (conf.random_firepower ? (double)(attacker_max_fp + 1) / 2.0 : (double)attacker_max_fp);
    double defender_fp = (conf.random_firepower ? (double)(defender_max_fp + 1) / 2.0 : (double)defender_max_fp);

    // calculate effective hp

    int attacker_effective_hp = (int)ceil((double)attacker_hp / defender_fp);
    int defender_effective_hp = (int)ceil((double)defender_hp / attacker_fp);

    // determine attacker round winning probability

    double p = (double)attacker_weapon_strength / ((double)attacker_weapon_strength + (double)defender_armor_strength);
    double q = 1 - p;

    // calculate attacker winning probability

    double attacker_winning_probability = 0.0;
    double p1 = pow(p, defender_effective_hp);

    for (int alhp = 0; alhp < attacker_effective_hp; alhp++)
    {
        double q1 = pow(q, alhp);

        double c = binomial_koefficient(defender_effective_hp - 1 + alhp, alhp);

        attacker_winning_probability += p1 * q1 * c;

    }

    debug("p=%f, attacker_hp=%d, defender_hp=%d, attacker_fp=%f, defender_fp=%f, attacker_effective_hp=%d, defender_effective_hp=%d\n", p, attacker_hp, defender_hp, attacker_fp, defender_fp, attacker_effective_hp, defender_effective_hp);
    debug("attacker_winning_probability=%f\n", attacker_winning_probability);

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

