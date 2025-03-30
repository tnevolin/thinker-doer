
#include <exception>
#include <vector>
#include <set>
#include <queue>
#include <unordered_map>
#include "robin_hood.h"
#include "wtp_ai.h"
#include "wtp_aiRoute.h"
#include "wtp_aiMoveColony.h"
#include "wtp_aiMoveTransport.h"

// approach time cache
// [factionId][movementType][org][dst]
robin_hood::unordered_flat_map<int, double> cachedUnitApproachTimes;
double getCachedUnitApproachTime(int factionId, MovementType movementType, MAP *org, MAP *dst)
{
	int orgIndex = org - *MapTiles;
	int dstIndex = dst - *MapTiles;
	int key =
		+ factionId		* *MapAreaTiles * *MapAreaTiles * MOVEMENT_TYPE_COUNT
		+ movementType	* *MapAreaTiles * *MapAreaTiles
		+ orgIndex		* *MapAreaTiles
		+ dstIndex		* 1
	;
	robin_hood::unordered_flat_map<int, double>::const_iterator cachedUnitApproachTimeIterator = cachedUnitApproachTimes.find(key);
	return cachedUnitApproachTimeIterator == cachedUnitApproachTimes.end() ? -1.0 : cachedUnitApproachTimeIterator->second;
}
void setCachedUnitApproachTime(int factionId, MovementType movementType, MAP *org, MAP *dst, double value)
{
	int orgIndex = org - *MapTiles;
	int dstIndex = dst - *MapTiles;
	int key =
		+ factionId		* *MapAreaTiles * *MapAreaTiles * MOVEMENT_TYPE_COUNT
		+ movementType	* *MapAreaTiles * *MapAreaTiles
		+ orgIndex		* *MapAreaTiles
		+ dstIndex		* 1
	;
	robin_hood::unordered_flat_map<int, double>::iterator cachedUnitApproachTimeIterator = cachedUnitApproachTimes.find(key);
	if (cachedUnitApproachTimeIterator == cachedUnitApproachTimes.end())
	{
		cachedUnitApproachTimes.insert({key, value});
	}
	else
	{
		cachedUnitApproachTimeIterator->second = value;
	}
}

// map snapshot
MapSnapshot mapSnapshot;

// faction movement infos
std::array<FactionMovementInfo, MaxPlayerNum> factionMovementInfos;
//// approach movement infos
//ApproachMovementInfo approachMovementInfo;
// player faction movement info
PlayerFactionMovementInfo aiFactionMovementInfo;

/**
Processing tiles marker array.
Tile value represent iteration level.
This way it is easy to distinguish inner, border, and new border tiles.
*/
std::vector<int> innerTiles;
std::vector<int> borderTiles;
std::vector<int> newBorderTiles;

//// ==================================================
//// A* cached travel times
//// ==================================================
//
//// [movementType][speed][originIndex][dstIndex]
//robin_hood::unordered_flat_map <long long, double> cachedATravelTimes;
//
// ==================================================
// Main operations
// ==================================================

void populateRouteData()
{
	Profiling::start("populateRouteData", "populateAIData");
	
//	Profiling::start("cachedATravelTimes.clear()", "populateRouteData");
//	cachedATravelTimes.clear();
//	Profiling::stop("cachedATravelTimes.clear()");
	
	populateMapSnapshot();
	precomputeRouteData();
	
	Profiling::stop("populateRouteData");
	
}

void populateMapSnapshot()
{
	Profiling::start("populateMapSnapshot", "populateRouteData");
	
	debug("populateMapSnapshot - %s\n", aiMFaction->noun_faction);
	
	mapSnapshot.mapChanged = false;
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		mapSnapshot.baseChanges.at(factionId).clear();
	}
	
	// check changes
	
	if (mapSnapshot.mapTilesReferencingAddress != *MapTiles || (int)mapSnapshot.surfaceTypes.size() != *MapAreaTiles)
	{
		// map size changed - invalidate everything
		
		mapSnapshot.mapChanged = true;
		debug("\tmapChanged\n");
		
		// reset mapTilesReferencingAddress
		
		mapSnapshot.mapTilesReferencingAddress = *MapTiles;
		
		// repopulate surfaceTypes
		
		mapSnapshot.surfaceTypes.resize(*MapAreaTiles);
		std::fill(mapSnapshot.surfaceTypes.begin(), mapSnapshot.surfaceTypes.end(), -1);
		
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int tileIndex = tile - *MapTiles;
			int surfaceType = is_ocean(tile) ? 1 : 0;
			mapSnapshot.surfaceTypes.at(tileIndex) = surfaceType;
		}
		
		// repopulate mapBases
		
		mapSnapshot.mapBases.clear();
		
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = getBase(baseId);
			MAP *baseTile = getBaseMapTile(baseId);
			mapSnapshot.mapBases.emplace(baseTile, base->faction_id);
		}
		
	}
	else
	{
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int tileIndex = tile - *MapTiles;
			int surfaceType = is_ocean(tile) ? 1 : 0;
			
			if (mapSnapshot.surfaceTypes.at(tileIndex) != surfaceType)
			{
				mapSnapshot.mapChanged = true;
				mapSnapshot.surfaceTypes.at(tileIndex) = surfaceType;
			}
			
		}
		
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = getBase(baseId);
			MAP *baseTile = getBaseMapTile(baseId);
			int baseFactionId = base->faction_id;
			bool sea = is_ocean(baseTile);
			bool port = !sea && isBaseAccessesWater(baseId);
			
			// base appeared
			if (mapSnapshot.mapBases.find(baseTile) == mapSnapshot.mapBases.end())
			{
				debug("\t%s %-24s - appeared\n", getLocationString(baseTile).c_str(), Bases[baseId].name);
				
				for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
				{
					if (isFriendly(factionId, baseFactionId))
					{
						if (sea)
						{
							mapSnapshot.baseChanges.at(factionId).setFriendlySea();
						}
						if (port)
						{
							mapSnapshot.baseChanges.at(factionId).setFriendlyPort();
						}
					}
					
					if (isUnfriendly(factionId, baseFactionId))
					{
						if (sea)
						{
							mapSnapshot.baseChanges.at(factionId).setUnfriendlySea();
						}
						else
						{
							mapSnapshot.baseChanges.at(factionId).setUnfriendlyLand();
						}
					}
					
				}
				
				mapSnapshot.mapBases.emplace(baseTile, baseFactionId);
				
			}
			// base factionId changed
			else if (mapSnapshot.mapBases.at(baseTile) != baseFactionId)
			{
				debug("\t%s %-24s - faction changed\n", getLocationString(baseTile).c_str(), Bases[baseId].name);
				
				int oldBaseFactionId = mapSnapshot.mapBases.at(baseTile);
				int newBaseFactionId = baseFactionId;
				
				for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
				{
					if (isFriendly(factionId, oldBaseFactionId) || isFriendly(factionId, newBaseFactionId))
					{
						if (port)
						{
							mapSnapshot.baseChanges.at(factionId).setFriendlyPort();
						}
					}
					
					if (isUnfriendly(factionId, oldBaseFactionId) || isUnfriendly(factionId, newBaseFactionId))
					{
						if (sea)
						{
							mapSnapshot.baseChanges.at(factionId).setUnfriendlySea();
						}
						else
						{
							mapSnapshot.baseChanges.at(factionId).setUnfriendlyLand();
						}
					}
				}
				
				mapSnapshot.mapBases.at(baseTile) = baseFactionId;
				
			}
			
		}
		
		for (robin_hood::pair<MAP *, int> const &mapBase : mapSnapshot.mapBases)
		{
			MAP *baseTile = mapBase.first;
			int baseFactionId = mapBase.second;
			TileInfo &tileInfo = aiData.getTileInfo(baseTile);
			bool sea = tileInfo.ocean;
			bool port = tileInfo.coast;
			
			int baseId = getBaseAt(baseTile);
			
			// base disappeared
			if (baseId == -1)
			{
				debug("\t%s %-24s - disappeared\n", getLocationString(baseTile).c_str(), Bases[baseId].name);
				
				for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
				{
					if (isFriendly(factionId, baseFactionId))
					{
						if (port)
						{
							mapSnapshot.baseChanges.at(factionId).setFriendlyPort();
						}
					}
					
					if (isUnfriendly(factionId, baseFactionId))
					{
						if (sea)
						{
							mapSnapshot.baseChanges.at(factionId).setUnfriendlySea();
						}
						else
						{
							mapSnapshot.baseChanges.at(factionId).setUnfriendlyLand();
						}
					}
					
				}
				
				mapSnapshot.mapBases.erase(baseTile);
				
			}
			// base factionId changed
			else if (baseFactionId != Bases[baseId].faction_id)
			{
				debug("\t%s %-24s - faction changed\n", getLocationString(baseTile).c_str(), Bases[baseId].name);
				
				int oldBaseFactionId = baseFactionId;
				int newBaseFactionId = Bases[baseId].faction_id;
				
				for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
				{
					if (isFriendly(factionId, oldBaseFactionId) || isFriendly(factionId, newBaseFactionId))
					{
						if (port)
						{
							mapSnapshot.baseChanges.at(factionId).setFriendlyPort();
						}
					}
					
					if (isUnfriendly(factionId, oldBaseFactionId) || isUnfriendly(factionId, newBaseFactionId))
					{
						if (sea)
						{
							mapSnapshot.baseChanges.at(factionId).setUnfriendlySea();
						}
						else
						{
							mapSnapshot.baseChanges.at(factionId).setUnfriendlyLand();
						}
					}
				}
				
				mapSnapshot.mapBases.at(baseTile) = Bases[baseId].faction_id;
				
			}
			
		}
		
	}
	
	Profiling::stop("populateMapSnapshot");
	
}

void precomputeRouteData()
{
	Profiling::start("precomputeRouteData", "populateRouteData");
	
	debug("precomputeRouteData - %s\n", MFactions[aiFactionId].noun_faction);
	
	// generic factions data
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		populateImpediments(factionId);
		populateAirbases(factionId);
		populateAirClusters(factionId);
		populateSeaTransportWaitTimes(factionId);
		populateSeaCombatClusters(factionId);
		populateLandCombatClusters(factionId);
		populateSeaLandmarks(factionId);
		populateLandLandmarks(factionId);
		
	}
	
	// approach data
	
	// player faction data
	
	populateSeaClusters(aiFactionId);
	populateLandClusters();
	populateTransfers(aiFactionId);
	populateLandTransportedClusters();
	populateReachableLocations();
	
	populateSharedSeas();
	
	Profiling::stop("precomputeRouteData");
}

/**
Computes estimated values on how much faction vehicles will be slowed down passing through territory occupied by unfriendly bases/vehicles.
*/
void populateImpediments(int factionId)
{
	// skip processing if no changes required
	
	if (!(mapSnapshot.mapChanged || mapSnapshot.baseChanges.at(factionId).unfriendly))
		return;
	
	Profiling::start("populateImpediments", "precomputeRouteData");
	
	debug("populateImpediments factionId=%d\n", factionId);
	
	std::vector<double> &impediments = factionMovementInfos.at(factionId).impediments;
	impediments.resize(*MapAreaTiles); std::fill(impediments.begin(), impediments.end(), 0.0);
	
	// territory impediments
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tileIndex);
		
		if (tileInfo.ocean)
		{
			// sea
			
			// hostile faction destroys trespassing enemy ships
			
			if (isHostileTerritory(factionId, tile))
			{
				impediments.at(tileIndex) += Rules->move_rate_roads * IMPEDIMENT_SEA_TERRITORY_HOSTILE;
			}
			
		}
		else
		{
			// land
			
			// hostile faction destroys trespassing enemy  vehicles
			
			if (isHostileTerritory(factionId, tile))
			{
				impediments.at(tileIndex) += Rules->move_rate_roads * IMPEDIMENT_LAND_TERRITORY_HOSTILE;
			}
			
		}
		
	}
	
	// interbase impediments
	
	std::vector<double> sumBaseDiagonalDistanceRemainders(*MapAreaTiles);
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// land base
		
		if (is_ocean(baseTile))
			continue;
		
		// unfriendly base
		
		if (!isUnfriendly(factionId, base->faction_id))
			continue;
		
		// scan range tiles
		
		for (MAP *rangeTile : getRangeTiles(baseTile, 5, true))
		{
			int rangeTileIndex = rangeTile - *MapTiles;
			
			// land
			
			if (is_ocean(rangeTile))
				continue;
			
			// unfriendly territory
			
			if (!isUnfriendlyTerritory(factionId, rangeTile))
				continue;
			
			// compute value
			
			double diagonalDistance = getDiagonalDistance(baseTile, rangeTile);
			double diagonalDistanceRemainder = 5.0 - diagonalDistance;
			
			sumBaseDiagonalDistanceRemainders.at(rangeTileIndex) += diagonalDistanceRemainder;
			
		}
		
	}
			
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		if (sumBaseDiagonalDistanceRemainders.at(tileIndex) >= 5.0)
		{
			impediments.at(tileIndex) += Rules->move_rate_roads * IMPEDIMENT_LAND_INTERBASE_UNFRIENDLY;
		}
			
	}
	
	if (DEBUG)
	{
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int tileIndex = tile - *MapTiles;
			
			debug("\t%s %5.2f\n", getLocationString(tile).c_str(), impediments.at(tileIndex));
			
		}
		
	}
	
	Profiling::stop("populateImpediments");
	
}

