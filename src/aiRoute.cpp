#include <vector>
#include <set>
#include <queue>
#include <unordered_map>
#include "robin_hood.h"
#include "terranx.h"
#include "game_wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMove.h"
#include "aiMoveColony.h"
#include "aiMoveTransport.h"
#include "aiRoute.h"

// how many association aread tiles one landmark covers

const int LANDMARK_COVERAGE = 300;
const double ACCEPTABLE_LANDMARK_DISTANCE_RELATIVE_ERROR = 0.1;

// ==================================================
// Objects
// ==================================================

bool FValueComparator::operator() (const FValue &o1, const FValue &o2)
{
   return o1.f > o2.f;
}

bool FValueHeap::empty()
{
	return minF >= (int)fGroups.size();
}

void FValueHeap::add(int f, MAP *tile)
{
	// update top pointer
	
	if (minF >= (int)fGroups.size())
	{
		minF = f;
	}
	else
	{
		minF = std::min(minF, f);
	}
	
	// resize the vector to fit the value
	
	if (f >= (int)fGroups.size())
	{
		fGroups.resize(f + 1 + 1000);
	}
	
	// add value
	
	fGroups.at(f).push_back(tile);
	
}

FValue FValueHeap::top()
{
	// no more fGroups
	
	if (minF >= (int)fGroups.size())
		return {nullptr, minF};
	
	// get top
	
	int topF = minF;
	MAP *top = fGroups.at(minF).back();
	fGroups.at(minF).pop_back();
	
	// shift top pointer if needed
	
	if (fGroups.at(minF).size() == 0U)
	{
		for (minF++; minF < (int)fGroups.size(); minF++)
		{
			if (fGroups.at(minF).size() > 0U)
				break;
		}
		
	}
	
	return {top, topF};
	
}

// ==================================================
// A* cached movement costs
// ==================================================

// [origin, destination, movementType, factionId, ignoreHostile]
//robin_hood::unordered_flat_map<std::tuple<MAP *, MAP *, MovementType, int, bool>, int> cachedAMovementCosts;
robin_hood::unordered_flat_map<long long, int> cachedAMovementCosts;

long long getCachedAMovementCostKey(MAP *origin, MAP *destination, MovementType movementType, int factionId, bool ignoreHostile)
{
	long long key =
		+ (long long)(origin - *MapPtr)			* 2L * (long long)MaxPlayerNum * (long long)MOVEMENT_TYPE_COUNT * (long long)*map_area_tiles
		+ (long long)(destination - *MapPtr)	* 2L * (long long)MaxPlayerNum * (long long)MOVEMENT_TYPE_COUNT
		+ (long long)movementType				* 2L * (long long)MaxPlayerNum
		+ (long long)factionId					* 2L
		+ (long long)ignoreHostile				
	;
	
	return key;
	
}

/*
Returns cachedMovementCost or -1 if not found.
*/
int getCachedAMovementCost(long long key)
{
	return cachedAMovementCosts.count(key) == 0 ? -1 : cachedAMovementCosts.at(key);
}

void setCachedAMovementCost(long long key, int movementCost)
{
	cachedAMovementCosts[key] = movementCost;
}

// ==================================================
// Main operations
// ==================================================

void precomputeAIRouteData()
{
	executionProfiles["1.2.0. cachedAMovementCosts.clear()"].start();
	cachedAMovementCosts.clear();
	executionProfiles["1.2.0. cachedAMovementCosts.clear()"].stop();
	
	profileFunction("1.2.3. populateTileMovementRestrictions", populateTileMovementRestrictions);
	profileFunction("1.2.4. populateBaseEnemyMovementImpediments", populateBaseEnemyMovementImpediments);
	profileFunction("1.2.5. populateAIFactionMovementInfo", populateAIFactionMovementInfo);
	
}

/*
Sets tile combat movement realated info for each faction: blocked, zoc.
*/
void populateTileMovementRestrictions()
{
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		// blockNeutral, zocNeutral
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			TileInfo &tileInfo = aiData.getTileInfo(vehicleTile);
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			
			// neutral or AI for other faction
			
			if (!isNeutral(factionId, vehicle->faction_id))
				continue;
			if (factionId != aiFactionId && vehicle->faction_id == aiFactionId)
				continue;
			
			// blockedNeutral
			
			tileFactionInfo.blockedNeutral = true;
			
			// zocNeutral
			
			if (!is_ocean(vehicleTile))
			{
				for (MAP *adjacentTile : getAdjacentTiles(vehicleTile))
				{
					TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
					TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
					
					if (is_ocean(adjacentTile))
						continue;
					
					adjacentTileFactionInfo.zocNeutral = true;
					
				}
				
			}
			
		}
		
		// blocked, zoc
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			TileInfo &tileInfo = aiData.getTileInfo(vehicleTile);
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			
			// unfriendly
			
			if (isFriendly(factionId, vehicle->faction_id))
				continue;
			
			// blocked
			
			tileFactionInfo.blocked = true;
			
			// zoc
			
			if (!is_ocean(vehicleTile))
			{
				for (MAP *adjacentTile : getAdjacentTiles(vehicleTile))
				{
					TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
					TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
					
					if (is_ocean(adjacentTile))
						continue;
					
					adjacentTileFactionInfo.zoc = true;
					
				}
				
			}
			
		}
		
		// base is not zoc
		
		for (int baseId = 0; baseId < *total_num_bases; baseId++)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			TileInfo &tileInfo = aiData.getTileInfo(baseTile);
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			
			// disable zoc
			
			tileFactionInfo.zocNeutral = false;
			tileFactionInfo.zoc = false;
			
		}
		
		// friendly vehicle (except probe)
		// disables zoc
		
		for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			TileInfo &tileInfo = aiData.getTileInfo(vehicleTile);
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			
			// friendly
			
			if (!isFriendly(factionId, vehicle->faction_id))
				continue;
			
			// not probe
			
			if (isProbeVehicle(vehicleId))
				continue;
			
			// set friendly
			
			tileFactionInfo.friendly = true;
			tileFactionInfo.zocNeutral = false;
			tileFactionInfo.zoc = false;
			
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

/*
Sets tile combat movement impediment caused by nearby bases.
*/
void populateBaseEnemyMovementImpediments()
{
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
					TileFactionInfo &tileFactionInfo = aiData.getTileFactionInfo(tile, factionId);
					
					tileFactionInfo.travelImpediment += getBaseTravelImpediment(range, baseTileOcean);
					
				}
				
			}
			
		}
		
	}
	
}

