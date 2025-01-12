#pragma GCC diagnostic ignored "-Wshadow"

#include <algorithm>
#include "wtp_aiMove.h"
#include "wtp_terranx.h"
#include "wtp_ai.h"
#include "wtp_aiData.h"
#include "wtp_aiMoveArtifact.h"
#include "wtp_aiMoveColony.h"
#include "wtp_aiMoveCombat.h"
#include "wtp_aiMoveFormer.h"
#include "wtp_aiProduction.h"
#include "wtp_aiMoveTransport.h"
#include "wtp_aiRoute.h"

// SeaTransit

void SeaTransit::set(Transfer *_boardTransfer, Transfer *_unboardTransfer, double _travelTime)
{
	this->valid = true;
	this->boardTransfer = _boardTransfer;
	this->unboardTransfer = _unboardTransfer;
	this->travelTime = _travelTime;
}

void SeaTransit::copy(SeaTransit &seaTransit)
{
	this->boardTransfer = seaTransit.boardTransfer;
	this->unboardTransfer = seaTransit.unboardTransfer;
	this->travelTime = seaTransit.travelTime;
}

// AI functions

void moveStrategy()
{
	debug("moveStrategy - %s\n", MFactions[aiFactionId].noun_faction);

	executionProfiles["1.5. moveStrategy"].start();
	
	// vanilla fix for transport dropoff

	fixUndesiredTransportDropoff();

	// generic

	moveAllStrategy();

	// artifact

	moveArtifactStrategy();

	// combat

	moveCombatStrategy();

	// colony

	moveColonyStrategy();

	// former

	moveFormerStrategy();

	// transport

	moveTranportStrategy();

	// vanilla fix for transpoft pickup

	fixUndesiredTransportPickup();

	executionProfiles["1.5. moveStrategy"].stop();
	
}

/*
vanilla enemy_strategy clears up SENTRY/BOARD status from ground unit in land port
restore boarded units order back to SENTRY/BOARD so they be able to coninue transported
*/
void fixUndesiredTransportDropoff()
{
	executionProfiles["1.5.1. fixUndesiredTransportDropoff"].start();
	
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// verify transportVehicleId is set

		if (!((vehicle->state & VSTATE_IN_TRANSPORT) != 0 && vehicle->waypoint_1_y == 0 && vehicle->waypoint_1_x >= 0))
			continue;

		// get transportVehicleId

		int transportVehicleId = vehicle->waypoint_1_x;

		// verify transportVehicleId is within range

		if (!(transportVehicleId >= 0 && transportVehicleId < *VehCount))
			continue;

		// get transport vehicle

		VEH *transportVehicle = &(Vehicles[transportVehicleId]);

		// verify transportVehicle is in same location

		if (!(transportVehicle->x == vehicle->x && transportVehicle->y == vehicle->y))
			continue;

		// restore status

		setVehicleOrder(vehicleId, ORDER_SENTRY_BOARD);

	}

	executionProfiles["1.5.1. fixUndesiredTransportDropoff"].stop();
	
}

/*
AI transport picks up idle units from port
set their order to hold to prevent this
*/
void fixUndesiredTransportPickup()
{
	executionProfiles["1.5.8. fixUndesiredTransportPickup"].start();
	
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		// verify vehicle is at own base

		if (!(vehicleTile->owner == aiFactionId && map_has_item(vehicleTile, BIT_BASE_IN_TILE)))
			continue;

		// verify vehicle is idle

		if (!(vehicle->order == ORDER_NONE))
			continue;

		// put vehicle on hold

		setVehicleOrder(vehicleId, ORDER_HOLD);

	}

	executionProfiles["1.5.8. fixUndesiredTransportPickup"].stop();
	
}

void moveAllStrategy()
{
	executionProfiles["1.5.2. moveAllStrategy"].start();
	
	// heal

	healStrategy();

	executionProfiles["1.5.2. moveAllStrategy"].stop();
	
}

