#pragma GCC diagnostic ignored "-Wshadow"

#include "wtp_ai.h"

#include "robin_hood.h"
#include "wtp_terranx.h"
#include "wtp_game.h"
#include "wtp_mod.h"
#include "wtp_aiRoute.h"
#include "wtp_aiMove.h"
#include "wtp_aiData.h"
#include "wtp_aiHurry.h"
#include "wtp_aiProduction.h"

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

void clearPlayerFactionReferences()
{
	aiFactionId = -1;
	aiMFaction = nullptr;
	aiFaction = nullptr;
	aiFactionInfo = nullptr;
}

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
debug(">modified_enemy_units_check(%d) <\n", factionId);
std::time_t now1 = std::time(nullptr);
debug(">%s\n", std::ctime(&now1));
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
	mod_enemy_turn(factionId);
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
	
debug(">modified_enemy_units_check(%d) >\n", factionId);
std::time_t now2 = std::time(nullptr);
debug(">%s\n", std::ctime(&now2));
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
	populatePlayerBaseRanges();
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
//	computeCombatEffects();
	
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
	
	aiData.bunkerCombatDatas.clear();
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		tileInfo.bunker = map_has_item(tile, BIT_BUNKER);
		tileInfo.airbase = map_has_item(tile, BIT_BASE_IN_TILE | BIT_AIRBASE);
		
		if (tileInfo.bunker && tile->owner == aiFactionId)
		{
			aiData.bunkerCombatDatas.emplace(tile, CombatData());
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
		
		for (std::array<HexCost, ANGLE_COUNT> &movementTypeHexCosts : tileInfo.hexCosts)
		{
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				movementTypeHexCosts.at(angle) = {-1, -1};
			}
		}
		
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
				
				tileInfo.hexCosts.at(MT_LAND).at(angle)					= {landHexCost,				approximateLandHexCost			};
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
				for (Adjacent const &adjacent : tileInfo.adjacents)
				{
					// zoc is exerted only to land
					
					if (adjacent.tileInfo->ocean)
						continue;
					
					// zoc is not exerted to base
					
					if (adjacent.tileInfo->base)
						continue;
					
					// zoc
					
					adjacent.tileInfo->unfriendlyVehicleZoc = true;
					
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
		tileInfo.maxBombardmentDamage = getPercentageBonusMultiplier(maxBombardmentDamagePercent);
		
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
				tileInfo.structureDefenseMultipliers.at(attackTriad)		= getBaseDefenseMultiplier(tileInfo.baseId, attackTriad);
			}
		}
		else if (tileInfo.bunker)
		{
			tileInfo.structureDefenseMultipliers.at(ATTACK_TRIAD_LAND)	= getPercentageBonusMultiplier(conf.bunker_bonus_surface);
			tileInfo.structureDefenseMultipliers.at(ATTACK_TRIAD_SEA)		= getPercentageBonusMultiplier(conf.bunker_bonus_surface);
			tileInfo.structureDefenseMultipliers.at(ATTACK_TRIAD_AIR)		= getPercentageBonusMultiplier(conf.bunker_bonus_aerial);
			tileInfo.structureDefenseMultipliers.at(ATTACK_TRIAD_PSI)		= 1.0;
		}
		else
		{
			for (AttackTriad attackTriad : ATTACK_TRIADS)
			{
				tileInfo.structureDefenseMultipliers.at(attackTriad)		= 1.0;
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

/*
Repopulates vehicle locations and corresponding blocked and zoc after vehicle moved/created/destroyed.
*/
void updateVehicleTileBlockedAndZocs()
{
	Profiling::start("- updateVehicleTileBlockedAndZocs");
	
	// reset values
	
	for (TileInfo &tileInfo : aiData.tileInfos)
	{
		tileInfo.vehicleIds.clear();
		tileInfo.playerVehicleIds.clear();
		std::fill(tileInfo.factionNeedlejetInFlights.begin(), tileInfo.factionNeedlejetInFlights.end(), false);
		std::fill(tileInfo.unfriendlyNeedlejetInFlights.begin(), tileInfo.unfriendlyNeedlejetInFlights.end(), false);
		
		tileInfo.friendlyVehicle = false;
		tileInfo.unfriendlyVehicle = false;
		tileInfo.unfriendlyVehicleZoc = false;
		
		tileInfo.blocked = false;
		tileInfo.orgZoc = false;
		tileInfo.dstZoc = false;
		
	}
	
	// vehicles
		
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH &vehicle = Vehs[vehicleId];
		TileInfo &tileInfo = aiData.getVehicleTileInfo(vehicleId);
		
		// tile vehicles
		
		tileInfo.vehicleIds.push_back(vehicleId);
		
		// tile player vehicles
		
		if (Vehs[vehicleId].faction_id == aiFactionId)
		{
			tileInfo.playerVehicleIds.push_back(vehicleId);
		}
		
		// tile faction needlejet in flight
		
		if (!tileInfo.airbase && isNeedlejetVehicle(vehicleId))
		{
			tileInfo.factionNeedlejetInFlights.at(vehicle.faction_id) = true;
		}
		
		// friendly/unfriendly vehicles
		
		if (isFriendly(aiFactionId, vehicle.faction_id))
		{
			tileInfo.friendlyVehicle = true;
		}
		else
		{
			tileInfo.unfriendlyVehicle = true;
			
			// zoc is exerted only from land
			
			if (tileInfo.land)
			{
				for (Adjacent const &adjacent : tileInfo.adjacents)
				{
					// zoc is exerted only to land
					
					if (adjacent.tileInfo->ocean)
						continue;
					
					// zoc is not exerted to base
					
					if (adjacent.tileInfo->base)
						continue;
					
					// zoc
					
					adjacent.tileInfo->unfriendlyVehicleZoc = true;
					
				}
				
			}
			
		}
		
	}
	
	// blocked and zoc
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		setTileBlockedAndZoc(tileInfo);
	}
	
	// needlejet in flight
	
	for (TileInfo &tileInfo : aiData.tileInfos)
	{
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			for (int otherFactionId = 0; otherFactionId < MaxPlayerNum; otherFactionId++)
			{
				if (isUnfriendly(factionId, otherFactionId) && tileInfo.factionNeedlejetInFlights.at(otherFactionId))
				{
					tileInfo.unfriendlyNeedlejetInFlights.at(factionId) = true;
				}
			}
			
		}
		
	}
	
	Profiling::stop("- updateVehicleTileBlockedAndZocs");
	
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
			tileInfo.baseDistances.at(triad) = DBL_MAX;
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
		baseTileInfo.baseDistances.at(TRIAD_AIR) = 0.0;
		openNodes.push_back(baseTile);
		
	}
	
	while (openNodes.size() > 0)
	{
		baseRange++;
		
		for (MAP *tile : openNodes)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			for (Adjacent const &adjacent : tileInfo.adjacents)
			{
				int angle = adjacent.angle;
				TileInfo *adjacentTileInfo = adjacent.tileInfo;
				
				if (baseRange < adjacentTileInfo->baseRanges.at(TRIAD_AIR))
				{
					adjacentTileInfo->baseRanges.at(TRIAD_AIR) = baseRange;
					newOpenNodes.push_back(adjacentTileInfo->tile);
				}
				
				double baseDistance = tileInfo.baseDistances.at(TRIAD_AIR) + (angle % 2 == 0 ? 1.0 : 1.5);
				adjacentTileInfo->baseDistances.at(TRIAD_AIR) = std::min(adjacentTileInfo->baseDistances.at(TRIAD_AIR), baseDistance);
				
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
		baseTileInfo.baseDistances.at(TRIAD_SEA) = 0.0;
		openNodes.push_back(baseTile);
		
	}
	
	while (openNodes.size() > 0)
	{
		baseRange++;
		
		for (MAP *tile : openNodes)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			for (Adjacent const &adjacent : tileInfo.adjacents)
			{
				int angle = adjacent.angle;
				TileInfo *adjacentTileInfo = adjacent.tileInfo;
				
				if (baseRange < adjacentTileInfo->baseRanges.at(TRIAD_SEA))
				{
					adjacentTileInfo->baseRanges.at(TRIAD_SEA) = baseRange;
					newOpenNodes.push_back(adjacentTileInfo->tile);
				}
				
				double baseDistance = tileInfo.baseDistances.at(TRIAD_SEA) + (angle % 2 == 0 ? 1.0 : 1.5);
				adjacentTileInfo->baseDistances.at(TRIAD_SEA) = std::min(adjacentTileInfo->baseDistances.at(TRIAD_SEA), baseDistance);
				
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
		baseTileInfo.baseDistances.at(TRIAD_LAND) = 0.0;
		openNodes.push_back(baseTile);
		
	}
	
	while (openNodes.size() > 0)
	{
		baseRange++;
		
		for (MAP *tile : openNodes)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			
			for (Adjacent const &adjacent : tileInfo.adjacents)
			{
				int angle = adjacent.angle;
				TileInfo *adjacentTileInfo = adjacent.tileInfo;
				
				if (baseRange < adjacentTileInfo->baseRanges.at(TRIAD_LAND))
				{
					adjacentTileInfo->baseRanges.at(TRIAD_LAND) = baseRange;
					newOpenNodes.push_back(adjacentTileInfo->tile);
				}
				
				double baseDistance = tileInfo.baseDistances.at(TRIAD_LAND) + (angle % 2 == 0 ? 1.0 : 1.5);
				adjacentTileInfo->baseDistances.at(TRIAD_LAND) = std::min(adjacentTileInfo->baseDistances.at(TRIAD_LAND), baseDistance);
				
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
	
	aiData.maxBaseSize = getMaxBaseSize(aiFactionId);
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
	
	aiData.pad0VehicleIds.clear();
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int vehicleLandTransportedCluster = getLandTransportedCluster(vehicleTile);
		int vehicleSeaCluster = getSeaCluster(vehicleTile);
		
		// store all vehicle current id in pad_0 field
		// shift pad_0 by MaxVehNum to make sure it is distinct from vehicleId
		
		vehicle->pad_0 = MaxVehNum + vehicleId;
		aiData.pad0VehicleIds.emplace(vehicle->pad_0, vehicleId);
		
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
	
	// populate vehicle pad0 map
	
	populateVehiclePad0Map();
	
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
		
		int vehicleMaxRange = (triad == TRIAD_LAND ? getVehicleMoveRate(vehicleId) : getVehicleSpeed(vehicleId));
		
		for (int unfriendlyAffectedVehicleId : unfriendlyAffectedVehicleIds)
		{
			VEH &unfriendlyAffectedVehicle = Vehs[unfriendlyAffectedVehicleId];
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
		for (robin_hood::pair<int, int> vehicleReachableLocation : getVehicleReachableLocations(vehicleId, false))
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
		
		int vehicleMaxRange = (triad == TRIAD_LAND ? getVehicleMoveRate(vehicleId) : getVehicleSpeed(vehicleId));
		
		for (int hostileAffectedVehicleId : hostileAffectedVehicleIds)
		{
			VEH &hostileAffectedVehicle = Vehs[hostileAffectedVehicleId];
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
		
		int vehicleMaxRange = (triad == TRIAD_LAND ? getVehicleMoveRate(vehicleId) : getVehicleSpeed(vehicleId));
		
		for (int artilleryAffectedVehicleId : artilleryAffectedVehicleIds)
		{
			VEH &artilleryAffectedVehicle = Vehs[artilleryAffectedVehicleId];
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
	
	// enemy attacks
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo *tileInfo = &aiData.tileInfos.at(tileIndex);
		for (int engagementMode : {EM_MELEE, EM_ARTILLERY})
		{
			tileInfo->potentialAttacks.at(engagementMode).clear();
		}
	}
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// not this faction
		
		if (vehicle->faction_id == aiFactionId)
			continue;
		
		// hostile
		
		if (!isHostile(aiFactionId, vehicle->faction_id))
			continue;
		
		if (isMeleeVehicle(vehicleId))
		{
			for (robin_hood::pair<MAP *, double> const &potentialMeleeAttackTargets : getPotentialMeleeAttackTargets(vehicleId))
			{
				MAP *tile = potentialMeleeAttackTargets.first;
				double hastyCoefficient = potentialMeleeAttackTargets.second;
				TileInfo *tileInfo = &aiData.getTileInfo(tile);
				
				tileInfo->potentialAttacks.at(EM_MELEE).push_back({vehicleId, hastyCoefficient});
				
			}
			
		}
		
		if (isArtilleryVehicle(vehicleId))
		{
			for (robin_hood::pair<MAP *, double> const &potentialMeleeAttackTargets : getPotentialArtilleryAttackTargets(vehicleId))
			{
				MAP *tile = potentialMeleeAttackTargets.first;
				TileInfo *tileInfo = &aiData.getTileInfo(tile);
				
				tileInfo->potentialAttacks.at(EM_ARTILLERY).push_back({vehicleId, 1.0});
				
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
		
		debug("\t\t%s\n", getLocationString(enemyStackTile).c_str());
		
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
	SummaryStatistics attackGainSummary;
	for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
	{
		EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;
		
		debug("\t\t%s\n", getLocationString(enemyStackInfo.tile).c_str());
		
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
			for (int vehiclePad0 : enemyStackInfo.vehiclePad0s)
			{
				int vehicleId = getVehicleIdByPad0(vehiclePad0);
				if (!(vehicleId >= 0 && vehicleId < *VehCount))
					continue;
				
				double attackGain = getDestroyingEnemyVehicleGain(vehicleId);
				attackGainSummary.add(attackGain);
				
			}
			
			averageAttackGain = attackGainSummary.mean();
			
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

void computeUnitDestructionGains()
{
	debug("computeUnitDestructionGains - %s\n", aiMFaction->noun_faction);
	
	aiData.continuousImprovmentGain = getContinuousImprovmentGain();
	aiData.continuousHarvestingGain = getContinuousHarvestingGain();
	aiData.stealingTechnologyGain = getStealingTechnologyGain();
	aiData.cachingArtifactGain = getCachingArtifactGain();
	
	for (int unitId = 0; unitId < MaxProtoNum; unitId++)
	{
		UNIT *unit = getUnit(unitId);
		
		if (!unit->is_active())
		{
			aiData.unitDestructionGains.at(unitId) = 0.0;
			continue;
		}
		
		aiData.unitDestructionGains.at(unitId) = getUnitDestructionGain(unitId);
		
	}
	
	if (DEBUG)
	{
		for (int unitId = 0; unitId < MaxProtoNum; unitId++)
		{
			UNIT *unit = getUnit(unitId);
			
			if (!unit->is_active())
				continue;
			
			debug("\t%-32s %7.2f\n", unit->name, aiData.unitDestructionGains.at(unitId));
			
		}
		
	}
	
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
	
	SummaryStatistics assaultEffectSummary;
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
					
					double assaultEffect = computeAssaultEffect(aiFactionId, aiUnitId, enemyUnitFactionId, enemyUnitId, baseTile);
					assaultEffectSummary.add(assaultEffect, enemyUnitWeight);
					
				}
				
			}
			
			double averageAssaultEffect = assaultEffectSummary.mean();
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
	
	std::array<FactionInfo, MaxPlayerNum> &factionInfos = aiData.factionInfos;
	
	for (robin_hood::pair<MAP *, EnemyStackInfo> &stackInfoEntry : aiData.enemyStacks)
	{
		MAP *enemyStackTile = stackInfoEntry.first;
		EnemyStackInfo &enemyStackInfo = stackInfoEntry.second;
		
		assert(isOnMap(enemyStackTile));
		
		CombatData &combatData = enemyStackInfo.combatData;
		TileCombatEffectTable &tileCombatEffectTable = combatData.tileCombatEffectTable;
		
		// ignore stacks without vehicles
		
		if (enemyStackInfo.vehiclePad0s.empty())
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
			
			if (isMeleeUnit(ownUnitId) && (conf.air_superiority_not_required_to_attack_needlejet || !enemyStackInfo.needlejetInFlight || isUnitHasAbility(ownUnitId, ABL_AIR_SUPERIORITY)))
			{
				int battleCount = 0;
				double totalLoss = 0.0;
				
				for (int foeVehiclePad0 : enemyStackInfo.vehiclePad0s)
				{
					int foeVehicleId = getVehicleIdByPad0(foeVehiclePad0);
					if (!(foeVehicleId >= 0 && foeVehicleId < *VehCount))
						continue;
					
					VEH *foeVehicle = getVehicle(foeVehicleId);
					
					double effect = tileCombatEffectTable.getCombatEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, EM_MELEE);
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
					
					double effect = tileCombatEffectTable.getCombatEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, EM_ARTILLERY);
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
					
					double effect = tileCombatEffectTable.getCombatEffect(aiFactionId, ownUnitId, foeVehicle->faction_id, foeVehicle->unit_id, EM_ARTILLERY);
					
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
		
		// calculate destructionGain
		
		Profiling::start("calculate destructionGain", "evaluateEnemyStacks");
		
		SummaryStatistics destructionGainSummary;
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
		debug("\tdestructionGain=%5.2f\n", enemyStackInfo.destructionGain);
		
		Profiling::stop("calculate destructionGain");
		
	}
	
	Profiling::stop("evaluateEnemyStacks");
	
}

void evaluateBaseDefense()
{
	debug("evaluateBaseDefense - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int baseId : aiData.baseIds)
	{
		MAP *tile = getBaseMapTile(baseId);
		CombatData &combatData = aiData.baseInfos.at(baseId).combatData;
		evaluateDefense(tile, combatData);
	}
	
	for (robin_hood::pair<MAP *, CombatData> &bunkerCombatDataEntry : aiData.bunkerCombatDatas)
	{
		MAP *tile = bunkerCombatDataEntry.first;
		CombatData &combatData = bunkerCombatDataEntry.second;
		evaluateDefense(tile, combatData);
	}
	
}

void evaluateDefense(MAP *tile, CombatData &combatData)
{
	Profiling::start("evaluateBaseDefense", "populateAIData");
	
	std::array<FactionInfo, MaxPlayerNum> &factionInfos = aiData.factionInfos;
	std::vector<int> const &unfriendlyFactionIds = aiData.unfriendlyFactionIds;
	
	// evaluate base threat

	Profiling::start("evaluate base threat", "evaluateBaseDefense");
	
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	BASE *base = getBase(tileInfo.baseId);
	
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
			
			// strength multiplier
			
			double strengthMultiplier = getVehicleStrenghtMultiplier(vehicleId);
			
			// threat coefficient
			
			double threatCoefficient = aiData.factionInfos[vehicle.faction_id].threatCoefficient;
			
			// approach time coefficient
			
			double approachTime = getVehicleApproachTime(vehicleId, tile);
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
		
		if (foeUnitWeights.at(0).find(alienUnitId) == foeUnitWeights.at(0).end())
		{
			foeUnitWeights.at(0).emplace(alienUnitId, 0.0);
		}
		foeUnitWeights.at(0).at(alienUnitId) += weight;
		
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
	
	combatData.clearAssailants();
	
	debug("\tassailants\n");
	for (int foeFactionId : unfriendlyFactionIds)
	{
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
			" conf.ai_combat_base_protection_superiority=%5.2f"
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

// --------------------------------------------------
// helper functions
// --------------------------------------------------

int getVehicleIdByPad0(int pad0)
{
	robin_hood::unordered_flat_map<int, int>::iterator pad0VehicleIdIterator = aiData.pad0VehicleIds.find(pad0);
	
	if (pad0VehicleIdIterator == aiData.pad0VehicleIds.end())
	{
		return -1;
	}
	else
	{
		return pad0VehicleIdIterator->second;
	}
	
}

VEH *getVehicleByPad0(int pad0)
{
	return getVehicle(getVehicleIdByPad0(pad0));
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

	for (int unitId : getDesignedFactionUnitIds(factionId, true, true))
	{
		UNIT *unit = &(Units[unitId]);

		// combat

		if (!isCombatUnit(unitId))
			continue;

		// weapon offense value

		int weaponOffenseValue = Weapon[unit->weapon_id].offense_value;

		// update best weapon offense value

		bestWeaponOffenseValue = std::max(bestWeaponOffenseValue, weaponOffenseValue);

	}

	return bestWeaponOffenseValue;

}

int getFactionMaxConDefenseValue(int factionId)
{
	int bestArmorDefenseValue = 0;

	for (int unitId : getDesignedFactionUnitIds(factionId, true, true))
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
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	// not in base

	if (map_has_item(vehicleTile, BIT_BASE_IN_TILE))
		return false;

	// check other units

	bool threatened = false;

	for (int otherVehicleId = 0; otherVehicleId < *VehCount; otherVehicleId++)
	{
		VEH *otherVehicle = getVehicle(otherVehicleId);
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
	VEH *vehicle = getVehicle(vehicleId);

	for (int otherVehicleId = 0; otherVehicleId < *VehCount; otherVehicleId++)
	{
		VEH *otherVehicle = getVehicle(otherVehicleId);

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

/*
Returns vehicle specific psi offense modifier to unit strength.
*/
double getVehiclePsiOffenseModifier(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);

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
	VEH *vehicle = getVehicle(vehicleId);

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
	VEH *vehicle = getVehicle(vehicleId);

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
	VEH *vehicle = getVehicle(vehicleId);

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
	return isOffensiveUnit(Vehs[vehicleId].unit_id, Vehs[vehicleId].faction_id);
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
robin_hood::unordered_flat_map<int, int> getVehicleReachableLocations(int vehicleId, bool regardObstacle)
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
			reachableLocations = getAirVehicleReachableLocations(vehicleTile, movementAllowance, regardObstacle);
		}
		break;
	case TRIAD_SEA:
		{
			MovementType movementType = getUnitMovementType(factionId, unitId);
			reachableLocations = getSeaVehicleReachableLocations(movementType, vehicleTile, movementAllowance, regardObstacle);
		}
		break;
	case TRIAD_LAND:
		{
			MovementType movementType = getUnitMovementType(factionId, unitId);
			reachableLocations = getLandVehicleReachableLocations(movementType, vehicleTile, movementAllowance, regardObstacle, regardObstacle && isZocAffectedVehicle(vehicleId));
		}
		break;
	}
	
	Profiling::stop("- getVehicleReachableLocations");
	return reachableLocations;
	
}

robin_hood::unordered_flat_map<int, int> getAirVehicleReachableLocations(MAP *origin, int movementAllowance, bool regardObstacle)
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
				
				// regard obstacle
				
				if (regardObstacle)
				{
					if (adjacent.tileInfo->blocked)
						continue;
				}
				
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

robin_hood::unordered_flat_map<int, int> getSeaVehicleReachableLocations(MovementType movementType, MAP *origin, int movementAllowance, bool regardObstacle)
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
				
				// regard obstacle
				
				if (regardObstacle)
				{
					if (adjacent.tileInfo->blocked)
						continue;
				}
				
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

robin_hood::unordered_flat_map<int, int> getLandVehicleReachableLocations(MovementType movementType, MAP *origin, int movementAllowance, bool regardObstacle, bool regardZoc)
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
				
				// regard obstacle
				
				if (regardObstacle)
				{
					if (adjacent.tileInfo->blocked)
						continue;
				}
				
				// regard ZoC
				
				if (regardObstacle && regardZoc)
				{
					if (currentTileInfo.orgZoc && adjacentTileInfo->dstZoc)
						continue;
				}
				
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

/*
Searches for potentially enemy attackable locations.
Path search excludes hostile vehicles.
*/
robin_hood::unordered_flat_map<MAP *, double> getPotentialMeleeAttackTargets(int vehicleId)
{
	robin_hood::unordered_flat_map<MAP *, double> attackTargets;
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return attackTargets;
	
	// explore reachable locations
	
	Profiling::start("- getMeleeAttackActions - getVehicleReachableLocations");
	for (robin_hood::pair<int, int> const &reachableLocation : getVehicleReachableLocations(vehicleId, false))
	{
		int tileIndex = reachableLocation.first;
		int movementAllowance = reachableLocation.second;
		
		MAP *tile = *MapTiles + tileIndex;
		double hastyCoefficient = movementAllowance >= Rules->move_rate_roads ? 1.0 : (double)movementAllowance / (double)Rules->move_rate_roads;
		
		// cannot attack without movementAllowance
		
		if (movementAllowance <= 0)
			continue;
		
		// explore adjacent tiles
		
		for (Adjacent const &adjacent : aiData.getTileInfo(tile).adjacents)
		{
			MAP *target = adjacent.tileInfo->tile;
			
			// can melee attack
			
			if (!isVehicleCanMeleeAttackTile(vehicleId, target, tile))
				continue;
			
			// add action
			
			if (attackTargets.find(target) == attackTargets.end())
			{
				attackTargets.emplace(target, hastyCoefficient);
			}
			else
			{
				if (hastyCoefficient > attackTargets.at(target))
				{
					attackTargets.at(target) = hastyCoefficient;
				}
			}
			
		}
		
	}
	Profiling::stop("- getMeleeAttackActions - getVehicleReachableLocations");
	
	return attackTargets;
	
}

/*
Searches for potentially attackable locations.
Path search excludes hostile vehicles.
*/
robin_hood::unordered_flat_map<MAP *, double> getPotentialArtilleryAttackTargets(int vehicleId)
{
	robin_hood::unordered_flat_map<MAP *, double> attackTargets;
	
	// artillery vehicle
	
	if (!isArtilleryVehicle(vehicleId))
		return attackTargets;
	
	// explore reachable locations
	
	Profiling::start("- getMeleeAttackActions - getVehicleReachableLocations");
	for (robin_hood::pair<int, int> const &reachableLocation : getVehicleReachableLocations(vehicleId, false))
	{
		int tileIndex = reachableLocation.first;
		int movementAllowance = reachableLocation.second;
		
		MAP *tile = *MapTiles + tileIndex;
		
		// cannot attack without movementAllowance
		
		if (movementAllowance <= 0)
			continue;
		
		// explore adjacent tiles
		
		for (Adjacent const &adjacent : aiData.getTileInfo(tile).adjacents)
		{
			MAP *target = adjacent.tileInfo->tile;
			
			// can artillery attack
			
			if (!isVehicleCanMeleeAttackTile(vehicleId, target, tile))
				continue;
			
			// add action
			
			if (attackTargets.find(target) == attackTargets.end())
			{
				attackTargets.emplace(target, 1.0);
			}
			
		}
		
	}
	Profiling::stop("- getMeleeAttackActions - getVehicleReachableLocations");
	
	return attackTargets;
	
}

/**
Searches for locations vehicle can melee attack at.
*/
std::vector<AttackAction> getMeleeAttackActions(int vehicleId, bool regardObstacle)
{
	std::vector<AttackAction> attackActions;
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return attackActions;
	
	// explore reachable locations
	
	Profiling::start("- getMeleeAttackActions - getVehicleReachableLocations");
	for (robin_hood::pair<int, int> const &reachableLocation : getVehicleReachableLocations(vehicleId, regardObstacle))
	{
		int tileIndex = reachableLocation.first;
		int movementAllowance = reachableLocation.second;
		
		MAP *tile = *MapTiles + tileIndex;
		double hastyCoefficient = movementAllowance >= Rules->move_rate_roads ? 1.0 : (double)movementAllowance / (double)Rules->move_rate_roads;
		
		// need movementAllowance for attack
		
		if (movementAllowance <= 0)
			continue;
		
		// explore adjacent tiles
		
		for (Adjacent const &adjacent : aiData.getTileInfo(tile).adjacents)
		{
			MAP *targetTile = adjacent.tileInfo->tile;
			
			// hostile at target
			
			if (!isHostile(aiFactionId, targetTile->veh_who()))
				continue;
			
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
std::vector<AttackAction> getArtilleryAttackActions(int vehicleId, bool regardObstacle)
{
	std::vector<AttackAction> attackActions;
	
	// artillery vehicle
	
	if (!isArtilleryVehicle(vehicleId))
		return attackActions;
	
	// explore reachable locations
	
	Profiling::start("- getArtilleryAttackActions - getVehicleReachableLocations");
	for (robin_hood::pair<int, int> const &reachableLocation : getVehicleReachableLocations(vehicleId, regardObstacle))
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
			MAP *targetTile = targetTileInfo->tile;
			
			// add action
			
			attackActions.push_back({tile, targetTile, 1.0});
			
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
	
	VEH &vehicle = Vehs[vehicleId];
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
				
				double hastyCoefficient = currentTileMovementAllowance >= Rules->move_rate_roads ? 1.0 : (double)currentTileMovementAllowance / (double)Rules->move_rate_roads;
				if (attackLocations.find(adjacentTileIndex) == attackLocations.end() || hastyCoefficient > attackLocations.at(adjacentTileIndex))
				{
					attackLocations[adjacentTileIndex] = hastyCoefficient;
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
	
	VEH &vehicle = Vehs[vehicleId];
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
	int factionId = vehicle->faction_id;
	int offenseValue = vehicle->offense_value();
	int defenseValue = vehicle->defense_value();
	
	double attackGain = 0.0;
	
	if (isArtifactVehicle(vehicleId))
	{
		attackGain = getGainBonus(Rules->mineral_cost_multi * Units[BSC_ALIEN_ARTIFACT].cost);
	}
	else if (factionId == 0)
	{
		double attackGainCoefficient;
		switch (vehicle->unit_id)
		{
		case BSC_MIND_WORMS:
			attackGainCoefficient = conf.ai_combat_attack_bonus_alien_mind_worms;
			break;
		case BSC_SPORE_LAUNCHER:
			attackGainCoefficient = conf.ai_combat_attack_bonus_alien_spore_launcher;
			break;
		case BSC_FUNGAL_TOWER:
			attackGainCoefficient = conf.ai_combat_attack_bonus_alien_fungal_tower;
			break;
		default:
			attackGainCoefficient = 0.0;
		}
		attackGain = attackGainCoefficient * aiFactionInfo->averageBaseGain;
	}
	else if (isHostile(aiFactionId, factionId))
	{
		if (isColonyVehicle(vehicleId))
		{
			attackGain = 1.0 * aiFactionInfo->averageBaseGain;
		}
		else if (isFormerVehicle(vehicleId))
		{
			attackGain = 0.2 * aiFactionInfo->averageBaseGain;
		}
		else
		{
			double destructionCoefficient;
			
			if (isCombatVehicle(vehicleId))
			{
				double offenseCoefficient = 1.0 * (offenseValue > 0 ? (double) offenseValue / (double) aiFactionInfo->maxConDefenseValue : offenseValue < 0 ? 1.0 : 0.0);
				double defenseCoefficient = 0.5 * (defenseValue > 0 ? (double) defenseValue / (double) aiFactionInfo->maxConOffenseValue : defenseValue < 0 ? 1.0 : 0.0);
				destructionCoefficient = 0.5 * (offenseCoefficient + defenseCoefficient);
			}
			else if (isProbeVehicle(vehicleId))
			{
				destructionCoefficient = 2.0;
			}
			else if (isTransportVehicle(vehicleId))
			{
				destructionCoefficient = 2.0;
			}
			else
			{
				destructionCoefficient = 0.5;
			}
			
			attackGain = destructionCoefficient * conf.ai_combat_attack_bonus_hostile * aiFactionInfo->averageBaseGain;
			
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
	return getCombatUnitTrueCost(Vehs[vehicleId].unit_id);
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
//	period = std::max(1.0, period);
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
		
		for (int stackVechileId : getStackVehicleIds(vehicleId))
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
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && defenderUnit->chassis_id == CHS_NEEDLEJET && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// different surface triads cannot attack each other
	
	if ((attackerTriad == TRIAD_LAND && defenderTriad == TRIAD_SEA) || (attackerTriad == TRIAD_SEA && defenderTriad == TRIAD_LAND))
		return false;
	
	// all checks passed
	
	return true;
	
}

/*
Player unit can melee attack.
*/
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
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && targetTileInfo.unfriendlyNeedlejetInFlights.at(aiFactionId) && !isUnitHasAbility(unitId, ABL_AIR_SUPERIORITY))
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

/*
Player unit can attack enemy at the tile.
*/
bool isUnitCanMeleeAttackTile(int unitId, MAP *target, MAP *position) // TODO remove
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(unitId))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && targetTileInfo.unfriendlyNeedlejetInFlights.at(aiFactionId) && !isUnitHasAbility(unitId, ABL_AIR_SUPERIORITY))
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
	return isUnitCanMeleeAttackTile(Vehs[vehicleId].unit_id, target, position);
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

/*
Computes assault effect at specific tile.
Assailant tries to capture tile.
Protector tries to protect tile.
One of the parties should be an AI unit to utilize combat effect table.
*/
double computeAssaultEffect(int assailantFactionId, int assailantUnitId, int protectorFactionId, int protectorUnitId, MAP *tile)
{
	assert(assailantFactionId >= 0 && assailantFactionId < MaxPlayerNum);
	assert(assailantUnitId >= 0 && assailantUnitId < MaxProtoNum);
	assert(protectorFactionId >= 0 && protectorFactionId < MaxPlayerNum);
	assert(protectorUnitId >= 0 && protectorUnitId < MaxProtoNum);
	assert(protectorFactionId != assailantFactionId);
	assert(protectorFactionId == aiFactionId || assailantFactionId == aiFactionId);
	
	int assailantSpeed = getUnitSpeed(assailantFactionId, assailantUnitId);
	int protectorSpeed = getUnitSpeed(protectorFactionId, protectorUnitId);
	
	bool assailantMelee = isUnitCanMeleeAttackUnitAtTile(assailantUnitId, protectorUnitId, tile);
	bool protectorMelee = isUnitCanMeleeAttackUnitFromTile(protectorUnitId, assailantUnitId, tile);
	bool assailantArtillery = isUnitCanArtilleryAttackUnitAtTile(assailantUnitId, protectorUnitId, tile);
	bool protectorArtillery = isUnitCanArtilleryAttackUnitFromTile(protectorUnitId, assailantUnitId, tile);
	
	// compute combat
	
	double assaultEffect = 0.0;
	
	if (!assailantMelee && !assailantArtillery)
	{
		// impotent assailant
		
		assaultEffect = 0.0;
		
	}
	else if (assailantMelee && !assailantArtillery)
	{
		// melee assailant
		
		if (!protectorMelee && !protectorArtillery)
		{
			// impotent protector
			
			double assailantMeleeEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_MELEE, tile, true);
			
			double assailantAttackEffect = assailantMeleeEffect;
			
			assaultEffect = assailantAttackEffect;
			
		}
		else if (protectorMelee && !protectorArtillery)
		{
			// melee combat
			
			double assailantMeleeEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_MELEE, tile, true);
			double protectorMeleeEffect = getTileCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, EM_MELEE, tile, false);
			
			double assailantAttackEffect = assailantMeleeEffect;
			double assailantDefendEffect = 1.0 / protectorMeleeEffect;
			
			if (assailantAttackEffect <= assailantDefendEffect)
			{
				// assailant is better in defense than in offense
				// defense does not contributes to the assault
				assaultEffect = assailantAttackEffect;
			}
			else
			{
				// assailant is better in defense than in offense
				double speedCoefficient = getProportionalCoefficient(1, protectorSpeed + 1, assailantSpeed);
				assaultEffect = assailantDefendEffect + speedCoefficient * (assailantAttackEffect - assailantDefendEffect);
			}
			
		}
		else if (!protectorMelee && protectorArtillery)
		{
			// melee vs. artillery
			
			double assailantMeleeEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_MELEE, tile, true);
			double protectorBombardEffect = getTileCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, EM_ARTILLERY, tile, false);
			
			// pre attack bobardment round
			if (assailantSpeed <= 2)
			{
				assailantMeleeEffect *= std::max(0.0, 1.0 - protectorBombardEffect);
			}
			
			double assailantAttackEffect = assailantMeleeEffect;
			
			// assailant attacks
			// not attacking is useless against artillery
			assaultEffect = assailantAttackEffect;
			
		}
		else if (protectorMelee && protectorArtillery)
		{
			// melee vs. ship
			
			double assailantMeleeEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_MELEE, tile, true);
			double protectorMeleeEffect = getTileCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, EM_MELEE, tile, false);
			double protectorBombardEffect = getTileCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, EM_ARTILLERY, tile, false);
			
			// pre attack bobardment round
			if (assailantSpeed <= 2)
			{
				assailantMeleeEffect *= std::max(0.0, 1.0 - protectorBombardEffect);
			}
			
			double assailantAttackEffect = assailantMeleeEffect;
			double assailantDefendEffect = 1.0 / protectorMeleeEffect;
			
			// assailant attacks
			// not attacking is useless against artillery
			double speedCoefficient = getProportionalCoefficient(1, protectorSpeed + 1, assailantSpeed);
			assaultEffect = assailantDefendEffect + speedCoefficient * (assailantAttackEffect - assailantDefendEffect);
			
		}
		
	}
	else if (!assailantMelee && assailantArtillery)
	{
		// artillery assailant
		
		if (!protectorMelee && !protectorArtillery)
		{
			// impotent protector
			
			// infinitely bombarded
			assaultEffect = INF;
			
		}
		else if (protectorMelee && !protectorArtillery)
		{
			// artillery vs. melee
			
			double assailantBombardEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_ARTILLERY, tile, true);
			double protectorMeleeEffect = getTileCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, EM_MELEE, tile, false);
			
			// pre attack bobardment round
			if (assailantSpeed > protectorSpeed - 1)
			{
				protectorMeleeEffect *= std::max(0.0, 1.0 - assailantBombardEffect);
			}
			
			double assailantDefendEffect = 1.0 / protectorMeleeEffect;
			
			// protector attacks
			// not attacking is useless against artillery
			assaultEffect = assailantDefendEffect;
			
		}
		else if (!protectorMelee && protectorArtillery)
		{
			// artillery vs. artillery
			
			double assailantArtilleryDuelEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_ARTILLERY, tile, true);
			
			// artillery duel is symmetric
			assaultEffect = assailantArtilleryDuelEffect;
			
		}
		else if (protectorMelee && protectorArtillery)
		{
			// artillery vs. ship
			
			double assailantArtilleryDuelEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_ARTILLERY, tile, true);
			double protectorMeleeEffect = getTileCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, EM_MELEE, tile, false);
			double protectorArtilleryDuelEffect = assailantArtilleryDuelEffect;
			
			double assailantAttackEffect = assailantArtilleryDuelEffect;
			double assailantDefendEffect = 1.0 / std::max(protectorMeleeEffect, protectorArtilleryDuelEffect);
			
			if (assailantAttackEffect <= assailantDefendEffect)
			{
				// assailant is better in defense than in offense
				// defense does not contributes to the assault
				assaultEffect = assailantAttackEffect;
			}
			else
			{
				// assailant is better in defense than in offense
				double speedCoefficient = getProportionalCoefficient(1, protectorSpeed + 1, assailantSpeed);
				assaultEffect = assailantDefendEffect + speedCoefficient * (assailantAttackEffect - assailantDefendEffect);
			}
			
		}
		
	}
	else if (assailantMelee && assailantArtillery)
	{
		// ship assailant
		
		if (!protectorMelee && !protectorArtillery)
		{
			// impotent protector
			
			// infinitely bombarded
			assaultEffect = INF;
			
		}
		else if (protectorMelee && !protectorArtillery)
		{
			// ship vs. melee
			
			double assailantMeleeEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_MELEE, tile, true);
			double assailantBombardEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_ARTILLERY, tile, true);
			double protectorMeleeEffect = getTileCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, EM_MELEE, tile, false);
			
			// pre attack bobardment round
			if (assailantSpeed > protectorSpeed - 1)
			{
				protectorMeleeEffect *= std::max(0.0, 1.0 - assailantBombardEffect);
			}
			
			double assailantAttackEffect = assailantMeleeEffect;
			double assailantDefendEffect = 1.0 / protectorMeleeEffect;
			
			if (assailantAttackEffect <= assailantDefendEffect)
			{
				// assailant is better in defense than in offense
				// defense does not contributes to the assault
				assaultEffect = assailantAttackEffect;
			}
			else
			{
				// assailant is better in defense than in offense
				double speedCoefficient = getProportionalCoefficient(1, protectorSpeed + 1, assailantSpeed);
				assaultEffect = assailantDefendEffect + speedCoefficient * (assailantAttackEffect - assailantDefendEffect);
			}
			
		}
		else if (!protectorMelee && protectorArtillery)
		{
			// ship vs. artillery
			
			double assailantMeleeEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_MELEE, tile, true);
			double assailantArtilleryDuelEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_ARTILLERY, tile, true);
			double protectorArtilleryDuelEffect = assailantArtilleryDuelEffect;
			
			double assailantAttackEffect = std::max(assailantMeleeEffect, assailantArtilleryDuelEffect);
			double assailantDefendEffect = 1.0 / protectorArtilleryDuelEffect;
			
			if (assailantAttackEffect <= assailantDefendEffect)
			{
				// assailant is better in defense than in offense
				// defense does not contributes to the assault
				assaultEffect = assailantAttackEffect;
			}
			else
			{
				// assailant is better in defense than in offense
				double speedCoefficient = getProportionalCoefficient(1, protectorSpeed + 1, assailantSpeed);
				assaultEffect = assailantDefendEffect + speedCoefficient * (assailantAttackEffect - assailantDefendEffect);
			}
			
		}
		else if (protectorMelee && protectorArtillery)
		{
			// ship vs. ship
			
			double assailantMeleeEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_MELEE, tile, true);
			double assailantArtilleryDuelEffect = getTileCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, EM_ARTILLERY, tile, true);
			double protectorMeleeEffect = getTileCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, EM_MELEE, tile, false);
			double protectorArtilleryDuelEffect = assailantArtilleryDuelEffect;
			
			double assailantAttackEffect = std::max(assailantMeleeEffect, assailantArtilleryDuelEffect);
			double assailantDefendEffect = 1.0 / std::max(protectorMeleeEffect, protectorArtilleryDuelEffect);
			
			if (assailantAttackEffect <= assailantDefendEffect)
			{
				// assailant is better in defense than in offense
				// defense does not contributes to the assault
				assaultEffect = assailantAttackEffect;
			}
			else
			{
				// assailant is better in defense than in offense
				double speedCoefficient = getProportionalCoefficient(1, protectorSpeed + 1, assailantSpeed);
				assaultEffect = assailantDefendEffect + speedCoefficient * (assailantAttackEffect - assailantDefendEffect);
			}
			
		}
		
	}
	
	return assaultEffect;
	
}