/*
Populates all movemen costs from given origin to all accessible destinations.
*/
void populateBuildSiteDMovementCosts(MAP *origin, MovementType movementType, int factionId, bool ignoreHostile)
{
	executionProfiles["| populateBuildSiteDMovementCosts"].start();
	
	std::vector<int> openNodes;
	std::vector<int> newOpenNodes;
	
	// initialize collections
	
	long long originToOriginMovementCostKey = getCachedAMovementCostKey(origin, origin, movementType, factionId, ignoreHostile);
	setCachedAMovementCost(originToOriginMovementCostKey, 0);
	int originTileIndex = origin - *MapPtr;
	openNodes.push_back(originTileIndex);
	
	// process nodes
	
	while (!openNodes.empty())
	{
		for (int tileIndex : openNodes)
		{
			MAP *tile = *MapPtr + tileIndex;
			TileInfo &tileInfo = aiData.tileInfos[tileIndex];
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			bool tileZoc = (ignoreHostile ? tileFactionInfo.zocNeutral : tileFactionInfo.zoc);
			
			// cached value
			
			long long originToTileMovementCostKey = getCachedAMovementCostKey(origin, tile, movementType, factionId, ignoreHostile);
			int tileMovementCost = getCachedAMovementCost(originToTileMovementCostKey);
			
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				int adjacentTileIndex = tileInfo.adjacentTileIndexes[angle];
				
				if (adjacentTileIndex == -1)
					continue;
				
				MAP *adjacentTile = *MapPtr + adjacentTileIndex;
				
				TileInfo &adjacentTileInfo = aiData.tileInfos[adjacentTileIndex];
				TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
				ExpansionTileInfo &adjacentTileExpansionInfo = getExpansionTileInfo(adjacentTile);
				bool adjacentTileBlocked = (ignoreHostile ? adjacentTileFactionInfo.blockedNeutral : adjacentTileFactionInfo.blocked);
				bool adjacentTileZoc = (ignoreHostile ? adjacentTileFactionInfo.zocNeutral : adjacentTileFactionInfo.zoc);
				
				// hex cost
				
				int hexCost = tileInfo.hexCosts1[movementType][angle];
				
				// not allowed move
				
				if (hexCost == -1)
					continue;
				
				// valid build site
				
				if (!adjacentTileExpansionInfo.expansionRange)
					continue;
				
				// cached value
				
				long long originToAdjacentTileMovementCostKey = getCachedAMovementCostKey(origin, tile, movementType, factionId, ignoreHostile);
				int adjacentTileMovementCost = getCachedAMovementCost(originToAdjacentTileMovementCostKey);
				
				// movementCost
				
				int computedMovementCost = (adjacentTileBlocked || tileZoc && adjacentTileZoc ? MOVEMENT_INFINITY : tileMovementCost + hexCost);
				
				// update cached value and add tile to open nodes if better estimate
				
				if (adjacentTileMovementCost == -1 || computedMovementCost < adjacentTileMovementCost)
				{
					setCachedAMovementCost(originToAdjacentTileMovementCostKey, computedMovementCost);
					newOpenNodes.push_back(adjacentTileIndex);
				}
				
			}
			
		}
		
		// swap nodes
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	executionProfiles["| populateBuildSiteDMovementCosts"].stop();
	
}

/*
Populates movement costs for existing and newly build colonies.
*/
void populateColonyMovementCosts()
{
	bool TRACE = DEBUG && false;
	
	debug("populateColonyMovementCosts TRACE=%d\n", TRACE);
	
	// bases
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		int baseLandAssociation = getLandAssociation(baseTile, aiFactionId);
		int baseOceanAssociation = getOceanAssociation(baseTile, aiFactionId);
		
		std::vector<int> triads;
		triads.push_back(TRIAD_AIR);
		if (baseOceanAssociation != -1)
		{
			triads.push_back(TRIAD_SEA);
		}
		if (baseLandAssociation != -1)
		{
			triads.push_back(TRIAD_LAND);
		}
		
		for (int unitId : aiData.colonyUnitIds)
		{
			MovementType movementType = getUnitMovementType(aiFactionId, unitId);
			
			populateBuildSiteDMovementCosts(baseTile, movementType, aiFactionId, false);
			
		}
		
	}
	
	// colonies
	
	for (int vehicleId : aiData.colonyVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int unitId = vehicle->unit_id;
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		MovementType movementType = getUnitMovementType(aiFactionId, unitId);
		
		populateBuildSiteDMovementCosts(vehicleTile, movementType, aiFactionId, false);
		
	}
	
}

/*
Returns all location where this vehicle can move and execute action on same turn.
*/
robin_hood::unordered_flat_set<MAP *> getOneTurnActionLocations(int vehicleId)
{
	debug("getOneTurnActionLocations\n");
	
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int vehicleTileIndex = vehicleTile - *MapPtr;
	MovementType movementType = getUnitMovementType(factionId, unitId);
	int speed = getVehicleSpeed(vehicleId);
	
	robin_hood::unordered_flat_map<MAP *, int> movementCosts;
	std::vector<int> openNodes;
	std::vector<int> newOpenNodes;
	
	movementCosts.emplace(vehicleTile, 0);
	openNodes.push_back(vehicleTileIndex);
	
	while (!openNodes.empty())
	{
		for (int tileIndex : openNodes)
		{
			MAP *tile = *MapPtr + tileIndex;
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			TileFactionInfo &tileFactionInfo = aiData.getTileFactionInfo(tile, factionId);
			bool tileZoc = tileFactionInfo.zoc;
			int tileMovementCost = movementCosts.at(tile);
			
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				int adjacentTileIndex = tileInfo.adjacentTileIndexes[angle];
				
				if (adjacentTileIndex == -1)
					continue;
				
				MAP *adjacentTile = *MapPtr + adjacentTileIndex;
				
				TileFactionInfo &adjacentTileFactionInfo = aiData.getTileFactionInfo(adjacentTile, factionId);
				bool adjacentTileBlocked = adjacentTileFactionInfo.blocked;
				bool adjacentTileZoc = adjacentTileFactionInfo.zoc;
				int hexCost = tileInfo.hexCosts1[movementType][angle];
				
				// exclude inaccessible location
				
				if (adjacentTileBlocked || tileZoc && adjacentTileZoc || hexCost == -1)
					continue;
				
				// movementCost
				
				int movementCost = tileMovementCost + hexCost;
				
				// limit condition
				// movementCost + 1 should not exceed speed
				
				if (movementCost + 1 > speed)
					continue;
				
				// update cost and add new open node if better
				
				if (movementCosts.count(adjacentTile) == 0 || movementCost < movementCosts.at(adjacentTile))
				{
					movementCosts[adjacentTile] = movementCost;
					newOpenNodes.push_back(adjacentTileIndex);
				}
				
			}
			
		}
		
		// swap nodes
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	// collect locations
	
	robin_hood::unordered_flat_set<MAP *> locations;
	
	for (robin_hood::pair<MAP *, int> &movementCostEntry : movementCosts)
	{
		locations.insert(movementCostEntry.first);
	}
	
	return locations;
	
}

// ==================================================
// Generic algorithms
// ==================================================

// ==================================================
// A* path finding algorithm
// ==================================================

