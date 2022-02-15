#include "ai.h"
#include "main.h"
#include "terranx.h"
#include "game_wtp.h"
#include "wtp.h"
#include "aiData.h"
#include "aiHurry.h"

/*
Faction upkeep entry point.
This is called for WTP AI enabled factions only.
*/
int aiFactionUpkeep(const int factionId)
{
	debug("aiFactionUpkeep - %s\n", MFactions[factionId].noun_faction);
	
	// set AI faction id for global reference
	
	aiFactionId = factionId;

	// recalculate global faction parameters on this turn if not yet done
	
	clock_t c = clock();
	aiFactionStrategy();
	debug("(time) [WTP] aiFactionStrategy: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
	
	// consider hurrying production in all bases
	
	considerHurryingProduction(factionId);
	
	// redirect to vanilla function for the rest of processing
	// which is overriden by Thinker
	
	return faction_upkeep(factionId);
	
}

/*
Faction level AI strategy.
*/
void aiFactionStrategy()
{
	debug("aiStrategy: aiFactionId=%d\n", aiFactionId);

	// clear lists
	
	aiData.clear();
	
}

VEH *getVehicleByAIId(int aiId)
{
	// check if ID didn't change

	VEH *oldVehicle = &(Vehicles[aiId]);

	if (oldVehicle->pad_0 == aiId)
		return oldVehicle;

	// otherwise, scan all vehicles

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);

		if (vehicle->pad_0 == aiId)
			return vehicle;

	}

	return NULL;

}

MAP *getNearestPod(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	MAP *nearestPod = nullptr;
	int minRange = INT_MAX;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int x = getX(mapIndex);
		int y = getY(mapIndex);
		
		if (isDestinationReachable(vehicleId, x, y, false) && (goody_at(x, y) != 0))
		{
			int range = map_range(vehicle->x, vehicle->y, x, y);
			
			if (range < minRange)
			{
				nearestPod = getMapTile(mapIndex);
				minRange = range;
			}
			
		}
		
	}
	
	return nearestPod;
	
}

int getNearestFactionBaseRange(int factionId, int x, int y)
{
	int nearestFactionBaseRange = 9999;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		// own bases

		if (base->faction_id != factionId)
			continue;

		nearestFactionBaseRange = std::min(nearestFactionBaseRange, map_range(x, y, base->x, base->y));

	}

	return nearestFactionBaseRange;

}

int getNearestOtherFactionBaseRange(int factionId, int x, int y)
{
	int nearestFactionBaseRange = INT_MAX;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		// other faction bases

		if (base->faction_id == factionId)
			continue;

		nearestFactionBaseRange = std::min(nearestFactionBaseRange, map_range(x, y, base->x, base->y));

	}

	return nearestFactionBaseRange;

}

int getNearestBaseId(int x, int y, std::set<int> baseIds)
{
	int nearestBaseId = -1;
	int nearestBaseRange = INT_MAX;

	for (int baseId : baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		int range = map_range(x, y, base->x, base->y);
		
		if (nearestBaseId == -1 || range < nearestBaseRange)
		{
			nearestBaseId = baseId;
			nearestBaseRange = range;
		}

	}

	return nearestBaseId;

}

int getFactionBestPrototypedWeaponOffenseValue(int factionId)
{
	int bestWeaponOffenseValue = 0;
	
	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(Units[unitId]);
		
		// skip non combat units

		if (!isCombatUnit(unitId))
			continue;
		
		// get weapon offense value
		
		int weaponOffenseValue = Weapon[unit->weapon_type].offense_value;
		
		// update best weapon offense value
		
		bestWeaponOffenseValue = std::max(bestWeaponOffenseValue, weaponOffenseValue);
		
	}
	
	return bestWeaponOffenseValue;

}

int getFactionBestPrototypedArmorDefenseValue(int factionId)
{
	int bestArmorDefenseValue = 0;
	
	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(Units[unitId]);
		
		// skip non combat units

		if (!isCombatUnit(unitId))
			continue;
		
		// get armor defense value
		
		int armorDefenseValue = Armor[unit->armor_type].defense_value;
		
		// update best armor defense value
		
		bestArmorDefenseValue = std::max(bestArmorDefenseValue, armorDefenseValue);
		
	}
	
	return bestArmorDefenseValue;

}

/*
Arbitrary algorithm to evaluate generic combat strength.
*/
double evaluateCombatStrength(double offenseValue, double defenseValue)
{
	double adjustedOffenseValue = evaluateOffenseStrength(offenseValue);
	double adjustedDefenseValue = evaluateDefenseStrength(defenseValue);
	
	double combatStrength = std::max(adjustedOffenseValue, adjustedDefenseValue) + 0.5 * std::min(adjustedOffenseValue, adjustedDefenseValue);
	
	return combatStrength;
	
}