void healStrategy()
{
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		// exclude ogres

		if (vehicle->unit_id == BSC_BATTLE_OGRE_MK1 || vehicle->unit_id == BSC_BATTLE_OGRE_MK2 || vehicle->unit_id == BSC_BATTLE_OGRE_MK3)
			continue;

		// exclude artifact

		if (isArtifactVehicle(vehicleId))
			continue;

		// exclude colony

		if (isColonyVehicle(vehicleId))
			continue;

		// exclude former

		if (isFormerVehicle(vehicleId))
			continue;

		// exclude loaded transport

		if (isTransportVehicle(vehicleId) && getTransportUsedCapacity(vehicleId) > 0)
			continue;

		// exclude combat

		if (isCombatVehicle(vehicleId))
			continue;

		// at base

		if (map_has_item(vehicleTile, BIT_BASE_IN_TILE))
		{
			// exclude undamaged

			if (vehicle->damage_taken == 0)
				continue;

			// exclude under alien artillery bombardment

			if (isWithinAlienArtilleryRange(vehicleId))
				continue;

			// heal

			setTask(Task(vehicleId, TT_HOLD));

		}

		// in the field

		else
		{
			// exclude irrepairable

			if (vehicle->damage_taken <= (isFactionHasProject(aiFactionId, FAC_NANO_FACTORY) ? 0 : 2 * vehicle->reactor_type()))
				continue;

			// find nearest monolith

			MAP *nearestMonolith = getNearestMonolith(vehicle->x, vehicle->y, vehicle->triad());

			if (nearestMonolith != nullptr && map_range(vehicle->x, vehicle->y, getX(nearestMonolith), getY(nearestMonolith)) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
				setTask(Task(vehicleId, TT_MOVE, nearestMonolith));
				continue;
			}

			// find nearest base

			MAP *nearestBase = getNearestFriendlyBase(vehicleId);

			if (nearestBase != nullptr && map_range(vehicle->x, vehicle->y, getX(nearestBase), getY(nearestBase)) <= (vehicle->triad() == TRIAD_SEA ? 10 : 5))
			{
				setTask(Task(vehicleId, TT_HOLD, nearestBase));
				continue;
			}

			// find nearest not warzone tile

			MAP *healingTile = nullptr;

			for (MAP *tile : getRangeTiles(vehicleTile, 9, true))
			{
				TileInfo &tileInfo = aiData.getTileInfo(tile);

				// exclude warzone

				if (tileInfo.warzone)
					continue;

				// select healingTile

				healingTile = tile;
				break;

			}

			if (healingTile == nullptr)
			{
				healingTile = vehicleTile;
			}

			// heal

			transitVehicle(Task(vehicleId, TT_HOLD, healingTile));

		}

	}

}

