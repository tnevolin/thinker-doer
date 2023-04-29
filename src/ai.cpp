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
#include "aiRoute.h"

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

	// disband oversupported vehicles
	
	disbandOrversupportedVehicles(factionId);
	
	// store base current production
	// it is overriden by vanilla code
	
	storeBaseSetProductionItems();
	
	// execute original code

	return faction_upkeep(factionId);

}

/*
Movement phase entry point.
*/
void __cdecl modified_enemy_units_check(int factionId)
{
	// set AI faction id for global reference
	
	aiFactionId = factionId;
	
	// run WTP AI code for AI enabled factions
	
	if (isWtpEnabledFaction(factionId))
	{
		// assign vehicles to transports
		
		assignVehiclesToTransports();
		
		// execute unit movement strategy
		
		profileFunction("1. strategy", strategy);
		
	}
	
	// execute original code
	
	executionProfiles["~ enemy_units_check"].start();
	enemy_units_check(factionId);
	executionProfiles["~ enemy_units_check"].stop();
	
}

void strategy()
{
	// design units
	
	designUnits();
	
	// populate data
	
	profileFunction("1.1. populateAIData", populateAIData);
	
	// precompute aiPath
	
	profileFunction("1.2. precomputeAIRouteData", precomputeAIRouteData);
	
	// delete units
	
	balanceVehicleSupport();
	disbandVehicles();
	
	// move strategy
	
	profileFunction("1.3. moveStrategy", moveStrategy);
	
	// compute production demands
	
	profileFunction("1.4. productionStrategy", productionStrategy);
	
	// execute tasks
	
	profileFunction("1.5. executeTasks", executeTasks);
	
}

void executeTasks()
{
	debug("Tasks - %s\n", MFactions[aiFactionId].noun_faction);
	for (std::pair<const int, Task> &taskEntry : aiData.tasks)
	{
		Task &task = taskEntry.second;
		int vehicleId = task.getVehicleId();
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// do not execute combat tasks immediatelly
		
		if (task.type == TT_MELEE_ATTACK || task.type == TT_LONG_RANGE_FIRE)
			continue;
		
		debug
		(
			"\t[%4d] (%3d,%3d)->(%3d,%3d)/(%3d,%3d) taskType=%2d, %s\n",
			vehicleId,
			vehicle->x,
			vehicle->y,
			getX(task.getDestination()),
			getY(task.getDestination()),
			getX(task.attackTarget),
			getY(task.attackTarget),
			task.type,
			Units[vehicle->unit_id].name
		)
		;
		
		task.execute();
		
	}
	
}

void populateAIData()
{
	// clear aiData
	
	aiData.clear();
	
	// global variables
	
	profileFunction("1.1.1. populateGlobalVariables", populateGlobalVariables);
	
	// geography data
	
	profileFunction("1.1.2. populateGeographyData", populateGeographyData);
	
	// faction data
	
	profileFunction("1.1.3. populateFactionData", populateFactionData);
	
	// movement data
	
	profileFunction("1.1.4. populateMovementData", populateMovementData);
	
	// base police request
	
	profileFunction("1.1.5. evaluateBasePoliceRequests", evaluateBasePoliceRequests);
	
	// base police request
	
	profileFunction("1.1.A. populateEnemyStacks", populateEnemyStacks);
	
	// base threat
	
	profileFunction("1.1.6. evaluateBaseDefense", evaluateBaseDefense);
	
}

/*
Analyzes and sets geographical parameters.
*/
void analyzeGeography()
{
	const bool TRACE = DEBUG && false;
	
	if (TRACE)
	{
		debug("analyzeGeography - %s\n", MFactions[aiFactionId].noun_faction);
	}
	
	// populate initial extended region associations
	
	executionProfiles["1.1.1.1. populate initial extended region associations"].start();
	
	std::map<int, int> defaultExtendedRegionAssociations;
	
	for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
	{
		int extededRegion = getExtendedRegion(tile);
		defaultExtendedRegionAssociations.insert({extededRegion, extededRegion});
		
	}
	
	executionProfiles["1.1.1.1. populate initial extended region associations"].stop();
	
	// get joined default associations
		
	executionProfiles["1.1.1.2. get joined default associations"].start();
	
	std::map<int, std::set<int>> defaultExtendedRegionAssociationJoins;
	
	for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
	{
		bool tileOcean = is_ocean(tile);
		int tileExtendedRegion = getExtendedRegion(tile);
		int tileDefalutAssociation = defaultExtendedRegionAssociations.at(tileExtendedRegion);
		
		// polar
		
		if (!isPolarRegion(tile))
			continue;
		
		for (MAP *adjacentTile : getAdjacentTiles(tile))
		{
			bool adjacentTileOcean = is_ocean(adjacentTile);
			int adjacentTileExtendedRegion = getExtendedRegion(adjacentTile);
			int adjacentTileDefalutAssociation = defaultExtendedRegionAssociations.at(adjacentTileExtendedRegion);
			
			// same realm
			
			if (adjacentTileOcean != tileOcean)
				continue;
			
			// different current associations
			
			if (adjacentTileDefalutAssociation == tileDefalutAssociation)
				continue;
			
			// add connection
			
			defaultExtendedRegionAssociationJoins[tileDefalutAssociation].insert(adjacentTileDefalutAssociation);
			defaultExtendedRegionAssociationJoins[adjacentTileDefalutAssociation].insert(tileDefalutAssociation);
			
		}
		
	}
	
	executionProfiles["1.1.1.2. get joined default associations"].stop();
	
	// join default extended region associations
	
	executionProfiles["1.1.1.3. join default extended region associations"].start();
	
	joinAssociations(defaultExtendedRegionAssociations, defaultExtendedRegionAssociationJoins);
	
	executionProfiles["1.1.1.3. join default extended region associations"].stop();
	
	if (TRACE)
	{
		debug("\tdefault extended region associations\n");
		for (std::pair<int const, int> &extendedRegionAssociationEntry : defaultExtendedRegionAssociations)
		{
			int region = extendedRegionAssociationEntry.first;
			int association = extendedRegionAssociationEntry.second;
			
			debug("\t\t%3d: %3d\n", region, association);
			
		}
		
	}
	
	// populate faction specific data
	
	executionProfiles["1.1.1.4. populate faction specific data"].start();
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		if (TRACE)
		{
			debug("\t%-25s\n", getMFaction(factionId)->noun_faction);
		}
		
		// associations
		
		executionProfiles["1.1.1.4.1. associations"].start();
		
		std::map<int, int> &extendedRegionAssociations = aiData.geography.factions[factionId].extendedRegionAssociations;
		
		extendedRegionAssociations.clear();
		extendedRegionAssociations.insert(defaultExtendedRegionAssociations.begin(), defaultExtendedRegionAssociations.end());
		
		executionProfiles["1.1.1.4.1. associations"].stop();
		
		// get oceanAssociationJoins
		
		executionProfiles["1.1.1.4.2. get oceanAssociationJoins"].start();
		
		std::map<int, std::set<int>> oceanAssociationJoins;
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			
			// friendly base
			
			if (!isFriendly(factionId, base->faction_id))
				continue;
			
			// land base
			
			if (is_ocean(baseTile))
				continue;
			
			// gather adjacent ocean associations
			
			std::set<int> adjacentOceanAssociations;
			
			for (MAP *adjacentTile : getAdjacentTiles(baseTile))
			{
				// ocean
				
				if (!is_ocean(adjacentTile))
					continue;
				
				// add ocean association
				
				int adjacentTileExtendedRegion = getExtendedRegion(adjacentTile);
				int adjacentTileAssociation = extendedRegionAssociations.at(adjacentTileExtendedRegion);
				adjacentOceanAssociations.insert(adjacentTileAssociation);
				
			}
			
			// multiple associations
			
			if (adjacentOceanAssociations.size() < 2)
				continue;
			
			// add connected associations
			
			for (int associationA : adjacentOceanAssociations)
			{
				for (int associationB : adjacentOceanAssociations)
				{
					// exclude same
					
					if (associationA == associationB)
						continue;
					
					// add connection
					
					oceanAssociationJoins[associationA].insert(associationB);
					
				}
				
			}
			
		}
		
		executionProfiles["1.1.1.4.2. get oceanAssociationJoins"].stop();
		
		// join ocean associations
		
		executionProfiles["1.1.1.4.3. join ocean associations"].start();
		
		joinAssociations(extendedRegionAssociations, oceanAssociationJoins);
		
		if (TRACE)
		{
			debug("\t\textended region associations\n");
			for (std::pair<int const, int> &extendedRegionAssociationEntry : extendedRegionAssociations)
			{
				int region = extendedRegionAssociationEntry.first;
				int association = extendedRegionAssociationEntry.second;
				
				debug("\t\t\t%3d: %3d\n", region, association);
				
			}
			
		}
		
		executionProfiles["1.1.1.4.3. join ocean associations"].stop();
		
		// coastalBaseOceanAssociations
		
		executionProfiles["1.1.1.4.4. coastalBaseOceanAssociations"].start();
		
		std::map<MAP *, int> &coastalBaseOceanAssociations = aiData.geography.factions[factionId].coastalBaseOceanAssociations;
		coastalBaseOceanAssociations.clear();
		
		executionProfiles["1.1.1.4.4. coastalBaseOceanAssociations"].stop();
		
		// connectedOceanAssociations
		
		executionProfiles["1.1.1.4.5. connectedOceanAssociations"].start();
		
		std::vector<std::set<int>> connectedOceanAssociations;
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			
			// friendly base
			
			if (!isFriendly(factionId, base->faction_id))
				continue;
			
			// land base
			
			if (is_ocean(baseTile))
				continue;
			
			// get ocean association
			
			int oceanAssociation = -1;
			
			for (MAP *adjacentTile : getAdjacentTiles(baseTile))
			{
				// ocean
				
				if (!is_ocean(adjacentTile))
					continue;
				
				// set ocean association
				
				int adjacentTileExtendedRegion = getExtendedRegion(adjacentTile);
				int adjacentTileAssociation = getExtendedRegionAssociation(adjacentTileExtendedRegion, factionId);
				oceanAssociation = adjacentTileAssociation;
				break;
				
			}
			
			if (oceanAssociation == -1)
				continue;
			
			// add coastal base
			
			coastalBaseOceanAssociations.insert({baseTile, oceanAssociation});
			
		}
		
		if (TRACE)
		{
			debug("\t\tcoastal base ocean associations\n");
			for (std::pair<MAP * const, int> &coastalBaseOceanAssociationEntry : coastalBaseOceanAssociations)
			{
				MAP *tile = coastalBaseOceanAssociationEntry.first;
				int association = coastalBaseOceanAssociationEntry.second;
				
				debug("\t\t\t(%3d,%3d): %3d\n", getX(tile), getY(tile), association);
				
			}
			
		}
		
		executionProfiles["1.1.1.4.5. connectedOceanAssociations"].stop();
		
		// populate connections
		
		executionProfiles["1.1.1.4.6. populate connections"].start();
		
		std::map<int, std::set<int>> &associationConnections = aiData.geography.factions[factionId].associationConnections;
		associationConnections.clear();
		
		for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
		{
			bool tileOcean = is_ocean(tile);
			int tileAssociation = getAssociation(tile, factionId);
			
			if (associationConnections.count(tileAssociation) == 0)
			{
				associationConnections.insert({tileAssociation, std::set<int>()});
			}
			
			for (unsigned int angle = 0; angle < TABLE_next_cell_count; angle++)
			{
				MAP *adjacentTile = getTileByAngle(tile, angle);
				
				if (adjacentTile == nullptr)
					continue;
				
				bool adjacentTileOcean = is_ocean(adjacentTile);
				int adjacentTileAssociation = getAssociation(adjacentTile, factionId);
				
				// different realm
				
				if (adjacentTileOcean == tileOcean)
					continue;
				
				// add connection
				
				associationConnections.at(tileAssociation).insert(adjacentTileAssociation);
				
			}
			
		}
		
		executionProfiles["1.1.1.4.6. populate connections"].stop();
		
		if (TRACE)
		{
			debug("\t\tassociation connections\n");
			for (std::pair<int const, std::set<int>> &associationConnectionEntry : associationConnections)
			{
				int association = associationConnectionEntry.first;
				std::set<int> connections = associationConnectionEntry.second;
				
				debug("\t\t\t%3d -", association);
				
				for (int connection : connections)
				{
					debug(" %3d", connection);
				}
				
				debug("\n");
				
			}
			
		}
		
	}
	
	executionProfiles["1.1.1.4. populate faction specific data"].stop();
	
	// associations and association areas
	
	executionProfiles["1.1.1.5. association areas"].start();
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		aiData.geography.factions[factionId].landAssociations.clear();
		aiData.geography.factions[factionId].oceanAssociations.clear();
		aiData.geography.factions[factionId].associationAreas.clear();
		
		for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
		{
			bool ocean = is_ocean(tile);
			int association = getAssociation(tile, factionId);
			
			if (!ocean)
			{
				aiData.geography.factions[factionId].landAssociations.insert(association);
			}
			else
			{
				aiData.geography.factions[factionId].oceanAssociations.insert(association);
			}
			
			aiData.geography.factions[factionId].associationAreas[association]++;
			
		}
		
	}
	
	executionProfiles["1.1.1.5. association areas"].stop();
	
}

void populateGlobalVariables()
{
	aiData.developmentScale = getDevelopmentScale();
	aiData.maxBaseSize = getMaxBaseSize(aiFactionId);
	aiData.bestOffenseValue = getFactionBestOffenseValue(aiFactionId);
	aiData.bestDefenseValue = getFactionBestDefenseValue(aiFactionId);
	
}

void populateGeographyData()
{
	profileFunction("1.1.2.1. analyzeGeography", analyzeGeography);
	profileFunction("1.1.2.2. populatePathInfoParameters", populatePathInfoParameters);
	profileFunction("1.1.2.3. populateBaseBorderDistances", populateBaseBorderDistances);
	profileFunction("1.1.2.4. populateFactionAirbases", populateFactionAirbases);
	profileFunction("1.1.2.5. populateNetworkCoverage", populateNetworkCoverage);
//	profileFunction("1.1.2.6. populateEnemyMovementInfos", populateEnemyMovementInfos);
	profileFunction("1.1.2.7. populateMovementInfos", populateMovementInfos);
	profileFunction("1.1.2.8. populateTransfers", populateTransfers);
	profileFunction("1.1.2.9. populateLandClusters", populateLandClusters);
	profileFunction("1.1.2.B. populateEnemyOffenseThreat", populateEnemyOffenseThreat);
	profileFunction("1.1.2.C. populateEmptyEnemyBaseTiles", populateEmptyEnemyBaseTiles);
	profileFunction("1.1.2.D. populateEnemyBaseOceanAssociations", populateEnemyBaseOceanAssociations);
	profileFunction("1.1.2.E. populateShipyardsAndSeaTransports", populateShipyardsAndSeaTransports);
	profileFunction("1.1.2.F. populateReachableAssociations", populateReachableAssociations);
	profileFunction("1.1.2.G. populateWarzones", populateWarzones);
	
}

/*
Populates faction data dependent on geography data.
*/
void populateFactionData()
{
	aiData.netIncome = getFactionNetIncome(aiFactionId);
	aiData.grossIncome = getFactionGrossIncome(aiFactionId);
	
	profileFunction("1.1.3.1. populateFactionInfos", populateFactionInfos);
	profileFunction("1.1.3.2. populateBases", populateBases);
	profileFunction("1.1.3.3. populateOurBases", populateOurBases);
	profileFunction("1.1.3.4. populateMaxMineralSurplus", populateMaxMineralSurplus);
	profileFunction("1.1.3.5. populateFactionCombatModifiers", populateFactionCombatModifiers);
	profileFunction("1.1.3.6. populateUnits", populateUnits);
	profileFunction("1.1.3.7. populateVehicles", populateVehicles);
	
}

/*
Populates movement data dependent on both geography and faction data.
*/
void populateMovementData()
{
//	profileFunction("1.1.4.1. populateBaseEnemyMovementCosts", populateBaseEnemyMovementCosts);
	
}

void populateFactionInfos()
{
	// enemy count

	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		aiData.factionInfos[factionId].enemyCount = 0;

		for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
		{
			// others

			if (otherFactionId == factionId)
				continue;

			if (isVendetta(factionId, otherFactionId))
			{
				aiData.factionInfos[factionId].enemyCount++;
			}

		}

	}

}

void populateBases()
{
	aiData.locationBaseIds.clear();
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		
		// store base location
		
		aiData.locationBaseIds.insert({baseTile, baseId});
		
		// set base garrison
		
		baseInfo.garrison = getBaseGarrison(baseId);
		
	}
	
}

void populateOurBases()
{
	aiData.baseIds.clear();
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		
		// our
		
		if (base->faction_id != aiFactionId)
			continue;
		
		// add base to our baseIds
		
		aiData.baseIds.push_back(baseId);
		
	}
	
}

void populateBaseBorderDistances()
{
	debug("populateBaseBorderDistances - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	std::set<int> unfriendlyOceanAssociations;
	std::set<MAP *> friendlyControlledLocations;
	
	// unfriendlyOceanAssociations
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = getBase(baseId);
		int baseOceanAssociation = getBaseOceanAssociation(baseId);
		
		// unfriendly
		
		if (isFriendly(aiFactionId, base->faction_id))
			continue;
		
		// ocean connected
		
		if (baseOceanAssociation == -1)
			continue;
		
		// update unfriendly ocean associations
		
		unfriendlyOceanAssociations.insert(baseOceanAssociation);
		
	}
	
	// friendlyControlledLocations
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// friendly
		
		if (!isFriendly(aiFactionId, base->faction_id))
			continue;
		
		// add base tile to friendlyControlledLocations
		
		friendlyControlledLocations.insert(baseTile);
		
		// add land tile within 3 squares around friendly bases
		
		for (MAP *tile : getRangeTiles(baseTile, 3, false))
		{
			bool tileOcean = is_ocean(tile);
			
			// land
			
			if (tileOcean)
				continue;
			
			// add tile
			
			friendlyControlledLocations.insert(tile);
			
		}
		
	}
	
	// base 
		
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		
		baseInfo.borderDistance = -1;
		
		for (int ringRange = 3; ringRange <= 20; ringRange++)
		{
			for (MAP *tile : getEqualRangeTiles(baseTile, ringRange))
			{
				bool tileOcean = is_ocean(tile);
				int tileOceanAssociation = getOceanAssociation(tile, aiFactionId);
				
				// exclude friendly oceans
				
				if (tileOcean && unfriendlyOceanAssociations.count(tileOceanAssociation) == 0)
					continue;
				
				// exclude controlled locations
				
				if (friendlyControlledLocations.count(tile) != 0)
					continue;
				
				// store border distance
				
				baseInfo.borderDistance = ringRange - 3;
				break;
				
			}
			
			if (baseInfo.borderDistance != -1)
				break;
			
		}
		
		debug("\t%-25s borderDistance=%2d\n", getBase(baseId)->name, baseInfo.borderDistance);
		
	}
	
}

void populateFactionCombatModifiers()
{
	debug("factionCombatModifiers - %-24s\n", MFactions[aiFactionId].noun_faction);
	
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
	
	for (int factionId = 0; factionId < 8; factionId++)
	{
		// skip AI
		
		if (factionId == aiFactionId)
			continue;
		
		// get relation from other faction
		
		int otherFactionRelation = Factions[factionId].diplo_status[aiFactionId];
		
		// calculate threat koefficient
		
		double threatCoefficient;
		
		if (factionId == 0 || otherFactionRelation & DIPLO_VENDETTA)
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
		
		if (is_human(factionId))
		{
			threatCoefficient += conf.ai_production_threat_coefficient_human;
		}
		
		// store threat koefficient
		
		aiData.factionInfos[factionId].threatCoefficient = threatCoefficient;
		
		debug("\t%-24s: %08x => %4.2f\n", MFactions[factionId].noun_faction, otherFactionRelation, threatCoefficient);
		
	}
	
	// populate factions best combat item values
	
	aiData.bestOffenseValue = 0;
	aiData.bestDefenseValue = 0;
	for (int factionId = 1; factionId < 8; factionId++)
	{
		aiData.factionInfos[factionId].bestOffenseValue = getFactionBestOffenseValue(factionId);
		aiData.factionInfos[factionId].bestDefenseValue = getFactionBestDefenseValue(factionId);
		
		aiData.bestOffenseValue = std::max(aiData.bestOffenseValue, aiData.factionInfos[factionId].bestOffenseValue);
		aiData.bestDefenseValue = std::max(aiData.bestDefenseValue, aiData.factionInfos[factionId].bestDefenseValue);
		
	}
	
}

void populateUnits()
{
	debug("populateUnits - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	aiData.unitIds.clear();
	aiData.prototypeUnitIds.clear();
	aiData.colonyUnitIds.clear();
	aiData.landColonyUnitIds.clear();
	aiData.seaColonyUnitIds.clear();
	aiData.airColonyUnitIds.clear();
	aiData.combatUnitIds.clear();
	aiData.landCombatUnitIds.clear();
	aiData.landAndAirCombatUnitIds.clear();
	aiData.seaCombatUnitIds.clear();
	aiData.seaAndAirCombatUnitIds.clear();
	aiData.airCombatUnitIds.clear();
	
    for (int unitId : getFactionUnitIds(aiFactionId, false, true))
	{
		UNIT *unit = &(Units[unitId]);
		int unitOffenseValue = getUnitOffenseValue(unitId);
		int unitDefenseValue = getUnitDefenseValue(unitId);
		
		debug("\t[%3d] %-32s\n", unitId, unit->name);
		
		// add unit
		
		aiData.unitIds.push_back(unitId);
		
		// add prototype units
		
		if (isPrototypeUnit(unitId))
		{
			aiData.prototypeUnitIds.push_back(unitId);
			debug("\t\tprototype\n");
		}
		
		// add colony units
		
		if (isColonyUnit(unitId))
		{
			debug("\t\tcolony\n");
			
			aiData.colonyUnitIds.push_back(unitId);
			
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
				(unitOffenseValue < 0 || unitDefenseValue < 0)
				||
				(has_ability(aiFactionId, ABL_ID_TRANCE) ? isUnitHasAbility(unitId, ABL_TRANCE) : unitOffenseValue == 1 && unitDefenseValue == 1)
				||
				(has_ability(aiFactionId, ABL_ID_EMPATH) ? isUnitHasAbility(unitId, ABL_EMPATH) : unitOffenseValue == 1 && unitDefenseValue == 1)
				||
				(unitOffenseValue >= aiData.factionInfos[aiFactionId].bestOffenseValue)
				||
				(unitDefenseValue >= aiData.factionInfos[aiFactionId].bestDefenseValue)
			)
		)
		{
			debug("\t\tcombat\n");
			
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
	
}

void populateMaxMineralSurplus()
{
	aiData.maxMineralSurplus = 1;
	aiData.oceanAssociationMaxMineralSurpluses.clear();

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		int baseOceanAssociation = getBaseOceanAssociation(baseId);

		aiData.maxMineralSurplus = std::max(aiData.maxMineralSurplus, base->mineral_surplus);

		if (baseOceanAssociation != -1)
		{
			if
			(
				aiData.oceanAssociationMaxMineralSurpluses.count(baseOceanAssociation) == 0
				||
				base->mineral_surplus > aiData.oceanAssociationMaxMineralSurpluses.at(baseOceanAssociation)
			)
			{
				aiData.oceanAssociationMaxMineralSurpluses[baseOceanAssociation] = base->mineral_surplus;
			}

		}

	}

}

void populateFactionAirbases()
{
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

}

void populateVehicles()
{
	debug("populate vehicles - %s\n", MFactions[aiFactionId].noun_faction);

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int vehicleTileLandAssociation = getLandAssociation(vehicleTile, aiFactionId);
		int vehicleTileOceanAssociation = getOceanAssociation(vehicleTile, aiFactionId);

		// store all vehicle current id in pad_0 field

		vehicle->pad_0 = vehicleId;

		// further process only own vehicles

		if (vehicle->faction_id != aiFactionId)
			continue;

		debug("\t[%4d] (%3d,%3d) region = %3d\n", vehicleId, vehicle->x, vehicle->y, vehicleTile->region);

		// add vehicle

		aiData.vehicleIds.push_back(vehicleId);

		// combat vehicles

		if (isCombatVehicle(vehicleId))
		{
			// exclude paratroopers - I do not know what to do with them yet
			// TODO think about how to move paratroopers
			
			if (isVehicleHasAbility(vehicleId, ABL_DROP_POD))
				continue;
			
			// add vehicle to global list

			aiData.combatVehicleIds.push_back(vehicleId);

			// add vehicle unit to global list

			if (std::find(aiData.activeCombatUnitIds.begin(), aiData.activeCombatUnitIds.end(), vehicle->unit_id) == aiData.activeCombatUnitIds.end())
			{
				aiData.activeCombatUnitIds.push_back(vehicle->unit_id);
			}

			// scout and native

			if (isScoutVehicle(vehicleId) || isNativeVehicle(vehicleId))
			{
				aiData.scoutVehicleIds.push_back(vehicleId);
			}

			// add surface vehicle to region list
			// except land unit in ocean

			if (vehicle->triad() != TRIAD_AIR && !(vehicle->triad() == TRIAD_LAND && isOceanRegion(vehicleTile->region)))
			{
				if (aiData.regionSurfaceCombatVehicleIds.count(vehicleTile->region) == 0)
				{
					aiData.regionSurfaceCombatVehicleIds[vehicleTile->region] = std::vector<int>();
				}
				aiData.regionSurfaceCombatVehicleIds[vehicleTile->region].push_back(vehicleId);

				// add scout to region list

				if (isScoutVehicle(vehicleId))
				{
					if (aiData.regionSurfaceScoutVehicleIds.count(vehicleTile->region) == 0)
					{
						aiData.regionSurfaceScoutVehicleIds[vehicleTile->region] = std::vector<int>();
					}
					aiData.regionSurfaceScoutVehicleIds[vehicleTile->region].push_back(vehicleId);
				}

			}

			// find if vehicle is at base
			
			if (!isFactionBaseAt(vehicleTile, aiFactionId))
			{
				// add outside vehicle
				
				aiData.outsideCombatVehicleIds.push_back(vehicleId);
				
			}
			
		}
		else if (isColonyVehicle(vehicleId))
		{
			aiData.colonyVehicleIds.push_back(vehicleId);
		}
		else if (isFormerVehicle(vehicleId))
		{
			aiData.formerVehicleIds.push_back(vehicleId);
			switch (triad)
			{
			case TRIAD_AIR:
				aiData.airFormerVehicleIds.push_back(vehicleId);
				break;
			case TRIAD_LAND:
				aiData.landFormerVehicleIds[vehicleTileLandAssociation].push_back(vehicleId);
				break;
			case TRIAD_SEA:
				aiData.seaFormerVehicleIds[vehicleTileOceanAssociation].push_back(vehicleId);
				break;
			}
		}
		else if (isSeaTransportVehicle(vehicleId))
		{
			aiData.seaTransportVehicleIds.push_back(vehicleId);
		}

	}

}

