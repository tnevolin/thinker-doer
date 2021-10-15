#include <float.h>
#include <unordered_map>
#include "aiTransport.h"
#include "game_wtp.h"
#include "ai.h"
#include "aiMoveColony.h"

int moveSeaTransport(int vehicleId)
{
	assert(isSeaTransportVehicle(vehicleId));
	
	debug("moveSeaTransport\n");
	
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	// deliver artifact
	
	int carryingArtifactVehicleId = getCarryingArtifactVehicleId(vehicleId);
	if (carryingArtifactVehicleId >= 0)
	{
		if (deliverArtifact(vehicleId, carryingArtifactVehicleId))
		{
			return SYNC;
		}
		else
		{
			return mod_enemy_move(vehicleId);
		}
	}

	// deliver colony
	
	int carryingColonyVehicleId = getCarryingColonyVehicleId(vehicleId);
	if (carryingColonyVehicleId >= 0)
	{
		if (deliverColony(vehicleId, carryingColonyVehicleId))
		{
			return SYNC;
		}
		else
		{
			return mod_enemy_move(vehicleId);
		}
	}


	// deliver former
	
	int carryingFormerVehicleId = getCarryingFormerVehicleId(vehicleId);
	if (carryingFormerVehicleId >= 0)
	{
		if (deliverFormer(vehicleId, carryingFormerVehicleId))
		{
			return SYNC;
		}
		else
		{
			return mod_enemy_move(vehicleId);
		}
	}

	// deliver scout
	
	int carryingScoutVehicleId = getCarryingScoutVehicleId(vehicleId);
	if (carryingScoutVehicleId >= 0)
	{
		if (deliverScout(vehicleId, carryingScoutVehicleId))
		{
			return SYNC;
		}
		else
		{
			return mod_enemy_move(vehicleId);
		}
	}

	// other unit types - default to thinker
	
	if (isCarryingVehicle(vehicleId))
	{
		return mod_enemy_move(vehicleId);
	}

	// pickup colony
	
	if (pickupColony(vehicleId))
	{
		return SYNC;
	}

	// pickup former
	
	if (pickupFormer(vehicleId))
	{
		return SYNC;
	}
	
	// pop pod
	
	if (popPod(vehicleId))
	{
		return SYNC;
	}
	
	// no transportable vehicles - destroy transport
	
	debug("\tregion=%d, seaTransportRequest=%d\n", vehicleTile->region, activeFactionInfo.seaTransportRequests[vehicleTile->region]);
	
	if (activeFactionInfo.seaTransportRequests.count(vehicleTile->region) == 0 || activeFactionInfo.seaTransportRequests[vehicleTile->region] == 0)
	{
		veh_kill(vehicleId);
		return EM_DONE;
	}		

	// default to Thinker
	
	return trans_move(vehicleId);

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
	
	for (int baseId : activeFactionInfo.baseIds)
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
	
	return deliverVehicle(transportVehicleId, baseLocation, artifactVehicleId);
	
}