/*
Top level AI enemy move entry point

Transporting land vehicles by sea transports idea.

- Land vehicle not on transport
Land vehicle moves wherever it wants regardless if destination is reachable.
Land vehicle waits ferry pickup when it is at ocean or at the coast and wants to get to another land region.

- Empty transport
Empty transport rushes to nearest higher priority vehicle waiting for ferry.
When at or next to awaiting land vehicle it stops and explicitly commands passenger to board.

- Land vehicle on transport
Land vehicle on transport is not controlled by this AI code.

- Loaded transport
Loaded transport selects highest priority passenger.
Loaded transport then request highest priority passenger to generate desired destination.
Loaded transport then computes unload location closest to passenger destination location.
When at or next unload location it stops and explicitly commands passenger to unboard to unload location.

*/
int lastEnemyMoveVehicleId = -1;
int lastEnemyMoveVehicleX = -1;
int lastEnemyMoveVehicleY = -1;
int enemyMoveVehicle(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	debug("enemyMoveVehicle [%4d] %s %2d %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), getVehicleRemainingMovement(vehicleId), Units[vehicle->unit_id].name);
	
	// fall over to default if vehicle did not move since last iteration
	
	if (vehicle->order == ORDER_MOVE_TO && vehicleId == lastEnemyMoveVehicleId && vehicle->x == lastEnemyMoveVehicleX && vehicle->y == lastEnemyMoveVehicleY)
	{
		return enemy_move(vehicleId);
	}
	
	// set last values
	
	lastEnemyMoveVehicleId = vehicleId;
	lastEnemyMoveVehicleX = vehicle->x;
	lastEnemyMoveVehicleY = vehicle->y;
	
	// execute task
	
	if (hasExecutableTask(vehicleId))
	{
		return executeTask(vehicleId);
	}
	
    // use vanilla algorithm for probes
	
    if (isProbeVehicle(vehicleId))
	{
		return enemy_move(vehicleId);
	}
	
	// unhandled cases handled by default
	
	return enemy_move(vehicleId);
	
}

/**
Creates tasks to transit vehicle to destination.
Returns true on successful task creation.
*/
bool transitVehicle(Task task)
{
	const bool TRACE = DEBUG && false;

	int vehicleId = task.getVehicleId();
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MAP *destination = task.getDestination();

	if (destination == nullptr)
		return false;

	if (TRACE)
	{
		debug
		(
			"transitVehicle"
			" [%4d]"
			" %s->%s"
			"\n"
			, vehicleId
			, getLocationString(vehicleTile).c_str(), getLocationString(destination).c_str()
		);
	}

	// vehicle is already at destination

	if (vehicleTile == destination)
	{
		if (TRACE)
		{
			debug("\tat destination\n");
		}
		setTask(task);
		return true;
	}

	// process triads

	bool success = false;

	// sea and air units do not need sophisticated transportation algorithms

	switch (vehicle->triad())
	{
	case TRIAD_AIR:
		{
			if (TRACE)
			{
				debug("\tair unit\n");
			}
			setTask(task);
			success = true;
		}
		break;

	case TRIAD_SEA:
		{
			if (TRACE)
			{
				debug("\tsea unit\n");
			}
			if (isSameSeaCluster(vehicleTile, destination))
			{
				if (TRACE)
				{
					debug("\t\tsame sea cluster\n");
				}
				setTask(task);
				success = true;
			}
			else
			{
				if (TRACE)
				{
					debug("\t\tdifferent sea cluster\n");
				}
				success = false;
			}
		}
		break;

	case TRIAD_LAND:
		success = transitLandVehicle(task);
		break;

	}

	return success;

}

bool transitLandVehicle(Task task)
{
	int vehicleId = task.getVehicleId();
	MAP *destination = task.getDestination();
	
	assert(vehicleId >= 0 && vehicleId < *VehCount);
	assert(destination >= *MapTiles && destination < *MapTiles + *MapAreaTiles);
	
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	TileInfo &vehicleTileInfo = aiData.getVehicleTileInfo(vehicleId);
	
	// land vehicle
	
	if (vehicle->triad() != TRIAD_LAND)
		return false;
	
	// valid destination
	
	if (destination == nullptr)
		return false;
	
	// transport
	
	int transportId = getVehicleTransportId(vehicleId);
	
	if (transportId != -1)
	{
		// sea transport
		
		if (!isSeaTransportVehicle(transportId))
			return false;
		
		// find dropoff transfer
		
		Transfer transfer = getOptimalDropoffTransfer(vehicleTile, destination, vehicleId, transportId);
		
		if (!transfer.valid())
			return false;
		
		// set unboarding task and add unload request
		
		task.type = TT_UNBOARD;
		task.destination = transfer.passengerStop;
		setTask(task);
		aiData.transportControl.addUnloadRequest(transportId, vehicleId, transfer.transportStop, transfer.passengerStop);
		
		return true;
		
	}
	else if (!vehicleTileInfo.ocean && isSameLandCluster(vehicleTile, destination)) // vehicle is on land and is moving to same land cluster
	{
		setTask(task);
		return true;
	}
	else // requires transportation
	{
		// sometimes game may leave land vehicle in the water without transport - ignore this case
		
		if (!vehicleTileInfo.landAllowed)
			return false;
		
		// find pickup transfer
		
		Transfer transfer = getOptimalPickupTransfer(vehicleTile, destination);
		
		if (!transfer.valid())
			return false;
		
		// handle transit
		
		task.type = TT_BOARD;
		task.destination = transfer.passengerStop;
		setTask(task);
		
		if (vehicleTile == transfer.passengerStop)
		{
			// request transportation
			
			aiData.transportControl.transitRequests.emplace_back
			(
				vehicleId,
				transfer.transportStop,
				destination
			);
			
		}
		
		return true;
		
	}
	
	return false;
	
}

/**
Redistributes vehicle between bases to proportinally burden them with support.
*/
void balanceVehicleSupport()
{
	debug("balanceVehicleSupport - %s\n", aiMFaction->noun_faction);
	
	executionProfiles["1.3. balanceVehicleSupport"].start();
	
	// find base with lowest/highest mineral surplus
	
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = getBase(baseId);
		
		// player faction base
		
		if (base->faction_id != aiFactionId)
			continue;
		
		int support = base->mineral_intake_2 - base->mineral_surplus;
		
		if (support > 0 && base->mineral_surplus <= 1)
		{
			// find vehicle to reassign
			
			int reassignVehicleId = -1;
			
			for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
			{
				VEH *vehicle = &(Vehicles[vehicleId]);
				
				// this home base
				
				if (vehicle->home_base_id != baseId)
					continue;
				
				reassignVehicleId = vehicleId;
				break;
				
			}
			
			// not found
			
			if (reassignVehicleId == -1)
				continue;
			
			// find base to reassign to
			
			for (int otherBaseId = 0; otherBaseId < *BaseCount; otherBaseId++)
			{
				BASE *otherBase = getBase(otherBaseId);
				
				// player faction base
				
				if (otherBase->faction_id != aiFactionId)
					continue;
				
				// exclude self
				
				if (otherBaseId == baseId)
					continue;
				
				// exclude project base
				
				if (isBaseBuildingProject(otherBaseId))
					continue;
				
				// compute relative support
				
				double otherBaseRelativeSupport = 1.0 - (double)otherBase->mineral_surplus / (double)otherBase->mineral_intake_2;
				
				// check support/surplus
				
				if (otherBase->mineral_surplus >= 4 && otherBaseRelativeSupport < conf.ai_production_support_ratio)
				{
					// reassign

					getVehicle(reassignVehicleId)->home_base_id = otherBaseId;
					
					// compute both bases
					
					computeBase(baseId, false);
					computeBase(otherBaseId, false);
					
					// exit cycle
					
					break;
					
				}
				
			}
			
		}
		
	}
	
	executionProfiles["1.3. balanceVehicleSupport"].start();
	
}