void populateNetworkCoverage()
{
	int roadCount = 0;
	int tubeCount = 0;
	for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
	{
		if (map_has_item(tile, TERRA_MAGTUBE))
		{
			roadCount++;
			tubeCount++;
		}
		else if (map_has_item(tile, TERRA_ROAD))
		{
			roadCount++;
		}

	}

	aiData.roadCoverage = (double)roadCount / (double)*map_area_tiles;
	aiData.tubeCoverage = (double)tubeCount / (double)*map_area_tiles;

}

void populateShipyardsAndSeaTransports()
{
	debug("populateShipyardsAndSeaTransports\n");
	
	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		// shipyards
		
		std::map<int, std::vector<int>> &oceanAssociationShipyards = aiData.geography.factions[factionId].oceanAssociationShipyards;
		oceanAssociationShipyards.clear();
		
		if (has_tech(factionId, getChassis(CHS_FOIL)->preq_tech))
		{
			for (int baseId = 0; baseId < *total_num_bases; baseId++)
			{
				BASE *base = &(Bases[baseId]);
				int baseOceanAssociation = getBaseOceanAssociation(baseId);
				
				// faction base
				
				if (base->faction_id != factionId)
					continue;
				
				// ocean associated base
				
				if (baseOceanAssociation == -1)
					continue;
				
				// add shipyard
				
				oceanAssociationShipyards[baseOceanAssociation].push_back(baseId);

			}
			
		}
		
		// sea transports
		
		std::map<int, std::vector<int>> &oceanAssociationSeaTransports = aiData.geography.factions[factionId].oceanAssociationSeaTransports;
		oceanAssociationSeaTransports.clear();
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			int vehicleAssociation = getVehicleAssociation(vehicleId);
			
			// faction vehicle
			
			if (vehicle->faction_id != factionId)
				continue;
			
			// sea transport
			
			if (!isSeaTransportVehicle(vehicleId))
				continue;
			
			// add sea transport
			
			oceanAssociationSeaTransports[vehicleAssociation].push_back(vehicleId);
			
		}
		
		debug("\t%-32s\n", getMFaction(factionId)->noun_faction);
		if (DEBUG)
		{
			for (std::pair<const int, std::vector<int>> &oceanAssociationShipyardEntry : oceanAssociationShipyards)
			{
				int oceanAssociation = oceanAssociationShipyardEntry.first;
				std::vector<int> &shipyards = oceanAssociationShipyardEntry.second;

				debug("\t\t%3d %3d\n", oceanAssociation, shipyards.size());

			}

			for (std::pair<const int, std::vector<int>> &oceanAssociationSeaTransportEntry : oceanAssociationSeaTransports)
			{
				int oceanAssociation = oceanAssociationSeaTransportEntry.first;
				std::vector<int> &seaTransports = oceanAssociationSeaTransportEntry.second;

				debug("\t\t%3d %3d\n", oceanAssociation, seaTransports.size());

			}

		}

	}

}

void populateReachableAssociations()
{
	debug("populateReachableAssociations\n");
	
	// populate faction reachable associations
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		debug("\t%s\n", getMFaction(factionId)->noun_faction);
		
		std::map<int, std::vector<int>> &oceanAssociationSeaTransports = aiData.geography.factions[factionId].oceanAssociationSeaTransports;
		std::map<int, std::vector<int>> &oceanAssociationShipyards = aiData.geography.factions[factionId].oceanAssociationShipyards;
		
		// immediatelly reachable
		
		std::set<int> &immediatelyReachableAssociations = aiData.geography.factions[factionId].immediatelyReachableAssociations;
		immediatelyReachableAssociations.clear();
		
		for (std::pair<int const, std::vector<int>> &oceanAssociationSeaTransportEntry : oceanAssociationSeaTransports)
		{
			int association = oceanAssociationSeaTransportEntry.first;
			int seaTransportCount = oceanAssociationSeaTransportEntry.second.size();
			
			if (seaTransportCount == 0)
				continue;
			
			immediatelyReachableAssociations.insert(association);
			
			for (int connection : getAssociationConnections(association, factionId))
			{
				immediatelyReachableAssociations.insert(connection);
			}
			
		}
		
		// potentialy reachable
		
		std::set<int> &potentiallyReachableAssociations = aiData.geography.factions[factionId].potentiallyReachableAssociations;
		potentiallyReachableAssociations.clear();
		
		potentiallyReachableAssociations.insert(immediatelyReachableAssociations.begin(), immediatelyReachableAssociations.end());

		for (std::pair<int const, std::vector<int>> &oceanAssociationShipyardEntry : oceanAssociationShipyards)
		{
			int association = oceanAssociationShipyardEntry.first;
			int shipyardCount = oceanAssociationShipyardEntry.second.size();
			
			if (shipyardCount == 0)
				continue;
			
			potentiallyReachableAssociations.insert(association);
			
			for (int connection : getAssociationConnections(association, factionId))
			{
				potentiallyReachableAssociations.insert(connection);
			}
			
		}
		
		// all base associations are immediatelly and potentially reachable
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = &(Bases[baseId]);
			MAP *baseTile = getBaseMapTile(baseId);
			
			// faction base
			
			if (base->faction_id != factionId)
				continue;
			
			// get base association
			
			int baseAssociation = getAssociation(baseTile, factionId);
			
			// set base association as reachable
			
			immediatelyReachableAssociations.insert(baseAssociation);
			potentiallyReachableAssociations.insert(baseAssociation);
			
		}
		
		if (DEBUG)
		{
			debug("\t\timmediately reachable\n");
			
			for (int association : immediatelyReachableAssociations)
			{
				debug("\t\t\t%3d\n", association);
			}
			
			debug("\t\tpotentially reachable\n");
			
			for (int association : potentiallyReachableAssociations)
			{
				debug("\t\t\t%3d\n", association);
			}
			
		}
		
	}
	
}

/*
Populates locations where empty base can be captured.
*/
void populateWarzones()
{
	debug("populateWarzones - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// enemy vehicle reachable locations
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		UNIT *unit = getUnit(vehicle->unit_id);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		bool vehicleTileOcean = is_ocean(vehicleTile);
		
		// hostile
		
		if (!isVendetta(aiFactionId, vehicle->faction_id))
			continue;
		
		// combat
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		// exclude land unit in ocean
		
		if (triad == TRIAD_LAND && vehicleTileOcean)
			continue;
		
		// able to capture base
		
		bool baseCapture;
		
		switch (unit->chassis_type)
		{
		case CHS_NEEDLEJET:
		case CHS_COPTER:
		case CHS_MISSILE:
			baseCapture = false;
			break;
		default:
			baseCapture = true;
		}
		
		// psi and conventional offense
		
		int offenseValue = getVehicleOffenseValue(vehicleId);
		
		// get threatened tiles
		
		std::vector<MAP *> threatenedTiles = getVehicleThreatenedTiles(vehicleId);
		
		// set warzone
		
		for (MAP *tile : threatenedTiles)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			tileInfo.warzone = true;
			
			tileInfo.warzoneBaseCapture = baseCapture;
			
			if (offenseValue < 0)
			{
				tileInfo.warzonePsi = true;
			}
			else
			{
				tileInfo.warzoneConventionalOffenseValue = std::max(tileInfo.warzoneConventionalOffenseValue, offenseValue);
			}
			
		}
		
	}
	
	if (DEBUG)
	{
		for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
		{
			if (aiData.getTileInfo(tile).warzone)
			{
				debug("\t(%3d,%3d)\n", getX(tile), getY(tile));
			}
		}
	}
	
	// friendly base is not warzone
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// friendly
		
		if (!isFriendly(aiFactionId, base->faction_id))
			continue;
		
		// clear warzone
		
		aiData.getTileInfo(baseTile).warzone = false;
		
	}
	
}

/*
Populates locations where empty base can be captured.
*/
void populateBaseEnemyMovementCosts()
{
	bool TRACE = DEBUG && false;
	
	debug("populateBaseEnemyMovementCosts - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		// not our
		
		if (factionId == aiFactionId)
			continue;
		
		// not friendly
		
		if (isFriendly(aiFactionId, factionId))
			continue;
		
		if (TRACE)
		{
			debug("\t%s\n", getMFaction(factionId)->noun_faction);
		}
		
		// assumed vehicle speeds
		
		int landCombatSpeed = getChassis(CHS_INFANTRY)->speed * Rules->mov_rate_along_roads;
		int seaCombatSpeed = (factionId == 0 ? getChassis(CHS_INFANTRY)->speed * Rules->mov_rate_along_roads : getBestSeaCombatSpeed(factionId));
		int seaTransportSpeed = (factionId == 0 ? 0 : getBestSeaTransportSpeed(factionId));
		
		// prevalent movement type
		
		MOVEMENT_TYPE prevalentSeaMovementType = (factionId == 0 || has_project(factionId, FAC_XENOEMPATHY_DOME) ? MT_SEA_NATIVE : MT_SEA_REGULAR);
		MOVEMENT_TYPE prevalentLandMovementType = (factionId == 0 || has_project(factionId, FAC_XENOEMPATHY_DOME) ? MT_LAND_NATIVE : MT_LAND_REGULAR);
		
		// iterate bases
		
		for (int baseId : aiData.baseIds)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			bool baseTileOcean = is_ocean(baseTile);
			int baseOceanAssociation = getBaseOceanAssociation(baseId);
			TileMovementInfo &baseTileMovementInfo = aiData.getTileMovementInfo(baseTile, factionId);
			
			// cannot reach ocean base without sea unit
			
			if (baseTileOcean && seaCombatSpeed == 0)
				continue;
			
			if (TRACE)
			{
				debug("\t\t%-25s\n", getBase(baseId)->name);
			}
			
			std::map<MAP *, double> &estimatedTravelTimes = baseTileMovementInfo.estimatedTravelTimes;
			std::vector<MAP *> openNodes;
			
			estimatedTravelTimes.insert({baseTile, 0.0});
			openNodes.push_back(baseTile);
			
			while (openNodes.size() > 0)
			{
				std::vector<MAP *> newOpenNodes;
				
				for (MAP *tile : openNodes)
				{
					bool tileOcean = is_ocean(tile);
					TileMovementInfo &tileMovementInfo = aiData.getTileMovementInfo(tile, factionId);
					double tileTravelTime = estimatedTravelTimes.at(tile);
					
					// exit condition
					
					if (tileTravelTime > BASE_TRAVEL_TIME_PRECOMPUTATION_LIMIT)
						continue;
					
					// get tile impediment
					
					int baseRange = getRange(baseTile, tile);
					double baseTravelImpediment = (isFriendly(aiFactionId, factionId) ? 0.0 : getBaseTravelImpediment(baseRange, baseTileOcean));
					double travelImpediment = tileMovementInfo.travelImpediment - baseTravelImpediment;
					
					// iterate adjacent tiles
					
					for (int angle = 0; angle < ANGLE_COUNT; angle++)
					{
						MAP *adjacentTile = getTileByAngle(tile, angle);
						
						if (adjacentTile == nullptr)
							continue;
						
						bool adjacentTileOcean = is_ocean(adjacentTile);
						TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
						
						// different movement types
						
						if (!baseTileOcean) // land base
						{
							if (!adjacentTileOcean && !tileOcean) // land unit
							{
								// hexCost
								
								int hexCost = adjacentTileInfo.hexCosts[(angle + 4) % ANGLE_COUNT][prevalentLandMovementType];
								double travelTime = (double)hexCost / (double)landCombatSpeed + travelImpediment;
								double adjacentTileTravelTime = tileTravelTime + travelTime;
								
								// update travelTime
								
								if (estimatedTravelTimes.count(adjacentTile) == 0 || adjacentTileTravelTime < estimatedTravelTimes.at(adjacentTile))
								{
									estimatedTravelTimes[adjacentTile] = adjacentTileTravelTime;
									newOpenNodes.push_back(adjacentTile);
								}
								
							}
							else if (adjacentTileOcean && tileOcean) // land unit transported
							{
								// no sea transport
								
								if (seaTransportSpeed == 0)
									continue;
								
								// hexCost
								
								int hexCost = adjacentTileInfo.hexCosts[(angle + 4) % ANGLE_COUNT][prevalentSeaMovementType];
								double travelTime = (double)hexCost / (double)seaTransportSpeed;
								double adjacentTileTravelTime = tileTravelTime + travelTime;
								
								// update travelTime
								
								if (estimatedTravelTimes.count(adjacentTile) == 0 || adjacentTileTravelTime < estimatedTravelTimes.at(adjacentTile))
								{
									estimatedTravelTimes[adjacentTile] = adjacentTileTravelTime;
									newOpenNodes.push_back(adjacentTile);
								}
								
							}
							else if (!adjacentTileOcean && tileOcean) // land unit boarding
							{
								// no sea transport
								
								if (seaTransportSpeed == 0)
									continue;
								
								// boarding wait
								
								double travelTime = 10.0;
								double adjacentTileTravelTime = tileTravelTime + travelTime;
								
								// update travelTime
								
								if (estimatedTravelTimes.count(adjacentTile) == 0 || adjacentTileTravelTime < estimatedTravelTimes.at(adjacentTile))
								{
									estimatedTravelTimes[adjacentTile] = adjacentTileTravelTime;
									newOpenNodes.push_back(adjacentTile);
								}
								
							}
							else if (adjacentTileOcean && !tileOcean) // land unit unboarding
							{
								// no sea transport
								
								if (seaTransportSpeed == 0)
									continue;
								
								// unboarding
								
								double travelTime = 1.0;
								double adjacentTileTravelTime = tileTravelTime + travelTime;
								
								// update travelTime
								
								if (estimatedTravelTimes.count(adjacentTile) == 0 || adjacentTileTravelTime < estimatedTravelTimes.at(adjacentTile))
								{
									estimatedTravelTimes[adjacentTile] = adjacentTileTravelTime;
									newOpenNodes.push_back(adjacentTile);
								}
								
							}
							
						}
						else // ocean base
						{
							int adjacentTileOceanAssociation = getOceanAssociation(adjacentTile, factionId);
							
							if (baseOceanAssociation == adjacentTileOceanAssociation) // same ocean
							{
								// hexCost
								
								int hexCost = adjacentTileInfo.hexCosts[(angle + 4) % ANGLE_COUNT][prevalentSeaMovementType];
								double travelTime = (double)hexCost / (double)seaCombatSpeed;
								double adjacentTileTravelTime = tileTravelTime + travelTime;
								
								// update travelTime
								
								if (estimatedTravelTimes.count(adjacentTile) == 0 || adjacentTileTravelTime < estimatedTravelTimes.at(adjacentTile))
								{
									estimatedTravelTimes[adjacentTile] = adjacentTileTravelTime;
									newOpenNodes.push_back(adjacentTile);
								}
								
							}
							
						}
						
					}
					
				}
				
				// swap open nodes
				
				openNodes.clear();
				openNodes.swap(newOpenNodes);
				
			}
			
			if (TRACE)
			{
				for (std::pair<MAP *, double> estimatedTravelTimeEntry : estimatedTravelTimes)
				{
					MAP *tile = estimatedTravelTimeEntry.first;
					double estimatedTravelTime = estimatedTravelTimeEntry.second;
					
					debug("\t\t\t(%3d,%3d) %5.2f\n", getX(tile), getY(tile), estimatedTravelTime);
					
				}
				
			}
			
		}
		
	}
	
}

void populateEnemyStacks()
{
	debug("populateEnemyStacks - %s\n", MFactions[aiFactionId].noun_faction);
	
	// add enemy vehicles to stacks
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// not ours
		
		if (vehicle->faction_id == aiFactionId)
			continue;
		
		// exclude protected artifact or artifact in base
		
		bool unprotectedArtifact = false;
		
		if (vehicle->unit_id == BSC_ALIEN_ARTIFACT)
		{
			// ignore artifact in base
			
			if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
				continue;
			
			// check artifact stack to determine whether it is protected
			
			bool protectedArtifact = false;
			
			for (int stackVehicleId : getStackVehicles(vehicleId))
			{
				VEH *stackVehicle = getVehicle(stackVehicleId);
				
				// ignore artifact
				
				if (stackVehicle->unit_id == BSC_ALIEN_ARTIFACT)
					continue;
				
				// stack is protected
				
				protectedArtifact = true;
				break;
				
			}
			
			if (protectedArtifact)
				continue;
			
			// this vehicle is unprotected artifact
			
			unprotectedArtifact = true;
			
		}
		
		// unprotectedArtifact or at war
		
		if (!(unprotectedArtifact || isVendetta(aiFactionId, vehicle->faction_id)))
			continue;
		
		// exclude land vehicle on transport
		
		if (isVehicleOnTransport(vehicleId))
			continue;
		
		// ignore alien too far from base
		
		if (vehicle->faction_id == 0)
		{
			if (getNearestBaseRange(vehicleTile, aiData.baseIds, true) > 4)
				continue;
		}
		
		// add enemy vehicle
		
		aiData.enemyStacks[vehicleTile].addVehicle(vehicleId);
		
	}
	
	// remove empty stacks (just for safety)
	
	std::vector<MAP *> emptyEnemyStacks;
	
	for (std::pair<MAP * const, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		MAP *enemyStackTile = enemyStackEntry.first;
		EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
		if (enemyStackInfo.vehicleIds.size() == 0)
		{
			emptyEnemyStacks.push_back(enemyStackTile);
		}
		
	}
		
	for (MAP *stackTile : emptyEnemyStacks)
	{
		aiData.enemyStacks.erase(stackTile);
	}
		
	// populate enemy stacks secondary values
	
	debug("\tpopulate enemy stacks secondary values\n");
	
	for (std::pair<MAP * const, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		MAP *enemyStackTile = enemyStackEntry.first;
		EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
		debug("\t\t(%3d,%3d)\n", getX(enemyStackTile), getY(enemyStackTile));
		
		// populate baseRange
		
		enemyStackInfo.baseRange = getNearestBaseRange(enemyStackTile, aiData.baseIds, false);
		
		// populate base/bunker/airbase
		
		enemyStackInfo.base = isBaseAt(enemyStackTile);
		enemyStackInfo.baseOrBunker = isBaseAt(enemyStackTile) || isBunkerAt(enemyStackTile);
		enemyStackInfo.airbase = isAirbaseAt(enemyStackTile);
		
		// bombardmentDestructive
		
		if (!enemyStackInfo.baseOrBunker)
		{
			for (int vehicleId : enemyStackInfo.bombardmentVehicleIds)
			{
				VEH *vehicle = getVehicle(vehicleId);
				int triad = vehicle->triad();
				
				if (triad == TRIAD_SEA || triad == TRIAD_AIR && enemyStackInfo.airbase)
				{
					enemyStackInfo.bombardmentDestructive = true;
					break;
				}
				
			}
			
		}
		
	}
	
	// track closest to our bases stacks only
	
	std::vector<IdIntValue> stackTileRanges;
	
	for (const std::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		const MAP *enemyStackLocation = enemyStackEntry.first;
		const EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
		stackTileRanges.push_back({(int)enemyStackLocation, enemyStackInfo.baseRange});
		
	}
	
	std::sort(stackTileRanges.begin(), stackTileRanges.end(), compareIdIntValueAscending);
	
	for (unsigned int i = 0; i < stackTileRanges.size(); i++)
	{
		IdIntValue stackTileRange = stackTileRanges.at(i);
		MAP *enemyStackLocation = (MAP *)stackTileRange.id;
		
		if (i < STACK_MAX_COUNT || stackTileRange.value <= STACK_MAX_BASE_RANGE)
			continue;
		
		aiData.enemyStacks.erase(enemyStackLocation);
		
	}
	
	// set priority
	
	debug("\tpriority\n");
	
	for (std::pair<MAP * const, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
		double totalPriority = 0.0;
		
		for (int vehicleId : enemyStackInfo.vehicleIds)
		{
			double priority = getVehicleDestructionPriority(vehicleId);
			totalPriority += priority;
		}
		
		enemyStackInfo.priority = totalPriority / (double)enemyStackInfo.vehicleIds.size();
		
		debug
		(
			"\t\t(%3d,%3d)"
			" priority=%5.2f"
			"\n"
			, getX(enemyStackEntry.first), getY(enemyStackEntry.first)
			, enemyStackInfo.priority
		);
		
	}
	
}

/*
Populates completely empty enemy bases.
They are not part of enemy stacks. So need to be handled separately.
*/
void populateEmptyEnemyBaseTiles()
{
	debug("populateEmptyEnemyBaseTiles - %s\n", MFactions[aiFactionId].noun_faction);
	
	aiData.emptyEnemyBaseTiles.clear();
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// not ours
		
		if (base->faction_id == aiFactionId)
			continue;
		
		// unfriendly
		
		if (isFriendly(aiFactionId, base->faction_id))
			continue;
		
		// empty
		
		if (isVehicleAt(baseTile))
			continue;
		
		// store unprotected enemy base
		
		aiData.emptyEnemyBaseTiles.push_back(baseTile);
		
	}
	
}

void populateEnemyBaseOceanAssociations()
{
	aiData.enemyBaseOceanAssociations.clear();

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		bool baseTileOcean = is_ocean(baseTile);
		int baseOceanAssociation = getBaseOceanAssociation(baseId);

		// skip ours

		if (base->faction_id == aiFactionId)
			continue;

		// skip not ocean

		if (!baseTileOcean)
			continue;

		// add enemy base ocean association

		aiData.enemyBaseOceanAssociations.insert(baseOceanAssociation);

	}

}

/*
Sets tile combat movement realated info for each faction: blocked, zoc.
*/
void populateEnemyMovementInfos()
{
	bool TRACE = DEBUG && false;
	
	debug("populateEnemyMovementInfos\n");
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		// exclude our
		
		if (factionId == aiFactionId)
			continue;
		
		// populate base enemy movement impediments
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = getBase(baseId);
			MAP *baseTile = getBaseMapTile(baseId);
			bool baseTileOcean = is_ocean(baseTile);
			
			// skip friendly base
			
			if (isFriendly(factionId, base->faction_id))
				continue;
			
			// populate impediments
			
			for (int range = 0; range <= BASE_MOVEMENT_IMPEDIMENT_MAX_RANGE; range++)
			{
				for (MAP *tile : getEqualRangeTiles(baseTile, range))
				{
					bool tileOcean = is_ocean(tile);
					TileMovementInfo &tileMovementInfo = aiData.getTileMovementInfo(tile, factionId);
					
					// land
					
					if (tileOcean)
						continue;
					
					// accumulate impediment
					
					tileMovementInfo.travelImpediment += getBaseTravelImpediment(range, baseTileOcean);
					
				}
				
			}
			
		}
		
	}
	
	if (TRACE)
	{
		debug("travelImpediments\n");
		
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			// exclude our
			
			if (factionId == aiFactionId)
				continue;
			
			debug("\t%s\n", getMFaction(factionId)->noun_faction);
			
			// populate base enemy movement impediments
			
			for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
			{
				TileMovementInfo &tileMovementInfo = aiData.getTileMovementInfo(tile, factionId);
				
				debug("\t\t(%3d,%3d) %5.2f\n", getX(tile), getY(tile), tileMovementInfo.travelImpediment);
				
			}
			
		}
		
	}
	
}

/*
Sets tile combat movement realated info for each faction: blocked, zoc.
*/
void populateMovementInfos()
{
	debug("populateMovementInfos\n");
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		// populate base enemy movement impediments
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = getBase(baseId);
			MAP *baseTile = getBaseMapTile(baseId);
			bool baseTileOcean = is_ocean(baseTile);
			
			// skip friendly base
			
			if (isFriendly(factionId, base->faction_id))
				continue;
			
			// populate impediments
			
			for (int range = 0; range <= BASE_MOVEMENT_IMPEDIMENT_MAX_RANGE; range++)
			{
				for (MAP *tile : getEqualRangeTiles(baseTile, range))
				{
					TileMovementInfo &tileMovementInfo = aiData.getTileMovementInfo(tile, factionId);
					
					tileMovementInfo.travelImpediment += getBaseTravelImpediment(range, baseTileOcean);
					
				}
				
			}
			
		}
		
		// blockNeutral, zocNeutral
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			TileInfo &tileInfo = aiData.getTileInfo(vehicleTile);
			TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[factionId];
			
			// neutral or AI for other faction
			
			if (!isNeutral(factionId, vehicle->faction_id))
				continue;
			if (factionId != aiFactionId && vehicle->faction_id == aiFactionId)
				continue;
			
			// blockedNeutral
			
			tileMovementInfo.blockedNeutral = true;
			
			// zocNeutral
			
			if (!is_ocean(vehicleTile))
			{
				for (MAP *adjacentTile : getAdjacentTiles(vehicleTile))
				{
					TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
					TileMovementInfo &adjacentTileMovementInfo = adjacentTileInfo.movementInfos[factionId];
					
					if (is_ocean(adjacentTile))
						continue;
					
					adjacentTileMovementInfo.zocNeutral = true;
					
				}
				
			}
			
		}
		
		// blocked, zoc
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			TileInfo &tileInfo = aiData.getTileInfo(vehicleTile);
			TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[factionId];
			
			// unfriendly
			
			if (isFriendly(factionId, vehicle->faction_id))
				continue;
			
			// blocked
			
			tileMovementInfo.blocked = true;
			
			// zoc
			
			if (!is_ocean(vehicleTile))
			{
				for (MAP *adjacentTile : getAdjacentTiles(vehicleTile))
				{
					TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
					TileMovementInfo &adjacentTileMovementInfo = adjacentTileInfo.movementInfos[factionId];
					
					if (is_ocean(adjacentTile))
						continue;
					
					adjacentTileMovementInfo.zoc = true;
					
				}
				
			}
			
		}
		
		// base is not zoc
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			TileInfo &tileInfo = aiData.getTileInfo(baseTile);
			TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[factionId];
			
			// disable zoc
			
			tileMovementInfo.zocNeutral = false;
			tileMovementInfo.zoc = false;
			
		}
		
		// friendly vehicle (except probe)
		// disables zoc
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			TileInfo &tileInfo = aiData.getTileInfo(vehicleTile);
			TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[factionId];
			
			// friendly
			
			if (!isFriendly(factionId, vehicle->faction_id))
				continue;
			
			// not probe
			
			if (isProbeVehicle(vehicleId))
				continue;
			
			// set friendly
			
			tileMovementInfo.friendly = true;
			tileMovementInfo.zocNeutral = false;
			tileMovementInfo.zoc = false;
			
		}
		
		// populate targets
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			bool vehicleTileOcean = is_ocean(vehicleTile);
			
			// hostile
			
			if (!isHostile(aiFactionId, vehicle->faction_id))
				continue;
			
			// process adjacent tiles
			
			for (MAP *adjacentTile : getAdjacentTiles(vehicleTile))
			{
				bool adjacentTileOcean = is_ocean(adjacentTile);
				
				// exclude different realm
				
				if (!vehicleTileOcean)
				{
					if (adjacentTileOcean)
						continue;
				}
				else
				{
					if (!adjacentTileOcean)
						continue;
				}
				
			}
			
		}
		
	}
	
}

