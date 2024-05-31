#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <map>
#include "main.h"
#include "wtp_terranx.h"
#include "wtp_mod.h"
#include "patch.h"
#include "engine.h"
#include "wtp_game.h"
#include "wtp_ai.h"
#include "tech.h"
#include "patch.h"

int alphax_tgl_probe_steal_tech_value;

void Profile::start()
{
	if (DEBUG)
	{
		this->startTime = clock();
	}
}
void Profile::stop()
{
	if (DEBUG)
	{
		this->executionCount++;
		this->totalTime += clock() - this->startTime;
	}
}
int Profile::getCount()
{
	return this->executionCount;
}
double Profile::getTime()
{
	return (double)this->totalTime / (double)CLOCKS_PER_SEC;
}
std::map<std::string, Profile> executionProfiles;

void profileFunction(const std::string &name, void (*f)())
{
	if (DEBUG)
	{
		executionProfiles[name].start();
		f();
		executionProfiles[name].stop();
	}
	else
	{
		f();
	}

}

// player setup helper variable

int balanceFactionId = -1;

// storage variables for modified odds dialog

int currentAttackerVehicleId = -1;
int currentDefenderVehicleId = -1;
int currentAttackerOdds = -1;
int currentDefenderOdds = -1;

// storage variables for attacking interceptor

bool bomberAttacksInterceptor = false;
int bomberAttacksInterceptorAttackerVehicleId = -1;
int bomberAttacksInterceptorDefenderVehicleId = -1;
int bomberAttacksInterceptorPass = 0;

/*
Modified code to read basic rules.
*/
__cdecl int wtp_mod_read_basic_rules()
{
    // run Thinker mod code

    int value = mod_read_basic_rules();

    // update rules

    if (value == 0)
    {
        if (conf.magtube_movement_rate > 0)
        {
            // set road movement cost
            conf.road_movement_cost = conf.magtube_movement_rate;
            // set tube movement rate
            Rules->move_rate_roads *= conf.magtube_movement_rate;

        }

    }

    return value;

}

/*
Combat calculation placeholder.
All custom combat calculation goes here.
*/
__cdecl void wtp_mod_battle_compute(int attackerVehicleId, int defenderVehicleId, int attackerOffenseValuePointer, int defenderStrengthPointer, int flags)
{
//    debug
//    (
//        "mod_battle_compute(attackerVehicleId=%d, defenderVehicleId=%d, attackerOffenseValuePointer=%d, defenderStrengthPointer=%d, flags=%d)\n",
//        attackerVehicleId,
//        defenderVehicleId,
//        attackerOffenseValuePointer,
//        defenderStrengthPointer,
//        flags
//    )
//    ;
//
	// get attacker/defender vehicle

	VEH *attackerVehicle = &Vehicles[attackerVehicleId];
	VEH *defenderVehicle = &Vehicles[defenderVehicleId];

	// get attacker/defender unit

	UNIT *attackerUnit = &Units[attackerVehicle->unit_id];
	UNIT *defenderUnit = &Units[defenderVehicle->unit_id];

	// get item values

	int attackerOffenseValue = getUnitOffenseValue(attackerVehicle->unit_id);
	int defenderOffenseValue = getUnitOffenseValue(defenderVehicle->unit_id);
	int attackerDefenseValue = getUnitDefenseValue(attackerVehicle->unit_id);
	int defenderDefenseValue = getUnitDefenseValue(defenderVehicle->unit_id);

	// determine psi combat

	bool psiCombat = attackerOffenseValue < 0 || defenderDefenseValue < 0;

	// get vehicle unit speeds

	int attackerUnitSpeed = unit_speed(attackerVehicle->unit_id);
	int defenderUnitSpeed = unit_speed(defenderVehicle->unit_id);

	// get combat map tile

	MAP *attackerMapTile = getVehicleMapTile(attackerVehicleId);
	MAP *defenderMapTile = getVehicleMapTile(defenderVehicleId);

	// run original function

	battle_compute(attackerVehicleId, defenderVehicleId, (int *)attackerOffenseValuePointer, (int *)defenderStrengthPointer, flags);

    // ----------------------------------------------------------------------------------------------------
    // conventional artillery duel uses weapon + armor value
    // ----------------------------------------------------------------------------------------------------

    if
	(
		// artillery duel
		(flags & 0x2) != 0
		&&
		// conventional combat
		!psiCombat
		&&
		// fix is enabled
		conf.conventional_artillery_duel_uses_weapon_and_armor
	)
	{
		// artillery duelants use weapon + armor value

		*(int *)attackerOffenseValuePointer = *(int *)attackerOffenseValuePointer * (attackerOffenseValue + attackerDefenseValue) / attackerOffenseValue;
		*(int *)defenderStrengthPointer = *(int *)defenderStrengthPointer * (defenderOffenseValue + defenderDefenseValue) / defenderOffenseValue;

	}

    // ----------------------------------------------------------------------------------------------------
    // psi artillery duel uses base intrinsic bonus for attacker
    // ----------------------------------------------------------------------------------------------------

    if
	(
		// artillery duel
		(flags & 0x2) != 0
		// psi combat
		&& psiCombat
		// attacker is in base
		&& map_has_item(attackerMapTile, BIT_BASE_IN_TILE)
	)
	{
		// modify attacker strength

		*(int *)attackerOffenseValuePointer +=
			(*(int *)attackerOffenseValuePointer) * Rules->combat_bonus_intrinsic_base_def / 100;

		// add effect description

		if (*tx_battle_compute_attacker_effect_count < 4)
		{
			strcpy
			(
				(*tx_battle_compute_attacker_effect_labels)[*tx_battle_compute_attacker_effect_count],
				*(*tx_labels + LABEL_OFFSET_BASE)
			);
			(*tx_battle_compute_attacker_effect_values)[*tx_battle_compute_attacker_effect_count] =
				Rules->combat_bonus_intrinsic_base_def;

			(*tx_battle_compute_attacker_effect_count)++;

		}

	}

    // ----------------------------------------------------------------------------------------------------
    // interceptor scramble fix
    // ----------------------------------------------------------------------------------------------------

    if
	(
		// fix is enabled
		conf.interceptor_scramble_fix
		&&
		// attacker uses conventional weapon
		Weapon[attackerUnit->weapon_id].offense_value > 0
		&&
		// attacker is an air unit
		attackerUnit->triad() == TRIAD_AIR
		&&
		// attacker has no air superiority
		!unit_has_ability(attackerVehicle->unit_id, ABL_AIR_SUPERIORITY)
		&&
		// defender uses conventional armor
		Armor[defenderUnit->armor_id].defense_value > 0
		&&
		// defender is an air unit
		defenderUnit->triad() == TRIAD_AIR
		&&
		// defender has air superiority
		unit_has_ability(defenderVehicle->unit_id, ABL_AIR_SUPERIORITY)
	)
	{
		// attacker uses armor, not weapon

		*(int *)attackerOffenseValuePointer = *(int *)attackerOffenseValuePointer * attackerDefenseValue / attackerOffenseValue;

		// defender strength is increased due to air superiority

		*(int *)defenderStrengthPointer = *(int *)defenderStrengthPointer * (100 + Rules->combat_bonus_air_supr_vs_air) / 100;

		// add defender effect description

		if (*tx_battle_compute_defender_effect_count < 4)
		{
			strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], *(*tx_labels + LABEL_OFFSET_AIRTOAIR));
			(*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = Rules->combat_bonus_air_supr_vs_air;

			(*tx_battle_compute_defender_effect_count)++;

		}

		// set interceptor global variables

		bomberAttacksInterceptor = true;
		bomberAttacksInterceptorAttackerVehicleId = attackerVehicleId;
		bomberAttacksInterceptorDefenderVehicleId = defenderVehicleId;
		bomberAttacksInterceptorPass = 0;

	}
	else
	{
		// clear interceptor global variables

		bomberAttacksInterceptor = false;
		bomberAttacksInterceptorAttackerVehicleId = -1;
		bomberAttacksInterceptorDefenderVehicleId = -1;

	}

    // ----------------------------------------------------------------------------------------------------
    // PLANET combat bonus on defense
    // ----------------------------------------------------------------------------------------------------

    if (conf.planet_defense_bonus)
    {
        // PLANET bonus applies to psi combat only
        // psi combat is triggered by either attacker negative weapon value or defender negative armor value

        if (Weapon[Units[attackerVehicle->unit_id].weapon_id].offense_value < 0 || Armor[Units[defenderVehicle->unit_id].armor_id].defense_value < 0)
        {
            // get defender faction id

            int defenderFactionId = Vehicles[defenderVehicleId].faction_id;

			// PLANET bonus applies to normal factions only

            if (defenderFactionId > 0)
            {
                // get defender planet rating at the beginning of the turn (not pending)

                int defenderPlanetRating = Factions[defenderFactionId].SE_planet;

                if (defenderPlanetRating != 0)
                {
                    // calculate defender psi combat bonus

                    int defenderPsiCombatBonus = Rules->combat_psi_bonus_per_planet * defenderPlanetRating;

                    // modify defender strength

                    *(int *)defenderStrengthPointer = (int)round((double)(*(int *)defenderStrengthPointer) * (1.0 + (double)defenderPsiCombatBonus / 100.0));

                    // add effect description

                    if (*tx_battle_compute_defender_effect_count < 4)
                    {
                        strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], *(*tx_labels + LABEL_OFFSET_PLANET));
                        (*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = defenderPsiCombatBonus;

                        (*tx_battle_compute_defender_effect_count)++;

                    }

                }

            }

        }

    }

    // ----------------------------------------------------------------------------------------------------
    // sensor
    // ----------------------------------------------------------------------------------------------------

    // sensor bonus on attack

    if (conf.combat_bonus_sensor_offense)
	{
		if
		(
			// either combat is on land or sensor bonus is applicable on ocean
			(!is_ocean(defenderMapTile))
			||
			(is_ocean(defenderMapTile) && conf.combat_bonus_sensor_ocean)
		)
		{
			// attacker is in range of friendly sensor and combat is happening on attacker territory

			if
			(
				isWithinFriendlySensorRange(attackerVehicle->faction_id, defenderVehicle->x, defenderVehicle->y)
				&&
				defenderMapTile->owner == attackerVehicle->faction_id
			)
			{
				// modify attacker strength

				*(int *)attackerOffenseValuePointer = (int)round((double)(*(int *)attackerOffenseValuePointer) * (1.0 + (double)Rules->combat_defend_sensor / 100.0));

				// add effect description

				if (*tx_battle_compute_attacker_effect_count < 4)
				{
					strcpy((*tx_battle_compute_attacker_effect_labels)[*tx_battle_compute_attacker_effect_count], *(*tx_labels + LABEL_OFFSET_SENSOR));
					(*tx_battle_compute_attacker_effect_values)[*tx_battle_compute_attacker_effect_count] = Rules->combat_defend_sensor;

					(*tx_battle_compute_attacker_effect_count)++;

				}

			}

		}

	}

    // sensor bonus on defense

	{
		if
		(
			// ocean combat only
			// land defence is already covered by vanilla
			(is_ocean(defenderMapTile) && conf.combat_bonus_sensor_ocean)
		)
		{
			// defender is in range of friendly sensor

			if (isWithinFriendlySensorRange(defenderVehicle->faction_id, defenderVehicle->x, defenderVehicle->y))
			{
				// modify defender strength

				*(int *)defenderStrengthPointer = (int)round((double)(*(int *)defenderStrengthPointer) * (1.0 + (double)Rules->combat_defend_sensor / 100.0));

				// add effect description

				if (*tx_battle_compute_defender_effect_count < 4)
				{
					strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], *(*tx_labels + LABEL_OFFSET_SENSOR));
					(*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = Rules->combat_defend_sensor;

					(*tx_battle_compute_defender_effect_count)++;

				}

			}

		}

	}

    // ----------------------------------------------------------------------------------------------------
    // attacking along the road
    // ----------------------------------------------------------------------------------------------------

    if
	(
		// not psi combat
		!psiCombat
		&&
		// attacking along roads bonus is enabled
		conf.combat_bonus_attacking_along_road != 0
		&&
		// attacker is on road or in base
		map_has_item(attackerMapTile, BIT_ROAD | BIT_MAGTUBE | BIT_BASE_IN_TILE)
		&&
		// defender is on road and NOT in base/bunker/airbase
		map_has_item(defenderMapTile, BIT_ROAD | BIT_MAGTUBE) && !map_has_item(defenderMapTile, BIT_BASE_IN_TILE | BIT_BUNKER | BIT_AIRBASE)
		&&
		// attacker unit speed is greater than defender unit speed
		attackerUnitSpeed > defenderUnitSpeed
	)
	{
		// modify attacker strength

		*(int *)attackerOffenseValuePointer += (*(int *)attackerOffenseValuePointer * conf.combat_bonus_attacking_along_road) / 100;

		// add effect description

		if (*tx_battle_compute_attacker_effect_count < 4)
		{
			strcpy((*tx_battle_compute_attacker_effect_labels)[*tx_battle_compute_attacker_effect_count], *(*tx_labels + LABEL_OFFSET_ROAD_ATTACK));
			(*tx_battle_compute_attacker_effect_values)[*tx_battle_compute_attacker_effect_count] = conf.combat_bonus_attacking_along_road;

			(*tx_battle_compute_attacker_effect_count)++;

		}

	}

    // ----------------------------------------------------------------------------------------------------
    // artillery duel uses all combat bonuses
    // ----------------------------------------------------------------------------------------------------

    if
	(
		// artillery duel
		(flags & 0x2) != 0
		&&
		// fix is enabled
		conf.artillery_duel_uses_bonuses
	)
	{
		// add sensor bonus on land defense

		if
		(
			// land combat tile
			!is_ocean(defenderMapTile)
			&&
			// defender is within own sensor range
			isWithinFriendlySensorRange(defenderVehicle->faction_id, defenderVehicle->x, defenderVehicle->y)
		)
		{
			// modify defender strength

			*(int *)defenderStrengthPointer = (int)round((double)(*(int *)defenderStrengthPointer) * (1.0 + (double)Rules->combat_defend_sensor / 100.0));

			// add effect description

			if (*tx_battle_compute_defender_effect_count < 4)
			{
				strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], *(*tx_labels + LABEL_OFFSET_SENSOR));
				(*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = Rules->combat_defend_sensor;

				(*tx_battle_compute_defender_effect_count)++;

			}

		}

		// add base defense bonuses

		if
		(
			// defender is in base
			map_has_item(defenderMapTile, BIT_BASE_IN_TILE)
		)
		{
			if (psiCombat)
			{
				// psi combat

				// base intrinsic defense

				*(int *)defenderStrengthPointer = (int)round((double)(*(int *)defenderStrengthPointer) * getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def));

				// add effect description

				if (*tx_battle_compute_defender_effect_count < 4)
				{
					strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], *(*tx_labels + LABEL_OFFSET_BASE));
					(*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = Rules->combat_bonus_intrinsic_base_def;

					(*tx_battle_compute_defender_effect_count)++;

				}

			}
			else
			{
				// conventional combat

				// search base

				int defenderBaseId = -1;

				for (int baseId = 0; baseId < *BaseCount; baseId++)
				{
					BASE *base = &(Bases[baseId]);

					if (base->x == defenderVehicle->x && base->y == defenderVehicle->y)
					{
						defenderBaseId = baseId;
						break;
					}

				}

				if (defenderBaseId != -1)
				{
					// assign label and multiplier

					const char *label = nullptr;
					int multiplier = 2;

					switch (attackerUnit->triad())
					{
					case TRIAD_LAND:
						if (isBaseHasFacility(defenderBaseId, FAC_PERIMETER_DEFENSE))
						{
							label = *(*tx_labels + LABEL_OFFSET_PERIMETER);
							multiplier += conf.perimeter_defense_bonus;
						}
						break;

					case TRIAD_SEA:
						if (isBaseHasFacility(defenderBaseId, FAC_NAVAL_YARD))
						{
							label = LABEL_NAVAL_YARD;
							multiplier += conf.perimeter_defense_bonus;
						}
						break;

					case TRIAD_AIR:
						if (isBaseHasFacility(defenderBaseId, FAC_AEROSPACE_COMPLEX))
						{
							label = LABEL_AEROSPACE_COMPLEX;
							multiplier += conf.perimeter_defense_bonus;
						}
						break;

					}

					if (isBaseHasFacility(defenderBaseId, FAC_TACHYON_FIELD))
					{
						label = *(*tx_labels + LABEL_OFFSET_TACHYON);
						multiplier += conf.tachyon_field_bonus;
					}

					if (label == nullptr)
					{
						// base intrinsic defense

						*(int *)defenderStrengthPointer = (int)round((double)(*(int *)defenderStrengthPointer) * getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def));

						// add effect description

						if (*tx_battle_compute_defender_effect_count < 4)
						{
							strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], *(*tx_labels + LABEL_OFFSET_BASE));
							(*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = Rules->combat_bonus_intrinsic_base_def;

							(*tx_battle_compute_defender_effect_count)++;

						}

					}
					else
					{
						// base defensive facility

						*(int *)defenderStrengthPointer = (int)round((double)(*(int *)defenderStrengthPointer) * (double)multiplier / 2.0);

						// add effect description

						if (*tx_battle_compute_defender_effect_count < 4)
						{
							strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], label);
							(*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = (multiplier - 2) * 50;

							(*tx_battle_compute_defender_effect_count)++;

						}

					}

				}

			}

		}

	}

    // TODO - remove. This code doesn't work well with Hasty and Gas modifiers.