// ==================================================
// enemy_move entry point
// ==================================================

/*
Modified vehicle movement.
*/
int __cdecl modified_enemy_move(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	int returnValue;
	
	// choose AI logic
	
	// run WTP AI code for AI eanbled factions
	if (isWtpEnabledFaction(vehicle->faction_id))
	{
		executionProfiles["| enemyMoveVehicle"].start();
		returnValue = enemyMoveVehicle(vehicleId);
		executionProfiles["| enemyMoveVehicle"].stop();
	}
	// default
	else
	{
		executionProfiles["~ mod_enemy_move"].start();
		returnValue = mod_enemy_move(vehicleId);
		executionProfiles["~ mod_enemy_move"].stop();
	}
	
	// return
	
	return returnValue;
	
}

/*
Searches for allied base where vehicle can get into.
*/
MAP *getNearestFriendlyBase(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int triad = vehicle->triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	MAP *nearestBaseLocation = nullptr;
	
	// iterate bases
	
	int minRange = INT_MAX;
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		// friendly base only
		
		if (!isFriendly(aiFactionId, base->faction_id))
			continue;
		
		// same cluster for surface vehicle
		
		switch (triad)
		{
		case TRIAD_SEA:
			
			if (!isSameSeaCluster(vehicleTile, baseTile))
				continue;
			
			break;
			
		case TRIAD_LAND:
			
			if (!isSameLandCluster(vehicleTile, baseTile))
				continue;
			
			break;
			
		}
		
		int range = map_range(vehicle->x, vehicle->y, base->x, base->y);
		
		if (range < minRange)
		{
			nearestBaseLocation = baseTile;
			minRange = range;
		}
		
	}
	
	return nearestBaseLocation;
	
}