int getAMovementCost(MAP *origin, MAP *destination, MovementType movementType, int factionId, bool ignoreHostile)
{
	executionProfiles["| getAMovementCost"].start();
	
	const bool TRACE = DEBUG && false;
	
	if (TRACE)
	{
		debug
		(
			"getAMovementCost"
			" (%3d,%3d)->(%3d,%3d)"
			" movementType=%d"
			" factionId=%d"
			" ignoreHostile=%d"
			"\n"
			, getX(origin), getY(origin), getX(destination), getY(destination)
			, movementType
			, factionId
			, ignoreHostile
		);
	}
	
	// at destination
	
	if (origin == destination)
	{
		executionProfiles["| getAMovementCost"].stop();
		return 0;
	}
	
	// check cached value
	
	executionProfiles["| getAMovementCost 1. check cached value"].start();
	
	long long originToDestinationMovementCostKey = getCachedAMovementCostKey(origin, destination, movementType, factionId, ignoreHostile);
	int cachedMovementCost = getCachedAMovementCost(originToDestinationMovementCostKey);
	
	executionProfiles["| getAMovementCost 1. check cached value"].stop();
	
	if (cachedMovementCost != -1)
	{
		executionProfiles["| getAMovementCost"].stop();
		return cachedMovementCost;
	}
	
	// initialize collections
	
	std::priority_queue<FValue, std::vector<FValue>, FValueComparator> openNodes;
	
	long long originToOriginMovementCostKey = getCachedAMovementCostKey(origin, origin, movementType, factionId, ignoreHostile);
	setCachedAMovementCost(originToOriginMovementCostKey, 0);
	int originF = getRouteVectorDistance(origin, destination);
	openNodes.push({origin, originF});
	
	// process path
	
	executionProfiles["| getAMovementCost 2. process path"].start();
	
	while (!openNodes.empty())
	{
		// get top node and remove it from list
		
		FValue currentOpenNode = openNodes.top();
		openNodes.pop();
		
		if (TRACE)
		{
			debug
			(
				"\tcurrentOpenNode (%3d,%3d)"
				" f=%4d"
				"\n"
				, getX(currentOpenNode.tile), getY(currentOpenNode.tile)
				, currentOpenNode.f
			);
		}
		
		// get current tile data
		
		MAP *currentTile = currentOpenNode.tile;
		TileInfo &currentTileInfo = aiData.getTileInfo(currentTile);
		TileFactionInfo &currentTileFactionInfo = currentTileInfo.factionInfos[factionId];
		bool currentTileZoc = (ignoreHostile ? currentTileFactionInfo.zocNeutral : currentTileFactionInfo.zoc);
		
		long long originToCurrentTileMovementCostKey = getCachedAMovementCostKey(origin, currentTile, movementType, factionId, ignoreHostile);
		int currentTileMovementCost = getCachedAMovementCost(originToCurrentTileMovementCostKey);
		
		// check surrounding tiles
		
		for (int angle = 0; angle < ANGLE_COUNT; angle++)
		{
			MAP *adjacentTile = getTileByAngle(currentTile, angle);
			
			if (adjacentTile == nullptr)
				continue;
			
			// hex cost
			
			int adjacentTileHexCost = currentTileInfo.hexCosts1[movementType][angle];
			
			// allowed step
			
			if (adjacentTileHexCost == -1)
				continue;
			
			// parameters
			
			TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
			TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
			bool adjacentTileBlocked = (ignoreHostile ? adjacentTileFactionInfo.blockedNeutral : adjacentTileFactionInfo.blocked);
			bool adjacentTileZoc = (ignoreHostile ? adjacentTileFactionInfo.zocNeutral : adjacentTileFactionInfo.zoc);
			
			long long originToAdjacentTileMovementCostKey = getCachedAMovementCostKey(origin, adjacentTile, movementType, factionId, ignoreHostile);
			int adjacentTileMovementCost = getCachedAMovementCost(originToAdjacentTileMovementCostKey);
			
			if (TRACE)
			{
				debug
				(
					"\t\tadjacentTile (%3d,%3d)"
					" adjacentTileBlocked=%d"
					" adjacentTileZoc=%d"
					"\n"
					, getX(adjacentTile), getY(adjacentTile)
					, adjacentTileBlocked
					, adjacentTileZoc
				);
			}
			
			// inaccessible: blocked or zoc
			
			if (adjacentTileBlocked || currentTileZoc && adjacentTileZoc)
			{
				continue;
			}
			
			// movementCost
			
			int movementCost = currentTileMovementCost + adjacentTileHexCost;
			
			// update adjacentTile cachedMovementCost
			
			if (adjacentTileMovementCost == -1 || movementCost < adjacentTileMovementCost)
			{
				setCachedAMovementCost(originToAdjacentTileMovementCostKey, movementCost);
				
				// f value
				// take cashed movement cost if it is known, otherwise estimate
				
				long long adjacentTileToDestinationMovementCostKey =
					getCachedAMovementCostKey(adjacentTile, destination, movementType, factionId, ignoreHostile);
				int adjacentTileDestinationMovementCost = getCachedAMovementCost(adjacentTileToDestinationMovementCostKey);
				
				int adjacentTileF =
					adjacentTileDestinationMovementCost != -1 ?
						adjacentTileDestinationMovementCost
						:
						movementCost + getRouteVectorDistance(adjacentTile, destination)
				;
				
				if (TRACE)
				{
					debug
					(
						"\t\t\tadjacentTileMovementCost=%2d"
						" adjacentTileF=%4d"
						"\n"
						, adjacentTileMovementCost
						, adjacentTileF
					);
				}
				
				openNodes.push({adjacentTile, adjacentTileF});
				
			}
			
		}
		
	}
	
	executionProfiles["| getAMovementCost 2. process path"].stop();
	
	executionProfiles["| getAMovementCost 3. reconstruct path"].start();
	
	// retrieve cache value that should be already there
	
	int movementCost = getCachedAMovementCost(originToDestinationMovementCostKey);
	
	if (movementCost == -1)
	{
		movementCost = MOVEMENT_INFINITY;
	}
	
	executionProfiles["| getAMovementCost"].stop();
	return movementCost;
	
}

/*
Estimates unit movement cost based on unit type.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getUnitATravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool ignoreHostile, int action)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	UNIT *unit = getUnit(unitId);
	
	// compute travel time by chassis type
	
	int travelTime = MOVEMENT_INFINITY;
	
	switch (unit->chassis_type)
	{
	case CHS_GRAVSHIP:
		travelTime = getGravshipUnitTravelTime(origin, destination, speed, action);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		travelTime = getAirRangedUnitTravelTime(origin, destination, speed, action);
		break;
	case CHS_FOIL:
	case CHS_CRUISER:
		travelTime = getSeaUnitATravelTime(origin, destination, factionId, unitId, speed, ignoreHostile, action);
		break;
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		travelTime = getLandUnitATravelTime(origin, destination, factionId, unitId, speed, ignoreHostile, action);
		break;
	}
	
	return travelTime;
	
}

/*
Estimates sea vehicle combat travel time not counting any obstacles.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getSeaUnitATravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool ignoreHostile, bool action)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	// verify same ocean association
	
	if (!isSameOceanAssociation(origin, destination, factionId))
		return -1;
	
	// movementCost
	
	MovementType movementType = getUnitMovementType(factionId, unitId);
	
	int movementCost = getAMovementCost(origin, destination, movementType, factionId, ignoreHostile);
	
	// unreachable
	
	if (movementCost == MOVEMENT_INFINITY)
		return MOVEMENT_INFINITY;
	
	// total actionCost
	
	int actionCost = movementCost + action;
	
	// travelTime
	
	return divideIntegerRoundUp(actionCost, speed);
	
}

/*
Estimates land vehicle combat travel time not counting any obstacles.
Land vehicle travels within its association and fights through enemies.
Optionally it can be transported across associations.
*/
int getLandUnitATravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool ignoreHostile, bool action)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	int speedOnLand = speed / Rules->mov_rate_along_roads;
	int speedOnRoad = speed / conf.tube_movement_rate_multiplier;
	int speedOnTube = speed;
	
	// evaluate different cases
	
	if (isSameLandAssociation(origin, destination, factionId)) // same continent
	{
		// movementCost
		
		MovementType movementType = getUnitMovementType(factionId, unitId);
		
		int movementCost = getAMovementCost(origin, destination, movementType, factionId, ignoreHostile);
		
		// unreachable
		
		if (movementCost == MOVEMENT_INFINITY)
			return MOVEMENT_INFINITY;
		
		// total actionCost
		
		int actionCost = movementCost + action;
		
		// travelTime
		
		return divideIntegerRoundUp(actionCost, speed);
		
	}
	else
	{
		// alien cannot travel between landmasses
		
		if (factionId == 0)
			return MOVEMENT_INFINITY;
		
		// both associations are reachable
		
		if (!isPotentiallyReachable(origin, factionId) || !isPotentiallyReachable(destination, factionId))
			return MOVEMENT_INFINITY;
		
		// range
		
		int range = getRange(origin, destination);
		
		// cross ocean association
		
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
		
		int travelTime = (int)((double)range / (0.75 * averageMovementSpeed));
		
		// add ferry wait time if not on transport already
		
		travelTime += 15;
		
		return travelTime;
		
	}
	
}

