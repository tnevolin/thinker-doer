#include <float.h>
#include <unordered_map>
#include "aiTransport.h"
#include "game_wtp.h"
#include "ai.h"
#include "aiMoveColony.h"

void moveTranportStrategy()
{
	// iterate sea transports
	
	VehicleFilter seaTransportFilter;
	seaTransportFilter.factionId = aiFactionId;
	seaTransportFilter.weaponType = WPN_TROOP_TRANSPORT;
	seaTransportFilter.triad = TRIAD_SEA;
	for (int vehicleId : selectVehicles(seaTransportFilter))
	{
		// without task
		
		if (hasTask(vehicleId))
			continue;
		
		moveSeaTransportStrategy(vehicleId);
		
	}
	
}

void moveSeaTransportStrategy(int vehicleId)
{
	assert(isSeaTransportVehicle(vehicleId));
	
	debug("moveSeaTransportStrategy\n");
	
	// get loaded vehicles
	
	std::vector<int> loadedVehicleIds = getLoadedVehicleIds(vehicleId);
	
	// loaded
	
	if (loadedVehicleIds.size() == 0)
	{
		moveEmptySeaTransportStrategy(vehicleId);
	}
	else
	{
		moveLoadedSeaTransportStrategy(vehicleId, *(loadedVehicleIds.begin()));
	}
	
}

void moveEmptySeaTransportStrategy(int vehicleId)
{
	// pop pod
	
	popPodStrategy(vehicleId);
	
	// no transportable vehicles - destroy transport
	
	setTask(vehicleId, Task(KILL, vehicleId));

}

void moveLoadedSeaTransportStrategy(int vehicleId, int passengerId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// find nearest owned land
	
	Location nearestLandTerritory = getNearestLandTerritory(vehicle->x, vehicle->y, aiFactionId);
	
	if (!isValidLocation(nearestLandTerritory))
		return;
	
	// set task
	
	transitVehicle(passengerId, Task(MOVE, passengerId, nearestLandTerritory));
	
}

int getCarryingArtifactVehicleId(int transportVehicleId)
{
	int carriedVehicleId = -1;
	
	for (int loadedVehicleId : getLoadedVehicleIds(transportVehicleId))
	{
		if (isVehicleArtifact(loadedVehicleId))
		{
			carriedVehicleId = loadedVehicleId;
		}
	}

	return carriedVehicleId;

}

bool isCarryingArtifact(int vehicleId)
{
	return getCarryingArtifactVehicleId(vehicleId) >= 0;
}

int getCarryingColonyVehicleId(int transportVehicleId)
{
	int carriedVehicleId = -1;
	
	for (int loadedVehicleId : getLoadedVehicleIds(transportVehicleId))
	{
		if (isColonyVehicle(loadedVehicleId))
		{
			carriedVehicleId = loadedVehicleId;
		}
	}

	return carriedVehicleId;

}

bool isCarryingColony(int vehicleId)
{
	return getCarryingColonyVehicleId(vehicleId) >= 0;
}

int getCarryingFormerVehicleId(int transportVehicleId)
{
	int carriedVehicleId = -1;
	
	for (int loadedVehicleId : getLoadedVehicleIds(transportVehicleId))
	{
		if (isFormerVehicle(loadedVehicleId))
		{
			carriedVehicleId = loadedVehicleId;
		}
	}

	return carriedVehicleId;

}

bool isCarryingFormer(int vehicleId)
{
	return getCarryingFormerVehicleId(vehicleId) >= 0;
}

bool isCarryingVehicle(int vehicleId)
{
	return getLoadedVehicleIds(vehicleId).size() >= 1;
}

