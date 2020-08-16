#include "aiProduction.h"
#include "aiCombat.h"
#include "ai.h"
#include "engine.h"

FACTION_INFO factionInfos[8];
std::map<int, double> unitTypeProductionDemands;
std::unordered_set<int> bestProductionBaseIds;

/*
Prepares production orders.
*/
void aiProductionStrategy()
{
	// populate lists

	populateProducitonLists();

	// calculate defender demand

	calculateConventionalDefenseDemand();

}

void populateProducitonLists()
{
	unitTypeProductionDemands.clear();
	bestProductionBaseIds.clear();

	// populate factions combat modifiers

	debug("%-24s\nfactionCombatModifiers:\n", tx_metafactions[aiFactionId].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// store combat modifiers

		factionInfos[id].conventionalOffenseMultiplier = getFactionConventionalOffenseMultiplier(id);
		factionInfos[id].conventionalDefenseMultiplier = getFactionConventionalDefenseMultiplier(id);

		debug("\t%-24s: conventionalOffenseMultiplier=%4.2f, conventionalDefenseMultiplier=%4.2f\n", tx_metafactions[id].noun_faction, factionInfos[id].conventionalOffenseMultiplier, factionInfos[id].conventionalDefenseMultiplier);

	}

	debug("\n");

	// populate other factions threat koefficients

	debug("%-24s\notherFactionThreatKoefficients:\n", tx_metafactions[aiFactionId].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// skip self

		if (id == aiFactionId)
			continue;

		// get relation from other faction

		int otherFactionRelation = tx_factions[id].diplo_status[aiFactionId];

		// calculate threat koefficient

		double threatKoefficient;

		if (otherFactionRelation & DIPLO_VENDETTA)
		{
			threatKoefficient = 1.00;
		}
		else if (otherFactionRelation & DIPLO_TRUCE)
		{
			threatKoefficient = 0.50;
		}
		else if (otherFactionRelation & DIPLO_TREATY)
		{
			threatKoefficient = 0.25;
		}
		else if (otherFactionRelation & DIPLO_PACT)
		{
			threatKoefficient = 0.00;
		}
		else
		{
			// no commlink or unknown status
			threatKoefficient = 0.50;
		}

		// add extra for treacherous human player

		if (is_human(id))
		{
			threatKoefficient += 0.50;
		}

		// store threat koefficient

		factionInfos[id].threatKoefficient = threatKoefficient;

		debug("\t%-24s: %08x => %4.2f\n", tx_metafactions[id].noun_faction, otherFactionRelation, threatKoefficient);

	}

	debug("\n");

	// calculate average base production

	double totalBaseProduction = 0.0;

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		totalBaseProduction += (double)base->mineral_surplus_final;

	}

	double averageBaseProduction = totalBaseProduction / (double)baseIds.size();

	// populate best production bases

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		if (base->mineral_surplus_final >= averageBaseProduction)
		{
			bestProductionBaseIds.insert(id);
		}

	}

}