/*
Searches for nearest accessible monolith on non-enemy territory.
*/
MAP *getNearestMonolith(int x, int y, int triad)
{
	MAP *startingTile = getMapTile(x, y);
	bool ocean = is_ocean(startingTile);

	MAP *nearestMonolithLocation = nullptr;

	// search only for realm unit can move on

	if (!(triad == TRIAD_AIR || triad == (ocean ? TRIAD_SEA : TRIAD_LAND)))
		return nullptr;

	int minRange = INT_MAX;
	for (MAP* tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int mapX = getX(tile);
		int mapY = getY(tile);

		// same region for non-air units

		if (triad != TRIAD_AIR && tile->region != startingTile->region)
			continue;

		// friendly territory only

		if (tile->owner != -1 && tile->owner != aiFactionId && isVendetta(tile->owner, aiFactionId))
			continue;

		// monolith

		if (!map_has_item(tile, BIT_MONOLITH))
			continue;

		int range = map_range(x, y, mapX, mapY);

		if (range < minRange)
		{
			nearestMonolithLocation = tile;
			minRange = range;
		}

	}

	return nearestMonolithLocation;

}

Transfer getOptimalPickupTransfer(MAP *org, MAP *dst)
{
	debug("getOptimalPickupTransfer %s -> %s\n", getLocationString(org).c_str(), getLocationString(dst).c_str());
	
	TileInfo &orgTileInfo = aiData.getTileInfo(org);
	
	// populate available transfers
	
	std::vector<Transfer> transfers;
	
	if (orgTileInfo.ocean && orgTileInfo.base) // org is sea base
	{
		std::vector<Transfer> const &oceanBaseTransfers = getOceanBaseTransfers(org);
		transfers.insert(transfers.end(), oceanBaseTransfers.begin(), oceanBaseTransfers.end());
		
	}
	else if (!orgTileInfo.ocean) // org is land
	{
		int orgCluster = getCluster(org);
		
		for (int firstConnectedCluster : getFirstConnectedClusters(org, dst))
		{
			std::vector<Transfer> const &firstConnectedClusterTransfers = getTransfers(orgCluster, firstConnectedCluster);
			transfers.insert(transfers.end(), firstConnectedClusterTransfers.begin(), firstConnectedClusterTransfers.end());
		}
		
	}
	
	// select best transfer
	
	Transfer bestTransfer;
	int bestTransferTotalRange = INT_MAX;
	
	for (Transfer const &transfer : transfers)
	{
		int passengerStopRange = getRange(org, transfer.passengerStop);
		int transportStopRange = getRange(transfer.transportStop, dst);
		int totalRange = passengerStopRange + transportStopRange;
		
		if (totalRange < bestTransferTotalRange)
		{
			bestTransfer = transfer;
			bestTransferTotalRange = totalRange;
		}
		
	}
	
	return bestTransfer;
	
}

Transfer getOptimalDropoffTransfer(MAP *org, MAP *dst, int passengerVehicleId, int transportVehicleId)
{
	debug("getOptimalDropoffTransfer %s -> %s\n", getLocationString(org).c_str(), getLocationString(dst).c_str());
	
	TileInfo &dstTileInfo = aiData.getTileInfo(dst);
	int passengerSpeed = getVehicleSpeed(passengerVehicleId);
	int transportSpeed = getVehicleSpeed(transportVehicleId);
	
	// populate available transfers
	
	std::vector<Transfer> transfers;
	
	if (isSameSeaCluster(org, dst) && dstTileInfo.ocean && dstTileInfo.base) // dst is in same sea cluster and dst is sea base
	{
		std::vector<Transfer> const &oceanBaseTransfers = getOceanBaseTransfers(dst);
		transfers.insert(transfers.end(), oceanBaseTransfers.begin(), oceanBaseTransfers.end());
		
	}
	else // all other cases
	{
		int orgCluster = getCluster(org);
		
		for (int firstConnectedCluster : getFirstConnectedClusters(org, dst))
		{
			std::vector<Transfer> const &firstConnectedClusterTransfers = getTransfers(firstConnectedCluster, orgCluster);
			transfers.insert(transfers.end(), firstConnectedClusterTransfers.begin(), firstConnectedClusterTransfers.end());
		}
		
	}
	
	// select best transfer
	
	Transfer bestTransfer;
	double bestTransferTotalTravelTime = DBL_MAX;
	
	for (Transfer const &transfer : transfers)
	{
		// range to corresponding stops
		
		int transportStopRange = getRange(org, transfer.transportStop);
		int passengerStopRange = getRange(transfer.passengerStop, dst);
		
		// estimated travel time
		
		double transportTravelTime = (double)transportStopRange / (double)transportSpeed;
		double passengerTravelTime = (double)passengerStopRange / (double)passengerSpeed;
		
		double totalTravelTime = transportTravelTime + passengerTravelTime;
		
		if (totalTravelTime < bestTransferTotalTravelTime)
		{
			bestTransfer = transfer;
			bestTransferTotalTravelTime = totalTravelTime;
		}
		
		debug
		(
			"\t%s -> %s"
			" transportStopRange=%3d"
			" passengerStopRange=%3d"
			" transportTravelTime=%5.2f"
			" passengerTravelTime=%5.2f"
			" totalTravelTime=%5.2f"
			"\n"
			, getLocationString(transfer.transportStop).c_str(), getLocationString(transfer.passengerStop).c_str()
			, transportStopRange
			, passengerStopRange
			, transportTravelTime
			, passengerTravelTime
			, totalTravelTime
		);
		
	}
	
	return bestTransfer;
	
}