void populateTransfers()
{
	bool TRACE = DEBUG & false;
	
	if (TRACE) { debug("populateTransfers\n"); }
	
	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		std::map<int, std::map<int, std::vector<Transfer>>> &associationTransfers = aiData.geography.factions[factionId].associationTransfers;
		associationTransfers.clear();
		
		for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
		{
			bool tileOcean = is_ocean(tile);
			int tileAssociation = getAssociation(tile, factionId);
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			// land
			
			if (tileOcean)
				continue;
			
			// not blocked
			
			if (tileInfo.isBlocked(factionId, false))
				continue;
			
			// adjacent tiles
			
			for (MAP *adjacentTile : getAdjacentTiles(tile))
			{
				bool adjacentTileOcean = is_ocean(adjacentTile);
				int adjacentTileAssociation = getAssociation(adjacentTile, factionId);
				TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
				
				// ocean
				
				if (!adjacentTileOcean)
					continue;
				
				// not blocked
				
				if (adjacentTileInfo.isBlocked(factionId, false))
					continue;
				
				// not base
				
				if (isBaseAt(adjacentTile))
					continue;
				
				// add transfer
				
				associationTransfers[tileAssociation][adjacentTileAssociation].push_back({tile, adjacentTile});
				associationTransfers[adjacentTileAssociation][tileAssociation].push_back({tile, adjacentTile});
				
			}
			
		}
		
		if (TRACE)
		{
			debug("\t\ttransfers\n");
			for (std::pair<int const, std::map<int, std::vector<Transfer>>> &associationTransferEntry : associationTransfers)
			{
				int association1 = associationTransferEntry.first;
				std::map<int, std::vector<Transfer>> &association1Transfers = associationTransferEntry.second;
				
				for (std::pair<int const, std::vector<Transfer>> &association1TransferEntry : association1Transfers)
				{
					int association2 = association1TransferEntry.first;
					std::vector<Transfer> association1association2Transfers = association1TransferEntry.second;
					
					debug("\t\t\t%3d - %3d\n", association1, association2);
					
					for (Transfer transfer : association1association2Transfers)
					{
						debug
						(
							"\t\t\t\t(%3d,%3d)-(%3d,%3d)\n"
							, getX(transfer.passengerStop), getY(transfer.passengerStop), getX(transfer.transportStop), getY(transfer.transportStop)
						);
					}
					
				}
						
			}
			
		}
		
		// sea base transfers
		
		std::map<MAP *, std::vector<Transfer>> &oceanBaseTransfers = aiData.geography.factions[factionId].oceanBaseTransfers;
		oceanBaseTransfers.clear();
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = getBase(baseId);
			MAP *baseTile = getBaseMapTile(baseId);
			bool baseTileOcean = is_ocean(baseTile);
			
			// faction base
			
			if (base->faction_id != factionId)
				continue;
			
			// ocean base
			
			if (!baseTileOcean)
				continue;
			
			for (MAP *adjacentTile : getAdjacentTiles(baseTile))
			{
				bool adjacentTileOcean = is_ocean(adjacentTile);
				
				// ocean
				
				if (!adjacentTileOcean)
					continue;
				
				// add transfer
				
				oceanBaseTransfers[baseTile].push_back({baseTile, adjacentTile});
				
			}
			
		}
		
		if (TRACE)
		{
			debug("\t\tocean base transfers\n");
			for (std::pair<MAP * const, std::vector<Transfer>> &oceanBaseTransferEntry : oceanBaseTransfers)
			{
				MAP *baseTile = oceanBaseTransferEntry.first;
				std::vector<Transfer> &transfers = oceanBaseTransferEntry.second;
				
				debug("\t\t\t(%3d,%3d)\n", getX(baseTile), getY(baseTile));
				
				for (Transfer transfer : transfers)
				{
					debug
					(
						"\t\t\t\t(%3d,%3d)-(%3d,%3d)\n"
						, getX(transfer.passengerStop), getY(transfer.passengerStop), getX(transfer.transportStop), getY(transfer.transportStop)
					);
				}
				
			}
			
		}
		
	}
	
}

void populateLandClusters()
{
	debug("populateLandClusters\n");
	
	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		debug("\t%s\n", getMFaction(factionId)->noun_faction);
		
		FactionGeography &factionGeography = aiData.geography.factions[factionId];
		
		factionGeography.raiseableCoasts.clear();
		
		int landClusterIndex = 1;
		std::set<MAP *> raiseableCoasts;
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			BASE *base = getBase(baseId);
			MAP *baseTile = getBaseMapTile(baseId);
			bool baseTileOcean = is_ocean(baseTile);
			TileInfo &baseTileInfo = aiData.getTileInfo(baseTile);
			
			// our
			
			if (base->faction_id != factionId)
				continue;
			
			// land
			
			if (baseTileOcean)
				continue;
			
			// skip already processed
			
			if (baseTileInfo.landCluster[factionId] > 0)
				continue;
			
			// process
			
			
			std::set<MAP *> openTiles;
			
			baseTileInfo.landCluster[factionId] = landClusterIndex;
			openTiles.insert(baseTile);
			
			while (openTiles.size() > 0)
			{
				std::set<MAP *> newOpenTiles;
				
				for (MAP *openTile : openTiles)
				{
					for (MAP *adjacentTile : getAdjacentTiles(openTile))
					{
						bool adjacentTileOcean = is_ocean(adjacentTile);
						TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
						
						// land
						
						if (adjacentTileOcean)
						{
							raiseableCoasts.insert(openTile);
							continue;
						}
						
						// skip already processed
						
						if (adjacentTileInfo.landCluster[factionId] > 0)
							continue;
						
						// skip unfriendly
						
						if (!isFriendlyTerritory(factionId, adjacentTile))
							continue;
						
						// add to open nodes
						
						adjacentTileInfo.landCluster[factionId] = landClusterIndex;
						newOpenTiles.insert(adjacentTile);
						
					}
					
				}
				
				// update open tiles
				
				openTiles.clear();
				openTiles.swap(newOpenTiles);
				
			}
			
			// increment landClusterIndex
			
			landClusterIndex++;
			
		}
		
		// keep raiseable coasts
		
		for (MAP *tile : raiseableCoasts)
		{
			int elevation = map_elevation(tile);
			
			// exclude max elevation
			
			if (elevation == ALT_THREE_ABOVE_SEA - ALT_SHORE_LINE)
				continue;
			
			// exclude unsafe
			
			if (!isRaiseLandSafe(tile))
				continue;
			
			// keep the tile
			
			factionGeography.raiseableCoasts.push_back(tile);
			
		}
		
		if (DEBUG)
		{
			debug("\t\traiseableCoasts\n");
			
			for (MAP *tile : factionGeography.raiseableCoasts)
			{
				debug("\t\t\t(%3d,%3d)\n", getX(tile), getY(tile));
			}
			
		}
		
	}
	
}

void evaluateBasePoliceRequests()
{
	for (int baseId : aiData.baseIds)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		PoliceData &basePoliceData = baseInfo.policeData;
		basePoliceData.clear();

		basePoliceData.effect = getBasePoliceEffect(baseId);
		basePoliceData.unitsAllowed = getBasePoliceAllowed(baseId);
		basePoliceData.dronesExisting = getBasePoliceDrones(baseId);

	}

}

void evaluateBaseDefense()
{
	debug("evaluateBaseDefense - %s\n", MFactions[aiFactionId].noun_faction);
	
	Faction *faction = &(Factions[aiFactionId]);
	
	std::set<int> ownCombatUnitIds;
	std::vector<int> foeFactionIds;
	std::vector<int> hostileFoeFactionIds;
	std::vector<int> foeCombatVehicleIds;
	std::vector<std::set<int>> foeUnitIds(MaxPlayerNum, std::set<int>());
	std::vector<std::set<int>> foeCombatUnitIds(MaxPlayerNum, std::set<int>());
	FoeUnitWeightTable foeUnitWeightTable;
	
	CombatEffectTable &combatEffectTable = aiData.combatEffectTable;
	
	executionProfiles["1.1.6.1. collect data"].start();
	
	// collect own combat units
	
	ownCombatUnitIds.insert(aiData.activeCombatUnitIds.begin(), aiData.activeCombatUnitIds.end());
	ownCombatUnitIds.insert(aiData.combatUnitIds.begin(), aiData.combatUnitIds.end());
	
	// populate enemy factionIds
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		// exclude current AI faction
		
		if (factionId == aiFactionId)
			continue;
		
		// exclude pact faction
		
		if (isPact(aiFactionId, factionId))
			continue;
		
		// unfriendly faction
		
		foeFactionIds.push_back(factionId);
		
		// hostile faction
		
		if (isVendetta(aiFactionId, factionId))
		{
			hostileFoeFactionIds.push_back(factionId);
		}
		
	}
	
	// collect active foe non-combat and combat units
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// exclude own
		
		if (vehicle->faction_id == aiFactionId)
			continue;
		
		// exclude pact faction
		
		if (isPact(aiFactionId, vehicle->faction_id))
			continue;
		
		// add to all list
		
		foeUnitIds.at(vehicle->faction_id).insert(vehicle->unit_id);
		
		// add to combat list
		
		if (isCombatVehicle(vehicleId))
		{
			foeCombatUnitIds.at(vehicle->faction_id).insert(vehicle->unit_id);
			foeCombatVehicleIds.push_back(vehicleId);
		}
		
	}
	
	// add aliens - they are always there
	
	foeUnitIds.at(0).insert(BSC_MIND_WORMS);
	foeUnitIds.at(0).insert(BSC_SPORE_LAUNCHER);
	foeUnitIds.at(0).insert(BSC_SEALURK);
	foeUnitIds.at(0).insert(BSC_ISLE_OF_THE_DEEP);
	foeUnitIds.at(0).insert(BSC_LOCUSTS_OF_CHIRON);
	foeUnitIds.at(0).insert(BSC_FUNGAL_TOWER);
	foeCombatUnitIds.at(0).insert(BSC_MIND_WORMS);
	foeCombatUnitIds.at(0).insert(BSC_SPORE_LAUNCHER);
	foeCombatUnitIds.at(0).insert(BSC_SEALURK);
	foeCombatUnitIds.at(0).insert(BSC_ISLE_OF_THE_DEEP);
	foeCombatUnitIds.at(0).insert(BSC_LOCUSTS_OF_CHIRON);
	// fungal tower is not a combat unit as it does not attack
	
	executionProfiles["1.1.6.1. collect data"].stop();
	
	// compute combatEffects
	
	executionProfiles["1.1.6.2. compute combatEffects"].start();
	
	debug("\tcombatEffects\n");
	
	for (int ownUnitId : ownCombatUnitIds)
	{
		for (int foeFactionId : foeFactionIds)
		{
			for (int foeUnitId : foeUnitIds.at(foeFactionId))
			{
				// calculate how many foe units our unit can destroy in melee attack
				
				{
					double combatEffect = 0.0;
					
					if (isMeleeUnit(ownUnitId))
					{
						combatEffect = getMeleeRelativeUnitStrength(ownUnitId, aiFactionId, foeUnitId, foeFactionId);
					}
					
					combatEffectTable.setCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_MELEE, combatEffect);
					
				}
				
				// calculate how many our units foe unit can destroy in melee attack
				
				{
					double combatEffect = 0.0;
					
					if (isMeleeUnit(foeUnitId))
					{
						combatEffect = getMeleeRelativeUnitStrength(foeUnitId, foeFactionId, ownUnitId, aiFactionId);
					}
					
					combatEffectTable.setCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_MELEE, combatEffect);
					
				}
				
				// calculate how many foe units our unit can destroy in artillery duel
				
				{
					double combatEffect = 0.0;
					
					if (isArtilleryUnit(ownUnitId) && isArtilleryUnit(foeUnitId))
					{
						combatEffect = getArtilleryDuelRelativeUnitStrength(ownUnitId, aiFactionId, foeUnitId, foeFactionId);
					}
					
					combatEffectTable.setCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_ARTILLERY_DUEL, combatEffect);
					
				}
				
				// calculate how many our units foe unit can destroy in artillery duel
				
				{
					double combatEffect = 0.0;
					
					if (isArtilleryUnit(foeUnitId) && isArtilleryUnit(ownUnitId))
					{
						combatEffect = getArtilleryDuelRelativeUnitStrength(foeUnitId, foeFactionId, ownUnitId, aiFactionId);
					}
					
					combatEffectTable.setCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_ARTILLERY_DUEL, combatEffect);
					
				}
				
				// calculate how many foe units our unit can destroy by bombardment
				
				{
					double combatEffect = 0.0;
					
					if (isArtilleryUnit(ownUnitId) && !isArtilleryUnit(foeUnitId))
					{
						combatEffect = getUnitBombardmentDamage(ownUnitId, aiFactionId, foeUnitId, foeFactionId);
//						combatEffect = getBombardmentRelativeUnitStrength(ownUnitId, aiFactionId, foeUnitId, foeFactionId);
					}
					
					combatEffectTable.setCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_BOMBARDMENT, combatEffect);
					
				}
				
				// calculate how many own units foe unit can destroy by bombardment
				
				{
					double combatEffect = 0.0;
					
					if (isArtilleryUnit(foeUnitId) && !isArtilleryUnit(ownUnitId))
					{
						combatEffect = getUnitBombardmentDamage(foeUnitId, foeFactionId, ownUnitId, aiFactionId);
//						combatEffect = getBombardmentRelativeUnitStrength(foeUnitId, foeFactionId, ownUnitId, aiFactionId);
					}
					
					combatEffectTable.setCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_BOMBARDMENT, combatEffect);
					
				}
				
			}
			
		}
		
	}
	
	executionProfiles["1.1.6.2. compute combatEffects"].stop();
	
	if (DEBUG)
	{
		for (int ownUnitId : ownCombatUnitIds)
		{
			debug("\t\t[%3d] %-32s\n", ownUnitId, getUnit(ownUnitId)->name);
			
			for (int foeFactionId : foeFactionIds)
			{
				for (int foeUnitId : foeUnitIds.at(foeFactionId))
				{
					debug("\t\t\t%-24s [%3d] %-32s\n", getMFaction(foeFactionId)->noun_faction, foeUnitId, getUnit(foeUnitId)->name);
					
					for (int side = 0; side <= 1; side++)
					{
						for (int combatType = (int)CT_MELEE; combatType <= (int)CT_BOMBARDMENT; combatType++)
						{
							double effect = combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, side, (COMBAT_TYPE)combatType);
							
							if (effect == 0.0)
								continue;
							
							debug("\t\t\t\tside=%d CT=%d effect=%5.2f\n", side, combatType, effect);
							
						}
						
					}
					
				}
				
			}
			
		}
		
	}
	
	// ****************************************************************************************************
	// stack combat computation
	// evaluates our unit average combat effect based on average losses
	// ****************************************************************************************************
	
	executionProfiles["1.1.6.3. stack combat computation"].start();
	
	debug("\tStacks\n");
	
	for (std::pair<MAP * const, EnemyStackInfo> &stackInfoEntry : aiData.enemyStacks)
	{
		MAP *enemyStackTile = stackInfoEntry.first;
		EnemyStackInfo &enemyStackInfo = stackInfoEntry.second;
		
		debug("\t(%3d,%3d)\n", getX(enemyStackTile), getY(enemyStackTile));
		
		// summarize foe total weight (how much stronger vehicle is comparing to unmodified unit)
		
		debug("\t\tstack foeMilitaryStrength\n");
		
		double totalWeight = 0.0;
		double totalWeightBombarded = 0.0;
		
		for (int vehicleId : enemyStackInfo.vehicleIds)
		{
			VEH *vehicle = getVehicle(vehicleId);
			
			// weight
			
			double weight = getVehicleStrenghtMultiplier(vehicleId);
			totalWeight += weight;
			
			// weight bombarded
			
			double maxRelativeBombardmentDamage = getVehicleMaxRelativeBombardmentDamage(vehicleId);
			double minBombardedRelativePower = 1.0 - maxRelativeBombardmentDamage;
			double currentRelativePower = getVehicleRelativePower(vehicleId);
			
			double weightBombarded = weight * minBombardedRelativePower / currentRelativePower;
			totalWeightBombarded += weightBombarded;
			
			debug
			(
				"\t\t\t(%3d,%3d) %-25s %-32s"
				" weight=%5.2f"
				" weightBombarded=%5.2f"
				"\n"
				, vehicle->x, vehicle->y, getMFaction(vehicle->faction_id)->noun_faction, Units[vehicle->unit_id].name
				, weight
				, weightBombarded
			);
			
		}
		
		// compute required effect
		
		double requiredSuperiority;
		double desiredSuperiority;
		
		if (isBaseAt(enemyStackTile))
		{
			requiredSuperiority = conf.ai_combat_base_attack_superiority_required;
			desiredSuperiority = conf.ai_combat_base_attack_superiority_desired;
		}
		else
		{
			requiredSuperiority = conf.ai_combat_field_attack_superiority_required;
			desiredSuperiority = conf.ai_combat_field_attack_superiority_desired;
		}
		
		enemyStackInfo.requiredEffect = requiredSuperiority * totalWeight;
		enemyStackInfo.requiredEffectBombarded = requiredSuperiority * totalWeightBombarded;
		enemyStackInfo.desiredEffect = desiredSuperiority * totalWeight;
		
		debug
		(
			"\t\trequiredEffect=%5.2f desiredEffect=%5.2f"
			" requiredSuperiority=%5.2f desiredSuperiority=%5.2f"
			" totalWeight=%5.2f"
			" providedEffect=%5.2f"
			"\n"
			, enemyStackInfo.requiredEffect
			, enemyStackInfo.desiredEffect
			, requiredSuperiority
			, desiredSuperiority
			, totalWeight
			, enemyStackInfo.providedEffect
		);
		
		// calculate unit effects
		
		debug("\t\tunit effects\n");
		
		for (int ownUnitId : ownCombatUnitIds)
		{
			debug("\t\t\t[%3d] %-32s\n", ownUnitId, getUnit(ownUnitId)->name);
			
			// melee
			
			if (isMeleeUnit(ownUnitId) && !(enemyStackInfo.needlejetInFlight && !isUnitHasAbility(ownUnitId, ABL_AIR_SUPERIORITY)))
			{
				int battleCount = 0;
				double totalLoss = 0.0;
				
				for (int foeVehicleId : enemyStackInfo.vehicleIds)
				{
					// effect
					
					double effect = getUnitMeleeAttackEffect(aiFactionId, ownUnitId, foeVehicleId, enemyStackTile);
					
					if (effect <= 0.0)
						continue;
					
					// loss
					
					double loss = 1.0 / effect;
					battleCount++;
					totalLoss += loss;
					
				}
				
				if (battleCount > 0 && totalLoss > 0.0)
				{
					double averageEffect = battleCount / totalLoss;
					debug("\t\t\t\t%-15s %5.2f\n", "melee", averageEffect);
					
					enemyStackInfo.setUnitEffect(ownUnitId, AT_MELEE, CT_MELEE, true, averageEffect);
					
				}
				
			}
			
			// artillery
			
			if (isArtilleryUnit(ownUnitId) && enemyStackInfo.artillery)
			{
				int battleCount = 0;
				double totalLoss = 0.0;
				
				for (int foeVehicleId : enemyStackInfo.artilleryVehicleIds)
				{
					// effect
					
					double effect = getUnitArtilleryDuelAttackEffect(aiFactionId, ownUnitId, foeVehicleId, enemyStackTile);
					
					if (effect <= 0.0)
						continue;
					
					// loss
					
					double loss = 1.0 / effect;
					battleCount++;
					totalLoss += loss;
					
				}
				
				if (battleCount > 0 && totalLoss > 0.0)
				{
					double averageEffect = battleCount / totalLoss;
					debug("\t\t\t\t%-15s %5.2f\n", "artilleryDuel", averageEffect);
					
					enemyStackInfo.setUnitEffect(ownUnitId, AT_ARTILLERY, CT_ARTILLERY_DUEL, true, averageEffect);
					
				}
				
			}
			else if (isArtilleryUnit(ownUnitId) && enemyStackInfo.bombardment)
			{
				bool destructive = false;
				int battleCount = 0;
				double totalLoss = 0.0;
				
				for (int foeVehicleId : enemyStackInfo.bombardmentVehicleIds)
				{
					double foeVehicleMaxRelativeBombardmentDamage = getVehicleMaxRelativeBombardmentDamage(foeVehicleId);
					double foeVehicleRemainingRelativeBombardmentDamage = getVehicleRemainingRelativeBombardmentDamage(foeVehicleId);
					
					// can bombard
					
					if (foeVehicleRemainingRelativeBombardmentDamage == 0.0)
						continue;
					
					// destructive
					
					if (foeVehicleMaxRelativeBombardmentDamage >= 1.0)
					{
						destructive = true;
					}
					
					// bombardment effect adjusted to vehicle and terrain
					
					double effect = getUnitBombardmentAttackEffect(aiFactionId, ownUnitId, foeVehicleId, enemyStackTile);
					
					if (effect <= 0.0)
						continue;
					
					// loss
					
					double loss = 1.0 / effect;
					battleCount++;
					totalLoss += loss;
					
				}
				
				if (battleCount > 0 && totalLoss > 0.0)
				{
					double averageEffect = battleCount / totalLoss;
					debug("\t\t\t\t%-15s %5.2f\n", "bombardment", averageEffect);
					
					enemyStackInfo.setUnitEffect(ownUnitId, AT_ARTILLERY, CT_BOMBARDMENT, destructive, averageEffect);
					
				}
				
			}
			
		}
		
	}
	
	executionProfiles["1.1.6.3. stack combat computation"].stop();
	
	// evaluate base threat
	
	executionProfiles["1.1.6.4. evaluate base threat"].start();
	
	aiData.mostVulnerableBaseId = -1;
	aiData.mostVulnerableBaseThreat = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool baseTileOcean = is_ocean(baseTile);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		int baseAssociation = getAssociation(baseTile, base->faction_id);
		
		BaseCombatData &baseCombatData = baseInfo.combatData;
		baseCombatData.clear();
		foeUnitWeightTable.clear();
		baseInfo.safeTime = INT_MAX;
		
		debug
		(
			"\t%-25s"
			"\n"
			, base->name
		);
		
		// calculate foe strength
		
		executionProfiles["1.1.6.4.1. calculate foe strength"].start();
		
		debug("\t\tbase foeMilitaryStrength\n");
		
		int alienCount = 0;
		for (int vehicleId : foeCombatVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			UNIT *unit = &(Units[vehicle->unit_id]);
			CChassis *chassis = &(Chassis[unit->chassis_type]);
			int triad = chassis->triad;
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			int vehicleAssociation = getVehicleAssociation(vehicleId);
			
			// land base
			if (!baseTileOcean)
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
			
			// strength multiplier
			
			double strengthMultiplier = getVehicleStrenghtMultiplier(vehicleId);
			
			// threat coefficient
			
			double threatCoefficient = aiData.factionInfos[vehicle->faction_id].threatCoefficient;
			
			// travel time coefficient
			
			executionProfiles["1.1.6.4.1.1. getVehicleAttackATravelTime"].start();
			int travelTime = getVehicleAttackATravelTime(vehicleId, baseTile, true);
			executionProfiles["1.1.6.4.1.1. getVehicleAttackATravelTime"].stop();
			
			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
				continue;
			
			// multiply travel time by approach chance
			// the more targets vehicle has in vicinity the less likely it approaches us
			
			executionProfiles["1.1.6.4.1.2. getVehicleApproachChance"].start();
			double approachChance = getVehicleApproachChance(baseId, vehicleId);
			executionProfiles["1.1.6.4.1.2. getVehicleApproachChance"].stop();
			
			if (approachChance <= 0.0)
				continue;
			
			double approachChanceTravelTime = approachChance * (double)travelTime;
			
			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, (double)travelTime / approachChance);
			
			// set base safe time
			
			baseInfo.safeTime = std::min(baseInfo.safeTime, (int)floor(approachChanceTravelTime) - 1);
			
			// borderDistanceCoefficient
			// applies to land vehicles
			// assuming they are traveling at 1/3 of square per turn
			
			double borderDistanceCoefficient;
			if (triad == TRIAD_LAND)
			{
				borderDistanceCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, 3.0 * baseInfo.borderDistance);
			}
			else
			{
				borderDistanceCoefficient = 1.0;
			}
			
			// calculate weight
			
			double weight = strengthMultiplier * threatCoefficient * travelTimeCoefficient * borderDistanceCoefficient;
			
			// store value
			
			foeUnitWeightTable.add(vehicle->faction_id, vehicle->unit_id, weight);
			if (vehicle->faction_id == 0)
			{
				alienCount++;
			}
			
			debug
			(
				"\t\t\t(%3d,%3d) %-32s"
				" weight=%5.2f"
				" strengthMultiplier=%5.2f"
				" travelTime=%4d approachChance=%5.2f travelTimeCoefficient=%5.2f"
				" threatCoefficient=%5.2f"
				" borderDistanceCoefficient=%5.2f"
				"\n"
				, vehicle->x, vehicle->y, Units[vehicle->unit_id].name
				, weight
				, strengthMultiplier
				, travelTime, approachChance, travelTimeCoefficient
				, threatCoefficient
				, borderDistanceCoefficient
			);
			
		}
		
		executionProfiles["1.1.6.4.1. calculate foe strength"].stop();
		
		if (DEBUG)
		{
			for (int foeFactionId : foeFactionIds)
			{
				for (int foeUnitId : foeCombatUnitIds.at(foeFactionId))
				{
					double weight = foeUnitWeightTable.get(foeFactionId, foeUnitId);
					
					if (weight == 0.0)
						continue;
					
					debug
					(
						"\t\t\t%-24s %-32s weight=%5.2f\n",
						MFactions[foeFactionId].noun_faction,
						Units[foeUnitId].name,
						weight
					)
					;
					
				}
				
			}
			
		}
		
		// summarize foe unit weights by faction
		
		executionProfiles["1.1.6.4.2. summarize foe unit weights by faction"].start();
		
		double foeTriadWeights[3]{0.0};
		double foeFactionWeights[MaxPlayerNum];
		
		for (int foeFactionId : foeFactionIds)
		{
			double foeFactionWeight = 0.0;
			
			for (int foeUnitId : foeCombatUnitIds.at(foeFactionId))
			{
				UNIT *foeUnit = getUnit(foeUnitId);
				int foeTriad = foeUnit->triad();
				
				// exclude alien spore launchers and fungal tower
				
				if (foeFactionId == 0 && (foeUnitId == BSC_SPORE_LAUNCHER || foeUnitId == BSC_FUNGAL_TOWER))
					continue;
				
				// accumulate weight
				
				double weight = foeUnitWeightTable.get(foeFactionId, foeUnitId);
				if (foeFactionId == 0)
				{
					weight /= sqrt((double)std::max(1, alienCount));
				}
				foeFactionWeight += weight;
				
				// accumulate base defense type weight
				
				if (!isNativeUnit(foeUnitId))
				{
					foeTriadWeights[foeTriad] += weight;
				}
				
			}
			
			foeFactionWeights[foeFactionId] = foeFactionWeight;
			
		}
		
		// --------------------------------------------------
		// calculate potential alien strength
		// --------------------------------------------------
		
		debug("\t\tfoe MilitaryStrength, potential alien\n");
		
		{
			int alienUnitId;
			
			if (!baseTileOcean)
			{
				alienUnitId = BSC_MIND_WORMS;
			}
			else
			{
				alienUnitId = BSC_ISLE_OF_THE_DEEP;
			}
			
			// strength modifier
			
			double moraleMultiplier = getAlienMoraleMultiplier();
			double gameTurnCoefficient = getAlienTurnStrengthModifier();
			
			// set basic numbers to occasional vehicle
			
			double numberCoefficient = 1.0;
			
			// add more numbers based on eco-damage and number of fungal pops
			
			if (!baseTileOcean)
			{
				numberCoefficient += (double)((faction->fungal_pop_count / 3) * (base->eco_damage / 20));
			}
			
			// calculate weight
			// reduce potential alien weight slightly to increase mobility even if for increased risk
			
			double weight = 0.75 * moraleMultiplier * gameTurnCoefficient * numberCoefficient;
			
			// do not add potential weight more than actual one
			
			if (weight <= foeFactionWeights[0])
			{
				weight = 0.0;
			}
			else
			{
				weight -= foeFactionWeights[0];
			}
			
			debug
			(
				"\t\t\t%-32s weight=%5.2f, moraleMultiplier=%5.2f, gameTurnCoefficient=%5.2f, numberCoefficient=%5.2f\n",
				Units[alienUnitId].name,
				weight, moraleMultiplier, gameTurnCoefficient, numberCoefficient
			);
			
			// store value

			foeUnitWeightTable.set(0, alienUnitId, weight);
			foeFactionWeights[0] += weight;
			
		}
		
		// --------------------------------------------------
		// calculate potential alien strength - end
		// --------------------------------------------------
		
		// store base defense type threats
		
		for (int i = 0; i < 3; i++)
		{
			debug("foeTriadWeights[%d]=%5.2f\n", i, foeTriadWeights[i]);
			baseCombatData.triadThreats[i] = foeTriadWeights[i];
		}
		
		// summarize total foe unit weights to estimate threat
		
		double totalThreat = 0.0;

		for (int foeFactionId : foeFactionIds)
		{
			totalThreat += foeFactionWeights[foeFactionId];
		}
		
		executionProfiles["1.1.6.4.2. summarize foe unit weights by faction"].stop();
		
		// compute required protection
		
		executionProfiles["1.1.6.4.3. compute required protection"].start();
		
		double requiredEffect = conf.ai_combat_base_protection_superiority * totalThreat;
		baseInfo.combatData.requiredEffect = requiredEffect;
		
		debug
		(
			"\t\trequiredEffect=%5.2f"
			" conf.ai_combat_base_protection_superiority=%5.2F"
			" totalThreat=%5.2f"
			"\n"
			, requiredEffect
			, conf.ai_combat_base_protection_superiority
			, totalThreat
		);
		
		executionProfiles["1.1.6.4.3. compute required protection"].stop();
		
		// update most vulnerable base
		
		if (totalThreat > aiData.mostVulnerableBaseThreat)
		{
			aiData.mostVulnerableBaseId = baseId;
			aiData.mostVulnerableBaseThreat = totalThreat;
		}
		
		// calculate unit effects
		
		executionProfiles["1.1.6.4.4. calculate unit effects"].start();
		
		debug("\t\tcalculate base unit effects\n");
		
		for (int ownUnitId : ownCombatUnitIds)
		{
			UNIT *ownUnit = &(Units[ownUnitId]);
			
			debug("\t\t\t[%3d] %-32s\n", ownUnitId, ownUnit->name);
			
			double totalWeight = 0.0;
			double totalWeightedEffect = 0.0;
			
			for (int foeFactionId : foeFactionIds)
			{
				for (int foeUnitId : foeCombatUnitIds.at(foeFactionId))
				{
					double weight = foeUnitWeightTable.get(foeFactionId, foeUnitId);
					
					// exclude unit without weight
					
					if (weight == 0.0)
						continue;
					
					double effect = getUnitBaseProtectionEffect(aiFactionId, ownUnitId, foeFactionId, foeUnitId, baseTile);
					
					totalWeight += weight;
					totalWeightedEffect += weight * effect;
					
					debug
					(
						"\t\t\t\t[%3d] %-32s - %-20s"
						" weight=%5.2f"
						" effect=%5.2f"
						"\n"
						, foeUnitId, getUnit(foeUnitId)->name, getMFaction(foeFactionId)->noun_faction
						, weight
						, effect
					);
					
				}
				
			}
			
			double averageEffect = totalWeight > 0.0 ? totalWeightedEffect / totalWeight : 0.0;
			
			debug
			(
				"\t\t\t%-32s"
				" averageEffect=%5.2f"
				"\n"
				, ownUnit->name
				, averageEffect
			);
			
			baseCombatData.setUnitEffect(ownUnitId, averageEffect);
			
		}
		
		executionProfiles["1.1.6.4.4. calculate unit effects"].stop();
		
	}
	
	executionProfiles["1.1.6.4. evaluate base threat"].stop();
	
	// calculate faction average base required protection
	
	executionProfiles["1.1.6.5. calculate faction average base required protection"].start();
	
	int factionAverageBaseRequiredProtectionCount = 0;
	double factionAverageBaseRequiredProtectionSum = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		factionAverageBaseRequiredProtectionCount++;
		factionAverageBaseRequiredProtectionSum += aiData.getBaseInfo(baseId).combatData.requiredEffect;
	}
	
	aiData.factionBaseAverageRequiredProtection =
		factionAverageBaseRequiredProtectionCount == 0
		?
			0.0
			:
			factionAverageBaseRequiredProtectionSum / (double)factionAverageBaseRequiredProtectionCount
	;
	
	executionProfiles["1.1.6.5. calculate faction average base required protection"].stop();
	
	// calculate globalAverageUnitEffects
	
	executionProfiles["1.1.6.6. calculate globalAverageUnitEffects"].start();
	
	for (int unitId : ownCombatUnitIds)
	{
		// average baseUnitEffect
		
		double totalBaseUnitEffect = 0.0;
		
		for (int baseId : aiData.baseIds)
		{
			BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
			
			totalBaseUnitEffect += baseInfo.combatData.getUnitEffect(unitId);
			
		}
		
		double averageBaseUnitEffect = (aiData.baseIds.size() == 0 ? 0.0 : totalBaseUnitEffect / (double)aiData.baseIds.size());
		
		// average stackUnitEffect
		
		double totalStackUnitEffect = 0.0;
		
		for (std::pair<MAP * const, EnemyStackInfo> &stackInfoEntry : aiData.enemyStacks)
		{
			EnemyStackInfo &stackInfo = stackInfoEntry.second;
			totalStackUnitEffect += stackInfo.getAverageUnitEffect(unitId);
		}
		
		double averageStackUnitEffect = (aiData.enemyStacks.size() == 0 ? 0.0 : totalStackUnitEffect / (double)aiData.enemyStacks.size());
		
		double globalAverageUnitEffect = (averageBaseUnitEffect + averageStackUnitEffect) / 2.0;
		
		aiData.setGlobalAverageUnitEffect(unitId, globalAverageUnitEffect);
		
	}
	
	executionProfiles["1.1.6.6. calculate globalAverageUnitEffects"].stop();
	
}

