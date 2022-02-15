#include "aiMove.h"
#include "terranx_wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMoveColony.h"
#include "aiMoveCombat.h"
#include "aiMoveFormer.h"
#include "aiProduction.h"
#include "aiTransport.h"

/**
Top level AI strategu class. Contains AI methods entry points and global AI variable precomputations.
Faction uses WTP AI algorithms if configured. Otherwise, it falls back to Thinker.
Move strategy prepares demands for production strategy.
Production is set not on vanilla hooks but at the end of enemy_units_check phase for all bases one time.

--- control flow ---

faction_upkeep -> modifiedFactionUpkeep [WTP] (substitute)
-> aiFactionUpkeep [WTP]
	-> considerHurryingProduction [WTP]
	-> faction_upkeep
		-> mod_faction_upkeep [Thinker]
			-> production_phase

enemy_units_check -> modified_enemy_units_check [WTP] (substitute)
-> moveStrategy [WTP]
	-> all kind of specific move strategies [WTP]
	-> aiProductionStrategy [WTP]
	-> setProduction [WTP]
	-> enemy_units_check

*/

/*
Movement phase entry point.
*/
void __cdecl modified_enemy_units_check(int factionId)
{
	// run WTP AI code for AI enabled factions
	
	if (isUseWtpAlgorithms(factionId))
	{
		clock_t c = clock();
		aiStrategy(factionId);
		debug("(time) [WTP] moveStrategy: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
		
	}
	
	// execute original code
	
	clock_t c = clock();
	enemy_units_check(factionId);
	debug("(time) [vanilla] enemy_units_check: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
	
}

void aiStrategy(int factionId)
{
	// should be same faction that had upkeep earlier this turn
	
	assert(factionId == aiFactionId);
	
	clock_t c;
	
 	// setup AI Data
	
	aiData.setup();
	
	// populate shared strategy lists
	
	analyzeGeography();
	
	setSharedOceanRegions();
	populateGlobalVariables();
	evaluateBaseExposures();
	
	// bases native defense
	
	evaluateBaseNativeDefenseDemands();
	
	// bases defense
	
	evaluateDefenseDemand();
	
	// design units
	
	designUnits();
	
	// generate move strategy
	
	c = clock();
	moveStrategy();
	debug("(time) [WTP] moveStrategy: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
	
	// compute production demands

	c = clock();
	aiProductionStrategy();
	debug("(time) [WTP] aiProductionStrategy: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);

	// update base production
	
	c = clock();
	setProduction();
	debug("(time) [WTP] setProduction: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
	
	// cleanup AI Data
	
	aiData.cleanup();
	
}

/*
Analyzes and sets geographical parameters.
*/
void analyzeGeography()
{
	debug("analyzeGeography - %s\n", MFactions[aiFactionId].noun_faction);
	
	std::map<int, int> *associations = &(aiData.geography.associations);
	associations->clear();
	
	std::map<int, std::set<int>> *connections = &(aiData.geography.connections);
	connections->clear();
	
	std::set<MAP *> polarTiles;
	
	// populate associations
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		int region = getExtendedRegion(tile);
		
		associations->insert({region, {region}});
		
		if (isPolarRegion(tile))
		{
			bool ocean = is_ocean(tile);
			
			polarTiles.insert(tile);
			
			for (MAP *adjacentTile : getBaseAdjacentTiles(tile, false))
			{
				bool adjacentOcean = is_ocean(adjacentTile);
				int adjacentRegion = getExtendedRegion(adjacentTile);
				
				// do not associate polar region with polar region
				
				if (isPolarRegion(adjacentTile))
					continue;
				
				if (adjacentOcean == ocean)
				{
					associations->at(region) = adjacentRegion;
					break;
				}
				
			}
			
		}
		
	}
	
	// populate connections
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		bool ocean = is_ocean(tile);
		int region = getExtendedRegion(tile);
		
		connections->insert({region, {}});
		
		for (MAP *adjacentTile : getBaseAdjacentTiles(tile, false))
		{
			bool adjacentOcean = is_ocean(adjacentTile);
			int adjacentRegion = getExtendedRegion(adjacentTile);
			int adjacentAssociation = associations->at(adjacentRegion);
			
			if (adjacentOcean != ocean)
			{
				connections->at(region).insert(adjacentAssociation);
			}
			
		}
		
	}
	
	// extend polar region connections
	
	for (MAP *tile : polarTiles)
	{
		int region = getExtendedRegion(tile);
		int association = associations->at(region);
		
		if (association != region)
		{
			connections->at(region).insert(connections->at(association).begin(), connections->at(association).end());
		}
		
	}
	
	// populate faction specific data
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		std::map<int, int> *factionAssociations = &(aiData.geography.faction[factionId].associations);
		factionAssociations->clear();
		
		std::map<MAP *, int> *factionCoastalBaseOceanAssociations = &(aiData.geography.faction[factionId].coastalBaseOceanAssociations);
		factionCoastalBaseOceanAssociations->clear();
		
		std::set<MAP *> *oceanBaseTiles = &(aiData.geography.faction[factionId].oceanBaseTiles);
		oceanBaseTiles->clear();
		
		std::map<int, std::set<int>> *factionConnections = &(aiData.geography.faction[factionId].connections);
		factionConnections->clear();
		
		std::map<MAP *, std::set<int>> coastalBaseOceanRegionSets;
		std::vector<std::set<int>> joinedOceanRegionSets;
		std::set<int> presenseOceanAssociations;
		
		// populate friendly base ocean region sets
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			
			// friendly base
			
			if (!(base->faction_id == factionId || has_pact(factionId, base->faction_id)))
				continue;
			
			// land base
			
			if (isOceanRegion(baseTile->region))
			{
				oceanBaseTiles->insert(getMapTile(base->x, base->y));
				continue;
			}
			
			// get base ocean regions
			
			std::set<int> baseOceanRegionSet = getBaseOceanRegions(baseId);
			
			// coastal base
			
			if (baseOceanRegionSet.size() == 0)
				continue;
			
			// store base ocean regions
			
			coastalBaseOceanRegionSets.insert({baseTile, baseOceanRegionSet});
			factionCoastalBaseOceanAssociations->insert({baseTile, associations->at(*(baseOceanRegionSet.begin()))});
			
			// joined oceans
			
			if (baseOceanRegionSet.size() >= 2)
			{
				joinedOceanRegionSets.push_back(baseOceanRegionSet);
			}
			
		}
		
		// no joined oceans
		
		if (joinedOceanRegionSets.size() == 0)
			continue;
		
		// populate faction associations and connections
		
		factionAssociations->insert(associations->begin(), associations->end());
		factionConnections->insert(connections->begin(), connections->end());
		
		// update faction associations
		
		for (std::set<int> &joinedOceanRegionSet : joinedOceanRegionSets)
		{
			// find affected associations
			
			std::set<int> affectedAssociations;
			int resultingAssociation = INT_MAX;
			std::set<int> joinedConnections;
			
			for (int joinedOceanRegion : joinedOceanRegionSet)
			{
				int joinedOceanAssociation = factionAssociations->at(joinedOceanRegion);
				
				affectedAssociations.insert(joinedOceanAssociation);
				resultingAssociation = std::min(resultingAssociation, joinedOceanAssociation);
				joinedConnections.insert(factionConnections->at(joinedOceanAssociation).begin(), factionConnections->at(joinedOceanAssociation).end());
				
			}
			
			// update affected associations and connections
			
			for (const auto &factionAssociationsEntry : *factionAssociations)
			{
				int region = factionAssociationsEntry.first;
				int association = factionAssociationsEntry.second;
				
				if (affectedAssociations.count(association) != 0)
				{
					factionAssociations->at(region) = resultingAssociation;
					factionConnections->at(region).insert(joinedConnections.begin(), joinedConnections.end());
				}
				
			}
			
		}
		
		// update coastalBaseOceanAssociations
		
		for (const auto &factionCoastalBaseOceanAssociationsEntry : *factionCoastalBaseOceanAssociations)
		{
			MAP *baseTile = factionCoastalBaseOceanAssociationsEntry.first;
			int association = factionCoastalBaseOceanAssociationsEntry.second;
			
			factionCoastalBaseOceanAssociations->at(baseTile) = factionAssociations->at(association);
			
		}
		
	}
	
	// populate faction reachable associations
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		std::set<int> *immediatelyReachableAssociations = &(aiData.geography.faction[factionId].immediatelyReachableAssociations);
		immediatelyReachableAssociations->clear();
		
		std::set<int> *potentiallyReachableAssociations = &(aiData.geography.faction[factionId].potentiallyReachableAssociations);
		potentiallyReachableAssociations->clear();
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// faction vehicle
			
			if (vehicle->faction_id != factionId)
				continue;
			
			// transport
			
			if (!isTransportVehicle(vehicleId))
				continue;
			
			// sea transport
			
			if (vehicle->triad() != TRIAD_SEA)
				continue;
			
			// get vehicle ocean association
			
			int oceanAssociation = getOceanAssociation(vehicleTile, factionId);
			
			if (oceanAssociation == -1)
				continue;
			
			// presence ocean is immediately and potentially reachable
			
			immediatelyReachableAssociations->insert(oceanAssociation);
			potentiallyReachableAssociations->insert(oceanAssociation);
			
			// presence ocean connections are immediatelly and potentially reachable
			
			std::set<int> *oceanConnections = getConnections(oceanAssociation, factionId);
			
			immediatelyReachableAssociations->insert(oceanConnections->begin(), oceanConnections->end());
			potentiallyReachableAssociations->insert(oceanConnections->begin(), oceanConnections->end());
			
		}
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			
			// faction base
			
			if (base->faction_id != factionId)
				continue;
			
			// get base ocean association
			
			int oceanAssociation = getOceanAssociation(baseTile, factionId);
			
			if (oceanAssociation == -1)
				continue;
			
			// presence ocean is potentially reachable
			
			potentiallyReachableAssociations->insert(oceanAssociation);
			
			// presence ocean connections are potentially reachable
			
			std::set<int> *oceanConnections = getConnections(oceanAssociation, factionId);
			
			potentiallyReachableAssociations->insert(oceanConnections->begin(), oceanConnections->end());
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("\tassociations\n");
		
		for (const auto &association : aiData.geography.associations)
		{
			debug("\t\t%4d -> %4d\n", association.first, association.second);
			
		}
		
		debug("\tconnections\n");
		
		for (auto const &connectionsEntry : aiData.geography.connections)
		{
			int region = connectionsEntry.first;
			const std::set<int> *regionConnections = &(connectionsEntry.second);
			
			debug("\t%4d ->", region);
			
			for (int connectionRegion : *regionConnections)
			{
				debug(" %4d", connectionRegion);
			
			}
			
			debug("\n");
			
		}
		
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			debug("\tfactionId=%d\n", factionId);
			
			debug("\t\tassociations\n");
			
			for (auto const &associationsEntry : aiData.geography.faction[factionId].associations)
			{
				int region = associationsEntry.first;
				int association = associationsEntry.second;
				
				debug("\t\t\t%4d -> %4d\n", region, association);
				
			}
			
			debug("\t\tconnections\n");
			
			for (auto const &connectionsEntry : aiData.geography.faction[factionId].connections)
			{
				int region = connectionsEntry.first;
				const std::set<int> *regionConnections = &(connectionsEntry.second);
				
				debug("\t\t\t%4d ->", region);
				
				for (int connectionRegion : *regionConnections)
				{
					debug(" %4d", connectionRegion);
				
				}
				
				debug("\n");
				
			}
			
			debug("\t\tcoastalBaseOceanAssociations\n");
			
			for (auto const &coastalBaseOceanAssociationsEntry : aiData.geography.faction[factionId].coastalBaseOceanAssociations)
			{
				MAP *baseTile = coastalBaseOceanAssociationsEntry.first;
				int coastalBaseOceanAssociation = coastalBaseOceanAssociationsEntry.second;
				
				int baseX = getX(baseTile);
				int baseY = getY(baseTile);
				
				debug("\t\t\t(%3d,%3d) -> %d", baseX, baseY, coastalBaseOceanAssociation);
				
			}
			
			debug("\n");
			
			debug("\t\timmediatelyReachableAssociations\n");
			
			for (int reachableLandAssociation : aiData.geography.faction[factionId].immediatelyReachableAssociations)
			{
				debug("\t\t\t%4d\n", reachableLandAssociation);
				
			}
			
			debug("\t\tpotentiallyReachableAssociations\n");
			
			for (int reachableLandAssociation : aiData.geography.faction[factionId].potentiallyReachableAssociations)
			{
				debug("\t\t\t%4d\n", reachableLandAssociation);
				
			}
			
		}
		
	}
		
}

