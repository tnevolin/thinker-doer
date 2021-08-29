#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "ai.h"
#include "wtp.h"
#include "game.h"
#include "ai.h"
#include "aiProduction.h"
#include "aiFormer.h"
#include "aiHurry.h"
#include "aiProduction.h"

// global variables

int currentTurn = -1;
int aiFactionId = -1;

ActiveFactionInfo activeFactionInfo;

/*
Top level faction upkeep entry point.
This is called for AI enabled factions only.
*/
int aiFactionUpkeep(const int factionId)
{
	// set AI faction id for future reference
	
	aiFactionId = factionId;
	
	// recalculate global faction parameters on this turn if not yet done
	
	aiStrategy();
	
	// redirect to vanilla function for the rest of processing
	// that, in turn, is overriden by Thinker
	
	int returnValue = faction_upkeep(factionId);
	
	// update base productions
	
	setProduction();
	
	// consider hurrying production in all bases
	
	considerHurryingProduction(factionId);
	
	// return value
	
	return returnValue;
	
}

/*
Top level AI enemy move entry point
*/
/*

Transporting land vehicles by sea transports idea.

- Land vehicle not on transport
Land vehicle moves wherever it wants regardless if destination is reachable.
Land vehicle waits ferry pickup when it is at ocean or at the coast and wants to get to another land region.

- Empty transport
Empty transport rushes to nearest higher priority vehicle waiting for ferry.
When at or next to awaiting land vehicle it stops and explicitly commands passenger to board.

- Land vehicle on transport
Land vehicle on transport is not controlled by this AI code.

- Loaded transport
Loaded transport selects highest priority passenger.
Loaded transport then request highest priority passenger to generate desired destination.
Loaded transport then computes unload location closest to passenger destination location.
When at or next unload location it stops and explicitly commands passenger to unboard to unload location.

*/
int aiEnemyMove(const int vehicleId)
{
//	return mod_enemy_move(vehicleId);
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// do not try multiple iterations
	
	if (vehicle->iter_count > 0)
	{
		return mod_enemy_move(vehicleId);
	}
	
    // do not control probes
    // Thinker ignores subversion opportunities

    if (vehicle->weapon_type() == WPN_PROBE_TEAM)
	{
		return enemy_move(vehicleId);
	}

	// do not control boarded land units
	
	if (isVehicleLandUnitOnTransport(vehicleId))
	{
		return mod_enemy_move(vehicleId);
	}
	
	if (isVehicleColony(vehicleId))
	{
		return moveColony(vehicleId);
	}
	
	if (isVehicleFormer(vehicleId))
	{
		return moveFormer(vehicleId);
	}
	
	if (isVehicleSeaTransport(vehicleId))
	{
		return moveSeaTransport(vehicleId);
	}
	
	if (isVehicleCombat(vehicleId))
	{
		return moveCombat(vehicleId);
	}
	
	// unhandled cases default to Thinker
	
	return mod_enemy_move(vehicleId);
	
}

/*
AI strategy.
*/
void aiStrategy()
{
	debug("aiStrategy: aiFactionId=%d\n", aiFactionId);

	// populate shared strategy lists
	
	populateGlobalVariables();
	evaluateBaseExposures();
	setSharedOceanRegions();
	
	// design units
	
	designUnits();
	
	// bases native defense
	
	evaluateBaseNativeDefenseDemands();
	
	// bases defense
	
	evaluateDefenseDemand();
	
	// compute production demands

	aiProductionStrategy();

	// prepare former orders

	aiTerraformingStrategy();

	// prepare combat orders

	aiCombatStrategy();

}