//    // adjust summary lines to the bottom
//
//    while (*battle_compute_attacker_effect_count < 4)
//    {
//        (*battle_compute_attacker_effect_labels)[*battle_compute_attacker_effect_count][0] = '\x0';
//        (*battle_compute_attacker_effect_values)[*battle_compute_attacker_effect_count] = 0;
//
//        (*battle_compute_attacker_effect_count)++;
//
//    }
//
//    while (*battle_compute_defender_effect_count < 4)
//    {
//        (*battle_compute_defender_effect_labels)[*battle_compute_defender_effect_count][0] = '\x0';
//        (*battle_compute_defender_effect_values)[*battle_compute_defender_effect_count] = 0;
//
//        (*battle_compute_defender_effect_count)++;
//
//    }
//
}

/*
Composes combat effect value percentage.
Overwrites zero with empty string.
*/
__cdecl void wtp_mod_battle_compute_compose_value_percentage(int output_string_pointer, int input_string_pointer)
{
    debug
    (
        "wtp_mod_battle_compute_compose_value_percentage:input(output_string=%s, input_string=%s)\n",
        (char *)output_string_pointer,
        (char *)input_string_pointer
    )
    ;

    // call original function

    tx_strcat(output_string_pointer, input_string_pointer);

    debug
    (
        "wtp_mod_battle_compute_compose_value_percentage:output(output_string=%s, input_string=%s)\n",
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
        "wtp_mod_battle_compute_compose_value_percentage:corrected(output_string=%s, input_string=%s)\n",
        (char *)output_string_pointer,
        (char *)input_string_pointer
    )
    ;

}

/*
Prototype cost calculation.

0.
All non combat sea unit (colony, former, transport) have their chassis cost reduced to match land analogues.
foil = infantry
cruiser = speeder

1. Calculate module and weapon/armor reactor modified costs.
module reactor modified cost = item cost * (Fission reactor value / 100)
weapon/armor reactor modified cost = item cost * (reactor value / 100)
1a. Planet Buster is treated as module.

2. Select primary and secondary item.
primary item = the one with higher reactor modified cost
secondary item = the one with lower reactor modified cost

3. Calculate primary item cost and secondary item shifted cost
primary item cost = item cost * (corresponding reactor value / 100)
secondary item shifted cost = (item cost - 1) * (corresponding reactor value / 100)

4. Calculate unit base cost.
unit base cost = [primary item cost + secondary item shifted cost / 2] * chassis cost / <infrantry chassis cost>

5. Round.

6. Multiply by ability cost factor.
ability bytes 0-3 is cost factor

7. Add ability flat cost
ability bytes 4-7 is flat cost

8. Round.

*/
__cdecl int wtp_mod_proto_cost(int chassis_id, int weapon_id, int armor_id, int abilities, int reactor_level)
{
    // calculate reactor cost factors

    double fission_reactor_cost_factor = (double)conf.reactor_cost_factors[0] / 100.0;
    double reactor_cost_factor = (double)conf.reactor_cost_factors[reactor_level - 1] / 100.0;

    // calculate minimal item costs

    int minimal_weapon_cost = Weapon[WPN_HAND_WEAPONS].cost;
    int minimal_armor_cost = Armor[ARM_NO_ARMOR].cost;

    // get component values

    int weapon_offence_value = Weapon[weapon_id].offense_value;
    int armor_defense_value = Armor[armor_id].defense_value;
    int chassis_speed = Chassis[chassis_id].speed;

    // determine whether we have module or weapon

    bool module = ((weapon_id >= WPN_PLANET_BUSTER && weapon_id <= WPN_ALIEN_ARTIFACT) || weapon_id == WPN_TECTONIC_PAYLOAD || weapon_id == WPN_FUNGAL_PAYLOAD);

    // get component costs

    int weapon_cost = Weapon[weapon_id].cost;
    int armor_cost = Armor[armor_id].cost;
    int chassis_cost = Chassis[chassis_id].cost;

    // modify chassis cost for sea based non combat related modules

    if (weapon_id == WPN_COLONY_MODULE || weapon_id == WPN_TERRAFORMING_UNIT || weapon_id == WPN_TROOP_TRANSPORT)
	{
		switch (chassis_id)
		{
		case CHS_FOIL:
			chassis_cost = Chassis[CHS_INFANTRY].cost;
			break;
		case CHS_CRUISER:
			chassis_cost = Chassis[CHS_SPEEDER].cost;
			break;
		}
	}

    // calculate items reactor modified costs

    double weapon_reactor_modified_cost = (double)weapon_cost * (module ? fission_reactor_cost_factor : reactor_cost_factor);
    double armor_reactor_modified_cost = (double)armor_cost * reactor_cost_factor;

    // select primary item and secondary item shifted costs

    double primary_item_cost;
    double secondary_item_shifted_cost;

    if (weapon_reactor_modified_cost >= armor_reactor_modified_cost)
	{
		primary_item_cost = weapon_reactor_modified_cost;
		secondary_item_shifted_cost = ((double)armor_cost - minimal_armor_cost) * reactor_cost_factor;
	}
	else
	{
		primary_item_cost = armor_reactor_modified_cost;
		secondary_item_shifted_cost = ((double)weapon_cost - minimal_weapon_cost) * (module ? fission_reactor_cost_factor : reactor_cost_factor);
	}

    // set minimal cost to reactor level (this is checked in some other places so we should do this here to avoid any conflicts)

    int minimal_cost = reactor_level;

    // calculate base unit cost without abilities

    double base_cost = (primary_item_cost + secondary_item_shifted_cost / 2) * chassis_cost / Chassis[CHS_INFANTRY].cost;

    // get abilities cost modifications

    int abilities_cost_factor = 0;
    int abilities_cost_addition = 0;

    for (int ability_id = 0; ability_id < 32; ability_id++)
    {
        if (((abilities >> ability_id) & 0x1) == 0x1)
        {
            int ability_cost = Ability[ability_id].cost;

            switch (ability_cost)
            {
            	// increased with weapon/armor ratio
			case -1:
				abilities_cost_factor += std::min(2, std::max(0, weapon_offence_value / armor_defense_value));
				break;

            	// increased with weapon
			case -2:
				abilities_cost_factor += weapon_offence_value;
				break;

            	// increased with armor
			case -3:
				abilities_cost_factor += armor_defense_value;
				break;

            	// increased with speed
			case -4:
				abilities_cost_factor += chassis_speed;
				break;

            	// increased with weapon + armor
			case -5:
				abilities_cost_factor += weapon_offence_value + armor_defense_value;
				break;

            	// increased with weapon + speed
			case -6:
				abilities_cost_factor += weapon_offence_value + chassis_speed;
				break;

            	// increased with armor + speed
			case -7:
				abilities_cost_factor += armor_defense_value + chassis_speed;
				break;

				// positive values
			default:

				// take ability cost factor bits: 0-3

				int ability_proportional_value = (ability_cost & 0xF);

				if (ability_proportional_value > 0)
				{
					abilities_cost_factor += ability_proportional_value;

				}

				// take ability cost addition bits: 4-7

				int ability_flat_value = ((ability_cost >> 4) & 0xF);

				if (ability_flat_value > 0)
				{
					abilities_cost_addition += ability_flat_value;

				}

            }

            // special case: cost increased for land units

            if ((Ability[ability_id].flags & AFLAG_COST_INC_LAND_UNIT) && Chassis[chassis_id].triad == TRIAD_LAND)
			{
				abilities_cost_factor += 1;
			}

        }

    }

    // calculate final cost

    int cost =
        std::max
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
                (1.0 + 0.25 * (double)abilities_cost_factor)
            )
            +
            // abilities flat cost
            abilities_cost_addition
        )
    ;

    return cost;

}

__cdecl int wtp_mod_upgrade_cost(int faction_id, int new_unit_id, int old_unit_id)
{
    // get old unit cost

    int old_unit_cost = Units[old_unit_id].cost;

    // get new unit cost

    int new_unit_cost = Units[new_unit_id].cost;

    // double new unit cost if not prototyped

    if ((Units[new_unit_id].unit_flags & 0x4) == 0)
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

        int nano_factory_base_id = SecretProjects[0x16];

        // get The Nano Factory faction id

        if (nano_factory_base_id >= 0)
        {
            if (Bases[nano_factory_base_id].faction_id == faction_id)
            {
                upgrade_cost /= 2;

            }

        }

    }

    return upgrade_cost;

}

/*
Multipurpose combat roll function.
Determines which side wins this round. Returns 1 for attacker, 0 for defender.
*/
__cdecl int combat_roll
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

