
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

// [movementType][speed][orgIndex][dstIndex]
robin_hood::unordered_flat_map <long long, double> cachedATravelTimes;

// ==================================================
// Main operations
// ==================================================

void populateRouteData()
{
	cachedATravelTimes.clear();
	
	precomputeRouteData();
	
}

void precomputeRouteData()
{
	debug("precomputeRouteData - %s\n", MFactions[aiFactionId].noun_faction);
	
	executionProfiles["1.1.4. precomputeRouteData"].start();
	
	// enemy factions data
	
	for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
	{
		if (factionId == aiFactionId)
			continue;
		
		populateAirbases(factionId);
		populateAirClusters(factionId);
		populateSeaTransportClusters(factionId);
		populateSeaTransportWaitTimes(factionId);
		populateSeaCombatClusters(factionId);
		populateLandCombatClusters(factionId);
		populateImpediments(factionId);
		populateSeaApproachTimes(factionId);
		populateLandApproachTimes(factionId);
		
	}
	
	// player faction data
	
	populateAirbases(aiFactionId);
	populateAirClusters(aiFactionId);
	populateSeaTransportClusters(aiFactionId);
	populateSeaTransportWaitTimes(aiFactionId);
	populateSeaCombatClusters(aiFactionId);
	populateLandCombatClusters(aiFactionId);
	populateImpediments(aiFactionId);
	populateSeaApproachTimes(aiFactionId);
	populateLandApproachTimes(aiFactionId);
	
	populateSeaClusters(aiFactionId);
	populateLandClusters();
	populateTransfers(aiFactionId);
	populateLandTransportedClusters();
	populateReachableLocations();
	populateSharedSeaClusters();
	populateAdjacentClusters();
	
	executionProfiles["1.1.4. precomputeRouteData"].stop();
	
}

void populateAirbases(int factionId)
{
	debug("populateAirbases aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.1. populateAirbases"].start();
	
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
	
	executionProfiles["1.1.4.1. populateAirbases"].stop();
	
}

void populateAirClusters(int factionId)
{
	debug("populateAirClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.2. populateAirClusters"].start();
	
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
		int range = unit->range();
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
		int range = unit->range();
		int speed = getUnitSpeed(factionId, unitId);
		
		// this faction
		
		if (vehicle->faction_id != factionId)
			continue;
		
		// ranged air unit
		
		if (triad != TRIAD_AIR || range == 0)
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
	
	executionProfiles["1.1.4.2. populateAirClusters"].stop();
	
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
	
}

void populateSeaTransportClusters(int factionId)
{
	debug("populateSeaTransportClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.3. populateSeaTransportClusters"].start();
	
	std::vector<int> &seaTransportClusters = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportClusters : enemyFactionMovementInfos.at(factionId).seaTransportClusters);
	
	seaTransportClusters.resize(*MapAreaTiles); std::fill(seaTransportClusters.begin(), seaTransportClusters.end(), -1);
	
	// seaTransportLocations
	// existing or potential seaTransport locations
	
	robin_hood::unordered_flat_set<int> seaTransportTileIndexes;
	
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
		
		// set
		
		seaTransportTileIndexes.insert(getVehicleMapTile(vehicleId) - *MapTiles);
		
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
		
		// set
		
		seaTransportTileIndexes.insert(getBaseMapTile(baseId) - *MapTiles);
		
	}
	
//	if (DEBUG)
//	{
//		debug("\tshipyardBaseIds\n");
//		for (int baseId : shipyardBaseIds)
//		{
//			debug("\t\t%d\n", baseId);
//		}
//		
//	}
	
//	if (DEBUG)
//	{
//		debug("\tseaTransportTileIndexes\n");
//		for (int tileIndex : seaTransportTileIndexes)
//		{
//			debug("\t\t%s\n", getLocationString(*MapTiles + tileIndex).c_str());
//		}
//		
//	}
	
	// availableSeaTransportTileIndexes
	
	std::vector<bool> availableSeaTransportTileIndexes(*MapAreaTiles);
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		bool ocean = is_ocean(tile);
		
		// sea
		
		if (!ocean)
			continue;
		
		// set
		
		availableSeaTransportTileIndexes.at(tileIndex) = true;
		
	}
	
	// friendly poirts
	
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
		
		availableSeaTransportTileIndexes.at(baseTileIndex) = true;
		
	}
	
	// sea transport blocks
	// not dependent on aiData.tileInfos
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int vehicleTileIndex = vehicleTile - *MapTiles;
		bool ocean = is_ocean(vehicleTile);
		
		// unfriendly
		
		if (!isUnfriendly(factionId, vehicle->faction_id))
			continue;
		
		// sea
		
		if (!ocean)
			continue;
		
		// set
		
		availableSeaTransportTileIndexes.at(vehicleTileIndex) = false;
		
	}
	
//	if (DEBUG)
//	{
//		debug("\tavailableSeaTransportTileIndexes\n");
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			if (availableSeaTransportTileIndexes.at(tileIndex))
//			{
//				debug("\t\t%s\n", getLocationString(*MapTiles + tileIndex).c_str());
//			}
//			
//		}
//		
//	}
	
	// seaTransportClusters
	
	std::vector<int> borderTiles;
	std::vector<int> newBorderTiles;
	
	int clusterIndex = 0;
	
	for (int tileIndex : seaTransportTileIndexes)
	{
		// available
		
		assert(availableSeaTransportTileIndexes.at(tileIndex));
		
		// not yet assigned
		
		if (seaTransportClusters.at(tileIndex) != -1)
			continue;
		
		// set collections
		
		borderTiles.clear();
		newBorderTiles.clear();
		
		// insert initial tile
		
		seaTransportClusters.at(tileIndex) = clusterIndex;
		
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
					
					if (!availableSeaTransportTileIndexes.at(adjacentTileIndex))
						continue;
					
					// not yet assigned
					
					if (seaTransportClusters.at(adjacentTileIndex) != -1)
						continue;
					
					// insert adjacent tile
					
					seaTransportClusters.at(adjacentTileIndex) = clusterIndex;
					
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
	
	executionProfiles["1.1.4.3. populateSeaTransportClusters"].stop();
	
//	if (DEBUG)
//	{
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), seaTransportClusters.at(tileIndex));
//			
//		}
//		
//	}
	
}