void populateGlobalVariables()
{
	// clear lists
	
	activeFactionInfo.clear();
	
	// best weapon and armor
	
	activeFactionInfo.bestWeaponOffenseValue = getFactionBestPrototypedWeaponOffenseValue(aiFactionId);
	activeFactionInfo.bestArmorDefenseValue = getFactionBestPrototypedArmorDefenseValue(aiFactionId);
	
	// populate factions combat modifiers

	debug("%-24s\nfactionCombatModifiers:\n", MFactions[aiFactionId].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// store combat modifiers

		activeFactionInfo.factionInfos[id].offenseMultiplier = getFactionOffenseMultiplier(id);
		activeFactionInfo.factionInfos[id].defenseMultiplier = getFactionDefenseMultiplier(id);
		activeFactionInfo.factionInfos[id].fanaticBonusMultiplier = getFactionFanaticBonusMultiplier(id);

		debug
		(
			"\t%-24s: offenseMultiplier=%4.2f, defenseMultiplier=%4.2f, fanaticBonusMultiplier=%4.2f\n",
			MFactions[id].noun_faction,
			activeFactionInfo.factionInfos[id].offenseMultiplier,
			activeFactionInfo.factionInfos[id].defenseMultiplier,
			activeFactionInfo.factionInfos[id].fanaticBonusMultiplier
		);

	}

	debug("\n");

	// populate other factions threat koefficients

	debug("%-24s\notherFactionthreatCoefficients:\n", MFactions[aiFactionId].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// skip aliens

		if (id == 0)
			continue;

		// skip self

		if (id == aiFactionId)
			continue;

		// get relation from other faction

		int otherFactionRelation = Factions[id].diplo_status[aiFactionId];

		// calculate threat koefficient

		double threatCoefficient;

		if (is_human(id))
		{
			threatCoefficient = conf.ai_production_threat_coefficient_human;
		}
		else if (otherFactionRelation & DIPLO_VENDETTA)
		{
			threatCoefficient = conf.ai_production_threat_coefficient_vendetta;
		}
		else if (otherFactionRelation & DIPLO_PACT)
		{
			threatCoefficient = conf.ai_production_threat_coefficient_pact;
		}
		else if (otherFactionRelation & DIPLO_TREATY)
		{
			threatCoefficient = conf.ai_production_threat_coefficient_treaty;
		}
		else
		{
			threatCoefficient = conf.ai_production_threat_coefficient_other;
		}

		// store threat koefficient

		activeFactionInfo.factionInfos[id].threatCoefficient = threatCoefficient;

		debug("\t%-24s: %08x => %4.2f\n", MFactions[id].noun_faction, otherFactionRelation, threatCoefficient);

	}

	// populate factions best combat item values
	
	for (int factionId = 1; factionId < 8; factionId++)
	{
		activeFactionInfo.factionInfos[factionId].bestWeaponOffenseValue = getFactionBestPrototypedWeaponOffenseValue(factionId);
		activeFactionInfo.factionInfos[factionId].bestArmorDefenseValue = getFactionBestPrototypedArmorDefenseValue(factionId);
	}

	debug("\n");

	// populate bases

	debug("activeFactionInfo.baseStrategies\n");

	for (int id = 0; id < *total_num_bases; id++)
	{
		BASE *base = &(Bases[id]);

		// exclude not own bases

		if (base->faction_id != aiFactionId)
			continue;

		// add base
		
		activeFactionInfo.baseIds.push_back(id);
		
		// add base location

		MAP *baseLocation = getMapTile(base->x, base->y);
		activeFactionInfo.baseLocations[baseLocation] = id;

		// add base strategy

		activeFactionInfo.baseStrategies[id] = {};
		BaseStrategy *baseStrategy = &(activeFactionInfo.baseStrategies[id]);
		
		baseStrategy->base = base;
		baseStrategy->intrinsicDefenseMultiplier = getBaseDefenseMultiplier(id, -1);
		baseStrategy->conventionalDefenseMultipliers[TRIAD_LAND] = getBaseDefenseMultiplier(id, TRIAD_LAND);
		baseStrategy->conventionalDefenseMultipliers[TRIAD_SEA] = getBaseDefenseMultiplier(id, TRIAD_SEA);
		baseStrategy->conventionalDefenseMultipliers[TRIAD_AIR] = getBaseDefenseMultiplier(id, TRIAD_AIR);
		baseStrategy->sensorOffenseMultiplier = getSensorOffenseMultiplier(base->faction_id, base->x, base->y);
		baseStrategy->sensorDefenseMultiplier = getSensorDefenseMultiplier(base->faction_id, base->x, base->y);

		debug("\n[%3d] %-25s\n", id, activeFactionInfo.baseStrategies[id].base->name);

		// add base regions
		
		int baseRegion = getBaseMapTile(id)->region;
		
		activeFactionInfo.presenceRegions.insert(getBaseMapTile(id)->region);
		activeFactionInfo.regionBaseIds[baseRegion].insert(id);

		std::set<int> baseConnectedRegions = getBaseConnectedRegions(id);

		for (int region : baseConnectedRegions)
		{
			if (activeFactionInfo.regionBaseGroups.find(region) == activeFactionInfo.regionBaseGroups.end())
			{
				activeFactionInfo.regionBaseGroups[region] = std::vector<int>();
			}

			activeFactionInfo.regionBaseGroups[region].push_back(id);

		}

	}

	debug("\n");

	// populate vehicles
	
	debug("populate vehicles - %s\n", MFactions[aiFactionId].noun_faction);
	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);
		MAP *vehicleTile = getVehicleMapTile(id);

		// store all vehicle current id in pad_0 field

		vehicle->pad_0 = id;

		// further process only own vehicles

		if (vehicle->faction_id != aiFactionId)
			continue;
		
		debug("\t[%3d] (%3d,%3d) region = %3d\n", id, vehicle->x, vehicle->y, vehicleTile->region);
		
		// add vehicle
		
		activeFactionInfo.vehicleIds.push_back(id);

		// combat vehicles

		if (isVehicleCombat(id))
		{
			// add vehicle to global list

			activeFactionInfo.combatVehicleIds.push_back(id);
			
			// scout
			
			if (isScoutVehicle(id))
			{
				activeFactionInfo.scoutVehicleIds.push_back(id);
			}

			// add surface vehicle to region list
			// except land unit in ocean
			
			if (vehicle->triad() != TRIAD_AIR && !(vehicle->triad() == TRIAD_LAND && isOceanRegion(vehicleTile->region)))
			{
				if (activeFactionInfo.regionSurfaceCombatVehicleIds.count(vehicleTile->region) == 0)
				{
					activeFactionInfo.regionSurfaceCombatVehicleIds[vehicleTile->region] = std::vector<int>();
				}
				activeFactionInfo.regionSurfaceCombatVehicleIds[vehicleTile->region].push_back(id);
				
				// add scout to region list
				
				if (isScoutVehicle(id))
				{
					if (activeFactionInfo.regionSurfaceScoutVehicleIds.count(vehicleTile->region) == 0)
					{
						activeFactionInfo.regionSurfaceScoutVehicleIds[vehicleTile->region] = std::vector<int>();
					}
					activeFactionInfo.regionSurfaceScoutVehicleIds[vehicleTile->region].push_back(id);
				}
				
			}

			// find if vehicle is at base

			MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);
			std::unordered_map<MAP *, int>::iterator baseLocationsIterator = activeFactionInfo.baseLocations.find(vehicleLocation);

			if (baseLocationsIterator == activeFactionInfo.baseLocations.end())
			{
				// add outside vehicle

				activeFactionInfo.outsideCombatVehicleIds.push_back(id);

			}
			else
			{
				BaseStrategy *baseStrategy = &(activeFactionInfo.baseStrategies[baseLocationsIterator->second]);

				// add combat vehicle to garrison
				
				if (isVehicleCombat(id))
				{
					baseStrategy->garrison.push_back(id);
				}

				// add to native protection

				double nativeProtection = calculateNativeDamageDefense(id) / 10.0;

				if (vehicle_has_ability(vehicle, ABL_TRANCE))
				{
					nativeProtection *= (1 + (double)Rules->combat_bonus_trance_vs_psi / 100.0);
				}

				baseStrategy->nativeProtection += nativeProtection;

			}

		}
		else if (isVehicleColony(id))
		{
			activeFactionInfo.colonyVehicleIds.push_back(id);
		}
		else if (isVehicleFormer(id))
		{
			activeFactionInfo.formerVehicleIds.push_back(id);
		}

	}

	// populate units

    for (int i = 0; i < 128; i++)
	{
        int id = (i < 64 ? i : (aiFactionId - 1) * 64 + i);

        UNIT *unit = &Units[id];

		// skip not enabled

		if (id < 64 && !has_tech(aiFactionId, unit->preq_tech))
			continue;

        // skip empty

        if (strlen(unit->name) == 0)
			continue;

        // skip obsolete

        if ((unit->obsolete_factions & (0x1 << aiFactionId)) != 0)
			continue;

		// add unit

		activeFactionInfo.unitIds.push_back(id);
		
		// add combat unit
		// either psi or anti psi unit or conventional with best weapon/armor
		
		if
		(
			isUnitCombat(id)
			&&
			(
				Weapon[unit->weapon_type].offense_value < 0
				||
				Armor[unit->armor_type].defense_value < 0
				||
				unit_has_ability(id, ABL_TRANCE)
				||
				unit_has_ability(id, ABL_EMPATH)
				||
				Weapon[unit->weapon_type].offense_value >= activeFactionInfo.bestWeaponOffenseValue
				||
				Armor[unit->armor_type].defense_value >= activeFactionInfo.bestArmorDefenseValue
			)
		)
		{
			activeFactionInfo.combatUnitIds.push_back(id);
			
			// populate specific triads
			
			switch (unit->triad())
			{
			case TRIAD_LAND:
				activeFactionInfo.landCombatUnitIds.push_back(id);
				activeFactionInfo.landAndAirCombatUnitIds.push_back(id);
				break;
			case TRIAD_SEA:
				activeFactionInfo.seaCombatUnitIds.push_back(id);
				activeFactionInfo.seaAndAirCombatUnitIds.push_back(id);
				break;
			case TRIAD_AIR:
				activeFactionInfo.airCombatUnitIds.push_back(id);
				activeFactionInfo.landAndAirCombatUnitIds.push_back(id);
				activeFactionInfo.seaAndAirCombatUnitIds.push_back(id);
				break;
			}
			
		}
		
	}
	
	// max mineral surplus
	
	activeFactionInfo.maxMineralSurplus = 1;
	
	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		activeFactionInfo.maxMineralSurplus = std::max(activeFactionInfo.maxMineralSurplus, base->mineral_surplus_final);
		
		if (activeFactionInfo.regionMaxMineralSurpluses.count(baseTile->region) == 0)
		{
			activeFactionInfo.regionMaxMineralSurpluses[baseTile->region] = 1;
		}
		
		activeFactionInfo.regionMaxMineralSurpluses[baseTile->region] = std::max(activeFactionInfo.regionMaxMineralSurpluses[baseTile->region], base->mineral_surplus_final);
		
	}
	
	// populate factions airbases
	
	for (int factionId = 1; factionId < 8; factionId++)
	{
		activeFactionInfo.factionInfos[factionId].airbases.clear();
		
		// stationary airbases
		
		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			Location location = getMapIndexLocation(mapIndex);
			MAP *tile = getMapTile(mapIndex);
			
			if
			(
				map_has_item(tile, TERRA_BASE_IN_TILE | TERRA_AIRBASE)
				&&
				(tile->owner == factionId || has_pact(factionId, tile->owner))
			)
			{
				activeFactionInfo.factionInfos[factionId].airbases.push_back(location);
			}
			
		}
		
		// mobile airbases
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			if
			(
				vehicle_has_ability(vehicleId, ABL_CARRIER)
				&&
				(vehicle->faction_id == factionId || has_pact(factionId, vehicle->faction_id))
			)
			{
				activeFactionInfo.factionInfos[factionId].airbases.push_back(Location(vehicle->x, vehicle->y));
			}
			
		}
		
	}
	
	// base exposure
	
	populateBaseExposures();
	
}