/*
Finds all regions those are accessible by other factions.
Set bases accessible to shared ocean regions.
*/
void setSharedOceanRegions()
{
	// find shared ocean regions
	
	std::set<int> sharedOceanRegions;
	
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
	
	for (int baseId : aiData.baseIds)
	{
		// clear flag by default
		
		aiData.baseStrategies[baseId].inSharedOceanRegion = false;
		
		// get connected ocean regions
		
		std::set<int> baseConnectedOceanRegions = getBaseConnectedOceanRegions(baseId);
		
		for (int baseConnectedOceanRegion : baseConnectedOceanRegions)
		{
			if (sharedOceanRegions.count(baseConnectedOceanRegion))
			{
				aiData.baseStrategies[baseId].inSharedOceanRegion = true;
				break;
			}
			
		}
		
	}
	
}

void populateGlobalVariables()
{
	// maxBaseSize
	
	aiData.maxBaseSize = getMaxBaseSize(aiFactionId);
	
	// best weapon and armor
	
	aiData.bestWeaponOffenseValue = getFactionBestPrototypedWeaponOffenseValue(aiFactionId);
	aiData.bestArmorDefenseValue = getFactionBestPrototypedArmorDefenseValue(aiFactionId);
	
	// populate factions combat modifiers

	debug("%-24s\nfactionCombatModifiers:\n", MFactions[aiFactionId].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// store combat modifiers

		aiData.factionInfos[id].offenseMultiplier = getFactionOffenseMultiplier(id);
		aiData.factionInfos[id].defenseMultiplier = getFactionDefenseMultiplier(id);
		aiData.factionInfos[id].fanaticBonusMultiplier = getFactionFanaticBonusMultiplier(id);

		debug
		(
			"\t%-24s: offenseMultiplier=%4.2f, defenseMultiplier=%4.2f, fanaticBonusMultiplier=%4.2f\n",
			MFactions[id].noun_faction,
			aiData.factionInfos[id].offenseMultiplier,
			aiData.factionInfos[id].defenseMultiplier,
			aiData.factionInfos[id].fanaticBonusMultiplier
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

		if (otherFactionRelation & DIPLO_VENDETTA)
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
		
		// human threat is increased
		
		if (is_human(id))
		{
			threatCoefficient += conf.ai_production_threat_coefficient_human;
		}
		
		// store threat koefficient

		aiData.factionInfos[id].threatCoefficient = threatCoefficient;

		debug("\t%-24s: %08x => %4.2f\n", MFactions[id].noun_faction, otherFactionRelation, threatCoefficient);

	}

	// populate factions best combat item values
	
	for (int factionId = 1; factionId < 8; factionId++)
	{
		aiData.factionInfos[factionId].bestWeaponOffenseValue = getFactionBestPrototypedWeaponOffenseValue(factionId);
		aiData.factionInfos[factionId].bestArmorDefenseValue = getFactionBestPrototypedArmorDefenseValue(factionId);
	}

	debug("\n");

	// populate bases

	debug("aiData.baseStrategies\n");

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		// exclude not own bases

		if (base->faction_id != aiFactionId)
			continue;

		// add base
		
		aiData.baseIds.push_back(baseId);
		
		// add base

		MAP *baseTile = getMapTile(base->x, base->y);
		aiData.baseTiles[baseTile] = baseId;

		// add base strategy

		aiData.baseStrategies[baseId] = {};
		BaseStrategy *baseStrategy = &(aiData.baseStrategies[baseId]);
		
		baseStrategy->base = base;
		baseStrategy->intrinsicDefenseMultiplier = getBaseDefenseMultiplier(baseId, -1);
		baseStrategy->conventionalDefenseMultipliers[TRIAD_LAND] = getBaseDefenseMultiplier(baseId, TRIAD_LAND);
		baseStrategy->conventionalDefenseMultipliers[TRIAD_SEA] = getBaseDefenseMultiplier(baseId, TRIAD_SEA);
		baseStrategy->conventionalDefenseMultipliers[TRIAD_AIR] = getBaseDefenseMultiplier(baseId, TRIAD_AIR);
		baseStrategy->sensorOffenseMultiplier = getSensorOffenseMultiplier(base->faction_id, base->x, base->y);
		baseStrategy->sensorDefenseMultiplier = getSensorDefenseMultiplier(base->faction_id, base->x, base->y);

		debug("\n[%3d] %-25s\n", baseId, aiData.baseStrategies[baseId].base->name);

		// add base regions
		
		int baseRegion = getBaseMapTile(baseId)->region;
		
		aiData.presenceRegions.insert(getBaseMapTile(baseId)->region);
		aiData.regionBaseIds[baseRegion].insert(baseId);

		std::set<int> baseConnectedRegions = getBaseConnectedRegions(baseId);

		for (int region : baseConnectedRegions)
		{
			if (aiData.regionBaseGroups.find(region) == aiData.regionBaseGroups.end())
			{
				aiData.regionBaseGroups[region] = std::vector<int>();
			}

			aiData.regionBaseGroups[region].push_back(baseId);

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
		
		aiData.vehicleIds.push_back(id);

		// combat vehicles

		if (isCombatVehicle(id))
		{
			// add vehicle to global list

			aiData.combatVehicleIds.push_back(id);
			
			// scout and native
			
			if (isScoutVehicle(id) || isNativeVehicle(id))
			{
				aiData.scoutVehicleIds.push_back(id);
			}

			// add surface vehicle to region list
			// except land unit in ocean
			
			if (vehicle->triad() != TRIAD_AIR && !(vehicle->triad() == TRIAD_LAND && isOceanRegion(vehicleTile->region)))
			{
				if (aiData.regionSurfaceCombatVehicleIds.count(vehicleTile->region) == 0)
				{
					aiData.regionSurfaceCombatVehicleIds[vehicleTile->region] = std::vector<int>();
				}
				aiData.regionSurfaceCombatVehicleIds[vehicleTile->region].push_back(id);
				
				// add scout to region list
				
				if (isScoutVehicle(id))
				{
					if (aiData.regionSurfaceScoutVehicleIds.count(vehicleTile->region) == 0)
					{
						aiData.regionSurfaceScoutVehicleIds[vehicleTile->region] = std::vector<int>();
					}
					aiData.regionSurfaceScoutVehicleIds[vehicleTile->region].push_back(id);
				}
				
			}

			// find if vehicle is at base

			std::map<MAP *, int>::iterator baseTilesIterator = aiData.baseTiles.find(vehicleTile);

			if (baseTilesIterator == aiData.baseTiles.end())
			{
				// add outside vehicle

				aiData.outsideCombatVehicleIds.push_back(id);

			}
			else
			{
				BaseStrategy *baseStrategy = &(aiData.baseStrategies[baseTilesIterator->second]);

				// add combat vehicle to garrison
				
				if (isCombatVehicle(id))
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
		else if (isColonyVehicle(id))
		{
			aiData.colonyVehicleIds.push_back(id);
		}
		else if (isFormerVehicle(id))
		{
			aiData.formerVehicleIds.push_back(id);
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

		aiData.unitIds.push_back(id);
		
		// add combat unit
		// either psi or anti psi unit or conventional with best weapon/armor
		
		if
		(
			isCombatUnit(id)
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
				Weapon[unit->weapon_type].offense_value >= aiData.bestWeaponOffenseValue
				||
				Armor[unit->armor_type].defense_value >= aiData.bestArmorDefenseValue
			)
		)
		{
			aiData.combatUnitIds.push_back(id);
			
			// populate specific triads
			
			switch (unit->triad())
			{
			case TRIAD_LAND:
				aiData.landCombatUnitIds.push_back(id);
				aiData.landAndAirCombatUnitIds.push_back(id);
				break;
			case TRIAD_SEA:
				aiData.seaCombatUnitIds.push_back(id);
				aiData.seaAndAirCombatUnitIds.push_back(id);
				break;
			case TRIAD_AIR:
				aiData.airCombatUnitIds.push_back(id);
				aiData.landAndAirCombatUnitIds.push_back(id);
				aiData.seaAndAirCombatUnitIds.push_back(id);
				break;
			}
			
		}
		
	}
	
	// max mineral surplus
	
	aiData.maxMineralSurplus = 1;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		aiData.maxMineralSurplus = std::max(aiData.maxMineralSurplus, base->mineral_surplus);
		
		if (aiData.regionMaxMineralSurpluses.count(baseTile->region) == 0)
		{
			aiData.regionMaxMineralSurpluses[baseTile->region] = 1;
		}
		
		aiData.regionMaxMineralSurpluses[baseTile->region] = std::max(aiData.regionMaxMineralSurpluses[baseTile->region], base->mineral_surplus);
		
	}
	
	// populate factions airbases
	
	for (int factionId = 1; factionId < 8; factionId++)
	{
		aiData.factionInfos[factionId].airbases.clear();
		
		// stationary airbases
		
		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			MAP *tile = getMapTile(mapIndex);
			
			if
			(
				map_has_item(tile, TERRA_BASE_IN_TILE | TERRA_AIRBASE)
				&&
				(tile->owner == factionId || has_pact(factionId, tile->owner))
			)
			{
				aiData.factionInfos[factionId].airbases.push_back(tile);
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
				aiData.factionInfos[factionId].airbases.push_back(getMapTile(vehicle->x, vehicle->y));
			}
			
		}
		
	}
	
	// base exposure
	
	populateBaseExposures();
	
	// best vehicle armor
	
	aiData.bestVehicleWeaponOffenseValue = 0;
	aiData.bestVehicleArmorDefenseValue = 0;
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		aiData.bestVehicleWeaponOffenseValue = std::max(aiData.bestVehicleWeaponOffenseValue, vehicle->offense_value());
		aiData.bestVehicleArmorDefenseValue = std::max(aiData.bestVehicleArmorDefenseValue, vehicle->defense_value());
		
	}
	
}

