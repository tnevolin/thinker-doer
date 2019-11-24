#include "prototype.h"

/**
Reads reactor cost multiplier and stores it in reactor structure.
*/
HOOK_API void read_reactor_cost_multiplier()
{
    // static reactor cost multiplier variables
    // there supposed to be 4 reactors and configuration file is read twice
    // so we should end up with 8 reads - no more no less

    static bool reactor_configuration_complete = false;
    static int reactor_index = 0;
    static int reactor_cost_multipliers[8];

    // check if we have already completed all reads

    if (reactor_configuration_complete)
    {
        throw std::runtime_error("Incorrect number of reactor line reads. Expected 8.");

    }

    // reading stage

    if (reactor_index < 8)
    {
        // read configuration field

        char* field = tx_read_configuration_field();

        // parse field into number

        int cost_multiplier;

        int sscanfOutput = sscanf(field, "%d", &cost_multiplier);

        if (sscanfOutput == 1)
        {
            // correct read

        }
        else
        {
            throw std::runtime_error("Reactor cost multiplier should be an integer.");

        }

        // store reactor cost multiplier

        reactor_cost_multipliers[reactor_index] = cost_multiplier;
        reactor_index++;

    }

    // populating stage

    if (reactor_index == 8)
    {
        for (int i = 0; i < 4; i++)
        {
            // check both reads for this reactor are identical

            if (reactor_cost_multipliers[i] != reactor_cost_multipliers[i + 4])
            {
                throw "First and second reads for same reactor produced different results.";

            }

            // set reactor cost multiplier

            tx_reactor[i].cost_multiplier = (short) reactor_cost_multipliers[i];

        }

        reactor_configuration_complete = true;

    }

}

/**
Prototype cost calculation.

Chassis cost divided by 2 is a unit cost multiplier.

Weapon and armor cost is multiplied by reactor cost multiplier.
Module cost is NOT multiplied by reactor cost multiplier.

Ability bytes 0-3 is unit cost multiplier.
Ability bytes 4-7 is flat addition to unit cost.

All fractional multipliers are turned into whole rational fractions.
For example, ability with cost 1 multiplies unit cost by 1.25. This is coded in formula as multiplication by 5/4.

All divisions are done after multiplications. This is to minimize an effect of integer rounding on intermediary steps.

*/
HOOK_API int proto_cost(int chassis_id, int weapon_id, int armor_id, int abilities, int reactor_level)
{
    // special case: supply foil and cruiser chassis are counted as infantry and speeder

    if (tx_weapon[weapon_id].mode == WMODE_CONVOY)
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

    // get reactor cost multiplier

    int reactor_cost_multiplier = tx_reactor[reactor_level - 1].cost_multiplier;

    // get elements cost

    int chassis_cost = tx_chassis[chassis_id].cost;
    int weapon_cost = tx_weapon[weapon_id].cost * (tx_weapon[weapon_id].mode < 3 ? reactor_cost_multiplier : 10 /*reactor cost multiplier denominator*/);
    int armor_cost = tx_defense[armor_id].cost * reactor_cost_multiplier;

    // get abilities cost modifications

    int abilities_cost_multiplier = 1;
    int abilities_cost_divider = 1;
    int abilities_cost_addition = 0;
    for (int ability_id = 0; ability_id < 32; ability_id++)
    {
        if (((abilities >> ability_id) & 0x1) == 0x1)
        {
            int ability_cost = tx_ability[ability_id].cost;

            // take ability cost multiplier bits: 0-3

            int ability_cost_multiplier = (ability_cost & 0xF);

            if (ability_cost_multiplier > 0)
            {
                abilities_cost_multiplier *= (4 + ability_cost_multiplier);
                abilities_cost_divider *= 4;

            }

            // take ability cost addition bits: 4-7

            int ability_cost_addition = ((ability_cost >> 4) & 0xF);

            if (ability_cost_addition > 0)
            {
                abilities_cost_addition += ability_cost_addition;

            }

        }

    }

    int cost = max(1, ((max(weapon_cost, armor_cost) + (weapon_cost + armor_cost)) * chassis_cost) * abilities_cost_multiplier / 2/*averaging weapon and armor*/ / 2/*chassis denominator*/ / 10 /*reactor cost multiplier denominator*/ / abilities_cost_divider + abilities_cost_addition);

    return cost;

}

