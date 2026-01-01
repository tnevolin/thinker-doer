#pragma GCC diagnostic ignored "-Wshadow"

#include "wtp_ai.h"

#include <float.h>
#include "robin_hood.h"

#include "wtp_ai_game.h"
#include "wtp_aiRoute.h"
#include "wtp_aiHurry.h"
#include "wtp_aiMove.h"
#include "wtp_aiProduction.h"

/**
Top level AI strategy class. Contains AI methods entry points and global AI variable precomputations.
Faction uses WTP AI algorithms if configured. Otherwise, it falls back to Thinker.
Move strategy prepares demands for production strategy.
Production is set not on vanilla hooks but at the end of enemy_turn phase for all bases one time.

--- control flow ---

faction_upkeep -> modifiedFactionUpkeep [WTP] (substitute)
-> aiFactionUpkeep [WTP]
	-> considerHurryingProduction [WTP]
	-> faction_upkeep
		-> mod_faction_upkeep [Thinker]
			-> production_phase

enemy_turn -> wtp_mod_enemy_turn [WTP] (substitute)
-> moveStrategy [WTP]
	-> all kind of specific move strategies [WTP]
	-> productionStrategy [WTP]
	-> setProduction [WTP]
	-> enemy_turn

*/

/**
AI Faction upkeep entry point.
This is called for WTP AI enabled factions only.
This is an additional code and DOES NOT invoke original function!
*/
void aiFactionUpkeep(const int factionId)
{
	debug("aiFactionUpkeep - %s\n", MFactions[factionId].noun_faction);
	
	// set AI faction id for global reference
	
	setPlayerFactionReferences(factionId);
	
	// consider hurrying production in all bases
	
	considerHurryingProduction(factionId);
	
}

/**
Movement phase entry point.
*/
void __cdecl wtp_mod_enemy_turn(int factionId)
{
	debug("wtp_mod_enemy_turn - %s\n", getMFaction(factionId)->noun_faction);
	
	// set AI faction id for global reference

	setPlayerFactionReferences(factionId);
	
	// run WTP AI code for AI enabled factions
	
	if (isWtpEnabledFaction(factionId))
	{
		// assign vehicles to transports
		
		assignVehiclesToTransports();
		
		// vanilla fix for transpoft pickup
		
		fixUndesiredTransportPickup();
		
		// balance vehicle support
		
		balanceVehicleSupport();
		
		// execute unit movement strategy
		
		try
		{
			strategy(true);
		}
		catch(const std::exception &e)
		{
			debug(e.what());debug("\n");flushlog();
			std::rethrow_exception(std::current_exception());
		}
		
		// move vehicles
		
		aiEnemyMoveVehicles();
		
	}
	
	// execute original code
	
	debug("enemy_turn - %s - end\n", getMFaction(factionId)->noun_faction);
	
	Profiling::start("~ enemy_turn");
	mod_enemy_turn(factionId);
	Profiling::stop("~ enemy_turn");
	
	debug("wtp_mod_enemy_turn - %s - end\n", getMFaction(factionId)->noun_faction);
	
	// run WTP AI code for AI enabled factions
	
	if (isWtpEnabledFaction(factionId))
	{
		// exhaust all units movement points
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			uint8_t vehicleMoveRate = (uint8_t)getVehicleMaxMoves(vehicleId);
			
			if (vehicle->faction_id != factionId)
				continue;
			
			vehicle->moves_spent = std::max(vehicle->moves_spent, vehicleMoveRate);
			
		}
		
		// disband oversupported vehicles
		
		disbandOrversupportedVehicles(factionId);
		
		// disband unneeded vehicles
		
		disbandUnneededVehicles();
		
	}
	
}

void strategy(bool computer)
{
	Profiling::start("strategy", "");
	
	// design units
	
	if (computer)
	{
		designUnits();
	}
	
	// populate data
	
	populateAIData();
	
	// move strategy
	
	moveStrategy();
	
	// compute production demands
	
	if (computer)
	{
		productionStrategy();
	}
	
	// execute tasks
	
	executeTasks();
	
	Profiling::stop("strategy");
	
}

void executeTasks()
{
	Profiling::start("executeTasks", "strategy");
	
	debug("Tasks - %s\n", MFactions[aiFactionId].noun_faction);
	
	if (DEBUG)
	{
		debug("\ttask list\n");
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			Task *task = getTask(vehicleId);
			if (task == nullptr)
				continue;
			
			debug("\t\t%s\n", task->toString());
			
		}
		debug("\t\t-----------------------------------------------------------------------------\n\n");
		
	}
	
	for (robin_hood::pair<int, Task> &taskEntry : aiData.tasks)
	{
		Task &task = taskEntry.second;
		
		int vehicleId = task.getTaskVehicleId();
		if (vehicleId == -1)
			continue;
		
		VEH *vehicle = getVehicle(vehicleId);
		if (vehicle == nullptr)
			continue;
		
		// skip not fully automated human player units
		
		if (aiFactionId == *CurrentPlayerFaction && !(conf.manage_player_units && ((vehicle->state & VSTATE_ON_ALERT) != 0) && vehicle->movement_turns == 0))
			continue;
		
		// do not execute combat tasks immediatelly
		
		if (task.type == TT_MELEE_ATTACK || task.type == TT_ARTILLERY_ATTACK)
			continue;
		
		debug("\t%s\n", task.toString());
		
		task.execute();
		
	}
	
	Profiling::stop("executeTasks");
	
}

void populateAIData()
{
	Profiling::start("populateAIData", "strategy");
	
	// clear aiData
	
	Profiling::start("aiData.clear()", "populateAIData");
	aiData.clear();
	Profiling::stop("aiData.clear()");
	
	// global
	
	populateGlobalVariables();
	
	// map of tile infos
	
	populateTileInfos();
	populateTileInfoBaseRanges();
	populateRegionAreas();
	
	// basic info
	// not dependent on other computations
	
	populateFactionInfos();
	populateBaseInfos();
	
	// route data
	// dependent on faction info (bestSeaTransport)
	
	populateRouteData();
	
	// player info
	
	populatePlayerGlobalVariables();
	populatePlayerBaseIds();
debug(">populatePlayerBaseRanges\n");flushlog();
	populatePlayerBaseRanges();
debug(">populateUnits\n");flushlog();
	populateUnits();
//	saveVehicles();
	populateVehicles();
	populateEmptyEnemyBaseTiles();
	populateDangerZones();
	populatePlayerFactionIncome();
	populateMaxMineralSurplus();
	populateBasePoliceData();
	
	// other computations
	
	computeUnitDestructionGains();
	populateCombatEffects();
	
	// enemy info
	
	populateEnemyBaseInfos();
	populateEnemyStacks();
	evaluateEnemyStacks();
	
	// evaluate defense
	
	evaluateBaseDefense();
	evaluateBaseProbeDefense();
	
	Profiling::stop("populateAIData");
	
}

void populateGlobalVariables()
{
	aiData.developmentScale = getDevelopmentScale();
	aiData.newBaseGain = getNewBaseGain();
	
	// psi vehicle ratio
	
	int combatVehicleCount = 0;
	int combatPsiGearCount = 0;
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		if (isUnitPsiOffense(vehicle->unit_id))
		{
			combatPsiGearCount++;
		}
		if (isUnitPsiDefense(vehicle->unit_id))
		{
			combatPsiGearCount++;
		}
		
		combatVehicleCount++;
		
	}
	
	aiData.psiVehicleRatio = combatVehicleCount == 0 ? 0.0 : (combatPsiGearCount / 2.0) / combatVehicleCount;
	
}