void evaluateBaseExposures()
{
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool ocean = isOceanRegion(baseTile->region);
		
		// find nearest not own tile
		
		int nearestNotOwnTileRange = INT_MAX;

		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			int x = getX(mapIndex);
			int y = getY(mapIndex);
			MAP* tile = getMapTile(mapIndex);
			
			// exclude own tiles
			
			if (tile->owner == aiFactionId)
				continue;
			
			// calculate range
			
			int range = map_range(base->x, base->y, x, y);
			
			nearestNotOwnTileRange = std::min(nearestNotOwnTileRange, range);
			
		}
		
		// set base distance based on realm
		
		double baseDistance = (ocean ? 7.0 : 3.0);
		
		// estimate exposure
		
		double exposure = 1.0 / std::max(1.0, (double)nearestNotOwnTileRange / baseDistance);
		
		aiData.baseStrategies[baseId].exposure = exposure;
		
	}
	
}

void evaluateBaseNativeDefenseDemands()
{
	debug("evaluateBaseNativeDefenseDemands\n");
	
	for (int baseId : aiData.baseIds)
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
		
		for (int otherBaseId : aiData.baseIds)
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
		
		aiData.baseAnticipatedNativeAttackStrengths[baseId] = anticipatedNativePsiAttackStrength;
		aiData.baseRemainingNativeProtectionDemands[baseId] = remainingBaseNativeProtection;
		
	}
	
}