/*
Estimates unit travel time using A* algorithm.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getVehicleATravelTime(int vehicleId, MAP *origin, MAP *destination, bool ignoreHostile, int action)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	UNIT *unit = getUnit(unitId);
	int speed = getVehicleSpeed(vehicleId);
	
	int travelTime = -1;
	
	switch (unit->chassis_type)
	{
	case CHS_GRAVSHIP:
		travelTime = getGravshipUnitTravelTime(origin, destination, speed, action);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		travelTime = getAirRangedUnitTravelTime(origin, destination, speed, action);
		break;
	case CHS_FOIL:
	case CHS_CRUISER:
		travelTime = getSeaUnitATravelTime(origin, destination, factionId, unitId, speed, ignoreHostile, action);
		break;
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		travelTime = getLandVehicleATravelTime(vehicleId, origin, destination, ignoreHostile, action);
		break;
	}
	
	return travelTime;
	
}

/*
Estimates land unit travel time using A* algorithm.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getLandVehicleATravelTime(int vehicleId, MAP *origin, MAP *destination, bool ignoreHostile, int action)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	int speed = getVehicleSpeed(vehicleId);
	
	int travelTime = MOVEMENT_INFINITY;
	
	if (isSameLandAssociation(origin, destination, factionId)) // same continent
	{
		travelTime = getUnitATravelTime(origin, destination, factionId, unitId, speed, ignoreHostile, action);
	}
	else // not same continent
	{
		// transportId
		
		int transportVehicleId = getVehicleTransportId(vehicleId);
		
		if (transportVehicleId != -1) // transported
		{
			bool seaTransport = isSeaTransportVehicle(transportVehicleId);
			
			if (!seaTransport) // not sea transport
			{
				// don't know what to do with it
				travelTime = getUnitATravelTime(origin, destination, factionId, unitId, speed, ignoreHostile, action);
			}
			else // sea transport
			{
				VEH *transportVehicle = getVehicle(transportVehicleId);
				int transportUnitId = transportVehicle->unit_id;
				int transportSpeed = getVehicleSpeed(transportVehicleId);
				MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);
				int transportOceanAssociation = getVehicleAssociation(transportVehicleId);
				bool destinationOcean = is_ocean(destination);
				int destinationAssociation = getAssociation(destination, factionId);
				
				if (destinationOcean && isSameOceanAssociation(transportVehicleTile, destination, factionId)) // same ocean
				{
					// travel time equals transport travel time
					travelTime = getUnitATravelTime(origin, destination, factionId, transportUnitId, transportSpeed, ignoreHostile, action);
				}
				else if (!destinationOcean && isConnected(transportOceanAssociation, destinationAssociation, factionId)) // connected continent
				{
					// roughly estimate travel time
					
					int range = getRange(origin, destination);
					
					double averageMovementSpeed =
						+ 0.5 * (double)(speed / Rules->mov_rate_along_roads)
						+ 0.5 * (double)(transportSpeed / Rules->mov_rate_along_roads)
					;
					
					travelTime = (int)ceil((double)range / averageMovementSpeed);
					
				}
				else // remote association
				{
					// roughly estimate travel time
					
					int range = getRange(origin, destination);
					
					double averageMovementSpeed =
						+ 0.5 * (double)(speed / Rules->mov_rate_along_roads)
						+ 0.5 * (double)(transportSpeed / Rules->mov_rate_along_roads)
					;
					
					travelTime = (int)ceil((double)range / averageMovementSpeed + 10.0);
					
				}
				
			}
			
		}
		else // not transported
		{
			// roughly estimate travel time
			
			double estimatedTransportMovementSpeed = 4.0;
			
			int range = getRange(origin, destination);
			
			double averageMovementSpeed =
				+ 0.5 * (double)(speed / Rules->mov_rate_along_roads)
				+ 0.5 * estimatedTransportMovementSpeed
			;
			
			travelTime = (int)ceil((double)range / averageMovementSpeed + 10.0);
			
		}
		
	}
	
	return travelTime;
	
}

int getVehicleATravelTime(int vehicleId, MAP *origin, MAP *destination)
{
	return getVehicleATravelTime(vehicleId, origin, destination, false, 0);
}

int getVehicleATravelTime(int vehicleId, MAP *destination)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	return getVehicleATravelTime(vehicleId, vehicleTile, destination, false, 0);
	
}

int getVehicleActionATravelTime(int vehicleId, MAP *destination)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return getVehicleATravelTime(vehicleId, vehicleTile, destination, false, 1);
}

int getVehicleAttackATravelTime(int vehicleId, MAP *destination, bool ignoreHostile)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return getVehicleATravelTime(vehicleId, vehicleTile, destination, ignoreHostile, Rules->mov_rate_along_roads);
}

int getVehicleATravelTimeByTaskType(int vehicleId, MAP *destination, TaskType taskType)
{
	int travelTime;
	
	switch (taskType)
	{
	case TT_MELEE_ATTACK:
	case TT_LONG_RANGE_FIRE:
		travelTime = getVehicleAttackATravelTime(vehicleId, destination, false);
		break;
	case TT_BUILD:
	case TT_TERRAFORM:
		travelTime = getVehicleActionATravelTime(vehicleId, destination);
		break;
	default:
		travelTime = getVehicleATravelTime(vehicleId, destination);
	}
	
	return travelTime;
	
}

// ============================================================
// Landmark estimated distance and travel time
// ============================================================

void populateAIFactionMovementInfo()
{
	debug("populateAIFactionMovementInfo\n");
	
	const int factionId = aiFactionId;
	
	Geography &factionGeography = aiData.factionGeographys[factionId];
	factionGeography.clusters.clear();
	
	populateAIFactionNonCombatMovementInfo();
	
}

/*
For each base and non-combat unit:
1. Assign cluster ID.
2. Generate landmarks based on cluster size.
3. Computes non-blocked distances from landmarks.
*/
void populateAIFactionNonCombatMovementInfo()
{
	debug("populateAIFactionNonCombatMovementInfo\n");
	
	const int clusterType = 0;
	const int factionId = aiFactionId;
	
	Geography &factionGeography = aiData.factionGeographys[factionId];
	factionGeography.clusters.clear();
	
	// process base tiles
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		int baseTileIndex = baseTile - *MapPtr;
		TileInfo &baseTileInfo = aiData.tileInfos[baseTileIndex];
		TileFactionInfo &baseTileFactionInfo = baseTileInfo.factionInfos[factionId];
		
		for (int surfaceIndex = 0; surfaceIndex < 2; surfaceIndex++)
		{
			int surfaceAssociation = baseTileFactionInfo.surfaceAssociations[surfaceIndex];
			
			if (surfaceAssociation != -1 && baseTileFactionInfo.clusterIds[clusterType][surfaceIndex] == -1)
			{
				generateNonCombatCluster(baseTileIndex, surfaceIndex, factionId);
			}
			
		}
		
	}
	
}