/*
Populates enemy offense threat for each tile.
*/
void populateEnemyOffenseThreat()
{
	const bool TRACE = DEBUG && false;
	
	debug("populateEnemyOffenseThreat - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// populate hostile vehicle enemyOffensiveForces
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		bool vehicleTileOcean = is_ocean(vehicleTile);
		int triad = vehicle->triad();
		
		// hostile
		
		if (!isHostile(aiFactionId, vehicle->faction_id))
			continue;
		
		// combat
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		// able to reach around and act
		
		if (triad == TRIAD_LAND && vehicleTileOcean)
			continue;
		
		if (TRACE)
		{
			debug("\t[%4d] (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y);
		}
		
		if (isMeleeVehicle(vehicleId))
		{
			for (MovementAction &vehicleMovementAction : getVehicleMeleeAttackLocations(vehicleId))
			{
				MAP *tile = vehicleMovementAction.destination;
				int movementAllowance = vehicleMovementAction.movementAllowance;
				double hastyCoefficient = std::min(1.0, (double)movementAllowance / (double)Rules->mov_rate_along_roads);
				TileInfo &tileInfo = aiData.getTileInfo(tile);
				
				// stop condition
				// zero movement allowance - cannot attack this place
				
				if (movementAllowance <= 0)
					continue;
				
				tileInfo.enemyOffensiveForces.emplace_back(vehicleId, AT_MELEE, hastyCoefficient);
				
			}
			
		}
		
		if (isArtilleryVehicle(vehicleId))
		{
			for (MovementAction &vehicleMovementAction : getVehicleArtilleryAttackPositions(vehicleId))
			{
				MAP *tile = vehicleMovementAction.destination;
				int movementAllowance = vehicleMovementAction.movementAllowance;
				TileInfo &tileInfo = aiData.getTileInfo(tile);
				
				// stop condition
				// zero movement allowance - cannot attack this place
				
				if (movementAllowance <= 0)
					continue;
				
				tileInfo.enemyOffensiveForces.push_back({vehicleId, AT_ARTILLERY, 1.0});
				
			}
			
		}
		
	}
	
	if (TRACE)
	{
		debug("\tenemyOffenseThreats\n");
		
		for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			for (Force force : tileInfo.enemyOffensiveForces)
			{
				int vehicleId = force.getVehicleId();
				if (vehicleId == -1)
					continue;
				
				VEH *vehicle = getVehicle(vehicleId);
				
				debug
				(
					"\t\t[%4d] (%3d,%3d)"
					" attackType=%d"
					" hastyCoefficient=%5.2f"
					"\n"
					, vehicleId, vehicle->x, vehicle->y
					, force.attackType
					, force.hastyCoefficient
				);
				
			}
			
		}
		
	}
	
}

/*
Designs units.
This function works before AI Data are populated and should not rely on them.
*/
void designUnits()
{
	// get best values
	
	int bestWeapon = getFactionBestWeapon(aiFactionId);
	int bestArmor = getFactionBestArmor(aiFactionId);
	int defenderWeapon = getFactionBestWeapon(aiFactionId, (bestArmor + 1) / 2);
	int attackerArmor = getFactionBestArmor(aiFactionId, bestWeapon);
	int fastLandChassis = (has_chassis(aiFactionId, CHS_HOVERTANK) ? CHS_HOVERTANK : CHS_SPEEDER);
	int fastSeaChassis = (has_chassis(aiFactionId, CHS_CRUISER) ? CHS_CRUISER : CHS_FOIL);
	int bestReactor = best_reactor(aiFactionId);
	
	// land anti-native defenders
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{WPN_HAND_WEAPONS},
		{ARM_NO_ARMOR},
		{
			{ABL_ID_TRANCE, ABL_ID_CLEAN_REACTOR, ABL_ID_POLICE_2X},
		},
		bestReactor,
		PLAN_DEFENSIVE,
		NULL
	);
	
	// land regular defenders
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{defenderWeapon},
		{bestArmor},
		{
			{ABL_ID_CLEAN_REACTOR},
			{ABL_ID_POLICE_2X, ABL_ID_CLEAN_REACTOR, ABL_ID_TRANCE},
			{ABL_ID_COMM_JAMMER, ABL_ID_CLEAN_REACTOR},
			{ABL_ID_AAA, ABL_ID_CLEAN_REACTOR},
		},
		bestReactor,
		PLAN_DEFENSIVE,
		NULL
	);
	
	// land armored attackers
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{bestWeapon},
		{attackerArmor},
		{
			// plain
			{},
			// protected against fast and air units
			{ABL_ID_COMM_JAMMER},
			{ABL_ID_AAA},
			// fast and deadly in field
			{ABL_ID_SOPORIFIC_GAS, ABL_ID_ANTIGRAV_STRUTS},
			// fast and deadly against bases
			{ABL_ID_BLINK_DISPLACER, ABL_ID_ANTIGRAV_STRUTS},
		},
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
		{attackerArmor},
		{
			// plain
			{},
			// anti native
			{ABL_ID_EMPATH},
			// next to shore sea base attacker
			{ABL_ID_AMPHIBIOUS, ABL_ID_BLINK_DISPLACER},
			// fast and deadly in field
			{ABL_ID_SOPORIFIC_GAS, ABL_ID_ANTIGRAV_STRUTS},
			// fast and deadly against bases
			{ABL_ID_BLINK_DISPLACER, ABL_ID_ANTIGRAV_STRUTS},
			// anti air
			{ABL_ID_AIR_SUPERIORITY, ABL_ID_ANTIGRAV_STRUTS},
			// anti anti fast
			{ABL_ID_DISSOCIATIVE_WAVE},
			{ABL_ID_DISSOCIATIVE_WAVE, ABL_ID_BLINK_DISPLACER},
		},
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
		{bestArmor},
		{
			{ABL_ID_ARTILLERY},
		},
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
		{
			{ABL_ID_DROP_POD, ABL_ID_BLINK_DISPLACER},
		},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
	// ships
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastSeaChassis},
		{bestWeapon},
		{attackerArmor},
		{
			// plain
			{},
			// anti native
			{ABL_ID_EMPATH},
			// air defended
			{ABL_ID_AAA, ABL_ID_SOPORIFIC_GAS},
			// anti air
			{ABL_ID_AIR_SUPERIORITY, ABL_ID_SOPORIFIC_GAS},
			// anti base
			{ABL_ID_BLINK_DISPLACER, ABL_ID_SOPORIFIC_GAS},
			// pirates
			{ABL_ID_MARINE_DETACHMENT, ABL_ID_SOPORIFIC_GAS},
		},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
	// armored transport
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastSeaChassis},
		{WPN_TROOP_TRANSPORT},
		{bestArmor},
		{
			// heavy
			{ABL_ID_HEAVY_TRANSPORT, ABL_ID_CLEAN_REACTOR},
			// air protected
			{ABL_ID_HEAVY_TRANSPORT, ABL_ID_AAA, ABL_ID_CLEAN_REACTOR},
		},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
	// armored land probe
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastLandChassis},
		{WPN_PROBE_TEAM},
		{bestArmor},
		{
			// psi protected
			{ABL_ID_TRANCE},
			// air protected
			{ABL_ID_AAA},
		},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
	// armored sea probe
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastSeaChassis},
		{WPN_PROBE_TEAM},
		{bestArmor},
		{
			// psi protected
			{ABL_ID_TRANCE},
			// air protected
			{ABL_ID_AAA},
		},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
	// --------------------------------------------------
	// obsolete units
	// --------------------------------------------------
	
//	// land fast artillery
//	
//	obsoletePrototypes
//	(
//		aiFactionId,
//		{CHS_SPEEDER, CHS_HOVERTANK},
//		{},
//		{},
//		{ABL_ID_ARTILLERY},
//		{}
//	);
//	
	// --------------------------------------------------
	// reinstate units
	// --------------------------------------------------
	
	// find Trance Scout Patrol
	
	int tranceScoutPatrolUnitId = -1;
	
	for (int unitId : getFactionUnitIds(aiFactionId, true, false))
	{
		UNIT *unit = getUnit(unitId);
		int unitOffenseValue = getUnitOffenseValue(unitId);
		int unitDefenseValue = getUnitDefenseValue(unitId);
		
		if (unit->chassis_type == CHS_INFANTRY && unitOffenseValue == 1 && unitDefenseValue == 1 && isUnitHasAbility(unitId, ABL_ID_TRANCE))
		{
			tranceScoutPatrolUnitId = unitId;
			break;
		}
		
	}
	
	if (tranceScoutPatrolUnitId == -1)
	{
		// reinstate Scout Patrol if Trance Scout Patrol is not available to faction
		
		reinstateUnit(BSC_SCOUT_PATROL, aiFactionId);
		
	}
	else
	{
		// reinstate Trance Scout Patrol
		
		reinstateUnit(tranceScoutPatrolUnitId, aiFactionId);
		
	}
	
}

/*
Propose multiple prototype combinations.
*/
void proposeMultiplePrototypes(int factionId, std::vector<int> chassisIds, std::vector<int> weaponIds, std::vector<int> armorIds, std::vector<std::vector<int>> abilitiesSets, int reactor, int plan, char *name)
{
	for (int chassisId : chassisIds)
	{
		for (int weaponId : weaponIds)
		{
			for (int armorId : armorIds)
			{
				for (std::vector<int> abilitiesSet : abilitiesSets)
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
void checkAndProposePrototype(int factionId, int chassisId, int weaponId, int armorId, std::vector<int> abilityIds, int reactor, int plan, char *name)
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
	
	int allowedAbilityCount = (has_tech(factionId, Rules->tech_preq_allow_2_spec_abil) ? 2 : 1);
	int abilityCount = 0;
	int abilities = 0;
	
	for (int abilityId : abilityIds)
	{
		if (!has_ability(factionId, abilityId))
			continue;
		
		CAbility *ability = getAbility(abilityId);
		
		// not allowed for triad
		
		switch (Chassis[chassisId].triad)
		{
		case TRIAD_LAND:
			if ((ability->flags & AFLAG_ALLOWED_LAND_UNIT) == 0)
				continue;
			break;
			
		case TRIAD_SEA:
			if ((ability->flags & AFLAG_ALLOWED_SEA_UNIT) == 0)
				continue;
			break;
			
		case TRIAD_AIR:
			if ((ability->flags & AFLAG_ALLOWED_AIR_UNIT) == 0)
				continue;
			break;
			
		}
		
		// not allowed for combat unit
		
		if (Weapon[weaponId].offense_value != 0 && (ability->flags & AFLAG_ALLOWED_COMBAT_UNIT) == 0)
			continue;
		
		// not allowed for terraform unit
		
		if (weaponId == WPN_TERRAFORMING_UNIT && (ability->flags & AFLAG_ALLOWED_TERRAFORM_UNIT) == 0)
			continue;
		
		// not allowed for non-combat unit
		
		if (Weapon[weaponId].offense_value == 0 && (ability->flags & AFLAG_ALLOWED_NONCOMBAT_UNIT) == 0)
			continue;
		
		// not allowed for probe team
		
		if (weaponId == WPN_PROBE_TEAM && (ability->flags & AFLAG_NOT_ALLOWED_PROBE_TEAM) != 0)
			continue;
		
		// not allowed for non transport unit
		
		if (weaponId != WPN_TROOP_TRANSPORT && (ability->flags & AFLAG_TRANSPORT_ONLY_UNIT) != 0)
			continue;
		
		// not allowed for fast unit
		
		if (chassisId == CHS_INFANTRY && Chassis[chassisId].speed > 1 && (ability->flags & AFLAG_NOT_ALLOWED_FAST_UNIT) != 0)
			continue;
		
		// not allowed for non probes
		
		if (weaponId != WPN_PROBE_TEAM && (ability->flags & AFLAG_ONLY_PROBE_TEAM) != 0)
			continue;
		
		// all condition passed - add ability
		
		abilities |= (0x1 << abilityId);
		abilityCount++;
		
		if (abilityCount == allowedAbilityCount)
			break;
		
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
	for (int unitId : getFactionUnitIds(factionId, false, true))
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

int getVehicleIdByAIId(int aiId)
{
	// check if ID didn't change
	
	VEH *oldVehicle = getVehicle(aiId);
	
	if (oldVehicle != nullptr && oldVehicle->pad_0 == aiId)
		return aiId;
	
	// otherwise, scan all vehicles
	
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		if (vehicle->pad_0 == aiId)
			return vehicleId;
		
	}
	
	return -1;
	
}

VEH *getVehicleByAIId(int aiId)
{
	return getVehicle(getVehicleIdByAIId(aiId));
}

MAP *getClosestPod(int vehicleId)
{
	MAP *closestPod = nullptr;
	int minTravelTime = INT_MAX;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int x = getX(mapIndex);
		int y = getY(mapIndex);
		MAP *tile = getMapTile(mapIndex);

		// pod

		if (mod_goody_at(x, y) == 0)
			continue;

		// reachable

		if (!isVehicleDestinationReachable(vehicleId, tile, false))
			continue;

		// get distance

		int travelTime = getVehicleATravelTime(vehicleId, tile);

		if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
			continue;

		// update best

		if (travelTime < minTravelTime)
		{
			closestPod = tile;
			minTravelTime = travelTime;
		}

	}

	return closestPod;

}

int getNearestAIFactionBaseRange(int x, int y)
{
	int nearestFactionBaseRange = INT_MAX;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);

		nearestFactionBaseRange = std::min(nearestFactionBaseRange, map_range(x, y, base->x, base->y));

	}

	return nearestFactionBaseRange;

}

int getNearestBaseRange(MAP *tile, std::vector<int> baseIds, bool sameRealm)
{
	bool tileOcean = is_ocean(tile);
	
	int nearestBaseRange = INT_MAX;
	
	for (int baseId : baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		bool baseTileOcean = is_ocean(baseTile);
		
		// same association if requested
		
		if (sameRealm && tileOcean != baseTileOcean)
			continue;
		
		int range = getRange(tile, baseTile);
		
		if (range < nearestBaseRange)
		{
			nearestBaseRange = range;
		}
		
	}
	
	return nearestBaseRange;
	
}

int getFactionBestOffenseValue(int factionId)
{
	int bestWeaponOffenseValue = 0;

	for (int unitId : getFactionUnitIds(factionId, false, false))
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

	for (int unitId : getFactionUnitIds(factionId, false, false))
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

		if (otherVehicle->faction_id != 0 && !isVendetta(otherVehicle->faction_id, vehicle->faction_id))
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
Verifies if unit can get to and stay at this spot without aid of transport.
Gravship can reach everywhere.
Sea unit can reach only their ocean association.
Land unit can reach their land association or any other land association potentially connected by transport.
Land unit also can reach any friendly ocean base.
*/
bool isUnitDestinationReachable(int factionId, int unitId, MAP *origin, MAP *destination, bool immediatelyReachable)
{
	assert(origin != nullptr && destination != nullptr);

	UNIT *unit = getUnit(unitId);
	int chassisSpeed = unit->speed();

	switch (unit->chassis_type)
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
			// get nearest airbase range

			int nearestAirbaseRange = getNearestAirbaseRange(factionId, destination);

			// return true if destination reachable from airbase

			if (chassisSpeed >= nearestAirbaseRange)
			{
				return true;
			}
			else
			{
				return false;
			}

		}
		break;

	case CHS_FOIL:
	case CHS_CRUISER:
		{
			if (isSameOceanAssociation(origin, destination, factionId))
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
			int originAssociation = getAssociation(origin, factionId);
			int destinationAssociation = getAssociation(destination, factionId);

			if (originAssociation == -1 || destinationAssociation == -1)
				return false;

			// land or friendly ocean base
			if (!is_ocean(destination) || isFriendlyBaseAt(destination, factionId))
			{
				// same association (continent or ocean)
				if (originAssociation == destinationAssociation)
				{
					return true;
				}
				else
				{
					if (immediatelyReachable)
					{
						// immediately reachable required
						return isImmediatelyReachableAssociation(destinationAssociation, factionId);
					}
					else
					{
						// potentially reachable required
						return isPotentiallyReachableAssociation(destinationAssociation, factionId);
					}

				}

			}

		}
		break;

	}

	return false;

}

bool isUnitDestinationArtilleryReachable(int factionId, int unitId, MAP *origin, MAP *destination, bool immediatelyReachable)
{
	for (MAP *tile : getRangeTiles(destination, Rules->artillery_max_rng, false))
	{
		if (isUnitDestinationReachable(factionId, unitId, origin, tile, immediatelyReachable))
			return true;
	}

	return false;

}

/*
Verifies if vehicle can get to and stay at this spot without aid of transport.
Gravship can reach everywhere.
Sea unit can reach only their ocean association.
Land unit can reach their land association or any other land association potentially connected by transport.
Land unit also can reach any friendly ocean base.
*/
bool isVehicleDestinationReachable(int vehicleId, MAP *destination, bool immediatelyReachable)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	return isUnitDestinationReachable(vehicle->faction_id, vehicle->unit_id, vehicleTile, destination, immediatelyReachable);

}