void populateAirbases(int factionId)
{
	// skip processing if no changes required
	
	if (!(mapSnapshot.mapChanged || mapSnapshot.baseChanges.at(factionId).friendly))
		return;
	
	Profiling::start("populateAirbases", "precomputeRouteData");
	
	debug("populateAirbases aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<MAP *> &airbases = factionMovementInfos.at(factionId).airbases;
	airbases.clear();
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		if (map_has_item(tile, BIT_BASE_IN_TILE | BIT_AIRBASE) && isFriendlyTerritory(factionId, tile))
		{
			airbases.push_back(tile);
		}
		
	}
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		if (vehicle_has_ability(vehicleId, ABL_CARRIER) && vehicle->faction_id == factionId)
		{
			airbases.push_back(getMapTile(vehicle->x, vehicle->y));
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (MAP *airbase : airbases)
//		{
//			debug("\t%s\n", getLocationString(airbase).c_str());
//		}
//		
//	}
	
	Profiling::stop("populateAirbases");
	
}

void populateAirClusters(int factionId)
{
	Profiling::start("populateAirClusters", "precomputeRouteData");
	
	debug("populateAirClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<MAP *> const &airbases = factionMovementInfos.at(factionId).airbases;
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<int>>> &airClusters = factionMovementInfos.at(factionId).airClusters;
	
	airClusters.clear();
	
	// collect unit/vehicle speeds
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>> speeds;
	
	for (int unitId : getFactionUnitIds(factionId, false, true))
	{
		UNIT *unit = getUnit(unitId);
		int chassisId = unit->chassis_id;
		int triad = unit->triad();
		int speed = getUnitSpeed(factionId, unitId);
		
		// air unit
		
		if (triad != TRIAD_AIR)
			continue;
		
		// add speed
		
		speeds[chassisId].insert(speed);
		
	}
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int unitId = vehicle->unit_id;
		UNIT *unit = getUnit(unitId);
		int chassisId = unit->chassis_id;
		int triad = unit->triad();
		int speed = getUnitSpeed(factionId, unitId);
		
		// this faction
		
		if (vehicle->faction_id != factionId)
			continue;
		
		// air unit
		
		if (triad != TRIAD_AIR)
			continue;
		
		// add speed
		
		speeds[chassisId].insert(speed);
		
	}
	
	// process speeds
	
	for (robin_hood::pair<int, robin_hood::unordered_flat_set<int>> &airChassisSpeedEntry : speeds)
	{
		int chassisId = airChassisSpeedEntry.first;
		robin_hood::unordered_flat_set<int> &speeds = airChassisSpeedEntry.second;
		
		int range = (chassisId == CHS_COPTER ? 4 : Chassis[chassisId].range);
		
		// set collection
		
		airClusters.emplace(chassisId, robin_hood::unordered_flat_map<int, std::vector<int>>());
		
		// process speeds
		
		for (int speed : speeds)
		{
			
			int transferRange = range * speed;
			int reachRange = speed - 1;
			
			// set collection
			
			airClusters.at(chassisId).emplace(speed, std::vector<int>(*MapAreaTiles));
			std::fill(airClusters.at(chassisId).at(speed).begin(), airClusters.at(chassisId).at(speed).end(), -1);
			
			// gravship reaches everywhere
			
			if (chassisId == CHS_GRAVSHIP)
			{
				for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
				{
					airClusters.at(chassisId).at(speed).at(tileIndex) = 0;
				}
				
			}
			
			// set available nodes
			
			robin_hood::unordered_flat_set<MAP *> availableNodes(airbases.begin(), airbases.end());
			
			int airClusterIndex = 0;
			
			while (!availableNodes.empty())
			{
				robin_hood::unordered_flat_set<MAP *> closedNodes;
				robin_hood::unordered_flat_set<MAP *> openNodes;
				robin_hood::unordered_flat_set<MAP *> newOpenNodes;
				
				openNodes.insert(*(availableNodes.begin()));
				availableNodes.erase(availableNodes.begin());
				
				while (!openNodes.empty())
				{
					for (MAP *openNode : openNodes)
					{
						for (MAP *availableNode : availableNodes)
						{
							int nodeRange = getRange(openNode, availableNode);
							
							if (nodeRange > transferRange)
								continue;
							
							newOpenNodes.insert(availableNode);
							
						}
						
						// erase openNodes from availableNodes
						
						for (MAP *newOpenNode : newOpenNodes)
						{
							availableNodes.erase(newOpenNode);
						}
						
						for (MAP *openNode : openNodes)
						{
							closedNodes.insert(openNode);
						}
						
						openNodes.swap(newOpenNodes);
						newOpenNodes.clear();
						
					}
					
				}
				
				// set air reach cluster
				
				for (MAP *closedNode : closedNodes)
				{
					for (MAP *tile : getRangeTiles(closedNode, reachRange, true))
					{
						airClusters.at(chassisId).at(speed).at(tile - *MapTiles) = airClusterIndex;
					}
					
				}
				
				// increment airClusterIndex
				
				airClusterIndex++;
				
			}
			
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (robin_hood::pair<int, robin_hood::unordered_flat_map<int, std::vector<int>>> const &airClusterEntry : airClusters)
//		{
//			int chassisId = airClusterEntry.first;
//			robin_hood::unordered_flat_map<int, std::vector<int>> const &chassisAirClusters = airClusterEntry.second;
//			
//			debug("\tchassisId=%2d\n", chassisId);
//			
//			for (robin_hood::pair<int, std::vector<int>> const &chassisAirClusterEntry : chassisAirClusters)
//			{
//				int speed = chassisAirClusterEntry.first;
//				std::vector<int> const &chassisAirSpeedAirClusters = chassisAirClusterEntry.second;
//				
//				debug("\t\tspeed=%2d\n", speed);
//				
//				for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//				{
//					debug("\t\t\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), chassisAirSpeedAirClusters.at(tileIndex));
//				}
//				
//			}
//			
//		}
//		
//	}
	
	Profiling::stop("populateAirClusters");
	
}

void populateSeaTransportWaitTimes(int factionId)
{
	Profiling::start("populateSeaTransportWaitTimes", "precomputeRouteData");
	
	debug("populateSeaTransportWaitTimes aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	int const WAIT_TIME_MULTIPLIER = 2.0;
	
	FactionInfo const &factionInfo = aiData.factionInfos.at(factionId);
	
	std::vector<double> &seaTransportWaitTimes = factionMovementInfos.at(factionId).seaTransportWaitTimes;
	seaTransportWaitTimes.resize(*MapAreaTiles); std::fill(seaTransportWaitTimes.begin(), seaTransportWaitTimes.end(), INF);
	
	std::vector<double> const &impediments = factionMovementInfos.at(factionId).impediments;
	
	robin_hood::unordered_flat_set<MAP *> openNodes;
	robin_hood::unordered_flat_set<MAP *> newOpenNodes;
	
	// initial tiles
	
	struct SeaTransportInitialTile
	{
		MAP *tile;
		int buildTime;
		int capacity;
		MovementType movementType;
		int moveRate;
	};
	
	// [build time, capacity, speed]
	std::vector<SeaTransportInitialTile> initialTiles;
	
	// seaTransports
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// this faction
		
		if (vehicle->faction_id != factionId)
			continue;
		
		// seaTransport
		
		if (!isSeaTransportVehicle(vehicleId))
			continue;
		
		// add initial tile
		
		initialTiles.push_back({getVehicleMapTile(vehicleId), 0, Units[vehicle->unit_id].carry_capacity, getVehicleMovementType(vehicleId), getVehicleMoveRate(vehicleId)});
		
	}
	
	// shipyards
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		
		// this faction
		
		if (base->faction_id != factionId)
			continue;
		
		// can build ship
		
		if (!isBaseCanBuildShip(baseId))
			continue;
		
		// build time
		
		int buildTime = 2 * getBaseItemBuildTime(baseId, factionInfo.bestSeaTransportUnitId, false);
		
		// add initial tile
		
		initialTiles.push_back({getBaseMapTile(baseId), buildTime, factionInfo.bestSeaTransportUnitCapacity, getUnitMovementType(factionInfo.bestSeaTransportUnitId, factionId), Rules->move_rate_roads * factionInfo.bestSeaTransportUnitSpeed});
		
	}
	
//	if (DEBUG)
//	{
//		debug("\tinitialTiles\n");
//		for (SeaTransportInitialTile &initialTile : initialTiles)
//		{
//			debug("\t\t%s %2d %d %d\n", getLocationString(initialTile.tile).c_str(), initialTile.buildTime, initialTile.capacity, initialTile.moveRate);
//		}
//		
//	}
	
	// process sea transport initial tiles
	
	for (SeaTransportInitialTile &initialTile : initialTiles)
	{
		MAP *tile = initialTile.tile;
		int tileIndex = tile - *MapTiles;
		
		// has capacity
		
		if (initialTile.capacity <= 0)
			continue;
		
		// has speed
		
		if (initialTile.moveRate <= 0)
			continue;
		
		// parameters
		
		double capacityCoefficient = initialTile.capacity >= 8 ? 1.0 : 1.0 + 1.0 * (1.0 - (double)initialTile.capacity / (double)8);
		
		// process tiles
		
		openNodes.clear();
		newOpenNodes.clear();
		
		
		double waitTime = (double)initialTile.buildTime;
		if (waitTime < seaTransportWaitTimes.at(tileIndex))
		{
			seaTransportWaitTimes.at(tileIndex) = waitTime;
			openNodes.insert(tile);
		}
		
		while (!openNodes.empty())
		{
			for (MAP * currentTile : openNodes)
			{
				int currentTileIndex = currentTile - *MapTiles;
				double currentTileWaitTime = seaTransportWaitTimes.at(currentTileIndex);
				
				for (MapAngle mapAngle : getAdjacentMapAngles(tile))
				{
					MAP *adjacentTile = mapAngle.tile;
					int adjacentTileAngle = mapAngle.angle;
					
					int adjacentTileIndex = adjacentTile - *MapTiles;
					TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
					
					// sea or friendly base
					
					if (!(is_ocean(adjacentTile) || isFriendlyBaseAt(adjacentTile, factionId)))
						continue;
					
					// hexCost
					
					int hexCost = adjacentTileInfo.hexCosts.at(initialTile.movementType).at(adjacentTileAngle).approximate;
					
					if (hexCost == -1)
						continue;
					
					double stepCost = (double)hexCost + impediments.at(adjacentTileIndex);
					
					double moveTime = stepCost / (double)initialTile.moveRate;
					double waitTime = WAIT_TIME_MULTIPLIER * capacityCoefficient * moveTime;
					double adjacentTileWaitTime = currentTileWaitTime + waitTime;
					
					if (adjacentTileWaitTime < seaTransportWaitTimes.at(adjacentTileIndex))
					{
						seaTransportWaitTimes.at(adjacentTileIndex) = adjacentTileWaitTime;
						newOpenNodes.insert(adjacentTile);
					}
					
				}
				
			}
			
			openNodes.clear();
			openNodes.swap(newOpenNodes);
			
		}
		
	}
	
//	if (DEBUG)
//	{
//		debug("\twaitTimes\n");
//		
//		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
//		{
//			int tileIndex = tile - *MapTiles;
//			double seaTransportWaitTime = seaTransportWaitTimes.at(tileIndex);
//			
//			if (seaTransportWaitTime != INF)
//			{
//				debug("\t\t%s %5.2f\n", getLocationString(tile).c_str(), seaTransportWaitTime);
//			}
//			
//		}
//		
//	}
	
	Profiling::stop("populateSeaTransportWaitTimes");
	
}

void populateSeaCombatClusters(int factionId)
{
	// skip processing if no changes required
	
	if (!(mapSnapshot.mapChanged || mapSnapshot.baseChanges.at(factionId).unfriendlySea || mapSnapshot.baseChanges.at(factionId).friendlyPort))
		return;
	
	Profiling::start("populateSeaCombatClusters", "precomputeRouteData");
	
	debug("populateSeaCombatClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<int> &seaCombatClusters = factionMovementInfos.at(factionId).seaCombatClusters;
	seaCombatClusters.resize(*MapAreaTiles); std::fill(seaCombatClusters.begin(), seaCombatClusters.end(), -1);
	
	robin_hood::unordered_flat_set<int> openNodes;
	robin_hood::unordered_flat_set<int> newOpenNodes;
	
	// availableTiles
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		
		// sea
		
		if (!tile->is_sea())
			continue;
		
		// not unfriendly base
		
		if (isUnfriendlyBaseAt(tile, factionId))
			continue;
		
		// set available
		
		seaCombatClusters.at(tileIndex) = -2;
		
	}
	
	// friendly ports
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		int baseTileIndex = baseTile - *MapTiles;
		
		// land
		
		if (!baseTile->is_land())
			continue;
		
		// friendly
		
		if (!isFriendly(factionId, base->faction_id))
			continue;
		
		// accesses to water
		
		if (!isBaseAccessesWater(baseId))
			continue;
		
		// set available
		
		seaCombatClusters.at(baseTileIndex) = -2;
		
	}
	
	// process tiles
	
	int clusterIndex = 0;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		// available
		
		if (seaCombatClusters.at(tileIndex) != -2)
			continue;
		
		// reset collections
		
		openNodes.clear();
		newOpenNodes.clear();
		
		// set initial tile
		
		seaCombatClusters.at(tileIndex) = clusterIndex;
		openNodes.insert(tileIndex);
		
		// expand cluster
		
		while (!openNodes.empty())
		{
			for (int currentTileIndex : openNodes)
			{
				TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
				
				for (Adjacent const &adjacent : currentTileInfo.adjacents)
				{
					int adjacentTileIndex = adjacent.tileInfo->index;
					
					// available
					
					if (seaCombatClusters.at(adjacentTileIndex) != -2)
						continue;
					
					// insert adjacent tile
					
					seaCombatClusters.at(adjacentTileIndex) = clusterIndex;
					newOpenNodes.insert(adjacentTileIndex);
					
				}
				
			}
			
			openNodes.clear();
			openNodes.swap(newOpenNodes);
			
		}
		
		// increment clusterIndex
		
		clusterIndex++;
			
	}
	
//	if (DEBUG)
//	{
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), seaCombatClusters.at(tileIndex));
//			
//		}
//		
//	}
	
	Profiling::stop("populateSeaCombatClusters");
	
}