bool deliverArtifact(int transportVehicleId, int artifactVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);

	// find base in need of artifact (building project or closest)

	Location baseLocation;
	int minRange = INT_MAX;
	
	for (int baseId : data.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		if(isBaseBuildingProject(baseId))
		{
			baseLocation.set(base->x, base->y);
			minRange = 0;
		}
		else
		{
			int range = map_range(transportVehicle->x, transportVehicle->y, base->x, base->y);
			
			if (range < minRange)
			{
				baseLocation.set(base->x, base->y);
				minRange = range;
			}
			
		}
		
	}
	
	// not found
	
	if (minRange == INT_MAX)
		return false;
	
	// deliver vehicle
	
//	return deliverVehicle(transportVehicleId, baseLocation, artifactVehicleId);
	transitVehicle(artifactVehicleId, Task(HOLD, artifactVehicleId, baseLocation));
	
	return true;
	
}

bool deliverColony(int colonyVehicleId)
{
	// find best base location
	
	Location buildLocation = findLandBaseBuildLocation(colonyVehicleId);

	if (!isValidLocation(buildLocation))
		return false;

	// deliver vehicle
	
//	return deliverVehicle(transportVehicleId, buildLocation, colonyVehicleId);
	transitVehicle(colonyVehicleId, Task(BUILD, colonyVehicleId, buildLocation));
	
	return true;
	
}

bool deliverFormer(int transportVehicleId, int formerVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);
	
	debug("\ndeliverFormer (%3d,%3d)\n", transportVehicle->x, transportVehicle->y);

	// search for reachable regions

	std::unordered_set<int> reachableRegions;

	for (int mapTileIndex = 0; mapTileIndex < *map_area_tiles; mapTileIndex++)
	{
		Location location = getMapIndexLocation(mapTileIndex);
		MAP *tile = getMapTile(location.x, location.y);
		
		// reachable coast only

		if (!isOceanRegionCoast(location.x, location.y, transportVehicleTile->region))
			continue;
		
		// add reachable region
		
		reachableRegions.insert(tile->region);

	}
	
	// populate region former ratios

	std::unordered_map<int, int> regionBaseCounts;
	std::unordered_map<int, int> regionFormerCounts;
	std::unordered_map<int, BASE *> regionClosestBases;
	std::unordered_map<int, int> regionClosestBaseRanges;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);

		// only own bases

		if (base->faction_id != transportVehicle->faction_id)
			continue;

		// only reachable regions

		if (reachableRegions.count(baseTile->region) == 0)
			continue;

		// count base in region

		if (regionBaseCounts.count(baseTile->region) == 0)
		{
			regionBaseCounts[baseTile->region] = 0;
		}

		regionBaseCounts[baseTile->region]++;
		
		// update closest base range
		
		if (regionClosestBaseRanges.count(baseTile->region) == 0)
		{
			regionClosestBaseRanges[baseTile->region] = INT_MAX;
		}

		int range = map_range(transportVehicle->x, transportVehicle->y, base->x, base->y);
		
		if (range < regionClosestBaseRanges[baseTile->region])
		{
			regionClosestBases[baseTile->region] = base;
			regionClosestBaseRanges[baseTile->region] = range;
		}
		
	}

	for (int vehicleId = 0; vehicleId < *total_num_vehicles; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		// only own vehicles

		if (vehicle->faction_id != transportVehicle->faction_id)
			continue;

		// only count regions where our bases are

		if (regionBaseCounts.count(vehicleTile->region) == 0)
			continue;

		// count former in region

		if (regionFormerCounts.count(vehicleTile->region) == 0)
		{
			regionFormerCounts[vehicleTile->region] = 0;
		}

		regionFormerCounts[vehicleTile->region]++;

	}

	// search for most demanding region

	int mostDemandingRegion = -1;
	double minFormerRatio = DBL_MAX;

	for (const auto &kv : regionBaseCounts)
	{
		int region = kv.first;
		int baseCount = kv.second;
		int formerCount = (regionFormerCounts.count(region) == 0 ? 0 : regionFormerCounts[region]);

		double formerRatio = (double)formerCount / (double)baseCount;

		if (mostDemandingRegion == -1 || formerRatio < minFormerRatio)
		{
			mostDemandingRegion = region;
			minFormerRatio = formerRatio;
		}

	}

	// no regions with bases ?

	if (mostDemandingRegion == -1)
		return false;
	
	// find nearest base in most demanding region

	// retrive destination location

	BASE *regionClosestBase = regionClosestBases[mostDemandingRegion];

	debug("\tdestinationLocation=(%3d,%3d)\n", regionClosestBase->x, regionClosestBase->y);

	// transport vehicles
	