/*
Verifies if vehicle can get to and stay at artillery range from destination.
*/
bool isVehicleDestinationArtilleryReachable(int vehicleId, MAP *destination, bool immediatelyReachable)
{
	for (MAP *tile : getRangeTiles(destination, Rules->artillery_max_rng, false))
	{
		if (isVehicleDestinationReachable(vehicleId, tile, immediatelyReachable))
			return true;
	}

	return false;

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
	
	if ((ability_flags & ABL_POLICE_2X) != 0)
	{
		abilitiesString += " POLICE_2X";
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
bool isWtpEnabledFaction(int factionId)
{
	return (factionId != 0 && !is_human(factionId) && ai_enabled(factionId) && conf.ai_useWTPAlgorithms && conf.wtp_enabled_factions[factionId]);
}

int getCoastalBaseOceanAssociation(MAP *tile, int factionId)
{
	int coastalBaseOceanAssociation = -1;

	if (aiData.geography.factions[factionId].coastalBaseOceanAssociations.count(tile) != 0)
	{
		coastalBaseOceanAssociation = aiData.geography.factions[factionId].coastalBaseOceanAssociations.at(tile);
	}

	return coastalBaseOceanAssociation;

}

int getExtendedRegionAssociation(int extendedRegion, int factionId)
{
	return aiData.geography.factions[factionId].extendedRegionAssociations.at(extendedRegion);
}
int getAssociation(MAP *tile, int factionId)
{
	return getExtendedRegionAssociation(getExtendedRegion(tile), factionId);
}

std::set<int> &getAssociationConnections(int association, int factionId)
{
	return aiData.geography.factions[factionId].associationConnections.at(association);
}
std::set<int> &getTileAssociationConnections(MAP *tile, int factionId)
{
	assert(tile >= *MapPtr && tile < *MapPtr + *map_area_tiles);
	return getAssociationConnections(getAssociation(tile, factionId), factionId);
}

bool isConnected(int association1, int association2, int factionId)
{
	return getAssociationConnections(association1, factionId).count(association2) != 0;
}

bool isConnected(MAP *tile1, MAP *tile2, int factionId)
{
	return isConnected(getAssociation(tile1, factionId), getAssociation(tile2, factionId), factionId);
}

bool isSameAssociation(MAP *tile1, MAP *tile2, int factionId)
{
	return isSameLandAssociation(tile1, tile2, factionId) || isSameOceanAssociation(tile1, tile2, factionId);
}

bool isSameLandAssociation(MAP *tile1, MAP *tile2, int factionId)
{
	assert(tile1 >= *MapPtr && tile1 < *MapPtr + *map_area_tiles);
	assert(tile2 >= *MapPtr && tile2 < *MapPtr + *map_area_tiles);
	
	int tile1LandAssociation = getLandAssociation(tile1, factionId);
	int tile2LandAssociation = getLandAssociation(tile2, factionId);
	
	return tile1LandAssociation != -1 && tile2LandAssociation != -1 && tile1LandAssociation == tile2LandAssociation;
	
}

bool isSameOceanAssociation(MAP *tile1, MAP *tile2, int factionId)
{
	if (tile1 == nullptr || tile2 == nullptr)
		return false;
	
	int tile1OceanAssociation = getOceanAssociation(tile1, factionId);
	int tile2OceanAssociation = getOceanAssociation(tile2, factionId);
	
	return tile1OceanAssociation != -1 && tile2OceanAssociation != -1 && tile1OceanAssociation == tile2OceanAssociation;
	
}

bool isImmediatelyReachableAssociation(int association, int factionId)
{
	return aiData.geography.factions[factionId].immediatelyReachableAssociations.count(association) != 0;
}
bool isImmediatelyReachable(MAP *tile, int factionId)
{
	return isImmediatelyReachableAssociation(getAssociation(tile, factionId), factionId);
}

bool isPotentiallyReachableAssociation(int association, int factionId)
{
	return aiData.geography.factions[factionId].potentiallyReachableAssociations.count(association) != 0;
}
bool isPotentiallyReachable(MAP *tile, int factionId)
{
	return isPotentiallyReachableAssociation(getAssociation(tile, factionId), factionId);
}

/*
Returns region for all map tiles.
Returns shifted map index for polar regions.
*/
int getExtendedRegion(MAP *tile)
{
	assert(tile >= *MapPtr && tile < *MapPtr + *map_area_tiles);
	
	if (tile->region == 0x3f || tile->region == 0x7f)
	{
		int mapIndex = getMapIndexByPointer(tile);
		int polarRegionIndex = (mapIndex < *map_area_tiles / 2 ? mapIndex : *map_half_x + mapIndex - (*map_area_tiles - *map_half_x));
		return 0x80 + polarRegionIndex;
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

bool getEdgeRange(MAP *tile)
{
	assert(tile >= *MapPtr && tile < *MapPtr + *map_area_tiles);
	
	int x = getX(tile);
	int y = getY(tile);
	
	int yEdgeRange = std::min(y, (*map_axis_y - 1) - y);
	int xEdgeRange = (*map_toggle_flat ? std::min(x, (*map_axis_x - 1) - x) : INT_MAX);
	
	return std::min(xEdgeRange, yEdgeRange);
		
}

bool isEdgeTile(MAP *tile)
{
	return getEdgeRange(tile) == 0;
}

/*
Returns vehicle region association.
*/
int getVehicleAssociation(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	int vehicleAssociation = -1;

	if (vehicle->triad() == TRIAD_SEA)
	{
		// sea unit can be only in ocean or port
		
		if (is_ocean(vehicleTile))
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
	else
	{
		vehicleAssociation = getAssociation(vehicleTile, vehicle->faction_id);
	}

	return vehicleAssociation;

}

/*
Returns land association.
*/
int getLandAssociation(MAP *tile, int factionId)
{
	int landAssociation = -1;

	if (!is_ocean(tile))
	{
		landAssociation = getAssociation(tile, factionId);
	}

	return landAssociation;

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
Returns land or ocean association based on switch.
*/
int getSurfaceAssociation(MAP *tile, int factionId, bool ocean)
{
	return (ocean ? getOceanAssociation(tile, factionId) : getLandAssociation(tile, factionId));
}

/*
Returns base continent association.
*/
int getBaseLandAssociation(int baseId)
{
	BASE *base = &(Bases[baseId]);
	MAP *baseTile = getBaseMapTile(baseId);

	int baseLandAssociation = -1;

	if (isLandRegion(baseTile->region))
	{
		baseLandAssociation = getAssociation(baseTile, base->faction_id);
	}

	return baseLandAssociation;

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
Computes path movement cost.
action: 0 = no action, 1 = non melee attack action (terraform, build, artillery attack), 2 = melee attack.
*/
std::map<std::tuple<MAP *, MAP *, int, int, int, bool>, int> pathMovementCosts;
int getPathMovementCost(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	executionProfiles["| getPathMovementCost"].start();
	
	// at destination
	
	if (origin == destination)
		return 0;
	
	// check cached value
	
	int chassis_class = getUnit(unitId)->chassis_type;
	if (chassis_class == CHS_CRUISER) chassis_class = CHS_FOIL;
	if (chassis_class == CHS_SPEEDER) chassis_class = CHS_INFANTRY;
	int unit_type = (isNativeUnit(unitId) ? -1 : isFormerUnit(unitId) ? 0 : 1);
	
	std::map<std::tuple<MAP *, MAP *, int, int, int, bool>, int>::iterator pathMovementCostIterator =
		pathMovementCosts.find({origin, destination, factionId, chassis_class, unit_type, ignoreHostile});
	
	if (pathMovementCostIterator != pathMovementCosts.end())
	{
		executionProfiles["| getPathMovementCost"].stop();
		return pathMovementCostIterator->second;
	}
	
	// parameters
	
	int originX = getX(origin);
	int originY = getY(origin);
	int destinationX = getX(destination);
	int destinationY = getY(destination);
	
	// verify path is withing valid association
	
	switch (Units[unitId].triad())
	{
	case TRIAD_LAND:
		{
			// land unit can path only within same association
			
			int originLandAssociation = getLandAssociation(origin, factionId);
			int destiantionLandAssociation = getLandAssociation(destination, factionId);
			
			if (originLandAssociation == -1 || destiantionLandAssociation == -1 || originLandAssociation != destiantionLandAssociation)
			{
				executionProfiles["| getPathMovementCost"].stop();
				return -1;
			}
			
		}
		break;
		
	case TRIAD_SEA:
		{
			// sea unit can move only within the same ocean association
			
			int originOceanAssociation = getOceanAssociation(origin, factionId);
			int destinationOceanAssociation = getOceanAssociation(destination, factionId);
			
			if (originOceanAssociation == -1 || destinationOceanAssociation == -1 || originOceanAssociation != destinationOceanAssociation)
			{
				executionProfiles["| getPathMovementCost"].stop();
				return -1;
			}
			
		}
		break;
		
	}
	
	// disable path reuse
	
	*PATH_flags = 0x80000000;
	
	// process path
	
	int flags = 0xC0 | (!isCombatUnit(unitId) && !isProjectBuilt(factionId, FAC_XENOEMPATHY_DOME) ? 0x10 : 0x00);
	
	int pX = originX;
	int pY = originY;
	int angle = -1;
	
	bool previousZoc = false;
	int maxMovementCost = std::max(*map_axis_x, *map_axis_y) * Rules->mov_rate_along_roads;
	int movementCost = 0;
	
	while (true)
	{
		// exit condition - destination reached
		
		if (map_range(pX, pY, destinationX, destinationY) == 0)
			break;
		
		// get next angle
		
		angle = PATH_find(PATH, pX, pY, destinationX, destinationY, unitId, factionId, flags, angle);
		
		// wrong angle
		
		if (angle == -1)
		{
			executionProfiles["| getPathMovementCost"].stop();
			return -1;
		}
		
		// get next location
		
		int nX = wrap(pX + BaseOffsetX[angle]);
		int nY = pY + BaseOffsetY[angle];
		
		TileMovementInfo &nTileMovementInfo = aiData.getTileInfo(nX, nY).movementInfos[factionId];
		bool blocked = (ignoreHostile ? nTileMovementInfo.blockedNeutral : nTileMovementInfo.blocked);
		bool zoc = (ignoreHostile ? nTileMovementInfo.zocNeutral : nTileMovementInfo.zoc);
		
		// check blocked tile
		
		if (blocked)
		{
			executionProfiles["| getPathMovementCost"].stop();
			return -1;
		}
		
		// check zoc
		
		if (previousZoc && zoc)
		{
			executionProfiles["| getPathMovementCost"].stop();
			return -1;
		}
		
		previousZoc = zoc;
		
		// calculate movement cost
		
		int hexCost = getHexCost(unitId, factionId, pX, pY, nX, nY, 0);
		movementCost += hexCost;
		
		// step to next tile
		
		pX = nX;
		pY = nY;
		
		// step limit reached
		
		if (movementCost > maxMovementCost)
		{
			executionProfiles["| getPathMovementCost"].stop();
			return -1;
		}
		
	}
	
	int returnValue = movementCost;
	
	// cache value
	
	pathMovementCosts[{origin, destination, factionId, chassis_class, unit_type, ignoreHostile}] = returnValue;
	
	executionProfiles["| getPathMovementCost"].stop();
	return returnValue;
	
}

/*
Computes path travel time.
action: take action at the end of movement.
*/
int getPathTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	// movementCost
	
	int movementCost = getPathMovementCost(origin, destination, factionId, unitId, ignoreHostile);
	
	if (movementCost == -1)
		return -1;
	
	// add action if requested
	
	if (action)
	{
		movementCost += Rules->mov_rate_along_roads;
	}
	
	// travelTime
	
	int travelTime = divideIntegerRoundUp(movementCost, speed);
	
	// return
	
	return travelTime;
	
}

/*
Computes unit travel time based on unit chassis type.
*/
int getUnitTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	UNIT *unit = getUnit(unitId);
	
	// compute travel time by chassis type
	
	int travelTime = -1;
	
	switch (unit->chassis_type)
	{
	case CHS_GRAVSHIP:
		travelTime = getGravshipUnitTravelTime(origin, destination, factionId, unitId, ignoreHostile, action, speed);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		travelTime = getAirRangedUnitTravelTime(origin, destination, factionId, unitId, ignoreHostile, action, speed);
		break;
	case CHS_FOIL:
	case CHS_CRUISER:
		travelTime = getSeaUnitTravelTime(origin, destination, factionId, unitId, ignoreHostile, action, speed);
		break;
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		travelTime = getLandUnitTravelTime(origin, destination, factionId, unitId, ignoreHostile, action, speed);
		break;
	}
	
	return travelTime;
	
}

int getGravshipUnitTravelTime(MAP *origin, MAP *destination, int /*factionId*/, int /*unitId*/, bool /*ignoreHostile*/, bool action, int speed)
{
	int movementSpeed = speed / Rules->mov_rate_along_roads;
	int actionMove = (action ? 1 : 0);
	
	int range = getRange(origin, destination);
	
	return divideIntegerRoundUp(range + actionMove, movementSpeed);

}

/*
Estimates air ranged vehicle combat travel time.
Air ranged vehicle hops between airbases.
*/
int getAirRangedUnitTravelTime(MAP *origin, MAP *destination, int /*factionId*/, int unitId, bool /*ignoreHostile*/, bool action, int speed)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	int movementSpeed = speed / Rules->mov_rate_along_roads;
	int transferSpeed = divideIntegerRoundUp(movementSpeed * 3, 4);
	int actionMove = (action == 1 ? 1 : 0);
	
	int movementTurns = 0;
	
	switch (getUnit(unitId)->chassis_type)
	{
	case CHS_NEEDLEJET:
		movementTurns = 1;
		break;
	case CHS_COPTER:
		movementTurns = 2;
		break;
	case CHS_MISSILE:
		movementTurns = 1;
		break;
	}
	
	int movementRange = movementTurns * movementSpeed;
	
	// find optimal airbase to launch strike from
	
	MAP *optimalAirbase = nullptr;
	int optimalAirbaseRange = INT_MAX;
	int optimalAirbaseToTargetRange = 0;
	int optimalVehicleToAirbaseRange = 0;
	
	for (MAP *airbase : aiData.factionInfos[aiFactionId].airbases)
	{
		int airbaseToTargetRange = getRange(airbase, destination);
		
		// reachable
		
		if (airbaseToTargetRange + actionMove > movementRange)
			continue;
		
		int vehicleToAirbaseRange = getRange(origin, airbase);
		
		if (vehicleToAirbaseRange < optimalAirbaseRange)
		{
			optimalAirbase = airbase;
			optimalAirbaseRange = vehicleToAirbaseRange;
			optimalAirbaseToTargetRange = airbaseToTargetRange;
			optimalVehicleToAirbaseRange = vehicleToAirbaseRange;
		}
		
	}
	
	// not found
	
	if (optimalAirbase == nullptr)
		return -1;
	
	// estimate travel time
	
	return
		+ divideIntegerRoundUp(optimalVehicleToAirbaseRange, transferSpeed)
		+ divideIntegerRoundUp(optimalAirbaseToTargetRange, movementSpeed)
	;
	
}

/*
Estimates sea vehicle combat travel time not counting any obstacles.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getSeaUnitTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	// verify same ocean association
	
	if (!isSameOceanAssociation(origin, destination, factionId))
		return -1;
	
	// travel time
	
	return getPathTravelTime(origin, destination, factionId, unitId, ignoreHostile, action, speed);
	
}

/*
Estimates land vehicle combat travel time not counting any obstacles.
Land vehicle travels within its association and fights through enemies.
Optionally it can be transported across associations.
*/
int getLandUnitTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, bool ignoreHostile, bool action, int speed)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	int speedOnLand = speed / Rules->mov_rate_along_roads;
	int speedOnRoad = speed / conf.tube_movement_rate_multiplier;
	int speedOnTube = speed;
	
	// roughly estimate beeline travel time if too far
	
	int travelTime;
	
	if (isSameLandAssociation(origin, destination, factionId)) // same continent
	{
		travelTime = getPathTravelTime(origin, destination, factionId, unitId, ignoreHostile, action, speed);
	}
	else
	{
		// alien cannot travel between landmasses
		
		if (factionId == 0)
			return -1;
		
		// both associations are reachable
		
		if (!isPotentiallyReachable(origin, factionId) || !isPotentiallyReachable(destination, factionId))
			return -1;
		
		// range
		
		int range = getRange(origin, destination);
		
		// get cross ocean association
		
		int crossOceanAssociation = getCrossOceanAssociation(origin, getAssociation(destination, factionId), factionId);
		
		if (crossOceanAssociation == -1)
			return -1;
		
		// transport speed
		
		int seaTransportMovementSpeed = getSeaTransportChassisSpeed(crossOceanAssociation, factionId, false);
		
		if (seaTransportMovementSpeed == -1)
			return -1;
		
		// estimate straight line travel time (very roughly)
		
		double averageVehicleMovementSpeed =
			speedOnLand
			+ (speedOnRoad - speedOnLand) * std::min(1.0, aiData.roadCoverage / 0.5)
			+ (speedOnTube - speedOnRoad) * std::min(1.0, aiData.tubeCoverage / 0.5)
		;
		double averageMovementSpeed = 0.5 * (averageVehicleMovementSpeed + seaTransportMovementSpeed);
		
		travelTime = (int)((double)range / (0.75 * averageMovementSpeed));
		
		// add ferry wait time if not in ocean already
		
		if (!is_ocean(origin) || !isBaseAt(origin))
		{
			travelTime += 10;
		}
		
	}
	
	return travelTime;
	
}

int getVehicleTravelTime(int vehicleId, MAP *origin, MAP *destination, bool ignoreHostile, int action)
{
	VEH *vehicle = getVehicle(vehicleId);
	int speed = getVehicleSpeed(vehicleId);
	return getUnitTravelTime(origin, destination, vehicle->faction_id, vehicle->unit_id, ignoreHostile, action, speed);
}

int getVehicleTravelTime(int vehicleId, MAP *destination, bool ignoreHostile, int action)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return getVehicleTravelTime(vehicleId, vehicleTile, destination, ignoreHostile, action);
}

int getVehicleTravelTime(int vehicleId, MAP *destination)
{
	return getVehicleTravelTime(vehicleId, destination, false, 0);
}

int getVehicleMeleeAttackTravelTime(int vehicleId, MAP *target, bool ignoreHostile)
{
	// attackPosition
	
	MAP *attackPosition = getVehicleMeleeAttackPosition(vehicleId, target, ignoreHostile);
	
	if (attackPosition == nullptr)
		return -1;
	
	// travelTime
	
	return getVehicleTravelTime(vehicleId, attackPosition, ignoreHostile, true);
	
}

int getVehicleArtilleryAttackTravelTime(int vehicleId, MAP *target, bool ignoreHostile)
{
	// attackPosition
	
	MAP *attackPosition = getVehicleArtilleryAttackPosition(vehicleId, target, ignoreHostile);
	
	if (attackPosition == nullptr)
		return -1;
	
	// travelTime
	
	return getVehicleTravelTime(vehicleId, attackPosition, ignoreHostile, true);
	
}

int getVehicleTravelTimeByTaskType(int vehicleId, MAP *target, TaskType taskType)
{
	int travelTime;
	
	switch (taskType)
	{
	case TT_MELEE_ATTACK:
		travelTime = getVehicleMeleeAttackTravelTime(vehicleId, target, false);
		break;
	case TT_LONG_RANGE_FIRE:
		travelTime = getVehicleArtilleryAttackTravelTime(vehicleId, target, false);
		break;
	case TT_BUILD:
	case TT_TERRAFORM:
		travelTime = getVehicleTravelTime(vehicleId, target, false, true);
		break;
	default:
		travelTime = getVehicleTravelTime(vehicleId, target);
	}
	
	return travelTime;
	
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

		debug("\t[%4d] (%3d,%3d) -> (%3d,%3d)\n", vehicleId, Vehicles[vehicleId].x, Vehicles[vehicleId].y, getX(location), getY(location));

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

bool compareIdIntValueAscending(const IdIntValue &a, const IdIntValue &b)
{
	return a.value < b.value;
}

bool compareIdIntValueDescending(const IdIntValue &a, const IdIntValue &b)
{
	return a.value > b.value;
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
Calculates relative unit strength for melee attack.
How many defender units can attacker destroy until own complete destruction.
*/
double getMeleeRelativeUnitStrength(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId)
{
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = getUnitOffenseValue(attackerUnitId);
	int defenderDefenseValue = getUnitDefenseValue(defenderUnitId);
	
	// illegal arguments - should not happen
	
	if (attackerOffenseValue == 0 || defenderDefenseValue == 0)
		return 0.0;
	
	// attacker should be melee unit
	
	if (!isMeleeUnit(attackerUnitId))
		return 0.0;
	
	// calculate relative strength
	
	double relativeStrength;
	
	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
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
			relativeStrength = 0.0;
		
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
		
		// hybrid item
		
		if (attackerUnit->weapon_type == WPN_RESONANCE_LASER || attackerUnit->weapon_type == WPN_RESONANCE_LASER)
		{
			relativeStrength *= 1.25;
		}
		
		if (defenderUnit->armor_type == ARM_RESONANCE_3_ARMOR || defenderUnit->armor_type == ARM_RESONANCE_8_ARMOR)
		{
			relativeStrength /= 1.25;
		}
		
		// PLANET bonus
		
		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
 		if (conf.planet_combat_bonus_on_defense)
		{
			relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);
		}
		
	}
	else
	{
		// conventional combat
		
		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;
		
		// reactor
		
		if (!isReactorIgnoredInConventionalCombat())
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
		
		// fanatic bonus
		
		relativeStrength *= getFactionFanaticBonusMultiplier(attackerFactionId);
		
	}
	
	// ability applied regardless of combat type
	
	if
	(
		attackerUnit->triad() == TRIAD_LAND && attackerUnit->speed() > 1
		&& unit_has_ability(defenderUnitId, ABL_COMM_JAMMER)
		&& !unit_has_ability(attackerUnitId, ABL_DISSOCIATIVE_WAVE)
	)
	{
		relativeStrength /= getPercentageBonusMultiplier(Rules->combat_comm_jammer_vs_mobile);
	}
	
	if
	(
		attackerUnit->triad() == TRIAD_AIR
		&& unit_has_ability(defenderUnitId, ABL_AAA)
		&& !unit_has_ability(attackerUnitId, ABL_DISSOCIATIVE_WAVE)
	)
	{
		relativeStrength /= getPercentageBonusMultiplier(Rules->combat_AAA_bonus_vs_air);
	}
	
	if (unit_has_ability(attackerUnitId, ABL_SOPORIFIC_GAS) && !isNativeUnit(defenderUnitId))
	{
		relativeStrength *= 1.25;
	}
	
	// faction bonuses
	
	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);
	
	// artifact and no armor probe are explicitly weak
	
	if (defenderUnit->weapon_type == WPN_ALIEN_ARTIFACT || defenderUnit->weapon_type == WPN_PROBE_TEAM && defenderUnit->armor_type == ARM_NO_ARMOR)
	{
		relativeStrength *= 50.0;
	}
	
	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *current_turn <= conf.aliens_fight_half_strength_unit_turn)
	{
		relativeStrength /= 2.0;
	}
	if (defenderFactionId == 0 && *current_turn <= conf.aliens_fight_half_strength_unit_turn)
	{
		relativeStrength *= 2.0;
	}
	
	return relativeStrength;
	
}

/*
Calculates relative unit strength for artillery duel attack.
How many defender units can attacker destroy until own complete destruction.
*/
double getArtilleryDuelRelativeUnitStrength(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId)
{
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = getUnitOffenseValue(attackerUnitId);
	int defenderDefenseValue = getUnitDefenseValue(defenderUnitId);

	// illegal arguments - should not happen

	if (attackerOffenseValue == 0 || defenderDefenseValue == 0)
		return 0.0;

	// both units should be artillery capable

	if (!isArtilleryUnit(attackerUnitId) || !isArtilleryUnit(defenderUnitId))
		return 0.0;

	// calculate relative strength

	double relativeStrength;

	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
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

		// PLANET bonus

		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
		relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);

	}
	else
	{
		// [WTP] uses both weapon and armor for conventional artillery duel
		
		if (conf.conventional_artillery_duel_uses_weapon_and_armor)
		{
			attackerOffenseValue = Weapon[attackerUnit->weapon_type].offense_value + Armor[attackerUnit->armor_type].defense_value;
			defenderDefenseValue = Weapon[defenderUnit->weapon_type].offense_value + Armor[defenderUnit->armor_type].defense_value;
		}
		else
		{
			attackerOffenseValue = Weapon[attackerUnit->weapon_type].offense_value;
			defenderDefenseValue = Weapon[defenderUnit->weapon_type].offense_value;
		}

		// get relative strength

		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;

		// reactor

		if (!isReactorIgnoredInConventionalCombat())
		{
			relativeStrength *= (double)attackerUnit->reactor_type / (double)defenderUnit->reactor_type;
		}

	}

	// ability applied regardless of combat type

	// faction bonuses

	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);

	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *current_turn <= conf.aliens_fight_half_strength_unit_turn)
	{
		relativeStrength /= 2.0;
	}
	if (defenderFactionId == 0 && *current_turn <= conf.aliens_fight_half_strength_unit_turn)
	{
		relativeStrength *= 2.0;
	}
	
	// land vs. sea guns bonus
	
	if (attackerUnit->triad() == TRIAD_LAND && defenderUnit->triad() == TRIAD_SEA)
	{
		relativeStrength *= getPercentageBonusMultiplier(Rules->combat_land_vs_sea_artillery);
	}
	else if (attackerUnit->triad() == TRIAD_SEA && defenderUnit->triad() == TRIAD_LAND)
	{
		relativeStrength /= getPercentageBonusMultiplier(Rules->combat_land_vs_sea_artillery);
	}

	return relativeStrength;

}

/*
Calculates relative unit strength for bombardment attack.
How many defender units can attacker destroy until own complete destruction.
*/
double getBombardmentRelativeUnitStrength(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId)
{
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = getUnitOffenseValue(attackerUnitId);
	int defenderDefenseValue = getUnitDefenseValue(defenderUnitId);
	
	// illegal arguments - should not happen
	
	if (attackerOffenseValue == 0 || defenderDefenseValue == 0)
		return 0.0;
	
	// attacker should be artillery capable and defender should not
	
	if (!(isArtilleryUnit(attackerUnitId) && !isArtilleryUnit(defenderUnitId)))
		return 0.0;
	
	// calculate relative strength
	
	double relativeStrength;
	
	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
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

		// PLANET bonus

		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
		relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);

	}
	else
	{
		// get relative strength
		
		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;
		
		// reactor
		
		if (!isReactorIgnoredInConventionalCombat())
		{
			relativeStrength *= (double)attackerUnit->reactor_type / (double)defenderUnit->reactor_type;
		}
		
	}
	
	// ability applied regardless of combat type
	
	// faction bonuses
	
	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);
	
	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *current_turn <= conf.aliens_fight_half_strength_unit_turn)
	{
		relativeStrength /= 2.0;
	}
	if (defenderFactionId == 0 && *current_turn <= conf.aliens_fight_half_strength_unit_turn)
	{
		relativeStrength *= 2.0;
	}
	
	return relativeStrength;
	
}