/*
Finds all regions those are accessible by other factions.
Set bases accessible to shared ocean regions.
*/
void setSharedOceanRegions()
{
	// find shared ocean regions
	
	std::unordered_set<int> sharedOceanRegions;
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		
		// skip own
		
		if (base->faction_id == aiFactionId)
			continue;
		
		// get connected ocean regions
		
		std::set<int> baseConnectedOceanRegions = getBaseConnectedOceanRegions(baseId);
		
		// add regions
		
		sharedOceanRegions.insert(baseConnectedOceanRegions.begin(), baseConnectedOceanRegions.end());
		
	}
	
	// update own bases
	
	for (int baseId : activeFactionInfo.baseIds)
	{
		// clear flag by default
		
		activeFactionInfo.baseStrategies[baseId].inSharedOceanRegion = false;
		
		// get connected ocean regions
		
		std::set<int> baseConnectedOceanRegions = getBaseConnectedOceanRegions(baseId);
		
		for (int baseConnectedOceanRegion : baseConnectedOceanRegions)
		{
			if (sharedOceanRegions.count(baseConnectedOceanRegion))
			{
				activeFactionInfo.baseStrategies[baseId].inSharedOceanRegion = true;
				break;
			}
			
		}
		
	}
	
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

Location getNearestPodLocation(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehilceTile = getVehicleMapTile(vehicleId);
	int triad = vehicle->triad();
	
	Location nearestPodLocation;
	int minRange = INT_MAX;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		Location location = getMapIndexLocation(mapIndex);
		MAP* tile = getMapTile(mapIndex);
		
		if
		(
			(triad == TRIAD_AIR || tile->region == vehilceTile->region)
			&&
			map_has_item(tile, TERRA_SUPPLY_POD)
		)
		{
			int range = map_range(vehicle->x, vehicle->y, location.x, location.y);
			
			if (range < minRange)
			{
				location.set(location);
				minRange = range;
			}
			
		}
		
	}
	
	return nearestPodLocation;
	
}

void designUnits()
{
	// get best reactor
	
	int bestReactor = best_reactor(aiFactionId);
	
	// defenders
	
	int bestArmor = getFactionBestArmor(aiFactionId);
	
	std::vector<int> chassisIds = {CHS_INFANTRY, CHS_FOIL};
	std::vector<int> weaponIds = {WPN_HAND_WEAPONS};
	std::vector<int> armorIds = {bestArmor};
	std::vector<int> abilitiesSets = {0, ABL_COMM_JAMMER, ABL_AAA};
	
	proposeMultiplePrototypes(aiFactionId, chassisIds, weaponIds, armorIds, abilitiesSets, bestReactor, PLAN_DEFENSIVE, NULL);
	
}

/*
Propose multiple prototype combinations.
*/
void proposeMultiplePrototypes(int factionId, std::vector<int> chassisIds, std::vector<int> weaponIds, std::vector<int> armorIds, std::vector<int> abilitiesSets, int reactorId, int plan, char *name)
{
	for (int chassisId : chassisIds)
	{
		for (int weaponId : weaponIds)
		{
			for (int armorId : armorIds)
			{
				for (int abilitiesSet : abilitiesSets)
				{
					checkAndProposePrototype(factionId, chassisId, weaponId, armorId, abilitiesSet, reactorId, plan, name);
					
				}
				
			}
			
		}
		
	}
	
}

/*
Verify proposed prototype is allowed and propose it if yes.
Verifies all technologies are available and abilities area allowed.
*/
void checkAndProposePrototype(int factionId, int chassisId, int weaponId, int armorId, int abilities, int reactorId, int plan, char *name)
{
	// check chassis is available
	
	if (!has_chassis(factionId, chassisId))
		return;
	
	// check weapon is available
	
	if (!has_weapon(factionId, weaponId))
		return;
	
	// check armor is available
	
	if (!has_armor(factionId, armorId))
		return;
	
	// check reactor is available
	
	if (!has_reactor(factionId, reactorId))
		return;
	
	// check abilities are available and allowed
	
	for (int abilityId = ABL_ID_SUPER_TERRAFORMER; abilityId <= ABL_ID_ALGO_ENHANCEMENT; abilityId++)
	{
		int abilityFlag = (0x1 << abilityId);
		
		if (abilities & abilityFlag == 0)
			continue;
		
		if (!has_ability(factionId, abilityId))
			return;
		
		CAbility *ability = &(Ability[abilityId]);
		
		// not allowed for triad
		
		switch (Chassis[chassisId].triad)
		{
		case TRIAD_LAND:
			if (ability->flags & AFLAG_ALLOWED_LAND_UNIT == 0)
				return;
			break;
			
		case TRIAD_SEA:
			if (ability->flags & AFLAG_ALLOWED_SEA_UNIT == 0)
				return;
			break;
			
		case TRIAD_AIR:
			if (ability->flags & AFLAG_ALLOWED_AIR_UNIT == 0)
				return;
			break;
			
		}
		
		// not allowed for combat unit
		
		if (Weapon[weaponId].offense_value != 0 && ability->flags & AFLAG_ALLOWED_COMBAT_UNIT == 0)
			return;
		
		// not allowed for terraform unit
		
		if (weaponId == WPN_TERRAFORMING_UNIT && ability->flags & AFLAG_ALLOWED_TERRAFORM_UNIT == 0)
			return;
		
		// not allowed for non-combat unit
		
		if (Weapon[weaponId].offense_value == 0 && ability->flags & AFLAG_ALLOWED_NONCOMBAT_UNIT == 0)
			return;
		
		// not allowed for probe team
		
		if (weaponId == WPN_PROBE_TEAM && ability->flags & AFLAG_NOT_ALLOWED_PROBE_TEAM == 1)
			return;
		
		// not allowed for non transport unit
		
		if (weaponId != WPN_TROOP_TRANSPORT && ability->flags & AFLAG_TRANSPORT_ONLY_UNIT == 1)
			return;
		
		// not allowed for fast unit
		
		if (chassisId == CHS_INFANTRY && Chassis[chassisId].speed > 1 && ability->flags & AFLAG_NOT_ALLOWED_FAST_UNIT == 1)
			return;
		
		// not allowed for non probes
		
		if (weaponId != WPN_PROBE_TEAM && ability->flags & AFLAG_ONLY_PROBE_TEAM == 1)
			return;
		
	}
	
	// propose prototype
	
	propose_proto(factionId, chassisId, weaponId, armorId, abilities, reactorId, plan, name);
	
}