/*
Checks if unit can melee attack at tile.
*/
bool isUnitCanMeleeAttackAtTile(int unitId, MAP *tile)
{
	assert(unitId >= 0 && unitId < MaxProtoNum);
	assert(tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles);
	
	int triad = Units[unitId].triad();
	
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(unitId))
		return false;
	
	// triad
	
	switch (triad)
	{
	case TRIAD_AIR:
		
		// can attack any unit in any tile
		
		break;
		
	case TRIAD_SEA:
		
		if (unitId == BSC_SEALURK)
		{
			// sealurk can attack sea and land
		}
		else
		{
			// sea can attack sea only
			
			if (!tile->is_sea())
				return false;
			
		}
		
		break;
		
	case TRIAD_LAND:
		
		if (isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
		{
			// amphibious cannot attack open sea
			
			if (tile->is_sea() && !tile->is_base())
				return false;
			
		}
		else
		{
			// land can attack land only
			
			if (!tile->is_land())
				return false;
			
		}
		
		break;
		
	}
	
	// all checks passed
	
	return true;
	
}

/*
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
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && !tileInfo.airbase && defenderUnit->chassis_id == CHS_NEEDLEJET && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
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

/*
Checks if unit can attack another unit from a certain tile.
It is supposed the other tile realm is same.
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
	
	// unit cannot melee attack from the tile of different realm
	
	if ((attackerTriad == TRIAD_SEA && !tileInfo.ocean) || (attackerTriad == TRIAD_LAND && !tileInfo.land))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	// assuming the land around tile is an open field
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && defenderUnit->chassis_id == CHS_NEEDLEJET && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
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

/*
Checks if unit can artillery attack tile.
*/
bool isUnitCanArtilleryAttackAtTile(int unitId, MAP */*tile*/)
{
	assert(unitId >= 0 && unitId < MaxProtoNum);
	
	// non artillery unit cannot bombard
	
	if (!isArtilleryUnit(unitId))
		return false;
	
	// all checks passed
	
	return true;
	
}