/*
Calculates relative bombardment damage for units.
How many defender units can attacker destroy with single shot.
*/
double getUnitBombardmentDamage(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId)
{
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = Weapon[attackerUnit->weapon_type].offense_value;
	int defenderDefenseValue = Armor[defenderUnit->armor_type].defense_value;

	// illegal arguments - should not happen

	if (attackerOffenseValue == 0 || defenderDefenseValue == 0)
		return 0.0;
	
	// attacker should be artillery capable and defender should not and should be surface unit
	
	if (!isArtilleryUnit(attackerUnitId) || isArtilleryUnit(defenderUnitId) || defenderUnit->triad() == TRIAD_AIR)
		return 0.0;

	// calculate relative damage

	double relativeStrength;

	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
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

		// PLANET bonus

		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
		relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);

	}
	else
	{
		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;

		// reactor

		if (!isReactorIgnoredInConventionalCombat())
		{
			relativeStrength *= 1.0 / (double)defenderUnit->reactor_type;
		}
		
	}
	
	// faction bonuses

	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);
	
	// artifact and no armor probe are explicitly weak
	
	if (defenderUnit->weapon_type == WPN_ALIEN_ARTIFACT || defenderUnit->weapon_type == WPN_PROBE_TEAM && defenderUnit->armor_type == ARM_NO_ARMOR)
	{
		relativeStrength *= 50.0;
	}
	
	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *current_turn <= conf.aliens_fight_half_strength_unit_turn)
	{
		relativeStrength /= 2.0;
	}
	if (defenderFactionId == 0 && *current_turn <= conf.aliens_fight_half_strength_unit_turn)
	{
		relativeStrength *= 2.0;
	}
	
	// artillery damage numerator/denominator
	
	relativeStrength *= (double)Rules->artillery_dmg_numerator / (double)Rules->artillery_dmg_denominator;
	
	// divide by 10 to convert damage to units destroyed
	
	return relativeStrength / 10.0;
	
}

/*
Calculates relative bombardment damage for units.
How many defender units can attacker destroy with single shot.
*/
double getVehicleBombardmentDamage(int attackerVehicleId, int defenderVehicleId)
{
	VEH *attackerVehicle = getVehicle(attackerVehicleId);
	VEH *defenderVehicle = getVehicle(defenderVehicleId);
	
	double unitBombardmentDamage =
		getUnitBombardmentDamage(attackerVehicle->unit_id, attackerVehicle->faction_id, defenderVehicle->unit_id, defenderVehicle->faction_id);
	
	double vehicleBombardmentDamage =
		unitBombardmentDamage
		* getVehicleMoraleMultiplier(attackerVehicleId)
		/ getVehicleMoraleMultiplier(defenderVehicleId)
	;
	
	return vehicleBombardmentDamage;
	
}

/*
Returns vehicle specific psi offense modifier to unit strength.
*/
double getVehiclePsiOffenseModifier(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	double psiOffenseModifier = 1.0;

	// morale modifier

	psiOffenseModifier *= getVehicleMoraleMultiplier(vehicleId);

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

	psiDefenseModifier *= getVehicleMoraleMultiplier(vehicleId);

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

	conventionalOffenseModifier *= getVehicleMoraleMultiplier(vehicleId);

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

	conventionalDefenseModifier *= getVehicleMoraleMultiplier(vehicleId);

	// faction defense

	conventionalDefenseModifier *= aiData.factionInfos[vehicle->faction_id].defenseMultiplier;

	// return modifier

	return conventionalDefenseModifier;

}

MAP *getSafeLocation(int vehicleId)
{
	return getSafeLocation(vehicleId, 0);
}

/*
Searches for closest friendly base or not blocked bunker withing range if range is given.
Otherwies, searches for any closest safe location (above options + not warzone).
*/
MAP *getSafeLocation(int vehicleId, int baseRange)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int vehicleAssociation = getVehicleAssociation(vehicleId);

	// exclude transported land vehicle

	if (isLandVehicleOnTransport(vehicleId))
		return nullptr;

	// do not search for safe location for vehicles with range
	// they have to return to safe location anyway

	if (getVehicleChassis(vehicleId)->range > 0)
		return nullptr;

	// search best safe location (base, bunker)

	for (MAP *tile : getRangeTiles(vehicleTile, baseRange, true))
	{
		int tileAssociation = getAssociation(tile, vehicle->faction_id);
		int tileOceanAssociation = getOceanAssociation(tile, vehicle->faction_id);
		TileMovementInfo &tileMovementInfo = aiData.getTileInfo(tile).movementInfos[vehicle->faction_id];

		// within vehicle association for surface vechile

		if
		(
			triad == TRIAD_LAND && vehicleAssociation != tileAssociation
			||
			triad == TRIAD_SEA && vehicleAssociation != tileOceanAssociation
		)
			continue;

		// search for best safe location if not found yet

		if (map_has_item(tile, TERRA_BASE_IN_TILE) && isFriendly(vehicle->faction_id, tile->owner))
		{
			// friendly base is the safe location

			return tile;

		}

		if ((triad == TRIAD_LAND || triad == TRIAD_SEA) && map_has_item(tile, TERRA_BUNKER) && !tileMovementInfo.blocked)
		{
			// not blocked bunker is a safe location for surface vehicle

			return tile;

		}

	}

	// search closest safe location

	MAP *closestSafeLocation = nullptr;
	int closestSafeLocationRange = INT_MAX;
	int closestSafeLocationTravelTime = INT_MAX;

	for (MAP *tile : getRangeTiles(vehicleTile, MAX_SAFE_LOCATION_SEARCH_RANGE, true))
	{
		int x = getX(tile);
		int y = getY(tile);
		int tileAssociation = getAssociation(tile, vehicle->faction_id);
		int tileOceanAssociation = getOceanAssociation(tile, vehicle->faction_id);
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[vehicle->faction_id];

		// within vehicle current association for surface vechile

		if
		(
			triad == TRIAD_LAND && vehicleAssociation != tileAssociation
			||
			triad == TRIAD_SEA && vehicleAssociation != tileOceanAssociation
		)
			continue;

		if (map_has_item(tile, TERRA_BASE_IN_TILE) && isFriendly(vehicle->faction_id, tile->owner))
		{
			// base is a best safe location
		}
		else if ((triad == TRIAD_LAND || triad == TRIAD_SEA) && map_has_item(tile, TERRA_BUNKER) && !tileMovementInfo.blocked)
		{
			// not blocked bunker is a safe location for surface vehicle
		}
		else
		{
			// not blocked

			if (tileMovementInfo.blocked)
				continue;

			// not zoc

			if (tileMovementInfo.zoc)
				continue;

			// not warzone

			if (tileInfo.warzone)
				continue;

			// not fungus for surface vehicle unless native or XENOEMPATY_DOME or road

			if
			(
				map_has_item(tile, TERRA_FUNGUS)
				&&
				!(isNativeVehicle(vehicleId) || isFactionHasProject(vehicle->faction_id, FAC_XENOEMPATHY_DOME) || map_has_item(tile, TERRA_ROAD))
			)
				continue;

			// everything else is safe

		}

		// get range

		int range = map_range(vehicle->x, vehicle->y, x, y);

		// break cycle if farther than closest location

		if (range > closestSafeLocationRange)
			break;

		// get travel time

		int travelTime = getVehicleATravelTime(vehicleId, tile);
		
		if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
			continue;

		// update best

		if (range <= closestSafeLocationRange && travelTime < closestSafeLocationTravelTime)
		{
			closestSafeLocation = tile;
			closestSafeLocationRange = range;
			closestSafeLocationTravelTime = travelTime;
		}

	}

	// return closest safe location

	return closestSafeLocation;

}

/*
Searches for item closest to origin within given realm withing given range.
realm: 0 = land, 1 = ocean, 2 = both
Ignore blocked locations and warzones if requested.
factionId = aiFactionId
*/
MapValue findClosestItemLocation(int vehicleId, terrain_flags item, int maxSearchRange, bool avoidWarzone)
{
	VEH *vehicle = getVehicle(vehicleId);
	int triad = vehicle->triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	MAP *closestItemLocation = nullptr;
	int closestItemLocationTravelTime = INT_MAX;
	
	for (MAP *tile : getRangeTiles(vehicleTile, maxSearchRange, true))
	{
		bool ocean = is_ocean(tile);
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[aiFactionId];
		
		// corresponding realm
		
		if (triad == TRIAD_LAND && ocean || triad == TRIAD_SEA && !ocean)
			continue;
		
		// item
		
		if (!map_has_item(tile, item))
			continue;
		
		// exclude blocked location
		
		if (tileMovementInfo.blocked)
			continue;
		
		// exclude warzone if requested
		
		if (avoidWarzone && isWarzone(tile))
			continue;
		
		// get travel time
		
		int travelTime = getVehicleATravelTime(vehicleId, tile);
		
		if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
			continue;
		
		// update best

		if (travelTime < closestItemLocationTravelTime)
		{
			closestItemLocation = tile;
			closestItemLocationTravelTime = travelTime;
		}
		
	}
	
	return {closestItemLocation, closestItemLocationTravelTime};
	
}

double getAverageSensorMultiplier(MAP *tile)
{
	bool ocean = is_ocean(tile);

	double averageSensorMultiplier;

	if (ocean)
	{
		if (conf.combat_bonus_sensor_ocean)
		{
			if (conf.combat_bonus_sensor_offense)
			{
				averageSensorMultiplier = getPercentageBonusMultiplier(Rules->combat_defend_sensor);
			}
			else
			{
				averageSensorMultiplier = (1.0 + getPercentageBonusMultiplier(Rules->combat_defend_sensor)) / 2.0;
			}
		}
		else
		{
			averageSensorMultiplier = 1.0;
		}
	}
	else
	{
		if (conf.combat_bonus_sensor_offense)
		{
			averageSensorMultiplier = getPercentageBonusMultiplier(Rules->combat_defend_sensor);
		}
		else
		{
			averageSensorMultiplier = (1.0 + getPercentageBonusMultiplier(Rules->combat_defend_sensor)) / 2.0;
		}
	}

	return averageSensorMultiplier;

}

MAP *getNearestAirbase(int factionId, MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);

	MAP *nearestAirbase = nullptr;
	int nearestAirbaseRange = INT_MAX;

	for (MAP *airbase : aiData.factionInfos[factionId].airbases)
	{
		int airbaseX = getX(airbase);
		int airbaseY = getY(airbase);

		int range = map_range(x, y, airbaseX, airbaseY);

		if (range < nearestAirbaseRange)
		{
			nearestAirbase = airbase;
			nearestAirbaseRange = std::min(nearestAirbaseRange, range);
		}

	}

	return nearestAirbase;

}

int getNearestAirbaseRange(int factionId, MAP *tile)
{
	MAP *nearestAirbase = getNearestAirbase(factionId, tile);

	if (nearestAirbase == nullptr)
		return INT_MAX;

	return map_range(getX(tile), getY(tile), getX(nearestAirbase), getY(nearestAirbase));

}

bool isOffensiveUnit(int unitId)
{
	UNIT *unit = getUnit(unitId);
	int offenseValue = getUnitOffenseValue(unitId);
	int defenseValue = getUnitDefenseValue(unitId);

	// non infantry unit is always offensive

	if (unit->chassis_type != CHS_INFANTRY)
		return true;

	// psi offense makes it offensive unit

	if (offenseValue < 0)
		return true;

	// psi defense and conventional offense makes it NOT offensive unit

	if (defenseValue < 0)
		return false;

	// conventional offense should be greater or equal than conventional defense

	return offenseValue >= defenseValue;

}

bool isOffensiveVehicle(int vehicleId)
{
	return isOffensiveUnit(getVehicle(vehicleId)->unit_id);
}

double getExponentialCoefficient(double scale, double travelTime)
{
	return exp(- travelTime / scale);
}

/*
Improved psych allocation routine.
*/
void modifiedAllocatePsych(int factionId)
{
	debug("modifiedAllocatePsych - %s\n", getMFaction(factionId)->noun_faction);

	Faction *faction = getFaction(factionId);

	// get ai faction bases

	std::vector<int> baseIds;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = getBase(baseId);

		// ai faction base

		if (base->faction_id != factionId)
			continue;

		// store faction base

		baseIds.push_back(baseId);

	}

	// record current allocation

	int currentAllocationPsych = faction->SE_alloc_psych;
	int currentAllocationLabs = faction->SE_alloc_labs;

	// process all allocation psych and pick best one

	int bestAllocationPsych = 0;
	double bestTotalBenefit = 0.0;

	for (int allocationPsych = 0; allocationPsych <= 6; allocationPsych++)
	{
		debug("\tallocationPsych = %d\n", allocationPsych);

		// set allocation psych

		faction->SE_alloc_psych = allocationPsych;
		faction->SE_alloc_labs = (10 - faction->SE_alloc_psych) / 2;

		// process bases

		double totalBenefit = 0.0;

		for (int baseId : baseIds)
		{
			BASE *base = getBase(baseId);

			//compute base

			computeBase(baseId, true);

			// collect output

			int nutrientSurplus = base->nutrient_surplus;
			int mineralSurplus = base->mineral_surplus;
			int economyTotal = base->economy_total;
			int labsTotal = base->labs_total;

			// compute benefit

			double benefit = getYieldScore(nutrientSurplus, mineralSurplus, economyTotal + economyTotal);

			debug
			(
				"\t\t%-25s"
				" benefit=%7.2f"
				" nutrientSurplus=%3d"
				" mineralSurplus=%3d"
				" economyTotal=%3d"
				" labsTotal=%3d"
				"\n"
				, base->name
				, benefit
				, nutrientSurplus
				, mineralSurplus
				, economyTotal
				, labsTotal
			);

			// accumulate total

			totalBenefit += benefit;

		}

		// update best values

		if (totalBenefit > bestTotalBenefit)
		{
			bestAllocationPsych = allocationPsych;
			bestTotalBenefit = totalBenefit;
		}

	}

	debug("\tbestAllocationPsych=%d\n", bestAllocationPsych);

	// set bestAllocationPsych and reset all bases

	if (bestAllocationPsych == currentAllocationPsych)
	{
		// restore current allocation if no changes

		faction->SE_alloc_psych = currentAllocationPsych;
		faction->SE_alloc_labs = currentAllocationLabs;

	}
	else
	{
		// otherwise, set new

		faction->SE_alloc_psych = bestAllocationPsych;
		faction->SE_alloc_labs = (10 - faction->SE_alloc_psych) / 2;

	}

	for (int baseId : baseIds)
	{
		//compute base

		computeBase(baseId, true);

	}

}

/*
Estimates turns to build given item at given base.
*/
int getBaseItemBuildTime(int baseId, int item, bool accumulatedMinerals)
{
	BASE *base = &(Bases[baseId]);
	
	if (base->mineral_surplus <= 0)
		return 9999;
	
	int mineralCost = std::max(0, getBaseMineralCost(baseId, item) - (accumulatedMinerals ? base->minerals_accumulated : 0));
	
	return divideIntegerRoundUp(mineralCost, base->mineral_surplus);
	
}
int getBaseItemBuildTime(int baseId, int item)
{
	return getBaseItemBuildTime(baseId, item, false);
}

double getStatisticalBaseSize(double age)
{
	double adjustedAge = std::max(0.0, age - conf.ai_base_size_b);
	double baseSize = std::max(1.0, conf.ai_base_size_a * sqrt(adjustedAge) + conf.ai_base_size_c);
	return baseSize;
}

/*
Estimates base size based on statistics.
*/
double getBaseEstimatedSize(double currentSize, double currentAge, double futureAge)
{
	return currentSize * (getStatisticalBaseSize(futureAge) / getStatisticalBaseSize(currentAge));
}

double getBaseStatisticalWorkerCount(double age)
{
	double adjustedAge = std::max(0.0, age - conf.ai_base_size_b);
	return std::max(1.0, conf.ai_base_size_a * sqrt(adjustedAge) + conf.ai_base_size_c);
}

/*
Estimates base size based on statistics.
*/
double getBaseStatisticalProportionalWorkerCountIncrease(double currentAge, double futureAge)
{
	return getBaseStatisticalWorkerCount(futureAge) / getBaseStatisticalWorkerCount(currentAge);
}

/*
Projects base population in given number of turns.
*/
int getBaseProjectedSize(int baseId, int turns)
{
	BASE *base = &(Bases[baseId]);
	
	int nutrientCostFactor = cost_factor(base->faction_id, 0, baseId);
	int nutrientsAccumulated = base->nutrients_accumulated + base->nutrient_surplus * turns;
	int currentPopulation = base->pop_size;
	int projectedPopulation = currentPopulation;
	
	while (nutrientsAccumulated >= nutrientCostFactor * (projectedPopulation + 1))
	{
		nutrientsAccumulated -= nutrientCostFactor * (projectedPopulation + 1);
		projectedPopulation++;
	}
	
	// do not go over population limit
	
	int populationLimit = getBasePopulationLimit(baseId);
	projectedPopulation = std::min(projectedPopulation, std::max(currentPopulation, populationLimit));
	
	return projectedPopulation;
	
}

/*
Estimates time for base to reach given population size.
*/
int getBaseGrowthTime(int baseId, int targetSize)
{
	BASE *base = &(Bases[baseId]);

	// already reached

	if (base->pop_size >= targetSize)
		return 0;

	// calculate total number of nutrient rows to fill

	int totalNutrientRows = (targetSize + base->pop_size + 1) * (targetSize - base->pop_size) / 2;

	// calculate total number of nutrients

	int totalNutrients = totalNutrientRows * cost_factor(base->faction_id, 0, baseId);

	// calculate time to accumulate these nutrients

	int projectedTime = (totalNutrients + (base->nutrient_surplus - 1)) / std::max(1, base->nutrient_surplus) + (targetSize - base->pop_size);

	return projectedTime;

}

/*
Estimates minimal number of doctors base should maintain to avoid riot.
*/
int getBaseOptimalDoctors(int baseId)
{
	BASE *base = getBase(baseId);

	// punishment sphere disables riot

	if (has_facility(baseId, FAC_PUNISHMENT_SPHERE))
		return 0;

	// store current base specialists parameters

	int specialistParameters[] = {base->specialist_total, base->specialist_unk_1, base->specialist_types[0], base->specialist_types[1]};
	int doctors = getBaseDoctors(baseId);

	// optimal number of doctors

	if (base->drone_total == base->talent_total)
	{
		return doctors;
	}

	// attempt to modify number of doctors to achieve optimal content

	int optimalDoctors;
	bool baseModified = false;

	while (true)
	{
		if (base->drone_total > base->talent_total)
		{
			// too many drones - try to increase number of doctors

			// population limit is reached

			if (doctors == base->pop_size)
			{
				optimalDoctors = doctors;
				break;
			}

			// specialist limit is reached

			if (doctors == 16)
			{
				optimalDoctors = doctors;
				break;
			}

			// add doctor

			addBaseDoctor(baseId);
			doctors++;
			computeBase(baseId, true);
			baseModified = true;

			// reached happiness

			if (base->drone_total <= base->talent_total)
			{
				optimalDoctors = doctors;
				break;
			}

		}
		else
		{
			// too little drones - try to reduce number of doctors

			// no doctors left

			if (doctors == 0)
			{
				optimalDoctors = 0;
				break;
			}

			// remove doctor

			removeBaseDoctor(baseId);
			doctors--;
			computeBase(baseId, true);
			baseModified = true;

			// reached optimal number of doctors

			if (base->drone_total == base->talent_total)
			{
				optimalDoctors = doctors;
				break;
			}

			// too little doctors

			if (base->drone_total > base->talent_total)
			{
				optimalDoctors = doctors + 1;
				break;
			}

		}

	}

	// restore base

	if (baseModified)
	{
		base->specialist_total = specialistParameters[0];
		base->specialist_unk_1 = specialistParameters[1];
		base->specialist_types[0] = specialistParameters[2];
		base->specialist_types[1] = specialistParameters[3];
		computeBase(baseId, true);

	}

	// return optimal number of doctors

	return optimalDoctors;

}

/*
Calculates aggreated minerals, economy, labs resource score.
*/
double getResourceScore(double minerals, double energy)
{
	return getYieldScore(0.0, minerals, energy);
}

void populatePathInfoParameters()
{
	populateHexCosts();
}

/*
Populates hex cost for all triads.
*/
void populateHexCosts()
{
	bool TRACE = DEBUG && false;
	
	debug("populateHexCosts TRACE=%d\n", TRACE);
	
	for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
	{
		int tileX = getX(tile);
		int tileY = getY(tile);
		bool tileOcean = is_ocean(tile);
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		for (unsigned int angle = 0; angle < ANGLE_COUNT; angle++)
		{
			MAP *adjacentTile = getTileByAngle(tile, angle);
			
			if (adjacentTile == nullptr)
				continue;
			
			int adjacentTileX = getX(adjacentTile);
			int adjacentTileY = getY(adjacentTile);
			bool adjacentTileOcean = is_ocean(adjacentTile);
			
			// air movement type
			
			tileInfo.hexCosts[angle][MT_AIR] = Rules->mov_rate_along_roads;
			
			// sea vehicle moves on ocean or from/to base
			
			if (tileOcean && adjacentTileOcean || tileOcean && isBaseAt(adjacentTile) || isBaseAt(tile) && adjacentTileOcean)
			{
				int nativeHexCost = getHexCost(BSC_ISLE_OF_THE_DEEP, -1, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				int regularHexCost = getHexCost(BSC_UNITY_GUNSHIP, -1, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				
				tileInfo.hexCosts[angle][MT_SEA_NATIVE] = nativeHexCost;
				tileInfo.hexCosts[angle][MT_SEA_REGULAR] = regularHexCost;
				
			}
			else
			{
				tileInfo.hexCosts[angle][MT_SEA_NATIVE] = -1;
				tileInfo.hexCosts[angle][MT_SEA_REGULAR] = -1;
			}
			
			// land vehicle moves on land
			
			if (!tileOcean && !adjacentTileOcean)
			{
				int nativeHexCost = getHexCost(BSC_MIND_WORMS, -1, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				int regularHexCost = getHexCost(BSC_SCOUT_PATROL, -1, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				
				// easy movement reduces fungus entry
				
				int easyHexCost =
					regularHexCost >= 3 * Rules->mov_rate_along_roads && map_has_item(adjacentTile, TERRA_FUNGUS) ?
						Rules->mov_rate_along_roads * 4 / 3
						:
						regularHexCost
				;
				
				tileInfo.hexCosts[angle][MT_LAND_NATIVE_HOVER] = std::min(Rules->mov_rate_along_roads, nativeHexCost);
				tileInfo.hexCosts[angle][MT_LAND_NATIVE] = nativeHexCost;
				tileInfo.hexCosts[angle][MT_LAND_HOVER] = std::min(Rules->mov_rate_along_roads, regularHexCost);
				tileInfo.hexCosts[angle][MT_LAND_EASY] = easyHexCost;
				tileInfo.hexCosts[angle][MT_LAND_REGULAR] = regularHexCost;
				
			}
			else
			{
				tileInfo.hexCosts[angle][MT_LAND_NATIVE_HOVER] = -1;
				tileInfo.hexCosts[angle][MT_LAND_NATIVE] = -1;
				tileInfo.hexCosts[angle][MT_LAND_HOVER] = -1;
				tileInfo.hexCosts[angle][MT_LAND_EASY] = -1;
				tileInfo.hexCosts[angle][MT_LAND_REGULAR] = -1;
			}
			
		}
		
	}

}

/*
Sets base foundation year to current turn (if not set).
*/
void setBaseFoundationYear(int baseId)
{
	BASE *base = getBase(baseId);

	if (base->pad_1 == 0)
	{
		base->pad_1 = *current_turn;
	}

}
/*
Gets base age.
*/
int getBaseAge(int baseId)
{
	BASE *base = getBase(baseId);

	return (*current_turn) - base->pad_1 + 1;

}

/*
Estimates base population size growth rate.
*/
double getBaseSizeGrowth(int baseId)
{
	BASE *base = getBase(baseId);
	double age = std::max(10.0, std::max(conf.ai_base_size_b, (double)getBaseAge(baseId)));
	double sqrtValue = std::max(1.0, sqrt(age - conf.ai_base_size_b));
	double growthRatio =
		(conf.ai_base_size_a / 2.0 / sqrtValue)
		/
		std::max(1.0, (conf.ai_base_size_a * sqrtValue + conf.ai_base_size_c))
	;

	return growthRatio * base->pop_size;

}

/*
Estimates single action value diminishing with delay.
Example: colony is building a base.
*/
double getBonusDelayEffectCoefficient(double delay)
{
	return exp(- delay / aiData.developmentScale);
}

/*
Estimates multi action value diminishing with delay.
Example: base is building production.
*/
double getBuildTimeEffectCoefficient(double delay)
{
	return 1.0 / (exp(delay / aiData.developmentScale) - 1.0);
}

/*
Computes vehicle additional combat multiplier on top if unit properties.
= moraleMultiplier * relativePower
*/
double getVehicleStrenghtMultiplier(int vehicleId)
{
	return getVehicleMoraleMultiplier(vehicleId) * getVehicleRelativePower(vehicleId);
}

/*
Computes unit stack asault melee combat effect against enemy unit.
*/
double getUnitMeleeAttackEffect(int ownFactionId, int ownUnitId, int foeVehicleId, MAP *tile)
{
	UNIT *ownUnit = getUnit(ownUnitId);
	VEH *foeVehicle = getVehicle(foeVehicleId);
	int foeFactionId = foeVehicle->faction_id;
	int foeUnitId = foeVehicle->unit_id;
	int ownUnitChassisSpeed = ownUnit->speed();
	
	// own melee attack
	
	double ownMeleeAttackEffect = 0.0;
	
	if (isUnitCanMeleeAttackAtTile(ownUnitId, foeUnitId, tile))
	{
		ownMeleeAttackEffect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_MELEE)
			* getUnitMeleeAttackStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, true)
			/ getVehicleStrenghtMultiplier(foeVehicleId)
		;
	}
	
	double attackEffect = ownMeleeAttackEffect;
	
	// foe melee attack
	
	double foeMeleeAttackEffect = 0.0;
	
	if (isUnitCanMeleeAttackFromTile(foeUnitId, ownUnitId, tile))
	{
		foeMeleeAttackEffect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_MELEE)
			* getUnitMeleeAttackStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, false)
			* getVehicleStrenghtMultiplier(foeVehicleId)
		;
	}
	
	// foe artillery attack
	
	double foeArtilleryAttackEffect = 0.0;
	
	if (isUnitCanInitiateArtilleryDuel(foeUnitId, ownUnitId))
	{
		foeArtilleryAttackEffect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_ARTILLERY_DUEL)
			* getUnitMeleeAttackStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, false)
			* getVehicleStrenghtMultiplier(foeVehicleId)
		;
	}
	else if (isUnitCanInitiateBombardment(foeUnitId, ownUnitId))
	{
		foeArtilleryAttackEffect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_BOMBARDMENT)
			* getUnitMeleeAttackStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, false)
			* getVehicleMoraleMultiplier(foeVehicleId)
		;
		
		if (foeArtilleryAttackEffect > 0.0)
		{
			double bombardmentRoundCount = 0.0;
			
			switch (ownUnitChassisSpeed)
			{
			case 1:
				bombardmentRoundCount = 2.0;
				break;
			case 2:
				bombardmentRoundCount = 1.0;
				break;
			default:
				bombardmentRoundCount = 0.0;
			}
			
			foeArtilleryAttackEffect *= bombardmentRoundCount;
			
		}
		
	}
	
	// pick foe best effect
	
	double foeAttackEffect = std::max(foeMeleeAttackEffect, foeArtilleryAttackEffect);
	double defendEffect = foeAttackEffect > 0.0 ? (1.0 / foeAttackEffect) : 0.0;
	
	// effect
	
	double effect;
	
	if (attackEffect <= 0.0 && defendEffect <= 0.0)
	{
		// nobody can attack
		effect = 0.0;
	}
	else if (attackEffect > 0.0 && defendEffect <= 0.0)
	{
		// enemy cannot attack
		effect = attackEffect;
	}
	else if (attackEffect <= 0.0 && defendEffect > 0.0)
	{
		// we cannot attack
		// enemy chooses to attack only if effect is good for them
		effect = (defendEffect < 1.0 ? defendEffect : 0.0);
	}
	// defend is better than attack
	else if (defendEffect >= attackEffect)
	{
		// enemy chooses not to attack as it worse for them
		effect = attackEffect;
	}
	// attack is better than defend
	else
	{
		// slow unit - need to adjace the location before attacking
		// unit from location attacks first with small retaliation chance
		if (ownUnitChassisSpeed == 1)
		{
			effect = 0.25 * attackEffect + 0.75 * defendEffect;
		}
		// fast unit - about same chanse of attack and defend
		else
		{
			effect = 0.50 * attackEffect + 0.50 * defendEffect;
		}
	}
	
	return effect;
	
}