void populateLandCombatClusters(int factionId)
{
	// skip processing if no changes required
	
	if (!(mapSnapshot.mapChanged || mapSnapshot.baseChanges.at(factionId).unfriendlyLand))
		return;
	
	Profiling::start("populateLandCombatClusters", "precomputeRouteData");
	
	debug("populateLandCombatClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<double> const &seaTransportWaitTimes = factionMovementInfos.at(factionId).seaTransportWaitTimes;
	std::vector<int> &landCombatClusters = factionMovementInfos.at(factionId).landCombatClusters;
	
	landCombatClusters.resize(*MapAreaTiles); std::fill(landCombatClusters.begin(), landCombatClusters.end(), -1);
	
	// availableTileIndexes
	
	for (TileInfo const &tileInfo : aiData.tileInfos)
	{
		// land or sea with finite wait time
		
		if (!(tileInfo.land || seaTransportWaitTimes.at(tileInfo.index) < INF))
			continue;
		
		// not unfriendly base
		
		if (isUnfriendlyBaseAt(tileInfo.tile, factionId))
			continue;
		
		landCombatClusters.at(tileInfo.index) = -2;
		
	}
	
	// process tiles
	
	std::vector<int> borderTiles;
	std::vector<int> newBorderTiles;
	
	int clusterIndex = 0;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		// available
		
		if (landCombatClusters.at(tileIndex) != -2)
			continue;
		
		// set collections
		
		borderTiles.clear();
		newBorderTiles.clear();
		
		// insert initial tile
		
		landCombatClusters.at(tileIndex) = clusterIndex;
		
		borderTiles.push_back(tileIndex);
		
		// expand cluster
		
		while (!borderTiles.empty())
		{
			for (int currentTileIndex : borderTiles)
			{
				TileInfo &currentTileInfo = aiData.tileInfos[currentTileIndex];
				
				for (Adjacent const &adjacent : currentTileInfo.adjacents)
				{
					int adjacentTileIndex = adjacent.tileInfo->index;
					
					// available
					
					if (landCombatClusters.at(adjacentTileIndex) != -2)
						continue;
					
					// insert adjacent tile
					
					landCombatClusters.at(adjacentTileIndex) = clusterIndex;
					
					newBorderTiles.push_back(adjacentTileIndex);
					
				}
				
			}
			
			// swap border tiles and clear them
			
			borderTiles.swap(newBorderTiles);
			newBorderTiles.clear();
			
		}
		
		// increment clusterIndex
		
		clusterIndex++;
			
	}
	
//	if (DEBUG)
//	{
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), landCombatClusters.at(tileIndex));
//			
//		}
//		
//	}
	
	Profiling::stop("populateLandCombatClusters");
	
}

void populateSeaLandmarks(int factionId)
{
	// skip processing if no changes required
	
	if (!(mapSnapshot.mapChanged || mapSnapshot.baseChanges.at(factionId).unfriendlySea || mapSnapshot.baseChanges.at(factionId).friendlyPort))
		return;
	
	Profiling::start("populateSeaLandmarks", "precomputeRouteData");
	
	debug("populateSeaLandmarks - %s\n", aiMFaction->noun_faction);
	
	int minLandmarkDistance = MIN_LANDMARK_DISTANCE * *MapAreaX / 80;
	debug("\tminLandmarkDistance=%d\n", minLandmarkDistance);
	
	std::vector<int> const &seaCombatClusters = factionMovementInfos.at(factionId).seaCombatClusters;
	std::vector<double> const &impediments = factionMovementInfos.at(factionId).impediments;
	std::array<std::vector<SeaLandmark>, SEA_MOVEMENT_TYPE_COUNT> &seaLandmarks = factionMovementInfos.at(factionId).seaLandmarks;
	
	robin_hood::unordered_flat_set<size_t> openNodes;
	robin_hood::unordered_flat_set<size_t> newOpenNodes;
	robin_hood::unordered_flat_set<size_t> endNodes;
	
	// collect initialTiles
	
	std::map<int, int> initialTiles;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		int seaCluster = seaCombatClusters.at(tileIndex);
		
		if (seaCluster == -1)
			continue;
		
		if (initialTiles.find(seaCluster) != initialTiles.end())
			continue;
		
		initialTiles.emplace(seaCluster, tileIndex);
		
	}
	
	// iterate movementTypes
	
	for (size_t seaMovementTypeIndex = 0; seaMovementTypeIndex < SEA_MOVEMENT_TYPE_COUNT; seaMovementTypeIndex++)
	{
		MovementType movementType = SEA_MOVEMENT_TYPES.at(seaMovementTypeIndex);
		
		std::vector<SeaLandmark> &landmarks = seaLandmarks.at(seaMovementTypeIndex);
		landmarks.clear();
		
		// iterate seaClusters
		
		for (std::pair<int, int> const &initialTileEntry : initialTiles)
		{
			int seaCluster = initialTileEntry.first;
			int landmarkTileIndex = initialTileEntry.second;
			
			endNodes.clear();
			
			// generate landmarks
			
			while (true)
			{
				// create landmark
				
				landmarks.emplace_back();
				SeaLandmark &landmark = landmarks.back();
				
				landmark.tileIndex = landmarkTileIndex;
				landmark.tileInfos.clear();
				landmark.tileInfos.resize(*MapAreaTiles);
				
				// populate initial open nodes
				
				openNodes.clear();
				newOpenNodes.clear();
				
				landmark.tileInfos.at(landmarkTileIndex).distance = 0;
				landmark.tileInfos.at(landmarkTileIndex).movementCost = 0.0;
				openNodes.insert(landmarkTileIndex);
				
				int currentDistance = 0;
				
				// time sensitive code - minimize function call and computations inside
				while (!openNodes.empty())
				{
					for (int currentTileIndex : openNodes)
					{
						TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
						SeaLandmarkTileInfo &currentTileSeaLandmarkTileInfo = landmark.tileInfos.at(currentTileIndex);
						
						bool endNode = true;
						
						for (Adjacent const &adjacent : currentTileInfo.adjacents)
						{
							TileInfo *adjacentTileInfo = adjacent.tileInfo;
							int adjacentTileIndex = adjacentTileInfo->index;
							int hexCost = currentTileInfo.hexCosts.at(movementType).at(adjacent.angle).approximate;
							
							if (hexCost == -1)
								continue;
							
							int adjacentTileSeaCluster = seaCombatClusters.at(adjacentTileIndex);
							
							if (adjacentTileSeaCluster != seaCluster)
								continue;
							
							double stepCost = (double)hexCost + impediments.at(adjacentTileIndex);
							
							// update value
							
							SeaLandmarkTileInfo &adjacentTileSeaLandmarkTileInfo = landmark.tileInfos.at(adjacentTileIndex);
							
							double oldMovementCost = adjacentTileSeaLandmarkTileInfo.movementCost;
							double newMovementCost = currentTileSeaLandmarkTileInfo.movementCost + stepCost;
							
							if (newMovementCost < oldMovementCost)
							{
								adjacentTileSeaLandmarkTileInfo.distance = currentDistance;
								adjacentTileSeaLandmarkTileInfo.movementCost = newMovementCost;
								newOpenNodes.insert(adjacentTileIndex);
								
								endNode = false;
								
							}
							
						}
						
						if (endNode)
						{
							endNodes.insert(currentTileIndex);
						}
						
					}
					
					openNodes.clear();
					openNodes.swap(newOpenNodes);
					
					currentDistance++;
					
				}
				
				// select next landmark
				
				int fartherstEndNodeTileIndex = -1;
				int fartherstEndNodeDistance = 0;
				
				for (int otherEndNodeTileIndex : endNodes)
				{
					int nearestLandmarkDistance = INT_MAX;
					
					for (SeaLandmark &otherSeaLandmark : landmarks)
					{
						size_t otherSeaLandmarkTileIndex = otherSeaLandmark.tileIndex;
						int otherSeaLandmarkCluster = seaCombatClusters.at(otherSeaLandmarkTileIndex);
						
						if (otherSeaLandmarkCluster != seaCluster)
							continue;
						
						int distance = otherSeaLandmark.tileInfos.at(otherEndNodeTileIndex).distance;
						
						if (distance == INT_MAX)
							continue;
						
						if (distance < nearestLandmarkDistance)
						{
							nearestLandmarkDistance = distance;
						}
						
					}
					
					if (nearestLandmarkDistance > fartherstEndNodeDistance)
					{
						fartherstEndNodeTileIndex = otherEndNodeTileIndex;
						fartherstEndNodeDistance = nearestLandmarkDistance;
					}
					
				}
				
				if (fartherstEndNodeTileIndex == -1 || fartherstEndNodeDistance < minLandmarkDistance)
				{
					// exit cycle
					break;
				}
				
				// set new landmark
				
				landmarkTileIndex = fartherstEndNodeTileIndex;
				endNodes.erase(landmarkTileIndex);
				
			}
			
		}
			
	}
	
//	if (DEBUG)
//	{
//		for (size_t seaMovementTypeIndex = 0; seaMovementTypeIndex < SEA_MOVEMENT_TYPE_COUNT; seaMovementTypeIndex++)
//		{
//			MovementType const movementType = SEA_MOVEMENT_TYPES.at(seaMovementTypeIndex);
//			
//			debug("\tmovementType=%d\n", movementType);
//			
//			std::vector<SeaLandmark> const &landmarks = landmarksArray.at(seaMovementTypeIndex);
//			
//			for (SeaLandmark const &landmark : landmarks)
//			{
//				debug("\t\t%s\n", getLocationString(landmark.tileIndex).c_str());
//				for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//				{
//					debug("\t\t\t%s %f %f\n", getLocationString(tileIndex).c_str(), landmark.tileInfos.at(tileIndex).movementCost, landmark.tileInfos.at(tileIndex).seaMovementCost);
//				}
//			}
//			
//		}
//			
//	}
	
	Profiling::stop("populateSeaLandmarks");
	
}