/*
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

/*
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

    VEH attacker_vehicle = Vehicles[attacker_vehicle_offset / 0x34];
    VEH defender_vehicle = Vehicles[defender_vehicle_offset / 0x34];

    UNIT attacker_unit = Units[attacker_vehicle.unit_id];
    UNIT defender_unit = Units[defender_vehicle.unit_id];

    int attacker_power = attacker_unit.reactor_id * 0xA - attacker_vehicle.damage_taken;
    int defender_power = defender_unit.reactor_id * 0xA - defender_vehicle.damage_taken;

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

/*
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

    *qA = *q / conf.alternative_combat_mechanics_loss_divisor;
    *pA = 1 - *qA;
    *pD = *p / conf.alternative_combat_mechanics_loss_divisor;
    *qD = 1 - *pD;

}

/*
Standard combat mechanics.
Calculates attacker winning probability.
*/
double standard_combat_mechanics_calculate_attacker_winning_probability
(
    double p,
    int attacker_hp,
    int defender_hp
)
{
    debug("standard_combat_mechanics_calculate_attacker_winning_probability(p=%f, attacker_hp=%d, defender_hp=%d)\n",
        p,
        attacker_hp,
        defender_hp
    )
    ;

    // determine round probability

//    double p = (double)attacker_strength / ((double)attacker_strength + (double)defender_strength);
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

/*
Alternative combat mechanics.
Calculates attacker winning probability.
*/
double alternative_combat_mechanics_calculate_attacker_winning_probability
(
    double p,
    int attacker_hp,
    int defender_hp
)
{
    debug("alternative_combat_mechanics_calculate_attacker_winning_probability(p=%f, attacker_hp=%d, defender_hp=%d)\n",
		p,
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

//        double p = (double)attacker_strength / ((double)attacker_strength + (double)defender_strength);
        double q = 1 - p;

        // determine following rounds probabilities

        double qA = q / conf.alternative_combat_mechanics_loss_divisor;
        double pA = 1 - qA;
        double pD = p / conf.alternative_combat_mechanics_loss_divisor;
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

///*
//For sea squares pretend they are on the same continent as closest reachable coastal base
//if such base is also closer than any other sea base in the same sea.
//*/
//__cdecl int base_find3(int x, int y, int unknown_1, int region, int unknown_2, int unknown_3)
//{
//    debug
//    (
//        "base_find3(x=%d, y=%d, region=%d)\n",
//        x,
//        y,
//        region
//    )
//    ;
//
//    bool closest_reachable_base_found = false;
//    bool closest_reachable_base_is_coastal = false;
//    int closest_reachable_base_distance = 9999;
//    BASE *closest_reachable_base = NULL;
//    MAP *closest_reachable_base_square = NULL;
//
//    int saved_coastal_base_region = -1;
//
//    // check if given square is at sea
//
//    if (region >= 0x41)
//    {
//        // loop through bases
//
//        for (int base_id = 0; base_id < *BaseCount; base_id++)
//        {
//            // get base
//
//            BASE *base = &Bases[base_id];
//
//            // get base map square
//
//            MAP *base_map_square = mapsq(base->x, base->y);
//
//            // get base map square body id
//
//            int base_map_square_region = base_map_square->region;
//
//            // check if it is the same body
//            if (base_map_square_region == region)
//            {
//                // calculate base distance
//
//                int base_distance = map_distance(x, y, base->x, base->y);
//
//                // improve distance if smaller
//
//                if (!closest_reachable_base_found || base_distance < closest_reachable_base_distance)
//                {
//                    closest_reachable_base_found = true;
//                    closest_reachable_base_is_coastal = false;
//                    closest_reachable_base_distance = base_distance;
//                    closest_reachable_base = base;
//                    closest_reachable_base_square = base_map_square;
//
//                }
//
//            }
//            // otherwise, check it it is a land base
//            else if (base_map_square_region < 0x41)
//            {
//                // iterate over nearby base squares
//
//                for (int dx = -2; dx <= 2; dx++)
//                {
//                    for (int dy = -(2 - abs(dx)); dy <= (2 - abs(dx)); dy += 2)
//                    {
//                        int near_x = base->x + dx;
//                        int near_y = base->y + dy;
//
//                        // exclude squares beyond the map
//
//                        if (near_y < 0 || near_y >= *MapAreaY)
//                            continue;
//
//                        if (*MapToggleFlat && (near_x < 0 || near_x >= *MapAreaX))
//                            continue;
//
//                        // wrap x on cylinder map
//
//                        if (!*MapToggleFlat && near_x < 0)
//                        {
//                            near_x += *MapAreaX;
//
//                        }
//
//                        if (!*MapToggleFlat && near_x >= *MapAreaX)
//                        {
//                            near_x -= *MapAreaX;
//
//                        }
//
//                        // get near map square
//
//                        MAP *near_map_square = mapsq(near_x, near_y);
//
//                        // get near map square body id
//
//                        int near_map_square_region = near_map_square->region;
//
//                        // check if it is a matching body id
//
//                        if (near_map_square_region == region)
//                        {
//                            // calculate base distance
//
//                            int base_distance = map_distance(x, y, base->x, base->y);
//
//                            // improve distance if smaller
//
//                            if (!closest_reachable_base_found || base_distance < closest_reachable_base_distance)
//                            {
//                                closest_reachable_base_found = true;
//                                closest_reachable_base_is_coastal = true;
//                                closest_reachable_base_distance = base_distance;
//                                closest_reachable_base = base;
//                                closest_reachable_base_square = base_map_square;
//
//                            }
//
//                        }
//
//                    }
//
//                }
//
//            }
//
//        }
//
//        debug
//        (
//            "closest_reachable_base_found=%d, closest_reachable_base_is_coastal=%d, closest_reachable_base_distance=%d\n",
//            closest_reachable_base_found,
//            closest_reachable_base_is_coastal,
//            closest_reachable_base_distance
//        )
//        ;
//
//        // if closest coastal base is found - pretend it is placed on same body
//
//        if (closest_reachable_base_found && closest_reachable_base_is_coastal)
//        {
//            saved_coastal_base_region = closest_reachable_base_square->region;
//
//            debug
//            (
//                "Before emulation: closest_reachable_base->x=%d, closest_reachable_base->y=%d, closest_reachable_base_square->region=%d\n",
//                closest_reachable_base->x,
//                closest_reachable_base->y,
//                closest_reachable_base_square->region
//            )
//            ;
//
//            closest_reachable_base_square->region = region;
//
//            debug
//            (
//                "After emulation: closest_reachable_base_square->region=%d\n",
//                closest_reachable_base_square->region
//            )
//            ;
//
//        }
//
//    }
//
//    // run original function
//
//    int nearest_base_id = tx_base_find3(x, y, unknown_1, region, unknown_2, unknown_3);
//
//    // revert coastal base map square body to original
//
//    if (closest_reachable_base_found && closest_reachable_base_is_coastal)
//    {
//        debug
//        (
//            "Before restoration: closest_reachable_base_square->region=%d\n",
//            closest_reachable_base_square->region
//        )
//        ;
//
//        closest_reachable_base_square->region = saved_coastal_base_region;
//
//        debug
//        (
//            "After restoration: closest_reachable_base_square->region=%d\n",
//            closest_reachable_base_square->region
//        )
//        ;
//
//    }
//
//    // return nearest base faction id
//
//    return nearest_base_id;
//
//}
//
/*
Calculates map distance as in vanilla.
*/
int map_distance(int x1, int y1, int x2, int y2)
{
    int xd = abs(x1-x2);
    int yd = abs(y1-y2);
    if (!*MapToggleFlat && xd > *MapAreaX/2)
        xd = *MapAreaX - xd;

    return ((xd + yd) + abs(xd - yd) / 2) / 2;

}

/*
Verifies two locations are within the base radius of each other.
*/
bool isWithinBaseRadius(int x1, int y1, int x2, int y2)
{
	return map_distance(x1, y1, x2, y2) <= 2;

}

/*
Rolls artillery damage.
*/
__cdecl int roll_artillery_damage(int attacker_strength, int defender_strength, int attacker_firepower)
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
        "artillery_damage=%d\n",
        artillery_damage
    )
    ;

    return artillery_damage;

}

/*
SE accumulated resource adjustment
*/
__cdecl int se_accumulated_resource_adjustment(int a1, int a2, int faction_id, int a4, int a5)
{
    // get faction

    Faction* faction = &Factions[faction_id];

    // get old ratings

    int SE_growth_pending_old = faction->SE_growth_pending;
    int SE_industry_pending_old = faction->SE_industry_pending;

    // execute original code

    // use modified function instead of original code
//	int returnValue = tx_set_se_on_dialog_close(a1, a2, faction_id, a4, a5);
    int returnValue = modifiedSocialCalc(a1, a2, faction_id, a4, a5);

    // get new ratings

    int SE_growth_pending_new = faction->SE_growth_pending;
    int SE_industry_pending_new = faction->SE_industry_pending;

    debug
    (
        "se_accumulated_resource_adjustment: faction_id=%d, SE_growth_pending: %+d -> %+d, SE_industry_pending: %+d -> %+d\n",
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

        for (int i = 0; i < *BaseCount; i++)
        {
            BASE* base = &Bases[i];

            if (base->faction_id == faction_id)
            {
                base->nutrients_accumulated = (int)round((double)base->nutrients_accumulated * conversion_ratio);
            }

        }

    }

    // adjust minerals

    if (SE_industry_pending_new != SE_industry_pending_old)
    {
        int row_size_old = Rules->nutrient_cost_multi - SE_industry_pending_old;
        int row_size_new = Rules->nutrient_cost_multi - SE_industry_pending_new;

        double conversion_ratio = (double)row_size_new / (double)row_size_old;

        for (int i = 0; i < *BaseCount; i++)
        {
            BASE* base = &Bases[i];

            if (base->faction_id == faction_id)
            {
                base->minerals_accumulated = (int)round((double)base->minerals_accumulated * conversion_ratio);
                base->minerals_accumulated_2 = (int)round((double)base->minerals_accumulated_2 * conversion_ratio);
            }

        }

    }

    return returnValue;

}

/*
hex cost
*/
__cdecl int wtp_mod_hex_cost(int unit_id, int faction_id, int from_x, int from_y, int to_x, int to_y, int speed1)
{
    // execute original/Thinker code

    int value = mod_hex_cost(unit_id, faction_id, from_x, from_y, to_x, to_y, speed1);

    // [vanilla bug] coordinates sometimes computed incorrectly
    // this is an attempt to make program at least not fall

    if (((from_x ^ from_y) & 1) == 1)
	{
		from_x ^= 1;
	}

    if (((to_x ^ to_y) & 1) == 1)
	{
		to_x ^= 1;
	}

    // correct road/tube movement rate

    if (conf.magtube_movement_rate > 0)
    {
        MAP* square_from = mapsq(from_x, from_y);
        MAP* square_to = mapsq(to_x, to_y);

        if (square_from && square_to)
        {
            // set movement cost along tube to 1
            // base implies tube in its tile
            if
            (
                (square_from->items & (BIT_BASE_IN_TILE | BIT_MAGTUBE))
                &&
                (square_to->items & (BIT_BASE_IN_TILE | BIT_MAGTUBE))
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

    // right of passage agreement

    if (conf.right_of_passage_agreement > 0)
    {
        MAP* square_from = mapsq(from_x, from_y);
        MAP* square_to = mapsq(to_x, to_y);

        if (square_from && square_to)
        {
        	// check if we don't have right of passage

        	if
			(
				(
					square_from->owner != -1 && square_from->owner != faction_id
					&&
					(
						(conf.right_of_passage_agreement == 1 && isVendetta(faction_id, square_from->owner))
						||
						(conf.right_of_passage_agreement == 2 && !has_pact(faction_id, square_from->owner))
					)
				)
				||
				(
					square_to->owner != -1 && square_to->owner != faction_id
					&&
					(
						(conf.right_of_passage_agreement == 1 && isVendetta(faction_id, square_to->owner))
						||
						(conf.right_of_passage_agreement == 2 && !has_pact(faction_id, square_to->owner))
					)
				)
			)
			{
				// get road/tube improvements

				int squareFromRoadTube = square_from->items & (BIT_ROAD | BIT_MAGTUBE);
				int squareToRoadTube = square_to->items & (BIT_ROAD | BIT_MAGTUBE);

				// at least one tile has road/tube

				if (squareFromRoadTube != 0 || squareToRoadTube != 0)
				{
					// remove road/tube from the map

					square_from->items &= ~(BIT_ROAD | BIT_MAGTUBE);
					square_to->items &= ~(BIT_ROAD | BIT_MAGTUBE);

					// recalculate value without road/tube

					value = hex_cost(unit_id, faction_id, from_x, from_y, to_x, to_y, speed1);

					// restore road/tube to the map

					square_from->items |= squareFromRoadTube;
					square_to->items |= squareToRoadTube;

				}

			}

        }

    }

    return value;

}

/*
Calculates tech level recursively.
*/
int wtp_tech_level(int id)
{
    if (id < 0 || id > TECH_TranT)
    {
        return 0;
    }
    else
    {
        int v1 = wtp_tech_level(Tech[id].preq_tech1);
        int v2 = wtp_tech_level(Tech[id].preq_tech2);

        return std::max(v1, v2) + 1;

    }

}

/*
Calculates tech cost.
cost grow accelerated from the beginning then linear.
a1														// constant (first tech cost)
b1														// linear coefficient
c1														// quadratic coefficient
d1														// qubic coefficient
b2 = ? * <map size>										// linear slope
x0 = (-c1 + sqrt(c1^2 - 3 * d1 * (b1 - b2))) / (3 * d1)	// break point
a2 = a1 + b1 * x0 + c1 * x0^2 + d1 * x0^3 - b2 * x0		// linear intercept
x  = (<level> - 1)

correction = (number of tech discovered by anybody / total tech count) / (turn / 350)

cost = (x < x0 ? a1 + b1 * x + c1 * x^2 + d1 * x^3 : a2 + b2 * x) * scale * correction

*/
int wtp_tech_cost(int fac, int tech)
{
    assert(fac >= 0 && fac < 8);
    MFaction* m = &MFactions[fac];
    int level = 1;

    if (tech >= 0)
	{
        level = wtp_tech_level(tech);
    }

    double a1 =  10.0;
    double b1 =   0.0;
    double c1 =  14.7;
    double d1 =  10.3;
    double b2 = 840.0 * ((double)*MapAreaTiles / 3200.0);
    double x0 = (-c1 + sqrt(c1 * c1 - 3 * d1 * (b1 - b2))) / (3 * d1);
    double a2 = a1 + b1 * x0 + c1 * x0 * x0 + d1 * x0 * x0 * x0 - b2 * x0;
    double x = (double)(level - 1);

    double base = (x < x0 ? a1 + b1 * x + c1 * x * x + d1 * x * x * x : a2 + b2 * x);

    double cost =
        base
        * (double)m->rule_techcost / 100.0
        * (*GameRules & RULES_TECH_STAGNATION ? 1.5 : 1.0)
        * (double)Rules->rules_tech_discovery_rate / 100.0
    ;

    double dw;

    if (is_human(fac))
	{
        dw = 1.0 + 0.1 * (10.0 - CostRatios[*DiffLevel]);
    }
    else
    {
        dw = 1.0;
    }

    cost *= dw;

    debug
    (
		"tech_cost %d %d | %8.4f %8.4f %8.4f %d %d %s\n",
		*CurrentTurn,
		fac,
        base,
        dw,
        cost,
        level,
        tech,
        (tech >= 0 ? Tech[tech].name : NULL)
	);

	// scale

	cost *= conf.tech_cost_scale;

	// correction

	if (level >= 3 && *CurrentTurn >= 25)
	{
		int totalTechCount = 0;
		int discoveredTechCount = 0;

		for (int otherTechId = TECH_Biogen; otherTechId <= TECH_BFG9000; otherTechId++)
		{
			CTech *otherTech = &(Tech[otherTechId]);

			if (otherTech->preq_tech1 == TECH_Disable)
				continue;

			totalTechCount++;

			if (TechOwners[otherTechId] != 0)
			{
				discoveredTechCount++;
			}

		}

		double correction = ((double)discoveredTechCount / (double)totalTechCount) / ((double)*CurrentTurn / 350.0);
		debug("tech_cost correction: discoveredTechCount = %d, totalTechCount = %d, current_turn = %d, end_turn = %d, correction = %f\n", discoveredTechCount, totalTechCount, *CurrentTurn, 350, correction);

		cost *= correction;

	}

    return std::max(2, (int)cost);

}

__cdecl int sayBase(char *buffer, int id)
{
	// execute original code

	int returnValue = tx_say_base(buffer, id);

	// get base

	BASE *base = &(Bases[id]);

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

bool isBaseFacilityBuilt(int baseId, int facilityId)
{
	return (Bases[baseId].facilities_built[facilityId / 8]) & (1 << (facilityId % 8));
}

bool isBaseFacilityBuilt(BASE *base, int facilityId)
{
	return (base->facilities_built[facilityId / 8]) & (1 << (facilityId % 8));
}

int getBasePopulationLimit(BASE *base)
{
    int pop_rule = MFactions[base->faction_id].rule_population;
    int hab_complex_limit = Rules->pop_limit_wo_hab_complex - pop_rule;
    int hab_dome_limit = Rules->pop_limit_wo_hab_dome - pop_rule;

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

/*
Return next unbuilt growth facility or -1 if there is no next available facility.
*/
int getnextAvailableGrowthFacility(BASE *base)
{
	if (!isBaseFacilityBuilt(base, FAC_HAB_COMPLEX))
	{
		return (has_tech(base->faction_id, Facility[FAC_HAB_COMPLEX].preq_tech) ? FAC_HAB_COMPLEX : -1);
	}
	else if (!isBaseFacilityBuilt(base, FAC_HABITATION_DOME))
	{
		return (has_tech(base->faction_id, Facility[FAC_HABITATION_DOME].preq_tech) ? FAC_HABITATION_DOME : -1);
	}
	else
	{
		return -1;
	}

}

/*
Modifies base init computation.
*/
__cdecl int baseInit(int factionId, int x, int y)
{
	// execute original code

	int baseId = tx_base_init(factionId, x, y);

	// return immediatelly if base wasn't created

	if (baseId < 0)
		return baseId;

	// get base

	BASE *base = getBase(baseId);

	// set base founding turn

	base->pad_1 = *CurrentTurn;

	// The Planetary Transit System fix
	// new base size is limited by average faction base size

	if (isProjectBuilt(factionId, FAC_PLANETARY_TRANSIT_SYSTEM) && base->pop_size == 3)
	{
		// calculate average faction base size

		int totalFactionBaseCount = 0;
		int totalFactionPopulation = 0;

		for (int otherBaseId = 0; otherBaseId < *BaseCount; otherBaseId++)
		{
			BASE *otherBase = &(Bases[otherBaseId]);

			// skip this base

			if (otherBaseId == baseId)
				continue;

			// only own bases

			if (otherBase->faction_id != base->faction_id)
				continue;

			totalFactionBaseCount++;
			totalFactionPopulation += otherBase->pop_size;

		}

		double averageFactionBaseSize = (double)totalFactionPopulation / (double)totalFactionBaseCount;

		// calculate new base size

		int newBaseSize = std::max(1, std::min(3, (int)floor(averageFactionBaseSize) - conf.pts_new_base_size_less_average));

		// set base size

		base->pop_size = newBaseSize;

	}

	// return base id

	return baseId;

}

/*
Wraps display help ability functionality.
This expands single number packed ability value (proportional + flat) into human readable text.
*/
__cdecl char *getAbilityCostText(int cost, char *destination, int radix)
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

/*
Wraps social_calc.
This is initially for CV GROWTH effect.
*/
__cdecl int modifiedSocialCalc(int seSelectionsPointer, int seRatingsPointer, int factionId, int ignored4, int seChoiceEffectOnly)
{
	// execute original code

	int value = tx_social_calc(seSelectionsPointer, seRatingsPointer, factionId, ignored4, seChoiceEffectOnly);

	// ignore native faction

	if (factionId == 0)
		return value;

	// CV changes GROWTH rate if applicable

	if (conf.cloning_vats_se_growth != 0)
	{
		if (isProjectBuilt(factionId, FAC_CLONING_VATS))
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

/*
Displays base nutrient cost factor in nutrient header.
*/
__cdecl void displayBaseNutrientCostFactor(int destinationStringPointer, int sourceStringPointer)
{
    // call original function

    tx_strcat(destinationStringPointer, sourceStringPointer);

	// get current variables

	int baseid = *CurrentBaseID;
	BASE *base = *CurrentBase;
	Faction *faction = &(Factions[base->faction_id]);

	// get current base nutrient cost factor

	int nutrientCostFactor = cost_factor(base->faction_id, 0, baseid);

    // modify output

    char *destinationString = (char *)destinationStringPointer;
	sprintf(destinationString + strlen("NUT"), " (%+1d) %+1d => %02d", faction->SE_growth_pending, *current_base_growth_rate, nutrientCostFactor);

}

/*
Replaces growth turn indicator with correct information for pop boom and stagnation.
*/
__cdecl void correctGrowthTurnsIndicator(int destinationStringPointer, int sourceStringPointer)
{
    // call original function

    tx_strcat(destinationStringPointer, sourceStringPointer);

    if (*current_base_growth_rate > conf.se_growth_rating_max)
	{
		// update indicator for population boom

		strcpy((char *)destinationStringPointer, "-- POP BOOM --");

	}
    else if (*current_base_growth_rate < conf.se_growth_rating_min)
	{
		// update indicator for stagnation

		strcpy((char *)destinationStringPointer, "-- STAGNATION --");

	}

}

/*
Modifies base mineral computation.
*/
__cdecl int modifiedBaseMinerals(int facilityId, int baseId, int queueSlotId)
{
	BASE *base = getBase(baseId);

	if (conf.recycling_tanks_mineral_multiplier)
	{
		// Recycling Tanks increases minerals by 50%.
		// Pressure Dome now do not automatically implies Recycling Tanks so it does not deliver any RT benefits.

		if (isBaseHasFacility(baseId, FAC_RECYCLING_TANKS) && base->mineral_intake >= 1)
		{
			// find mineral multiplier

			int multiplier = divideIntegerRoundUp(2 * base->mineral_intake_2, base->mineral_intake);

			// increment multiplier

			multiplier++;

			// calculate updated value and addition

			int updatedMineralIntake2 = (base->mineral_intake * multiplier) / 2;
			int additionalMinerals = updatedMineralIntake2 - base->mineral_intake_2;

			// update values

			base->mineral_intake_2 += additionalMinerals;
			base->mineral_surplus += additionalMinerals;

		}

	}

	// execute original function

	int value = tx_has_fac(facilityId, baseId, queueSlotId);

	// return original value

	return value;

}

/*
Computes modified inefficiency based on given energy intake.

inefficiency = energy intake - efficiency; a complementary metric to inefficiency

efficiency = SE EFFICIENCY effect + HQ effect; capped at energy intake

SE EFFICIENCY effect = 1/8 * SE EFFICIENCY rating

HQ effect = (1 - <distance to HQ> / <1.5 * 1/4 of map width>) * energy intake; non negative
no effect if there is no HQ

*/
__cdecl int modifiedInefficiency(int energyIntake)
{
	// execute original code

	int inefficiency = tx_black_market(energyIntake);

	// modify formula

	if (conf.alternative_inefficiency)
	{
		// accumulate efficiency from all effects

		double efficiencyRate = 0.0;

		// get current base

		int id = *CurrentBaseID;
		BASE *base = *CurrentBase;

		// get current base faction

		int factionId = base->faction_id;
		Faction *faction = &(Factions[factionId]);

		// get SE EFFICIENCY rating (with Children Creche effect)

		int seEfficiencyRating = std::max(-4, std::min(+4, faction->SE_effic_pending + (isBaseHasFacility(id, FAC_CHILDREN_CRECHE) ? 1 : 0)));

		// add SE EFFICIENCY effect

		efficiencyRate += (double)(4 + seEfficiencyRating) / 8.0;

		// get HQ base

		int hqBaseId = find_hq(factionId);

		if (hqBaseId != -1)
		{
			BASE *hqBase = &(Bases[hqBaseId]);

			// calculate distance to HQ

			int hqDistance = map_distance(base->x, base->y, hqBase->x, hqBase->y);

			// calculate HQ effect radius

			double hqEffectRadius = 1.5 * (double)*MapHalfX / 4.0;

			// calculate HQ effect

			if (hqDistance < hqEffectRadius)
			{
				efficiencyRate += 1.0 - (double)hqDistance / hqEffectRadius;

			}

		}

		// calculate efficiency

		int efficiency = std::max(0, std::min(energyIntake, (int)ceil(efficiencyRate * (double)energyIntake)));

		// update inefficiency

		inefficiency = energyIntake - efficiency;

	}

	return inefficiency;

}

int __cdecl wtp_mod_setup_player(int factionId, int a2, int a3)
{

    // original/Thinker

    mod_setup_player(factionId, a2, a3);

    // rerun balance for late starting faction

    if (a2 == -282)
    {
        // set balanceFactionId

        balanceFactionId = factionId;

        // run balance

        tx_balance();

        // clear balanceFactionId

        balanceFactionId = -1;

    }

    // reset territory
    // vanilla does not do it on player setup

    if (factionId > 0 || !is_human(factionId))
    {
		modifiedResetTerritory();
    }

    return 0;

}

/*
Wraps veh_init in balance to correctly generate number of colonies at start.
*/
__cdecl void modifiedVehInitInBalance(int unitId, int factionId, int x, int y)
{
	// do nothing for not target factions

	if (balanceFactionId != -1 && factionId != balanceFactionId)
		return;

	// execute original code

	veh_init(unitId, factionId, x, y);

}

void createFreeVehicles(int factionId)
{
	// find first faction vehicle location

	VEH *factionVehicle = NULL;

	for (int id = 0; id < *VehCount; id++)
	{
		VEH* vehicle = &(Vehicles[id]);

		if (vehicle->faction_id != factionId)
			continue;

		factionVehicle = vehicle;

		break;

	}

	// faction vehicle is not found

	if (factionVehicle == NULL)
		return;

	// get spawn location

	MAP *tile = getMapTile(factionVehicle->x, factionVehicle->y);

	// create free vehicles

	int colonyCount = is_human(factionId) ? conf.player_colony_pods : conf.computer_colony_pods;
	int colony = (is_ocean(tile) ? BSC_SEA_ESCAPE_POD : BSC_COLONY_POD);
	for (int j = 0; j < colonyCount; j++)
	{
		mod_veh_init(colony, factionId, factionVehicle->x, factionVehicle->y);
	}

	int formerCount = is_human(factionId) ? conf.player_formers : conf.computer_formers;
	int former = (is_ocean(tile) ? BSC_SEA_FORMERS : BSC_FORMERS);
	for (int j = 0; j < formerCount; j++)
	{
		mod_veh_init(former, factionId, factionVehicle->x, factionVehicle->y);
	}

}

int getLandUnitSpeedOnRoads(int unitId)
{
	UNIT *unit = getUnit(unitId);
	int chassisSpeed = unit->speed();

	int speed;

	if (conf.magtube_movement_rate > 0)
	{
		speed = chassisSpeed * Rules->move_rate_roads / conf.road_movement_cost;
	}
	else
	{
		speed = chassisSpeed * Rules->move_rate_roads;
	}

	return speed;

}

int getLandUnitSpeedOnTubes(int unitId)
{
	UNIT *unit = getUnit(unitId);
	int chassisSpeed = unit->speed();

	int speed;

	if (conf.magtube_movement_rate > 0)
	{
		speed = chassisSpeed * Rules->move_rate_roads;
	}
	else
	{
		speed = 1000;
	}

	return speed;

}

int getLandVehicleSpeedOnRoads(int id)
{
	int landVehicleSpeedOnRoads;

	if (conf.magtube_movement_rate > 0)
	{
		landVehicleSpeedOnRoads = getVehicleSpeed(id) / conf.road_movement_cost;
	}
	else
	{
		landVehicleSpeedOnRoads = getVehicleSpeed(id);
	}

	return landVehicleSpeedOnRoads;

}

int getLandVehicleSpeedOnTubes(int id)
{
	int landVehicleSpeedOnTubes;

	if (conf.magtube_movement_rate > 0)
	{
		landVehicleSpeedOnTubes = getVehicleSpeed(id);
	}
	else
	{
		landVehicleSpeedOnTubes = 1000;
	}

	return landVehicleSpeedOnTubes;

}

/*
Overrides world_build.
*/
__cdecl void modifiedWorldBuild()
{
	// execute originial function

	tx_world_build();

	// modify ocean depth

	if (conf.ocean_depth_multiplier != 1.0)
	{
		debug("modify ocean depth %d\n", *MapAreaTiles);

		for (int i = 0; i < *MapAreaTiles; i++)
		{
			MAP *tile = &((*MapTiles)[i]);

			// process only ocean

			if (!is_ocean(tile))
				continue;

			// calculate level and altitude

			int oldLevel = tile->climate & 0xE0;
			int oldLevelIndex = oldLevel >> 5;
			int oldAltitude = tile->contour;
			int oldAltitudeM = 50 * (tile->contour - 60);
			int oldAltitudeLevelIndex = oldAltitude / 20;

			// modify altitude and level

			int newAltitudeM = (int)floor(conf.ocean_depth_multiplier * (double)oldAltitudeM);
			int newAltitude = newAltitudeM / 50 + 60;
			int newAltitudeLevelIndex = newAltitude / 20;
			int newLevelIndex = newAltitudeLevelIndex;
			int newLevel = newLevelIndex << 5;

			// update map

			tile->climate = (tile->climate & ~0xE0) | newLevel;
			tile->contour = newAltitude;

			debug("[%4d] %d, %3d, %5d -> %d, %3d, %5d\n", i, oldLevelIndex, oldAltitude, oldAltitudeM, newLevelIndex, newAltitude, newAltitudeM);

			if (oldAltitudeLevelIndex != oldLevelIndex)
			{
				debug("\toldAltitudeLevelIndex and oldLevelIndex do not match.\n");
			}

		}

		debug("\n");

	}

}

/*
Calculates summary cost of all not prototyped components.
*/
int calculateNotPrototypedComponentsCost(int chassisId, int weaponId, int armorId, int chassisPrototyped, int weaponPrototyped, int armorPrototyped)
{
	// summarize all non prototyped component costs

	int notPrototypedComponentsCost = 0;

	if (!chassisPrototyped)
	{
		notPrototypedComponentsCost += Chassis[chassisId].cost;
	}

	if (!weaponPrototyped)
	{
		notPrototypedComponentsCost += Weapon[weaponId].cost;
	}

	if (!armorPrototyped)
	{
		notPrototypedComponentsCost += Armor[armorId].cost;
	}

	return notPrototypedComponentsCost;

}

/*
Calculates not prototyped components cost.
*/
__cdecl int calculateNotPrototypedComponentsCostForProduction(int unitId)
{
	UNIT *unit = &(Units[unitId]);

	// predefined units are all prototyped

	if (unitId < 64)
		return 0;

	// unit without name does not exist yet - impossible to get specs

	if (strlen(unit->name) == 0)
		return 0;

	// prototyped units do not have extra prototype cost

	if (unit->unit_flags & UNIT_PROTOTYPED)
		return 0;

	// get faction ID

	int factionId = unitId / 64;

	// check all prototyped components

	bool chassisPrototyped = false;
	bool weaponPrototyped = false;
	bool armorPrototyped = false;

	for (int factionPrototypedUnitId : getFactionUnitIds(factionId, false, false))
	{
		UNIT *factionPrototypedUnit = &(Units[factionPrototypedUnitId]);

		if (unit->chassis_id == factionPrototypedUnit->chassis_id || Chassis[unit->chassis_id].speed == Chassis[factionPrototypedUnit->chassis_id].speed)
		{
			chassisPrototyped = true;
		}

		if (unit->weapon_id == factionPrototypedUnit->weapon_id || Weapon[unit->weapon_id].offense_value == Weapon[factionPrototypedUnit->weapon_id].offense_value)
		{
			weaponPrototyped = true;
		}

		if (unit->armor_id == factionPrototypedUnit->armor_id || Armor[unit->armor_id].defense_value == Armor[factionPrototypedUnit->armor_id].defense_value)
		{
			armorPrototyped = true;
		}

	}

//	debug("chassisPrototyped=%d, weaponPrototyped=%d, armorPrototyped=%d\n", chassisPrototyped, weaponPrototyped, armorPrototyped);
//
	// calculate not prototyped components cost

	return calculateNotPrototypedComponentsCost(unit->chassis_id, unit->weapon_id, unit->armor_id, chassisPrototyped, weaponPrototyped, armorPrototyped);

}

/*
Calculates extra prototype cost for design workshop INSTEAD of calculating base unit cost.
The prototype cost is calculated without regard to faction bonuses.
*/
__cdecl int calculateNotPrototypedComponentsCostForDesign(int chassisId, int weaponId, int armorId, int chassisPrototyped, int weaponPrototyped, int armorPrototyped)
{
	// calculate modified extra prototype cost

	return calculateNotPrototypedComponentsCost(chassisId, weaponId, armorId, chassisPrototyped, weaponPrototyped, armorPrototyped);

}

__cdecl int getActiveFactionMineralCostFactor()
{
	return cost_factor(*CurrentFaction, 1, -1);
}

__cdecl void displayArtifactMineralContributionInformation(int input_string_pointer, int output_string_pointer)
{
	// execute original function

	tx_parse_string(input_string_pointer, output_string_pointer);

	// calculate artifact mineral contribution

	int artifactMineralContribution = Units[BSC_ALIEN_ARTIFACT].cost * cost_factor(*CurrentFaction, 1, -1) / 2;

	// get output string pointer

	char *output = (char *)output_string_pointer;

	// append information to string

	sprintf(output + strlen(output), "  %d minerals will be added to production", artifactMineralContribution);

}

/*
Calculates current base production mineral cost.
*/
__cdecl int getCurrentBaseProductionMineralCost()
{
	int baseId = *CurrentBaseID;
	BASE *base = *CurrentBase;

	// get current base production mineral cost

	int baseProductionMineralCost = getBaseMineralCost(baseId, base->queue_items[0]);

	// return value

	return baseProductionMineralCost;

}

__cdecl void displayHurryCostScaledToBasicMineralCostMultiplierInformation(int input_string_pointer, int output_string_pointer)
{
	// execute original function

	tx_parse_string(input_string_pointer, output_string_pointer);

	// get output string pointer

	char *output = (char *)output_string_pointer;

	// append information to string

	sprintf(output + strlen(output), " (hurry cost is scaled to basic mineral cost multiplier: %d)", Rules->mineral_cost_multi);

}

/*
Scales value to basic mineral cost multiplier.
*/
int scaleValueToBasicMinieralCostMultiplier(int factionId, int value)
{
	// get mineral cost multiplier

	int mineralCostMultiplier = Rules->mineral_cost_multi;

	// get mineral cost factor

	int mineralCostFactor = cost_factor(factionId, 1, -1);

	// scale value

	int scaledValue = (value * mineralCostMultiplier + mineralCostFactor / 2) / mineralCostFactor;

	// return value

	return scaledValue;

}

__cdecl void displayPartialHurryCostToCompleteNextTurnInformation(int input_string_pointer, int output_string_pointer)
{
	// execute original function

	tx_parse_string(input_string_pointer, output_string_pointer);

	// apply additional information message only when flat hurry cost is enabled

	if (!conf.flat_hurry_cost)
		return;

	// get output string pointer

	char *output = (char *)output_string_pointer;

	// append information to string

	sprintf(output + strlen(output), " (minimal hurry cost to complete production next turn = %d)", getPartialHurryCostToCompleteNextTurn());

}

/*
Calculates partial hurry cost to complete next turn.
Applied to the current base.
*/
int getPartialHurryCostToCompleteNextTurn()
{
	// get current base

	int baseId = *CurrentBaseID;
	BASE *base = *CurrentBase;

	// get minerals needed to hurry for next turn completion

	int mineralsToHurry = getBaseMineralCost(baseId, base->queue_items[0]) - base->minerals_accumulated - base->mineral_surplus;

	// calculate partial hurry cost

	return getPartialFlatHurryCost(baseId, mineralsToHurry);

}

/*
Adds pact condition to the spying check giving pact faction all benfits of the spying.
*/
__cdecl int modifiedSpyingForPactBaseProductionDisplay(int factionId)
{
	// execute vanilla code

	int spying = tx_spying(factionId);

	// check if there is a pact with faction

	bool pact = isDiploStatus(*CurrentPlayerFaction, factionId, DIPLO_PACT);

	// return value based on combined condition

	return ((spying != 0 || pact) ? 1 : 0);

}

void expireInfiltrations(int factionId)
{
	// check if feature is on

	if (!conf.infiltration_expire)
		return;

	debug("expireInfiltrations: %s\n", MFactions[factionId].noun_faction);

	// try to discover other faction infiltration devices

	for (int infiltratingFactionId = 1; infiltratingFactionId <= 7; infiltratingFactionId++)
	{
		// skip self

		if (infiltratingFactionId == factionId)
			continue;

		// skip computer factions

		if (!is_human(infiltratingFactionId))
			continue;

		// skip faction not spying on us

		if (!isDiploStatus(infiltratingFactionId, factionId, DIPLO_HAVE_INFILTRATOR))
			continue;

		debug("\t%s\n", MFactions[infiltratingFactionId].noun_faction);

		// get infiltration device count

		int infiltrationDeviceCount = getInfiltrationDeviceCount(infiltratingFactionId, factionId);

		debug("\t\tinfiltrationDeviceCount=%d\n", infiltrationDeviceCount);

		// retrieve lifetime formula parameters

		double lifetimeBase;
		double lifetimeProbeEffect;

		if (is_human(factionId))
		{
			lifetimeBase = conf.human_infiltration_device_lifetime_base;
			lifetimeProbeEffect = conf.human_infiltration_device_lifetime_probe_effect;
		}
		else
		{
			lifetimeBase = conf.computer_infiltration_device_lifetime_base;
			lifetimeProbeEffect = conf.computer_infiltration_device_lifetime_probe_effect;
		}

		// get our PROBE rating

		int probeRating = Factions[factionId].SE_probe_pending;
		debug("\t\tprobeRating=%d\n", probeRating);

		// calculate lifetime

		double lifetime = std::max(1.0, lifetimeBase + lifetimeProbeEffect * probeRating);
		debug("\t\tlifetime=%f\n", lifetime);

		// calculate probability

		double probability = std::min(1.0, 1.0 / lifetime);
		debug("\t\tprobability=%f\n", probability);

		// roll dice

		double probabilityRoll = random_double(1.0);
		debug("\t\tprobabilityRoll=%f\n", probabilityRoll);
		debug("\t\tdisabled=%d\n", (probabilityRoll < probability ? 1 : 0));

		if (probabilityRoll < probability)
		{
			// deactivate infiltration device

			infiltrationDeviceCount--;
			setInfiltrationDeviceCount(infiltratingFactionId, factionId, infiltrationDeviceCount);

			if (infiltrationDeviceCount == 0)
			{
				// deactivate infiltration if number of devices reaches zero

				setDiploStatus(infiltratingFactionId, factionId, DIPLO_HAVE_INFILTRATOR, false);

				// display info to humans

				if (is_human(factionId))
				{
					parse_says(0, MFactions[infiltratingFactionId].noun_faction, -1, -1);
					popp(scriptTxtID, "INFILTRATIONDEACTIVATEDENEMY", 0, "capture_sm.pcx", 0);
				}

				if (is_human(infiltratingFactionId))
				{
					parse_says(0, MFactions[factionId].noun_faction, -1, -1);
					popp(scriptTxtID, "INFILTRATIONDEACTIVATEDOURS", 0, "capture_sm.pcx", 0);
				}

			}
			else
			{
				// display info to humans

				if (is_human(factionId))
				{
					parse_says(0, MFactions[infiltratingFactionId].noun_faction, -1, -1);
					parse_num(1, infiltrationDeviceCount);
					popp(scriptTxtID, "INFILTRATIONDEVICEDISABLEDENEMY", 0, "capture_sm.pcx", 0);
				}

				if (is_human(infiltratingFactionId))
				{
					parse_says(0, MFactions[factionId].noun_faction, -1, -1);
					parse_num(1, infiltrationDeviceCount);
					popp(scriptTxtID, "INFILTRATIONDEVICEDISABLEDOURS", 0, "capture_sm.pcx", 0);
				}

			}

		}

	}

	debug("\n");

}

void setInfiltrationDeviceCount(int infiltratingFactionId, int infiltratedFactionId, int deviceCount)
{
	// device count is in range 0-0x7FFF

	deviceCount = std::min(0x7FFF, std::max(0x0, deviceCount));

	// clear infiltration devices count

	Factions[infiltratingFactionId].diplo_agenda[infiltratedFactionId] &= 0x0000FFFF;

	// set infiltration devices count

	Factions[infiltratingFactionId].diplo_agenda[infiltratedFactionId] |= (deviceCount << 16);

}

int getInfiltrationDeviceCount(int infiltratingFactionId, int infiltratedFactionId)
{
	return Factions[infiltratingFactionId].diplo_agenda[infiltratedFactionId] >> 16;

}

/*
Returns modified probe action risk.
*/
__cdecl void modifiedProbeActionRisk(int action, int riskPointer)
{
	// convert risk pointer

	int *risk = (int *)riskPointer;

	switch (action)
	{
		// genetic plague
	case 7:
		*risk = conf.probe_action_risk_genetic_plague;
		break;
	}

}

/*
Overrides Infiltrate Datalinks option text.
*/
__cdecl int modifiedInfiltrateDatalinksOptionTextGet()
{
	// execute original code

	int textPointer = tx_text_get();

	// apply only when infiltration expiration is enabled

	if (conf.infiltration_expire)
	{
		// get number of devices

		int infiltrationDeviceCount = getInfiltrationDeviceCount(*CurrentPlayerFaction, *g_PROBE_RFLAG_TARGET);

		// convert pointer to char *

		char *text = (char *)textPointer;

		// modify text

		sprintf(text + strlen(text), " (%d/%d devices planted)", infiltrationDeviceCount, conf.infiltration_devices);

	}

	return textPointer;

}

/*
Wraps set_treaty call for infiltration flag to also set number of devices.
*/
__cdecl void modifiedSetTreatyForInfiltrationExpiration(int initiatingFactionId, int targetFactionId, int diploState, int set_clear)
{
	// execute original code

	tx_set_treaty(initiatingFactionId, targetFactionId, diploState, set_clear);

	// plant devices

	setInfiltrationDeviceCount(initiatingFactionId, targetFactionId, conf.infiltration_devices);

}

/*
Substitutes vanilla hurry cost with flat hurry cost.
*/
__cdecl int modifiedHurryCost()
{
	// get current base

	int baseId = *CurrentBaseID;

	// calculate flat hurry cost

	return getFlatHurryCost(baseId);

}

/*
Calculates flat hurry cost.
Applies only when flat hurry cost is enabled.
*/
int getFlatHurryCost(int baseId)
{
	BASE *base = &(Bases[baseId]);

	// apply only when flat hurry cost is enabled

	if (!conf.flat_hurry_cost)
		return 0;

	// get hurry cost multiplier

	int hurryCostMultiplier;

	if (isBaseBuildingUnit(baseId))
	{
		hurryCostMultiplier = conf.flat_hurry_cost_multiplier_unit;
	}
	else if (isBaseBuildingFacility(baseId))
	{
		hurryCostMultiplier = conf.flat_hurry_cost_multiplier_facility;
	}
	else if (isBaseBuildingProject(baseId))
	{
		hurryCostMultiplier = conf.flat_hurry_cost_multiplier_project;
	}
	else
	{
		// should not happen
		hurryCostMultiplier = 1;
	}

	// get production remaining minerals

	int remainingMinerals = getRemainingMinerals(baseId);

	// calculate hurry cost

	int hurryCost = hurryCostMultiplier * remainingMinerals;

	// scale value to mineral cost multiplier if configured

	if (is_human(base->faction_id) && conf.fix_mineral_contribution)
	{
		hurryCost = scaleValueToBasicMinieralCostMultiplier(base->faction_id, hurryCost);
	}

	return hurryCost;

}

/*
Calculates partial flat hurry cost.
Applies only when flat hurry cost is enabled.
*/
int getPartialFlatHurryCost(int baseId, int minerals)
{
	// apply only when flat hurry cost is enabled

	if (!conf.flat_hurry_cost)
		return 0;

	// get remaining minerals

	int remainingMinerals = getRemainingMinerals(baseId);

	// nothing to hurry

	if (remainingMinerals == 0)
		return 0;

	// get hurry cost

	int hurryCost = getFlatHurryCost(baseId);

	// calculate partial flat hurry cost

	int partialHurryCost = (hurryCost * minerals + remainingMinerals - 1) / remainingMinerals;

	return partialHurryCost;

}

/*
Fixes bugs in best defender selection.
*/
__cdecl int modifiedBestDefender(int defenderVehicleId, int attackerVehicleId, int bombardment)
{
	int defenderTriad = veh_triad(defenderVehicleId);
	int attackerTriad = veh_triad(attackerVehicleId);

	// best defender

	int bestDefenderVehicleId = defenderVehicleId;

	// sea unit directly attacking sea unit except probe

	if
	(
		Units[Vehicles[attackerVehicleId].unit_id].weapon_id != WPN_PROBE_TEAM
		&&
		attackerTriad == TRIAD_SEA && defenderTriad == TRIAD_SEA && bombardment == 0
	)
	{
		// iterate the stack

		double bestDefenderEffectiveness = 0.0;

		for (int stackedVehicleId : getStackVehicles(defenderVehicleId))
		{
			VEH *stackedVehicle = &(Vehicles[stackedVehicleId]);
			UNIT *stackedVehicleUnit = &(Units[stackedVehicle->unit_id]);

			int attackerOffenseValue;
			int defenderStrength;
			wtp_mod_battle_compute(attackerVehicleId, stackedVehicleId, (int)&attackerOffenseValue, (int)&defenderStrength, 0);

			double defenderEffectiveness = ((double)defenderStrength / (double)attackerOffenseValue) * ((double)(stackedVehicleUnit->reactor_id * 10 - stackedVehicle->damage_taken) / (double)(stackedVehicleUnit->reactor_id * 10));

			if (bestDefenderVehicleId == -1 || defenderEffectiveness > bestDefenderEffectiveness)
			{
				bestDefenderVehicleId = stackedVehicleId;
				bestDefenderEffectiveness = defenderEffectiveness;
			}

		}

	}
	else
	{
		bestDefenderVehicleId = best_defender(defenderVehicleId, attackerVehicleId, bombardment);
	}

	// store variables for modified odds dialog unless bombardment

	if (bombardment)
	{
		currentAttackerVehicleId = -1;
		currentDefenderVehicleId = -1;
	}
	else
	{
		currentAttackerVehicleId = attackerVehicleId;
		currentDefenderVehicleId = bestDefenderVehicleId;
	}


	// return best defender

	return bestDefenderVehicleId;

}

/*
Fixes empty square bombardment and prevents subsequent artillery duel.
*/
__cdecl void modifiedVehSkipForActionDestroy(int vehicleId)
{
	// execute original code

	veh_skip(vehicleId);

	// clear global combat variable

	*g_UNK_ATTACK_FLAGS = 0x0;

}

__cdecl void appendAbilityCostTextInWorkshop(int output_string_pointer, int input_string_pointer)
{
	const char *COST_PREFIX = "Cost: ";

    debug
    (
        "appendAbilityCostTextInWorkshop:input(output_string=%s, input_string=%s)\n",
        (char *)output_string_pointer,
        (char *)input_string_pointer
    )
    ;

    // call original function

    tx_strcat(output_string_pointer, input_string_pointer);

    // get output string

    char *outputString = (char *)output_string_pointer;

    // find cost entry

    char *costEntry = strstr(outputString, COST_PREFIX);

    // not found

    if (costEntry == NULL)
		return;

	// extract number

	int cost = atoi(costEntry + strlen(COST_PREFIX));

	// generate explanaroty text

	int proportionalCost = (cost & 0xF);
	int flatCost = (cost >> 4);

    // add explanatory text

    sprintf(outputString + strlen(outputString), " | +%02d%%, +%2d rows", proportionalCost * 25, flatCost);
    debug("appendAbilityCostTextInWorkshop: %s\n", outputString);

}

/*
Request for break treaty before combat actions.
*/
__cdecl void modifiedBattleFight2(int attackerVehicleId, int angle, int tx, int ty, int do_arty, int flag1, int flag2)
{
	debug("modifiedBattleFight2(attackerVehicleId=%d, angle=%d, tx=%d, ty=%d, do_arty=%d, flag1=%d, flag2=%d)\n", attackerVehicleId, angle, tx, ty, do_arty, flag1, flag2);

	VEH *attackerVehicle = &(Vehicles[attackerVehicleId]);

	if (attackerVehicle->faction_id == *CurrentPlayerFaction)
	{
		debug("\tattackerVehicle->faction_id == *CurrentPlayerFaction\n");
		int defenderVehicleId = veh_at(tx, ty);

		if (defenderVehicleId >= 0)
		{
			bool nonArtifactUnitExists = false;

			std::vector<int> stackedVehicleIds = getStackVehicles(defenderVehicleId);

			for (int vehicleId : stackedVehicleIds)
			{
				if (Units[Vehicles[vehicleId].unit_id].weapon_id != WPN_ALIEN_ARTIFACT)
				{
					nonArtifactUnitExists = true;
					break;
				}
			}

			if (nonArtifactUnitExists)
			{
				VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);

				int treatyNotBroken = tx_break_treaty(attackerVehicle->faction_id, defenderVehicle->faction_id, 0xB);

				if (treatyNotBroken)
					return;

				// break treaty really

				tx_act_of_aggression(attackerVehicle->faction_id, defenderVehicle->faction_id);

			}

		}

	}

	// execute original code

	tx_battle_fight_2(attackerVehicleId, angle, tx, ty, do_arty, flag1, flag2);

}

/*
Calculates sprite offset in SocialWin__draw_social to display TALENT icon properly.
*/
__cdecl int modifiedSocialWinDrawSocialCalculateSpriteOffset(int spriteBaseIndex, int effectValue)
{
	// calculate sprite color index for negative/positive value

	int colorIndex = (effectValue >= 1 ? 2 : 1);

	// calculate sprite index

	int spriteIndex = spriteBaseIndex + colorIndex;

	// adjust sprite index based on effect

	switch (spriteBaseIndex / 9)
	{
		// before TALENT
	case 0:
	case 1:
	case 2:
		// no change
		break;
		// TALENT
	case 3:
		// 6 sprites left
		spriteIndex -= 6;
		break;
		// after TALENT
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
		// previous row
		spriteIndex -= 9;
		break;
	default:
		// should not happen
		return 0;
	}

	// calculate sprite offset

	int spriteOffset = spriteIndex * 0x2C;

	// calculate sprite address

	byte *sprite = g_SOCIALWIN + 0x2F20 + spriteOffset;

	// convert and return value

	return (int)sprite;

}

__cdecl void modified_tech_research(int factionId, int labs)
{
	Faction *faction = &(Factions[factionId]);

	// modify labs contribution from base

	if (conf.se_research_bonus_percentage != 10 && faction->SE_research_pending != 0)
	{
		int currentMultiplierPercentage = 100 + 10 * faction->SE_research_pending;
		int modifiedMultiplierPercentage = 100 + conf.se_research_bonus_percentage * faction->SE_research_pending;

		labs = (labs * modifiedMultiplierPercentage + currentMultiplierPercentage / 2) / currentMultiplierPercentage;

	}

	// pass labs to original function

	tx_tech_research(factionId, labs);

}

/*
This is a bitmask call within base_nutrient that has nothing to do with base GROWTH.
I just highjack it to modify base GROWTH.
*/
__cdecl void modifiedBaseGrowth(int a1, int a2, int a3)
{
	// execute original code

	tx_bitmask(a1, a2, a3);

	// get current base

	int baseId = *CurrentBaseID;

	// modify base GROWTH by habitation facilities

	*current_base_growth_rate += getHabitationFacilitiesBaseGrowthModifier(baseId);

}

/*
Replaces SE_growth_pending reads in cost_factor to introduce habitation facilities effect.
*/
__cdecl int modifiedNutrientCostFactorSEGrowth(int factionId, int baseId)
{
	// get faction

	Faction *faction = &(Factions[factionId]);

	// get faction GROWTH

	int baseGrowth = faction->SE_growth_pending;

	// update GROWTH if base id is set

	if (baseId >= 0)
	{
		// modify base GROWTH by habitation facilities

		baseGrowth += getHabitationFacilitiesBaseGrowthModifier(baseId);

	}

	return baseGrowth;

}

/*
Computes habitation facilties base GROWTH modifier.

for each present facility
	bonus at limit = 0
	less population => more bonus
	restricted by range [0, habitation_facility_present_growth_bonus_max]

for each missing facility
	penalty at limit = habitation_facility_absent_growth_penalty
	more population => more penalty
	restricted by range [-infinity, 0]
*/
int getHabitationFacilitiesBaseGrowthModifier(int baseId)
{
	// get base

	BASE *base = &(Bases[baseId]);

	// calculate habitation facilities base GROWTH modifier

	int baseGrowthModifier = 0;

	for (int i = 0; i < HABITATION_FACILITIES_COUNT; i++)
	{
		int habitationFacility = HABITATION_FACILITIES[i];

		bool habitationFacilityPresent = isBaseHasFacility(baseId, habitationFacility);
		int populationLimit = getFacilityPopulationLimit(base->faction_id, habitationFacility);

		if (habitationFacilityPresent)
		{
			baseGrowthModifier += std::min(conf.habitation_facility_present_growth_bonus_max, std::max(0, (populationLimit - base->pop_size)));
		}
		else
		{
			baseGrowthModifier += std::min(0, (populationLimit - base->pop_size) - conf.habitation_facility_absent_growth_penalty);
		}

	}

	return baseGrowthModifier;

}

/*
; When one former starts terraforming action all idle formers in same tile do too.
; May have unwanted consequenses of intercepting idle formers.
*/
__cdecl int modifiedActionTerraform(int vehicleId, int action, int execute)
{
	// execute action

	int returnValue = action_terraform(vehicleId, action, execute);

	// execute action for other idle formers in tile

	if (execute == 1)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int vehicleFactionId = vehicle->faction_id;
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		for (int otherVehicleId = 0; otherVehicleId < *VehCount; otherVehicleId++)
		{
			VEH *otherVehicle = &(Vehicles[otherVehicleId]);
			int otherVehicleFactionId = otherVehicle->faction_id;
			MAP *otherVehicleTile = getVehicleMapTile(otherVehicleId);

			// only ours

			if (otherVehicleFactionId != vehicleFactionId)
				continue;

			// only formers

			if (!isFormerVehicle(otherVehicleId))
				continue;

			// only same locaton

			if (otherVehicleTile != vehicleTile)
				continue;

			// only idle

			if (!isVehicleIdle(otherVehicleId))
				continue;

			// only idle

			if (!isVehicleIdle(otherVehicleId))
				continue;

			// set other vehicle terraforming action

			setTerraformingAction(otherVehicleId, action);

		}

	}

	return returnValue;

}

/*
Disables land artillery bombard from sea.
*/
__cdecl void modifiedActionMoveForArtillery(int vehicleId, int x, int y)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	// disable bombardment if vehicle is land unit at sea and not in a base

	if (is_ocean(vehicleTile) && veh_triad(vehicleId) == TRIAD_LAND && !map_base(vehicleTile))
		return;

	// execute action

	tx_action_move(vehicleId, x, y);

}

/*
Disables air transport unload everywhere.
Pretends that air transport has zero cargo capacity when in not appropriate unload location.
*/
__cdecl int modifiedVehicleCargoForAirTransportUnload(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	// disable air transport unload not in base

	if
	(
		vehicle->triad() == TRIAD_AIR
		&&
		isTransportVehicle(vehicle)
		&&
		!
		(
			map_has_item(vehicleTile, BIT_BASE_IN_TILE) || map_has_item(vehicleTile, BIT_AIRBASE)
		)
	)
		return 0;

	// return default value

	return tx_veh_cargo(vehicleId);

}

/*
Overrides faction_upkeep calls to amend vanilla and Thinker AI functionality.
*/
__cdecl int modifiedFactionUpkeep(const int factionId)
{
	debug("modifiedFactionUpkeep - %s\n", getMFaction(factionId)->noun_faction);fflush(debug_log);

	executionProfiles["0. modifiedFactionUpkeep"].start();

    // remove wrong units from bases

    if (factionId > 0)
	{
		removeWrongVehiclesFromBases();
	}

    // expire infiltrations for normal factions

    if (factionId > 0)
	{
		expireInfiltrations(factionId);
	}

	// choose AI logic

	int returnValue;

	if (isWtpEnabledFaction(factionId))
	{
		// run WTP AI code for AI eanbled factions

		executionProfiles["0.1. aiFactionUpkeep"].start();
		aiFactionUpkeep(factionId);
		executionProfiles["0.1. aiFactionUpkeep"].stop();

	}

	// execute original function

	returnValue = faction_upkeep(factionId);

    // fix vehicle home bases for normal factions

    if (factionId > 0)
	{
		fixVehicleHomeBases(factionId);
	}

	executionProfiles["0. modifiedFactionUpkeep"].stop();

	// check vehicles with available moves

	debug(">check moves - %s\n", getMFaction(factionId)->noun_faction);fflush(debug_log);
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);

		if (vehicle->faction_id != factionId)
			continue;

		if (vehicle->moves_spent >= getVehicleSpeed(vehicleId))
			continue;

		debug
		(
			">moves left:"
			" [%4d] (%3d,%3d)"
			" speed=%2d"
			" road_moves_spent=%2d"
			"\n"
			, vehicleId, vehicle->x, vehicle->y
			, getVehicleSpeed(vehicleId)
			, vehicle->moves_spent
		);
		fflush(debug_log);

	}

	// return original value

	return returnValue;

}

__cdecl void captureAttackerOdds(const int position, const int value)
{
	// capture attacker odds

	currentAttackerOdds = value;

	// execute original code

	parse_num(position, value);

}

__cdecl void captureDefenderOdds(const int position, const int value)
{
	// capture defender odds

	currentDefenderOdds = value;

	// execute original code

	parse_num(position, value);

}

__cdecl void modifiedDisplayOdds(const char* file_name, const char* label, int a3, const char* pcx_file_name, int a5)
{
	// fall into default vanilla code if global parameters are not set

	if (currentAttackerVehicleId == -1 || currentDefenderVehicleId == -1 || currentAttackerOdds == -1 || currentDefenderOdds == -1)
	{
		// execute original code

		popp(file_name, label, a3, pcx_file_name, a5);
	}
	else
	{
		if (conf.ignore_reactor_power_in_combat)
		{
			// get attacker and defender vehicles

			VEH *attackerVehicle = &(Vehicles[currentAttackerVehicleId]);
			VEH *defenderVehicle = &(Vehicles[currentDefenderVehicleId]);

			// get attacker and defender units

			UNIT *attackerUnit = &(Units[attackerVehicle->unit_id]);
			UNIT *defenderUnit = &(Units[defenderVehicle->unit_id]);

			// calculate attacker and defender power
			// artifact gets 1 HP regardless of reactor

			int attackerPower = (attackerUnit->plan == PLAN_ALIEN_ARTIFACT ? 1 : attackerUnit->reactor_id * 10 - attackerVehicle->damage_taken);
			int defenderPower = (defenderUnit->plan == PLAN_ALIEN_ARTIFACT ? 1 : defenderUnit->reactor_id * 10 - defenderVehicle->damage_taken);

			// calculate FP

			int attackerFP = defenderUnit->reactor_id;
			int defenderFP = attackerUnit->reactor_id;

			// calculate HP

			int attackerHP = (attackerPower + (defenderFP - 1)) / defenderFP;
			int defenderHP = (defenderPower + (attackerFP - 1)) / attackerFP;

			// compute vanilla odds

			int attackerOdds = currentAttackerOdds * attackerHP * defenderPower;
			int defenderOdds = currentDefenderOdds * defenderHP * attackerPower;
			simplifyOdds(&attackerOdds, &defenderOdds);

			// reparse vanilla odds into dialog

			parse_num(2, attackerOdds);
			parse_num(3, defenderOdds);

			// calculate round probabilty

			double attackerStrength = (double)attackerOdds / (double)attackerHP;
			double defenderStrength = (double)defenderOdds / (double)defenderHP;
			double p = attackerStrength / (attackerStrength + defenderStrength);

			// calculate attacker winning probability

			double attackerWinningProbability = calculateWinningProbability(p, attackerHP, defenderHP);

			// compute exact odds

			int attackerExactOdds;
			int defenderExactOdds;

			if (attackerWinningProbability >= 0.99)
			{
				attackerExactOdds = 100;
				defenderExactOdds = 1;
			}
			else if (attackerWinningProbability <= 0.01)
			{
				attackerExactOdds = 1;
				defenderExactOdds = 100;
			}
			else if (attackerWinningProbability >= 0.5)
			{
				attackerExactOdds = (int)round(attackerWinningProbability / (1.0 - attackerWinningProbability) * (double)defenderOdds);
				defenderExactOdds = defenderOdds;
			}
			else
			{
				attackerExactOdds = attackerOdds;
				defenderExactOdds = (int)round((1.0 - attackerWinningProbability) / attackerWinningProbability * (double)attackerOdds);
			}

			parse_num(0, attackerExactOdds);
			parse_num(1, defenderExactOdds);

			// convert to percentage

			int attackerWinningPercentage = (int)round(attackerWinningProbability * 100);

			// add value to parse

			parse_num(4, attackerWinningPercentage);

			// display modified odds dialog

			popp(file_name, "GOODIDEA_IGNORE_REACTOR", a3, pcx_file_name, a5);

		}
		else
		{
			// execute original code

			popp(file_name, label, a3, pcx_file_name, a5);

		}

	}

	// clear global variables

	currentAttackerVehicleId = -1;
	currentDefenderVehicleId = -1;
	currentAttackerOdds = -1;
	currentDefenderOdds = -1;

}

/*
Calculates winning probability.
*/
double calculateWinningProbability(double p, int attackerHP, int defenderHP)
{
    debug("calculateWinningProbability(p=%f, attackerHP=%d, defenderHP=%d)\n",
		p,
        attackerHP,
        defenderHP
    )
    ;

    // calculate attacker winning probability

    double attackerWinningProbability;

    if (attackerHP <= 0)
    {
        attackerWinningProbability = 0.0;
    }
    else if (defenderHP <= 0)
    {
        attackerWinningProbability = 1.0;
    }
    else
    {
        if (conf.alternative_combat_mechanics)
        {
            attackerWinningProbability =
                alternative_combat_mechanics_calculate_attacker_winning_probability
                (
                    p,
                    attackerHP,
                    defenderHP
                )
            ;
        }
        else
        {
            attackerWinningProbability =
                standard_combat_mechanics_calculate_attacker_winning_probability
                (
                    p,
                    attackerHP,
                    defenderHP
                )
            ;
        }

    }

	// return attacker winning probability

	return attackerWinningProbability;

}

void simplifyOdds(int *attackerOdds, int *defenderOdds)
{
	for (int divisor = 2; divisor < *attackerOdds && divisor < *defenderOdds; divisor++)
	{
		while (*attackerOdds % divisor == 0 && *defenderOdds % divisor == 0 && *attackerOdds > divisor && *defenderOdds > divisor)
		{
			*attackerOdds /= divisor;
			*defenderOdds /= divisor;
		}

	}

}

__cdecl int wtp_mod_tech_rate(int factionId)
{
	// original/Thinker
	
	int techCost = mod_tech_rate(factionId);
	
	// [WTP]
	
	Faction *faction = &(Factions[factionId]);
	MFaction *mfaction = &(MFactions[factionId]);
    
	// find technology to compute the rate for
    
	int techId = 0;
	
	if (faction->tech_research_id >= 0)
	{
		// use selected tech ID
		
		techId = faction->tech_research_id;
		
	}
	else
	{
		// search for lowest available tech instead
		
		techId = getLowestLevelAvilableTech(factionId);
		
	}
	
	// compute alternative tech cost
	
	techCost = std::max(2, wtp_tech_cost(factionId, techId));
	
	// set Thinker variables
	
	mfaction->thinker_tech_id = techId;
	mfaction->thinker_tech_cost = techCost;
	
	// return techCost
	
	return techCost;
	
}

__cdecl int modifiedTechPick(int factionId, int a2, int a3, int a4)
{
	Faction *faction = &(Factions[factionId]);

	// call original function

	int techId = tech_pick(factionId, a2, a3, a4);

	// check if techId is about to change

	if (techId != faction->tech_research_id)
	{
		// recalculate tech cost

		faction->tech_cost = wtp_tech_cost(factionId, techId);

	}

	// return techId

	return techId;

}

int getLowestLevelAvilableTech(int factionId)
{
	int lowestLevelTechId = -1;
	int minLevel = INT_MAX;

	for (int techId = 0; techId < MaxTechnologyNum; techId++)
	{
		// only faction available techs

		if (!isTechAvailable(factionId, techId))
			continue;

		// get level

		int level = wtp_tech_level(techId);

		// update min level

		if (level < minLevel)
		{
			lowestLevelTechId = techId;
			minLevel = level;
		}

	}

	return lowestLevelTechId;

}

bool isTechDiscovered(int factionId, int techId)
{
	return (TechOwners[techId] & (0x1 << factionId)) != 0;
}

bool isTechAvailable(int factionId, int techId)
{
	// check prerequisite tech 1 is discovered

	int preqTech1Id = Tech[techId].preq_tech1;

	if (preqTech1Id >= 0)
	{
		if (!isTechDiscovered(factionId, preqTech1Id))
			return false;
	}

	// check prerequisite tech 2 is discovered

	int preqTech2Id = Tech[techId].preq_tech2;

	if (preqTech2Id >= 0)
	{
		if (!isTechDiscovered(factionId, preqTech2Id))
			return false;
	}

	// no undiscovered prerequisites

	return true;

}

__cdecl void modifiedBattleReportItemNameDisplay(int destinationPointer, int sourcePointer)
{
	if (bomberAttacksInterceptor)
	{
		if (bomberAttacksInterceptorPass % 2 == 0)
		{
			// use armor for attacker

			sourcePointer = (int)Armor[Units[Vehicles[bomberAttacksInterceptorAttackerVehicleId].unit_id].armor_id].name;

		}

	}

	// execute original function

	tx_strcat(destinationPointer, sourcePointer);

}

__cdecl void modifiedBattleReportItemValueDisplay(int destinationPointer, int sourcePointer)
{
	if (bomberAttacksInterceptor)
	{
		if (bomberAttacksInterceptorPass % 2 == 0)
		{
			// use armor for attacker

			itoa(Armor[Units[Vehicles[bomberAttacksInterceptorAttackerVehicleId].unit_id].armor_id].defense_value, (char *)sourcePointer, 10);

		}

		// increment pass number

		bomberAttacksInterceptorPass++;

	}

	// execute original function

	tx_strcat(destinationPointer, sourcePointer);

}

/*
Destroys all improvement on lost territory.
*/
__cdecl void modifiedResetTerritory()
{
	// build all destroyable terrain improvements

	uint32_t destroyableImprovments =
		Terraform[FORMER_FARM].bit |
		Terraform[FORMER_SOIL_ENR].bit |
		Terraform[FORMER_MINE].bit |
		Terraform[FORMER_SOLAR].bit |
		Terraform[FORMER_FOREST].bit |
		Terraform[FORMER_ROAD].bit |
		Terraform[FORMER_MAGTUBE].bit |
		Terraform[FORMER_BUNKER].bit |
		Terraform[FORMER_AIRBASE].bit |
		Terraform[FORMER_SENSOR].bit |
		Terraform[FORMER_CONDENSER].bit |
		Terraform[FORMER_ECH_MIRROR].bit |
		Terraform[FORMER_THERMAL_BORE].bit
	;

	// record exising map owners

	int *oldMapOwners = new int[*MapAreaTiles];

	for (int mapIndex = 0; mapIndex < *MapAreaTiles; mapIndex++)
	{
		MAP *tile = &((*MapTiles)[mapIndex]);

		oldMapOwners[mapIndex] = tile->owner;

	}

	// execute original function

	reset_territory();

	// destroy improvements on lost territory

	for (int mapIndex = 0; mapIndex < *MapAreaTiles; mapIndex++)
	{
		MAP *tile = &((*MapTiles)[mapIndex]);

		int oldMapOwner = oldMapOwners[mapIndex];
		int newMapOwner = tile->owner;

		if (oldMapOwner > 0 && newMapOwner != oldMapOwner)
		{
			tile->items &= ~destroyableImprovments;
		}

	}

	// delete allocated array

	delete[] oldMapOwners;

}

/*
Returns modified limited orbital yield based on configured limit.
*/
__cdecl int modifiedOrbitalYieldLimit()
{
	int limit = (int)floor(conf.orbital_yield_limit * (double)(*CurrentBase)->pop_size);

	return limit;

}

/*
Modifies bitmask to invoke vendetta dialog if there commlink.
*/
__cdecl int modifiedBreakTreaty(int actingFactionId, int targetFactionId, int bitmask)
{
	// include commlink mask

	bitmask |= DIPLO_COMMLINK;

	// call original function

	return break_treaty(actingFactionId, targetFactionId, bitmask);

}

/*
Returns 0 on base production turn to disable retooling penalty after production.
*/
int __cdecl modifiedBaseMaking(int item, int baseId)
{
	BASE *base = &(Bases[baseId]);
	int currentItem = base->queue_items[0];

	// do not penalize retooling on first turn

	if (base->state_flags & BSTATE_PRODUCTION_DONE)
	{
		return 0;
	}

	// do not penalize retooling from already built facility

	if (currentItem < 0 && -currentItem < SP_ID_First && isBaseHasFacility(baseId, -currentItem))
	{
		return 0;
	}

	// do not penalize retooling from already built project

	if (currentItem < 0 && -currentItem >= SP_ID_First && SecretProjects[-currentItem - SP_ID_First] >= 0)
	{
		return 0;
	}

	// run original code in all other cases

	return base_making(item, baseId);

}

/*
Computes alternative MC cost.
Adds base and adjacent vehicle costs to mind control cost.
*/
int __cdecl modifiedMindControlCost(int baseId, int probeFactionId, int cornerMarket)
{
	BASE *base = &(Bases[baseId]);
	Faction *baseFaction = &(Factions[base->faction_id]);

	// if HQ and not corner market - return negative value

	if (isBaseFacilityBuilt(baseId, FAC_HEADQUARTERS) && cornerMarket == 0)
		return -1;

	// calculate MC cost

	double mindControlCost = 0.0;

	// count surplus

	mindControlCost += conf.alternative_mind_control_nutrient_cost * (double)base->nutrient_surplus;
	mindControlCost += conf.alternative_mind_control_mineral_cost * (double)base->mineral_surplus;
	mindControlCost += conf.alternative_mind_control_energy_cost * (double)base->economy_total;
	mindControlCost += conf.alternative_mind_control_energy_cost * (double)base->psych_total;
	mindControlCost += conf.alternative_mind_control_energy_cost * (double)base->labs_total;

	// count built facilities

	for (int facilityId = FAC_HEADQUARTERS; facilityId <= FAC_GEOSYNC_SURVEY_POD; facilityId++)
	{
		if (isBaseFacilityBuilt(baseId, facilityId))
		{
			mindControlCost += conf.alternative_mind_control_facility_cost_multiplier * (double)Facility[facilityId].cost;
		}
	}

	// count projects

	for (int facilityId = SP_ID_First; facilityId <= SP_ID_Last; facilityId++)
	{
		if (isBaseHasFacility(baseId, facilityId))
		{
			mindControlCost += conf.alternative_mind_control_project_cost_multiplier * (double)Facility[facilityId].cost;
		}
	}

	// find HQ base

	int hqBaseId = find_hq(base->faction_id);

	// find HQ distance

	int hqDistance;

	if (hqBaseId == -1)
	{
		hqDistance = -1;
	}
	else
	{
		BASE *hqBase = &(Bases[hqBaseId]);
		hqDistance = vector_dist(base->x, base->y, hqBase->x, hqBase->y);
	}

	// distance to HQ factor

	if (hqDistance >= 0 && hqDistance < *MapHalfX / 2)
	{
		mindControlCost *= (2.0 - (double)hqDistance / (double)(*MapHalfX / 2));
	}

	// happiness factor

	mindControlCost *= pow(conf.alternative_mind_control_happiness_power_base, ((double)base->talent_total - (double)base->drone_total) / (double)base->pop_size);

	// previous MC factor

	mindControlCost *= 1.0 + (0.1 * (double)(baseFaction->subvert_total / 4));

	// recapturing factor

	if (base->faction_id_former == probeFactionId)
	{
		mindControlCost /= 2.0;
	}

	// diplomatic factors

	if (isDiploStatus(base->faction_id, probeFactionId, DIPLO_ATROCITY_VICTIM))
	{
		mindControlCost *= 2.0;
	}
	else if (isDiploStatus(base->faction_id, probeFactionId, DIPLO_WANT_REVENGE))
	{
		mindControlCost *= 1.5;
	}

	// difficulty bonus

	if (!is_human(probeFactionId) && is_human(base->faction_id) && baseFaction->diff_level > 3)
	{
		mindControlCost *= 3.0 / (double)baseFaction->diff_level;
	}

	// corner market ends here

	if (cornerMarket != 0)
		return (int)round(mindControlCost);

	// global scaling factor

	mindControlCost *= conf.alternative_subversion_and_mind_control_scale;

	// summarize all adjacent vehicles subversion cost

	double totalSubversionCost = 0;

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// skip not base faction

		if (vehicle->faction_id != base->faction_id)
			continue;

		// nearby only

		if (map_range(base->x, base->y, vehicle->x, vehicle->y) > 1)
			continue;

		// add subversion cost

		totalSubversionCost += (double)getBasicAlternativeSubversionCostWithHQDistance(vehicleId, hqDistance);

	}

	// difficulty bonus

	if (!is_human(probeFactionId) && is_human(base->faction_id) && baseFaction->diff_level > 3)
	{
		totalSubversionCost *= 3.0 / (double)baseFaction->diff_level;
	}

	// add total subversion cost to mind control cost

	mindControlCost += totalSubversionCost;

	// return rounded value

	return (int)round(mindControlCost);

}

/*
Calculates vehicle alternative subversion cost.
*/
int __cdecl getBasicAlternativeSubversionCost(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	int hqDistance;

	// find HQ

	int hqBaseId = find_hq(vehicle->faction_id);

	if (hqBaseId == -1)
	{
		// no HQ

		hqDistance = -1;
	}
	else
	{
		// get HQ base

		BASE *hqBase = &(Bases[hqBaseId]);

		// calculate hqDistance

		hqDistance = vector_dist(vehicle->x, vehicle->y, hqBase->x, hqBase->y);

	}

	// compute subversion cost

	return getBasicAlternativeSubversionCostWithHQDistance(vehicleId, hqDistance);

}

/*
Calculates vehicle alternative subversion cost.
*/
int getBasicAlternativeSubversionCostWithHQDistance(int vehicleId, int hqDistance)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *vehicleUnit = &(Units[vehicle->unit_id]);

	// calculate base unit subversion cost

	double subversionCost = 10.0 * conf.alternative_subversion_unit_cost_multiplier * (double)vehicleUnit->cost;

	// distance to HQ factor

	if (hqDistance >= 0 && hqDistance < *MapHalfX / 2)
	{
		subversionCost *= (2.0 - (double)hqDistance / (double)(*MapHalfX / 2));
	}

	// count number of friendly PE units in vicinity

	int peVehCount = 0;

	for (int otherVehicleId = 0; otherVehicleId < *VehCount; otherVehicleId++)
	{
		VEH *otherVehicle = &(Vehicles[otherVehicleId]);

		// same faction only

		if (otherVehicle->faction_id != vehicle->faction_id)
			continue;

		// nearby only

		if (map_range(vehicle->x, vehicle->y, otherVehicle->x, otherVehicle->y) > 1)
			continue;

		// with PE

		if (vehicle_has_ability(otherVehicleId, ABL_POLY_ENCRYPTION))
		{
			peVehCount++;
		}

	}

	// PE factor

	subversionCost *= (1.0 + (double)peVehCount);

	// unit plan factor

	if (vehicleUnit->plan == PLAN_COLONIZATION || vehicleUnit->plan == PLAN_TERRAFORMING)
	{
		subversionCost *= 2.0;
	}

	// global scaling factor

	subversionCost *= conf.alternative_subversion_and_mind_control_scale;

	// return rounded value

	return (int)round(subversionCost);

}

/*
Destroys unsubverted units in MC-d base.
*/
int __cdecl modifiedMindControlCaptureBase(int baseId, int faction, int probe)
{
	BASE *base = &(Bases[baseId]);

	// call orinal code
	// this actually skips mod_capture_base HQ relocation code but we should believe MD-d base wasn't HQ

	int value = capture_base(baseId, faction, probe);

	// destroy not base faction units in base

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// ignore base faction

		if (vehicle->faction_id == base->faction_id)
			continue;

		// check vehicle is at base

		if (vehicle->x == base->x && vehicle->y == base->y)
		{
			// kill vehicle

			killVehicle(vehicleId);

		}

	}

	return value;

}

