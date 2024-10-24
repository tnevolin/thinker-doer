#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <map>
#include <regex>
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
void Profile::pause()
{
	if (DEBUG)
	{
		this->totalTime += clock() - this->startTime;
	}
}
void Profile::resume()
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
		this->totalTime += clock() - this->startTime;
		this->executionCount++;
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
Combat calculation placeholder.
All custom combat calculation goes here.
*/
__cdecl void wtp_mod_battle_compute(int attackerVehicleId, int defenderVehicleId, int attackerStrengthPointer, int defenderStrengthPointer, int flags)
{
//    debug
//    (
//        "mod_battle_compute(attackerVehicleId=%d, defenderVehicleId=%d, attackerStrengthPointer=%d, defenderStrengthPointer=%d, flags=%d)\n",
//        attackerVehicleId,
//        defenderVehicleId,
//        attackerStrengthPointer,
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
	
	int attackerUnitSpeed = getUnitSpeed(attackerVehicle->faction_id, attackerVehicle->unit_id);
	int defenderUnitSpeed = getUnitSpeed(defenderVehicle->faction_id, defenderVehicle->unit_id);
	
	// get combat map tile
	
	MAP *attackerMapTile = getVehicleMapTile(attackerVehicleId);
	MAP *defenderMapTile = getVehicleMapTile(defenderVehicleId);
	
	// run original function
	
	battle_compute(attackerVehicleId, defenderVehicleId, (int *)attackerStrengthPointer, (int *)defenderStrengthPointer, flags);
	
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
		
		*(int *)attackerStrengthPointer = *(int *)attackerStrengthPointer * (attackerOffenseValue + attackerDefenseValue) / attackerOffenseValue;
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
		addAttackerBonus(attackerStrengthPointer, Rules->combat_bonus_intrinsic_base_def, *(*tx_labels + LABEL_OFFSET_BASE));
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
		
		*(int *)attackerStrengthPointer = *(int *)attackerStrengthPointer * attackerDefenseValue / attackerOffenseValue;
		
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
					addDefenderBonus(defenderStrengthPointer, Rules->combat_psi_bonus_per_planet * defenderPlanetRating, *(*tx_labels + LABEL_OFFSET_PLANET));
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
		// land or ocean with sensor allowed
		
		if (!is_ocean(defenderMapTile) || conf.combat_bonus_sensor_ocean)
		{
			// attacker is in range of friendly sensor and combat is happening on neutral/attacker territory
			
			if (isWithinFriendlySensorRange(attackerVehicle->faction_id, defenderMapTile))
			{
				addAttackerBonus(attackerStrengthPointer, Rules->combat_defend_sensor, *(*tx_labels + LABEL_OFFSET_SENSOR));
			}
			
		}
		
	}
	
    // sensor bonus on defense
	
	{
		// ocean combat only
		// land defence is already covered by vanilla
		
		if (is_ocean(defenderMapTile) && conf.combat_bonus_sensor_ocean)
		{
			// defender is in range of friendly sensor
			
			if (isWithinFriendlySensorRange(defenderVehicle->faction_id, defenderMapTile))
			{
				addDefenderBonus(defenderStrengthPointer, Rules->combat_defend_sensor, *(*tx_labels + LABEL_OFFSET_SENSOR));
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
		addAttackerBonus(attackerStrengthPointer, conf.combat_bonus_attacking_along_road, *(*tx_labels + LABEL_OFFSET_ROAD_ATTACK));
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
		// sensor offense bonus
		
		if (conf.combat_bonus_sensor_offense && (!is_ocean(defenderMapTile) || conf.combat_bonus_sensor_ocean) && isWithinFriendlySensorRange(attackerVehicle->faction_id, defenderMapTile))
		{
			addAttackerBonus(attackerStrengthPointer, Rules->combat_defend_sensor, *(*tx_labels + LABEL_OFFSET_SENSOR));
		}
		
		// sensor defense bonus
		
		if ((!is_ocean(attackerMapTile) || conf.combat_bonus_sensor_ocean) && isWithinFriendlySensorRange(attackerVehicle->faction_id, attackerMapTile))
		{
			addAttackerBonus(attackerStrengthPointer, Rules->combat_defend_sensor, *(*tx_labels + LABEL_OFFSET_SENSOR));
		}
		
		// add base defense bonuses
		
		int defenderBaseId = base_at(defenderVehicle->x, defenderVehicle->y);
		
		if (defenderBaseId != -1)
		{
			if (psiCombat)
			{
				addDefenderBonus(defenderStrengthPointer, Rules->combat_bonus_intrinsic_base_def, *(*tx_labels + LABEL_OFFSET_BASE));
			}
			else
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
					addDefenderBonus(defenderStrengthPointer, Rules->combat_bonus_intrinsic_base_def, *(*tx_labels + LABEL_OFFSET_BASE));
				}
				else
				{
					// base defensive facility
					addDefenderBonus(defenderStrengthPointer, (multiplier - 2) * 50, label);
				}
				
			}
			
		}
		
	}
	
    // ----------------------------------------------------------------------------------------------------
    // conventional power contributes to psi combat
    // ----------------------------------------------------------------------------------------------------
	
    if (psiCombat && conf.conventional_power_psi_percentage > 0 && attackerUnit->offense_value() > 0)
	{
		addAttackerBonus(attackerStrengthPointer, conf.conventional_power_psi_percentage * attackerUnit->offense_value(), LABEL_WEAPON);
	}
	
    if (psiCombat && conf.conventional_power_psi_percentage > 0 && defenderUnit->defense_value() > 0)
	{
		addDefenderBonus(defenderStrengthPointer, conf.conventional_power_psi_percentage * defenderUnit->defense_value(), LABEL_ARMOR);
	}
	
    // ----------------------------------------------------------------------------------------------------
    // AAA range effect
    // ----------------------------------------------------------------------------------------------------
	
    if (attackerUnit->triad() == TRIAD_AIR && defenderUnit->triad() != TRIAD_AIR && !isUnitHasAbility(defenderVehicle->unit_id, ABL_AAA) && conf.aaa_range >= 0)
	{
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = &Vehs[vehicleId];
			
			if (vehicle->faction_id != defenderVehicle->faction_id)
				continue;
			
			if (!isVehicleHasAbility(vehicleId, ABL_AAA))
				continue;
			
			int range = map_range(defenderVehicle->x, defenderVehicle->y, vehicle->x, vehicle->y);
			
			if (range > conf.aaa_range)
				continue;
			
			addDefenderBonus(defenderStrengthPointer, Rules->combat_aaa_bonus_vs_air, *(*tx_labels + LABEL_OFFSET_TRACKING));
			
			break;
			
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
colony, former, transport have their chassis cost reduced to match land analogues.
foil = infantry
cruiser = speeder

1. Calculate module and weapon/armor reactor modified costs.
module reactor modified cost = item cost * (Fission reactor value / 100)
weapon/armor reactor modified cost = item cost * (reactor value / 100)
1a. Planet Buster, Tectonic Payload, Fungal Payload are treated as modules.

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
    int weapon_cost = Weapon[weapon_id].cost;
    int armor_cost = Armor[armor_id].cost;
    int chassis_cost = Chassis[chassis_id].cost;
	
    // reactor cost factors
	
    double fission_reactor_cost_factor = (double)conf.reactor_cost_factors[0] / 100.0;
    double reactor_cost_factor = (double)conf.reactor_cost_factors[reactor_level - 1] / 100.0;
	
    // minimal item costs
	
    int minimal_weapon_cost = Weapon[WPN_HAND_WEAPONS].cost;
    int minimal_armor_cost = Armor[ARM_NO_ARMOR].cost;
	
    // component values
	
    int weapon_offence_value = Weapon[weapon_id].offense_value;
    int armor_defense_value = Armor[armor_id].defense_value;
    int chassis_speed = Chassis[chassis_id].speed;
	
    // module or weapon
	
    bool module = ((weapon_id >= WPN_PLANET_BUSTER && weapon_id <= WPN_ALIEN_ARTIFACT) || weapon_id == WPN_TECTONIC_PAYLOAD || weapon_id == WPN_FUNGAL_PAYLOAD);
	
    // get component costs
	
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
	
    // items reactor modified costs
	
    double weapon_reactor_modified_cost = (double)weapon_cost * (module ? fission_reactor_cost_factor : reactor_cost_factor);
    double armor_reactor_modified_cost = (double)armor_cost * reactor_cost_factor;
	
    // primary item and secondary item shifted costs
	
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

/*
Multipurpose combat roll function.
Determines which side wins this round. Returns 1 for attacker, 0 for defender.
*/
__cdecl int combat_roll(int attacker_strength, int defender_strength, int attacker_vehicle_offset, int defender_vehicle_offset, int attacker_initial_power, int defender_initial_power, int *attacker_won_last_round)
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
	
    return standard_combat_mechanics_combat_roll(attacker_strength, defender_strength);
	
}