/*
Computes unit stack asault melee combat effect against enemy unit.
*/
double getUnitArtilleryDuelAttackEffect(int ownFactionId, int ownUnitId, int foeVehicleId, MAP *tile)
{
	VEH *foeVehicle = getVehicle(foeVehicleId);
	int foeFactionId = foeVehicle->faction_id;
	int foeUnitId = foeVehicle->unit_id;
	
	// artillery duel
	
	if (!isUnitCanInitiateArtilleryDuel(ownUnitId, foeUnitId))
		return 0.0;
	
	// own attack
	
	double ownArtilleryAttackEffect =
		aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_ARTILLERY_DUEL)
		* getUnitMeleeAttackStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, true)
		/ getVehicleStrenghtMultiplier(foeVehicleId)
	;
	
	double attackEffect = ownArtilleryAttackEffect;
	
	// foe attack
	
	double foeArtilleryAttackEffect =
		aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_MELEE)
		* getUnitMeleeAttackStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, false)
		* getVehicleStrenghtMultiplier(foeVehicleId)
	;
	
	double defendEffect = foeArtilleryAttackEffect > 0.0 ? 1.0 / foeArtilleryAttackEffect : 0.0;
	
	// effect
	
	double effect;
	
	if (attackEffect <= 0.0)
	{
		// we cannot attack
		effect = 0.0;
	}
	else if (attackEffect > 0.0 && defendEffect <= 0.0)
	{
		// enemy cannot attack
		effect = attackEffect;
	}
	// attack is worse
	else if (attackEffect <= defendEffect)
	{
		// enemy chooses not to attack as it worse for them
		effect = attackEffect;
	}
	// attack is better than defend
	else
	{
		// average both effects
		effect = 0.50 * attackEffect + 0.50 * defendEffect;
	}
	
	return effect;
	
}

/*
Computes unit stack asault melee combat effect against enemy unit.
*/
double getUnitBombardmentAttackEffect(int ownFactionId, int ownUnitId, int foeVehicleId, MAP *tile)
{
	VEH *foeVehicle = getVehicle(foeVehicleId);
	int foeFactionId = foeVehicle->faction_id;
	int foeUnitId = foeVehicle->unit_id;
	UNIT *foeUnit = getUnit(foeUnitId);
	int foeUnitChassisSpeed = foeUnit->speed();
	
	// bombardment
	
	if (!isUnitCanInitiateBombardment(ownUnitId, foeVehicleId))
		return 0.0;
	
	// own attack
	
	double ownBombardmentAttackEffect =
		aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_BOMBARDMENT)
		* getUnitArtilleryAttackStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, true)
		/ getVehicleMoraleMultiplier(foeVehicleId)
	;
	
	double attackEffect = ownBombardmentAttackEffect;
	
	// foe attack (melee if they can)
	
	double foeMeleeAttackEffect = 0.0;
	
	if (isUnitCanMeleeAttackFromTile(foeVehicleId, ownUnitId, tile))
	{
		foeMeleeAttackEffect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_MELEE)
			* getUnitMeleeAttackStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, false)
			* getVehicleStrenghtMultiplier(foeVehicleId)
		;
	}
	
	double defendEffect = foeMeleeAttackEffect > 0.0 ? (1.0 / foeMeleeAttackEffect) : 0.0;
	
	// effect
	
	double effect;
	
	if (attackEffect <= 0.0)
	{
		// we cannot attack
		effect = 0.0;
	}
	else if (attackEffect > 0.0 && defendEffect <= 0.0)
	{
		// enemy cannot attack
		effect = attackEffect;
	}
	// attack is worse
	else if (attackEffect <= defendEffect)
	{
		// enemy chooses not to attack as it worse for them
		effect = attackEffect;
	}
	// attack is better than defend
	else
	{
		// enemy chooses to kill our artillery with small chance of our artillery to shoot once
		
		if (foeUnitChassisSpeed == 1)
		{
			effect = 0.20 * attackEffect + 0.80 * defendEffect;
		}
		else if (foeUnitChassisSpeed == 2)
		{
			effect = 0.10 * attackEffect + 0.90 * defendEffect;
		}
		else
		{
			effect = 0.00 * attackEffect + 1.00 * defendEffect;
		}
		
	}
	
	return effect;
	
}

/*
Computes unit protect base effect against enemy vehicle.
*/
double getUnitBaseProtectionEffect(int ownFactionId, int ownUnitId, int foeFactionId, int foeUnitId, MAP *tile)
{
	UNIT *foeUnit = getUnit(foeUnitId);
	int foeUnitChassisSpeed = foeUnit->speed();
	
	// own melee attack
	
	double ownMeleeAttackEffect = 0.0;
	
	if (isMeleeUnit(ownUnitId) && !(isNeedlejetUnit(foeUnitId) && !isUnitHasAbility(ownUnitId, ABL_AIR_SUPERIORITY)))
	{
		double effect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_MELEE)
			* getUnitMeleeAttackStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, false)
		;
		
		if (effect > 0.0)
		{
			ownMeleeAttackEffect = effect;
		}
		
	}
	
	// own artillery attack
	
	double ownArtilleryAttackEffect = 0.0;
	
	if (isArtilleryUnit(ownUnitId) && isSurfaceUnit(foeUnitId))
	{
		if (isArtilleryUnit(foeUnitId))
		{
			double effect =
				aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_ARTILLERY_DUEL)
				* getUnitMeleeAttackStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, false)
			;
			
			if (effect > 0.0)
			{
				ownArtilleryAttackEffect = effect;
			}
			
		}
		else
		{
			double effect =
				aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_BOMBARDMENT)
				* getUnitMeleeAttackStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, false)
			;
			
			if (effect > 0.0)
			{
				double foeVehicleMaxRelativeBombardmentDamage = 1.0;
				double foeVehicleRemainingRelativeBombardmentDamage = 1.0;
				
				double bombardmentRoundCount = 0.0;
				
				switch (foeUnitChassisSpeed)
				{
				case 1:
					bombardmentRoundCount = 3.0;
					break;
				case 2:
					bombardmentRoundCount = 2.0;
					break;
				default:
					bombardmentRoundCount = 1.0;
				}
				
				double totalRelativeBombardmentDamage = std::min(foeVehicleRemainingRelativeBombardmentDamage, bombardmentRoundCount * effect);
				
				if (foeVehicleMaxRelativeBombardmentDamage == 1.0)
				{
					ownArtilleryAttackEffect = 1.0 + totalRelativeBombardmentDamage / foeVehicleRemainingRelativeBombardmentDamage;
				}
				else
				{
					ownArtilleryAttackEffect = 1.0 + totalRelativeBombardmentDamage;
				}
				
			}
			
		}
		
	}
	
	// pick own best effect
	
	double ownAttackEffect = std::max(ownMeleeAttackEffect, ownArtilleryAttackEffect);
	double attackEffect = ownAttackEffect;
	
	// foe melee attack
	
	double foeMeleeAttackEffect = 0.0;
	
	if (isMeleeUnit(foeUnitId))
	{
		double effect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_MELEE)
			* getUnitMeleeAttackStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, true)
		;
		
		if (effect > 0.0)
		{
			foeMeleeAttackEffect = effect;
		}
		
	}
	
	// foe artillery attack
	
	double foeArtilleryAttackEffect = 0.0;
	
	if (isArtilleryUnit(foeUnitId) && isSurfaceUnit(ownUnitId))
	{
		if (isArtilleryUnit(ownUnitId))
		{
			double effect =
				aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_ARTILLERY_DUEL)
				* getUnitMeleeAttackStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, true)
			;
			
			if (effect > 0.0)
			{
				foeArtilleryAttackEffect = effect;
			}
			
		}
		else
		{
			double effect =
				aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_BOMBARDMENT)
				* getUnitMeleeAttackStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, true)
			;
			
			if (effect > 0.0)
			{
				double bombardmentRoundCount = 4.0;
				
				double totalRelativeBombardmentDamage = std::min(1.0, bombardmentRoundCount * effect);
				
				foeArtilleryAttackEffect = 1.0 + totalRelativeBombardmentDamage;
				
			}
			
		}
		
	}
	
	// pick foe best effect
	
	double foeAttackEffect = std::max(foeMeleeAttackEffect, foeArtilleryAttackEffect);
	double defendEffect = foeAttackEffect > 0.0 ? 1.0 / foeAttackEffect : 0.0;
	
	// effect
	
	double effect;
	
	if (attackEffect <= 0.0 && defendEffect <= 0.0)
	{
		// nobody can attack
		effect = 0.0;
	}
	else if (attackEffect > 0.0 && defendEffect <= 0.0)
	{
		// enemy cannot attack
		effect = attackEffect;
	}
	else if (attackEffect <= 0.0 && defendEffect > 0.0)
	{
		// we cannot attack
		// enemy chooses to attack only if effect is good for them
		effect = (defendEffect >= 1.0 ? 0.0 : defendEffect);
	}
	// defend is better than attack
	else if (defendEffect >= attackEffect)
	{
		// we choose to defend as it is better for us
		effect = defendEffect;
	}
	// attack is better than defend
	else
	{
		// we have more chances to attack from base
		effect = 0.75 * attackEffect + 0.25 * defendEffect;
	}
	
	return effect;
	
}

/*
Computes unit protect location long range combat effect against enemy unit.
*/
double getUnitProtectLocationLongRangeCombatEffect(int ownFactionId, int ownUnitId, int foeFactionId, int foeUnitId, MAP *tile)
{
	// artillery unit
	
	if (!isArtilleryUnit(ownUnitId))
		return 0.0;
	
	// process long range combat
	
	if (isArtilleryUnit(foeUnitId))
	{
		// artillery duel
		
		double artilleryDuelAttackEffect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_ARTILLERY_DUEL)
			* getUnitArtilleryAttackStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, false)
		;
		
		double artilleryDuelDefendEffect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 1, CT_ARTILLERY_DUEL)
			* getUnitArtilleryAttackStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, true)
		;
		
		double artilleryDuelEffect;
		
		if (artilleryDuelAttackEffect <= 0.0 && artilleryDuelDefendEffect <= 0.0)
		{
			// nobody can attack
			artilleryDuelEffect = 0.0;
		}
		else if (artilleryDuelAttackEffect > 0.0 && artilleryDuelDefendEffect <= 0.0)
		{
			// enemy cannot attack
			artilleryDuelEffect = artilleryDuelAttackEffect;
		}
		else if (artilleryDuelAttackEffect <= 0.0 && artilleryDuelDefendEffect > 0.0)
		{
			// we cannot attack but enemy will most likely attack anyway
			artilleryDuelEffect = artilleryDuelDefendEffect;
		}
		else
		{
			// about half of the time each side initiates the duel
			artilleryDuelEffect = 0.5 * artilleryDuelAttackEffect + 0.5 * artilleryDuelDefendEffect;
		}
		
		return artilleryDuelEffect;
		
	}
	else
	{
		// bombardment
		
		double bombardmentAttackCombatEffect =
			aiData.combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, 0, CT_BOMBARDMENT)
			* getUnitArtilleryAttackStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, false)
		;
		
		return bombardmentAttackCombatEffect;
		
	}
	
}

double getTileDefenseMultiplier(MAP *tile, int factionId)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	
	double defenseMultiplier = getSensorDefenseMultiplier(factionId, x, y);
	
	if (map_has_item(tile, TERRA_BASE_IN_TILE) && isFriendlyTerritory(factionId, tile))
	{
		int baseId = base_at(x, y);
		
		double baseDefensiveMultiplier = 1.0;
		
		int firstLevelFacility = (!ocean ? FAC_PERIMETER_DEFENSE : FAC_NAVAL_YARD);
		if (!has_facility(baseId, firstLevelFacility) && !has_facility(baseId, FAC_TACHYON_FIELD))
		{
			baseDefensiveMultiplier = getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def);
		}
		else if (has_facility(baseId, firstLevelFacility) && !has_facility(baseId, FAC_TACHYON_FIELD))
		{
			baseDefensiveMultiplier *= (double)conf.perimeter_defense_multiplier / 2.0;
		}
		else if (!has_facility(baseId, firstLevelFacility) && has_facility(baseId, FAC_TACHYON_FIELD))
		{
			baseDefensiveMultiplier *= (double)(2 + conf.tachyon_field_bonus) / 2.0;
		}
		else if (has_facility(baseId, firstLevelFacility) && has_facility(baseId, FAC_TACHYON_FIELD))
		{
			baseDefensiveMultiplier *= (double)(conf.perimeter_defense_multiplier + conf.tachyon_field_bonus) / 2.0;
		}
		
		// average base defensive multiplier between psi and conventional combat
		
		baseDefensiveMultiplier = (getPercentageBonusMultiplier(Rules->combat_bonus_intrinsic_base_def) + baseDefensiveMultiplier) / 2.0;
		
		// set defensive bonus
		
		defenseMultiplier *= baseDefensiveMultiplier;
		
	}
	else
	{
		if (isRoughTerrain(tile))
		{
			defenseMultiplier *= getPercentageBonusMultiplier(50);
		}
		
		if (map_has_item(tile, TERRA_BUNKER))
		{
			defenseMultiplier *= getPercentageBonusMultiplier(50);
		}
			
	}
	
	return defenseMultiplier;
	
}

double getHarmonicMean(std::vector<std::vector<double>> parameters)
{
	int count = 0;
	double reciprocalSum = 0.0;
	
	for (std::vector<double> &fraction : parameters)
	{
		if (fraction.size() != 2)
			continue;
		
		double numerator = fraction.at(0);
		double denominator = fraction.at(1);
		
		if (numerator <= 0.0 || denominator <= 0.0)
			continue;
		
		double value = numerator / denominator;
		double reciprocal = 1.0 / value;
		
		count++;
		reciprocalSum += reciprocal;
		
	}
	
	if (count == 0)
		return 0.0;
	
	return 1.0 / (reciprocalSum / (double)count);
	
}

/*
Returns locations vehicle can reach in one turn and corresponding movement allowance left when arriving there.
AI vehicles subtract current moves spent as they are currently moving.
*/
bool compareMovementActions(MovementAction &o1, MovementAction &o2)
{
    return (o1.movementAllowance > o2.movementAllowance);
}
std::vector<MovementAction> getVehicleReachableLocations(int vehicleId)
{
	executionProfiles["| getVehicleReachableLocations"].start();
	
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	int initialMovementAllowance = getVehicleSpeed(vehicleId) - (vehicle->faction_id == aiFactionId ? vehicle->road_moves_spent : 0);
	UNIT *unit = getUnit(vehicle->unit_id);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MOVEMENT_TYPE movementType = getUnitMovementType(factionId, unitId);
	
	std::vector<MovementAction> movementActions;
	
	// land vehicle on transport cannot move
	
	if (isLandVehicleOnTransport(vehicleId))
	{
		executionProfiles["| getVehicleReachableLocations"].stop();
		return movementActions;
	}
	
	// explore paths
	
	std::map<MAP *, int> movementCosts;
	movementCosts.emplace(vehicleTile, 0);
	std::set<MAP *> processingTiles;
	processingTiles.emplace(vehicleTile);
	
	while (processingTiles.size() > 0)
	{
		std::set<MAP *> newProcessingTiles;
		
		for (MAP *tile : processingTiles)
		{
			TileInfo &processingTileInfo = aiData.getTileInfo(tile);
			int procesingTileMovementCost = movementCosts.at(tile);
			int movementAllowance = initialMovementAllowance - procesingTileMovementCost;
			
			// stop exploring condition
			// no more moves to go to adjacent tile
			
			if (movementAllowance <= 0)
				continue;
			
			// iterate adjacent tiles
			
			for (unsigned int angle = 0; angle < 8; angle++)
			{
				MAP *adjacentTile = getTileByAngle(tile, angle);
				
				if (adjacentTile == nullptr)
					continue;
				
				// compute hex cost
				
				int hexCost = processingTileInfo.hexCosts[angle][movementType];
				
				// allowed step
				
				if (hexCost == -1)
					continue;
				
				// parameters
				
				TileInfo adjacentTileInfo = aiData.getTileInfo(adjacentTile);
				TileMovementInfo adjacentTileMovementInfo = adjacentTileInfo.movementInfos[vehicle->faction_id];
				
				// allowed move
				
				if (!isVehicleAllowedMove(vehicleId, tile, adjacentTile, false))
					continue;
				
				// hovertank ignores rough terrain
				
				if (unit->chassis_type == CHS_HOVERTANK)
				{
					hexCost = std::min(Rules->mov_rate_along_roads, hexCost);
				}
				
				// movement cost
				
				int adjacentTileMovementCost = procesingTileMovementCost + hexCost;
				
				// wounded vehicle stepping on monolith loses all its movement points
				
				if (map_has_item(adjacentTile, TERRA_MONOLITH) && vehicle->damage_taken > 0)
				{
					adjacentTileMovementCost = initialMovementAllowance;
				}
				
				// update best value
				
				if (movementCosts.count(adjacentTile) == 0)
				{
					movementCosts.emplace(adjacentTile, adjacentTileMovementCost);
					newProcessingTiles.emplace(adjacentTile);
				}
				else if (adjacentTileMovementCost < movementCosts.at(adjacentTile))
				{
					movementCosts.at(adjacentTile) = adjacentTileMovementCost;
					newProcessingTiles.emplace(adjacentTile);
				}
				
			}
			
		}
		
		processingTiles.clear();
		processingTiles.swap(newProcessingTiles);
		
	}
	
	for (std::pair<MAP * const, int> movementCostEntry : movementCosts)
	{
		MAP *tile = movementCostEntry.first;
		int movementCost = movementCostEntry.second;
		int movementAllowance = initialMovementAllowance - movementCost;
		
		movementActions.push_back({tile, movementAllowance});
		
	}
	
	std::sort(movementActions.begin(), movementActions.end(), compareMovementActions);
	
	executionProfiles["| getVehicleReachableLocations"].stop();
	return movementActions;
	
}

/*
Select locations vehicle can melee attack with corresponding movement allocation left.
*/
std::vector<MovementAction> getVehicleMeleeAttackLocations(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	int triad = vehicle->triad();
	
	std::vector<MovementAction> attackLocations;
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return attackLocations;
	
	// explore reachable locations
	
	std::map<MAP *, int> attackLocationMovementAllowances;
	
	for (MovementAction &movementAction : getVehicleReachableLocations(vehicleId))
	{
		MAP *tile = movementAction.destination;
		int movementAllowance = movementAction.movementAllowance;
		
		// cannot attack without movementAllowance
		
		if (movementAllowance <= 0)
			continue;
		
		// explore adjacent tiles
		
		for (MAP *adjacentTile : getAdjacentTiles(tile))
		{
			int adjacentTileOcean = is_ocean(adjacentTile);
			
			// same realm
			
			if (triad == TRIAD_LAND && adjacentTileOcean || triad == TRIAD_SEA && !adjacentTileOcean)
				continue;
			
			// update best movementAllowance
			
			attackLocationMovementAllowances[adjacentTile] = std::max(attackLocationMovementAllowances[adjacentTile], movementAllowance);
			
		}
		
	}
	
	// build return value
	
	for (std::pair<MAP * const, int> attackLocationMovementAllowanceEntry : attackLocationMovementAllowances)
	{
		MAP *tile = attackLocationMovementAllowanceEntry.first;
		int movementAllowance = attackLocationMovementAllowanceEntry.second;
		
		attackLocations.push_back({tile, movementAllowance});
		
	}
	
	std::sort(attackLocations.begin(), attackLocations.end(), compareMovementActions);
	
	return attackLocations;
	
}

/*
Select locations vehicle can artillery attack with corresponding movement allocation left.
*/
std::vector<MovementAction> getVehicleArtilleryAttackPositions(int vehicleId)
{
	std::vector<MovementAction> attackLocations;
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return attackLocations;
	
	// explore reachable locations
	
	std::map<MAP *, int> attackLocationMovementAllowances;
	
	for (MovementAction &movementAction : getVehicleReachableLocations(vehicleId))
	{
		MAP *tile = movementAction.destination;
		int movementAllowance = movementAction.movementAllowance;
		
		// cannot attack without movementAllowance
		
		if (movementAllowance <= 0)
			continue;
		
		// explore artillery tiles
		
		for (MAP *targetTile : getArtilleryAttackPositions(tile))
		{
			// update best movementAllowance
			
			attackLocationMovementAllowances[targetTile] = std::max(attackLocationMovementAllowances[targetTile], movementAllowance);
			
		}
		
	}
	
	// build return value
	
	for (std::pair<MAP * const, int> attackLocationMovementAllowanceEntry : attackLocationMovementAllowances)
	{
		MAP *tile = attackLocationMovementAllowanceEntry.first;
		int movementAllowance = attackLocationMovementAllowanceEntry.second;
		
		attackLocations.push_back({tile, movementAllowance});
		
	}
	
	std::sort(attackLocations.begin(), attackLocations.end(), compareMovementActions);
	
	return attackLocations;
	
}

/*
Evaluates chance of this vehicle destruction at this tile
accounting for existing vehicles landed on this tile.
*/
double getVehicleTileDanger(int vehicleId, MAP *tile)
{
//	debug("getVehicleTileDanger\n");
//	
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	// get vehicle at tile
	// add this one as needed
	
	std::vector<int> tileVehicleIds = getTileVehicleIds(tile);
	
	if (vehicleTile != tile)
	{
		tileVehicleIds.push_back(vehicleId);
	}
	
	// collect vehicles landed on this tile
	
	std::vector<int> ourVehicleIds;
	std::vector<int> ourArtilleryVehicleIds;
	std::vector<int> ourNonArtilleryVehicleIds;
	
	for (int tileVehicleId : tileVehicleIds)
	{
		// exclude not holding vehicles with movement allowance
		
		if (tileVehicleId != vehicleId && !isVehicleOnHold(tileVehicleId) && getVehicleRemainingMovement(tileVehicleId) > 0)
			continue;
		
		// add vehicle
		
		ourVehicleIds.push_back(tileVehicleId);
		
		if (isArtilleryVehicle(tileVehicleId))
		{
			ourArtilleryVehicleIds.push_back(tileVehicleId);
		}
		else
		{
			ourNonArtilleryVehicleIds.push_back(tileVehicleId);
		}
		
	}
	
	double artilleryPart = (double)ourArtilleryVehicleIds.size() / (double)ourVehicleIds.size();
	double nonArtilleryPart = (double)ourNonArtilleryVehicleIds.size() / (double)ourVehicleIds.size();
	
	// process enemy force
	
	double totalOurMeleeLoss = 0.0;
	double totalOurArtilleryLoss = 0.0;
	double totalOurBombardmentLoss = 0.0;
	
	for (Force &force : tileInfo.enemyOffensiveForces)
	{
		int enemyVehicleId = force.getVehicleId();
		
		if (enemyVehicleId == -1)
			continue;
		
		switch (force.attackType)
		{
		case AT_MELEE:
			{
				double totalEnemyVehicleLoss = 0.0;
				
				for (int ourVehicleId : ourVehicleIds)
				{
					double battleOdds = getBattleOddsAt(enemyVehicleId, ourVehicleId, false, tile);
					
					if (battleOdds <= 0.0)
						continue;
					
					totalEnemyVehicleLoss += 1.0 / battleOdds;
					
				}
				
				if (totalEnemyVehicleLoss <= 0.0)
					continue;
				
				totalOurMeleeLoss += 1.0 / totalEnemyVehicleLoss;
				
			}
			break;
			
		case AT_ARTILLERY:
			{
				double totalEnemyVehicleLoss = 0.0;
				
				for (int ourVehicleId : ourArtilleryVehicleIds)
				{
					double battleOdds = getBattleOddsAt(enemyVehicleId, ourVehicleId, true, tile);
					
					if (battleOdds <= 0.0)
						continue;
					
					totalEnemyVehicleLoss += 1.0 / battleOdds;
					
				}
				
				if (totalEnemyVehicleLoss <= 0.0)
					continue;
				
				totalOurArtilleryLoss += 1.0 / totalEnemyVehicleLoss;
				
				for (int ourVehicleId : ourNonArtilleryVehicleIds)
				{
					totalOurBombardmentLoss += getVehicleBombardmentDamage(enemyVehicleId, ourVehicleId);
				}
				
			}
			break;
			
		}
		
	}
	
	double totalOurStackLoss = 0.0;
	
	if (totalOurArtilleryLoss <= 1.0)
	{
		totalOurStackLoss =
			+ totalOurArtilleryLoss * artilleryPart
			+ totalOurBombardmentLoss * nonArtilleryPart
			+ totalOurMeleeLoss
		;
		
	}
	else
	{
		totalOurStackLoss =
			+ totalOurArtilleryLoss * artilleryPart
			+ totalOurMeleeLoss
		;
		
	}
	
	// return danger
	
	return totalOurStackLoss;
	
}