void evaluateDefenseDemand()
{
	debug("\n\n==================================================\nevaluateDefenseDemand - %s\n", MFactions[aiFactionId].noun_faction);
	
	// evaluate base defense demands
	
	aiData.mostVulnerableBaseId = -1;
	aiData.mostVulnerableBaseDefenseDemand = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool ocean = isOceanRegion(baseTile->region);
		BaseStrategy *baseStrategy = &(aiData.baseStrategies[baseId]);
		
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
		
		for (int vehicleId : aiData.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			int unitId = vehicle->unit_id;
			UNIT *unit = &(Units[unitId]);
			int chassisId = unit->chassis_type;
			CChassis *chassis = &(Chassis[chassisId]);
			int triad = chassis->triad;
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// land base
			if (isLandRegion(baseTile->region))
			{
				// no sea vehicle for land base protection
				
				if (triad == TRIAD_SEA)
					continue;
				
				// no land vehicle in other region for land base protection
				
				if (triad == TRIAD_LAND && vehicleTile->region != baseTile->region)
					continue;
				
			}
			// ocean base
			else
			{
				// no land vehicle everywhere besides base for ocean base protection
				
				if (triad == TRIAD_LAND && !(vehicle->x == base->x && vehicle->y == base->y))
					continue;
				
				// no sea vehicle in other region for ocean base protection
				
				if (triad == TRIAD_SEA && !isVehicleInRegion(vehicleId, baseTile->region))
					continue;
				
			}
			
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
			
			// calculate time to reach the base
			
			double time = (double)distance / speed;
			
			// increase effective time for land unit in different region or in ocean
			
			if (triad == TRIAD_LAND && distance > 0 && (isOceanRegion(vehicleTile->region) || vehicleTile->region != baseTile->region))
			{
				time *= 5;
			}
			
			// reduce weight based on time to reach the base
			// every 10 turns reduce weight in half
			
			double weight = pow(0.5, time / 10.0);
			
			// update total weight
			
			defenderStrength->totalWeight += weight;
			
			// create and get entry
			
			if (defenderStrength->unitStrengths.count(unitId) == 0)
			{
				defenderStrength->unitStrengths[unitId] = UnitStrength();
			}
			
			UnitStrength *unitStrength = &(defenderStrength->unitStrengths[unitId]);
			
			// add weight to unit type
			
			unitStrength->weight += weight;
			
			// calculate strength
			
			double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId);
			double psiDefenseStrength = getVehiclePsiDefenseStrength(vehicleId, true);
			
			double conventionalOffenseStrength = getVehicleConventionalOffenseStrength(vehicleId);
			double conventionalDefenseStrength = getVehicleConventionalDefenseStrength(vehicleId, true);
			
			// psi strength for all vehicles
			
			unitStrength->psiOffense += weight * psiOffenseStrength;
			unitStrength->psiDefense += weight * psiDefenseStrength;
			
			// conventional strength for regular vehicles only
			
			if (!isNativeUnit(unitId))
			{
				unitStrength->conventionalOffense += weight * conventionalOffenseStrength;
				unitStrength->conventionalDefense += weight * conventionalDefenseStrength;
			}
			
			debug
			(
				"\t\t\t(%3d,%3d) %-32s distance=%3d, time=%6.2f, weight=%f, psiOffenseStrength=%f, psiDefenseStrength=%f, conventionalOffenseStrength=%f, conventionalDefenseStrength=%f\n",
				vehicle->x, vehicle->y, Units[vehicle->unit_id].name, distance, time, weight,
				psiOffenseStrength, psiDefenseStrength, conventionalOffenseStrength, conventionalDefenseStrength
			);
			
		}
		
		// normalize weights
		
		defenderStrength->normalize();
		
		debug("\t\t\ttotalWeight=%f\n", defenderStrength->totalWeight);
		if (DEBUG)
		{
			for (const auto &k : defenderStrength->unitStrengths)
			{
				const int unitId = k.first;
				const UnitStrength *unitStrength = &(k.second);
				
				UNIT *unit = &(Units[unitId]);
				
				debug("\t\t\t\t[%3d] %-32s weight=%f, psiOffense=%f, psiDefense=%f, conventionalOffense=%f, conventionalDefense=%f\n", unitId, unit->name, unitStrength->weight, unitStrength->psiOffense, unitStrength->psiDefense, unitStrength->conventionalOffense, unitStrength->conventionalDefense);
				
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
			
			// combat
			
			if (!isCombatVehicle(vehicleId))
				continue;
			
			// exclude vehicles unable to attack base
			
			if (!ocean && triad == TRIAD_SEA || (ocean && triad == TRIAD_LAND && !vehicle_has_ability(vehicleId, ABL_AMPHIBIOUS)))
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
			
			// calculate time to reach the base
			
			double time = (double)distance / speed;
			
			// increase effective turns for land unit in different region or in ocean
			
			if (triad == TRIAD_LAND && distance > 0 && (isOceanRegion(vehicleTile->region) || vehicleTile->region != baseTile->region))
			{
				time *= 5;
			}
			
			// reduce weight based on time to reach the base
			// every 10 turns reduce weight in half
			
			double weight = pow(0.5, time / 10.0);
			
			// modify strength multiplier based on diplomatic relations
			
			weight *= aiData.factionInfos[vehicle->faction_id].threatCoefficient;
			
			// exclude empty weight
			
			if (weight == 0.0)
				continue;
			
			// update total weight
			
			opponentStrength->totalWeight += weight;
			
			// pack unit type, create and get entry
			
			if (opponentStrength->unitStrengths.count(unitId) == 0)
			{
				opponentStrength->unitStrengths[unitId] = UnitStrength();
			}
			
			UnitStrength *unitStrength = &(opponentStrength->unitStrengths[unitId]);
			
			// add weight to unit type
			
			unitStrength->weight += weight;
			
			// calculate strength
			
			double basePsiDefenseMultiplier = baseStrategy->intrinsicDefenseMultiplier;
			
			double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId) / baseStrategy->sensorDefenseMultiplier / basePsiDefenseMultiplier;
			double psiDefenseStrength = getVehiclePsiDefenseStrength(vehicleId, false) / baseStrategy->sensorOffenseMultiplier;
			
			double baseConventionalDefenseMultiplier = (isVehicleHasAbility(vehicleId, ABL_BLINK_DISPLACER) ? baseStrategy->intrinsicDefenseMultiplier : baseStrategy->conventionalDefenseMultipliers[triad]);
			
			double conventionalOffenseStrength = getVehicleConventionalOffenseStrength(vehicleId) / baseStrategy->sensorDefenseMultiplier / baseConventionalDefenseMultiplier;
			double conventionalDefenseStrength = getVehicleConventionalDefenseStrength(vehicleId, false) / baseStrategy->sensorOffenseMultiplier;
			
			// psi strength for all vehicles
			
			unitStrength->psiOffense += weight * psiOffenseStrength;
			unitStrength->psiDefense += weight * psiDefenseStrength;
			
			// conventional strength for regular vehicles only
			
			if (!isNativeUnit(unitId))
			{
				unitStrength->conventionalOffense += weight * conventionalOffenseStrength;
				unitStrength->conventionalDefense += weight * conventionalDefenseStrength;
			}
			
			debug
			(
				"\t\t\t(%3d,%3d) %-32s distance=%3d, time=%6.2f, weight=%f, psiOffenseStrength=%f, psiDefenseStrength=%f, conventionalOffenseStrength=%f, conventionalDefenseStrength=%f\n",
				vehicle->x, vehicle->y, Units[vehicle->unit_id].name, distance, time, weight,
				psiOffenseStrength, psiDefenseStrength, conventionalOffenseStrength, conventionalDefenseStrength
			);
			
		}
		
		// normalize weights
		
		opponentStrength->normalize();
		
		debug("\t\t\ttotalWeight=%f\n", opponentStrength->totalWeight);
		if (DEBUG)
		{
			for (const auto &k : opponentStrength->unitStrengths)
			{
				const int unitId = k.first;
				const UnitStrength *unitStrength = &(k.second);
				
				UNIT *unit = &(Units[unitId]);
				
				debug("\t\t\t\t[%3d] %-32s weight=%f, psiOffense=%f, psiDefense=%f, conventionalOffense=%f, conventionalDefense=%f\n", unitId, unit->name, unitStrength->weight, unitStrength->psiOffense, unitStrength->psiDefense, unitStrength->conventionalOffense, unitStrength->conventionalDefense);
				
			}
			
		}
		
		// evaluate relative portion of defender destroyed
		
		debug("\t\tdefenderDestroyed\n");
		
		double defenderDestroyed = 0.0;
		
		for (const auto &opponentUnitStrengthEntry : opponentStrength->unitStrengths)
		{
			const int opponentUnitId = opponentUnitStrengthEntry.first;
			const UnitStrength *opponentUnitStrength = &(opponentUnitStrengthEntry.second);
			
			UNIT *opponentUnit = &(Units[opponentUnitId]);
			
			for (const auto defenderStrengthEntry : defenderStrength->unitStrengths)
			{
				const int defenderUnitId = defenderStrengthEntry.first;
				const UnitStrength *defenderUnitStrength = &(defenderStrengthEntry.second);
				
				UNIT *defenderUnit = &(Units[defenderUnitId]);
				
				// impossible combinations
				
				// two needlejets without air superiority
				if
				(
					(opponentUnit->chassis_type == CHS_NEEDLEJET && !unit_has_ability(defenderUnitId, ABL_AIR_SUPERIORITY))
					&&
					(defenderUnit->chassis_type == CHS_NEEDLEJET && !unit_has_ability(opponentUnitId, ABL_AIR_SUPERIORITY))
				)
				{
					continue;
				}
				
				// calculate odds
				
				double attackOdds;
				double defendOdds;
				
				if (isNativeUnit(opponentUnitId) || isNativeUnit(defenderUnitId))
				{
					attackOdds =
						opponentUnitStrength->psiOffense
						/
						defenderUnitStrength->psiDefense
					;
					
					defendOdds =
						opponentUnitStrength->psiDefense
						/
						defenderUnitStrength->psiOffense
					;
					
				}
				else
				{
					attackOdds =
						opponentUnitStrength->conventionalOffense
						/
						defenderUnitStrength->conventionalDefense
						// type to type combat modifier (opponent attacks)
						*
						getConventionalCombatBonusMultiplier(opponentUnitId, defenderUnitId)
					;
					
					defendOdds =
						opponentUnitStrength->conventionalDefense
						/
						defenderUnitStrength->conventionalOffense
						// type to type combat modifier (defender attacks)
						/
						getConventionalCombatBonusMultiplier(defenderUnitId, opponentUnitId)
					;
					
				}
				
				// calculate attack and defend probabilities
				
				double attackProbability;
				double defendProbability;
				
				// defender cannot attack
				if
				(
					// unit without air superiority cannot attack neeglejet
					(opponentUnit->chassis_type == CHS_NEEDLEJET && !unit_has_ability(defenderUnitId, ABL_AIR_SUPERIORITY))
					||
					// land unit cannot attack from sea base
					(ocean && defenderUnit->triad() == TRIAD_LAND)
				)
				{
					attackProbability = 1.0;
					defendProbability = 0.0;
				}
				// opponent cannot attack
				else if
				(
					// unit without air superiority cannot attack neeglejet
					(defenderUnit->chassis_type == CHS_NEEDLEJET && !unit_has_ability(opponentUnitId, ABL_AIR_SUPERIORITY))
				)
				{
					attackProbability = 0.0;
					defendProbability = 1.0;
				}
				// opponent disengages
				else if
				(
					(
						(opponentUnit->triad() == TRIAD_LAND && defenderUnit->triad() == TRIAD_LAND)
						||
						(opponentUnit->triad() == TRIAD_SEA && defenderUnit->triad() == TRIAD_SEA)
					)
					&&
					(unit_chassis_speed(opponentUnitId) > unit_chassis_speed(defenderUnitId))
				)
				{
					attackProbability = 1.0;
					defendProbability = 0.0;
				}
				// defender disengages
				else if
				(
					(
						(opponentUnit->triad() == TRIAD_LAND && defenderUnit->triad() == TRIAD_LAND)
						||
						(opponentUnit->triad() == TRIAD_SEA && defenderUnit->triad() == TRIAD_SEA)
					)
					&&
					(unit_chassis_speed(defenderUnitId) > unit_chassis_speed(opponentUnitId))
				)
				{
					attackProbability = 0.0;
					defendProbability = 1.0;
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
					(opponentUnitStrength->weight * defenderUnitStrength->weight)
					*
					(
						attackProbability * attackOdds
						+
						defendProbability * defendOdds
					)
				;
				
				debug
				(
					"\t\t\topponent: weight=%f %-32s\n\t\t\tdefender: weight=%f %-32s\n\t\t\t\tattack: probability=%f, odds=%f\n\t\t\t\tdefend: probatility=%f, odds=%f\n\t\t\t\tdefenderDestroyedTypeVsType=%f\n",
					opponentUnitStrength->weight,
					opponentUnit->name,
					defenderUnitStrength->weight,
					defenderUnit->name,
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
			// proportion of total weight
			*
			(opponentStrength->totalWeight / defenderStrength->totalWeight)
			// defense superiority coefficient
			*
			conf.ai_production_defense_superiority_coefficient
		;
		
		debug("\t\tdefenderDestroyed=%f, strengthProportion=%f, superiorityCoefficient=%f, defenderDestroyedTotal=%f\n", defenderDestroyed, (opponentStrength->totalWeight / defenderStrength->totalWeight), conf.ai_production_defense_superiority_coefficient, defenderDestroyedTotal);
		
		// set defenseDemand
		
		if (defenderDestroyedTotal > 1.0)
		{
			aiData.baseStrategies[baseId].defenseDemand = (defenderDestroyedTotal - 1.0) / defenderDestroyedTotal;
		}
		
		debug("\t\tdefenseDemand=%f\n", aiData.baseStrategies[baseId].defenseDemand);
		
		// update global values
		
		if (aiData.baseStrategies[baseId].defenseDemand > aiData.mostVulnerableBaseDefenseDemand)
		{
			aiData.mostVulnerableBaseId = baseId;
			aiData.mostVulnerableBaseDefenseDemand = aiData.baseStrategies[baseId].defenseDemand;
		}
		
	}
	
	debug("\n\tmostVulnerableBase = %s, mostVulnerableBaseDefenseDemand = %f\n", Bases[aiData.mostVulnerableBaseId].name, aiData.mostVulnerableBaseDefenseDemand);
	
	// assign defense demand targets
	
	debug("\n\t->\n\ttargetBases\n");
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		int bestTargetBaseId = -1;
		double bestTargetBasePreference = 0.0;
		
		for (int targetBaseId : aiData.baseIds)
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
			
			double targetBasePreference = rangeCoefficient * aiData.baseStrategies[targetBaseId].defenseDemand;
			
			// update best
			
			if (targetBasePreference > bestTargetBasePreference)
			{
				bestTargetBaseId = targetBaseId;
				bestTargetBasePreference = targetBasePreference;
			}
			
		}
		
		aiData.baseStrategies[baseId].targetBaseId = bestTargetBaseId;
		
		debug("\t\t%-25s -> %-25s\n", Bases[baseId].name, (bestTargetBaseId == -1 ? "" : Bases[bestTargetBaseId].name));
		
	}
	
	debug("\n");
	
}

