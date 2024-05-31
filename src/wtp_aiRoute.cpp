
#include <vector>
#include <set>
#include <queue>
#include <unordered_map>
#include "robin_hood.h"
#include "wtp_ai.h"
#include "wtp_aiRoute.h"
#include "wtp_aiMoveColony.h"
#include "wtp_aiMoveTransport.h"

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
robin_hood::unordered_flat_map<long long, int> cachedAMovementCosts;

long long getCachedAMovementCostKey(MAP *origin, MAP *destination, MovementType movementType, int factionId, bool ignoreHostile)
{
	long long key =
		+ (long long)(origin - *MapTiles)			* 2L * (long long)MaxPlayerNum * (long long)MOVEMENT_TYPE_COUNT * (long long)*MapAreaTiles
		+ (long long)(destination - *MapTiles)	* 2L * (long long)MaxPlayerNum * (long long)MOVEMENT_TYPE_COUNT
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
	profileFunction("1.2.5. populateMovementInfos", populateMovementInfos);
	
}

/*
Sets tile combat movement realated info for each faction: blocked, zoc.
*/
void populateTileMovementRestrictions()
{
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		// stationary vehicle effects: blocked and zoc
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			TileInfo &vehicleTileInfo = aiData.getTileInfo(vehicleTile);
			TileFactionInfo &vehicleTileFactionInfo = vehicleTileInfo.factionInfos[factionId];
			
			// exclude friendly
			
			if (isFriendly(factionId, vehicle->faction_id))
				continue;
			
			// exclude not stationary
			
			if (!(vehicle->unit_id == BSC_FUNGAL_TOWER || vehicleTileInfo.base || vehicleTileInfo.bunker))
				continue;
			
			// blocked for non-combat
			
			vehicleTileFactionInfo.blocked[0] = true;
			
			// blocked for combat only if neutral
			
			if (isNeutral(factionId, vehicle->faction_id))
			{
				vehicleTileFactionInfo.blocked[1] = true;
			}
			
			// zoc is exerted only by vehicle on land
			
			if (vehicleTileInfo.ocean)
				continue;
			
			// explore adjacent tiles for zoc
			
			for (int i = 0; i < vehicleTileInfo.validAdjacentTileCount; i++)
			{
				int adjacentTileIndex = vehicleTileInfo.validAdjacentTileIndexes[i];
				TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTileIndex);
				TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
				
				// zoc is exerted only to land
				
				if (adjacentTileInfo.ocean)
					continue;
				
				// zoc is not exerted to base
				
				if (adjacentTileInfo.base)
					continue;
				
				// set zoc for non-combat vehicle
				
				adjacentTileFactionInfo.zoc[0] = true;
				
				// set zoc for combat vehicle only for neutral
				
				if (isNeutral(factionId, vehicle->faction_id))
				{
					adjacentTileFactionInfo.zoc[1] = true;
				}
				
			}
				
		}
		
		// moving vehicles effects: impediment
				
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			int triad = vehicle->triad();
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// exclude friendly
			
			if (isFriendly(factionId, vehicle->faction_id))
				continue;
			
			// exclude not moving
			
			if (vehicle->unit_id == BSC_FUNGAL_TOWER)
				continue;
			
			// impediment coefficient
			
			double impedimentCoefficients[2] = {};
			
			// not attacking
			// neutral or hostile non-combat
			if (isNeutral(factionId, vehicle->faction_id) || (isHostile(factionId, vehicle->faction_id) && !isCombatVehicle(vehicleId)))
			{
				impedimentCoefficients[0] = 1.0;
				impedimentCoefficients[1] = 1.0;
			}
			// attacking
			// hostile combat
			else if (isHostile(factionId, vehicle->faction_id) && isCombatVehicle(vehicleId))
			{
				impedimentCoefficients[0] = 5.0;
				impedimentCoefficients[1] = 1.0;
			}
			
			// impediment is created by land or air units
			
			if (triad == TRIAD_SEA)
				continue;
			
			// explore range tiles for impediment
			
			for (MAP *rangeTile : getRangeTiles(vehicleTile, IMPEDIMENT_RANGE, true))
			{
				int rangeTileIndex = rangeTile - *MapTiles;
				TileInfo &rangeTileInfo = aiData.tileInfos[rangeTileIndex];
				TileFactionInfo &rangeTileFactionInfo = rangeTileInfo.factionInfos[factionId];
				
				// impediment is exerted to land
				
				if (rangeTileInfo.ocean)
					continue;
				
				// set impediment
				
				rangeTileFactionInfo.impediment[0] = impedimentCoefficients[0] * NEUTRAL_IMPEDIMENT * (double)Rules->move_rate_roads;
				rangeTileFactionInfo.impediment[1] = impedimentCoefficients[0] * NEUTRAL_IMPEDIMENT * (double)Rules->move_rate_roads;
				
			}
			
		}
		
		// base is not zoc
		
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			TileInfo &tileInfo = aiData.getTileInfo(baseTile);
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			
			// disable zoc
			
			tileFactionInfo.zoc[0] = false;
			tileFactionInfo.zoc[1] = false;
			
		}
		
		// friendly vehicle disables zoc
		
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			TileInfo &tileInfo = aiData.getTileInfo(vehicleTile);
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			
			// friendly
			
			if (!isFriendly(factionId, vehicle->faction_id))
				continue;
			
			// except probe (if probe excluded)
			
			if
			(
				conf.zoc_regular_army_sneaking_disabled
				&&
				(isProbeVehicle(vehicleId) || isArtifactVehicle(vehicleId) || isVehicleHasAbility(vehicleId, ABL_CLOAKED))
			)
				continue;
			
			// disable zoc
			
			tileFactionInfo.zoc[0] = false;
			tileFactionInfo.zoc[1] = false;
			
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
	int originTileIndex = origin - *MapTiles;
	openNodes.push_back(originTileIndex);
	
	// process nodes
	
	while (!openNodes.empty())
	{
		for (int tileIndex : openNodes)
		{
			MAP *tile = *MapTiles + tileIndex;
			TileInfo &tileInfo = aiData.tileInfos[tileIndex];
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			bool tileZoc = tileFactionInfo.zoc[ignoreHostile];
			
			// cached value
			
			long long originToTileMovementCostKey = getCachedAMovementCostKey(origin, tile, movementType, factionId, ignoreHostile);
			int tileMovementCost = getCachedAMovementCost(originToTileMovementCostKey);
			
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				int adjacentTileIndex = tileInfo.adjacentTileIndexes[angle];
				
				if (adjacentTileIndex == -1)
					continue;
				
				MAP *adjacentTile = *MapTiles + adjacentTileIndex;
				
				TileInfo &adjacentTileInfo = aiData.tileInfos[adjacentTileIndex];
				TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
				ExpansionTileInfo &adjacentTileExpansionInfo = getExpansionTileInfo(adjacentTile);
				bool adjacentTileBlocked = adjacentTileFactionInfo.blocked[ignoreHostile];
				bool adjacentTileZoc = adjacentTileFactionInfo.zoc[ignoreHostile];
				
				// hex cost
				
				int hexCost = tileInfo.hexCosts[movementType][angle];
				
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
				
				int computedMovementCost = (adjacentTileBlocked || (tileZoc && adjacentTileZoc) ? MOVEMENT_INFINITY : tileMovementCost + hexCost);
				
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
	int vehicleTileIndex = vehicleTile - *MapTiles;
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
			MAP *tile = *MapTiles + tileIndex;
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			TileFactionInfo &tileFactionInfo = aiData.getTileFactionInfo(tile, factionId);
			bool tileZoc = tileFactionInfo.zoc[0];
			int tileMovementCost = movementCosts.at(tile);
			
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				int adjacentTileIndex = tileInfo.adjacentTileIndexes[angle];
				
				if (adjacentTileIndex == -1)
					continue;
				
				MAP *adjacentTile = *MapTiles + adjacentTileIndex;
				
				TileFactionInfo &adjacentTileFactionInfo = aiData.getTileFactionInfo(adjacentTile, factionId);
				bool adjacentTileBlocked = adjacentTileFactionInfo.blocked[0];
				bool adjacentTileZoc = adjacentTileFactionInfo.zoc[0];
				int hexCost = tileInfo.hexCosts[movementType][angle];
				
				// exclude inaccessible location
				
				if (adjacentTileBlocked || (tileZoc && adjacentTileZoc) || hexCost == -1)
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
		bool currentTileZoc = currentTileFactionInfo.zoc[ignoreHostile];
		
		long long originToCurrentTileMovementCostKey = getCachedAMovementCostKey(origin, currentTile, movementType, factionId, ignoreHostile);
		int currentTileMovementCost = getCachedAMovementCost(originToCurrentTileMovementCostKey);
		
		// check surrounding tiles
		
		for (int angle = 0; angle < ANGLE_COUNT; angle++)
		{
			MAP *adjacentTile = getTileByAngle(currentTile, angle);
			
			if (adjacentTile == nullptr)
				continue;
			
			// hex cost
			
			int adjacentTileHexCost = currentTileInfo.hexCosts[movementType][angle];
			
			// allowed step
			
			if (adjacentTileHexCost == -1)
				continue;
			
			// parameters
			
			TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
			TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
			bool adjacentTileBlocked = adjacentTileFactionInfo.blocked[ignoreHostile];
			bool adjacentTileZoc = adjacentTileFactionInfo.zoc[ignoreHostile];
			
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
			
			if (adjacentTileBlocked || (currentTileZoc && adjacentTileZoc))
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
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(destination >= *MapTiles && destination < *MapTiles + *MapAreaTiles);
	
	UNIT *unit = getUnit(unitId);
	
	// compute travel time by chassis type
	
	int travelTime = MOVEMENT_INFINITY;
	
	switch (unit->chassis_id)
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
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(destination >= *MapTiles && destination < *MapTiles + *MapAreaTiles);
	
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
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(destination >= *MapTiles && destination < *MapTiles + *MapAreaTiles);
	
	int speedOnLand = speed / Rules->move_rate_roads;
	int speedOnRoad = speed / conf.magtube_movement_rate;
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
	
	switch (unit->chassis_id)
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
						+ 0.5 * (double)(speed / Rules->move_rate_roads)
						+ 0.5 * (double)(transportSpeed / Rules->move_rate_roads)
					;
					
					travelTime = (int)ceil((double)range / averageMovementSpeed);
					
				}
				else // remote association
				{
					// roughly estimate travel time
					
					int range = getRange(origin, destination);
					
					double averageMovementSpeed =
						+ 0.5 * (double)(speed / Rules->move_rate_roads)
						+ 0.5 * (double)(transportSpeed / Rules->move_rate_roads)
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
				+ 0.5 * (double)(speed / Rules->move_rate_roads)
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
	return getVehicleATravelTime(vehicleId, vehicleTile, destination, ignoreHostile, Rules->move_rate_roads);
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

const bool MOVEMENT_INFO_TRACE = DEBUG && false;
void populateMovementInfos()
{
	if (MOVEMENT_INFO_TRACE) { debug("populateMovementInfos - %s\n", getMFaction(aiFactionId)->noun_faction); }
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		aiData.factionGeographys[factionId].clusters.clear();
	}
	
	// populate AI faction non-combat movement info
	
	populateFactionMovementInfo(aiFactionId, 0);
	
	// populate all factions combat movement infos
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		populateFactionMovementInfo(factionId, 1);
	}
	
}

/*
For each base and non-combat or combat unit:
1. Assign cluster ID.
2. Generate landmarks based on cluster size.
3. Computes non-blocked distances from landmarks.
*/
void populateFactionMovementInfo(int factionId, int clusterType)
{
	if (MOVEMENT_INFO_TRACE) { debug("populateFactionMovementInfo(factionId=%d, clusterType=%d)\n", factionId, clusterType); }
	
	Geography factionGeography = aiData.factionGeographys[factionId];
	
	// process tiles
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos[tileIndex];
		TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
		
		// exclude blocked
		
		if (tileFactionInfo.blocked[clusterType])
			continue;
		
		for (int surfaceIndex = 0; surfaceIndex < 2; surfaceIndex++)
		{
			int surfaceAssociation = tileFactionInfo.surfaceAssociations[surfaceIndex];
			
			// correct surfaceAssociation only
			
			if (surfaceAssociation == -1)
				continue;
			
			// exclude unreacheable ocean associations
			
			if (surfaceIndex == 1 && factionGeography.potentiallyReachableAssociations.count(surfaceAssociation) == 0)
				continue;
			
			// skip tiles already assigned to cluster
			
			if (tileFactionInfo.clusterIds[clusterType][surfaceIndex] != -1)
				continue;
			
			// generate cluster
			
			generateCluster(factionId, clusterType, surfaceIndex, tileIndex);
			
		}
		
	}
	
}

