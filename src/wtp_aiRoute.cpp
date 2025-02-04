
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

/// player faction movement info
PlayerFactionMovementInfo aiFactionMovementInfo;
/// unfirendly faction movement infos
std::array<EnemyFactionMovementInfo, MaxPlayerNum> enemyFactionMovementInfos;

/**
Processing tiles marker array.
Tile value represent iteration level.
This way it is easy to distinguish inner, border, and new border tiles.
*/
std::vector<int> innerTiles;
std::vector<int> borderTiles;
std::vector<int> newBorderTiles;

// ==================================================
// A* cached travel times
// ==================================================

// [movementType][speed][originIndex][dstIndex]
robin_hood::unordered_flat_map <long long, double> cachedATravelTimes;

// ==================================================
// Main operations
// ==================================================

void populateRouteData()
{
	Profiling::start("populateRouteData", "populateAIData");
	
	Profiling::start("cachedATravelTimes.clear()", "populateRouteData");
	cachedATravelTimes.clear();
	Profiling::stop("cachedATravelTimes.clear()");
	
	precomputeRouteData();
	
	Profiling::stop("populateRouteData");
	
}

void precomputeRouteData()
{
	Profiling::start("precomputeRouteData", "populateRouteData");
	
	debug("precomputeRouteData - %s\n", MFactions[aiFactionId].noun_faction);
	
	// enemy factions data
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		if (factionId == aiFactionId)
			continue;
		
		populateAirbases(factionId);
		populateAirClusters(factionId);
		populateImpediments(factionId);
		populateSeaTransportWaitTimes(factionId);
		populateSeaCombatClusters(factionId);
		populateLandCombatClusters(factionId);
		populateSeaRangeLandmarks(factionId);
		populateSeaLandmarks(factionId);
//		populateSeaApproachHexCosts(factionId);
//		populateLandApproachHexCosts(factionId);
		
	}
	
	// player faction data
	
	populateAirbases(aiFactionId);
	populateAirClusters(aiFactionId);
	populateImpediments(aiFactionId);
	populateSeaTransportWaitTimes(aiFactionId);
	populateSeaCombatClusters(aiFactionId);
	populateLandCombatClusters(aiFactionId);
	populateSeaRangeLandmarks(aiFactionId);
	populateSeaLandmarks(aiFactionId);
//	populateSeaApproachHexCosts(aiFactionId);
//	populateLandApproachHexCosts(aiFactionId);
	
	populateSeaClusters(aiFactionId);
	populateLandClusters();
	populateTransfers(aiFactionId);
	populateLandTransportedClusters();
	populateReachableLocations();
	populateAdjacentClusters();
	
	populateSharedSeas();
	
	Profiling::stop("precomputeRouteData");
}

void populateAirbases(int factionId)
{
	Profiling::start("populateAirbases", "precomputeRouteData");
	
	debug("populateAirbases aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<MAP *> &airbases = (factionId == aiFactionId ? aiFactionMovementInfo.airbases : enemyFactionMovementInfos.at(factionId).airbases);
	
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
	
	std::vector<MAP *> const &airbases = (factionId == aiFactionId ? aiFactionMovementInfo.airbases : enemyFactionMovementInfos.at(factionId).airbases);
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<int>>> &airClusters = (factionId == aiFactionId ? aiFactionMovementInfo.airClusters : enemyFactionMovementInfos.at(factionId).airClusters);
	
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
	
	if (DEBUG)
	{
		for (robin_hood::pair<int, robin_hood::unordered_flat_map<int, std::vector<int>>> const &airClusterEntry : airClusters)
		{
			int chassisId = airClusterEntry.first;
			robin_hood::unordered_flat_map<int, std::vector<int>> const &chassisAirClusters = airClusterEntry.second;
			
			debug("\tchassisId=%2d\n", chassisId);
			
			for (robin_hood::pair<int, std::vector<int>> const &chassisAirClusterEntry : chassisAirClusters)
			{
				int speed = chassisAirClusterEntry.first;
				std::vector<int> const &chassisAirSpeedAirClusters = chassisAirClusterEntry.second;
				
				debug("\t\tspeed=%2d\n", speed);
				
				for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
				{
					debug("\t\t\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), chassisAirSpeedAirClusters.at(tileIndex));
				}
				
			}
				
		}
		
	}
	
	Profiling::stop("populateAirClusters");
	
}

/**
Computes estimated values on how much faction vehicles will be slowed down passing through territory occupied by unfriendly bases/vehicles.
*/
void populateImpediments(int factionId)
{
	Profiling::start("populateImpediments", "precomputeRouteData");
	
	debug("populateImpediments aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<double> &seaTransportImpediments = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportImpediments : enemyFactionMovementInfos.at(factionId).seaTransportImpediments);
	std::vector<double> &seaCombatImpediments = (factionId == aiFactionId ? aiFactionMovementInfo.seaCombatImpediments : enemyFactionMovementInfos.at(factionId).seaCombatImpediments);
	std::vector<double> &landCombatImpediments = (factionId == aiFactionId ? aiFactionMovementInfo.landCombatImpediments : enemyFactionMovementInfos.at(factionId).landCombatImpediments);
	
	seaTransportImpediments.resize(*MapAreaTiles); std::fill(seaTransportImpediments.begin(), seaTransportImpediments.end(), 0.0);
	seaCombatImpediments.resize(*MapAreaTiles); std::fill(seaCombatImpediments.begin(), seaCombatImpediments.end(), 0.0);
	landCombatImpediments.resize(*MapAreaTiles); std::fill(landCombatImpediments.begin(), landCombatImpediments.end(), 0.0);
	
	// territory impediments
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tileIndex);
		
		if (tileInfo.ocean)
		{
			// sea
			
			// hostile faction destroys trespassing enemy transports
			
			if (isHostileTerritory(factionId, tile))
			{
				seaTransportImpediments.at(tileIndex) += Rules->move_rate_roads * IMPEDIMENT_SEA_TERRITORY_HOSTILE;
			}
			
			// hostile faction destroys trespassing enemy ships
			
			if (isHostileTerritory(factionId, tile))
			{
				seaCombatImpediments.at(tileIndex) += Rules->move_rate_roads * IMPEDIMENT_SEA_TERRITORY_HOSTILE;
			}
			
		}
		else
		{
			// land
			
			// hostile faction destroys trespassing enemy combat vehicles
			
			if (isHostileTerritory(factionId, tile))
			{
				landCombatImpediments.at(tileIndex) += Rules->move_rate_roads * IMPEDIMENT_LAND_TERRITORY_HOSTILE;
			}
			
		}
		
	}
	
	// interbase impediments
	
	std::vector<double> sumBaseDiagonalDistanceRemainders(*MapAreaTiles);
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// unfriendly base
		
		if (!isUnfriendly(factionId, base->faction_id))
			continue;
		
		// scan range tiles
		
		for (MAP *rangeTile : getRangeTiles(baseTile, 5, true))
		{
			int rangeTileIndex = rangeTile - *MapTiles;
			
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
			landCombatImpediments.at(tileIndex) += Rules->move_rate_roads * IMPEDIMENT_LAND_INTERBASE_UNFRIENDLY;
		}
			
	}
	
	if (DEBUG)
	{
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int tileIndex = tile - *MapTiles;
			
			debug("\t%s %5.2f %5.2f %5.2f\n", getLocationString(tile).c_str(), seaTransportImpediments.at(tileIndex), seaCombatImpediments.at(tileIndex), landCombatImpediments.at(tileIndex));
			
		}
		
	}
	
	Profiling::stop("populateImpediments");
	
}

void populateSeaTransportWaitTimes(int factionId)
{
	Profiling::start("populateSeaTransportWaitTimes", "precomputeRouteData");
	
	debug("populateSeaTransportWaitTimes aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	int const WAIT_TIME_MULTIPLIER = 2.0;
	
	FactionInfo const &factionInfo = aiData.factionInfos.at(factionId);
	
	std::vector<double> &seaTransportWaitTimes = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportWaitTimes : enemyFactionMovementInfos.at(factionId).seaTransportWaitTimes);
	seaTransportWaitTimes.resize(*MapAreaTiles); std::fill(seaTransportWaitTimes.begin(), seaTransportWaitTimes.end(), INF);
	
	std::vector<double> const &seaCombatImpediments = (factionId == aiFactionId ? aiFactionMovementInfo.seaCombatImpediments : enemyFactionMovementInfos.at(factionId).seaCombatImpediments);
	
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
	
	if (DEBUG)
	{
		debug("\tinitialTiles\n");
		for (SeaTransportInitialTile &initialTile : initialTiles)
		{
			debug("\t\t%s %2d %d %d\n", getLocationString(initialTile.tile).c_str(), initialTile.buildTime, initialTile.capacity, initialTile.moveRate);
		}
		
	}
	
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
					
					int hexCost = adjacentTileInfo.hexCosts.at(initialTile.movementType).at(adjacentTileAngle);
					
					if (hexCost == -1)
						continue;
					
					double stepCost = (double)hexCost + seaCombatImpediments.at(adjacentTileIndex);
					
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
	
	if (DEBUG)
	{
		debug("\twaitTimes\n");
		
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int tileIndex = tile - *MapTiles;
			double seaTransportWaitTime = seaTransportWaitTimes.at(tileIndex);
			
			if (seaTransportWaitTime != INF)
			{
				debug("\t\t%s %5.2f\n", getLocationString(tile).c_str(), seaTransportWaitTime);
			}
			
		}
		
	}
	
	Profiling::stop("populateSeaTransportWaitTimes");
	
}