/*
Sends vehicle along safest travel waypoints.
*/
void setSafeMoveTo(int vehicleId, MAP *destination)
{
	debug("setSafeMoveTo [%4d] -> %s\n", vehicleId, getLocationString(destination).c_str());
	
	MAP *bestTile = destination;
	double bestTileWeight = DBL_MAX;
	
	bool dangerous = false;
	
	for (MoveAction &reachableLocation : getVehicleReachableLocations(vehicleId))
	{
		MAP *tile = reachableLocation.tile;
		double danger = getVehicleTileDanger(vehicleId, tile);
		
		double distance = getEuqlideanDistance(tile, destination);
		double weight = danger + 0.2 * distance;
		
		if (danger > 0.0)
		{
			dangerous = true;
		}
		
		if (weight < bestTileWeight)
		{
			bestTile = tile;
			bestTileWeight = weight;
		}
		
//		debug
//		(
//			"%s"
//			" danger=%5.2f"
//			" distance=%5.2f"
//			" weight=%5.2f"
//			" dangerous=%d"
//			" best=%d"
//			"\n"
//			, getLocationString(tile).c_str()
//			, danger
//			, distance
//			, weight
//			, dangerous
//			, weight < bestTileWeight
//		);
	
	}
	
	if (!dangerous || bestTile == destination)
	{
		setMoveTo(vehicleId, destination);
	}
	else
	{
		setMoveTo(vehicleId, {bestTile, destination});
	}
	
}

/**
Searches for item closest to origin within given realm withing given range.
realm: 0 = land, 1 = ocean, 2 = both
Ignore blocked locations and warzones if requested.
factionId = aiFactionId
*/
MapDoubleValue findClosestItemLocation(int vehicleId, MapItem item, int maxSearchRange, bool avoidWarzone)
{
	VEH *vehicle = getVehicle(vehicleId);
	int triad = vehicle->triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	MAP *closestItemLocation = nullptr;
	double closestItemLocationTravelTime = DBL_MAX;
	
	for (MAP *tile : getRangeTiles(vehicleTile, maxSearchRange, true))
	{
		bool ocean = is_ocean(tile);
		
		// corresponding realm
		
		if ((triad == TRIAD_LAND && ocean) || (triad == TRIAD_SEA && !ocean))
			continue;
		
		// item
		
		if (!map_has_item(tile, item))
			continue;
		
		// exclude blocked location
		
		if (isBlocked(tile))
			continue;
		
		// exclude warzone if requested
		
		if (avoidWarzone && isWarzone(tile))
			continue;
		
		// get travel time
		
		double travelTime = getVehicleATravelTime(vehicleId, tile);
		
		if (travelTime == INF)
			continue;
		
		// update best
		
		if (travelTime < closestItemLocationTravelTime)
		{
			closestItemLocation = tile;
			closestItemLocationTravelTime = travelTime;
		}
		
	}
	
	return {closestItemLocation, closestItemLocationTravelTime};
	
}