//	return deliverVehicle(transportVehicleId, {regionClosestBase->x, regionClosestBase->y}, formerVehicleId);
	transitVehicle(formerVehicleId, Task(HOLD, formerVehicleId, {regionClosestBase->x, regionClosestBase->y}));
	
	return true;
	
}

bool deliverScout(int transportVehicleId, int scoutVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	
	debug("\ndeliverScount (%3d,%3d)\n", transportVehicle->x, transportVehicle->y);

	// find nearest pod
	
	Location nearestPodLocation;
	int minRange = INT_MAX;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		Location location = getMapIndexLocation(mapIndex);
		
		// exclude targetted locations
		
		if (isTargettedLocation(location))
			continue;
		
		if (goody_at(location.x, location.y) != 0)
		{
			int range = map_range(transportVehicle->x, transportVehicle->y, location.x, location.y);
			
			if (range < minRange)
			{
				nearestPodLocation.set(location);
				minRange = range;
			}
			
		}
		
	}
	
	// not found
	
	if (!isValidLocation(nearestPodLocation))
		return false;
	
	// deliver scout
	
//	return deliverVehicle(transportVehicleId, nearestPodLocation, scoutVehicleId);
	transitVehicle(scoutVehicleId, Task(MOVE, scoutVehicleId, nearestPodLocation));
	
	return true;
	
}

bool pickupColony(int transportVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);

	// search for nearest colony need ferry

	Location pickupLocation;
	int minRange = INT_MAX;

	for (int otherVehicleId = 0; otherVehicleId < *total_num_vehicles; otherVehicleId++)
	{
		VEH *otherVehicle = &(Vehicles[otherVehicleId]);
		MAP *otherVehicleTile = getVehicleMapTile(otherVehicleId);

		if
		(
			// own transportVehicle
			otherVehicle->faction_id == transportVehicle->faction_id
			&&
			// colony
			isColonyVehicle(otherVehicleId)
			&&
			// land colony
			veh_triad(otherVehicleId) == TRIAD_LAND
			&&
			// in base
			map_has_item(otherVehicleTile, TERRA_BASE_IN_TILE)
			&&
			// on ocean
			is_ocean(otherVehicleTile)
			&&
			// same region
			otherVehicleTile->region == transportVehicleTile->region
		)
		{
			int range = map_range(transportVehicle->x, transportVehicle->y, otherVehicle->x, otherVehicle->y);

			if (range < minRange)
			{
				pickupLocation.set(otherVehicle->x, otherVehicle->y);
				minRange = range;
			}

		}

	}

	if (minRange == INT_MAX)
	{
		// not found
		return false;
	}
	else
	{
		// at destination - pickup colonies
		if (isVehicleAtLocation(transportVehicleId, pickupLocation))
		{
			// load

			for (int otherVehicleId : getFactionLocationVehicleIds(transportVehicle->faction_id, pickupLocation))
			{
				if
				(
					isColonyVehicle(otherVehicleId)
					&&
					veh_triad(otherVehicleId) == TRIAD_LAND
				)
				{
					set_board_to(otherVehicleId, transportVehicleId);
				}

			}
			
			// move transport right away
			
//			moveSeaTransport(transportVehicleId);

		}
		// not at destination - go to destination
		else
		{
			setMoveTo(transportVehicleId, pickupLocation.x, pickupLocation.y);
		}

		// colony found
		return true;

	}

}