void evaluateBaseNativeDefenseDemands()
{
	debug("evaluateBaseNativeDefenseDemands\n");
	
	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool ocean = isOceanRegion(baseTile->region);
		
		debug("\t%s\n", base->name);
		
		// estimate native attack strength
		
		double nativePsiAttackStrength = getNativePsiAttackStrength(ocean ? TRIAD_SEA : TRIAD_LAND);
		debug("\t\tnativePsiAttackStrength = %f\n", nativePsiAttackStrength);
		
		// calculate count of the nearest unpopulated tiles
		
		int unpopulatedTiles = 28 - (nearby_items(base->x, base->y, 3, TERRA_BASE_RADIUS) - 21);
		debug("\t\tunpopulatedTiles = %d\n", unpopulatedTiles);
		
		// estimate count
		
		double anticipatedCount = 0.5 + 1.0 * ((double)unpopulatedTiles / 28.0);
		debug("\t\tanticipatedCount = %f\n", anticipatedCount);
		
		// calculate pop chance
		
		double popChance = 0.0;
		
		for (int otherBaseId : activeFactionInfo.baseIds)
		{
			BASE *otherBase =&(Bases[otherBaseId]);
			MAP *otherBaseTile = getBaseMapTile(otherBaseId);
			
			// same region only
			
			if (otherBaseTile->region != baseTile->region)
				continue;
			
			// calculate range
			
			int range = map_range(base->x, base->y, otherBase->x, otherBase->y);
			
			// only in vicinity
			
			if (range >= 10)
				continue;
			
			// compute contribution
			
			popChance += ((double)std::min(100, otherBase->eco_damage) / 100.0) * (1.0 - (double)range / 10.0);
			
		}
		
		anticipatedCount += (double)((Factions[aiFactionId].pop_total + 1) / 3) * popChance;
		
		double anticipatedNativePsiAttackStrength = std::max(0.0, nativePsiAttackStrength * anticipatedCount);

		// calculate current native protectors in base

		double baseNativeProtection = std::max(0.0, getBaseNativeProtection(baseId));
		double remainingBaseNativeProtection = anticipatedNativePsiAttackStrength - baseNativeProtection;
		
		debug("\t\tbaseNativeProtection=%f, remainingBaseNativeProtection=%f\n", baseNativeProtection, remainingBaseNativeProtection);
		
		// store demand
		
		activeFactionInfo.baseAnticipatedNativeAttackStrengths[baseId] = anticipatedNativePsiAttackStrength;
		activeFactionInfo.baseRemainingNativeProtectionDemands[baseId] = remainingBaseNativeProtection;
		
	}
	
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

int getNearestBaseId(int x, int y, std::unordered_set<int> baseIds)
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

int getNearestBaseRange(int x, int y, std::unordered_set<int> baseIds)
{
	int nearestBaseRange = INT_MAX;

	for (int baseId : baseIds)
	{
		BASE *base = &(Bases[baseId]);

		nearestBaseRange = std::min(nearestBaseRange, map_range(x, y, base->x, base->y));

	}

	return nearestBaseRange;

}

void evaluateBaseExposures()
{
	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool ocean = isOceanRegion(baseTile->region);
		
		// find nearest not own tile
		
		int nearestNotOwnTileRange = INT_MAX;

		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			Location location = getMapIndexLocation(mapIndex);
			MAP* tile = getMapTile(mapIndex);
			
			// exclude own tiles
			
			if (tile->owner == aiFactionId)
				continue;
			
			// calculate range
			
			int range = map_range(base->x, base->y, location.x, location.y);
			
			nearestNotOwnTileRange = std::min(nearestNotOwnTileRange, range);
			
		}
		
		// set base distance based on realm
		
		double baseDistance = (ocean ? 7.0 : 3.0);
		
		// estimate exposure
		
		double exposure = 1.0 / std::max(1.0, (double)nearestNotOwnTileRange / baseDistance);
		
		activeFactionInfo.baseStrategies[baseId].exposure = exposure;
		
	}
	
}

int getFactionBestPrototypedWeaponOffenseValue(int factionId)
{
	int bestWeaponOffenseValue = 0;
	
	for (int unitId : getFactionPrototypes(factionId, false))
	{
		UNIT *unit = &(Units[unitId]);
		
		// skip non combat units

		if (!isUnitCombat(unitId))
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

		if (!isUnitCombat(unitId))
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
		
		if (!isVehicleCombat(otherVehicleId))
			continue;
		
		// unit is in different region and not air
		
		if (otherVehicle->triad() != TRIAD_AIR && otherVehicleTile->region != vehicleTile->region)
			continue;
		
		// calculate range
		
		int range = map_range(otherVehicle->x, otherVehicle->y, vehicle->x, vehicle->y);
		
		// threatened in one turn
		
		if (getVehicleSpeedWithoutRoads(otherVehicleId) >= range)
		{
			threatened = true;
			break;
		}
		
	}
	
	return threatened;
	
}

bool isDestinationReachable(int vehicleId, int x, int y)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MAP *destinationTile = getMapTile(x, y);
	
	assert(vehicleTile != NULL);
	assert(destinationTile != NULL);
	
	bool reachable = false;
	
	switch (Units[vehicle->unit_id].chassis_type)
	{
	case CHS_GRAVSHIP:
		
		reachable = true;
		
		break;
		
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		
		{
			int rangeToNearestFactionAirbase = getRangeToNearestFactionAirbase(x, y, vehicle->faction_id);
			int turns = (vehicle->triad() == CHS_COPTER ? 2 : 1);
			int reachableRange = rangeToNearestFactionAirbase * turns;
			int range = map_range(vehicle->x, vehicle->y, x, y);
			
			reachable = (range <= reachableRange);
			
		}
		
		break;
		
	case CHS_FOIL:
	case CHS_CRUISER:
		
		{
			std::unordered_set<int> vehicleAdjacentOceanRegions = getAdjacentOceanRegions(vehicle->x, vehicle->y);
			std::unordered_set<int> destinationAdjacentOceanRegions = getAdjacentOceanRegions(x, y);
			
			for (int destinationAdjacentOceanRegion : destinationAdjacentOceanRegions)
			{
				if (vehicleAdjacentOceanRegions.count(destinationAdjacentOceanRegion) != 0)
				{
					reachable = true;
					break;
				}
			}
			
		}
		
		break;
		
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		
		reachable = (destinationTile->region == vehicleTile->region);
		
		break;
		
	}
	
	return reachable;
	
}

int getRangeToNearestFactionAirbase(int x, int y, int factionId)
{
	int rangeToNearestFactionAirbase = INT_MAX;
	
	for (Location location : activeFactionInfo.factionInfos[factionId].airbases)
	{
		int range = map_range(x, y, location.x, location.y);
		
		rangeToNearestFactionAirbase = std::min(rangeToNearestFactionAirbase, range);
		
	}
	
	return rangeToNearestFactionAirbase;
	
}