void generateCluster(int factionId, int clusterType, int surfaceIndex, int initialTileIndex)
{
	if (MOVEMENT_INFO_TRACE)
	{
		debug
		(
			"generateCluster(factionId=%d, clusterType=%d, surfaceIndex=%d, tile=(%3d,%3d)"
			"\n"
			, factionId
			, clusterType
			, surfaceIndex
			, getX(initialTileIndex), getY(initialTileIndex)
		);
	}
	
	Geography &factionGeography = aiData.factionGeographys[factionId];
	int clusterId = factionGeography.clusters.size();
	factionGeography.clusters.emplace_back();
	Cluster &cluster = factionGeography.clusters.at(clusterId);
	cluster.type = clusterType;
	
	executionProfiles["1.2.5.0. initializeCluster"].start();
	
	// populate movementTypeSet
	
	robin_hood::unordered_flat_set<MovementType> movementTypeSet;
	
	if (factionId == 0)
	{
		if (clusterType == 0)
		{
			// there are no non-combat aliens
			return;
		}
		else
		{
			movementTypeSet.insert(getUnitMovementType(factionId, (surfaceIndex == 0 ? BSC_MIND_WORMS : BSC_SEALURK)));
		}
		
	}
	else
	{
		if (clusterType == 0)
		{
			movementTypeSet.insert(getUnitMovementType(factionId, (surfaceIndex == 0 ? BSC_COLONY_POD : BSC_SEA_ESCAPE_POD)));
			movementTypeSet.insert(getUnitMovementType(factionId, (surfaceIndex == 0 ? BSC_FORMERS : BSC_SEA_FORMERS)));
		}
		else
		{
			movementTypeSet.insert(getUnitMovementType(factionId, (surfaceIndex == 0 ? BSC_MIND_WORMS : BSC_SEALURK)));
			movementTypeSet.insert(getUnitMovementType(factionId, (surfaceIndex == 0 ? BSC_SCOUT_PATROL : BSC_UNITY_GUNSHIP)));
		}
		
	}
	
	std::vector<MovementType> movementTypes(movementTypeSet.begin(), movementTypeSet.end());
	
	// scan area initially computing cluster area and next landmark
	
	int initialLandmarkTileIndex = initializeCluster(factionId, clusterId, surfaceIndex, movementTypes, initialTileIndex);
	debug("\tarea=%4d\n", cluster.area);
	
	executionProfiles["1.2.5.0. initializeCluster"].stop();
	
	for (MovementType &movementType : movementTypes)
	{
		// compute landmarkCount
		
		int landmarkCount = cluster.area / LANDMARK_COVERAGE;
		cluster.movementInfos.at(movementType).landmarkNetwork.resize(landmarkCount);
		
		// generate landmarks
		
		int landmarkTileIndex = initialLandmarkTileIndex;
		
		for (int landmarkId = 0; landmarkId < landmarkCount; landmarkId++)
		{
			executionProfiles["1.2.5.1. generate landmark"].start();
			landmarkTileIndex = generateLandmark(factionId, clusterId, surfaceIndex, movementType, landmarkId, landmarkTileIndex);
			executionProfiles["1.2.5.1. generate landmark"].stop();
		}
		
	}
	
}

