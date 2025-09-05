#include "wtp_mod.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <map>
#include <regex>
#include "tech.h"
#include "patch.h"
#include "wtp_terranx.h"
#include "wtp_game.h"
#include "wtp_ai.h"
#include "wtp_aiMove.h"

int alphax_tgl_probe_steal_tech_value;

// probe subversion parameters
int probe_probeVehicleId;
int probe_targetBaseId;
int probe_targetVehicleId;
int probe_probeMovesSpent = -1;

/*
Combat calculation placeholder.
All custom combat calculation goes here.
*/
__cdecl void wtp_mod_battle_compute(int attackerVehicleId, int defenderVehicleId, int *attackerStrengthPointer, int *defenderStrengthPointer, int combat_type)
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
	
	VEH &attackerVehicle = Vehs[attackerVehicleId];
	VEH &defenderVehicle = Vehs[defenderVehicleId];
	
	// get attacker/defender unit
	
	UNIT &attackerUnit = Units[attackerVehicle.unit_id];
	UNIT &defenderUnit = Units[defenderVehicle.unit_id];
	
	// triad
	
	Triad attackerTriad = (Triad) attackerUnit.triad();
	Triad defenderTriad = (Triad) defenderUnit.triad();
	
	// get item values
	
	int attackerOffenseValue = getUnitOffenseValue(attackerVehicle.unit_id);
	int defenderDefenseValue = getUnitDefenseValue(defenderVehicle.unit_id);
	
	// determine psi combat
	
	bool psiCombat = attackerOffenseValue < 0 || defenderDefenseValue < 0;
	
	// get combat map tile
	
	MAP *attackerMapTile = getVehicleMapTile(attackerVehicleId);
	MAP *defenderMapTile = getVehicleMapTile(defenderVehicleId);
	
	// run original function
	
	mod_battle_compute(attackerVehicleId, defenderVehicleId, attackerStrengthPointer, defenderStrengthPointer, combat_type);
	
    // ----------------------------------------------------------------------------------------------------
    // conventional air-air weapon-weapon combat uses Air Superiority bonus
    // ----------------------------------------------------------------------------------------------------
	
    if
	(
		Rules->combat_bonus_air_supr_vs_air != 0
		&&
		!psiCombat
		&&
		(attackerTriad == TRIAD_AIR && !attackerUnit.is_missile() && defenderTriad == TRIAD_AIR && !defenderUnit.is_missile())
		&&
		((combat_type & CT_WEAPON_ONLY) && defenderVehicleId >= 0) // weapon-weapon
		&&
		(combat_type & (CT_INTERCEPT|CT_AIR_DEFENSE)) // air-air
	)
	{
		if (has_abil(attackerVehicle->unit_id, ABL_AIR_SUPERIORITY))
		{
			addAttackerBonus(attackerStrengthPointer, Rules->combat_bonus_air_supr_vs_air, label_get(449));
		}
		if (has_abil(defenderVehicle->unit_id, ABL_AIR_SUPERIORITY))
		{
			addDefenderBonus(defenderStrengthPointer, Rules->combat_bonus_air_supr_vs_air, label_get(449));
		}
	}
	
    // ----------------------------------------------------------------------------------------------------
    // psi air-air combat uses psi bonuses
    // ----------------------------------------------------------------------------------------------------
	
    if
	(
		psiCombat
		&&
		(attackerUnit->triad() == TRIAD_AIR && !attackerUnit->is_missile() && defenderUnit->triad() == TRIAD_AIR && !defenderUnit->is_missile())
		&&
		((combat_type & CT_WEAPON_ONLY) && defenderVehicleId >= 0) // weapon-weapon
		&&
		(combat_type & (CT_INTERCEPT|CT_AIR_DEFENSE)) // air-air
	)
	{
		if (has_abil(attackerVehicle->unit_id, ABL_EMPATH))
		{
			addAttackerBonus(attackerStrengthPointer, Rules->combat_bonus_empath_song_vs_psi, Ability[ABL_ID_EMPATH].name);
		}
		if (has_abil(defenderVehicle->unit_id, ABL_TRANCE))
		{
			addDefenderBonus(defenderStrengthPointer, Rules->combat_bonus_empath_song_vs_psi, Ability[ABL_ID_TRANCE].name);
		}
	}
	
    // ----------------------------------------------------------------------------------------------------
    // sensor
    // ----------------------------------------------------------------------------------------------------
	
    // sensor bonus on attack (except probe)
	
	if (conf.sensor_offense && !attackerUnit.is_probe())
	{
		// land or ocean with sensor allowed
		
		if (!is_ocean(defenderMapTile) || conf.sensor_offense_ocean)
		{
			// attacker is in range of friendly sensor and combat is happening on neutral/attacker territory
			
			if (isWithinFriendlySensorRange(attackerVehicle.faction_id, defenderMapTile))
			{
				addAttackerBonus(attackerStrengthPointer, Rules->combat_defend_sensor, *(*tx_labels + LABEL_OFFSET_SENSOR));
			}
			
		}
		
	}
	
    // sensor bonus on defense
    // ocean sensor defense is implemented in Thinker
	
    // ----------------------------------------------------------------------------------------------------
    // artillery duel uses all combat bonuses symmetrically for attacker and defender
    // ----------------------------------------------------------------------------------------------------
	
    if
	(
		conf.artillery_duel_uses_bonuses // fix is enabled
		&&
		((combat_type & CT_WEAPON_ONLY) != 0 && (combat_type & (CT_INTERCEPT|CT_AIR_DEFENSE)) == 0) // artillery duel
	)
	{
		// sensor offense bonus applies to defender within attacker-friendly sensor radius
		
		if (isOffenseSensorBonusApplicable(attackerVehicle.faction_id, defenderMapTile))
		{
			addAttackerBonus(attackerStrengthPointer, Rules->combat_defend_sensor, *(*tx_labels + LABEL_OFFSET_SENSOR));
		}
		
		// sensor defense bonus applies to attacker within defender-friendly sensor radius
		
		if (isDefenseSensorBonusApplicable(defenderVehicle.faction_id, attackerMapTile))
		{
			addDefenderBonus(defenderStrengthPointer, Rules->combat_defend_sensor, *(*tx_labels + LABEL_OFFSET_SENSOR));
		}
		
		// add base defense bonuses to attacker
		
		int attackerBaseId = base_at(attackerVehicle.x, attackerVehicle.y);
		if (attackerBaseId != -1)
		{
			if (psiCombat)
			{
				addAttackerBonus(attackerStrengthPointer, Rules->combat_bonus_intrinsic_base_def, *(*tx_labels + LABEL_OFFSET_BASE));
			}
			else
			{
				// assign label and multiplier
				
				const char *label = nullptr;
				int multiplier = 100;
				
				switch (attackerTriad)
				{
				case TRIAD_LAND:
					if (isBaseHasFacility(attackerBaseId, FAC_PERIMETER_DEFENSE))
					{
						label = *(*tx_labels + LABEL_OFFSET_PERIMETER);
						multiplier += conf.facility_defense_bonus[0];
					}
					break;
					
				case TRIAD_SEA:
					if (isBaseHasFacility(attackerBaseId, FAC_NAVAL_YARD))
					{
						label = LABEL_NAVAL_YARD;
						multiplier += conf.facility_defense_bonus[1];
					}
					break;
					
				case TRIAD_AIR:
					if (isBaseHasFacility(attackerBaseId, FAC_AEROSPACE_COMPLEX))
					{
						label = LABEL_AEROSPACE_COMPLEX;
						multiplier += conf.facility_defense_bonus[2];
					}
					break;
					
				}
				
				if (isBaseHasFacility(attackerBaseId, FAC_TACHYON_FIELD))
				{
					label = *(*tx_labels + LABEL_OFFSET_TACHYON);
					multiplier += conf.facility_defense_bonus[3];
				}
				
				if (label == nullptr)
				{
					// base intrinsic defense
					addAttackerBonus(attackerStrengthPointer, Rules->combat_bonus_intrinsic_base_def, *(*tx_labels + LABEL_OFFSET_BASE));
				}
				else
				{
					// base defensive facility
					addAttackerBonus(attackerStrengthPointer, (multiplier - 100), label);
				}
				
			}
			
		}
		
		// add base defense bonuses to defender
		
		int defenderBaseId = base_at(defenderVehicle.x, defenderVehicle.y);
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
				int multiplier = 100;
				
				switch (attackerTriad)
				{
				case TRIAD_LAND:
					if (isBaseHasFacility(defenderBaseId, FAC_PERIMETER_DEFENSE))
					{
						label = *(*tx_labels + LABEL_OFFSET_PERIMETER);
						multiplier += conf.facility_defense_bonus[0];
					}
					break;
					
				case TRIAD_SEA:
					if (isBaseHasFacility(defenderBaseId, FAC_NAVAL_YARD))
					{
						label = LABEL_NAVAL_YARD;
						multiplier += conf.facility_defense_bonus[1];
					}
					break;
					
				case TRIAD_AIR:
					if (isBaseHasFacility(defenderBaseId, FAC_AEROSPACE_COMPLEX))
					{
						label = LABEL_AEROSPACE_COMPLEX;
						multiplier += conf.facility_defense_bonus[2];
					}
					break;
					
				}
				
				if (isBaseHasFacility(defenderBaseId, FAC_TACHYON_FIELD))
				{
					label = *(*tx_labels + LABEL_OFFSET_TACHYON);
					multiplier += conf.facility_defense_bonus[3];
				}
				
				if (label == nullptr)
				{
					// base intrinsic defense
					addDefenderBonus(defenderStrengthPointer, Rules->combat_bonus_intrinsic_base_def, *(*tx_labels + LABEL_OFFSET_BASE));
				}
				else
				{
					// base defensive facility
					addDefenderBonus(defenderStrengthPointer, (multiplier - 100), label);
				}
				
			}
			
		}
		
	}
	
    // ----------------------------------------------------------------------------------------------------
    // conventional power contributes to psi combat
    // ----------------------------------------------------------------------------------------------------
	
    if (psiCombat && conf.conventional_power_psi_percentage > 0 && attackerUnit.offense_value() > 0)
	{
		addAttackerBonus(attackerStrengthPointer, conf.conventional_power_psi_percentage * attackerUnit.offense_value(), LABEL_WEAPON);
	}
	
    if (psiCombat && conf.conventional_power_psi_percentage > 0 && defenderUnit.defense_value() > 0)
	{
		addDefenderBonus(defenderStrengthPointer, conf.conventional_power_psi_percentage * defenderUnit.defense_value(), LABEL_ARMOR);
	}
	
    // ----------------------------------------------------------------------------------------------------
    // AAA range effect
    // ----------------------------------------------------------------------------------------------------
	
    if (attackerTriad == TRIAD_AIR && defenderTriad != TRIAD_AIR && !isUnitHasAbility(defenderVehicle.unit_id, ABL_AAA) && conf.aaa_range >= 0)
	{
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			
			if (vehicle->faction_id != defenderVehicle.faction_id)
				continue;
			
			if (!isVehicleHasAbility(vehicleId, ABL_AAA))
				continue;
			
			int range = map_range(defenderVehicle.x, defenderVehicle.y, vehicle->x, vehicle->y);
			
			if (range > conf.aaa_range)
				continue;
			
			addDefenderBonus(defenderStrengthPointer, Rules->combat_aaa_bonus_vs_air, *(*tx_labels + LABEL_OFFSET_TRACKING));
			
			break;
			
		}
		
	}
	
    // ----------------------------------------------------------------------------------------------------
    // SE MORALE combat bonus
    // ----------------------------------------------------------------------------------------------------
    
    if (conf.se_morale_combat_bonus != 0)
	{
		// regular combat units only, not probes
		
		if (isRegularUnit(attackerVehicle.unit_id) && isCombatUnit(attackerVehicle.unit_id) && !isProbeUnit(attackerVehicle.unit_id))
		{
			int attackerSEMorale = Factions[attackerVehicle.faction_id].SE_morale;
			int attackerBonus = conf.se_morale_combat_bonus * attackerSEMorale;
			
			if (attackerBonus != 0)
			{
				addAttackerBonus(attackerStrengthPointer, attackerBonus, "MORALE");
			}
			
		}
		
		// combat units only, not probes
		
		if (isRegularUnit(defenderVehicle.unit_id) && isCombatUnit(defenderVehicle.unit_id) && !isProbeUnit(defenderVehicle.unit_id))
		{
			int defenderSEMorale = Factions[defenderVehicle.faction_id].SE_morale;
			int defenderBonus = conf.se_morale_combat_bonus * defenderSEMorale;
			
			if (defenderBonus != 0)
			{
				addDefenderBonus(defenderStrengthPointer, defenderBonus, "MORALE");
			}
			
		}
		
	}
	
    // ----------------------------------------------------------------------------------------------------
    // probe combat uses sensors and intrinsic base defense
    // ----------------------------------------------------------------------------------------------------
	
    if (conf.probe_combat_uses_bonuses && attackerUnit.is_probe() && defenderUnit.is_probe()) // probe combat
	{
		// attacker probe benefits from sensor offense bonus
		
		if (isOffenseSensorBonusApplicable(attackerVehicle.faction_id, defenderMapTile))
		{
			addAttackerBonus(attackerStrengthPointer, Rules->combat_defend_sensor, *(*tx_labels + LABEL_OFFSET_SENSOR));
		}
		
		// defender probe benefits from sensor defense bonus
		
		if (isDefenseSensorBonusApplicable(defenderVehicle.faction_id, defenderMapTile))
		{
			addDefenderBonus(defenderStrengthPointer, Rules->combat_defend_sensor, *(*tx_labels + LABEL_OFFSET_SENSOR));
		}
		
		// defender probe benefits from base intrinsic defense bonus
		
		int defenderBaseId = base_at(defenderVehicle.x, defenderVehicle.y);
		if (defenderBaseId != -1)
		{
			addDefenderBonus(defenderStrengthPointer, Rules->combat_bonus_intrinsic_base_def, label_get(332)/*Base*/);
		}
		
	}
	
    // ----------------------------------------------------------------------------------------------------
    // defense facilities extend their effect 2 tiles outside of the base
    // ----------------------------------------------------------------------------------------------------
	
	constexpr char const *triadFacilityFieldLabels[3] = {"PD - field", "NY - field", "AC - field", };
	constexpr char const *tachyonFieldFieldLabel = "TF - field";
	
	// blink displacer ignores defensive facilities
	if (!has_abil(attackerVehicle.unit_id, ABL_BLINK_DISPLACER))
	{
		int facilityFieldBonus = 0;
		char const *facilityFieldLabel = nullptr;
		if (isFriendlyBaseInRangeHasFacility(defenderVehicle.faction_id, defenderVehicle.x, defenderVehicle.y, 2, TRIAD_DEFENSIVE_FACILITIES[attackerTriad]))
		{
			facilityFieldBonus += conf.facility_field_defense_bonus[attackerTriad];
			facilityFieldLabel = triadFacilityFieldLabels[attackerTriad];
		}
		if (isFriendlyBaseInRangeHasFacility(defenderVehicle.faction_id, defenderVehicle.x, defenderVehicle.y, 2, FAC_TACHYON_FIELD))
		{
			facilityFieldBonus += conf.facility_field_defense_bonus[3];
			facilityFieldLabel = tachyonFieldFieldLabel;
		}
		
		if (facilityFieldBonus > 0)
		{
			addDefenderBonus(defenderStrengthPointer, facilityFieldBonus, facilityFieldLabel);
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
Prototype cost calculation.

Sea transport, colony, former, supply chassis cost similar to their land counterparts.
foil	= infantry
cruiser	= speeder

Buster missile (Planet Buster, Tectonic Payload, Fungal Payload) cost is multiplied by reactor.
This is a special case because their power is directly proportional to reactor.

- Calculate module and weapon/armor reactor modified costs.
module cost is unchanged
buster cost			= item cost * reactor
weapon/armor cost	= item cost * (reactor value / Fission reactor value)

- Select primary and secondary item
primary item = the one with higher reactor modified cost
secondary item = the one with lower reactor modified cost

- Calculate primary item cost and secondary item shifted cost
primary item cost	= item cost
secondary item cost	= item cost - 1

- Calculate unit base cost
unit base cost		= [<primary item cost> + <secondary item cost> / 2] * <chassis cost> / <infrantry chassis cost>

- Multiply by ability cost factor
ability bytes 0-3 is cost factor

- Add ability flat cost
ability bytes 4-7 is flat cost

- Round down

*/
__cdecl int wtp_mod_proto_cost(int chassis_id, int weapon_id, int armor_id, int abilities, int reactor)
{
    double weapon_cost	= (double)Weapon[weapon_id].cost;
    double armor_cost	= (double)Armor[armor_id].cost;
    double chassis_cost	= (double)Chassis[chassis_id].cost;
	
    // reactor cost factors
	
    double reactor_cost_factor = (double)conf.reactor_cost_factors[reactor - 1] / (double)conf.reactor_cost_factors[0];
	
    // component values
	
    int weapon_offence_value = Weapon[weapon_id].offense_value;
    int armor_defense_value = Armor[armor_id].defense_value;
    int chassis_speed = Chassis[chassis_id].speed;
	
    // modified component costs
    
    double weaponModifiedCost;
    switch (weapon_id)
    {
	case WPN_PLANET_BUSTER:
	case WPN_TECTONIC_PAYLOAD:
	case WPN_FUNGAL_PAYLOAD:
		weaponModifiedCost = weapon_cost * (double)reactor;
		break;
	case WPN_TROOP_TRANSPORT:
	case WPN_COLONY_MODULE:
	case WPN_TERRAFORMING_UNIT:
	case WPN_SUPPLY_TRANSPORT:
	case WPN_PROBE_TEAM:
	case WPN_ALIEN_ARTIFACT:
		weaponModifiedCost = weapon_cost;
		break;
	default:
		weaponModifiedCost = weapon_cost * reactor_cost_factor;
    }
    
    double armorModifiedCost;
    switch (weapon_id)
    {
	case WPN_PROBE_TEAM:
		armorModifiedCost = armor_cost * reactor_cost_factor * 0.5;
		break;
	default:
		armorModifiedCost = armor_cost * reactor_cost_factor;
    }
	
    // modified chassis cost for sea based non combat related modules
	
    double chassisModifiedCost;
    switch (weapon_id)
    {
	case WPN_TROOP_TRANSPORT:
	case WPN_COLONY_MODULE:
	case WPN_TERRAFORMING_UNIT:
	case WPN_SUPPLY_TRANSPORT:
		switch (chassis_id)
		{
		case CHS_FOIL:
			chassisModifiedCost = Chassis[CHS_INFANTRY].cost;
			break;
		case CHS_CRUISER:
			chassisModifiedCost = Chassis[CHS_SPEEDER].cost;
			break;
		default:
			chassisModifiedCost = chassis_cost;
		}
		break;
	default:
		chassisModifiedCost = chassis_cost;
    }
    
    double chassisCostFactor = chassisModifiedCost / (double)Chassis[CHS_INFANTRY].cost;
    
    // primary item and secondary item shifted costs
	
    double primary_item_cost;
    double secondary_item_cost;
	
    if (weaponModifiedCost >= armorModifiedCost)
	{
		primary_item_cost	= weaponModifiedCost;
		secondary_item_cost	= armorModifiedCost;
	}
	else
	{
		primary_item_cost	= armorModifiedCost;
		secondary_item_cost	= weaponModifiedCost;
	}
	
    // set minimal cost to reactor level (this is checked in some other places so we should do this here to avoid any conflicts)
	
    int minimal_cost = reactor;
	
    // calculate base unit cost without abilities
	
    double base_cost = (primary_item_cost + 0.5 * secondary_item_cost) * chassisCostFactor;
	
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
	
	int finalCost = (int)floor(base_cost * (1.0 + 0.25 * (double)abilities_cost_factor)) + abilities_cost_addition;
    int cost = std::max(minimal_cost, finalCost);
	
    return cost;
	
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
        * (double)Rules->tech_discovery_rate / 100.0
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

__cdecl int sayBase(int buffer, int baseId)
{
	// execute original code
	
	int returnValue = say_base(buffer, baseId);
	
	// get base
	
	BASE *base = &(Bases[baseId]);
	
	// get base population limit
	
	int populationLimit = getBasePopulationLimit(baseId);
	
	// generate symbol
	
	char symbol[10] = "   <P>";
	
	if (populationLimit != -1 && base->pop_size >= populationLimit)
	{
		strcat((char *)buffer, symbol);
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
	
	int baseId = mod_base_init(factionId, x, y);
	
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
	
	return output;
	
}

/*
Wraps social_calc.
This is initially for CV GROWTH effect.
*/
__cdecl void modifiedSocialCalc(int seSelectionsPointer, int seRatingsPointer, int factionId, int ignored4, int seChoiceEffectOnly)
{
	// old cost factors (for accumulated resources adjustment below)
	
	int nutrient_cost_factor_old = mod_cost_factor(factionId, RSC_NUTRIENT, -1);
	int mineral_cost_factor_old = mod_cost_factor(factionId, RSC_MINERAL, -1);
	
	// execute original code
	
	social_calc((CSocialCategory *) seSelectionsPointer, (CSocialEffect *) seRatingsPointer, factionId, ignored4, seChoiceEffectOnly);
	
	// CV changes GROWTH rate if configured
	
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
	
	// excess police industry bonus
	
	if (conf.se_police_excess_industry_bonus != 0)
	{
		int *sePoliceRating = (int *)seRatingsPointer + (SE_POLICE - SE_ECONOMY);
		int *seIndustryRating = (int *)seRatingsPointer + (SE_INDUSTRY - SE_ECONOMY);
		
		int sePoliceExcessRating = (*sePoliceRating > +3 ? *sePoliceRating - 3 : (*sePoliceRating < -5 ? *sePoliceRating + 5 : 0));
		int seIndustryBonus = conf.se_police_excess_industry_bonus * sePoliceExcessRating;
		
		*seIndustryRating += seIndustryBonus;
		
	}
	
	// new cost factors (for accumulated resources adjustment below)
	
	int nutrient_cost_factor_new = mod_cost_factor(factionId, RSC_NUTRIENT, -1);
	int mineral_cost_factor_new = mod_cost_factor(factionId, RSC_MINERAL, -1);
	
	debug
	(
		"modifiedSocialCalc: factionId=%d, nutrient_cost_factor: %+d -> %+d, mineral_cost_factor_new: %+d -> %+d\n"
		, factionId
		, nutrient_cost_factor_old
		, nutrient_cost_factor_new
		, mineral_cost_factor_old
		, mineral_cost_factor_new
	)
	;
	
	// adjust accumulated resources for human faction
	
	if (is_human(factionId))
	{
		// adjust nutrients
		
		if (nutrient_cost_factor_new != nutrient_cost_factor_old)
		{
			for (int baseId = 0; baseId < *BaseCount; baseId++)
			{
				BASE* base = &Bases[baseId];
				
				if (base->faction_id != factionId)
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
				
				if (base->faction_id != factionId)
					continue;
				
				base->minerals_accumulated = (base->minerals_accumulated * mineral_cost_factor_new + ((mineral_cost_factor_old - 1) / 2)) / mineral_cost_factor_old;
				
			}
			
		}
		
	}
	
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
	
	int nutrientCostFactor = mod_cost_factor(base->faction_id, RSC_NUTRIENT, baseid);
	
    // modify output
	
    char *destinationString = (char *)destinationStringPointer;
//	sprintf(destinationString + strlen("NUT"), " (%+1d) %+1d => %02d", faction->SE_growth_pending, *BaseGrowthRate, nutrientCostFactor);
	sprintf(destinationString, "GR: %+1d %+1d    || %02d", faction->SE_growth_pending, *BaseGrowthRate - faction->SE_growth_pending, nutrientCostFactor);
	
}

/*
Replaces growth turn indicator with correct information for pop boom and stagnation.
*/
__cdecl void correctGrowthTurnsIndicator(int destinationStringPointer, int sourceStringPointer)
{
    // call original function
	
    tx_strcat(destinationStringPointer, sourceStringPointer);
	
    if (base_pop_boom(*CurrentBaseID))
	{
		// update indicator for population boom
		
		strcpy((char *)destinationStringPointer, "-- POP BOOM --");
		
	}
    else if (*BaseGrowthRate < conf.se_growth_rating_min)
	{
		// update indicator for stagnation
		
		strcpy((char *)destinationStringPointer, "-- STAGNANT --");
		
	}
	
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
	return mod_cost_factor(*CurrentFaction, RSC_MINERAL, -1);
}

__cdecl void displayArtifactMineralContributionInformation(int input_string_pointer, int output_string_pointer)
{
	// execute original function
	
	tx_parse_string(input_string_pointer, output_string_pointer);
	
	// calculate artifact mineral contribution
	
	int artifactMineralContribution = Units[BSC_ALIEN_ARTIFACT].cost * mod_cost_factor(*CurrentFaction, RSC_MINERAL, -1) / 2;
	
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

	int mineralCostFactor = mod_cost_factor(factionId, RSC_MINERAL, -1);

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
		// Procure Research Data
	case 1:
		*risk = conf.probe_risk_procure_research_data;
		break;
		// Introduce Genetic Plague
	case 7:
		*risk = conf.probe_risk_introduce_genetic_plague;
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
		Units[Vehs[attackerVehicleId].unit_id].weapon_id != WPN_PROBE_TEAM
		&&
		attackerTriad == TRIAD_SEA && defenderTriad == TRIAD_SEA && bombardment == 0
	)
	{
		// iterate the stack
		
		double bestDefenderEffectiveness = 0.0;
		
		for (int stackedVehicleId : getStackVehicleIds(defenderVehicleId))
		{
			VEH *stackedVehicle = &(Vehs[stackedVehicleId]);
			UNIT *stackedVehicleUnit = &(Units[stackedVehicle->unit_id]);
			
			int attackerOffenseValue;
			int defenderStrength;
			wtp_mod_battle_compute(attackerVehicleId, stackedVehicleId, &attackerOffenseValue, &defenderStrength, 0);
			
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
		bestDefenderVehicleId = mod_best_defender(defenderVehicleId, attackerVehicleId, bombardment);
	}
	
	// return best defender
	
	return bestDefenderVehicleId;
	
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
int __cdecl wtp_mod_battle_fight_2(int veh_id_atk, int offset, int tx, int ty, int table_offset, int option, int* def_id)
//(int attackerVehicleId, int angle, int tx, int ty, int do_arty, int flag1, int flag2)
{
	debug("modifiedBattleFight2(attackerVehicleId=%d, angle=%d, tx=%d, ty=%d, do_arty=%d, option=%d)\n", veh_id_atk, offset, tx, ty, table_offset, option);
	
	VEH *attackerVehicle = &Vehs[veh_id_atk];
	
	if (attackerVehicle->faction_id == *CurrentPlayerFaction)
	{
		debug("\tattackerVehicle->faction_id == *CurrentPlayerFaction\n");
		int defenderVehicleId = veh_at(tx, ty);
		
		if (defenderVehicleId >= 0)
		{
			bool nonArtifactUnitExists = false;
			
			std::vector<int> stackedVehicleIds = getStackVehicleIds(defenderVehicleId);
			
			for (int vehicleId : stackedVehicleIds)
			{
				if (Units[Vehs[vehicleId].unit_id].weapon_id != WPN_ALIEN_ARTIFACT)
				{
					nonArtifactUnitExists = true;
					break;
				}
			}
			
			if (nonArtifactUnitExists)
			{
				VEH *defenderVehicle = &(Vehs[defenderVehicleId]);
				
				int treatyNotBroken = tx_break_treaty(attackerVehicle->faction_id, defenderVehicle->faction_id, 0xB);
				
				if (treatyNotBroken)
					return 0;
				
				// break treaty really
				
				tx_act_of_aggression(attackerVehicle->faction_id, defenderVehicle->faction_id);
				
			}
			
		}
		
	}
	
	// execute Thinker mod version
	
	return mod_battle_fight_2(veh_id_atk, offset, tx, ty, table_offset, option, def_id);
	
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

/*
Disables air transport unload everywhere.
Pretends that air transport has zero cargo capacity when in not appropriate unload location.
*/
__cdecl int modifiedVehicleCargoForAirTransportUnload(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
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
__cdecl void modifiedFactionUpkeep(const int factionId)
{
	debug("modifiedFactionUpkeep - %s\n", getMFaction(factionId)->noun_faction);
	
	Profiling::start("modifiedFactionUpkeep", "");
	
    // remove wrong units from bases
	
    if (factionId > 0)
	{
		Profiling::start("removeWrongVehiclesFromBases", "modifiedFactionUpkeep");
		removeWrongVehiclesFromBases();
		Profiling::stop("removeWrongVehiclesFromBases");
	}
	
	// choose AI logic
	
	clearPlayerFactionReferences();
	
	if (isWtpEnabledFaction(factionId))
	{
		// run WTP AI code for AI eanbled factions
		
		Profiling::start("aiFactionUpkeep", "modifiedFactionUpkeep");
		aiFactionUpkeep(factionId);
		Profiling::stop("aiFactionUpkeep");
		
	}
	
	Profiling::pause("modifiedFactionUpkeep");
	
	// execute original function
	
	mod_faction_upkeep(factionId);
	
	Profiling::resume("modifiedFactionUpkeep");
	
    // fix vehicle home bases for normal factions
	
    if (factionId > 0)
	{
		Profiling::start("fixVehicleHomeBases", "modifiedFactionUpkeep");
		fixVehicleHomeBases(factionId);
		Profiling::stop("fixVehicleHomeBases");
	}
	
	// improve broken relationships
	
	int factionRevengeForgiveChance = is_human(factionId) ? conf.diplomacy_improvement_chance_human_faction_revenge : conf.diplomacy_improvement_chance_ai_faction_revenge;
	int majorAtrocityForgiveChance = is_human(factionId) ? conf.diplomacy_improvement_chance_human_major_atrocity : conf.diplomacy_improvement_chance_ai_major_atrocity;
	
	if (factionRevengeForgiveChance > 0)
	{
		for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
		{
			if (otherFactionId == factionId)
				continue;
			
			for (int diplo_flag : {DIPLO_WANT_REVENGE, DIPLO_UNK_40, DIPLO_ATROCITY_VICTIM, DIPLO_WANT_REVENGE})
			{
				if (has_treaty(otherFactionId, factionId, diplo_flag))
				{
					if (random(100) < factionRevengeForgiveChance)
					{
						set_treaty(otherFactionId, factionId, diplo_flag, 0);
					}
					
				}
				
			}
			
		}
		
	}
	
	if (majorAtrocityForgiveChance > 0)
	{
		if (Factions[factionId].major_atrocities > 0)
		{
			if (random(100) < majorAtrocityForgiveChance)
			{
				Factions[factionId].major_atrocities--;
			}
			
		}
		
	}
	
	// return original value
	
	Profiling::stop("modifiedFactionUpkeep");
	
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
	
	return mod_base_making(item, baseId);
	
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

	mindControlCost *= 1.0 + (0.1 * (double)(baseFaction->mind_control_total / 4));

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
		VEH *vehicle = getVehicle(vehicleId);

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
	VEH *vehicle = getVehicle(vehicleId);

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
	VEH *vehicle = getVehicle(vehicleId);
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
		VEH *otherVehicle = getVehicle(otherVehicleId);

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
Intercept probe call to record actors.
*/
void __cdecl wtp_mod_probe(int probeVehicleId, int targetBaseId, int targetVehicleId, int flags)
{
	probe_probeVehicleId = probeVehicleId;
	probe_targetBaseId = targetBaseId;
	probe_targetVehicleId = targetVehicleId;
	
	// execute original function
	
	probe(probeVehicleId, targetBaseId, targetVehicleId, flags);
	
}

/*
Moves subverted vehicle on probe tile.
*/
void __cdecl modifiedSubveredVehicleDrawTile(int x, int y, int radius)
{
	// vehicle targeted
	if (probe_targetVehicleId >= 0)
	{
		// get vehicles
		
		VEH *probeVehicle = getVehicle(probe_probeVehicleId);
		VEH *targetVehicle = getVehicle(probe_targetVehicleId);
		
		// vehicle subverted
		
		if (targetVehicle->faction_id == probeVehicle->faction_id)
		{
			// move subverted vehicle to probe tile
			
			veh_put(probe_targetVehicleId, probeVehicle->x, probeVehicle->y);
			
			// stop probe
			
			probe_probeMovesSpent = probeVehicle->moves_spent + Rules->move_rate_roads;
			probeVehicle->moves_spent = veh_speed(probe_probeVehicleId, false);
			
		}
		
	}
	
	// execute original function
	
	draw_tile(x, y, radius);
	
}

void __cdecl modifiedTurnUpkeep()
{
	// executionProfiles
	
	if (DEBUG)
	{
		Profiling::print();
		Profiling::reset();
	}
	
	// collect statistics
	
	if (DEBUG)
	{
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
			
			int nutrientCostFactor = mod_cost_factor(base->faction_id, RSC_NUTRIENT, baseId);
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
		
	}
	
	// execute original function
	
	mod_turn_upkeep();
	
}

/*
Exclude sensor from destruction menu.
*/
int __thiscall wtp_mod_Console_destroy(Console *This, int vehicleId)
{
	MAP *tile = getVehicleMapTile(vehicleId);
	bool sensor = map_has_item(tile, BIT_SENSOR);
	
	if (conf.sensor_indestructible)
	{
		// temporarily remove sensor if exists
		
		if (sensor)
		{
			tile->items &= ~BIT_SENSOR;
		}
		
	}
	
	int returnValue = Console_destroy(This, vehicleId);
	
	if (conf.sensor_indestructible)
	{
		// restore sensor
		
		if (sensor)
		{
			tile->items |= BIT_SENSOR;
		}
		
	}
	
	return returnValue;
	
}

/*
Exclude sensor from destruction action.
*/
int __cdecl wtp_mod_action_destroy(int vehicleId, int terrainBit, int x, int y)
{
	MAP *tile = isOnMap(x, y) ? getMapTile(x, y) : getVehicleMapTile(vehicleId);
	bool sensor = map_has_item(tile, BIT_SENSOR);
	
	if (conf.sensor_indestructible)
	{
		// temporarily remove sensor if exists
		
		if (sensor)
		{
			tile->items &= ~BIT_SENSOR;
		}
		
	}
	
	int returnValue = action_destroy(vehicleId, terrainBit, x, y);
	
	if (conf.sensor_indestructible)
	{
		// restore sensor
		
		if (sensor)
		{
			tile->items |= BIT_SENSOR;
		}
		
	}
	
	return returnValue;
	
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
		VEH *vehicle = getVehicle(vehicleId);
		
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
	int x = getX(dst);
	int y = getY(dst);
	
	if (is_human(factionId))
	{
		// reset probeMovesSpent
		
		probe_probeMovesSpent = -1;
		
		// execute original function
		
		int returnValue = tx_order_veh(vehicleId, angle, a3);
		
		// set probe moves
		
		if (probe_probeMovesSpent >= 0)
		{
			VEH *probeVehicle = getVehicle(vehicleId);
			
			// set probe moves
			
			probeVehicle->moves_spent = probe_probeMovesSpent;
			
			// reset probe_probeMovesSpent
			
			probe_probeMovesSpent = -1;
			
		}
		
		return returnValue;
		
	}
	else
	{
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
		
		int destinationVehicleFactionId = veh_who(x, y);
		bool destinationBlocked = destinationVehicleFactionId == -1 ? false : !isFriendly(factionId, destinationVehicleFactionId);
		
		if (!destinationBlocked && triad == TRIAD_LAND && zocAffectedVehicle && !is_ocean(src) && !is_ocean(dst) && isZoc(factionId, src, dst))
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
		
		// execute original function if nothing else applied or worked
		
		return tx_order_veh(vehicleId, angle, a3);
		
	}
	
}

/*
Verifies that given vehicle can move to this angle.
*/
bool isValidMovementAngle(int vehicleId, int angle)
{
	VEH *vehicle = getVehicle(vehicleId);

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
	VEH *vehicle = getVehicle(vehicleId);

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

	for (int targetTileStackVehicleId : getStackVehicleIds(targetTileVehicleId))
	{
		VEH *targetTileStackVehicle = &(Vehs[targetTileStackVehicleId]);

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
		VEH *vehicle = getVehicle(vehicleId);
		
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
		mod_veh_skip(vehicleId);
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
		
		if (isTileAccessesWater(vehicleTile))
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
int __cdecl modified_kill(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	
	// do not allow computer kill AI vehicle
	
	if (isWtpEnabledFaction(vehicle->faction_id))
		return 0;
	
	// execute original code
	
	return kill(vehicleId);
	
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
void addAttackerBonus(int *strengthPointer, double bonus, const char *label)
{
	// bonus cannot be lower than -100
	
	bonus = std::max(-100.0, bonus);
	
	// modify strength
	
	*strengthPointer = (int)round((double)(*strengthPointer) * (100.0 + bonus) / 100.0);
	
	// add effect description
	
	if (*tx_battle_compute_attacker_effect_count < 4)
	{
		strcpy((*tx_battle_compute_attacker_effect_labels)[*tx_battle_compute_attacker_effect_count], label);
		(*tx_battle_compute_attacker_effect_values)[*tx_battle_compute_attacker_effect_count] = (int)floor(bonus);
		(*tx_battle_compute_attacker_effect_count)++;
	}
	
}
void addDefenderBonus(int *strengthPointer, double bonus, const char *label)
{
	bonus = std::max(-100.0, bonus);
	
	// modify strength
	
	*strengthPointer = (int)round((double)(*strengthPointer) * (100.0 + bonus) / 100.0);
	
	// add effect description
	
	if (*tx_battle_compute_defender_effect_count < 4)
	{
		strcpy((*tx_battle_compute_defender_effect_labels)[*tx_battle_compute_defender_effect_count], label);
		(*tx_battle_compute_defender_effect_values)[*tx_battle_compute_defender_effect_count] = (int)floor(bonus);
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
		Profiling::start("- modified_base_check_support - computeBase");
		computeBase(baseId, false);
		Profiling::stop("- modified_base_check_support - computeBase");
		
		debug("\t%-25s %-32s killed because of oversupport\n", base->name, getVehicle(cheapestVehicleId)->name());
		
	}
	
	// original function
	
	base_check_support();
	
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

/*
BaseWin psych row.
Does no draw specialists.
*/
int __thiscall wtp_mod_BaseWin_psych_row(Win* This, int horizontal_pos, int vertical_pos, int a4, int a5, int talents, int drones, int sdrones)
{
	BASE *base = *CurrentBase;
	
	// save current specialist count
	
	int specialist_total = base->specialist_total;
	
	// pretend there are no specialists
	base->specialist_total = 0;
	
	// execute function
	
	int returnValue = BaseWin_psych_row(This, horizontal_pos, vertical_pos, a4, a5, talents, drones, sdrones);
	
	// restore specialist count
	base->specialist_total = specialist_total;
	
	return returnValue;
	
}

/*
BaseWin pop click.
Displays popup for base size >= min_base_size_specialists.
*/
int __thiscall wtp_mod_BaseWin_pop_click_popup_start(Win* This, const char* filename, const char* label, int a4, int a5, int a6, int a7)
{
	BASE *base = *CurrentBase;
	
	if (base->pop_size >= Rules->min_base_size_specialists)
	{
		return Popup_start(This, filename, "CITIZEN3", a4, a5, a6, a7);
	}
	
	return Popup_start(This, filename, label, a4, a5, a6, a7);
	
}

///*
//BaseWin pop click.
//Disable specialist selection if no content citizens.
//*/
//int basewin_pop_click_citizen_id = 0;
//char * __cdecl wtp_mod_BaseWin_pop_click_specialist_cycle_strcat(char *dst, char const *src)
//{
//	basewin_pop_click_citizen_id = 0;
//	return strcat(dst, src);
//}
//int __cdecl wtp_mod_BaseWin_pop_click_specialist_has_tech(int tech_id, int faction_id)
//{
//	BASE *base = *CurrentBase;
//	CCitizen *citizen = &Citizen[basewin_pop_click_citizen_id];
//	basewin_pop_click_citizen_id++;
//	
//	if (base->pop_size >= Rules->min_base_size_specialists && base->drone_total >= base->pop_size - base->specialist_total && citizen->psych_bonus < 2)
//	{
//		return 0;
//	}
//	
//	return has_tech(tech_id, faction_id) ? 1 : 0;
//	
//}

int __cdecl wtp_mod_base_police_pending()
{
	return Factions[(*CurrentBase)->faction_id].SE_police_pending + (has_facility(FAC_BROOD_PIT, *CurrentBaseID) ? 2 : 0);
}

/**
Returns current base item hurry mineral cost.
Could be either complete cost or minimal depending on the settings.
*/
int getHurryMineralCost(int mineralCost)
{
	int hurryMineralCost = mineralCost;
	
	// allow minimal hurry if configured and is base is not rioting
	
	if (conf.hurry_minimal_minerals && !(*CurrentBase)->drone_riots_active())
	{
		// reduce hurry mineral cost by mineral surplus to complete production next turn
		
		hurryMineralCost = std::max((*CurrentBase)->minerals_accumulated, mineralCost - std::max(0, (*CurrentBase)->mineral_surplus));
		
	}
	
	return hurryMineralCost;
	
}

/**
Returns current base item hurry minimal mineral cost to complete production on next turn.
*/
int __cdecl wpt_mod_Base_hurry_cost_factor_mineral_cost(int itemCost, int /*a1*/, int /*a2*/)
{
	return getHurryMineralCost(mod_cost_factor((*CurrentBase)->faction_id, RSC_MINERAL, -1) * itemCost);
}

int __thiscall wtp_mod_Console_human_turn(Console *This)
{
	// control human player automated units with AI algorithms
	
	if (conf.manage_player_units)
	{
		// set AI faction id for global reference
		
		setPlayerFactionReferences(*CurrentPlayerFaction);
		
		// generate automation strategy
		
		strategy(false);
		
	}
	
	// continue with normail play
	
	return Console_human_turn(This);
	
}

/*
Disables the call from action_terraform.
*/
int __cdecl wtp_mod_action_terraform_cause_friction(int /*a1*/, int /*a2*/, int /*a3*/)
{
	return 0;
}

/*
Disables the call from action_terraform.
*/
int __cdecl wtp_mod_action_terraform_set_treaty(int /*a1*/, int /*a2*/, int /*a3*/, int /*a4*/)
{
	return 0;
}

/*
Patches enemy diplomacy.
*/
int __cdecl wtp_mod_enemy_diplomacy(int factionId)
{
	MFaction *mFaction = getMFaction(factionId);
	Faction *faction = getFaction(factionId);
	
	int returnValue = enemy_diplomacy(factionId);
	
	// adjust aggressiveness effect on diplomacy relation
	
	if (conf.diplomacy_aggressiveness_effect_interval != 1 && faction->AI_fight != 0)
	{
		bool oldTrigger = (*CurrentTurn % 1) == 0;
		bool newTrigger = (*CurrentTurn % conf.diplomacy_aggressiveness_effect_interval) == 0;
		
		if (oldTrigger || newTrigger)
		{
			for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
			{
				if (otherFactionId == factionId)
					continue;
				
				if (!is_human(otherFactionId))
					continue;
				
				if (!has_treaty(factionId, otherFactionId, DIPLO_COMMLINK))
					continue;
				
				if (oldTrigger)
				{
					faction->diplo_friction[otherFactionId] -= (faction->AI_fight);
				}
				
				if (newTrigger)
				{
					faction->diplo_friction[otherFactionId] += (faction->AI_fight);
				}
				
			}
			
		}
		
	}
	
	// adjust SE choice effect on diplomacy relation
	
	if (conf.diplomacy_se_choice_effect_interval != 2 && mFaction->soc_priority_category >= 0 && mFaction->soc_priority_model > 0)
	{
		bool oldTrigger = (*CurrentTurn % 2) == 0;
		bool newTrigger = (*CurrentTurn % conf.diplomacy_se_choice_effect_interval) == 0;
		
		if (oldTrigger || newTrigger)
		{
			for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
			{
				Faction *otherFaction = getFaction(otherFactionId);
				
				if (otherFactionId == factionId)
					continue;
				
				if (!is_human(otherFactionId))
					continue;
				
				if (!has_treaty(factionId, otherFactionId, DIPLO_COMMLINK))
					continue;
				
				if (oldTrigger)
				{
					int otherFactionPriorityModel = (&otherFaction->SE_Politics_pending)[mFaction->soc_priority_category];
					
					int oldPriorityChange = 0;
					if (otherFactionPriorityModel == 0)
					{
					}
					else if (otherFactionPriorityModel == mFaction->soc_priority_model)
					{
						oldPriorityChange = -1;
					}
					else if (otherFactionPriorityModel != mFaction->soc_priority_model)
					{
						oldPriorityChange = +1;
					}
					faction->diplo_friction[otherFactionId] -= oldPriorityChange;
				}
				
				if (newTrigger)
				{
					int otherFactionPriorityModel = (&otherFaction->SE_Politics_pending)[mFaction->soc_priority_category];
					
					int newPriorityChange = 0;
					if (otherFactionPriorityModel == 0)
					{
					}
					else if (otherFactionPriorityModel == mFaction->soc_priority_model)
					{
						newPriorityChange = -1;
					}
					else if (otherFactionPriorityModel != mFaction->soc_priority_model)
					{
					}
					faction->diplo_friction[otherFactionId] += newPriorityChange;
					
					if (mFaction->soc_opposition_category >= 0 && mFaction->soc_opposition_model > 0)
					{
						int otherFactionOppositionModel = (&otherFaction->SE_Politics_pending)[mFaction->soc_opposition_category];
						
						int newOppositionChange = 0;
						if (otherFactionOppositionModel == 0)
						{
						}
						else if (otherFactionOppositionModel == mFaction->soc_opposition_model)
						{
							newOppositionChange = +1;
						}
						else if (otherFactionOppositionModel != mFaction->soc_opposition_model)
						{
						}
						faction->diplo_friction[otherFactionId] += newOppositionChange;
						
					}
					
				}
				
			}
			
		}
		
	}
	
	return returnValue;
	
}

/*
Increases global friction as a probe actions consequence.
*/
void __cdecl wtp_mod_probe_treaty_on(int faction1Id, int faction2Id, int treaty)
{
	debug("wtp_mod_probe_treaty_on\n");
	
	treaty_on(faction1Id, faction2Id, treaty);
	
	if (conf.diplomacy_probe_action_vendetta_global_friction <= 0)
		return;
	
	bool human1 = is_human(faction1Id);
	bool human2 = is_human(faction2Id);
	
	// human vs AI
	
	if (human1 != human2)
	{
		int randomBound = conf.diplomacy_probe_action_vendetta_global_friction + 1;
		
		int humanFactionId = human1 ? faction1Id : faction2Id;
		int aiFactionId = !human1 ? faction1Id : faction2Id;
		
		for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
		{
			if (otherFactionId == faction1Id || otherFactionId == faction2Id)
				continue;
			
			if (is_human(otherFactionId))
				continue;
			
			if (!has_treaty(otherFactionId, humanFactionId, DIPLO_COMMLINK) || !has_treaty(humanFactionId, otherFactionId, DIPLO_COMMLINK))
				continue;
			
			// worsen relation to human
			
			int value0 = random(randomBound);
			cause_friction(otherFactionId, humanFactionId, value0);
			debug("\tcause_friction %-24s -> %-24s : %+2d\n", MFactions[otherFactionId].noun_faction, MFactions[humanFactionId].noun_faction, value0);
			
			if (Factions[otherFactionId].diplo_friction[humanFactionId] >= 10)
			{
				treaty_on(otherFactionId, humanFactionId, DIPLO_VENDETTA);
			}
			
		}
		for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
		{
			if (otherFactionId == faction1Id || otherFactionId == faction2Id)
				continue;
			
			if (is_human(otherFactionId))
				continue;
			
			if (!has_treaty(otherFactionId, aiFactionId, DIPLO_COMMLINK) || !has_treaty(aiFactionId, otherFactionId, DIPLO_COMMLINK))
				continue;
			
			// improve relation between AIs
			
			int value1 = -random(randomBound);
			int value2 = -random(randomBound);
			cause_friction(otherFactionId, aiFactionId, value1);
			cause_friction(aiFactionId, otherFactionId, value2);
			debug("\tcause_friction %-24s -> %-24s : %+2d\n", MFactions[otherFactionId].noun_faction, MFactions[aiFactionId].noun_faction, value1);
			debug("\tcause_friction %-24s -> %-24s : %+2d\n", MFactions[aiFactionId].noun_faction, MFactions[otherFactionId].noun_faction, value2);
			
		}
		flushlog();
		
	}
	
}

/*
Intercepts enemy_move to correct probe moves.
*/
int __cdecl wtp_mod_enemy_move(int vehicleId)
{
	// reset probe_probeMovesSpent
	
	probe_probeMovesSpent = -1;
	
	// run original function
	// which also wraps AI logic
	
	int returnValue = wtp_mod_ai_enemy_move(vehicleId);
	
	// set probe moves
	
	if (probe_probeMovesSpent >= 0)
	{
		VEH *probeVehicle = getVehicle(vehicleId);
		
		// set probe moves
		
		probeVehicle->moves_spent = probe_probeMovesSpent;
		
		// reset probe_probeMovesSpent
		
		probe_probeMovesSpent = -1;
		
	}
	
	return returnValue;
	
}

/*
Set energy stolen flag.
*/
int __cdecl wtp_mod_steal_energy(int baseId)
{
	// execute original function
	
	int returnValue = steal_energy(baseId);
	
	// set energy stolen flag
	
	Bases[baseId].state_flags |= BSTATE_ENERGY_RESERVES_DRAINED;
	
	// return value
	
	return returnValue;
	
}

/*
Intercepts diplomacy_caption -> say_fac_special to display the mood.
*/
int __cdecl wtp_mod_diplomacy_caption_say_fac_special(int dst, int src, int factionId)
{
	// execute original
	
	int returnValue = say_fac_special(dst, src, factionId);
	
	// append numeric mood if configured
	
	if (conf.display_numeric_mood)
	{
		char numericFriction[10];
		sprintf(numericFriction, " <%d>", *DiploFriction);
		strcat((char *)dst, numericFriction);
	}
	
	return returnValue;
	
}

/*
Intercepts probe -> success_rates for procure research data action.
*/
int wtp_mod_probe_success_rates_procure_research_data(int position, int morale, int risk, int baseId)
{
	risk += conf.probe_risk_procure_research_data;
	return success_rates(position, morale, risk, baseId);
}

/*
Intercepts probe -> veh_skip to disable promotion on infiltrate datalinks.
*/
int wtp_mod_probe_veh_skip(int vehicleId)
{
	int value = mod_veh_skip(vehicleId);
	
	// disable probe promotion on infiltrate datalinks by reducing morale
	
	if (isProbeVehicle(vehicleId))
	{
		VEH &vehicle = Vehs[vehicleId];
		
		if (vehicle.probe_action == 0)
		{
			vehicle.morale--;
		}
		
	}
	
	return value;
	
}

int __cdecl wtp_mod_action_terraform(int vehicleId, int action, int execute)
{
	MAP *tile = getVehicleMapTile(vehicleId);
	int rockiness = -1;
	
	if (tile != nullptr && tile->is_sea() && action == FORMER_SOLAR)
	{
		rockiness = tile->val3 & (TILE_ROLLING | TILE_ROCKY);
		tile->val3 &= ~(TILE_ROLLING | TILE_ROCKY);
		tile->val3 |= TILE_ROLLING;
	}
	
	int returnValue = action_terraform(vehicleId, action, execute);
	
	if (tile != nullptr && tile->is_sea() && action == FORMER_SOLAR && rockiness >= 0)
	{
		tile->val3 &= ~(TILE_ROLLING | TILE_ROCKY);
		tile->val3 |= rockiness;
	}
	
	return returnValue;
	
}

int __thiscall wtp_mod_Console_go_to(Console* This, int a1, int a2, int a3)
{
	int returnValue = Console_go_to(This, a1, a2, a3);
	
	// reset Psi Gate use flag in all bases
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		Bases[baseId].state_flags &= ~(BSTATE_PSI_GATE_USED);
	}
	
	return returnValue;
	
}

int __cdecl wtp_mod_tech_achieved(int factionId, int techId, int targetFactionId, int steal)
{
	// execute original code
	
	int value = tech_achieved(factionId, techId, targetFactionId, steal);
	
	// select technology if not set
	
	if (Factions[factionId].tech_research_id < 0)
	{
		mod_tech_selection(factionId);
	}
	
	return value;
	
}

/*
Does not initialize disabled popup.
*/
int __cdecl mod_popb(char const *label, int flags, int sound_id, char const *pcx_filename, int a5)
{
	if ((*GameWarnings & flags) == 0)
	{
		return 0;
	}
	else
	{
		return popb(label, flags, sound_id, pcx_filename, a5);
	}
	
}

int __cdecl wtp_mod_alien_veh_init(int unitId, int factionId, int x, int y)
{
	// do not create mobile natives until they gain full strength
	
	if (factionId == 0 && unitId != BSC_FUNGAL_TOWER && *CurrentTurn < conf.native_weak_until_turn)
	{
		return -1;
	}
	
	// execute original code
	
	return veh_init(unitId, factionId, x, y);
	
}

/*
Adjusts energy allocation to maximize an effect.
*/
void wtp_mod_social_ai(int factionId)
{
	// not applicable cases
	
	if (is_human(factionId) || !is_alive(factionId))
	{
		return;
	}
	if (!isWtpEnabledFaction(factionId) || !conf.social_ai)
	{
		return;
	}
	
	// method starts
	
	Profiling::start("- wtp_mod_social_ai");
	
	debug("wtp_mod_social_ai - %s\n", MFactions[factionId].noun_faction);
	
	Faction &faction = Factions[factionId];
	
	// evaluate best psych allocation
	
	int oldPsychAllocation = faction.SE_alloc_psych;
	int oldLabsAllocation = faction.SE_alloc_labs;
	
	debug
	(
		"\told allocation={%d,%d,%d}"
		"\n"
		, 10 - oldPsychAllocation - oldLabsAllocation
		, oldPsychAllocation
		, oldLabsAllocation
	);
	
	int newPsychAllocation = oldPsychAllocation;
	int newLabsAllocation = oldLabsAllocation;
	double bestScore = 0.0;
	
	for (int psychAllocationChange = -1; psychAllocationChange <= +1; psychAllocationChange++)
	{
		for (int labsAllocationChange = -1; labsAllocationChange <= +1; labsAllocationChange++)
		{
			int psychAllocation;
			int labsAllocation;
			
			// compute modified allocation
			
			psychAllocation = oldPsychAllocation + psychAllocationChange;
			labsAllocation = oldLabsAllocation + labsAllocationChange;
			
			if (psychAllocation < 0 || psychAllocation > 10 || labsAllocation < 0 || labsAllocation > 10)
				continue;
			
			// compute bases
			
			faction.SE_alloc_psych = psychAllocation;
			faction.SE_alloc_labs = labsAllocation;
			
			for (int baseId = 0; baseId < *BaseCount; baseId++)
			{
				BASE &base = Bases[baseId];
				
				if (base.faction_id != factionId)
					continue;
				
				computeBase(baseId, true);
				
			}
			
			energy_compute(factionId, 0);
			
			// avoid negative net income
			
			if (*net_income < 0)
				continue;
			
			// evaluate effect score
			
			int totalNutrient = 0;
			int totalMineral = 0;
			int totalEconomy = 0;
			int totalPsych = 0;
			int totalLabs = 0;
			int totalBudget = 0;
			int totalDoctorCount = 0;
			
			for (int baseId = 0; baseId < *BaseCount; baseId++)
			{
				BASE &base = Bases[baseId];
				
				if (base.faction_id != factionId)
					continue;
				
				totalNutrient += base.nutrient_surplus;
				totalMineral += base.mineral_surplus;
				totalEconomy += base.economy_total;
				totalPsych += base.psych_total;
				totalLabs += base.labs_total;
				totalBudget += base.economy_total + base.labs_total;
				totalDoctorCount += getBaseDoctorCount(baseId);
				
			}
			
			// how many credits one lab is worth
			
			double totalLabsWorth =
				(double) totalLabs
				*
				(
					conf.flat_hurry_cost ?
						conf.lab_mineral_worth * conf.flat_hurry_cost_multiplier_facility
						:
						1.0
				)
			;
			
			double score = getResourceScore((double) totalNutrient, (double) totalMineral, (double) totalEconomy + totalLabsWorth);
			
			debug
			(
				"\tallocation={%d,%d,%d}"
				" totalNutrient=%3d"
				" totalMineral=%3d"
				" totalEconomy=%3d"
				" totalPsych=%3d"
				" totalLabs=%3d"
				" totalLabsWorth=%5.2f"
				" totalBudget=%3d"
				" totalDoctorCount=%3d"
				" *net_income=%3d"
				" score=%5.2f"
				"\n"
				, 10 - psychAllocation - labsAllocation
				, psychAllocation
				, labsAllocation
				, totalNutrient
				, totalMineral
				, totalEconomy
				, totalPsych
				, totalLabs
				, totalLabsWorth
				, totalBudget
				, totalDoctorCount
				, *net_income
				, score
			);
			
			if (score > bestScore)
			{
				newPsychAllocation = psychAllocation;
				newLabsAllocation = labsAllocation;
				bestScore = score;
			}
			
		}
		
	}
	
	// set values
	
	faction.SE_alloc_psych = newPsychAllocation;
	faction.SE_alloc_labs = newLabsAllocation;
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE &base = Bases[baseId];
		
		if (base.faction_id != factionId)
			continue;
		
		computeBase(baseId, true);
		
	}
	
	energy_compute(factionId, 0);
	
	debug
	(
		"\tnew allocation={%d,%d,%d}"
		"\n"
		, 10 - newPsychAllocation - newLabsAllocation
		, newPsychAllocation
		, newLabsAllocation
	);
	
	Profiling::stop("- wtp_mod_social_ai");
	
}

/*
Destroys terrain improvements on lost territory.
*/
int __cdecl wtp_mod_capture_base(int base_id, int faction, int is_probe)
{
	BASE &base = Bases[base_id];
	int oldBaseFactionId = base.faction_id;
	
	// destroyable terrain improvements
	
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
	
	int *oldMapOwners = nullptr;
	
	if (conf.scorched_earth)
	{
		// record exising map owners
		
		int *oldMapOwners = new int[*MapAreaTiles];
		
		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
		{
			MAP *tile = *MapTiles + tileIndex;
			oldMapOwners[tileIndex] = tile->owner;
		}
		
	}
	// execute original code
	
	int returnValue = mod_capture_base(base_id, faction, is_probe);
	
	if (conf.scorched_earth && oldMapOwners != nullptr)
	{
		// destroy improvements on lost territory
		
		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
		{
			MAP *tile = *MapTiles + tileIndex;
			
			int oldMapOwner = oldMapOwners[tileIndex];
			int newMapOwner = tile->owner;
			
			if (oldMapOwner == oldBaseFactionId && newMapOwner != oldBaseFactionId)
			{
				tile->items &= ~destroyableImprovments;
			}
			
		}
		
		// delete allocated array
		
		delete[] oldMapOwners;
		
	}
	
	if (conf.destroy_captured_base_defense)
	{
		// destroy base defensive structures
		
		for (FacilityId facilityId : DEFENSIVE_FACILITIES)
		{
			setBaseFacility(base_id, facilityId, false);
		}
		
	}
	
	// return value
	
	return returnValue;
	
}

/*
Do not retire default unit.
*/
void __cdecl wtp_mod_retire_proto(int unitId, int factionId)
{
	if (unitId < MaxProtoFactionNum)
		return;
	
	retire_proto(unitId, factionId);
	
}

/*
Intercepts has_abil call to disallow land air superiority vehicle attacking needlejet at sea.
*/
int __cdecl wtp_mod_has_abil_land_air_superiority_attack_needlejet_at_sea(int unit_id, VehAblFlag ability)
{
	if (ability == ABL_AIR_SUPERIORITY)
	{
		return 0;
	}
	
	// execute original code
	
	return has_abil(unit_id, ability);
	
}

/*
Intercepts has_abil call for the purpose of determining whether vehicle can attack needlejet in flight.
*/
int __cdecl wtp_mod_has_abil_air_superiority_attack_needlejet(int unit_id, VehAblFlag ability)
{
	if (conf.air_superiority_not_required_to_attack_needlejet && ability == ABL_AIR_SUPERIORITY)
	{
		return 1;
	}
	
	// execute original code
	
	return has_abil(unit_id, ability);
	
}