void populateTileInfos()
{
	Profiling::start("populateTileInfos", "populateAIData");
	
	debug("populateTileInfos - %s\n", aiMFaction->noun_faction);
	
	// ocean, surface type, coast
	
	Profiling::start("tile info", "populateTileInfos");
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		// set tileIndex
		
		tileInfo.index = tileIndex;
		tileInfo.tile = tile;
		
		// set ocean
		
		if (is_ocean(tile))
		{
			tileInfo.land = false;
			tileInfo.ocean = true;
			tileInfo.surfaceType = ST_SEA;
			tileInfo.coast = false;
			tileInfo.rough = false;
		}
		else
		{
			tileInfo.land = true;
			tileInfo.ocean = false;
			tileInfo.surfaceType = ST_LAND;
			tileInfo.coast = false;
			tileInfo.rough = tile->is_rocky() || map_has_item(tile, BIT_FOREST);
			tileInfo.fungus = tile->is_fungus();
		}
		
		int adjacentTileCount = 0;
		int adjacentLandTileCount = 0;
		int adjacentSeaTileCount = 0;
		for (MAP *adjacentTile : getAdjacentTiles(tile))
		{
			if (is_ocean(adjacentTile))
			{
				adjacentSeaTileCount++;
			}
			else
			{
				adjacentLandTileCount++;
			}
			
			adjacentTileCount++;
			
		}
		
		tileInfo.adjacentLand = adjacentLandTileCount > 0;
		tileInfo.adjacentSea = adjacentSeaTileCount > 0;
		tileInfo.coast = tileInfo.land && tileInfo.adjacentSea;
		tileInfo.adjacentLandRatio = adjacentTileCount <= 0 ? 0.0 : (double) adjacentLandTileCount / (double) adjacentTileCount;
		tileInfo.adjacentSeaRatio = adjacentTileCount <= 0 ? 0.0 : (double) adjacentSeaTileCount / (double) adjacentTileCount;
		
	}
	
	Profiling::stop("tile info");
	
	// base
	
	Profiling::start("base", "populateTileInfos");
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		BaseInfo &baseInfo  = aiData.getBaseInfo(baseId);
		TileInfo &baseTileInfo = aiData.getTileInfo(baseTile);
		
		baseTileInfo.baseId = baseId;
		baseTileInfo.base = true;
		
		// port
		
		bool port = (!is_ocean(baseTile) && isBaseAccessesWater(baseId));
		
		baseInfo.port = port;
		baseTileInfo.port = port;
		
	}
	
	Profiling::stop("base");
	
	// bunkers and airbases
	
	Profiling::start("bunkers and airbases", "populateTileInfos");
	
	aiData.bunkerInfos.clear();
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		tileInfo.bunker = map_has_item(tile, BIT_BUNKER);
		tileInfo.airbase = map_has_item(tile, BIT_BASE_IN_TILE | BIT_AIRBASE);
		
		if (tileInfo.bunker && tile->owner == aiFactionId)
		{
			aiData.bunkerInfos.emplace(tile, BunkerInfo());
		}
		
	}
	
	Profiling::stop("bunkers and airbases");
	
	// land vehicle allowed
	
	Profiling::start("land vehicle allowed", "populateTileInfos");
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		tileInfo.landAllowed = !tileInfo.ocean || tileInfo.base;
		
	}
	
	Profiling::stop("land vehicle allowed");
	
	// vehicles
	
	Profiling::start("vehicles", "populateTileInfos");
	
	for (TileInfo &tileInfo : aiData.tileInfos)
	{
		tileInfo.vehicleIds.clear();
		tileInfo.playerVehicleIds.clear();
		std::fill(tileInfo.factionNeedlejetInFlights.begin(), tileInfo.factionNeedlejetInFlights.end(), false);
	}
		
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH &vehicle = Vehs[vehicleId];
		TileInfo &tileInfo = aiData.getVehicleTileInfo(vehicleId);
		
		tileInfo.vehicleIds.push_back(vehicleId);
		
		if (Vehs[vehicleId].faction_id == aiFactionId)
		{
			tileInfo.playerVehicleIds.push_back(vehicleId);
		}
		
		if (!tileInfo.airbase && isNeedlejetVehicle(vehicleId))
		{
			tileInfo.factionNeedlejetInFlights.at(vehicle.faction_id) = true;
		}
		
	}
	
	Profiling::stop("vehicles");
	
	// adjacent tiles
	
	Profiling::start("adjacent tiles", "populateTileInfos");
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		tileInfo.adjacentAngleTileInfos.clear();
		
		for (int angle = 0; angle < ANGLE_COUNT; angle++)
		{
			int adjacentTileIndex = getTileIndexByAngle(tileIndex, angle);
			
			if (adjacentTileIndex != -1)
			{
				TileInfo &adjacentTileInfo = aiData.tileInfos.at(adjacentTileIndex);
				tileInfo.adjacentAngleTileInfos.push_back({angle, &adjacentTileInfo});
			}
			
		}
		
	}
	
	Profiling::stop("adjacent tiles");
	
	// range tiles
	
	Profiling::start("range tiles", "populateTileInfos");
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		
		tileInfo.range2CenterTileInfos.clear();
		for (MAP *rangeTile : getRangeTiles(tileInfo.tile, 2, true))
		{
			TileInfo &rangeTileInfo = aiData.getTileInfo(rangeTile);
			tileInfo.range2CenterTileInfos.push_back(&rangeTileInfo);
		}
		
		tileInfo.range2NoCenterTileInfos.clear();
		for (MAP *rangeTile : getRangeTiles(tileInfo.tile, 2, false))
		{
			TileInfo &rangeTileInfo = aiData.getTileInfo(rangeTile);
			tileInfo.range2NoCenterTileInfos.push_back(&rangeTileInfo);
		}
		
	}
	
	Profiling::stop("range tiles");
	
	// hexCosts
	
	Profiling::start("hexCosts", "populateTileInfos");
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		int tileX = getX(tileIndex);
		int tileY = getY(tileIndex);
		TileInfo &tileInfo = aiData.getTileInfo(tileIndex);
		
		// clear values
		
		for (std::array<int, ANGLE_COUNT> &movementTypeHexCosts : tileInfo.hexCosts)
		{
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				movementTypeHexCosts.at(angle) = -1;
			}
		}
		
		for (std::array<int, ANGLE_COUNT> &movementTypeHexCosts : tileInfo.averageHexCosts)
		{
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				movementTypeHexCosts.at(angle) = -1;
			}
		}
		
		// compute values
		
		int approximateSeaFungusHexCost = divideIntegerRoundUp(Rules->move_rate_roads * 5, 2);
		int approximateLandRoughHexCost = divideIntegerRoundUp(Rules->move_rate_roads * 5, 4);
		int approximateLandFungusHexCost = Rules->move_rate_roads * 3;
		
		for (int angle = 0; angle < ANGLE_COUNT; angle++)
		{
			int adjacentTileIndex = getTileIndexByAngle(tileIndex, angle);
			
			if (adjacentTileIndex == -1)
				continue;
			
			MAP *adjacentTile = *MapTiles + adjacentTileIndex;
			int adjacentTileX = getX(adjacentTileIndex);
			int adjacentTileY = getY(adjacentTileIndex);
			TileInfo *adjacentTileInfo = &aiData.getTileInfo(adjacentTileIndex);
			
			// air movement type
			
			int airHexCost = Rules->move_rate_roads;
			
			tileInfo.hexCosts.at(MT_AIR).at(angle)			= airHexCost;
			
			tileInfo.averageHexCosts.at(MT_AIR).at(angle)	= airHexCost;
			
			// sea vehicle moves on ocean or from/to base
			if ((tileInfo.ocean || tileInfo.base) && (adjacentTileInfo->ocean || adjacentTileInfo->base))
			{
				int seaHexCost = mod_hex_cost(BSC_UNITY_GUNSHIP, -1, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				int seaNativeHexCost = mod_hex_cost(BSC_ISLE_OF_THE_DEEP, 0, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				
				int approximateSeaHexCost = adjacentTile->is_fungus() ? approximateSeaFungusHexCost : seaHexCost;
				
				tileInfo.hexCosts.at(MT_SEA).at(angle)					= seaHexCost;
				tileInfo.hexCosts.at(MT_SEA_NATIVE).at(angle)			= seaNativeHexCost;
				
				tileInfo.averageHexCosts.at(MT_SEA).at(angle)			= approximateSeaHexCost;
				tileInfo.averageHexCosts.at(MT_SEA_NATIVE).at(angle)	= seaNativeHexCost;
				
			}
			// land vehicle moves on land
			if (tileInfo.land && adjacentTileInfo->land)
			{
				int landHexCost = mod_hex_cost(BSC_UNITY_ROVER, -1, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				int landNativeHexCost = mod_hex_cost(BSC_MIND_WORMS, 0, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				int landEasyHexCost = mod_hex_cost(BSC_FORMERS, -1, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				int landHoverHexCost = std::min(Rules->move_rate_roads, landHexCost);
				int landNativeHoverHexCost = std::min(landHoverHexCost, landNativeHexCost);
				
				int approximateLandHexCost = (adjacentTile->is_rocky() || map_has_item(adjacentTile, BIT_FOREST)) ? approximateLandRoughHexCost : landHexCost;
				int approximateLandNativeHexCost = (adjacentTile->is_rocky() || map_has_item(adjacentTile, BIT_FOREST)) ? approximateLandRoughHexCost : landNativeHexCost;
				int approximateLandEasyHexCost = adjacentTile->is_fungus() ? approximateLandFungusHexCost : landEasyHexCost;
				
				tileInfo.hexCosts.at(MT_LAND).at(angle)						= landHexCost;
				tileInfo.hexCosts.at(MT_LAND_NATIVE).at(angle)				= landNativeHexCost;
				tileInfo.hexCosts.at(MT_LAND_EASY).at(angle)				= landEasyHexCost;
				tileInfo.hexCosts.at(MT_LAND_HOVER).at(angle)				= landHoverHexCost;
				tileInfo.hexCosts.at(MT_LAND_NATIVE_HOVER).at(angle)		= landNativeHoverHexCost;
				
				tileInfo.averageHexCosts.at(MT_LAND).at(angle)				= approximateLandHexCost;
				tileInfo.averageHexCosts.at(MT_LAND_NATIVE).at(angle)		= approximateLandNativeHexCost;
				tileInfo.averageHexCosts.at(MT_LAND_EASY).at(angle)			= approximateLandEasyHexCost;
				tileInfo.averageHexCosts.at(MT_LAND_HOVER).at(angle)		= landHoverHexCost;
				tileInfo.averageHexCosts.at(MT_LAND_NATIVE_HOVER).at(angle)	= landNativeHoverHexCost;
				
			}
			
		}
		
	}
	
	Profiling::stop("hexCosts");
	
	// blocked and zoc
	
	Profiling::start("blocked and zoc", "populateTileInfos");
	
	// reset values
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		tileInfo.friendlyBase = false;
		tileInfo.unfriendlyBase = false;
		tileInfo.friendlyVehicle = false;
		tileInfo.unfriendlyVehicle = false;
		tileInfo.unfriendlyVehicleZoc = false;
	}
	
	// bases
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		TileInfo &tileInfo = aiData.getBaseTileInfo(baseId);
		
		if (isFriendly(aiFactionId, base->faction_id))
		{
			tileInfo.friendlyBase = true;
		}
		else
		{
			tileInfo.unfriendlyBase = true;
		}
		
	}
	
	// vehicles
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		TileInfo &tileInfo = aiData.getVehicleTileInfo(vehicleId);
		
		if (isFriendly(aiFactionId, vehicle->faction_id))
		{
			tileInfo.friendlyVehicle = true;
		}
		else
		{
			tileInfo.unfriendlyVehicle = true;
			
			// zoc is exerted only from land
			
			if (tileInfo.land)
			{
				for (AngleTileInfo const &adjacentAngleTileInfo : tileInfo.adjacentAngleTileInfos)
				{
					TileInfo *adjacentTileInfo = adjacentAngleTileInfo.tileInfo;
					
					// zoc is exerted only to land
					
					if (adjacentTileInfo->ocean)
						continue;
					
					// zoc is not exerted to base
					
					if (adjacentTileInfo->base)
						continue;
					
					// zoc
					
					adjacentTileInfo->unfriendlyVehicleZoc = true;
					
				}
				
			}
			
		}
		
	}
	
	// set values
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		setTileBlockedAndZoc(tileInfo);
	}
	
	Profiling::stop("blocked and zoc");
	
	// sensors
	
	Profiling::start("sensors", "populateTileInfos");
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		
		if (!map_has_item(tileInfo.tile, BIT_SENSOR))
			continue;
		
		if (tileInfo.tile->owner >= 0 && tileInfo.tile->owner < MaxPlayerNum)
		{
			for (TileInfo *rangeTileInfo : tileInfo.range2CenterTileInfos)
			{
				rangeTileInfo->sensors.at(tileInfo.tile->owner) = true;
			}
		}
		else
		{
			for (TileInfo *rangeTileInfo : tileInfo.range2CenterTileInfos)
			{
				std::fill(rangeTileInfo->sensors.begin(), rangeTileInfo->sensors.end(), true);
			}
		}
		
	}
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		int baseTileIndex = baseTile - *MapTiles;
		TileInfo &baseTileInfo = aiData.tileInfos.at(baseTileIndex);
		
		if (!has_facility(FAC_GEOSYNC_SURVEY_POD, baseId))
			continue;
		
		if (baseTile->owner >= 0 && baseTile->owner < MaxPlayerNum)
		{
			for (TileInfo *rangeTileInfo : baseTileInfo.range2CenterTileInfos)
			{
				rangeTileInfo->sensors.at(baseTile->owner) = true;
			}
		}
		else
		{
			for (TileInfo *rangeTileInfo : baseTileInfo.range2CenterTileInfos)
			{
				std::fill(rangeTileInfo->sensors.begin(), rangeTileInfo->sensors.end(), true);
			}
		}
		
	}
	
	Profiling::stop("sensors");
	
	// monoliths
	
	Profiling::start("monoliths", "populateTileInfos");
	
	aiData.monoliths.clear();
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		
		if (!map_has_item(tileInfo.tile, BIT_MONOLITH))
			continue;
		
		aiData.monoliths.push_back(tileInfo.tile);
		
	}
	
	Profiling::stop("monoliths");
	
	// pods
	
	Profiling::start("pods", "populateTileInfos");
	
	aiData.pods.clear();
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		
		if (!mod_goody_at(getX(tileInfo.index), getY(tileInfo.index)))
			continue;
		
		aiData.pods.push_back(tileInfo.tile);
		
	}
	
	Profiling::stop("pods");
	
	Profiling::start("strength multipliers");
	
	for (TileInfo &tileInfo : aiData.tileInfos)
	{
		// bombardment damage
		
		int maxBombardmentDamagePercent = (tileInfo.base || tileInfo.bunker) ? Rules->max_dmg_percent_arty_base_bunker : tileInfo.land ? Rules->max_dmg_percent_arty_open : Rules->max_dmg_percent_arty_sea;
		tileInfo.maxBombardmentDamage = (double)maxBombardmentDamagePercent / 100.0;
		
		// strength multipliers
		
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			tileInfo.sensorOffenseMultipliers.at(factionId) = getSensorOffenseMultiplier(factionId, tileInfo.tile);
			tileInfo.sensorDefenseMultipliers.at(factionId) = getSensorDefenseMultiplier(factionId, tileInfo.tile);
		}
		
		// terrain defense bonuses
		
		if (tileInfo.base)
		{
			for (AttackTriad attackTriad : ATTACK_TRIADS)
			{
				tileInfo.terrainDefenseMultipliers.at(attackTriad)		= getBaseDefenseMultiplier(tileInfo.baseId, attackTriad);
			}
		}
		else if (tileInfo.bunker)
		{
			tileInfo.terrainDefenseMultipliers.at(ATTACK_TRIAD_LAND)	= getPercentageBonusMultiplier(conf.bunker_bonus_surface);
			tileInfo.terrainDefenseMultipliers.at(ATTACK_TRIAD_SEA)		= getPercentageBonusMultiplier(conf.bunker_bonus_surface);
			tileInfo.terrainDefenseMultipliers.at(ATTACK_TRIAD_AIR)		= getPercentageBonusMultiplier(conf.bunker_bonus_aerial);
			tileInfo.terrainDefenseMultipliers.at(ATTACK_TRIAD_PSI)		= 1.0;
		}
		else
		{
			for (AttackTriad attackTriad : ATTACK_TRIADS)
			{
				tileInfo.terrainDefenseMultipliers.at(attackTriad)		= 1.0;
			}
		}
		
	}
	
	Profiling::stop("strength multipliers");
	
//	if (DEBUG)
//	{
//		debug("\t\tblocked\n");
//		
//		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
//		{
//			if (isBlocked(tile))
//			{
//				debug("\t\t\t%s\n", getLocationString(tile));
//			}
//			
//		}
//		
//		debug("\t\tzoc\n");
//		
//		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
//		{
//			if (isZoc(tile))
//			{
//				debug("\t\t\t%s\n", getLocationString(tile));
//			}
//			
//		}
//		
//	}
	
	Profiling::stop("populateTileInfos");
	
}

void populateTileInfoBaseRanges()
{
	Profiling::start("populateTileInfoBaseRanges", "populateAIData");
	
	debug("populateTileInfoBaseRanges - %s\n", aiMFaction->noun_faction);
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		int tileX = getX(tileIndex);
		int tileY = getY(tileIndex);
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tileIndex);
		
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE const &base = Bases[baseId];
			MAP *baseTile = getBaseMapTile(baseId);
			
			if (base.faction_id != aiFactionId)
				continue;
			
			int range = map_range(tileX, tileY, base.x, base.y);
			
			if (range < tileInfo.nearestBaseRanges.at(TRIAD_AIR).value)
			{
				tileInfo.nearestBaseRanges.at(TRIAD_AIR).id = baseId;
				tileInfo.nearestBaseRanges.at(TRIAD_AIR).value = range;
			}
			
			if (is_ocean(tile) && is_ocean(baseTile) && tile->region == baseTile->region)
			{
				if (range < tileInfo.nearestBaseRanges.at(TRIAD_SEA).value)
				{
					tileInfo.nearestBaseRanges.at(TRIAD_SEA).id = baseId;
					tileInfo.nearestBaseRanges.at(TRIAD_SEA).value = range;
				}
			}
			
			if (!is_ocean(tile) && !is_ocean(baseTile) && tile->region == baseTile->region)
			{
				if (range < tileInfo.nearestBaseRanges.at(TRIAD_SEA).value)
				{
					tileInfo.nearestBaseRanges.at(TRIAD_SEA).id = baseId;
					tileInfo.nearestBaseRanges.at(TRIAD_SEA).value = range;
				}
			}
			
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
//		{
//			TileInfo &tileInfo = aiData.getTileInfo(tile);
//			
//			debug("\t%s", getLocationString(tile));
//			for (int i = 0; i < 3; i ++)
//			{
//				debug(" [%3d] %4d", tileInfo.nearestBaseRanges.at(i).id, tileInfo.nearestBaseRanges.at(i).id == -1 ? -1 : tileInfo.nearestBaseRanges.at(i).value);
//			}
//			debug("\n");
//			
//		}
//		
//	}
	
	Profiling::stop("populateTileInfoBaseRanges");
	
}

void populateRegionAreas()
{
	Profiling::start("populateRegionAreas", "populateAIData");
	
	debug("populateRegionAreas - %s\n", aiMFaction->noun_faction);
	
	robin_hood::unordered_flat_map<int, int> &regionAreas = aiData.regionAreas;
	regionAreas.clear();
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		// not polar region
		
		if (isPolarRegion(tile))
			continue;
		
		// accumulate region area
		
		if (regionAreas.find(tile->region) == regionAreas.end())
		{
			regionAreas.insert({tile->region, 0});
		}
		regionAreas.at(tile->region)++;
		
		// populate adjacent sea regions
		
		if (is_ocean(tile))
		{
			for (MAP *adjacentTile : getAdjacentTiles(tile))
			{
				// land region, not polar
				
				if (!isLandRegion(adjacentTile))
					continue;
				
				// accumulate adjacent sea regions
				
				aiData.tileInfos.at(adjacentTile - *MapTiles).adjacentSeaRegions.insert(tile->region);
				
			}
			
		}
		
	}
	
	// enemyRegions
	
	aiData.enemyRegions.clear();
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE &base = Bases[baseId];
		MAP *baseTile = getBaseMapTile(baseId);
		
		if (base.faction_id == aiFactionId)
			continue;
		
		aiData.enemyRegions.insert(baseTile->region);
		
	}
	
//	if (DEBUG)
//	{
//		debug("\tseaRegionAreas\n");
//		
//		for (std::pair<int, int> const &seaRegionAreaEntry : aiData.seaRegionAreas)
//		{
//			int seaRegion = seaRegionAreaEntry.first;
//			int seaRegionArea = seaRegionAreaEntry.second;
//			debug("\t\t%2d %4d\n", seaRegion, seaRegionArea);
//		}
//		
//		debug("\tadjacentSeaRegions\n");
//		
//		for (TileInfo const &tileInfo : aiData.tileInfos)
//		{
//			if (tileInfo.adjacentSeaRegions.empty())
//				continue;
//			
//			debug("\t\t%s %d\n", getLocationString(tileInfo.tile).c_str(), tileInfo.adjacentSeaRegions.size());
//			
//		}
//		
//	}
	
	Profiling::stop("populateRegionAreas");
	
}

void populatePlayerBaseIds()
{
	Profiling::start("populatePlayerBaseIds", "populateAIData");
	
	aiData.baseIds.clear();
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
		
		// player base
		
		if (base->faction_id != aiFactionId)
			continue;
			
		// add base to player baseIds
		
		aiData.baseIds.push_back(baseId);
		
		// set base artillery
		
		baseInfo.artillery = false;
		
		for (int vehicleId : baseTileInfo.vehicleIds)
		{
			if (isArtilleryVehicle(vehicleId))
			{
				baseInfo.artillery = true;
				break;
			}
			
		}
		
	}
	
	Profiling::stop("populatePlayerBaseIds");
	
}