/*
Standard combat mechanics.
Determines which side wins this round. Returns 1 for attacker, 0 for defender.
*/
int standard_combat_mechanics_combat_roll(int attacker_strength, int defender_strength)
{
    debug("standard_combat_mechanics_combat_roll(attacker_strength=%d, defender_strength=%d)\n", attacker_strength, defender_strength);
	
    // generate random roll
	
    int random_roll = random(attacker_strength + defender_strength);
    debug("\trandom_roll=%d\n", random_roll);
	
    // calculate outcome
	
    int attacker_wins = (random_roll < attacker_strength ? 1 : 0);
    debug("\tattacker_wins=%d\n", attacker_wins);
	
    return attacker_wins;
	
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

__cdecl int modified_artillery_damage(int attacker_strength, int defender_strength, int attacker_firepower)
{
    debug
    (
        "modified_artillery_damage(attacker_strength=%d, defender_strength=%d, attacker_firepower=%d)\n",
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
	// get old cost factors
	
	int nutrient_cost_factor_old = mod_cost_factor(faction_id, RSC_NUTRIENT, -1);
	int mineral_cost_factor_old = mod_cost_factor(faction_id, RSC_MINERAL, -1);
	
	// execute original code
	
	// use modified function instead of original code
//	int returnValue = tx_set_se_on_dialog_close(a1, a2, faction_id, a4, a5);
	int returnValue = modifiedSocialCalc(a1, a2, faction_id, a4, a5);
	
	// get new cost factors
	
	int nutrient_cost_factor_new = mod_cost_factor(faction_id, RSC_NUTRIENT, -1);
	int mineral_cost_factor_new = mod_cost_factor(faction_id, RSC_MINERAL, -1);
	
	debug
	(
		"se_accumulated_resource_adjustment: faction_id=%d, nutrient_cost_factor: %+d -> %+d, mineral_cost_factor_new: %+d -> %+d\n"
		, faction_id
		, nutrient_cost_factor_old
		, nutrient_cost_factor_new
		, mineral_cost_factor_old
		, mineral_cost_factor_new
	)
	;
	
	// human faction
	
	if (!is_human(faction_id))
		return returnValue;
	
	// adjust nutrients
	
	if (nutrient_cost_factor_new != nutrient_cost_factor_old)
	{
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE* base = &Bases[baseId];
			
			if (base->faction_id != faction_id)
				continue;
			
			base->nutrients_accumulated = (base->nutrients_accumulated * nutrient_cost_factor_new + ((nutrient_cost_factor_old - 1) / 2)) / nutrient_cost_factor_old;
			
		}
		
	}
	
	// adjust minerals
	
	if (mineral_cost_factor_new != mineral_cost_factor_old)
	{
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE* base = &Bases[baseId];
			
			if (base->faction_id != faction_id)
				continue;
			
			base->minerals_accumulated = (base->minerals_accumulated * mineral_cost_factor_new + ((mineral_cost_factor_old - 1) / 2)) / mineral_cost_factor_old;
			
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
	
	// implemented in Thinker
//    // correct road/tube movement rate
//	
//    if (conf.magtube_movement_rate > 0)
//    {
//        MAP* square_from = mapsq(from_x, from_y);
//        MAP* square_to = mapsq(to_x, to_y);
//		
//        if (square_from && square_to)
//        {
//            // set movement cost along tube to 1
//            // base implies tube in its tile
//            if
//            (
//                (square_from->items & (BIT_BASE_IN_TILE | BIT_MAGTUBE))
//                &&
//                (square_to->items & (BIT_BASE_IN_TILE | BIT_MAGTUBE))
//            )
//            {
//                value = 1;
//            }
//            // set road movement cost
//            else if (value == 1)
//            {
//                value = conf.road_movement_cost;
//            }
//			
//        }
//		
//    }
	
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

__cdecl int sayBase(char *buffer, int baseId)
{
	// execute original code

	int returnValue = tx_say_base(buffer, baseId);

	// get base

	BASE *base = &(Bases[baseId]);

	// get base population limit

	int populationLimit = getBasePopulationLimit(baseId);

	// generate symbol

	char symbol[10] = "   <P>";

	if (populationLimit != -1 && base->pop_size >= populationLimit)
	{
		strcat(buffer, symbol);
	}

	// return original value

	return returnValue;

}

/*
Modifies base init computation.
*/
__cdecl int wtp_mod_base_init(int factionId, int x, int y)
{
	// execute original code
	
	int baseId = base_init(factionId, x, y);
	
	// return immediatelly if base wasn't created
	
	if (baseId < 0)
		return baseId;
	
	BASE *base = getBase(baseId);
	
	// set base founding turn
	
	base->pad_1 = *CurrentTurn;
	
	// alternative support - disable no free minerals for new base penalty
	
	if (conf.alternative_support)
	{
		base->minerals_accumulated = std::max(Rules->retool_exemption, base->minerals_accumulated);
	}
	
	// The Planetary Transit System fix
	// new base size is limited by average faction base size
	
	if (isFactionHasProject(factionId, FAC_PLANETARY_TRANSIT_SYSTEM) && base->pop_size == 3)
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
		if (isFactionHasProject(factionId, FAC_CLONING_VATS))
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
	sprintf(destinationString + strlen("NUT"), " (%+1d) %+1d => %02d", faction->SE_growth_pending, *BaseGrowthRate, nutrientCostFactor);

}

/*
Replaces growth turn indicator with correct information for pop boom and stagnation.
*/
__cdecl void correctGrowthTurnsIndicator(int destinationStringPointer, int sourceStringPointer)
{
    // call original function

    tx_strcat(destinationStringPointer, sourceStringPointer);

    if (*BaseGrowthRate > conf.se_growth_rating_max)
	{
		// update indicator for population boom

		strcpy((char *)destinationStringPointer, "-- POP BOOM --");

	}
    else if (*BaseGrowthRate < conf.se_growth_rating_min)
	{
		// update indicator for stagnation

		strcpy((char *)destinationStringPointer, "-- STAGNATION --");

	}

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

/**
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

/**
Calculates summary cost of all not prototyped components.
Scans all faction units to see if components are already prototyped.
*/
int calculateNotPrototypedComponentsCost(int factionId, int chassisId, int weaponId, int armorId)
{
	int chassisPrototyped = 0;
	int weaponPrototyped = 0;
	int armorPrototyped = 0;
	
	for (int unitId : getFactionUnitIds(factionId, true, false))
	{
		UNIT *unit = getUnit(unitId);
		
		if (unit->chassis_id == chassisId)
		{
			chassisPrototyped = 1;
		}
		
		if (unit->weapon_id == weaponId)
		{
			weaponPrototyped = 1;
		}
		
		if (unit->armor_id == armorId)
		{
			armorPrototyped = 1;
		}
		
	}
	
	return calculateNotPrototypedComponentsCost(chassisId, weaponId, armorId, chassisPrototyped, weaponPrototyped, armorPrototyped);
	
}

int calculateNotPrototypedComponentsCost(int unitId)
{
	// all predesigned units are always prototyped
	
	if (unitId < MaxProtoFactionNum)
		return 0;
	
	int factionId = unitId / MaxProtoFactionNum;
	UNIT *unit = getUnit(unitId);
	
	return calculateNotPrototypedComponentsCost(factionId, unit->chassis_id, unit->weapon_id, unit->armor_id);
	
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
	case 1:
		*risk += conf.probe_action_risk_procure_research_data;
		break;
		// genetic plague
	case 7:
		*risk += conf.probe_action_risk_introduce_genetic_plague;
		break;
	}

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

	*BaseGrowthRate += getHabitationFacilitiesBaseGrowthModifier(baseId);

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

/**
Computes habitation facilties base GROWTH modifier.
*/
int getHabitationFacilitiesBaseGrowthModifier(int baseId)
{
	BASE *base = getBase(baseId);
	
	// return zero if not set
	
	if (conf.habitation_facility_growth_bonus == 0)
		return 0;
	
	// calculate habitation facilities base GROWTH modifier
	
	int baseGrowthModifier = 0;
	
	if (isBaseHasFacility(baseId, FAC_HAB_COMPLEX) && base->pop_size <= Rules->pop_limit_wo_hab_complex)
	{
		baseGrowthModifier += conf.habitation_facility_growth_bonus;
	}
	
	if (isBaseHasFacility(baseId, FAC_HABITATION_DOME) && base->pop_size <= Rules->pop_limit_wo_hab_dome)
	{
		baseGrowthModifier += conf.habitation_facility_growth_bonus;
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
	debug("modifiedFactionUpkeep - %s\n", getMFaction(factionId)->noun_faction);
	
	executionProfiles["0. modifiedFactionUpkeep"].start();
	
    // remove wrong units from bases
	
    if (factionId > 0)
	{
		executionProfiles["0.1. removeWrongVehiclesFromBases"].start();
		removeWrongVehiclesFromBases();
		executionProfiles["0.1. removeWrongVehiclesFromBases"].stop();
	}
	
	// choose AI logic
	
	int returnValue;
	
	if (isWtpEnabledFaction(factionId))
	{
		// run WTP AI code for AI eanbled factions
		
		executionProfiles["0.3. aiFactionUpkeep"].start();
		aiFactionUpkeep(factionId);
		executionProfiles["0.3. aiFactionUpkeep"].stop();
		
	}
	
	executionProfiles["0. modifiedFactionUpkeep"].pause();
	
	// execute original function
	
	returnValue = mod_faction_upkeep(factionId);
	
	executionProfiles["0. modifiedFactionUpkeep"].resume();
	
    // fix vehicle home bases for normal factions
	
    if (factionId > 0)
	{
		executionProfiles["0.4. fixVehicleHomeBases"].start();
		fixVehicleHomeBases(factionId);
		executionProfiles["0.4. fixVehicleHomeBases"].stop();
	}
	
	executionProfiles["0. modifiedFactionUpkeep"].stop();
	
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

			int attackerPower = (attackerUnit->plan == PLAN_ARTIFACT ? 1 : attackerUnit->reactor_id * 10 - attackerVehicle->damage_taken);
			int defenderPower = (defenderUnit->plan == PLAN_ARTIFACT ? 1 : defenderUnit->reactor_id * 10 - defenderVehicle->damage_taken);

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
		attackerWinningProbability =
			standard_combat_mechanics_calculate_attacker_winning_probability
			(
				p,
				attackerHP,
				defenderHP
			)
		;

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

	if (isBaseHasFacility(baseId, FAC_HEADQUARTERS) && cornerMarket == 0)
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
		if (isBaseHasFacility(baseId, facilityId))
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

	if (vehicleUnit->plan == PLAN_COLONY || vehicleUnit->plan == PLAN_TERRAFORM)
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
	double totalEcoLab = 0.0;
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
		
		int nutrientCostFactor = cost_factor(base->faction_id, 0, baseId);
		int nutrientSurplus = base->nutrient_surplus;
		
		int mineralIntake = base->mineral_intake;
		int mineralIntake2 = base->mineral_intake_2;
		double mineralMultiplier = getBaseMineralMultiplier(baseId);
		
		Budget budgetIntake = getBaseBudgetIntake(baseId);
		Budget budgetIntake2 = getBaseBudgetIntake2(baseId);
		
//		int economyIntake = budgetIntake.economy;
//		int economyIntake2 = budgetIntake2.economy;
//		double economyMultiplier = getBaseEconomyMultiplier(baseId);
//		int psychIntake = budgetIntake.psych;
//		int psychIntake2 = budgetIntake2.psych;
//		double psychMultiplier = getBasePsychMultiplier(baseId);
//		int labsIntake = budgetIntake.labs;
//		int labsIntake2 = budgetIntake2.labs;
//		double labsMultiplier = getBaseLabsMultiplier(baseId);
		int ecolabIntake = budgetIntake.economy + budgetIntake.labs;
		int ecolabIntake2 = budgetIntake2.economy + budgetIntake2.labs;
		double ecolabMultiplier = ecolabIntake <= 0 ? 1.0 : (double)ecolabIntake2 / (double)ecolabIntake;
		
		// totals
		
		totalBaseCount++;
		totalCitizenCount += popSize;
		totalWorkerCount += workerCount;
		totalMinerals += mineralIntake2;
		totalEcoLab += ecolabIntake2;
		
		fprintf
		(
			statistics_base_log,
			"%4X"
			"\t%d"
			"\t%d"
			"\t%-25s"
			"\t%d"
			"\t%d"
			"\t%d"
			"\t%d"
			"\t%d"
			"\t%d"
			"\t%d"
			"\t%d"
			"\t%5.4f"
			"\t%d"
			"\t%d"
			"\t%5.4f"
			"\n"
			, *MapRandomSeed
			, *CurrentTurn
			, baseId
			, base->name
			, foundingTurn
			, age
			, popSize
			, workerCount
			, nutrientCostFactor
			, nutrientSurplus
			, mineralIntake
			, mineralIntake2
			, mineralMultiplier
			, ecolabIntake
			, ecolabIntake2
			, ecolabMultiplier
		);
		
	}
	fclose(statistics_base_log);
	
	// faction
	
	double averageFactionBaseCount = (factionCount == 0 ? 0.0 : (double)totalBaseCount / (double)factionCount);
	double averageFactionCitizenCount = (factionCount == 0 ? 0.0 : (double)totalCitizenCount / (double)factionCount);
	double averageFactionWorkerCount = (factionCount == 0 ? 0.0 : (double)totalWorkerCount / (double)factionCount);
	double averageFactionMinerals = (factionCount == 0 ? 0.0 : totalMinerals / (double)factionCount);
	double averageFactionEcoLab = (factionCount == 0 ? 0.0 : totalEcoLab / (double)factionCount);
	
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
		, averageFactionEcoLab
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
		if (isFactionHasTech(factionId, techId))
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

		int territoryOwner = mod_whose_territory(factionId, vehicle->x, vehicle->y, 0, 0);

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

	if (isFactionHasProject(base->faction_id, FAC_SUPERCOLLIDER))
	{
		bonus += conf.science_projects_supercollider_labs_bonus;
	}

	if (isFactionHasProject(base->faction_id, FAC_THEORY_OF_EVERYTHING))
	{
		bonus += conf.science_projects_theoryofeverything_labs_bonus;
	}

	if (isFactionHasProject(base->faction_id, FAC_UNIVERSAL_TRANSLATOR))
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
Calculates stockpile energy as vanilla does it.
*/
int getStockpileEnergy(int baseId)
{
	BASE *base = &(Bases[baseId]);

	if (base->mineral_surplus <= 0)
		return 0;

	int credits = (base->mineral_surplus + 1) / 2;

	if (isFactionHasProject(base->faction_id, FAC_PLANETARY_ENERGY_GRID))
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
Patches:
1. AI transport steals passenger from other transports when their paths cross. This modification makes sure they don't.
2. Path::move sometimes cannot move land unit through ZOC. This modification offers alternative path.
*/
int __cdecl wtp_mod_order_veh(int vehicleId, int angle, int a3)
{
	// call default for invalid angle or vehicle
	
	if (!((angle >= 0 && angle < ANGLE_COUNT) && (vehicleId >= 0 && vehicleId < *VehCount)))
		return tx_order_veh(vehicleId, angle, a3);
	
	// parameters
		
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int triad = vehicle->triad();
	bool zocAffectedVehicle = isZocAffectedVehicle(vehicleId);
	MAP *src = getVehicleMapTile(vehicleId);
	MAP *dst = getTileByAngle(src, angle);
	
	// do not alter human player vehicle movement
	
	if (is_human(factionId))
		return tx_order_veh(vehicleId, angle, a3);
	
	// correct sea transport path
	
	if (isSeaTransportVehicle(vehicleId))
	{
		// check if there is a sea transport at a given angle
		
		if (!isAdjacentTransportAtSea(vehicleId, angle))
			return tx_order_veh(vehicleId, angle, a3);
		
		// try other angles
		
		for (int newAngle = angle + 1; newAngle < angle + ANGLE_COUNT; newAngle++)
		{
			if (!isAdjacentTransportAtSea(vehicleId, newAngle % ANGLE_COUNT))
				return tx_order_veh(vehicleId, newAngle, a3);
		}
		
	}
	
	// correct land unit ZOC movement
	
	else if (triad == TRIAD_LAND && zocAffectedVehicle && !is_ocean(src) && !is_ocean(dst) && isZoc(factionId, src, dst))
	{
		for (int newAngleIndex = 1; newAngleIndex < ANGLE_COUNT; newAngleIndex++)
		{
			int newAngle;
			if (newAngleIndex % 2 == 1)
			{
				newAngle = (angle + ((newAngleIndex / 2) + 1)) % ANGLE_COUNT;
			}
			else
			{
				newAngle = (angle + ANGLE_COUNT - (newAngleIndex / 2)) % ANGLE_COUNT;
			}
			
			MAP *newDst = getTileByAngle(src, newAngle);
			
			if (newDst == nullptr)
				continue;
			
			if (!is_ocean(newDst) && !isBlocked(factionId, newDst) && !isZoc(factionId, src, newDst))
				return tx_order_veh(vehicleId, newAngle, a3);
				
		}
		
	}
	
	// return original angle if nothing else applied or worked
	
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
			" [%4d] %s %-32s"
			" territoryOwner=%d"
			" vehicleOwner=%d"
			"\n"
			, vehicleId, getLocationString({vehicle->x, vehicle->y}).c_str(), getVehicleUnitName(vehicleId)
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
			" [%4d] %s %-32s"
			"\n"
			, vehicleId, getLocationString({vehicle->x, vehicle->y}).c_str(), getVehicleUnitName(vehicleId)
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

	if (isVendetta(*CurrentFaction, *g_PROBE_FACT_TARGET) && conf.disable_tech_steal_vendetta)
	{
		disableTechSteal = true;
	}
	else if (isPact(*CurrentFaction, *g_PROBE_FACT_TARGET) && conf.disable_tech_steal_pact)
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
				"   count=%8d"
				"   totalTime=%7.3f"
				"   averageTime=%10.6f"
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

/*
Disable base_hurry call.
*/
void __cdecl wtp_mod_base_hurry()
{
}

/**
Adds bonus to battle odds dialog.
*/
void addAttackerBonus(int strengthPointer, int bonus, const char *label)
{
	// modify strength
	
	*(int *)strengthPointer = (int)round((double)(*(int *)strengthPointer) * (double)(100 + bonus) / 100.0);
	
	// add effect description
	
	if (*tx_battle_compute_attacker_effect_count < 4)
	{
		strcpy((*tx_battle_compute_attacker_effect_labels)[*tx_battle_compute_attacker_effect_count], label);
		(*tx_battle_compute_attacker_effect_values)[*tx_battle_compute_attacker_effect_count] = bonus;
		(*tx_battle_compute_attacker_effect_count)++;
	}
	
}
void addDefenderBonus(int strengthPointer, int bonus, const char *label)
{
	// modify strength
	
	*(int *)strengthPointer = (int)round((double)(*(int *)strengthPointer) * (double)(100 + bonus) / 100.0);
	
	// add effect description
	
	if (*tx_battle_compute_defender_effect_count < 4)
	{
		strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], label);
		(*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = bonus;
		(*tx_battle_compute_defender_effect_count)++;
	}
	
}

/**
Disables displaying resource bonus text in map status windows.
*/
__cdecl int modStatusWinBonus_bonus_at(int /*x*/, int /*y*/)
{
	// do not display bonus
	
    return 0;
    
}

/**
Sets moved factions = 8 if there is no human player.
*/
__cdecl int modified_load_game(int a0, int a1)
{
	// original function
	
	int returnValue = load_game(a0, a1);
	
	// set moved factions = 8 (beyond all factions) if there is no human player
	
	if ((*FactionStatus && 0xFF) == 0)
	{
		*TurnFactionsMoved = 8;
	}
	
	// return original value
	
	return returnValue;
	
}

/**
Sets moved factions = 8 if there is no human player.
*/
__cdecl int modified_zoc_veh(int a0, int a1, int a2)
{
	// original function
	
	int returnValue = zoc_veh(a0, a1, a2);
	
	// do not allow returning 1 - this scares formers
	
	if (returnValue == 1)
	{
		returnValue = 0;
	}
	
	// return value
	
	return returnValue;
	
}

/**
Deletes supported units in more sane order.
*/
__cdecl void modified_base_check_support()
{
	// kill vehicles in bases with low mineral surplus
	
	int baseId = *CurrentBaseID;
	BASE *base = *CurrentBase;
	
	MAP *baseTile = getBaseMapTile(baseId);
	
	// delete vehicles until surplus becomes non negative
	
	while (base->mineral_surplus < 0)
	{
		int cheapestVehicleId = -1;
		int cheapestVehicleCost = INT_MAX;
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// this base
			
			if (vehicle->home_base_id != baseId)
				continue;
			
			// requires support
			
			if (!isVehicleRequiresSupport(vehicleId))
				continue;
			
			int cost = vehicle->cost();
			
			if (cost < cheapestVehicleCost || (cost == cheapestVehicleCost && vehicleTile == baseTile))
			{
				cheapestVehicleId = vehicleId;
				cheapestVehicleCost = cost;
			}
			
		}
		
		if (cheapestVehicleId == -1)
			break;
		
		killVehicle(cheapestVehicleId);
		computeBase(baseId, false);
		
		debug("\t%-25s %-32s killed because of oversupport\n", base->name, getVehicle(cheapestVehicleId)->name());
		
	}
	
	// original function
	
	base_check_support();
	
}

void __cdecl wtp_mod_base_support()
{
    BASE* base = *CurrentBase;
    int base_id = *CurrentBaseID;
    Faction* f = &Factions[base->faction_id];

    const int SupportCosts[8][2]  = {
        {2, 0}, // -4, Each unit costs 2 to support; no free minerals for new base.
        {1, 0}, // -3, Each unit costs 1 to support; no free minerals for new base.
        {1, 1}, // -2, Support 1 unit free per base; no free minerals for new base.
        {1, 1}, // -1, Support 1 unit free per base
        {1, 2}, //  0, Support 2 units free per base
        {1, 3}, //  1, Support 3 units free per base
        {1, 4}, //  2, Support 4 units free per base!
        {1, max((int)base->pop_size, 4)}, // 3, Support 4 units OR up to base size for free!!
    };
    const int support_val = clamp(f->SE_support_pending + 4, 0, 7);
    const int support_type = (conf.modify_unit_support == 1 ? PLAN_SUPPLY :
        (conf.modify_unit_support == 2 ? PLAN_PROBE : PLAN_TERRAFORM));
    const int SE_police = base_police(base_id, true);

    BaseResourceConvoyTo[0] = 0;
    BaseResourceConvoyTo[1] = 0;
    BaseResourceConvoyTo[2] = 0;
    BaseResourceConvoyTo[3] = 0;
    BaseResourceConvoyFrom[0] = 0;
    BaseResourceConvoyFrom[1] = 0;
    BaseResourceConvoyFrom[2] = 0;
    BaseResourceConvoyFrom[3] = 0;
    *BaseVehPacifismCount = 0;
    *BaseForcesSupported = 0;
    *BaseForcesMaintCost = 0;
    *BaseForcesMaintCount = 0;

    for (int i = 0; i < *VehCount; i++) {
        VEH* veh = &Vehs[i];
        if (veh->faction_id != base->faction_id) {
            continue;
        }
        MAP* sq = mapsq(veh->x, veh->y);
        if (veh->is_supply() && veh->order == ORDER_CONVOY) {
            BaseResType type = (BaseResType)veh->order_auto_type;
            assert(type <= RSC_UNUSED);
            if (veh->home_base_id == base_id && type <= RSC_UNUSED) {
                if (!sq->is_base() || sq->veh_owner() < 0) { // other condition redundant?
                    int value = resource_yield(type, veh->faction_id, base_id, veh->x, veh->y);
                    BaseResourceConvoyTo[type] += value;
                }
                if (sq->is_base() && sq->veh_owner() >= 0) {
                    BaseResourceConvoyFrom[type]++;
                }
            } else if (veh->x == base->x && veh->y == base->y
            && veh->home_base_id >= 0 && type <= RSC_UNUSED) {
                BaseResourceConvoyTo[type]++;
            }
        }
        if (veh->home_base_id == base_id) {
            veh->state &= ~(VSTATE_PACIFISM_FREE_SKIP|VSTATE_PACIFISM_DRONE|VSTATE_REQUIRES_SUPPORT);

            if (veh->offense_value() && (veh->offense_value() > 0 || veh->unit_id >= MaxProtoFactionNum)
            && veh->plan() != PLAN_RECONNAISSANCE && veh->plan() != PLAN_PLANET_BUSTER) {
                f->unk_46 += 1 + (veh->plan() == PLAN_OFFENSIVE || veh->plan() == PLAN_COMBAT);
            }
            // Exclude probe, supply, artifact, fungal and tectonic missiles
            // Native units do not require support on fungus
            // However fungus is not rendered on tiles deeper than ocean shelf
            if (veh->plan() <= support_type) {
                if (!has_abil(veh->unit_id, ABL_CLEAN_REACTOR)) {
                    if (veh->unit_id >= MaxProtoFactionNum
                    || (veh->offense_value() >= 0 || !sq->is_fungus())
                    || sq->alt_level() < ALT_OCEAN_SHELF) {
                        (*BaseForcesSupported)++;
                        if (*BaseForcesSupported > SupportCosts[support_val][1]) {
                            veh->state |= VSTATE_REQUIRES_SUPPORT;
                            (*BaseForcesMaintCount)++;
                            (*BaseForcesMaintCost) += SupportCosts[support_val][0];
                        }
                    }
                    if (*BaseUpkeepState == 1) {
                        for (int j = 0; j < 8; j++) {
                            if (*BaseForcesSupported > SupportCosts[j][1]) {
                                f->unk_40[j] += SupportCosts[j][0];
                            }
                        }
                    }
                }
                if (veh->offense_value() != 0 && veh->plan() != PLAN_AIR_SUPERIORITY
                && (!sq->is_base() || sq->veh_owner() < 0) && (veh->triad() == TRIAD_AIR
                || mod_whose_territory(base->faction_id, veh->x, veh->y, 0, 0) != base->faction_id)) {
                    (*BaseVehPacifismCount)++;
                    if (SE_police == -3 && *BaseVehPacifismCount == 1) {
                        veh->state |= VSTATE_PACIFISM_FREE_SKIP;
                    } else if (SE_police <= -3) {
                        veh->state |= VSTATE_PACIFISM_DRONE;
                    }
                }
            }
        }
    }
}

void __cdecl displayHurryCostScaledToBasicMineralCostMultiplierInformation(int input_string_pointer, int output_string_pointer)
{
	// execute original function
	
	tx_parse_string(input_string_pointer, output_string_pointer);
	
	// get output string pointer
	
	char *output = (char *)output_string_pointer;
	
	// append information to string
	
	sprintf(output + strlen(output), " (hurry cost is scaled to basic mineral cost multiplier: %d)", Rules->mineral_cost_multi);
	
}

void __cdecl displayPartialHurryCostToCompleteNextTurnInformation(int input_string_pointer, int output_string_pointer)
{
	// execute original function
	
	tx_parse_string(input_string_pointer, output_string_pointer);
	
//	// apply additional information message only when flat hurry cost is enabled
//	
//	if (!conf.flat_hurry_cost)
//		return;
//	
//	// get output string pointer
//	
//	char *output = (char *)output_string_pointer;
//	
//	// append information to string
//	
//	sprintf(output + strlen(output), " (minimal hurry cost to complete production next turn = %d)", getPartialHurryCostToCompleteNextTurn());
//	
}

/**
Returns current base item hurry minimal mineral cost to complete production on next turn.
*/
int __cdecl getHurryMinimalMineralCost(int itemCost, int /*a1*/, int /*a2*/)
{
	BASE *base = *CurrentBase;
	
	int mineralCostFactor = cost_factor(base->faction_id, 1, -1);
	int hurryMineralCost = mineralCostFactor * itemCost;
	
	if (conf.hurry_minimal_minerals)
	{
		// reduce hurry mineral cost by mineral surplus to complete production next turn
		
		hurryMineralCost -= std::max(0, base->mineral_surplus);
		
	}
	
	return hurryMineralCost;
	
}

/**
Returns current base item hurry mineral cost.
Could be either complete cost or minimal depending on the settings.
*/
int getHurryMineralCost(int mineralCost)
{
	int hurryMineralCost = mineralCost;
	
	if (conf.hurry_minimal_minerals)
	{
		// reduce hurry mineral cost by mineral surplus to complete production next turn
		
		hurryMineralCost = std::max((*CurrentBase)->minerals_accumulated, mineralCost - std::max(0, (*CurrentBase)->mineral_surplus));
		
	}
	
	return hurryMineralCost;
	
}

/*
Reverts Thinker modification.
*/
int __thiscall wtp_BaseWin_popup_start(Win* This, const char* filename, const char* label, int a4, int a5, int a6, int a7)
{
	return Popup_start(This, filename, label, a4, a5, a6, a7);
}

/*
Reverts Thinker modification.
*/
int __cdecl wtp_BaseWin_ask_number(const char* label, int value, int a3)
{
    return pop_ask_number(ScriptFile, label, value, a3);
}

int __cdecl wtp_mod_quick_zoc(int /*a0*/, int /*a1*/, int /*a2*/, int /*a3*/, int /*a4*/, int /*a5*/, int /*a6*/)
{
	return 0;
}

int __cdecl wtp_mod_zoc_any(int /*a0*/, int /*a1*/, int /*a2*/)
{
	return 0;
}

int __cdecl wtp_mod_zoc_veh(int /*a0*/, int /*a1*/, int /*a2*/)
{
	return 0;
}

int __cdecl wtp_mod_zoc_sea(int /*a0*/, int /*a1*/, int /*a2*/)
{
	return 0;
}

int __cdecl wtp_mod_zoc_move(int /*a0*/, int /*a1*/, int /*a2*/)
{
	return 0;
}

int __thiscall wtp_mod_zoc_path(Path */*This*/, int /*a0*/, int /*a1*/, int /*a2*/)
{
	return 0;
}