void generateNonCombatCluster(int initialTileIndex, int surfaceIndex, int factionId)
{
	debug
	(
		"generateNonCombatCluster"
		" (%3d,%3d) surfaceIndex=%d"
		" factionId=%d"
		"\n"
		, getX(initialTileIndex), getY(initialTileIndex), surfaceIndex
		, factionId
	);
	
	Geography &factionGeography = aiData.factionGeographys[factionId];
	int clusterId = factionGeography.clusters.size();
	factionGeography.clusters.emplace_back();
	Cluster &cluster = factionGeography.clusters.at(clusterId);
	
	executionProfiles["1.2.5.0. initializeCluster"].start();
	
	// there is no non-combat aliens
	
	if (factionId == 0)
		return;
	
	// populate movementTypes
	
	robin_hood::unordered_flat_set<MovementType> movementTypes;
	
	switch (surfaceIndex)
	{
	case 1:
		movementTypes.insert(getUnitApproximateMovementType(factionId, BSC_SEA_ESCAPE_POD));
		break;
	case 0:
		movementTypes.insert(getUnitApproximateMovementType(factionId, BSC_COLONY_POD));
		break;
	}
	
	// scan area initially computing cluster area and next landmark
	
	int initialLandmarkTileIndex = initializeNonCombatCluster(initialTileIndex, surfaceIndex, factionId, clusterId, movementTypes);
	debug("\tarea=%4d\n", cluster.area);
	
	executionProfiles["1.2.5.0. initializeCluster"].stop();
	
	executionProfiles["1.2.5.1. generate landmarks"].start();
	
	for (MovementType &movementType : movementTypes)
	{
		// compute landmarkCount
		
		int landmarkCount = cluster.area / LANDMARK_COVERAGE;
		cluster.movementInfos.at(movementType).landmarkNetwork.resize(landmarkCount);
		
		// generate landmarks
		
		int landmarkTileIndex = initialLandmarkTileIndex;
		
		for (int landmarkId = 0; landmarkId < landmarkCount; landmarkId++)
		{
			debug("\t\t(%3d,%3d)\n", getX(landmarkTileIndex), getY(landmarkTileIndex));
			landmarkTileIndex = generateNonCombatClusterLandmark(landmarkTileIndex, surfaceIndex, factionId, clusterId, movementType, landmarkId);
		}
		
	}
	
	executionProfiles["1.2.5.1. generate landmarks"].stop();
	
}

int initializeNonCombatCluster(int initialTileIndex, int surfaceIndex, int factionId, int clusterId, robin_hood::unordered_flat_set<MovementType> &movementTypes)
{
	debug
	(
		"initializeCluster"
		" (%3d,%3d) surfaceIndex=%d"
		" factionId=%1d"
		" clusterId=%3d"
		"\n"
		, getX(initialTileIndex), getY(initialTileIndex), surfaceIndex
		, factionId
		, clusterId
	);
	
	int clusterType = 0;
	
	Geography &factionGeography = aiData.factionGeographys[factionId];
	Cluster &cluster = factionGeography.clusters.at(clusterId);
	std::vector<int> distances;
	
	// set all distances to infinity
	
	executionProfiles["1.2.5.0.0. set all distances to infinity"].start();
	
	distances.resize(*map_area_tiles, MOVEMENT_INFINITY);
	
	executionProfiles["1.2.5.0.0. set all distances to infinity"].stop();
	
	executionProfiles["1.2.5.0.1. initialize cluster"].start();
	
	// cycle variables
	
	std::vector<int> openNodes;
	std::vector<int> newOpenNodes;
	
	// initialize search
	
	distances.at(initialTileIndex) = 0;
	openNodes.push_back(initialTileIndex);
	
	// process open nodes
	
	int hexCostCount = 0;
	robin_hood::unordered_flat_map<MovementType, int> hexCostTotal;
	
	int landmarkTileIndex = initialTileIndex;
	int landmarkTileDistance = 0;
	
	while (!openNodes.empty())
	{
		for (int tileIndex : openNodes)
		{
			TileInfo &tileInfo = aiData.tileInfos[tileIndex];
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			bool tileZoc = tileFactionInfo.zoc;
			int tileDistance = distances.at(tileIndex);
			
			// assign clusterId
			
			tileFactionInfo.clusterIds[clusterType][surfaceIndex] = clusterId;
			
			// process adjacent tiles
			
			bool extended = false;
			
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				int adjacentTileIndex = tileInfo.adjacentTileIndexes[angle];
				
				if (adjacentTileIndex == -1)
					continue;
				
				TileInfo &adjacentTileInfo = aiData.tileInfos[adjacentTileIndex];
				TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
				int adjacentTileSurfaceAssociation = adjacentTileFactionInfo.surfaceAssociations[surfaceIndex];
				bool adjacentTileBlocked = adjacentTileFactionInfo.blocked;
				bool adjacentTileZoc = adjacentTileFactionInfo.zoc;
				bool adjacentTileWarzone = adjacentTileInfo.warzone;
				
				// corresponding realm
				
				if (adjacentTileSurfaceAssociation == -1)
					continue;
				
				// available
				
				if (adjacentTileBlocked || (tileZoc && adjacentTileZoc) || adjacentTileWarzone)
					continue;
				
				// update distance
				
				int oldAdjacentTileDistance = distances.at(adjacentTileIndex);
				int newAdjacentTileDistance = tileDistance + 1;
				
				if (newAdjacentTileDistance < oldAdjacentTileDistance)
				{
					distances.at(adjacentTileIndex) = newAdjacentTileDistance;
					newOpenNodes.push_back(adjacentTileIndex);
					
					extended = true;
					
					// accumulate hexCosts
					
					hexCostCount++;
					
					for (MovementType &movementType : movementTypes)
					{
						hexCostTotal[movementType] += tileInfo.hexCosts1[movementType][angle];
					}
					
				}
				
			}
			
			// open node was extended - not a good candidate for next landmark
			
			if (extended)
				continue;
			
			// update nextLandmarkTileIndex
			
			if (tileDistance > landmarkTileDistance)
			{
				landmarkTileIndex = tileIndex;
				landmarkTileDistance = tileDistance;
			}
			
		}
		
		// swap nodes
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	// update average hexCost
	
	for (MovementType &movementType : movementTypes)
	{
		cluster.movementInfos[movementType].averageHexCost =
			hexCostCount == 0 ?
				(double)Rules->mov_rate_along_roads
				:
				(double)hexCostTotal[movementType] / (double)hexCostCount
		;
		
	}
	
	executionProfiles["1.2.5.0.1. initialize cluster"].stop();
	
	executionProfiles["1.2.5.0.2. compute cluster area"].start();
	
	// compute cluster area
	
	if (cluster.area == 0)
	{
		for (int distance : distances)
		{
			if (distance < MOVEMENT_INFINITY)
			{
				cluster.area++;
			}
			
		}
		
	}
	
	executionProfiles["1.2.5.0.2. compute cluster area"].stop();
	
	return landmarkTileIndex;
	
}