void populateSeaCombatClusters(int factionId)
{
	Profiling::start("populateSeaCombatClusters", "precomputeRouteData");
	
	debug("populateSeaCombatClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<int> &seaCombatClusters = (factionId == aiFactionId ? aiFactionMovementInfo.seaCombatClusters : enemyFactionMovementInfos.at(factionId).seaCombatClusters);
	seaCombatClusters.resize(*MapAreaTiles); std::fill(seaCombatClusters.begin(), seaCombatClusters.end(), -1);
	
	robin_hood::unordered_flat_set<int> openNodes;
	robin_hood::unordered_flat_set<int> newOpenNodes;
	
	// availableTiles
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		bool ocean = is_ocean(tile);
		
		// sea
		
		if (!ocean)
			continue;
		
		// exclude unfriendly base
		
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
		bool ocean = is_ocean(baseTile);
		
		// land
		
		if (ocean)
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
				
				for (int adjacentTileIndex : currentTileInfo.adjacentTileIndexes)
				{
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
	Profiling::start("populateLandCombatClusters", "precomputeRouteData");
	
	debug("populateLandCombatClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<double> const &seaTransportWaitTimes = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportWaitTimes : enemyFactionMovementInfos.at(factionId).seaTransportWaitTimes);
	std::vector<int> &landCombatClusters = (factionId == aiFactionId ? aiFactionMovementInfo.landCombatClusters : enemyFactionMovementInfos.at(factionId).landCombatClusters);
	
	landCombatClusters.resize(*MapAreaTiles); std::fill(landCombatClusters.begin(), landCombatClusters.end(), -1);
	
	// availableTileIndexes
	
	for (TileInfo const &tileInfo : aiData.tileInfos)
	{
		// land and not blocked
		
		if (!tileInfo.ocean)
		{
			landCombatClusters.at(tileInfo.index) = -2;
		}
		
		// or sea with not infinite wait time
		
		if (seaTransportWaitTimes.at(tileInfo.index) != INF)
		{
			landCombatClusters.at(tileInfo.index) = -2;
		}
		
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
				
				for (int adjacentTileIndex : currentTileInfo.adjacentTileIndexes)
				{
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

void populateSeaRangeLandmarks(int factionId)
{
	Profiling::start("populateSeaRangeLandmarks", "precomputeRouteData");
	
	debug("populateSeaRangeLandmarks - %s\n", aiMFaction->noun_faction);
	
	int minLandmarkDistance = MIN_LANDMARK_DISTANCE * *MapAreaX / 80;
	debug("\tminLandmarkDistance=%d\n", minLandmarkDistance);
	
	std::vector<RangeLandmark> &seaRangeLandmarks = (factionId == aiFactionId ? aiFactionMovementInfo.seaRangeLandmarks : enemyFactionMovementInfos.at(factionId).seaRangeLandmarks);
	std::vector<int> &seaCombatClusters = (factionId == aiFactionId ? aiFactionMovementInfo.seaCombatClusters : enemyFactionMovementInfos.at(factionId).seaCombatClusters);
	
	robin_hood::unordered_flat_set<int> openNodes;
	robin_hood::unordered_flat_set<int> newOpenNodes;
	robin_hood::unordered_flat_set<int> endNodes;
	
	// collect initialTiles
	
	robin_hood::unordered_flat_map<int, int> initialTiles;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		int seaCluster = seaCombatClusters.at(tileIndex);
		
		if (seaCluster == -1)
			continue;
		
		if (initialTiles.find(seaCluster) != initialTiles.end())
			continue;
		
		initialTiles.emplace(seaCluster, tileIndex);
		
	}
	
	// iterate seaClusters
	
	for (robin_hood::pair<int, int> const &initialTileEntry : initialTiles)
	{
		int seaCluster = initialTileEntry.first;
		int landmarkTileIndex = initialTileEntry.second;
		
		endNodes.clear();
		
		// generate landmarks
		
		while (true)
		{
			// create landmark
			
			seaRangeLandmarks.emplace_back();
			RangeLandmark &seaRangeLandmark = seaRangeLandmarks.back();
			
			seaRangeLandmark.tileIndex = landmarkTileIndex;
			seaRangeLandmark.ranges.resize(*MapAreaTiles);
			std::fill(seaRangeLandmark.ranges.begin(), seaRangeLandmark.ranges.end(), INT_MAX);
			
			// populate initial open nodes
			
			openNodes.clear();
			newOpenNodes.clear();
			
			seaRangeLandmark.ranges.at(landmarkTileIndex) = 0;
			openNodes.insert(landmarkTileIndex);
			
			while (!openNodes.empty())
			{
				for (int currentTileIndex : openNodes)
				{
					int currentTileRange = seaRangeLandmark.ranges.at(currentTileIndex);
					
					bool endNode = true;
					
					for (int adjacentTileIndex : getAdjacentTileIndexes(currentTileIndex))
					{
						int adjacentTileSeaCluster = seaCombatClusters.at(adjacentTileIndex);
						
						if (adjacentTileSeaCluster != seaCluster)
							continue;
						
						// update value
						
						int oldAdjacentTileRange = seaRangeLandmark.ranges.at(adjacentTileIndex);
						int newAdjacentTileRange = currentTileRange + 1;
						
						if (newAdjacentTileRange < oldAdjacentTileRange)
						{
							seaRangeLandmark.ranges.at(adjacentTileIndex) = newAdjacentTileRange;
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
				
			}
			
			// select next landmark
			
			int fartherstEndNode = -1;
			int fartherstEndNodeRange = 0;
			
			for (int otherEndNode : endNodes)
			{
				int nearestLandmarkRange = INT_MAX;
				
				for (RangeLandmark const &otherSeaRangeLandmark : seaRangeLandmarks)
				{
					int otherSeaRangeLandmarkCluster = seaCombatClusters.at(otherSeaRangeLandmark.tileIndex);
					
					if (otherSeaRangeLandmarkCluster != seaCluster)
						continue;
					
					int range = otherSeaRangeLandmark.ranges.at(otherEndNode);
					
					if (range == INT_MAX)
						continue;
					
					if (range < nearestLandmarkRange)
					{
						nearestLandmarkRange = range;
					}
					
				}
				
				if (nearestLandmarkRange > fartherstEndNodeRange)
				{
					fartherstEndNode = otherEndNode;
					fartherstEndNodeRange = nearestLandmarkRange;
				}
				
			}
			
			if (fartherstEndNode == -1 || fartherstEndNodeRange < minLandmarkDistance)
			{
				// exit cycle
				break;
			}
			
			// set new landmark
			
			landmarkTileIndex = fartherstEndNode;
			endNodes.erase(landmarkTileIndex);
			
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (RangeLandmark const &seaRangeLandmark : seaRangeLandmarks)
//		{
//			debug("\t\t%s\n", getLocationString(seaRangeLandmark.tileIndex).c_str());
//		}
//		
//	}
	
	Profiling::stop("populateSeaRangeLandmarks");
	
}

void populateSeaLandmarks(int factionId)
{
	Profiling::start("populateSeaLandmarks", "precomputeRouteData");
	
	debug("populateSeaLandmarks - %s\n", aiMFaction->noun_faction);
	
	int minLandmarkDistance = MIN_LANDMARK_DISTANCE * *MapAreaX / 80;
	debug("\tminLandmarkDistance=%d\n", minLandmarkDistance);
	
	std::array<std::vector<SeaLandmark>, SeaMovementTypeCount> &seaLandmarksArray = (factionId == aiFactionId ? aiFactionMovementInfo.seaLandmarks : enemyFactionMovementInfos.at(factionId).seaLandmarks);
	std::vector<int> &seaCombatClusters = (factionId == aiFactionId ? aiFactionMovementInfo.seaCombatClusters : enemyFactionMovementInfos.at(factionId).seaCombatClusters);
	
	robin_hood::unordered_flat_set<MAP *> openNodes;
	robin_hood::unordered_flat_set<MAP *> newOpenNodes;
	robin_hood::unordered_flat_set<MAP *> endNodes;
	
	// collect initialTiles
	
	std::map<int, MAP *> initialTiles;
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		int seaCluster = seaCombatClusters.at(tileIndex);
		
		if (seaCluster == -1)
			continue;
		
		if (initialTiles.find(seaCluster) != initialTiles.end())
			continue;
		
		initialTiles.emplace(seaCluster, tile);
		
	}
	
	// iterate movementTypes
	
	for (size_t seaMovementTypeIndex = 0; seaMovementTypeIndex < SeaMovementTypeCount; seaMovementTypeIndex++)
	{
		MovementType movementType = seaMovementTypes.at(seaMovementTypeIndex);
		
		std::vector<SeaLandmark> &seaLandmarks = seaLandmarksArray.at(seaMovementTypeIndex);
		seaLandmarks.clear();
		
		// iterate seaClusters
		
		for (std::pair<int, MAP *> const &initialTileEntry : initialTiles)
		{
			int seaCluster = initialTileEntry.first;
			MAP *landmarkTile = initialTileEntry.second;
			
			endNodes.clear();
			
			// generate landmarks
			
			while (true)
			{
				// recalculate landmarkTileIndex as landmarkTileIndex changes every iteration
				
				int landmarkTileIndex = landmarkTile - *MapTiles;
				
				// create landmark
				
				seaLandmarks.emplace_back();
				SeaLandmark &seaLandmark = seaLandmarks.back();
				
				seaLandmark.tile = landmarkTile;
				seaLandmark.tileInfos.clear();
				seaLandmark.tileInfos.resize(*MapAreaTiles);
				
				// populate initial open nodes
				
				openNodes.clear();
				newOpenNodes.clear();
				
				seaLandmark.tileInfos.at(landmarkTileIndex).distance = 0;
				seaLandmark.tileInfos.at(landmarkTileIndex).movementCost = 0.0;
				seaLandmark.tileInfos.at(landmarkTileIndex).seaMovementCost = 0.0;
				openNodes.insert(landmarkTile);
				
				int currentDistance = 0;
				
				while (!openNodes.empty())
				{
					for (MAP *currentTile : openNodes)
					{
						int currentTileIndex = currentTile - *MapTiles;
						TileInfo &currentTileInfo = aiData.getTileInfo(currentTile);
						SeaLandmarkTileInfo &currentTileSeaLandmarkTileInfo = seaLandmark.tileInfos.at(currentTileIndex);
						
						bool endNode = true;
						
						for (MapAngle adjacentTileMapAngle : getAdjacentMapAngles(currentTile))
						{
							int adjacentTileAngle = adjacentTileMapAngle.angle;
							MAP *adjacentTile = adjacentTileMapAngle.tile;
							int adjacentTileIndex = adjacentTile - *MapTiles;
							int adjacentTileSeaCluster = seaCombatClusters.at(adjacentTileIndex);
							
							if (adjacentTileSeaCluster != seaCluster)
								continue;
							
							int hexCost = currentTileInfo.hexCosts.at(movementType).at(adjacentTileAngle);
							
							if (hexCost == -1)
								continue;
							
							// update value
							
							double adjacentTileMovementCost = currentTileSeaLandmarkTileInfo.movementCost + (double)hexCost;
							SeaLandmarkTileInfo &adjacentTileSeaLandmarkTileInfo = seaLandmark.tileInfos.at(adjacentTileIndex);
							
							if (adjacentTileMovementCost < adjacentTileSeaLandmarkTileInfo.movementCost)
							{
								adjacentTileSeaLandmarkTileInfo.distance = currentDistance;
								adjacentTileSeaLandmarkTileInfo.movementCost = adjacentTileMovementCost;
								adjacentTileSeaLandmarkTileInfo.seaMovementCost = adjacentTileMovementCost;
								newOpenNodes.insert(adjacentTile);
								
								endNode = false;
								
							}
							
						}
						
						if (endNode)
						{
							endNodes.insert(currentTile);
						}
						
					}
					
					openNodes.clear();
					openNodes.swap(newOpenNodes);
					
					currentDistance++;
					
				}
				
				// select next landmark
				
				MAP *fartherstEndNodeTile = nullptr;
				int fartherstEndNodeDistance = 0;
				
				for (MAP *otherEndNodeTile : endNodes)
				{
					int otherEndNodeTileIndex = otherEndNodeTile - *MapTiles;
					
					int nearestLandmarkDistance = INT_MAX;
					
					for (SeaLandmark &otherSeaLandmark : seaLandmarks)
					{
						size_t otherSeaLandmarkTileIndex = otherSeaLandmark.tile - *MapTiles;
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
						fartherstEndNodeTile = otherEndNodeTile;
						fartherstEndNodeDistance = nearestLandmarkDistance;
					}
					
				}
				
				if (fartherstEndNodeTile == nullptr || fartherstEndNodeDistance < minLandmarkDistance)
				{
					// exit cycle
					break;
				}
				
				// set new landmark
				
				landmarkTile = fartherstEndNodeTile;
				endNodes.erase(landmarkTile);
				
			}
			
		}
			
	}
	
	if (DEBUG)
	{
		for (size_t seaMovementTypeIndex = 0; seaMovementTypeIndex < SeaMovementTypeCount; seaMovementTypeIndex++)
		{
			MovementType const movementType = seaMovementTypes.at(seaMovementTypeIndex);
			
			debug("\tmovementType=%d\n", movementType);
			
			std::vector<SeaLandmark> const &seaLandmarks = seaLandmarksArray.at(seaMovementTypeIndex);
			
			for (SeaLandmark const &seaLandmark : seaLandmarks)
			{
				debug("\t\t%s\n", getLocationString(seaLandmark.tile).c_str());
			}
			
		}
		flushlog();
			
	}
	
	Profiling::stop("populateSeaLandmarks");
	
}

/**
Populates sea combat vehicle travel times to bases.
This is used for both 1) enemy vehicles -> player bases approach times, and 2) player vehicles -> enemy bases approach times
*/
void populateSeaApproachHexCosts(int factionId)
{
	Profiling::start("populateSeaApproachHexCosts", "precomputeRouteData");
	
	debug("populateSeaApproachHexCosts aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	double absoluteTolerance = APPROACH_HEX_COST_ABSOLUTE_TOLERANCE * (double)Rules->move_rate_roads;
	double relativeTolerance = APPROACH_HEX_COST_RELATIVE_TOLERANCE;
	
	bool enemy = (factionId != aiFactionId);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<double>>> &seaApproachHexCosts = (enemy ? enemyFactionMovementInfos.at(factionId).seaApproachHexCosts : aiFactionMovementInfo.seaApproachHexCosts);
	seaApproachHexCosts.clear();
	
	std::vector<int> const &seaCombatClusters = (enemy ? enemyFactionMovementInfos.at(factionId).seaCombatClusters : aiFactionMovementInfo.seaCombatClusters);
	std::vector<double> const &seaCombatImpediments = (enemy ? enemyFactionMovementInfos.at(factionId).seaCombatImpediments : aiFactionMovementInfo.seaCombatImpediments);
	
	std::vector<int> openNodes;
	std::vector<int> newOpenNodes;
	
	// collect sea bases
	
	std::vector<BaseAdjacentClusterSet> baseAdjacentClusterSets;
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
		
		// player base for enemy faction OR enemy base for player faction
		
		if (enemy)
		{
			if (!(base->faction_id == aiFactionId))
				continue;
		}
		else
		{
			if (!(base->faction_id != aiFactionId))
				continue;
		}
		
		// sea
		
		if (!baseTileInfo.ocean)
			continue;
		
		// scan adjacent tiles
		
		baseAdjacentClusterSets.push_back({baseId, baseTile, {}});
		BaseAdjacentClusterSet &baseAdjacentClusterSet = baseAdjacentClusterSets.back();
		
		for (MAP *adjacentTile : getAdjacentTiles(baseTile))
		{
			int adjacentTileIndex = adjacentTile - *MapTiles;
			int seaCombatCluster = seaCombatClusters.at(adjacentTileIndex);
			
			if (seaCombatCluster == -1)
				continue;
			
			baseAdjacentClusterSet.adjacentClusters.insert(seaCombatCluster);
			
		}
		
		if (baseAdjacentClusterSet.adjacentClusters.empty())
		{
			baseAdjacentClusterSets.pop_back();
		}
		
	}
	
//	if (DEBUG)
//	{
//		debug("seaBases\n");
//		
//		for (BaseAdjacentClusterSet baseAdjacentClusterSet : baseAdjacentClusterSets)
//		{
//			debug("\t[%3d] %s %-24s\n", baseAdjacentClusterSet.baseId, getLocationString(getBaseMapTile(baseAdjacentClusterSet.baseId)).c_str(), Bases[baseAdjacentClusterSet.baseId].name);
//		}
//		
//	}
	
	// process bases
	
	for (BaseAdjacentClusterSet const &baseAdjacentClusterSet : baseAdjacentClusterSets)
	{
		int baseId = baseAdjacentClusterSet.baseId;
		MAP *baseTile = baseAdjacentClusterSet.baseTile;
		robin_hood::unordered_flat_set<int> const &baseAdjacentClusters = baseAdjacentClusterSet.adjacentClusters;
debug(">base=%s\n", Bases[baseId].name);
int tileCount = 0;
		
		// create base approachHexCosts
		
		seaApproachHexCosts.emplace(baseId, robin_hood::unordered_flat_map<MovementType, std::vector<double>>());
		
		for (MovementType movementType : seaMovementTypes)
		{
			seaApproachHexCosts.at(baseId).emplace(movementType, std::vector<double>(*MapAreaTiles));
			std::vector<double> &approachHexCosts = seaApproachHexCosts.at(baseId).at(movementType);
			std::fill(approachHexCosts.begin(), approachHexCosts.end(), INF);
			
			// set running variables
			
			openNodes.clear();
			newOpenNodes.clear();
			
			// insert initial tile
			
			for (MAP *baseAdjacentTile : getBaseAdjacentTiles(baseTile, true))
			{
				int baseAdjacentTileIndex = baseAdjacentTile - *MapTiles;
				int baseAdjacentTileSeaCombatClusterIndex = seaCombatClusters.at(baseAdjacentTileIndex);
				
				// within cluster
				
				if (baseAdjacentClusters.find(baseAdjacentTileSeaCombatClusterIndex) == baseAdjacentClusters.end())
					continue;
				
				// add to open nodes
				
debug(">baseAdjacentTile=%s\n", getLocationString(baseAdjacentTile).c_str());
debug(">oldCost=%f\n", approachHexCosts.at(baseAdjacentTileIndex) == INF ? -1 : approachHexCosts.at(baseAdjacentTileIndex));
				approachHexCosts.at(baseAdjacentTileIndex) = 0.0;
				openNodes.push_back(baseAdjacentTileIndex);
debug(">newCost=%f\n", approachHexCosts.at(baseAdjacentTileIndex) == INF ? -1 : approachHexCosts.at(baseAdjacentTileIndex));
				
			}
			
			// iterate tiles
			
			while (!openNodes.empty())
			{
				for (int currentTileIndex : openNodes)
				{
					TileInfo &currentTileInfo = aiData.tileInfos[currentTileIndex];
					
					double currentTileApproachHexCost = approachHexCosts.at(currentTileIndex);
					
					for (MapAngle mapAngle : currentTileInfo.adjacentMapAngles)
					{
						MAP *adjacentTile = mapAngle.tile;
						int angle = mapAngle.angle;
						int adjacentTileIndex = adjacentTile - *MapTiles;
						
						int adjacentTileSeaCombatClusterIndex = seaCombatClusters.at(adjacentTileIndex);
						
						// within cluster
						
						if (baseAdjacentClusters.find(adjacentTileSeaCombatClusterIndex) == baseAdjacentClusters.end())
							continue;
						
						// tile info and opposite angle
						
						TileInfo &adjacentTileInfo = aiData.tileInfos[adjacentTileIndex];
						int opppositeAngle = (angle + ANGLE_COUNT / 2) % ANGLE_COUNT;
						
						int hexCost = adjacentTileInfo.hexCosts[movementType][opppositeAngle];
						
						if (hexCost == -1)
							continue;
						
						// stepCost with impediment
						
						double stepCost = (double)hexCost + seaCombatImpediments.at(currentTileIndex);
						
						// adjacentTileApproachHexCost
						
						double oldCost = approachHexCosts.at(adjacentTileIndex);
						double newCost = currentTileApproachHexCost + stepCost;
						double newCostThreshold = oldCost == INF ? INF : std::min(oldCost - absoluteTolerance, oldCost / relativeTolerance);
						
						// update best
						
						if ((newCostThreshold == INF && newCost < INF) || (newCostThreshold < INF && newCost < newCostThreshold))
						{
debug(">currentTile=%s adjacentTile=%s oldCost=%f newCost=%f\n", getLocationString(*MapTiles + currentTileIndex).c_str(), getLocationString(adjacentTile).c_str(), oldCost == INF ? -1 : oldCost, newCost);
debug(">tileCount=%d\n", tileCount++);
							approachHexCosts.at(adjacentTileIndex) = newCost;
							newOpenNodes.push_back(adjacentTileIndex);
							
							// update information from other bases at first hit
							
//							if (adjacentTileInfo.base && seaApproachHexCosts.find(adjacentTileInfo.baseId) != seaApproachHexCosts.end())
//							{
//	debug(">basehit\n");
//								std::vector<double> const &otherBaseApproachHexCosts = seaApproachHexCosts.at(adjacentTileInfo.baseId).at(movementType);
//								
//								for (int otherTileIndex = 0; otherTileIndex < *MapAreaTiles; otherTileIndex++)
//								{
//									double otherTileOldCost = approachHexCosts.at(otherTileIndex);
//									double otherTileNewCost = otherBaseApproachHexCosts.at(otherTileIndex) + newCost;
//									double otherTileNewCostThreshold = otherTileOldCost == INF ? INF : std::min(otherTileOldCost - absoluteTolerance, otherTileOldCost / relativeTolerance);
//									
//									if (otherTileNewCost < otherTileNewCostThreshold)
//									{
//										approachHexCosts.at(otherTileIndex) = otherTileNewCost;
//									}
//									
//								}
//								
//							}
							
						}
						
					}
					
				}
				
				// swap border tiles with newOpenNodes and clear them
				
				openNodes.swap(newOpenNodes);
				newOpenNodes.clear();
				
			}
			
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (robin_hood::pair<int, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>> const &seaApproachTimeEntry : seaApproachHexCosts)
//		{
//			int baseId = seaApproachTimeEntry.first;
//			robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>> const &baseseaApproachHexCosts = seaApproachHexCosts.at(baseId);
//			
//			debug("\t%s\n", getBase(baseId)->name);
//			
//			for (robin_hood::pair<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>> const &baseSeaApproachTimeEntry : baseseaApproachHexCosts)
//			{
//				MovementType movementType = baseSeaApproachTimeEntry.first;
//				robin_hood::unordered_flat_map<int, std::vector<double>> const &baseMovementTypeseaApproachHexCosts = baseSeaApproachTimeEntry.second;
//				
//				debug("\t\tmovementType=%d\n", movementType);
//				
//				for (robin_hood::pair<int, std::vector<double>> const &baseMovementTypeSeaApproachTimeEntry : baseMovementTypeseaApproachHexCosts)
//				{
//					int speed = baseMovementTypeSeaApproachTimeEntry.first;
//					std::vector<double> const &approachTimes = baseMovementTypeSeaApproachTimeEntry.second;
//					
//					debug("\t\t\tspeed=%d\n", speed);
//					
//					for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
//					{
//						int tileIndex = tile - *MapTiles;
//						
//						double approachTime = approachTimes.at(tileIndex);
//						
//						if (approachTime != INF)
//						{
//							debug("\t\t\t\t%s %5.2f\n", getLocationString(tile).c_str(), approachTime);
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
	
	Profiling::stop("populateSeaApproachHexCosts");
	
}

/**
Populates enamy land combat vehicle travel times to player bases.
*/
void populateLandApproachHexCosts(int factionId)
{
	Profiling::start("populateLandApproachHexCosts", "precomputeRouteData");
	
	debug("populateLandApproachHexCosts aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	bool enemy = (factionId != aiFactionId);
	FactionInfo const &factionInfo = aiData.factionInfos.at(factionId);
	
	std::vector<double> const &seaTransportWaitTimes = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportWaitTimes : enemyFactionMovementInfos.at(factionId).seaTransportWaitTimes);
	std::vector<int> const &landCombatClusters = (factionId == aiFactionId ? aiFactionMovementInfo.landCombatClusters : enemyFactionMovementInfos.at(factionId).landCombatClusters);
	std::vector<double> const &seaTransportImpediments = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportImpediments : enemyFactionMovementInfos.at(factionId).seaTransportImpediments);
	std::vector<double> const &landCombatImpediments = (factionId == aiFactionId ? aiFactionMovementInfo.landCombatImpediments : enemyFactionMovementInfos.at(factionId).landCombatImpediments);
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<std::array<double,3>>>> &landApproachHexCosts =
		(factionId == aiFactionId ? aiFactionMovementInfo.landApproachHexCosts : enemyFactionMovementInfos.at(factionId).landApproachHexCosts);
	
	landApproachHexCosts.clear();
	
	// collect land bases
	
	std::vector<int> landBaseIds;
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
		
		// player base for enemy faction OR enemy base for player faction
		
		if (enemy)
		{
			if (!(base->faction_id == aiFactionId))
				continue;
		}
		else
		{
			if (!(base->faction_id != aiFactionId))
				continue;
		}
		
		// land
		
		if (baseTileInfo.ocean)
			continue;
		
		landBaseIds.push_back(baseId);
		
	}
	
	if (DEBUG)
	{
		debug("landBases\n");
		
		for (int baseId : landBaseIds)
		{
			debug("\t[%3d] %s %-24s\n", baseId, getLocationString(getBaseMapTile(baseId)).c_str(), getBase(baseId)->name);
		}
		
	}
	
	// process land bases
	
	robin_hood::unordered_flat_set<int> borderTiles;
	robin_hood::unordered_flat_set<int> newBorderTiles;

	for (int baseId : landBaseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		int baseTileIndex = baseTile - *MapTiles;
		int baseLandCombatClusterIndex = landCombatClusters.at(baseTileIndex);
		
		// no enemy presense
		
		if (baseLandCombatClusterIndex == -1)
			continue;
		
		// create base approachHexCosts
		
		landApproachHexCosts.emplace(baseId, robin_hood::unordered_flat_map<MovementType, std::vector<std::array<double,3>>>());
		
		for (MovementType movementType : landMovementTypes)
		{
			landApproachHexCosts.at(baseId).emplace(movementType, std::vector<std::array<double,3>>(*MapAreaTiles));
			std::vector<std::array<double,3>> &approachHexCosts = landApproachHexCosts.at(baseId).at(movementType);
			std::fill(approachHexCosts.begin(), approachHexCosts.end(), std::array<double,3>({INF, INF, INF}));
			
			// set running variables
			
			borderTiles.clear();
			newBorderTiles.clear();
			
			robin_hood::unordered_flat_set<int> utilizedOtherBaseIds;
			
			// insert initial tile
			
			for (MAP *baseAdjacentTile : getBaseAdjacentTiles(baseTile, true))
			{
				int baseAdjacentTileIndex = baseAdjacentTile - *MapTiles;
				int baseAdjacentTileLandCombatClusterIndex = landCombatClusters.at(baseAdjacentTileIndex);
				
				// within cluster
				
				if (baseAdjacentTileLandCombatClusterIndex != baseLandCombatClusterIndex)
					continue;
				
				// add to border tiles
				
				approachHexCosts.at(baseAdjacentTileIndex) = {0.0, 0.0, 0.0};
				borderTiles.insert(baseAdjacentTileIndex);
				
			}
			
			// iterate tiles
			
			while (!borderTiles.empty())
			{
				for (int currentTileIndex : borderTiles)
				{
					TileInfo &currentTileInfo = aiData.tileInfos[currentTileIndex];
					
					std::array<double,3> currentTileApproachHexCost = approachHexCosts.at(currentTileIndex);
					double currentTileLandApproachHexCost = currentTileApproachHexCost.at(0);
					double currentTileSeaTransportApproachTime = currentTileApproachHexCost.at(1);
					
					for (MapAngle mapAngle : currentTileInfo.adjacentMapAngles)
					{
						MAP *adjacentTile = mapAngle.tile;
						int angle = mapAngle.angle;
						int adjacentTileIndex = adjacentTile - *MapTiles;
						
						int adjacentTileLandCombatClusterIndex = landCombatClusters.at(adjacentTileIndex);
						
						// within cluster
						
						if (adjacentTileLandCombatClusterIndex != baseLandCombatClusterIndex)
							continue;
						
						// tile info and opposite angle
						
						TileInfo &adjacentTileInfo = aiData.tileInfos[adjacentTileIndex];
						int opppositeAngle = (angle + ANGLE_COUNT / 2) % ANGLE_COUNT;
						
						// process move by surface type
						
						double seaTransportStepTime = 0.0;
						double landStepCost = 0.0;
						
						if (adjacentTileInfo.landAllowed && currentTileInfo.landAllowed) // land-land
						{
							int hexCost = adjacentTileInfo.hexCosts[movementType][opppositeAngle];
							
							if (hexCost == -1)
								continue;
							
							// stepCost with impediment
							
							landStepCost = (double)hexCost + landCombatImpediments.at(currentTileIndex);
							
						}
						else if (!adjacentTileInfo.landAllowed && !currentTileInfo.landAllowed) // sea-sea transport
						{
							int hexCost = adjacentTileInfo.hexCosts[MT_SEA_REGULAR][opppositeAngle];
							
							if (hexCost == -1)
								continue;
							
							// stepCost with impediment
							
							double seaTransportStepCost = (double)hexCost + seaTransportImpediments.at(currentTileIndex);
							
							// stepTime
							
							seaTransportStepTime = seaTransportStepCost / (double)(Rules->move_rate_roads * factionInfo.bestSeaTransportUnitSpeed);
							
						}
						else if (adjacentTileInfo.landAllowed && !currentTileInfo.landAllowed) // boarding
						{
							// seaTransportWaitTime
							
							double seaTransportWaitTime = seaTransportWaitTimes.at(currentTileIndex);
							
							if (seaTransportWaitTime == INF)
								continue;
							
							seaTransportStepTime = seaTransportWaitTime;
							landStepCost = (double)Rules->move_rate_roads;
							
						}
						else if (!adjacentTileInfo.landAllowed && currentTileInfo.landAllowed) // unboarding
						{
							landStepCost = (double)Rules->move_rate_roads;
						}
						
						// update adjacentTileApproachTime
						
						double oldAdjacentTileCombinedApproachTime = approachHexCosts.at(adjacentTileIndex).at(2);
						
						double newAdjacentTileLandApproachHexCost = currentTileLandApproachHexCost + landStepCost;
						double newAdjacentTileSeaTransportApproachTime = currentTileSeaTransportApproachTime + seaTransportStepTime;
						double newAdjacentTileCombinedApproachTime = newAdjacentTileLandApproachHexCost / (double)(2 * Rules->move_rate_roads) + newAdjacentTileSeaTransportApproachTime;
						
						// update best
						
						if (newAdjacentTileCombinedApproachTime < oldAdjacentTileCombinedApproachTime)
						{
							approachHexCosts.at(adjacentTileIndex).at(0) = newAdjacentTileLandApproachHexCost;
							approachHexCosts.at(adjacentTileIndex).at(1) = newAdjacentTileSeaTransportApproachTime;
							approachHexCosts.at(adjacentTileIndex).at(2) = newAdjacentTileCombinedApproachTime;
							
							newBorderTiles.insert(adjacentTileIndex);
							
						}
						
//						// update information from other bases at first hit
//						
//						if (adjacentTileInfo.base && landApproachHexCosts.find(adjacentTileInfo.baseId) != landApproachHexCosts.end() && utilizedOtherBaseIds.find(adjacentTileInfo.baseId) == utilizedOtherBaseIds.end())
//						{
//							utilizedOtherBaseIds.insert(adjacentTileInfo.baseId);
//							
//							std::vector<std::array<double,3>> const &otherBaseApproachHexCosts = landApproachHexCosts.at(adjacentTileInfo.baseId).at(movementType);
//							
//							for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//							{
//								double combinedApproachTime = approachHexCosts.at(tileIndex).at(2);
//								double throughOtherBaseCombinedApproachTime = otherBaseApproachHexCosts.at(tileIndex).at(2) + newAdjacentTileCombinedApproachTime;
//								
//								if (throughOtherBaseCombinedApproachTime < combinedApproachTime)
//								{
//									approachHexCosts.at(tileIndex).at(0) = otherBaseApproachHexCosts.at(tileIndex).at(0);
//									approachHexCosts.at(tileIndex).at(1) = otherBaseApproachHexCosts.at(tileIndex).at(1);
//									approachHexCosts.at(tileIndex).at(2) = otherBaseApproachHexCosts.at(tileIndex).at(2);
//								}
//								
//							}
//							
//						}
//						
					}
					
				}
				
				// swap border tiles with newBorderTiles and clear them
				
				borderTiles.swap(newBorderTiles);
				newBorderTiles.clear();
				
			}
			
		}
		
	}
	
	if (DEBUG)
	{
		for (robin_hood::pair<int, robin_hood::unordered_flat_map<MovementType, std::vector<std::array<double,3>>>> const &landApproachHexCostEntry : landApproachHexCosts)
		{
			int baseId = landApproachHexCostEntry.first;
			robin_hood::unordered_flat_map<MovementType, std::vector<std::array<double,3>>> const &baseLandApproachHexCosts = landApproachHexCosts.at(baseId);
			
			debug("\t%s\n", getBase(baseId)->name);
			
			for (robin_hood::pair<MovementType, std::vector<std::array<double,3>>> const &baseLandApproachHexCostEntry : baseLandApproachHexCosts)
			{
				MovementType movementType = baseLandApproachHexCostEntry.first;
				std::vector<std::array<double,3>> const &baseMovementTypeLandApproachHexCosts = baseLandApproachHexCostEntry.second;
				
				debug("\t\tmovementType=%d\n", movementType);
				
				for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
				{
					int tileIndex = tile - *MapTiles;
					
					std::array<double,3> approachHexCost = baseMovementTypeLandApproachHexCosts.at(tileIndex);
					
					if (approachHexCost.at(2) != INF)
					{
						debug("\t\t\t\t%s %5.2f %5.2f %5.2f\n", getLocationString(tile).c_str(), approachHexCost.at(0), approachHexCost.at(1), approachHexCost.at(2));
					}
					
				}
				
			}
			
		}
		
	}
	
	Profiling::stop("populateLandApproachHexCosts");
	
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
				
				for (int adjacentTileIndex : currentTileInfo.adjacentTileIndexes)
				{
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
	
	if (DEBUG)
	{
		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
		{
			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), seaClusters.at(tileIndex));
		}
		
	}
	
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
				
				for (int adjacentTileIndex : currentTileInfo.adjacentTileIndexes)
				{
					// available
					
					if (landClusters.at(adjacentTileIndex) != -2)
						continue;
					
					// not zoc
					
					if (isZoc(currentTileIndex) && isZoc(adjacentTileIndex))
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
	
	if (DEBUG)
	{
		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
		{
			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), landClusters.at(tileIndex));
		}
		
	}
	
	Profiling::stop("populateLandClusters");
	
}

void populateLandTransportedClusters()
{
	Profiling::start("populateLandTransportedClusters", "precomputeRouteData");
	
	debug("populateLandTransportedClusters aiFactionId=%d\n", aiFactionId);
	
	std::vector<double> const &seaTransportWaitTimes = aiFactionMovementInfo.seaTransportWaitTimes;
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
				
				for (int adjacentTileIndex : currentTileInfo.adjacentTileIndexes)
				{
					// available
					
					if (landTransportedClusters.at(adjacentTileIndex) != -2)
						continue;
					
					// not zoc on land
					
					if (isZoc(currentTileIndex) && isZoc(adjacentTileIndex))
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
	
	if (DEBUG)
	{
		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
		{
			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), landTransportedClusters.at(tileIndex));
			
		}
		
	}
	
	Profiling::stop("populateLandTransportedClusters");
	
}

void populateTransfers(int factionId)
{
	Profiling::start("populateTransfers", "precomputeRouteData");
	
	debug("populateTransfers aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	std::vector<int> const &landClusters = aiFactionMovementInfo.landClusters;
	std::vector<int> const &seaClusters = aiFactionMovementInfo.seaClusters;
	std::map<int, std::map<int, std::vector<Transfer>>> &transfers = aiFactionMovementInfo.transfers;
	std::map<MAP *, std::vector<Transfer>> &oceanBaseTransfers = aiFactionMovementInfo.oceanBaseTransfers;
	std::map<int, std::set<int>> &connectedClusters = aiFactionMovementInfo.connectedClusters;
	std::map<int, std::map<int, std::set<int>>> &firstConnectedClusters = aiFactionMovementInfo.firstConnectedClusters;
	
	transfers.clear();
	oceanBaseTransfers.clear();
	connectedClusters.clear();
	firstConnectedClusters.clear();
	
	// land-sea transfers
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tileIndex);
		int landClusterIndex = landClusters.at(tileIndex);
		
		// land
		
		if (tileInfo.ocean)
			continue;
		
		// not blocked
		
		if (isBlocked(tileIndex))
			continue;
		
		// assigned cluster
		
		if (landClusterIndex == -1)
			continue;
		
		// iterate adjacent tiles
		
		for (int adjacentTileIndex : tileInfo.adjacentTileIndexes)
		{
			MAP *adjacentTile = *MapTiles + adjacentTileIndex;
			TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTileIndex);
			int seaClusterIndex = seaClusters.at(adjacentTileIndex);
			
			// sea
			
			if (!adjacentTileInfo.ocean)
				continue;
			
			// not blocked
			
			if (isBlocked(adjacentTileIndex))
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
	
	// ocean base transfers
	
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
		
		for (MAP *adjacentTile : baseTileInfo.adjacentTiles)
		{
			TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
			
			// ocean
			
			if (!adjacentTileInfo.ocean)
				continue;
			
			// add transfer
			
			oceanBaseTransfers.at(baseTile).emplace_back(baseTile, adjacentTile);
			
		}
		
	}
	
	// connecting clusters
	
	struct RemoteConnection
	{
		int remoteCluster;
		int firstConnection;
	};
	
	for (std::pair<int, std::set<int>> connectedClusterEntry : connectedClusters)
	{
		int initialCluster = connectedClusterEntry.first;
		std::set<int> &initialConnectedClusters = connectedClusterEntry.second;
		
		// initialize connections
		
		firstConnectedClusters.emplace(initialCluster, std::map<int, std::set<int>>());
		std::map<int, std::set<int>> &initialFirstConnectedClusters = firstConnectedClusters.at(initialCluster);
		
		// expand connections
		
		std::vector<RemoteConnection> openNodes;
		std::vector<RemoteConnection> newOpenNodes;
		
		// insert initial nodes
		
		for (int initialConnectedCluster : initialConnectedClusters)
		{
			initialFirstConnectedClusters[initialConnectedCluster].insert(initialConnectedCluster);
			openNodes.push_back({initialConnectedCluster, initialConnectedCluster});
		}
		
		while (!openNodes.empty())
		{
			for (RemoteConnection openNode : openNodes)
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
			
			openNodes.swap(newOpenNodes);
			newOpenNodes.clear();
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("transfers\n");
		
		for (std::pair<int, std::map<int, std::vector<Transfer>>> const &transferEntry : aiFactionMovementInfo.transfers)
		{
			int landCluster = transferEntry.first;
			
			for (std::pair<int, std::vector<Transfer>> const &landClusterTransferEntry : transferEntry.second)
			{
				int seaCluster = landClusterTransferEntry.first;
				
				debug("\t%d -> %d\n", landCluster, seaCluster);
				
				for (Transfer const &transfer : landClusterTransferEntry.second)
				{
					debug("\t\t%s -> %s\n", getLocationString(transfer.passengerStop).c_str(), getLocationString(transfer.transportStop).c_str());
				}
				
			}
			
		}
		
		debug("oceanBaseTransfers\n");
		
		for (std::pair<MAP *, std::vector<Transfer>> const &oceanBaseTransferEntry : aiFactionMovementInfo.oceanBaseTransfers)
		{
			MAP *baseTile = oceanBaseTransferEntry.first;
			
			debug("\t%s\n", getLocationString(baseTile).c_str());
			
			for (Transfer const &transfer : oceanBaseTransferEntry.second)
			{
				debug("\t\t%s -> %s\n", getLocationString(transfer.passengerStop).c_str(), getLocationString(transfer.transportStop).c_str());
			}
			
		}
		
		debug("connectedClusters\n");
		
		for (std::pair<int, std::set<int>> const &connectedClusterEntry : aiFactionMovementInfo.connectedClusters)
		{
			int cluster1 = connectedClusterEntry.first;
			
			debug("\t%d ->\n", cluster1);
			
			for (int cluster2 : connectedClusterEntry.second)
			{
				debug("\t\t%d\n", cluster2);
			}
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("firstConnectedClusters\n");
		
		for (std::pair<int, std::map<int, std::set<int>>> const &connectingClusterEntry : firstConnectedClusters)
		{
			int initialCluster = connectingClusterEntry.first;
			std::map<int, std::set<int>> const &initialConnectingClusters = connectingClusterEntry.second;
			
			for (std::pair<int, std::set<int>> const &initialConnectingClusterEntry : initialConnectingClusters)
			{
				int terminalCluster = initialConnectingClusterEntry.first;
				std::set<int> const &initialTerminslConnectingClusters = initialConnectingClusterEntry.second;
				
				for (int firstCluster : initialTerminslConnectingClusters)
				{
					debug("\tinitial=%2d terminal=%2d first=%2d\n", initialCluster, terminalCluster, firstCluster);
				}
				
			}
			
		}
		
	}

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

void populateAdjacentClusters()
{
	Profiling::start("populateAdjacentClusters", "precomputeRouteData");
	
	debug("populateAdjacentClusters aiFactionId=%d\n", aiFactionId);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<robin_hood::unordered_flat_set<int>>>> &adjacentAirClusters = aiFactionMovementInfo.adjacentAirClusters;
	std::vector<robin_hood::unordered_flat_set<int>> &adjacentSeaClusters = aiFactionMovementInfo.adjacentSeaClusters;
	std::vector<robin_hood::unordered_flat_set<int>> &adjacentLandClusters = aiFactionMovementInfo.adjacentLandClusters;
	std::vector<robin_hood::unordered_flat_set<int>> &adjacentLandTransportedClusters = aiFactionMovementInfo.adjacentLandTransportedClusters;
	
	adjacentAirClusters.clear();
	adjacentSeaClusters.resize(*MapAreaTiles); std::fill(adjacentSeaClusters.begin(), adjacentSeaClusters.end(), robin_hood::unordered_flat_set<int>());
	adjacentLandClusters.resize(*MapAreaTiles); std::fill(adjacentLandClusters.begin(), adjacentLandClusters.end(), robin_hood::unordered_flat_set<int>());
	adjacentLandTransportedClusters.resize(*MapAreaTiles); std::fill(adjacentLandTransportedClusters.begin(), adjacentLandTransportedClusters.end(), robin_hood::unordered_flat_set<int>());
	
	// airClusters
	
	for (robin_hood::pair<int, robin_hood::unordered_flat_map<int, std::vector<int>>> const &airClusterEntry : aiFactionMovementInfo.airClusters)
	{
		int chassisId = airClusterEntry.first;
		robin_hood::unordered_flat_map<int, std::vector<int>> const &chassisAirClusters = airClusterEntry.second;
		
		for (robin_hood::pair<int, std::vector<int>> const &chassisAirClusterEntry : chassisAirClusters)
		{
			int speed = chassisAirClusterEntry.first;
			
			adjacentAirClusters[chassisId][speed].resize(*MapAreaTiles);
			
			for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
			{
				int tileIndex = tile - *MapTiles;
				
				for (MAP *adjacentTile : getAdjacentTiles(tile))
				{
					int airCluster = getAirCluster(chassisId, speed, adjacentTile);
					
					if (airCluster == -1)
						continue;
					
					adjacentAirClusters.at(chassisId).at(speed).at(tileIndex).insert(airCluster);
					
				}
				
			}
			
		}
		
	}
	
	// seaClusters
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		
		for (MAP *adjacentTile : getAdjacentTiles(tile))
		{
			int seaCluster = getSeaCluster(adjacentTile);
			
			if (seaCluster == -1)
				continue;
			
			adjacentSeaClusters.at(tileIndex).insert(seaCluster);
			
		}
		
	}
	
	// landClusters
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		
		for (MAP *adjacentTile : getAdjacentTiles(tile))
		{
			int landCluster = getLandCluster(adjacentTile);
			
			if (landCluster == -1)
				continue;
			
			adjacentLandClusters.at(tileIndex).insert(landCluster);
			
		}
		
	}
	
	// landTransportedClusters
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		
		for (MAP *adjacentTile : getAdjacentTiles(tile))
		{
			int landTransportedCluster = getLandTransportedCluster(adjacentTile);
			
			if (landTransportedCluster == -1)
				continue;
			
			adjacentLandTransportedClusters.at(tileIndex).insert(landTransportedCluster);
			
		}
		
	}
	
	Profiling::stop("populateAdjacentClusters");
	
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

//double getTravelTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst)
//{
//	double travelTime = INF;
//	
//	switch (movementType)
//	{
//	case MT_AIR:
//		travelTime = getATravelTime(movementType, speed, org, dst);
//		break;
//		
//	case MT_SEA_REGULAR:
//	case MT_SEA_NATIVE:
//		travelTime = getSeaTravelTime(factionId, movementType, speed, org, dst);
//		break;
//		
//	case MT_LAND_REGULAR:
//	case MT_LAND_NATIVE:
//		travelTime = getATravelTime(movementType, speed, org, dst);
//		break;
//		
//	default:
//		debug("ERROR: unknown movementType: %d\n", movementType);
//		
//	}
//	
//	return travelTime;
//	
//}
//
//double getSeaTravelTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst)
//{
//	assert(factionId >= 0 && factionId < MaxPlayerNum);
//	assert(movementType >= seaMovementTypes.front() && movementType <= seaMovementTypes.back());
//	assert(speed > 0);
//	assert(isOnMap(org));
//	assert(isOnMap(dst));
//	
//	double travelTime = getSeaLTravelTime(factionId, movementType, speed, org, dst);
//	double distance = travelTime / (double)(Rules->move_rate_roads * speed);
//	
//	if (distance < A_DISTANCE_TRESHOLD)
//	{
//		travelTime = getATravelTime(movementType, speed, org, dst);
//	}
//	
//	return travelTime;
//
//}
//
//double getSeaLTravelTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst)
//{
//	assert(factionId >= 0 && factionId < MaxPlayerNum);
//	assert(movementType >= seaMovementTypes.front() && movementType <= seaMovementTypes.back());
//	assert(speed > 0);
//	assert(isOnMap(org));
//	assert(isOnMap(dst));
//	
//	int seaMovementTypeIndex = movementType - MT_SEA_REGULAR;
//	
//	std::vector<int> const &factionSeaRegions = landmarkData.allFactionSeaRegions.at(factionId);
//	
//	int orgTileIndex = org - *MapTiles;
//	int dstTileIndex = dst - *MapTiles;
//	
//	int orgSeaRegion = factionSeaRegions.at(orgTileIndex);
//	int dstSeaRegion = factionSeaRegions.at(dstTileIndex);
//	
//	if (orgSeaRegion == -1 || dstSeaRegion == -1 || orgSeaRegion != dstSeaRegion)
//		return INF;
//	
//	double maxLandmarkMovementCost = 0.0;
//	
//	for (SeaLandmark const &seaLandmark : landmarkData.seaLandmarks.at(seaMovementTypeIndex))
//	{
//		int seaLandmarkTileIndex = seaLandmark.tile - *MapTiles;
//		int seaLandmarkSeaRegion = factionSeaRegions.at(seaLandmarkTileIndex);
//		
//		// reachable landmark
//		
//		if (seaLandmarkSeaRegion == -1 || seaLandmarkSeaRegion != orgSeaRegion)
//			continue;
//		
//		// landmarkTravelTime
//		
//		double landmarkMovementCost = abs(seaLandmark.tileInfos.at(dstTileIndex).movementCost - seaLandmark.tileInfos.at(orgTileIndex).movementCost);
//		
//		// update max
//		
//		maxLandmarkMovementCost = std::max(maxLandmarkMovementCost, landmarkMovementCost);
//		
//	}
//	
//	return maxLandmarkMovementCost / (double)(Rules->move_rate_roads * speed);
//	
//}
//
/**
Computes enemy combat vehicle travel time to our bases.
*/
double getEnemyApproachTime(int baseId, int vehicleId)
{
	Profiling::start("- getEnemyApproachTime");
	
	VEH *vehicle = getVehicle(vehicleId);
	int chassisId = vehicle->chassis_type();
	
	double approachTime = INF;
	
	switch (chassisId)
	{
	case CHS_GRAVSHIP:
		approachTime = getEnemyGravshipApproachTime(baseId, vehicleId);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		approachTime = getEnemyRangedAirApproachTime(baseId, vehicleId);
		break;
	case CHS_CRUISER:
	case CHS_FOIL:
//		approachTime = getEnemySeaApproachTime(baseId, vehicleId);
//		approachTime = getPathTravelTime(vehicleId, getBaseMapTile(baseId));
		approachTime = getVehicleATravelTime(vehicleId, getBaseMapTile(baseId));
		break;
	case CHS_HOVERTANK:
	case CHS_SPEEDER:
	case CHS_INFANTRY:
//		approachTime = getEnemyLandApproachTime(baseId, vehicleId);
//		approachTime = getPathTravelTime(vehicleId, getBaseMapTile(baseId));
		approachTime = getVehicleATravelTime(vehicleId, getBaseMapTile(baseId));
		break;
	}
	
	Profiling::stop("- getEnemyApproachTime");
	return approachTime;
	
}

/**
Computes enemy gravship combat vehicle travel time to our bases.
*/
double getEnemyGravshipApproachTime(int baseId, int vehicleId)
{
	MAP *baseTile = getBaseMapTile(baseId);
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int chassisId = vehicle->chassis_type();
	int speed = getVehicleSpeed(vehicleId);
	
	assert(chassisId == CHS_GRAVSHIP);
	assert(speed > 0);
	
	// approach time
	
	int range = getRange(vehicleTile, baseTile);
	
	return (double)range / (double)speed;
	
}

/**
Computes enemy ranged air combat vehicle travel time to our bases.
*/
double getEnemyRangedAirApproachTime(int baseId, int vehicleId)
{
	MAP *baseTile = getBaseMapTile(baseId);
	int baseTileIndex = baseTile - *MapTiles;
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int vehicleTileIndex = vehicleTile - *MapTiles;
	int chassisId = vehicle->chassis_type();
	int speed = getVehicleSpeed(vehicleId);
	
	assert(vehicle->triad() == TRIAD_AIR && chassisId != CHS_GRAVSHIP);
	assert(speed > 0);
	
	std::vector<int> const &airClusters = enemyFactionMovementInfos.at(factionId).airClusters.at(chassisId).at(speed);
	
	// vehicle air cluster
	
	int vehicleAirCluster = airClusters.at(vehicleTileIndex);
	assert(vehicleAirCluster != -1);
	
	// base should be in same air cluster
	
	if (airClusters.at(baseTileIndex) != vehicleAirCluster)
		return INF;
	
	// approach time
	
	int range = getRange(vehicleTile, baseTile);
	
	return RANGED_AIR_TRAVEL_TIME_COEFFICIENT * (double)range / (double)speed;
	
}

/**
Computes enemy combat vehicle travel time to our bases.
*/
double getEnemySeaApproachTime(int baseId, int vehicleId)
{
	Profiling::start("- getEnemySeaApproachTime");
	
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	
	assert(vehicle->triad() == TRIAD_SEA);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<double>>> const &seaApproachHexCosts = enemyFactionMovementInfos.at(factionId).seaApproachHexCosts;
	
	// no approach hex cost for this base - sea unit is not in base sea cluster
	
	if (seaApproachHexCosts.find(baseId) == seaApproachHexCosts.end())
	{
		Profiling::stop("- getEnemySeaApproachTime");
		return INF;
	}
	
	// return sea approach time
	
	MovementType movementType = getVehicleMovementType(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int vehicleTileIndex = vehicleTile - *MapTiles;
	int moveRate = std::max(Rules->move_rate_roads, getVehicleMoveRate(vehicleId));
	
	Profiling::stop("- getEnemySeaApproachTime");
	return seaApproachHexCosts.at(baseId).at(movementType).at(vehicleTileIndex) / (double)moveRate;
	
}

/**
Computes enemy combat vehicle travel time to player base.
*/
double getEnemyLandApproachTime(int baseId, int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	
	assert(vehicle->triad() == TRIAD_LAND);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<std::array<double,3>>>> const &landApproachHexCosts = enemyFactionMovementInfos.at(factionId).landApproachHexCosts;
	
	// no approach time for this base - land unit is not in base land cluster
	
	if (landApproachHexCosts.find(baseId) == landApproachHexCosts.end())
		return INF;
	
	// return land approach time
	
	MovementType movementType = getVehicleMovementType(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int vehicleTileIndex = vehicleTile - *MapTiles;
	int moveRate = getVehicleMoveRate(vehicleId);
	std::array<double,3> landApproachHexCost = landApproachHexCosts.at(baseId).at(movementType).at(vehicleTileIndex);
	
	if (landApproachHexCost.at(2) == INF)
		return INF;
	
	return landApproachHexCost.at(0) / (double)moveRate + landApproachHexCost.at(1);
	
}

/**
Computes player combat unit travel time to enemy base.
*/
double getPlayerApproachTime(int baseId, int unitId, MAP *origin)
{
	UNIT *unit = getUnit(unitId);
	int chassisId = unit->chassis_id;
	
	double approachTime = INF;
	
	switch (chassisId)
	{
	case CHS_GRAVSHIP:
		approachTime = getPlayerGravshipApproachTime(baseId, unitId, origin);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		approachTime = getPlayerRangedAirApproachTime(baseId, unitId, origin);
		break;
	case CHS_CRUISER:
	case CHS_FOIL:
		approachTime = getPlayerSeaApproachTime(baseId, unitId, origin);
		break;
	case CHS_HOVERTANK:
	case CHS_SPEEDER:
	case CHS_INFANTRY:
		approachTime = getPlayerLandApproachHexCost(baseId, unitId, origin);
		break;
	}
	
	return approachTime;
	
}

/**
Computes player gravship combat unit travel time to enemy base.
*/
double getPlayerGravshipApproachTime(int baseId, int unitId, MAP *origin)
{
	MAP *baseTile = getBaseMapTile(baseId);
	UNIT *unit = getUnit(unitId);
	int chassisId = unit->chassis_id;
	int speed = getUnitSpeed(aiFactionId, unitId);
	
	assert(chassisId == CHS_GRAVSHIP);
	assert(speed > 0);
	
	// approach time
	
	int range = getRange(origin, baseTile);
	
	return (double)range / (double)speed;
	
}

/**
Computes player ranged air combat unit travel time to enemy base.
*/
double getPlayerRangedAirApproachTime(int baseId, int unitId, MAP *origin)
{
	MAP *baseTile = getBaseMapTile(baseId);
	int baseTileIndex = baseTile - *MapTiles;
	UNIT *unit = getUnit(unitId);
	int chassisId = unit->chassis_id;
	int speed = getUnitSpeed(aiFactionId, unitId);
	int originTileIndex = origin - *MapTiles;
	
	assert(unit->triad() == TRIAD_AIR && chassisId != CHS_GRAVSHIP);
	assert(speed > 0);
	
	std::vector<int> const &airClusters = aiFactionMovementInfo.airClusters.at(chassisId).at(speed);
	
	// origin air cluster
	
	int originAirCluster = airClusters.at(originTileIndex);
	assert(originAirCluster != -1);
	
	// base should be in same air cluster
	
	int baseAirCluster = airClusters.at(baseTileIndex);
		
	if (baseAirCluster != originAirCluster)
		return INF;
	
	// approach time
	
	int range = getRange(origin, baseTile);
	
	return RANGED_AIR_TRAVEL_TIME_COEFFICIENT * (double)range / (double)speed;
	
}

/**
Computes player combat unit travel time to enemy base.
*/
double getPlayerSeaApproachTime(int baseId, int unitId, MAP *origin)
{
	UNIT *unit = getUnit(unitId);
	int originTileIndex = origin - *MapTiles;
	int moveRate = getUnitMoveRate(aiFactionId, unitId);
	
	assert(unit->triad() == TRIAD_SEA);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<double>>> const &seaApproachHexCosts = aiFactionMovementInfo.seaApproachHexCosts;
	
	// no approach hex cost for this base - sea unit is not in base sea cluster
	
	if (seaApproachHexCosts.find(baseId) == seaApproachHexCosts.end())
		return INF;
	
	// return sea approach time
	
	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
	
	return seaApproachHexCosts.at(baseId).at(movementType).at(originTileIndex) / (double)moveRate;
	
}

/**
Computes player combat vehicle travel time to enemy base.
*/
double getPlayerLandApproachHexCost(int baseId, int unitId, MAP *origin)
{
	UNIT *unit = getUnit(unitId);
	int moveRate = getUnitMoveRate(aiFactionId, unitId);
	int originTileIndex = origin - *MapTiles;
	
	assert(unit->triad() == TRIAD_LAND);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<std::array<double,3>>>> const &landApproachHexCosts = aiFactionMovementInfo.landApproachHexCosts;
	
	// no approach time for this base - land unit is not in base land cluster
	
	if (landApproachHexCosts.find(baseId) == landApproachHexCosts.end())
		return INF;
	
	// return land approach time
	
	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
	std::array<double,3> landApproachHexCost = landApproachHexCosts.at(baseId).at(movementType).at(originTileIndex);
	
	return landApproachHexCost.at(0) / (double)moveRate + landApproachHexCost.at(1);
	
}

// ==================================================
// Generic, not path related travel computations
// ==================================================

double getGravshipUnitTravelTime(int /*unitId*/, int speed, MAP *origin, MAP *dst)
{
	int range = getRange(origin, dst);
	
	return (double)range / (double)speed;
	
}

double getGravshipUnitTravelTime(int unitId, MAP *origin, MAP *dst)
{
	int speed = getUnitSpeed(aiFactionId, unitId);
	
	return getGravshipUnitTravelTime(unitId, speed, origin, dst);
	
}

double getGravshipVehicleTravelTime(int vehicleId, MAP *origin, MAP *dst)
{
	VEH *vehicle = getVehicle(vehicleId);
	int unitId = vehicle->unit_id;
	int speed = getVehicleSpeed(vehicleId);
	
	return getGravshipUnitTravelTime(unitId, speed, origin, dst);
	
}

double getAirRangedUnitTravelTime(int unitId, int speed, MAP *origin, MAP *dst)
{
	UNIT *unit = getUnit(unitId);
	int chassisId = unit->chassis_id;
	
	// same airCluster
	
	if (!isSameAirCluster(chassisId, speed, origin, dst))
		return INF;
	
	// travel time
	
	int range = getRange(origin, dst);
	return RANGED_AIR_TRAVEL_TIME_COEFFICIENT * (double)range / (double)speed;
	
}

double getAirRangedUnitTravelTime(int unitId, MAP *origin, MAP *dst)
{
	int speed = getUnitSpeed(aiFactionId, unitId);
	return getAirRangedUnitTravelTime(unitId, speed, origin, dst);
}

double getAirRangedVehicleTravelTime(int vehicleId, MAP *origin, MAP *dst)
{
	VEH *vehicle = getVehicle(vehicleId);
	int unitId = vehicle->unit_id;
	int speed = getVehicleSpeed(vehicleId);
	
	return getAirRangedUnitTravelTime(unitId, speed, origin, dst);
	
}

// ==================================================
// A* cached movement costs
// ==================================================

double getCachedATravelTime(MovementType movementType, int speed, int originIndex, int dstIndex)
{
	assert(speed < 256);
	
//	executionProfiles["- getATravelTime - cachedATravelTime::get"].start();
	
	long long key =
		+ (long long)movementType	* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles) * 256
		+ (long long)speed			* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles)
		+ (long long)originIndex		* (long long)(*MapAreaTiles)
		+ (long long)dstIndex
	;
	
	robin_hood::unordered_flat_map <long long, double>::iterator cachedATravelTimeIterator = cachedATravelTimes.find(key);
	
	if (cachedATravelTimeIterator == cachedATravelTimes.end())
		return INF;
	
//	executionProfiles["- getATravelTime - cachedATravelTime::get"].stop();
	
	return cachedATravelTimeIterator->second;
	
}

void setCachedATravelTime(MovementType movementType, int speed, int originIndex, int dstIndex, double travelTime)
{
	assert(speed < 256);
	
//	executionProfiles["- getATravelTime - cachedATravelTime::set"].start();
	
	long long key =
		+ (long long)movementType	* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles) * 256
		+ (long long)speed			* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles)
		+ (long long)originIndex		* (long long)(*MapAreaTiles)
		+ (long long)dstIndex
	;
	
	cachedATravelTimes[key] = travelTime;
	
//	executionProfiles["- getATravelTime - cachedATravelTime::set"].stop();
	
}

// ==================================================
// A* path finding algorithm
// ==================================================

/**
Computes A* travel time for *player* faction.
*/
double getATravelTime(MovementType movementType, int speed, MAP *origin, MAP *dst)
{
//	debug("getATravelTime movementType=%d speed=%d %s->%s\n", movementType, speed, getLocationString(origin).c_str(), getLocationString(dst).c_str());
	
	Profiling::start("- getATravelTime");
	
	int originIndex = origin - *MapTiles;
	int dstIndex = dst - *MapTiles;
	
	// at dst
	
	if (origin == dst)
	{
		Profiling::stop("- getATravelTime");
		return 0.0;
	}
	
	// origin cluster
	
	int originCluster = -1;
	
	switch (movementType)
	{
	case MT_AIR:
		
		// no cluster restriction
		
		break;
		
	case MT_SEA_REGULAR:
	case MT_SEA_NATIVE:
		
		originCluster = getSeaCluster(origin);
		
		break;
		
	case MT_LAND_REGULAR:
	case MT_LAND_NATIVE:
		
		originCluster = getLandTransportedCluster(origin);
		
		break;
		
	}
	
	// check cached value
	
	double cachedTravelTime = getCachedATravelTime(movementType, speed, originIndex, dstIndex);
	
	if (cachedTravelTime != INF)
	{
		Profiling::stop("- getATravelTime");
		return cachedTravelTime;
	}
	
	// initialize collections
	
	std::priority_queue<FValue, std::vector<FValue>, FValueComparator> openNodes;
	
	setCachedATravelTime(movementType, speed, originIndex, originIndex, 0.0);
	double originF = getRouteVectorDistance(origin, dst);
	openNodes.push({originIndex, originF});
	
	// process path
	
	while (!openNodes.empty())
	{
		// get top node and remove it from list

		FValue currentOpenNode = openNodes.top();
		openNodes.pop();
		
		// get current tile data
		
		int currentTileIndex = currentOpenNode.tileIndex;
		TileInfo &currentTileInfo = aiData.getTileInfo(currentTileIndex);
		
		double currentTileTravelTime = getCachedATravelTime(movementType, speed, originIndex, currentTileIndex);
		
		// check surrounding tiles
		
		for (MapAngle mapAngle : currentTileInfo.adjacentMapAngles)
		{
			MAP *adjacentTile = mapAngle.tile;
			int adjacentTileIndex = adjacentTile - *MapTiles;
			int angle = mapAngle.angle;
			
			TileInfo &adjacentTileInfo = aiData.getTileInfo(adjacentTile);
			
			// within cluster for surface vehicle
			
			switch (movementType)
			{
			case MT_AIR:
				
				// no cluster restriction
				
				break;
				
			case MT_SEA_REGULAR:
			case MT_SEA_NATIVE:
				{
					if (getSeaCluster(adjacentTile) != originCluster)
						continue;
				}
				
				break;
				
			case MT_LAND_REGULAR:
			case MT_LAND_NATIVE:
				{
					if (getLandTransportedCluster(adjacentTile) != originCluster)
						continue;
				}
				
				break;
				
			}
			
			// stepTime and movement restrictions by movementType
			
			double stepTime = INF;
			
			switch (movementType)
			{
			case MT_AIR:
			case MT_SEA_REGULAR:
			case MT_SEA_NATIVE:
				{
					// hexCost
					
					int hexCost = currentTileInfo.hexCosts[movementType][angle];
					
					// sensible hexCost
					
					if (hexCost == -1)
						continue;
					
					// stepTime
					
					stepTime = (double)hexCost / (double)(Rules->move_rate_roads * speed);
					
				}
				
				break;
				
			case MT_LAND_REGULAR:
			case MT_LAND_NATIVE:
				
				// movement restrictions by surface type
				
				if (!currentTileInfo.ocean && !adjacentTileInfo.ocean) // land-land
				{
					// not zoc (could be within the cluster but specific movement can still be restricted by zoc)
					
					if (isZoc(currentTileIndex) && isZoc(adjacentTileIndex))
						continue;
					
					// hexCost
					
					int hexCost = currentTileInfo.hexCosts[movementType][angle];
					
					// sensible hexCost
					
					if (hexCost == -1)
						continue;
					
					// stepTime
					
					stepTime = (double)hexCost / (double)(Rules->move_rate_roads * speed);
					
				}
				else if (currentTileInfo.ocean && adjacentTileInfo.ocean) // sea-sea transport movement
				{
					// hexCost for transport movement
					
					int hexCost = currentTileInfo.hexCosts[MT_SEA_REGULAR][angle];
					
					// sensible hexCost
					
					if (hexCost == -1)
						continue;
					
					// stepTime
					
					stepTime = (double)hexCost / (double)(Rules->move_rate_roads * aiFactionInfo->bestSeaTransportUnitSpeed);
					
				}
				else if (!currentTileInfo.ocean && adjacentTileInfo.ocean) // boarding
				{
					// not blocked
					
					if (isBlocked(adjacentTileIndex))
						continue;
					
					// transport is available
					
					double seaTransportWaitTime = aiFactionMovementInfo.seaTransportWaitTimes.at(adjacentTileIndex);
					
					if (seaTransportWaitTime == INF)
						continue;
					
					// stepTime
					
					stepTime = seaTransportWaitTime;
					
				}
				else if (currentTileInfo.ocean && !adjacentTileInfo.ocean) // unboarding
				{
					// not blocked
					
					if (isBlocked(adjacentTileIndex))
						continue;
					
					// stepTime
					
					stepTime = (double)Rules->move_rate_roads / (double)(Rules->move_rate_roads * speed);
					
				}
				
				break;
				
			}
			
			// travelTime
			
			double travelTime = currentTileTravelTime + stepTime;
			
			// get cached value or Infinity
			
			double cachedAdjacentTileTravelTime = getCachedATravelTime(movementType, speed, originIndex, adjacentTileIndex);
			
			// update adjacentTile cachedTravelTime
			
			if (cachedAdjacentTileTravelTime == INF || travelTime < cachedAdjacentTileTravelTime)
			{
				setCachedATravelTime(movementType, speed, originIndex, adjacentTileIndex, travelTime);
				
				// f value
				// take cashed movement cost if it is known, otherwise estimate
				
				double adjacentTileDestinationTravelTime = getCachedATravelTime(movementType, speed, adjacentTileIndex, dstIndex);
				
				if (adjacentTileDestinationTravelTime == INF)
				{
					adjacentTileDestinationTravelTime = getRouteVectorDistance(adjacentTile, dst);
				}
				
				double adjacentTileF = travelTime + getRouteVectorDistance(adjacentTile, dst);
				
				openNodes.push({adjacentTileIndex, adjacentTileF});
				
			}
			
		}
		
	}
	
	// retrieve cache value that should be already there
	
	double travelTime = getCachedATravelTime(movementType, speed, originIndex, dstIndex);
	
	Profiling::stop("- getATravelTime");
	return travelTime;
	
}

double getUnitATravelTime(int unitId, int speed, MAP *origin, MAP *dst)
{
	UNIT *unit = getUnit(unitId);
	int chassisId = unit->chassis_id;
	
	if (speed <= 0)
		return INF;
	
	double travelTime = INF;
	
	switch (chassisId)
	{
	case CHS_GRAVSHIP:
		travelTime = getGravshipUnitTravelTime(unitId, speed, origin, dst);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		travelTime = getAirRangedUnitTravelTime(unitId, speed, origin, dst);
		break;
	case CHS_FOIL:
	case CHS_CRUISER:
		travelTime = getSeaUnitATravelTime(unitId, speed, origin, dst);
		break;
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		travelTime = getLandUnitATravelTime(unitId, speed, origin, dst);
		break;
	}
	
	return travelTime;
	
}

double getUnitATravelTime(int unitId, MAP *origin, MAP *dst)
{
	int speed = getUnitSpeed(aiFactionId, unitId);
	
	return getUnitATravelTime(unitId, speed, origin, dst);
	
}

double getSeaUnitATravelTime(int unitId, int speed, MAP *origin, MAP *dst)
{
	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
	
	// possible travel
	
	if (!isSameSeaCluster(origin, dst))
		return INF;
	
	// travelTime
	
	return getATravelTime(movementType, speed, origin, dst);
	
}

double getLandUnitATravelTime(int unitId, int speed, MAP *origin, MAP *dst)
{
	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
	
	// possible travel within land transported cluster
	
	if (!isSameLandTransportedCluster(origin, dst))
		return INF;
	
	// travelTime
	
	return getATravelTime(movementType, speed, origin, dst);
	
}

double getVehicleATravelTime(int vehicleId, MAP *origin, MAP *dst)
{
	VEH *vehicle = getVehicle(vehicleId);
	int unitId = vehicle->unit_id;
	int speed = getVehicleSpeed(vehicleId);
	
	return getUnitATravelTime(unitId, speed, origin, dst);
	
}

double getVehicleATravelTime(int vehicleId, MAP *dst)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	return getVehicleATravelTime(vehicleId, vehicleTile, dst);
	
}

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
MovementType getUnitMovementType(int factionId, int unitId)
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
			movementType = native ? MT_SEA_NATIVE : MT_SEA_REGULAR;
		}
		break;
		
	case TRIAD_LAND:
		{
			bool native = xenoempatyDome || isNativeUnit(unitId);
			movementType = native ? MT_LAND_NATIVE : MT_LAND_REGULAR;
		}
		break;
		
	}
	
	return movementType;
	
}

