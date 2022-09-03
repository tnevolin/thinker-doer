#include <vector>
#include <set>
#include <queue>
#include "terranx.h"
#include "game_wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMove.h"
#include "aiMoveColony.h"
#include "aiMoveTransport.h"
#include "aiRoute.h"

int clusterSize;

std::vector<Cluster> clusters;

// ==================================================
// Objects
// ==================================================

Cluster::Cluster(Location _coordinates)
: coordinates{_coordinates}
{
	
}

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
// cached movement costs
// ==================================================

// [origin, destination, movementType, factionId, ignoreHostile]
//std::map<std::tuple<MAP *, MAP *, MOVEMENT_TYPE, int, bool>, int> cachedMovementCosts;
std::map<long long, int> cachedMovementCosts;

long long getCachedMovementCostIndex(MAP *origin, MAP *destination, MOVEMENT_TYPE movementType, int factionId, bool ignoreHostile)
{
	long long index =
		+ (long long)(origin - *MapPtr)			* 2L * (long long)MaxPlayerNum * (long long)MOVEMENT_TYPE_COUNT * (long long)*map_area_tiles
		+ (long long)(destination - *MapPtr)	* 2L * (long long)MaxPlayerNum * (long long)MOVEMENT_TYPE_COUNT
		+ (long long)movementType				* 2L * (long long)MaxPlayerNum
		+ (long long)factionId					* 2L
		+ (long long)ignoreHostile				
	;
	
	return index;
	
}

/*
Returns cachedMovementCost or -1 if not found.
*/
int getCachedMovementCost(long long index)
{
	return cachedMovementCosts.count(index) == 0 ? -1 : cachedMovementCosts.at(index);
}

void setCachedMovementCost(long long index, int movementCost)
{
	cachedMovementCosts[index] = movementCost;
}

bool isInfinity(int value)
{
	return value >= MOVEMENT_INFINITY;
}

// ==================================================
// Main operations
// ==================================================

void precomputeAIRouteData()
{
	cachedMovementCosts.clear();
}

/*
Populates all movemen costs from given origin to all accessible destinations.
*/
void populateBuildSiteDMovementCosts(MAP *origin, MOVEMENT_TYPE movementType, int factionId, bool ignoreHostile)
{
	executionProfiles["| populateBuildSiteDMovementCosts"].start();
	
	// initialize collections
	
	std::vector<MAP *> openNodes;
	
	long long originOriginIndex = getCachedMovementCostIndex(origin, origin, movementType, factionId, ignoreHostile);
	setCachedMovementCost(originOriginIndex, 0);
	openNodes.push_back(origin);
	
	// process nodes
	
	while (!openNodes.empty())
	{
		std::vector<MAP *> newOpenNodes;
		
		for (MAP *tile : openNodes)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			bool tileZoc = tileInfo.isZoc(factionId, ignoreHostile);
			
			// cached value
			
			long long originTileIndex = getCachedMovementCostIndex(origin, tile, movementType, factionId, ignoreHostile);
			int tileMovementCost = getCachedMovementCost(originTileIndex);
			
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				MAP *adjacentTile = getTileByAngle(tile, angle);
				
				if (adjacentTile == nullptr)
					continue;
				
				TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
				ExpansionTileInfo &adjacentTileExpansionInfo = getExpansionTileInfo(adjacentTile);
				bool adjacentTileBlocked = adjacentTileInfo.isBlocked(factionId, ignoreHostile);
				bool adjacentTileZoc = adjacentTileInfo.isZoc(factionId, ignoreHostile);
				
				// hex cost
				
				int hexCost = tileInfo.hexCosts[angle][movementType];
				
				// not allowed move
				
				if (hexCost == -1)
					continue;
				
				// valid build site
				
				if (!adjacentTileExpansionInfo.expansionRange)
					continue;
				
				// cached value
				
				long long originAdjacentTileIndex = getCachedMovementCostIndex(origin, tile, movementType, factionId, ignoreHostile);
				int adjacentTileMovementCost = getCachedMovementCost(originAdjacentTileIndex);
				
				// movementCost
				
				int computedMovementCost = (adjacentTileBlocked || tileZoc && adjacentTileZoc ? MOVEMENT_INFINITY : tileMovementCost + hexCost);
				
				// update cached value and add tile to open nodes if better estimate
				
				if (adjacentTileMovementCost == -1 || computedMovementCost < adjacentTileMovementCost)
				{
					setCachedMovementCost(originAdjacentTileIndex, computedMovementCost);
					newOpenNodes.push_back(adjacentTile);
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
			MOVEMENT_TYPE movementType = getUnitMovementType(aiFactionId, unitId);
			
			populateBuildSiteDMovementCosts(baseTile, movementType, aiFactionId, false);
			
		}
		
	}
	
	// colonies
	
	for (int vehicleId : aiData.colonyVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int unitId = vehicle->unit_id;
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		MOVEMENT_TYPE movementType = getUnitMovementType(aiFactionId, unitId);
		
		populateBuildSiteDMovementCosts(vehicleTile, movementType, aiFactionId, false);
		
	}
	
}