void populatePlayerBaseRanges()
{
	Profiling::start("populatePlayerBaseRanges", "populateAIData");
	
	debug("populatePlayerBaseRanges - %s\n", aiMFaction->noun_faction);
	
	for (TileInfo &tileInfo : aiData.tileInfos)
	{
		for (Triad triad : TRIADS)
		{
			tileInfo.baseRanges.at(triad) = INT_MAX;
//			tileInfo.baseDistances.at(triad) = DBL_MAX;
		}
	}
	
	std::vector<MAP *> openNodes;
	std::vector<MAP *> newOpenNodes;
	
	int baseRange = 0;
	
	// air baseRange
	
	openNodes.clear();	
	newOpenNodes.clear();
	
	baseRange = 0;
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		TileInfo &baseTileInfo = aiData.getTileInfo(baseTile);
		
		baseTileInfo.baseRanges.at(TRIAD_AIR) = 0;
//		baseTileInfo.baseDistances.at(TRIAD_AIR) = 0.0;
		openNodes.push_back(baseTile);
		
	}
	
	while (openNodes.size() > 0)
	{
		baseRange++;
		
		for (MAP *tile : openNodes)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			for (AngleTileInfo const &adjacentAngleTileInfo : tileInfo.adjacentAngleTileInfos)
			{
//				int angle = adjacentAngleTileInfo.angle;
				TileInfo *adjacentTileInfo = adjacentAngleTileInfo.tileInfo;
				
				if (baseRange < adjacentTileInfo->baseRanges.at(TRIAD_AIR))
				{
					adjacentTileInfo->baseRanges.at(TRIAD_AIR) = baseRange;
					newOpenNodes.push_back(adjacentTileInfo->tile);
				}
				
//				double baseDistance = tileInfo.baseDistances.at(TRIAD_AIR) + (angle % 2 == 0 ? 1.0 : 1.5);
//				adjacentTileInfo->baseDistances.at(TRIAD_AIR) = std::min(adjacentTileInfo->baseDistances.at(TRIAD_AIR), baseDistance);
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	// seaBaseRange
	
	openNodes.clear();	
	newOpenNodes.clear();
	
	baseRange = 0;
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		TileInfo &baseTileInfo = aiData.getTileInfo(baseTile);
		
		// sea base
		
		if (!is_ocean(baseTile))
			continue;
		
		baseTileInfo.baseRanges.at(TRIAD_SEA) = 0;
//		baseTileInfo.baseDistances.at(TRIAD_SEA) = 0.0;
		openNodes.push_back(baseTile);
		
	}
	
	while (openNodes.size() > 0)
	{
		baseRange++;
		
		for (MAP *tile : openNodes)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			for (AngleTileInfo const &adjacentAngleTileInfo : tileInfo.adjacentAngleTileInfos)
			{
//				int angle = adjacentAngleTileInfo.angle;
				TileInfo *adjacentTileInfo = adjacentAngleTileInfo.tileInfo;
				
				if (baseRange < adjacentTileInfo->baseRanges.at(TRIAD_SEA))
				{
					adjacentTileInfo->baseRanges.at(TRIAD_SEA) = baseRange;
					newOpenNodes.push_back(adjacentTileInfo->tile);
				}
				
//				double baseDistance = tileInfo.baseDistances.at(TRIAD_SEA) + (angle % 2 == 0 ? 1.0 : 1.5);
//				adjacentTileInfo->baseDistances.at(TRIAD_SEA) = std::min(adjacentTileInfo->baseDistances.at(TRIAD_SEA), baseDistance);
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	// landBaseRange
	
	openNodes.clear();	
	newOpenNodes.clear();
	
	baseRange = 0;
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		TileInfo &baseTileInfo = aiData.getTileInfo(baseTile);
		
		// land base
		
		if (is_ocean(baseTile))
			continue;
		
		baseTileInfo.baseRanges.at(TRIAD_LAND) = 0;
//		baseTileInfo.baseDistances.at(TRIAD_LAND) = 0.0;
		openNodes.push_back(baseTile);
		
	}
	
	while (openNodes.size() > 0)
	{
		baseRange++;
		
		for (MAP *tile : openNodes)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			for (AngleTileInfo const &adjacentAngleTileInfo : tileInfo.adjacentAngleTileInfos)
			{
//				int angle = adjacentAngleTileInfo.angle;
				TileInfo *adjacentTileInfo = adjacentAngleTileInfo.tileInfo;
				
				if (baseRange < adjacentTileInfo->baseRanges.at(TRIAD_LAND))
				{
					adjacentTileInfo->baseRanges.at(TRIAD_LAND) = baseRange;
					newOpenNodes.push_back(adjacentTileInfo->tile);
				}
				
//				double baseDistance = tileInfo.baseDistances.at(TRIAD_LAND) + (angle % 2 == 0 ? 1.0 : 1.5);
//				adjacentTileInfo->baseDistances.at(TRIAD_LAND) = std::min(adjacentTileInfo->baseDistances.at(TRIAD_LAND), baseDistance);
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	if (DEBUG)
	{
		for (TileInfo const &tileInfo : aiData.tileInfos)
		{
			debug("\t%s", getLocationString(tileInfo.tile));
			int range;
			if ((range = tileInfo.baseRanges.at(TRIAD_AIR))		== INT_MAX) { debug(" INF"); } else { debug(" %3d", range); }
			if ((range = tileInfo.baseRanges.at(TRIAD_SEA))		== INT_MAX) { debug(" INF"); } else { debug(" %3d", range); }
			if ((range = tileInfo.baseRanges.at(TRIAD_LAND))	== INT_MAX) { debug(" INF"); } else { debug(" %3d", range); }
			debug("\n");
		}
		
	}
	
	Profiling::stop("populatePlayerBaseRanges");
	
}

void populateFactionInfos()
{
	Profiling::start("populateFactionInfos", "populateAIData");
	
	debug("populateFactionInfos - %s\n", aiMFaction->noun_faction);
	
	aiData.maxConOffenseValue = 0;
	aiData.maxConDefenseValue = 0;
	
	// iterate factions
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		Faction &faction = Factions[factionId];
		FactionInfo &factionInfo = aiData.factionInfos[factionId];
		
		// can build unit types
		
		for (int type = 0; type < 2; type++)
		{
			for (int triad = 0; triad < 3; triad++)
			{
				factionInfo.canBuildMelee.at(type).at(triad) = isFactionCanBuildMelee(factionId, type, triad);
			}
			
		}
		
		// combat modifiers
		
		factionInfo.offenseMultiplier = getFactionOffenseMultiplier(factionId);
		factionInfo.defenseMultiplier = getFactionDefenseMultiplier(factionId);
		factionInfo.fanaticBonusMultiplier = getFactionFanaticBonusMultiplier(factionId);
		
		debug
		(
			"\t%-24s: offenseMultiplier=%4.2f, defenseMultiplier=%4.2f, fanaticBonusMultiplier=%4.2f\n",
			MFactions[factionId].noun_faction,
			factionInfo.offenseMultiplier,
			factionInfo.defenseMultiplier,
			factionInfo.fanaticBonusMultiplier
		);
		
		// best combat item values
		
		factionInfo.maxConOffenseValue = getFactionMaxConOffenseValue(factionId);
		factionInfo.maxConDefenseValue = getFactionMaxConDefenseValue(factionId);
		factionInfo.fastestTriadChassisIds = getFactionFastestTriadChassisIds(factionId);
		
		aiData.maxConOffenseValue = std::max(aiData.maxConOffenseValue, factionInfo.maxConOffenseValue);
		aiData.maxConDefenseValue = std::max(aiData.maxConDefenseValue, factionInfo.maxConDefenseValue);
		
		// threat koefficient
		
		double threatCoefficient;
		
		if (factionId == aiFactionId)
		{
			threatCoefficient = 0.0;
		}
		else if (factionId == 0 || isVendetta(aiFactionId, factionId))
		{
			threatCoefficient = conf.ai_production_threat_coefficient_vendetta;
		}
		else if (isPact(aiFactionId, factionId))
		{
			threatCoefficient = conf.ai_production_threat_coefficient_pact;
		}
		else if (isTreaty(aiFactionId, factionId))
		{
			threatCoefficient = conf.ai_production_threat_coefficient_treaty;
		}
		else
		{
			threatCoefficient = conf.ai_production_threat_coefficient_other;
		}
		
		if (is_human(factionId))
		{
			threatCoefficient *= conf.ai_production_threat_coefficient_human;
		}
		
		factionInfo.threatCoefficient = threatCoefficient;
		
		debug("\t%-24s threatCoefficient=%4.2f\n", MFactions[factionId].noun_faction, threatCoefficient);
		
		// enemy count
		
		factionInfo.enemyCount = 0;
		
		for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
		{
			// others
			
			if (otherFactionId == factionId)
				continue;
				
			if (isVendetta(factionId, otherFactionId))
			{
				factionInfo.enemyCount++;
			}
			
		}
		
		// closestPlayerBaseRange
		
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = getBase(baseId);
			MAP *baseTile = getBaseMapTile(baseId);
			BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
			
			// faction base
			
			if (base->faction_id != factionId)
				continue;
			
			baseInfo.closestPlayerBaseRange = INT_MAX;
			
			for (int playerBaseId = 0; playerBaseId < *BaseCount; playerBaseId++)
			{
				BASE *playerBase = getBase(playerBaseId);
				MAP *playerBaseTile = getBaseMapTile(playerBaseId);
				
				// player base
				
				if (playerBase->faction_id != aiFactionId)
					continue;
				
				int range = getRange(playerBaseTile, baseTile);
				
				baseInfo.closestPlayerBaseRange = std::min(baseInfo.closestPlayerBaseRange, range);
				
			}
			
		}
		
		// totalVendettaCount
		
		factionInfo.totalVendettaCount = 0;
		
		for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
		{
			// exclude self
			
			if (otherFactionId == factionId)
				continue;
			
			// exclude player
			
			if (otherFactionId == aiFactionId)
				continue;
			
			// vendetta
			
			if (!isVendetta(factionId, otherFactionId))
				continue;
			
			factionInfo.totalVendettaCount++;
			
		}
		
		// productionPower
		
		factionInfo.productionPower = 1.0;
		
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = getBase(baseId);
			
			// faction base
			
			if (base->faction_id != factionId)
				continue;
			
			// base production power
			
			double baseProductionPower = getResourceScore(base->mineral_surplus, base->economy_total);
			
			factionInfo.productionPower += baseProductionPower;
			
		}
		
		// sea transport
		
		factionInfo.bestSeaTransportUnitId = -1;
		factionInfo.bestSeaTransportUnitSpeed = 0;
		factionInfo.bestSeaTransportUnitCapacity = 0;
		
		for (int unitId : getDesignedFactionUnitIds(factionId, false, true))
		{
			// sea transport
			
			if (!isSeaTransportUnit(unitId))
				continue;
			
			// update value
			
			UNIT *unit = getUnit(unitId);
			int speed = getUnitSpeed(factionId, unitId);
			int capacity = unit->carry_capacity;
			
			if (speed > factionInfo.bestSeaTransportUnitSpeed || (speed == factionInfo.bestSeaTransportUnitSpeed && capacity > factionInfo.bestSeaTransportUnitCapacity))
			{
				factionInfo.bestSeaTransportUnitId = unitId;
				factionInfo.bestSeaTransportUnitSpeed = speed;
				factionInfo.bestSeaTransportUnitCapacity = capacity;
			}
			
		}
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			
			// this faction
			
			if (vehicle->faction_id != factionId)
				continue;
			
			// sea transport
			
			if (!isSeaTransportVehicle(vehicleId))
				continue;
			
			// update value
			
			int speed = getVehicleSpeed(vehicleId);
			int capacity = Units[vehicle->unit_id].carry_capacity;
			
			if (speed > factionInfo.bestSeaTransportUnitSpeed || (speed == factionInfo.bestSeaTransportUnitSpeed && capacity > factionInfo.bestSeaTransportUnitCapacity))
			{
				factionInfo.bestSeaTransportUnitId = vehicle->unit_id;
				factionInfo.bestSeaTransportUnitSpeed = speed;
				factionInfo.bestSeaTransportUnitCapacity = capacity;
			}
			
		}
		
		// save diplo_status
		
		std::copy(std::begin(faction.diplo_status), std::end(faction.diplo_status), std::begin(factionInfo.diplo_status));
		
		// units
		
		for (int unitId : getDesignedFactionUnitIds(factionId, true, true))
		{
			// available
			
			factionInfo.availableUnitIds.push_back(unitId);
			
			// buildable (not obsolete)
			
			if (!isUnitObsolete(unitId, aiFactionId))
			{
				factionInfo.buildableUnitIds.push_back(unitId);
			}
			
		}
		
		if (DEBUG)
		{
			debug("\tavailableUnitIds\n");
			for (int unitId : factionInfo.availableUnitIds)
			{
				debug("\t\t[%4d] %-32s\n", unitId, Units[unitId].name);
			}
			
			debug("\tbuildableUnitIds\n");
			for (int unitId : factionInfo.buildableUnitIds)
			{
				debug("\t\t[%4d] %-32s\n", unitId, Units[unitId].name);
			}
			
		}
	
	}
	
	// average conventional values
	
	int countConOffenseValue = 0;
	int sumConOffenseValue = 0;
	int countConDefenseValue = 0;
	int sumConDefenseValue = 0;
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		// combat
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		// values
		
		int offenseValue = getVehicleOffenseValue(vehicleId);
		int defenseValue = getVehicleDefenseValue(vehicleId);
		
		// accumulate
		
		if (offenseValue > 0)
		{
			countConOffenseValue++;
			sumConOffenseValue += offenseValue;
		}
		
		if (defenseValue > 0)
		{
			countConDefenseValue++;
			sumConDefenseValue += defenseValue;
		}
		
	}
	
	aiData.avgConOffenseValue = countConOffenseValue == 0 ? 1.0 : (double)sumConOffenseValue / (double)countConOffenseValue;
	aiData.avgConDefenseValue = countConDefenseValue == 0 ? 1.0 : (double)sumConDefenseValue / (double)countConDefenseValue;
	
	Profiling::stop("populateFactionInfos");
	
}

void populateBaseInfos()
{
	Profiling::start("populateBaseInfos", "populateAIData");
	
	debug("populateBaseInfos - %s\n", aiMFaction->noun_faction);
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		BaseInfo &baseInfo = aiData.baseInfos.at(baseId);
		
		// store base snapshot
		
        memcpy(&baseInfo.snapshot, base, sizeof(BASE));
		
		// base info
		
		baseInfo.id = baseId;
		baseInfo.tile = getBaseMapTile(baseId);
		baseInfo.factionId = base->faction_id;
		
		// morale
		
		for (int extendedTriad = 0; extendedTriad < 4; extendedTriad++)
		{
			baseInfo.moraleMultipliers.at(extendedTriad) = getMoraleMultiplier(2 + getBaseMoraleModifier(baseId, extendedTriad));
		}
		
		// gain
		
		baseInfo.gain = getBaseGain(baseId);
		
		debug("\t%-25s gain=%5.2f\n", base->name, baseInfo.gain);
		
	}
	
	// populate faction average base gain
	
	SummaryStatistics baseGainSummary;
	
	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		FactionInfo &factionInfo = aiData.factionInfos.at(factionId);
		
		debug("averageBaseGain - %s\n", MFactions[factionId].noun_faction);
		
		baseGainSummary.clear();
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = getBase(baseId);
			BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
			
			// this faction
			
			if (base->faction_id != factionId)
				continue;
			
			debug("\t%-25s gain=%7.2f\n", base->name, baseInfo.gain);
			
			// accumulate
			
			baseGainSummary.add(baseInfo.gain);
			
		}
		
		factionInfo.averageBaseGain = baseGainSummary.mean();
		debug("\t%-25s gain=%7.2f\n", "--- average ---", factionInfo.averageBaseGain);
		
	}
	
	Profiling::stop("populateBaseInfos");
	
}

