#include <float.h>
#include <map>
#include "game_wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMove.h"
#include "aiMoveColony.h"
#include "aiMoveTransport.h"
#include "aiRoute.h"

void moveTranportStrategy()
{
	debug("moveTranportStrategy - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// iterate sea transports
	
	for (int vehicleId : aiData.seaTransportVehicleIds)
	{
		moveSeaTransportStrategy(vehicleId);
		
		if (!hasTask(vehicleId))
		{
			moveAvailableSeaTransportStrategy(vehicleId);
		}
		
	}
	
	// generate transport requests
	
	for (TransitRequest &transitRequest : aiData.transportControl.transitRequests)
	{
		if (transitRequest.isFulfilled())
			continue;
		
		int seaTransportAssociation = getOceanAssociation(transitRequest.origin, aiFactionId);
		
		debug
		(
			"addSeaTransportRequest unfullfilled request"
			" seaTransportAssociation=%2d"
			" [%4d] (%3d,%3d)->(%3d,%3d)"
			"\n"
			, seaTransportAssociation
			, transitRequest.getVehicleId()
			, getX(transitRequest.origin), getY(transitRequest.origin)
			, getX(transitRequest.destination), getY(transitRequest.destination)
		);
		
		aiData.addSeaTransportRequest(seaTransportAssociation);
		
	}
	
	if (DEBUG)
	{
		debug("\t=== Transportation requests ===\n");
		debug("\t\tunloadRequests\n");
		for (robin_hood::pair<int, std::vector<UnloadRequest>> &unloadRequestEntry : aiData.transportControl.unloadRequests)
		{
			int seaTransportVehiclePad0 = unloadRequestEntry.first;
			std::vector<UnloadRequest> &transportUnloadRequests = unloadRequestEntry.second;
			int seaTransportVehicleId = getVehicleIdByAIId(seaTransportVehiclePad0);
			VEH *seaTransportVehicle = getVehicle(seaTransportVehicleId);
			
			debug("\t\t\t[%4d] (%3d,%3d)\n", seaTransportVehicleId, seaTransportVehicle->x, seaTransportVehicle->y);
			
			for (UnloadRequest unloadRequest : transportUnloadRequests)
			{
				int vehicleId = unloadRequest.getVehicleId();
				debug
				(
					"\t\t\t\t[%4d]->(%3d,%3d)*>(%3d,%3d)\n"
					, vehicleId
					, getX(unloadRequest.destination), getY(unloadRequest.destination)
					, getX(unloadRequest.unboardLocation), getY(unloadRequest.unboardLocation)
				);
				
			}
			
		}
		
		debug("\t\ttransitRequests\n");
		for (TransitRequest transitRequest : aiData.transportControl.transitRequests)
		{
			int vehicleId = transitRequest.getVehicleId();
			
			debug
			(
				"\t\t\t[%4d] (%3d,%3d)->(%3d,%3d)\n"
				, vehicleId
				, getX(transitRequest.origin), getY(transitRequest.origin)
				, getX(transitRequest.destination), getY(transitRequest.destination)
			);
			
			if (transitRequest.isFulfilled())
			{
				int seaTransportVehicleId = transitRequest.getSeaTransportVehicleId();
				VEH *seaTransportVehicle = getVehicle(seaTransportVehicleId);
				
				debug
				(
					"\t\t\t\t[%4d] (%3d,%3d)\n"
					, seaTransportVehicleId
					, seaTransportVehicle->x, seaTransportVehicle->y
				);
				
			}
			
		}
		
	}
	
}

void moveSeaTransportStrategy(int vehicleId)
{
	bool TRACE = DEBUG && false;
	
	debug("moveSeaTransportStrategy [%4d] (%3d,%3d)\n", vehicleId, getVehicle(vehicleId)->x, getVehicle(vehicleId)->y);
	
	// assign unload task
	
	MAP *closestUnloadLocation = nullptr;
	int closestUnloadLocationPreference = -1;
	int closestUnloadLocationTravelTime = INT_MAX;
	
	for (UnloadRequest &unloadRequest : aiData.transportControl.getSeaTransportUnloadRequests(vehicleId))
	{
		int passengerVehicleId = unloadRequest.getVehicleId();
		int travelTime = getVehicleATravelTime(vehicleId, unloadRequest.destination);
		
		// preference
		
		int preference;
		
		if (isColonyVehicle(passengerVehicleId))
		{
			preference = 10;
		}
		else if (isFormerVehicle(passengerVehicleId))
		{
			preference = 9;
		}
		else
		{
			preference = 0;
		}
			
		// update best
		
		if
		(
			preference > closestUnloadLocationPreference
			||
			preference == closestUnloadLocationPreference && travelTime < closestUnloadLocationTravelTime
		)
		{
			closestUnloadLocation = unloadRequest.destination;
			closestUnloadLocationPreference = preference;
			closestUnloadLocationTravelTime = travelTime;
		}
		
	}
	
	if (closestUnloadLocation != nullptr)
	{
		setTask(Task(vehicleId, TT_UNLOAD, closestUnloadLocation));
		if (TRACE) { debug("\tunload task: (%3d,%3d)\n", getX(closestUnloadLocation), getY(closestUnloadLocation)); }
	}
	
	// assign load tasks
	
	// not full
	
	if (getTransportRemainingCapacity(vehicleId) == 0)
	{
		if (TRACE) { debug("\ttransport is full\n"); }
		return;
	}
	
	// not carrying artifact
	
	for (int passengerVehicleId : getTransportPassengers(vehicleId))
	{
		if (isArtifactVehicle(passengerVehicleId))
		{
			return;
		}
	}
	
	if (!hasTask(vehicleId)) // no task
	{
		TransitRequest *closestTransitRequest = nullptr;
		int closestLoadLocationTravelTime = INT_MAX;
		
		for (TransitRequest &transitRequest : aiData.transportControl.transitRequests)
		{
			// not yet fulfilled
			
			if (transitRequest.isFulfilled())
				continue;
			
			int travelTime = getVehicleATravelTime(vehicleId, transitRequest.origin);
			
			if (travelTime < closestLoadLocationTravelTime)
			{
				closestTransitRequest = &transitRequest;
				closestLoadLocationTravelTime = travelTime;
			}
			
		}
		
		if (closestTransitRequest != nullptr)
		{
			setTask(Task(vehicleId, TT_LOAD, closestTransitRequest->origin));
			closestTransitRequest->setSeaTransportVehicleId(vehicleId);
			if (TRACE) { debug("\tload task: (%3d,%3d)\n", getX(closestTransitRequest->origin), getY(closestTransitRequest->origin)); }
		}
		
	}
	else // has task
	{
		Task *task = getTask(vehicleId);
		
		if (task == nullptr)
			return;
		
		// for now don't bother mixing load tasks
		
		if (task->type == TT_LOAD)
			return;
		
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		MAP *destination = task->getDestination();
		int currentTravelTime = getVehicleATravelTime(vehicleId, destination);
		int destinationAssociation = getAssociation(destination, aiFactionId);
		
		TransitRequest *closestTransitRequest = nullptr;
		int closestLoadLocationTravelTime = currentTravelTime * 150 / 100;
		
		for (TransitRequest &transitRequest : aiData.transportControl.transitRequests)
		{
			// not yet fulfilled
			
			if (transitRequest.isFulfilled())
				continue;
			
			// same destination association
			
			if (destinationAssociation != getAssociation(transitRequest.destination, aiFactionId))
				continue;
			
			// combined travel time
			
			int travelTime =
				+ getVehicleATravelTime(vehicleId, vehicleTile, transitRequest.origin)
				+ getVehicleATravelTime(vehicleId, transitRequest.origin, destination)
			;
			
			if (travelTime < closestLoadLocationTravelTime)
			{
				closestTransitRequest = &transitRequest;
				closestLoadLocationTravelTime = travelTime;
			}
			
		}
		
		if (closestTransitRequest != nullptr)
		{
			setTask(Task(vehicleId, TT_LOAD, closestTransitRequest->origin));
			closestTransitRequest->setSeaTransportVehicleId(vehicleId);
			if (TRACE) { debug("\tload task: (%3d,%3d)\n", getX(closestTransitRequest->origin), getY(closestTransitRequest->origin)); }
		}
		
	}
	
}

void moveAvailableSeaTransportStrategy(int vehicleId)
{
	// pop pod
	
	popPodStrategy(vehicleId);
	
	if (hasTask(vehicleId))
		return;
	
	// no pods - destroy transport
	
	setTask(Task(vehicleId, TT_KILL));

}

void moveLoadedSeaTransportStrategy(int vehicleId, int passengerId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// find nearest owned land
	
	MAP *nearestLandTerritory = getNearestLandTerritory(vehicle->x, vehicle->y, aiFactionId);
	
	if (nearestLandTerritory == nullptr)
		return;
	
	// set task
	
	transitVehicle(Task(passengerId, TT_MOVE, nearestLandTerritory));
	
}

int getCarryingArtifactVehicleId(int transportVehicleId)
{
	int carriedVehicleId = -1;
	
	for (int loadedVehicleId : getLoadedVehicleIds(transportVehicleId))
	{
		if (isArtifactVehicle(loadedVehicleId))
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

	MAP *baseLocation = nullptr;
	int minRange = INT_MAX;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		if(isBaseBuildingProject(baseId))
		{
			baseLocation = getBaseMapTile(baseId);
			minRange = 0;
		}
		else
		{
			int range = map_range(transportVehicle->x, transportVehicle->y, base->x, base->y);
			
			if (range < minRange)
			{
				baseLocation = getBaseMapTile(baseId);
				minRange = range;
			}
			
		}
		
	}
	
	// not found
	
	if (minRange == INT_MAX)
		return false;
	
	// deliver vehicle
	
	transitVehicle(Task(artifactVehicleId, TT_HOLD, baseLocation));
	
	return true;
	
}

bool deliverFormer(int transportVehicleId, int formerVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);
	
	debug("\ndeliverFormer (%3d,%3d)\n", transportVehicle->x, transportVehicle->y);

	// search for reachable regions

	robin_hood::unordered_flat_set<int> reachableRegions;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int mapX = getX(mapIndex);
		int mapY = getY(mapIndex);
		MAP *tile = getMapTile(mapIndex);
		
		// reachable coast only

		if (!isOceanRegionCoast(mapX, mapY, transportVehicleTile->region))
			continue;
		
		// add reachable region
		
		reachableRegions.insert(tile->region);

	}
	
	// populate region former ratios

	robin_hood::unordered_flat_map<int, int> regionBaseCounts;
	robin_hood::unordered_flat_map<int, int> regionFormerCounts;
	robin_hood::unordered_flat_map<int, BASE *> regionClosestBases;
	robin_hood::unordered_flat_map<int, int> regionClosestBaseRanges;

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
	
	transitVehicle(Task(formerVehicleId, TT_HOLD, getMapTile(regionClosestBase->x, regionClosestBase->y)));
	
	return true;
	
}