void populateSeaTransportWaitTimes(int factionId)
{
	debug("populateSeaTransportWaitTimes aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.4. populateSeaTransportWaitTimes"].start();
	
	FactionInfo const &factionInfo = aiData.factionInfos.at(factionId);
	
	std::vector<int> const &seaTransportClusters = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportClusters : enemyFactionMovementInfos.at(factionId).seaTransportClusters);
	std::vector<double> &seaTransportWaitTimes = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportWaitTimes : enemyFactionMovementInfos.at(factionId).seaTransportWaitTimes);
	
	seaTransportWaitTimes.resize(*MapAreaTiles); std::fill(seaTransportWaitTimes.begin(), seaTransportWaitTimes.end(), INF);
	
	// seaTransports
	
	robin_hood::unordered_flat_set<int> seaTransportVehicleIds;
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		
		// this faction
		
		if (vehicle->faction_id != factionId)
			continue;
		
		// seaTransport
		
		if (!isSeaTransportVehicle(vehicleId))
			continue;
		
		// set
		
		seaTransportVehicleIds.insert(vehicleId);
		
	}
	
//	if (DEBUG)
//	{
//		debug("\tseaTransportVehicleIds\n");
//		for (int vehicleId : seaTransportVehicleIds)
//		{
//			debug("\t\t%d\n", vehicleId);
//		}
//		
//	}
	
	// shipyards
	
	robin_hood::unordered_flat_set<int> shipyardBaseIds;
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		
		// this faction
		
		if (base->faction_id != factionId)
			continue;
		
		// can build ship
		
		if (!isBaseCanBuildShip(baseId))
			continue;
		
		// set
		
		shipyardBaseIds.insert(baseId);
		
	}
	
//	if (DEBUG)
//	{
//		debug("\tshipyardBaseIds\n");
//		for (int baseId : shipyardBaseIds)
//		{
//			debug("\t\t%d\n", baseId);
//		}
//		
//	}
	
	// process sea transport and sea/port bases
	
	for (int vehicleId : seaTransportVehicleIds)
	{
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int vehicleTileIndex = vehicleTile - *MapTiles;
		int vehicleSeaTransportClusterIndex = seaTransportClusters.at(vehicleTileIndex);
		
		assert(vehicleSeaTransportClusterIndex != -1);
		
		int capacity = getTransportCapacity(vehicleId);
		int remainingCapacity = getTransportRemainingCapacity(vehicleId);
		int speed = getVehicleSpeed(vehicleId);
		
		// has speed
		
		if (speed <= 0)
			continue;
		
		// has capacity
		
		if (capacity <= 0)
			continue;
		
		// has remainingCapacity
		
		if (remainingCapacity <= 0)
			continue;
		
		// relative remaining capacity
		
		double remainingCapacityCoefficient = (double)capacity / (double)remainingCapacity;
		
		// process tiles
		
		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
		{
			MAP *tile = *MapTiles + tileIndex;
			int tileSeaTransportClusterIndex = seaTransportClusters.at(tileIndex);
			
			// existing seaTransportCluster
			
			if (tileSeaTransportClusterIndex == -1)
				continue;
			
			// same seaTransport cluster
			
			if (tileSeaTransportClusterIndex != vehicleSeaTransportClusterIndex)
				continue;
			
			// range
			
			int range = getRange(vehicleTile, tile);
			
			// seaTransportWaitTime
			
			double seaTransportWaitTime = SEA_TRANSPORT_WAIT_TIME_COEFFICIENT * remainingCapacityCoefficient * ((double)range / (double)speed);
			
			// update best
			
			seaTransportWaitTimes.at(tileIndex) = std::min(seaTransportWaitTimes.at(tileIndex), seaTransportWaitTime);
			
		}
		
	}
	
	if (factionInfo.bestSeaTransportUnitId != -1 && factionInfo.bestSeaTransportUnitSpeed > 0)
	{
		for (int baseId : shipyardBaseIds)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			int baseTileIndex = baseTile - *MapTiles;
			int baseSeaTransportClusterIndex = seaTransportClusters.at(baseTileIndex);
			
			assert(baseSeaTransportClusterIndex != -1);
			
			// process tiles
			
			for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
			{
				MAP *tile = *MapTiles + tileIndex;
				int tileSeaTransportClusterIndex = seaTransportClusters.at(tileIndex);
				
				// existing seaTransportCluster
				
				if (tileSeaTransportClusterIndex == -1)
					continue;
				
				// same seaTransport cluster
				
				if (tileSeaTransportClusterIndex != baseSeaTransportClusterIndex)
					continue;
				
				// build time
				
				int buildTime = getBaseItemBuildTime(baseId, factionInfo.bestSeaTransportUnitId);
				
				// range
				
				int range = getRange(baseTile, tile);
				
				// seaTransportWaitTime
				
				double seaTransportWaitTime = SEA_TRANSPORT_WAIT_TIME_COEFFICIENT * ((double)buildTime + ((double)range / (double)factionInfo.bestSeaTransportUnitSpeed));
				
				// update best
				
				seaTransportWaitTimes.at(tileIndex) = std::min(seaTransportWaitTimes.at(tileIndex), seaTransportWaitTime);
				
			}
			
		}
		
	}
	
	executionProfiles["1.1.4.4. populateSeaTransportWaitTimes"].stop();
	
	if (DEBUG)
	{
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int tileIndex = tile - *MapTiles;
			double seaTransportWaitTime = seaTransportWaitTimes.at(tileIndex);
			
			if (seaTransportWaitTime != INF)
			{
				debug("\t%s %5.2f\n", getLocationString(tile).c_str(), seaTransportWaitTime);
			}
			
		}
		
	}
	
}

void populateSeaCombatClusters(int factionId)
{
	debug("populateSeaCombatClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.5. populateSeaCombatClusters"].start();
	
	std::vector<int> &seaCombatClusters = (factionId == aiFactionId ? aiFactionMovementInfo.seaCombatClusters : enemyFactionMovementInfos.at(factionId).seaCombatClusters);
	
	seaCombatClusters.resize(*MapAreaTiles); std::fill(seaCombatClusters.begin(), seaCombatClusters.end(), -1);
	
	// availableTileIndexes
	
	std::vector<bool> availableTileIndexes(*MapAreaTiles);
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileIndex = tile - *MapTiles;
		bool ocean = is_ocean(tile);
		
		// sea
		
		if (ocean)
		{
			seaCombatClusters.at(tileIndex) = -2;
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
		
		seaCombatClusters.at(baseTileIndex) = -2;
		
	}
	
	// process tiles
	
	std::vector<int> borderTiles;
	std::vector<int> newBorderTiles;
	
	int clusterIndex = 0;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		// available
		
		if (seaCombatClusters.at(tileIndex) != -2)
			continue;
		
		// set collections
		
		borderTiles.clear();
		newBorderTiles.clear();
		
		// insert initial tile
		
		seaCombatClusters.at(tileIndex) = clusterIndex;
		
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
					
					if (seaCombatClusters.at(adjacentTileIndex) != -2)
						continue;
					
					// insert adjacent tile
					
					seaCombatClusters.at(adjacentTileIndex) = clusterIndex;
					
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
	
	executionProfiles["1.1.4.5. populateSeaCombatClusters"].stop();
	
//	if (DEBUG)
//	{
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), seaCombatClusters.at(tileIndex));
//			
//		}
//		
//	}
	
}