void populatePlayerGlobalVariables()
{
	Profiling::start("populatePlayerGlobalVariables", "populateAIData");
	
	// police2xUnitAvailable
	
	aiData.police2xUnitAvailable = false;
	
	for (int unitId : aiFactionInfo->availableUnitIds)
	{
		// police2xUnit
		
		if (isInfantryDefensivePolice2xUnit(unitId, aiFactionId))
		{
			aiData.police2xUnitAvailable = true;
		}
		
	}
	
	// summaries
	
	aiData.maxConOffenseValue = getFactionMaxConOffenseValue(aiFactionId);
	aiData.maxConDefenseValue = getFactionMaxConDefenseValue(aiFactionId);
	
	// average values
	
	{
		int citizenCount = 0;
		double citizenMineralIntakeSum = 0.0;
		double citizenEconomyIntakeSum = 0.0;
		double citizenLabsIntakeSum = 0.0;
		double citizenPsychIntakeSum = 0.0;
		double citizenMineralIntake2Sum = 0.0;
		double citizenEconomyIntake2Sum = 0.0;
		double citizenLabsIntake2Sum = 0.0;
		double citizenPsychIntake2Sum = 0.0;
		double citizenResourceIncomeSum = 0.0;
		
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = getBase(baseId);
			
			// faction base
			
			if (base->faction_id != aiFactionId)
				continue;
			
			// base resource
			
			double energyEfficiencyCoefficient = getBaseEnergyEfficiencyCoefficient(baseId);
			
			double economyAllocation = (double)(10 - aiFaction->SE_alloc_labs + aiFaction->SE_alloc_psych) / 10.0;
			double labsAllocation = (double)(aiFaction->SE_alloc_labs) / 10.0;
			double psychAllocation = (double)(aiFaction->SE_alloc_psych) / 10.0;
			
			double mineralMultiplier = getBaseMineralMultiplier(baseId);
			double economyMultiplier = getBaseEconomyMultiplier(baseId);
			double labsMultiplier = getBaseLabsMultiplier(baseId);
			double psychMultiplier = getBasePsychMultiplier(baseId);
			
			double baseSquareMineralIntake = (double)ResInfo->base_sq.mineral;
			double baseSquareEconomyIntake = (double)ResInfo->base_sq.energy * energyEfficiencyCoefficient * economyAllocation;
			double baseSquareLabsIntake = (double)ResInfo->base_sq.energy * energyEfficiencyCoefficient * labsAllocation;
			double baseSquarePsychIntake = (double)ResInfo->base_sq.energy * energyEfficiencyCoefficient * psychAllocation;
			
			double baseCitizensMineralIntake = base->mineral_intake - baseSquareMineralIntake;
			double baseCitizensEconomyIntake = base->energy_intake_2 * energyEfficiencyCoefficient * economyAllocation - baseSquareEconomyIntake;
			double baseCitizensLabsIntake = base->energy_intake_2 * energyEfficiencyCoefficient * labsAllocation - baseSquareLabsIntake;
			double baseCitizensPsychIntake = base->energy_intake_2 * energyEfficiencyCoefficient * psychAllocation - baseSquarePsychIntake;
			
			double baseCitizensMineralIntake2 = baseCitizensMineralIntake * mineralMultiplier;
			double baseCitizensEconomyIntake2 = baseCitizensEconomyIntake * economyMultiplier;
			double baseCitizensLabsIntake2 = baseCitizensLabsIntake * labsMultiplier;
			double baseCitizensPsychIntake2 = baseCitizensPsychIntake * psychMultiplier;
			
			double baseCitizensResourceIncome = getResourceScore(baseCitizensMineralIntake2, baseCitizensEconomyIntake2 + baseCitizensLabsIntake2);
			
			citizenCount += base->pop_size;
			citizenMineralIntakeSum += baseCitizensMineralIntake;
			citizenEconomyIntakeSum += baseCitizensEconomyIntake;
			citizenLabsIntakeSum += baseCitizensLabsIntake;
			citizenPsychIntakeSum += baseCitizensPsychIntake;
			citizenMineralIntake2Sum += baseCitizensMineralIntake2;
			citizenEconomyIntake2Sum += baseCitizensEconomyIntake2;
			citizenLabsIntake2Sum += baseCitizensLabsIntake2;
			citizenPsychIntake2Sum += baseCitizensPsychIntake2;
			citizenResourceIncomeSum += baseCitizensResourceIncome;
			
		}
		
		aiData.averageCitizenMineralIntake = citizenCount == 0 ? 0.0 : citizenMineralIntakeSum / (double)citizenCount;
		aiData.averageCitizenEconomyIntake = citizenCount == 0 ? 0.0 : citizenEconomyIntakeSum / (double)citizenCount;
		aiData.averageCitizenLabsIntake = citizenCount == 0 ? 0.0 : citizenLabsIntakeSum / (double)citizenCount;
		aiData.averageCitizenPsychIntake = citizenCount == 0 ? 0.0 : citizenPsychIntakeSum / (double)citizenCount;
		aiData.averageCitizenMineralIntake2 = citizenCount == 0 ? 0.0 : citizenMineralIntake2Sum / (double)citizenCount;
		aiData.averageCitizenEconomyIntake2 = citizenCount == 0 ? 0.0 : citizenEconomyIntake2Sum / (double)citizenCount;
		aiData.averageCitizenLabsIntake2 = citizenCount == 0 ? 0.0 : citizenLabsIntake2Sum / (double)citizenCount;
		aiData.averageCitizenPsychIntake2 = citizenCount == 0 ? 0.0 : citizenPsychIntake2Sum / (double)citizenCount;
		aiData.averageCitizenResourceIncome = citizenCount == 0 ? 0.0 : citizenResourceIncomeSum / (double)citizenCount;
		
	}
	
	// averageWorkerNutrientIntake
	
	{
		int workerCount = 0;
		double workerNutrientIntake2Sum = 0.0;
		
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = getBase(baseId);
			
			// faction base
			
			if (base->faction_id != aiFactionId)
				continue;
			
			for(MAP *tile : getBaseWorkedTiles(baseId))
			{
				int nutrientYield = mod_crop_yield(aiFactionId, baseId, getX(tile), getY(tile), 0);
				double nutrientIntake2 = (double)(nutrientYield - Rules->nutrient_intake_req_citizen);
				
				workerCount++;
				workerNutrientIntake2Sum += nutrientIntake2;
				
			}
			
		}
		
		aiData.averageWorkerNutrientIntake2 = workerCount == 0 ? 0.0 : workerNutrientIntake2Sum / (double)workerCount;
		
	}
	
	// averageWorkerResourceIncome
	
	{
		int workerCount = 0;
		double workerMineralIntake2Sum = 0.0;
		double workerEnergyIntake2Sum = 0.0;
		
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = getBase(baseId);
			
			double mineralCoefficient = getBaseMineralMultiplier(baseId);
			double energyCoefficient = getBaseEnergyEfficiencyCoefficient(baseId) * getBaseEnergyMultiplier(baseId);
			
			// faction base
			
			if (base->faction_id != aiFactionId)
				continue;
			
			for(MAP *tile : getBaseWorkedTiles(baseId))
			{
				int mineralYield = mod_mine_yield(aiFactionId, baseId, getX(tile), getY(tile), 0);
				int energyYield = mod_energy_yield(aiFactionId, baseId, getX(tile), getY(tile), 0);
				
				double mineralIntake2 = mineralCoefficient * (double)mineralYield;
				double energyIntake2 = energyCoefficient * (double)energyYield;
				
				workerCount++;
				workerMineralIntake2Sum += mineralIntake2;
				workerEnergyIntake2Sum += energyIntake2;
				
			}
			
		}
		
		aiData.averageWorkerMineralIntake2 = workerCount == 0 ? 0.0 : workerMineralIntake2Sum / (double)workerCount;
		aiData.averageWorkerEnergyIntake2 = workerCount == 0 ? 0.0 : workerEnergyIntake2Sum / (double)workerCount;
		aiData.averageWorkerResourceIncome = getResourceScore(aiData.averageWorkerMineralIntake2, aiData.averageWorkerEnergyIntake2);
		
	}
	
	Profiling::stop("populatePlayerGlobalVariables");
	
}

/*
Populates faction data dependent on geography data.
*/
void populatePlayerFactionIncome()
{
	Profiling::start("populatePlayerFactionIncome", "populateAIData");
	
	aiData.netIncome = getFactionNetIncome(aiFactionId);
	aiData.grossIncome = getFactionGrossIncome(aiFactionId);
	
	Profiling::stop("populatePlayerFactionIncome");
	
}

void populateUnits()
{
	Profiling::start("populateUnits", "populateAIData");
	
	debug("populateUnits - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	aiData.colonyUnitIds.clear();
	aiData.formerUnitIds.clear();
	aiData.landColonyUnitIds.clear();
	aiData.seaColonyUnitIds.clear();
	aiData.airColonyUnitIds.clear();
	aiData.combatUnitIds.clear();
	aiData.landCombatUnitIds.clear();
	aiData.landAndAirCombatUnitIds.clear();
	aiData.seaCombatUnitIds.clear();
	aiData.seaAndAirCombatUnitIds.clear();
	aiData.airCombatUnitIds.clear();
	
    for (int unitId : aiFactionInfo->availableUnitIds)
	{
		UNIT *unit = &(Units[unitId]);
		int unitOffenseValue = getUnitOffenseValue(unitId);
		int unitDefenseValue = getUnitDefenseValue(unitId);
		
		debug("\t[%3d] %-32s\n", unitId, unit->name);
		
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
		
		// add former units
		
		if (isFormerUnit(unitId))
		{
			debug("\t\tformer\n");
			
			aiData.formerUnitIds.push_back(unitId);
			
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
				(isFactionHasAbility(aiFactionId, ABL_ID_TRANCE) ? isUnitHasAbility(unitId, ABL_TRANCE) : unitOffenseValue == 1 && unitDefenseValue == 1)
				||
				(isFactionHasAbility(aiFactionId, ABL_ID_EMPATH) ? isUnitHasAbility(unitId, ABL_EMPATH) : unitOffenseValue == 1 && unitDefenseValue == 1)
				||
				(unitOffenseValue >= aiData.factionInfos[aiFactionId].maxConOffenseValue)
				||
				(unitDefenseValue >= aiData.factionInfos[aiFactionId].maxConDefenseValue)
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
	
	Profiling::stop("populateUnits");
	
}

void populateBasePoliceData()
{
	Profiling::start("populateBasePoliceData", "populateAIData");
	
	debug("populateBasePoliceData - %s\n", aiMFaction->noun_faction);
	
	for (int baseId : aiData.baseIds)
	{
		BasePoliceData &basePoliceData = aiData.getBaseInfo(baseId).policeData;
		
		debug("\t%-25s\n", getBase(baseId)->name);
		
		basePoliceData.allowedUnitCount = getBasePoliceAllowed(baseId);
		std::fill(basePoliceData.providedUnitCounts.begin(), basePoliceData.providedUnitCounts.end(), 0);
		basePoliceData.requiredPower = getBasePoliceRequiredPower(baseId);
		basePoliceData.providedPower = 0;
		
		double extraWorkerGain = getBaseExtraWorkerGain(baseId);
		for (int i = 0; i < 2; i++)
		{
			basePoliceData.policePowers.at(i) = getBasePolicePower(baseId, i);
			basePoliceData.policeGains.at(i) = std::min(basePoliceData.requiredPower, basePoliceData.policePowers.at(i)) * extraWorkerGain;
		}
		
//		debug
//		(
//			"\t\tallowedUnitCount=%d"
//			" providedUnitCounts={%d,%d}"
//			" requiredPower=%d"
//			" providedPower=%d"
//			" policePowers={%d,%d}"
//			" policeGains={%5.2f,%5.2f}"
//			"\n"
//			, basePoliceData.allowedUnitCount
//			, basePoliceData.providedUnitCounts.at(0), basePoliceData.providedUnitCounts.at(1)
//			, basePoliceData.requiredPower
//			, basePoliceData.providedPower
//			, basePoliceData.policePowers.at(0), basePoliceData.policePowers.at(1)
//			, basePoliceData.policeGains.at(0), basePoliceData.policeGains.at(1)
//		);
		
	}
	
	Profiling::stop("populateBasePoliceData");
	
}

void populateMaxMineralSurplus()
{
	Profiling::start("populateMaxMineralSurplus", "populateAIData");
	
	aiData.totalMineralIntake2 = 0;
	aiData.maxMineralSurplus = 1;
	aiData.maxMineralIntake2 = 1;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		aiData.totalMineralIntake2 += base->mineral_intake_2;
		
		aiData.maxMineralSurplus = std::max(aiData.maxMineralSurplus, base->mineral_surplus);
		aiData.maxMineralIntake2 = std::max(aiData.maxMineralSurplus, base->mineral_intake_2);
		
	}
	
	Profiling::stop("populateMaxMineralSurplus");
	
}

void populateVehicles()
{
	Profiling::start("populateVehicles", "populateAIData");
	
	debug("populate vehicles - %s\n", MFactions[aiFactionId].noun_faction);
	
	// assign pad0 values and populate vehicle pad0 map
	
	populateVehiclePad0Map(true);
	
	// process vehicles
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int vehicleLandTransportedCluster = getLandTransportedCluster(vehicleTile);
		int vehicleSeaCluster = getSeaCluster(vehicleTile);
		
		// further process only own vehicles
		
		if (vehicle->faction_id != aiFactionId)
			continue;
		
		debug("\t[%4d] %s region = %3d\n", vehicleId, getLocationString(vehicleTile), vehicleTile->region);
		
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
			
			// scout and native
			
			if (isScoutVehicle(vehicleId) || isNativeVehicle(vehicleId))
			{
				aiData.scoutVehicleIds.push_back(vehicleId);
			}
			
			// add surface vehicle to region list
			// except land unit in ocean
			
			if (vehicle->triad() != TRIAD_AIR && !(vehicle->triad() == TRIAD_LAND && is_ocean(vehicleTile)))
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
				aiData.landFormerVehicleIds[vehicleLandTransportedCluster].push_back(vehicleId);
				break;
			case TRIAD_SEA:
				aiData.seaFormerVehicleIds[vehicleSeaCluster].push_back(vehicleId);
				break;
			}
		}
		else if (isSeaTransportVehicle(vehicleId))
		{
			int vehicleSeaCluster = getSeaCluster(vehicleTile);
			aiData.seaTransportVehicleIds[vehicleSeaCluster].push_back(vehicleId);
		}
		
	}
	
	// psi unit proportion
	
	int psiCounts[2][3] = {};
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH &vehicle = Vehs[vehicleId];
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		int sideIndex;
		if (vehicle.faction_id == aiFactionId)
		{
			sideIndex = 0;
		}
		else if (isHostile(aiFactionId, vehicle.faction_id))
		{
			sideIndex = 1;
		}
		else
		{
			continue;
		}
		
		psiCounts[sideIndex][0]++;
		
		if (vehicle.offense_value() < 0)
		{
			psiCounts[sideIndex][1]++;
		}
		
		if (vehicle.defense_value() < 0)
		{
			psiCounts[sideIndex][2]++;
		}
		
	}
	
	if (psiCounts[0][0] > 0)
	{
		aiData.playerPsiOffenseProportion = psiCounts[0][1] / psiCounts[0][0];
		aiData.playerPsiDefenseProportion = psiCounts[0][2] / psiCounts[0][0];
	}
	else
	{
		aiData.playerPsiOffenseProportion = 0.5;
		aiData.playerPsiDefenseProportion = 0.5;
	}
	
	if (psiCounts[1][0] > 0)
	{
		aiData.enemyPsiOffenseProportion = psiCounts[1][1] / psiCounts[1][0];
		aiData.enemyPsiDefenseProportion = psiCounts[1][2] / psiCounts[1][0];
	}
	else
	{
		aiData.enemyPsiOffenseProportion = 0.5;
		aiData.enemyPsiDefenseProportion = 0.5;
	}
	
	Profiling::stop("populateVehicles");
	
}