void calculateConventionalDefenseDemand()
{
	// find total enemy offensive value in the vicinity of our bases

	double totalEnemyOffensiveStrength = 0.0;

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(tx_vehicles[id]);
		int triad = veh_triad(id);
		MAP *location = getMapTile(vehicle->x, vehicle->y);

		// exclude natives

		if (vehicle->faction_id == 0)
			continue;

		// exclude own vehicles

		if (vehicle->faction_id == aiFactionId)
			continue;

		// find vehicle weapon offensive value

		int vehicleWeaponOffensiveValue = tx_weapon[tx_units[vehicle->proto_id].weapon_type].offense_value;

		// count only conventional non zero offensive values

		if (vehicleWeaponOffensiveValue < 1)
			continue;

		// get vehicle region

		int region = (triad == TRIAD_AIR ? -1 : location->region);

		// calculate range to own nearest base

		int rangeToNearestOwnBase = findRangeToNearestOwnBase(vehicle->x, vehicle->y, region);

		// ignore vehicles on or farther than max range

		if (rangeToNearestOwnBase >= MAX_RANGE)
			continue;

		// calculate vehicle speed
		// for land units assuming there are roads everywhere

		double vehicleSpeed = (triad == TRIAD_LAND ? getLandVehicleSpeedOnRoads(id) : getVehicleSpeedWithoutRoads(id));

		// calculate threat time koefficient

		double threatTimeKoefficient = MIN_THREAT_TURNS / max(MIN_THREAT_TURNS, ((double)rangeToNearestOwnBase / vehicleSpeed));

		// calculate vehicle offensive strength

		double vehicleOffensiveStrength = (double)vehicleWeaponOffensiveValue;

		// adjust artillery value

		if (vehicle_has_ability(vehicle, ABL_ARTILLERY))
		{
			vehicleOffensiveStrength *= ARTILLERY_OFFENSIVE_VALUE_KOEFFICIENT;
		}

		// update total

		totalEnemyOffensiveStrength += factionInfos[vehicle->faction_id].threatKoefficient * factionInfos[vehicle->faction_id].conventionalOffenseMultiplier * threatTimeKoefficient * vehicleOffensiveStrength;

		debug("\t\t[%3d](%3d,%3d), vehicleFactionId=%d, rangeToNearestOwnBase=%3d, threatTimeKoefficient=%4.2f, vehicleWeaponOffensiveValue=%2d, vehicleOffensiveStrength=%4.1f\n", id, vehicle->x, vehicle->y, vehicle->faction_id, rangeToNearestOwnBase, threatTimeKoefficient, vehicleWeaponOffensiveValue, vehicleOffensiveStrength);

	}

	// find total own defense value

	double totalOwnDefensiveStrength = 0.0;

	for (int id : combatVehicleIds)
	{
		VEH *vehicle = &(tx_vehicles[id]);

		// find vehicle armor defensive value

		int vehicleArmorDefensiveValue = tx_defense[tx_units[vehicle->proto_id].armor_type].defense_value;

		// calculate vehicle defensive strength

		double vehicleDefensiveStrength = (double)vehicleArmorDefensiveValue;

		// update total

		totalOwnDefensiveStrength += factionInfos[vehicle->faction_id].conventionalDefenseMultiplier * vehicleDefensiveStrength;

	}

	// set defensive unit production demand

	if (totalOwnDefensiveStrength < totalEnemyOffensiveStrength)
	{
		unitTypeProductionDemands[UT_DEFENSE] = (totalEnemyOffensiveStrength - totalOwnDefensiveStrength) / totalEnemyOffensiveStrength;
	}

	debug("%-24s\ncalculateConventionalDefenseDemand:\n", tx_metafactions[aiFactionId].noun_faction);
	debug("totalEnemyOffensiveStrength=%.0f, totalOwnDefensiveStrength=%.0f, unitTypeProductionDemands[UT_DEFENSE]=%4.2f\n", round(totalEnemyOffensiveStrength), round(totalOwnDefensiveStrength), unitTypeProductionDemands[UT_DEFENSE]);
	debug("\n");

}

double getFactionConventionalOffenseMultiplier(int id)
{
	double factionOffenseMultiplier = 1.0;

	MetaFaction *metaFaction = &(tx_metafactions[id]);

	// faction bonus

	for (int bonusIndex = 0; bonusIndex < metaFaction->faction_bonus_count; bonusIndex++)
	{
		int bonusId = metaFaction->faction_bonus_id[bonusIndex];

		if (bonusId == FCB_OFFENSE)
		{
			factionOffenseMultiplier *= ((double)metaFaction->faction_bonus_val1[bonusIndex] / 100.0);
		}

	}

	// fanatic bonus

	if (metaFaction->rule_flags & FACT_FANATIC)
	{
		factionOffenseMultiplier *= (1.0 + (double)tx_basic->combat_bonus_fanatic / 100.0);
	}

	return factionOffenseMultiplier;

}

double getFactionConventionalDefenseMultiplier(int id)
{
	double factionDefenseMultiplier = 1.0;

	MetaFaction *metaFaction = &(tx_metafactions[id]);

	// faction bonus

	for (int bonusIndex = 0; bonusIndex < metaFaction->faction_bonus_count; bonusIndex++)
	{
		int bonusId = metaFaction->faction_bonus_id[bonusIndex];

		if (bonusId == FCB_DEFENSE)
		{
			factionDefenseMultiplier *= ((double)metaFaction->faction_bonus_val1[bonusIndex] / 100.0);
		}

	}

	return factionDefenseMultiplier;

}

int findRangeToNearestOwnBase(int x, int y, int region)
{
	int rangeToNearestOwnBase = MAX_RANGE;

	// iterate through own bases to find the closest

	for (std::pair<MAP *, int> element : baseLocations)
	{
		MAP *location = element.first;
		int id = element.second;
		BASE *base = &(tx_bases[id]);

		// skip bases in other region

		if (region != -1 && location->region != region)
			continue;

		// calculate range

		int range = map_range(base->x, base->y, x, y);

		// find minimal range

		rangeToNearestOwnBase = min(rangeToNearestOwnBase, range);

	}

	return rangeToNearestOwnBase;

}

int suggestBaseProduction(int id, int choice)
{
	BASE *base = &(tx_bases[id]);

	// need global defense

	if (unitTypeProductionDemands.count(UT_DEFENSE) > 0 && unitTypeProductionDemands[UT_DEFENSE] >= 0.5)
	{
		// check if this is a best production base

		if (bestProductionBaseIds.count(id) != 0)
		{
			choice = find_proto(id, TRIAD_LAND, COMBAT, DEF);

			debug("suggestBaseProduction: [%s] UT_DEFENSE, [%d] %s\n", base->name, choice, tx_units[choice].name);

		}

	}

	return choice;

}