void designUnits()
{
	// get best values
	
	int bestWeapon = getFactionBestWeapon(aiFactionId);
	int bestArmor = getFactionBestArmor(aiFactionId);
	int fastLandChassis = (has_chassis(aiFactionId, CHS_HOVERTANK) ? CHS_HOVERTANK : CHS_SPEEDER);
	int fastSeaChassis = (has_chassis(aiFactionId, CHS_CRUISER) ? CHS_CRUISER : CHS_FOIL);
	int bestReactor = best_reactor(aiFactionId);
	
	// land defenders
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{WPN_HAND_WEAPONS},
		{bestArmor},
		{0, ABL_COMM_JAMMER, ABL_AAA, ABL_POLY_ENCRYPTION, ABL_POLICE_2X},
		bestReactor,
		PLAN_DEFENSIVE,
		NULL
	);
	
	// land attackers
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{bestWeapon},
		{ARM_NO_ARMOR},
		{0, ABL_AMPHIBIOUS, ABL_BLINK_DISPLACER, ABL_SOPORIFIC_GAS, ABL_ANTIGRAV_STRUTS},
		bestReactor,
		PLAN_OFFENSIVE,
		NULL
	);
	
	// land paratroopers
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{bestWeapon},
		{bestArmor},
		{ABL_DROP_POD},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
	// land armored attackers
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{bestWeapon},
		{bestArmor},
		{0, ABL_AMPHIBIOUS, ABL_BLINK_DISPLACER, ABL_SOPORIFIC_GAS, ABL_COMM_JAMMER, ABL_AAA, ABL_POLY_ENCRYPTION},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
	// land fast attackers
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastLandChassis},
		{bestWeapon},
		{ARM_NO_ARMOR},
		{0, ABL_AMPHIBIOUS, ABL_BLINK_DISPLACER, ABL_SOPORIFIC_GAS, ABL_AIR_SUPERIORITY, ABL_ANTIGRAV_STRUTS, ABL_DISSOCIATIVE_WAVE},
		bestReactor,
		PLAN_OFFENSIVE,
		NULL
	);
	
	// ships
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastSeaChassis},
		{bestWeapon},
		{bestArmor},
		{0, ABL_AAA, ABL_BLINK_DISPLACER, ABL_SOPORIFIC_GAS, ABL_MARINE_DETACHMENT},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
}