bool pickupFormer(int transportVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);

	// search for nearest Former need ferry

	Location pickupLocation;
	int minRange = INT_MAX;

	for (int otherVehicleId = 0; otherVehicleId < *total_num_vehicles; otherVehicleId++)
	{
		VEH *otherVehicle = &(Vehicles[otherVehicleId]);
		MAP *otherVehicleTile = getVehicleMapTile(otherVehicleId);

		if
		(
			// own transportVehicle
			otherVehicle->faction_id == transportVehicle->faction_id
			&&
			// Former
			isFormerVehicle(otherVehicleId)
			&&
			// land Former
			veh_triad(otherVehicleId) == TRIAD_LAND
			&&
			// in base
			map_has_item(otherVehicleTile, TERRA_BASE_IN_TILE)
			&&
			// on ocean
			is_ocean(otherVehicleTile)
			&&
			// same region
			otherVehicleTile->region == transportVehicleTile->region
		)
		{
			int range = map_range(transportVehicle->x, transportVehicle->y, otherVehicle->x, otherVehicle->y);

			if (range < minRange)
			{
				pickupLocation.set(otherVehicle->x, otherVehicle->y);
				minRange = range;
			}

		}

	}

	if (minRange == INT_MAX)
	{
		// not found
		return false;
	}
	else
	{
		// at destination - pickup
		if (isVehicleAtLocation(transportVehicleId, pickupLocation))
		{
			// load

			for (int otherVehicleId : getFactionLocationVehicleIds(transportVehicle->faction_id, pickupLocation))
			{
				if
				(
					isFormerVehicle(otherVehicleId)
					&&
					veh_triad(otherVehicleId) == TRIAD_LAND
				)
				{
					set_board_to(otherVehicleId, transportVehicleId);
				}

			}

			// move transport right away
			
//			moveSeaTransport(transportVehicleId);

		}
		// not at destination - go to destination
		else
		{
			setMoveTo(transportVehicleId, pickupLocation.x, pickupLocation.y);
		}

		// Former found
		return true;

	}

}

void popPodStrategy(int transportVehicleId)
{
	Location nearestPodLocation = getNearestPodLocation(transportVehicleId);
	
	if (!isValidLocation(nearestPodLocation))
		return;
	
	// go to destination
	
	setTask(transportVehicleId, Task(MOVE, transportVehicleId, nearestPodLocation));

}

/*
Searches for unboard location.
*/
Location getSeaTransportUnboardLocation(int seaTransportVehicleId, const Location destination)
{
	VEH *seaTransportVehicle = &(Vehicles[seaTransportVehicleId]);
	int seaTransportVehicleAssociation = getVehicleAssociation(seaTransportVehicleId);
	
	Location unboardLocation;
	
	// destination should be valid
	
	if (!isValidLocation(destination))
		return unboardLocation;
	
	// get destination tile
	
	MAP *destinationTile = getMapTile(destination);
	
	if (destinationTile == nullptr)
		return unboardLocation;
	
	// destination is ocean with no base
	
	if (isOceanRegion(destinationTile->region) && !map_has_item(destinationTile, TERRA_BASE_IN_TILE))
		return unboardLocation;
	
	// destination is ocean and is same association
	
	if (isOceanRegion(destinationTile->region) && isSameAssociation(seaTransportVehicleAssociation, destinationTile, seaTransportVehicle->faction_id))
	{
		unboardLocation.set(destination);
		return unboardLocation;
	}
	
	// search for closest reachable location
	
	int minRange = INT_MAX;
	
	for (int mapTileIndex = 0; mapTileIndex < *map_area_tiles; mapTileIndex++)
	{
		Location location = getMapIndexLocation(mapTileIndex);
		
		if (!isOceanAssociationCoast(location.x, location.y, seaTransportVehicleAssociation, seaTransportVehicle->faction_id))
			continue;
		
		// get range
		
		int range = map_range(location.x, location.y, destination.x, destination.y);
		
		if (range < minRange)
		{
			unboardLocation.set(location);
			minRange = range;
		}
		
	}
	
	return unboardLocation;
	
}