/*
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
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && !tileInfo.airbase && defenderUnit->triad() == TRIAD_AIR && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
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
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && defenderUnit->triad() == TRIAD_AIR && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
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
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && defenderUnit->chassis_id == CHS_NEEDLEJET && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
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
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && defenderUnit->triad() == TRIAD_AIR && !isUnitHasAbility(attackerUnitId, ABL_AIR_SUPERIORITY))
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
		
		double travelTime = getUnitApproachTime(aiFactionId, unitId, origin, positionTile);
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
		
		double travelTime = getUnitApproachTime(aiFactionId, unitId, origin, positionTile);
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
	else if (combatEffect >= DBL_MAX)
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

void setTileBlockedAndZoc(TileInfo &tileInfo)
{
	// blocked
	
	tileInfo.blocked =
		// unfriendly base blocks
		tileInfo.unfriendlyBase
		||
		// unfriendly vehicle blocks
		tileInfo.unfriendlyVehicle
	;
	
	// zoc
	
	tileInfo.orgZoc =
		// not base
		!tileInfo.base
		&&
		// unfriendly vehicle zoc
		tileInfo.unfriendlyVehicleZoc
	;
	
	tileInfo.dstZoc =
		// not base
		!tileInfo.base
		&&
		// not friendly vehicle
		!tileInfo.friendlyVehicle
		&&
		// unfriendly vehicle zoc
		tileInfo.unfriendlyVehicleZoc
	;
	
}

double getContinuousImprovmentGain()
{
	int nutrient = ResInfo->improved_land.nutrient;
	int mineral = 0;
	int energy = 2 + has_tech(getTerraform(FORMER_ECH_MIRROR)->preq_tech, aiFactionId);
	double resourceScore = getResourceScore(nutrient, mineral, energy);
	double incomeGain = getGainIncome(resourceScore);
	double continuousImprovmentGain = aiData.developmentScale * incomeGain;
	return continuousImprovmentGain;
}

double getContinuousHarvestingGain()
{
	int nutrient = 0;
	int mineral = 4 - getUnitSupport(BSC_SUPPLY_CRAWLER);
	int energy = 0;
	double resourceScore = getResourceScore(nutrient, mineral, energy);
	double incomeGain = getGainIncome(resourceScore);
	return incomeGain;
}

double getStealingTechnologyGain()
{
	int nutrient = 0;
	int mineral = 0;
	int energy = aiFaction->tech_cost;
	double resourceScore = getResourceScore(nutrient, mineral, energy);
	double bonusGain = getGainBonus(resourceScore);
	return bonusGain;
}

double getCachingArtifactGain()
{
	int nutrient = 0;
	int mineral = Rules->mineral_cost_multi * Units[BSC_ALIEN_ARTIFACT].cost;
	int energy = 0;
	double resourceScore = getResourceScore(nutrient, mineral, energy);
	double bonusGain = getGainBonus(resourceScore);
	return bonusGain;
}

/*
Estimates unit destruction value.
pillaging
psi defense
psi offense
con defense
con offense
*/
double getUnitDestructionGain(int unitId)
{
	return isCombatUnit(unitId) ? getCombatUnitDestructionGain(unitId) : getCivicUnitDestructionGain(unitId);
}