void populateDangerZones()
{
	Profiling::start("populateDangerZones", "populateAIData");
	
	debug("populateDangerZones - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// unfriendly (artifact and colony)
	
	std::vector<int> unfriendlyAffectedVehicleIds;
	
	for (int vehicleId : aiData.vehicleIds)
	{
		// artifact or colony
		
		if (!(isArtifactVehicle(vehicleId) || isColonyVehicle(vehicleId)))
			continue;
		
		// not on transport
		
		if (isVehicleOnTransport(vehicleId))
			continue;
		
		// collect vehicle
		
		unfriendlyAffectedVehicleIds.push_back(vehicleId);
		
	}
	
	// reaching unfriendly vehicles
	
	robin_hood::unordered_flat_set<int> reachingUnfriendlyVehicleIds;
	aiData.unfriendlyEndangeredVehicleIds.clear();
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH &vehicle = Vehs[vehicleId];
		UNIT &unit = Units[vehicle.unit_id];
		CChassis &chassis = Chassis[unit.chassis_id];
		int triad = chassis.triad;
		
		// not player
		
		if (vehicle.faction_id == aiFactionId)
			continue;
		
		// unfriendly
		
		if (!isUnfriendly(aiFactionId, vehicle.faction_id))
			continue;
		
		// combat melee
		
		if (!isMeleeVehicle(vehicleId))
			continue;
		
		// triad
		
		switch (triad)
		{
		case TRIAD_AIR:
			{
				if (chassis.range > 0 && vehicle.movement_turns + 1 >= chassis.range)
				{
					// too long out of base
					continue;
				}
			}
			break;
			
		case TRIAD_SEA:
			{
			}
			break;
			
		case TRIAD_LAND:
			{
				// not on transport
				
				if (isVehicleOnTransport(vehicleId))
					continue;
				
			}
			break;
			
		}
		
		// verify reach intersection
		
		int vehicleMaxRange = (triad == TRIAD_LAND ? getVehicleMaxMoves(vehicleId) : getVehicleSpeed(vehicleId));
		
		for (int unfriendlyAffectedVehicleId : unfriendlyAffectedVehicleIds)
		{
			VEH &unfriendlyAffectedVehicle = Vehs[unfriendlyAffectedVehicleId];
			int unfriendlyAffectedVehicleTriad = unfriendlyAffectedVehicle.triad();
			
			// able to attack
			
			if ((unfriendlyAffectedVehicleTriad == TRIAD_LAND && triad == TRIAD_SEA) || (unfriendlyAffectedVehicleTriad == TRIAD_SEA && triad == TRIAD_LAND))
				continue;
			
			// range
			
			int unfriendlyAffectedVehicleMaxRange = (unfriendlyAffectedVehicleTriad == TRIAD_LAND ? getVehicleMaxMoves(unfriendlyAffectedVehicleId) : getVehicleSpeed(unfriendlyAffectedVehicleId));
			int combinedMaxRange = unfriendlyAffectedVehicleMaxRange + vehicleMaxRange;
			
			int range = map_range(vehicle.x, vehicle.y, unfriendlyAffectedVehicle.x, unfriendlyAffectedVehicle.y);
			
			if (range <= combinedMaxRange)
			{
				reachingUnfriendlyVehicleIds.insert(vehicleId);
				aiData.unfriendlyEndangeredVehicleIds.insert(unfriendlyAffectedVehicleId);
			}
			
		}
		
	}
	
	// generate unfriendly danger zones
	
	for (int vehicleId : reachingUnfriendlyVehicleIds)
	{
		for (MoveAction const &moveAction : getVehicleMoveActions(vehicleId, false))
		{
			TileInfo &tileInfo = aiData.getTileInfo(moveAction.destination);
			tileInfo.unfriendlyDangerZone = true;
		}
		
	}
	
	// hostile (other types)
	
	std::vector<int> hostileAffectedVehicleIds;
	
	for (int vehicleId : aiData.vehicleIds)
	{
		// not artifact or colony
		
		if ((isArtifactVehicle(vehicleId) || isColonyVehicle(vehicleId)))
			continue;
		
		// not on transport
		
		if (isVehicleOnTransport(vehicleId))
			continue;
		
		// collect vehicle
		
		hostileAffectedVehicleIds.push_back(vehicleId);
		
	}
	
	// reaching hostile vehicles
	
	robin_hood::unordered_flat_set<int> reachingHostileVehicleIds;
	aiData.hostileEndangeredVehicleIds.clear();
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH &vehicle = Vehs[vehicleId];
		UNIT &unit = Units[vehicle.unit_id];
		CChassis &chassis = Chassis[unit.chassis_id];
		int triad = chassis.triad;
		
		// not this AI faction
		
		if (vehicle.faction_id == aiFactionId)
			continue;
		
		// not alien
		// alien should not scare combat unit moving next to them
		
		if (vehicle.faction_id == 0)
			continue;
		
		// hostile
		
		if (!isHostile(aiFactionId, vehicle.faction_id))
			continue;
		
		// combat melee
		
		if (!isMeleeVehicle(vehicleId))
			continue;
		
		// triad
		
		switch (triad)
		{
		case TRIAD_AIR:
			{
				if (chassis.range > 0 && vehicle.movement_turns + 1 >= chassis.range)
				{
					// too long out of base
					continue;
				}
			}
			break;
			
		case TRIAD_SEA:
			{
			}
			break;
			
		case TRIAD_LAND:
			{
				// not on transport
				
				if (isVehicleOnTransport(vehicleId))
					continue;
				
			}
			break;
			
		}
		
		// verify reach intersection
		
		int vehicleMaxRange = (triad == TRIAD_LAND ? getVehicleMaxMoves(vehicleId) : getVehicleSpeed(vehicleId));
		
		for (int hostileAffectedVehicleId : hostileAffectedVehicleIds)
		{
			VEH &hostileAffectedVehicle = Vehs[hostileAffectedVehicleId];
			int hostileAffectedVehicleTriad = hostileAffectedVehicle.triad();
			
			// able to attack
			
			if ((hostileAffectedVehicleTriad == TRIAD_LAND && triad == TRIAD_SEA) || (hostileAffectedVehicleTriad == TRIAD_SEA && triad == TRIAD_LAND))
				continue;
			
			// range
			
			int hostileAffectedVehicleMaxRange = (hostileAffectedVehicleTriad == TRIAD_LAND ? getVehicleMaxMoves(hostileAffectedVehicleId) : getVehicleSpeed(hostileAffectedVehicleId));
			int combinedMaxRange = hostileAffectedVehicleMaxRange + vehicleMaxRange;
			
			int range = map_range(vehicle.x, vehicle.y, hostileAffectedVehicle.x, hostileAffectedVehicle.y);
			
			if (range <= combinedMaxRange)
			{
				reachingHostileVehicleIds.insert(vehicleId);
				aiData.hostileEndangeredVehicleIds.insert(hostileAffectedVehicleId);
			}
			
		}
		
	}
	
	// generate hostile danger zones
	
	for (int vehicleId : reachingHostileVehicleIds)
	{
		for (MoveAction const &moveAction : getVehicleMoveActions(vehicleId, true))
		{
			TileInfo &tileInfo = aiData.getTileInfo(moveAction.destination);
			tileInfo.hostileDangerZone = true;
		}
		
	}
	
	// artillery (combat types)
	
	std::vector<int> artilleryAffectedVehicleIds;
	
	for (int vehicleId : aiData.vehicleIds)
	{
		// combat
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		// not on transport
		
		if (isVehicleOnTransport(vehicleId))
			continue;
		
		// collect vehicle
		
		artilleryAffectedVehicleIds.push_back(vehicleId);
		
	}
	
	// reaching artillery vehicles
	
	robin_hood::unordered_flat_set<int> reachingArtilleryVehicleIds;
	aiData.artilleryEndangeredVehicleIds.clear();
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH &vehicle = Vehs[vehicleId];
		UNIT &unit = Units[vehicle.unit_id];
		CChassis &chassis = Chassis[unit.chassis_id];
		int triad = chassis.triad;
		
		// not player
		
		if (vehicle.faction_id == aiFactionId)
			continue;
		
		// hostile
		
		if (!isHostile(aiFactionId, vehicle.faction_id))
			continue;
		
		// combat artillery
		
		if (!isArtilleryVehicle(vehicleId))
			continue;
		
		// verify reach intersection
		
		int vehicleMaxRange = (triad == TRIAD_LAND ? getVehicleMaxMoves(vehicleId) : getVehicleSpeed(vehicleId));
		
		for (int artilleryAffectedVehicleId : artilleryAffectedVehicleIds)
		{
			VEH &artilleryAffectedVehicle = Vehs[artilleryAffectedVehicleId];
			int artilleryAffectedVehicleTriad = artilleryAffectedVehicle.triad();
			
			// able to attack
			
			if ((artilleryAffectedVehicleTriad == TRIAD_LAND && triad == TRIAD_SEA) || (artilleryAffectedVehicleTriad == TRIAD_SEA && triad == TRIAD_LAND))
				continue;
			
			// range
			
			int artilleryAffectedVehicleMaxRange = (artilleryAffectedVehicleTriad == TRIAD_LAND ? getVehicleMaxMoves(artilleryAffectedVehicleId) : getVehicleSpeed(artilleryAffectedVehicleId));
			int combinedMaxRange = artilleryAffectedVehicleMaxRange + vehicleMaxRange;
			
			int range = map_range(vehicle.x, vehicle.y, artilleryAffectedVehicle.x, artilleryAffectedVehicle.y);
			
			if (range <= combinedMaxRange)
			{
				reachingArtilleryVehicleIds.insert(vehicleId);
				aiData.artilleryEndangeredVehicleIds.insert(artilleryAffectedVehicleId);
			}
			
		}
		
	}
	
	// generate artillery danger zones
	
	for (int vehicleId : reachingArtilleryVehicleIds)
	{
		for (MoveAction const &moveAction : getVehicleMoveActions(vehicleId, true))
		{
			TileInfo &tileInfo = aiData.getTileInfo(moveAction.destination);
			tileInfo.artilleryDangerZone = true;
		}
		
	}
	
	// enemy attacks
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo *tileInfo = &aiData.tileInfos.at(tileIndex);
		tileInfo->potentialAttacks.clear();
	}
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// hostile
		
		if (!isHostile(aiFactionId, vehicle->faction_id))
			continue;
		
		if (isMeleeVehicle(vehicleId))
		{
			for (robin_hood::pair<MAP *, double> const &potentialMeleeAttackTarget : getPotentialMeleeAttackTargets(vehicleId))
			{
				MAP *tile = potentialMeleeAttackTarget.first;
				double hastyCoefficient = potentialMeleeAttackTarget.second;
				
				TileInfo &tileInfo = aiData.getTileInfo(tile);
				
				tileInfo.potentialAttacks.push_back({vehicle->pad_0, hastyCoefficient, EM_MELEE});
				
			}
			
		}
		
		if (isArtilleryVehicle(vehicleId))
		{
			for (robin_hood::pair<MAP *, double> const &potentialArtilleryAttackTarget : getPotentialArtilleryAttackTargets(vehicleId))
			{
				MAP *tile = potentialArtilleryAttackTarget.first;
				TileInfo &tileInfo = aiData.getTileInfo(tile);
				
				tileInfo.potentialAttacks.push_back({vehicle->pad_0, 1.0, EM_ARTILLERY});
				
			}
			
		}
		
	}
	
	Profiling::stop("populateDangerZones");
	
}

void populateEnemyStacks()
{
	Profiling::start("populateEnemyStacks", "populateAIData");
	
	debug("populateEnemyStacks - %s\n", MFactions[aiFactionId].noun_faction);
	
	// add enemy vehicles to stacks
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		TileInfo const &vehicleTileInfo = aiData.tileInfos.at(vehicleTile - *MapTiles);
		
		// ignore sea aliens
		
		if (vehicle->faction_id == 0 && is_ocean(vehicleTile))
			continue;
		
		// ignore land aliens too far from player bases or colonies (if there are no bases)
		
		if (vehicle->faction_id == 0 && !is_ocean(vehicleTile))
		{
			if (aiFaction->base_count > 0)
			{
				if (vehicleTileInfo.baseRanges.at(TRIAD_LAND) > 6)
					continue;
			}
			else
			{
				int colonyRange = INT_MAX;
				for (int colonyVehicleId : aiData.colonyVehicleIds)
				{
					MAP *colonyVehicleTile = getVehicleMapTile(colonyVehicleId);
					int range = getRange(vehicleTile, colonyVehicleTile);
					colonyRange = std::min(colonyRange, range);
				}
				if (colonyRange > 6)
					continue;
			}
		}
		
		// hostile
		
		if (!isHostile(aiFactionId, vehicle->faction_id))
			continue;
		
		// exclude land vehicle on transport
		
		if (isVehicleOnTransport(vehicleId))
			continue;
		
		// add enemy vehicle
		
		aiData.enemyStacks[vehicleTile].addVehicle(vehicleId);
		
	}
	
	std::vector<MAP *> emptyEnemyStacks;
	
	for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		MAP *enemyStackTile = enemyStackEntry.first;
		EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
		if (enemyStackInfo.vehiclePad0s.size() == 0)
		{
			emptyEnemyStacks.push_back(enemyStackTile);
		}
		
	}
	
	for (MAP *stackTile : emptyEnemyStacks)
	{
		aiData.enemyStacks.erase(stackTile);
	}
	
	// select enemy stacks by base range
	
	for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		MAP *enemyStackTile = enemyStackEntry.first;
		EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
		assert(isOnMap(enemyStackTile));
		
		debug("\t\t%s\n", getLocationString(enemyStackTile));
		
		// populate baseRange
		
		enemyStackInfo.baseRange = aiData.getTileInfo(enemyStackTile).nearestBaseRanges.at(TRIAD_AIR).value;
		
		// populate base/bunker/airbase
		
		enemyStackInfo.base = isBaseAt(enemyStackTile);
		enemyStackInfo.baseOrBunker = isBaseAt(enemyStackTile) || isBunkerAt(enemyStackTile);
		enemyStackInfo.airbase = isAirbaseAt(enemyStackTile);
		enemyStackInfo.baseId = base_at(getX(enemyStackTile), getY(enemyStackTile));
		
		// bombardmentDestructive
		
		enemyStackInfo.bombardmentDestructive = getMaxBombardmentDamage(TRIAD_LAND, enemyStackInfo.tile) == 1.0;
		
	}
	
	// track closest to our bases stacks only
	
	std::vector<IdIntValue> stackTileRanges;
	
	for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		MAP *enemyStackLocation = enemyStackEntry.first;
		EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
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
	
	// populate enemy stacks secondary values
	
	debug("\tpopulate enemy stacks secondary values\n");
	
	SummaryStatistics unitCostSummary;
	SummaryStatistics destructionGainSummary;
	for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
		debug("\t\t%s\n", getLocationString(enemyStackInfo.tile));
		
		// averageUnitCost
		
		unitCostSummary.clear();
		for (int vehiclePad0 : enemyStackInfo.vehiclePad0s)
		{
			VEH *vehicle = getVehicleByPad0(vehiclePad0);
			if (vehicle == nullptr)
				continue;
			
			int unitCost = vehicle->cost();
			unitCostSummary.add(unitCost);
			
		}
		
		enemyStackInfo.averageUnitCost = unitCostSummary.mean();
		
		debug
		(
			"\t\t\t"
			" averageUnitCost=%5.2f"
			"\n"
			, enemyStackInfo.averageUnitCost
		);
		
	}
	
	Profiling::stop("populateEnemyStacks");
	
}

/*
Populates completely empty enemy bases.
They are not part of enemy stacks. So need to be handled separately.
*/
void populateEmptyEnemyBaseTiles()
{
	Profiling::start("populateEmptyEnemyBaseTiles", "populateAIData");
	
	debug("populateEmptyEnemyBaseTiles - %s\n", MFactions[aiFactionId].noun_faction);
	
	aiData.emptyEnemyBaseIds.clear();
	aiData.emptyEnemyBaseTiles.clear();
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
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
		
		aiData.emptyEnemyBaseIds.push_back(baseId);
		aiData.emptyEnemyBaseTiles.insert(baseTile);
		
	}
	
//	if (DEBUG)
//	{
//		for (int emptyEnemyBaseId : aiData.emptyEnemyBaseIds)
//		{
//			BASE *emptyEnemyBase = getBase(emptyEnemyBaseId);
//			MAP *emptyEnemyBaseTile = getBaseMapTile(emptyEnemyBaseId);
//			
//			debug("\t%s %-25s\n", getLocationString(emptyEnemyBaseTile).c_str(), emptyEnemyBase->name);
//			
//		}
//		
//	}
	
	Profiling::stop("populateEmptyEnemyBaseTiles");
	
}