/*
Arbitrary algorithm to evaluate generic offense strength.
*/
double evaluateOffenseStrength(double offenseValue)
{
	double adjustedOffenseValue = offenseValue * offenseValue;
	
	return adjustedOffenseValue;
	
}

/*
Arbitrary algorithm to evaluate generic oefense strength.
*/
double evaluateDefenseStrength(double DefenseValue)
{
	double adjustedDefenseValue = DefenseValue * DefenseValue;
	
	return adjustedDefenseValue;
	
}

bool isVehicleThreatenedByEnemyInField(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	// not in base
	
	if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
		return false;
	
	// check other units
	
	bool threatened = false;
	
	for (int otherVehicleId = 0; otherVehicleId < *total_num_vehicles; otherVehicleId++)
	{
		VEH *otherVehicle = &(Vehicles[otherVehicleId]);
		MAP *otherVehicleTile = getVehicleMapTile(otherVehicleId);
		
		// exclude non alien units not in war
		
		if (otherVehicle->faction_id != 0 && !at_war(otherVehicle->faction_id, vehicle->faction_id))
			continue;
		
		// exclude non combat units
		
		if (!isCombatVehicle(otherVehicleId))
			continue;
		
		// unit is in different region and not air
		
		if (otherVehicle->triad() != TRIAD_AIR && otherVehicleTile->region != vehicleTile->region)
			continue;
		
		// calculate threat range
		
		int threatRange = (otherVehicle->triad() == TRIAD_LAND ? 2 : 1) * Units[otherVehicle->unit_id].speed();
		
		// calculate range
		
		int range = map_range(otherVehicle->x, otherVehicle->y, vehicle->x, vehicle->y);
		
		// compare ranges
		
		if (range <= threatRange)
		{
			threatened = true;
			break;
		}
		
	}
	
	return threatened;
	
}

/*
Verifies if vehicle can get to and stay at this spot without aid of transport.
Gravship can reach everywhere.
Sea unit can reach only their ocean association.
Land unit can reach their land association or any other land association potentially connected by transport.
Land unit also can reach any friendly ocean base.
*/
bool isDestinationReachable(int vehicleId, int x, int y, bool immediatelyReachable)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MAP *destination = getMapTile(x, y);
	
	assert(vehicleTile != nullptr && destination != nullptr);
	
	switch (Units[vehicle->unit_id].chassis_type)
	{
	case CHS_GRAVSHIP:
		
		{
			return true;
			
		}
		
		break;
		
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		
		{
			// Some complex logic here. For now disable it.
			
			return false;
			
		}
		
		break;
		
	case CHS_FOIL:
	case CHS_CRUISER:
		
		{
			int vehicleOceanAssociation = getOceanAssociation(vehicleTile, vehicle->faction_id);
			int destinationOceanAssociation = getOceanAssociation(destination, vehicle->faction_id);
			
			// undetermined associations
			
			if (vehicleOceanAssociation == -1 || destinationOceanAssociation == -1)
				return false;
			
			if (destinationOceanAssociation == vehicleOceanAssociation)
			{
				return true;
			}
			else
			{
				return false;
			}
			
		}
		
		break;
		
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		
		{
			int vehicleAssociation = getAssociation(vehicleTile, vehicle->faction_id);
			int destinationAssociation = getAssociation(destination, vehicle->faction_id);
			
			if (vehicleAssociation == -1 || destinationAssociation == -1)
				return false;
			
			if
			(
				// land
				!is_ocean(destination)
				||
				// friendly ocean base
				isOceanBaseTile(destination, vehicle->faction_id)
			)
			{
				// same association (continent or ocean)
				if (destinationAssociation == vehicleAssociation)
				{
					return true;
				}
				else
				{
					if (immediatelyReachable)
					{
						// immediately reachable required
						return isImmediatelyReachableAssociation(destinationAssociation, vehicle->faction_id);
					}
					else
					{
						// potentially reachable required
						return isPotentiallyReachableAssociation(destinationAssociation, vehicle->faction_id);
					}
					
				}
				
			}
			
		}
		
		break;
		
	}
	
	return false;
	
}

int getRangeToNearestFactionAirbase(int x, int y, int factionId)
{
	int rangeToNearestFactionAirbase = INT_MAX;
	
	for (MAP *airbaseTile : aiData.factionInfos[factionId].airbases)
	{
		int airbaseX = getX(airbaseTile);
		int airbaseY = getY(airbaseTile);
		
		int range = map_range(x, y, airbaseX, airbaseY);
		
		rangeToNearestFactionAirbase = std::min(rangeToNearestFactionAirbase, range);
		
	}
	
	return rangeToNearestFactionAirbase;
	
}