int generateNonCombatClusterLandmark(int initialTileIndex, int surfaceIndex, int factionId, int clusterId, MovementType movementType, int landmarkId)
{
	bool TRACE = DEBUG & false;
	
	debug
	(
		"generateNonCombatClusterLandmark"
		" (%3d,%3d) surfaceIndex=%d"
		" factionId=%1d"
		" clusterId=%3d"
		" movementType=%d"
		" landmarkId=%2d"
		"\n"
		, getX(initialTileIndex), getY(initialTileIndex), surfaceIndex
		, factionId
		, clusterId
		, movementType
		, landmarkId
	);
	
	Geography &factionGeography = aiData.factionGeographys[factionId];
	Cluster &cluster = factionGeography.clusters.at(clusterId);
	ClusterMovementInfo &clusterMovementInfo = cluster.movementInfos.at(movementType);
	std::vector<std::vector<int>> &landmarkNetwork = clusterMovementInfo.landmarkNetwork;
	std::vector<int> &movementCosts = landmarkNetwork.at(landmarkId);
	
	// set all movementCosts to infinity
	
	executionProfiles["1.2.5.1.0. set all movementCosts to infinity"].start();
	
	movementCosts.resize(*map_area_tiles, MOVEMENT_INFINITY);
	
	executionProfiles["1.2.5.1.0. set all movementCosts to infinity"].stop();
	
	executionProfiles["1.2.5.1.1. compute movementCosts"].start();
	
	// cycle variables
	
	std::vector<int> openNodes;
	std::vector<int> newOpenNodes;
	
	// initialize search
	
	movementCosts.at(initialTileIndex) = 0;
	openNodes.push_back(initialTileIndex);
	
	// process open nodes
	
	int nextLandmarkTileIndex = initialTileIndex;
	int nextLandmarkTileSummaryMovementCost = 0;
	
	while (!openNodes.empty())
	{
		for (int tileIndex : openNodes)
		{
			TileInfo &tileInfo = aiData.tileInfos[tileIndex];
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			bool tileZoc = tileFactionInfo.zoc;
			int tileMovementCost = movementCosts.at(tileIndex);
			
			// process adjacent tiles
			
			bool extended = false;
			
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				int adjacentTileIndex = tileInfo.adjacentTileIndexes[angle];
				int hexCost = tileInfo.hexCosts1[movementType][angle];
				
				if (adjacentTileIndex == -1 || hexCost == -1)
					continue;
				
				TileInfo &adjacentTileInfo = aiData.tileInfos[adjacentTileIndex];
				TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
				int adjacentTileSurfaceAssociation = adjacentTileFactionInfo.surfaceAssociations[surfaceIndex];
				bool adjacentTileBlocked = adjacentTileFactionInfo.blocked;
				bool adjacentTileZoc = adjacentTileFactionInfo.zoc;
				bool adjacentTileWarzone = adjacentTileInfo.warzone;
				
				// corresponding realm
				
				if (adjacentTileSurfaceAssociation == -1)
					continue;
				
				// available
				
				if (adjacentTileBlocked || (tileZoc && adjacentTileZoc) || adjacentTileWarzone)
					continue;
				
				// update movementCost
				
				int oldAdjacentTileMovementCost = movementCosts.at(adjacentTileIndex);
				int newAdjacentTileMovementCost = tileMovementCost + hexCost;
				
				if (newAdjacentTileMovementCost < oldAdjacentTileMovementCost)
				{
					movementCosts.at(adjacentTileIndex) = newAdjacentTileMovementCost;
					newOpenNodes.push_back(adjacentTileIndex);
					
					extended = true;
					
				}
				
			}
			
			// open node was extended - not a good candidate for next landmark
			
			if (extended)
				continue;
			
			// update nextLandmarkTileIndex
			
			int summaryMovementCost = tileMovementCost;
			
			for (int otherLandmarkId = 0; otherLandmarkId < landmarkId; otherLandmarkId++)
			{
				std::vector<int> &otherLandmarkMovementCosts = landmarkNetwork.at(otherLandmarkId);
				int otherLandmarkMovementCost = otherLandmarkMovementCosts.at(tileIndex);
				summaryMovementCost += otherLandmarkMovementCost;
			}
			
			if (summaryMovementCost > nextLandmarkTileSummaryMovementCost)
			{
				nextLandmarkTileIndex = tileIndex;
				nextLandmarkTileSummaryMovementCost = summaryMovementCost;
			}
			
		}
		
		// swap nodes
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	executionProfiles["1.2.5.1.1. compute movementCosts"].stop();
	
	if (TRACE)
	{
		debug("\tlandmark movementCosts\n");
		
		for (int tileIndex = 0; tileIndex < *map_area_tiles; tileIndex++)
		{
			int movementCost = movementCosts.at(tileIndex);
			
			if (movementCost == MOVEMENT_INFINITY)
				continue;
			
			debug("\t\t(%3d,%3d) %3d\n", getX(tileIndex), getY(tileIndex), movementCost);
			
		}
		
	}
	
	return nextLandmarkTileIndex;
	
}

// [origin, destination, factionId, movementType, combat]
robin_hood::unordered_flat_map<long long, int> cachedLMovementCosts;

long long getCachedLMovementCostKey(MAP *origin, MAP *destination, int factionId, int clusterType, int surfaceType, MovementType movementType)
{
	long long key =
		+ (long long)(origin - *MapPtr)			* (long long)MOVEMENT_TYPE_COUNT * 2L * 2L * (long long)MaxPlayerNum * (long long)*map_area_tiles
		+ (long long)(destination - *MapPtr)	* (long long)MOVEMENT_TYPE_COUNT * 2L * 2L * (long long)MaxPlayerNum
		+ (long long)factionId					* (long long)MOVEMENT_TYPE_COUNT * 2L * 2L
		+ (long long)clusterType				* (long long)MOVEMENT_TYPE_COUNT * 2L
		+ (long long)surfaceType				* (long long)MOVEMENT_TYPE_COUNT
		+ (long long)movementType				
	;
	
	return key;
	
}

/*
Returns cachedLMovementCost or -1 if not found.
*/
int getCachedLMovementCost(long long key)
{
	return cachedLMovementCosts.count(key) == 0 ? -1 : cachedLMovementCosts.at(key);
}

void setCachedLMovementCost(long long key, int movementCost)
{
	cachedAMovementCosts[key] = movementCost;
}