void populateCombatEffects()
{
	Profiling::start("populateCombatEffects", "populateAIData");
	
	debug("populateCombatEffects - %s\n", MFactions[aiFactionId].noun_faction);
	
	// reset combatEffects
	
	aiData.combatEffectTable.clear();
	
	// populate lists
	
	std::vector<int> &rivalFactionIds = aiData.rivalFactionIds;
	std::vector<int> &unfriendlyFactionIds = aiData.unfriendlyFactionIds;
	std::vector<int> &hostileFactionIds = aiData.hostileFactionIds;
	std::array<FactionInfo, MaxPlayerNum> &factionInfos = aiData.factionInfos;
	
	rivalFactionIds.clear();
	unfriendlyFactionIds.clear();
	hostileFactionIds.clear();
	for (FactionInfo &factionInfo : factionInfos)
	{
		factionInfo.combatUnitIds.clear();
		factionInfo.combatVehicleIds.clear();
	}
		
	// populate factions
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		// exclude player
		
		if (factionId == aiFactionId)
			continue;
		
		rivalFactionIds.push_back(factionId);
		
		// add unfriendly faction
		
		if (isUnfriendly(aiFactionId, factionId))
		{
			unfriendlyFactionIds.push_back(factionId);
		}
		
		// add hostile faction if hostile
		
		if (isHostile(aiFactionId, factionId))
		{
			hostileFactionIds.push_back(factionId);
		}
		
	}
		
	// populate active units
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		FactionInfo &factionInfo = factionInfos.at(factionId);
		
		for (int unitId : factionInfo.availableUnitIds)
		{
			// add to combat list
			
			if (isCombatUnit(unitId))
			{
				factionInfo.combatUnitIds.push_back(unitId);
			}
			
		}
		
	}
	
	// populate vehicles
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int factionId = vehicle->faction_id;
		
		// add to combat list
		
		if (isCombatVehicle(vehicleId))
		{
			factionInfos.at(factionId).combatVehicleIds.push_back(vehicleId);
		}
		
	}
	
	// do not compute combatEffects
	// they are computed on demand and cached
	
	Profiling::stop("populateCombatEffects");
	
}

void populateEnemyBaseInfos()
{
	Profiling::start("populateEnemyBaseInfos", "populateAIData");
	
	populateEnemyBaseCaptureGains();
	populateEnemyBaseProtectorWeights();
	
	Profiling::stop("populateEnemyBaseInfos");
	
}

void populateEnemyBaseCaptureGains()
{
	Profiling::start("populateEnemyBaseCaptureGains", "populateEnemyBaseInfos");
	
	debug("populateEnemyBaseCaptureGains - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		aiData.factionInfos.at(factionId).baseIds.clear();
	}
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		int baseFactionId = base->faction_id;
		BaseInfo &baseInfo = aiData.baseInfos.at(baseId);
		
		// exclude player faction
		
		if (baseFactionId == aiFactionId)
			continue;
		
		// add to faction bases
		
		aiData.factionInfos.at(baseFactionId).baseIds.push_back(baseId);
		
		// captureGain
		
		double baseRegularGain = BASE_CAPTURE_GAIN_COEFFICIENT * baseInfo.gain;
		
		double baseProjectGain = 0.0;
		
		for (int projectFacilityId : getBaseProjects(baseId))
		{
			CFacility *projectFacility = getFacility(projectFacilityId);
			
			double projectMineralCost = Rules->mineral_cost_multi * projectFacility->cost;
			double projectValue = PROJECT_VALUE_MULTIPLIER * projectMineralCost;
			double projectGain = getGainBonus(projectValue);
			
			baseProjectGain += projectGain;
			
		}
		
		double baseGain = baseRegularGain + baseProjectGain;
		
		baseInfo.captureGain = baseGain;
		
	}
	
//	if (DEBUG)
//	{
//		for (int baseId = 0; baseId < *BaseCount; baseId++)
//		{
//			BaseInfo const &baseInfo = aiData.getBaseInfo(baseId);
//			
//			debug("\t%-25s\n", getBase(baseId)->name);
//			
//			debug("\t\tcaptureGain=%7.2f\n", baseInfo.captureGain);
//			
//		}
//		
//	}
	
	Profiling::stop("populateEnemyBaseCaptureGains");
	
}

void populateEnemyBaseProtectorWeights()
{
	Profiling::start("populateEnemyBaseProtectorWeights", "populateEnemyBaseInfos");
	
	debug("populateEnemyBaseProtectorWeights - %s\n", MFactions[aiFactionId].noun_faction);
	
	// reset base values
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		baseInfo.protectorUnitWeights.clear();
	}
	
	// iterate protectors
	
	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		FactionInfo &factionInfo = aiData.factionInfos.at(factionId);
		
		// exclude player
		
		if (factionId == aiFactionId)
			continue;
		
		// faction vehicles
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			int chassisId = vehicle->chassis_type();
			int speed = getVehicleSpeed(vehicleId);
			TileInfo &vehicleTileInfo = aiData.getVehicleTileInfo(vehicleId);
			
			// this faction
			
			if (vehicle->faction_id != factionId)
				continue;
			
			// combat
			
			if (!isCombatVehicle(vehicleId))
				continue;
			
			// land infrantry defensive unit stationed at base
			
			int stationedBase = (isBaseDefenderVehicle(vehicleId) ? vehicleTileInfo.baseId : -1);
			
			// list base weights by distance
			
			std::vector<IdDoubleValue> baseWeights;
			double sumWeight = 0.0;
			
			for (int baseId : factionInfo.baseIds)
			{
				MAP *baseTile = getBaseMapTile(baseId);
				TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
				
				// skip not stationed base
				
				if (stationedBase != -1 && baseId != stationedBase)
					continue;
				
				// check reachable and set defenseRange
				
				double defenseRange;
				
				switch (chassisId)
				{
				case CHS_GRAVSHIP:
					
					// always reachable
					
					defenseRange = BASE_DEFENSE_RANGE_AIR;
					
					break;
					
				case CHS_NEEDLEJET:
				case CHS_COPTER:
				case CHS_MISSILE:
					
					// same airCluster
					
					if (!isSameEnemyAirCluster(factionId, chassisId, speed, vehicleTile, baseTile))
						continue;
					
					defenseRange = BASE_DEFENSE_RANGE_AIR;
					
					break;
					
				case CHS_FOIL:
				case CHS_CRUISER:
					
					// sea base
					
					if (!baseTileInfo.ocean)
						continue;
					
					// same seaCombatCluster
					
					if (!isSameEnemySeaCombatCluster(factionId, vehicleTile, baseTile))
						continue;
					
					defenseRange = BASE_DEFENSE_RANGE_SEA;
					
					break;
					
				case CHS_INFANTRY:
				case CHS_SPEEDER:
				case CHS_HOVERTANK:
					
					// exclude transported
					
					if (vehicleTileInfo.ocean && !vehicleTileInfo.base)
						continue;
					
					// either land base or sea base where vehicle is located
					
					if (!(!baseTileInfo.ocean || baseTile == vehicleTile))
						continue;
					
					// same landCombatCluster
					
					if (!isSameEnemyLandCombatCluster(factionId, vehicleTile, baseTile))
						continue;
					
					defenseRange = BASE_DEFENSE_RANGE_LAND;
					
					break;
					
				default:
					
					debug("ERROR: unknown chassis.");
					exit(1);
					
				}
				
				int range = getRange(vehicleTile, baseTile);
				double weight = 1.0 / (2.0 + (double)range / defenseRange);
				baseWeights.push_back({baseId, weight});
				
				sumWeight += weight;
				
			}
			
			for (IdDoubleValue &baseWeight : baseWeights)
			{
				int baseId = baseWeight.id;
				double weight = baseWeight.value;
				BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
				
				// normalize by weight
				
				double normalizedWeight = weight / sumWeight;
				
				// add weighted protector
				
				double vehicleStrengthMultiplier = getVehicleStrenghtMultiplier(vehicleId);
				double vehicleWeight = normalizedWeight * vehicleStrengthMultiplier;
				
				if (baseInfo.protectorUnitWeights.find(vehicle->faction_id) == baseInfo.protectorUnitWeights.end())
				{
					baseInfo.protectorUnitWeights.emplace(vehicle->faction_id, robin_hood::unordered_flat_map<int, double>());
				}
				if (baseInfo.protectorUnitWeights.at(vehicle->faction_id).find(vehicle->unit_id) == baseInfo.protectorUnitWeights.at(vehicle->faction_id).end())
				{
					baseInfo.protectorUnitWeights.at(vehicle->faction_id).emplace(vehicle->unit_id, 0.0);
				}
				baseInfo.protectorUnitWeights.at(vehicle->faction_id).at(vehicle->unit_id) += vehicleWeight;
				
			}
			
		}
		
//		if (DEBUG)
//		{
//			debug("\tfactionId=%d\n", factionId);
//			
//			for (int baseId : factionInfo.baseIds)
//			{
//				BaseInfo const &baseInfo = aiData.getBaseInfo(baseId);
//				
//				debug("\t\t%-25s\n", Bases[baseId].name);
//				
//				for (robin_hood::pair<int, robin_hood::unordered_flat_map<int, double>> const &protectorUnitWeightEntry : baseInfo.protectorUnitWeights)
//				{
//					int const factionId = protectorUnitWeightEntry.first;
//					robin_hood::unordered_flat_map<int, double> const &factionProtectorUnitWeights = protectorUnitWeightEntry.second;
//					
//					for (robin_hood::pair<int, double> const &factionProtectorUnitWeightEntry : factionProtectorUnitWeights)
//					{
//						int const unitId = factionProtectorUnitWeightEntry.first;
//						double const weight = factionProtectorUnitWeightEntry.second;
//						
//						debug("\t\t\t%-24s %-32s weight=%5.2f\n", MFactions[factionId].noun_faction, Units[unitId].name, weight);
//						
//					}
//					
//				}
//				
//			}
//			
//		}
		
	}
	
	Profiling::stop("populateEnemyBaseProtectorWeights");
	
}

void evaluateEnemyStacks()
{
	Profiling::start("evaluateEnemyStacks", "populateAIData");
	
	debug("evaluateEnemyStacks - %s\n", MFactions[aiFactionId].noun_faction);
	
	std::array<FactionInfo, MaxPlayerNum> &factionInfos = aiData.factionInfos;
	
	for (robin_hood::pair<MAP *, EnemyStackInfo> &stackInfoEntry : aiData.enemyStacks)
	{
		MAP *enemyStackTile = stackInfoEntry.first;
		EnemyStackInfo &enemyStackInfo = stackInfoEntry.second;
		
		assert(isOnMap(enemyStackTile));
		
		CombatData &combatData = enemyStackInfo.combatData;
		
		// initialize combatData
		
		combatData.initialize(enemyStackTile);
		
		// ignore stacks without vehicles
		
		if (enemyStackInfo.vehiclePad0s.empty())
			continue;
		
		debug("\t%s\n", getLocationString(enemyStackTile));
		
		// destructionGain
		
		Profiling::start("calculate destructionGain", "evaluateEnemyStacks");
		
		SummaryStatistics destructionGainSummary;
		destructionGainSummary.clear();
		for (int vehiclePad0 : enemyStackInfo.vehiclePad0s)
		{
			int vehicleId = getVehicleIdByPad0(vehiclePad0);
			if (!(vehicleId >= 0 && vehicleId < *VehCount))
				continue;
			
			double destructionGain = getVehicleDestructionGain(vehicleId);
			destructionGainSummary.add(destructionGain);
			
		}
		double averageDestructionGain = destructionGainSummary.mean();
		enemyStackInfo.destructionGain = averageDestructionGain;
		debug("\t\tdestructionGain=%5.2f\n", enemyStackInfo.destructionGain);
		
		Profiling::stop("calculate destructionGain");
		
		// superiorities
		
		Profiling::start("superiorities", "evaluateEnemyStacks");
		
		if (isBaseAt(enemyStackTile))
		{
			   enemyStackInfo.requiredSuperiority	= 1.0 + 1.0 * conf.ai_combat_advantage;
			   enemyStackInfo.desiredSuperiority	= 1.0 + 2.0 * conf.ai_combat_advantage;
		}
		// attacking aliens (except tower) does not require minimal superiority
		else if (enemyStackInfo.alien && !enemyStackInfo.alienFungalTower)
		{
			   enemyStackInfo.requiredSuperiority	= 0.0;
			   enemyStackInfo.desiredSuperiority	= 1.0 + 2.0 * conf.ai_combat_advantage;
		}
		else
		{
			   enemyStackInfo.requiredSuperiority	= 1.0 + 1.0 * conf.ai_combat_advantage;
			   enemyStackInfo.desiredSuperiority	= 1.0 + 2.0 * conf.ai_combat_advantage;
		}
		
		Profiling::stop("superiorities");
		
		// maxTotalBombardmentEffect
		
		Profiling::start("maxTotalBombardmentEffect", "evaluateEnemyStacks");
		
		debug("\t\tmaxBombardmentTotalEffect\n");
		
		double maxBombardmentDamage = getMaxBombardmentDamage(TRIAD_LAND, enemyStackTile);
		
		double maxBombardmentEffect = 0.0;
		
		for (int vehiclePad0 : enemyStackInfo.vehiclePad0s)
		{
			int vehicleId = getVehicleIdByPad0(vehiclePad0);
			if (!(vehicleId >= 0 && vehicleId < *VehCount))
				continue;
			
			VEH *vehicle = getVehicle(vehicleId);
			int triad = vehicle->triad();
			
			// not air in flight
			
			if (triad == TRIAD_AIR && !enemyStackInfo.airbase)
				continue;
			
			// not artillery
			
			if (isArtilleryVehicle(vehicleId))
				continue;
			
			// find remaining bombardment damage
			
			double vehicleRelativeDamage = getVehicleRelativeDamage(vehicleId);
			double vehicleMaxBombardmentDamage = maxBombardmentDamage - vehicleRelativeDamage;
			
			maxBombardmentEffect += vehicleMaxBombardmentDamage;
			
			debug
			(
				"\t\t\t(%3d,%3d) %-25s %-32s"
				" vehicleMaxBombardmentDamage=%5.2f"
				"\n"
				, vehicle->x, vehicle->y, getMFaction(vehicle->faction_id)->noun_faction, vehicle->name()
				, vehicleMaxBombardmentDamage
			);
			
		}
		
		enemyStackInfo.maxBombardmentEffect = maxBombardmentEffect;
		
		debug
		(
			"\t\trequiredSuperiority=%5.2f desiredSuperiority=%5.2f"
			" maxBombardmentEffect=%5.2f"
			"\n"
			, enemyStackInfo.requiredSuperiority, enemyStackInfo.desiredSuperiority
			, enemyStackInfo.maxBombardmentEffect
		);
		
		Profiling::stop("maxTotalBombardmentEffect");
		
		// calculate unit offense effects
		
		Profiling::start("calculate unit offense effects", "evaluateEnemyStacks");
		
		debug("\t\tunit offense effects\n");
		
		for (int ownUnitId : factionInfos.at(aiFactionId).combatUnitIds)
		{
			debug("\t\t\t[%3d] %-32s\n", ownUnitId, getUnit(ownUnitId)->name);
			
			// melee
			
			if (enemyStackInfo.isUnitCanMeleeAttackStack(ownUnitId))
			{
				int battleCount = 0;
				double totalLoss = 0.0;
				
				for (int foeVehiclePad0 : enemyStackInfo.vehiclePad0s)
				{
					int foeVehicleId = getVehicleIdByPad0(foeVehiclePad0);
					if (!(foeVehicleId >= 0 && foeVehicleId < *VehCount))
						continue;
					
					VEH *foeVehicle = getVehicle(foeVehicleId);
					
					double effect = combatData.getUnitCombatEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, EM_MELEE, false, true);
					if (effect <= 0.0)
						continue;
					
					// bonuses and multipliers
					
					double offenseStrengthMultiplier = getUnitMeleeOffenseStrengthMultipler(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, enemyStackTile, true);
					double foeVehicleStrengthMultiplier = getVehicleStrenghtMultiplier(foeVehicleId);
					
					effect *= (offenseStrengthMultiplier / foeVehicleStrengthMultiplier);
					
					// accumulate values
					
					battleCount++;
					
					double loss = 1.0 / effect;
					totalLoss += loss;
					
				}
				
				double averageLoss = (battleCount <= 0 ? 0.0 : totalLoss / (double)battleCount);
				double averageEffect = (averageLoss <= 0.0 ? 0.0 : 1.0 / averageLoss);
				debug("\t\t\t\t%-15s %5.2f\n", "melee", averageEffect);
				
				enemyStackInfo.setUnitOffenseEffect(ownUnitId, CM_MELEE, averageEffect);
				
			}
			
			// artillery duel
			
			if (isArtilleryUnit(ownUnitId) && enemyStackInfo.artillery)
			{
				int battleCount = 0;
				double totalLoss = 0.0;
				
				for (int foeVehiclePad0 : enemyStackInfo.artilleryVehiclePad0s)
				{
					int foeVehicleId = getVehicleIdByPad0(foeVehiclePad0);
					if (!(foeVehicleId >= 0 && foeVehicleId < *VehCount))
						continue;
					
					VEH *foeVehicle = getVehicle(foeVehicleId);
					
					double effect = combatData.getUnitCombatEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, EM_ARTILLERY, false, true);
					if (effect <= 0.0)
						continue;
					
					// bonuses and multipliers
					
					double offenseStrengthMultiplier = getUnitArtilleryOffenseStrengthMultipler(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, enemyStackTile, true);
					double foeVehicleStrengthMultiplier = getVehicleStrenghtMultiplier(foeVehicleId);
					
					effect *= (offenseStrengthMultiplier / foeVehicleStrengthMultiplier);
					
					// accumulate values
					
					battleCount++;
					
					double loss = 1.0 / effect;
					totalLoss += loss;
					
				}
				
				double averageLoss = (battleCount <= 0 ? 0.0 : totalLoss / (double)battleCount);
				double averageEffect = (averageLoss <= 0.0 ? 0.0 : 1.0 / averageLoss);
				debug("\t\t\t\t%-15s %5.2f\n", "artilleryDuel", averageEffect);
				
				enemyStackInfo.setUnitOffenseEffect(ownUnitId, CM_ARTILLERY_DUEL, averageEffect);
				
			}
			
			// bombardment
			
			if (isArtilleryUnit(ownUnitId) && !enemyStackInfo.artillery && enemyStackInfo.bombardment)
			{
				double totalEffect = 0.0;
				
				for (int foeVehiclePad0 : enemyStackInfo.nonArtilleryVehiclePad0s)
				{
					int foeVehicleId = getVehicleIdByPad0(foeVehiclePad0);
					if (!(foeVehicleId >= 0 && foeVehicleId < *VehCount))
						continue;
					
					VEH *foeVehicle = getVehicle(foeVehicleId);
					
					double effect = combatData.getUnitCombatEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, EM_ARTILLERY, false, true);
					
					if (effect <= 0.0)
						continue;
					
					// bonuses and multipliers
					
					double offenseStrengthMultiplier = getUnitArtilleryOffenseStrengthMultipler(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, enemyStackTile, true);
					double foeVehicleStrengthMultiplier = getVehicleMoraleMultiplier(foeVehicleId);
					
					effect *= (offenseStrengthMultiplier / foeVehicleStrengthMultiplier);
					
					// accumulate values
					
					totalEffect += effect;
					
				}
				
				debug("\t\t\t\t%-15s %5.2f\n", "bombardment", totalEffect);
				
				enemyStackInfo.setUnitOffenseEffect(ownUnitId, CM_BOMBARDMENT, totalEffect);
				
			}
			
		}
		
		Profiling::stop("calculate unit offense effects");
		
		// populate protectors
		
		Profiling::start("populate protectors", "evaluateEnemyStacks");
		
		for (int vehiclePad0 : enemyStackInfo.vehiclePad0s)
		{
			int vehicleId = getVehicleIdByPad0(vehiclePad0);
			if (!(vehicleId >= 0 && vehicleId < *VehCount))
				continue;
			
			combatData.addProtector(vehicleId);
			
		}
		
		Profiling::stop("populate protectors");
		
	}
	
	Profiling::stop("evaluateEnemyStacks");
	
}