/*
Estimates unit destruction value.
pillaging
psi defense
psi offense
con defense
con offense
*/
double getCombatUnitDestructionGain(int unitId)
{
	double const PSI_PSI_COEFFICIENT = 1.00;
	double const PSI_CON_COEFFICIENT = 1.00 - PSI_PSI_COEFFICIENT;
	double const CON_PSI_COEFFICIENT = aiData.psiVehicleRatio;
	double const CON_CON_COEFFICIENT = 1.00 - CON_PSI_COEFFICIENT;
	
	double const OFFENCE_COEFFICIENT = 0.75;
	double const DEFENCE_COEFFICIENT = 0.25;
	
	UNIT *unit = getUnit(unitId);
	int offenseValue = unit->offense_value();
	int defenseValue = unit->defense_value();
	bool psiOffense = isUnitPsiOffense(unitId);
	bool psiDefense = isUnitPsiDefense(unitId);
	
	// psi defense
	
	double psiDefensePower = (double)(Rules->mineral_cost_multi * Units[BSC_MIND_WORMS].cost);
	if (!psiDefense)
	{
		psiDefensePower *= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * (offenseValue + defenseValue));
	}
	if (has_abil(unitId, ABL_TRANCE))
	{
		psiDefensePower *= 1.50;
	}
	
	// psi offense
	
	double psiOffensePower = (double)(Rules->mineral_cost_multi * Units[BSC_MIND_WORMS].cost);
	if (!psiOffense)
	{
		psiOffensePower *= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * (offenseValue + offenseValue));
	}
	if (has_abil(unitId, ABL_EMPATH))
	{
		psiOffensePower *= 1.50;
	}
	
	// con defense
	
	double conDefensePower;
	if (psiDefense)
	{
		conDefensePower = 0.0;
	}
	else
	{
		conDefensePower = (double)(Rules->mineral_cost_multi * defenseValue);
		if (has_abil(unitId, ABL_AAA))
		{
			conDefensePower *= 1.50;
		}
		if (has_abil(unitId, ABL_COMM_JAMMER))
		{
			conDefensePower *= 1.25;
		}
		if (unit->chassis_id == CHS_NEEDLEJET)
		{
			conDefensePower += 1.0 * (double)(Rules->mineral_cost_multi * aiData.maxConOffenseValue);
		}
	}
	
	// con offense
	
	double conOffensePower;
	if (psiOffense)
	{
		conOffensePower = 0.0;
	}
	else
	{
		conOffensePower = 1.0 * (double)(Rules->mineral_cost_multi * offenseValue);
		if (has_abil(unitId, ABL_ARTILLERY))
		{
			conOffensePower *= 2.00;
		}
		if (has_abil(unitId, ABL_AIR_SUPERIORITY))
		{
			conOffensePower *= 1.25;
		}
		if (has_abil(unitId, ABL_BLINK_DISPLACER))
		{
			conOffensePower *= 1.25;
		}
		if (has_abil(unitId, ABL_NERVE_GAS))
		{
			conOffensePower *= 1.50;
		}
		if (has_abil(unitId, ABL_SOPORIFIC_GAS))
		{
			conOffensePower *= 1.25;
		}
		if (has_abil(unitId, ABL_DISSOCIATIVE_WAVE))
		{
			conOffensePower *= 1.25;
		}
	}
	
	double combatPower =
		+ OFFENCE_COEFFICIENT * (psiOffense ? PSI_PSI_COEFFICIENT * psiOffensePower + PSI_CON_COEFFICIENT * conOffensePower : CON_PSI_COEFFICIENT * psiOffensePower + CON_CON_COEFFICIENT * conOffensePower)
		+ DEFENCE_COEFFICIENT * (psiDefense ? PSI_PSI_COEFFICIENT * psiDefensePower + PSI_CON_COEFFICIENT * conDefensePower : CON_PSI_COEFFICIENT * psiDefensePower + CON_CON_COEFFICIENT * conDefensePower)
	;
	double combatGain = getGainBonus(conf.ai_combat_strength_destruction_gain * combatPower);
	
	return combatGain;
	
}