/*
Propose multiple prototype combinations.
*/
void proposeMultiplePrototypes(int factionId, std::vector<int> chassisIds, std::vector<int> weaponIds, std::vector<int> armorIds, std::vector<int> abilitiesSets, int reactor, int plan, char *name)
{
	for (int chassisId : chassisIds)
	{
		for (int weaponId : weaponIds)
		{
			for (int armorId : armorIds)
			{
				for (int abilitiesSet : abilitiesSets)
				{
					checkAndProposePrototype(factionId, chassisId, weaponId, armorId, abilitiesSet, reactor, plan, name);
					
				}
				
			}
			
		}
		
	}
	
}

/*
Verify proposed prototype is allowed and propose it if yes.
Verifies all technologies are available and abilities area allowed.
*/
void checkAndProposePrototype(int factionId, int chassisId, int weaponId, int armorId, int abilities, int reactor, int plan, char *name)
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
	
	if (!has_reactor(factionId, reactor))
		return;
	
	// check abilities are available and allowed
	
	for (int abilityId = ABL_ID_SUPER_TERRAFORMER; abilityId <= ABL_ID_ALGO_ENHANCEMENT; abilityId++)
	{
		int abilityFlag = (0x1 << abilityId);
		
		if ((abilities & abilityFlag) == 0)
			continue;
		
		if (!has_ability(factionId, abilityId))
			return;
		
		CAbility *ability = &(Ability[abilityId]);
		
		// not allowed for triad
		
		switch (Chassis[chassisId].triad)
		{
		case TRIAD_LAND:
			if ((ability->flags & AFLAG_ALLOWED_LAND_UNIT) == 0)
				return;
			break;
			
		case TRIAD_SEA:
			if ((ability->flags & AFLAG_ALLOWED_SEA_UNIT) == 0)
				return;
			break;
			
		case TRIAD_AIR:
			if ((ability->flags & AFLAG_ALLOWED_AIR_UNIT) == 0)
				return;
			break;
			
		}
		
		// not allowed for combat unit
		
		if (Weapon[weaponId].offense_value != 0 && (ability->flags & AFLAG_ALLOWED_COMBAT_UNIT) == 0)
			return;
		
		// not allowed for terraform unit
		
		if (weaponId == WPN_TERRAFORMING_UNIT && (ability->flags & AFLAG_ALLOWED_TERRAFORM_UNIT) == 0)
			return;
		
		// not allowed for non-combat unit
		
		if (Weapon[weaponId].offense_value == 0 && (ability->flags & AFLAG_ALLOWED_NONCOMBAT_UNIT) == 0)
			return;
		
		// not allowed for probe team
		
		if (weaponId == WPN_PROBE_TEAM && (ability->flags & AFLAG_NOT_ALLOWED_PROBE_TEAM) != 0)
			return;
		
		// not allowed for non transport unit
		
		if (weaponId != WPN_TROOP_TRANSPORT && (ability->flags & AFLAG_TRANSPORT_ONLY_UNIT) != 0)
			return;
		
		// not allowed for fast unit
		
		if (chassisId == CHS_INFANTRY && Chassis[chassisId].speed > 1 && (ability->flags & AFLAG_NOT_ALLOWED_FAST_UNIT) != 0)
			return;
		
		// not allowed for non probes
		
		if (weaponId != WPN_PROBE_TEAM && (ability->flags & AFLAG_ONLY_PROBE_TEAM) != 0)
			return;
		
	}
	
	// propose prototype
	
	int unitId = modified_propose_proto(factionId, chassisId, weaponId, armorId, abilities, reactor, plan, name);
	
	debug("checkAndProposePrototype - %s\n", MFactions[aiFactionId].noun_faction);
	debug("\treactor=%d, chassisId=%d, weaponId=%d, armorId=%d, abilities=%s\n", reactor, chassisId, weaponId, armorId, getAbilitiesString(abilities).c_str());
	debug("\tunitId=%d\n", unitId);
	
}