void populateLandLandmarks(int factionId)
{
	// skip processing if no changes required
	
	if (!(mapSnapshot.mapChanged || mapSnapshot.baseChanges.at(factionId).unfriendly || mapSnapshot.baseChanges.at(factionId).friendlyPort))
		return;
	
	Profiling::start("populateLandLandmarks", "precomputeRouteData");
	
	debug("populateLandLandmarks - aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	FactionInfo const &factionInfo = aiData.factionInfos.at(factionId);
	
	int minLandmarkDistance = MIN_LANDMARK_DISTANCE * *MapAreaX / 80;
	debug("\tminLandmarkDistance=%d\n", minLandmarkDistance);
	
	std::vector<int> const &landCombatClusters = factionMovementInfos.at(factionId).landCombatClusters;
	std::vector<double> const &seaTransportWaitTimes = factionMovementInfos.at(factionId).seaTransportWaitTimes;
	std::vector<double> const &impediments = factionMovementInfos.at(factionId).impediments;
	std::array<std::vector<LandLandmark>, BASIC_LAND_MOVEMENT_TYPE_COUNT> &landLandmarks = factionMovementInfos.at(factionId).landLandmarks;
	
	robin_hood::unordered_flat_set<size_t> openNodes;
	robin_hood::unordered_flat_set<size_t> newOpenNodes;
	robin_hood::unordered_flat_set<size_t> endNodes;
	
	// collect initialTiles
	
	std::map<int, int> initialTiles;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		int landCluster = landCombatClusters.at(tileIndex);
		
		if (landCluster == -1)
			continue;
		
		if (initialTiles.find(landCluster) != initialTiles.end())
			continue;
		
		initialTiles.emplace(landCluster, tileIndex);
		
	}
	
	// iterate basic movementTypes
	
	for (size_t movementTypeIndex = 0; movementTypeIndex < BASIC_LAND_MOVEMENT_TYPE_COUNT; movementTypeIndex++)
	{
		MovementType movementType = BASIC_LAND_MOVEMENT_TYPES.at(movementTypeIndex);
		
		std::vector<LandLandmark> &landmarks = landLandmarks.at(movementTypeIndex);
		landmarks.clear();
		
		// iterate landClusters
		
		for (std::pair<int, int> const &initialTileEntry : initialTiles)
		{
			int landCluster = initialTileEntry.first;
			int landmarkTileIndex = initialTileEntry.second;
			
			endNodes.clear();
			
			// generate landmarks
			
			while (true)
			{
				// create landmark
				
				landmarks.emplace_back();
				LandLandmark &landmark = landmarks.back();
				
				landmark.tileIndex = landmarkTileIndex;
				landmark.tileInfos.clear();
				landmark.tileInfos.resize(*MapAreaTiles);
				
				// populate initial open nodes
				
				openNodes.clear();
				newOpenNodes.clear();
				
				landmark.tileInfos.at(landmarkTileIndex).distance = 0;
				landmark.tileInfos.at(landmarkTileIndex).travelTime = 0.0;
				openNodes.insert(landmarkTileIndex);
				
				int currentDistance = 0;
				
				// time sensitive code - minimize function call and computations inside
				while (!openNodes.empty())
				{
					for (int currentTileIndex : openNodes)
					{
						TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
						LandLandmarkTileInfo &currentTileLandLandmarkTileInfo = landmark.tileInfos.at(currentTileIndex);
						
						bool endNode = true;
						
						for (Adjacent const &adjacent : currentTileInfo.adjacents)
						{
							int adjacentTileIndex = adjacent.tileInfo->index;
							
							// should be in same land transported cluster
							
							int adjacentTileLandCluster = landCombatClusters.at(adjacentTileIndex);
							if (adjacentTileLandCluster != landCluster)
								continue;
							
							// land-land
							if (currentTileInfo.land && adjacent.tileInfo->land)
							{
								// sensible land movement cost
								
								int hexCost = currentTileInfo.hexCosts.at(movementType).at(adjacent.angle).approximate;
								if (hexCost == -1)
									continue;
								
								LandLandmarkTileInfo &adjacentTileLandLandmarkTileInfo = landmark.tileInfos.at(adjacentTileIndex);
								
								double stepCost = (double)hexCost + impediments.at(adjacentTileIndex);
								
								// update value
								
								double oldTravelTime = adjacentTileLandLandmarkTileInfo.travelTime;
								
								double newLandMovementCost = currentTileLandLandmarkTileInfo.landMovementCost + stepCost;
								double newTravelTime = currentTileLandLandmarkTileInfo.travelTime + stepCost / 2.0;
								
								if (newTravelTime < oldTravelTime)
								{
									adjacentTileLandLandmarkTileInfo.distance = currentDistance;
									adjacentTileLandLandmarkTileInfo.landMovementCost = newLandMovementCost;
									adjacentTileLandLandmarkTileInfo.travelTime = newTravelTime;
									newOpenNodes.insert(adjacentTileIndex);
									
									endNode = false;
									
								}
								
							}
							// land-sea
							else if (currentTileInfo.land && adjacent.tileInfo->ocean)
							{
								LandLandmarkTileInfo &adjacentTileLandLandmarkTileInfo = landmark.tileInfos.at(adjacentTileIndex);
								
								double seaTransportWaitTime = seaTransportWaitTimes.at(adjacentTileIndex);
								double stepCost = (double)Rules->move_rate_roads;
								
								// update value
								
								double oldTravelTime = adjacentTileLandLandmarkTileInfo.travelTime;
								
								double newSeaTransportWaitTime = currentTileLandLandmarkTileInfo.seaTransportWaitTime + seaTransportWaitTime;
								double newLandMovementCost = currentTileLandLandmarkTileInfo.landMovementCost + stepCost;
								double newTravelTime = currentTileLandLandmarkTileInfo.travelTime + seaTransportWaitTime + stepCost / 2.0;
								
								if (newTravelTime < oldTravelTime)
								{
									adjacentTileLandLandmarkTileInfo.distance = currentDistance;
									adjacentTileLandLandmarkTileInfo.seaTransportWaitTime = newSeaTransportWaitTime;
									adjacentTileLandLandmarkTileInfo.landMovementCost = newLandMovementCost;
									adjacentTileLandLandmarkTileInfo.travelTime = newTravelTime;
									newOpenNodes.insert(adjacentTileIndex);
									
									endNode = false;
									
								}
								
							}
							// sea-sea
							else if (currentTileInfo.ocean && adjacent.tileInfo->ocean)
							{
								// sensible sea movement cost
								
								int hexCost = currentTileInfo.hexCosts.at(MT_SEA).at(adjacent.angle).approximate;
								if (hexCost == -1)
									continue;
								
								LandLandmarkTileInfo &adjacentTileLandLandmarkTileInfo = landmark.tileInfos.at(adjacentTileIndex);
								
								double stepCost = (double)hexCost + impediments.at(adjacentTileIndex);
								
								// update value
								
								double oldTravelTime = adjacentTileLandLandmarkTileInfo.travelTime;
								
								double newSeaMovementCost = currentTileLandLandmarkTileInfo.seaMovementCost + stepCost;
								double newTravelTime = currentTileLandLandmarkTileInfo.travelTime + stepCost / (double)(Rules->move_rate_roads * factionInfo.bestSeaTransportUnitSpeed);
								
								if (newTravelTime < oldTravelTime)
								{
									adjacentTileLandLandmarkTileInfo.distance = currentDistance;
									adjacentTileLandLandmarkTileInfo.seaMovementCost = newSeaMovementCost;
									adjacentTileLandLandmarkTileInfo.travelTime = newTravelTime;
									newOpenNodes.insert(adjacentTileIndex);
									
									endNode = false;
									
								}
								
							}
							// sea-land
							else if (currentTileInfo.ocean && adjacent.tileInfo->land)
							{
								LandLandmarkTileInfo &adjacentTileLandLandmarkTileInfo = landmark.tileInfos.at(adjacentTileIndex);
								
								double stepCost = (double)Rules->move_rate_roads;
								
								// update value
								
								double oldTravelTime = adjacentTileLandLandmarkTileInfo.travelTime;
								
								double newLandMovementCost = currentTileLandLandmarkTileInfo.landMovementCost + stepCost;
								double newTravelTime = currentTileLandLandmarkTileInfo.travelTime + stepCost / 2.0;
								
								if (newTravelTime < oldTravelTime)
								{
									adjacentTileLandLandmarkTileInfo.distance = currentDistance;
									adjacentTileLandLandmarkTileInfo.landMovementCost = newLandMovementCost;
									adjacentTileLandLandmarkTileInfo.travelTime = newTravelTime;
									newOpenNodes.insert(adjacentTileIndex);
									
									endNode = false;
									
								}
								
							}
							
						}
						
						if (endNode)
						{
							endNodes.insert(currentTileIndex);
						}
						
					}
					
					openNodes.clear();
					openNodes.swap(newOpenNodes);
					
					currentDistance++;
					
				}
				
				// select next landmark
				
				int fartherstEndNodeTileIndex = -1;
				int fartherstEndNodeDistance = 0;
				
				for (int otherEndNodeTileIndex : endNodes)
				{
					int nearestLandmarkDistance = INT_MAX;
					
					for (LandLandmark &otherLandLandmark : landmarks)
					{
						size_t otherLandLandmarkTileIndex = otherLandLandmark.tileIndex;
						int otherLandLandmarkCluster = landCombatClusters.at(otherLandLandmarkTileIndex);
						
						if (otherLandLandmarkCluster != landCluster)
							continue;
						
						int distance = otherLandLandmark.tileInfos.at(otherEndNodeTileIndex).distance;
						
						if (distance == INT_MAX)
							continue;
						
						if (distance < nearestLandmarkDistance)
						{
							nearestLandmarkDistance = distance;
						}
						
					}
					
					if (nearestLandmarkDistance > fartherstEndNodeDistance)
					{
						fartherstEndNodeTileIndex = otherEndNodeTileIndex;
						fartherstEndNodeDistance = nearestLandmarkDistance;
					}
					
				}
				
				if (fartherstEndNodeTileIndex == -1 || fartherstEndNodeDistance < minLandmarkDistance)
				{
					// exit cycle
					break;
				}
				
				// set new landmark
				
				landmarkTileIndex = fartherstEndNodeTileIndex;
				endNodes.erase(landmarkTileIndex);
				
			}
			
		}
			
	}
	
//	if (DEBUG)
//	{
//		for (size_t landMovementTypeIndex = 0; landMovementTypeIndex < LAND_MOVEMENT_TYPE_COUNT; landMovementTypeIndex++)
//		{
//			MovementType const movementType = SEA_MOVEMENT_TYPES.at(landMovementTypeIndex);
//			
//			debug("\tmovementType=%d\n", movementType);
//			
//			std::vector<LandLandmark> const &landmarks = landLandmarks.at(landMovementTypeIndex);
//			
//			for (LandLandmark const &landmark : landmarks)
//			{
//				debug("\t\t%s\n", getLocationString(landmark.tileIndex).c_str());
//				for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//				{
//					debug("\t\t\t%s %f %f %f\n", getLocationString(tileIndex).c_str(), landmark.tileInfos.at(tileIndex).seaTransportWaitTime, landmark.tileInfos.at(tileIndex).seaMovementCost, landmark.tileInfos.at(tileIndex).landMovementCost);
//				}
//			}
//			
//		}
//			
//	}
	
	Profiling::stop("populateLandLandmarks");
	
}

void populateSeaClusters(int factionId)
{
	Profiling::start("populateSeaClusters", "precomputeRouteData");
	
	debug("populateSeaClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<int> &seaClusters = aiFactionMovementInfo.seaClusters;
	std::map<int, int> &seaClusterAreas = aiFactionMovementInfo.seaClusterAreas;
	
	seaClusters.resize(*MapAreaTiles); std::fill(seaClusters.begin(), seaClusters.end(), -1);
	seaClusterAreas.clear();
	
	// availableTileIndexes
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		
		// sea and not blocked
		
		if (is_ocean(tile) && !isBlocked(tile))
		{
			seaClusters.at(tileIndex) = -2;
		}
		
	}
	
	// friendly ports
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		int baseTileIndex = baseTile - *MapTiles;
		bool ocean = is_ocean(baseTile);
		
		// friendly
		
		if (!isFriendly(factionId, base->faction_id))
			continue;
		
		// land
		
		if (ocean)
			continue;
		
		// accesses to water
		
		if (!isBaseAccessesWater(baseId))
			continue;
		
		// set
		
		seaClusters.at(baseTileIndex) = -2;
		
	}
	
	// process tiles
	
	std::vector<int> borderTiles;
	std::vector<int> newBorderTiles;
	
	int clusterIndex = 0;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		// available
		
		if (seaClusters.at(tileIndex) != -2)
			continue;
		
		// set collections
		
		borderTiles.clear();
		newBorderTiles.clear();
		
		// insert initial tile
		
		seaClusters.at(tileIndex) = clusterIndex;
		aiFactionMovementInfo.seaClusterCount = clusterIndex + 1;
		
		borderTiles.push_back(tileIndex);
		
		// expand cluster
		
		while (!borderTiles.empty())
		{
			for (int currentTileIndex : borderTiles)
			{
				TileInfo &currentTileInfo = aiData.tileInfos[currentTileIndex];
				
				for (Adjacent const &adjacent : currentTileInfo.adjacents)
				{
					int adjacentTileIndex = adjacent.tileInfo->index;
					
					// available
					
					if (seaClusters.at(adjacentTileIndex) != -2)
						continue;
					
					// insert adjacent tile
					
					seaClusters.at(adjacentTileIndex) = clusterIndex;
					
					newBorderTiles.push_back(adjacentTileIndex);
					
				}
				
			}
			
			// swap border tiles and clear them
			
			borderTiles.swap(newBorderTiles);
			newBorderTiles.clear();
			
		}
		
		// increment clusterIndex
		
		clusterIndex++;
			
	}
	
	// populate seaClusterAreas
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		int seaCluster = seaClusters.at(tileIndex);
		
		if (seaCluster >= 0)
		{
			seaClusterAreas[seaCluster]++;
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), seaClusters.at(tileIndex));
//		}
//		
//	}
	
	Profiling::stop("populateSeaClusters");
	
}

void populateLandClusters()
{
	Profiling::start("populateLandClusters", "precomputeRouteData");
	
	debug("populateLandClusters aiFactionId=%d\n", aiFactionId);
	
	std::vector<int> &landClusters = aiFactionMovementInfo.landClusters;
	std::map<int, int> &landClusterAreas = aiFactionMovementInfo.landClusterAreas;
	
	landClusters.resize(*MapAreaTiles); std::fill(landClusters.begin(), landClusters.end(), -1);
	landClusterAreas.clear();
	
	// availableTileIndexes
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		
		// land and not blocked
		
		if (!is_ocean(tile) && !isBlocked(tile))
		{
			landClusters.at(tileIndex) = -2;
		}
		
	}
	
	// process tiles
	
	std::vector<int> borderTiles;
	std::vector<int> newBorderTiles;
	
	int clusterIndex = aiFactionMovementInfo.seaClusterCount;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		// available
		
		if (landClusters.at(tileIndex) != -2)
			continue;
		
		// set collections
		
		borderTiles.clear();
		newBorderTiles.clear();
		
		// insert initial tile
		
		landClusters.at(tileIndex) = clusterIndex;
		aiFactionMovementInfo.landClusterCount = clusterIndex + 1;
		
		borderTiles.push_back(tileIndex);
		
		// expand cluster
		
		while (!borderTiles.empty())
		{
			for (int currentTileIndex : borderTiles)
			{
				TileInfo &currentTileInfo = aiData.tileInfos[currentTileIndex];
				
				for (Adjacent const &adjacent : currentTileInfo.adjacents)
				{
					int adjacentTileIndex = adjacent.tileInfo->index;
					
					// available
					
					if (landClusters.at(adjacentTileIndex) != -2)
						continue;
					
					// not zoc
					
					if (isZoc(currentTileIndex, adjacentTileIndex))
						continue;
					
					// insert adjacent tile
					
					landClusters.at(adjacentTileIndex) = clusterIndex;
					
					newBorderTiles.push_back(adjacentTileIndex);
					
				}
				
			}
			
			// swap border tiles and clear them
			
			borderTiles.swap(newBorderTiles);
			newBorderTiles.clear();
			
		}
		
		// increment clusterIndex
		
		clusterIndex++;
			
	}
	
	// populate landClusterAreas
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		int landCluster = landClusters.at(tileIndex);
		
		if (landCluster >= 0)
		{
			landClusterAreas[landCluster]++;
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), landClusters.at(tileIndex));
//		}
//		
//	}
	
	Profiling::stop("populateLandClusters");
	
}

void populateLandTransportedClusters()
{
	Profiling::start("populateLandTransportedClusters", "precomputeRouteData");
	
	debug("populateLandTransportedClusters aiFactionId=%d\n", aiFactionId);
	
	std::vector<double> const &seaTransportWaitTimes = factionMovementInfos.at(aiFactionId).seaTransportWaitTimes;
	std::vector<int> &landTransportedClusters = aiFactionMovementInfo.landTransportedClusters;
	
	landTransportedClusters.resize(*MapAreaTiles); std::fill(landTransportedClusters.begin(), landTransportedClusters.end(), -1);
	
	// availableTileIndexes
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		
		// land and not blocked
		
		if (!is_ocean(tile) && !isBlocked(tile))
		{
			landTransportedClusters.at(tileIndex) = -2;
		}
		
		// or sea with non infinite wait time
		
		if (seaTransportWaitTimes.at(tileIndex) != INF)
		{
			landTransportedClusters.at(tileIndex) = -2;
		}
		
	}
	
	// landTransportedClusters
	
	std::vector<int> borderTiles;
	std::vector<int> newBorderTiles;
	
	int clusterIndex = 0;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		// available
		
		if (landTransportedClusters.at(tileIndex) != -2)
			continue;
		
		// set collections
		
		borderTiles.clear();
		newBorderTiles.clear();
		
		// insert initial tile
		
		landTransportedClusters.at(tileIndex) = clusterIndex;
		
		borderTiles.push_back(tileIndex);
		
		// expand cluster
		
		while (!borderTiles.empty())
		{
			for (int currentTileIndex : borderTiles)
			{
				TileInfo &currentTileInfo = aiData.tileInfos[currentTileIndex];
				
				for (Adjacent const &adjacent : currentTileInfo.adjacents)
				{
					int adjacentTileIndex = adjacent.tileInfo->index;
					
					// available
					
					if (landTransportedClusters.at(adjacentTileIndex) != -2)
						continue;
					
					// not zoc on land
					
					if (isZoc(currentTileIndex, adjacentTileIndex))
						continue;
					
					// insert adjacent tile
					
					landTransportedClusters.at(adjacentTileIndex) = clusterIndex;
					
					newBorderTiles.push_back(adjacentTileIndex);
					
				}
				
			}
			
			// swap border tiles and clear them
			
			borderTiles.swap(newBorderTiles);
			newBorderTiles.clear();
			
		}
		
		// increment clusterIndex
		
		clusterIndex++;
			
	}
	