/*
Estimates civic unit destruction value.
*/
double getCivicUnitDestructionGain(int unitId)
{
	UNIT *unit = getUnit(unitId);
	
	double destructionGain;
	
	switch (unit->weapon_id)
	{
	case WPN_COLONY_MODULE:
		destructionGain = aiData.newBaseGain;
		break;
	case WPN_TERRAFORMING_UNIT:
		// once a 10 turns
		destructionGain = aiData.continuousImprovmentGain / 10.0;
		break;
	case WPN_TROOP_TRANSPORT:
		destructionGain = aiData.newBaseGain;
		break;
	case WPN_SUPPLY_TRANSPORT:
		destructionGain = aiData.continuousHarvestingGain;
		break;
	case WPN_PROBE_TEAM:
		destructionGain = aiData.stealingTechnologyGain;
		break;
	case WPN_ALIEN_ARTIFACT:
		// enemy loss + our gain
		destructionGain = aiData.cachingArtifactGain * 2.0;
		break;
	default:
		destructionGain = 0.0;
	}
	
	return destructionGain;
	
}

/*
Returns combat effect with tile modifiers.
*/
double getTileCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, MAP *tile, bool atTile)
{
	TileInfo const &tileInfo = aiData.getTileInfo(tile);
	
	double combatEffect =
		aiData.combatEffectTable.getCombatEffect(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode)
		* tileInfo.sensorOffenseMultipliers.at(attackerFactionId)
		/ tileInfo.sensorDefenseMultipliers.at(defenderFactionId)
	;
	
	if (atTile)
	{
		AttackTriad attackTriad = getAttackTriad(attackerUnitId, defenderUnitId);
		combatEffect /= tileInfo.structureDefenseMultipliers.at(attackTriad);
	}
	
	return combatEffect;
	
}