int getLMovementCost(MAP *origin, MAP *destination, int factionId, int clusterType, int surfaceType, MovementType movementType)
{
	executionProfiles["| getLMovementCost"].start();
	
	const bool TRACE = DEBUG && false;
	
	if (TRACE)
	{
		debug
		(
			"getLMovementCost"
			" (%3d,%3d)->(%3d,%3d)"
			" factionId=%d"
			" clusterType=%d"
			" movementType=%d"
			"\n"
			, getX(origin), getY(origin), getX(destination), getY(destination)
			, factionId
			, clusterType
			, movementType
		);
	}
	
	int originTileIndex = origin - *MapPtr;
	int destinationTileIndex = destination - *MapPtr;
	TileFactionInfo &originTileFactionInfo = aiData.tileInfos[originTileIndex].factionInfos[factionId];
	TileFactionInfo &destinationTileFactionInfo = aiData.tileInfos[destinationTileIndex].factionInfos[factionId];
	int originClusterId = originTileFactionInfo.clusterIds[clusterType][surfaceType];
	int destinationClusterId = destinationTileFactionInfo.clusterIds[clusterType][surfaceType];
	
	// at destination
	
	if (origin == destination)
	{
		executionProfiles["| getLMovementCost"].stop();
		return 0;
	}
	
	// not same cluster
	
	if (originClusterId == -1 || destinationClusterId == -1 || originClusterId != destinationClusterId)
		return -1;
	
	// check cached value
	
	executionProfiles["| getLMovementCost 1. check cached value"].start();
	
	long long originToDestinationMovementCostKey = getCachedLMovementCostKey(origin, destination, factionId, clusterType, surfaceType, movementType);
	int cachedMovementCost = getCachedLMovementCost(originToDestinationMovementCostKey);
	
	executionProfiles["| getLMovementCost 1. check cached value"].stop();
	
	if (cachedMovementCost != -1)
	{
		executionProfiles["| getLMovementCost"].stop();
		return cachedMovementCost;
	}
	
	executionProfiles["| getLMovementCost 2. estimate movementCost"].start();
	
	// get cluster
	
	Cluster &cluster = aiData.factionGeographys[factionId].clusters.at(originClusterId);
	ClusterMovementInfo &clusterMovementInfo = cluster.movementInfos.at(movementType);
	
	// estimate direct movementCost
	
	int range = getRange(origin, destination);
	int movementCost = (int)ceil((double)range / clusterMovementInfo.averageHexCost);
	
	// estimate landmark movementCost
	
	for (std::vector<int> &landmarkMovementCosts : clusterMovementInfo.landmarkNetwork)
	{
		int landmarkMovementCost = abs(landmarkMovementCosts.at(originTileIndex) - landmarkMovementCosts.at(destinationTileIndex));
		movementCost = std::max(movementCost, landmarkMovementCost);
	}
	
	executionProfiles["| getLMovementCost 2. estimate movementCost"].stop();
	
	// set cached value
	
	executionProfiles["| getAMovementCost 3. set cached value"].start();
	
	setCachedLMovementCost(originToDestinationMovementCostKey, movementCost);
	
	executionProfiles["| getAMovementCost 3. set cached value"].stop();
	
	executionProfiles["| getLMovementCost"].stop();
	
	// return value
	
	return movementCost;
	
}

/*
Estimates unit movement cost based on unit type.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getUnitLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, int action)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	UNIT *unit = getUnit(unitId);
	
	// compute travel time by chassis type
	
	int travelTime = MOVEMENT_INFINITY;
	
	switch (unit->chassis_type)
	{
	case CHS_GRAVSHIP:
		travelTime = getGravshipUnitLTravelTime(origin, destination, speed, action);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		travelTime = getAirRangedUnitLTravelTime(origin, destination, speed, action);
		break;
	case CHS_FOIL:
	case CHS_CRUISER:
		travelTime = getSeaUnitLTravelTime(origin, destination, factionId, unitId, speed, action);
		break;
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		travelTime = getLandUnitLTravelTime(origin, destination, factionId, unitId, speed, action);
		break;
	}
	
	return travelTime;
	
}

int getGravshipUnitLTravelTime(MAP *origin, MAP *destination, int speed, int action)
{
	int range = getRange(origin, destination);
	
	return divideIntegerRoundUp(range * Rules->mov_rate_along_roads + action, speed);
	
}

/*
Estimates air ranged vehicle combat travel time.
Air ranged vehicle hops between airbases.
*/
int getAirRangedUnitLTravelTime(MAP *origin, MAP *destination, int speed, int action)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	int movementRange = (speed - action) / Rules->mov_rate_along_roads;
	
	// find optimal airbase to launch strike from
	
	MAP *optimalAirbase = nullptr;
	int optimalAirbaseRange = INT_MAX;
	int optimalVehicleToAirbaseRange = 0;
	
	for (MAP *airbase : aiData.factionInfos[aiFactionId].airbases)
	{
		int airbaseToDestinationRange = getRange(airbase, destination);
		
		// reachable
		
		if (airbaseToDestinationRange > movementRange)
			continue;
		
		int vehicleToAirbaseRange = getRange(origin, airbase);
		
		if (vehicleToAirbaseRange < optimalAirbaseRange)
		{
			optimalAirbase = airbase;
			optimalAirbaseRange = vehicleToAirbaseRange;
			optimalVehicleToAirbaseRange = vehicleToAirbaseRange;
		}
		
	}
	
	// not found
	
	if (optimalAirbase == nullptr)
		return -1;
	
	// estimate travel time
	
	return divideIntegerRoundUp(2 * optimalVehicleToAirbaseRange * Rules->mov_rate_along_roads, speed) + 1;
	
}

/*
Estimates sea vehicle combat travel time not counting any obstacles.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getSeaUnitLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool action)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	// verify same ocean association
	
	if (!isSameOceanAssociation(origin, destination, factionId))
		return -1;
	
	// combatType
	
	int combatType = (isCombatUnit(unitId) ? 1 : 0);
	
	// movementType
	
	MovementType movementType = getUnitMovementType(factionId, unitId);
	
	// movementCost
	
	int movementCost = getLMovementCost(origin, destination, factionId, combatType, 1, movementType);
	
	// unreachable
	
	if (movementCost == -1 || movementCost >= MOVEMENT_INFINITY)
		return -1;
	
	// total actionCost
	
	int actionCost = movementCost + action;
	
	// travelTime
	
	return divideIntegerRoundUp(actionCost, speed);
	
}

/*
Estimates land vehicle combat travel time not counting any obstacles.
Land vehicle travels within its association and fights through enemies.
Optionally it can be transported across associations.
*/
int getLandUnitLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool action)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	int speedOnLand = speed / Rules->mov_rate_along_roads;
	int speedOnRoad = speed / conf.tube_movement_rate_multiplier;
	int speedOnTube = speed;
	
	// evaluate different cases
	
	if (isSameLandAssociation(origin, destination, factionId)) // same continent
	{
		// combatType
		
		int combatType = (isCombatUnit(unitId) ? 1 : 0);
		
		// movementType
		
		MovementType movementType = getUnitMovementType(factionId, unitId);
		
		// movementCost
		
		int movementCost = getLMovementCost(origin, destination, factionId, combatType, 0, movementType);
		
		// unreachable
		
		if (movementCost == -1 || movementCost >= MOVEMENT_INFINITY)
			return -1;
		
		// total actionCost
		
		int actionCost = movementCost + action;
		
		// travelTime
		
		return divideIntegerRoundUp(actionCost, speed);
		
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
		
		// cross ocean association
		
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
		
		int travelTime = (int)((double)range / (0.75 * averageMovementSpeed));
		
		// add ferry wait time if not on transport already
		
		travelTime += 15;
		
		return travelTime;
		
	}
	
}