int initializeCluster(int factionId, int clusterId, int surfaceIndex, std::vector<MovementType> &movementTypes, int initialTileIndex)
{
	if (MOVEMENT_INFO_TRACE)
	{
		debug
		(
			"initializeCluster(factionId=%1d, clusterId=%3d, surfaceIndex=%1d, initialTile=(%3d,%3d))"
			"\n"
			, factionId
			, clusterId
			, surfaceIndex
			, getX(initialTileIndex), getY(initialTileIndex)
		);
	}
	
	Geography &factionGeography = aiData.factionGeographys[factionId];
	Cluster &cluster = factionGeography.clusters.at(clusterId);
	std::vector<int> distances;
	
	// set all distances to infinity
	
	executionProfiles["1.2.5.0.0. set all distances to infinity"].start();
	
	distances.resize(*MapAreaTiles, MOVEMENT_INFINITY);
	
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
			bool tileZoc = tileFactionInfo.zoc[cluster.type];
			int tileDistance = distances.at(tileIndex);
			
			// assign clusterId
			
			tileFactionInfo.clusterIds[cluster.type][surfaceIndex] = clusterId;
			
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
				bool adjacentTileBlocked = adjacentTileFactionInfo.blocked[cluster.type];
				bool adjacentTileZoc = adjacentTileFactionInfo.zoc[cluster.type];
				
				// corresponding realm
				
				if (adjacentTileSurfaceAssociation == -1)
					continue;
				
				// available
				
				if (adjacentTileBlocked || (tileZoc && adjacentTileZoc))
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
						hexCostTotal[movementType] += tileInfo.hexCosts[movementType][angle];
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
				(double)Rules->move_rate_roads
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

int generateLandmark(int factionId, int clusterId, int surfaceIndex, MovementType movementType, int landmarkId, int initialTileIndex)
{
	if (MOVEMENT_INFO_TRACE)
	{
		debug
		(
			"generateLandmark(factionId=%1d, clusterId=%3d, surfaceIndex=%1d, movementType=%1d, landmarkId=%2d, initialTile=(%3d,%3d))"
			"\n"
			, factionId
			, clusterId
			, surfaceIndex
			, movementType
			, landmarkId
			, getX(initialTileIndex), getY(initialTileIndex)
		);
	}
	
	Geography &factionGeography = aiData.factionGeographys[factionId];
	Cluster &cluster = factionGeography.clusters.at(clusterId);
	ClusterMovementInfo &clusterMovementInfo = cluster.movementInfos.at(movementType);
	std::vector<std::vector<double>> &landmarkNetwork = clusterMovementInfo.landmarkNetwork;
	std::vector<double> &movementCosts = landmarkNetwork.at(landmarkId);
	
	// set all movementCosts to infinity
	
	executionProfiles["1.2.5.1.0. set all movementCosts to infinity"].start();
	
	movementCosts.resize(*MapAreaTiles, MOVEMENT_INFINITY);
	
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
	double nextLandmarkTileSummaryMovementCost = 0.0;
	
	while (!openNodes.empty())
	{
		for (int tileIndex : openNodes)
		{
			TileInfo &tileInfo = aiData.tileInfos[tileIndex];
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[factionId];
			bool tileZoc = tileFactionInfo.zoc[cluster.type];
			double tileMovementCost = movementCosts.at(tileIndex);
			
			// process adjacent tiles
			
			bool extended = false;
			bool edge = false;
			
			for (int i = 0; i < tileInfo.validAdjacentTileCount; i++)
			{
				int angle = tileInfo.validAdjacentTileAngles[i];
				int adjacentTileIndex = tileInfo.validAdjacentTileIndexes[i];
				int hexCost = tileInfo.hexCosts[movementType][angle];
				
				if (hexCost == -1)
				{
					edge = true;
					continue;
				}
				
				TileInfo &adjacentTileInfo = aiData.tileInfos[adjacentTileIndex];
				TileFactionInfo &adjacentTileFactionInfo = adjacentTileInfo.factionInfos[factionId];
				int adjacentTileSurfaceAssociation = adjacentTileFactionInfo.surfaceAssociations[surfaceIndex];
				bool adjacentTileBlocked = adjacentTileFactionInfo.blocked[cluster.type];
				bool adjacentTileZoc = adjacentTileFactionInfo.zoc[cluster.type];
				
				// corresponding realm
				
				if (adjacentTileSurfaceAssociation == -1)
					continue;
				
				// available
				
				if (adjacentTileBlocked || (tileZoc && adjacentTileZoc))
					continue;
				
				// adjust hexCost with movementImpediment
				
				double impededHexCost = hexCost + adjacentTileFactionInfo.impediment[cluster.type];
				
				// update movementCost
				
				double oldAdjacentTileMovementCost = movementCosts.at(adjacentTileIndex);
				double newAdjacentTileMovementCost = tileMovementCost + impededHexCost;
				
				if (newAdjacentTileMovementCost < oldAdjacentTileMovementCost - 0.1)
				{
					movementCosts.at(adjacentTileIndex) = newAdjacentTileMovementCost;
					newOpenNodes.push_back(adjacentTileIndex);
					
					extended = true;
					
				}
				
			}
			
			// not edge or extended - not a good candidate for next landmark
			
			if (!edge || extended)
				continue;
			
			// update nextLandmarkTileIndex
			
			double summaryMovementCost = tileMovementCost;
			
			for (int otherLandmarkId = 0; otherLandmarkId < landmarkId; otherLandmarkId++)
			{
				double otherLandmarkMovementCost = landmarkNetwork.at(otherLandmarkId).at(tileIndex);
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
	
	return nextLandmarkTileIndex;
	
}

// [origin, destination, factionId, movementType, combat]
robin_hood::unordered_flat_map<long long, int> cachedLMovementCosts;

long long getCachedLMovementCostKey(MAP *origin, MAP *destination, int factionId, int clusterType, int surfaceType, MovementType movementType)
{
	long long key =
		+ (long long)(origin - *MapTiles)			* (long long)MOVEMENT_TYPE_COUNT * 2L * 2L * (long long)MaxPlayerNum * (long long)*MapAreaTiles
		+ (long long)(destination - *MapTiles)	* (long long)MOVEMENT_TYPE_COUNT * 2L * 2L * (long long)MaxPlayerNum
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
	cachedLMovementCosts[key] = movementCost;
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
	
	int originTileIndex = origin - *MapTiles;
	int destinationTileIndex = destination - *MapTiles;
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
	int cachedLMovementCost = getCachedLMovementCost(originToDestinationMovementCostKey);
	
	executionProfiles["| getLMovementCost 1. check cached value"].stop();
	
	if (cachedLMovementCost != -1)
	{
		executionProfiles["| getLMovementCost"].stop();
		return cachedLMovementCost;
	}
	
	executionProfiles["| getLMovementCost 2. estimate movementCost"].start();
	
	// get cluster
	
	Cluster &cluster = aiData.factionGeographys[factionId].clusters.at(originClusterId);
	ClusterMovementInfo &clusterMovementInfo = cluster.movementInfos.at(movementType);
	
	// estimate direct movementCost
	
	int range = getRange(origin, destination);
	double movementCost = (double)range * clusterMovementInfo.averageHexCost;
	
	// estimate landmark movementCost
	
	for (std::vector<double> &landmarkMovementCosts : clusterMovementInfo.landmarkNetwork)
	{
		double landmarkMovementCost = abs(landmarkMovementCosts.at(originTileIndex) - landmarkMovementCosts.at(destinationTileIndex));
		movementCost = std::max(movementCost, landmarkMovementCost);
	}
	
	executionProfiles["| getLMovementCost 2. estimate movementCost"].stop();
	
	// set cached value
	
	executionProfiles["| getAMovementCost 3. set cached value"].start();
	
	setCachedLMovementCost(originToDestinationMovementCostKey, (int)movementCost);
	
	executionProfiles["| getAMovementCost 3. set cached value"].stop();
	
	executionProfiles["| getLMovementCost"].stop();
	
	// return value
	
	return (int)movementCost;
	
}

/*
Estimates unit movement cost based on unit type.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getUnitLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, int action)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(destination >= *MapTiles && destination < *MapTiles + *MapAreaTiles);
	
	UNIT *unit = getUnit(unitId);
	
	// compute travel time by chassis type
	
	int travelTime = MOVEMENT_INFINITY;
	
	switch (unit->chassis_id)
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
int getUnitActionLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId)
{
	int speed = getUnitSpeed(unitId);
	return getUnitLTravelTime(origin, destination, factionId, unitId, speed, 1);
}
int getUnitAttackLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	
	int ocean;
	
	switch (triad)
	{
	case TRIAD_LAND:
		ocean = false;
		break;
	case TRIAD_SEA:
		ocean = true;
		break;
	default:
		// not applicable
		return -1;
	}
	
	int speed = getUnitSpeed(unitId);
	MAP *attackPosition = getMeleeAttackPosition(factionId, ocean, origin, destination, true);
	
	if (attackPosition == nullptr)
		return -1;
	
	return getUnitLTravelTime(origin, attackPosition, factionId, unitId, speed, Rules->move_rate_roads);
	
}

int getGravshipUnitLTravelTime(MAP *origin, MAP *destination, int speed, int action)
{
	int range = getRange(origin, destination);
	
	return divideIntegerRoundUp(range * Rules->move_rate_roads + action, speed);
	
}

/*
Estimates air ranged vehicle combat travel time.
Air ranged vehicle hops between airbases.
*/
int getAirRangedUnitLTravelTime(MAP *origin, MAP *destination, int speed, int action)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(destination >= *MapTiles && destination < *MapTiles + *MapAreaTiles);
	
	int movementRange = (speed - action) / Rules->move_rate_roads;
	
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
	
	return divideIntegerRoundUp(2 * optimalVehicleToAirbaseRange * Rules->move_rate_roads, speed) + 1;
	
}

/*
Estimates sea vehicle combat travel time not counting any obstacles.
ignoreHostile: ignore hostile blocked and zoc.
*/
int getSeaUnitLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool action)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(destination >= *MapTiles && destination < *MapTiles + *MapAreaTiles);
	
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
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(destination >= *MapTiles && destination < *MapTiles + *MapAreaTiles);
	
	int speedOnLand = speed / Rules->move_rate_roads;
	int speedOnRoad = speed / conf.magtube_movement_rate;
	int speedOnTube = speed;
	
	// evaluate different cases
	
	if (isSameLandAssociation(origin, destination, factionId)) // same continent
	{
		// clusterType
		
		int clusterType = (isCombatUnit(unitId) ? 1 : 0);
		
		// movementType
		
		MovementType movementType = getUnitMovementType(factionId, unitId);
		
		// movementCost
		
		int movementCost = getLMovementCost(origin, destination, factionId, clusterType, 0, movementType);
		
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
	
	switch (unit->chassis_id)
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
						+ 0.5 * (double)(speed / Rules->move_rate_roads)
						+ 0.5 * (double)(transportSpeed / Rules->move_rate_roads)
					;
					
					travelTime = (int)ceil((double)range / averageMovementSpeed);
					
				}
				else // remote association
				{
					// roughly estimate travel time
					
					int range = getRange(origin, destination);
					
					double averageMovementSpeed =
						+ 0.5 * (double)(speed / Rules->move_rate_roads)
						+ 0.5 * (double)(transportSpeed / Rules->move_rate_roads)
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
				+ 0.5 * (double)(speed / Rules->move_rate_roads)
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
	MAP *attackPosition = getVehicleMeleeAttackPosition(vehicleId, destination, true);
	return getVehicleLTravelTime(vehicleId, vehicleTile, attackPosition, Rules->move_rate_roads);
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
	if (!*MapToggleFlat)
	{
		if (dx > *MapHalfX)
		{
			dx = *MapAreaX - dx;
		}
	}
	
	// multiply deltas by road movement rate
	
	dx *= Rules->move_rate_roads;
	dy *= Rules->move_rate_roads;
	
	return std::max(dx, dy) - ((((dx + dy) / 2) - std::min(dx, dy) + 1) / 2);
	
}

/*
Returns vehicle movement type.
Ignoring hovering unit ability.
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