MovementType getVehicleMovementType(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	return getUnitMovementType(vehicle->faction_id, vehicle->unit_id);
}

int getAirCluster(int chassisId, int speed, MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return aiFactionMovementInfo.airClusters.at(chassisId).at(speed).at(tileIndex);
	
}

int getVehicleAirCluster(int vehicleId)
{
	return getAirCluster(getVehicle(vehicleId)->chassis_type(), getVehicleSpeed(vehicleId), getVehicleMapTile(vehicleId));
}

bool isSameAirCluster(int chassisId, int speed, MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1AirCluster = getAirCluster(chassisId, speed, tile1);
	int tile2AirCluster = getAirCluster(chassisId, speed, tile2);
	
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
	
	int originAirCluster = getAirCluster(chassisId, speed, origin);
	
	for (MAP *adjacentTile : getAdjacentTiles(target))
	{
		if (getAirCluster(chassisId, speed, adjacentTile) == originAirCluster)
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
	
	int originSeaCluster = getSeaCluster(origin);
	
	for (MAP *adjacentTile : getRangeTiles(target, Rules->artillery_max_rng, false))
	{
		if (getSeaCluster(adjacentTile) == originSeaCluster)
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
	
	int originLandCluster = getLandCluster(origin);
	
	for (MAP *adjacentTile : getRangeTiles(target, Rules->artillery_max_rng, false))
	{
		if (getLandCluster(adjacentTile) == originLandCluster)
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
	
	int originLandTransportedCluster = getLandTransportedCluster(origin);
	
	for (MAP *adjacentTile : getRangeTiles(target, Rules->artillery_max_rng, false))
	{
		if (getLandTransportedCluster(adjacentTile) == originLandTransportedCluster)
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
	return aiFactionMovementInfo.seaCombatClusters.at(tileIndex);
	
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
	return aiFactionMovementInfo.landCombatClusters.at(tileIndex);
	
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

std::set<int> &getConnectedClusters(int cluster)
{
	return aiFactionMovementInfo.connectedClusters[cluster];
}

std::set<int> &getConnectedSeaClusters(MAP *landTile)
{
	int landCluster = getLandCluster(landTile);
	return getConnectedClusters(landCluster);
}

std::set<int> &getConnectedLandClusters(MAP *seaTile)
{
	int seaCluster = getSeaCluster(seaTile);
	return getConnectedClusters(seaCluster);
}

std::set<int> &getFirstConnectedClusters(int originCluster, int dstCluster)
{
	return aiFactionMovementInfo.firstConnectedClusters[originCluster][dstCluster];
}

std::set<int> &getFirstConnectedClusters(MAP *origin, MAP *dst)
{
	int originCluster = getCluster(origin);
	int dstCluster = getCluster(dst);
	
	return getFirstConnectedClusters(originCluster, dstCluster);
}

int getEnemyAirCluster(int factionId, int chassisId, int speed, MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return enemyFactionMovementInfos.at(factionId).airClusters.at(chassisId).at(speed).at(tileIndex);
	
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
	return enemyFactionMovementInfos.at(factionId).seaCombatClusters.at(tileIndex);
	
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
	return enemyFactionMovementInfos.at(factionId).landCombatClusters.at(tileIndex);
	
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
	std::map<int, std::set<int>> const &connectedClusters = aiFactionMovementInfo.connectedClusters;
	
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

robin_hood::unordered_flat_set<int> getAdjacentAirClusters(int chassisId, int speed, MAP *tile)
{
	return aiFactionMovementInfo.adjacentAirClusters.at(chassisId).at(speed).at(tile - *MapTiles);
}

robin_hood::unordered_flat_set<int> getAdjacentSeaClusters(MAP *tile)
{
	return aiFactionMovementInfo.adjacentSeaClusters.at(tile - *MapTiles);
}

robin_hood::unordered_flat_set<int> getAdjacentLandClusters(MAP *tile)
{
	return aiFactionMovementInfo.adjacentLandClusters.at(tile - *MapTiles);
}

robin_hood::unordered_flat_set<int> getAdjacentLandTransportedClusters(MAP *tile)
{
	return aiFactionMovementInfo.adjacentLandTransportedClusters.at(tile - *MapTiles);
}

/**
Checks wheater tile has at least one adjacent land cluster tile.
*/
bool isAdjacentLandCluster(MAP *tile)
{
	return !aiFactionMovementInfo.adjacentLandClusters.at(tile - *MapTiles).empty();
}

/**
Checks whether at least one adjacent tile belongs to sea or land cluster == not blocked.
*/
bool isAdjacentSeaOrLandCluster(MAP *tile)
{
	return !aiFactionMovementInfo.adjacentSeaClusters.at(tile - *MapTiles).empty() || !aiFactionMovementInfo.adjacentLandClusters.at(tile - *MapTiles).empty();
}

/**
Checks wheater destination had at least one adjacent tile in the same air cluster as origin.
*/
bool isSameDstAdjacentAirCluster(int chassisId, int speed, MAP *src, MAP *dst)
{
	int srcAirCluster = getAirCluster(chassisId, speed, src);
	robin_hood::unordered_flat_set<int> const &adjacentAirClusters = getAdjacentAirClusters(chassisId, speed, dst);
	return adjacentAirClusters.find(srcAirCluster) != adjacentAirClusters.end();
}

/**
Checks wheater destination had at least one adjacent tile in the same sea cluster as origin.
*/
bool isSameDstAdjacentSeaCluster(MAP *src, MAP *dst)
{
	int srcSeaCluster = getSeaCluster(src);
	robin_hood::unordered_flat_set<int> const &adjacentSeaClusters = getAdjacentSeaClusters(dst);
	return adjacentSeaClusters.find(srcSeaCluster) != adjacentSeaClusters.end();
}

/**
Checks wheater destination had at least one adjacent tile in the same landTransported cluster as origin.
*/
bool isSameDstAdjacentLandTransportedCluster(MAP *src, MAP *dst)
{
	int srcLandTransportedCluster = getLandTransportedCluster(src);
	robin_hood::unordered_flat_set<int> const &adjacentLandTransportedClusters = getAdjacentLandTransportedClusters(dst);
	return adjacentLandTransportedClusters.find(srcLandTransportedCluster) != adjacentLandTransportedClusters.end();
}

double getPathMovementCost(MAP *org, MAP *dst, int unitId, int factionId, bool impediment)
{
	assert(isOnMap(org));
	assert(isOnMap(dst));
	
	int x1 = getX(org);
	int y1 = getY(org);
	int x2 = getX(dst);
	int y2 = getY(dst);
	MovementType movementType = getUnitMovementType(factionId, unitId);
	
	std::vector<double> const *combatImpediments = nullptr;
	if (impediment)
	{
		switch (movementType)
		{
		case MT_SEA_REGULAR:
		case MT_SEA_NATIVE:
			combatImpediments = &(factionId == aiFactionId ? aiFactionMovementInfo.seaCombatImpediments : enemyFactionMovementInfos.at(factionId).seaCombatImpediments);
			break;
		
		case MT_LAND_REGULAR:
		case MT_LAND_NATIVE:
			combatImpediments = &(factionId == aiFactionId ? aiFactionMovementInfo.landCombatImpediments : enemyFactionMovementInfos.at(factionId).landCombatImpediments);
			break;
		
		default:
			combatImpediments = nullptr;
		
		}
	}
	
	double movementCost = 0.0;
	
	int x = x1;
	int y = y1;
	
	while (!(x == x2 && y == y2))
	{
		int angle = path_get_next(x, y, x2, y2, unitId, factionId);
		
		if (angle == -1)
			return INF;
		
		Location nextLocation = getLocationByAngle(x, y, angle);
		
		if (!nextLocation.isValid())
			return INF;
		
		int tileIndex = getMapTileIndex(x, y);
		TileInfo &tileInfo = aiData.getTileInfo(tileIndex);
		
		int hexCost = tileInfo.hexCosts.at(movementType).at(angle);
		
		if (hexCost == -1)
			return INF;
		
		movementCost += (double)hexCost;
		
		if (combatImpediments != nullptr)
		{
			movementCost += combatImpediments->at(tileIndex);
		}
		
		x = nextLocation.x;
		y = nextLocation.y;
		
	}
	
	return movementCost;
	
}

double getPathTravelTime(MAP *org, MAP *dst, int unitId, int factionId)
{
	int moveRate = getUnitMoveRate(factionId, unitId);
	
	if (moveRate <= 0)
		return INF;
	
	double pathMovementCost = getPathMovementCost(org, dst, unitId, factionId);
	
	if (pathMovementCost == INF)
		return INF;
	
	return pathMovementCost / (double)moveRate;
	
}

double getPathTravelTime(int vehicleId, MAP *dst)
{
	VEH *vehicle = &Vehicles[vehicleId];
	return getPathTravelTime(getVehicleMapTile(vehicleId), dst, vehicle->unit_id, vehicle->faction_id);
}