int getNearestEnemyBaseDistance(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	int nearestEnemyBaseDistance = INT_MAX;
	
	for (int otherBaseId = 0; otherBaseId < *total_num_bases; otherBaseId++)
	{
		BASE *otherBase = &(Bases[otherBaseId]);
		
		// skip own
		
		if (otherBase->faction_id == base->faction_id)
			continue;
		
		// get distance
		
		int distance = vector_dist(base->x, base->y, otherBase->x, otherBase->y);
		
		// update best values
		
		if (distance < nearestEnemyBaseDistance)
		{
			nearestEnemyBaseDistance = distance;
		}
		
	}
	
	return nearestEnemyBaseDistance;
	
}

/*
Returns combat bonus multiplier for specific units.
Conventional combat only.

ABL_DISSOCIATIVE_WAVE cancels following abilities:
ABL_EMPATH
ABL_TRANCE
ABL_COMM_JAMMER
ABL_AAA
ABL_SOPORIFIC_GAS
*/
double getConventionalCombatBonusMultiplier(int attackerUnitId, int defenderUnitId)
{
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	
	// do not modify psi combat
	
	if (isNativeUnit(attackerUnitId) || isNativeUnit(defenderUnitId))
		return 1.0;
	
	// conventional combat
	
	double combatBonusMultiplier = 1.0;
	
	// fast unit without blink displacer against comm jammer
	
	if
	(
		unit_has_ability(defenderUnitId, ABL_COMM_JAMMER)
		&&
		attackerUnit->triad() == TRIAD_LAND && unit_chassis_speed(attackerUnitId) > 1
		&&
		!unit_has_ability(attackerUnitId, ABL_DISSOCIATIVE_WAVE)
	)
	{
		combatBonusMultiplier /= getPercentageBonusMultiplier(Rules->combat_comm_jammer_vs_mobile);
	}
	
	// air unit without blink displacer against air tracking
	
	if
	(
		unit_has_ability(defenderUnitId, ABL_AAA)
		&&
		attackerUnit->triad() == TRIAD_AIR
		&&
		!unit_has_ability(attackerUnitId, ABL_DISSOCIATIVE_WAVE)
	)
	{
		combatBonusMultiplier /= getPercentageBonusMultiplier(Rules->combat_AAA_bonus_vs_air);
	}
	
	// soporific unit against conventional unit without blink displacer
	
	if
	(
		unit_has_ability(attackerUnitId, ABL_SOPORIFIC_GAS)
		&&
		!isNativeUnit(defenderUnitId)
		&&
		!unit_has_ability(defenderUnitId, ABL_DISSOCIATIVE_WAVE)
	)
	{
		combatBonusMultiplier *= 1.25;
	}
	
	// interceptor ground strike
	
	if
	(
		attackerUnit->triad() == TRIAD_AIR && unit_has_ability(attackerUnitId, ABL_AIR_SUPERIORITY)
		&&
		defenderUnit->triad() != TRIAD_AIR
	)
	{
		combatBonusMultiplier *= getPercentageBonusMultiplier(-Rules->combat_penalty_air_supr_vs_ground);
	}
	
	// interceptor air-to-air
		
	if
	(
		attackerUnit->triad() == TRIAD_AIR && unit_has_ability(attackerUnitId, ABL_AIR_SUPERIORITY)
		&&
		defenderUnit->triad() == TRIAD_AIR
	)
	{
		combatBonusMultiplier *= getPercentageBonusMultiplier(Rules->combat_bonus_air_supr_vs_air);
	}
	
	// gas
	
	if
	(
		unit_has_ability(attackerUnitId, ABL_NERVE_GAS)
	)
	{
		combatBonusMultiplier *= 1.5;
	}
	
	return combatBonusMultiplier;
	
}

