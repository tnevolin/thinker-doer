#pragma GCC diagnostic ignored "-Wshadow"

#include "wtp_ai.h"
#include "main.h"
#include "engine.h"
#include "wtp_terranx.h"
#include "wtp_game.h"
#include "wtp_mod.h"
#include "wtp_aiData.h"
#include "wtp_aiHurry.h"
#include "wtp_aiMove.h"
#include "wtp_aiProduction.h"
#include "wtp_aiRoute.h"

// MoveAction vector
ArrayVector<MoveAction, MAX_REACHABLE_LOCATION_COUNT> REACHABLE_LOCATIONS;

// fast hexCost cache
robin_hood::unordered_flat_map<int, int> cachedHexCost;
int getCachedHexCost(MovementType movementType, int speed1, int tile1Index, int angle)
{
	int key =
		+ (int)movementType 
		+ speed1						* MOVEMENT_TYPE_COUNT
		+ angle							* MOVEMENT_TYPE_COUNT * 2
		+ tile1Index					* MOVEMENT_TYPE_COUNT * 2 * ANGLE_COUNT
	;
	
	robin_hood::unordered_flat_map<int, int>::iterator cachedHexCostIterator = cachedHexCost.find(key);
	return cachedHexCostIterator == cachedHexCost.end() ? -1 : cachedHexCostIterator->second;
	
}
void setCachedHexCost(MovementType movementType, int speed1, int tile1Index, int angle, int hexCost)
{
	int key =
		+ (int)movementType 
		+ speed1						* MOVEMENT_TYPE_COUNT
		+ angle							* MOVEMENT_TYPE_COUNT * 2
		+ tile1Index					* MOVEMENT_TYPE_COUNT * 2 * ANGLE_COUNT
	;
	
	robin_hood::unordered_flat_map<int, int>::iterator cachedHexCostIterator = cachedHexCost.find(key);
	if (cachedHexCostIterator == cachedHexCost.end())
	{
		cachedHexCost.emplace(key, hexCost);
	}
	else
	{
		cachedHexCostIterator->second = hexCost;
	}
	
}

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

void setPlayerFactionReferences(int factionId)
{
	aiFactionId = factionId;
	aiMFaction = getMFaction(aiFactionId);
	aiFaction = getFaction(aiFactionId);
	aiFactionInfo = &(aiData.factionInfos.at(aiFactionId));
}

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
void __cdecl modified_enemy_units_check(int factionId)
{
	debug("modified_enemy_units_check - %s\n", getMFaction(factionId)->noun_faction);
	
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
		
	}
	
	// execute original code
	
	debug("enemy_units_check - %s - end\n", getMFaction(factionId)->noun_faction);
	
	Profiling::start("~ enemy_units_check");
	enemy_units_check(factionId);
	Profiling::stop("~ enemy_units_check");
	
	debug("modified_enemy_units_check - %s - end\n", getMFaction(factionId)->noun_faction);
	
	// run WTP AI code for AI enabled factions
	
	if (isWtpEnabledFaction(factionId))
	{
		// exhaust all units movement points
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			uint8_t vehicleMoveRate = (uint8_t)getVehicleMoveRate(vehicleId);
			
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
	
//	// store base state
//	
//	BaseSnapshots::takeSnapshots();
//	
	// precompute baseCompute
	
	precomputeBaseComputeValues(aiFactionId);
	
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
	
//	// restore base state
//	
//	BaseSnapshots::restoreSnapshots();
//	
	// clear baseCompute
	
	clearBaseComputeValues();
	
	Profiling::stop("strategy");
	
}

void executeTasks()
{
	Profiling::start("executeTasks", "strategy");
	
	debug("Tasks - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (robin_hood::pair<int, Task> &taskEntry : aiData.tasks)
	{
		Task &task = taskEntry.second;
		int vehicleId = task.getVehicleId();
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// skip not fully automated human player units
		
		if (aiFactionId == *CurrentPlayerFaction && !(conf.manage_player_units && ((vehicle->state & VSTATE_ON_ALERT) != 0) && vehicle->movement_turns == 0))
			continue;

		// do not execute combat tasks immediatelly

		if (task.type == TT_MELEE_ATTACK || task.type == TT_LONG_RANGE_FIRE)
			continue;

		debug
		(
			"\t[%4d] %s->%s/%s taskType=%2d, %s\n",
			vehicleId,
			getLocationString({vehicle->x,vehicle->y}).c_str(),
			getLocationString(task.getDestination()).c_str(),
			getLocationString(task.attackTarget).c_str(),
			task.type,
			Units[vehicle->unit_id].name
		)
		;

		task.execute();

	}

	Profiling::stop("executeTasks");
	
}

void populateAIData()
{
	Profiling::start("populateAIData", "strategy");
	
	// clear fast cachedHexCost
	cachedHexCost.clear();

	
	// clear aiData
	
	Profiling::start("aiData.clear()", "populateAIData");
	aiData.clear();
	Profiling::stop("aiData.clear()");
	
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
	
	// player faction info
	
	populatePlayerGlobalVariables();
	populatePlayerBaseIds();
	populatePlayerBaseRanges();
	populateUnits();
	populateVehicles();
	populateEmptyEnemyBaseTiles();
	populateDangerZones();
	populatePlayerFactionIncome();
	populateMaxMineralSurplus();
	populateBasePoliceData();
	
	// other computations
	
	computeCombatEffects();
	
	populateEnemyBaseInfos();
	
	populateEnemyStacks();
	evaluateEnemyStacks();
	evaluateBaseDefense();
	evaluateBaseProbeDefense();
	
	Profiling::stop("populateAIData");
	
}

void populateTileInfos()
{
	Profiling::start("populateTileInfos", "populateAIData");
	
	debug("populateTileInfos - %s\n", aiMFaction->noun_faction);
	
	// ocean, surface type, coast
	
	Profiling::start("ocean, surface type, coast", "populateTileInfos");
	
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
			
			for (MAP *adjacentTile : getAdjacentTiles(tile))
			{
				if (is_ocean(adjacentTile))
				{
					tileInfo.coast = true;
					break;
				}
				
			}
			
		}
		
	}
	
	Profiling::stop("ocean, surface type, coast");
	
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
	
	aiData.bunkerCombatDatas.clear();
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		tileInfo.bunker = map_has_item(tile, BIT_BUNKER);
		tileInfo.airbase = map_has_item(tile, BIT_BASE_IN_TILE | BIT_AIRBASE);
		
		if (tileInfo.bunker && tile->owner == aiFactionId)
		{
			aiData.bunkerCombatDatas.emplace(tile, ProtectCombatData());
			if (conf.bunker_ignores_terrain)
			{
				aiData.bunkerCombatDatas.at(tile).tileDefenseMultipliers.at(TRIAD_LAND) = getPercentageBonusMultiplier(conf.bunker_bonus_surface);
				aiData.bunkerCombatDatas.at(tile).tileDefenseMultipliers.at(TRIAD_SEA) = getPercentageBonusMultiplier(conf.bunker_bonus_surface);
				aiData.bunkerCombatDatas.at(tile).tileDefenseMultipliers.at(TRIAD_AIR) = getPercentageBonusMultiplier(conf.bunker_bonus_aerial);
				aiData.bunkerCombatDatas.at(tile).tileDefenseMultipliers.at(3) = 1.0;
			}
			
		}
		
	}
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &Vehs[vehicleId];
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		if (vehicle->faction_id != aiFactionId)
			continue;
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		if (aiData.bunkerCombatDatas.find(vehicleTile) != aiData.bunkerCombatDatas.end())
		{
			aiData.bunkerCombatDatas.at(vehicleTile).garrison.push_back(vehicleId);
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
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		tileInfo.vehicleIds = getTileVehicleIds(tile);
		
		tileInfo.needlejetInFlight = false;
		
		if (!tileInfo.airbase)
		{
			for (int vehicleId : tileInfo.vehicleIds)
			{
				if (isNeedlejetVehicle(vehicleId))
				{
					tileInfo.needlejetInFlight = true;
					break;
				}
				
			}
			
		}
		
	}
	
	Profiling::stop("vehicles");
	
	// adjacent tiles
	
	Profiling::start("adjacent tiles", "populateTileInfos");
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		tileInfo.adjacents.clear();
		
		for (int angle = 0; angle < ANGLE_COUNT; angle++)
		{
			int adjacentTileIndex = getTileIndexByAngle(tileIndex, angle);
			
			if (adjacentTileIndex != -1)
			{
				TileInfo *adjacentTileInfo = &aiData.tileInfos.at(adjacentTileIndex);
				tileInfo.adjacents.push_back({angle, adjacentTileInfo});
			}
			
		}
		
	}
	
	Profiling::stop("adjacent tiles");
	
	// range tiles
	
	Profiling::start("range tiles", "populateTileInfos");
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		
		int x = getX(tileIndex);
		int y = getY(tileIndex);
		
		int minIndex = 0;
		int maxIndex = TABLE_square_block_radius[2];
		
		int count = 0;
		for (int index = minIndex; index < maxIndex; index++)
		{
			int offsetX = TABLE_square_offset_x[index];
			int offsetY = TABLE_square_offset_y[index];
			
			int cellX = wrap(x + offsetX);
			int cellY = y + offsetY;
			
			if (!isOnMap(cellX, cellY))
				continue;
			
			int rangeTileIndex = getMapTileIndex(cellX, cellY);
			TileInfo &rangeTileInfo = aiData.tileInfos.at(rangeTileIndex);
			
			tileInfo.range2TileInfosArray.at(count) = &rangeTileInfo;
			count++;
			
		}
		
		tileInfo.range2CenterTileInfos.set(&tileInfo.range2TileInfosArray, 0, count);
		tileInfo.range2NoCenterTileInfos.set(&tileInfo.range2TileInfosArray, 1, count);
		
	}
	
	Profiling::stop("range tiles");
	
	// hexCosts
	
	Profiling::start("hexCosts", "populateTileInfos");
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		int tileX = getX(tileIndex);
		int tileY = getY(tileIndex);
		TileInfo &tileInfo = aiData.getTileInfo(tileIndex);
		
		for (std::array<HexCost, ANGLE_COUNT> &movementTypeHexCosts : tileInfo.hexCosts)
		{
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				movementTypeHexCosts.at(angle) = {-1, -1};
			}
		}
		
		int approximateSeaFungusHexCostSum = 0;
		for (VehChassis const &vehChassis : {CHS_FOIL, CHS_CRUISER})
		{
			CChassis const &chassis = Chassis[vehChassis];
			int adjustedCost = std::min(3, (int)chassis.speed);
			approximateSeaFungusHexCostSum += Rules->move_rate_roads * adjustedCost * ((2 * chassis.speed) + adjustedCost - 1) / (2 * chassis.speed);
		}
		int approximateSeaFungusHexCost = approximateSeaFungusHexCostSum / 2;
		
		int approximateLandRoughHexCostSum = 0;
		for (VehChassis const &vehChassis : {CHS_FOIL, CHS_CRUISER})
		{
			CChassis const &chassis = Chassis[vehChassis];
			int adjustedCost = std::min(2, (int)chassis.speed);
			approximateLandRoughHexCostSum += Rules->move_rate_roads * adjustedCost * ((2 * chassis.speed) + adjustedCost - 1) / (2 * chassis.speed);
		}
		int approximateLandRoughHexCost = approximateLandRoughHexCostSum / 2;
		
		int approximateLandFungusHexCostSum = 0;
		for (VehChassis const &vehChassis : {CHS_FOIL, CHS_CRUISER})
		{
			CChassis const &chassis = Chassis[vehChassis];
			int adjustedCost = std::min(3, (int)chassis.speed);
			approximateLandFungusHexCostSum += Rules->move_rate_roads * adjustedCost * ((2 * chassis.speed) + adjustedCost - 1) / (2 * chassis.speed);
		}
		int approximateLandFungusHexCost = approximateLandFungusHexCostSum / 2;
		
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
			
			tileInfo.hexCosts.at(MT_AIR).at(angle) = {airHexCost, airHexCost};
			
			// sea vehicle moves on ocean or from/to base
			if ((tileInfo.ocean || tileInfo.base) && (adjacentTileInfo->ocean || adjacentTileInfo->base))
			{
				int seaHexCost = mod_hex_cost(BSC_UNITY_GUNSHIP, -1, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				int seaNativeHexCost = mod_hex_cost(BSC_ISLE_OF_THE_DEEP, 0, tileX, tileY, adjacentTileX, adjacentTileY, 0);
				
				int approximateSeaHexCost = adjacentTile->is_fungus() ? approximateSeaFungusHexCost : seaHexCost;
				
				tileInfo.hexCosts.at(MT_SEA).at(angle)			= {seaHexCost,			approximateSeaHexCost			};
				tileInfo.hexCosts.at(MT_SEA_NATIVE).at(angle)	= {seaNativeHexCost,	seaNativeHexCost				};
				
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
				
				tileInfo.hexCosts.at(MT_LAND).at(angle		)			= {landHexCost,				approximateLandHexCost			};
				tileInfo.hexCosts.at(MT_LAND_NATIVE).at(angle)			= {landNativeHexCost,		approximateLandNativeHexCost	};
				tileInfo.hexCosts.at(MT_LAND_EASY).at(angle)			= {landEasyHexCost,			approximateLandEasyHexCost		};
				tileInfo.hexCosts.at(MT_LAND_HOVER).at(angle)			= {landHoverHexCost,		landHoverHexCost				};
				tileInfo.hexCosts.at(MT_LAND_NATIVE_HOVER).at(angle)	= {landNativeHoverHexCost,	landNativeHoverHexCost			};
				
			}
			
		}
		
	}
	
	Profiling::stop("hexCosts");
	
	// blocked and zoc
	
	Profiling::start("blocked and zoc", "populateTileInfos");
	
	// unfriendly base blocks
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		int baseTileIndex = getBaseMapTileIndex(baseId);
		
		// unfriendly
		
		if (!isUnfriendly(aiFactionId, base->faction_id))
			continue;
		
		// blocks
		
		aiData.tileInfos[baseTileIndex].blocked = true;
		
	}
	
	// unfriendly vehicle blocks and zocs on land
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		TileInfo &vehicleTileInfo = aiData.getVehicleTileInfo(vehicleId);
		
		// unfriendly
		
		if (!isUnfriendly(aiFactionId, vehicle->faction_id))
			continue;
		
		// blocks
		
		vehicleTileInfo.blocked = true;
		
		// zoc is exerted only by vehicle on land
		
		if (vehicleTileInfo.ocean)
			continue;
		
		// explore adjacent tiles for zoc
		
		for (Adjacent const &adjacent : vehicleTileInfo.adjacents)
		{
			// zoc is exerted only to land
			
			if (adjacent.tileInfo->ocean)
				continue;
			
			// zoc is not exerted to base
			
			if (adjacent.tileInfo->base)
				continue;
			
			// zoc
			
			adjacent.tileInfo->orgZoc = true;
			adjacent.tileInfo->dstZoc = true;
			
		}
		
	}
	
	// other faction friendly vehicle disables zocDst
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		TileInfo &vehicleTileInfo = aiData.getVehicleTileInfo(vehicleId);
		
		// not this faction
		
		if (vehicle->faction_id == aiFactionId)
			continue;
		
		// friendly
		
		if (!isFriendly(aiFactionId, vehicle->faction_id))
			continue;
		
		// disable zoc
		
		vehicleTileInfo.dstZoc = false;
		
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
	