/*
Calculates movement points.
*/
int getVehicleTurnsToDestination(int vehicleId, int x, int y)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MAP *destinationTile = getMapTile(x, y);
	
	assert(vehicleTile != NULL);
	assert(destinationTile != NULL);
	
	int turns = INT_MAX;
	
	if (!isDestinationReachable(vehicleId, x, y))
		return turns;
	
	switch (vehicle->triad())
	{
	case TRIAD_AIR:
	case TRIAD_SEA:
		
		{
			int range = map_range(vehicle->x, vehicle->y, x, y);
			int speed = getVehicleSpeedWithoutRoads(vehicleId);
			turns = (range + (speed - 1)) / speed;
			
		}
		
		break;
		
	case TRIAD_LAND:
		
		{
			int vehicleMovementPoints = mod_veh_speed(vehicleId);
			int destinationMapIndex = getLocationMapIndex(x, y);
			std::unordered_map<int, int> currentLocations;
			std::unordered_set<int> deletedLocations;
			std::unordered_set<int> minCostLocations;
			currentLocations[getLocationMapIndex(vehicle->x, vehicle->y)] = 0;
			int minCost = 0;
			
			bool run = true;
			while (true)
			{
				minCostLocations.clear();
				
				for (std::pair<int, int> element : currentLocations)
				{
					int mapIndex = element.first;
					int cost = element.second;
					
					// process only min cost locations
					
					if (cost > minCost)
						continue;
					
					// register min cost location
					
					minCostLocations.insert(mapIndex);
					
					// iterate adjacent tiles
					
					for (int dx = -2; dx <= +2; dx++)
					{
						for (int dy = -(2 - abs(dx)); dy <= +(2 - abs(dx)); dy += 2)
						{
							// exclude center
							
							if (dx == 0 && dy == 0)
								continue;
							
							int adjacentTileMapIndex = getLocationMapIndex(x + dx, y + dy);
							MAP *adjacentTile = getMapTile(x + dx, y + dy);
							
							if (adjacentTile == NULL)
								continue;
							
							// exclude ocean
							
							if (is_ocean(adjacentTile))
								continue;
							
							// exclude deleted
							
							if (deletedLocations.count(adjacentTileMapIndex) != 0)
								continue;
							
							// calculate cost
							
							int adjacentTileCost = cost + mod_hex_cost(vehicle->unit_id, vehicle->faction_id, vehicle->x, vehicle->y, x, y, 0);
							
							// create/update node
							
							if (currentLocations.count(adjacentTileMapIndex) == 0 || adjacentTileCost < currentLocations[adjacentTileMapIndex])
							{
								currentLocations[adjacentTileMapIndex] = adjacentTileCost;
								
								// exit condition
								
								if (adjacentTileMapIndex == destinationMapIndex)
								{
									turns = (cost + vehicleMovementPoints) / vehicleMovementPoints;
									run = false;
								}
								
							}
							
						}
						
					}
					
				}
				
				if (!run)
				{
					break;
				}
				
				// remove min cost locations
				
				for (int mapIndex : minCostLocations)
				{
					deletedLocations.insert(mapIndex);
					currentLocations.erase(mapIndex);
				}
				
				// calculate new min cost
				
				minCost = INT_MAX;
				
				for (std::pair<int, int> element : currentLocations)
				{
					int cost = element.second;
					
					minCost = std::min(minCost, cost);
					
				}
				
			}
			
		}
		
		break;
		
	}
	
	return turns;
	
}