std::string getAbilitiesString(int ability_flags)
{
	std::string abilitiesString;
	
	if ((ability_flags & ABL_AMPHIBIOUS) != 0)
	{
		abilitiesString += " AMPHIBIOUS";
	}
	
	if ((ability_flags & ABL_AIR_SUPERIORITY) != 0)
	{
		abilitiesString += " AIR_SUPERIORITY";
	}
	
	if ((ability_flags & ABL_AAA) != 0)
	{
		abilitiesString += " AAA";
	}
	
	if ((ability_flags & ABL_COMM_JAMMER) != 0)
	{
		abilitiesString += " ECM";
	}
	
	if ((ability_flags & ABL_EMPATH) != 0)
	{
		abilitiesString += " EMPATH";
	}
	
	if ((ability_flags & ABL_ARTILLERY) != 0)
	{
		abilitiesString += " ARTILLERY";
	}
	
	if ((ability_flags & ABL_BLINK_DISPLACER) != 0)
	{
		abilitiesString += " BLINK_DISPLACER";
	}
	
	if ((ability_flags & ABL_TRANCE) != 0)
	{
		abilitiesString += " TRANCE";
	}
	
	if ((ability_flags & ABL_NERVE_GAS) != 0)
	{
		abilitiesString += " NERVE_GAS";
	}
	
	if ((ability_flags & ABL_SOPORIFIC_GAS) != 0)
	{
		abilitiesString += " SOPORIFIC_GAS";
	}
	
	if ((ability_flags & ABL_DISSOCIATIVE_WAVE) != 0)
	{
		abilitiesString += " DISSOCIATIVE_WAVE";
	}
	
	return abilitiesString;
	
}

bool isWithinAlienArtilleryRange(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	for (int otherVehicleId = 0; otherVehicleId < *total_num_vehicles; otherVehicleId++)
	{
		VEH *otherVehicle = &(Vehicles[otherVehicleId]);
		
		if (otherVehicle->faction_id == 0 && otherVehicle->unit_id == BSC_SPORE_LAUNCHER && map_range(vehicle->x, vehicle->y, otherVehicle->x, otherVehicle->y) <= 2)
			return true;
		
	}
	
	return false;
	
}

/*
Checks if faction is enabled to use WTP algorithms.
*/
bool isUseWtpAlgorithms(int factionId)
{
	return (factionId != 0 && !is_human(factionId) && ai_enabled(factionId) && conf.ai_useWTPAlgorithms);
}

int getCoastalBaseOceanAssociation(MAP *tile, int factionId)
{
	int coastalBaseOceanAssociation = -1;
	
	if (aiData.geography.faction[factionId].coastalBaseOceanAssociations.count(tile) != 0)
	{
		coastalBaseOceanAssociation = aiData.geography.faction[factionId].coastalBaseOceanAssociations.at(tile);
	}
	
	return coastalBaseOceanAssociation;
	
}

bool isOceanBaseTile(MAP *tile, int factionId)
{
	return aiData.geography.faction[factionId].oceanBaseTiles.count(tile) != 0;
}

int getAssociation(MAP *tile, int factionId)
{
	return getAssociation(getExtendedRegion(tile), factionId);
}

int getAssociation(int region, int factionId)
{
	int association;
	
	if (aiData.geography.faction[factionId].associations.count(region) != 0)
	{
		association = aiData.geography.faction[factionId].associations.at(region);
	}
	else
	{
		association = aiData.geography.associations.at(region);
	}
	
	return association;
	
}

std::set<int> *getConnections(MAP *tile, int factionId)
{
	return getConnections(getExtendedRegion(tile), factionId);
}
	
std::set<int> *getConnections(int region, int factionId)
{
	std::set<int> *connections;
	
	if (aiData.geography.faction[factionId].connections.count(region) != 0)
	{
		connections = &(aiData.geography.faction[factionId].connections.at(region));
	}
	else
	{
		connections = &(aiData.geography.connections.at(region));
	}
	
	return connections;
	
}

bool isSameAssociation(MAP *tile1, MAP *tile2, int factionId)
{
	return getAssociation(tile1, factionId) == getAssociation(tile2, factionId);
}

bool isSameAssociation(int association1, MAP *tile2, int factionId)
{
	return association1 == getAssociation(tile2, factionId);
}

bool isImmediatelyReachableAssociation(int association, int factionId)
{
	return aiData.geography.faction[factionId].immediatelyReachableAssociations.count(association) != 0;
}

bool isPotentiallyReachableAssociation(int association, int factionId)
{
	return aiData.geography.faction[factionId].potentiallyReachableAssociations.count(association) != 0;
}

/*
Returns region for all map tiles.
Returns shifted map index for polar regions.
*/
int getExtendedRegion(MAP *tile)
{
	if (tile->region == 0x3f || tile->region == 0x7f)
	{
		return 0x80 + getMapIndexByPointer(tile);
	}
	else
	{
		return tile->region;
	}
	
}

/*
Checks for polar region
*/
bool isPolarRegion(MAP *tile)
{
	return (tile->region == 0x3f || tile->region == 0x7f);
	
}

