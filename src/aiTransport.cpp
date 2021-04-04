#include <float.h>
#include <unordered_map>
#include "aiTransport.h"
#include "game_wtp.h"

int moveTransport(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// only ship

	if (vehicle->triad() != TRIAD_SEA)
		return SYNC;

	// only transport

	if (!isVehicleTransport(vehicle))
		return SYNC;

	// deliver

	if (isCarryingColony(vehicleId))
	{
		transportColonyToClosestUnoccupiedCoast(vehicleId);
	}
	else if (isCarryingFormer(vehicleId))
	{
		transportFormerToMostDemandingLandmass(vehicleId);
	}
	else
	{
		// default to Thinker
		return trans_move(vehicleId);
	}

	return SYNC;

}

bool isCarryingColony(int vehicleId)
{
	for (int loadedVehicleId : getLoadedVehicleIds(vehicleId))
	{
		if (isVehicleColony(loadedVehicleId))
			return true;
	}

	return false;

}

void transportColonyToClosestUnoccupiedCoast(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleLocation = getVehicleMapTile(vehicleId);

	// search for closest uninhabitable coast

	LOCATION location;
	int minRange = INT_MAX;

	for (int x = 0; x < *map_axis_x; x++)
	for (int y = 0; y < *map_axis_y; y++)
	{
		MAP *tile = getMapTile(x, y);
		if (!tile) continue;

		// reachable coast only

		if (!isOceanRegionCoast(x, y, vehicleLocation->region))
			continue;

		// neutral or friendly outside of base radius only

		if (!(tile->owner == -1 || (tile->owner == vehicle->faction_id && !map_has_item(tile, TERRA_BASE_RADIUS))))
			continue;

		// update minimal range

		int range = map_range(vehicle->x, vehicle->y, x, y);

		if (range < minRange)
		{
			location = {x, y};
			minRange = range;
		}

	}

	// not found ?

	if (minRange == INT_MAX)
		return;

	if (minRange == 1)
	{
		// wake up colonies and move them to the coast

		for (int loadedVehicleId : getLoadedVehicleIds(vehicleId))
		{
			if (isVehicleColony(loadedVehicleId))
			{
				set_move_to(loadedVehicleId, location.x, location.y);
			}
		}

	}
	else
	{
		// move transport to destination

		set_move_to(vehicleId, location.x, location.y);

	}

}

bool isCarryingFormer(int vehicleId)
{
	for (int loadedVehicleId : getLoadedVehicleIds(vehicleId))
	{
		if (isVehicleFormer(loadedVehicleId))
			return true;
	}

	return false;

}

void transportFormerToMostDemandingLandmass(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleLocation = getVehicleMapTile(vehicleId);

	// search for reachable regions

	std::unordered_map<int, LOCATION_RANGE> regionUnloadLocations;

	for (int x = 0; x < *map_axis_x; x++)
	for (int y = 0; y < *map_axis_y; y++)
	{
		MAP *tile = getMapTile(x, y);
		if (!tile) continue;

		// reachable coast only

		if (!isOceanRegionCoast(x, y, vehicleLocation->region))
			continue;

		// update minimal range

		int range = map_range(vehicle->x, vehicle->y, x, y);

		if (regionUnloadLocations.count(tile->region) == 0)
		{
			regionUnloadLocations[tile->region] = {x, y, range};
		}
		else if (range < regionUnloadLocations[tile->region].range)
		{
			regionUnloadLocations[tile->region] = {x, y, range};
		}

	}

	// populate region former ratios

	std::unordered_map<int, int> regionBaseCounts;
	std::unordered_map<int, int> regionFormerCounts;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseLocation = getBaseMapTile(baseId);

		// only own bases

		if (base->faction_id != vehicle->faction_id)
			continue;

		// only reachable regions

		if (regionUnloadLocations.count(baseLocation->region) == 0)
			continue;

		// count base in region

		if (regionBaseCounts.count(baseLocation->region) == 0)
		{
			regionBaseCounts[baseLocation->region] = 0;
		}

		regionBaseCounts[baseLocation->region]++;

	}

	for (int formerVehicleId = 0; formerVehicleId < *total_num_bases; formerVehicleId++)
	{
		VEH *formerVehicle = &(Vehicles[formerVehicleId]);
		MAP *formerVehicleLocation = getVehicleMapTile(formerVehicleId);

		// only own vehicles

		if (formerVehicle->faction_id != vehicle->faction_id)
			continue;

		// only count regions where our bases are

		if (regionBaseCounts.count(formerVehicleLocation->region) == 0)
			continue;

		// count former in region

		if (regionFormerCounts.count(formerVehicleLocation->region) == 0)
		{
			regionFormerCounts[formerVehicleLocation->region] = 0;
		}

		regionFormerCounts[formerVehicleLocation->region]++;

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
		return;

	// retrive unload location

	LOCATION_RANGE location = regionUnloadLocations[mostDemandingRegion];

	// transport formers

	if (location.range == 1)
	{
		// wake up formers and move them to the coast

		for (int loadedVehicleId : getLoadedVehicleIds(vehicleId))
		{
			if (isVehicleFormer(loadedVehicleId))
			{
				set_move_to(loadedVehicleId, location.x, location.y);
			}
		}

	}
	else
	{
		// move transport to destination

		set_move_to(vehicleId, location.x, location.y);

	}


//	for (int vehicleId = 0)
//	for (int y = 0; y < *map_axis_y; y++)
//	{
//		MAP *tile = getMapTile(x, y);
//		if (!tile) continue;
//
//		// coast only
//
//		if (!isOceanRegionCoast(x, y, vehicleLocation->region))
//			continue;
//
//		// neutral or friendly outside of base radius only
//
//		if (!(tile->owner == -1 || (tile->owner == vehicle->faction_id && !map_has_item(tile, TERRA_BASE_RADIUS))))
//			continue;
//
//		// update minimal range
//
//		int range = map_range(vehicle->x, vehicle->y, x, y);
//
//		if (range < minRange)
//		{
//			location = {x, y};
//			minRange = range;
//		}
//
//	}
//
//	if (minRange == 1)
//	{
//		// wake up colonies and move them to the coast
//
//		for (int loadedVehicleId : getLoadedVehicleIds(vehicleId))
//		{
//			if (isVehicleColony(loadedVehicleId))
//			{
//				set_move_to(loadedVehicleId, location.x, location.y);
//			}
//		}
//
//	}
//	else if (minRange < INT_MAX)
//	{
//		// move transport to destination
//
//		set_move_to(vehicleId, location.x, location.y);
//
//	}
//
}