//	if (DEBUG)
//	{
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), landTransportedClusters.at(tileIndex));
//			
//		}
//		
//	}
	
	Profiling::stop("populateLandTransportedClusters");
	
}

void populateTransfers(int factionId)
{
	Profiling::start("populateTransfers", "precomputeRouteData");
	
	debug("populateTransfers aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<int> const &landClusters = aiFactionMovementInfo.landClusters;
	std::vector<int> const &seaClusters = aiFactionMovementInfo.seaClusters;
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<Transfer>>> &transfers = aiFactionMovementInfo.transfers;
	robin_hood::unordered_flat_map<MAP *, std::vector<Transfer>> &oceanBaseTransfers = aiFactionMovementInfo.oceanBaseTransfers;
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>> &connectedClusters = aiFactionMovementInfo.connectedClusters;
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>>> &firstConnectedClusters = aiFactionMovementInfo.firstConnectedClusters;
	
	Profiling::start("populateTransfers clear()", "populateTransfers");
	
	transfers.clear();
	oceanBaseTransfers.clear();
	connectedClusters.clear();
	firstConnectedClusters.clear();
	
	Profiling::stop("populateTransfers clear()");
	
	// land-sea transfers
	
	Profiling::start("land-sea transfers", "populateTransfers");
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tileIndex);
		int landClusterIndex = landClusters.at(tileIndex);
		
		// land
		
		if (!tileInfo.land)
			continue;
		
		// not blocked
		
		if (tileInfo.blocked)
			continue;
		
		// assigned cluster
		
		if (landClusterIndex == -1)
			continue;
		
		// iterate adjacent tiles
		
		for (Adjacent const &adjacent : tileInfo.adjacents)
		{
			int adjacentTileIndex = adjacent.tileInfo->index;
			MAP *adjacentTile = adjacent.tileInfo->tile;
			
			// sea
			
			if (!adjacent.tileInfo->ocean)
				continue;
			
			int seaClusterIndex = seaClusters.at(adjacentTileIndex);
			
			// not blocked
			
			if (adjacent.tileInfo->blocked)
				continue;
			
			// assigned cluster
			
			if (seaClusterIndex == -1)
				continue;
			
			// populate collections
			
			transfers[landClusterIndex][seaClusterIndex].emplace_back(tile, adjacentTile);
			connectedClusters[landClusterIndex].insert(seaClusterIndex);
			connectedClusters[seaClusterIndex].insert(landClusterIndex);
			
		}
		
	}
	
	Profiling::stop("land-sea transfers");
	
	// ocean base transfers
	
	Profiling::start("ocean base transfers", "populateTransfers");
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
		
		// faction base
		
		if (base->faction_id != factionId)
			continue;
		
		// ocean base
		
		if (!baseTileInfo.ocean)
			continue;
		
		// add base
		// there could be ocean base without any transfers
		
		oceanBaseTransfers.emplace(baseTile, std::vector<Transfer>());
		
		// process adjacent tiles
		
		for (Adjacent const &adjacent : baseTileInfo.adjacents)
		{
			MAP *adjacentTile = adjacent.tileInfo->tile;
			
			// ocean
			
			if (!adjacent.tileInfo->ocean)
				continue;
			
			// add transfer
			
			oceanBaseTransfers.at(baseTile).emplace_back(baseTile, adjacentTile);
			
		}
		
	}
	
	Profiling::stop("ocean base transfers");
	
	// connecting clusters
	
	Profiling::start("connecting clusters", "populateTransfers");
	
	struct RemoteConnection
	{
		int remoteCluster;
		int firstConnection;
	};
	
	std::vector<RemoteConnection> openNodes;
	std::vector<RemoteConnection> newOpenNodes;
	
	for (robin_hood::pair<int, robin_hood::unordered_flat_set<int>> connectedClusterEntry : connectedClusters)
	{
		int initialCluster = connectedClusterEntry.first;
		robin_hood::unordered_flat_set<int> &initialConnectedClusters = connectedClusterEntry.second;
		
		// initialize connections
		
		firstConnectedClusters.emplace(initialCluster, robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>>());
		robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>> &initialFirstConnectedClusters = firstConnectedClusters.at(initialCluster);
		
		// expand connections
		
		openNodes.clear();
		newOpenNodes.clear();
		
		// insert initial nodes
		
		for (int initialConnectedCluster : initialConnectedClusters)
		{
			initialFirstConnectedClusters[initialConnectedCluster].insert(initialConnectedCluster);
			openNodes.push_back({initialConnectedCluster, initialConnectedCluster});
		}
		
		while (!openNodes.empty())
		{
			for (RemoteConnection const &openNode : openNodes)
			{
				for (int cluster : connectedClusters.at(openNode.remoteCluster))
				{
					// not initial
					
					if (cluster == initialCluster)
						continue;
					
					// not yet added
					
					if (initialFirstConnectedClusters.find(cluster) != initialFirstConnectedClusters.end())
						continue;
					
					// add new open node
					
					newOpenNodes.push_back({cluster, openNode.firstConnection});
					
				}
				
			}
			
			// store connections
			
			for (RemoteConnection newOpenNode : newOpenNodes)
			{
				initialFirstConnectedClusters[newOpenNode.remoteCluster].insert(newOpenNode.firstConnection);
			}
			
			openNodes.clear();
			openNodes.swap(newOpenNodes);
			
		}
		
	}
	
	Profiling::stop("connecting clusters");
	
//	if (DEBUG)
//	{
//		debug("transfers\n");
//		
//		for (robin_hood::pair<int, robin_hood::unordered_flat_map<int, std::vector<Transfer>>> const &transferEntry : aiFactionMovementInfo.transfers)
//		{
//			int landCluster = transferEntry.first;
//			
//			for (robin_hood::pair<int, std::vector<Transfer>> const &landClusterTransferEntry : transferEntry.second)
//			{
//				int seaCluster = landClusterTransferEntry.first;
//				
//				debug("\t%d -> %d\n", landCluster, seaCluster);
//				
//				for (Transfer const &transfer : landClusterTransferEntry.second)
//				{
//					debug("\t\t%s -> %s\n", getLocationString(transfer.passengerStop).c_str(), getLocationString(transfer.transportStop).c_str());
//				}
//				
//			}
//			
//		}
//		
//		debug("oceanBaseTransfers\n");
//		
//		for (robin_hood::pair<MAP *, std::vector<Transfer>> const &oceanBaseTransferEntry : aiFactionMovementInfo.oceanBaseTransfers)
//		{
//			MAP *baseTile = oceanBaseTransferEntry.first;
//			
//			debug("\t%s\n", getLocationString(baseTile).c_str());
//			
//			for (Transfer const &transfer : oceanBaseTransferEntry.second)
//			{
//				debug("\t\t%s -> %s\n", getLocationString(transfer.passengerStop).c_str(), getLocationString(transfer.transportStop).c_str());
//			}
//			
//		}
//		
//		debug("connectedClusters\n");
//		
//		for (robin_hood::pair<int, robin_hood::unordered_flat_set<int>> const &connectedClusterEntry : aiFactionMovementInfo.connectedClusters)
//		{
//			int cluster1 = connectedClusterEntry.first;
//			
//			debug("\t%d ->\n", cluster1);
//			
//			for (int cluster2 : connectedClusterEntry.second)
//			{
//				debug("\t\t%d\n", cluster2);
//			}
//			
//		}
//		
//	}
//	
//	if (DEBUG)
//	{
//		debug("firstConnectedClusters\n");
//		
//		for (robin_hood::pair<int, robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>>> const &connectingClusterEntry : firstConnectedClusters)
//		{
//			int initialCluster = connectingClusterEntry.first;
//			robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>> const &initialConnectingClusters = connectingClusterEntry.second;
//			
//			for (robin_hood::pair<int, robin_hood::unordered_flat_set<int>> const &initialConnectingClusterEntry : initialConnectingClusters)
//			{
//				int terminalCluster = initialConnectingClusterEntry.first;
//				robin_hood::unordered_flat_set<int> const &initialTerminslConnectingClusters = initialConnectingClusterEntry.second;
//				
//				for (int firstCluster : initialTerminslConnectingClusters)
//				{
//					debug("\tinitial=%2d terminal=%2d first=%2d\n", initialCluster, terminalCluster, firstCluster);
//				}
//				
//			}
//			
//		}
//		
//	}
	
	Profiling::stop("populateTransfers");
	
}

void populateReachableLocations()
{
	Profiling::start("populateReachableLocations", "precomputeRouteData");
	
	debug("populateReachableLocations aiFactionId=%d\n", aiFactionId);
	
	std::vector<bool> &reachableTileIndexes = aiFactionMovementInfo.reachableTileIndexes;
	
	reachableTileIndexes.resize(*MapAreaTiles); std::fill(reachableTileIndexes.begin(), reachableTileIndexes.end(), false);
	
	// collect our vehicle clusters
	
	robin_hood::unordered_flat_set<int> reachableSeaClusters;
	robin_hood::unordered_flat_set<int> reachableLandTransportedClusters;
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();
		
		// ours
		
		if (vehicle->faction_id != aiFactionId)
			continue;
		
		switch (triad)
		{
		case TRIAD_SEA:
			{
				int seaCluster = getSeaCluster(vehicleTile);
				reachableSeaClusters.insert(seaCluster);
				
			}
			
			break;
			
		case TRIAD_LAND:
			{
				int landTransportedCluster = getLandTransportedCluster(vehicleTile);
				reachableLandTransportedClusters.insert(landTransportedCluster);
				
			}
			
			break;
			
		}
		
	}
	
	// popualte reachable tiles
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		if (tileInfo.ocean)
		{
			int tileSeaCluster = getSeaCluster(tile);
			if (reachableSeaClusters.find(tileSeaCluster) != reachableSeaClusters.end())
			{
				reachableTileIndexes.at(tileIndex) = true;
			}
			
		}
		else
		{
			int tileLandTransportedCluster = getLandTransportedCluster(tile);
			if (reachableLandTransportedClusters.find(tileLandTransportedCluster) != reachableLandTransportedClusters.end())
			{
				reachableTileIndexes.at(tileIndex) = true;
			}
			
		}
		
	}
	
	Profiling::stop("populateReachableLocations");
	
}