/*
Moves subverted vehicle on probe tile and stops probe.
*/
void __cdecl modifiedSubveredVehicleDrawTile(int probeVehicleId, int subvertedVehicleId, int radius)
{
	// store subverted vehicle original location

	VEH *subvertedVehicle = &(Vehicles[subvertedVehicleId]);
	int targetX = subvertedVehicle->x;
	int targetY = subvertedVehicle->y;

	// move subverted vehicle to probe tile

	VEH *probeVehicle = &(Vehicles[probeVehicleId]);
	veh_put(subvertedVehicleId, probeVehicle->x, probeVehicle->y);

	// stop probe

	mod_veh_skip(probeVehicleId);

	// redraw original target tile

	draw_tile(targetX, targetY, radius);

}

/*
Moves subverted vehicle on probe tile and stops probe.
*/
void __cdecl interceptBaseWinDrawSupport(int output_string_pointer, int input_string_pointer)
{
    // call original function

    tx_strcat(output_string_pointer, input_string_pointer);

    // get current base variables

    int baseId = *CurrentBaseID;
    BASE *base = *CurrentBase;
    Faction *faction = &(Factions[base->faction_id]);

    // calculate support numerator

    int supportNumerator = 4 - std::max(-4, std::min(3, faction->SE_support_pending));

    // set units support

    int supportedVehCount = 0;

    for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (vehicle->home_base_id != baseId)
			continue;

		if ((vehicle->state & VSTATE_REQUIRES_SUPPORT) == 0)
			continue;

		vehicle->pad_0 =
			(supportNumerator * (supportedVehCount + 1)) / 4
			-
			(supportNumerator * supportedVehCount) / 4
		;

		supportedVehCount++;

	}

}