/*
Searches for closest location reachable by transport.
*/
Location getSeaTransportUnloadLocation(int seaTransportVehicleId, const Location destination, const Location unboardLocation)
{
	VEH *seaTransportVehicle = &(Vehicles[seaTransportVehicleId]);
	int seaTransportVehicleAssociation = getVehicleAssociation(seaTransportVehicleId);
	
	Location unloadLocation;
	
	// destination should be valid
	
	if (!isValidLocation(destination))
		return unloadLocation;
	
	// get destination tile
	
	MAP *destinationTile = getMapTile(destination);
	
	if (destinationTile == nullptr)
		return unloadLocation;
	
	// destination is ocean with no base
	
	if (isOceanRegion(destinationTile->region) && !map_has_item(destinationTile, TERRA_BASE_IN_TILE))
		return unloadLocation;
	
	// destination is ocean and is same association and is frendly coastal base
	
	if (isOceanRegion(destinationTile->region) && isSameAssociation(seaTransportVehicleAssociation, destinationTile, seaTransportVehicle->faction_id) && data.geography[seaTransportVehicle->faction_id].friendlyCoastalBaseOceanAssociations.count(destinationTile) != 0)
	{
		unloadLocation.set(destination);
		return unloadLocation;
	}
	
	// search for reachable location around destination
	
	for (MAP *adjacentTile : getAdjacentTiles(unboardLocation.x, unboardLocation.y, false))
	{
		Location adjacentTileLocation = getLocation(adjacentTile);
		
		// same ocean association
		
		int oceanAssociation = getOceanAssociation(adjacentTile, seaTransportVehicle->faction_id);
		
		if (oceanAssociation == seaTransportVehicleAssociation)
		{
			unloadLocation.set(adjacentTileLocation);
			return unloadLocation;
		}
		
	}
	
	return unloadLocation;
	
}

//bool deliverVehicle(const int transportVehicleId, const Location destinationLocation, const int vehicleId)
//{
//	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
//	MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);
//	
//	debug("\ntransportVehicles (%3d,%3d) -> (%3d,%3d)\n", transportVehicle->x, transportVehicle->y, destinationLocation.x, destinationLocation.y);
//	
//	// determine unload location
//	
//	Location unloadLocation = getSeaTransportUnloadLocation(transportVehicleTile->region, destinationLocation, transportVehicle->faction_id);
//	
//	if (!isValidLocation(unloadLocation))
//		return false;
//	
//	MAP *unloadLocationTile = getMapTile(unloadLocation.x, unloadLocation.y);
//	
//	// at location
//	if (isVehicleAtLocation(transportVehicleId, unloadLocation))
//	{
//		// stop ship
//
//		veh_skip(transportVehicleId);
//
//		// wake up vehicle
//
//		setVehicleOrder(vehicleId, ORDER_NONE);
//
//	}
//	// next to unload location
//	else if
//	(
//		map_range(transportVehicle->x, transportVehicle->y, unloadLocation.x, unloadLocation.y) == 1
//		&&
//		!map_base(unloadLocationTile) && !is_ocean(unloadLocationTile)
//	)
//	{
//		// stop ship
//
//		veh_skip(transportVehicleId);
//
//		// wake up vehicle and move it to the coast
//
//		setMoveTo(vehicleId, unloadLocation.x, unloadLocation.y);
//
//	}
//	else
//	{
//		// move to destination
//
//		setMoveTo(transportVehicleId, unloadLocation.x, unloadLocation.y);
//
//	}
//	
//	return true;
//	
//}
//
/*
Checks if sea vehicle is in ocean region or coastal base.
*/
bool isInOceanRegion(int vehicleId, int region)
{
	if (!isOceanRegion(region))
		return false;
	
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	if (vehicle->triad() != TRIAD_SEA)
		return false;
	
	if (vehicleTile->region == region)
		return true;
	
	if (map_has_item(vehicleTile, TERRA_BASE_IN_TILE))
	{
		for (MAP *tile : getAdjacentTiles(vehicle->x, vehicle->y, false))
		{
			if (!isOceanRegion(tile->region))
				continue;
			
			if (tile->region == region)
				return true;
			
		}
		
	}
	
	return false;
	
}