void populateSharedSeas()
{
	debug("populateSharedSeas aiFactionId=%d\n", aiFactionId);
	
	std::vector<bool> &sharedSeas = aiFactionMovementInfo.sharedSeas;
	sharedSeas.resize(*MapAreaTiles);
	std::fill(sharedSeas.begin(), sharedSeas.end(), false);
	
	std::vector<MAP *> openNodes;
	std::vector<MAP *> newOpenNodes;
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// not player's
		
		if (base->faction_id == aiFactionId)
			continue;
		
		// mark and add to closed node
		
		if (is_ocean(baseTile))
		{
			sharedSeas.at(baseTile - *MapTiles) = true;
		}
		openNodes.push_back(baseTile);
		
	}
	
	// extend territory
	
	while (!openNodes.empty())
	{
		for (MAP *tile : openNodes)
		{
			for (MAP *adjacentTile : getAdjacentTiles(tile))
			{
				// sea or friendly base
				
				if (!(is_ocean(adjacentTile) || isFriendlyBaseAt(adjacentTile, aiFactionId)))
					continue;
				
				if (sharedSeas.at(adjacentTile - *MapTiles) == false)
				{
					sharedSeas.at(adjacentTile - *MapTiles) = true;
					newOpenNodes.push_back(adjacentTile);
				}
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
}

double getVehicleAttackApproachTime(int vehicleId, MAP *dst)
{
	VEH *vehicle = &Vehicles[vehicleId];
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return getUnitApproachTime(vehicle->faction_id, vehicle->unit_id, vehicleTile, dst, false);
}
double getVehicleAttackApproachTime(int vehicleId, MAP *org, MAP *dst)
{
	VEH *vehicle = &Vehicles[vehicleId];
	return getUnitApproachTime(vehicle->faction_id, vehicle->unit_id, org, dst, false);
}
double getVehicleApproachTime(int vehicleId, MAP *dst, bool includeDestination)
{
	VEH *vehicle = &Vehicles[vehicleId];
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	return getUnitApproachTime(vehicle->faction_id, vehicle->unit_id, vehicleTile, dst, includeDestination);
}
double getVehicleApproachTime(int vehicleId, MAP *org, MAP *dst, bool includeDestination)
{
	VEH *vehicle = &Vehicles[vehicleId];
	return getUnitApproachTime(vehicle->faction_id, vehicle->unit_id, org, dst, includeDestination);
}
double getUnitApproachTime(int factionId, int unitId, MAP *org, MAP *dst, bool includeDestination)
{
	Profiling::start("- getUnitApproachTime");
	
	UNIT *unit = &Units[unitId];
	int speed = getUnitSpeed(factionId, unitId);
	MovementType movementType = getUnitBasicMovementType(factionId, unitId);
	
	// check cached values
	
	double cachedUnitApproachTime = getCachedUnitApproachTime(factionId, movementType, org, dst);
	if (cachedUnitApproachTime >= 0)
	{
		Profiling::stop("- getUnitApproachTime");
		return cachedUnitApproachTime;
	}
	
	// find approach time
	
	double approachTime = INF;
	
	switch (unit->chassis_id)
	{
	case CHS_GRAVSHIP:
		approachTime = getGravshipTravelTime(speed, org, dst, includeDestination);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		approachTime = getRangedAirTravelTime(factionId, unit->chassis_id, speed, org, dst, includeDestination);
		break;
	case CHS_CRUISER:
	case CHS_FOIL:
		approachTime = getSeaLApproachTime(factionId, movementType, speed, org, dst, includeDestination);
		break;
	case CHS_HOVERTANK:
	case CHS_SPEEDER:
	case CHS_INFANTRY:
		approachTime = getLandLApproachTime(factionId, movementType, speed, org, dst, includeDestination);
		break;
	}
	
	// store cached values
	
	setCachedUnitApproachTime(factionId, movementType, org, dst, approachTime);
	
	Profiling::stop("- getUnitApproachTime");
	return approachTime;
	
}

double getGravshipTravelTime(int speed, MAP *org, MAP *dst, bool includeDestination)
{
	assert(speed > 0);
	
	// approach time
	
	int range = getRange(org, dst);
	
	// exclude destination if requested
	
	if (!includeDestination)
	{
		range--;
	}
	
	return (double)range / (double)speed;
	
}

double getRangedAirTravelTime(int factionId, int chassisId, int speed, MAP *org, MAP *dst, bool includeDestination)
{
	assert(chassisId == CHS_NEEDLEJET || chassisId == CHS_COPTER || chassisId == CHS_MISSILE);
	assert(speed > 0);
	
	std::vector<int> const &airClusters = factionMovementInfos.at(factionId).airClusters.at(chassisId).at(speed);
	
	// air clusters
	
	int orgAirCluster = airClusters.at(org - *MapTiles);
	int dstAirCluster = airClusters.at(dst - *MapTiles);
	if (orgAirCluster == -1 || dstAirCluster == -1 || dstAirCluster != orgAirCluster)
		return INF;
	
	// approach time
	
	int range = getRange(org, dst);
	
	// exclude destination if requested
	
	if (!includeDestination)
	{
		range--;
	}
	
	return RANGED_AIR_TRAVEL_TIME_COEFFICIENT * (double)range / (double)speed;
	
}

double getSeaLApproachTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst, bool includeDestination)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	assert(movementType >= SEA_MOVEMENT_TYPE_FIRST && movementType <= SEA_MOVEMENT_TYPE_LAST);
	assert(speed > 0);
	assert(isOnMap(org));
	assert(isOnMap(dst));
	
	FactionMovementInfo const &factionMovementInfo = factionMovementInfos.at(factionId);
	std::vector<int> const &combatClusters = factionMovementInfo.seaCombatClusters;
	
	int seaMovementTypeIndex = movementType - SEA_MOVEMENT_TYPE_FIRST;
	
	int orgTileIndex = org - *MapTiles;
	int dstTileIndex = dst - *MapTiles;
	
	int orgCombatCluster = combatClusters.at(orgTileIndex);
	int dstCombatCluster = combatClusters.at(dstTileIndex);
	
	if (orgCombatCluster == -1 || dstCombatCluster == -1 || orgCombatCluster != dstCombatCluster)
		return INF;
	
	int landmarkCount = 0;
	double maxLandmarkMovementCost = 0.0;
	
	for (SeaLandmark const &landmark : factionMovementInfo.seaLandmarks.at(seaMovementTypeIndex))
	{
		int landmarkTileIndex = landmark.tileIndex;
		int landmarkCombatCluster = combatClusters.at(landmarkTileIndex);
		
		// reachable landmark
		
		if (landmarkCombatCluster == -1 || landmarkCombatCluster != orgCombatCluster)
			continue;
		
		// landmarkApproachTime
		
		double landmarkMovementCost = abs(landmark.tileInfos.at(dstTileIndex).movementCost - landmark.tileInfos.at(orgTileIndex).movementCost);
		
		// update max
		
		landmarkCount++;
		maxLandmarkMovementCost = std::max(maxLandmarkMovementCost, landmarkMovementCost);
		
	}
	
	if (landmarkCount == 0)
	{
		debug("ERROR: no suitable landmark.\n");
		return INF;
	}
	
	// exclude destination if requested
	
	if (!includeDestination)
	{
		maxLandmarkMovementCost -= (Rules->move_rate_roads + factionMovementInfo.impediments.at(dstTileIndex));
	}
	
	return maxLandmarkMovementCost / (double)(Rules->move_rate_roads * speed);
	
}

double getLandLApproachTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst, bool includeDestination)
{
	Profiling::start("- getLandLApproachTime");
	
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	assert(movementType >= BASIC_LAND_MOVEMENT_TYPE_FIRST && movementType <= BASIC_LAND_MOVEMENT_TYPE_LAST);
	assert(speed > 0);
	assert(isOnMap(org));
	assert(isOnMap(dst));
	
	FactionInfo const &factionInfo = aiData.factionInfos.at(factionId);
	FactionMovementInfo const &factionMovementInfo = factionMovementInfos.at(factionId);
	std::vector<int> const &combatClusters = factionMovementInfo.landCombatClusters;
	
	int landMovementTypeIndex = movementType - LAND_MOVEMENT_TYPE_FIRST;
	
	int orgTileIndex = org - *MapTiles;
	int dstTileIndex = dst - *MapTiles;
	
	int orgCombatCluster = combatClusters.at(orgTileIndex);
	int dstCombatCluster = combatClusters.at(dstTileIndex);
	
	if (orgCombatCluster == -1 || dstCombatCluster == -1 || orgCombatCluster != dstCombatCluster)
	{
		Profiling::stop("- getLandLApproachTime");
		return INF;
	}
	
	int landmarkCount = 0;
	double maxLandmarkTravelTime = 0.0;
	
	for (LandLandmark const &landmark : factionMovementInfo.landLandmarks.at(landMovementTypeIndex))
	{
		int landmarkTileIndex = landmark.tileIndex;
		int landmarkCombatCluster = combatClusters.at(landmarkTileIndex);
		
		// reachable landmark
		
		if (landmarkCombatCluster == -1 || landmarkCombatCluster != orgCombatCluster)
			continue;
		
		LandLandmarkTileInfo const &orgLandLandmarkTileInfo = landmark.tileInfos.at(orgTileIndex);
		LandLandmarkTileInfo const &dstLandLandmarkTileInfo = landmark.tileInfos.at(dstTileIndex);
		
		// landmarkApproachTime
		
		double orgLandmarkTravelTime;
		double dstLandmarkTravelTime;
		if (factionInfo.bestSeaTransportUnitSpeed <= 0)
		{
			if (orgLandLandmarkTileInfo.seaMovementCost > 0.0 || dstLandLandmarkTileInfo.seaMovementCost > 0.0)
			{
				debug("ERROR: bestSeaTransportUnitSpeed <= 0 AND seaMovementCost > 0.0.\n");
				continue;
			}
			
			orgLandmarkTravelTime =
				+ orgLandLandmarkTileInfo.seaTransportWaitTime
				+ orgLandLandmarkTileInfo.landMovementCost / (double)(Rules->move_rate_roads * speed)
			;
			dstLandmarkTravelTime =
				+ dstLandLandmarkTileInfo.seaTransportWaitTime
				+ dstLandLandmarkTileInfo.landMovementCost / (double)(Rules->move_rate_roads * speed)
			;
			
		}
		else
		{
			orgLandmarkTravelTime =
				+ orgLandLandmarkTileInfo.seaTransportWaitTime
				+ orgLandLandmarkTileInfo.seaMovementCost / (double)(Rules->move_rate_roads * factionInfo.bestSeaTransportUnitSpeed)
				+ orgLandLandmarkTileInfo.landMovementCost / (double)(Rules->move_rate_roads * speed)
			;
			dstLandmarkTravelTime =
				+ dstLandLandmarkTileInfo.seaTransportWaitTime
				+ dstLandLandmarkTileInfo.seaMovementCost / (double)(Rules->move_rate_roads * factionInfo.bestSeaTransportUnitSpeed)
				+ dstLandLandmarkTileInfo.landMovementCost / (double)(Rules->move_rate_roads * speed)
			;
			
		}
		
		double landmarkTravelTime = abs(dstLandmarkTravelTime - orgLandmarkTravelTime);
		
		// update max
		
		landmarkCount++;
		maxLandmarkTravelTime = std::max(maxLandmarkTravelTime, landmarkTravelTime);
		
	}
	
	if (landmarkCount == 0)
	{
		debug("ERROR: no suitable landmark.\n");
		return INF;
	}
	
	// exclude destination if requested
	
	if (!includeDestination)
	{
		maxLandmarkTravelTime -= (Rules->move_rate_roads + factionMovementInfo.impediments.at(dstTileIndex)) / (double)(Rules->move_rate_roads * speed);
	}
	
	Profiling::stop("- getLandLApproachTime");
	return maxLandmarkTravelTime;
	
}

double getLandLMovementCost(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst, bool includeDestination)
{
	Profiling::start("- getLandLMovementCost");
	
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	assert(movementType >= BASIC_LAND_MOVEMENT_TYPE_FIRST && movementType <= BASIC_LAND_MOVEMENT_TYPE_LAST);
	assert(speed > 0);
	assert(isOnMap(org));
	assert(isOnMap(dst));
	
	FactionInfo const &factionInfo = aiData.factionInfos.at(factionId);
	FactionMovementInfo const &factionMovementInfo = factionMovementInfos.at(factionId);
	std::vector<int> const &combatClusters = factionMovementInfo.landCombatClusters;
	
	int landMovementTypeIndex = movementType - LAND_MOVEMENT_TYPE_FIRST;
	
	int orgTileIndex = org - *MapTiles;
	int dstTileIndex = dst - *MapTiles;
	
	// same land combat cluster
	
	if (!isSameLandCombatCluster(org, dst))
	{
		Profiling::stop("- getLandLMovementCost");
		return INF;
	}
	
	int landmarkCount = 0;
	double maxLandmarkLandMovementCost = 0.0;
	
	for (LandLandmark const &landmark : factionMovementInfo.landLandmarks.at(landMovementTypeIndex))
	{
		int landmarkTileIndex = landmark.tileIndex;
		MAP *landmarkTile = *MapTiles + landmarkTileIndex;
		
		// same combat cluster
		
		if (!isSameLandCombatCluster(org, landmarkTile))
			continue;
		
		LandLandmarkTileInfo const &orgLandLandmarkTileInfo = landmark.tileInfos.at(orgTileIndex);
		LandLandmarkTileInfo const &dstLandLandmarkTileInfo = landmark.tileInfos.at(dstTileIndex);
		
		// landmarkLandMovementCost
		
		double orgLandmarkLandMovementCost = orgLandLandmarkTileInfo.landMovementCost;
		double dstLandmarkLandMovementCost = dstLandLandmarkTileInfo.landMovementCost;
		
		double landmarkLandMovementCost = abs(dstLandmarkLandMovementCost - orgLandmarkLandMovementCost);
		
		// update max
		
		landmarkCount++;
		maxLandmarkLandMovementCost = std::max(maxLandmarkLandMovementCost, landmarkLandMovementCost);
		
	}
	
	if (landmarkCount == 0)
	{
		debug("ERROR: no suitable landmark.\n");
		Profiling::stop("- getLandLMovementCost");
		return INF;
	}
	
	// exclude destination if requested
	
	if (!includeDestination)
	{
		maxLandmarkLandMovementCost -= (Rules->move_rate_roads + factionMovementInfo.impediments.at(dstTileIndex));
	}
	
	Profiling::stop("- getLandLMovementCost");
	return maxLandmarkLandMovementCost;
	
}

/*
Uses approach time algorithm but makes sure destination is reachable.
*/
double getVehicleTravelTime(int vehicleId, MAP *dst)
{
	VEH *vehicle = &Vehicles[vehicleId];
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	if (!isVehicleDestinationReachable(vehicleId, vehicleTile, dst))
		return INF;
	
	return getUnitApproachTime(vehicle->faction_id, vehicle->unit_id, vehicleTile, dst, true);
	
}
double getVehicleTravelTime(int vehicleId, MAP *org, MAP *dst)
{
	VEH *vehicle = &Vehicles[vehicleId];
	
	if (!isVehicleDestinationReachable(vehicleId, org, dst))
		return INF;
	
	return getUnitApproachTime(vehicle->faction_id, vehicle->unit_id, org, dst, true);
	
}
double getUnitTravelTime(int factionId, int unitId, MAP *org, MAP *dst)
{
	if (!isUnitDestinationReachable(unitId, org, dst))
		return INF;
	
	return getUnitApproachTime(factionId, unitId, org, dst, true);
	
}