int __cdecl modifiedTurnUpkeep()
{
	// collect statistics

	std::vector<int> computerFactions;
	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		if (factionId == 0 || is_human(factionId))
			continue;

		computerFactions.push_back(factionId);

	}
	int factionCount = computerFactions.size();

	int totalBaseCount = 0;
	int totalCitizenCount = 0;
	int totalWorkerCount = 0;
	double totalMinerals = 0.0;
	double totalBudget = 0.0;
	double totalResearch = 0.0;
	int totalTechCount = 0;

	FILE* statistics_base_log = fopen("statistics_base.txt", "a");
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);

		// computer

		if (base->faction_id == 0 || is_human(base->faction_id))
			continue;

		int foundingTurn = getBaseFoundingTurn(baseId);
		int age = getBaseAge(baseId);
		int popSize = base->pop_size;
		int workerCount = base->pop_size - base->specialist_total;

		double mineralIntake2 = (double)base->mineral_intake_2;
		double mineralMultiplier = getBaseMineralMultiplier(baseId);
		double mineralIntake = mineralIntake2 / mineralMultiplier;

		double economyIntake2 = (double)base->economy_total;
		double economyMultiplier = getBaseEconomyMultiplier(baseId);
		double economyIntake = economyIntake2 / economyMultiplier;
		double psychIntake2 = (double)base->psych_total;
		double psychMultiplier = getBasePsychMultiplier(baseId);
		double psychIntake = psychIntake2 / psychMultiplier;
		double labsIntake2 = (double)base->labs_total;
		double labsMultiplier = getBaseLabsMultiplier(baseId);
		double labsIntake = labsIntake2 / labsMultiplier;
		double budgetIntake2 = economyIntake2 + psychIntake2 + labsIntake2;
		double budgetIntake = economyIntake + psychIntake + labsIntake;
		double budgetMultiplier = (budgetIntake <= 0.0 ? 1.0 : budgetIntake2 / budgetIntake);

		// totals

		totalBaseCount++;
		totalCitizenCount += popSize;
		totalWorkerCount += workerCount;
		totalMinerals += mineralIntake2;
		totalBudget += budgetIntake2;

		fprintf
		(
			statistics_base_log,
			"%4X"
			"\t%3d"
			"\t%3d"
			"\t%3d"
			"\t%3d"
			"\t%2d"
			"\t%2d"
			"\t%7.2f"
			"\t%7.2f"
			"\t%5.4f"
			"\t%7.2f"
			"\t%7.2f"
			"\t%5.4f"
			"\n"
			, *MapRandomSeed
			, *CurrentTurn
			, baseId
			, foundingTurn
			, age
			, popSize
			, workerCount
			, mineralIntake
			, mineralIntake2
			, mineralMultiplier
			, budgetIntake
			, budgetIntake2
			, budgetMultiplier
		);

	}
	fclose(statistics_base_log);

	// faction

	double averageFactionBaseCount = (factionCount == 0 ? 0.0 : (double)totalBaseCount / (double)factionCount);
	double averageFactionCitizenCount = (factionCount == 0 ? 0.0 : (double)totalCitizenCount / (double)factionCount);
	double averageFactionWorkerCount = (factionCount == 0 ? 0.0 : (double)totalWorkerCount / (double)factionCount);
	double averageFactionMinerals = (factionCount == 0 ? 0.0 : totalMinerals / (double)factionCount);
	double averageFactionBudget = (factionCount == 0 ? 0.0 : totalBudget / (double)factionCount);

	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		// computer

		if (factionId == 0 || is_human(factionId))
			continue;

		for (int techId = 0; techId <= TECH_BFG9000; techId++)
		{
			if (isTechDiscovered(factionId, techId))
			{
				totalTechCount++;
			}

		}

		totalResearch += getFactionTechPerTurn(factionId);

	}

	double averageFactionTechCount = (factionCount == 0 ? 0.0 : (double)totalTechCount / (double)factionCount);
	double averageFactionResearch = (factionCount == 0 ? 0.0 : totalResearch / (double)factionCount);

	FILE* statistics_faction_log = fopen("statistics_faction.txt", "a");
	fprintf
	(
		statistics_faction_log,
		"%4X"
		"\t%3d"
		"\t%7.2f"
		"\t%7.2f"
		"\t%7.2f"
		"\t%7.2f"
		"\t%7.2f"
		"\t%7.2f"
		"\t%7.2f"
		"\n"
		, *MapRandomSeed
		, *CurrentTurn
		, averageFactionBaseCount
		, averageFactionCitizenCount
		, averageFactionWorkerCount
		, averageFactionMinerals
		, averageFactionBudget
		, averageFactionResearch
		, averageFactionTechCount
	);
	fclose(statistics_faction_log);

	// modify native units cost

	if (conf.native_unit_cost_time_multiplier > 1.0)
	{
		double multiplier = pow(conf.native_unit_cost_time_multiplier, *CurrentTurn);

		Units[BSC_MIND_WORMS].cost = (int)floor((double)conf.native_unit_cost_initial_mind_worm * multiplier);
		Units[BSC_SPORE_LAUNCHER].cost = (int)floor((double)conf.native_unit_cost_initial_spore_launcher * multiplier);
		Units[BSC_SEALURK].cost = (int)floor((double)conf.native_unit_cost_initial_sealurk * multiplier);
		Units[BSC_ISLE_OF_THE_DEEP].cost = (int)floor((double)conf.native_unit_cost_initial_isle_of_the_deep * multiplier);
		Units[BSC_LOCUSTS_OF_CHIRON].cost = (int)floor((double)conf.native_unit_cost_initial_locusts_of_chiron * multiplier);

	}

	// execute original function

	return mod_turn_upkeep();

}