bool deliverScout(int transportVehicleId, int scoutVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	
	debug("\ndeliverScount (%3d,%3d)\n", transportVehicle->x, transportVehicle->y);

	// find nearest pod
	
	MAP *nearestPodLocation = nullptr;
	int minRange = INT_MAX;

	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		int mapX = getX(mapIndex);
		int mapY = getY(mapIndex);
		MAP *tile = getMapTile(mapIndex);
		
		// exclude targetted maps
		
		if (isTargettedLocation(tile))
			continue;
		
		if (mod_goody_at(mapX, mapY) != 0)
		{
			int range = map_range(transportVehicle->x, transportVehicle->y, mapX, mapY);
			
			if (range < minRange)
			{
				nearestPodLocation = tile;
				minRange = range;
			}
			
		}
		
	}
	
	// not found
	
	if (nearestPodLocation == nullptr)
		return false;
	
	// deliver scout
	
	transitVehicle(Task(scoutVehicleId, TT_MOVE, nearestPodLocation));
	
	return true;
	
}

bool pickupColony(int transportVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	MAP *transportVehicleTile = getVehicleMapTile(transportVehicleId);

	// search for nearest colony need ferry

	MAP *pickupLocation = nullptr;
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
				pickupLocation = getMapTile(otherVehicle->x, otherVehicle->y);
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
			setMoveTo(transportVehicleId, pickupLocation);
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

	MAP *pickupLocation = nullptr;
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
				pickupLocation = getMapTile(otherVehicle->x, otherVehicle->y);
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
			setMoveTo(transportVehicleId, pickupLocation);
		}

		// Former found
		return true;

	}

}