void populateLandCombatClusters(int factionId)
{
	debug("populateLandCombatClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.6. populateLandCombatClusters"].start();
	
	std::vector<int> const &seaTransportClusters = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportClusters : enemyFactionMovementInfos.at(factionId).seaTransportClusters);
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
		
		// or sea transport cluster
		
		if (seaTransportClusters.at(tileInfo.index) != -1)
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
	
	executionProfiles["1.1.4.6. populateLandCombatClusters"].stop();
	
//	if (DEBUG)
//	{
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), landCombatClusters.at(tileIndex));
//			
//		}
//		
//	}
	
}

/**
Computes estimated values on how much faction vehicles will be slowed down passing through territory occupied by unfriendly bases/vehicles.
*/
void populateImpediments(int factionId)
{
	debug("populateImpediments aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.7. populateImpediments"].start();
	
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
	
	executionProfiles["1.1.4.7. populateImpediments"].stop();
	
	if (DEBUG)
	{
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int tileIndex = tile - *MapTiles;
			
			debug("\t%s %5.2f %5.2f %5.2f\n", getLocationString(tile).c_str(), seaTransportImpediments.at(tileIndex), seaCombatImpediments.at(tileIndex), landCombatImpediments.at(tileIndex));
			
		}
		
	}
	
}

/**
Populates sea combat vehicle travel times to bases.
This is used for both 1) enemy vehicles -> player bases approach times, and 2) player vehicles -> enemy bases approach times
*/
void populateSeaApproachTimes(int factionId)
{
	debug("populateSeaApproachTimes aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.8. populateSeaApproachTimes"].start();
	
	bool enemy = (factionId != aiFactionId);
	
	std::vector<int> const &seaCombatClusters = (factionId == aiFactionId ? aiFactionMovementInfo.seaCombatClusters : enemyFactionMovementInfos.at(factionId).seaCombatClusters);
	std::vector<double> const &seaCombatImpediments = (factionId == aiFactionId ? aiFactionMovementInfo.seaCombatImpediments : enemyFactionMovementInfos.at(factionId).seaCombatImpediments);
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>> &seaApproachTimes =
		(factionId == aiFactionId ? aiFactionMovementInfo.seaApproachTimes : enemyFactionMovementInfos.at(factionId).seaApproachTimes);
	
	seaApproachTimes.clear();
	
	// collect sea bases
	
	std::vector<int> seaBaseIds;
	
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
		
		// sea
		
		if (!baseTileInfo.ocean)
			continue;
		
		seaBaseIds.push_back(baseId);
		
	}
	
//	if (DEBUG)
//	{
//		debug("seaBases\n");
//		
//		for (int baseId : seaBaseIds)
//		{
//			debug("\t[%3d] %s %-24s\n", baseId, getLocationString(getBaseMapTile(baseId)).c_str(), getBase(baseId)->name);
//		}
//		
//	}
	
	// collect sea speeds by movement type
	
	std::map<MovementType, std::set<int>> speeds;
	
	for (int unitId : getFactionUnitIds(factionId, false, true))
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		
		// sea
		
		if (triad != TRIAD_SEA)
			continue;
		
		// combat
		
		if (!isCombatUnit(unitId))
			continue;
		
		// movementType
		
		MovementType movementType = getUnitMovementType(factionId, unitId);
		
		// speed
		
		int speed = getUnitSpeed(factionId, unitId);
		
		if (speed <= 0)
			continue;
		
		// collect
		
		speeds[movementType].insert(speed);
		
	}
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		
		// this faction (either enemy or player)
		
		if (vehicle->faction_id != factionId)
			continue;
		
		// sea
		
		if (triad != TRIAD_SEA)
			continue;
		
		// combat
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		// movementType
		
		MovementType movementType = getVehicleMovementType(vehicleId);
		
		// speed
		
		int speed = getVehicleSpeed(vehicleId);
		
		if (speed <= 0)
			continue;
		
		// collect
		
		speeds[movementType].insert(speed);
		
	}
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		
		// this faction (either enemy or player)
		
		if (vehicle->faction_id != factionId)
			continue;
		
		// sea
		
		if (triad != TRIAD_SEA)
			continue;
		
		// combat
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		// movementType
		
		MovementType movementType = getVehicleMovementType(vehicleId);
		
		// speed
		
		int speed = getVehicleSpeed(vehicleId);
		
		if (speed <= 0)
			continue;
		
		// collect
		
		speeds[movementType].insert(speed);
		
	}
	