/*
Returns 1 if terraform improvement is destroyable, 0 otherwise.
*/
int __cdecl isDestroyableImprovement(int terraformIndex, int items)
{
	// exclude road if there is magtube

	if (terraformIndex == FORMER_ROAD && (items & BIT_MAGTUBE))
		return 0;

	// exclude farm if there is soil enricher

	if (terraformIndex == FORMER_FARM && (items & BIT_SOIL_ENRICHER))
		return 0;

	// exclude plant fungus

	if (terraformIndex == FORMER_PLANT_FUNGUS)
		return 0;

	// exclude sensor

	if (terraformIndex == FORMER_SENSOR)
		return 0;

	// return 1 by default

	return 1;

}

int __cdecl modified_tech_value(int techId, int factionId, int flag)
{
	debug("modified_tech_value: %-25s %s\n", MFactions[factionId].noun_faction, Tech[techId].name);

	// read original value

	int value = tech_val(techId, factionId, flag);

	debug("\tvalue(o)=%4x\n", value);

	// adjust value

    if (conf.tech_balance && thinker_enabled(factionId))
	{
		if
		(
			techId == Rules->tech_preq_allow_3_energy_sq
			||
			techId == Rules->tech_preq_allow_3_minerals_sq
			||
			techId == Rules->tech_preq_allow_3_nutrients_sq
			||
			techId == Weapon[WPN_TERRAFORMING_UNIT].preq_tech
		)
		{
			value += 4000;
		}

		if
		(
			techId == Weapon[WPN_TROOP_TRANSPORT].preq_tech
			||
			techId == Weapon[WPN_SUPPLY_TRANSPORT].preq_tech
			||
			techId == Facility[FAC_RECYCLING_TANKS].preq_tech
			||
			techId == Facility[FAC_CHILDREN_CRECHE].preq_tech
			||
			techId == Facility[FAC_RECREATION_COMMONS].preq_tech
		)
		{
			value += 500;
		}

	}

	debug("\tvalue(a)=%4x\n", value);

	return value;

}

