#include "prototype.h"

/**
Reads reactor cost factor and stores it in reactor structure.
*/
HOOK_API void read_reactor_cost_factor()
{
    // static reactor cost factor variables
    // there supposed to be 4 reactors and configuration file is read twice
    // so we should end up with 8 reads - no more no less

    static bool reactor_configuration_complete = false;
    static int reactor_index = 0;
    static int reactor_cost_factors[8];

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

        int cost_factor = atoi(field);

        if (cost_factor == 0)
        {
            // incorrect read

            throw std::runtime_error("Reactor cost factor should be an integer.");

        }

        // store reactor cost factor

        reactor_cost_factors[reactor_index] = cost_factor;
        reactor_index++;

    }

    // populating stage

    if (reactor_index == 8)
    {
        for (int i = 0; i < 4; i++)
        {
            // check both reads for this reactor are identical

            if (reactor_cost_factors[i] != reactor_cost_factors[i + 4])
            {
                throw "First and second reads for same reactor produced different results.";

            }

            // set reactor cost factor

            tx_reactor[i].cost_factor = (short)reactor_cost_factors[i];

        }

        reactor_configuration_complete = true;

    }

}

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

All fractional factors are turned into whole rational fractions.
For example, ability with cost 1 multiplies unit cost by 1.25. This is coded in formula as multiplication by 5/4.

All divisions are done after multiplications. This is to minimize an effect of integer rounding on intermediary steps.

Special rules:
Supply module foil/cruiser chassis cost is replaced with infantry/speeder chassis cost, correspondingly.

*/
HOOK_API int proto_cost(int chassis_id, int weapon_id, int armor_id, int abilities, int reactor_level)
{
    // Special rules:
    // Supply module foil/cruiser chassis cost is replaced with infantry/speeder chassis cost, correspondingly.

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

    // get Fission reactor cost factor

    int fission_reactor_cost_factor = tx_reactor[REC_FISSION - 1].cost_factor;

    // get reactor cost factor

    int reactor_cost_factor = tx_reactor[reactor_level - 1].cost_factor;

    // set minimal cost to reactor level (this is checked in some other places so we should do this here to avoid any conflicts)

    int minimal_cost = reactor_level;

    // get pieces cost

    int chassis_cost = tx_chassis[chassis_id].cost;
    int weapon_cost = tx_weapon[weapon_id].cost;
    int armor_cost = tx_defense[armor_id].cost;

    // select primary and secondary item cost

    int primary_item_cost = max(weapon_cost, armor_cost);
    int secondary_item_cost = min(weapon_cost, armor_cost);

    // get abilities cost modifications

    int abilities_cost_factor = 1;
    int abilities_cost_divider = 1;
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
                abilities_cost_factor *= (4 + ability_cost_factor);
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

    int cost =
        max
        (
            minimal_cost,                           // minimal cost
            (
                (
                    primary_item_cost * 2           // primary item true adjusted cost
                    +
                    secondary_item_cost             // secondary item true adjusted cost
                    -
                    1                               // scale adjustment to make Scout Patrol cost 1
                )
                *
                reactor_cost_factor                 // reactor cost factor
                *
                chassis_cost
                *
                abilities_cost_factor
            )
            /
            (
                2                                     // unit base cost denominator
                *
                fission_reactor_cost_factor           // fission reactor cost factor
                *
                2                                     // chassis denominator
                *
                abilities_cost_divider                // abilities denominator
            )
            +
            abilities_cost_addition                 // abilities flat cost
        )
    ;

    return cost;

}

