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