void moveStrategy()
{
	debug("moveStrategy - %s\n", MFactions[aiFactionId].noun_faction);
	
	clock_t s1;
	
	// update variables
	
	s1 = clock();
	updateGlobalVariables();
	debug("(time) [WTP] -updateGlobalVariables: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
	
	// generic
	
	s1 = clock();
	moveAllStrategy();
	debug("(time) [WTP] -moveAllStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
	
	// combat

	s1 = clock();
	moveCombatStrategy();
	debug("(time) [WTP] -moveCombatStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);

	// colony
	
	s1 = clock();
	moveColonyStrategy();
	debug("(time) [WTP] -moveColonyStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);
	
	// former

	s1 = clock();
	moveFormerStrategy();
	debug("(time) [WTP] -moveFormerStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);

	// transport

	s1 = clock();
	moveTranportStrategy();
	debug("(time) [WTP] -moveTranportStrategy: %6.3f\n", (double)(clock() - s1) / (double)CLOCKS_PER_SEC);

}

void updateGlobalVariables()
{
	// populate vehicles
	
	aiData.vehicleIds.clear();
	aiData.combatVehicleIds.clear();
	aiData.scoutVehicleIds.clear();
	aiData.regionSurfaceCombatVehicleIds.clear();
	aiData.outsideCombatVehicleIds.clear();
	aiData.colonyVehicleIds.clear();
	aiData.formerVehicleIds.clear();
	
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
		
		aiData.vehicleIds.push_back(id);

		// combat vehicles

		if (isCombatVehicle(id))
		{
			// add vehicle to global list

			aiData.combatVehicleIds.push_back(id);
			
			// scout and native
			
			if (isScoutVehicle(id) || isNativeVehicle(id))
			{
				aiData.scoutVehicleIds.push_back(id);
			}

			// add surface vehicle to region list
			// except land unit in ocean
			
			if (vehicle->triad() != TRIAD_AIR && !(vehicle->triad() == TRIAD_LAND && isOceanRegion(vehicleTile->region)))
			{
				if (aiData.regionSurfaceCombatVehicleIds.count(vehicleTile->region) == 0)
				{
					aiData.regionSurfaceCombatVehicleIds[vehicleTile->region] = std::vector<int>();
				}
				aiData.regionSurfaceCombatVehicleIds[vehicleTile->region].push_back(id);
				
				// add scout to region list
				
				if (isScoutVehicle(id))
				{
					if (aiData.regionSurfaceScoutVehicleIds.count(vehicleTile->region) == 0)
					{
						aiData.regionSurfaceScoutVehicleIds[vehicleTile->region] = std::vector<int>();
					}
					aiData.regionSurfaceScoutVehicleIds[vehicleTile->region].push_back(id);
				}
				
			}

			// find if vehicle is at base

			std::map<MAP *, int>::iterator baseTilesIterator = aiData.baseTiles.find(vehicleTile);

			if (baseTilesIterator == aiData.baseTiles.end())
			{
				// add outside vehicle

				aiData.outsideCombatVehicleIds.push_back(id);

			}
			else
			{
				// temporarily disable as I don't know what to do with it
//				BaseStrategy *baseStrategy = &(aiData.baseStrategies[baseTilesIterator->second]);
//
//				// add combat vehicle to garrison
//				
//				if (isCombatVehicle(id))
//				{
//					baseStrategy->garrison.push_back(id);
//				}
//
//				// add to native protection
//
//				double nativeProtection = calculateNativeDamageDefense(id) / 10.0;
//
//				if (vehicle_has_ability(vehicle, ABL_TRANCE))
//				{
//					nativeProtection *= (1 + (double)Rules->combat_bonus_trance_vs_psi / 100.0);
//				}
//
//				baseStrategy->nativeProtection += nativeProtection;
//
			}

		}
		else if (isColonyVehicle(id))
		{
			aiData.colonyVehicleIds.push_back(id);
		}
		else if (isFormerVehicle(id))
		{
			aiData.formerVehicleIds.push_back(id);
		}

	}

}

void moveAllStrategy()
{
	// heal
	
	healStrategy();
	
}

void healStrategy()
{
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// exclude ogres
		
		if (vehicle->unit_id == BSC_BATTLE_OGRE_MK1 || vehicle->unit_id == BSC_BATTLE_OGRE_MK2 || vehicle->unit_id == BSC_BATTLE_OGRE_MK3)
			continue;
		
		// exclude artifact
		
		if (isArtifactVehicle(vehicleId))
			continue;
		
		// exclude colony
		
		if (isColonyVehicle(vehicleId))
			continue;
		
		// at base
		
		if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
		{
			// damaged
			
			if (vehicle->damage_taken == 0)
				continue;
			
			// not under alien artillery bombardment
			
			if (isWithinAlienArtilleryRange(vehicleId))
				continue;
			
			// heal
			
			setTask(vehicleId, Task(HOLD, vehicleId));
//			setMoveOrder(vehicleId, vehicle->x, vehicle->y, ORDER_SENTRY_BOARD);
			
		}
		
		// in the field
		
		else
		{
			// repairable
			
			if (vehicle->damage_taken <= (has_project(aiFactionId, FAC_NANO_FACTORY) ? 0 : 2 * vehicle->reactor_type()))
				continue;
			
			// find nearest monolith
			
			MAP *nearestMonolith = getNearestMonolith(vehicle->x, vehicle->y, vehicle->triad());
			
			if (nearestMonolith != nullptr && map_range(vehicle->x, vehicle->y, getX(nearestMonolith), getY(nearestMonolith)) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
				setTask(vehicleId, Task(MOVE, vehicleId, nearestMonolith));
				continue;
			}
			
			// find nearest base
			
			MAP *nearestBase = getNearestBase(vehicle->x, vehicle->y, vehicle->triad());
			
			if (nearestBase != nullptr && map_range(vehicle->x, vehicle->y, getX(nearestBase), getY(nearestBase)) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
				setTask(vehicleId, Task(HOLD, vehicleId, nearestBase));
				continue;
			}
			
			// heal in field
			
//			setMoveOrder(vehicleId, vehicle->x, vehicle->y, ORDER_SENTRY_BOARD);
			setTask(vehicleId, Task(HOLD, vehicleId));
			
		}
		
	}
	
}

/*
Top level AI enemy move entry point

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
int moveVehicle(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	debug("moveVehicle (%3d,%3d) %s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
	
	// do not try multiple iterations
	
	if (vehicle->iter_count > 0)
	{
		return mod_enemy_move(vehicleId);
	}
	
	// execute task
	
	if (hasTask(vehicleId))
	{
		return executeTask(vehicleId);
	}
	
	// combat
	
	if (isCombatVehicle(vehicleId))
	{
		return moveCombat(vehicleId);
	}
	
	// colony
	// all colony movement is scheduled in strategy phase
	
	// former
	
	if (isFormerVehicle(vehicleId))
	{
		// WTP vs. Thinker terraforming comparison
		
		return (aiFactionId <= conf.ai_terraforming_factions_enabled ? moveFormer(vehicleId) : mod_enemy_move(vehicleId));
		
	}
	
    // use vanilla algorithm for probes
    // Thinker ignores subversion opportunities

    if (isProbeVehicle(vehicleId))
	{
		return enemy_move(vehicleId);
	}

	// unhandled cases default to Thinker
	
	return mod_enemy_move(vehicleId);
	
}

/*
Creates tasks to transit vehicle to destination.
*/
void transitVehicle(int vehicleId, Task task)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	debug("transitVehicle (%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
	
	// get destination
	
	MAP *destination = task.getDestination();
	
	if (destination == nullptr)
	{
		debug("\tinvalid destination\n");
		return;
	}
	
	int x = getX(destination);
	int y = getY(destination);
	
	debug("\t->(%3d,%3d)\n", x, y);
	
	// vehicle is at destination already
	
	if (vehicle->x == x && vehicle->y == y)
	{
		debug("\tat destination\n");
		setTask(vehicleId, task);
		return;
	}
	
	// sea and air units do not need sophisticated transportation algorithms
	
	if (vehicle->triad() == TRIAD_SEA || vehicle->triad() == TRIAD_AIR)
	{
		debug("\tsea/air unit\n");
		setTask(vehicleId, task);
		return;
	}
	
	// vehicle is on land same association
	
	if (isLandRegion(vehicleTile->region) && isLandRegion(destination->region) && isSameAssociation(vehicleTile, destination, vehicle->faction_id))
	{
		debug("\tsame association\n");
		setTask(vehicleId, task);
		return;
	}
	
	// get transport id
	
	int seaTransportVehicleId = getLandVehicleSeaTransportVehicleId(vehicleId);
	
	// is being transported
	
	if (seaTransportVehicleId != -1)
	{
		debug("\tis being transported\n");
		
		// find unboard location
		
		MAP *unboardLocation = getSeaTransportUnboardLocation(seaTransportVehicleId, destination);
		
		if (unboardLocation == nullptr)
		{
			debug("\tcannot find unboard location\n");
			return;
		}
		
		int unboardLocationX = getX(unboardLocation);
		int unboardLocationY = getY(unboardLocation);
		
		debug("\tunboardLocation=(%3d,%3d)\n", unboardLocationX, unboardLocationY);
		
		// find unload location
		
		MAP *unloadLocation = getSeaTransportUnloadLocation(seaTransportVehicleId, destination, unboardLocation);
		
		if (unloadLocation == nullptr)
		{
			debug("\tcannot find unload location\n");
			return;
		}
		
		int unloadLocationX = getX(unloadLocation);
		int unloadLocationY = getY(unloadLocation);
		
		debug("\tunloadLocation=(%3d,%3d)\n", unloadLocationX, unloadLocationY);
		
		if (vehicle->x == unloadLocationX && vehicle->y == unloadLocationY)
		{
			debug("\tat unload location\n");
			
			// wake up vehicle
			
			setVehicleOrder(vehicleId, ORDER_NONE);
			
			// skip transport
			
			setTask(seaTransportVehicleId, Task(SKIP, seaTransportVehicleId));
			
			// move to unboard location
			
			setTask(vehicleId, Task(MOVE, vehicleId, unboardLocation));
			
			return;
			
		}
		
		// add orders
		
		setTask(vehicleId, Task(UNBOARD, vehicleId, unboardLocation));
		setTaskIfCloser(seaTransportVehicleId, Task(UNLOAD, seaTransportVehicleId, unloadLocation));
		
		return;
		
	}
	
	// get cross ocean association
	
	int crossOceanAssociation = getCrossOceanAssociation(getMapTile(vehicle->x, vehicle->y), destination, vehicle->faction_id);
	
	if (crossOceanAssociation == -1)
	{
		debug("\tcannot find cross ocean region\n");
		return;
	}
	
	debug("\tcrossOceanAssociation=%d\n", crossOceanAssociation);
	
	// search for available sea transport
	
	int availableSeaTransportVehicleId = getAvailableSeaTransport(crossOceanAssociation, vehicleId);
	
	if (availableSeaTransportVehicleId == -1)
	{
		debug("\tno available sea transports\n");
		
		// request a transport to be built
		
		std::map<int, int>::iterator seaTransportRequestIterator = aiData.seaTransportRequests.find(crossOceanAssociation);
		
		if (seaTransportRequestIterator == aiData.seaTransportRequests.end())
		{
			aiData.seaTransportRequests.insert({crossOceanAssociation, 1});
		}
		else
		{
			seaTransportRequestIterator->second++;
		}
		
		// meanwhile continue with current task
		
		setTask(vehicleId, task);
		
		return;
		
	}
	
	// get load location
	
	MAP *availableSeaTransportLoadLocation = getSeaTransportLoadLocation(availableSeaTransportVehicleId, vehicleId);
	
	if (availableSeaTransportLoadLocation == nullptr)
		return;
	
	// add boarding tasks
	
	debug("\tadd boarding tasks: [%3d]\n", availableSeaTransportVehicleId);
	
	setTask(vehicleId, Task(BOARD, vehicleId, nullptr, availableSeaTransportVehicleId));
	setTaskIfCloser(availableSeaTransportVehicleId, Task(LOAD, availableSeaTransportVehicleId, availableSeaTransportLoadLocation));
	
	return;
	
}

void populateBaseExposures()
{
	for (int baseId : aiData.baseIds)
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
		
		aiData.baseStrategies[baseId].exposure = exposure;
		
	}
	
}

// ==================================================
// enemy_move entry point
// ==================================================

/*
Modified vehicle movement.
*/
int __cdecl modified_enemy_move(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// choose AI logic
	
	// run WTP AI code for AI eanbled factions
	if (isUseWtpAlgorithms(vehicle->faction_id))
	{
		return moveVehicle(vehicleId);
	}
	// default
	else
	{
		return mod_enemy_move(vehicleId);
	}
	
}

