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
	Profiling::start("moveStrategy", "strategy");
	
	debug("moveStrategy - %s\n", MFactions[aiFactionId].noun_faction);
	
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
	
	Profiling::stop("moveStrategy");
	
}

/*
vanilla enemy_strategy clears up SENTRY/BOARD status from ground unit in land port
restore boarded units order back to SENTRY/BOARD so they be able to coninue transported
*/
void fixUndesiredTransportDropoff()
{
	Profiling::start("fixUndesiredTransportDropoff", "moveStrategy");
	
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

	Profiling::stop("fixUndesiredTransportDropoff");
	
}

/*
AI transport picks up idle units from port
set their order to hold to prevent this
*/
void fixUndesiredTransportPickup()
{
	Profiling::start("fixUndesiredTransportPickup", "");
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// AI vehicle
		
		if (vehicle->faction_id != aiFactionId)
			continue;
		
		// verify vehicle is at own base
		
		if (!(vehicleTile->owner == aiFactionId && map_has_item(vehicleTile, BIT_BASE_IN_TILE)))
			continue;
		
		// verify vehicle is idle
		
		if (!(vehicle->order == ORDER_NONE))
			continue;
		
		// put vehicle on hold
		
		setVehicleOrder(vehicleId, ORDER_HOLD);
		
	}
	
	Profiling::stop("fixUndesiredTransportPickup");
	
}

void moveAllStrategy()
{
	Profiling::start("moveAllStrategy", "moveStrategy");
	
	// heal

	healStrategy();

	Profiling::stop("moveAllStrategy");
	
}

void healStrategy()
{
	Profiling::start("- healStrategy");
	
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

			for (MAP *tile : getRangeTiles(vehicleTile, 8, true))
			{
				TileInfo &tileInfo = aiData.getTileInfo(tile);

				// exclude warzone

				if (tileInfo.hostileDangerZone)
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

	Profiling::stop("- healStrategy");
	
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
	
    // use vanilla algorithm for non defensive probes
	
    if (isProbeVehicle(vehicleId) && !isInfantryVehicle(vehicleId))
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
	Profiling::start("balanceVehicleSupport", "");
	
	debug("balanceVehicleSupport - %s\n", aiMFaction->noun_faction);
	
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
					
					Profiling::start("- balanceVehicleSupport - computeBase");
					computeBase(baseId, false);
					computeBase(otherBaseId, false);
					Profiling::stop("- balanceVehicleSupport - computeBase");
					
					// exit cycle
					
					break;
					
				}
				
			}
			
		}
		
	}
	
	Profiling::stop("balanceVehicleSupport");
	
}

// ==================================================
// enemy_move entry point
// ==================================================