int getFactionHighestResearchedTechLevel(int factionId)
{
	int highestResearchedTechLevel = 0;

	for (int techId = TECH_Biogen; techId <= TECH_TranT; techId++)
	{
		if (has_tech(factionId, techId))
		{
			highestResearchedTechLevel = std::max(highestResearchedTechLevel, wtp_tech_level(techId));
		}

	}

	return highestResearchedTechLevel;

}

/*
Moves all faction sea units from pact faction territory into proper ports on pact termination.
*/
int __cdecl modified_pact_withdraw(int factionId, int pactFactionId)
{
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (vehicle->faction_id != factionId)
			continue;

		if (vehicle->triad() != TRIAD_SEA)
			continue;

		int closestBaseId = base_find(vehicle->x, vehicle->y);

		if (closestBaseId < 0)
			continue;

		BASE *closestBase = &(Bases[closestBaseId]);

		int territoryOwner = whose_territory(factionId, vehicle->x, vehicle->y, 0, 0);

		if (!(closestBase->faction_id == pactFactionId || territoryOwner == pactFactionId))
			continue;

		// find accessible ocean regions

		robin_hood::unordered_flat_set<int> adjacentOceanRegions = getAdjacentOceanRegions(vehicle->x, vehicle->y);

		// find proper port

		int portBaseId = -1;

		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = &(Bases[baseId]);

			if (base->faction_id != factionId)
				continue;

			// default base

			if (portBaseId == -1)
			{
				portBaseId = baseId;
			}

			// find port base

			bool portBaseFound = false;

			for (int oceanRegion : adjacentOceanRegions)
			{
				if (base_on_sea(baseId, oceanRegion))
				{
					portBaseId =baseId;
					portBaseFound = true;
					break;
				}

			}

			if (portBaseFound)
				break;

		}

		if (portBaseId != -1)
		{
			BASE *portBase = &(Bases[portBaseId]);

			veh_put(vehicleId, portBase->x, portBase->y);

		}

	}

	// execute original code

	return pact_withdraw(factionId, pactFactionId);

}