int getCarryingScoutVehicleId(int transportVehicleId)
{
	int carriedVehicleId = -1;
	
	for (int loadedVehicleId : getLoadedVehicleIds(transportVehicleId))
	{
		if (isScoutVehicle(loadedVehicleId))
		{
			carriedVehicleId = loadedVehicleId;
		}
	}

	return carriedVehicleId;

}

/*
Finds the first ocean association that is needed to be crossed in order to get to destination.
*/
int getCrossOceanAssociation(Location initialLocation, Location terminalLocation, int factionId)
{
	// check locations
	
	if (!isValidLocation(initialLocation) || !isValidLocation(terminalLocation))
		return -1;
	
	// get tiles
	
	MAP *initialLocationTile = getMapTile(initialLocation);
	MAP *terminalLocationTile = getMapTile(terminalLocation);
	
	if (initialLocationTile == NULL || terminalLocationTile == NULL)
		return -1;
	
	// get associations
	
	int initialLocationAssociation = getAssociation(initialLocationTile, factionId);
	int terminalLocationAssociation = getAssociation(terminalLocationTile, factionId);
	
	// if in ocean already this is the ocean to cross
	
	if (isOceanRegion(initialLocationTile->region))
		return initialLocationAssociation;
	
	// get connections
	
	std::set<int> initialLocationAssociationConnections = data.geography[factionId].connections.at(initialLocationAssociation);
	
	if (initialLocationAssociationConnections.size() == 0)
		return -1;
	
	// preset path variables
	
	std::unordered_map<int, std::unordered_set<int>> paths;
	for (int connection : initialLocationAssociationConnections)
	{
		paths.insert({connection, {initialLocationAssociation, connection}});
	}
	
	// select best path
	
	bool updated;
	{
		updated = false;
	
		for (auto &path : paths)
		{
			int crossOceanAssociation = path.first;
			std::unordered_set<int> *pathAssociations = &(path.second);
			
			std::unordered_set<int> newPathAssociations;
			for (int pathAssociation : *pathAssociations)
			{
				for (int connection : data.geography[factionId].connections.at(pathAssociation))
				{
					if (connection == terminalLocationAssociation)
						return crossOceanAssociation;
					
					if (pathAssociations->count(connection) != 0)
						continue;
					
					newPathAssociations.insert(connection);
					updated = true;
					
				}
				
			}
			
			// add new associations
			
			pathAssociations->insert(newPathAssociations.begin(), newPathAssociations.end());
			
		}
		
	}
	while (updated);
	
	return -1;
	
}