/*
Modified vehicle movement.
*/
int __cdecl wtp_mod_ai_enemy_move(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	int returnValue;
	
	// choose AI logic
	
	if (isWtpEnabledFaction(vehicle->faction_id) || (aiFactionId == *CurrentPlayerFaction && conf.manage_player_units && ((vehicle->state & VSTATE_ON_ALERT) != 0) && vehicle->terraform_turns == 0))
	{
		// run WTP AI code for AI eanbled factions or human player managed units
		
		Profiling::start("| enemyMoveVehicle");
		returnValue = enemyMoveVehicle(vehicleId);
		Profiling::stop("| enemyMoveVehicle");
		
	}
	else
	{
		// default
		
		Profiling::start("~ mod_enemy_move");
		returnValue = mod_enemy_move(vehicleId);
		Profiling::stop("~ mod_enemy_move");
		
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
	
	Profiling::start("- setSafeMoveTo - getVehicleReachableLocations");
	for (robin_hood::pair<int, int> const &reachableLocation : getVehicleReachableLocations(vehicleId))
	{
		int tileIndex = reachableLocation.first;
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		
		double danger = tileInfo.hostileDangerZone ? 1.0 : 0.0;
		double distance = getEuqlideanDistance(tile, destination);
		double weight = danger + 0.2 * distance;
		
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
	Profiling::stop("- setSafeMoveTo - getVehicleReachableLocations");
	
	if (!dangerous || bestTile == destination)
	{
		setMoveTo(vehicleId, destination);
	}
	else
	{
		setMoveTo(vehicleId, {bestTile, destination});
	}
	
}

/*
Ends combat vehicle turn at the best tile.
*/
void setCombatMoveTo(int vehicleId, MAP *destination)
{
	VEH &vehicle = Vehicles[vehicleId];
	int factionId = vehicle.faction_id;
	int triad = vehicle.triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MovementType movementType = getVehicleMovementType(vehicleId);
	bool fungusBonus = isNativeVehicle(vehicleId) || has_project(FAC_XENOEMPATHY_DOME, factionId);
	
	debug("setCombatMoveTo [%4d] %s -> %s\n", vehicleId, getLocationString(vehicleTile).c_str(), getLocationString(destination).c_str());
	
	// land
	
	if (triad != TRIAD_LAND)
	{
		debug("\tnot land\n");
		setMoveTo(vehicleId, destination);
		return;
	}
	
	// not in the same cluster
	
	if (!isSameLandCluster(vehicleTile, destination))
	{
		debug("\tnot in same land cluster\n");
		setMoveTo(vehicleId, destination);
		return;
	}
	
	// no need for best location when vehicle is not in the danger zone
	
	if (aiData.hostileEndangeredVehicleIds.find(vehicleId) == aiData.hostileEndangeredVehicleIds.end())
	{
		debug("\tnot hostileEndangeredVehicle\n");
		setMoveTo(vehicleId, destination);
		return;
	}
	
	MAP *bestTile = nullptr;
	double bestTileMovementCost = DBL_MAX;
	bool bestTileSafe = false;
	bool bestTileDefenseBonus = false;
	
	Profiling::start("- setSafeMoveTo - getVehicleReachableLocations");
	for (robin_hood::pair<int, int> const &reachableLocation : getVehicleReachableLocations(vehicleId))
	{
		int tileIndex = reachableLocation.first;
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		double defenseBonus = tileInfo.rough || (fungusBonus && tile->is_fungus());
		
		double movementCost = getLandLMovementCost(factionId, movementType, tile, destination, false);
		
		if (movementCost >= bestTileMovementCost + (double)Rules->move_rate_roads)
			continue;
		
		if (movementCost > bestTileMovementCost - (double)Rules->move_rate_roads)
		{
			if (bestTileSafe && tileInfo.hostileDangerZone)
				continue;
			
			
			if (bestTileDefenseBonus && !defenseBonus)
				continue;
			
		}
		
		bestTile = tile;
		bestTileMovementCost = movementCost;
		bestTileSafe = !tileInfo.hostileDangerZone;
		bestTileDefenseBonus = defenseBonus;
		
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
	Profiling::stop("- setSafeMoveTo - getVehicleReachableLocations");
	
	if (bestTile == nullptr)
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
MapDoubleValue findClosestMonolith(int vehicleId, int maxSearchRange, bool avoidWarzone)
{
	Profiling::start("- findClosestMonolith");
	
	VEH *vehicle = getVehicle(vehicleId);
	int triad = vehicle->triad();
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	MAP *closestMonolith = nullptr;
	double closestMonolithTravelTime = INF;
	
	for (MAP *tile : aiData.monoliths)
	{
		int range = getRange(vehicleTile, tile);
		
		if (range > maxSearchRange)
			continue;
		
		bool ocean = is_ocean(tile);
		
		// corresponding realm
		
		if ((triad == TRIAD_LAND && ocean) || (triad == TRIAD_SEA && !ocean))
			continue;
		
		// item
		
		if (!map_has_item(tile, BIT_MONOLITH))
			continue;
		
		// exclude blocked location
		
		if (isBlocked(tile))
			continue;
		
		// exclude warzone if requested
		
		if (avoidWarzone && isWarzone(tile))
			continue;
		
		// get travel time
		
		double travelTime = getVehicleTravelTime(vehicleId, tile);
		if (travelTime == INF)
			continue;
		
		// update best
		
		if (travelTime < closestMonolithTravelTime)
		{
			closestMonolith = tile;
			closestMonolithTravelTime = travelTime;
		}
		
	}
	
	Profiling::stop("- findClosestMonolith");
	return {closestMonolith, closestMonolithTravelTime};
	
}

/*
Searches for closest friendly base or not blocked bunker.
If none found, searches for any closest safe location (from unfriendly or hostile).
*/
MAP *getSafeLocation(int vehicleId, bool unfriendly)
{
	Profiling::start("- getSafeLocation");
	
	VEH &vehicle = Vehicles[vehicleId];
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int triad = vehicle.triad();
	
	// artifact
	
	if (!isArtifactVehicle(vehicleId))
	{
		Profiling::stop("- getSafeLocation");
		return nullptr;
	}
	
	// infantry chassis
	
	if (vehicle.chassis_type() != CHS_INFANTRY)
	{
		Profiling::stop("- getSafeLocation");
		return nullptr;
	}
	
	// exclude transported land vehicle
	
	if (isLandVehicleOnTransport(vehicleId))
	{
		Profiling::stop("- getSafeLocation");
		return nullptr;
	}
	
	// search closest safe location
	
	for (MAP *tile : getRangeTiles(vehicleTile, MAX_SAFE_LOCATION_SEARCH_RANGE, true))
	{
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
		
		if (map_has_item(tile, BIT_BASE_IN_TILE) && isFriendly(vehicle.faction_id, tile->owner))
		{
			// base is a best safe location
			Profiling::stop("- getSafeLocation");
			return tile;
		}
		
		if ((triad == TRIAD_LAND || triad == TRIAD_SEA) && map_has_item(tile, BIT_BUNKER) && !isBlocked(tile))
		{
			// not blocked bunker is a safe location for surface vehicle
			Profiling::stop("- getSafeLocation");
			return tile;
		}
		
		// not fungus
		
		if (map_has_item(tile, BIT_FUNGUS))
			continue;
		
		// not blocked
		
		if (tileInfo.blocked)
			continue;
		
		// not danger zone
		
		if (unfriendly ? tileInfo.unfriendlyDangerZone : tileInfo.hostileDangerZone)
			continue;
		
		Profiling::stop("- getSafeLocation");
		return tile;
		
	}
	
	// nothing found
	
	Profiling::stop("- getSafeLocation");
	return nullptr;
	
}