void populateBaseExposures()
{
	for (int baseId : activeFactionInfo.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		bool ocean = isOceanRegion(baseTile->region);
		
		// find nearest enemy base distance
		
		int nearestEnemyBaseDistance = getNearestEnemyBaseDistance(baseId);
		
		// get exposure decay distance
		
		double exposureDecayDistance = (ocean ? 10.0 : 5.0);
		
		// calculate exposure
		
		double exposure = std::max(exposureDecayDistance, (double)nearestEnemyBaseDistance) / exposureDecayDistance;
		
		// set exposure
		
		activeFactionInfo.baseStrategies[baseId].exposure = exposure;
		
	}
	
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

void evaluateDefenseDemand()
{
	debug("\n\n==================================================\nevaluateDefenseDemand - %s\n", MFactions[aiFactionId].noun_faction);
	
	// evaluate base defense demands
	
	activeFactionInfo.mostVulnerableBaseId = -1;
	activeFactionInfo.mostVulnerableBaseDefenseDemand = 0.0;
	
	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool ocean = is_ocean(baseTile);
		BaseStrategy *baseStrategy = &(activeFactionInfo.baseStrategies[baseId]);
		
		debug
		(
			"\n\t<>\n\t(%3d,%3d) %s, sensorOffenseMultiplier=%f, sensorDefenseMultiplier=%f, intrinsicDefenseMultiplier=%f, conventionalDefenseMultipliers[TRIAD_LAND]=%f, conventionalDefenseMultipliers[TRIAD_SEA]=%f, conventionalDefenseMultipliers[TRIAD_AIR]=%f\n",
			base->x,
			base->y,
			base->name,
			baseStrategy->sensorOffenseMultiplier,
			baseStrategy->sensorDefenseMultiplier,
			baseStrategy->intrinsicDefenseMultiplier,
			baseStrategy->conventionalDefenseMultipliers[TRIAD_LAND],
			baseStrategy->conventionalDefenseMultipliers[TRIAD_SEA],
			baseStrategy->conventionalDefenseMultipliers[TRIAD_AIR]
		);
		
		// calculate defender strength
		
		MilitaryStrength *defenderStrength = &(baseStrategy->defenderStrength);
		
		debug("\t\tdefenderMilitaryStrength\n");
		
		for (int vehicleId : activeFactionInfo.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			int unitId = vehicle->unit_id;
			UNIT *unit = &(Units[unitId]);
			int chassisId = unit->chassis_type;
			CChassis *chassis = &(Chassis[chassisId]);
			int triad = chassis->triad;
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// exclude sea vehicle not in the region
			
			if (triad == TRIAD_SEA && !isVehicleInRegion(vehicleId, baseTile->region))
				continue;
			
			// calculate vehicle distance to base
			
			int distance = map_range(base->x, base->y, vehicle->x, vehicle->y);
			
			// limit only vehicle by no farther than 20 tiles away
			
			if (distance > 20)
				continue;
			
			// calculate vehicle speed
			
			double speed = (double)veh_speed_without_roads(vehicleId);
			double roadSpeed = (double)mod_veh_speed(vehicleId);
			
			// increase land vehicle speed based on game stage
			
			if (triad == TRIAD_LAND)
			{
				speed = speed + (roadSpeed - speed) * (double)std::min(250, std::max(0, *current_turn - 50)) / 250.0;
			}
			
			// calculate number of turns to reach the base
			
			double turns = (double)distance / speed;
			
			// increase effective turns for land unit in different region or in ocean
			
			if (triad == TRIAD_LAND && distance > 0 && (isOceanRegion(vehicleTile->region) || vehicleTile->region != baseTile->region))
			{
				turns *= 5;
			}
			
			// reduce weight based on time to reach the base
			// every 10 turns reduce weight in half
			
			double weight = pow(0.5, turns / 10.0);
			
			debug("\t\t\t(%3d,%3d) %-25s: distance=%3d, turns=%6.2f, weight=%f\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name, distance, turns, weight);
			
			// update total weight
			
			defenderStrength->totalWeight += weight;
			
			// pack unit type, create and get entry
			
			int unitType = packUnitType(unitId);
			
			if (defenderStrength->unitTypeStrengths.count(unitType) == 0)
			{
				defenderStrength->unitTypeStrengths[unitType] = UnitTypeStrength();
			}
			
			UnitTypeStrength *unitTypeStrength = &(defenderStrength->unitTypeStrengths[unitType]);
			
			// calculate strength
			
			if (isUnitTypeNative(unitType))
			{
				unitTypeStrength->weight += weight;
				
				// native unit initiates psi combat only
				
				double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId);
				double psiDefenseStrength = getVehiclePsiDefenseStrength(vehicleId, true);
				
				unitTypeStrength->psiOffense += weight * psiOffenseStrength;
				unitTypeStrength->psiDefense += weight * psiDefenseStrength;
				
			}
			else
			{
				unitTypeStrength->weight += weight;
				
				// regular unit initiates psi combat when engage native unit
				
				double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId);
				double psiDefenseStrength = getVehiclePsiDefenseStrength(vehicleId, true);
				
				unitTypeStrength->psiOffense += weight * psiOffenseStrength;
				unitTypeStrength->psiDefense += weight * psiDefenseStrength;
				
				// regular unit initiates conventional combat when engage regular unit
				
				double conventionalOffenseStrength = getVehicleConventionalOffenseStrength(vehicleId);
				double conventionalDefenseStrength = getVehicleConventionalDefenseStrength(vehicleId, true);
				
				unitTypeStrength->conventionalOffense += weight * conventionalOffenseStrength;
				unitTypeStrength->conventionalDefense += weight * conventionalDefenseStrength;
				
			}
			
		}
		
		// normalize weights
		
		defenderStrength->normalize();
		
		debug("\t\t\ttotalWeight=%f\n", defenderStrength->totalWeight);
		if (DEBUG)
		{
			for (const auto &k : defenderStrength->unitTypeStrengths)
			{
				const int unitType = k.first;
				const UnitTypeStrength *unitTypeStrength = &(k.second);
				
				debug("\t\t\t\tweight=%f, native=%d, chassisType=%d, psiOffense=%f, psiDefense=%f, conventionalOffense=%f, conventionalDefense=%f, unitType=%s\n", unitTypeStrength->weight, (isUnitTypeNative(unitType) ? 1 : 0), getUnitTypeChassisType(unitType), unitTypeStrength->psiOffense, unitTypeStrength->psiDefense, unitTypeStrength->conventionalOffense, unitTypeStrength->conventionalDefense, getUnitTypeAbilitiesString(unitType).c_str());
				
			}
			
		}
		
		// calculate opponent strength
		
		MilitaryStrength *opponentStrength = &(baseStrategy->opponentStrength);
		
		debug("\t\topponentMilitaryStrength\n");
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			int unitId = vehicle->unit_id;
			UNIT *unit = &(Units[unitId]);
			int chassisId = unit->chassis_type;
			CChassis *chassis = &(Chassis[chassisId]);
			int triad = chassis->triad;
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// not alien
			
			if (vehicle->faction_id == 0)
				continue;
			
			// not own
			
			if (vehicle->faction_id == aiFactionId)
				continue;
			
			// combat only
			
			if (!isVehicleCombat(vehicleId))
				continue;
			
			// exclude vehicles unable to attack base
			
			if (!ocean && triad == TRIAD_SEA || ocean && triad == TRIAD_LAND && !vehicle_has_ability(vehicleId, ABL_AMPHIBIOUS))
				continue;
			
			// exclude sea units in different region - they cannot possibly reach us
			
			if (triad == TRIAD_SEA && vehicleTile->region != baseTile->region)
				continue;
			
			// calculate vehicle distance to base
			
			int distance = map_range(base->x, base->y, vehicle->x, vehicle->y);
			
			// limit only vehicle by no farther than 20 tiles away
			
			if (distance > 20)
				continue;
			
			// calculate vehicle speed
			
			double speed = (double)veh_chassis_speed(vehicleId);
			double roadSpeed = (double)mod_veh_speed(vehicleId);
			
			// increase land vehicle speed based on game stage
			
			if (triad == TRIAD_LAND)
			{
				speed = speed + (roadSpeed - speed) * (double)std::min(250, std::max(0, *current_turn - 50)) / 250.0;
			}
			
			// calculate number of turns to reach the base
			
			double turns = (double)distance / speed;
			
			// increase effective turns for land unit in different region or in ocean
			
			if (triad == TRIAD_LAND && distance > 0 && (isOceanRegion(vehicleTile->region) || vehicleTile->region != baseTile->region))
			{
				turns *= 5;
			}
			
			// reduce weight based on time to reach the base
			// every 10 turns reduce weight in half
			
			double weight = pow(0.5, turns / 10.0);
			
			// modify strength multiplier based on diplomatic relations
			
			weight *= activeFactionInfo.factionInfos[vehicle->faction_id].threatCoefficient;
			
			// exclude empty weight
			
			if (weight == 0.0)
				continue;
			
			debug("\t\t\t(%3d,%3d) %-25s: distance=%3d, turns=%6.2f, weight=%f\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name, distance, turns, weight);
			
			// update total weight
			
			opponentStrength->totalWeight += weight;
			
			// pack unit type, create and get entry
			
			int unitType = packUnitType(unitId);
			
			if (opponentStrength->unitTypeStrengths.count(unitType) == 0)
			{
				opponentStrength->unitTypeStrengths[unitType] = UnitTypeStrength();
			}
			
			UnitTypeStrength *unitTypeStrength = &(opponentStrength->unitTypeStrengths[unitType]);
			
			// calculate strength
			
			if (isUnitTypeNative(unitType))
			{
				unitTypeStrength->weight += weight;
				
				// native unit initiates psi combat only
				
				double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId) / baseStrategy->sensorDefenseMultiplier / baseStrategy->intrinsicDefenseMultiplier;
				double psiDefenseStrength = getVehiclePsiDefenseStrength(vehicleId, false) / baseStrategy->sensorOffenseMultiplier;
				
				unitTypeStrength->psiOffense += weight * psiOffenseStrength;
				unitTypeStrength->psiDefense += weight * psiDefenseStrength;
				
			}
			else
			{
				unitTypeStrength->weight += weight;
				
				// regular unit initiates psi combat when engage native unit
				
				double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId) / baseStrategy->sensorDefenseMultiplier / baseStrategy->intrinsicDefenseMultiplier;
				double psiDefenseStrength = getVehiclePsiDefenseStrength(vehicleId, false) / baseStrategy->sensorOffenseMultiplier;
				
				unitTypeStrength->psiOffense += weight * psiOffenseStrength;
				unitTypeStrength->psiDefense += weight * psiDefenseStrength;
				
				// regular unit initiates conventional combat when engage regular unit
				
				double baseDefenseMultiplier = (isUnitTypeHasAbility(unitType, ABL_BLINK_DISPLACER) ? baseStrategy->intrinsicDefenseMultiplier : baseStrategy->conventionalDefenseMultipliers[triad]);
				
				double conventionalOffenseStrength = getVehicleConventionalOffenseStrength(vehicleId) / baseStrategy->sensorDefenseMultiplier / baseDefenseMultiplier;
				double conventionalDefenseStrength = getVehicleConventionalDefenseStrength(vehicleId, false) / baseStrategy->sensorOffenseMultiplier;
				
				unitTypeStrength->conventionalOffense += weight * conventionalOffenseStrength;
				unitTypeStrength->conventionalDefense += weight * conventionalDefenseStrength;
				
			}
			
		}
		
		// normalize weights
		
		opponentStrength->normalize();
		
		debug("\t\t\ttotalWeight=%f\n", opponentStrength->totalWeight);
		if (DEBUG)
		{
			for (const auto &k : opponentStrength->unitTypeStrengths)
			{
				const int unitType = k.first;
				const UnitTypeStrength *unitTypeStrength = &(k.second);
				
				debug("\t\t\t\tweight=%f, native=%d, chassisType=%d, psiOffense=%f, psiDefense=%f, conventionalOffense=%f, conventionalDefense=%f, unitType=%s\n", unitTypeStrength->weight, (isUnitTypeNative(unitType) ? 1 : 0), getUnitTypeChassisType(unitType), unitTypeStrength->psiOffense, unitTypeStrength->psiDefense, unitTypeStrength->conventionalOffense, unitTypeStrength->conventionalDefense, getUnitTypeAbilitiesString(unitType).c_str());
				
			}
			
		}
		
		// evaluate relative portion of defender destroyed
		
		debug("\t\tdefenderDestroyed\n");
		
		double defenderDestroyed = 0.0;
		
		for (const auto &opponentUnitTypeStrengthEntry : opponentStrength->unitTypeStrengths)
		{
			const int opponentUnitType = opponentUnitTypeStrengthEntry.first;
			const UnitTypeStrength *opponentUnitTypeStrength = &(opponentUnitTypeStrengthEntry.second);
			
			for (const auto defenderStrengthEntry : defenderStrength->unitTypeStrengths)
			{
				const int defenderUnitType = defenderStrengthEntry.first;
				const UnitTypeStrength *defenderUnitTypeStrength = &(defenderStrengthEntry.second);
				
				// impossible combinations
				
				// two needlejets without air superiority
				if
				(
					(isUnitTypeHasChassisType(opponentUnitType, CHS_NEEDLEJET) && !isUnitTypeHasAbility(defenderUnitType, ABL_AIR_SUPERIORITY))
					&&
					(isUnitTypeHasChassisType(defenderUnitType, CHS_NEEDLEJET) && !isUnitTypeHasAbility(opponentUnitType, ABL_AIR_SUPERIORITY))
				)
				{
					continue;
				}
				
				// calculate odds
				
				double attackOdds;
				double defendOdds;
				
				if (isUnitTypeNative(opponentUnitType) || isUnitTypeNative(defenderUnitType))
				{
					attackOdds = opponentUnitTypeStrength->psiOffense / defenderUnitTypeStrength->psiDefense;
					defendOdds = opponentUnitTypeStrength->psiDefense / defenderUnitTypeStrength->psiOffense;
				}
				else
				{
					attackOdds = opponentUnitTypeStrength->conventionalOffense / defenderUnitTypeStrength->conventionalDefense * getConventionalCombatBonusMultiplier(opponentUnitType, defenderUnitType);
					defendOdds = opponentUnitTypeStrength->conventionalDefense / defenderUnitTypeStrength->conventionalOffense * getConventionalCombatBonusMultiplier(defenderUnitType, opponentUnitType);
				}
				
				// calculate attack and defend probabilities
				
				double attackProbability;
				double defendProbability;
				
				// cases when unit at base cannot attack
				
				if
				(
					// unit without air superiority cannot attack neeglejet
					(isUnitTypeHasChassisType(opponentUnitType, CHS_NEEDLEJET) && !isUnitTypeHasAbility(defenderUnitType, ABL_AIR_SUPERIORITY))
					||
					// land unit cannot attack from sea base
					(ocean && getUnitTypeTriad(defenderUnitType) == TRIAD_LAND)
					||
					// sea unit cannot attack from land base
					(!ocean && getUnitTypeTriad(defenderUnitType) == TRIAD_SEA)
				)
				{
					attackProbability = 1.0;
					defendProbability = 0.0;
				}
				else
				{
					attackProbability = 0.5;
					defendProbability = 0.5;
				}
				
				// work out defender choice
				// defender can choose not to attack if it worse than defending
				
				if
				(
					defendProbability > 0.0
					&&
					attackOdds < defendOdds
				)
				{
					attackProbability = 1.0;
					defendProbability = 0.0;
				}
				
				// calculate defender destroyed
				
				double defenderDestroyedTypeVsType =
					// occurence probability
					(opponentUnitTypeStrength->weight * defenderUnitTypeStrength->weight)
					*
					(
						attackProbability * attackOdds
						+
						defendProbability * defendOdds
					)
				;
				
				debug
				(
					"\t\t\topponent: weight=%f, native=%d, chassisType=%d, unitType=%s\n\t\t\tdefender: weight=%f, native=%d, chassisType=%d, unitType=%s\n\t\t\t\tattack: probability=%f, odds=%f\n\t\t\t\tdefend: probatility=%f, odds=%f\n\t\t\t\tdefenderDestroyedTypeVsType=%f\n",
					opponentUnitTypeStrength->weight,
					(isUnitTypeNative(opponentUnitType) ? 1 : 0), getUnitTypeChassisType(opponentUnitType), getUnitTypeAbilitiesString(opponentUnitType).c_str(),
					defenderUnitTypeStrength->weight,
					(isUnitTypeNative(defenderUnitType) ? 1 : 0), getUnitTypeChassisType(defenderUnitType), getUnitTypeAbilitiesString(defenderUnitType).c_str(),
					attackProbability,
					attackOdds,
					defendProbability,
					defendOdds,
					defenderDestroyedTypeVsType
				)
				;
				
				// update summaries
				
				defenderDestroyed += defenderDestroyedTypeVsType;
				
			}
			
		}
		
		// defender destroyed total
		
		double defenderDestroyedTotal =
			defenderDestroyed
			*
			// proportion of total weight
			(opponentStrength->totalWeight / defenderStrength->totalWeight)
		;
		
		debug("\t\tdefenderDestroyed=%f, defenderDestroyedTotal=%f\n", defenderDestroyed, defenderDestroyedTotal);
		
		// set defenseDemand
		
		if (defenderDestroyedTotal > 1.0)
		{
			activeFactionInfo.baseStrategies[baseId].defenseDemand = (defenderDestroyedTotal - 1.0) / defenderDestroyedTotal;
		}
		
		debug("\t\tdefenseDemand=%f\n", activeFactionInfo.baseStrategies[baseId].defenseDemand);
		
		// update global values
		
		if (activeFactionInfo.baseStrategies[baseId].defenseDemand > activeFactionInfo.mostVulnerableBaseDefenseDemand)
		{
			activeFactionInfo.mostVulnerableBaseId = baseId;
			activeFactionInfo.mostVulnerableBaseDefenseDemand = activeFactionInfo.baseStrategies[baseId].defenseDemand;
		}
		
	}
	
	debug("\n\tmostVulnerableBase = %s, mostVulnerableBaseDefenseDemand = %f\n", Bases[activeFactionInfo.mostVulnerableBaseId].name, activeFactionInfo.mostVulnerableBaseDefenseDemand);
	
	// assign defense demand targets
	
	debug("\n\t->\n\ttargetBases\n");
	
	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		int bestTargetBaseId = -1;
		double bestTargetBasePreference = 0.0;
		
		for (int targetBaseId : activeFactionInfo.baseIds)
		{
			BASE *targetBase = &(Bases[targetBaseId]);
			MAP *targetBaseTile = getBaseMapTile(targetBaseId);
			
			// ignore target ocean base without access to it
			
			if (isOceanRegion(targetBaseTile->region) && !isBaseConnectedToRegion(baseId, targetBaseTile->region))
				continue;
			
			// ignore target land base in different region
			
			if (isLandRegion(targetBaseTile->region) && baseTile->region != targetBaseTile->region)
				continue;
			
			// get range
			
			double range = (double)map_range(base->x, base->y, targetBase->x, targetBase->y);
			
			// adjust range for different regions except coastal base supplying sea units to ocean base
			
			if (baseTile->region != targetBaseTile->region && !(isOceanRegion(targetBaseTile->region) && isBaseConnectedToRegion(baseId, targetBaseTile->region)))
			{
				range *= 5;
			}
			
			// calculate range coefficient
			// preference halves every 20 tiles
			
			double rangeCoefficient = pow(0.5, range / 20.0);
			
			// calculate preference
			
			double targetBasePreference = rangeCoefficient * activeFactionInfo.baseStrategies[targetBaseId].defenseDemand;
			
			// update best
			
			if (targetBasePreference > bestTargetBasePreference)
			{
				bestTargetBaseId = targetBaseId;
				bestTargetBasePreference = targetBasePreference;
			}
			
		}
		
		activeFactionInfo.baseStrategies[baseId].targetBaseId = bestTargetBaseId;
		
		debug("\t\t%-25s -> %-25s\n", Bases[baseId].name, (bestTargetBaseId == -1 ? "" : Bases[bestTargetBaseId].name));
		
	}
	
	debug("\n");
	
}