//	if (DEBUG)
//	{
//		debug("speeds\n");
//		
//		for (std::pair<MovementType, std::set<int>> const &vehicleSpeedEntry : speeds)
//		{
//			MovementType const &movementType = vehicleSpeedEntry.first;
//			std::set<int> const &movementTypeVehicleSpeeds = vehicleSpeedEntry.second;
//			
//			debug("\tmovementType=%d\n", movementType);
//			
//			for (int speed : movementTypeVehicleSpeeds)
//			{
//				debug("\t\tspeed=%2d\n", speed);
//			}
//			
//		}
//		
//	}
	
	// process bases
	
	std::vector<int> borderTiles;
	std::vector<int> newBorderTiles;

	for (int baseId : seaBaseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		int baseTileIndex = baseTile - *MapTiles;
		int baseSeaCombatClusterIndex = seaCombatClusters.at(baseTileIndex);
		
		// no vehicle presense
		
		if (baseSeaCombatClusterIndex == -1)
			continue;
		
		// create base approachTimes
		
		seaApproachTimes.emplace(baseId, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>());
		
		for (std::pair<MovementType, std::set<int>> const &vehicleSpeedEntry : speeds)
		{
			MovementType movementType = vehicleSpeedEntry.first;
			std::set<int> const &movementTypeVehicleSpeeds = vehicleSpeedEntry.second;
			
			seaApproachTimes.at(baseId).emplace(movementType, robin_hood::unordered_flat_map<int, std::vector<double>>());
			
			for (int speed : movementTypeVehicleSpeeds)
			{
				// resize speed approachTimes
				
				seaApproachTimes.at(baseId).at(movementType).emplace(speed, std::vector<double>(*MapAreaTiles));
				std::vector<double> &approachTimes = seaApproachTimes.at(baseId).at(movementType).at(speed);
				std::fill(approachTimes.begin(), approachTimes.end(), INF);
				
				// set running variables
				
				borderTiles.clear();
				newBorderTiles.clear();
				
				robin_hood::unordered_flat_set<int> utilizedOtherBaseIds;
				
				// insert initial tile
				
				for (MAP *baseAdjacentTile : getBaseAdjacentTiles(baseTile, true))
				{
					int baseAdjacentTileIndex = baseAdjacentTile - *MapTiles;
					int baseAdjacentTileSeaCombatClusterIndex = seaCombatClusters.at(baseAdjacentTileIndex);
					
					// within cluster
					
					if (baseAdjacentTileSeaCombatClusterIndex != baseSeaCombatClusterIndex)
						continue;
					
					// add to border tiles
					
					approachTimes.at(baseAdjacentTileIndex) = 0.0;
					borderTiles.push_back(baseAdjacentTileIndex);
					
				}
				
				// iterate tiles
				
				while (!borderTiles.empty())
				{
					for (int currentTileIndex : borderTiles)
					{
						TileInfo &currentTileInfo = aiData.tileInfos[currentTileIndex];
						
						double currentTileApproachTime = approachTimes.at(currentTileIndex);
						
						for (MapAngle mapAngle : currentTileInfo.adjacentMapAngles)
						{
							MAP *adjacentTile = mapAngle.tile;
							int angle = mapAngle.angle;
							int adjacentTileIndex = adjacentTile - *MapTiles;
							
							int adjacentTileSeaCombatClusterIndex = seaCombatClusters.at(adjacentTileIndex);
							
							// within cluster
							
							if (adjacentTileSeaCombatClusterIndex != baseSeaCombatClusterIndex)
								continue;
							
							// tile info and opposite angle
							
							TileInfo &adjacentTileInfo = aiData.tileInfos[adjacentTileIndex];
							int opppositeAngle = (angle + ANGLE_COUNT / 2) % ANGLE_COUNT;
							
							int hexCost = adjacentTileInfo.hexCosts[movementType][opppositeAngle];
							
							// sensible hexCost
							
							if (hexCost == -1)
								continue;
							
							// stepCost with impediment
							
							double stepCost = (double)hexCost + seaCombatImpediments.at(currentTileIndex);
							
							// stepTime
							
							double stepTime = stepCost / (double)(Rules->move_rate_roads * speed);
							
							// update adjacentTileApproachTime
							
							double adjacentTileApproachTime = currentTileApproachTime + stepTime;
							
							// update best
							
							if (adjacentTileApproachTime < approachTimes.at(adjacentTileIndex))
							{
								approachTimes.at(adjacentTileIndex) = adjacentTileApproachTime;
								newBorderTiles.push_back(adjacentTileIndex);
							}
							
							// update information from other bases at first hit
							
							if (adjacentTileInfo.base && seaApproachTimes.find(adjacentTileInfo.baseId) != seaApproachTimes.end() && utilizedOtherBaseIds.find(adjacentTileInfo.baseId) == utilizedOtherBaseIds.end())
							{
								utilizedOtherBaseIds.insert(adjacentTileInfo.baseId);
								
								std::vector<double> const &otherBaseApproachTimes = seaApproachTimes.at(adjacentTileInfo.baseId).at(movementType).at(speed);
								
								for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
								{
									double throughOtherbaseApproachTime = otherBaseApproachTimes.at(tileIndex) + adjacentTileApproachTime;
									approachTimes.at(tileIndex) = std::min(approachTimes.at(tileIndex), throughOtherbaseApproachTime);
								}
								
							}
							
						}
						
					}
					
					// swap border tiles with newBorderTiles and clear them
					
					borderTiles.swap(newBorderTiles);
					newBorderTiles.clear();
					
				}
				
			}
			
		}
		
	}
	
//	if (DEBUG)
//	{
//		for (robin_hood::pair<int, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>> const &seaApproachTimeEntry : seaApproachTimes)
//		{
//			int baseId = seaApproachTimeEntry.first;
//			robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>> const &baseSeaApproachTimes = seaApproachTimes.at(baseId);
//			
//			debug("\t%s\n", getBase(baseId)->name);
//			
//			for (robin_hood::pair<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>> const &baseSeaApproachTimeEntry : baseSeaApproachTimes)
//			{
//				MovementType movementType = baseSeaApproachTimeEntry.first;
//				robin_hood::unordered_flat_map<int, std::vector<double>> const &baseMovementTypeSeaApproachTimes = baseSeaApproachTimeEntry.second;
//				
//				debug("\t\tmovementType=%d\n", movementType);
//				
//				for (robin_hood::pair<int, std::vector<double>> const &baseMovementTypeSeaApproachTimeEntry : baseMovementTypeSeaApproachTimes)
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
	
	executionProfiles["1.1.4.8. populateSeaApproachTimes"].stop();
	
}