/*
Returns all location where this vehicle can move and execute action on same turn.
*/
std::set<MAP *> getOneTurnActionLocations(int vehicleId)
{
	debug("getOneTurnActionLocations\n");
	
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int unitId = vehicle->unit_id;
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MOVEMENT_TYPE movementType = getUnitMovementType(factionId, unitId);
	int speed = getVehicleSpeed(vehicleId);
	
	std::map<MAP *, int> movementCosts;
	std::vector<MAP *> openNodes;
	
	movementCosts.insert({vehicleTile, 0});
	openNodes.push_back(vehicleTile);
	
	while (!openNodes.empty())
	{
		std::vector<MAP *> newOpenNodes;
		
		for (MAP *tile : openNodes)
		{
			TileInfo &tileInfo = aiData.getTileInfo(tile);
			TileMovementInfo &tileMovementInfo = aiData.getTileMovementInfo(tile, factionId);
			bool tileZoc = tileMovementInfo.zoc;
			int tileMovementCost = movementCosts.at(tile);
			
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				MAP *adjacentTile = getTileByAngle(tile, angle);
				
				if (adjacentTile == nullptr)
					continue;
				
				TileMovementInfo &adjacentTileMovementInfo = aiData.getTileMovementInfo(adjacentTile, factionId);
				bool adjacentTileBlocked = adjacentTileMovementInfo.blocked;
				bool adjacentTileZoc = adjacentTileMovementInfo.zoc;
				int hexCost = tileInfo.hexCosts[angle][movementType];
				
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
					newOpenNodes.push_back(adjacentTile);
				}
				
			}
			
		}
		
		// swap nodes
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	// collect locations
	
	std::set<MAP *> locations;
	
	for (std::pair<MAP * const, int> movementCostEntry : movementCosts)
	{
		locations.insert(movementCostEntry.first);
	}
	
	return locations;
	
}