/*
Packs significant combat unit type values into integer.
*/
int packUnitType(int unitId)
{
	UNIT *unit = &(Units[unitId]);
	
	return
		// combat ability flags
		(unit->ability_flags & COMBAT_ABILITY_FLAGS)
		|
		// chassis type
		(unit->chassis_type << (ABL_ID_DISSOCIATIVE_WAVE + 1))
		|
		// native
		((isNativeUnit(unitId) ? 1 : 0) << (ABL_ID_DISSOCIATIVE_WAVE + 1 + 4))
	;
	
}

int isUnitTypeNative(int unitType)
{
	return (unitType >> (ABL_ID_DISSOCIATIVE_WAVE + 1 + 4));
}

int getUnitTypeChassisType(int unitType)
{
	return (unitType >> (ABL_ID_DISSOCIATIVE_WAVE + 1));
}

int isUnitTypeHasChassisType(int unitType, int chassisType)
{
	return getUnitTypeChassisType(unitType) == chassisType;
}

int getUnitTypeTriad(int unitType)
{
	return Chassis[getUnitTypeChassisType(unitType)].triad;
}

int isUnitTypeHasTriad(int unitType, int triad)
{
	return getUnitTypeTriad(unitType) == triad;
}

int isUnitTypeHasAbility(int unitType, int abilityFlag)
{
	return ((unitType & abilityFlag) != 0);
}