void popPodStrategy(int transportVehicleId)
{
	MAP *nearestPodLocation = getClosestPod(transportVehicleId);
	
	if (nearestPodLocation == nullptr)
		return;
	
	// go to destination
	
	setTask(Task(transportVehicleId, TT_MOVE, nearestPodLocation));

}

/*
Searches for unboard location.
*/
DeliveryLocation getSeaTransportDeliveryLocation(int seaTransportVehicleId, int passengerVehicleId, MAP *destination)
{
	assert(seaTransportVehicleId >= 0 && seaTransportVehicleId < *total_num_vehicles);
	assert(passengerVehicleId >= 0 && passengerVehicleId < *total_num_vehicles);
	assert(destination != nullptr);
	
	VEH *seaTransportVehicle = &(Vehicles[seaTransportVehicleId]);
	int seaTransportSpeed = getVehicleSpeedWithoutRoads(seaTransportVehicleId);
	int seaTransportVehicleAssociation = getVehicleAssociation(seaTransportVehicleId);
	VEH *passengerVehicle = &(Vehicles[passengerVehicleId]);
	int passengerSpeed = getVehicleSpeedWithoutRoads(passengerVehicleId);
	int destinationX = getX(destination);
	int destinationY = getY(destination);
	int destinationAssociation = getAssociation(destination, passengerVehicle->faction_id);
	int destinationOceanAssociation = getOceanAssociation(destination, seaTransportVehicle->faction_id);
	
	DeliveryLocation deliveryLocation;
	
	// destination is ocean with no base
	
	if (isOceanRegion(destination->region) && !map_has_item(destination, TERRA_BASE_IN_TILE))
		return deliveryLocation;
	
	// destination is same ocean association
	
	if (destinationOceanAssociation != -1 && destinationOceanAssociation == seaTransportVehicleAssociation)
	{
		deliveryLocation.unloadLocation = destination;
		deliveryLocation.unboardLocation = destination;
		return deliveryLocation;
	}
	
	// search for closest reachable location
	
	double minTravelTime = DBL_MAX;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		int tileX = getX(tile);
		int tileY = getY(tile);
		int tileOceanAssociation = getOceanAssociation(tile, seaTransportVehicle->faction_id);
		
		// skip not same ocean association
		
		if (tileOceanAssociation == -1 || tileOceanAssociation != seaTransportVehicleAssociation)
			continue;
		
		// get seaTransportTravelTime
		// increased by coefficient to make it more valuable as transport is shared between passengers
		
		int seaTransportRange = map_range(seaTransportVehicle->x, seaTransportVehicle->y, tileX, tileY);
		double seaTransportTravelTime = 1.5 * (double)seaTransportRange / (double)seaTransportSpeed;
		
		for (MAP *adjacentTile : getAdjacentTiles(tile))
		{
			int adjacentTileX = getX(adjacentTile);
			int adjacentTileY = getY(adjacentTile);
			int adjacentTileAssociation = getAssociation(adjacentTile, passengerVehicle->faction_id);
			
			// skip not land
			
			if (is_ocean(adjacentTile))
				continue;
			
			// get passengerTravelTime
			
			int passengerRange = map_range(adjacentTileX, adjacentTileY, destinationX, destinationY);
			double passengerTravelTime = (double)passengerRange / (double)passengerSpeed;
			
			// increase passengerTravelTime if unloaded to not the same association
			
			if (adjacentTileAssociation != destinationAssociation)
			{
				passengerTravelTime *= 3.0;
			}
			
			// calculate travelTime
			
			double travelTime = seaTransportTravelTime + passengerTravelTime;
			
			// update best
			
			if (travelTime < minTravelTime)
			{
				deliveryLocation.unloadLocation = tile;
				deliveryLocation.unboardLocation = adjacentTile;
				minTravelTime = travelTime;
			}
			
		}
		
	}
	
	return deliveryLocation;
	
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
		for (MAP *tile : getBaseAdjacentTiles(vehicle->x, vehicle->y, false))
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
int getCrossOceanAssociation(MAP *origin, int destinationAssociation, int factionId)
{
	bool originOcean = is_ocean(origin);
	int originAssociation = getAssociation(origin, factionId);
	
	// origin is ocean
	
	if (originOcean)
		return originAssociation;
	
	// same continent
	
	if (originAssociation == destinationAssociation)
		return -1;
	
	// immediately connected ocean
	
	if (isConnected(originAssociation, destinationAssociation, factionId))
		return destinationAssociation;
	
	robin_hood::unordered_flat_map<int, int> processedAssociations;
	robin_hood::unordered_flat_set<int> processingAssociations;
	
	for (int connection : getAssociationConnections(originAssociation, factionId))
	{
		processedAssociations.insert({connection, connection});
		processingAssociations.insert(connection);
	}
	
	while (processingAssociations.size() > 0)
	{
		robin_hood::unordered_flat_set<int> newProcessingAssociations;
		
		for (int processingAssociation : processingAssociations)
		{
			int crossAssociation = processedAssociations.at(processingAssociation);
			
			for (int connection : getAssociationConnections(processingAssociation, factionId))
			{
				if (connection == originAssociation)
					continue;
				
				if (processedAssociations.count(connection) != 0)
					continue;
				
				// exit condition
				
				if (connection == destinationAssociation)
				{
					return crossAssociation;
				}
				
				// update collections
				
				processedAssociations.insert({connection, crossAssociation});
				newProcessingAssociations.insert(connection);
				
			}
			
		}
		
		processingAssociations.clear();
		processingAssociations.swap(newProcessingAssociations);
		
	}
	
	return -1;
	
}