//// ==================================================
//// A* cached movement costs
//// ==================================================
//
//double getCachedATravelTime(MovementType movementType, int speed, int originIndex, int dstIndex)
//{
//	Profiling::start("- getATravelTime - cachedATravelTime::get");
//	
//	assert(speed < 256);
//	
//	long long key =
//		+ (long long)movementType	* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles) * 256
//		+ (long long)speed			* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles)
//		+ (long long)originIndex		* (long long)(*MapAreaTiles)
//		+ (long long)dstIndex
//	;
//	
//	robin_hood::unordered_flat_map <long long, double>::iterator cachedATravelTimeIterator = cachedATravelTimes.find(key);
//	
//	if (cachedATravelTimeIterator == cachedATravelTimes.end())
//		return INF;
//	
//	Profiling::stop("- getATravelTime - cachedATravelTime::get");
//	return cachedATravelTimeIterator->second;
//	
//}
//
//void setCachedATravelTime(MovementType movementType, int speed, int originIndex, int dstIndex, double travelTime)
//{
//	Profiling::start("- getATravelTime - cachedATravelTime::get");
//	
//	assert(speed < 256);
//	
//	long long key =
//		+ (long long)movementType	* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles) * 256
//		+ (long long)speed			* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles)
//		+ (long long)originIndex		* (long long)(*MapAreaTiles)
//		+ (long long)dstIndex
//	;
//	
//	cachedATravelTimes[key] = travelTime;
//	
//	Profiling::stop("- getATravelTime - cachedATravelTime::get");
//	
//}
//
//// ==================================================
//// A* path finding algorithm
//// ==================================================
//
///**
//Computes A* travel time for *player* faction.
//*/
//double getATravelTime(MovementType movementType, int speed, MAP *origin, MAP *dst)
//{
////	debug("getATravelTime movementType=%d speed=%d %s->%s\n", movementType, speed, getLocationString(origin).c_str(), getLocationString(dst).c_str());
//	
//	Profiling::start("- getATravelTime");
//	
//	int originIndex = origin - *MapTiles;
//	int dstIndex = dst - *MapTiles;
//	
//	// at dst
//	
//	if (origin == dst)
//	{
//		Profiling::stop("- getATravelTime");
//		return 0.0;
//	}
//	
//	// origin cluster
//	
//	int originCluster = -1;
//	
//	switch (movementType)
//	{
//	case MT_AIR:
//		
//		// no cluster restriction
//		
//		break;
//		
//	case MT_SEA_REGULAR:
//	case MT_SEA_NATIVE:
//		
//		originCluster = getSeaCluster(origin);
//		
//		break;
//		
//	case MT_LAND_REGULAR:
//	case MT_LAND_NATIVE:
//		
//		originCluster = getLandTransportedCluster(origin);
//		
//		break;
//		
//	}
//	
//	// check cached value
//	
//	double cachedTravelTime = getCachedATravelTime(movementType, speed, originIndex, dstIndex);
//	
//	if (cachedTravelTime != INF)
//	{
//		Profiling::stop("- getATravelTime");
//		return cachedTravelTime;
//	}
//	
//	// initialize collections
//	
//	std::priority_queue<FValue, std::vector<FValue>, FValueComparator> openNodes;
//	
//	setCachedATravelTime(movementType, speed, originIndex, originIndex, 0.0);
//	double originF = getRouteVectorDistance(origin, dst);
//	openNodes.push({originIndex, originF});
//	
//	// process path
//	
//	while (!openNodes.empty())
//	{
//		// get top node and remove it from list
//
//		FValue currentOpenNode = openNodes.top();
//		openNodes.pop();
//		
//		// get current tile data
//		
//		int currentTileIndex = currentOpenNode.tileIndex;
//		TileInfo &currentTileInfo = aiData.getTileInfo(currentTileIndex);
//		
//		double currentTileTravelTime = getCachedATravelTime(movementType, speed, originIndex, currentTileIndex);
//		
//		// check surrounding tiles
//		
//		for (Adjacent const &adjacent : currentTileInfo.adjacents)
//		{
//			int angle = adjacent.angle;
//			int adjacentTileIndex = adjacent.tileInfo->index;
//			MAP *adjacentTile = adjacent.tileInfo->tile;
//			
//			// within cluster for surface vehicle
//			
//			switch (movementType)
//			{
//			case MT_AIR:
//				
//				// no cluster restriction
//				
//				break;
//				
//			case MT_SEA_REGULAR:
//			case MT_SEA_NATIVE:
//				{
//					if (getSeaCluster(adjacentTile) != originCluster)
//						continue;
//				}
//				
//				break;
//				
//			case MT_LAND_REGULAR:
//			case MT_LAND_NATIVE:
//				{
//					if (getLandTransportedCluster(adjacentTile) != originCluster)
//						continue;
//				}
//				
//				break;
//				
//			}
//			
//			// stepTime and movement restrictions by movementType
//			
//			double stepTime = INF;
//			
//			switch (movementType)
//			{
//			case MT_AIR:
//			case MT_SEA_REGULAR:
//			case MT_SEA_NATIVE:
//				{
//					// hexCost
//					
//					int hexCost = currentTileInfo.hexCosts[movementType][angle];
//					
//					// sensible hexCost
//					
//					if (hexCost == -1)
//						continue;
//					
//					// stepTime
//					
//					stepTime = (double)hexCost / (double)(Rules->move_rate_roads * speed);
//					
//				}
//				
//				break;
//				
//			case MT_LAND_REGULAR:
//			case MT_LAND_NATIVE:
//				
//				// movement restrictions by surface type
//				
//				if (!currentTileInfo.ocean && !adjacent.tileInfo->ocean) // land-land
//				{
//					// not zoc (could be within the cluster but specific movement can still be restricted by zoc)
//					
//					if (isZoc(currentTileIndex) && isZoc(adjacentTileIndex))
//						continue;
//					
//					// hexCost
//					
//					int hexCost = currentTileInfo.hexCosts[movementType][angle];
//					
//					// sensible hexCost
//					
//					if (hexCost == -1)
//						continue;
//					
//					// stepTime
//					
//					stepTime = (double)hexCost / (double)(Rules->move_rate_roads * speed);
//					
//				}
//				else if (currentTileInfo.ocean && adjacent.tileInfo->ocean) // sea-sea transport movement
//				{
//					// hexCost for transport movement
//					
//					int hexCost = currentTileInfo.hexCosts[MT_SEA_REGULAR][angle];
//					
//					// sensible hexCost
//					
//					if (hexCost == -1)
//						continue;
//					
//					// stepTime
//					
//					stepTime = (double)hexCost / (double)(Rules->move_rate_roads * aiFactionInfo->bestSeaTransportUnitSpeed);
//					
//				}
//				else if (!currentTileInfo.ocean && adjacent.tileInfo->ocean) // boarding
//				{
//					// not blocked
//					
//					if (isBlocked(adjacentTileIndex))
//						continue;
//					
//					// transport is available
//					
//					double seaTransportWaitTime = factionMovementInfos.at(aiFactionId).seaTransportWaitTimes.at(adjacentTileIndex);
//					
//					if (seaTransportWaitTime == INF)
//						continue;
//					
//					// stepTime
//					
//					stepTime = seaTransportWaitTime;
//					
//				}
//				else if (currentTileInfo.ocean && !adjacent.tileInfo->ocean) // unboarding
//				{
//					// not blocked
//					
//					if (isBlocked(adjacentTileIndex))
//						continue;
//					
//					// stepTime
//					
//					stepTime = (double)Rules->move_rate_roads / (double)(Rules->move_rate_roads * speed);
//					
//				}
//				
//				break;
//				
//			}
//			
//			// travelTime
//			
//			double travelTime = currentTileTravelTime + stepTime;
//			
//			// get cached value or Infinity
//			
//			double cachedAdjacentTileTravelTime = getCachedATravelTime(movementType, speed, originIndex, adjacentTileIndex);
//			
//			// update adjacentTile cachedTravelTime
//			
//			if (cachedAdjacentTileTravelTime == INF || travelTime < cachedAdjacentTileTravelTime)
//			{
//				setCachedATravelTime(movementType, speed, originIndex, adjacentTileIndex, travelTime);
//				
//				// f value
//				// take cashed movement cost if it is known, otherwise estimate
//				
//				double adjacentTileDestinationTravelTime = getCachedATravelTime(movementType, speed, adjacentTileIndex, dstIndex);
//				
//				if (adjacentTileDestinationTravelTime == INF)
//				{
//					adjacentTileDestinationTravelTime = getRouteVectorDistance(adjacentTile, dst);
//				}
//				
//				double adjacentTileF = travelTime + getRouteVectorDistance(adjacentTile, dst);
//				
//				openNodes.push({adjacentTileIndex, adjacentTileF});
//				
//			}
//			
//		}
//		
//	}
//	
//	// retrieve cache value that should be already there
//	
//	double travelTime = getCachedATravelTime(movementType, speed, originIndex, dstIndex);
//	
//	Profiling::stop("- getATravelTime");
//	return travelTime;
//	
//}
//
//double getUnitATravelTime(int factionId, int unitId, int speed, MAP *org, MAP *dst)
//{
//	UNIT *unit = getUnit(unitId);
//	int chassisId = unit->chassis_id;
//	
//	if (speed <= 0)
//		return INF;
//	
//	double travelTime = INF;
//	
//	switch (chassisId)
//	{
//	case CHS_GRAVSHIP:
//		travelTime = getGravshipTravelTime(speed, org, dst);
//		break;
//	case CHS_NEEDLEJET:
//	case CHS_COPTER:
//	case CHS_MISSILE:
//		travelTime = getRangedAirTravelTime(factionId, chassisId, speed, org, dst);
//		break;
//	case CHS_FOIL:
//	case CHS_CRUISER:
//		travelTime = getSeaUnitATravelTime(unitId, speed, org, dst);
//		break;
//	case CHS_INFANTRY:
//	case CHS_SPEEDER:
//	case CHS_HOVERTANK:
//		travelTime = getLandUnitATravelTime(unitId, speed, org, dst);
//		break;
//	}
//	
//	return travelTime;
//	
//}
//
//double getUnitATravelTime(int factionId, int unitId, MAP *origin, MAP *dst)
//{
//	int speed = getUnitSpeed(aiFactionId, unitId);
//	
//	return getUnitATravelTime(factionId, unitId, speed, origin, dst);
//	
//}
//
//double getSeaUnitATravelTime(int unitId, int speed, MAP *origin, MAP *dst)
//{
//	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
//	
//	// possible travel
//	
//	if (!isSameSeaCluster(origin, dst))
//		return INF;
//	
//	// travelTime
//	
//	return getATravelTime(movementType, speed, origin, dst);
//	
//}
//
//double getLandUnitATravelTime(int unitId, int speed, MAP *origin, MAP *dst)
//{
//	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
//	
//	// possible travel within land transported cluster
//	
//	if (!isSameLandTransportedCluster(origin, dst))
//		return INF;
//	
//	// travelTime
//	
//	return getATravelTime(movementType, speed, origin, dst);
//	
//}
//
//double getVehicleATravelTime(int vehicleId, MAP *origin, MAP *dst)
//{
//	VEH *vehicle = getVehicle(vehicleId);
//	int unitId = vehicle->unit_id;
//	int speed = getVehicleSpeed(vehicleId);
//	
//	return getUnitATravelTime(unitId, speed, origin, dst);
//	
//}
//
//double getVehicleATravelTime(int vehicleId, MAP *dst)
//{
//	MAP *vehicleTile = getVehicleMapTile(vehicleId);
//	
//	return getVehicleATravelTime(vehicleId, vehicleTile, dst);
//	
//}
//
// ============================================================
// Reachability
// ============================================================

bool isUnitDestinationReachable(int unitId, MAP *origin, MAP *dst)
{
	UNIT *unit = getUnit(unitId);
	int chassisId = unit->chassis_id;
	int speed = getUnitSpeed(aiFactionId, unitId);
	
	if (speed <= 0)
		return false;
	
	bool reachable = false;
	
	switch (chassisId)
	{
	case CHS_GRAVSHIP:
		reachable = true;
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		reachable = isSameAirCluster(chassisId, speed, origin, dst);
		break;
	case CHS_FOIL:
	case CHS_CRUISER:
		reachable = isSameSeaCluster(origin, dst);
		break;
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		reachable = isSameLandTransportedCluster(origin, dst);
		break;
	}
	
	return reachable;
	
}

bool isVehicleDestinationReachable(int vehicleId, MAP *origin, MAP *dst)
{
	VEH *vehicle = getVehicle(vehicleId);
	int unitId = vehicle->unit_id;
	
	return isUnitDestinationReachable(unitId, origin, dst);
	
}

bool isVehicleDestinationReachable(int vehicleId, MAP *dst)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	return isVehicleDestinationReachable(vehicleId, vehicleTile, dst);
	
}

// ============================================================
// Helper functions
// ============================================================

/**
Computes vector distance with movement cost granularity for path computation.
*/
double getRouteVectorDistance(MAP *tile1, MAP *tile2)
{
	assert(isOnMap(tile1));
	assert(isOnMap(tile2));
	
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
	
	return (double)(std::max(dx, dy) - ((((dx + dy) / 2) - std::min(dx, dy) + 1) / 2));
	
}

/**
Returns vehicle movement type.
Ignoring hovering unit ability.
*/
MovementType getUnitBasicMovementType(int factionId, int unitId)
{
	bool xenoempatyDome = isFactionHasProject(factionId, FAC_XENOEMPATHY_DOME);
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	
	MovementType movementType = MT_AIR;
	
	switch (triad)
	{
	case TRIAD_AIR:
		{
			movementType = MT_AIR;
		}
		break;
		
	case TRIAD_SEA:
		{
			bool native = xenoempatyDome || isNativeUnit(unitId);
			movementType = native ? MT_SEA_NATIVE : MT_SEA;
		}
		break;
		
	case TRIAD_LAND:
		{
			bool native = xenoempatyDome || isNativeUnit(unitId);
			movementType = native ? MT_LAND_NATIVE : MT_LAND;
		}
		break;
		
	}
	
	return movementType;
	
}

/**
Returns vehicle movement type.
*/
MovementType getUnitMovementType(int factionId, int unitId)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	
	MovementType movementType = getUnitBasicMovementType(factionId, unitId);
	
	if (triad == TRIAD_LAND && (unit->chassis_id == CHS_HOVERTANK || has_abil(unitId, ABL_ANTIGRAV_STRUTS)))
	{
		switch (movementType)
		{
		case MT_LAND:
			movementType = MT_LAND_HOVER;
			break;
		case MT_LAND_NATIVE:
			movementType = MT_LAND_NATIVE_HOVER;
			break;
		default:
			break;
		}
	}
	
	return movementType;
	
}

MovementType getVehicleBasicMovementType(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	return getUnitBasicMovementType(vehicle->faction_id, vehicle->unit_id);
}