/*
Computes combat effect with tile modifiers.
*/
double getTileCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode, MAP *tile, bool atTile)
{
	VEH &attackerVehicle = Vehs[attackerVehicleId];
	VEH &defenderVehicle = Vehs[defenderVehicleId];
	
	return getTileCombatEffect(attackerVehicle.faction_id, attackerVehicle.unit_id, defenderVehicle.faction_id, defenderVehicle.unit_id, engagementMode, tile, atTile);
	
}

/*
Returns the proportion of the given value from the lowest (0.0) to the highest (1.0) value.
*/
double getProportionalCoefficient(double minValue, double maxValue, double value)
{
	if (minValue == maxValue)
		return 0.5;
	
	return std::max(0.0, std::min(1.0, (value - minValue) / (maxValue - minValue)));
	
}

bool compareAssaultEffectAscending(AssaultEffect const &o1, AssaultEffect const &o2)
{
	return o1.value < o2.value;
}

//void initializeLocationCombatData(CombatData &combatData, TileInfo &tileInfo, bool playerAssaults)
//{
//	// reset
//	
//	CombatEffectData &combatEffectData = combatData.combatEffectData;
//	combatEffectData.clear();
//	
//	for (robin_hood::pair<int, std::array<CombatModeEffect, ENGAGEMENT_MODE_COUNT>> &combatModeEffectEntry : aiData.combatEffectData.combatModeEffects)
//	{
//		int key = combatModeEffectEntry.first;
//		std::array<CombatModeEffect, ENGAGEMENT_MODE_COUNT> &unitCombatModeEffects = combatModeEffectEntry.second;
//		
//		FactionUnitCombat factionUnitCombat(key);
//		int attackerFactionId	= factionUnitCombat.attackerFactionUnit.factionId;
//		int attackerUnitId		= factionUnitCombat.attackerFactionUnit.unitId;
//		int defenderFactionId	= factionUnitCombat.defenderFactionUnit.factionId;
//		int defenderUnitId		= factionUnitCombat.defenderFactionUnit.unitId;
//		
//		for (ENGAGEMENT_MODE engagementMode : ENGAGEMENT_MODES)
//		{
//			CombatModeEffect &combatModeEffect = unitCombatModeEffects.at(engagementMode);
//			if (!combatModeEffect.valid)
//				continue;
//			
//			// compute modified combat effect
//			
//			double attackerStrengthMultiplier = tileInfo.sensorOffenseMultipliers.at(attackerFactionId);
//			double defenderStrengthMultiplier = tileInfo.sensorDefenseMultipliers.at(defenderFactionId);
//			
//			bool structureDefense = false;
//			if (Units[attackerUnitId].triad() == TRIAD_AIR && isInterceptorUnit(defenderUnitId))
//			{
//				// structure defense does not apply to interceptor
//			}
//			else if (combatModeEffect.combatMode == CM_ARTILLERY_DUEL)
//			{
//				// structure defense applies to artillery duelant
//				structureDefense = true;
//			}
//			else if ((playerAssaults && defenderFactionId != aiFactionId) || (!playerAssaults && defenderFactionId == aiFactionId))
//			{
//				// structure defense applies to defending protector
//				structureDefense = true;
//			}
//			
//			if (structureDefense)
//			{
//				int attackTriad = getAttackTriad(attackerUnitId, defenderUnitId);
//				double structureDefenseMultiplier = tileInfo.structureDefenseMultipliers.at(attackTriad);
//				defenderStrengthMultiplier *= structureDefenseMultiplier;
//			}
//			
//			double combatEffectMultiplier = attackerStrengthMultiplier / defenderStrengthMultiplier;
//			double combatEffect = combatEffectMultiplier * combatModeEffect.value;
//			
//			combatEffectData.setCombatModeEffect(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode, combatModeEffect.combatMode, combatEffect);
//			
//		}
//		
//	}
//	
//}
//
void populateVehiclePad0Map()
{
	Profiling::start("- populateVehiclePad0Map");
	
	aiData.pad0VehicleIds.clear();
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH &vehicle = Vehs[vehicleId];
		
		if (vehicle.pad_0 == 0)
			continue;
		
		aiData.pad0VehicleIds.emplace(vehicle.pad_0, vehicleId);
		
	}
	
	Profiling::stop("- populateVehiclePad0Map");
	
}