/*
Finds the first land association that is needed to be landed on in order to get to destination.
*/
int getFirstLandAssociation(int oceanAssociation, int destinationAssociation, int factionId)
{
	// same ocean
	
	if (oceanAssociation == destinationAssociation)
		return -1;
	
	// immediately connected land
	
	if (isConnected(oceanAssociation, destinationAssociation, factionId))
		return destinationAssociation;
	
	robin_hood::unordered_flat_map<int, int> processedAssociations;
	robin_hood::unordered_flat_set<int> processingAssociations;
	
	for (int connection : getAssociationConnections(oceanAssociation, factionId))
	{
		processedAssociations.insert({connection, connection});
		processingAssociations.insert(connection);
	}
	
	while (processingAssociations.size() > 0)
	{
		robin_hood::unordered_flat_set<int> newProcessingAssociations;
		
		for (int processingAssociation : processingAssociations)
		{
			int crossAssociation = processedAssociations.at(processingAssociation);
			
			for (int connection : getAssociationConnections(processingAssociation, factionId))
			{
				if (connection == oceanAssociation)
					continue;
				
				if (processedAssociations.count(connection) != 0)
					continue;
				
				// exit condition
				
				if (connection == destinationAssociation)
				{
					return crossAssociation;
				}
				
				// update collections
				
				processedAssociations.insert({connection, crossAssociation});
				newProcessingAssociations.insert(connection);
				
			}
			
		}
		
		processingAssociations.clear();
		processingAssociations.swap(newProcessingAssociations);
		
	}
	
	return -1;
	
}