/**
Populates enamy land combat vehicle travel times to player bases.
*/
void populateLandApproachTimes(int factionId)
{
	debug("populateLandApproachTimes aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.9. populateLandApproachTimes"].start();
	
	bool enemy = (factionId != aiFactionId);
	FactionInfo const &factionInfo = aiData.factionInfos.at(factionId);
	
	std::vector<double> const &seaTransportWaitTimes = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportWaitTimes : enemyFactionMovementInfos.at(factionId).seaTransportWaitTimes);
	std::vector<int> const &landCombatClusters = (factionId == aiFactionId ? aiFactionMovementInfo.landCombatClusters : enemyFactionMovementInfos.at(factionId).landCombatClusters);
	std::vector<double> const &seaTransportImpediments = (factionId == aiFactionId ? aiFactionMovementInfo.seaTransportImpediments : enemyFactionMovementInfos.at(factionId).seaTransportImpediments);
	std::vector<double> const &landCombatImpediments = (factionId == aiFactionId ? aiFactionMovementInfo.landCombatImpediments : enemyFactionMovementInfos.at(factionId).landCombatImpediments);
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>> &landApproachTimes =
		(factionId == aiFactionId ? aiFactionMovementInfo.landApproachTimes : enemyFactionMovementInfos.at(factionId).landApproachTimes);
	
	landApproachTimes.clear();
	
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
	
	// collect enemy land speeds by movement type
	
	std::map<MovementType, std::set<int>> speeds;
	
	for (int unitId : getFactionUnitIds(factionId, false, true))
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		
		// land
		
		if (triad != TRIAD_LAND)
			continue;
		
		// combat
		
		if (!isCombatUnit(unitId))
			continue;
		
		// movementType
		
		MovementType movementType = getUnitMovementType(factionId, unitId);
		
		// speed
		
		int speed = getUnitSpeed(factionId, unitId);
		
		if (speed <= 0)
			continue;
		
		// collect
		
		speeds[movementType].insert(speed);
		
	}
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		
		// this faction (either enemy or player)
		
		if (vehicle->faction_id != factionId)
			continue;
		
		// land
		
		if (triad != TRIAD_LAND)
			continue;
		
		// combat
		
		if (!isCombatVehicle(vehicleId))
			continue;
		
		// movementType
		
		MovementType movementType = getVehicleMovementType(vehicleId);
		
		// speed
		
		int speed = getVehicleSpeed(vehicleId);
		
		if (speed <= 0)
			continue;
		
		// collect
		
		speeds[movementType].insert(speed);
		
	}
	
	if (DEBUG)
	{
		debug("speeds\n");
		
		for (std::pair<MovementType, std::set<int>> const &vehicleSpeedEntry : speeds)
		{
			MovementType const &movementType = vehicleSpeedEntry.first;
			std::set<int> const &movementTypeVehicleSpeeds = vehicleSpeedEntry.second;
			
			debug("\tmovementType=%d\n", movementType);
			
			for (int speed : movementTypeVehicleSpeeds)
			{
				debug("\t\tspeed=%2d\n", speed);
			}
			
		}
		
	}
	
	// process land bases
	
	std::vector<int> borderTiles;
	std::vector<int> newBorderTiles;

	for (int baseId : landBaseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		int baseTileIndex = baseTile - *MapTiles;
		int baseLandCombatClusterIndex = landCombatClusters.at(baseTileIndex);
		
		// no enemy presense
		
		if (baseLandCombatClusterIndex == -1)
			continue;
		
		// create base approachTimes
		
		landApproachTimes.emplace(baseId, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>());
		
		for (std::pair<MovementType, std::set<int>> const &vehicleSpeedEntry : speeds)
		{
			MovementType const &movementType = vehicleSpeedEntry.first;
			std::set<int> const &movementTypeVehicleSpeeds = vehicleSpeedEntry.second;
			
			landApproachTimes.at(baseId).emplace(movementType, robin_hood::unordered_flat_map<int, std::vector<double>>());
			
			for (int speed : movementTypeVehicleSpeeds)
			{
				// initialize speed approachTimes
				
				landApproachTimes.at(baseId).at(movementType).emplace(speed, std::vector<double>(*MapAreaTiles));
				std::vector<double> &approachTimes = landApproachTimes.at(baseId).at(movementType).at(speed);
				std::fill(approachTimes.begin(), approachTimes.end(), INF);
				
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
					
					approachTimes.at(baseAdjacentTileIndex) = 0.0;
					borderTiles.push_back(baseAdjacentTileIndex);
					
				}
				
				// iterate tiles
				
				while (!borderTiles.empty())
				{
					for (int currentTileIndex : borderTiles)
					{
						TileInfo &currentTileInfo = aiData.tileInfos[currentTileIndex];
						
						double currentTileApproachTime = approachTimes.at(currentTileIndex);
						
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
							
							double stepTime = 1.0;
							
							if (adjacentTileInfo.landAllowed && currentTileInfo.landAllowed) // land-land
							{
								int hexCost = adjacentTileInfo.hexCosts[movementType][opppositeAngle];
								
								// sensible hexCost
								
								if (hexCost == -1)
									continue;
								
								// stepCost with impediment
								
								double stepCost = (double)hexCost + landCombatImpediments.at(currentTileIndex);
								
								// stepTime
								
								stepTime = stepCost / (double)(Rules->move_rate_roads * speed);
								
							}
							else if (!adjacentTileInfo.landAllowed && !currentTileInfo.landAllowed) // sea-sea transport moement
							{
								int hexCost = adjacentTileInfo.hexCosts[MT_SEA_REGULAR][opppositeAngle];
								
								// sensible hexCost
								
								if (hexCost == -1)
									continue;
								
								// add impediment
								
								double stepCost = (double)hexCost + seaTransportImpediments.at(currentTileIndex);
								
								// stepTime
								
								stepTime = stepCost / (double)(Rules->move_rate_roads * factionInfo.bestSeaTransportUnitSpeed);
								
							}
							else if (adjacentTileInfo.landAllowed && !currentTileInfo.landAllowed) // boarding
							{
								// seaTransportWaitTime
								
								double seaTransportWaitTime = seaTransportWaitTimes.at(currentTileIndex);
								
								if (seaTransportWaitTime == INF)
									continue;
								
								stepTime = seaTransportWaitTime;
								
							}
							else if (!adjacentTileInfo.landAllowed && currentTileInfo.landAllowed) // unboarding
							{
								double stepCost = (double)Rules->move_rate_roads;
								
								// stepTime
								
								stepTime = stepCost / (double)(Rules->move_rate_roads * speed);
								
							}
							
							// update adjacentTileApproachTime
							
							double adjacentTileApproachTime = currentTileApproachTime + stepTime;
							
							// update best
							
							if (adjacentTileApproachTime < approachTimes.at(adjacentTileIndex))
							{
								approachTimes.at(adjacentTileIndex) = adjacentTileApproachTime;
								newBorderTiles.push_back(adjacentTileIndex);
							}
							
							// update information from other bases at first hit
							
							if (adjacentTileInfo.base && landApproachTimes.find(adjacentTileInfo.baseId) != landApproachTimes.end() && utilizedOtherBaseIds.find(adjacentTileInfo.baseId) == utilizedOtherBaseIds.end())
							{
								utilizedOtherBaseIds.insert(adjacentTileInfo.baseId);
								
								std::vector<double> const &otherBaseApproachTimes = landApproachTimes.at(adjacentTileInfo.baseId).at(movementType).at(speed);
								
								for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
								{
									double throughOtherbaseApproachTime = otherBaseApproachTimes.at(tileIndex) + adjacentTileApproachTime;
									approachTimes.at(tileIndex) = std::min(approachTimes.at(tileIndex), throughOtherbaseApproachTime);
								}
								
							}
							
						}
						
					}
					
					// swap border tiles with newBorderTiles and clear them
					
					borderTiles.swap(newBorderTiles);
					newBorderTiles.clear();
					
				}
				
			}
			
		}
		
	}
	
	if (DEBUG)
	{
		for (robin_hood::pair<int, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>> const &landApproachTimeEntry : landApproachTimes)
		{
			int baseId = landApproachTimeEntry.first;
			robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>> const &baseLandApproachTimes = landApproachTimes.at(baseId);
			
			debug("\t%s\n", getBase(baseId)->name);
			
			for (robin_hood::pair<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>> const &baseLandApproachTimeEntry : baseLandApproachTimes)
			{
				MovementType movementType = baseLandApproachTimeEntry.first;
				robin_hood::unordered_flat_map<int, std::vector<double>> const &baseMovementTypeLandApproachTimes = baseLandApproachTimeEntry.second;
				
				debug("\t\tmovementType=%d\n", movementType);
				
				for (robin_hood::pair<int, std::vector<double>> const &baseMovementTypeLandApproachTimeEntry : baseMovementTypeLandApproachTimes)
				{
					int speed = baseMovementTypeLandApproachTimeEntry.first;
					std::vector<double> const &approachTimes = baseMovementTypeLandApproachTimeEntry.second;
					
					debug("\t\t\tspeed=%d\n", speed);
					
					for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
					{
						int tileIndex = tile - *MapTiles;
						
						double approachTime = approachTimes.at(tileIndex);
						
						if (approachTime != INF)
						{
							debug("\t\t\t\t%s %5.2f\n", getLocationString(tile).c_str(), approachTime);
						}
						
					}
					
				}
					
			}
			
		}
		
	}
	
	executionProfiles["1.1.4.9. populateLandApproachTimes"].stop();
	
}