/*
Estimates unit travel time using A* algorithm.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getVehicleLTravelTime(int vehicleId, MAP *origin, MAP *destination, int action)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	UNIT *unit = getUnit(unitId);
	int speed = getVehicleSpeed(vehicleId);
	
	int travelTime = -1;
	
	switch (unit->chassis_type)
	{
	case CHS_GRAVSHIP:
		travelTime = getGravshipUnitLTravelTime(origin, destination, speed, action);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		travelTime = getAirRangedUnitLTravelTime(origin, destination, speed, action);
		break;
	case CHS_FOIL:
	case CHS_CRUISER:
		travelTime = getSeaUnitLTravelTime(origin, destination, factionId, unitId, speed, action);
		break;
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		travelTime = getLandVehicleLTravelTime(vehicleId, origin, destination, action);
		break;
	}
	
	return travelTime;
	
}

/*
Estimates land unit travel time using A* algorithm.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getLandVehicleLTravelTime(int vehicleId, MAP *origin, MAP *destination, int action)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	int speed = getVehicleSpeed(vehicleId);
	
	int travelTime = MOVEMENT_INFINITY;
	
	if (isSameLandAssociation(origin, destination, factionId)) // same continent
	{
		travelTime = getUnitLTravelTime(origin, destination, factionId, unitId, speed, action);
	}
	else // not same continent
	{
		// transportId
		
		int transportVehicleId = getVehicleTransportId(vehicleId);
		
		if (transportVehicleId != -1) // transported
		{
			bool seaTransport = isSeaTransportVehicle(transportVehicleId);
			
			if (!seaTransport) // not sea transport
			{
				// don't know what to do with it
				travelTime = getUnitLTravelTime(origin, destination, factionId, unitId, speed, action);
			}
			else // sea transport
			{
				VEH *transportVehicle = getVehicle(transportVehicleId);
				int transportUnitId = transportVehicle->unit_id;
				int transportSpeed = getVehicleSpeed(transportVehicleId);
				MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);
				int transportOceanAssociation = getVehicleAssociation(transportVehicleId);
				bool destinationOcean = is_ocean(destination);
				int destinationAssociation = getAssociation(destination, factionId);
				
				if (destinationOcean && isSameOceanAssociation(transportVehicleTile, destination, factionId)) // same ocean
				{
					// travel time equals transport travel time
					travelTime = getUnitLTravelTime(origin, destination, factionId, transportUnitId, transportSpeed, action);
				}
				else if (!destinationOcean && isConnected(transportOceanAssociation, destinationAssociation, factionId)) // connected continent
				{
					// roughly estimate travel time
					
					int range = getRange(origin, destination);
					
					double averageMovementSpeed =
						+ 0.5 * (double)(speed / Rules->mov_rate_along_roads)
						+ 0.5 * (double)(transportSpeed / Rules->mov_rate_along_roads)
					;
					
					travelTime = (int)ceil((double)range / averageMovementSpeed);
					
				}
				else // remote association
				{
					// roughly estimate travel time
					
					int range = getRange(origin, destination);
					
					double averageMovementSpeed =
						+ 0.5 * (double)(speed / Rules->mov_rate_along_roads)
						+ 0.5 * (double)(transportSpeed / Rules->mov_rate_along_roads)
					;
					
					travelTime = (int)ceil((double)range / averageMovementSpeed + 10.0);
					
				}
				
			}
			
		}
		else // not transported
		{
			// roughly estimate travel time
			
			double estimatedTransportMovementSpeed = 4.0;
			
			int range = getRange(origin, destination);
			
			double averageMovementSpeed =
				+ 0.5 * (double)(speed / Rules->mov_rate_along_roads)
				+ 0.5 * estimatedTransportMovementSpeed
			;
			
			travelTime = (int)ceil((double)range / averageMovementSpeed + 10.0);
			
		}
		
	}
	
	return travelTime;
	
}

int getVehicleLTravelTime(int vehicleId, MAP *origin, MAP *destination)
{
	return getVehicleLTravelTime(vehicleId, origin, destination, 0);
}

int getVehicleLTravelTime(int vehicleId, MAP *destination)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	return getVehicleLTravelTime(vehicleId, vehicleTile, destination, 0);
	
}

int getVehicleActionLTravelTime(int vehicleId, MAP *destination)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return getVehicleLTravelTime(vehicleId, vehicleTile, destination, 1);
}

int getVehicleAttackLTravelTime(int vehicleId, MAP *destination)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return getVehicleLTravelTime(vehicleId, vehicleTile, destination, Rules->mov_rate_along_roads);
}

int getVehicleLTravelTimeByTaskType(int vehicleId, MAP *destination, TaskType taskType)
{
	int travelTime;
	
	switch (taskType)
	{
	case TT_MELEE_ATTACK:
	case TT_LONG_RANGE_FIRE:
		travelTime = getVehicleAttackLTravelTime(vehicleId, destination);
		break;
	case TT_BUILD:
	case TT_TERRAFORM:
		travelTime = getVehicleActionLTravelTime(vehicleId, destination);
		break;
	default:
		travelTime = getVehicleLTravelTime(vehicleId, destination);
	}
	
	return travelTime;
	
}

// ============================================================
// Helper functions
// ============================================================

/*
Vector distance computed with movement cost granularity for path computation.
*/
int getRouteVectorDistance(MAP *tile1, MAP *tile2)
{
	int x1 = getX(tile1);
	int y1 = getY(tile1);
	int x2 = getX(tile2);
	int y2 = getY(tile2);
	
	int dx = abs(x1 - x2);
	int dy = abs(y1 - y2);
	if (!*map_toggle_flat)
	{
		if (dx > *map_half_x)
		{
			dx = *map_axis_x - dx;
		}
	}
	
	// multiply deltas by road movement rate
	
	dx *= Rules->mov_rate_along_roads;
	dy *= Rules->mov_rate_along_roads;
	
	return std::max(dx, dy) - ((((dx + dy) / 2) - std::min(dx, dy) + 1) / 2);
	
}

/*
Returns vehicle movement type.
*/
MovementType getUnitMovementType(int factionId, int unitId)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	
	MovementType movementType;
	
	switch (triad)
	{
	case TRIAD_AIR:
		{
			movementType = MT_AIR;
		}
		break;
		
	case TRIAD_SEA:
		{
			bool native = isNativeUnit(unitId);
			movementType = native ? MT_SEA_NATIVE : MT_SEA_REGULAR;
		}
		break;
		
	case TRIAD_LAND:
		{
			bool native = isFactionHasProject(factionId, FAC_XENOEMPATHY_DOME) || isNativeUnit(unitId);
			bool hover = isHoveringLandUnit(unitId);
			bool easy = getFaction(factionId)->SE_planet >= 1 || isEasyFungusEnteringLandUnit(unitId);
			
			if (native)
			{
				movementType = MT_LAND_NATIVE;
			}
			else if (hover)
			{
				movementType = MT_LAND_EASY;
			}
			else if (easy)
			{
				movementType = MT_LAND_EASY;
			}
			else
			{
				movementType = MT_LAND_REGULAR;
			}
		}
		break;
		
	// safeguarding case
	default:
		movementType = MT_AIR;
		
	}
	
	return movementType;
	
}

/*
Returns vehicle movement type.
Ignoring hovering unit ability.
*/
MovementType getUnitApproximateMovementType(int factionId, int unitId)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	
	MovementType movementType;
	
	switch (triad)
	{
	case TRIAD_AIR:
		{
			movementType = MT_AIR;
		}
		break;
		
	case TRIAD_SEA:
		{
			bool native = isNativeUnit(unitId);
			movementType = native ? MT_SEA_NATIVE : MT_SEA_REGULAR;
		}
		break;
		
	case TRIAD_LAND:
		{
			bool native = isNativeUnit(unitId);
			bool hover = isHoveringLandUnit(unitId);
			bool easy = isFactionHasProject(factionId, FAC_XENOEMPATHY_DOME) || isEasyFungusEnteringLandUnit(unitId);
			
			if (native && hover)
			{
				movementType = MT_LAND_NATIVE;
			}
			else if (native)
			{
				movementType = MT_LAND_NATIVE;
			}
			else if (hover)
			{
				movementType = MT_LAND_EASY;
			}
			else if (easy)
			{
				movementType = MT_LAND_EASY;
			}
			else
			{
				movementType = MT_LAND_REGULAR;
			}
		}
		break;
		
	// safeguarding case
	default:
		movementType = MT_AIR;
		
	}
	
	return movementType;
	
}