MovementType getVehicleMovementType(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	return getUnitMovementType(vehicle->faction_id, vehicle->unit_id);
}

int getAirCombatCluster(int chassisId, int speed, MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return factionMovementInfos.at(aiFactionId).airClusters.at(chassisId).at(speed).at(tileIndex);
	
}

int getVehicleAirCluster(int vehicleId)
{
	return getAirCombatCluster(getVehicle(vehicleId)->chassis_type(), getVehicleSpeed(vehicleId), getVehicleMapTile(vehicleId));
}

bool isSameAirCluster(int chassisId, int speed, MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1AirCluster = getAirCombatCluster(chassisId, speed, tile1);
	int tile2AirCluster = getAirCombatCluster(chassisId, speed, tile2);
	
	return tile1AirCluster != -1 && tile2AirCluster != -1 && tile1AirCluster == tile2AirCluster;
	
}

bool isVehicleSameAirCluster(int vehicleId, MAP *dst)
{
	return isSameAirCluster(getVehicle(vehicleId)->chassis_type(), getVehicleSpeed(vehicleId), getVehicleMapTile(vehicleId), dst);
}

bool isMeleeAttackableFromAirCluster(int chassisId, int speed, MAP *origin, MAP *target)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(target >= *MapTiles && target < *MapTiles + *MapAreaTiles);
	
	int originAirCluster = getAirCombatCluster(chassisId, speed, origin);
	
	for (MAP *adjacentTile : getAdjacentTiles(target))
	{
		if (getAirCombatCluster(chassisId, speed, adjacentTile) == originAirCluster)
			return true;
	}
	
	return false;
	
}

bool isVehicleMeleeAttackableFromAirCluster(int vehicleId, MAP *target)
{
	return isMeleeAttackableFromAirCluster(getVehicle(vehicleId)->chassis_type(), getVehicleSpeed(vehicleId), getVehicleMapTile(vehicleId), target);
}

int getSeaCluster(MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return aiFactionMovementInfo.seaClusters.at(tileIndex);
	
}

int getVehicleSeaCluster(int vehicleId)
{
	return(getSeaCluster(getVehicleMapTile(vehicleId)));
}

bool isSameSeaCluster(MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1SeaCluster = getSeaCluster(tile1);
	int tile2SeaCluster = getSeaCluster(tile2);
	
	return tile1SeaCluster != -1 && tile2SeaCluster != -1 && tile1SeaCluster == tile2SeaCluster;
	
}

bool isVehicleSameSeaCluster(int vehicleId, MAP *dst)
{
	return(isSameSeaCluster(getVehicleMapTile(vehicleId), dst));
}

bool isMeleeAttackableFromSeaCluster(MAP *origin, MAP *target)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(target >= *MapTiles && target < *MapTiles + *MapAreaTiles);
	
	int originSeaCluster = getSeaCluster(origin);
	
	for (MAP *adjacentTile : getAdjacentTiles(target))
	{
		if (getSeaCluster(adjacentTile) == originSeaCluster)
			return true;
	}
	
	return false;
	
}

bool isVehicleMeleeAttackableFromSeaCluster(int vehicleId, MAP *target)
{
	return isMeleeAttackableFromSeaCluster(getVehicleMapTile(vehicleId), target);
}

bool isArtilleryAttackableFromSeaCluster(MAP *origin, MAP *target)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(target >= *MapTiles && target < *MapTiles + *MapAreaTiles);
	
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	int originSeaCluster = getSeaCluster(origin);
	
	for (TileInfo const *rangeTileInfo : targetTileInfo.range2NoCenterTileInfos)
	{
		if (getSeaCluster(rangeTileInfo->tile) == originSeaCluster)
			return true;
	}
	
	return false;
	
}

bool isVehicleArtilleryAttackableFromSeaCluster(int vehicleId, MAP *target)
{
	return isArtilleryAttackableFromSeaCluster(getVehicleMapTile(vehicleId), target);
}

int getLandCluster(MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return aiFactionMovementInfo.landClusters.at(tileIndex);
	
}

int getVehicleLandCluster(int vehicleId)
{
	return(getLandCluster(getVehicleMapTile(vehicleId)));
}

bool isSameLandCluster(MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1LandCluster = getLandCluster(tile1);
	int tile2LandCluster = getLandCluster(tile2);
	
	return tile1LandCluster != -1 && tile2LandCluster != -1 && tile1LandCluster == tile2LandCluster;
	
}

bool isVehicleSameLandCluster(int vehicleId, MAP *dst)
{
	return(isSameLandCluster(getVehicleMapTile(vehicleId), dst));
}

bool isMeleeAttackableFromLandCluster(MAP *origin, MAP *target)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(target >= *MapTiles && target < *MapTiles + *MapAreaTiles);
	
	int originLandCluster = getLandCluster(origin);
	
	for (MAP *adjacentTile : getAdjacentTiles(target))
	{
		if (getLandCluster(adjacentTile) == originLandCluster)
			return true;
	}
	
	return false;
	
}

bool isVehicleMeleeAttackableFromLandCluster(int vehicleId, MAP *target)
{
	return isMeleeAttackableFromLandCluster(getVehicleMapTile(vehicleId), target);
}

bool isArtilleryAttackableFromLandCluster(MAP *origin, MAP *target)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(target >= *MapTiles && target < *MapTiles + *MapAreaTiles);
	
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	int originLandCluster = getLandCluster(origin);
	
	for (TileInfo const *targetTileInfo : targetTileInfo.range2NoCenterTileInfos)
	{
		if (getLandCluster(targetTileInfo->tile) == originLandCluster)
			return true;
	}
	
	return false;
	
}

bool isVehicleArtilleryAttackableFromLandCluster(int vehicleId, MAP *target)
{
	return isArtilleryAttackableFromLandCluster(getVehicleMapTile(vehicleId), target);
}

int getLandTransportedCluster(MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return aiFactionMovementInfo.landTransportedClusters.at(tileIndex);
	
}

int getVehicleLandTransportedCluster(int vehicleId)
{
	return(getLandTransportedCluster(getVehicleMapTile(vehicleId)));
}

bool isSameLandTransportedCluster(MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1LandTransportedCluster = getLandTransportedCluster(tile1);
	int tile2LandTransportedCluster = getLandTransportedCluster(tile2);
	
	return tile1LandTransportedCluster != -1 && tile2LandTransportedCluster != -1 && tile1LandTransportedCluster == tile2LandTransportedCluster;
	
}

bool isVehicleSameLandTransportedCluster(int vehicleId, MAP *dst)
{
	return(isSameLandTransportedCluster(getVehicleMapTile(vehicleId), dst));
}

bool isMeleeAttackableFromLandTransportedCluster(MAP *origin, MAP *target)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(target >= *MapTiles && target < *MapTiles + *MapAreaTiles);
	
	int originLandTransportedCluster = getLandTransportedCluster(origin);
	
	for (MAP *adjacentTile : getAdjacentTiles(target))
	{
		if (getLandTransportedCluster(adjacentTile) == originLandTransportedCluster)
			return true;
	}
	
	return false;
	
}

bool isVehicleMeleeAttackableFromLandTransportedCluster(int vehicleId, MAP *target)
{
	return isMeleeAttackableFromLandTransportedCluster(getVehicleMapTile(vehicleId), target);
}

bool isArtilleryAttackableFromLandTransportedCluster(MAP *origin, MAP *target)
{
	assert(origin >= *MapTiles && origin < *MapTiles + *MapAreaTiles);
	assert(target >= *MapTiles && target < *MapTiles + *MapAreaTiles);
	
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	int originLandTransportedCluster = getLandTransportedCluster(origin);
	
	for (TileInfo const *targetTileInfo : targetTileInfo.range2NoCenterTileInfos)
	{
		if (getLandTransportedCluster(targetTileInfo->tile) == originLandTransportedCluster)
			return true;
	}
	
	return false;
	
}

bool isVehicleArtilleryAttackableFromLandTransportedCluster(int vehicleId, MAP *target)
{
	return isArtilleryAttackableFromLandTransportedCluster(getVehicleMapTile(vehicleId), target);
}

int getSeaCombatCluster(MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return factionMovementInfos.at(aiFactionId).seaCombatClusters.at(tileIndex);
	
}

bool isSameSeaCombatCluster(MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1SeaCluster = getSeaCombatCluster(tile1);
	int tile2SeaCluster = getSeaCombatCluster(tile2);
	
	return tile1SeaCluster != -1 && tile2SeaCluster != -1 && tile1SeaCluster == tile2SeaCluster;
	
}

int getLandCombatCluster(MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return factionMovementInfos.at(aiFactionId).landCombatClusters.at(tileIndex);
	
}

bool isSameLandCombatCluster(MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1LandCluster = getLandCombatCluster(tile1);
	int tile2LandCluster = getLandCombatCluster(tile2);
	
	return tile1LandCluster != -1 && tile2LandCluster != -1 && tile1LandCluster == tile2LandCluster;
	
}

/**
Gets cluster based on surface type.
*/
int getCluster(MAP *tile)
{
	assert(isOnMap(tile));
	
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	return (tileInfo.ocean ? getSeaCluster(tile) : getLandCluster(tile));
	
}

/**
Gets vehicle cluster based on triad.
*/
int getVehicleCluster(int vehicleId)
{
	assert(vehicleId >= 0 && vehicleId < *VehCount);
	
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int triad = vehicle->triad();
	
	int cluster = -1;
	
	switch (triad)
	{
	case TRIAD_SEA:
		cluster = getSeaCluster(vehicleTile);
		break;
		
	case TRIAD_LAND:
		cluster = getLandCluster(vehicleTile);
		break;
		
	}
	
	return cluster;
	
}

/**
Gets base sea cluster.
*/
int getBaseSeaCluster(int baseId)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	
	MAP *baseTile = getBaseMapTile(baseId);
	
	return getSeaCluster(baseTile);
	
}

std::vector<Transfer> &getTransfers(int landCluster, int seaCluster)
{
	return aiFactionMovementInfo.transfers[landCluster][seaCluster];
}

std::vector<Transfer> &getOceanBaseTransfers(MAP *baseTile)
{
	return aiFactionMovementInfo.oceanBaseTransfers[baseTile];
}

robin_hood::unordered_flat_set<int> &getConnectedClusters(int cluster)
{
	return aiFactionMovementInfo.connectedClusters[cluster];
}

robin_hood::unordered_flat_set<int> &getConnectedSeaClusters(MAP *landTile)
{
	int landCluster = getLandCluster(landTile);
	return getConnectedClusters(landCluster);
}

robin_hood::unordered_flat_set<int> &getConnectedLandClusters(MAP *seaTile)
{
	int seaCluster = getSeaCluster(seaTile);
	return getConnectedClusters(seaCluster);
}

robin_hood::unordered_flat_set<int> &getFirstConnectedClusters(int originCluster, int dstCluster)
{
	return aiFactionMovementInfo.firstConnectedClusters[originCluster][dstCluster];
}

robin_hood::unordered_flat_set<int> &getFirstConnectedClusters(MAP *origin, MAP *dst)
{
	int originCluster = getCluster(origin);
	int dstCluster = getCluster(dst);
	
	return getFirstConnectedClusters(originCluster, dstCluster);
}

int getEnemyAirCluster(int factionId, int chassisId, int speed, MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return factionMovementInfos.at(factionId).airClusters.at(chassisId).at(speed).at(tileIndex);
	
}

bool isSameEnemyAirCluster(int factionId, int chassisId, int speed, MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1AirCluster = getEnemyAirCluster(factionId, chassisId, speed, tile1);
	int tile2AirCluster = getEnemyAirCluster(factionId, chassisId, speed, tile2);
	
	return tile1AirCluster != -1 && tile2AirCluster != -1 && tile1AirCluster == tile2AirCluster;
	
}

bool isSameEnemyAirCluster(int vehicleId, MAP *tile2)
{
	assert(vehicleId >= 0 && vehicleId < *VehCount);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	int chassisId = vehicle->chassis_type();
	int speed = getVehicleSpeed(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	return isSameEnemyAirCluster(factionId, chassisId, speed, vehicleTile, tile2);
	
}

int getEnemySeaCombatCluster(int factionId, MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return factionMovementInfos.at(factionId).seaCombatClusters.at(tileIndex);
	
}

bool isSameEnemySeaCombatCluster(int factionId, MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1SeaCluster = getEnemySeaCombatCluster(factionId, tile1);
	int tile2SeaCluster = getEnemySeaCombatCluster(factionId, tile2);
	
	return tile1SeaCluster != -1 && tile2SeaCluster != -1 && tile1SeaCluster == tile2SeaCluster;
	
}

int getEnemyLandCombatCluster(int factionId, MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return factionMovementInfos.at(factionId).landCombatClusters.at(tileIndex);
	
}

bool isSameEnemyLandCombatCluster(int factionId, MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1LandCluster = getEnemyLandCombatCluster(factionId, tile1);
	int tile2LandCluster = getEnemyLandCombatCluster(factionId, tile2);
	
	return tile1LandCluster != -1 && tile2LandCluster != -1 && tile1LandCluster == tile2LandCluster;
	
}

bool isConnected(int cluster1, int cluster2)
{
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>> const &connectedClusters = aiFactionMovementInfo.connectedClusters;
	
	if (connectedClusters.find(cluster1) == connectedClusters.end())
		return false;
	
	if (connectedClusters.at(cluster1).find(cluster2) == connectedClusters.at(cluster1).end())
		return false;
	
	return true;
		
}

/**
Generally reachable location by at least one of our vechiles.
*/
bool isReachable(MAP *tile)
{
	return aiFactionMovementInfo.reachableTileIndexes.at(tile - *MapTiles);
}

bool isSharedSea(MAP *tile)
{
	return aiFactionMovementInfo.sharedSeas.at(tile - *MapTiles);
}

int getSeaClusterArea(int seaCluster)
{
	return aiFactionMovementInfo.seaClusterAreas.at(seaCluster);
}