void populateSeaClusters(int factionId)
{
	debug("populateSeaClusters aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.5. populateSeaClusters"].start();
	
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
	
	executionProfiles["1.1.4.5. populateSeaClusters"].stop();
	
	if (DEBUG)
	{
		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
		{
			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), seaClusters.at(tileIndex));
		}
		
	}
	
}

void populateLandClusters()
{
	debug("populateLandClusters aiFactionId=%d\n", aiFactionId);
	
	executionProfiles["1.1.4.6. populateLandClusters"].start();
	
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
	
	executionProfiles["1.1.4.6. populateLandClusters"].stop();
	
	if (DEBUG)
	{
		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
		{
			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), landClusters.at(tileIndex));
		}
		
	}
	
}

void populateLandTransportedClusters()
{
	debug("populateLandTransportedClusters aiFactionId=%d\n", aiFactionId);
	
	executionProfiles["1.1.4.6. populateLandTransportedClusters"].start();
	
	std::vector<int> const &seaTransportClusters = aiFactionMovementInfo.seaTransportClusters;
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
		
		// or sea transport cluster
		
		if (seaTransportClusters.at(tileIndex) != -1)
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
	
	executionProfiles["1.1.4.6. populateLandTransportedClusters"].stop();
	
//	if (DEBUG)
//	{
//		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
//		{
//			debug("\t%s %2d\n", getLocationString(*MapTiles + tileIndex).c_str(), landCombatClusters.at(tileIndex));
//			
//		}
//		
//	}
	
}

void populateTransfers(int factionId)
{
	debug("populateTransfers aiFactionId=%d factionId=%d\n", aiFactionId, factionId);
	
	executionProfiles["1.1.4.7. populateTransfers"].start();
	
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

	executionProfiles["1.1.4.7. populateTransfers"].stop();
	
}

void populateReachableLocations()
{
	debug("populateReachableLocations aiFactionId=%d\n", aiFactionId);
	
	executionProfiles["1.1.4.7. populateTransfers"].start();
	
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
	
}

void populateSharedSeaClusters()
{
	debug("populateSharedSeaClusters aiFactionId=%d\n", aiFactionId);
	
	executionProfiles["1.1.4.7. populateTransfers"].start();
	
	std::set<int> &sharedSeaClusters = aiFactionMovementInfo.sharedSeaClusters;
	
	sharedSeaClusters.clear();
	
	// process vehicles
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();
		
		// not ours
		
		if (vehicle->faction_id == aiFactionId)
			continue;
		
		// sea
		
		if (triad != TRIAD_SEA)
			continue;
		
		// seaCluster
		
		int seaCluster = getSeaCluster(vehicleTile);
		
		if (seaCluster == -1)
			continue;
		
		sharedSeaClusters.insert(seaCluster);
		
	}
	
	// process bases
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		MAP *baseTile = getBaseMapTile(baseId);
		TileInfo &baseTileInfo = aiData.getBaseTileInfo(baseId);
		
		// not ours
		
		if (base->faction_id == aiFactionId)
			continue;
		
		// sea
		
		if (!baseTileInfo.ocean)
			continue;
		
		// seaCluster
		
		int seaCluster = getSeaCluster(baseTile);
		
		if (seaCluster == -1)
			continue;
		
		sharedSeaClusters.insert(seaCluster);
		
	}
	
}

void populateAdjacentClusters()
{
	debug("populateAdjacentClusters aiFactionId=%d\n", aiFactionId);
	
	executionProfiles["1.1.4.7. populateTransfers"].start();
	
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
	
}

/**
Computes enemy combat vehicle travel time to our bases.
*/
double getEnemyApproachTime(int baseId, int vehicleId)
{
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
		approachTime = getEnemySeaApproachTime(baseId, vehicleId);
		break;
	case CHS_HOVERTANK:
	case CHS_SPEEDER:
	case CHS_INFANTRY:
		approachTime = getEnemyLandApproachTime(baseId, vehicleId);
		break;
	}
	
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
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	
	assert(vehicle->triad() == TRIAD_SEA);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>> const &seaApproachTimes = enemyFactionMovementInfos.at(factionId).seaApproachTimes;
	
	// no approach time for this base - sea unit is not in base sea cluster
	
	if (seaApproachTimes.find(baseId) == seaApproachTimes.end())
		return INF;
	
	// return sea approach time
	
	MovementType movementType = getVehicleMovementType(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int vehicleTileIndex = vehicleTile - *MapTiles;
	int speed = getVehicleSpeed(vehicleId);
	
	return seaApproachTimes.at(baseId).at(movementType).at(speed).at(vehicleTileIndex);
	
}