/*
Searches for closest friendly base or not blocked bunker withing range if range is given.
Otherwies, searches for any closest safe location (above options + not warzone).
*/
MAP *getSafeLocation(int vehicleId, int baseRange)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int triad = vehicle->triad();
	
	// exclude transported land vehicle
	
	if (isLandVehicleOnTransport(vehicleId))
		return nullptr;
	
	// do not search for safe location for vehicles with range
	// they have to return to safe location anyway
	
	if (getVehicleChassis(vehicleId)->range > 0)
		return nullptr;
	
	// search best safe location (base, bunker)
	
	for (MAP *tile : getRangeTiles(vehicleTile, baseRange, true))
	{
		// in same cluster for surface vechile
		
		switch (triad)
		{
		case TRIAD_SEA:
			
			if (!isSameSeaCluster(vehicleTile, tile))
				continue;
			
			break;
			
		case TRIAD_LAND:
			
			if (!isSameLandCluster(vehicleTile, tile))
				continue;
			
			break;
			
		}
		
		// search for best safe location if not found yet
		
		if (map_has_item(tile, BIT_BASE_IN_TILE) && isFriendly(vehicle->faction_id, tile->owner))
		{
			// friendly base is the safe location
			
			return tile;
			
		}
		
		if ((triad == TRIAD_LAND || triad == TRIAD_SEA) && map_has_item(tile, BIT_BUNKER) && !isBlocked(tile))
		{
			// not blocked bunker is a safe location for surface vehicle
			
			return tile;
			
		}
		
	}
	
	// search closest safe location
	
	MAP *closestSafeLocation = nullptr;
	double closestSafeLocationRange = DBL_MAX;
	double closestSafeLocationTravelTime = DBL_MAX;
	
	for (MAP *tile : getRangeTiles(vehicleTile, MAX_SAFE_LOCATION_SEARCH_RANGE, true))
	{
		int x = getX(tile);
		int y = getY(tile);
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		// in same cluster for surface vechile
		
		switch (triad)
		{
		case TRIAD_SEA:
			
			if (!isSameSeaCluster(vehicleTile, tile))
				continue;
			
			break;
			
		case TRIAD_LAND:
			
			if (!isSameLandCluster(vehicleTile, tile))
				continue;
			
			break;
			
		}
		
		if (map_has_item(tile, BIT_BASE_IN_TILE) && isFriendly(vehicle->faction_id, tile->owner))
		{
			// base is a best safe location
		}
		else if ((triad == TRIAD_LAND || triad == TRIAD_SEA) && map_has_item(tile, BIT_BUNKER) && !isBlocked(tile))
		{
			// not blocked bunker is a safe location for surface vehicle
		}
		else
		{
			// not blocked
			
			if (isBlocked(tile))
				continue;
			
			// not zoc
			
			if (isZoc(tile))
				continue;
			
			// not warzone
			
			if (tileInfo.warzone)
				continue;
			
			// not fungus for surface vehicle unless native or XENOEMPATY_DOME or road
			
			if
			(
				map_has_item(tile, BIT_FUNGUS)
				&&
				!(isNativeVehicle(vehicleId) || isFactionHasProject(vehicle->faction_id, FAC_XENOEMPATHY_DOME) || map_has_item(tile, BIT_ROAD))
			)
				continue;
			
			// everything else is safe
			
		}
		
		// get range
		
		int range = map_range(vehicle->x, vehicle->y, x, y);
		
		// break cycle if farther than closest location
		
		if (range > closestSafeLocationRange)
			break;
		
		// get travel time
		
		double travelTime = getVehicleATravelTime(vehicleId, tile);
		
		if (travelTime == INF)
			continue;
		
		// update best
		
		if (range <= closestSafeLocationRange && travelTime < closestSafeLocationTravelTime)
		{
			closestSafeLocation = tile;
			closestSafeLocationRange = range;
			closestSafeLocationTravelTime = travelTime;
		}
		
	}
	
	// return closest safe location
	
	return closestSafeLocation;
	
}

MAP *getSafeLocation(int vehicleId)
{
	return getSafeLocation(vehicleId, 0);
}