bool isVehicleAllowedMove(int vehicleId, MAP *from, MAP *to, bool ignoreEnemy)
{
	VEH *vehicle = getVehicle(vehicleId);
	TileInfo &fromTileInfo = aiData.getTileInfo(from);
	TileInfo &toTileInfo = aiData.getTileInfo(to);
	TileMovementInfo &fromTileMovementInfo = fromTileInfo.movementInfos[vehicle->faction_id];
	TileMovementInfo &toTileMovementInfo = toTileInfo.movementInfos[vehicle->faction_id];
	bool toTileBlocked = (ignoreEnemy ? toTileMovementInfo.blockedNeutral : toTileMovementInfo.blocked);
	bool fromTileZoc = (ignoreEnemy ? fromTileMovementInfo.zocNeutral : fromTileMovementInfo.zoc);
	bool toTileZoc = (ignoreEnemy ? toTileMovementInfo.zocNeutral : toTileMovementInfo.zoc);
	
	if (toTileBlocked)
		return false;
	
	if (vehicle->triad() == TRIAD_LAND && !is_ocean(from) && !is_ocean(to) && fromTileZoc && toTileZoc)
		return false;
	
	return true;
	
}

/*
Lists all possible tile vehicle can attack.
*/
std::vector<MAP *> getVehicleThreatenedTiles(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MOVEMENT_TYPE movementType = getUnitMovementType(factionId, unitId);
	
	// set initial move allowance
	
	int initialMovementAllowance = getVehicleSpeed(vehicleId);
	
	// explore paths
	
	std::map<MAP *, int> movementAllowances;
	std::set<MAP *> processingTiles;
	
	movementAllowances.emplace(vehicleTile, initialMovementAllowance);
	processingTiles.emplace(vehicleTile);
	
	while (processingTiles.size() > 0)
	{
		std::set<MAP *> newProcessingTiles;
		
		for (MAP *processingTile : processingTiles)
		{
			TileInfo &processingTileInfo = aiData.getTileInfo(processingTile);
			int movementAllowance = movementAllowances.at(processingTile);
			
			// stop exploring condition
			// no more moves to go to adjacent tile
			
			if (movementAllowance <= 0)
				continue;
			
			// iterate adjacent tiles for moving
			
			for (unsigned int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				MAP *adjacentTile = getTileByAngle(processingTile, angle);
				
				if (adjacentTile == nullptr)
					continue;
				
				// retrieve hex cost
				
				int hexCost = processingTileInfo.hexCosts[angle][movementType];
				
				// allowed move (geography)
				
				if (hexCost == -1)
					continue;
				
				// adjacent tile movement allowance
				
				int adjacentTileMovementAllowance = movementAllowance - hexCost;
				
				// update best value
				
				if (adjacentTileMovementAllowance > movementAllowances[adjacentTile])
				{
					movementAllowances[adjacentTile] = adjacentTileMovementAllowance;
					newProcessingTiles.insert(adjacentTile);
				}
				
			}
			
		}
		
		processingTiles.clear();
		processingTiles.swap(newProcessingTiles);
		
	}
	
	std::vector<MAP *> threatenedTiles;
	
	for (std::pair<MAP * const, int> movementAllowanceEntry : movementAllowances)
	{
		MAP *tile = movementAllowanceEntry.first;
		
		threatenedTiles.push_back(tile);
		
	}
	
	return threatenedTiles;
	
}

std::vector<int> &getAssociationSeaTransports(int association, int factionId)
{
	return aiData.geography.factions[factionId].oceanAssociationSeaTransports[association];
}

std::vector<int> &getAssociationShipyards(int association, int factionId)
{
	return aiData.geography.factions[factionId].oceanAssociationShipyards[association];
}

/*
Vanilla sometimes not immediatelly board vehicles to transport.
This method corrects this situation.
*/
void assignVehiclesToTransports()
{
	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		bool vehicleTileOcean = is_ocean(vehicleTile);
		
		// ours
		
		if (vehicle->faction_id != aiFactionId)
			continue;
		
		// land
		
		if (triad != TRIAD_LAND)
			continue;
		
		// in ocean
		
		if (!vehicleTileOcean)
			continue;
		
		// board
		
		board(vehicleId);
		
	}
	
}

/*
Brings all association mappings to smallest connected association.
*/
void joinAssociations(std::map<int, int> &associations, std::map<int, std::set<int>> &joins)
{
//	debug("joinAssociations\n");
//	fflush(debug_log);
//	
	while (joins.size() > 0)
	{
		// get first connection
		
		std::map<int, std::set<int>>::iterator firstJoinIterator = joins.begin();
		int association1 = firstJoinIterator->first;
		int association2 = *(firstJoinIterator->second.begin());
		
		if (association1 == association2)
		{
			debug("connected associations contain same association: %2d %2d\n", association1, association1);
			fflush(debug_log);
			exit(-1);
		}
		
//		debug("\tassociation1=%2d association2=%2d\n", association1, association2);
//		fflush(debug_log);
//		
		// update associations
		
		for (std::pair<int const, int> &associationEntry : associations)
		{
			if (associationEntry.second == association2)
			{
				associationEntry.second = association1;
//				debug("\t\textendedRegion=%2d was: %2d now: %2d\n", associationEntry.first, association2, association1);
//				fflush(debug_log);
			}
			
		}
		
		// update joins
		
		std::set<int> &association1Joins = joins.at(association1);
		std::set<int> &association2Joins = joins.at(association2);
		
//		if (DEBUG)
//		{
//			debug("\t\tjoins before\n");
//			for (std::pair<int const, std::set<int>> &joinEntry : joins)
//			{
//				debug("\t\t\t%2d:", joinEntry.first);
//				for (int joinedAssociation : joinEntry.second)
//				{
//					debug(" %2d", joinedAssociation);
//				}
//				debug("\n");
//			}
//			fflush(debug_log);
//		}
//		
		association1Joins.insert(association2Joins.begin(), association2Joins.end());
		association1Joins.erase(association1);
		association1Joins.erase(association2);
		if (association1Joins.size() == 0)
		{
			joins.erase(association1);
		}
		joins.erase(association2);
		
		for (std::pair<int const, std::set<int>> &joinEntry : joins)
		{
			int association = joinEntry.first;
			std::set<int> &associationJoins = joinEntry.second;
			
			// except association1 - it was already handled
			
			if (association == association1)
				continue;
			
			// replace association2 to association1
			
			if (associationJoins.count(association2) != 0)
			{
				associationJoins.erase(association2);
				associationJoins.insert(association1);
			}
			
		}
		
//		if (DEBUG)
//		{
//			debug("\t\tjoins after\n");
//			for (std::pair<int const, std::set<int>> &joinEntry : joins)
//			{
//				debug("\t\t\t%2d:", joinEntry.first);
//				for (int joinedAssociation : joinEntry.second)
//				{
//					debug(" %2d", joinedAssociation);
//				}
//				debug("\n");
//			}
//			fflush(debug_log);
//		}
//		
	}
	
//	debug("\tcompleted\n");
//	fflush(debug_log);
//	
}

int getBestSeaCombatSpeed(int factionId)
{
	int bestSeaCombatSpeed = 0;
	
	for (int unitId : getFactionUnitIds(factionId, false, false))
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		
		// sea combat
		
		if (triad != TRIAD_SEA)
			continue;
		
		if (!isCombatUnit(unitId))
			continue;
		
		// speed
		
		int speed = getUnitSpeed(unitId);
		
		// update best
		
		if (speed > bestSeaCombatSpeed)
		{
			bestSeaCombatSpeed = speed;
		}
		
	}
	
	return bestSeaCombatSpeed;
	
}

int getSeaTransportUnitId(int oceanAssociation, int factionId, bool existing)
{
	// check existing transports first - faction may have transport even before discovering technology
	
	int bestUnitId = -1;
	int bestUnitChassisSpeed = 0;
	
	for (int vehicleId : getAssociationSeaTransports(oceanAssociation, factionId))
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		if (vehicle == nullptr)
			continue;
		
		int chassisSpeed = getVehicleSpeedWithoutRoads(vehicleId);
		
		if (chassisSpeed > bestUnitChassisSpeed)
		{
			bestUnitId = vehicle->unit_id;
			bestUnitChassisSpeed = chassisSpeed;
		}
		
	}
	
	if (bestUnitId != -1)
		return bestUnitId;
	
	// count existing transports only
	
	if (existing)
		return -1;
	
	// check shipyards
	
	if (getAssociationShipyards(oceanAssociation, factionId).size() == 0)
		return -1;
	
	// check units
	
	for (int unitId : getFactionUnitIds(factionId, false, false))
	{
		UNIT *unit = getUnit(unitId);
		
		int chassisSpeed = unit->speed();
		
		if (chassisSpeed > bestUnitChassisSpeed)
		{
			bestUnitId = unitId;
			bestUnitChassisSpeed = chassisSpeed;
		}
		
	}
	
	// return value
	
	return bestUnitId;
	
}

int getSeaTransportChassisSpeed(int oceanAssociation, int factionId, bool existing)
{
	int seaTransportUnitId = getSeaTransportUnitId(oceanAssociation, factionId, existing);
	
	if (seaTransportUnitId == -1)
		return -1;
	
	return getUnit(seaTransportUnitId)->speed();
	
}

int getBestSeaTransportSpeed(int factionId)
{
	int bestSeaTransportSpeed = 0;
	
	for (int unitId : getFactionUnitIds(factionId, false, false))
	{
		// sea transport
		
		if (!isSeaTransportUnit(unitId))
			continue;
		
		// speed
		
		int speed = getUnitSpeed(unitId);
		
		// update best
		
		if (speed > bestSeaTransportSpeed)
		{
			bestSeaTransportSpeed = speed;
		}
		
	}
	
	return bestSeaTransportSpeed;
	
}

/*
Searches for location closest to destination within given range of target.
*/
MAP *getClosestTargetLocation(int factionId, MAP *origin, MAP *target, int proximity, int triad, bool ignoreHostile)
{
	MAP *closestLocation = nullptr;
	int closestLocationRange = INT_MAX;
	for (MAP *tile : getRangeTiles(target, proximity, true))
	{
		bool ocean = is_ocean(tile);
		TileMovementInfo &tileMovementInfo = aiData.getTileInfo(tile).movementInfos[factionId];
		bool blocked = (ignoreHostile ? tileMovementInfo.blockedNeutral : tileMovementInfo.blocked);
			
		// not blocked except target
		
		if (tile != target && blocked)
			continue;
		
		// matching surface/oceanAssociation
		
		switch (triad)
		{
		case TRIAD_AIR:
			
			// no restrictions
			
			break;
			
		case TRIAD_SEA:
			
			// same ocean association
			
			if (!isSameOceanAssociation(origin, tile, factionId))
				continue;
			
			break;
			
		case TRIAD_LAND:
			
			// land
			
			if (ocean)
				continue;
				
			break;
			
		}
		
		// range
		
		int range = getRange(origin, tile);
		
		// update best
		
		if (range < closestLocationRange)
		{
			closestLocation = tile;
			closestLocationRange = range;
		}
		
	}
	
	return closestLocation;
	
}

/*
Delete unit to reduce support burden.
*/
void disbandOrversupportedVehicles(int factionId)
{
	debug("disbandOrversupportedVehicles - %s\n", MFactions[aiFactionId].noun_faction);
	
	// oversupported bases delete outside units then inside
	
	int sePolice = getFaction(aiFactionId)->SE_police_pending;
	int warWearinessLimit = (sePolice <= -4 ? 0 : (sePolice == -3 ? 1 : -1));
	
	std::vector<int> killVehicleIds;
	
	if (warWearinessLimit != -1)
	{
		debug("\tremove war weariness units outside\n");
		
		std::map<int, std::vector<int>> outsideVehicles;
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			int triad = vehicle->triad();
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			if (vehicle->faction_id != factionId)
				continue;
			
			if (!isCombatVehicle(vehicleId))
				continue;
			
			if (vehicle->home_base_id == -1)
				continue;
			
			BASE *base = getBase(vehicle->home_base_id);
			
			if ((triad == TRIAD_AIR || vehicleTile->owner != aiFactionId) && base->pop_size < 5)
			{
				outsideVehicles[vehicle->home_base_id].push_back(vehicleId);
				debug("\t\toutsideVehicles: (%3d,%3d)\n", vehicle->x, vehicle->y);
			}
			
		}
		
		for (std::pair<int const, std::vector<int>> &outsideVehicleEntry : outsideVehicles)
		{
			std::vector<int> &baseOutsideVehicles = outsideVehicleEntry.second;
			
			for (unsigned int i = 0; i < baseOutsideVehicles.size() - warWearinessLimit; i++)
			{
				int vehicleId = baseOutsideVehicles.at(i);
				
				killVehicleIds.push_back(vehicleId);
				debug
				(
					"\t\t[%4d] (%3d,%3d) : %-25s\n"
					, vehicleId, getVehicle(vehicleId)->x, getVehicle(vehicleId)->y, getBase(getVehicle(vehicleId)->home_base_id)->name
				);
				
			}
			
		}
		
	}
	
	std::sort(killVehicleIds.begin(), killVehicleIds.end(), std::greater<int>());
	
	for (int vehicleId : killVehicleIds)
	{
		veh_kill(vehicleId);
	}
	
}

void storeBaseSetProductionItems()
{
	aiData.baseBuildingItems.clear();
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		aiData.baseBuildingItems[baseId] = getBase(baseId)->queue_items[0];
	}
	
}

MAP *getVehicleMeleeAttackPosition(int vehicleId, MAP *target, bool ignoreHostile)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int triad = vehicle->triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	bool nativeVehicle = isNativeVehicle(vehicleId) || has_project(factionId, FAC_XENOEMPATHY_DOME);
	
	// can attack target
	
	if (!isVehicleCanMeleeAttackTile(vehicleId, target))
		return nullptr;
	
	// find closest attack position
	
	MAP *closestAttackPosition = nullptr;
	int closestAttackPositionRange = INT_MAX;
	int closestAttackPositionPreference = INT_MAX;
	
	for (MAP *tile : getMeleeAttackPositions(target))
	{
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[factionId];
		bool blocked = (ignoreHostile ? tileMovementInfo.blockedNeutral : tileMovementInfo.blocked);
		
		// not blocked
		
		if (blocked)
			continue;
		
		// destination is reachable
		
		if (!isVehicleDestinationReachable(vehicleId, tile, true))
			continue;
		
		// can attack from this tile
		
		if (!isVehicleCanMeleeAttackFromTile(vehicleId, tile))
			continue;
		
		// range
		
		int range = getRange(vehicleTile, tile);
		
		// hexCost
		
		int hexCost = Rules->mov_rate_along_roads;
		
		switch (triad)
		{
		case TRIAD_AIR:
			{
				hexCost = Rules->mov_rate_along_roads;
			}
			break;
			
		case TRIAD_SEA:
			{
				if (nativeVehicle)
				{
					hexCost = Rules->mov_rate_along_roads;
				}
				else
				{
					hexCost = Rules->mov_rate_along_roads * (map_has_item(tile, TERRA_FUNGUS) ? 3 : 1);
				}
			}
			break;
			
		case TRIAD_LAND:
			{
				if (map_has_item(tile, TERRA_MAGTUBE))
				{
					hexCost = getMovementRateAlongTube();
				}
				else if (map_has_item(tile, TERRA_ROAD) || nativeVehicle && map_has_item(tile, TERRA_FUNGUS))
				{
					hexCost = getMovementRateAlongRoad();
				}
				else if (map_has_item(tile, TERRA_RIVER))
				{
					hexCost = getMovementRateAlongRoad() + 1;
				}
				else
				{
					hexCost = Rules->mov_rate_along_roads * (map_has_item(tile, TERRA_FUNGUS) ? 3 : 1);
				}
			}
			break;
			
		}
		
		// preference
		
		int preference = hexCost + (isRoughTerrain(tile) ? -1 : 0);
		
		// update
		
		if (range < closestAttackPositionRange || range == closestAttackPositionRange && preference < closestAttackPositionPreference)
		{
			closestAttackPosition = tile;
			closestAttackPositionRange = range;
			closestAttackPositionPreference = preference;
		}
		
	}
	
	return closestAttackPosition;
	
}

MAP *getVehicleArtilleryAttackPosition(int vehicleId, MAP *target, bool ignoreHostile)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int triad = vehicle->triad();
	bool native = isNativeVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	MAP *closestAttackPosition = nullptr;
	int closestAttackPositionRange = INT_MAX;
	int closestAttackPositionMovementCost = 0;
	
	for (MAP *tile : getArtilleryAttackPositions(target))
	{
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[factionId];
		bool blocked = (ignoreHostile ? tileMovementInfo.blockedNeutral : tileMovementInfo.blocked);
		
		// destination is reachable
		
		if (!isVehicleDestinationReachable(vehicleId, tile, true))
			continue;
		
		// can attack from this tile
		
		if (!isVehicleCanArtilleryAttackFromTile(vehicleId, tile))
			continue;
		
		// not blocked
		
		if (blocked)
			continue;
		
		// range
		
		int range = getRange(vehicleTile, tile);
		
		// movement cost
		
		int movementCost = Rules->mov_rate_along_roads;
		
		switch (triad)
		{
		case TRIAD_AIR:
			movementCost = Rules->mov_rate_along_roads;
			break;
			
		case TRIAD_SEA:
			if
			(
				isBaseAt(tile)
				||
				!map_has_item(tile, TERRA_FUNGUS)
				||
				(native || has_project(factionId, FAC_XENOEMPATHY_DOME))
			)
			{
				movementCost = Rules->mov_rate_along_roads;
			}
			else
			{
				movementCost = Rules->mov_rate_along_roads * 3;
			}
			break;
			
		case TRIAD_LAND:
			if
			(
				isBaseAt(tile)
				||
				map_has_item(tile, TERRA_MAGTUBE)
			)
			{
				movementCost = getMovementRateAlongTube();
			}
			else if
			(
				map_has_item(tile, TERRA_ROAD)
				||
				(native || has_project(factionId, FAC_XENOEMPATHY_DOME)) && map_has_item(tile, TERRA_FUNGUS)
			)
			{
				movementCost = getMovementRateAlongRoad();
			}
			else if
			(
				!map_has_item(tile, TERRA_FUNGUS)
			)
			{
				movementCost = Rules->mov_rate_along_roads;
			}
			else
			{
				movementCost = Rules->mov_rate_along_roads * 3;
			}
			break;
			
		}
		
		if
		(
			range < closestAttackPositionRange
			||
			range == closestAttackPositionRange && movementCost < closestAttackPositionMovementCost
		)
		{
			closestAttackPosition = tile;
			closestAttackPositionRange = range;
			closestAttackPositionMovementCost = movementCost;
		}
		
	}
	
	return closestAttackPosition;
	
}

/*
Check if unit can capture given base.
*/
bool isUnitCanCaptureBase(int factionId, int unitId, MAP *origin, MAP *baseTile)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	bool baseTileOcean = is_ocean(baseTile);
	
	// vehicle should generally be able to capture base
	
	switch (unit->chassis_type)
	{
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		return false;
		break;
		
	}
	
	// check if vehicle can capture this specific base
	
	switch (triad)
	{
	case TRIAD_SEA:
		
		// sea unit can not capture land base
		
		if (!baseTileOcean)
			return false;
		
		// sea unit can move within same ocean association
		
		if (!isSameOceanAssociation(origin, baseTile, factionId))
			return false;
		
		break;
		
	case TRIAD_LAND:
		
		// non-amphibious land unit can not capture sea base
		
		if (baseTileOcean && !isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
			return false;
		
		break;
		
	}
	
	// all checks passed
	
	return true;
	
}

/*
Check if vehicle can capture given base.
*/
bool isVehicleCanCaptureBase(int vehicleId, MAP *baseTile)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	return isUnitCanCaptureBase(vehicle->faction_id, vehicle->unit_id, vehicleTile, baseTile);
	
}

/*
Checks if a unit can melee attack another vehicle at location.
*/
bool isUnitCanMeleeAttackStack(int factionId, int unitId, MAP *origin, MAP *enemyStackTile, EnemyStackInfo &enemyStackInfo)
{
	// unit should be generally capable attacking target tile
	
	if (!isMeleeUnit(unitId))
		return false;
	
	// unit without air superiority cannot attack needlejet in flight
	
	if (enemyStackInfo.needlejetInFlight && !isUnitHasAbility(unitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// reachable
	
	if (!isUnitDestinationReachable(factionId, unitId, origin, enemyStackTile, false))
		return false;
	
	// all checks passed
	
	return true;
	
}

/*
Checks if a unit can artillery attack stack at location.
*/
bool isUnitCanArtilleryAttackStack(int factionId, int unitId, MAP *origin, MAP *enemyStackTile, EnemyStackInfo &enemyStackInfo)
{
	// unit should be generally capable attacking units in target tile
	
	if (!(isArtilleryUnit(unitId) && enemyStackInfo.targetable))
		return false;
	
	// reachable
	
	if (!isUnitDestinationArtilleryReachable(factionId, unitId, origin, enemyStackTile, false))
		return false;
	
	// all checks passed
	
	return true;
	
}

/*
Additional benefit of destroying a vehicle.
*/
double getVehicleDestructionPriority(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	
	double destructionPriority;
	
	if (isArtifactVehicle(vehicleId))
	{
		destructionPriority = 4.0;
	}
	else if (isProbeVehicle(vehicleId))
	{
		destructionPriority = 3.0;
	}
	else if (isColonyVehicle(vehicleId))
	{
		destructionPriority = 2.0;
	}
	else if (isTransportVehicle(vehicleId))
	{
		destructionPriority = 2.0;
	}
	else if (factionId == 0 && unitId == BSC_SPORE_LAUNCHER)
	{
		destructionPriority = conf.ai_combat_attack_priority_alien_spore_launcher;
	}
	else if (factionId == 0 && unitId == BSC_FUNGAL_TOWER)
	{
		destructionPriority = conf.ai_combat_attack_priority_alien_fungal_tower;
	}
	else if (factionId == 0 && unitId == BSC_MIND_WORMS)
	{
		destructionPriority = conf.ai_combat_attack_priority_alien_mind_worms;
	}
	else
	{
		destructionPriority = 1.0;
	}
	
	return destructionPriority;
	
}

bool isBaseSetProductionProject(int baseId)
{
	if (aiData.baseBuildingItems.count(baseId) == 0)
		return false;
	int item = aiData.baseBuildingItems.at(baseId);
	return (item >= -PROJECT_ID_LAST && item <= -PROJECT_ID_FIRST);
}

/*
Returns unit cost accounting for support.
Clean unit does not require support.
Non combat unit lives for 80 turns.
Combat unit lives for 40 turns.
Police unit replaces another police unit and, therefore, is counted as half support.
*/
int getUnitTrueCost(int unitId, bool combat)
{
	return Units[unitId].cost + (isUnitHasAbility(unitId, ABL_CLEAN_REACTOR) ? 0 : (combat ? 4 : 8));
}

double getVehicleApproachChance(int baseId, int vehicleId)
{
	MAP *baseTile = getBaseMapTile(baseId);
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	// find closest enemy base
	
	int closestEnemyBaseId = -1;
	int closestEnemyBaseRange = INT_MAX;
	
	for (int enemyBaseId = 0; enemyBaseId < *total_num_bases; enemyBaseId++)
	{
		BASE *enemyBase = getBase(baseId);
		MAP *enemyBaseTile = getBaseMapTile(enemyBaseId);
		
		// exclude own
		
		if (enemyBase->faction_id == vehicle->faction_id)
			continue;
		
		// exclude our
		
		if (enemyBase->faction_id == aiFactionId)
			continue;
		
		// at war
		
		if (!isHostile(vehicle->faction_id, enemyBase->faction_id))
			continue;
		
		// range
		
		int range = getRange(vehicleTile, enemyBaseTile);
		
		// update closest
		
		if (range < closestEnemyBaseRange)
		{
			closestEnemyBaseId = enemyBaseId;
			closestEnemyBaseRange = range;
		}
		
	}
	
	// not found
	
	if (closestEnemyBaseId == -1)
		return 1.0;
	
	// this base
	
	if (closestEnemyBaseId == baseId)
		return 1.0;
	
	// range and angle
	
	MAP *closestEnemyBaseTile = getBaseMapTile(closestEnemyBaseId);
	double angleCos = getVectorAngleCos(vehicleTile, baseTile, closestEnemyBaseTile);
	double approachChance = std::max(0.0, angleCos);
	
	return approachChance;
	
}

/*
Calculates enemy land movement impediment around the base.
Higher value slows movement more.
*/
double getBaseTravelImpediment(int range, bool oceanBase)
{
	if (range > BASE_MOVEMENT_IMPEDIMENT_MAX_RANGE)
		return 0.0;
	
	return (BASE_TRAVEL_IMPEDIMENT_COEFFICIENT / (double)std::max(1, range)) * (oceanBase ? 0.5 : 1.0);
	
}