/**
Computes enemy combat vehicle travel time to player base.
*/
double getEnemyLandApproachTime(int baseId, int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	
	assert(vehicle->triad() == TRIAD_LAND);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>> const &landApproachTimes = enemyFactionMovementInfos.at(factionId).landApproachTimes;
	
	// no approach time for this base - land unit is not in base land cluster
	
	if (landApproachTimes.find(baseId) == landApproachTimes.end())
		return INF;
	
	// return land approach time
	
	MovementType movementType = getVehicleMovementType(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int vehicleTileIndex = vehicleTile - *MapTiles;
	int speed = getVehicleSpeed(vehicleId);
	
	return landApproachTimes.at(baseId).at(movementType).at(speed).at(vehicleTileIndex);
	
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
		approachTime = getPlayerLandApproachTime(baseId, unitId, origin);
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
	int speed = getUnitSpeed(aiFactionId, unitId);
	int originTileIndex = origin - *MapTiles;
	
	assert(unit->triad() == TRIAD_SEA);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>> const &seaApproachTimes = aiFactionMovementInfo.seaApproachTimes;
	
	// no approach time for this base - sea unit is not in base sea cluster
	
	if (seaApproachTimes.find(baseId) == seaApproachTimes.end())
		return INF;
	
	// return sea approach time
	
	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
	
	return seaApproachTimes.at(baseId).at(movementType).at(speed).at(originTileIndex);
	
}

/**
Computes player combat vehicle travel time to enemy base.
*/
double getPlayerLandApproachTime(int baseId, int unitId, MAP *origin)
{
	UNIT *unit = getUnit(unitId);
	int speed = getUnitSpeed(aiFactionId, unitId);
	int originTileIndex = origin - *MapTiles;
	
	assert(unit->triad() == TRIAD_LAND);
	
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, robin_hood::unordered_flat_map<int, std::vector<double>>>> const &landApproachTimes = aiFactionMovementInfo.landApproachTimes;
	
	// no approach time for this base - land unit is not in base land cluster
	
	if (landApproachTimes.find(baseId) == landApproachTimes.end())
		return INF;
	
	// return land approach time
	
	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
	
	return landApproachTimes.at(baseId).at(movementType).at(speed).at(originTileIndex);
	
}

// ==================================================
// Generic, not path related travel computations
// ==================================================

double getGravshipUnitTravelTime(int /*unitId*/, int speed, MAP *org, MAP *dst)
{
	int range = getRange(org, dst);
	
	return (double)range / (double)speed;
	
}

double getGravshipUnitTravelTime(int unitId, MAP *org, MAP *dst)
{
	int speed = getUnitSpeed(aiFactionId, unitId);
	
	return getGravshipUnitTravelTime(unitId, speed, org, dst);
	
}

double getGravshipVehicleTravelTime(int vehicleId, MAP *org, MAP *dst)
{
	VEH *vehicle = getVehicle(vehicleId);
	int unitId = vehicle->unit_id;
	int speed = getVehicleSpeed(vehicleId);
	
	return getGravshipUnitTravelTime(unitId, speed, org, dst);
	
}

double getAirRangedUnitTravelTime(int unitId, int speed, MAP *org, MAP *dst)
{
	UNIT *unit = getUnit(unitId);
	int chassisId = unit->chassis_id;
	
	// same airCluster
	
	if (!isSameAirCluster(chassisId, speed, org, dst))
		return INF;
	
	// travel time
	
	int range = getRange(org, dst);
	return RANGED_AIR_TRAVEL_TIME_COEFFICIENT * (double)range / (double)speed;
	
}

double getAirRangedUnitTravelTime(int unitId, MAP *org, MAP *dst)
{
	int speed = getUnitSpeed(aiFactionId, unitId);
	return getAirRangedUnitTravelTime(unitId, speed, org, dst);
}

double getAirRangedVehicleTravelTime(int vehicleId, MAP *org, MAP *dst)
{
	VEH *vehicle = getVehicle(vehicleId);
	int unitId = vehicle->unit_id;
	int speed = getVehicleSpeed(vehicleId);
	
	return getAirRangedUnitTravelTime(unitId, speed, org, dst);
	
}

// ==================================================
// A* cached movement costs
// ==================================================

double getCachedATravelTime(MovementType movementType, int speed, int orgIndex, int dstIndex)
{
	assert(speed < 256);
	
//	executionProfiles["| getATravelTime - cachedATravelTime::get"].start();
	
	long long key =
		+ (long long)movementType	* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles) * 256
		+ (long long)speed			* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles)
		+ (long long)orgIndex		* (long long)(*MapAreaTiles)
		+ (long long)dstIndex
	;
	
	robin_hood::unordered_flat_map <long long, double>::iterator cachedATravelTimeIterator = cachedATravelTimes.find(key);
	
	if (cachedATravelTimeIterator == cachedATravelTimes.end())
		return INF;
	
//	executionProfiles["| getATravelTime - cachedATravelTime::get"].stop();
	
	return cachedATravelTimeIterator->second;
	
}

void setCachedATravelTime(MovementType movementType, int speed, int orgIndex, int dstIndex, double travelTime)
{
	assert(speed < 256);
	
//	executionProfiles["| getATravelTime - cachedATravelTime::set"].start();
	
	long long key =
		+ (long long)movementType	* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles) * 256
		+ (long long)speed			* (long long)(*MapAreaTiles) * (long long)(*MapAreaTiles)
		+ (long long)orgIndex		* (long long)(*MapAreaTiles)
		+ (long long)dstIndex
	;
	
	cachedATravelTimes[key] = travelTime;
	
//	executionProfiles["| getATravelTime - cachedATravelTime::set"].stop();
	
}

// ==================================================
// A* path finding algorithm
// ==================================================