int getAMovementCost(MAP *origin, MAP *destination, MOVEMENT_TYPE movementType, int factionId, bool ignoreHostile)
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
	
	long long originDestinationIndex = getCachedMovementCostIndex(origin, destination, movementType, factionId, ignoreHostile);
	int cachedMovementCost = getCachedMovementCost(originDestinationIndex);
	
	executionProfiles["| getAMovementCost 1. check cached value"].stop();
	
	if (cachedMovementCost != -1)
	{
		executionProfiles["| getAMovementCost"].stop();
		return cachedMovementCost;
	}
	
	// initialize collections
	
	std::priority_queue<FValue, std::vector<FValue>, FValueComparator> openNodes;
	
	long long originOriginIndex = getCachedMovementCostIndex(origin, origin, movementType, factionId, ignoreHostile);
	setCachedMovementCost(originOriginIndex, 0);
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
		TileMovementInfo &currentTileMovementInfo = currentTileInfo.movementInfos[factionId];
		bool currentTileZoc = (ignoreHostile ? currentTileMovementInfo.zocNeutral : currentTileMovementInfo.zoc);
		
		long long originCurrentTileIndex = getCachedMovementCostIndex(origin, currentTile, movementType, factionId, ignoreHostile);
		int currentTileMovementCost = getCachedMovementCost(originCurrentTileIndex);
		
		// check surrounding tiles
		
		for (int angle = 0; angle < ANGLE_COUNT; angle++)
		{
			MAP *adjacentTile = getTileByAngle(currentTile, angle);
			
			if (adjacentTile == nullptr)
				continue;
			
			// hex cost
			
			int adjacentTileHexCost = currentTileInfo.hexCosts[angle][movementType];
			
			// allowed step
			
			if (adjacentTileHexCost == -1)
				continue;
			
			// parameters
			
			TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
			TileMovementInfo &adjacentTileMovementInfo = adjacentTileInfo.movementInfos[factionId];
			bool adjacentTileBlocked = (ignoreHostile ? adjacentTileMovementInfo.blockedNeutral : adjacentTileMovementInfo.blocked);
			bool adjacentTileZoc = (ignoreHostile ? adjacentTileMovementInfo.zocNeutral : adjacentTileMovementInfo.zoc);
			
			long long originAdjacentTileIndex = getCachedMovementCostIndex(origin, adjacentTile, movementType, factionId, ignoreHostile);
			int adjacentTileMovementCost = getCachedMovementCost(originAdjacentTileIndex);
			
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
				setCachedMovementCost(originAdjacentTileIndex, movementCost);
				
				// f value
				// take cashed movement cost if it is known, otherwise estimate
				
				long long adjacentTileDestinationIndex = getCachedMovementCostIndex(adjacentTile, destination, movementType, factionId, ignoreHostile);
				int adjacentTileDestinationMovementCost = getCachedMovementCost(adjacentTileDestinationIndex);
				
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
	
	int movementCost = getCachedMovementCost(originDestinationIndex);
	
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
		travelTime = getGravshipUnitATravelTime(origin, destination, speed, action);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		travelTime = getAirRangedUnitATravelTime(origin, destination, speed, action);
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

int getGravshipUnitATravelTime(MAP *origin, MAP *destination, int speed, int action)
{
	int range = getRange(origin, destination);
	
	return divideIntegerRoundUp(range * Rules->mov_rate_along_roads + action, speed);
	
}

/*
Estimates air ranged vehicle combat travel time.
Air ranged vehicle hops between airbases.
*/
int getAirRangedUnitATravelTime(MAP *origin, MAP *destination, int speed, int action)
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
int getSeaUnitATravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool ignoreHostile, bool action)
{
	assert(origin >= *MapPtr && origin < *MapPtr + *map_area_tiles);
	assert(destination >= *MapPtr && destination < *MapPtr + *map_area_tiles);
	
	// verify same ocean association
	
	if (!isSameOceanAssociation(origin, destination, factionId))
		return -1;
	
	// movementCost
	
	MOVEMENT_TYPE movementType = getUnitMovementType(factionId, unitId);
	
	int movementCost = getAMovementCost(origin, destination, movementType, factionId, ignoreHostile);
	
	// unreachable
	
	if (isInfinity(movementCost))
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
		
		MOVEMENT_TYPE movementType = getUnitMovementType(factionId, unitId);
		
		int movementCost = getAMovementCost(origin, destination, movementType, factionId, ignoreHostile);
		
		// unreachable
		
		if (isInfinity(movementCost))
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
		travelTime = getGravshipUnitATravelTime(origin, destination, speed, action);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		travelTime = getAirRangedUnitATravelTime(origin, destination, speed, action);
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