//	if (DEBUG)
//	{
//		debug("\t\tblocked\n");
//		
//		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
//		{
//			if (isBlocked(tile))
//			{
//				debug("\t\t\t%s\n", getLocationString(tile).c_str());
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
//				debug("\t\t\t%s\n", getLocationString(tile).c_str());
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
//			debug("\t%s", getLocationString(tile).c_str());
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
		
		// player base
		
		if (base->faction_id != aiFactionId)
			continue;
			
		// add base to player baseIds
		
		aiData.baseIds.push_back(baseId);
		
		// set base garrison
		
		baseInfo.combatData.garrison = getBaseGarrison(baseId);
		
		// set base artillery
		
		baseInfo.artillery = false;
		
		for (int vehicleId : baseInfo.combatData.garrison)
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
	
	std::vector<MAP *> openNodes;
	std::vector<MAP *> newOpenNodes;
	
	// baseRange
	
	openNodes.clear();	
	newOpenNodes.clear();
	
	int baseRange = 0;

	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		TileInfo &baseTileInfo = aiData.getTileInfo(baseTile);
		
		baseTileInfo.baseRange = 0;
		openNodes.push_back(baseTile);
		
	}
	
	while (openNodes.size() > 0)
	{
		baseRange++;
		
		for (MAP *tile : openNodes)
		{
			for (MAP *adjacentTile : getAdjacentTiles(tile))
			{
				TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
				
				if (baseRange < adjacentTileInfo.baseRange)
				{
					adjacentTileInfo.baseRange = baseRange;
					newOpenNodes.push_back(adjacentTile);
				}
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	// landBaseRange
	
	openNodes.clear();	
	newOpenNodes.clear();
	
	int landBaseRange = 0;
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		TileInfo &baseTileInfo = aiData.getTileInfo(baseTile);
		
		if (is_ocean(baseTile))
			continue;
		
		baseTileInfo.landBaseRange = 0;
		openNodes.push_back(baseTile);
		
	}
	
	while (openNodes.size() > 0)
	{
		landBaseRange++;
		
		for (MAP *tile : openNodes)
		{
			for (MAP *adjacentTile : getAdjacentTiles(tile))
			{
				TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
				
				if (landBaseRange < adjacentTileInfo.landBaseRange)
				{
					adjacentTileInfo.landBaseRange = landBaseRange;
					newOpenNodes.push_back(adjacentTile);
				}
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	Profiling::stop("populatePlayerBaseRanges");
	
}

void populateFactionInfos()
{
	Profiling::start("populateFactionInfos", "populateAIData");
	
	aiData.maxConOffenseValue = 0;
	aiData.maxConDefenseValue = 0;
	
	// iterate factions
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
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
		
		for (int unitId : getFactionUnitIds(factionId, false, true))
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
		MAP *baseTile = getBaseMapTile(baseId);
		
		// store base snapshot
		
        memcpy(&baseInfo.snapshot, base, sizeof(BASE));
		
		// base info
		
		baseInfo.id = baseId;
		baseInfo.factionId = base->faction_id;
		
		// morale
		
		for (int extendedTriad = 0; extendedTriad < 4; extendedTriad++)
		{
			baseInfo.moraleMultipliers.at(extendedTriad) = getMoraleMultiplier(2 + getBaseMoraleModifier(baseId, extendedTriad));
		}
		
		// strength multipliers
		
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			baseInfo.combatData.sensorOffenseMultipliers.at(factionId) = getSensorOffenseMultiplier(base->faction_id, baseTile);
			baseInfo.combatData.sensorDefenseMultipliers.at(factionId) = getSensorDefenseMultiplier(base->faction_id, baseTile);
		}
		
		// defense
		
		for (int extendedTriad = 0; extendedTriad < 4; extendedTriad++)
		{
			baseInfo.combatData.tileDefenseMultipliers.at(extendedTriad) = getBaseDefenseMultiplier(baseId, extendedTriad);
		}
		
		// gain
		
		baseInfo.gain = getBaseGain(baseId);
		
		debug("\t%-25s gain=%5.2f\n", base->name, baseInfo.gain);
		
	}
	
	// populate faction average base gain
	
	ArithmeticSummaryStatistics baseGainSummary;
	
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
		
		factionInfo.averageBaseGain = baseGainSummary.getArithmeticMean();
		debug("\t%-25s gain=%7.2f\n", "--- average ---", factionInfo.averageBaseGain);
		
	}
	
	Profiling::stop("populateBaseInfos");
	
}

void populatePlayerGlobalVariables()
{
	Profiling::start("populatePlayerGlobalVariables", "populateAIData");
	
	// police2xUnitAvailable
	
	aiData.police2xUnitAvailable = false;
	
	for (int unitId : aiData.unitIds)
	{
		// police2xUnit
		
		if (isInfantryDefensivePolice2xUnit(unitId, aiFactionId))
		{
			aiData.police2xUnitAvailable = true;
		}
		
	}
	
	// summaries
	
	aiData.maxBaseSize = getMaxBaseSize(aiFactionId);
	aiData.maxConOffenseValue = getFactionMaxConOffenseValue(aiFactionId);
	aiData.maxConDefenseValue = getFactionMaxConDefenseValue(aiFactionId);
	
	// statistical parameters
	
	aiData.developmentScale = getDevelopmentScale();
	
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
	
	aiData.unitIds.clear();
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
	
    for (int unitId : getFactionUnitIds(aiFactionId, false, true))
	{
		UNIT *unit = &(Units[unitId]);
		int unitOffenseValue = getUnitOffenseValue(unitId);
		int unitDefenseValue = getUnitDefenseValue(unitId);
		
		debug("\t[%3d] %-32s\n", unitId, unit->name);
		
		// add unit
		
		aiData.unitIds.push_back(unitId);
		
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

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int vehicleLandTransportedCluster = getLandTransportedCluster(vehicleTile);
		int vehicleSeaCluster = getSeaCluster(vehicleTile);

		// store all vehicle current id in pad_0 field

		vehicle->pad_0 = vehicleId;

		// further process only own vehicles

		if (vehicle->faction_id != aiFactionId)
			continue;

		debug("\t[%4d] %s region = %3d\n", vehicleId, getLocationString(vehicleTile).c_str(), vehicleTile->region);

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
		VEH &vehicle = Vehicles[vehicleId];
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
		
		int vehicleMaxRange = (triad == TRIAD_LAND ? getVehicleMoveRate(vehicleId) : getVehicleSpeed(vehicleId));
		
		for (int unfriendlyAffectedVehicleId : unfriendlyAffectedVehicleIds)
		{
			VEH &unfriendlyAffectedVehicle = Vehicles[unfriendlyAffectedVehicleId];
			int unfriendlyAffectedVehicleTriad = unfriendlyAffectedVehicle.triad();
			
			// able to attack
			
			if ((unfriendlyAffectedVehicleTriad == TRIAD_LAND && triad == TRIAD_SEA) || (unfriendlyAffectedVehicleTriad == TRIAD_SEA && triad == TRIAD_LAND))
				continue;
			
			// range
			
			int unfriendlyAffectedVehicleMaxRange = (unfriendlyAffectedVehicleTriad == TRIAD_LAND ? getVehicleMoveRate(unfriendlyAffectedVehicleId) : getVehicleSpeed(unfriendlyAffectedVehicleId));
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
		for (robin_hood::pair<int, int> vehicleReachableLocation : getVehicleReachableLocations(vehicleId, true))
		{
			int tileIndex = vehicleReachableLocation.first;
			TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
			
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
		VEH &vehicle = Vehicles[vehicleId];
		UNIT &unit = Units[vehicle.unit_id];
		CChassis &chassis = Chassis[unit.chassis_id];
		int triad = chassis.triad;
		
		// not player
		
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
		
		int vehicleMaxRange = (triad == TRIAD_LAND ? getVehicleMoveRate(vehicleId) : getVehicleSpeed(vehicleId));
		
		for (int hostileAffectedVehicleId : hostileAffectedVehicleIds)
		{
			VEH &hostileAffectedVehicle = Vehicles[hostileAffectedVehicleId];
			int hostileAffectedVehicleTriad = hostileAffectedVehicle.triad();
			
			// able to attack
			
			if ((hostileAffectedVehicleTriad == TRIAD_LAND && triad == TRIAD_SEA) || (hostileAffectedVehicleTriad == TRIAD_SEA && triad == TRIAD_LAND))
				continue;
			
			// range
			
			int hostileAffectedVehicleMaxRange = (hostileAffectedVehicleTriad == TRIAD_LAND ? getVehicleMoveRate(hostileAffectedVehicleId) : getVehicleSpeed(hostileAffectedVehicleId));
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
		for (robin_hood::pair<int, int> vehicleReachableLocation : getVehicleReachableLocations(vehicleId, true))
		{
			int tileIndex = vehicleReachableLocation.first;
			TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
			
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
		VEH &vehicle = Vehicles[vehicleId];
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
		
		int vehicleMaxRange = (triad == TRIAD_LAND ? getVehicleMoveRate(vehicleId) : getVehicleSpeed(vehicleId));
		
		for (int artilleryAffectedVehicleId : artilleryAffectedVehicleIds)
		{
			VEH &artilleryAffectedVehicle = Vehicles[artilleryAffectedVehicleId];
			int artilleryAffectedVehicleTriad = artilleryAffectedVehicle.triad();
			
			// able to attack
			
			if ((artilleryAffectedVehicleTriad == TRIAD_LAND && triad == TRIAD_SEA) || (artilleryAffectedVehicleTriad == TRIAD_SEA && triad == TRIAD_LAND))
				continue;
			
			// range
			
			int artilleryAffectedVehicleMaxRange = (artilleryAffectedVehicleTriad == TRIAD_LAND ? getVehicleMoveRate(artilleryAffectedVehicleId) : getVehicleSpeed(artilleryAffectedVehicleId));
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
		for (robin_hood::pair<int, int> vehicleReachableLocation : getVehicleReachableLocations(vehicleId, true))
		{
			int tileIndex = vehicleReachableLocation.first;
			TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
			
			tileInfo.artilleryDangerZone = true;
			
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
		
		// ignore land aliens too far from player bases
		
		if (vehicle->faction_id == 0 && !is_ocean(vehicleTile) && vehicleTileInfo.landBaseRange > 10)
			continue;
		
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
		
		if (enemyStackInfo.vehicleIds.size() == 0)
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
		
		debug("\t\t%s\n", getLocationString(enemyStackTile).c_str());
		
		// populate baseRange
		
		enemyStackInfo.baseRange = aiData.getTileInfo(enemyStackTile).nearestBaseRanges.at(TRIAD_AIR).value;
		
		// populate base/bunker/airbase
		
		enemyStackInfo.base = isBaseAt(enemyStackTile);
		enemyStackInfo.baseOrBunker = isBaseAt(enemyStackTile) || isBunkerAt(enemyStackTile);
		enemyStackInfo.airbase = isAirbaseAt(enemyStackTile);
		enemyStackInfo.baseId = base_at(getX(enemyStackTile), getY(enemyStackTile));
		
		// bombardmentDestructive
		
		enemyStackInfo.bombardmentDestructive = getLocationBombardmentMaxRelativeDamage(enemyStackInfo.tile) == 1.0;
		
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
	
	ArithmeticSummaryStatistics unitCostSummary;
	ArithmeticSummaryStatistics attackGainSummary;
	for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
		debug("\t\t%s\n", getLocationString(enemyStackInfo.tile).c_str());
		
		// averageUnitCost
		
		unitCostSummary.clear();
		for (int vehicleId : enemyStackInfo.vehicleIds)
		{
			int unitCost = Vehicles[vehicleId].cost();
			unitCostSummary.add(unitCost);
		}
		
		enemyStackInfo.averageUnitCost = unitCostSummary.getArithmeticMean();
		
		debug
		(
			"\t\t\t"
			" averageUnitCost=%5.2f"
			"\n"
			, enemyStackInfo.averageUnitCost
		);
		
		// averageAttackGain
		
		double averageAttackGain;
		
		if (enemyStackInfo.base && false)
		{
			// base capture gain
			
			averageAttackGain = conf.ai_combat_attack_priority_base * aiData.getBaseInfo(enemyStackInfo.baseId).gain;
			
		}
		else
		{
			// compute average priority based on each stack vehicle priority
			
			attackGainSummary.clear();
			for (int vehicleId : enemyStackInfo.vehicleIds)
			{
				double attackGain = getDestroyingEnemyVehicleGain(vehicleId);
				attackGainSummary.add(attackGain);
			}
			
			averageAttackGain = attackGainSummary.getArithmeticMean();
			
		}
		
		enemyStackInfo.averageAttackGain = averageAttackGain;
		
		debug
		(
			"\t\t\t"
			" averageAttackGain=%5.2f"
			"\n"
			, enemyStackInfo.averageAttackGain
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

void evaluateFactionMilitaryPowers()
{
	
}

void computeCombatEffects()
{
	Profiling::start("computeCombatEffects", "populateAIData");
	
	debug("computeCombatEffects - %s\n", MFactions[aiFactionId].noun_faction);
	
	CombatEffectTable &combatEffectTable = aiData.combatEffectTable;
	
	robin_hood::unordered_flat_set<int> &ownCombatUnitIds = combatEffectTable.ownCombatUnitIds;
	robin_hood::unordered_flat_set<int> &foeFactionIds = combatEffectTable.foeFactionIds;
	robin_hood::unordered_flat_set<int> &hostileFoeFactionIds = combatEffectTable.hostileFoeFactionIds;
	robin_hood::unordered_flat_set<int> &foeCombatVehicleIds = combatEffectTable.foeCombatVehicleIds;
	std::array<robin_hood::unordered_flat_set<int> , MaxPlayerNum> &foeUnitIds = combatEffectTable.foeUnitIds;
	std::array<robin_hood::unordered_flat_set<int> , MaxPlayerNum> &foeCombatUnitIds = combatEffectTable.foeCombatUnitIds;
	
	// collect own combat units
	
	ownCombatUnitIds.clear();
	ownCombatUnitIds.insert(aiData.combatUnitIds.begin(), aiData.combatUnitIds.end());
	ownCombatUnitIds.insert(aiData.activeCombatUnitIds.begin(), aiData.activeCombatUnitIds.end());
	
	// populate enemy factions, vehicles, units
	
	foeFactionIds.clear();foeFactionIds.insert(0);
	hostileFoeFactionIds.clear();hostileFoeFactionIds.insert(0);
	foeCombatVehicleIds.clear();
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		foeUnitIds.at(factionId).clear();
		foeCombatUnitIds.at(factionId).clear();
	}
		
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		// exclude own
		
		if (factionId == aiFactionId)
			continue;
		
		// exclude pact faction
		
		if (isPact(aiFactionId, factionId))
			continue;
		
		// add enemy faction
		
		foeFactionIds.insert(factionId);
		
		// add hostile faction
		
		if (isHostile(aiFactionId, factionId))
		{
			hostileFoeFactionIds.insert(factionId);
		}
		
		for (int unitId : getFactionUnitIds(factionId, true, true))
		{
			// add to all list
			
			foeUnitIds.at(factionId).insert(unitId);
			
			// add to combat list
			
			if (isCombatUnit(unitId))
			{
				foeCombatUnitIds.at(factionId).insert(unitId);
			}
			
		}
		
	}
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int factionId = vehicle->faction_id;
		int unitId = vehicle->unit_id;
		
		// exclude own
		
		if (factionId == aiFactionId)
			continue;
		
		// exclude pact faction
		
		if (isPact(aiFactionId, factionId))
			continue;
		
		// add enemy faction
		
		foeFactionIds.insert(factionId);
		
		// add hostile faction
		
		if (isHostile(aiFactionId, factionId))
		{
			hostileFoeFactionIds.insert(factionId);
		}
		
		// add to all list
		
		foeUnitIds.at(factionId).insert(unitId);
		
		// add to combat list
		
		if (isCombatVehicle(vehicleId))
		{
			foeCombatUnitIds.at(factionId).insert(unitId);
			foeCombatVehicleIds.insert(vehicleId);
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
	
	// compute combatEffects
	
	combatEffectTable.clear();
	
	for (int ownUnitId : ownCombatUnitIds)
	{
		for (int foeFactionId : foeFactionIds)
		{
			for (int foeUnitId : foeUnitIds.at(foeFactionId))
			{
				// calculate how many foe units our unit can destroy in melee attack
				
				{
					double combatEffect = (isMeleeUnit(ownUnitId) ? getMeleeRelativeUnitStrength(ownUnitId, aiFactionId, foeUnitId, foeFactionId) : 0.0);
					combatEffectTable.setCombatEffect(aiFactionId, ownUnitId, foeFactionId, foeUnitId, CM_MELEE, combatEffect);
				}
				
				// calculate how many our units foe unit can destroy in melee attack
				
				{
					double combatEffect = (isMeleeUnit(foeUnitId) ? getMeleeRelativeUnitStrength(foeUnitId, foeFactionId, ownUnitId, aiFactionId) : 0.0);
					combatEffectTable.setCombatEffect(foeFactionId, foeUnitId, aiFactionId, ownUnitId, CM_MELEE, combatEffect);
				}
				
				// calculate how many foe units our unit can destroy in artillery duel
				
				{
					double combatEffect = (isArtilleryUnit(ownUnitId) && isArtilleryUnit(foeUnitId) ? getArtilleryDuelRelativeUnitStrength(ownUnitId, aiFactionId, foeUnitId, foeFactionId) : 0.0);
					combatEffectTable.setCombatEffect(aiFactionId, ownUnitId, foeFactionId, foeUnitId, CM_ARTILLERY_DUEL, combatEffect);
				}
				
				// calculate how many our units foe unit can destroy in artillery duel
				
				{
					double combatEffect = (isArtilleryUnit(foeUnitId) && isArtilleryUnit(ownUnitId) ? getArtilleryDuelRelativeUnitStrength(foeUnitId, foeFactionId, ownUnitId, aiFactionId) : 0.0);
					combatEffectTable.setCombatEffect(foeFactionId, foeUnitId, aiFactionId, ownUnitId, CM_ARTILLERY_DUEL, combatEffect);
				}
				
				// calculate how many foe units our unit can destroy by bombardment
				
				{
					double combatEffect = (isArtilleryUnit(ownUnitId) && !isArtilleryUnit(foeUnitId) ? getUnitBombardmentDamage(ownUnitId, aiFactionId, foeUnitId, foeFactionId) : 0.0);
					combatEffectTable.setCombatEffect(aiFactionId, ownUnitId, foeFactionId, foeUnitId, CM_BOMBARDMENT, combatEffect);
				}
				
				// calculate how many own units foe unit can destroy by bombardment
				
				{
					double combatEffect = (isArtilleryUnit(foeUnitId) && !isArtilleryUnit(ownUnitId) ? getUnitBombardmentDamage(foeUnitId, foeFactionId, ownUnitId, aiFactionId) : 0.0);
					combatEffectTable.setCombatEffect(foeFactionId, foeUnitId, aiFactionId, ownUnitId, CM_BOMBARDMENT, combatEffect);
				}
				
			}
			
		}
		
	}
	
	// assaultEffects
	
	for (int ownUnitId : ownCombatUnitIds)
	{
		for (int foeFactionId : foeFactionIds)
		{
			for (int foeUnitId : foeUnitIds.at(foeFactionId))
			{
				// our unit assaults foe unit
				
				aiData.combatEffectTable.setAssaultEffect(aiFactionId, ownUnitId, foeFactionId, foeUnitId, computeAssaultEffect(aiFactionId, ownUnitId, foeFactionId, foeUnitId));
				
				// foe unit assaults our unit
				
				aiData.combatEffectTable.setAssaultEffect(foeFactionId, foeUnitId, aiFactionId, ownUnitId, computeAssaultEffect(foeFactionId, foeUnitId, aiFactionId, ownUnitId));
				
			}
			
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (int ownUnitId : ownCombatUnitIds)
//		{
//			debug("\t[%3d] %-32s\n", ownUnitId, getUnit(ownUnitId)->name);
//			
//			for (int foeFactionId : foeFactionIds)
//			{
//				debug("\t\t%-24s\n", getMFaction(foeFactionId)->noun_faction);
//				
//				for (int foeUnitId : foeUnitIds.at(foeFactionId))
//				{
//					debug("\t\t\t[%3d] %-32s\n", foeUnitId, getUnit(foeUnitId)->name);
//					
//					for (AttackingSide side : {AS_OWN, AS_FOE})
//					{
//						for (COMBAT_MODE combatMode : {CM_MELEE, CM_ARTILLERY_DUEL, CM_BOMBARDMENT})
//						{
//							double effect = combatEffectTable.getCombatEffect(ownUnitId, foeFactionId, foeUnitId, side, combatMode);
//							
//							if (effect == 0.0)
//								continue;
//							
//							debug("\t\t\t\tside=%d CT=%d effect=%5.2f\n", side, combatMode, effect);
//							
//						}
//						
//					}
//					
//				}
//				
//			}
//			
//		}
//		
//	}
	
	Profiling::stop("computeCombatEffects");
	
}

void populateEnemyBaseInfos()
{
	Profiling::start("populateEnemyBaseInfos", "populateAIData");
	
	populateEnemyBaseCaptureGains();
	populateEnemyBaseProtectorWeights();
	populateEnemyBaseAssaultEffects();
	
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
//				for (IdDoubleValue protectorWeight : baseInfo.protectorWeights)
//				{
//					int vehicleId = protectorWeight.id;
//					double weight = protectorWeight.value;
//					
//					debug("\t\t\t[%4d] %s %-32s weight=%5.2f\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), getVehicle(vehicleId)->name(), weight);
//					
//				}
//				
//			}
//			
//		}
		
	}
	
	Profiling::stop("populateEnemyBaseProtectorWeights");
	
}

void populateEnemyBaseAssaultEffects()
{
	Profiling::start("populateEnemyBaseAssaultEffects", "populateEnemyBaseInfos");
	
	debug("populateEnemyBaseAssaultEffects - %s\n", MFactions[aiFactionId].noun_faction);
	
	// reset base values
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		
		baseInfo.assaultEffects.clear();
		
	}
	
	// iterate bases
	
	ArithmeticSummaryStatistics assaultEffectSummary;
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		
		// exclude player base or friendly base
		
		if (isFriendly(aiFactionId, base->faction_id))
			continue;
		
		// evaluate player unit assault effects
		
		for (int aiUnitId : aiData.combatUnitIds)
		{
			UNIT *aiUnit = getUnit(aiUnitId);
			
			// can capture base
			
			if (!isUnitCanCaptureBase(aiUnitId, baseTile))
			{
				baseInfo.assaultEffects.insert({aiUnitId, 0.0});
				continue;
			}
			
			// iterate base protectors
			
			assaultEffectSummary.clear();
			for (robin_hood::pair<int, robin_hood::unordered_flat_map<int, double>> const &protectorUnitWeightEntry : baseInfo.protectorUnitWeights)
			{
				int enemyUnitFactionId = protectorUnitWeightEntry.first;
				robin_hood::unordered_flat_map<int, double> const &protectorFactionUnitWeights = protectorUnitWeightEntry.second;
				
				for (robin_hood::pair<int, double> const &protectorFactionUnitWeightEntry : protectorFactionUnitWeights)
				{
					int enemyUnitId = protectorFactionUnitWeightEntry.first;
					double enemyUnitWeight = protectorFactionUnitWeightEntry.second;
					
					// our unit assaulting enemy base
					
					AssaultEffect const &assaultEffect = aiData.combatEffectTable.getAssaultEffect(aiFactionId, aiUnitId, enemyUnitFactionId, enemyUnitId);
					
					bool psiCombat = isPsiCombat(aiUnitId, enemyUnitId);
					int extendedTriad = psiCombat ? 3 : aiUnit->triad();
					double attackMultiplier = 1.0 / (baseInfo.combatData.sensorDefenseMultipliers.at(base->faction_id) * baseInfo.combatData.tileDefenseMultipliers.at(extendedTriad));
					double defendMultiplier = 1.0 / (baseInfo.combatData.sensorOffenseMultipliers.at(base->faction_id));
					double effect = assaultEffect.getEffect(attackMultiplier, defendMultiplier);
					
					assaultEffectSummary.add(effect, enemyUnitWeight);
					
				}
				
			}
			
			double averageAssaultEffect = assaultEffectSummary.getArithmeticMean();
			baseInfo.assaultEffects.insert({aiUnitId, averageAssaultEffect});
			
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (int baseId = 0; baseId < *BaseCount; baseId++)
//		{
//			BASE *base = getBase(baseId);
//			int factionId = base->faction_id;
//			BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
//			
//			// exclude player base
//			
//			if (factionId == aiFactionId)
//				continue;
//			
//			debug("\t%-25s\n", Bases[baseId].name);
//			
//			for (robin_hood::pair<int, double> assaultEffectEntry : baseInfo.assaultEffects)
//			{
//				int unitId = assaultEffectEntry.first;
//				double assaultEffect = assaultEffectEntry.second;
//				
//				debug("\t\t%-32s %5.2f\n", getUnit(unitId)->name, assaultEffect);
//				
//			}
//			
//		}
//		
//	}
	
	Profiling::stop("populateEnemyBaseAssaultEffects");
	
}

void evaluateEnemyStacks()
{
	Profiling::start("evaluateEnemyStacks", "populateAIData");
	
	debug("evaluateEnemyStacks - %s\n", MFactions[aiFactionId].noun_faction);
	
	CombatEffectTable &combatEffectTable = aiData.combatEffectTable;
	
	robin_hood::unordered_flat_set<int> const &ownCombatUnitIds = combatEffectTable.ownCombatUnitIds;
	
	FoeUnitWeightTable foeUnitWeightTable;
	
	for (robin_hood::pair<MAP *, EnemyStackInfo> &stackInfoEntry : aiData.enemyStacks)
	{
		MAP *enemyStackTile = stackInfoEntry.first;
		EnemyStackInfo &enemyStackInfo = stackInfoEntry.second;
		
		assert(isOnMap(enemyStackTile));
		
		// ignore stacks without vehicles
		
		if (enemyStackInfo.vehicleIds.empty())
			continue;
		
		debug("\t(%3d,%3d)\n", getX(enemyStackTile), getY(enemyStackTile));
		
		// superiorities
		
		Profiling::start("superiorities", "evaluateEnemyStacks");
		
		if (isBaseAt(enemyStackTile))
		{
			enemyStackInfo.requiredSuperiority = conf.ai_combat_base_attack_superiority_required;
			enemyStackInfo.desiredSuperiority = conf.ai_combat_base_attack_superiority_desired;
		}
		// attacking aliens (except tower) does not require minimal superiority
		else if (enemyStackInfo.alien && !enemyStackInfo.alienFungalTower)
		{
			enemyStackInfo.requiredSuperiority = 0.0;
			enemyStackInfo.desiredSuperiority = conf.ai_combat_field_attack_superiority_desired;
		}
		else
		{
			enemyStackInfo.requiredSuperiority = conf.ai_combat_field_attack_superiority_required;
			enemyStackInfo.desiredSuperiority = conf.ai_combat_field_attack_superiority_desired;
		}
		
		Profiling::stop("superiorities");
		
		// maxTotalBombardmentEffect
		
		Profiling::start("maxTotalBombardmentEffect", "evaluateEnemyStacks");
		
		debug("\t\tmaxBombardmentTotalEffect\n");
		
		double locationMaxBombardmentRatio = getLocationBombardmentMaxRelativeDamage(enemyStackTile);
		
		double maxBombardmentEffect = 0.0;
		
		for (int vehicleId : enemyStackInfo.vehicleIds)
		{
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
			double vehicleMaxBombardmentDamage = locationMaxBombardmentRatio - vehicleRelativeDamage;
			
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
		
		// unit effects
		
		Profiling::start("unit effects", "evaluateEnemyStacks");
		
		debug("\t\tunit effects\n");
		
		ArithmeticSummaryStatistics assaultEffectSummary;
		for (int ownUnitId : ownCombatUnitIds)
		{
			debug("\t\t\t[%3d] %-32s\n", ownUnitId, Units[ownUnitId].name);
			
			assaultEffectSummary.clear();
			for (int foeVehicleId : enemyStackInfo.vehicleIds)
			{
				VEH *foeVehicle = getVehicle(foeVehicleId);
				
				AssaultEffect const &assaultEffect = aiData.combatEffectTable.getAssaultEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id);
				
				double foeVehicleStrengthMultiplier = getVehicleStrenghtMultiplier(foeVehicleId);
				double attackMultiplier = 1.0 / (foeVehicleStrengthMultiplier);
				double defendMultiplier = 1.0 / (foeVehicleStrengthMultiplier);
				double effect = assaultEffect.getEffect(attackMultiplier, defendMultiplier);
				
				assaultEffectSummary.add(effect);
				
				debug
				(
					"\t\t\t\t[%3d] %-32s - %-20s"
					" effect=%5.2f"
					"\n"
					, foeVehicle->unit_id, Units[foeVehicle->unit_id].name, MFactions[foeVehicle->faction_id].noun_faction
					, effect
				);
				
			}
			
			// effect
			
			double averageAssaultEffect = assaultEffectSummary.getArithmeticMean();
			enemyStackInfo.setUnitEffect(ownUnitId, averageAssaultEffect);
			
			debug
			(
				"\t\t\t%-32s"
				" averageAssaultEffect=%5.2f"
				"\n"
				, Units[ownUnitId].name
				, averageAssaultEffect
			);
			
		}
		
		Profiling::stop("unit effects");
		
		// calculate unit offense effects
		
		Profiling::start("calculate unit offense effects", "evaluateEnemyStacks");
		
		debug("\t\tunit offense effects\n");
		
		for (int ownUnitId : ownCombatUnitIds)
		{
			debug("\t\t\t[%3d] %-32s\n", ownUnitId, getUnit(ownUnitId)->name);
			
			// direct
			
			if (isMeleeUnit(ownUnitId) && !(enemyStackInfo.needlejetInFlight && !isUnitHasAbility(ownUnitId, ABL_AIR_SUPERIORITY)))
			{
				int battleCount = 0;
				double totalLoss = 0.0;
				
				for (int foeVehicleId : enemyStackInfo.vehicleIds)
				{
					VEH *foeVehicle = getVehicle(foeVehicleId);
					
					double effect = aiData.combatEffectTable.getCombatEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, CM_MELEE);
					
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
				
				for (int foeVehicleId : enemyStackInfo.artilleryVehicleIds)
				{
					VEH *foeVehicle = getVehicle(foeVehicleId);
					
					double effect = aiData.combatEffectTable.getCombatEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, CM_ARTILLERY_DUEL);
					
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
				
				for (int foeVehicleId : enemyStackInfo.nonArtilleryVehicleIds)
				{
					VEH *foeVehicle = getVehicle(foeVehicleId);
					
					double effect = aiData.combatEffectTable.getCombatEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, CM_BOMBARDMENT);
					
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
		
//		// calculate unit defense effects
//		
//		debug("\t\tunit defense effects\n");
//		
//		for (int ownUnitId : ownCombatUnitIds)
//		{
//			debug("\t\t\t[%3d] %-32s\n", ownUnitId, getUnit(ownUnitId)->name);
//			
//			// artillery
//			
//			if (isArtilleryUnit(ownUnitId))
//			{
//				// artillery duel effect
//				
//				{
//					int battleCount = 0;
//					double totalLoss = 0.0;
//					
//					for (int foeVehicleId : enemyStackInfo.artilleryVehicleIds)
//					{
//						// effect
//						
//						double effect = getUnitArtilleryDuelAttackEffect(aiFactionId, ownUnitId, foeVehicleId, enemyStackTile);
//						
//						if (effect <= 0.0)
//							continue;
//						
//						// loss
//						
//						battleCount++;
//						
//						double loss = 1.0 / effect;
//						totalLoss += loss;
//						
//					}
//					
//					double averageEffect = (battleCount == 0 || totalLoss == 0.0 ? 0.0 : battleCount / totalLoss);
//					debug("\t\t\t\t%-15s %5.2f\n", "artilleryDuel", averageEffect);
//					
//					enemyStackInfo.setUnitEffect(ownUnitId, CM_ARTILLERY_DUEL, true, averageEffect);
//					
//				}
//				
//				// bombardment effect
//				
//				{
//					bool destructive = getLocationBombardmentMinRelativePower(enemyStackTile) == 0.0;
//					
//					double totalEffect = 0.0;
//					
//					for (int foeVehicleId : enemyStackInfo.nonArtilleryVehicleIds)
//					{
//						double effect = getUnitBombardmentAttackEffect(aiFactionId, ownUnitId, foeVehicleId, enemyStackTile);
//						
//						if (effect <= 0.0)
//							continue;
//						
//						totalEffect += effect;
//						
//					}
//					
//					debug("\t\t\t\t%-15s %5.2f\n", "bombardment", totalEffect);
//					
//					enemyStackInfo.setUnitEffect(ownUnitId, CM_BOMBARDMENT, destructive, totalEffect);
//					
//				}
//				
//			}
//			
//			// melee
//			
//			if (isMeleeUnit(ownUnitId) && !(enemyStackInfo.needlejetInFlight && !isUnitHasAbility(ownUnitId, ABL_AIR_SUPERIORITY)))
//			{
//				int battleCount = 0;
//				double totalLoss = 0.0;
//				
//				for (int foeVehicleId : enemyStackInfo.vehicleIds)
//				{
//					// effect
//					
//					double effect = getUnitMeleeAssaultEffect(aiFactionId, ownUnitId, foeVehicleId, enemyStackTile);
//					
//					if (effect <= 0.0)
//						continue;
//					
//					// loss
//					
//					battleCount++;
//					
//					double loss = 1.0 / effect;
//					totalLoss += loss;
//					
//				}
//				
//				double averageEffect = (battleCount == 0 || totalLoss == 0.0 ? 0.0 : battleCount / totalLoss);
//				debug("\t\t\t\t%-15s %5.2f\n", "melee", averageEffect);
//				
//				enemyStackInfo.setUnitEffect(ownUnitId, CM_MELEE, true, averageEffect);
//				
//			}
//			
//		}
		
	}
	
	Profiling::stop("evaluateEnemyStacks");
	
}

void evaluateBaseDefense()
{
	debug("evaluateBaseDefense - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int baseId : aiData.baseIds)
	{
		MAP *tile = getBaseMapTile(baseId);
		ProtectCombatData &combatData = aiData.baseInfos.at(baseId).combatData;
		evaluateDefense(tile, combatData);
	}
	
	for (robin_hood::pair<MAP *, ProtectCombatData> &bunkerCombatDataEntry : aiData.bunkerCombatDatas)
	{
		MAP *tile = bunkerCombatDataEntry.first;
		ProtectCombatData &combatData = bunkerCombatDataEntry.second;
		evaluateDefense(tile, combatData);
	}
	
}

void evaluateDefense(MAP *tile, ProtectCombatData &combatData)
{
	Profiling::start("evaluateBaseDefense", "populateAIData");
	
	CombatEffectTable &combatEffectTable = aiData.combatEffectTable;
	robin_hood::unordered_flat_set<int> const &foeFactionIds = combatEffectTable.foeFactionIds;
	robin_hood::unordered_flat_set<int> const &foeCombatVehicleIds = combatEffectTable.foeCombatVehicleIds;
	std::array<robin_hood::unordered_flat_set<int> , MaxPlayerNum> const &foeCombatUnitIds = combatEffectTable.foeCombatUnitIds;
	
	// evaluate base threat

	Profiling::start("evaluate base threat", "evaluateBaseDefense");
	
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	BASE *base = getBase(tileInfo.baseId);
	
	combatData.clear();
	
	debug
	(
		"\t%s %-25s"
		"\n"
		, getLocationString(tile).c_str()
		, base == nullptr ? "--- bunker ---" : base->name
	);
	
	// calculate foe strength
	
	Profiling::start("calculate foe strength", "evaluate base threat");
	
	debug("\t\tbase foeMilitaryStrength\n");
	
	std::array<robin_hood::unordered_flat_map<int, double>, MaxPlayerNum> foeUnitWeights;
	
	int alienCount = 0;
	for (int vehicleId : foeCombatVehicleIds)
	{
		VEH &vehicle = Vehicles[vehicleId];
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
		
		// strength multiplier
		
		double strengthMultiplier = getVehicleStrenghtMultiplier(vehicleId);
		
		// threat coefficient
		
		double threatCoefficient = aiData.factionInfos[vehicle.faction_id].threatCoefficient;
		
		// approach time coefficient
		
		double approachTime = getVehicleAttackApproachTime(vehicleId, tile);
		if (approachTime == INF)
			continue;
		
		double approachTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, approachTime);
		
		// weight
		
		double weight = strengthMultiplier * threatCoefficient * approachTimeCoefficient;
		
		// store value
		
		robin_hood::unordered_flat_map<int, double> &foeFactionUnitWeights = foeUnitWeights.at(vehicle.faction_id);
		if (foeFactionUnitWeights.find(vehicle.unit_id) == foeFactionUnitWeights.end())
		{
			foeFactionUnitWeights.insert({vehicle.unit_id, 0.0});
		}
		foeFactionUnitWeights.at(vehicle.unit_id) += weight;
		
		if (vehicle.faction_id == 0)
		{
			alienCount++;
		}
		
		debug
		(
			"\t\t\t(%3d,%3d) %-32s"
			" weight=%5.2f"
			" strengthMultiplier=%5.2f"
			" approachTime=%7.2f approachTimeCoefficient=%5.2f"
			" threatCoefficient=%5.2f"
			"\n"
			, vehicle.x, vehicle.y, unit.name
			, weight
			, strengthMultiplier
			, approachTime, approachTimeCoefficient
			, threatCoefficient
		);
		
	}
	
	// reduce alien weight assuming uncoordinated attack
	
	if (alienCount > 0)
	{
		double alienWeightModifier = 1.0 / sqrt(alienCount);
		
		for (robin_hood::pair<int, double> &alienUnitWeightEntry : foeUnitWeights.at(0))
		{
			alienUnitWeightEntry.second *= alienWeightModifier;
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
		double gameTurnCoefficient = getAlienTurnStrengthModifier();
		
		// set basic numbers to occasional vehicle
		
		double numberCoefficient = 1.0;
		
		// add more numbers based on eco-damage and number of fungal pops
		
		if (base != nullptr && tileInfo.land)
		{
			numberCoefficient += (double)((aiFaction->clean_minerals_modifier / 3) * (base->eco_damage / 20));
		}
		
		// calculate weight
		// reduce potential alien weight slightly to increase mobility even if for increased risk
		
		double weight = 0.75 * moraleMultiplier * gameTurnCoefficient * numberCoefficient;
		
		// do not add potential weight more than actual one
		
		if (foeUnitWeights.at(0).find(BSC_MIND_WORMS) == foeUnitWeights.at(0).end())
		{
			foeUnitWeights.at(0).emplace(BSC_MIND_WORMS, 0.0);
		}
		foeUnitWeights.at(0).at(BSC_MIND_WORMS) = std::max(foeUnitWeights.at(0).at(BSC_MIND_WORMS), weight);
		
		debug
		(
			"\t\t\t%-32s weight=%5.2f, moraleMultiplier=%5.2f, gameTurnCoefficient=%5.2f, numberCoefficient=%5.2f\n",
			Units[alienUnitId].name,
			weight, moraleMultiplier, gameTurnCoefficient, numberCoefficient
		);
		
	}
	
	// --------------------------------------------------
	// calculate potential alien strength - end
	// --------------------------------------------------
	
	// summarize foe unit weights by faction
	
	Profiling::start("summarize foe unit weights by faction", "evaluate base threat");
	
	std::array<double, MaxPlayerNum> foeFactionWeights = {};
	
	for (int foeFactionId : foeFactionIds)
	{
		robin_hood::unordered_flat_map<int, double> &foeFactionUnitWeights = foeUnitWeights.at(foeFactionId);
		
		for (int foeUnitId : foeCombatUnitIds.at(foeFactionId))
		{
			// exclude alien spore launchers and fungal tower
			
			if (foeFactionId == 0 && (foeUnitId == BSC_SPORE_LAUNCHER || foeUnitId == BSC_FUNGAL_TOWER))
				continue;
			
			// not present in threat
			
			if (foeFactionUnitWeights.find(foeUnitId) == foeFactionUnitWeights.end())
				continue;
			
			// accumulate weight
			
			double weight = foeFactionUnitWeights.at(foeUnitId);
			foeFactionWeights.at(foeFactionId) += weight;
			
		}
		
	}
	
	// select neutral faction to beware of
	
	int neutralFactionId = -1;
	double neutralFactionWeight = 0.0;
	for (int foeFactionId : foeFactionIds)
	{
		if (!isNeutral(aiFactionId, foeFactionId))
			continue;
		
		double foeFactionWeight = foeFactionWeights.at(foeFactionId);
		if (foeFactionWeight > neutralFactionWeight)
		{
			neutralFactionId = foeFactionId;
			neutralFactionWeight = foeFactionWeight;
		}
		
	}
	
	Profiling::stop("summarize foe unit weights by faction");
	
	// compute required protection
	
	Profiling::start("compute required protection", "evaluate base threat");
	
	double coefficient = conf.ai_combat_base_protection_superiority * (tileInfo.ocean ? conf.ai_combat_sea_base_threat_coefficient : 1.0);
	
	debug("\t\trequiredEffects\n");
	combatData.requiredEffect = 0.0;
	for (int foeFactionId : foeFactionIds)
	{
		if (!(isHostile(aiFactionId, foeFactionId) || foeFactionId == neutralFactionId))
			continue;
		
		debug("\t\t\t%-24s\n", MFactions[foeFactionId].noun_faction);
		for (robin_hood::pair<int, double> const &foeFactionUnitWeightEntry : foeUnitWeights.at(foeFactionId))
		{
			int foeUnitId = foeFactionUnitWeightEntry.first;
			double foeUnitWeight = foeFactionUnitWeightEntry.second;
			double requiredEffect = coefficient * foeUnitWeight;
			combatData.counterEffects.at(foeFactionId).emplace(foeUnitId, CounterEffect(requiredEffect));
			combatData.requiredEffect += requiredEffect;
			debug("\t\t\t\t%-32s %5.2f coefficient=%5.2f foeUnitWeight=%5.2f requiredEffect=%5.2f\n", Units[foeUnitId].name, combatData.counterEffects.at(foeFactionId).at(foeUnitId).required, coefficient, foeUnitWeight, requiredEffect);
		}
		
	}
	
	Profiling::stop("compute required protection");
	
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
			VEH *vehicle = &(Vehicles[vehicleId]);
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
			
			double approachTime = getVehicleAttackApproachTime(vehicleId, baseTile);
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
			" conf.ai_combat_base_protection_superiority=%5.2F"
			" totalWeight=%5.2f"
			"\n"
			, requiredEffect
			, conf.ai_combat_base_protection_superiority
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
	
	for (int unitId : getFactionUnitIds(aiFactionId, true, false))
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
	
	int unitId = modified_propose_proto(factionId, chassisId, weaponId, armorId, abilities, reactor, plan, name);
	
	debug("checkAndProposePrototype - %s\n", MFactions[aiFactionId].noun_faction);
	debug("\treactor=%d, chassisId=%d, weaponId=%d, armorId=%d, abilities=%s\n", reactor, chassisId, weaponId, armorId, getAbilitiesString(abilities).c_str());
	debug("\tunitId=%d\n", unitId);
	
}

/*
Obsolete prototypes.
*/
void obsoletePrototypes(int factionId, robin_hood::unordered_flat_set<int> chassisIds, robin_hood::unordered_flat_set<int> weaponIds, robin_hood::unordered_flat_set<int> armorIds, robin_hood::unordered_flat_set<int> abilityFlagsSet, robin_hood::unordered_flat_set<int> reactors)
{
	debug("obsoletePrototypes - %s\n", MFactions[factionId].noun_faction);
	
//	if (DEBUG)
//	{
//		debug("\tchassis\n");
//		for (int chassisId : chassisIds)
//		{
//			debug("\t\t%d\n", chassisId);
//		}
//		
//		debug("\tweapons\n");
//		for (int weaponId : weaponIds)
//		{
//			debug("\t\t%s\n", Weapon[weaponId].name);
//		}
//		
//		debug("\tarmors\n");
//		for (int armorId : armorIds)
//		{
//			debug("\t\t%s\n", Armor[armorId].name);
//		}
//		
//		debug("\tabilityFlags\n");
//		for (int abilityFlags : abilityFlagsSet)
//		{
//			debug("\t\t%s\n", getAbilitiesString(abilityFlags).c_str());
//		}
//		
//		debug("\treactors\n");
//		for (int reactor : reactors)
//		{
//			debug("\t\t%s\n", Reactor[reactor - 1].name);
//		}
//		
//	}
	
	debug("\t----------\n");
	for (int unitId : getFactionUnitIds(factionId, false, true))
	{
		UNIT *unit = &(Units[unitId]);
		debug("\t%s\n", unit->name);
		debug("\t\t%d\n", unit->chassis_id);
		debug("\t\t%s\n", Weapon[unit->weapon_id].name);
		debug("\t\t%s\n", Armor[unit->armor_id].name);
		debug("\t\t%s\n", getAbilitiesString(unit->ability_flags).c_str());
		debug("\t\t%s\n", Reactor[unit->reactor_id - 1].name);
		
		// verify condition match
		
		if
		(
			(chassisIds.size() == 0 || chassisIds.count(unit->chassis_id) != 0)
			&&
			(weaponIds.size() == 0 || weaponIds.count(unit->weapon_id) != 0)
			&&
			(armorIds.size() == 0 || armorIds.count(unit->armor_id) != 0)
			&&
			(abilityFlagsSet.size() == 0 || abilityFlagsSet.count(unit->ability_flags) != 0)
			&&
			(reactors.size() == 0 || reactors.count(unit->reactor_id) != 0)
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

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
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
	double minTravelTime = DBL_MAX;
	
	for (MAP *tile : aiData.pods)
	{
		// reachable
		
		if (!isVehicleDestinationReachable(vehicleId, tile))
			continue;
		
		// get travel time
		
		double travelTime = getVehicleTravelTime(vehicleId, tile);
		if (travelTime == INF)
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

		// same realm if requested

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

int getFactionMaxConOffenseValue(int factionId)
{
	int bestWeaponOffenseValue = 0;

	for (int unitId : getFactionUnitIds(factionId, false, true))
	{
		UNIT *unit = &(Units[unitId]);

		// skip non combat units

		if (!isCombatUnit(unitId))
			continue;

		// get weapon offense value

		int weaponOffenseValue = Weapon[unit->weapon_id].offense_value;

		// update best weapon offense value

		bestWeaponOffenseValue = std::max(bestWeaponOffenseValue, weaponOffenseValue);

	}

	return bestWeaponOffenseValue;

}

int getFactionMaxConDefenseValue(int factionId)
{
	int bestArmorDefenseValue = 0;

	for (int unitId : getFactionUnitIds(factionId, false, true))
	{
		UNIT *unit = &(Units[unitId]);

		// skip non combat units

		if (!isCombatUnit(unitId))
			continue;

		// get armor defense value

		int armorDefenseValue = Armor[unit->armor_id].defense_value;

		// update best armor defense value

		bestArmorDefenseValue = std::max(bestArmorDefenseValue, armorDefenseValue);

	}

	return bestArmorDefenseValue;

}

std::array<int, 3> getFactionFastestTriadChassisIds(int factionId)
{
	std::array<int, 3> fastestTriadChassisIds;
	std::fill(fastestTriadChassisIds.begin(), fastestTriadChassisIds.end(), -1);
	
    for (VehChassis chassisId : {CHS_NEEDLEJET, CHS_COPTER, CHS_GRAVSHIP, })
	{
		if (has_tech(Chassis[chassisId].preq_tech, factionId))
		{
			fastestTriadChassisIds.at(TRIAD_AIR) = chassisId;
		}
	}
    
    for (VehChassis chassisId : {CHS_FOIL, CHS_CRUISER, })
	{
		if (has_tech(Chassis[chassisId].preq_tech, factionId))
		{
			fastestTriadChassisIds.at(TRIAD_SEA) = chassisId;
		}
	}
    
    for (VehChassis chassisId : {CHS_INFANTRY, CHS_SPEEDER, CHS_HOVERTANK, })
	{
		if (has_tech(Chassis[chassisId].preq_tech, factionId))
		{
			fastestTriadChassisIds.at(TRIAD_AIR) = chassisId;
		}
	}
	
	return fastestTriadChassisIds;
	
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

	if (map_has_item(vehicleTile, BIT_BASE_IN_TILE))
		return false;

	// check other units

	bool threatened = false;

	for (int otherVehicleId = 0; otherVehicleId < *VehCount; otherVehicleId++)
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

bool isUnitMeleeTargetReachable(int unitId, MAP *origin, MAP *target)
{
	if (!isMeleeUnit(unitId))
		return false;
	
	for (MAP *tile : getAdjacentTiles(target))
	{
		if (isUnitDestinationReachable(unitId, origin, tile) && isUnitCanMeleeAttackFromTileToTile(unitId, tile, target))
			return true;
	}
	
	return false;
	
}

bool isVehicleMeleeTargetReachable(int vehicleId, MAP *target)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return isUnitMeleeTargetReachable(vehicle->unit_id, vehicleTile, target);
}

bool isUnitArtilleryTargetReachable(int unitId, MAP *origin, MAP *target)
{
	if (!isArtilleryUnit(unitId))
		return false;
	
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	
	for (TileInfo const *rangeTileInfo : targetTileInfo.range2NoCenterTileInfos)
	{
		if (isUnitDestinationReachable(unitId, origin, rangeTileInfo->tile) && isUnitCanArtilleryAttackFromTile(unitId, rangeTileInfo->tile))
			return true;
		
	}
	
	return false;
	
}

bool isVehicleArtilleryTargetReachable(int vehicleId, MAP *target)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return isUnitArtilleryTargetReachable(vehicle->unit_id, vehicleTile, target);
}

int getNearestEnemyBaseDistance(int baseId)
{
	BASE *base = &(Bases[baseId]);

	int nearestEnemyBaseDistance = INT_MAX;

	for (int otherBaseId = 0; otherBaseId < *BaseCount; otherBaseId++)
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
		combatBonusMultiplier /= getPercentageBonusMultiplier(Rules->combat_aaa_bonus_vs_air);
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

	for (int otherVehicleId = 0; otherVehicleId < *VehCount; otherVehicleId++)
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
	return (factionId != 0 && !is_human(factionId) && thinker_enabled(factionId) && conf.ai_useWTPAlgorithms && conf.wtp_enabled_factions[factionId]);
}

int getMaxBaseSize(int factionId)
{
	int maxBaseSize = 0;

	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		if (base->faction_id != factionId)
			continue;

		maxBaseSize = std::max(maxBaseSize, (int)base->pop_size);

	}

	return maxBaseSize;

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

/**
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
	{
		return 0.0;
	}
	
	// attacker should be melee unit
	
	if (!isMeleeUnit(attackerUnitId))
	{
		return 0.0;
	}
	
	// calculate relative strength
	
	double relativeStrength;
	
	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
		relativeStrength = getPsiCombatBaseOdds(attackerUnit->triad());
		
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
		
		if (attackerUnit->weapon_id == WPN_RESONANCE_LASER || attackerUnit->weapon_id == WPN_RESONANCE_LASER)
		{
			relativeStrength *= 1.25;
		}
		
		if (defenderUnit->armor_id == ARM_RESONANCE_3_ARMOR || defenderUnit->armor_id == ARM_RESONANCE_8_ARMOR)
		{
			relativeStrength /= 1.25;
		}
		
		// PLANET bonus
		
		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
 		if (conf.planet_defense_bonus)
		{
			relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);
		}
		
		// gear bonus
		
		if (conf.conventional_power_psi_percentage != 0)
		{
			if (attackerOffenseValue > 0)
			{
				relativeStrength *= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * attackerOffenseValue);
			}
			
			if (defenderDefenseValue > 0)
			{
				relativeStrength /= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * defenderDefenseValue);
			}
			
		}
		
	}
	else
	{
		// conventional combat
		
		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;
		
		// reactor
		
		if (!isReactorIgnoredInConventionalCombat())
		{
			relativeStrength *= (double)attackerUnit->reactor_id / (double)defenderUnit->reactor_id;
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
		relativeStrength /= getPercentageBonusMultiplier(Rules->combat_aaa_bonus_vs_air);
	}
	
	if (unit_has_ability(attackerUnitId, ABL_SOPORIFIC_GAS) && !isNativeUnit(defenderUnitId))
	{
		relativeStrength *= 1.25;
	}
	
	// faction bonuses
	
	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);
	
	// artifact and no armor probe are explicitly weak
	
	if (defenderUnit->weapon_id == WPN_ALIEN_ARTIFACT || (defenderUnit->weapon_id == WPN_PROBE_TEAM && defenderUnit->armor_id == ARM_NO_ARMOR))
	{
		relativeStrength *= 50.0;
	}
	
	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength /= 2.0;
	}
	if (defenderFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength *= 2.0;
	}
	
	return relativeStrength;
	
}

/**
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
	{
		return 0.0;
	}
	
	// both units should be artillery capable
	
	if (!isArtilleryUnit(attackerUnitId) || !isArtilleryUnit(defenderUnitId))
	{
		return 0.0;
	}
	
	// calculate relative strength
	
	double relativeStrength;
	
	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
		relativeStrength = getPsiCombatBaseOdds(attackerUnit->triad());
		
		// reactor
		// psi combat ignores reactor
		
		// PLANET bonus
		
		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
		relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);
		
		// gear bonus
		
		if (conf.conventional_power_psi_percentage != 0)
		{
			if (attackerOffenseValue > 0)
			{
				relativeStrength *= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * attackerOffenseValue);
			}
			
			if (defenderDefenseValue > 0)
			{
				relativeStrength /= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * defenderDefenseValue);
			}
			
		}
		
	}
	else
	{
		attackerOffenseValue = Weapon[attackerUnit->weapon_id].offense_value;
		defenderDefenseValue = Weapon[defenderUnit->weapon_id].offense_value;
		
		// get relative strength
		
		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;
		
		// reactor
		
		if (!isReactorIgnoredInConventionalCombat())
		{
			relativeStrength *= (double)attackerUnit->reactor_id / (double)defenderUnit->reactor_id;
		}
		
	}
	
	// ability applied regardless of combat type
	
	// faction bonuses
	
	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);
	
	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength /= 2.0;
	}
	if (defenderFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
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

/**
Calculates relative bombardment damage for units.
How many defender units can attacker destroy with single shot.
*/
double getUnitBombardmentDamage(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId)
{
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = Weapon[attackerUnit->weapon_id].offense_value;
	int defenderDefenseValue = Armor[defenderUnit->armor_id].defense_value;
	
	// illegal arguments - should not happen
	
	if (attackerOffenseValue == 0 || defenderDefenseValue == 0)
	{
		return 0.0;
	}
	
	// attacker should be artillery capable and defender should not and should be surface unit
	
	if (!isArtilleryUnit(attackerUnitId) || isArtilleryUnit(defenderUnitId) || defenderUnit->triad() == TRIAD_AIR)
	{
		return 0.0;
	}
	
	// calculate relative damage
	
	double relativeStrength = 1.0;
	
	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
		relativeStrength = getPsiCombatBaseOdds(attackerUnit->triad());
		
		// reactor
		// psi combat ignores reactor
		
		// PLANET bonus
		
		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
		relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);
		
		// gear bonus
		
		if (conf.conventional_power_psi_percentage != 0)
		{
			if (attackerOffenseValue > 0)
			{
				relativeStrength *= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * attackerOffenseValue);
			}
			
			if (defenderDefenseValue > 0)
			{
				relativeStrength /= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * defenderDefenseValue);
			}
			
		}
		
	}
	else
	{
		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;
		
		// reactor
		
		if (!isReactorIgnoredInConventionalCombat())
		{
			relativeStrength *= 1.0 / (double)defenderUnit->reactor_id;
		}
		
	}
	
	// faction bonuses
	
	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);
	
	// artifact and no armor probe are explicitly weak
	
	if (defenderUnit->weapon_id == WPN_ALIEN_ARTIFACT || (defenderUnit->weapon_id == WPN_PROBE_TEAM && defenderUnit->armor_id == ARM_NO_ARMOR))
	{
		relativeStrength *= 50.0;
	}
	
	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength /= 2.0;
	}
	if (defenderFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
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

bool isOffensiveUnit(int unitId, int factionId)
{
	UNIT *unit = getUnit(unitId);
	int offenseValue = getUnitOffenseValue(unitId);
	
	// non infantry unit is always offensive
	
	if (unit->chassis_id != CHS_INFANTRY)
		return true;
	
	// artillery unit is always offensive
	
	if (isArtilleryUnit(unitId))
		return true;
	
	// psi offense makes it offensive unit
	
	if (offenseValue < 0)
		return true;
	
	// conventional offense should be in higher range of this faction offense
	
	return offenseValue > aiData.factionInfos.at(factionId).maxConOffenseValue / 2;
	
}

bool isOffensiveVehicle(int vehicleId)
{
	return isOffensiveUnit(Vehicles[vehicleId].unit_id, Vehicles[vehicleId].faction_id);
}

double getExponentialCoefficient(double scale, double value)
{
	return exp(-value / scale);
}

/*
Estimates turns to build given item at given base.
*/
int getBaseItemBuildTime(int baseId, int item, bool countAccumulatedMinerals)
{
	BASE *base = &(Bases[baseId]);

	if (base->mineral_surplus <= 0)
		return 9999;

	int mineralCost = std::max(0, getBaseMineralCost(baseId, item) - (countAccumulatedMinerals ? base->minerals_accumulated : 0));

	return divideIntegerRoundUp(mineralCost, base->mineral_surplus);

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
	
	int nutrientCostFactor = mod_cost_factor(base->faction_id, RSC_NUTRIENT, baseId);
	int nutrientsAccumulated = base->nutrients_accumulated + base->nutrient_surplus * turns;
	int projectedPopulation = base->pop_size;
	
	while (nutrientsAccumulated >= nutrientCostFactor * (projectedPopulation + 1))
	{
		nutrientsAccumulated -= nutrientCostFactor * (projectedPopulation + 1);
		projectedPopulation++;
	}
	
	// do not go over population limit
	
	int populationLimit = getBasePopulationLimit(baseId);
	projectedPopulation = std::min(projectedPopulation, std::max((int)base->pop_size, populationLimit));
	
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

	int totalNutrients = totalNutrientRows * mod_cost_factor(base->faction_id, RSC_NUTRIENT, baseId);

	// calculate time to accumulate these nutrients

	int projectedTime = (totalNutrients + (base->nutrient_surplus - 1)) / std::max(1, base->nutrient_surplus) + (targetSize - base->pop_size);

	return projectedTime;

}

/**
Calculates nutrient + minerals + energy resource score.
*/
double getResourceScore(double nutrient, double mineral, double energy)
{
	return
		+ conf.ai_resource_score_nutrient * nutrient
		+ conf.ai_resource_score_mineral * mineral
		+ conf.ai_resource_score_energy * energy
	;
}

/**
Calculates mineral + energy resource score.
*/
double getResourceScore(double mineral, double energy)
{
	return getResourceScore(0, mineral, energy);
}

int getBaseFoundingTurn(int baseId)
{
	BASE *base = getBase(baseId);

	return base->pad_1;

}

/*
Gets base age.
*/
int getBaseAge(int baseId)
{
	BASE *base = getBase(baseId);

	return (*CurrentTurn) - base->pad_1;

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

/**
Computes vehicle additional combat multiplier on top if unit properties.
= moraleMultiplier * relativePower
*/
double getVehicleStrenghtMultiplier(int vehicleId)
{
	return getVehicleMoraleMultiplier(vehicleId) * getVehicleRelativePower(vehicleId);
}

/**
Computes vehicle additional combat multiplier on top if unit properties.
= moraleMultiplier
*/
double getVehicleBombardmentStrenghtMultiplier(int vehicleId)
{
	return getVehicleMoraleMultiplier(vehicleId);
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

	if (!isUnitCanInitiateBombardment(ownUnitId, foeUnitId))
		return 0.0;

	// own attack

	double ownBombardmentAttackEffect =
		aiData.combatEffectTable.getCombatEffect(aiFactionId, ownUnitId, foeFactionId, foeUnitId, CM_BOMBARDMENT)
		* getUnitArtilleryOffenseStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, true)
		/ getVehicleMoraleMultiplier(foeVehicleId)
	;

	double attackEffect = ownBombardmentAttackEffect;

	// foe attack (melee if they can)

	double foeMeleeAttackEffect = 0.0;

	if (isUnitCanMeleeAttackUnit(foeUnitId, ownUnitId))
	{
		foeMeleeAttackEffect =
			aiData.combatEffectTable.getCombatEffect(foeFactionId, foeUnitId, aiFactionId, ownUnitId, CM_MELEE)
			* getUnitMeleeOffenseStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, false)
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
			aiData.combatEffectTable.getCombatEffect(aiFactionId, ownUnitId, foeFactionId, foeUnitId, CM_ARTILLERY_DUEL)
			* getUnitArtilleryOffenseStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, false)
		;

		double artilleryDuelDefendEffect =
			aiData.combatEffectTable.getCombatEffect(foeFactionId, foeUnitId, aiFactionId, ownUnitId, CM_ARTILLERY_DUEL)
			* getUnitArtilleryOffenseStrengthMultipler(foeFactionId, foeUnitId, ownFactionId, ownUnitId, tile, true)
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
			aiData.combatEffectTable.getCombatEffect(aiFactionId, ownUnitId, foeFactionId, foeUnitId, CM_BOMBARDMENT)
			* getUnitArtilleryOffenseStrengthMultipler(ownFactionId, ownUnitId, foeFactionId, foeUnitId, tile, false)
		;

		return bombardmentAttackCombatEffect;

	}

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
Returns reachable locations vehicle can reach in one turn.
AI vehicles subtract current moves spent as they are currently moving.
*/
robin_hood::unordered_flat_map<int, int> getVehicleReachableLocations(int vehicleId, bool ignoreVehicles)
{
	Profiling::start("- getVehicleReachableLocations");
	
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	int movementAllowance = getVehicleMoveRate(vehicleId) - (vehicle->faction_id == aiFactionId ? vehicle->moves_spent : 0);
	
	robin_hood::unordered_flat_map<int, int> reachableLocations;
	
	switch (vehicle->triad())
	{
	case TRIAD_AIR:
		{
			reachableLocations = getAirVehicleReachableLocations(vehicleTile, movementAllowance, ignoreVehicles);
		}
		break;
	case TRIAD_SEA:
		{
			MovementType movementType = getUnitMovementType(factionId, unitId);
			reachableLocations = getSeaVehicleReachableLocations(movementType, vehicleTile, movementAllowance, ignoreVehicles);
		}
		break;
	case TRIAD_LAND:
		{
			MovementType movementType = getUnitMovementType(factionId, unitId);
			reachableLocations = getLandVehicleReachableLocations(movementType, vehicleTile, movementAllowance, ignoreVehicles);
		}
		break;
	}
	
	Profiling::stop("- getVehicleReachableLocations");
	return reachableLocations;
	
}

robin_hood::unordered_flat_map<int, int> getAirVehicleReachableLocations(MAP *origin, int movementAllowance, bool ignoreVehicles)
{
	int originIndex = origin - *MapTiles;
	
	ReachSearch reachSearch;
	
	// explore paths
	
	reachSearch.set(originIndex, movementAllowance);
	
	for (int currentMovementAllowance = movementAllowance; currentMovementAllowance > 0; currentMovementAllowance--)
	{
		if (reachSearch.layers.find(currentMovementAllowance) == reachSearch.layers.end())
			continue;
		
		for (int currentTileIndex : reachSearch.layers.at(currentMovementAllowance))
		{
			TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
			
			// iterate adjacent tiles
			
			for (Adjacent const &adjacent : currentTileInfo.adjacents)
			{
				int adjacentTileIndex = adjacent.tileInfo->index;
				
				// not blocked
				
				if (!ignoreVehicles && adjacent.tileInfo->blocked)
					continue;
				
				// hexCost
				
				int hexCost = Rules->move_rate_roads;
				
				// movementAllowance
				
				int adjacentTileMovementAllowance = currentMovementAllowance - hexCost;
				
				// update value
				
				reachSearch.set(adjacentTileIndex, adjacentTileMovementAllowance);
				
			}
			
		}
		
	}
	
	return reachSearch.values;
	
}

robin_hood::unordered_flat_map<int, int> getSeaVehicleReachableLocations(MovementType movementType, MAP *origin, int movementAllowance, bool ignoreVehicles)
{
	int originIndex = origin - *MapTiles;
	
	ReachSearch reachSearch;
	
	// explore paths
	
	reachSearch.set(originIndex, movementAllowance);
	
	for (int currentMovementAllowance = movementAllowance; currentMovementAllowance > 0; currentMovementAllowance--)
	{
		if (reachSearch.layers.find(currentMovementAllowance) == reachSearch.layers.end())
			continue;
		
		for (int currentTileIndex : reachSearch.layers.at(currentMovementAllowance))
		{
			TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
			
			// iterate adjacent tiles
			
			for (Adjacent const &adjacent : currentTileInfo.adjacents)
			{
				int adjacentTileIndex = adjacent.tileInfo->index;
				
				// not blocked
				
				if (!ignoreVehicles && adjacent.tileInfo->blocked)
					continue;
				
				// hexCost
				
				int hexCost = currentTileInfo.hexCosts.at(movementType).at(adjacent.angle).exact;
				if (hexCost == -1)
					continue;
				
				// movementAllowance
				
				int adjacentTileMovementAllowance = currentMovementAllowance - hexCost;
				
				// update value
				
				reachSearch.set(adjacentTileIndex, adjacentTileMovementAllowance);
				
			}
			
		}
		
	}
	
	return reachSearch.values;
	
}

robin_hood::unordered_flat_map<int, int> getLandVehicleReachableLocations(MovementType movementType, MAP *origin, int movementAllowance, bool ignoreVehicles)
{
	int originIndex = origin - *MapTiles;
	
	ReachSearch reachSearch;
	
	// explore paths
	
	reachSearch.set(originIndex, movementAllowance);
	
	for (int currentMovementAllowance = movementAllowance; currentMovementAllowance > 0; currentMovementAllowance--)
	{
		if (reachSearch.layers.find(currentMovementAllowance) == reachSearch.layers.end())
			continue;
		
		for (int currentTileIndex : reachSearch.layers.at(currentMovementAllowance))
		{
			TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
			
			// iterate adjacent tiles
			
			for (Adjacent const &adjacent : currentTileInfo.adjacents)
			{
				TileInfo *adjacentTileInfo = adjacent.tileInfo;
				int adjacentTileIndex = adjacentTileInfo->index;
				
				// not blocked
				
				if (!ignoreVehicles && adjacentTileInfo->blocked)
					continue;
				
				// hexCost
				
				int hexCost = currentTileInfo.hexCosts.at(movementType).at(adjacent.angle).exact;
				if (hexCost == -1)
					continue;
				
				// movementAllowance
				
				int adjacentTileMovementAllowance = currentMovementAllowance - hexCost;
				
				// update value
				
				reachSearch.set(adjacentTileIndex, adjacentTileMovementAllowance);
				
			}
			
		}
		
	}
	
	return reachSearch.values;
	
}

/**
Searches for locations vehicle can melee attack at.
*/
std::vector<AttackAction> getMeleeAttackActions(int vehicleId)
{
	std::vector<AttackAction> attackActions;
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return attackActions;
	
	robin_hood::unordered_flat_map<MAP *, AttackAction *> attackActionMap;
	
	// explore reachable locations
	
	Profiling::start("- getMeleeAttackActions - getVehicleReachableLocations");
	for (robin_hood::pair<int, int> const &reachableLocation : getVehicleReachableLocations(vehicleId))
	{
		int tileIndex = reachableLocation.first;
		int movementAllowance = reachableLocation.second;
		MAP *tile = *MapTiles + tileIndex;
		double hastyCoefficient = std::min(1.0, (double)movementAllowance / (double)Rules->move_rate_roads);
		
		// cannot attack without movementAllowance
		
		if (movementAllowance <= 0)
			continue;
		
		// explore adjacent tiles
		
		for (Adjacent const &adjacent : aiData.getTileInfo(tile).adjacents)
		{
			MAP *targetTile = adjacent.tileInfo->tile;
			
			// can melee attack
			
			if (!isVehicleCanMeleeAttackTile(vehicleId, targetTile, tile))
				continue;
			
			// add action
			
			attackActions.push_back({tile, targetTile, hastyCoefficient});
			
		}
		
	}
	Profiling::stop("- getMeleeAttackActions - getVehicleReachableLocations");
	
	return attackActions;
	
}

/**
Searches for locations vehicle can artillery attack at.
*/
std::vector<AttackAction> getArtilleryAttackActions(int vehicleId)
{
	std::vector<AttackAction> attackActions;
	
	// artillery vehicle
	
	if (!isArtilleryVehicle(vehicleId))
		return attackActions;
	
	// explore reachable locations
	
	Profiling::start("- getArtilleryAttackActions - getVehicleReachableLocations");
	for (robin_hood::pair<int, int> const &reachableLocation : getVehicleReachableLocations(vehicleId))
	{
		int tileIndex = reachableLocation.first;
		int movementAllowance = reachableLocation.second;
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		// cannot attack without movementAllowance
		
		if (movementAllowance <= 0)
			continue;
		
		// explore artillery tiles
		
		for (TileInfo const *targetTileInfo : tileInfo.range2NoCenterTileInfos)
		{
			attackActions.push_back({tile, targetTileInfo->tile, 1.0});
		}
		
	}
	Profiling::stop("- getArtilleryAttackActions - getVehicleReachableLocations");
	
	return attackActions;
	
}

/**
Searches for locations vehicle can melee attack at.
*/
robin_hood::unordered_flat_map<int, double> getMeleeAttackLocations(int vehicleId)
{
	Profiling::start("- getMeleeAttackLocations");
	
	VEH &vehicle = Vehicles[vehicleId];
	int vehicleTileIndex = getVehicleMapTileIndex(vehicleId);
	int factionId = vehicle.faction_id;
	int unitId = vehicle.unit_id;
	int movementAllowance = getVehicleMoveRate(vehicleId) - (factionId == aiFactionId ? vehicle.moves_spent : 0);
	MovementType movementType = getUnitMovementType(factionId, unitId);
	
	robin_hood::unordered_flat_map<int, int> moveLocations;
	robin_hood::unordered_flat_map<int, double> attackLocations;
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
	{
		Profiling::stop("- getMeleeAttackLocations");
		return attackLocations;
	}
	
	// not on transport
	
	if (isVehicleOnTransport(vehicleId))
	{
		Profiling::stop("- getArtilleryAttackLocations");
		return attackLocations;
	}
	
	// has moves
	
	if (movementAllowance <= 0)
	{
		Profiling::stop("- getMeleeAttackLocations");
		return attackLocations;
	}
	
	// explore reachable locations
	
	robin_hood::unordered_flat_set<int> openNodes;
	robin_hood::unordered_flat_set<int> newOpenNodes;
	
	moveLocations.emplace(vehicleTileIndex, movementAllowance);
	openNodes.insert(vehicleTileIndex);
	
	while (!openNodes.empty())
	{
		for (int currentTileIndex : openNodes)
		{
			MAP *currentTile = *MapTiles + currentTileIndex;
			TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
			int currentTileMovementAllowance = moveLocations.at(currentTileIndex);
			
			// iterate adjacent tiles
			
			for (Adjacent const &adjacent : currentTileInfo.adjacents)
			{
				TileInfo &adjacentTileInfo = *(adjacent.tileInfo);
				int adjacentTileIndex = adjacentTileInfo.index;
				MAP *adjacentTile = *MapTiles + adjacentTileIndex;
				
				// not blocked
				
				if (adjacentTileInfo.blocked)
					continue;
				
				// hexCost
				
				int hexCost = currentTileInfo.hexCosts.at(movementType).at(adjacent.angle).exact;
				if (hexCost == -1)
					continue;
				
				// new movementAllowance
				
				int adjacentTileMovementAllowance = currentTileMovementAllowance - hexCost;
				
				// update value
				
				if (moveLocations.find(adjacentTileIndex) == moveLocations.end() || adjacentTileMovementAllowance > moveLocations.at(adjacentTileIndex))
				{
					moveLocations[adjacentTileIndex] = adjacentTileMovementAllowance;
					
					if (adjacentTileMovementAllowance > 0)
					{
						newOpenNodes.insert(adjacentTileIndex);
					}
					
				}
				
				// can melee attack
				
				if (!isVehicleCanMeleeAttackTile(vehicleId, adjacentTile, currentTile))
					continue;
				
				// update attack
				
				double hasteCoefficient = currentTileMovementAllowance >= Rules->move_rate_roads ? 1.0 : (double)currentTileMovementAllowance / (double)Rules->move_rate_roads;
				if (attackLocations.find(adjacentTileIndex) == attackLocations.end() || hasteCoefficient > attackLocations.at(adjacentTileIndex))
				{
					attackLocations[adjacentTileIndex] = hasteCoefficient;
				}
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	Profiling::stop("- getMeleeAttackLocations");
	return attackLocations;
	
}

/**
Searches for locations vehicle can melee attack at.
*/
robin_hood::unordered_flat_set<int> getArtilleryAttackLocations(int vehicleId)
{
	Profiling::start("- getArtilleryAttackLocations");
	
	VEH &vehicle = Vehicles[vehicleId];
	int vehicleTileIndex = getVehicleMapTileIndex(vehicleId);
	int factionId = vehicle.faction_id;
	int unitId = vehicle.unit_id;
	int movementAllowance = getVehicleMoveRate(vehicleId) - (factionId == aiFactionId ? vehicle.moves_spent : 0);
	MovementType movementType = getUnitMovementType(factionId, unitId);
	
	robin_hood::unordered_flat_map<int, int> moveLocations;
	robin_hood::unordered_flat_set<int> attackLocations;
	
	// artillery vehicle
	
	if (!isArtilleryVehicle(vehicleId))
	{
		Profiling::stop("- getArtilleryAttackLocations");
		return attackLocations;
	}
	
	// not on transport
	
	if (isVehicleOnTransport(vehicleId))
	{
		Profiling::stop("- getArtilleryAttackLocations");
		return attackLocations;
	}
	
	// has moves
	
	if (movementAllowance <= 0)
	{
		Profiling::stop("- getArtilleryAttackLocations");
		return attackLocations;
	}
	
	// explore reachable locations
	
	robin_hood::unordered_flat_set<int> openNodes;
	robin_hood::unordered_flat_set<int> newOpenNodes;
	
	moveLocations.emplace(vehicleTileIndex, movementAllowance);
	openNodes.insert(vehicleTileIndex);
	
	while (!openNodes.empty())
	{
		for (int currentTileIndex : openNodes)
		{
			MAP *currentTile = *MapTiles + currentTileIndex;
			TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
			int currentTileMovementAllowance = moveLocations.at(currentTileIndex);
			
			// update attack
			
			for (MAP *artilleryRangeTile : getRangeTiles(currentTile, Rules->artillery_max_rng, false))
			{
				int artilleryRangeTileIndex = artilleryRangeTile - *MapTiles;
				attackLocations.insert(artilleryRangeTileIndex);
			}
			
			// iterate adjacent tiles
			
			for (Adjacent const &adjacent : currentTileInfo.adjacents)
			{
				TileInfo &adjacentTileInfo = *(adjacent.tileInfo);
				int adjacentTileIndex = adjacentTileInfo.index;
				
				// not blocked
				
				if (adjacentTileInfo.blocked)
					continue;
				
				// hexCost
				
				int hexCost = currentTileInfo.hexCosts.at(movementType).at(adjacent.angle).exact;
				if (hexCost == -1)
					continue;
				
				// new movementAllowance
				
				int adjacentTileMovementAllowance = currentTileMovementAllowance - hexCost;
				
				// update value
				
				if (moveLocations.find(adjacentTileIndex) == moveLocations.end() || adjacentTileMovementAllowance > moveLocations.at(adjacentTileIndex))
				{
					moveLocations[adjacentTileIndex] = adjacentTileMovementAllowance;
					
					if (adjacentTileMovementAllowance > 0)
					{
						newOpenNodes.insert(adjacentTileIndex);
					}
					
				}
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	Profiling::stop("- getArtilleryAttackLocations");
	return attackLocations;
	
}

bool isVehicleAllowedMove(int vehicleId, MAP *from, MAP *to)
{
	VEH *vehicle = getVehicle(vehicleId);
	bool toTileBlocked = isBlocked(to);
	bool zoc = isZoc(from, to);

	if (toTileBlocked)
		return false;

	if (vehicle->triad() == TRIAD_LAND && !is_ocean(from) && !is_ocean(to) && zoc)
		return false;

	return true;

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

		int speed = getUnitSpeed(factionId, unitId);

		// update best

		if (speed > bestSeaCombatSpeed)
		{
			bestSeaCombatSpeed = speed;
		}

	}

	return bestSeaCombatSpeed;

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

		int speed = getUnitSpeed(factionId, unitId);

		// update best

		if (speed > bestSeaTransportSpeed)
		{
			bestSeaTransportSpeed = speed;
		}

	}

	return bestSeaTransportSpeed;

}

/*
Disband unit to reduce support burden.
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

		robin_hood::unordered_flat_map<int, std::vector<int>> outsideVehicles;

		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
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
				debug("\t\toutsideVehicles: %s\n", getLocationString(vehicleTile).c_str());
			}

		}

		for (robin_hood::pair<int, std::vector<int>> &outsideVehicleEntry : outsideVehicles)
		{
			std::vector<int> &baseOutsideVehicles = outsideVehicleEntry.second;

			for (unsigned int i = 0; i < baseOutsideVehicles.size() - warWearinessLimit; i++)
			{
				int vehicleId = baseOutsideVehicles.at(i);

				killVehicleIds.push_back(vehicleId);
				debug
				(
					"\t\t[%4d] %s : %-25s\n"
					, vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), getBase(getVehicle(vehicleId)->home_base_id)->name
				);

			}

		}

	}

	std::sort(killVehicleIds.begin(), killVehicleIds.end(), std::greater<int>());

	for (int vehicleId : killVehicleIds)
	{
		mod_veh_kill(vehicleId);
	}

}

/*
Disband unneeded vehicles.
*/
void disbandUnneededVehicles()
{
	debug("disbandUnneededVehicles - %s\n", MFactions[aiFactionId].noun_faction);

	// ship without task in internal sea

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();

		// ours

		if (vehicle->faction_id != aiFactionId)
			continue;

		// combat

		if (!isCombatVehicle(vehicleId))
			continue;

		// ship

		if (triad != TRIAD_SEA)
			continue;

		// does not have task

		if (hasTask(vehicleId))
			continue;

		// internal sea

		if (isSharedSea(vehicleTile))
			continue;

		// disband

		mod_veh_kill(vehicleId);

	}

}

MAP *getVehicleArtilleryAttackPosition(int vehicleId, MAP *target)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int factionId = vehicle->faction_id;
	int triad = vehicle->triad();
	bool native = isNativeVehicle(vehicleId);
	TileInfo &targetTileInfo = aiData.getTileInfo(target);

	MAP *closestAttackPosition = nullptr;
	int closestAttackPositionRange = INT_MAX;
	int closestAttackPositionMovementCost = 0;
	
	// air units are not artillery capable
	
	if (triad == TRIAD_AIR)
		return nullptr;
	
	for (TileInfo const *rangeTileInfo : targetTileInfo.range2NoCenterTileInfos)
	{
		MAP *tile = rangeTileInfo->tile;
		
		// should be not blocked
		
		if (isBlocked(tile))
			continue;
		
		// destination is reachable
		
		switch (triad)
		{
		case TRIAD_SEA:
			if (!isSameSeaCluster(vehicleTile, tile))
				continue;
			break;
		case TRIAD_LAND:
			if (!isSameLandTransportedCluster(vehicleTile, tile))
				continue;
			break;
		}
		
		// land artillery can attack from land only
		
		if (triad == TRIAD_LAND && is_ocean(tile))
			continue;
		
		// range
		
		int range = getRange(vehicleTile, tile);
		
		// movement cost
		
		int movementCost = Rules->move_rate_roads;
		
		switch (triad)
		{
		case TRIAD_SEA:
			if
			(
				isBaseAt(tile)
				||
				!map_has_item(tile, BIT_FUNGUS)
				||
				(native || isFactionHasProject(factionId, FAC_XENOEMPATHY_DOME))
			)
			{
				movementCost = Rules->move_rate_roads;
			}
			else
			{
				movementCost = Rules->move_rate_roads * 3;
			}
			break;
			
		case TRIAD_LAND:
			if
			(
				isBaseAt(tile)
				||
				map_has_item(tile, BIT_MAGTUBE)
			)
			{
				movementCost = getMovementRateAlongTube();
			}
			else if
			(
				map_has_item(tile, BIT_ROAD)
				||
				(native || (isFactionHasProject(factionId, FAC_XENOEMPATHY_DOME) && map_has_item(tile, BIT_FUNGUS)))
			)
			{
				movementCost = getMovementRateAlongRoad();
			}
			else if
			(
				!map_has_item(tile, BIT_FUNGUS)
			)
			{
				movementCost = Rules->move_rate_roads;
			}
			else
			{
				movementCost = Rules->move_rate_roads * 3;
			}
			break;
			
		}
		
		if
		(
			range < closestAttackPositionRange
			||
			(range == closestAttackPositionRange && movementCost < closestAttackPositionMovementCost)
		)
		{
			closestAttackPosition = tile;
			closestAttackPositionRange = range;
			closestAttackPositionMovementCost = movementCost;
		}
		
	}
	
	return closestAttackPosition;
	
}

/**
Checks if unit can capture given base in general.
*/
bool isUnitCanCaptureBase(int unitId, MAP *baseTile)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	bool baseTileOcean = is_ocean(baseTile);
	
	// vehicle should generally be able to capture base
	
	switch (unit->chassis_id)
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

/**
Checks if unit can capture given base from given position.
*/
bool isUnitCanCaptureBase(int unitId, MAP *origin, MAP *baseTile)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	bool baseTileOcean = is_ocean(baseTile);
	
	// vehicle should generally be able to capture base
	
	switch (unit->chassis_id)
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
		
		// sea unit can move within same sea cluster
		
		if (!isMeleeAttackableFromSeaCluster(origin, baseTile))
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

/**
Checks if vehicle can capture given base.
*/
bool isVehicleCanCaptureBase(int vehicleId, MAP *baseTile)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	return isUnitCanCaptureBase(vehicle->unit_id, vehicleTile, baseTile);
	
}

/**
Benefit of destroying an enemy vehicle.
Measured in average base gain.
*/
double getDestroyingEnemyVehicleGain(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	TileInfo &vehicleTileInfo = aiData.getTileInfo(vehicleTile);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	
	double attackGain = 0.0;
	
	if (isArtifactVehicle(vehicleId))
	{
		attackGain = getGainBonus(2 * Rules->mineral_cost_multi * Units[BSC_ALIEN_ARTIFACT].cost / 2);
	}
	else if (isHostile(aiFactionId, factionId))
	{
		if (isCombatVehicle(vehicleId) || vehicle->unit_id == BSC_FUNGAL_TOWER)
		{
			IdIntValue nearestBaseRange = vehicleTileInfo.nearestBaseRanges.at(vehicle->triad());
			
			if (nearestBaseRange.id >= 0 && nearestBaseRange.value < INT_MAX)
			{
				if (factionId == 0)
				{
					switch (unitId)
					{
					case BSC_MIND_WORMS:
						attackGain = getGainBonus(conf.ai_combat_attack_bonus_alien_mind_worms);
						break;
						
					case BSC_SPORE_LAUNCHER:
						attackGain = getGainBonus(conf.ai_combat_attack_bonus_alien_spore_launcher);
						break;
						
					case BSC_FUNGAL_TOWER:
						attackGain = getGainBonus(conf.ai_combat_attack_bonus_alien_fungal_tower);
						// extra gain for damaged tower
						attackGain *= (1.0 + 10.0 * getVehicleRelativeDamage(vehicleId));
						break;
					
					default:
						attackGain = 0.0;
						
					}
					
				}
				else
				{
					BaseInfo &baseInfo = aiData.getBaseInfo(nearestBaseRange.id);
					
					double baseAttackGain = 0.0;
					if (baseInfo.combatData.requiredEffect > 0.0)
					{
						double baseGain = baseInfo.gain;
						double combatEffect = baseInfo.combatData.getVehicleEffect(vehicleId);
						double baseGainProportion = combatEffect / baseInfo.combatData.requiredEffect;
						double combatEffectCoefficient = getCombatEffectCoefficient(combatEffect);
						baseAttackGain = baseGain * baseGainProportion * combatEffectCoefficient;
					}
					
					double yieldAttackGain = getGainBonus(conf.ai_combat_attack_bonus_hostile);
					
					attackGain = std::max(yieldAttackGain, baseAttackGain);
					
				}
				
				// baseProximityCoefficient
				
				double baseProximityCoefficient = getExponentialCoefficient(conf.ai_combat_field_attack_base_proximity_scale, nearestBaseRange.value - 1);
				attackGain *= baseProximityCoefficient;
				
			}
			
		}
		else if (isProbeVehicle(vehicleId))
		{
			attackGain = 2.0 * getGainBonus(conf.ai_combat_attack_bonus_hostile);
		}
		else if (isTransportVehicle(vehicleId))
		{
			attackGain = 2.0 * getGainBonus(conf.ai_combat_attack_bonus_hostile);
		}
		else if (isColonyVehicle(vehicleId))
		{
			attackGain = aiFactionInfo->averageBaseGain;
		}
		else
		{
			attackGain = 0.5 * getGainBonus(conf.ai_combat_attack_bonus_hostile);
		}
		
	}
	
	return attackGain;
	
}

/**
Estimates unit combined cost and support.
- mineral row = 10
- combat unit lives 40 turns
*/
int getCombatUnitTrueCost(int unitId)
{
	return 10 * Units[unitId].cost + 40 * getUnitSupport(unitId);
}

int getCombatVehicleTrueCost(int vehicleId)
{
	return getCombatUnitTrueCost(Vehicles[vehicleId].unit_id);
}

/**
Computes economical effect by given standard parameters.
delay:			how far from now the effect begins.
bonus:			one time economical bonus.
income:			income per turn.
incomeGrowth:	income growth per turn.
incomeGrowth2:	income growth growth per turn.
*/
double getGain(double bonus, double income, double incomeGrowth, double incomeGrowth2)
{
	double scale = aiData.developmentScale;
	return (bonus + income * scale + incomeGrowth * scale * scale + incomeGrowth2 * 2 * scale * scale * scale) / scale;
}

/**
Computes delayed gain.
gain:			economical gain without delay.
delay:			how far from now the effect begins.
*/
double getGainDelay(double gain, double delay)
{
	double scale = aiData.developmentScale;
	return exp(- delay / scale) * gain;
}

/**
Computes time interval gain.
gain:			economical gain without delay and lasting forever.
beginTime:		how far from now the effect begins.
endTime:		how far from now the effect ends.
*/
double getGainTimeInterval(double gain, double beginTime, double endTime)
{
	double scale = aiData.developmentScale;
	return (exp(- beginTime / scale) - exp(- endTime / scale)) * gain;
}

/**
Computes repetitive gain.
probability - the probability gain will be repeated next time
period - repetition period
*/
double getGainRepetion(double gain, double probability, double period)
{
	period = std::max(1.0, period);
	double scale = aiData.developmentScale;
	return exp(- period / scale) / (1.0 - probability * exp(- period / scale)) * gain;
}

/**
Computes one time bonus gain.
*/
double getGainBonus(double bonus)
{
	double scale = aiData.developmentScale;
	return bonus / scale;
}

/**
Computes steady income gain.
*/
double getGainIncome(double income)
{
	return income;
}

/**
Computes linear income growth gain.
*/
double getGainIncomeGrowth(double incomeGrowth)
{
	double scale = aiData.developmentScale;
	return incomeGrowth * scale;
}

/**
Computes quadradic income growth gain.
*/
double getGainIncomeGrowth2(double incomeGrowth)
{
	double scale = aiData.developmentScale;
	return incomeGrowth * 2 * scale * scale;
}

/**
Computes base summary resource score.
*/
double getBaseIncome(int baseId, bool withNutrients)
{
	BASE *base = getBase(baseId);
	
	double income;
	
	if (!withNutrients)
	{
		income = getResourceScore(base->mineral_intake_2, base->economy_total + base->labs_total);
	}
	else
	{
		income = getResourceScore(base->nutrient_surplus, base->mineral_intake_2, base->economy_total + base->labs_total);
	}
	
	return income;
	
}

/**
Computes average base citizen income.
*/
double getBaseCitizenIncome(int baseId)
{
	BASE *base = getBase(baseId);
	return getBaseIncome(baseId) / (double)base->pop_size;
}

/**
Mean base income.
*/
double getMeanBaseIncome(double age)
{
	return
		getResourceScore
		(
			conf.ai_base_mineral_intake2_a * age + conf.ai_base_mineral_intake2_b,
			conf.ai_base_budget_intake2_a * age * age + conf.ai_base_budget_intake2_b * age + conf.ai_base_budget_intake2_c
		)
	;
}

/**
Estimates average new base gain from current age to eternity.
*/
double getMeanBaseGain(double age)
{
	return
		// mineral intake 2
		+ getGain
			(
				0.0,
				getResourceScore(conf.ai_base_mineral_intake2_a * age + conf.ai_base_mineral_intake2_b, 0.0),
				getResourceScore(conf.ai_base_mineral_intake2_a, 0.0),
				0.0
			)
		// budget intake 2
		+ getGain
			(
				0.0,
				getResourceScore(0.0, conf.ai_base_budget_intake2_a * age * age + conf.ai_base_budget_intake2_b * age + conf.ai_base_budget_intake2_c),
				getResourceScore(0.0, conf.ai_base_budget_intake2_a * 2 * age + conf.ai_base_budget_intake2_b),
				getResourceScore(0.0, conf.ai_base_budget_intake2_a)
			)
	;
}

/**
Estimates expected new base gain from inception to eternity.
*/
double getNewBaseGain()
{
	return getMeanBaseGain(0);
}

/**
Estimates expected colony gain accounting for travel time and colony maintenance.
*/
double getLandColonyGain()
{
	return getGainDelay(getNewBaseGain(), AVERAGE_LAND_COLONY_TRAVEL_TIME) + getGainTimeInterval(getGainIncome(getResourceScore(-1.0, 0.0)), 0.0, AVERAGE_LAND_COLONY_TRAVEL_TIME);
}

/**
Computes average base income.
*/
double getAverageBaseIncome()
{
	if (aiData.baseIds.size() == 0)
		return 0.0;
	
	double sumBaseIncome = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		sumBaseIncome += getBaseIncome(baseId);
	}
	
	return sumBaseIncome / (double)aiData.baseIds.size();
	
}

/**
Computes expected base gain adjusting for current base income.
*/
double getBaseGain(int popSize, int nutrientCostFactor, Resource baseIntake2)
{
	// gain
	
	double income = getResourceScore(baseIntake2.mineral, baseIntake2.energy);
	double incomeGain = getGainIncome(income);
	
	double popualtionGrowth = baseIntake2.nutrient / (double)(nutrientCostFactor * (1 + popSize));
	double incomeGrowth = aiData.averageCitizenResourceIncome * popualtionGrowth;
	double incomeGrowthGain = getGainIncomeGrowth(incomeGrowth);
	
	double gain =
		+ incomeGain
		+ incomeGrowthGain
	;
	
//	if (DEBUG)
//	{
//		debug
//		(
//			"getBaseGain(popSize=%2d, nutrientCostFactor=%2d, baseIntake2=%f,%f,%f)\n"
//			, popSize, nutrientCostFactor, baseIntake2.nutrient, baseIntake2.mineral, baseIntake2.energy
//		);
//		
//		debug
//		(
//			"\tincome=%5.2f"
//			" incomeGain=%5.2f"
//			" averageCitizenResourceIncome=%5.2f"
//			" popualtionGrowth=%5.2f"
//			" incomeGrowth=%5.2f"
//			" incomeGrowthGain=%5.2f"
//			" gain=%5.2f"
//			"\n"
//			, income
//			, incomeGain
//			, aiData.averageCitizenResourceIncome
//			, popualtionGrowth
//			, incomeGrowth
//			, incomeGrowthGain
//			, gain
//		);
//		
//	}
	
	return gain;
	
}

double getBaseGain(int baseId, Resource baseIntake2)
{
	BASE *base = getBase(baseId);
	int nutrientCostFactor = mod_cost_factor(aiFactionId, RSC_NUTRIENT, baseId);
	
	return getBaseGain(base->pop_size, nutrientCostFactor, baseIntake2);
	
}

/**
Computes expected base gain adjusting for current base income.
*/
double getBaseGain(int baseId)
{
	BASE *base = getBase(baseId);
	int nutrientCostFactor = mod_cost_factor(aiFactionId, RSC_NUTRIENT, baseId);
	Resource baseIntake2 = getBaseResourceIntake2(baseId);
	
	return getBaseGain(base->pop_size, nutrientCostFactor, baseIntake2);
	
}

/**
Computes base value for keeping or capturing.
Base gain + SP values.
*/
double getBaseValue(int baseId)
{
	// base gain
	
	double baseGain = getBaseGain(baseId);
	
	// SP value
	
	double spValue = 0.0;
	
	for (int spFacilityId = SP_ID_First; spFacilityId <= SP_ID_Last; spFacilityId++)
	{
		if (isBaseHasFacility(baseId, spFacilityId))
		{
			double spCost = Rules->mineral_cost_multi * getFacility(spFacilityId)->cost;
			spValue += getGainBonus(spCost);
		}
		
	}
	
	return baseGain + spValue;
	
}

/**
Computes expected base improvement gain adjusting for current base income.
Compares the mean base gain adjusted for current age with same but with improved growth.
*/
double getBaseImprovementGain(int baseId, Resource oldBaseIntake2, Resource newBaseIntake2)
{
	double oldGain = getBaseGain(baseId, oldBaseIntake2);
	double newGain = getBaseGain(baseId, newBaseIntake2);
	
	double improvementGain = newGain - oldGain;
	
//	// extra adjustment for lack of minerals
//	
//	if (oldBaseIntake2.nutrient > 0.0)
//	{
//		double mineralThreshold = conf.ai_terraforming_baseMineralThresholdRatio * oldBaseIntake2.nutrient;
//		
//		double oldMineralExtraScore = 0.5 * (conf.ai_terraforming_baseMineralCostMultiplier - 1.0) * (2.0 - std::min(mineralThreshold, oldBaseIntake2.mineral) / mineralThreshold) * std::min(mineralThreshold, oldBaseIntake2.mineral);
//		double newMineralExtraScore = 0.5 * (conf.ai_terraforming_baseMineralCostMultiplier - 1.0) * (2.0 - std::min(mineralThreshold, newBaseIntake2.mineral) / mineralThreshold) * std::min(mineralThreshold, newBaseIntake2.mineral);
//		
//		improvementGain += newMineralExtraScore - oldMineralExtraScore;
//		
//	}
//	
	return improvementGain;
	
}

COMBAT_TYPE getWeaponType(int vehicleId)
{
	return (getVehicleOffenseValue(vehicleId) < 0 ? CT_PSI : CT_CON);
}

COMBAT_TYPE getArmorType(int vehicleId)
{
	return (getVehicleDefenseValue(vehicleId) < 0 ? CT_PSI : CT_CON);
}

CombatStrength getMeleeAttackCombatStrength(int vehicleId)
{
	COMBAT_TYPE attackerCOMBAT_TYPE = getWeaponType(vehicleId);
	
	CombatStrength combatStrength;
	
	// melee capable vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return combatStrength;
		
	switch (attackerCOMBAT_TYPE)
	{
	case CT_PSI:
		
		{
			double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId);
			
			// psi combat agains any armor
			
			combatStrength.values.at(CT_PSI).at(CT_PSI) = psiOffenseStrength;
			combatStrength.values.at(CT_PSI).at(CT_CON) = psiOffenseStrength;
			
		}
		
		break;
		
		
	case CT_CON:
		
		{
			double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId);
			double conOffenseStrength = getVehicleConOffenseStrength(vehicleId);
			
			// psi combat agains psi armor
			// con combat agains con armor
			
			combatStrength.values.at(CT_CON).at(CT_PSI) = psiOffenseStrength;
			combatStrength.values.at(CT_CON).at(CT_CON) = conOffenseStrength;
			
		}
		
		break;
		
	}
	
	return combatStrength;
	
}

/**
Checks if vehicle can use artillery.
*/
bool isVehicleCanArtilleryAttack(int vehicleId)
{
	// artillery
	
	if (!isArtilleryVehicle(vehicleId))
		return false;
	
	// not on transport
	
	if (isVehicleOnTransport(vehicleId))
		return false;
	
	// all checks passed
	
	return true;
	
}

/**
Engine sometimes forgets to assign vehicle to transport.
This function ensures land units at sea are assigned to transport.
*/
void assignVehiclesToTransports()
{
	debug("assignVehiclesToTransports - %s\n", aiMFaction->noun_faction);
	
	std::vector<int> orphanPassengerIds;
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// player faction
		
		if (vehicle->faction_id != aiFactionId)
			continue;
		
		// land unit
		
		if (vehicle->triad() != TRIAD_LAND)
			continue;
		
		// at sea and not at base
		
		if (!(is_ocean(vehicleTile) && !map_has_item(vehicleTile, BIT_BASE_IN_TILE)))
			continue;
		
		// not assigned to transport
		
		if (getVehicleTransportId(vehicleId) != -1)
			continue;
		
		debug("\t[%4d] %s\n", vehicleId, getLocationString(vehicleTile).c_str());
		
		// find available sea transport at this location and assign vehicle to it
		
		int transportId = -1;
		
		for (int stackVechileId : getStackVehicles(vehicleId))
		{
			// sea transport
			
			if (!isSeaTransportVehicle(stackVechileId))
				continue;
			
			// has capacity
			
			if (!isTransportHasCapacity(stackVechileId))
				continue;
			
			// select available transport
			
			transportId = stackVechileId;
			break;
			
		}
		
		// assign vehicle to selected transport or kill vehicle if no available transport
		
		if (transportId != -1)
		{
			board(vehicleId, transportId);
			debug("\t\tassigned to transport: [%4d]\n", transportId);
		}
		else
		{
			orphanPassengerIds.push_back(vehicleId);
			debug("\t\tkilled\n");
		}
		
	}
	
	std::sort(orphanPassengerIds.begin(), orphanPassengerIds.end(), std::greater<int>());
	
	for (int orphanPassengerId : orphanPassengerIds)
	{
		killVehicle(orphanPassengerId);
	}
	
}

/**
Checks if unit can attack another unit in the field.
*/
bool isUnitCanMeleeAttackUnit(int attackerUnitId, int defenderUnitId)
{
	UNIT *attackerUnit = getUnit(attackerUnitId);
	UNIT *defenderUnit = getUnit(defenderUnitId);
	int attackerTriad = attackerUnit->triad();
	int defenderTriad = defenderUnit->triad();
	
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(attackerUnitId))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	
	if (defenderUnit->chassis_id == CHS_NEEDLEJET && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// different surface triads cannot attack each other
	
	if ((attackerTriad == TRIAD_LAND && defenderTriad == TRIAD_SEA) || (attackerTriad == TRIAD_SEA && defenderTriad == TRIAD_LAND))
		return false;
	
	// all checks passed
	
	return true;
	
}

bool isUnitCanMeleeAttackFromTileToTile(int unitId, MAP *position, MAP *target)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	TileInfo &positionTileInfo = aiData.getTileInfo(position);
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(unitId))
		return false;
	
	// position should not be blocked
	
	if (positionTileInfo.blocked)
		return false;
	
	// cannot attack needlejet in flight without air superiority
	
	if (targetTileInfo.needlejetInFlight && !isUnitHasAbility(unitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// check movement
	
	switch (triad)
	{
	case TRIAD_AIR:
		
		// air unit can attack from any realm to any realm
		
		return true;
		
		break;
		
	case TRIAD_SEA:
		
		if (unitId == BSC_SEALURK)
		{
			// sealurk can attack from sea to sea or land
			
			if (positionTileInfo.ocean)
				return true;
			else
				return false;
			
		}
		else
		{
			// other sea units can attack from sea to sea
				
			if (positionTileInfo.ocean && targetTileInfo.ocean)
				return true;
			else
				return false;
			
		}
		
		break;
		
	case TRIAD_LAND:
		
		if (isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
		{
			// amphibious unit can attack from sea or land to sea base or land
			// the only tile they cannot attack is open sea
			
			if (targetTileInfo.land || (targetTileInfo.ocean && map_has_item(target, BIT_BASE_IN_TILE)))
				return true;
			else
				return false;
			
		}
		else
		{
			// other land units can attack from land to land
			
			if (positionTileInfo.land && targetTileInfo.land)
				return true;
			else
				return false;
			
		}
		
		break;
		
	}
	
	// all checks passed
	
	return true;
	
}

bool isUnitCanArtilleryAttackFromTile(int unitId, MAP *position)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	TileInfo &positionTileInfo = aiData.getTileInfo(position);
	
	// non artillery unit cannot artillery attack
	
	if (!isArtilleryUnit(unitId))
		return false;
	
	// position should not be blocked
	
	if (positionTileInfo.blocked)
		return false;
	
	// check movement
	
	switch (triad)
	{
	case TRIAD_AIR:
		
		// air unit can not artillery attack
		
		return false;
		
		break;
		
	case TRIAD_SEA:
		
		// sea units can attack from sea
			
		if (positionTileInfo.ocean)
			return true;
		else
			return false;
		
		break;
		
	case TRIAD_LAND:
		
		// land units can attack from land
		
		if (positionTileInfo.land)
			return true;
		else
			return false;
		
		break;
		
	}
	
	// all checks passed
	
	return true;
	
}

/**
Checks if unit can generally attack enemy at tile.
Tile may have base/bunker/airbase.
*/
bool isUnitCanMeleeAttackTile(int unitId, MAP *target, MAP *position)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(unitId))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	
	if (targetTileInfo.needlejetInFlight && !isUnitHasAbility(unitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// check movement
	
	switch (triad)
	{
	case TRIAD_SEA:
		
		if (unitId == BSC_SEALURK)
		{
			// sealurk can attack sea and land
		}
		else
		{
			// other sea units cannot attack land
				
			if (!targetTileInfo.ocean)
				return false;
			
		}
		
		break;
		
	case TRIAD_LAND:
		
		if (isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
		{
			// amphibious unit cannot attack sea without a base
			
			if (targetTileInfo.ocean && !map_has_item(target, BIT_BASE_IN_TILE))
				return false;
			
		}
		else
		{
			// other land units cannot attack sea
			
			if (targetTileInfo.ocean)
				return false;
			
		}
		
		break;
		
	}
	
	// additionally check position tile if provided
	
	if (position != nullptr)
	{
		TileInfo &positionTileInfo = aiData.getTileInfo(position);
		
		switch (triad)
		{
			case TRIAD_SEA:
				
				// sea unit cannot attack from land without a base
				
				if (!positionTileInfo.ocean && !map_has_item(position, BIT_BASE_IN_TILE))
					return false;
				
				break;
				
			case TRIAD_LAND:
				
				if (isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
				{
					// amphibious unit can attack from land and sea
				}
				else
				{
					// other land units cannot attack from sea
					
					if (positionTileInfo.ocean)
						return false;
					
				}
				
				break;
				
			}
			
		}
		
	// all checks passed
	
	return true;
	
}

bool isVehicleCanMeleeAttackTile(int vehicleId, MAP *target, MAP *position)
{
	return isUnitCanMeleeAttackTile(Vehicles[vehicleId].unit_id, target, position);
}

double getBaseExtraWorkerGain(int baseId)
{
	// basic base gain
	
	Resource oldBaseIntake2 = getBaseResourceIntake2(baseId);
	double oldBaseGain = getBaseGain(baseId, oldBaseIntake2);
	
	// extra worker base gain
	
	Resource newBaseIntake2 =
		Resource::combine
		(
			oldBaseIntake2,
			{
				aiData.averageWorkerNutrientIntake2,
				aiData.averageWorkerMineralIntake2,
				aiData.averageWorkerEnergyIntake2,
 			}
		)
	;
	double newBaseGain = getBaseGain(baseId, newBaseIntake2);
	
	// extra worker gain
	
	double extraWorkerGain = std::max(0.0, newBaseGain - oldBaseGain);
	return extraWorkerGain;
	
}

/**
Computes actual offense effect vehicle against vehicle on specific location.
One of the vehicles should belong to AI faction.
*/
double getOffenseEffect(int attackerVehicleId, int defenderVehicleId, COMBAT_MODE combatMode, MAP *tile)
{
	VEH *attackerVehicle = getVehicle(attackerVehicleId);
	VEH *defenderVehicle = getVehicle(defenderVehicleId);
	
	assert(attackerVehicle->faction_id == aiFactionId || defenderVehicle->faction_id == aiFactionId);
	
	double effect = aiData.combatEffectTable.getCombatEffect(attackerVehicle->faction_id, attackerVehicle->unit_id, defenderVehicle->faction_id, defenderVehicle->unit_id, combatMode);
	
	switch (combatMode)
	{
	case CM_MELEE:
		{
			double offenseStrengthMultiplier = getUnitMeleeOffenseStrengthMultipler(attackerVehicle->faction_id, attackerVehicle->unit_id, defenderVehicle->faction_id, defenderVehicle->unit_id, tile, true);
			double attackerStrengthMultiplier = getVehicleStrenghtMultiplier(attackerVehicleId);
			double defenderStrengthMultiplier = getVehicleStrenghtMultiplier(defenderVehicleId);
			
			effect *= offenseStrengthMultiplier * attackerStrengthMultiplier / defenderStrengthMultiplier;
			
		}
		break;
		
	case CM_ARTILLERY_DUEL:
		{
			double offenseStrengthMultiplier = getUnitArtilleryOffenseStrengthMultipler(attackerVehicle->faction_id, attackerVehicle->unit_id, defenderVehicle->faction_id, defenderVehicle->unit_id, tile, true);
			double attackerStrengthMultiplier = getVehicleStrenghtMultiplier(attackerVehicleId);
			double defenderStrengthMultiplier = getVehicleStrenghtMultiplier(defenderVehicleId);
			
			effect *= offenseStrengthMultiplier * attackerStrengthMultiplier / defenderStrengthMultiplier;
			
		}
		break;
		
	case CM_BOMBARDMENT:
		{
			double offenseStrengthMultiplier = getUnitArtilleryOffenseStrengthMultipler(attackerVehicle->faction_id, attackerVehicle->unit_id, defenderVehicle->faction_id, defenderVehicle->unit_id, tile, true);
			double attackerStrengthMultiplier = getVehicleBombardmentStrenghtMultiplier(attackerVehicleId);
			double defenderStrengthMultiplier = getVehicleBombardmentStrenghtMultiplier(defenderVehicleId);
			
			effect *= offenseStrengthMultiplier * attackerStrengthMultiplier / defenderStrengthMultiplier;
			
		}
		break;
		
	}
	
	return effect;
	
}

/**
Computes generic unit assault effect.
Assailant tries to capture tile.
Protector tries to protect tile.
One of the party should be an AI unit to utilize combat effect table.
*/
AssaultEffect computeAssaultEffect(int assailantFactionId, int assailantUnitId, int protectorFactionId, int protectorUnitId)
{
	assert(assailantFactionId >= 0 && assailantFactionId < MaxPlayerNum);
	assert(protectorFactionId >= 0 && protectorFactionId < MaxPlayerNum);
	assert(protectorFactionId != assailantFactionId);
	assert(protectorFactionId == aiFactionId || assailantFactionId == aiFactionId);
	assert(assailantUnitId >= 0 && assailantUnitId < MaxPlayerNum * MaxProtoFactionNum);
	assert(protectorUnitId >= 0 && protectorUnitId < MaxPlayerNum * MaxProtoFactionNum);
	
	int assailantUnitSpeed = getUnitSpeed(assailantFactionId, assailantUnitId);
	int protectorUnitSpeed = getUnitSpeed(protectorFactionId, protectorUnitId);
	
	bool assailantMelee = isUnitCanMeleeAttackUnitAtBase(assailantUnitId, protectorUnitId);
	bool protectorMelee = isUnitCanMeleeAttackUnitFromBase(protectorUnitId, assailantUnitId);
	bool assailantArtillery = isUnitCanArtilleryAttackUnitAtBase(assailantUnitId, protectorUnitId);
	bool protectorArtillery = isUnitCanArtilleryAttackUnitFromBase(protectorUnitId, assailantUnitId);
	
	double assailantMeleeEffect = assailantMelee ? aiData.combatEffectTable.getCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, CM_MELEE) : 0.0;
	double protectorMeleeEffect = protectorMelee ? aiData.combatEffectTable.getCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, CM_MELEE) : 0.0;
	double assailantArtilleryDuelEffect = assailantArtillery && protectorArtillery ? aiData.combatEffectTable.getCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, CM_ARTILLERY_DUEL) : 0.0;
	double protectorArtilleryDuelEffect = protectorArtillery && assailantArtillery ? aiData.combatEffectTable.getCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, CM_ARTILLERY_DUEL) : 0.0;
	double assailantBombardmentEffect = assailantArtillery && !protectorArtillery ? aiData.combatEffectTable.getCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, CM_BOMBARDMENT) : 0.0;
	double protectorBombardmentEffect = protectorArtillery && !assailantArtillery ? aiData.combatEffectTable.getCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, CM_BOMBARDMENT) : 0.0;
	
	AssaultEffect assaultEffect;
	
	assaultEffect.speedFactor = assailantUnitSpeed <= protectorUnitSpeed ? 0.0 : assailantUnitSpeed == protectorUnitSpeed + 1 ? 0.5 : 0.75;
	
	if (assailantArtillery && !protectorArtillery)
	{
		if (assailantMelee)
		{
			double attackBombardmentRoundCount = 5.0;
			double attackBombardmentCoefficient = 1.0 - std::min(0.5, attackBombardmentRoundCount * assailantBombardmentEffect);
			assaultEffect.attack = assailantMeleeEffect / attackBombardmentCoefficient;
			
			double defendBombardmentRoundCount = protectorUnitSpeed == 1 ? 2.0 : 1.0;
			double defendBombardmentCoefficient = 1.0 - std::min(0.5, defendBombardmentRoundCount * assailantBombardmentEffect);
			assaultEffect.defend = 1.0 / (protectorMeleeEffect * defendBombardmentCoefficient);
			
		}
		else
		{
			double attackBombardmentRoundCount = 5.0;
			assaultEffect.attack = 1.0 + std::min(0.5, attackBombardmentRoundCount * assailantBombardmentEffect);
			
			double defendBombardmentRoundCount = protectorUnitSpeed == 1 ? 2.0 : 1.0;
			double defendBombardmentCoefficient = 1.0 - std::min(0.5, defendBombardmentRoundCount * assailantBombardmentEffect);
			assaultEffect.defend = 1.0 / (protectorMeleeEffect * defendBombardmentCoefficient);
			
		}
		
	}
	else if (!assailantArtillery && protectorArtillery)
	{
		if (protectorMelee)
		{
			double attackBombardmentRoundCount = assailantUnitSpeed == 1 ? 3.0 : assailantUnitSpeed == 2 ? 2.0 : assailantUnitSpeed == 3 ? 1.0 : 0.0;
			double attackBombardmentCoefficient = 1.0 - std::min(1.0, attackBombardmentRoundCount * protectorBombardmentEffect);
			assaultEffect.attack = assailantMeleeEffect * attackBombardmentCoefficient;
			
			double defendBombardmentRoundCount = assailantUnitSpeed == 1 ? 2.0 : assailantUnitSpeed == 2 ? 1.0 : 0.0;
			double defendBombardmentCoefficient = 1.0 - std::min(1.0, defendBombardmentRoundCount * protectorBombardmentEffect);
			assaultEffect.defend = 1.0 / (protectorMeleeEffect / defendBombardmentCoefficient);
			
		}
		else
		{
			double attackBombardmentRoundCount = assailantUnitSpeed == 1 ? 3.0 : assailantUnitSpeed == 2 ? 2.0 : assailantUnitSpeed == 3 ? 1.0 : 0.0;
			double attackBombardmentCoefficient = 1.0 - std::min(1.0, attackBombardmentRoundCount * protectorBombardmentEffect);
			assaultEffect.attack = assailantMeleeEffect * attackBombardmentCoefficient;
			
			double defendBombardmentRoundCount = assailantUnitSpeed == 1 ? 2.0 : assailantUnitSpeed == 2 ? 1.0 : 0.0;
			assaultEffect.defend = 1.0 + std::min(1.0, defendBombardmentRoundCount * protectorBombardmentEffect);
			
		}
		
	}
	else if ((assailantArtillery && protectorArtillery) || (!assailantArtillery && !protectorArtillery))
	{
		assaultEffect.attack = std::max(assailantMeleeEffect, assailantArtilleryDuelEffect);
		assaultEffect.defend = 1.0 / std::max(protectorMeleeEffect, protectorArtilleryDuelEffect);
	}
	
	return assaultEffect;
	
}

/**
Checks if unit can attack another unit at certain tile.
*/
bool isUnitCanMeleeAttackUnitAtTile(int attackerUnitId, int defenderUnitId, MAP *tile)
{
	assert(tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles);
	
	UNIT *attackerUnit = getUnit(attackerUnitId);
	UNIT *defenderUnit = getUnit(defenderUnitId);
	int attackerTriad = attackerUnit->triad();
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(attackerUnitId))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	
	if (!tileInfo.airbase && defenderUnit->chassis_id == CHS_NEEDLEJET && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// triad
	
	switch (attackerTriad)
	{
	case TRIAD_AIR:
		
		// can attack any unit in any tile
		
		break;
		
	case TRIAD_SEA:
		
		if (attackerUnitId == BSC_SEALURK)
		{
			// sealurk can attack sea and land
		}
		else
		{
			// cannot attack land
			
			if (tileInfo.land)
				return false;
			
		}
		
		break;
		
	case TRIAD_LAND:
		
		if (isUnitHasAbility(attackerUnitId, ABL_AMPHIBIOUS))
		{
			// amphibious cannot attack open sea
			
			if (tileInfo.ocean && !tileInfo.base)
				return false;
			
		}
		else
		{
			// land cannot attack sea
			
			if (tileInfo.ocean)
				return false;
			
		}
		
		break;
		
	}
	
	// all checks passed
	
	return true;
	
}

/**
Checks if unit can attack another unit from a certain tile.
For surface unit trying to attack flying unit the worst case is taken: flying unit is assumed to approach from the inaccessible realm.
*/
bool isUnitCanMeleeAttackUnitFromTile(int attackerUnitId, int defenderUnitId, MAP *tile)
{
	assert(tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles);
	
	UNIT *attackerUnit = getUnit(attackerUnitId);
	UNIT *defenderUnit = getUnit(defenderUnitId);
	int attackerTriad = attackerUnit->triad();
	int defenderTriad = defenderUnit->triad();
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(attackerUnitId))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	// assuming the land around tile is an open field
	
	if (defenderUnit->chassis_id == CHS_NEEDLEJET && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// check if air defender can place itself in inaccesible realm
	
	if (attackerTriad != TRIAD_AIR && defenderTriad == TRIAD_AIR)
	{
		for (MAP *adjacentTile : getAdjacentTiles(tile))
		{
			TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
			
			if (adjacentTileInfo.surfaceType != attackerTriad)
				return false;
			
		}
		
	}
	
	// triad
	
	switch (attackerTriad)
	{
	case TRIAD_AIR:
		
		// can attack any unit in any tile
		
		break;
		
	case TRIAD_SEA:
		
		if (attackerUnitId == BSC_SEALURK)
		{
			// sealurk can attack sea and land
		}
		else
		{
			// cannot attack land unit
			
			if (defenderTriad == TRIAD_LAND)
				return false;
			
		}
		
		break;
		
	case TRIAD_LAND:
		
		// cannot attack ship
		
		if (defenderTriad == TRIAD_SEA)
			return false;
		
		// cannot attack from sea (base)
		
		if (tileInfo.ocean)
			return false;
		
		break;
		
	}
	
	// all checks passed
	
	return true;
	
}

/**
Checks if unit can attack another unit at certain tile.
*/
bool isUnitCanArtilleryAttackUnitAtTile(int attackerUnitId, int defenderUnitId, MAP *tile)
{
	assert(tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles);
	
	UNIT *defenderUnit = getUnit(defenderUnitId);
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	// non artillery unit cannot bombard
	
	if (!isArtilleryUnit(attackerUnitId))
		return false;
	
	// cannot bombard air unit without air superiority
	
	if (!tileInfo.airbase && defenderUnit->triad() == TRIAD_AIR && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// all checks passed
	
	return true;
	
}

/**
Checks if unit can attack another unit from a certain tile.
For surface unit trying to attack flying unit the worst case is taken: flying unit is assumed to approach from the inaccessible realm.
*/
bool isUnitCanArtilleryAttackUnitFromTile(int attackerUnitId, int defenderUnitId, MAP */*tile*/)
{
	UNIT *defenderUnit = getUnit(defenderUnitId);
	
	// non artillery unit cannot bombard
	
	if (!isArtilleryUnit(attackerUnitId))
		return false;
	
	// cannot bombard air unit without air superiority
	// assuming the land around tile is an open field
	
	if (defenderUnit->triad() == TRIAD_AIR && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// all checks passed
	
	return true;
	
}

/**
Checks if unit can attack another unit at certain Base.
*/
bool isUnitCanMeleeAttackUnitAtBase(int attackerUnitId, int /*defenderUnitId*/)
{
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(attackerUnitId))
		return false;
	
	// all checks passed
	
	return true;
	
}

/**
Checks if unit can attack another unit from a certain Base.
For surface unit trying to attack flying unit the worst case is taken: flying unit is assumed to approach from the inaccessible realm.
*/
bool isUnitCanMeleeAttackUnitFromBase(int attackerUnitId, int defenderUnitId)
{
	UNIT *defenderUnit = getUnit(defenderUnitId);
	
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(attackerUnitId))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	// assuming the land around Base is an open field
	
	if (defenderUnit->chassis_id == CHS_NEEDLEJET && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// all checks passed
	
	return true;
	
}

/**
Checks if unit can attack another unit at certain Base.
*/
bool isUnitCanArtilleryAttackUnitAtBase(int attackerUnitId, int /*defenderUnitId*/)
{
	// non artillery unit cannot bombard
	
	if (!isArtilleryUnit(attackerUnitId))
		return false;
	
	// all checks passed
	
	return true;
	
}

/**
Checks if unit can attack another unit from a certain Base.
For surface unit trying to attack flying unit the worst case is taken: flying unit is assumed to approach from the inaccessible realm.
*/
bool isUnitCanArtilleryAttackUnitFromBase(int attackerUnitId, int defenderUnitId)
{
	UNIT *defenderUnit = getUnit(defenderUnitId);
	
	// non artillery unit cannot bombard
	
	if (!isArtilleryUnit(attackerUnitId))
		return false;
	
	// cannot bombard air unit without air superiority
	// assuming the land around Base is an open field
	
	if (defenderUnit->triad() == TRIAD_AIR && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// all checks passed
	
	return true;
	
}

MapDoubleValue getMeleeAttackPosition(int unitId, MAP *origin, MAP *target)
{
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	
	// find closest attack position
	
	MapDoubleValue closestAttackPosition(nullptr, INF);
	
	for (Adjacent const &positionAdjacent : targetTileInfo.adjacents)
	{
		MAP *positionTile = positionAdjacent.tileInfo->tile;
		
		// reachable
		
		if (!isUnitDestinationReachable(unitId, origin, positionTile))
			continue;
		
		// not blocked
		
		if (positionAdjacent.tileInfo->blocked)
			continue;
		
		// can melee attack
		
		if (!isUnitCanMeleeAttackFromTileToTile(unitId, positionTile, target))
			continue;
		
		// travelTime
		
		double travelTime = getUnitApproachTime(aiFactionId, unitId, origin, positionTile, true);
		if (travelTime == INF)
			continue;
		
		// update best
		
		if (travelTime < closestAttackPosition.value)
		{
			closestAttackPosition.tile = positionTile;
			closestAttackPosition.value = travelTime;
		}
		
	}
	
	return closestAttackPosition;
	
}

MapDoubleValue getMeleeAttackPosition(int vehicleId, MAP *target)
{
	return getMeleeAttackPosition(getVehicle(vehicleId)->unit_id, getVehicleMapTile(vehicleId), target);
}

MapDoubleValue getArtilleryAttackPosition(int unitId, MAP *origin, MAP *target)
{
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	
	// find closest attack position
	
	MapDoubleValue closestAttackPosition(nullptr, INF);
	
	for (TileInfo *positionTileInfo : targetTileInfo.range2NoCenterTileInfos)
	{
		MAP *positionTile = positionTileInfo->tile;
		
		// reachable
		
		if (!isUnitDestinationReachable(unitId, origin, positionTile))
			continue;
		
		// not blocked
		
		if (positionTileInfo->blocked)
			continue;
		
		// can artillery attack
		
		if (!isUnitCanArtilleryAttackFromTile(unitId, positionTile))
			continue;
		
		// travelTime
		
		double travelTime = getUnitApproachTime(aiFactionId, unitId, origin, positionTile, true);
		if (travelTime == INF)
			continue;
		
		// update best
		
		if (travelTime < closestAttackPosition.value)
		{
			closestAttackPosition.tile = positionTile;
			closestAttackPosition.value = travelTime;
		}
		
	}
	
	return closestAttackPosition;
	
}

MapDoubleValue getArtilleryAttackPosition(int vehicleId, MAP *target)
{
	return getArtilleryAttackPosition(getVehicle(vehicleId)->unit_id, getVehicleMapTile(vehicleId), target);
}

/*
Winning probability by combat effect.
*/
double getWinningProbability(double combatEffect)
{
	// div/0 is eliminated by condition
	return combatEffect <= 1.0 ? std::max(0.0, -0.4 + 0.9 * combatEffect) : std::min(1.0, 1.0 - (-0.4 + 0.9 * (1.0 / combatEffect)));
}

/*
Survival effect assuming 50% change of healing after winning the battle.
*/
double getSurvivalEffect(double combatEffect)
{
	double p = getWinningProbability(combatEffect);
	double q = 1.0 - p;
	
	double survivalEffect;
	
	// div/0 is eliminated by condition
	if (combatEffect <= 0.0)
	{
		survivalEffect = 0.0;
	}
	else if (combatEffect > 0.0 && combatEffect <= 1.0 && p <= 0.0)
	{
		survivalEffect = (combatEffect) / (2.0);
	}
	else if (combatEffect > 0.0 && combatEffect <= 1.0 && p > 0.0 && p <= 0.5)
	{
		survivalEffect = (combatEffect + p) / (1.0 + q);
	}
	else if (combatEffect >= 1.0 && combatEffect < DBL_MAX && p >= 0.5 && p < 1.0)
	{
		survivalEffect = (1.0 + p) / (1.0 / combatEffect + q);
	}
	else if (combatEffect >= 1.0 && combatEffect < DBL_MAX && p >= 1.0)
	{
		survivalEffect = (2.0) * (combatEffect);
	}
	else if (combatEffect >= DBL_MAX )
	{
		survivalEffect = DBL_MAX;
	}
	else
	{
		survivalEffect = 1.0;
	}
	
	return survivalEffect;
	
}

/*
Attack gain combat effect coefficient.
*/
double getCombatEffectCoefficient(double combatEffect)
{
	return 2.0 * getWinningProbability(combatEffect);
}

double getSensorOffenseMultiplier(int factionId, MAP *tile)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	
	TileInfo &tileInfo = aiData.tileInfos.at(tile - *MapTiles);
	
	return
		Rules->combat_defend_sensor != 0 && tileInfo.sensors.at(factionId) && (conf.sensor_offense && (!is_ocean(tile) || conf.sensor_offense_ocean)) ?
			getPercentageBonusMultiplier(Rules->combat_defend_sensor)
			:
			1.0
	;
	
}

double getSensorDefenseMultiplier(int factionId, MAP *tile)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	
	TileInfo &tileInfo = aiData.tileInfos.at(tile - *MapTiles);
	
	return
		Rules->combat_defend_sensor != 0 && tileInfo.sensors.at(factionId) && ((!is_ocean(tile) || conf.sensor_offense_ocean)) ?
			getPercentageBonusMultiplier(Rules->combat_defend_sensor)
			:
			1.0
	;
	
}

/**
Computes melee attack strength multiplier taking all bonuses/penalties into account.
exactLocation: whether combat happens at this exact location (base)
*/
double getUnitMeleeOffenseStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	bool ocean = is_ocean(tile);
	bool fungus = map_has_item(tile, BIT_FUNGUS);
	bool landRough = !ocean && (map_has_item(tile, BIT_FUNGUS | BIT_FOREST) || map_rockiness(tile) == 2);
	bool landOpen = !ocean && !(map_has_item(tile, BIT_FUNGUS | BIT_FOREST) || map_rockiness(tile) == 2);
	
	double offenseStrengthMultiplier = 1.0;
	
	// sensor
	
	offenseStrengthMultiplier *= getSensorOffenseMultiplier(attackerFactionId, tile);
	offenseStrengthMultiplier /= getSensorDefenseMultiplier(defenderFactionId, tile);
	
	// other modifiers are difficult to guess at not exact location
	
	if (!exactLocation)
		return offenseStrengthMultiplier;
	
	// base and terrain
	
	int baseId = base_at(x, y);
	if (baseId != -1)
	{
		// base
		
		offenseStrengthMultiplier /= getBaseDefenseMultiplier(baseId, attackerUnitId, defenderUnitId);
		
		// infantry vs. base attack bonus
		
		if (isInfantryUnit(attackerUnitId))
		{
			offenseStrengthMultiplier *= getPercentageBonusMultiplier(Rules->combat_infantry_vs_base);
		}
		
	}
	else
	{
		// native -> regular (fungus) = 50% attack bonus
		
		if (isNativeUnit(attackerUnitId) && !isNativeUnit(defenderUnitId) && fungus)
		{
			offenseStrengthMultiplier *= getPercentageBonusMultiplier(50);
		}
		
		// conventional defense in land rough = 50% defense bonus
		
		if (!isPsiCombat(attackerUnitId, defenderUnitId) && landRough)
		{
			offenseStrengthMultiplier /= getPercentageBonusMultiplier(50);
		}
		
		// conventional defense against mobile in land rough bonus
		
		if (!isPsiCombat(attackerUnitId, defenderUnitId) && isMobileUnit(attackerUnitId) && landRough)
		{
			offenseStrengthMultiplier /= getPercentageBonusMultiplier(Rules->combat_mobile_def_in_rough);
		}
		
		// mobile attack in open bonus

		if (!isPsiCombat(attackerUnitId, defenderUnitId) && isMobileUnit(attackerUnitId) && landOpen)
		{
			offenseStrengthMultiplier *= getPercentageBonusMultiplier(Rules->combat_mobile_open_ground);
		}
		
	}
	
	return offenseStrengthMultiplier;
	
}

/**
Computes artillerry attack strength multiplier taking all bonuses/penalties into account.
exactLocation: whether combat happens at this exact location (base)
*/
double getUnitArtilleryOffenseStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	double attackStrengthMultiplier = 1.0;
	
	// sensor
	
	attackStrengthMultiplier *= getSensorOffenseMultiplier(attackerFactionId, tile);
	attackStrengthMultiplier /= getSensorDefenseMultiplier(defenderFactionId, tile);
	
	// other modifiers are difficult to guess at not exact location
	
	if (!exactLocation)
		return attackStrengthMultiplier;
	
	if (isArtilleryUnit(defenderUnitId)) // artillery duel
	{
		// base and terrain
		
		int baseId = base_at(x, y);
		
		if (baseId != -1) // base
		{
			// artillery duel defender may get base bonus if configured
			
			if (conf.artillery_duel_uses_bonuses)
			{
				attackStrengthMultiplier /= getBaseDefenseMultiplier(baseId, attackerUnitId, defenderUnitId);
			}
			
		}
		else
		{
			// no other bonuses for artillery duel in open
		}
		
	}
	else // bombardment
	{
		// base and terrain
		
		int baseId = base_at(x, y);
		
		if (baseId != -1) // base
		{
			// base defense
			
			attackStrengthMultiplier /= getBaseDefenseMultiplier(baseId, attackerUnitId, defenderUnitId);
			
		}
		else if (isRoughTerrain(tile))
		{
			// bombardment defense in rough terrain bonus
			
			attackStrengthMultiplier /= getPercentageBonusMultiplier(50);
			
		}
		else
		{
			// bombardment defense in open get 50% bonus
			
			attackStrengthMultiplier /= getPercentageBonusMultiplier(50);
			
		}
		
	}
	
	return attackStrengthMultiplier;
	
}

/*
Computes fast hexCost.
Ignores special case when unit spends rest of their moves in fungus.
*/
int getHexCost(int unitId, int factionId, int x1, int y1, int x2, int y2, int speed1)
{
	UNIT *unit = &Units[unitId];
	MAP* org = mapsq(x1, y1);
	MAP* dst = mapsq(x2, y2);
	
	if (org == NULL || dst == NULL)
		return -1;
	
	// angle
	
	int angle;
	
	int dx = x2 - x1;
	if (!map_is_flat())
	{
		if (dx > *MapHalfX)
		{
			dx -= *MapAreaX;
		}
		else if (dx < - *MapHalfX)
		{
			dx += *MapAreaX;
		}
	}
	int dy = y2 - y1;
	
	switch (dx)
	{
	case -2:
		{
			switch (dy)
			{
			case +0:
				angle = 5;
				break;
			default:
				return -1;
			}
		}
		break;
	case -1:
		{
			switch (dy)
			{
			case -1:
				angle = 6;
				break;
			case +1:
				angle = 4;
				break;
			default:
				return -1;
			}
		}
		break;
	case +0:
		{
			switch (dy)
			{
			case -2:
				angle = 7;
				break;
			case +2:
				angle = 3;
				break;
			default:
				return -1;
			}
		}
		break;
	case +1:
		{
			switch (dy)
			{
			case -1:
				angle = 0;
				break;
			case +2:
				angle = 2;
				break;
			default:
				return -1;
			}
		}
		break;
	case +2:
		{
			switch (dy)
			{
			case +0:
				angle = 1;
				break;
			default:
				return -1;
			}
		}
		break;
	default:
		return -1;
	}
    
	// read cache
	
	MovementType movementType = getUnitMovementType(factionId, unitId);
	int orgIndex = org - *MapTiles;
	int cachedHexCost = getCachedHexCost(movementType, speed1, orgIndex, angle);
	if (cachedHexCost != -1)
	{
		return cachedHexCost;
	}
	
	// compute hexCost
	
	int hexCost = -1;
	
	switch (unit->triad())
	{
	case TRIAD_AIR:
		return 1 * Rules->move_rate_roads;
		break;
	
	case TRIAD_SEA:
		{
			if (!((org->is_sea() || org->is_base()) && (dst->is_sea() || dst->is_base())))
				return -1;
			
			if (dst->is_fungus() && unitId != BSC_ISLE_OF_THE_DEEP && unitId != BSC_SEALURK && !has_project(FAC_XENOEMPATHY_DOME, factionId))
			{
				hexCost = 3 * Rules->move_rate_roads;
			}
			else
			{
				hexCost = 1 * Rules->move_rate_roads;
			}
			
		}
		break;
		
	case TRIAD_LAND:
		{
			if (!(org->is_land() && dst->is_land()))
				return -1;
			
			if (factionId != 0 && org->items & (BIT_MAGTUBE | BIT_BASE_IN_TILE) && dst->items & (BIT_MAGTUBE | BIT_BASE_IN_TILE))
			{
				hexCost = conf.magtube_movement_rate > 0 ? 1 : 0;
			}
			else if (dst->is_fungus() && (unitId == BSC_MIND_WORMS || unitId == BSC_SPORE_LAUNCHER || (factionId >= 0 && has_project(FAC_XENOEMPATHY_DOME, factionId))))
			{
				hexCost = conf.magtube_movement_rate > 0 ? conf.road_movement_rate : 1;
			}
			else if (factionId != 0 && ((org->items & (BIT_ROAD | BIT_BASE_IN_TILE)) || (org->is_fungus() && has_project(FAC_XENOEMPATHY_DOME, factionId))) && (dst->items & (BIT_ROAD | BIT_BASE_IN_TILE)))
			{
				hexCost = conf.magtube_movement_rate > 0 ? conf.road_movement_rate : 1;
			}
			else if (factionId != 0 && (org->items & BIT_RIVER) && (dst->items & BIT_RIVER) && x_dist(x1, x2) == 1 && abs(y1 - y2) == 1)
			{
				hexCost = conf.magtube_movement_rate > 0 ? conf.road_movement_rate : 1;
			}
			else if (unit->chassis_id == CHS_HOVERTANK || has_abil(unitId, ABL_ANTIGRAV_STRUTS))
			{
				hexCost = 1 * Rules->move_rate_roads;
			}
			else
			{
				hexCost = Rules->move_rate_roads;
				if (dst->is_rocky() && !speed1)
				{
					hexCost += Rules->move_rate_roads;
				}
				if ((dst->items & BIT_FOREST) && !speed1)
				{
					hexCost += Rules->move_rate_roads;
				}
				if (factionId != 0 && dst->is_fungus() && (unitId >= MaxProtoFactionNum || unit->offense_value() >= 0))
				{
					// ignore special case
	//				int plan = unit->plan;
	//				if (plan != PLAN_TERRAFORM && plan != PLAN_ARTIFACT && Factions[factionId].SE_planet <= 0)
	//				{
	//					return cost + 2 * Rules->move_rate_roads;
	//				}
	//				
	//				int value = proto_speed(unitId);
	//				if (cost <= value)
	//				{
	//					return value;
	//				}
					hexCost += 2 * Rules->move_rate_roads;
				}
			}
			
		}
		break;
		
	default:
		return -1;
		
	}
	
	if (hexCost != -1)
	{
		setCachedHexCost(movementType, speed1, orgIndex, angle, hexCost);
	}
	
	return hexCost;
	
}

/**
Determines number of drones could be quelled by police.
*/
int getBasePoliceRequiredPower(int baseId)
{
	BASE *base = getBase(baseId);
	
	// compute base without specialists

	Profiling::start("- getBasePoliceRequiredPower - computeBase");
	computeBase(baseId, true);
	Profiling::stop("- getBasePoliceRequiredPower - computeBase");
	
	// get drones currently quelled by police
	
	int quelledDrones = *CURRENT_BASE_DRONES_FACILITIES - *CURRENT_BASE_DRONES_POLICE;
	
	// get drones left after projects applied
	
	int leftDrones = *CURRENT_BASE_DRONES_PROJECTS;
	
	// calculate potential number of drones can be quelled with police
	
	int requiredPolicePower = quelledDrones + leftDrones - base->talent_total;
	
	// restore base
	
	aiData.resetBase(baseId);
	
	return requiredPolicePower;
	
}