/**
Computes A* travel time for *player* faction.
*/
double getATravelTime(MovementType movementType, int speed, MAP *org, MAP *dst)
{
//	debug("getATravelTime movementType=%d speed=%d %s->%s\n", movementType, speed, getLocationString(org).c_str(), getLocationString(dst).c_str());
	
	executionProfiles["| getATravelTime"].start();
	
	int orgIndex = org - *MapTiles;
	int dstIndex = dst - *MapTiles;
	
	// at dst
	
	if (org == dst)
	{
		executionProfiles["| getATravelTime"].stop();
		return 0.0;
	}
	
	// org cluster
	
	int orgCluster = -1;
	
	switch (movementType)
	{
	case MT_AIR:
		
		// no cluster restriction
		
		break;
		
	case MT_SEA_REGULAR:
	case MT_SEA_NATIVE:
		
		orgCluster = getSeaCluster(org);
		
		break;
		
	case MT_LAND_REGULAR:
	case MT_LAND_NATIVE:
		
		orgCluster = getLandTransportedCluster(org);
		
		break;
		
	}
	
	// check cached value
	
	double cachedTravelTime = getCachedATravelTime(movementType, speed, orgIndex, dstIndex);
	
	if (cachedTravelTime != INF)
	{
		executionProfiles["| getATravelTime"].stop();
		return cachedTravelTime;
	}
	
	// initialize collections
	
	std::priority_queue<FValue, std::vector<FValue>, FValueComparator> openNodes;
	
	setCachedATravelTime(movementType, speed, orgIndex, orgIndex, 0.0);
	double orgF = getRouteVectorDistance(org, dst);
	openNodes.push({orgIndex, orgF});
	
	// process path
	
	while (!openNodes.empty())
	{
		// get top node and remove it from list

		FValue currentOpenNode = openNodes.top();
		openNodes.pop();
		
		// get current tile data
		
		int currentTileIndex = currentOpenNode.tileIndex;
		TileInfo &currentTileInfo = aiData.getTileInfo(currentTileIndex);
		
		double currentTileTravelTime = getCachedATravelTime(movementType, speed, orgIndex, currentTileIndex);
		
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
					if (getSeaCluster(adjacentTile) != orgCluster)
						continue;
				}
				
				break;
				
			case MT_LAND_REGULAR:
			case MT_LAND_NATIVE:
				{
					if (getLandTransportedCluster(adjacentTile) != orgCluster)
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
			
			double cachedAdjacentTileTravelTime = getCachedATravelTime(movementType, speed, orgIndex, adjacentTileIndex);
			
			// update adjacentTile cachedTravelTime
			
			if (cachedAdjacentTileTravelTime == INF || travelTime < cachedAdjacentTileTravelTime)
			{
				setCachedATravelTime(movementType, speed, orgIndex, adjacentTileIndex, travelTime);
				
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
	
	double travelTime = getCachedATravelTime(movementType, speed, orgIndex, dstIndex);
	
	executionProfiles["| getATravelTime"].stop();
	
	return travelTime;
	
}

double getUnitATravelTime(int unitId, int speed, MAP *org, MAP *dst)
{
	UNIT *unit = getUnit(unitId);
	int chassisId = unit->chassis_id;
	
	if (speed <= 0)
		return INF;
	
	double travelTime = INF;
	
	switch (chassisId)
	{
	case CHS_GRAVSHIP:
		travelTime = getGravshipUnitTravelTime(unitId, speed, org, dst);
		break;
	case CHS_NEEDLEJET:
	case CHS_COPTER:
	case CHS_MISSILE:
		travelTime = getAirRangedUnitTravelTime(unitId, speed, org, dst);
		break;
	case CHS_FOIL:
	case CHS_CRUISER:
		travelTime = getSeaUnitATravelTime(unitId, speed, org, dst);
		break;
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		travelTime = getLandUnitATravelTime(unitId, speed, org, dst);
		break;
	}
	
	return travelTime;
	
}

double getUnitATravelTime(int unitId, MAP *org, MAP *dst)
{
	int speed = getUnitSpeed(aiFactionId, unitId);
	
	return getUnitATravelTime(unitId, speed, org, dst);
	
}

double getSeaUnitATravelTime(int unitId, int speed, MAP *org, MAP *dst)
{
	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
	
	// possible travel
	
	if (!isSameSeaCluster(org, dst))
		return INF;
	
	// travelTime
	
	return getATravelTime(movementType, speed, org, dst);
	
}

double getLandUnitATravelTime(int unitId, int speed, MAP *org, MAP *dst)
{
	MovementType movementType = getUnitMovementType(aiFactionId, unitId);
	
	// possible travel within land transported cluster
	
	if (!isSameLandTransportedCluster(org, dst))
		return INF;
	
	// travelTime
	
	return getATravelTime(movementType, speed, org, dst);
	
}

double getVehicleATravelTime(int vehicleId, MAP *org, MAP *dst)
{
	VEH *vehicle = getVehicle(vehicleId);
	int unitId = vehicle->unit_id;
	int speed = getVehicleSpeed(vehicleId);
	
	return getUnitATravelTime(unitId, speed, org, dst);
	
}

double getVehicleATravelTime(int vehicleId, MAP *dst)
{
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	return getVehicleATravelTime(vehicleId, vehicleTile, dst);
	
}

// ============================================================
// Reachability
// ============================================================

bool isUnitDestinationReachable(int unitId, MAP *org, MAP *dst)
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
		reachable = isSameAirCluster(chassisId, speed, org, dst);
		break;
	case CHS_FOIL:
	case CHS_CRUISER:
		reachable = isSameSeaCluster(org, dst);
		break;
	case CHS_INFANTRY:
	case CHS_SPEEDER:
	case CHS_HOVERTANK:
		reachable = isSameLandTransportedCluster(org, dst);
		break;
	}
	
	return reachable;
	
}

bool isVehicleDestinationReachable(int vehicleId, MAP *org, MAP *dst)
{
	VEH *vehicle = getVehicle(vehicleId);
	int unitId = vehicle->unit_id;
	
	return isUnitDestinationReachable(unitId, org, dst);
	
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

bool isSameAirCluster(int chassisId, int speed, MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1AirCluster = getAirCluster(chassisId, speed, tile1);
	int tile2AirCluster = getAirCluster(chassisId, speed, tile2);
	
	return tile1AirCluster != -1 && tile2AirCluster != -1 && tile1AirCluster == tile2AirCluster;
	
}

int getSeaCluster(MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return aiFactionMovementInfo.seaClusters.at(tileIndex);
	
}

bool isSameSeaCluster(MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1SeaCluster = getSeaCluster(tile1);
	int tile2SeaCluster = getSeaCluster(tile2);
	
	return tile1SeaCluster != -1 && tile2SeaCluster != -1 && tile1SeaCluster == tile2SeaCluster;
	
}

int getLandCluster(MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return aiFactionMovementInfo.landClusters.at(tileIndex);
	
}

bool isSameLandCluster(MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1LandCluster = getLandCluster(tile1);
	int tile2LandCluster = getLandCluster(tile2);
	
	return tile1LandCluster != -1 && tile2LandCluster != -1 && tile1LandCluster == tile2LandCluster;
	
}

int getLandTransportedCluster(MAP *tile)
{
	assert(isOnMap(tile));
	
	int tileIndex = tile - *MapTiles;
	return aiFactionMovementInfo.landTransportedClusters.at(tileIndex);
	
}

bool isSameLandTransportedCluster(MAP *tile1, MAP *tile2)
{
	assert(tile1 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	assert(tile2 >= *MapTiles && tile2 < *MapTiles + *MapAreaTiles);
	
	int tile1LandTransportedCluster = getLandTransportedCluster(tile1);
	int tile2LandTransportedCluster = getLandTransportedCluster(tile2);
	
	return tile1LandTransportedCluster != -1 && tile2LandTransportedCluster != -1 && tile1LandTransportedCluster == tile2LandTransportedCluster;
	
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

std::set<int> &getFirstConnectedClusters(int orgCluster, int dstCluster)
{
	return aiFactionMovementInfo.firstConnectedClusters[orgCluster][dstCluster];
}

std::set<int> &getFirstConnectedClusters(MAP *org, MAP *dst)
{
	int orgCluster = getCluster(org);
	int dstCluster = getCluster(dst);
	
	return getFirstConnectedClusters(orgCluster, dstCluster);
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

bool isSharedSeaCluster(MAP *tile)
{
	int seaCluster = getSeaCluster(tile);
	return aiFactionMovementInfo.sharedSeaClusters.find(seaCluster) != aiFactionMovementInfo.sharedSeaClusters.end();
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