/*
Returns vehicle region association.
*/
int getVehicleAssociation(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	int vehicleAssociation = -1;
	
	if (vehicle->triad() == TRIAD_LAND)
	{
		if (isLandRegion(vehicleTile->region))
		{
			vehicleAssociation = getAssociation(vehicleTile, vehicle->faction_id);
		}
		
	}
	else if (vehicle->triad() == TRIAD_SEA)
	{
		if (isOceanRegion(vehicleTile->region))
		{
			vehicleAssociation = getAssociation(vehicleTile, vehicle->faction_id);
		}
		else if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
		{
			int coastalBaseOceanAssociation = getCoastalBaseOceanAssociation(vehicleTile, vehicle->faction_id);
			
			if (coastalBaseOceanAssociation != -1)
			{
				vehicleAssociation = coastalBaseOceanAssociation;
			}
			
		}
		
	}
	
	return vehicleAssociation;
	
}

/*
Returns ocean association.
*/
int getOceanAssociation(MAP *tile, int factionId)
{
	int oceanAssociation = -1;
	
	if (is_ocean(tile))
	{
		oceanAssociation = getAssociation(tile, factionId);
	}
	else if (map_has_item(tile, TERRA_BASE_IN_TILE))
	{
		int coastalBaseOceanAssociation = getCoastalBaseOceanAssociation(tile, factionId);
		
		if (coastalBaseOceanAssociation != -1)
		{
			oceanAssociation = coastalBaseOceanAssociation;
		}
		
	}
	
	return oceanAssociation;

}

/*
Returns base ocean association.
*/
int getBaseOceanAssociation(int baseId)
{
	BASE *base = &(Bases[baseId]);
	MAP *baseTile = getBaseMapTile(baseId);
	
	int baseOceanAssociation = -1;
	
	if (isOceanRegion(baseTile->region))
	{
		baseOceanAssociation = getAssociation(baseTile, base->faction_id);
	}
	else
	{
		int coastalBaseOceanAssociation = getCoastalBaseOceanAssociation(baseTile, base->faction_id);
		
		if (coastalBaseOceanAssociation != -1)
		{
			baseOceanAssociation = coastalBaseOceanAssociation;
		}
		
	}
	
	return baseOceanAssociation;

}

bool isOceanAssociationCoast(MAP *tile, int oceanAssociation, int factionId)
{
	return isOceanAssociationCoast(getX(tile), getY(tile), oceanAssociation, factionId);
}

bool isOceanAssociationCoast(int x, int y, int oceanAssociation, int factionId)
{
	MAP *tile = getMapTile(x, y);

	if (is_ocean(tile))
		return false;

	for (MAP *adjacentTile : getBaseAdjacentTiles(x, y, false))
	{
		int adjacentTileOceanAssociation = getOceanAssociation(adjacentTile, factionId);
		
		if (adjacentTileOceanAssociation == oceanAssociation)
			return true;
		
	}

	return false;

}

int getMaxBaseSize(int factionId)
{
	int maxBaseSize = 0;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		if (base->faction_id != factionId)
			continue;

		maxBaseSize = std::max(maxBaseSize, (int)base->pop_size);

	}

	return maxBaseSize;

}

/*
Returns base ocean regions.
*/
std::set<int> getBaseOceanRegions(int baseId)
{
	BASE *base = &(Bases[baseId]);
	MAP *baseTile = getBaseMapTile(baseId);

	std::set<int> baseOceanRegions;

	if (is_ocean(baseTile))
	{
		baseOceanRegions.insert(getExtendedRegion(baseTile));
	}
	else
	{
		for (MAP *adjacentTile : getBaseAdjacentTiles(base->x, base->y, false))
		{
			if (!is_ocean(adjacentTile))
				continue;

			baseOceanRegions.insert(getExtendedRegion(adjacentTile));

		}

	}

	return baseOceanRegions;

}

/*
Searches for nearest AI faction base range.
*/
int getNearestBaseRange(int x, int y)
{
	int nearestBaseRange = INT_MAX;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);

		nearestBaseRange = std::min(nearestBaseRange, map_range(x, y, base->x, base->y));

	}

	return nearestBaseRange;

}
int getNearestBaseRange(int mapIndex)
{
	return getNearestBaseRange(getX(mapIndex), getY(mapIndex));
}

/*
Searches for nearest AI faction colony range.
*/
int getNearestColonyRange(int x, int y)
{
	int nearestColonyRange = INT_MAX;

	for (int vehicleId : aiData.colonyVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		nearestColonyRange = std::min(nearestColonyRange, map_range(x, y, vehicle->x, vehicle->y));

	}

	return nearestColonyRange;

}
int getNearestColonyRange(int mapIndex)
{
	return getNearestColonyRange(getX(mapIndex), getY(mapIndex));
}