bool deliverColony(int transportVehicleId, int colonyVehicleId)
{
	// find best base location
	
	Location buildLocation = findLandBaseBuildLocation(colonyVehicleId);

	if (!isValidLocation(buildLocation))
		return false;

	// deliver vehicle
	
	return deliverVehicle(transportVehicleId, buildLocation, colonyVehicleId);
	
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
	
	return deliverVehicle(transportVehicleId, {regionClosestBase->x, regionClosestBase->y}, formerVehicleId);

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
	
	return deliverVehicle(transportVehicleId, nearestPodLocation, scoutVehicleId);

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
			
			moveSeaTransport(transportVehicleId);

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
			
			moveSeaTransport(transportVehicleId);

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

bool popPod(int transportVehicleId)
{
	Location nearestPodLocation = getNearestPodLocation(transportVehicleId);
	
	if (!isValidLocation(nearestPodLocation))
		return false;
	
	// go to destination
	
	setMoveTo(transportVehicleId, nearestPodLocation.x, nearestPodLocation.y);

	return true;

}

/*
Searches for closest location reachable by transport.
*/
Location getSeaTransportUnloadLocation(int oceanRegion, const Location destination)
{
	Location unloadLocation;
	
	// region should be ocean region
	
	if (!isOceanRegion(oceanRegion))
		return unloadLocation;
	
	// destination should be valid
	
	if (!isValidLocation(destination))
		return unloadLocation;
	
	// get destination tile
	
	MAP *destinationTile = getMapTile(destination);
	
	if (destinationTile == NULL)
		return unloadLocation;
	
	// destination is in same ocean region
	
	if (destinationTile->region == oceanRegion)
	{
		unloadLocation.set(destination);
		return unloadLocation;
	}
	
	// search for closest reachable location
	
	int minRange = INT_MAX;
	
	for (int mapTileIndex = 0; mapTileIndex < *map_area_tiles; mapTileIndex++)
	{
		Location location = getMapIndexLocation(mapTileIndex);
		
		// ocean region coast
		
		if (isOceanRegionCoast(location.x, location.y, oceanRegion))
		{
			int range = map_range(destination.x, destination.y, location.x, location.y);
			
			if (range < minRange)
			{
				unloadLocation.set(location);
				minRange = range;
			}
			
		}
		
	}
	
	return unloadLocation;
	
}

bool deliverVehicle(const int transportVehicleId, const Location destinationLocation, const int vehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);
	
	debug("\ntransportVehicles (%3d,%3d) -> (%3d,%3d)\n", transportVehicle->x, transportVehicle->y, destinationLocation.x, destinationLocation.y);
	
	// determine unload location
	
	Location unloadLocation = getSeaTransportUnloadLocation(transportVehicleTile->region, destinationLocation);
	
	if (!isValidLocation(unloadLocation))
		return false;
	
	MAP *unloadLocationTile = getMapTile(unloadLocation.x, unloadLocation.y);
	
	// at location
	if (isVehicleAtLocation(transportVehicleId, unloadLocation))
	{
		// stop ship

		veh_skip(transportVehicleId);

		// wake up vehicle

		setVehicleOrder(vehicleId, ORDER_NONE);

	}
	// next to unload location
	else if
	(
		map_range(transportVehicle->x, transportVehicle->y, unloadLocation.x, unloadLocation.y) == 1
		&&
		!map_base(unloadLocationTile) && !is_ocean(unloadLocationTile)
	)
	{
		// stop ship

		veh_skip(transportVehicleId);

		// wake up vehicle and move it to the coast

		setMoveTo(vehicleId, unloadLocation.x, unloadLocation.y);

	}
	else
	{
		// move to destination

		setMoveTo(transportVehicleId, unloadLocation.x, unloadLocation.y);

	}
	
	return true;
	
}

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
Finds the first ocean that is needed to be crossed in order to get to destination.
*/
int getCrossOceanRegion(Location initialLocation, Location terminalLocation)
{
	// check locations
	
	if (!isValidLocation(initialLocation) || !isValidLocation(terminalLocation))
		return -1;
	
	// get tiles
	
	MAP *initialLocationTile = getMapTile(initialLocation);
	MAP *terminalLocationTile = getMapTile(terminalLocation);
	
	if (initialLocationTile == NULL || terminalLocationTile == NULL)
		return -1;
	
	// should be different regions
	
	if (initialLocationTile->region == terminalLocationTile->region)
		return -1;
	
	// initial ocean location => same ocean to cross
	
	if (isOceanRegion(initialLocationTile->region))
		return initialLocationTile->region;
	
	// track path
	
	int initialX = initialLocation.x;
	int initialY = initialLocation.y;
	int terminalX = terminalLocation.x;
	int terminalY = terminalLocation.y;
	
	if (!*map_toggle_flat && abs(terminalX - initialX) > *map_half_x)
	{
		if (initialX < terminalX)
		{
			initialX += *map_axis_x;
		}
		else
		{
			terminalX += *map_axis_x;
		}
		
	}
	
	// iterate path
	
	int currentX = terminalX;
	int currentY = terminalY;
	
	int lastOceanRegion = -1;
	
	while (true)
	{
		// check map tile
		
		MAP *currentTile = getMapTile(currentX, currentY);
		
		if (currentTile == NULL)
			return -1;
		
		if (isOceanRegion(currentTile->region))
		{
			lastOceanRegion = currentTile->region;
		}
		
		// exit condition
		
		if (currentTile->region == initialLocationTile->region)
		{
			return lastOceanRegion;
		}
		
		// check end condition and move one tile
		
		int dx = initialX - currentX;
		int dy = initialY - currentY;
		
		if (dx == 0 && dy == 0)
		{
			return lastOceanRegion;
		}
		else if (abs(dx) == abs(dy))
		{
			currentX += (dx > 0 ? +1 : -1);
			currentY += (dy > 0 ? +1 : -1);
		}
		else if (abs(dx) > abs(dy))
		{
			currentX += (dx > 0 ? +2 : -2);
		}
		else
		{
			currentY += (dx > 0 ? +2 : -2);
		}
		
	}
	
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
		MAP *seaTransportVehicleTile = getVehicleMapTile(seaTransportVehicleId);
		
		// within given region
		
		if (seaTransportVehicleTile->region != region)
		
		// with remaining cargo capacity
		
		if ((cargo_capacity(vehicleId) - cargo_loaded(vehicleId)) <= 0)
			continue;
		
		// get range
		
		int range = map_range(seaTransportVehicle->x, seaTransportVehicle->y, vehicle->x, vehicle->y);
		
		// compare to task range if any
		
		if (hasTask(seaTransportVehicleId))
		{
			std::shared_ptr<Task> seaTransportTask = getTask(seaTransportVehicleId);
			
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

