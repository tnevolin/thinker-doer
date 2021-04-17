#include <float.h>
#include <unordered_map>
#include "aiTransport.h"
#include "game_wtp.h"
#include "ai.h"
#include "aiColony.h"

int moveSeaTransport(int vehicleId)
{
	assert(isVehicleSeaTransport(vehicleId));
	
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
	if (isCarryingVehicle(vehicleId))
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
		if (isVehicleColony(loadedVehicleId))
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
		if (isVehicleFormer(loadedVehicleId))
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
				minRange = 0;
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
			isVehicleColony(otherVehicleId)
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
					isVehicleColony(otherVehicleId)
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
			isVehicleFormer(otherVehicleId)
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
					isVehicleFormer(otherVehicleId)
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
Location getSeaTransportUnloadLocation(int transportVehicleId, const Location destinationLocation)
{
	MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);
	MAP *destinationLocationTile = getMapTile(destinationLocation.x, destinationLocation.y);
	
	Location unloadLocation;
	
	if (isInOceanRegion(transportVehicleId, destinationLocationTile->region))
	{
		// same region
		unloadLocation.set(destinationLocation);
	}
	else
	{
		// search for closest reachable location
		
		int minRange = INT_MAX;
		
		for (int mapTileIndex = 0; mapTileIndex < *map_area_tiles; mapTileIndex++)
		{
			Location location = getMapIndexLocation(mapTileIndex);
			
			// transport ocean region coast
			
			if (isOceanRegionCoast(location.x, location.y, transportVehicleTile->region))
			{
				int range = map_range(destinationLocation.x, destinationLocation.y, location.x, location.y);
				
				if (range < minRange)
				{
					unloadLocation.set(location);
					minRange = range;
				}
				
			}
			
		}
		
	}
	
	return unloadLocation;
	
}

bool deliverVehicle(const int transportVehicleId, const Location destinationLocation, const int vehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	
	debug("\ntransportVehicles (%3d,%3d) -> (%3d,%3d)\n", transportVehicle->x, transportVehicle->y, destinationLocation.x, destinationLocation.y);
	
	// determine unload location
	
	Location unloadLocation = getSeaTransportUnloadLocation(transportVehicleId, destinationLocation);
	
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
//		// put other units at hold if at base to not pick them up
//
//		if (map_base(transportVehicleTile))
//		{
//			for (int stackedVehicleId : getStackedVehicleIds(transportVehicleId))
//			{
//				VEH *stackedVehicle = &(Vehicles[stackedVehicleId]);
//
//				// skip self
//
//				if (stackedVehicleId == transportVehicleId)
//					continue;
//
//				if
//				(
//					veh_triad(stackedVehicleId) == TRIAD_LAND
//					&&
//					stackedVehicle->move_status == ORDER_NONE
//				)
//				{
//					stackedVehicle->move_status = ORDER_HOLD;
//				}
//
//			}
//			
//		}
//
		// move to destination

		setMoveTo(transportVehicleId, unloadLocation.x, unloadLocation.y);

	}
	
	return true;
	
}

bool isInOceanRegion(int vehicleId, int region)
{
	if (!isOceanRegion(region))
		return false;
	
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	for (MAP *tile : getAdjacentTiles(vehicle->x, vehicle->y, true))
	{
		if (!isOceanRegion(tile->region))
			continue;
		
		if (tile->region == region)
			return true;
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

