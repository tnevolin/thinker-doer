#include <float.h>
#include <unordered_map>
#include "aiTransport.h"
#include "game_wtp.h"
#include "aiColony.h"

int moveTransport(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	// only ship

	if (vehicle->triad() != TRIAD_SEA)
		return SYNC;

	// only transport

	if (!isVehicleTransport(vehicle))
		return SYNC;

	// transport colony
	if (isCarryingColony(vehicleId))
	{
		transportColony(vehicleId);
		return SYNC;
	}

	// transport former
	if (isCarryingFormer(vehicleId))
	{
		transportFormer(vehicleId);
		return SYNC;
	}

	// other unit types - default to thinker
	if (isCarryingVehicle(vehicleId))
	{
		return trans_move(vehicleId);
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

	// default to Thinker
	return trans_move(vehicleId);

}

bool isCarryingColony(int vehicleId)
{
	for (int loadedVehicleId : getLoadedVehicleIds(vehicleId))
	{
		if (isColonyVehicle(loadedVehicleId))
			return true;
	}

	return false;

}

bool isCarryingFormer(int vehicleId)
{
	for (int loadedVehicleId : getLoadedVehicleIds(vehicleId))
	{
		if (isFormerVehicle(loadedVehicleId))
			return true;
	}

	return false;

}

bool isCarryingVehicle(int vehicleId)
{
	return getLoadedVehicleIds(vehicleId).size() > 0;

}

bool transportColony(int transportVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	MAP *transportVehicleLocation = getVehicleMapTile(transportVehicleId);

	// find best base location

	Location buildLocation = findTransportLandBaseBuildLocation(transportVehicleId);

	if (buildLocation.x == -1 || buildLocation.y == -1)
		return false;

	// get adjacent region location

	MAP *buildLocationTile = getMapTile(buildLocation.x, buildLocation.y);
	Location adjacentRegionLocation = getAdjacentRegionLocation(transportVehicle->x, transportVehicle->y, buildLocationTile->region);

	// transport is next to destination region
	if (isValidLocation(adjacentRegionLocation))
	{
		// stop ship

		veh_skip(transportVehicleId);

		// wake up colonies and move them to the coast

		for (int loadedVehicleId : getLoadedVehicleIds(transportVehicleId))
		{
			if (isColonyVehicle(loadedVehicleId))
			{
				set_move_to(loadedVehicleId, adjacentRegionLocation.x, adjacentRegionLocation.y);
			}
		}

	}
	else
	{
		// put skipped units at hold if at base to not pick them up

		if (map_base(transportVehicleLocation))
		{
			for (int stackedVehicleId : getStackedVehicleIds(transportVehicleId))
			{
				VEH *stackedVehicle = &(Vehicles[stackedVehicleId]);

				// skip self

				if (stackedVehicleId == transportVehicleId)
					continue;

				if
				(
					veh_triad(stackedVehicleId) == TRIAD_LAND
					&&
					stackedVehicle->move_status == ORDER_NONE
				)
				{
					stackedVehicle->move_status = ORDER_HOLD;
				}

			}
		}

		// move to destination

		set_move_to(transportVehicleId, buildLocation.x, buildLocation.y);

	}

	return true;

}

void transportFormer(int transportVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);
	MAP *transportVehicleLocation = getVehicleMapTile(transportVehicleId);

	// search for reachable regions

	std::unordered_map<int, LOCATION_RANGE> regionUnloadLocations;

	for (int x = 0; x < *map_axis_x; x++)
	for (int y = 0; y < *map_axis_y; y++)
	{
		MAP *tile = getMapTile(x, y);
		if (!tile) continue;

		// reachable coast only

		if (!isOceanRegionCoast(x, y, transportVehicleLocation->region))
			continue;

		// update minimal range

		int range = map_range(transportVehicle->x, transportVehicle->y, x, y);

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

		if (base->faction_id != transportVehicle->faction_id)
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

	for (int formerVehicleId = 0; formerVehicleId < *total_num_vehicles; formerVehicleId++)
	{
		VEH *formerVehicle = &(Vehicles[formerVehicleId]);
		MAP *formerVehicleLocation = getVehicleMapTile(formerVehicleId);

		// only own transportVehicles

		if (formerVehicle->faction_id != transportVehicle->faction_id)
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
		// stop ship

		veh_skip(transportVehicleId);

		// wake up formers and move them to the coast

		for (int loadedVehicleId : getLoadedVehicleIds(transportVehicleId))
		{
			if (isFormerVehicle(loadedVehicleId))
			{
				set_move_to(loadedVehicleId, location.x, location.y);
			}
		}

	}
	else
	{
		// put skipped units at hold if at base to not pick them up

		if (map_base(transportVehicleLocation))
		{
			for (int stackedVehicleId : getStackedVehicleIds(transportVehicleId))
			{
				VEH *stackedVehicle = &(Vehicles[stackedVehicleId]);

				// skip self

				if (stackedVehicleId == transportVehicleId)
					continue;

				if
				(
					veh_triad(stackedVehicleId) == TRIAD_LAND
					&&
					stackedVehicle->move_status == ORDER_NONE
				)
				{
					stackedVehicle->move_status = ORDER_HOLD;
				}

			}
		}

		// move to destination

		set_move_to(transportVehicleId, location.x, location.y);

	}

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

		}
		// not at destination - go to destination
		else
		{
			set_move_to(transportVehicleId, pickupLocation.x, pickupLocation.y);
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

		}
		// not at destination - go to destination
		else
		{
			set_move_to(transportVehicleId, pickupLocation.x, pickupLocation.y);
		}

		// Former found
		return true;

	}

}