/*
Searches for available sea transport in given association.
Full transport is not selected.
Transport without go to order is selected right away.
Transport with go to order is selected only if given location is closer than its current destination.
If such transport is found set its destination to the vehicle.
*/
int getAvailableSeaTransport(int association, int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// find closest available sea transport in this association
	
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
		
		if (seaTransportVehicleAssociation != association)
			continue;
		
		// with remaining cargo capacity
		
		if (getTransportRemainingCapacity(seaTransportVehicleId) <= 0)
			continue;
		
		// no more than 20% damaged
		
		if (seaTransportVehicle->damage_taken > (2 * seaTransportVehicle->reactor_type()))
			continue;
		
		// get range
		
		int range = map_range(seaTransportVehicle->x, seaTransportVehicle->y, vehicle->x, vehicle->y);
		
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

/*
Searches for available sea transport in vehicle stack.
*/
int getAvailableSeaTransportInStack(int vehicleId)
{
	// iterate vehicle stack
	
	for (int stackVehicleId : getStackVehicles(vehicleId))
	{
		VEH *stackVehicle = &(Vehicles[stackVehicleId]);
		
		// sea transport
		
		if (!(stackVehicle->triad() == TRIAD_SEA && isTransportVehicle(stackVehicleId)))
			continue;
		
		// available
		
		if (getTransportRemainingCapacity(stackVehicleId) == 0)
			continue;
		
		// do not disturb damaged transport
		
		if (stackVehicle->damage_taken > (2 * stackVehicle->reactor_type()))
			continue;
		
		// found
		
		return stackVehicleId;
		
	}
	
	// not found
	
	return -1;
	
}

MAP *getSeaTransportLoadLocation(int seaTransportVehicleId, int passengerVehicleId, MAP *destination)
{
	VEH *seaTransportVehicle = &(Vehicles[seaTransportVehicleId]);
	int seaTransportVehicleSpeed = getVehicleSpeedWithoutRoads(seaTransportVehicleId);
	VEH *passengerVehicle = &(Vehicles[passengerVehicleId]);
	MAP *passengerVehicleTile = getVehicleMapTile(passengerVehicleId);
	bool passengerVehicleTileOcean = is_ocean(passengerVehicleTile);
	int passengerVehicleSpeed = getVehicleSpeedWithoutRoads(passengerVehicleId);
	int destinationX = getX(destination);
	int destinationY = getY(destination);
	
	// default load location is passenger location
	
	MAP *loadLocation = getMapTile(passengerVehicle->x, passengerVehicle->y);
	
	// further location modification is for sea tranport picking land unit only
	
	if (seaTransportVehicle->triad() != TRIAD_SEA && passengerVehicle->triad() != TRIAD_LAND)
		return loadLocation;
	
	// get region associations
	
	int seaTransportAssociation = getVehicleAssociation(seaTransportVehicleId);
	int passengerAssociation = getVehicleAssociation(passengerVehicleId);
	
	// just in case they cannot be determined
	
	if (seaTransportAssociation == -1 || passengerAssociation == -1)
		return loadLocation;
	
	// find best load location for transport
	
	double minTravelTime = DBL_MAX;
	
	// land unit at ocean
	
	if (passengerVehicleTileOcean)
	{
		for (MAP *tile : getAdjacentTiles(passengerVehicleTile))
		{
			int x = getX(tile);
			int y = getY(tile);
			
			int tileOceanAssociation = getOceanAssociation(tile, seaTransportVehicle->faction_id);
			
			// sea transport association only
			
			if (tileOceanAssociation != seaTransportAssociation)
				continue;
			
			// compute range
			
			int rangeToDestination = map_range(x, y, destinationX, destinationY);
			double travelTimeToDestination = (double)rangeToDestination / (double)seaTransportVehicleSpeed;
			
			// update location
			
			if (travelTimeToDestination < minTravelTime)
			{
				loadLocation = tile;
				minTravelTime = travelTimeToDestination;
			}
			
		}
		
	}
	else
	{
		for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
		{
			MAP *tile = getMapTile(mapIndex);
			int x = getX(tile);
			int y = getY(tile);
			int tileOceanAssociation = getOceanAssociation(tile, seaTransportVehicle->faction_id);
			
			// exclude base
			
			if (map_has_item(tile, TERRA_BASE_IN_TILE))
				continue;
			
			// sea transport association only
			
			if (tileOceanAssociation != seaTransportAssociation)
				continue;
			
			// compute range
			
			int rangeToDestination = map_range(x, y, destinationX, destinationY);
			double travelTimeToDestination = (double)rangeToDestination / (double)seaTransportVehicleSpeed;
			
			// check adjacent tiles
			// including center tile to account for land ports
			
			for (MAP *adjacentTile : getBaseAdjacentTiles(tile, true))
			{
				int adjacentTileX = getX(adjacentTile);
				int adjacentTileY = getY(adjacentTile);
				int adjacentTileAssociation = getAssociation(adjacentTile, seaTransportVehicle->faction_id);
				
				// passenger association only
				
				if (adjacentTileAssociation != passengerAssociation)
					continue;
				
				// compute range and travel time
				
				int rangeToBoardLocation = map_range(passengerVehicle->x, passengerVehicle->y, adjacentTileX, adjacentTileY);
				double travelTimeToBoardLocation = (double)rangeToBoardLocation / (double)passengerVehicleSpeed;
				
				// get total travel time
				
				double totalTravelTime = travelTimeToBoardLocation + travelTimeToDestination;
				
				// update location
				
				if (totalTravelTime < minTravelTime)
				{
					loadLocation = tile;
					minTravelTime = totalTravelTime;
				}
				
			}
			
		}
		
	}
	
	return loadLocation;
	
}

