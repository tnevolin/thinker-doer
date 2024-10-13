#include <float.h>
#include <map>
#include "wtp_game.h"
#include "wtp_ai.h"
#include "wtp_aiData.h"
#include "wtp_aiMove.h"
#include "wtp_aiMoveColony.h"
#include "wtp_aiMoveTransport.h"
#include "wtp_aiRoute.h"

void moveTranportStrategy()
{
	debug("moveTranportStrategy - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	executionProfiles["1.5.7. moveTranportStrategy"].start();
	
	// iterate sea transports
	
	for (robin_hood::pair<int, std::vector<int>> seaTransportVehicleIdEntry : aiData.seaTransportVehicleIds)
	{
		std::vector<int> seaClusterSeaTransportVehicleIds = seaTransportVehicleIdEntry.second;
		
		for (int vehicleId : seaClusterSeaTransportVehicleIds)
		{
			moveSeaTransportStrategy(vehicleId);
			
			if (!hasTask(vehicleId))
			{
				moveAvailableSeaTransportStrategy(vehicleId);
			}
			
		}
		
	}
	
	// generate transport requests
	
	for (TransitRequest &transitRequest : aiData.transportControl.transitRequests)
	{
		if (transitRequest.isFulfilled())
			continue;
		
		int seaTransportCluster = getSeaCluster(transitRequest.origin);
		
		debug
		(
			"addSeaTransportRequest unfullfilled request"
			" seaTransportCluster=%2d"
			" [%4d] %s->%s"
			"\n"
			, seaTransportCluster
			, transitRequest.getVehicleId()
			, getLocationString(transitRequest.origin).c_str()
			, getLocationString(transitRequest.destination).c_str()
		);
		
		aiData.addSeaTransportRequest(seaTransportCluster);
		
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
			
			debug("\t\t\t[%4d] %s\n", seaTransportVehicleId, getLocationString({seaTransportVehicle->x, seaTransportVehicle->y}).c_str());
			
			for (UnloadRequest unloadRequest : transportUnloadRequests)
			{
				int vehicleId = unloadRequest.getVehicleId();
				debug
				(
					"\t\t\t\t[%4d]->%s*>%s\n"
					, vehicleId
					, getLocationString(unloadRequest.destination).c_str()
					, getLocationString(unloadRequest.unboardLocation).c_str()
				);
				
			}
			
		}
		
		debug("\t\ttransitRequests\n");
		for (TransitRequest transitRequest : aiData.transportControl.transitRequests)
		{
			int vehicleId = transitRequest.getVehicleId();
			
			debug
			(
				"\t\t\t[%4d] %s->%s\n"
				, vehicleId
				, getLocationString(transitRequest.origin).c_str()
				, getLocationString(transitRequest.destination).c_str()
			);
			
			if (transitRequest.isFulfilled())
			{
				int seaTransportVehicleId = transitRequest.getSeaTransportVehicleId();
				VEH *seaTransportVehicle = getVehicle(seaTransportVehicleId);
				
				debug
				(
					"\t\t\t\t[%4d] %s\n"
					, seaTransportVehicleId
					, getLocationString({seaTransportVehicle->x, seaTransportVehicle->y}).c_str()
				);
				
			}
			
		}
		
	}
	
	executionProfiles["1.5.7. moveTranportStrategy"].stop();
	
}

void moveSeaTransportStrategy(int vehicleId)
{
	bool TRACE = DEBUG && false;

	debug("moveSeaTransportStrategy [%4d] %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str());

	// assign unload task

	MAP *closestUnloadLocation = nullptr;
	int closestUnloadLocationPreference = -1;
	double closestUnloadLocationTravelTime = DBL_MAX;

	for (UnloadRequest &unloadRequest : aiData.transportControl.getSeaTransportUnloadRequests(vehicleId))
	{
		int passengerVehicleId = unloadRequest.getVehicleId();
		double travelTime = getVehicleATravelTime(vehicleId, unloadRequest.destination);

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
			(preference == closestUnloadLocationPreference && travelTime < closestUnloadLocationTravelTime)
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
		if (TRACE) { debug("\tunload task: %s\n", getLocationString(closestUnloadLocation).c_str()); }
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
		double closestLoadLocationTravelTime = INT_MAX;

		for (TransitRequest &transitRequest : aiData.transportControl.transitRequests)
		{
			// not yet fulfilled

			if (transitRequest.isFulfilled())
				continue;

			double travelTime = getVehicleATravelTime(vehicleId, transitRequest.origin);

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
			if (TRACE) { debug("\tload task: %s\n", getLocationString(closestTransitRequest->origin).c_str()); }
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
		double currentTravelTime = getVehicleATravelTime(vehicleId, destination);

		TransitRequest *closestTransitRequest = nullptr;
		double closestLoadLocationTravelTime = currentTravelTime * 150 / 100;

		for (TransitRequest &transitRequest : aiData.transportControl.transitRequests)
		{
			// not yet fulfilled

			if (transitRequest.isFulfilled())
				continue;

			// reachable

			if (isVehicleDestinationReachable(vehicleId, transitRequest.destination))
				continue;

			// combined travel time

			double travelTime =
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
			if (TRACE) { debug("\tload task: %s\n", getLocationString(closestTransitRequest->origin).c_str()); }
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
	
	debug("\ndeliverFormer %s\n", getLocationString({transportVehicle->x, transportVehicle->y}).c_str());
	
	// search for reachable regions
	
	robin_hood::unordered_flat_set<int> reachableRegions;
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int mapX = getX(tile);
		int mapY = getY(tile);
		
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
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
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
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
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
	
	debug("\tdestinationLocation=%s\n", getLocationString({regionClosestBase->x, regionClosestBase->y}).c_str());
	
	// transport vehicles
	
	transitVehicle(Task(formerVehicleId, TT_HOLD, getMapTile(regionClosestBase->x, regionClosestBase->y)));
	
	return true;
	
}

bool deliverScout(int transportVehicleId, int scoutVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);

	debug("\ndeliverScount %s\n", getLocationString({transportVehicle->x, transportVehicle->y}).c_str());

	// find nearest pod

	MAP *nearestPodLocation = nullptr;
	int minRange = INT_MAX;

	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int mapX = getX(tile);
		int mapY = getY(tile);

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

	for (int otherVehicleId = 0; otherVehicleId < *VehCount; otherVehicleId++)
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
			map_has_item(otherVehicleTile, BIT_BASE_IN_TILE)
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

	for (int otherVehicleId = 0; otherVehicleId < *VehCount; otherVehicleId++)
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
			map_has_item(otherVehicleTile, BIT_BASE_IN_TILE)
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