/*
Searches for available sea transport in given region.
Full transport is not selected.
Transport without go to order is selected right away.
Transport with go to order is selected only if given location is closer than its current destination.
If such transport is found set its destination to the vehicle.
*/
int getAvailableSeaTransport(int region, int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	Location vehicleLocation{vehicle->x, vehicle->y};
	
	// find closest available sea transport in this region
	
	int closestAvailableSeaTransportVehicleId = -1;
	int closestAvailableSeaTransportVehicleRange = INT_MAX;
	
	VehicleFilter seaTransportFilter;
	seaTransportFilter.factionId = vehicle->faction_id;
	seaTransportFilter.weaponType = WPN_TROOP_TRANSPORT;
	seaTransportFilter.triad = TRIAD_SEA;
	for (int seaTransportVehicleId : selectVehicles(seaTransportFilter))
	{
		VEH *seaTransportVehicle = &(Vehicles[seaTransportVehicleId]);
		
		int seaTransportVehicleAssociation = getVehicleAssociation(seaTransportVehicleId);
		
		// within same assosiation
		
		if (seaTransportVehicleAssociation != region)
			continue;
		
		// with remaining cargo capacity
		
		if ((cargo_capacity(seaTransportVehicleId) - cargo_loaded(seaTransportVehicleId)) <= 0)
			continue;
		
		// get range
		
		int range = map_range(seaTransportVehicle->x, seaTransportVehicle->y, vehicle->x, vehicle->y);
		
		// compare to task range if any
		
		if (hasTask(seaTransportVehicleId))
		{
			Task *seaTransportTask = getTask(seaTransportVehicleId);
			
			Location destination = seaTransportTask->getDestination();
			
			if (isValidLocation(destination))
			{
				int destinationRange = map_range(seaTransportVehicle->x, seaTransportVehicle->y, destination.x, destination.y);
				
				if (destinationRange <= range)
					continue;
			
			}
			
		}
		
		// update closest
		
		if (closestAvailableSeaTransportVehicleId == -1 || range < closestAvailableSeaTransportVehicleRange)
		{
			closestAvailableSeaTransportVehicleId = seaTransportVehicleId;
			closestAvailableSeaTransportVehicleRange = range;
		}
		
	}
	
	// return
	
	return closestAvailableSeaTransportVehicleId;
	
}

Location getSeaTransportLoadLocation(int seaTransportVehicleId, int passengerVehicleId)
{
	Location loadLocation;
	
	VEH *seaTransportVehicle = &(Vehicles[seaTransportVehicleId]);
	VEH *passengerVehicle = &(Vehicles[passengerVehicleId]);
	
	// default load location is passenger location
	
	loadLocation.set(passengerVehicle->x, passengerVehicle->y);
	
	// further location modification is for sea tranport picking land unit only
	
	if (seaTransportVehicle->triad() != TRIAD_SEA && passengerVehicle->triad() != TRIAD_LAND)
		return loadLocation;
	
	// get region associations
	
	int seaTransportAssociation = getVehicleAssociation(seaTransportVehicleId);
	int passengerAssociation = getVehicleAssociation(passengerVehicleId);
	
	// just in case they cannot be determined or are somehow in same association
	
	if (seaTransportAssociation == -1 || passengerAssociation == -1 || seaTransportAssociation == passengerAssociation)
		return loadLocation;
	
	// find best load location for transport
	
	int minRange = INT_MAX;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		Location tileLocation = getLocation(mapIndex);
		MAP *tile = getMapTile(mapIndex);
		
		int tileOceanAssociation = getOceanAssociation(tile, seaTransportVehicle->faction_id);
		
		// sea transport association only
		
		if (tileOceanAssociation != seaTransportAssociation)
			continue;
		
		// compute range
		
		int seaTransportRange = map_range(seaTransportVehicle->x, seaTransportVehicle->y, tileLocation.x, tileLocation.y);
		
		// check adjacent tiles
		
		for (MAP *adjacentTile : getAdjacentTiles(tileLocation.x, tileLocation.y, false))
		{
			Location adjacentTileLocation = getLocation(adjacentTile);
			
			int adjacentTileAssociation = getAssociation(adjacentTile, seaTransportVehicle->faction_id);
			
			// passenger association only
			
			if (adjacentTileAssociation != passengerAssociation)
				continue;
			
			// compute range
			
			int passengerRange = map_range(passengerVehicle->x, passengerVehicle->y, adjacentTileLocation.x, adjacentTileLocation.y);
			
			// get greater range
			
			int greaterRange = std::max(seaTransportRange, passengerRange);
			
			// update location
			
			if (greaterRange < minRange)
			{
				loadLocation.set(tileLocation);
				minRange = greaterRange;
			}
			
		}
		
	}
	
	return loadLocation;
	
}