void evaluateBaseDefense()
{
	debug("evaluateBaseDefense - %s\n", MFactions[aiFactionId].noun_faction);
	
	// bases
	
	for (int baseId : aiData.baseIds)
	{
		MAP *tile = getBaseMapTile(baseId);
		CombatData &combatData = aiData.getBaseInfo(baseId).combatData;
		
		evaluateDefense(tile, combatData);
		
	}
	
	// bunkers
	
	for (robin_hood::pair<MAP *, BunkerInfo> &bunkerInfoEntry : aiData.bunkerInfos)
	{
		MAP *tile = bunkerInfoEntry.first;
		BunkerInfo &bunkerInfo = bunkerInfoEntry.second;
		
		CombatData &combatData = bunkerInfo.combatData;
		
		evaluateDefense(tile, combatData);
		
	}
	
}

void evaluateDefense(MAP *tile, CombatData &combatData)
{
	Profiling::start("evaluateBaseDefense", "populateAIData");
	
	std::array<FactionInfo, MaxPlayerNum> &factionInfos = aiData.factionInfos;
	std::vector<int> const &unfriendlyFactionIds = aiData.unfriendlyFactionIds;
	
	// initialize combat data
	
	combatData.initialize(tile);
	
	// evaluate base threat

	Profiling::start("evaluate base threat", "evaluateBaseDefense");
	
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	BASE *base = getBase(tileInfo.baseId);
	
	debug
	(
		"\t%s %-25s"
		"\n"
		, getLocationString(tile)
		, base == nullptr ? "--- bunker ---" : base->name
	);
	
	// calculate foe strength
	
	Profiling::start("calculate foe strength", "evaluate base threat");
	
	debug("\t\tbase foeMilitaryStrength\n");
	
	std::array<robin_hood::unordered_flat_map<int, double>, MaxPlayerNum> foeVehicleWeights;
	std::array<robin_hood::unordered_flat_map<int, double>, MaxPlayerNum> foeUnitWeights;
	
	for (int foeFactionId : unfriendlyFactionIds)
	{
		for (int vehicleId : factionInfos.at(foeFactionId).combatVehicleIds)
		{
			VEH &vehicle = Vehs[vehicleId];
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			UNIT &unit = Units[vehicle.unit_id];
			CChassis &chassis = Chassis[unit.chassis_id];
			int triad = chassis.triad;
			
			// combat
			
			if (!isCombatVehicle(vehicleId))
				continue;
			
			// ignore infantry defensive vehicle at base
			
			if (isInfantryDefensiveVehicle(vehicleId) && map_base(vehicleTile))
				continue;
			
			// reachable
			
			switch (triad)
			{
			case TRIAD_AIR:
				
				// same enemy air combat cluster for air vehicle
				
				if (!isSameEnemyAirCluster(vehicleId, tile))
					continue;
				
				break;
				
			case TRIAD_SEA:
				
				if (tileInfo.ocean)
				{
					// same enemy sea combat cluster for sea vehicle
					
					if (!isSameEnemySeaCombatCluster(vehicleId, tile))
						continue;
					
				}
				else
				{
					// sea vehicle cannot attack land base
					continue;
				}
				
				break;
				
			case TRIAD_LAND:
				
				if (tileInfo.ocean)
				{
					if (vehicle_has_ability(vehicleId, ABL_AMPHIBIOUS))
					{
						// same land combat cluster for amphibious land vehicle
						
						if (!isSameEnemyLandCombatCluster(vehicleId, tile))
							continue;
						
					}
					else
					{
						// non-amphibious land vehicle cannot attack sea base
						continue;
					}
				}
				else
				{
					// same land combat cluster for land vehicle
					
					if (!isSameEnemyLandCombatCluster(vehicleId, tile))
						continue;
					
				}
				
			}
			
			// threat coefficient
			
			double threatCoefficient = aiData.factionInfos[vehicle.faction_id].threatCoefficient;
			
			// approach time coefficient
			
			double approachTime = getVehicleApproachTime(vehicleId, tile);
			if (approachTime == INF)
				continue;
			
			double approachTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, approachTime);
			
			// weight
			
			double weight = threatCoefficient * approachTimeCoefficient;
			
			// store value
			
			foeVehicleWeights.at(vehicle.faction_id)[vehicleId] = weight;
			foeUnitWeights.at(vehicle.faction_id)[vehicle.unit_id] += weight;
			
			debug
			(
				"\t\t\t(%3d,%3d) %-32s"
				" weight=%5.2f"
				" approachTime=%7.2f approachTimeCoefficient=%5.2f"
				" threatCoefficient=%5.2f"
				"\n"
				, vehicle.x, vehicle.y, unit.name
				, weight
				, approachTime, approachTimeCoefficient
				, threatCoefficient
			);
			
		}
		
	}
	
	Profiling::stop("calculate foe strength");
	
//		if (DEBUG)
//		{
//			for (int foeFactionId : foeFactionIds)
//			{
//				for (int foeUnitId : foeCombatUnitIds.at(foeFactionId))
//				{
//					double weight = foeUnitWeightTable.get(foeFactionId, foeUnitId);
//					
//					if (weight == 0.0)
//						continue;
//					
//					debug
//					(
//						"\t\t\t%-24s %-32s weight=%5.2f\n",
//						MFactions[foeFactionId].noun_faction,
//						Units[foeUnitId].name,
//						weight
//					)
//					;
//					
//				}
//				
//			}
//			
//		}
	
	// --------------------------------------------------
	// calculate potential alien strength
	// --------------------------------------------------
	
	debug("\t\tfoe MilitaryStrength, potential alien\n");
	
	{
		int alienUnitId = tileInfo.ocean ? BSC_ISLE_OF_THE_DEEP : BSC_MIND_WORMS;
		
		// strength modifier
		
		double moraleMultiplier = getAlienMoraleMultiplier();
		double gameTurnCoefficient = getAlienTurnOffenseModifier();
		
		// set basic numbers to occasional vehicle
		
		double alienCount = 1.0;
		
		// add more numbers based on eco-damage and number of fungal pops
		
		if (base != nullptr && tileInfo.land)
		{
			alienCount += (((double)aiFaction->clean_minerals_modifier / 3.0) * ((double)base->eco_damage / 20.0));
		}
		
		double weight = moraleMultiplier * gameTurnCoefficient * alienCount;
		
		// reduce alien weight based on accepted risk of population reduction
		
		if (base != nullptr)
		{
			if (alienCount < (double)base->pop_size)
			{
				weight *= alienCount / (double)base->pop_size;
			}
		}
		else
		{
			// bunker does not care much about aliens
			weight *= 0.5;
		}
		
		// do not add potential weight more than actual one
		
		foeUnitWeights.at(0)[alienUnitId] += weight;
		
		debug
		(
			"\t\t\t%-32s weight=%5.2f, moraleMultiplier=%5.2f, gameTurnCoefficient=%5.2f, alienCount=%5.2f\n",
			Units[alienUnitId].name,
			weight, moraleMultiplier, gameTurnCoefficient, alienCount
		);
		
	}
	
	// --------------------------------------------------
	// calculate potential alien strength - end
	// --------------------------------------------------
	
	// summarize foe unit weights by faction
	
	Profiling::start("summarize foe unit weights by faction", "evaluate base threat");
	
	std::array<double, MaxPlayerNum> foeFactionWeights = {};
	
	for (int foeFactionId : unfriendlyFactionIds)
	{
		robin_hood::unordered_flat_map<int, double> &foeFactionUnitWeights = foeUnitWeights.at(foeFactionId);
		
		for (int foeUnitId : factionInfos.at(foeFactionId).combatUnitIds)
		{
			// exclude fungal tower
			
			if (foeFactionId == 0 && foeUnitId == BSC_FUNGAL_TOWER)
				continue;
			
			// not present in threat
			
			if (foeFactionUnitWeights.find(foeUnitId) == foeFactionUnitWeights.end())
				continue;
			
			// accumulate weight
			
			double weight = foeFactionUnitWeights.at(foeUnitId);
			foeFactionWeights.at(foeFactionId) += weight;
			
		}
		
	}
	
	// leave only strongest neutral faction
	
	int strongestNeutralFactionId = -1;
	double strongestNeutralFactionWeight = 0.0;
	
	for (int foeFactionId : unfriendlyFactionIds)
	{
		// neutral
		
		if (!isNeutral(aiFactionId, foeFactionId))
			continue;
		
		// update strongest faction
		
		double foeFactionWeight = foeFactionWeights.at(foeFactionId);
		
		if (foeFactionWeight > strongestNeutralFactionWeight)
		{
			strongestNeutralFactionId = foeFactionId;
			strongestNeutralFactionWeight = foeFactionWeight;
		}
		
	}
	
	for (int foeFactionId : unfriendlyFactionIds)
	{
		// neutral
		
		if (!isNeutral(aiFactionId, foeFactionId))
			continue;
		
		// exclude strongest
		
		if (foeFactionId == strongestNeutralFactionId)
			continue;
		
		// clear this faction unit weights
		
		foeVehicleWeights.at(foeFactionId).clear();
		foeUnitWeights.at(foeFactionId).clear();
		foeFactionWeights.at(foeFactionId) = 0.0;
		
	}
	
	// do not reduce threat based on enemy slowing down each other because that what mutual impediment does
	
	// reduce alien weight to be not additive to combined threat because they fight everybody
	
	double alienThreat = foeFactionWeights.at(0);
	
	if (alienThreat > 0.0)
	{
		double combinedThreat = 0.0;
		
		for (int foeFactionId : unfriendlyFactionIds)
		{
			// not alien
			
			if (foeFactionId == 0)
				continue;
			
			combinedThreat += foeFactionWeights.at(foeFactionId);
			
		}
		
		double alienCoefficient = combinedThreat <= 0.0 ? 1.0 : alienThreat < combinedThreat ? 0.0 : (alienThreat - combinedThreat) / alienThreat;
		
		
		robin_hood::unordered_flat_map<int, double> &foeFactionUnitWeights = foeUnitWeights.at(0);
		
		for (int foeUnitId : factionInfos.at(0).combatUnitIds)
		{
			// exclude fungal tower
			
			if (foeUnitId == BSC_FUNGAL_TOWER)
				continue;
			
			// not present in threat
			
			if (foeFactionUnitWeights.find(foeUnitId) == foeFactionUnitWeights.end())
				continue;
			
			// reduce weight
			
			foeFactionUnitWeights.at(foeUnitId) *= alienCoefficient;
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("\tfoeUnitWeights\n");
		
		for (int foeFactionId : unfriendlyFactionIds)
		{
			for (robin_hood::pair<int, double> &foeUnitWeightEntry : foeUnitWeights.at(foeFactionId))
			{
				int foeUnitId = foeUnitWeightEntry.first;
				double foeUnitWeight = foeUnitWeightEntry.second;
				
				if (foeUnitWeight == 0.0)
					continue;
				
				debug("\t\t[%1d] %-24s [%3d] %-32s %5.2f\n", foeFactionId, MFactions[foeFactionId].noun_faction, foeUnitId, Units[foeUnitId].name, foeUnitWeight);
				
			}
			
		}
		
	}
	
	Profiling::stop("summarize foe unit weights by faction");
	
	// populate assailants
	
	Profiling::start("populate assailants", "evaluate base threat");
	
	debug("\tassailants\n");
	for (int foeFactionId : unfriendlyFactionIds)
	{
		for (robin_hood::pair<int, double> const &foeVehicleWeightsEntry : foeVehicleWeights.at(foeFactionId))
		{
			int foeVehicleId = foeVehicleWeightsEntry.first;
			double foeVehicleWeight = foeVehicleWeightsEntry.second;
			
			if (foeVehicleWeight <= 0.0)
				continue;
			
			debug("\t\t%-24s %-32s\n", MFactions[foeFactionId].noun_faction, Vehs[foeVehicleId].name());
			combatData.addAssailant(foeVehicleId, foeVehicleWeight);
			
		}
		
		for (robin_hood::pair<int, double> &foeUnitWeightEntry : foeUnitWeights.at(foeFactionId))
		{
			int foeUnitId = foeUnitWeightEntry.first;
			double foeUnitWeight = foeUnitWeightEntry.second;
			
			if (foeUnitWeight == 0.0)
				continue;
			
			debug("\t\t%-24s %-32s\n", MFactions[foeFactionId].noun_faction, Units[foeUnitId].name);
			combatData.addAssailantUnit(foeFactionId, foeUnitId, foeUnitWeight);
			
		}
		
	}
	
	Profiling::stop("populate assailants");
	
	Profiling::stop("evaluate base threat");
	
	Profiling::stop("evaluateBaseDefense");
	
}