/*
Returns combat bonus multiplier for specific unit types.
Conventional combat only.

ABL_DISSOCIATIVE_WAVE cancels following abilities:
ABL_EMPATH
ABL_TRANCE
ABL_COMM_JAMMER
ABL_AAA
ABL_SOPORIFIC_GAS
*/
double getConventionalCombatBonusMultiplier(int attackerUnitType, int defenderUnitType)
{
	// do not modify psi combat
	
	if (isUnitTypeNative(attackerUnitType) || isUnitTypeNative(defenderUnitType))
		return 1.0;
	
	// conventional combat
	
	int attackerChassisType = getUnitTypeChassisType(attackerUnitType);
	CChassis *attackerChassis = &(Chassis[attackerChassisType]);
	int defenderChassisType = getUnitTypeChassisType(defenderUnitType);
	CChassis *defenderChassis = &(Chassis[defenderChassisType]);
	
	double combatBonusMultiplier = 1.0;
	
	// fast unit without blink displacer against comm jammer
	
	if
	(
		isUnitTypeHasAbility(defenderUnitType, ABL_COMM_JAMMER)
		&&
		attackerChassis->triad == TRIAD_LAND && attackerChassis->speed > 1
		&&
		!isUnitTypeHasAbility(attackerUnitType, ABL_DISSOCIATIVE_WAVE)
	)
	{
		combatBonusMultiplier /= getPercentageBonusMultiplier(Rules->combat_comm_jammer_vs_mobile);
	}
	
	// air unit without blink displacer against air tracking
	
	if
	(
		isUnitTypeHasAbility(defenderUnitType, ABL_AAA)
		&&
		attackerChassis->triad == TRIAD_AIR
		&&
		!isUnitTypeHasAbility(attackerUnitType, ABL_DISSOCIATIVE_WAVE)
	)
	{
		combatBonusMultiplier /= getPercentageBonusMultiplier(Rules->combat_AAA_bonus_vs_air);
	}
	
	// soporific unit against conventional unit without blink displacer
	
	if
	(
		isUnitTypeHasAbility(attackerUnitType, ABL_SOPORIFIC_GAS)
		&&
		!isUnitTypeNative(defenderUnitType)
		&&
		!isUnitTypeHasAbility(defenderUnitType, ABL_DISSOCIATIVE_WAVE)
	)
	{
		combatBonusMultiplier *= 1.25;
	}
	
	// interceptor ground strike
	
	if
	(
		attackerChassis->triad == TRIAD_AIR && isUnitTypeHasAbility(attackerUnitType, ABL_AIR_SUPERIORITY)
		&&
		defenderChassis->triad != TRIAD_AIR
	)
	{
		combatBonusMultiplier *= getPercentageBonusMultiplier(-Rules->combat_penalty_air_supr_vs_ground);
	}
	
	// interceptor air-to-air
		
	if
	(
		attackerChassis->triad == TRIAD_AIR && isUnitTypeHasAbility(attackerUnitType, ABL_AIR_SUPERIORITY)
		&&
		defenderChassis->triad == TRIAD_AIR
	)
	{
		combatBonusMultiplier *= getPercentageBonusMultiplier(Rules->combat_bonus_air_supr_vs_air);
	}
	
	// gas
	
	if
	(
		isUnitTypeHasAbility(attackerUnitType, ABL_NERVE_GAS)
	)
	{
		combatBonusMultiplier *= 1.5;
	}
	
	return combatBonusMultiplier;
	
}

std::string getUnitTypeAbilitiesString(int unitType)
{
	std::string unitTypeAbilitiesString;
	
	if ((unitType & ABL_AMPHIBIOUS) != 0)
	{
		unitTypeAbilitiesString += " AMPHIBIOUS";
	}
	
	if ((unitType & ABL_AIR_SUPERIORITY) != 0)
	{
		unitTypeAbilitiesString += " AIR_SUPERIORITY";
	}
	
	if ((unitType & ABL_AAA) != 0)
	{
		unitTypeAbilitiesString += " AAA";
	}
	
	if ((unitType & ABL_COMM_JAMMER) != 0)
	{
		unitTypeAbilitiesString += " ECM";
	}
	
	if ((unitType & ABL_EMPATH) != 0)
	{
		unitTypeAbilitiesString += " EMPATH";
	}
	
	if ((unitType & ABL_ARTILLERY) != 0)
	{
		unitTypeAbilitiesString += " ARTILLERY";
	}
	
	if ((unitType & ABL_BLINK_DISPLACER) != 0)
	{
		unitTypeAbilitiesString += " BLINK_DISPLACER";
	}
	
	if ((unitType & ABL_TRANCE) != 0)
	{
		unitTypeAbilitiesString += " TRANCE";
	}
	
	if ((unitType & ABL_NERVE_GAS) != 0)
	{
		unitTypeAbilitiesString += " NERVE_GAS";
	}
	
	if ((unitType & ABL_SOPORIFIC_GAS) != 0)
	{
		unitTypeAbilitiesString += " SOPORIFIC_GAS";
	}
	
	if ((unitType & ABL_DISSOCIATIVE_WAVE) != 0)
	{
		unitTypeAbilitiesString += " DISSOCIATIVE_WAVE";
	}
	
	return unitTypeAbilitiesString;
	
}

