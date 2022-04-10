#include "ai.h"
#include "main.h"
#include "terranx.h"
#include "terranx_wtp.h"
#include "game_wtp.h"
#include "wtp.h"
#include "aiData.h"
#include "aiHurry.h"
#include "aiMove.h"
#include "aiProduction.h"

/**
Top level AI strategy class. Contains AI methods entry points and global AI variable precomputations.
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
	-> productionStrategy [WTP]
	-> setProduction [WTP]
	-> enemy_units_check

*/

/*
Faction upkeep entry point.
This is called for WTP AI enabled factions only.
*/
int aiFactionUpkeep(const int factionId)
{
	debug("aiFactionUpkeep - %s\n", MFactions[factionId].noun_faction);
	
	// set AI faction id for global reference
	
	aiFactionId = factionId;

	// consider hurrying production in all bases
	
	considerHurryingProduction(factionId);
	
	// redirect to vanilla function for the rest of processing
	// which is overriden by Thinker
	
	return faction_upkeep(factionId);
	
}

/*
Movement phase entry point.
*/
void __cdecl modified_enemy_units_check(int factionId)
{
	clock_t s = 0;
	
 	// setup AI Data
	
	s -= clock();
	aiData.setup();
	s += clock();
	
	// run WTP AI code for AI enabled factions
	
	if (isUseWtpAlgorithms(factionId))
	{
		clock_t c = clock();
		strategy();
		debug("(time) [WTP] aiStrategy: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
		
		// list tasks
		
		if (DEBUG)
		{
			debug("Tasks - %s\n", MFactions[aiFactionId].noun_faction);
			
			for (std::pair<const int, Task> &taskEntry : aiData.tasks)
			{
				Task *task = &(taskEntry.second);
				
				int vehicleId = task->getVehicleId();
				VEH *vehicle = &(Vehicles[vehicleId]);
				
				debug("\t(%3d,%3d)->(%3d,%3d) taskType=%2d, %s\n", vehicle->x, vehicle->y, getX(task->getDestination()), getY(task->getDestination()), task->type, Units[vehicle->unit_id].name);
				
			}
				
		}
		
	}
	
	// execute original code
	
	clock_t c = clock();
	enemy_units_check(factionId);
	debug("(time) [vanilla] enemy_units_check: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
	
	// cleanup AI Data
	// it is important to do it after enemy_units_check as it calls enemy_move that utilizes AI Data.
	
	s -= clock();
	aiData.cleanup();
	s += clock();
	
	debug("(time) [WTP] aiData setup/cleanup: %6.3f\n", (double)(s) / (double)CLOCKS_PER_SEC);
	
}

void strategy()
{
	clock_t c;
	
	// populate data
	
	c = clock();
	populateAIData();
	debug("(time) [WTP] populateAIData: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
	
	// design units
	
	c = clock();
	designUnits();
	debug("(time) [WTP] designUnits: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
	
	// move strategy
	
	c = clock();
	moveStrategy();
	debug("(time) [WTP] moveStrategy: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
	
	// compute production demands

	c = clock();
	productionStrategy();
	debug("(time) [WTP] productionStrategy: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);

	// update base production
	
	c = clock();
	setProduction();
	debug("(time) [WTP] setProduction: %6.3f\n", (double)(clock() - c) / (double)CLOCKS_PER_SEC);
	
}

void populateAIData()
{
	clock_t s;
	
	// associations and connections
	
	analyzeGeography();
	
	// other global variables
	
	populateGlobalVariables();
	populateVehicles();
	
	// base exposures
	
	evaluateBaseBorderDistances();
	
	// bases native defense
	
	evaluateBaseNativeDefenseDemands();
	
	// base police request
	
	evaluateBasePoliceRequests();
	
	// base threat
	
	s = clock();
	evaluateBaseDefense();
	debug("(time) [WTP] evaluateBaseDefense: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
	// warzones
	
	populateWarzones();
	
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

void populateGlobalVariables()
{
	// populate bases

	debug("aiData.baseStrategies\n");

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		// exclude not own bases

		if (base->faction_id != aiFactionId)
			continue;

		// add base to baseIds
		
		aiData.baseIds.push_back(baseId);
		
		// get and clear base info

		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		baseInfo->clear();
		
		// add base

		MAP *baseTile = getMapTile(base->x, base->y);
		aiData.baseTiles.insert({baseTile, baseId});
		debug("baseTiles: (%d)=>%3d\n", (int)baseTile, baseId);

		baseInfo->intrinsicDefenseMultiplier = getBaseDefenseMultiplier(baseId, -1);
		baseInfo->conventionalDefenseMultipliers[TRIAD_LAND] = getBaseDefenseMultiplier(baseId, TRIAD_LAND);
		baseInfo->conventionalDefenseMultipliers[TRIAD_SEA] = getBaseDefenseMultiplier(baseId, TRIAD_SEA);
		baseInfo->conventionalDefenseMultipliers[TRIAD_AIR] = getBaseDefenseMultiplier(baseId, TRIAD_AIR);
		baseInfo->sensorOffenseMultiplier = getSensorOffenseMultiplier(base->faction_id, base->x, base->y);
		baseInfo->sensorDefenseMultiplier = getSensorDefenseMultiplier(base->faction_id, base->x, base->y);

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

	// maxBaseSize
	
	aiData.maxBaseSize = getMaxBaseSize(aiFactionId);
	
	// best weapon and armor
	
	aiData.bestOffenseValue = getFactionBestOffenseValue(aiFactionId);
	aiData.bestDefenseValue = getFactionBestDefenseValue(aiFactionId);
	
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
		aiData.factionInfos[factionId].bestWeaponOffenseValue = getFactionBestOffenseValue(factionId);
		aiData.factionInfos[factionId].bestArmorDefenseValue = getFactionBestDefenseValue(factionId);
	}
	
	// populate units
	
    for (int unitId : getFactionUnitIds(aiFactionId, true))
	{
		UNIT *unit = &(Units[unitId]);
		
		// add unit

		aiData.unitIds.push_back(unitId);
		
		// add prototype units
		
		if (isPrototypeUnit(unitId))
		{
			// not prototyped units
			
			aiData.prototypeUnitIds.push_back(unitId);
			
		}
		
		// add colonu units
		
		if (isColonyUnit(unitId))
		{
			switch (unit->triad())
			{
			case TRIAD_LAND:
				aiData.landColonyUnitIds.push_back(unitId);
				break;
				
			case TRIAD_SEA:
				aiData.seaColonyUnitIds.push_back(unitId);
				break;
				
			case TRIAD_AIR:
				aiData.airColonyUnitIds.push_back(unitId);
				break;
				
			}
			
		}
		
		// add combat unit
		// either psi or anti psi unit or conventional with best weapon/armor
		
		if
		(
			// combat
			isCombatUnit(unitId)
			&&
			(
				Weapon[unit->weapon_type].offense_value < 0
				||
				Armor[unit->armor_type].defense_value < 0
				||
				unit_has_ability(unitId, ABL_TRANCE)
				||
				unit_has_ability(unitId, ABL_EMPATH)
				||
				Weapon[unit->weapon_type].offense_value >= aiData.bestOffenseValue
				||
				Armor[unit->armor_type].defense_value >= aiData.bestDefenseValue
			)
		)
		{
			// combat units
			
			aiData.combatUnitIds.push_back(unitId);
			
			// populate specific triads
			
			switch (unit->triad())
			{
			case TRIAD_LAND:
				aiData.landCombatUnitIds.push_back(unitId);
				aiData.landAndAirCombatUnitIds.push_back(unitId);
				break;
			case TRIAD_SEA:
				aiData.seaCombatUnitIds.push_back(unitId);
				aiData.seaAndAirCombatUnitIds.push_back(unitId);
				break;
			case TRIAD_AIR:
				aiData.airCombatUnitIds.push_back(unitId);
				aiData.landAndAirCombatUnitIds.push_back(unitId);
				aiData.seaAndAirCombatUnitIds.push_back(unitId);
				break;
			}
			
		}
		
	}
	
	// max mineral surplus
	
	aiData.maxMineralSurplus = 1;
	aiData.maxMineralIntake2 = 1;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		int baseOceanAssociation = getBaseOceanAssociation(baseId);
		
		aiData.maxMineralSurplus = std::max(aiData.maxMineralSurplus, base->mineral_surplus);
		aiData.maxMineralIntake2 = std::max(aiData.maxMineralIntake2, base->mineral_intake_2);
		
		if (baseOceanAssociation != -1)
		{
			if (aiData.oceanAssociationMaxMineralSurpluses.count(baseTile->region) == 0)
			{
				aiData.oceanAssociationMaxMineralSurpluses.insert({baseOceanAssociation, 0});
			}
			aiData.oceanAssociationMaxMineralSurpluses.at(baseOceanAssociation) = std::max(aiData.oceanAssociationMaxMineralSurpluses.at(baseOceanAssociation), base->mineral_surplus);
			
		}
		
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

void evaluateBaseBorderDistances()
{
	debug("evaluateBaseBorderDistances - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		
		// find nearest border
		
		int lastTerritoryRingRange = 0;
		
		for (int ringRange = 1; ringRange < *map_axis_x / 2; ringRange++)
		{
			int totalCount = 0;
			int territoryCount = 0;
			
			for (MAP *ringRangeTile : getEqualRangeTiles(base->x, base->y, ringRange))
			{
				totalCount++;
				
				if (ringRangeTile->owner == aiFactionId)
				{
					territoryCount++;
				}
				
			}
			
			// count ring if majority of it is our territory
			
			if ((double)territoryCount / (double)totalCount >= 0.85)
			{
				lastTerritoryRingRange = ringRange;
			}
			
			// exit after 5 not populated rings
			
			if (ringRange - lastTerritoryRingRange > 5)
				break;
			
		}
		
		baseInfo->borderBaseDistance = std::max(0, lastTerritoryRingRange - 3);
		
		// find nearest enemy airbase
		
		int nearestEnemyAirbaseRange = INT_MAX;

		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			int x = getX(mapIndex);
			int y = getY(mapIndex);
			MAP* tile = getMapTile(mapIndex);
			
			// exclude own tiles
			
			if (tile->owner == aiFactionId)
				continue;
			
			// exclude everything but airbase
			
			if (!map_has_item(tile, TERRA_BASE_IN_TILE | TERRA_AIRBASE))
				continue;
			
			// calculate range
			
			int range = map_range(base->x, base->y, x, y);
			
			nearestEnemyAirbaseRange = std::min(nearestEnemyAirbaseRange, range);
			
		}
		
		baseInfo->enemyAirbaseDistance = nearestEnemyAirbaseRange;
		
		debug("\t%-25s borderBaseDistance=%2d, enemyAirbaseDistance=%2d\n", base->name, baseInfo->borderBaseDistance, baseInfo->enemyAirbaseDistance);
		
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

void evaluateBasePoliceRequests()
{
	for (int baseId : aiData.baseIds)
	{
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		
		baseInfo->policeAllowed = getBasePoliceAllowed(baseId);
		baseInfo->policeEffect = getBasePoliceEffect(baseId);
		baseInfo->policeDrones = getBasePoliceDrones(baseId);
		
	}
	
}

void evaluateBaseDefense()
{
	debug("evaluateBaseDefense - %s\n", MFactions[aiFactionId].noun_faction);
	
	Faction *faction = &(Factions[aiFactionId]);
	
	std::set<int> ownCombatUnitIds;
	std::set<int> foeActiveCombatUnitIds;
	std::vector<std::vector<CombatEffect>> unitCombatEffects(2 * MaxProtoFactionNum, std::vector<CombatEffect>(MaxProtoNum));
	
	// collect own combat units
	
	ownCombatUnitIds.insert(aiData.activeCombatUnitIds.begin(), aiData.activeCombatUnitIds.end());
	ownCombatUnitIds.insert(aiData.combatUnitIds.begin(), aiData.combatUnitIds.end());
	
	// collect active foe combat units
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// exclude own
		
		if (vehicle->faction_id == aiFactionId)
			continue;
		
		// exclude non combat
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		// exclude fungal tower
		
		if (vehicle->unit_id == BSC_FUNGAL_TOWER)
			continue;
		
		// add unitId
		
		foeActiveCombatUnitIds.insert(vehicle->unit_id);
		
	}
	
	// add aliens - they are always there
	
	foeActiveCombatUnitIds.insert(BSC_MIND_WORMS);
	foeActiveCombatUnitIds.insert(BSC_SPORE_LAUNCHER);
	foeActiveCombatUnitIds.insert(BSC_SEALURK);
	foeActiveCombatUnitIds.insert(BSC_ISLE_OF_THE_DEEP);
	foeActiveCombatUnitIds.insert(BSC_LOCUSTS_OF_CHIRON);
	
	// store foeActiveCombatUnitIds
	
	aiData.foeActiveCombatUnitIds.clear();
	aiData.foeActiveCombatUnitIds.insert(aiData.foeActiveCombatUnitIds.end(), foeActiveCombatUnitIds.begin(), foeActiveCombatUnitIds.end());
	
	// compute unitCombatEffects
	
	debug("\tunitCombatEffects\n");
	
	// anti-native artillery
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{WPN_HAND_WEAPONS},
		{ARM_NO_ARMOR},
		{ABL_ARTILLERY, ABL_ARTILLERY | ABL_EMPATH, },
		bestReactor,
		PLAN_DEFENSIVE,
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
		UNIT *ownUnit = &(Units[ownUnitId]);
		
		debug("\t\t%-s\n", ownUnit->name);
		
		for (int foeUnitId : aiData.foeActiveCombatUnitIds)
		{
			UNIT *foeUnit = &(Units[foeUnitId]);
			CombatEffect *unitCombatEffect = &(unitCombatEffects.at(getUnitIndex(ownUnitId)).at(foeUnitId));
			
			// calculate how many foe units our unit can destroy
			
			double attackEffect;
			double defendEffect;
			
			if (canUnitAttack(ownUnitId, foeUnitId))
			{
				double relativeUnitStrength = getRelativeUnitStrength(ownUnitId, foeUnitId);
				
				if (relativeUnitStrength > 0.0)
				{
					attackEffect = relativeUnitStrength;
				}
				else
				{
					attackEffect = 0.0;
				}
				
			}
			else
			{
				attackEffect = 0.0;
			}
			
			if (canUnitAttack(foeUnitId, ownUnitId))
			{
				double relativeUnitStrength = getRelativeUnitStrength(foeUnitId, ownUnitId);
				
				if (relativeUnitStrength > 0.0)
				{
					defendEffect = 1.0 / relativeUnitStrength;
				}
				else
				{
					defendEffect = 0.0;
				}
				
			}
			else
			{
				defendEffect = 0.0;
			}
			
			// store values
			
			unitCombatEffect->attack = attackEffect;
			unitCombatEffect->defend = defendEffect;
			
			debug("\t\t\t%-32s a=%5.2f, d=%5.2f\n", foeUnit->name, attackEffect, defendEffect);
			
		}
		
	}

	// evaluate base threat
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool ocean = is_ocean(baseTile);
		BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
		int baseAssociation = getAssociation(baseTile, base->faction_id);
		
		debug
		(
			"\t%-25s, sensorOM=%f, sensorDM=%f, intrinsicDM=%f, conventionalDM[TRIAD_LAND]=%f, conventionalDMs[TRIAD_SEA]=%f, conventionalDMs[TRIAD_AIR]=%f\n",
			base->name,
			baseInfo->sensorOffenseMultiplier,
			baseInfo->sensorDefenseMultiplier,
			baseInfo->intrinsicDefenseMultiplier,
			baseInfo->conventionalDefenseMultipliers[TRIAD_LAND],
			baseInfo->conventionalDefenseMultipliers[TRIAD_SEA],
			baseInfo->conventionalDefenseMultipliers[TRIAD_AIR]
		);
		
		// calculate foe strength
		
		debug("\t\tfoe MilitaryStrength\n");
		
		std::vector<UnitStrength> foeUnitStrengthMultipliers(MaxProtoNum);
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			UNIT *unit = &(Units[vehicle->unit_id]);
			CChassis *chassis = &(Chassis[unit->chassis_type]);
			int triad = chassis->triad;
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			int vehicleAssociation = getVehicleAssociation(vehicleId);
			
			// exclude alien
			
			if (vehicle->faction_id == 0)
				continue;
			
			// exclude own
			
			if (vehicle->faction_id == aiFactionId)
				continue;
			
			// exclude not listed in active units
			
			if (foeActiveCombatUnitIds.count(vehicle->unit_id) == 0)
				continue;
			
			// exclude non combat
			
			if (!isCombatVehicle(vehicleId))
				continue;
			
			// land base
			if (!ocean)
			{
				// do not count sea vehicle for land base assault
				
				if (triad == TRIAD_SEA)
					continue;
				
			}
			// ocean base
			else
			{
				// do not count sea vehicle in other association for ocean base assault
				
				if (triad == TRIAD_SEA && vehicleAssociation != baseAssociation)
					continue;
				
				// do not count land vehicle for ocean base assault unless amphibious
				
				if (triad == TRIAD_LAND && !vehicle_has_ability(vehicle, ABL_AMPHIBIOUS))
					continue;
				
			}
			
			// exclude infantry defensive units in base
			
			if (isInfantryDefensiveVehicle(vehicleId) && map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
				continue;
			
			// strength modifier
			
			double psiOffenseModifier = getVehiclePsiOffenseModifier(vehicleId);
			double psiDefenseModifier = getVehiclePsiDefenseModifier(vehicleId);
			
			double conventionalOffenseModifier = getVehicleConventionalOffenseModifier(vehicleId);
			double conventionalDefenseModifier = getVehicleConventionalDefenseModifier(vehicleId);
			
			// partial health
			
			double partialHealth = (double)getVehiclePower(vehicleId) / (double)getVehicleMaxPower(vehicleId);
			
			// travel time coefficient
			
			double travelTimeCoefficient = getVehicleToBaseTravelTimeCoefficient(vehicleId, baseId);
			
			// threat coefficient
			
			double threatCoefficient = aiData.factionInfos[vehicle->faction_id].threatCoefficient;
			
			// depth coefficient
			
			double depthCoefficient = getVehicleToBaseDepthCoefficient(vehicleId, baseId);
			
			// calculate weight
			
			double weight = partialHealth * travelTimeCoefficient * threatCoefficient * depthCoefficient;
			
			// store values
			
			baseInfo->increaseFoeUnitWeight(vehicle->unit_id, weight);
			foeUnitStrengthMultipliers.at(vehicle->unit_id).psiOffense += weight * psiOffenseModifier;
			foeUnitStrengthMultipliers.at(vehicle->unit_id).psiDefense += weight * psiDefenseModifier;
			foeUnitStrengthMultipliers.at(vehicle->unit_id).conventionalOffense += weight * conventionalOffenseModifier;
			foeUnitStrengthMultipliers.at(vehicle->unit_id).conventionalDefense += weight * conventionalDefenseModifier;
			
			debug
			(
				"\t\t\t(%3d,%3d) %-32s weight=%5.2f, partialHealth=%5.2f, travelTimeCoefficient=%5.2f, threatCoefficient=%5.2f, depthCoefficient=%5.2f, pOM=%5.2f, pDM=%5.2f, cOM=%5.2f, cDM=%5.2f\n",
				vehicle->x, vehicle->y, Units[vehicle->unit_id].name,
				weight, partialHealth, travelTimeCoefficient, threatCoefficient, depthCoefficient,
				psiOffenseModifier, psiDefenseModifier, conventionalOffenseModifier, conventionalDefenseModifier
			);
			
		}
		
		// --------------------------------------------------
		// calculate potential alien strength
		// --------------------------------------------------
		
		debug("\t\tfoe MilitaryStrength, potential alien\n");
		
		{
			int alienUnitId;
			
			if (!ocean)
			{
				alienUnitId = BSC_MIND_WORMS;
			}
			else
			{
				alienUnitId = BSC_ISLE_OF_THE_DEEP;
			}
			
			UNIT *unit = &(Units[alienUnitId]);
			
			// strength modifier
			
			double psiOffenseModifier = getAlienMoraleModifier() * getAlienTurnStrengthModifier();
			double psiDefenseModifier = getAlienMoraleModifier() * getAlienTurnStrengthModifier();
			
			double conventionalOffenseModifier = getAlienMoraleModifier() * getAlienTurnStrengthModifier();
			double conventionalDefenseModifier = getAlienMoraleModifier() * getAlienTurnStrengthModifier();
			
			// depth coefficient
			
			double depthCoefficient = 1.0;
			
			switch (unit->chassis_type)
			{
			case CHS_INFANTRY:
			case CHS_SPEEDER:
			case CHS_HOVERTANK:
				{
					if (!ocean)
					{
						// enemy land unit will have hard time moving through our territory
						depthCoefficient = exp(- (double)baseInfo->borderBaseDistance / 3.0);
					}
					
				}
				break;
				
			case CHS_FOIL:
			case CHS_CRUISER:
				{
					if (ocean)
					{
						// enemy sea unit will have slightly hard time moving through our territory
						depthCoefficient = exp(- (double)baseInfo->borderBaseDistance / 10.0);
					}
					
				}
				break;
				
			}
			
			// set basic numbers to occasional vehicle
			
			double numbers = 1.0;
			
			// add more numbers based on eco-damage and number of fungal pops
			
			if (!ocean)
			{
				numbers += (double)((faction->fungal_pop_count / 3) * (base->eco_damage / 20));
			}
			
			// calculate weight
			
			double weight = depthCoefficient * numbers;
			
			// store values
			
			baseInfo->increaseFoeUnitWeight(alienUnitId, weight);
			foeUnitStrengthMultipliers.at(alienUnitId).psiOffense += weight * psiOffenseModifier;
			foeUnitStrengthMultipliers.at(alienUnitId).psiDefense += weight * psiDefenseModifier;
			foeUnitStrengthMultipliers.at(alienUnitId).conventionalOffense += weight * conventionalOffenseModifier;
			foeUnitStrengthMultipliers.at(alienUnitId).conventionalDefense += weight * conventionalDefenseModifier;
			
			debug
			(
				"\t\t\t%-32s weight=%5.2f, depthCoefficient=%5.2f, numbers=%5.2f, pOM=%5.2f, pDM=%5.2f, cOM=%5.2f, cDM=%5.2f\n",
				Units[alienUnitId].name,
				weight, depthCoefficient, numbers,
				psiOffenseModifier, psiDefenseModifier, conventionalOffenseModifier, conventionalDefenseModifier
			);
			
		}
		
		// --------------------------------------------------
		// calculate potential alien strength - end
		// --------------------------------------------------
		
		// normalize foe unit strengths
		
		for (int foeUnitId : foeActiveCombatUnitIds)
		{
			double weight = baseInfo->getFoeUnitWeight(foeUnitId);
			
			if (weight > 0.0)
			{
				foeUnitStrengthMultipliers.at(foeUnitId).psiOffense /= weight;
				foeUnitStrengthMultipliers.at(foeUnitId).psiDefense /= weight;
				foeUnitStrengthMultipliers.at(foeUnitId).conventionalOffense /= weight;
				foeUnitStrengthMultipliers.at(foeUnitId).conventionalDefense /= weight;
			}
			
		}
		
		// summarize foe unit weights to make total
		
		for (int foeUnitId : foeActiveCombatUnitIds)
		{
			baseInfo->getUnitTypeInfo(foeUnitId)->foeTotalWeight += baseInfo->getFoeUnitWeight(foeUnitId);
		}
		
		if (DEBUG)
		{
			debug("\t\tfoe unit strength multipliers\n");
			
			for (int foeUnitId : foeActiveCombatUnitIds)
			{
				double weight = baseInfo->getFoeUnitWeight(foeUnitId);
				
				if (weight == 0.0)
					continue;
				
				debug
				(
					"\t\t\t%-32s weight=%5.2f, pOM=%5.2f, pDM=%5.2f, cOM=%5.2f, cDM=%5.2f\n",
					Units[foeUnitId].name,
					weight,
					foeUnitStrengthMultipliers.at(foeUnitId).psiOffense,
					foeUnitStrengthMultipliers.at(foeUnitId).psiDefense,
					foeUnitStrengthMultipliers.at(foeUnitId).conventionalOffense,
					foeUnitStrengthMultipliers.at(foeUnitId).conventionalDefense
				)
				;
				
			}
			
		}
		if (DEBUG)
		{
			debug("\t\tfoeTotalWeight by unitType\n");
			for (unsigned int unitType = 0; unitType < UNIT_TYPE_COUNT; unitType++)
			{
				debug("\t\t\t%-10s foeTotalWeight=%5.2f\n", baseInfo->unitTypeInfos[unitType].name, baseInfo->unitTypeInfos[unitType].foeTotalWeight);
			}
		}
		
		// update base combat effects
		
		debug("\t\tcombat effects\n");
		
		for (int ownUnitId : ownCombatUnitIds)
		{
			int ownUnitOffenseValue = getUnitOffenseValue(ownUnitId);
			int ownUnitDefenseValue = getUnitDefenseValue(ownUnitId);
			
			// safety check
			
			if (ownUnitOffenseValue == 0 || ownUnitDefenseValue == 0)
				continue;
			
			debug("\t\t\t%-32s\n", Units[ownUnitId].name);
			
			// estimate probability to attack
			// estimate probability to stay in base/bunker
			
			double attackProbability;
			double stayInProbability;
			
			if (unit_triad(ownUnitId) == TRIAD_AIR)
			{
				attackProbability = 0.5;
				stayInProbability = 0.5;
			}
			else if (isUnitHasAbility(ownUnitId, ABL_ARTILLERY))
			{
				attackProbability = 0.8;
				stayInProbability = 1.0;
			}
			else if (isUnitHasAbility(ownUnitId, ABL_EMPATH | ABL_AIR_SUPERIORITY | ABL_DISSOCIATIVE_WAVE ))
			{
				attackProbability = 0.5;
				stayInProbability = 0.5;
			}
			else if (ownUnitOffenseValue < 0 || ownUnitDefenseValue < 0)
			{
				attackProbability = 0.5;
				stayInProbability = 0.5;
			}
			else
			{
				attackProbability = 0.5 * std::min(1.0, (double)ownUnitOffenseValue / (double)aiData.bestOffenseValue);
				stayInProbability = 1.0 - attackProbability;
			}
			
			double combatEffectWeight = 0.0;
			double combatEffectWeightSum = 0.0;
			
			for (int foeUnitId : foeActiveCombatUnitIds)
			{
				// exclude unit without weight
				
				double weight = baseInfo->getFoeUnitWeight(foeUnitId);
				
				if (weight == 0.0)
					continue;
				
				// exclude units those cannot attack each other
				
				if (!canUnitAttack(ownUnitId, foeUnitId) && !canUnitAttack(foeUnitId, ownUnitId))
					continue;
				
				// get unit combat effect
				
				CombatEffect *unitCombatEffect = &(unitCombatEffects.at(getUnitIndex(ownUnitId)).at(foeUnitId));
				
				// exclude unit from different types or those not attacking each other
				
				if (unitCombatEffect->attack == 0.0 && unitCombatEffect->defend == 0.0)
					continue;
				
				// calculate combat effect
				
				CombatEffect *combatEffect = baseInfo->getCombatEffect(ownUnitId, foeUnitId);
				
				double attackCombatEffect;
				double defendCombatEffect;
				
				if (isPsiCombat(ownUnitId, foeUnitId))
				{
					
					attackCombatEffect = unitCombatEffect->attack / foeUnitStrengthMultipliers.at(foeUnitId).psiDefense;
					defendCombatEffect = unitCombatEffect->defend / foeUnitStrengthMultipliers.at(foeUnitId).psiOffense;
					
					// PLANET multiplier
					
					attackCombatEffect *= getFactionSEPlanetOffenseModifier(aiFactionId);
					defendCombatEffect *= getFactionSEPlanetDefenseModifier(aiFactionId);
					
					// base psi defense multiplier
					
					double basePsiDefenseMultiplier = baseInfo->intrinsicDefenseMultiplier;
					
					defendCombatEffect *= ((1.0 - stayInProbability) + stayInProbability * basePsiDefenseMultiplier);
					
				}
				else
				{
					attackCombatEffect = unitCombatEffect->attack / foeUnitStrengthMultipliers.at(foeUnitId).conventionalDefense;
					defendCombatEffect = unitCombatEffect->defend / foeUnitStrengthMultipliers.at(foeUnitId).conventionalOffense;
					
					// base conventional defense multiplier
					
					double baseConventionalDefenseMultiplier = (isUnitHasAbility(foeUnitId, ABL_BLINK_DISPLACER) ? baseInfo->intrinsicDefenseMultiplier : baseInfo->conventionalDefenseMultipliers[unit_triad(foeUnitId)]);
					
					defendCombatEffect *= ((1.0 - stayInProbability) + stayInProbability * baseConventionalDefenseMultiplier);
					
				}
				
				// sensor effect multiplier
				
				attackCombatEffect *= baseInfo->sensorOffenseMultiplier;
				defendCombatEffect *= baseInfo->sensorDefenseMultiplier;
				
				// store values
				
				combatEffect->attack = attackCombatEffect;
				combatEffect->defend = defendCombatEffect;
				
				// set average estimate
				
				if (attackCombatEffect > 0.0 && defendCombatEffect <= 0.0)
				{
					combatEffect->average = attackCombatEffect;
				}
				else if (attackCombatEffect <= 0.0 && defendCombatEffect > 0.0)
				{
					combatEffect->average = defendCombatEffect;
				}
				else if (attackCombatEffect > 0.0 && defendCombatEffect > 0.0)
				{
					combatEffect->average = attackProbability * attackCombatEffect + (1.0 - attackProbability) * defendCombatEffect;
				}
				else
				{
					combatEffect->average = 0.0;
				}
				
				// summarize for average
				
				if (combatEffect->average > 0.0)
				{
					combatEffectWeight += weight;
					combatEffectWeightSum += weight * combatEffect->average;
				}
				
				debug
				(
					"\t\t\t\t%-32s average=%5.2f, a=%5.2f, d=%5.2f, aP=%5.2f, sP=%5.2f\n",
					Units[foeUnitId].name,
					baseInfo->getCombatEffect(ownUnitId, foeUnitId)->average,
					baseInfo->getCombatEffect(ownUnitId, foeUnitId)->attack,
					baseInfo->getCombatEffect(ownUnitId, foeUnitId)->defend,
					attackProbability, stayInProbability
				)
				;
				
			}
			
			// update average combat effect
			
			double averageCombatEffect = (combatEffectWeight <= 0.0 ? 0.0 : combatEffectWeightSum / combatEffectWeight);
			baseInfo->setAverageCombatEffect(ownUnitId, averageCombatEffect);
			debug("\t\t\t\t[%3d] averageCombatEffect=%5.2f\n", ownUnitId, averageCombatEffect);
			
		}
		
		// evaluate base own total combat effect
		
		debug("\t\town MilitaryStrength\n");
		
		for (int vehicleId : aiData.combatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			UNIT *unit = &(Units[vehicle->unit_id]);
			CChassis *chassis = &(Chassis[unit->chassis_type]);
			int triad = chassis->triad;
			int vehicleAssociation = getVehicleAssociation(vehicleId);
			
			// exclude infantry defensive units in other base
			
			if (isInfantryDefensiveVehicle(vehicleId) && map_has_item(vehicleTile, TERRA_BASE_IN_TILE) && vehicleTile != baseTile)
				continue;
			
			// land base
			if (!ocean)
			{
				// do not count sea vehicle for land base protection 
				
				if (triad == TRIAD_SEA)
					continue;
				
			}
			// ocean base
			else
			{
				// do not count sea vehicle in other association for ocean base protection
				
				if (triad == TRIAD_SEA && vehicleAssociation != baseAssociation)
					continue;
				
			}
			
			// partial health
			
			double partialHealth = (double)getVehiclePower(vehicleId) / (double)getVehicleMaxPower(vehicleId);
			
			// vehicle distance to base
			
			int distance = map_range(base->x, base->y, vehicle->x, vehicle->y);
			
			// vehicle speed
			
			double speed;
			
			if (triad == TRIAD_LAND)
			{
				double minSpeed = 1.5 * (double)veh_speed_without_roads(vehicleId);
				double maxSpeed = (double)mod_veh_speed(vehicleId);
				speed = minSpeed + (maxSpeed - minSpeed) * ((double)std::min(250, std::max(0, *current_turn - 50)) / 250.0);
			}
			else
			{
				speed = (double)veh_speed_without_roads(vehicleId);
			}
			
			// time to reach the base
			
			double travelTime = (double)distance / speed;
			
			// increase effective time for land unit in different association
			
			if (triad == TRIAD_LAND && distance > 0 && vehicleAssociation != baseAssociation)
			{
				travelTime *= 2.0;
			}
			
			// travel time coefficient
			// reduce weight based on time to reach the base
			// every 10 turns reduce weight in e
			
			double travelTimeCoefficient = exp(- travelTime / 3.0);
			
			// calculate weight
			
			double weight = partialHealth * travelTimeCoefficient;
			
			// accumulate weight
			
			baseInfo->increaseOwnUnitWeight(vehicle->unit_id, weight);
			baseInfo->getUnitTypeInfo(vehicle->unit_id)->ownTotalWeight += weight;
			
			// accumulate total combat effect
			
			double vehicleAveragecombatEffect = baseInfo->getVehicleAverageCombatEffect(vehicleId);
			baseInfo->getUnitTypeInfo(vehicle->unit_id)->ownTotalCombatEffect += weight * vehicleAveragecombatEffect;
			
			debug("\t\t\t[%3d](%3d,%3d) %-25s weight=%5.2f, vehicleAveragecombatEffect=%5.2f\n", vehicleId, vehicle->x, vehicle->y, unit->name, weight, vehicleAveragecombatEffect);
			
		}
		
		// populate protectors
		
		std::map<int, double> protectors;
		
		for (int ownUnitId : ownCombatUnitIds)
		{
			baseInfo->getUnitTypeInfo(ownUnitId)->protectors.push_back({ownUnitId, baseInfo->getAverageCombatEffect(ownUnitId)});
		}
		
		// calculate computed values
		
		baseInfo->setUnitTypeComputedValues();
		
		if (DEBUG)
		{
			debug("\t\tprotectors\n");
			for (const UnitTypeInfo &unitTypeInfo : baseInfo->unitTypeInfos)
			{
				debug("\t\t\t%-15s\n", unitTypeInfo.name);
				
				for (IdDoubleValue protector : unitTypeInfo.protectors)
				{
					debug("\t\t\t\t%-32s %5.2f\n", Units[protector.id].name, protector.value);
				}
				
			}
			
		}
		
	}
	
	// summarize our unit average effects across all bases
	
	for (int unitId : ownCombatUnitIds)
	{
		double averageCombatEffectWeight = 0.0;
		double averageCombatEffectWeightSum = 0.0;
		
		for (int baseId : aiData.baseIds)
		{
			BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
			
			double weight = baseInfo->getUnitTypeInfo(unitId)->foeTotalWeight;
			
			averageCombatEffectWeight += weight;
			averageCombatEffectWeightSum += weight * baseInfo->getAverageCombatEffect(unitId);
			
		}
		
		double averageCombatEffect = (averageCombatEffectWeight <= 0.0 ? 0.0 : averageCombatEffectWeightSum / averageCombatEffectWeight);
		aiData.setAverageCombatEffect(unitId, averageCombatEffect);
		
	}
	
}

void populateWarzones()
{
	// populate blocked/warzone and zoc locations

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		bool ocean = is_ocean(vehicleTile);
		UNIT *unit = &(Units[vehicle->unit_id]);
		int vehicleAssociation = getVehicleAssociation(vehicleId);

		// exclude own vehicles and vehicles with pact

		if (vehicle->faction_id == aiFactionId || has_pact(aiFactionId, vehicle->faction_id))
			continue;
		
		// store blocked location

		aiData.getTileInfo(vehicle->x, vehicle->y)->blocked = true;

		// calculate zoc locations

		if (!ocean)
		{
			for (MAP *adjacentTile : getAdjacentTiles(vehicle->x, vehicle->y, true))
			{
				if (!is_ocean(adjacentTile))
				{
					aiData.getTileInfo(adjacentTile)->zoc = true;
				}

			}

		}
		
		// exclude units in opposite realm
		
		if (unit->triad() == TRIAD_LAND && ocean || unit->triad() == TRIAD_SEA && !ocean)
			continue;

		// store warzone location
		
		if (isWar(aiFactionId, vehicle->faction_id))
		{
			// define range
			
			int range = 0;
			
			if (vehicle->faction_id == 0)
			{
				switch (vehicle->unit_id)
				{
				case BSC_FUNGAL_TOWER:
					range = 1;
					break;
					
				case BSC_MIND_WORMS:
				case BSC_SPORE_LAUNCHER:
					range = 2;
					break;
					
				}
				
			}
			else
			{
				switch (unit->triad())
				{
				case TRIAD_LAND:
					range = 3 * unit->speed();
					break;
					
				default:
					range = unit->speed();
					break;
					
				}
				
			}
			
			if (range >= 1)
			{
				for (MAP *rangeTile : getRangeTiles(vehicle->x, vehicle->y, range, true))
				{
					int rangeTileAssociation = getAssociation(rangeTile, vehicle->faction_id);
					
					// skip other associations
					
					if (rangeTileAssociation != vehicleAssociation)
						continue;
					
					aiData.getTileInfo(rangeTile)->warzone = true;
					
				}
				
			}
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("warzones - %s\n", MFactions[aiFactionId].noun_faction);
		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			if (aiData.getTileInfo(mapIndex)->warzone)
			{
				debug("\t(%3d,%3d)\n", getX(mapIndex), getY(mapIndex));
			}
		}
	}
	
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
	
	// land artillery
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{bestWeapon},
		{ARM_NO_ARMOR},
		{ABL_ARTILLERY},
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
	
	// --------------------------------------------------
	// obsolete units
	// --------------------------------------------------
	
	// land fast artillery
	
	obsoletePrototypes
	(
		aiFactionId,
		{CHS_SPEEDER, CHS_HOVERTANK},
		{},
		{},
		{ABL_ARTILLERY},
		{}
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

/*
Obsolete prototypes.
*/
void obsoletePrototypes(int factionId, std::set<int> chassisIds, std::set<int> weaponIds, std::set<int> armorIds, std::set<int> abilityFlagsSet, std::set<int> reactors)
{
	debug("obsoletePrototypes - %s\n", MFactions[factionId].noun_faction);
	
	if (DEBUG)
	{
		debug("\tchassis\n");
		for (int chassisId : chassisIds)
		{
			debug("\t\t%d\n", chassisId);
		}
		
		debug("\tweapons\n");
		for (int weaponId : weaponIds)
		{
			debug("\t\t%s\n", Weapon[weaponId].name);
		}
		
		debug("\tarmors\n");
		for (int armorId : armorIds)
		{
			debug("\t\t%s\n", Armor[armorId].name);
		}
		
		debug("\tabilityFlags\n");
		for (int abilityFlags : abilityFlagsSet)
		{
			debug("\t\t%s\n", getAbilitiesString(abilityFlags).c_str());
		}
		
		debug("\treactors\n");
		for (int reactor : reactors)
		{
			debug("\t\t%s\n", Reactor[reactor - 1].name);
		}
		
	}
	
	debug("\t----------\n");
	for (int unitId : getFactionUnitIds(factionId, true))
	{
		UNIT *unit = &(Units[unitId]);
		debug("\t%s\n", unit->name);
		debug("\t\t%d\n", unit->chassis_type);
		debug("\t\t%s\n", Weapon[unit->weapon_type].name);
		debug("\t\t%s\n", Armor[unit->armor_type].name);
		debug("\t\t%s\n", getAbilitiesString(unit->ability_flags).c_str());
		debug("\t\t%s\n", Reactor[unit->reactor_type - 1].name);
		
		// verify condition match
		
		if
		(
			(chassisIds.size() == 0 || chassisIds.count(unit->chassis_type) != 0)
			&&
			(weaponIds.size() == 0 || weaponIds.count(unit->weapon_type) != 0)
			&&
			(armorIds.size() == 0 || armorIds.count(unit->armor_type) != 0)
			&&
			(abilityFlagsSet.size() == 0 || abilityFlagsSet.count(unit->ability_flags) != 0)
			&&
			(reactors.size() == 0 || reactors.count(unit->reactor_type) != 0)
		)
		{
			// obsolete unit for this faction
			
			unit->obsolete_factions |= (0x1 << factionId);
			debug("\t> obsoleted\n");
			
		}
			
	}
	
}

// --------------------------------------------------
// helper functions
// --------------------------------------------------

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

MAP *getClosestPod(int vehicleId)
{
	MAP *closestPod = nullptr;
	int minPathDistance = INT_MAX;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int x = getX(mapIndex);
		int y = getY(mapIndex);
		
		// pod
		
		if (goody_at(x, y) == 0)
			continue;
		
		// reachable
		
		if (!isDestinationReachable(vehicleId, x, y, false))
			continue;
		
		// get distance
		
		int pathDistance = getVehiclePathDistance(vehicleId, x, y);
		
		if (pathDistance == -1)
			continue;
		
		// update best
		
		if (pathDistance < minPathDistance)
		{
			closestPod = getMapTile(mapIndex);
			minPathDistance = pathDistance;
		}
		
	}
	
	return closestPod;
	
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

int getFactionBestOffenseValue(int factionId)
{
	int bestWeaponOffenseValue = 0;
	
	for (int unitId : getFactionUnitIds(factionId, true))
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

int getFactionBestDefenseValue(int factionId)
{
	int bestArmorDefenseValue = 0;
	
	for (int unitId : getFactionUnitIds(factionId, true))
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
		
		if (otherVehicle->faction_id != 0 && !isWar(otherVehicle->faction_id, vehicle->faction_id))
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
			if (!map_has_item(vehicleTile, TERRA_BASE_IN_TILE | TERRA_AIRBASE))
				return false;
			
			return getVehicleSpeedWithoutRoads(vehicleId) >= map_range(vehicle->x, vehicle->y, getX(destination), getY(destination));
			
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
	return (factionId != 0 && !is_human(factionId) && ai_enabled(factionId) && conf.ai_useWTPAlgorithms && factionId <= conf.wtp_factions_enabled);
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

int getPathDistance(int srcX, int srcY, int dstX, int dstY, int unitId, int factionId)
{
//	debug("getPathDistance (%3d,%3d) -> (%3d,%3d) %s - %s\n", srcX, srcY, dstX, dstY, Units[unitId].name, MFactions[factionId].noun_faction);
//	
	if(!isOnMap(srcX, srcY) || !isOnMap(dstX, dstY))
		return -1;
	
	MAP *srcTile = getMapTile(srcX, srcY);
	MAP *dstTile = getMapTile(dstX, dstY);
	
	if (srcTile == nullptr || dstTile == nullptr)
		return -1;
	
	// verify src and dst are reachable by unit
	
	switch (Units[unitId].triad())
	{
	case TRIAD_LAND:
		{
			// land unit cannot path on ocean
			
			if (is_ocean(srcTile) || is_ocean(dstTile))
				return -1;
			
			int srcAssociation = getAssociation(srcTile, factionId);
			int dstAssociation = getAssociation(dstTile, factionId);
			
			// land unit can path only within same association
			
			if (srcAssociation == -1 || dstAssociation == -1 || dstAssociation != srcAssociation)
				return -1;
			
		}
		break;
		
	case TRIAD_SEA:
		{
			int srcOceanAssociation = getOceanAssociation(srcTile, factionId);
			int dstOceanAssociation = getOceanAssociation(dstTile, factionId);
			
			// sea unit can move only within the same ocean association
			
			if (srcOceanAssociation == -1 || dstOceanAssociation == -1 || dstOceanAssociation != srcOceanAssociation)
				return -1;
			
		}
		break;
		
	}
	
	// process path
	
	int pX = srcX;
	int pY = srcY;
	int angle = -1;
	int flags = 0x00 | (!isCombatUnit(unitId) && !has_project(factionId, FAC_XENOEMPATHY_DOME) ? 0x10 : 0x00);
	int distance = 0;
	
	while (distance < *map_axis_x + *map_axis_y)
	{
		if (pX == dstX && pY == dstY)
		{
			return distance;
		}
		
		angle = PATH_find(PATH, pX, pY, dstX, dstY, unitId, factionId, flags, angle);
		
		// wrong angle
		
		if (angle < 0)
			return -1;
		
		int nX = wrap(pX + BaseOffsetX[angle]);
		int nY = pY + BaseOffsetY[angle];
		
		// update distance
		
		distance++;
		
//		debug("\t(%3d,%3d)->(%3d,%3d) distance = %2d\n", pX, pY, nX, nY, distance);
//		
		// step to next tile
		
		pX = nX;
		pY = nY;
		
	}
	
	return -1;
	
}
int getVehiclePathDistance(int vehicleId, int x, int y)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	return getPathDistance(vehicle->x, vehicle->y, x, y, vehicle->unit_id, vehicle->faction_id);
	
}

int getPathMovementCost(int srcX, int srcY, int dstX, int dstY, int unitId, int factionId, bool excludeDestination)
{
//	debug("getPathTravelTime (%3d,%3d) -> (%3d,%3d) %s - %s\n", srcX, srcY, dstX, dstY, Units[unitId].name, MFactions[factionId].noun_faction);
//	
	if(!isOnMap(srcX, srcY) || !isOnMap(dstX, dstY))
		return -1;
	
	MAP *srcTile = getMapTile(srcX, srcY);
	MAP *dstTile = getMapTile(dstX, dstY);
	
	if (srcTile == nullptr || dstTile == nullptr)
		return -1;
	
	// verify src and dst are reachable by unit
	
	switch (Units[unitId].triad())
	{
	case TRIAD_LAND:
		{
			// land unit cannot path on ocean
			
			if (is_ocean(srcTile) || is_ocean(dstTile))
				return -1;
			
			int srcAssociation = getAssociation(srcTile, factionId);
			int dstAssociation = getAssociation(dstTile, factionId);
			
			// land unit can path only within same association
			
			if (srcAssociation == -1 || dstAssociation == -1 || dstAssociation != srcAssociation)
				return -1;
			
		}
		break;
		
	case TRIAD_SEA:
		{
			int srcOceanAssociation = getOceanAssociation(srcTile, factionId);
			int dstOceanAssociation = getOceanAssociation(dstTile, factionId);
			
			// sea unit can move only within the same ocean association
			
			if (srcOceanAssociation == -1 || dstOceanAssociation == -1 || dstOceanAssociation != srcOceanAssociation)
				return -1;
			
		}
		break;
		
	}
	
	// process path
	
	int pX = srcX;
	int pY = srcY;
	int angle = -1;
	int flags = 0xC0 | (!isCombatUnit(unitId) && !has_project(factionId, FAC_XENOEMPATHY_DOME) ? 0x10 : 0x00);
	int previousStepMovementCost = 0;
	int movementCost = 0;
	
	while (movementCost < (*map_axis_x + *map_axis_y) * Rules->mov_rate_along_roads)
	{
		if (pX == dstX && pY == dstY)
		{
			return (excludeDestination ? previousStepMovementCost : movementCost);
		}
		
		angle = PATH_find(PATH, pX, pY, dstX, dstY, unitId, factionId, flags, angle);
		
		// wrong angle
		
		if (angle < 0)
			return -1;
		
		int nX = wrap(pX + BaseOffsetX[angle]);
		int nY = pY + BaseOffsetY[angle];
		
		// calculate hex cost
		
		int hexCost = getHexCost(unitId, factionId, pX, pY, nX, nY);
		
		// update movement rate
		
		previousStepMovementCost = movementCost;
		movementCost += hexCost;
		
//		debug("\t(%3d,%3d)->(%3d,%3d) cost = %2d, movementCost = %2d\n", pX, pY, nX, nY, hexCost, movementCost);
//		
		// step to next tile
		
		pX = nX;
		pY = nY;
		
	}
	
	return -1;
	
}
int getVehiclePathMovementCost(int vehicleId, int x, int y, bool excludeDestination)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	return getPathMovementCost(vehicle->x, vehicle->y, x, y, vehicle->unit_id, vehicle->faction_id, excludeDestination);
	
}

int getPathTravelTime(int sourceX, int sourceY, int destinationX, int destinationY, int unitId, int factionId)
{
	int unitSpeed = unit_speed(unitId);
	
	int pathMovementCost = getPathMovementCost(sourceX, sourceY, destinationX, destinationY, unitId, factionId, false);
	
	if (pathMovementCost == -1)
		return -1;
	
	return (pathMovementCost + (unitSpeed - 1)) / unitSpeed;
	
}
int getVehiclePathTravelTime(int vehicleId, int x, int y)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	return getPathTravelTime(vehicle->x, vehicle->y, x, y, vehicle->unit_id, vehicle->faction_id);
	
}

/*
Sends vehicles to locations trying to minimize travel time.
Crude algorithm to match closest pairs first.
Takes triad into account.
*/
std::vector<VehicleLocation> matchVehiclesToUnrankedLocations(std::vector<int> vehicleIds, std::vector<MAP *> locations)
{
	debug("matchVehiclesToUnrankedLocations - %s\n", MFactions[aiFactionId].noun_faction);
	
	std::vector<VehicleLocation> vehicleLocations;
	
	if (vehicleIds.size() == 0 || locations.size() == 0)
	{
		debug("\tno vehicles or locations\n");
		return vehicleLocations;
	}
	
	// populate travel times
	
	std::vector<VehicleLocationTravelTime> vehicleLocaionTravelTimes;
	
	for (int vehicleId : vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int triad = vehicle->triad();
		int speed = getVehicleSpeedWithoutRoads(vehicleId);
		int vehicleAssociation = getVehicleAssociation(vehicleId);
		
		for (MAP *location : locations)
		{
			int x = getX(location);
			int y = getY(location);
			bool ocean = is_ocean(location);
			int locationOceanAssociation = getOceanAssociation(location, aiFactionId);
			
			// verify location is reachable
			
			switch (triad)
			{
			case TRIAD_LAND:
				{
					// land unit cannot go to ocean
					
					if (ocean)
						continue;
					
				}
				break;
				
			case TRIAD_SEA:
				{
					// sea unit cannot go to land
					
					if (!ocean)
						continue;
					
					// sea unit cannot go to different ocean association
					
					if (vehicleAssociation == -1 || locationOceanAssociation == -1 || vehicleAssociation != locationOceanAssociation)
						continue;
					
				}
				break;
				
			}
			
			int range = map_range(vehicle->x, vehicle->y, x, y);
			int travelTime = (range + (speed - 1)) / speed;
			
			vehicleLocaionTravelTimes.push_back({vehicleId, location, travelTime});
			
		}
		
	}
	
	// sort travel times
	
	std::sort
	(
		begin(vehicleLocaionTravelTimes),
		end(vehicleLocaionTravelTimes),
		[](const VehicleLocationTravelTime &vehicleLocationTravelTime1, const VehicleLocationTravelTime &vehicleLocationTravelTime2) -> bool
		{
			return vehicleLocationTravelTime1.travelTime < vehicleLocationTravelTime2.travelTime;
		}
	)
	;
	
	// match vehicles and locations
	
	std::set<int> matchedVehicleIds;
	std::set<MAP *> matchedLocations;
	
	for (VehicleLocationTravelTime &vehicleLocationTravelTime : vehicleLocaionTravelTimes)
	{
		int vehicleId = vehicleLocationTravelTime.vehicleId;
		MAP *location = vehicleLocationTravelTime.location;
		
		// skip matched
		
		if (matchedVehicleIds.count(vehicleId) != 0 || matchedLocations.count(location) != 0)
			continue;
		
		// match
		
		vehicleLocations.push_back({vehicleId, location});
		
		matchedVehicleIds.insert(vehicleId);
		matchedLocations.insert(location);
		
		debug("\t[%3d] (%3d,%3d) -> (%3d,%3d)\n", vehicleId, Vehicles[vehicleId].x, Vehicles[vehicleId].y, getX(location), getY(location));
		
	}
	
	return vehicleLocations;
	
}

/*
Sends vehicles to locations trying to minimize travel time.
Crude algorithm to match closest pairs first.
Takes triad into account.
Locations are sorted by priority.
*/
std::vector<VehicleLocation> matchVehiclesToRankedLocations(std::vector<int> vehicleIds, std::vector<MAP *> locations)
{
	debug("matchVehiclesToRankedLocations - %s\n", MFactions[aiFactionId].noun_faction);
	
	std::vector<VehicleLocation> vehicleLocations;
	
	if (vehicleIds.size() == 0 || locations.size() == 0)
	{
		debug("\tno vehicles or locations\n");
		return vehicleLocations;
	}
	
	// assign ranked locations to vehicles
	
	std::set<int> assignedVehicleIds;
	
	for (MAP *location : locations)
	{
		int x = getX(location);
		int y = getY(location);
		bool ocean = is_ocean(location);
		int locationOceanAssociation = getOceanAssociation(location, aiFactionId);
		
		int closestVehicleId = -1;
		int closestVehicleTravelTime = INT_MAX;
		
		for (int vehicleId : vehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			int triad = vehicle->triad();
			int speed = getVehicleSpeedWithoutRoads(vehicleId);
			int vehicleAssociation = getVehicleAssociation(vehicleId);
			
			// skip already assigned
			
			if (assignedVehicleIds.count(vehicleId) != 0)
				continue;
			
			// verify location is reachable
			
			switch (triad)
			{
			case TRIAD_LAND:
				{
					// land unit cannot go to ocean
					
					if (ocean)
						continue;
					
				}
				break;
				
			case TRIAD_SEA:
				{
					// sea unit cannot go to land
					
					if (!ocean)
						continue;
					
					// sea unit cannot go to different ocean association
					
					if (vehicleAssociation == -1 || locationOceanAssociation == -1 || vehicleAssociation != locationOceanAssociation)
						continue;
					
				}
				break;
				
			}
			
			int range = map_range(vehicle->x, vehicle->y, x, y);
			int travelTime = (range + (speed - 1)) / speed;
			
			if (travelTime < closestVehicleTravelTime)
			{
				closestVehicleId = vehicleId;
				closestVehicleTravelTime = travelTime;
			}
			
		}
		
		// not found
		
		if (closestVehicleId == -1)
			continue;
		
		// assign vehicle
		
		vehicleLocations.push_back({closestVehicleId, location});
		assignedVehicleIds.insert(closestVehicleId);
		
	}
	
	return vehicleLocations;
	
}

bool compareIdDoubleValueAscending(const IdDoubleValue &a, const IdDoubleValue &b)
{
	return a.value < b.value;
}

bool compareIdDoubleValueDescending(const IdDoubleValue &a, const IdDoubleValue &b)
{
	return a.value > b.value;
}

/*
Checks if one unit can attack another.
*/
bool canUnitAttack(int attackerUnitId, int defenderUnitId)
{
	// cannot attack needlejet without air superiority
	
	if (Units[defenderUnitId].chassis_type == CHS_NEEDLEJET && !unit_has_ability(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// otherwise can attack
	
	return true;
	
}

/*
Checks if one vehicle can attack another.
*/
bool canVehicleAttack(int attackerVehicleId, int defenderVehicleId)
{
	VEH *attackerVehicle = &(Vehicles[attackerVehicleId]);
	VEH *defenderVehicle = &(Vehicles[defenderVehicleId]);
	
	UNIT *attackerUnit = &(Units[attackerVehicle->unit_id]);
	UNIT *defenderUnit = &(Units[defenderVehicle->unit_id]);
	
	MAP *attackerVehicleTile = getVehicleMapTile(attackerVehicleId);
	MAP *defenderVehicleTile = getVehicleMapTile(defenderVehicleId);
	
	// unit cannot attack
	
	if (!canUnitAttack(attackerVehicle->unit_id, defenderVehicle->unit_id))
		return false;
	
	if (attackerUnit->triad() == TRIAD_LAND && is_ocean(attackerVehicleTile))
	{
		if (map_has_item(attackerVehicleTile, TERRA_BASE_IN_TILE))
		{
			// land unit cannot attack from sea base except artillery or amphibious
			
			if (!unit_has_ability(attackerVehicle->unit_id, ABL_AMPHIBIOUS) && !unit_has_ability(attackerVehicle->unit_id, ABL_ARTILLERY))
				return false;
			
		}
		else
		{
			// land unit cannot attack from sea NOT base except amphibious
			
			if (!unit_has_ability(attackerVehicle->unit_id, ABL_AMPHIBIOUS))
				return false;
			
		}
		
	}
	
	if (defenderUnit->triad() == TRIAD_LAND && is_ocean(defenderVehicleTile))
	{
		// land unit cannot attack to sea except amphibious
		
		if (!unit_has_ability(attackerVehicle->unit_id, ABL_AMPHIBIOUS))
			return false;
		
	}
	
	// otherwise, can attack
	
	return true;
	
}

/*
Calculates relative attacker odds for units (= relative strength * relative hit points).
*/
double getRelativeUnitStrength(int attackerUnitId, int defenderUnitId)
{
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerStrength = Weapon[attackerUnit->weapon_type].offense_value;
	int defenderStrength = Armor[defenderUnit->armor_type].defense_value;
	
	// illegal arguments - should not happen
	
	if (attackerStrength == 0 || defenderStrength == 0)
		return 0.0;
	
	// calculate relative strength
	
	double relativeStrength;
	
	if (attackerStrength < 0 || defenderStrength < 0)
	{
		// psi combat
		
		if (unit_has_ability(attackerUnitId, ABL_ARTILLERY) && !unit_has_ability(defenderUnitId, ABL_ARTILLERY) || !unit_has_ability(attackerUnitId, ABL_ARTILLERY) && unit_has_ability(defenderUnitId, ABL_ARTILLERY))
		{
			// do not compute strength for artillery vs. not artillery
			
			return 0.0;
			
		}
		
		// get relative strength
		
		switch (attackerUnit->triad())
		{
		case TRIAD_LAND:
			relativeStrength = (double)Rules->psi_combat_land_numerator / (double)Rules->psi_combat_land_denominator;
			break;
			
		case TRIAD_SEA:
			relativeStrength = (double)Rules->psi_combat_sea_numerator / (double)Rules->psi_combat_sea_denominator;
			break;
			
		case TRIAD_AIR:
			relativeStrength = (double)Rules->psi_combat_air_numerator / (double)Rules->psi_combat_air_denominator;
			break;
		
		default:
			relativeStrength = 1.0;
			
		}
		
		// reactor
		// psi combat ignores reactor
		
		// abilities
		
		if (unit_has_ability(attackerUnitId, ABL_EMPATH))
		{
			relativeStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_empath_song_vs_psi);
		}
		
		if (unit_has_ability(defenderUnitId, ABL_TRANCE))
		{
			relativeStrength /= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi);
		}
		
	}
	else
	{
		// conventional combat
		
		if (unit_has_ability(attackerUnitId, ABL_ARTILLERY) && unit_has_ability(defenderUnitId, ABL_ARTILLERY))
		{
			// conventional artillery duel
			
			if (conf.conventional_artillery_duel_uses_weapon_and_armor)
			{
				attackerStrength = Weapon[attackerUnit->weapon_type].offense_value + Armor[attackerUnit->armor_type].defense_value;
				defenderStrength = Weapon[defenderUnit->weapon_type].offense_value + Armor[defenderUnit->armor_type].defense_value;
			}
			else
			{
				attackerStrength = Weapon[attackerUnit->weapon_type].offense_value;
				defenderStrength = Weapon[defenderUnit->weapon_type].offense_value;
			}
			
		}
		else if (unit_has_ability(attackerUnitId, ABL_ARTILLERY) && !unit_has_ability(defenderUnitId, ABL_ARTILLERY) || !unit_has_ability(attackerUnitId, ABL_ARTILLERY) && unit_has_ability(defenderUnitId, ABL_ARTILLERY))
		{
			// do not compute strength for artillery vs. not artillery
			
			return 0.0;
			
		}
		
		// get relative strength
		
		relativeStrength = (double)attackerStrength / (double)defenderStrength;
		
		// reactor
		
		if (!isReactorIgnored())
		{
			relativeStrength *= (double)attackerUnit->reactor_type / (double)defenderUnit->reactor_type;
		}
		
		// abilities
		
		if (attackerUnit->triad() == TRIAD_AIR && unit_has_ability(attackerUnitId, ABL_AIR_SUPERIORITY))
		{
			if (defenderUnit->triad() == TRIAD_AIR)
			{
				relativeStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_air_supr_vs_air);
			}
			else
			{
				relativeStrength *= getPercentageBonusMultiplier(-Rules->combat_penalty_air_supr_vs_ground);
			}
		}
		
		if (unit_has_ability(attackerUnitId, ABL_NERVE_GAS))
		{
			relativeStrength *= 1.5;
		}
		
	}
	
	// ability applied regardless of combat type
	
	if (attackerUnit->triad() == TRIAD_LAND && attackerUnit->speed() > 1 && unit_has_ability(defenderUnitId, ABL_COMM_JAMMER) && !unit_has_ability(attackerUnitId, ABL_DISSOCIATIVE_WAVE))
	{
		relativeStrength /= getPercentageBonusMultiplier(Rules->combat_comm_jammer_vs_mobile);
	}
	
	if (attackerUnit->triad() == TRIAD_AIR && unit_has_ability(defenderUnitId, ABL_AAA) && !unit_has_ability(attackerUnitId, ABL_DISSOCIATIVE_WAVE))
	{
		relativeStrength /= getPercentageBonusMultiplier(Rules->combat_AAA_bonus_vs_air);
	}
	
	if (unit_has_ability(attackerUnitId, ABL_SOPORIFIC_GAS) && !isNativeUnit(defenderUnitId))
	{
		relativeStrength *= 1.25;
	}
	
	return relativeStrength;
	
}

/*
Returns vehicle specific psi offense modifier to unit strength.
*/
double getVehiclePsiOffenseModifier(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	double psiOffenseModifier = 1.0;
	
	// morale modifier
	
	psiOffenseModifier *= getVehicleMoraleModifier(vehicleId, false);
	
	// faction offense
	
	psiOffenseModifier *= aiData.factionInfos[vehicle->faction_id].offenseMultiplier;
	
	// faction PLANET rating modifier
	
	psiOffenseModifier *= getFactionSEPlanetOffenseModifier(vehicle->faction_id);
	
	// return strength
	
	return psiOffenseModifier;
	
}

/*
Returns vehicle specific psi defense modifier to unit strength.
*/
double getVehiclePsiDefenseModifier(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	double psiDefenseModifier = 1.0;
	
	// morale modifier
	
	psiDefenseModifier *= getVehicleMoraleModifier(vehicleId, false);
	
	// faction defense
	
	psiDefenseModifier *= aiData.factionInfos[vehicle->faction_id].defenseMultiplier;
	
	// faction PLANET rating modifier
	
	psiDefenseModifier *= getFactionSEPlanetDefenseModifier(vehicle->faction_id);
	
	// return strength
	
	return psiDefenseModifier;
	
}

/*
Returns vehicle specific conventional offense modifier to unit strength.
*/
double getVehicleConventionalOffenseModifier(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// modifier

	double conventionalOffenseModifier = 1.0;
	
	// morale modifier

	conventionalOffenseModifier *= getVehicleMoraleModifier(vehicleId, false);

	// faction offense
	
	conventionalOffenseModifier *= aiData.factionInfos[vehicle->faction_id].offenseMultiplier;
	
	// fanatic bonus
	
	conventionalOffenseModifier *= aiData.factionInfos[vehicle->faction_id].fanaticBonusMultiplier;
	
	// return modifier
	
	return conventionalOffenseModifier;

}

/*
Returns vehicle specific conventional defense modifier to unit strength.
*/
double getVehicleConventionalDefenseModifier(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// modifier

	double conventionalDefenseModifier = 1.0;
	
	// morale modifier

	conventionalDefenseModifier *= getVehicleMoraleModifier(vehicleId, false);

	// faction defense
	
	conventionalDefenseModifier *= aiData.factionInfos[vehicle->faction_id].defenseMultiplier;
	
	// return modifier
	
	return conventionalDefenseModifier;

}

double getVehicleToBaseTravelTimeCoefficient(int vehicleId, int baseId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	int vehicleAssociation = getVehicleAssociation(vehicleId);
	BASE *base = &(Bases[baseId]);
	int baseAssociation = getAssociation(getBaseMapTile(baseId), base->faction_id);
	
	// vehicle distance to base
	
	int distance = map_range(base->x, base->y, vehicle->x, vehicle->y);
	
	// vehicle speed
	
	double speed;
	
	if (triad == TRIAD_LAND)
	{
		double minSpeed = 1.5 * (double)veh_speed_without_roads(vehicleId);
		double maxSpeed = (double)mod_veh_speed(vehicleId);
		speed = minSpeed + (maxSpeed - minSpeed) * ((double)std::min(250, std::max(0, *current_turn - 50)) / 250.0);
	}
	else
	{
		speed = (double)veh_speed_without_roads(vehicleId);
	}
	
	// time to reach the base
	
	double travelTime = (double)distance / speed;
	
	// increase effective time for land unit in different association
	
	if (triad == TRIAD_LAND && distance > 0 && vehicleAssociation != baseAssociation)
	{
		travelTime *= 2.0;
	}
	
	// travel time coefficient
	// reduce weight based on time to reach the base
	
	double travelTimeCoefficient = exp(- travelTime / 3.0);
	
	return travelTimeCoefficient;
	
}

double getVehicleToBaseDepthCoefficient(int vehicleId, int baseId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	UNIT *unit = &(Units[vehicle->unit_id]);
	BaseInfo *baseInfo = aiData.getBaseInfo(baseId);
	bool ocean = is_ocean(getBaseMapTile(baseId));
	
	double depthCoefficient = 1.0;
	
	switch (unit->chassis_type)
	{
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		{
			if (unit->speed() < baseInfo->enemyAirbaseDistance)
			{
				// enemy air unit cannot reach that deep into the territory
				depthCoefficient = 0.0;
			}
			
		}
		break;
		
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		{
			if (!ocean)
			{
				// enemy land unit will have hard time moving through our territory
				depthCoefficient = exp(- (double)baseInfo->borderBaseDistance / 3.0);
			}
			
		}
		break;
		
	case CHS_FOIL:
	case CHS_CRUISER:
		{
			if (ocean)
			{
				// enemy sea unit will have slightly hard time moving through our territory
				depthCoefficient = exp(- (double)baseInfo->borderBaseDistance / 10.0);
			}
			
		}
		break;
		
	}
	
	return depthCoefficient;
	
}