void evaluateBaseProbeDefense()
{
	Profiling::start("evaluateBaseDefense", "populateAIData");
	
	debug("evaluateBaseProbeDefense - %s\n", MFactions[aiFactionId].noun_faction);
	
	// evaluate base threat
	
	Profiling::start("evaluate base threat", "evaluateBaseDefense");
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		bool baseTileOcean = is_ocean(baseTile);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		
		BaseProbeData &baseProbeData = baseInfo.probeData;
		baseProbeData.clear();
		
		debug
		(
			"\t%-25s"
			"\n"
			, base->name
		);
		
		// calculate foe strength
		
		Profiling::start("calculate foe strength", "evaluate base threat");
		
		debug("\t\tbase foeMilitaryStrength\n");
		
		double totalWeight = 0.0;
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			int chassisId = vehicle->chassis_type();
			UNIT *unit = &(Units[vehicle->unit_id]);
			CChassis *chassis = &(Chassis[unit->chassis_id]);
			int triad = chassis->triad;
			int speed = getVehicleSpeed(vehicleId);
			
			// other faction
			
			if (vehicle->faction_id == aiFactionId)
				continue;
			
			// probe
			
			if (!isProbeVehicle(vehicleId))
				continue;
			
			// ocean base
			if (baseTileOcean)
			{
				switch (triad)
				{
				case TRIAD_AIR:
					
					// same enemy air combat cluster for air vehicle
					
					if (!isSameEnemyAirCluster(vehicle->faction_id, chassisId, speed, vehicleTile, baseTile))
						continue;
					
					break;
					
				case TRIAD_SEA:
					
					// same enemy sea combat cluster for sea vehicle
					
					if (!isSameEnemySeaCombatCluster(vehicle->faction_id, vehicleTile, baseTile))
						continue;
					
					break;
					
				case TRIAD_LAND:
					
					if (vehicle_has_ability(vehicleId, ABL_AMPHIBIOUS))
					{
						// same land combat cluster for amphibious land vehicle
						
						if (!isSameEnemyLandCombatCluster(vehicle->faction_id, vehicleTile, baseTile))
							continue;
						
					}
					else
					{
						// non-amphibious land vehicle cannot attack ocean base
						
						continue;
						
					}
					
				}
				
			}
			// land base
			else
			{
				switch (triad)
				{
				case TRIAD_AIR:
					
					// same enemy air combat cluster for air vehicle
					
					if (!isSameEnemyAirCluster(vehicle->faction_id, chassisId, speed, vehicleTile, baseTile))
						continue;
					
					break;
					
				case TRIAD_SEA:
					
					// sea vehicle need to get next to coast base
					
					{
						bool accessible = false;
						for (MAP *adjacentTile : getAdjacentTiles(baseTile))
						{
							if (isSameEnemySeaCombatCluster(vehicle->faction_id, vehicleTile, adjacentTile))
							{
								accessible = true;
								break;
							}
						}
						
						if (!accessible)
							continue;
						
					}
					
					break;
					
				case TRIAD_LAND:
					
					// same land combat cluster for land vehicle
					
					if (!isSameEnemyLandCombatCluster(vehicle->faction_id, vehicleTile, baseTile))
						continue;
					
				}
				
			}
			
			// threat coefficient
			
			double threatCoefficient = aiData.factionInfos[vehicle->faction_id].threatCoefficient;
			
			// offense multiplier
			
			double offenseMultiplier = getVehicleStrenghtMultiplier(vehicleId);
			
			// defense multiplier
			
			double defenseMultiplier = 1.0 / (getBaseDefenseMultiplier(baseId, 4) * getSensorDefenseMultiplier(aiFactionId, baseTile));
			
			if (defenseMultiplier <= 0.0)
			{
				debug("ERROR: defenseMultiplier <= 0.0\n");
				continue;
			}
			
			// approach time coefficient
			
			double approachTime = getVehicleApproachTime(vehicleId, baseTile);
			if (approachTime == INF)
				continue;
			
			double approachTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, approachTime);
			
			// weight
			
			double weight = threatCoefficient * offenseMultiplier * defenseMultiplier * approachTimeCoefficient;
			totalWeight += weight;
			
			debug
			(
				"\t\t\t(%3d,%3d) %-32s"
				" weight=%5.2f"
				" offenseMultiplier=%5.2f"
				" defenseMultiplier=%5.2f"
				" approachTime=%7.2f approachTimeCoefficient=%5.2f"
				"\n"
				, vehicle->x, vehicle->y, Units[vehicle->unit_id].name
				, weight
				, offenseMultiplier
				, defenseMultiplier
				, approachTime, approachTimeCoefficient
			);
			
		}
		
		Profiling::stop("calculate foe strength");
		
		// compute required protection
		
		Profiling::start("compute required protection", "evaluate base threat");
		
		double requiredEffect = sqrt(totalWeight);
		baseInfo.probeData.requiredEffect = requiredEffect;
		
		debug
		(
			"\t\trequiredEffect=%5.2f"
			" conf.ai_combat_advantage=%5.2f"
			" totalWeight=%5.2f"
			"\n"
			, requiredEffect
			, conf.ai_combat_advantage
			, totalWeight
		);
		
		Profiling::stop("compute required protection");
		
	}
	
	Profiling::stop("evaluate base threat");
	
	Profiling::stop("evaluateBaseDefense");
	
}

/*
Designs units.
This function works before AI Data are populated and should not rely on them.
*/
void designUnits()
{
	Profiling::start("designUnits", "strategy");
	
	// get best values
	
	VehWeapon bestWeapon = getFactionBestWeapon(aiFactionId);
	VehArmor bestArmor = getFactionBestArmor(aiFactionId);
	VehArmor attackerArmor = getFactionBestArmor(aiFactionId, bestWeapon);
	VehWeapon defenderWeapon = getFactionBestWeapon(aiFactionId, (bestArmor + 1) / 2);
	VehChassis fastLandChassis = (has_chassis(aiFactionId, CHS_HOVERTANK) ? CHS_HOVERTANK : CHS_SPEEDER);
	VehChassis fastSeaChassis = (has_chassis(aiFactionId, CHS_CRUISER) ? CHS_CRUISER : CHS_FOIL);
	VehReactor bestReactor = best_reactor(aiFactionId);
	
	// fast colony
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastLandChassis, fastSeaChassis},
		{WPN_COLONY_MODULE},
		{ARM_NO_ARMOR},
		{
			{},
		},
		bestReactor,
		PLAN_COLONY,
		NULL
	);
	
	// protected former
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastLandChassis, fastSeaChassis, CHS_GRAVSHIP},
		{WPN_TERRAFORMING_UNIT},
		{bestArmor},
		{
			{ABL_ID_SUPER_TERRAFORMER, ABL_ID_CLEAN_REACTOR, ABL_ID_FUNGICIDAL, ABL_ID_TRANCE, },
		},
		bestReactor,
		PLAN_TERRAFORM,
		NULL
	);
	
	// protected crawler
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY, CHS_FOIL},
		{WPN_SUPPLY_TRANSPORT},
		{bestArmor},
		{
			{ABL_ID_CLEAN_REACTOR, ABL_ID_TRANCE, },
		},
		bestReactor,
		PLAN_SUPPLY,
		NULL
	);
	
	// police defender
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{defenderWeapon},
		{bestArmor},
		{
			{ABL_ID_POLICE_2X, ABL_ID_CLEAN_REACTOR, ABL_ID_AAA, ABL_ID_TRANCE, },
		},
		bestReactor,
		PLAN_DEFENSE,
		NULL
	);
	
	// regular defender
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{defenderWeapon},
		{bestArmor},
		{
			{ABL_ID_AAA, ABL_ID_CLEAN_REACTOR, },
			{ABL_ID_COMM_JAMMER, ABL_ID_CLEAN_REACTOR, },
			{ABL_ID_TRANCE, ABL_ID_CLEAN_REACTOR, },
		},
		bestReactor,
		PLAN_DEFENSE,
		NULL
	);
	
	// land armored infantry attackers
	
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
			{ABL_ID_EMPATH, },
			// next to shore sea base attacker
			{ABL_ID_AMPHIBIOUS, ABL_ID_BLINK_DISPLACER, },
			// fast and deadly in field
			{ABL_ID_SOPORIFIC_GAS, ABL_ID_ANTIGRAV_STRUTS, },
			// fast and deadly against bases
			{ABL_ID_BLINK_DISPLACER, ABL_ID_ANTIGRAV_STRUTS, },
			// anti air
			{ABL_ID_AIR_SUPERIORITY, ABL_ID_ANTIGRAV_STRUTS, },
			// anti anti fast
			{ABL_ID_DISSOCIATIVE_WAVE, ABL_ID_BLINK_DISPLACER, },
		},
		bestReactor,
		PLAN_OFFENSE,
		NULL
	);
	
	// land artillery
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastLandChassis},
		{bestWeapon},
		{bestArmor},
		{
			{ABL_ID_ARTILLERY, },
		},
		bestReactor,
		PLAN_OFFENSE,
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
			{ABL_ID_DROP_POD, ABL_ID_BLINK_DISPLACER, },
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
			// air defended
			{ABL_ID_AAA, ABL_ID_SOPORIFIC_GAS, },
			// anti air
			{ABL_ID_AIR_SUPERIORITY, ABL_ID_SOPORIFIC_GAS, },
			// anti base
			{ABL_ID_BLINK_DISPLACER, ABL_ID_SOPORIFIC_GAS, },
			// pirates
			{ABL_ID_MARINE_DETACHMENT, ABL_ID_SOPORIFIC_GAS, },
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
			{ABL_ID_HEAVY_TRANSPORT, ABL_ID_CLEAN_REACTOR, },
			// air protected
			{ABL_ID_HEAVY_TRANSPORT, ABL_ID_AAA, ABL_ID_CLEAN_REACTOR, },
		},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
	// defensive probe
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{CHS_INFANTRY},
		{WPN_PROBE_TEAM},
		{ARM_NO_ARMOR},
		{
			{},
		},
		bestReactor,
		PLAN_COMBAT,
		"Infantry Probe Team"
	);
	
	// armored land probe
	
	proposeMultiplePrototypes
	(
		aiFactionId,
		{fastLandChassis},
		{WPN_PROBE_TEAM},
		{bestArmor},
		{
			// air protected
			{ABL_ID_AAA, ABL_ID_ANTIGRAV_STRUTS, },
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
			// air protected
			{ABL_ID_AAA, ABL_ID_ANTIGRAV_STRUTS, },
		},
		bestReactor,
		PLAN_COMBAT,
		NULL
	);
	
	// --------------------------------------------------
	// reinstate units
	// --------------------------------------------------
	
	// find Trance Scout Patrol
	
	int tranceScoutPatrolUnitId = -1;
	
	for (int unitId : aiFactionInfo->availableUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int unitOffenseValue = getUnitOffenseValue(unitId);
		int unitDefenseValue = getUnitDefenseValue(unitId);
		
		if (unit->chassis_id == CHS_INFANTRY && unitOffenseValue == 1 && unitDefenseValue == 1 && isUnitHasAbility(unitId, ABL_ID_TRANCE))
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
	
	Profiling::stop("designUnits");
	
}

/*
Propose multiple prototype combinations.
*/
void proposeMultiplePrototypes(int factionId, std::vector<VehChassis> chassisIds, std::vector<VehWeapon> weaponIds, std::vector<VehArmor> armorIds, std::vector<std::vector<VehAbl>> potentialAbilityIdSets, VehReactor reactor, VehPlan plan, char const *name)
{
	size_t allowedAbilityCount = (has_tech(Rules->tech_preq_allow_2_spec_abil, factionId) ? 2U : 1U);
	
	for (VehChassis chassisId : chassisIds)
	{
		if (!has_chassis(factionId, chassisId))
			continue;
		
		for (VehWeapon weaponId : weaponIds)
		{
			if (!has_weapon(factionId, weaponId))
				continue;
			
			for (VehArmor armorId : armorIds)
			{
				if (!has_armor(factionId, armorId))
					continue;
				
				for (std::vector<VehAbl> potentialAbilityIdSet : potentialAbilityIdSets)
				{
					std::vector<VehAbl> abilityIdSets;
					
					for (VehAbl potentialAbilityId : potentialAbilityIdSet)
					{
						if (!has_tech(Ability[potentialAbilityId].preq_tech, factionId))
							continue;
						
						abilityIdSets.push_back(potentialAbilityId);
						
						if (abilityIdSets.size() >= allowedAbilityCount)
							break;
						
					}
					
					checkAndProposePrototype(factionId, chassisId, weaponId, armorId, abilityIdSets, reactor, plan, name);
					
				}

			}

		}

	}

}

/*
Verify proposed prototype is allowed and propose it if yes.
Verifies all technologies are available and abilities area allowed.
*/
void checkAndProposePrototype(int factionId, VehChassis chassisId, VehWeapon weaponId, VehArmor armorId, std::vector<VehAbl> abilityIdSet, VehReactor reactor, VehPlan plan, char const *name)
{
	size_t allowedAbilityCount = (has_tech(Rules->tech_preq_allow_2_spec_abil, factionId) ? 2U : 1U);
	
	// build abilities
	
	uint32_t abilities = 0;
	
	for (size_t i = 0; i < std::min(allowedAbilityCount, abilityIdSet.size()); i++)
	{
		VehAbl abilityId = abilityIdSet.at(i);
		abilities |= ((uint32_t)1 << abilityId);
	}
	
	// propose prototype
	
	char prototypeName[32];
	if (name != nullptr)
	{
		strcpy(prototypeName, name);
	}
	int unitId = mod_propose_proto(factionId, chassisId, weaponId, armorId, (VehAblFlag) abilities, reactor, plan, name == nullptr ? nullptr : prototypeName);
	
	debug("checkAndProposePrototype - %s\n", MFactions[aiFactionId].noun_faction);
	debug("\treactor=%d, chassisId=%d, weaponId=%d, armorId=%d, abilities=%s\n", reactor, chassisId, weaponId, armorId, getAbilitiesString(abilities).c_str());
	debug("\tunitId=%d\n", unitId);
	
}

/*
Updates AI precomputed data after vehicle kill.
*/
void vehicleKill(int vehicleId)
{
	VEH &vehicle = aiData.savedVehicles.at(vehicleId);
	int vehiclePad0 = vehicle.pad_0;
	MAP *vehicleTile = getMapTile(vehicle.x, vehicle.y);
	
	if (isHostile(aiFactionId, vehicle.faction_id))
	{
		// update location enemy stack
		
		if (aiData.hasEnemyStack(vehicleTile))
		{
			EnemyStackInfo &enemyStackInfo = aiData.getEnemyStackInfo(vehicleTile);
			
			// remove vehicle from stack
			
			enemyStackInfo.vehiclePad0s.erase(std::remove(enemyStackInfo.vehiclePad0s.begin(), enemyStackInfo.vehiclePad0s.end(), vehiclePad0), enemyStackInfo.vehiclePad0s.end());
			
			// remove stack if no more vehicles
			
			if (enemyStackInfo.vehiclePad0s.empty())
			{
				aiData.enemyStacks.erase(vehicleTile);
			}
			
		}
		
		// remove potential attack
		// TODO
		
	}
	
}