/*
Prototype proposal entry point.
*/
int __cdecl modified_propose_proto(int factionId, int chassisId, int weaponId, int armorId, int abilities, int reactorId, int plan, char *name)
{
	int offenseValue = Weapon[weaponId].offense_value;
	int defenseValue = Armor[armorId].defense_value;

	// do not build defender ships

	if
	(
		(chassisId == CHS_FOIL || chassisId == CHS_CRUISER)
		&&
		(offenseValue > 0 && defenseValue > 0 && offenseValue < defenseValue)
	)
	{
		debug("modified_propose_proto: rejected defender ship\n");
		debug("\treactorId=%d, chassisId=%d, weaponId=%d, armorId=%d, abilities=%s\n", reactorId, chassisId, weaponId, armorId, getAbilitiesString(abilities).c_str());

		return -1;

	}

	// otherwise execute original code

	return propose_proto(factionId, (VehChassis)chassisId, (VehWeapon)weaponId, (VehArmor)armorId, abilities, (VehReactor)reactorId, (VehPlan)plan, name);

}

/*
Modified base lab doubling function to account for Universal Translator and alternative labs bonuses.
*/
int __cdecl modified_base_double_labs(int labs)
{
	int baseId = *CurrentBaseID;
	BASE *base = &(Bases[baseId]);

	// summarize bonus

	int bonus = 0;

	if (isProjectBuilt(base->faction_id, FAC_SUPERCOLLIDER))
	{
		bonus += conf.science_projects_supercollider_labs_bonus;
	}

	if (isProjectBuilt(base->faction_id, FAC_THEORY_OF_EVERYTHING))
	{
		bonus += conf.science_projects_theoryofeverything_labs_bonus;
	}

	if (isProjectBuilt(base->faction_id, FAC_UNIVERSAL_TRANSLATOR))
	{
		bonus += conf.science_projects_universaltranslator_labs_bonus;
	}

	// apply bonus
	// round up

	if (bonus > 0)
	{
		labs = (labs * (100 + bonus) + (100 - 1)) / 100;
	}

	// return labs

	return labs;

}

/*
Highjack this function to fix stockpile energy bug.
*/
int __cdecl modified_base_production()
{
	int baseId = *CurrentBaseID;
	BASE *base = &(Bases[baseId]);
	Faction *faction = &(Factions[base->faction_id]);

	// add stockpile energy to faction credits

	if (base->queue_items[0] == -FAC_STOCKPILE_ENERGY && base->production_id_last == -FAC_STOCKPILE_ENERGY)
	{
		faction->energy_credits += getStockpileEnergy(baseId);
	}

	// execute original function

	return tx_base_production();

}

/*
Highjack this function to fix stockpile energy bug.
*/
int __cdecl modified_base_ecology()
{
	int baseId = *CurrentBaseID;
	BASE *base = &(Bases[baseId]);
	Faction *faction = &(Factions[base->faction_id]);

	// subtract stockpile energy from faction credits

	if (base->queue_items[0] == -FAC_STOCKPILE_ENERGY && base->production_id_last == -FAC_STOCKPILE_ENERGY)
	{
		faction->energy_credits -= getStockpileEnergy(baseId);
	}

	// execute original function

	return base_ecology();

}

/*
Calculates stockpile energy as vanilla does it.
*/
int getStockpileEnergy(int baseId)
{
	BASE *base = &(Bases[baseId]);

	if (base->mineral_surplus <= 0)
		return 0;

	int credits = (base->mineral_surplus + 1) / 2;

	if (isProjectBuilt(base->faction_id, FAC_PLANETARY_ENERGY_GRID))
	{
		credits = (credits * 5 + 3) / 4;
	}

	return credits;

}

void __cdecl modified_base_yield()
{
	BASE *base = *CurrentBase;
	Faction *faction = &(Factions[base->faction_id]);

	// execute original function

	base_yield();

	// nutrient

	int orbitalNutrient = std::min((int)base->pop_size, faction->satellites_nutrient);
	int orbitalNutrientLimit = (int)floor(conf.orbital_nutrient_population_limit * base->pop_size);
	if (orbitalNutrientLimit < orbitalNutrient)
	{
		int orbitalNutrientDifference = orbitalNutrientLimit - orbitalNutrient;
		base->nutrient_intake += orbitalNutrientDifference;
		base->nutrient_intake_2 += orbitalNutrientDifference;
	}

	// mineral

	int orbitalMineral = std::min((int)base->pop_size, faction->satellites_mineral);
	int orbitalMineralLimit = (int)floor(conf.orbital_mineral_population_limit * base->pop_size);
	if (orbitalMineralLimit < orbitalMineral)
	{
		int orbitalMineralDifference = orbitalMineralLimit - orbitalMineral;
		base->mineral_intake += orbitalMineralDifference;
		base->mineral_intake_2 += orbitalMineralDifference;
	}

	// energy

	int orbitalEnergy = std::min((int)base->pop_size, faction->satellites_energy);
	int orbitalEnergyLimit = (int)floor(conf.orbital_energy_population_limit * base->pop_size);
	if (orbitalEnergyLimit < orbitalEnergy)
	{
		int orbitalEnergyDifference = orbitalEnergyLimit - orbitalEnergy;
		base->energy_intake += orbitalEnergyDifference;
		base->energy_intake_2 += orbitalEnergyDifference;
	}

}

/*
AI transport steals passenger at sea whenever they can.
This modification makes sure they don't.
*/
int __cdecl modified_order_veh(int vehicleId, int angle, int a3)
{
	// invalid angle is not affected by this function
	// not sea transport vehicle is not affected

	if (!(angle >= 0 && angle < ANGLE_COUNT) || !isSeaTransportVehicle(vehicleId))
	{
		return tx_order_veh(vehicleId, angle, a3);
	}

	// check if there is a transport at sea at given angle

	if (!isAdjacentTransportAtSea(vehicleId, angle))
		return tx_order_veh(vehicleId, angle, a3);

	// try next angle

	int newAngle = (angle + 1) % 8;

	if (!isAdjacentTransportAtSea(vehicleId, newAngle))
		return tx_order_veh(vehicleId, newAngle, a3);

	// try all other angles

	for (newAngle = 0; newAngle < 8; newAngle++)
	{
		if (!isAdjacentTransportAtSea(vehicleId, newAngle))
			return tx_order_veh(vehicleId, newAngle, a3);
	}

	// give up and return original angle

	return tx_order_veh(vehicleId, angle, a3);

}

/*
Verifies that given vehicle can move to this angle.
*/
bool isValidMovementAngle(int vehicleId, int angle)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// find target tile

	int x = wrap(vehicle->x + BASE_TILE_OFFSETS[angle + 1][0]);
	int y = vehicle->y + BASE_TILE_OFFSETS[angle + 1][1];
	MAP *tile = getMapTile(x, y);

	// can move into friendly base regardless of realm

	if (map_has_item(tile, BIT_BASE_IN_TILE) && (tile->owner == vehicle->faction_id || has_pact(tile->owner, vehicle->faction_id)))
		return true;

	// cannot move to different realm

	if (vehicle->triad() == TRIAD_LAND && is_ocean(tile))
		return false;

	if (vehicle->triad() == TRIAD_SEA && !is_ocean(tile))
		return false;

	// cannot move to occupied tile
	// just for simplistic purposes - don't want to iterate all vehicles in stack

	if (veh_at(x, y) != -1)
		return false;

	// otherwise, return true

	return true;

}

/*
Verifies that there is same faction transport from given vehicle by given angle.
*/
bool isAdjacentTransportAtSea(int vehicleId, int angle)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// find target tile

	int x = wrap(vehicle->x + BASE_TILE_OFFSETS[angle + 1][0]);
	int y = vehicle->y + BASE_TILE_OFFSETS[angle + 1][1];
	MAP *tile = getMapTile(x, y);

	// verify there is no base

	if (map_has_item(tile, BIT_BASE_IN_TILE))
		return false;

	// get target tile stack

	int targetTileVehicleId = veh_at(x, y);

	// iterate target tile stack to find same faction transport

	for (int targetTileStackVehicleId : getStackVehicles(targetTileVehicleId))
	{
		VEH *targetTileStackVehicle = &(Vehicles[targetTileStackVehicleId]);

		// same faction

		if (targetTileStackVehicle->faction_id != vehicle->faction_id)
			continue;

		// transport

		if (!isTransportVehicle(targetTileStackVehicleId))
			continue;

		// found

		return true;

	}

	// not found

	return false;

}

/*
Assign vehicle home base to own base.
*/
void fixVehicleHomeBases(int factionId)
{
	robin_hood::unordered_flat_set<int> homeBaseIds;

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// exclude not own vehicles

		if (vehicle->faction_id != factionId)
			continue;

		// exclude vehicles without home base

		if (vehicle->home_base_id == -1)
			continue;

		// get home base

		int homeBaseId = vehicle->home_base_id;

		// exclude vehicles with correctly assigned home base

		if (homeBaseId >= 0 && Bases[homeBaseId].faction_id == factionId)
			continue;

		// search own base with highest mineral surplus

		int bestHomeBaseId = -1;
		int bestHomeBaseMineralSurplus = 2;

		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = &(Bases[baseId]);

			// exclude not own base

			if (base->faction_id != factionId)
				continue;

			// exclude already assigned to

			if (homeBaseIds.count(baseId) != 0)
				continue;

			// exclude base with exceeding supportRatio

			double supportRatio = getBaseSupportRatio(baseId);

			if (supportRatio >= conf.ai_production_support_ratio)
				continue;

			// get mineral surplus

			int mineralSurplus = base->mineral_surplus;

			// update best

			if (mineralSurplus > bestHomeBaseMineralSurplus)
			{
				bestHomeBaseId = baseId;
				bestHomeBaseMineralSurplus = mineralSurplus;
			}

		}

		// not found

		if (bestHomeBaseId == -1)
			continue;

		// set home base and mark assigned base

		vehicle->home_base_id = bestHomeBaseId;
		homeBaseIds.insert(bestHomeBaseId);

	}

}

void __cdecl modified_vehicle_range_boom(int x, int y, int flags)
{
	// get current attacking vehicle

	int currentAttacker = *current_attacker;
	int currentDefender = *current_defender;

	// get vehicle at location

	int vehicleId = veh_at(x, y);

	// set current attacker and defender if not set

	if (currentAttacker == -1)
	{
		*current_attacker = vehicleId;
	}

	if (currentDefender == -1)
	{
		*current_defender = vehicleId;
	}

	// execute original code

	boom(x, y, flags);

	// restore current attacker and defender vehicle id

	*current_attacker = currentAttacker;
	*current_defender = currentDefender;

}

/*
Intercept stack_veh call to disable WTP AI transports to pick up everybody.
*/
void __cdecl modified_stack_veh_disable_transport_pick_everybody(int vehicleId, int flag)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int region = vehicleTile->region;
	Faction *faction = getFaction(vehicle->faction_id);

	// get faction strategy flag 2 for this region

	byte region_base_plan = faction->region_base_plan[region];

	if (isWtpEnabledFaction(vehicle->faction_id) && isBaseAt(vehicleTile))
	{
		// temporary set strategy flag to not pick up combat units

		faction->region_base_plan[region] = 2;

	}

	// execute original code

	stack_veh(vehicleId, flag);

	if (isWtpEnabledFaction(vehicle->faction_id))
	{
		// restore strategy flag

		faction->region_base_plan[region] = region_base_plan;

	}

}

/*
Intercept veh_skip call to disable AI non transport to skip turn at destination base.
*/
void __cdecl modified_veh_skip_disable_non_transport_stop_in_base(int vehicleId)
{
	// execute original code for transport only

	if (isTransportVehicle(vehicleId))
	{
		veh_skip(vehicleId);
	}

}

/*
Intercepts alien_move to check whether vehicle is on transport.
*/
bool alienVehicleIsOnTransport = false;
void __cdecl modified_alien_move(int vehicleId)
{
	// set if vehicle is on trasport

	alienVehicleIsOnTransport = isVehicleOnTransport(vehicleId);

	// execute original code

	alien_move(vehicleId);

}
/*
Intercepts can_arty in alien_move to assure artillery cannot shoot from transport.
*/
int __cdecl modified_can_arty_in_alien_move(int unitId, bool allowSeaArty)
{
	// check if vehicle is on trasport and disable artillery if yes

	if (alienVehicleIsOnTransport)
		return 0;

	// otherwise execute original routine

	return tx_can_arty(unitId, allowSeaArty);

}

/*
Removes non pact vehicles from bases.
Removes sea vehicles from land bases.
*/
void removeWrongVehiclesFromBases()
{
	// remove non pact vehicles from bases

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		if (!map_has_item(vehicleTile, BIT_BASE_IN_TILE))
			continue;

		if (isFriendlyTerritory(vehicle->faction_id, vehicleTile))
			continue;

		debug
		(
			"[VANILLA BUG] non pact vehicle in base:"
			" [%4d] (%3d,%3d) %-32s"
			" territoryOwner=%d"
			" vehicleOwner=%d"
			"\n"
			, vehicleId, vehicle->x, vehicle->y, getVehicleUnitName(vehicleId)
			, vehicleTile->owner
			, vehicle->faction_id
		);
		killVehicle(vehicleId);

	}

	// remove sea vehicles from land bases

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();

		if (!map_has_item(vehicleTile, BIT_BASE_IN_TILE))
			continue;

		if (triad != TRIAD_SEA)
			continue;

		bool port = false;
		for (MAP *tile : getRangeTiles(vehicleTile, 1, true))
		{
			if (is_ocean(tile))
			{
				port = true;
				break;
			}
		}

		if (port)
			continue;

		debug
		(
			"[VANILLA BUG] sea vehicle in land base:"
			" [%4d] (%3d,%3d) %-32s"
			"\n"
			, vehicleId, vehicle->x, vehicle->y, getVehicleUnitName(vehicleId)
		);
		killVehicle(vehicleId);

	}

}

/*
Intercepts kill.
*/
void __cdecl modified_kill(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);

	// do not allow computer kill AI vehicle

	if (isWtpEnabledFaction(vehicle->faction_id))
		return;

	// execute original code

	tx_kill(vehicleId);

}

/*
Intercepts text_get 1 to disable tech_steal.
*/
__cdecl void modified_text_get_for_tech_steal_1()
{
	// execute original function

	tx_text_get();

	// save current probe steal tech toggle value

	alphax_tgl_probe_steal_tech_value = *alphax_tgl_probe_steal_tech;

	// check relations and decide whether to disable tech steal

	bool disableTechSteal = false;

	if (isVendetta(*CurrentFaction, *g_PROBE_RFLAG_TARGET) && conf.disable_tech_steal_vendetta)
	{
		disableTechSteal = true;
	}
	else if (isPact(*CurrentFaction, *g_PROBE_RFLAG_TARGET) && conf.disable_tech_steal_pact)
	{
		disableTechSteal = true;
	}
	else if (conf.disable_tech_steal_other)
	{
		disableTechSteal = true;
	}

	// disable tech steal if set

	if (disableTechSteal)
	{
		*alphax_tgl_probe_steal_tech = 0;
	}

}

/*
Intercepts text_get 2 to restore tech_steal.
*/
__cdecl void modified_text_get_for_tech_steal_2()
{
	// execute original function

	tx_text_get();

	// restore probe tech steal toggle value

	*alphax_tgl_probe_steal_tech = alphax_tgl_probe_steal_tech_value;

}

/*
Executes after autosave.
*/
void endOfTurn()
{
	// executionProfiles

	if (DEBUG)
	{
		const int NAME_LENGTH = 120;
		debug("executionProfiles\n");
		for (std::pair<std::string const, Profile> &executionProfileEntry : executionProfiles)
		{
			const std::string &name = executionProfileEntry.first;
			Profile &profile = executionProfileEntry.second;

			std::string displayName = name + " " + std::string(std::max(0, NAME_LENGTH - 1 - (int)name.length()), '.');
			int executionCount = profile.getCount();
			double totalExecutionTime = profile.getTime();
			double averageExectutionTime = (executionCount == 0 ? 0.0 : totalExecutionTime / (double)executionCount);

			debug
			(
				"\t%-*s"
				" count=%8d"
				" totalTime=%7.3f"
				" averageTime=%10.6f"
				"\n"
				, NAME_LENGTH, displayName.c_str()
				, executionCount
				, totalExecutionTime
				, averageExectutionTime
			);
		}
	}

	executionProfiles.clear();

}

