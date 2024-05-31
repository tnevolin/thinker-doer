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

void SeaTransit::set(Transfer *_boardTransfer, Transfer *_unboardTransfer, int _travelTime)
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

	// vanilla fix for transport dropoff

	fixUndesiredTransportDropoff();

	// generic

	moveAllStrategy();

	// artifact

	moveArtifactStrategy();

	// combat

	profileFunction("1.3.1. moveCombatStrategy", moveCombatStrategy);

	// colony

	profileFunction("1.3.2. moveColonyStrategy", moveColonyStrategy);

	// former

	profileFunction("1.3.3. moveFormerStrategy", moveFormerStrategy);

	// transport

	moveTranportStrategy();

	// vanilla fix for transpoft pickup

	fixUndesiredTransportPickup();

}

/*
vanilla enemy_strategy clears up SENTRY/BOARD status from ground unit in land port
restore boarded units order back to SENTRY/BOARD so they be able to coninue transported
*/
void fixUndesiredTransportDropoff()
{
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		// verify transportVehicleId is set

		if (!(vehicle->waypoint_1_y == 0 && vehicle->waypoint_1_x >= 0))
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

}

/*
AI transport picks up idle units from port
set their order to hold to prevent this
*/
void fixUndesiredTransportPickup()
{
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

}

void moveAllStrategy()
{
	// heal

	healStrategy();

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

			if (vehicle->damage_taken <= (isProjectBuilt(aiFactionId, FAC_NANO_FACTORY) ? 0 : 2 * vehicle->reactor_type()))
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
int enemyMoveVehicle(const int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	int vehicleSpeed = getVehicleSpeedWithoutRoads(vehicleId);

	debug("enemyMoveVehicle [%4d] (%3d,%3d) %2d %s\n", vehicleId, vehicle->x, vehicle->y, getVehicleRemainingMovement(vehicleId), Units[vehicle->unit_id].name);

	// default enemy_move function

	int (__attribute__((cdecl)) *defaultEnemyMove)(int) = enemy_move;
//	int (__attribute__((cdecl)) *defaultEnemyMove)(int) = mod_enemy_move;

	// do not try more iterations than vehicle has moves

	if (vehicle->iter_count > vehicleSpeed)
	{
		return defaultEnemyMove(vehicleId);
	}

//	// do not move artifact toward unfriendly vehicles
//
//	if (isArtifactVehicle(vehicleId) && redirectArtifact(vehicleId))
//	{
//		return EM_SYNC;
//	}
//
	// execute task

	if (hasExecutableTask(vehicleId))
	{
		return executeTask(vehicleId);
	}

	// colony
	// all colony movement is scheduled in strategy phase

    // use vanilla algorithm for probes
    // Thinker ignores subversion opportunities

    if (isProbeVehicle(vehicleId))
	{
		return enemy_move(vehicleId);
	}

	// unhandled cases handled by default

	return defaultEnemyMove(vehicleId);

}

/*
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
		debug("transitVehicle [%4d] (%3d,%3d)->(%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, getX(destination), getY(destination));
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
			if (isSameOceanAssociation(vehicleTile, destination, vehicle->faction_id))
			{
				if (TRACE)
				{
					debug("\t\tsame ocean association\n");
				}
				setTask(task);
				success = true;
			}
			else
			{
				if (TRACE)
				{
					debug("\t\tdifferent ocean association\n");
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
	const bool TRACE = DEBUG && false;

	int vehicleId = task.getVehicleId();
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	bool vehicleTileOcean = is_ocean(vehicleTile);
	int vehicleAssociation = getVehicleAssociation(vehicleId);
	MAP *destination = task.getDestination();
	bool destinationOcean = is_ocean(destination);
	int destinationAssociation = getAssociation(destination, vehicle->faction_id);

	// land vehicle

	if (vehicle->triad() != TRIAD_LAND)
		return false;

	// valid destination

	if (destination == nullptr)
		return false;

	if (TRACE)
	{
		debug("transitLandVehicle [%4d] (%3d,%3d)->(%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y, getX(destination), getY(destination));
	}

	int seaTransportVehicleId = getVehicleTransportId(vehicleId);
	if (seaTransportVehicleId != -1)
	{
		// sea transport

		if (!isSeaTransportVehicle(seaTransportVehicleId))
			return false;

		// find drop-off transfer

		Transfer *transfer = getVehicleOptimalDropOffTransfer(vehicleId, seaTransportVehicleId, destination);

		if (transfer == nullptr)
			return false;

		// set unboarding task and add unload request

		setTask(Task(vehicleId, TT_UNBOARD, destination));
		aiData.transportControl.addUnloadRequest(seaTransportVehicleId, vehicleId, transfer->transportStop, transfer->passengerStop);

		return true;

	}
	else // vehicle is not on transport
	{
		if (!vehicleTileOcean && !destinationOcean && vehicleAssociation == destinationAssociation) // same continent
		{
			// compute land travel time

			int landTravelTime = getVehicleATravelTimeByTaskType(vehicleId, destination, task.type);

			if (landTravelTime != -1)
			{
				setTask(task);
				return true;
			}
			else
			{
				// list ocean connections

				SeaTransit bestSeaTransit;
				int bestSeaTransitTravelTime = INT_MAX;
				for (int crossOceanAssociation : getAssociationConnections(vehicleAssociation, vehicle->faction_id))
				{
					if (getAssociationSeaTransports(crossOceanAssociation, vehicle->faction_id).size() == 0)
					{
						debug
						(
							"addSeaTransportRequest same continent"
							" crossOceanAssociation=%2d"
							" [%4d] (%3d,%3d)->(%3d,%3d)"
							"\n"
							, crossOceanAssociation
							, vehicleId, getX(vehicleTile), getY(vehicleTile), getX(destination), getY(destination)
						);

						aiData.addSeaTransportRequest(crossOceanAssociation);
						continue;

					}

					// find best cross ocean transit

					SeaTransit seaTransit = getVehicleOptimalCrossOceanTransit(vehicleId, crossOceanAssociation, destination);

					if (!seaTransit.valid)
						continue;

					if (seaTransit.travelTime < bestSeaTransitTravelTime)
					{
						bestSeaTransit.copy(seaTransit);
						bestSeaTransitTravelTime = seaTransit.travelTime;
					}

				}

				if (!bestSeaTransit.valid)
					return false;

				// handle transit

				setTask(Task(vehicleId, TT_BOARD, bestSeaTransit.boardTransfer->passengerStop));

				if (vehicleTile == bestSeaTransit.boardTransfer->passengerStop)
				{
					// request transportation

					aiData.transportControl.transitRequests.emplace_back
					(
						vehicleId,
						bestSeaTransit.boardTransfer->transportStop,
						bestSeaTransit.unboardTransfer->transportStop
					);

				}

				return true;

			}

		}
		else // all other cases
		{
			// get cross ocean association

			int crossOceanAssociation = getCrossOceanAssociation(vehicleTile, destinationAssociation, vehicle->faction_id);

			if (crossOceanAssociation == -1)
				return false;

			if (getAssociationSeaTransports(crossOceanAssociation, vehicle->faction_id).size() == 0)
			{
				debug
				(
					"addSeaTransportRequest not same continent"
					" crossOceanAssociation=%2d"
					" [%4d] (%3d,%3d)->(%3d,%3d)"
					"\n"
					, crossOceanAssociation
					, vehicleId, getX(vehicleTile), getY(vehicleTile), getX(destination), getY(destination)
				);

				aiData.addSeaTransportRequest(crossOceanAssociation);
				return false;

			}

			// get transit

			SeaTransit seaTransit = getVehicleOptimalCrossOceanTransit(vehicleId, crossOceanAssociation, destination);

			if (!seaTransit.valid)
				return false;

			// handle transit

			setTask(Task(vehicleId, TT_BOARD, seaTransit.boardTransfer->passengerStop));

			if (vehicleTile == seaTransit.boardTransfer->passengerStop)
			{
				// request transportation

				aiData.transportControl.transitRequests.emplace_back
				(
					vehicleId, seaTransit.boardTransfer->transportStop, seaTransit.unboardTransfer->transportStop
				);

			}

			return true;

		}

	}

	return false;

}

/*
Delete unit to reduce support burden.
*/
void disbandVehicles()
{
	debug("disbandVehicles - %s\n", MFactions[aiFactionId].noun_faction);

	// at the game start bases could be somewhat overburdened

	if (*CurrentTurn <= 20)
		return;

	// count total support vs. total intake

	int totalMineralIntake2 = 0;
	int totalSupport = 0;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);

		totalMineralIntake2 += base->mineral_intake_2;
		totalSupport += base->mineral_intake_2 - base->mineral_surplus;

	}

	// calculate allowed total support

	int allowedTotalSupport = (int)floor(conf.ai_production_support_ratio * (double)totalMineralIntake2) + 2;

	debug
	(
		"\ttotalSupport=%d, allowedTotalSupport=%d, conf.ai_production_support_ratio=%5.2f, totalMineralIntake2=%d\n"
		, totalSupport
		, allowedTotalSupport
		, conf.ai_production_support_ratio
		, totalMineralIntake2
	);

	// check if we are overburdened

	if (totalSupport <= allowedTotalSupport)
	{
		debug("\tsupport is bearable\n");
		return;
	}

	// find weakest and cheapest combat vehicle
	// assume cheapest is weakest

	int weakestCombatVehicleId = -1;
	double weakestCombatVehicleValue = DBL_MAX;

	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		// support from base

		if (vehicle->home_base_id == -1)
			continue;

		// not in the base

		if (isBaseAt(vehicleTile))
			continue;

		// exclude natives

		if (isNativeVehicle(vehicleId))
			continue;

		int cost = Units[vehicle->unit_id].cost;
		double moraleModifier = getVehicleMoraleMultiplier(vehicleId);

		// increase cost for those without clean reactor

		if (!isVehicleHasAbility(vehicleId, ABL_CLEAN_REACTOR))
		{
			cost += 4;
		}

		// build value

		double value = (double)cost * moraleModifier;

		// update best

		if (value < weakestCombatVehicleValue)
		{
			weakestCombatVehicleId = vehicleId;
			weakestCombatVehicleValue = value;
		}

	}

	// not found

	if (weakestCombatVehicleId == -1)
		return;

	VEH *weakestCombatVehicle = getVehicle(weakestCombatVehicleId);

	// delete vehicle

	debug
	(
		"\tweakestCombatVehicle: [%4d] (%3d,%3d) %-32s\n"
		, weakestCombatVehicleId
		, weakestCombatVehicle->x, weakestCombatVehicle->y
		, getVehicleUnitName(weakestCombatVehicleId)
	);

	setTask(Task(weakestCombatVehicleId, TT_KILL));

}

/*
Redistribute vehicle between bases to proportinally burden them with support.
*/
void balanceVehicleSupport()
{
	// find base with lowest/highest mineral surplus

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);

		int support = base->mineral_intake_2 - base->mineral_surplus;

		if (support > 0 && base->mineral_surplus <= 1)
		{
			// find vehicle to reassign

			int reassignVehicleId = -1;

			for (int vehicleId : aiData.vehicleIds)
			{
				VEH *vehicle = &(Vehicles[vehicleId]);

				if (vehicle->home_base_id == baseId)
				{
					reassignVehicleId = vehicleId;
					break;
				}

			}

			// not found

			if (reassignVehicleId == -1)
				continue;

			// find base to reassign to

			for (int otherBaseId : aiData.baseIds)
			{
				BASE *otherBase = &(Bases[otherBaseId]);

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
	int association = getVehicleAssociation(vehicleId);

	MAP *nearestBaseLocation = nullptr;

	// iterate bases

	int minRange = INT_MAX;
	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);

		// friendly base only

		if (!(base->faction_id == aiFactionId || has_pact(base->faction_id, aiFactionId)))
			continue;

		// same association for land unit

		if (triad == TRIAD_LAND)
		{
			if (!(getAssociation(baseTile, vehicle->faction_id) == association))
				continue;
		}

		// same ocean association for sea unit

		if (triad == TRIAD_SEA)
		{
			if (!(getBaseOceanAssociation(baseId) == association))
				continue;
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
	bool ocean = isOceanRegion(startingTile->region);

	MAP *nearestMonolithLocation = nullptr;

	// search only for realm unit can move on

	if (!(triad == TRIAD_AIR || triad == (ocean ? TRIAD_SEA : TRIAD_LAND)))
		return nullptr;

	int minRange = INT_MAX;
	for (int mapIndex = 0; mapIndex < *MapAreaTiles; mapIndex++)
	{
		int mapX = getX(mapIndex);
		int mapY = getY(mapIndex);
		MAP* tile = getMapTile(mapIndex);

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

/*
Finds optimal land vehicle transfer for land-ocean-land transition.
*/
SeaTransit getVehicleOptimalCrossOceanTransit(int vehicleId, int crossOceanAssociation, MAP *destination)
{
	const int TRACE = false;
	if (TRACE)
	{
		debug
		(
			"getVehicleOptimalCrossOceanTransit"
			" [%4d] ~>%3d ->(%3d,%3d)"
			"\n"
			, vehicleId
			, crossOceanAssociation
			, getX(destination), getY(destination)
		);
	}

	VEH *vehicle = getVehicle(vehicleId);
	int vehicleSpeedOnLand = getVehicleSpeedWithoutRoads(vehicleId);
	int factionId = vehicle->faction_id;
	MAP *origin = getVehicleMapTile(vehicleId);
	int originAssociation = getAssociation(origin, factionId);
	int destinationAssociation = getAssociation(destination, factionId);
	int seaTransportUnitId = getSeaTransportUnitId(crossOceanAssociation, factionId, false);
	UNIT *seaTransportUnit = getUnit(seaTransportUnitId);
	int seaTransportChassisSpeed = seaTransportUnit->speed();

	SeaTransit seaTransit;

	// land vehicle

	if (vehicle->triad() != TRIAD_LAND)
		return seaTransit;

	// sea transport is available

	if (seaTransportChassisSpeed == -1)
		return seaTransit;

	// transfers

	std::vector<Transfer> *boardTransfers = nullptr;
	std::vector<Transfer> *unboardTransfers = nullptr;
	bool measureTimeToDestination;

	if (originAssociation == crossOceanAssociation) // at cross ocean
	{
		// at base

		if (!map_has_item(origin, BIT_BASE_IN_TILE))
			return seaTransit;

		boardTransfers = &aiData.factionGeographys[factionId].getOceanBaseTransfers(origin);

	}
	else if (isConnected(originAssociation, crossOceanAssociation, factionId)) // connected to cross ocean
	{
		boardTransfers = &aiData.factionGeographys[factionId].getAssociationTransfers(originAssociation, crossOceanAssociation);
	}
	else // unsupported
	{
		return seaTransit;
	}

	if (crossOceanAssociation == destinationAssociation) // at cross ocean
	{
		// to base

		if (!map_has_item(destination, BIT_BASE_IN_TILE))
			return seaTransit;

		unboardTransfers = &aiData.factionGeographys[factionId].getOceanBaseTransfers(destination);
		measureTimeToDestination = false;

	}
	else if (isConnected(crossOceanAssociation, destinationAssociation, factionId)) // connected to cross ocean
	{
		unboardTransfers = &aiData.factionGeographys[factionId].getAssociationTransfers(destinationAssociation, crossOceanAssociation);
		measureTimeToDestination = true;
	}
	else // not connected
	{
		// find cross association

		int firstLandAssociation = getFirstLandAssociation(crossOceanAssociation, destinationAssociation, factionId);

		if (firstLandAssociation == -1)
			return seaTransit;

		unboardTransfers = &aiData.factionGeographys[factionId].getAssociationTransfers(crossOceanAssociation, firstLandAssociation);
		measureTimeToDestination = false;

	}

	if (boardTransfers == nullptr || unboardTransfers == nullptr)
		return seaTransit;

	// find optimal board transfer

	Transfer *optimalBoardTransfer = nullptr;
	int optimalBoardTransferTravelTime = INT_MAX;

	for (Transfer &boardTransfer : *boardTransfers)
	{
		// travelTime

		int vehicleTravelTime1 = getVehicleATravelTime(vehicleId, origin, boardTransfer.passengerStop);
		int transportTravelTime = divideIntegerRoundUp(getRange(boardTransfer.transportStop, destination), seaTransportChassisSpeed);

		int travelTime = vehicleTravelTime1 + 2 * transportTravelTime;

		// update best

		if (travelTime < optimalBoardTransferTravelTime)
		{
			optimalBoardTransfer = &boardTransfer;
			optimalBoardTransferTravelTime = travelTime;
		}

	}

	if (optimalBoardTransfer == nullptr)
		return seaTransit;

	// find optimal unboard transfer

	Transfer *optimalUnboardTransfer = nullptr;
	int optimalUnboardTransferTravelTime = INT_MAX;

	for (Transfer &unboardTransfer : *unboardTransfers)
	{
		// travelTime

		int transportTravelTime = divideIntegerRoundUp(getRange(unboardTransfer.transportStop, destination), seaTransportChassisSpeed);
		int vehicleTravelTime1 = getVehicleATravelTime(vehicleId, origin, unboardTransfer.passengerStop);

		int travelTime = vehicleTravelTime1 + 2 * transportTravelTime;

		// update best

		if (travelTime < optimalUnboardTransferTravelTime)
		{
			optimalUnboardTransfer = &unboardTransfer;
			optimalUnboardTransferTravelTime = travelTime;
		}

	}

	if (optimalUnboardTransfer == nullptr)
		return seaTransit;

	// find optimal travel time

	int optimalTravelTime = INT_MAX;

	for (Transfer &boardTransfer : *boardTransfers)
	{
		// ranges

		int landRange1 = getRange(origin, boardTransfer.passengerStop);
		int oceanRange = getRange(boardTransfer.transportStop, optimalUnboardTransfer->transportStop);
		int landRange2 = (measureTimeToDestination ? getRange(optimalUnboardTransfer->passengerStop, destination) : 0);

		// travelTime

		int landTravelTime1 = divideIntegerRoundUp(landRange1, vehicleSpeedOnLand);
		int oceanTravelTime = divideIntegerRoundUp(oceanRange, seaTransportChassisSpeed);
		int landTravelTime2 = divideIntegerRoundUp(landRange2, vehicleSpeedOnLand);

		int travelTime = landTravelTime1 + oceanTravelTime + landTravelTime2;

		// update best

		if (travelTime < optimalTravelTime)
		{
			optimalBoardTransfer = &boardTransfer;
			optimalTravelTime = travelTime;
		}

	}

	for (Transfer &unboardTransfer : *unboardTransfers)
	{
		// ranges

		int landRange1 = getRange(origin, optimalBoardTransfer->passengerStop);
		int oceanRange = getRange(optimalBoardTransfer->transportStop, unboardTransfer.transportStop);
		int landRange2 = (measureTimeToDestination ? getRange(unboardTransfer.passengerStop, destination) : 0);

		// travelTime

		int landTravelTime1 = divideIntegerRoundUp(landRange1, vehicleSpeedOnLand);
		int oceanTravelTime = divideIntegerRoundUp(oceanRange, seaTransportChassisSpeed);
		int landTravelTime2 = divideIntegerRoundUp(landRange2, vehicleSpeedOnLand);

		int travelTime = landTravelTime1 + oceanTravelTime + landTravelTime2;

		// update best

		if (travelTime < optimalTravelTime)
		{
			optimalUnboardTransfer = &unboardTransfer;
			optimalTravelTime = travelTime;
		}

	}

	// return valid seaTransit

	seaTransit.set(optimalBoardTransfer, optimalUnboardTransfer, optimalTravelTime);
	return seaTransit;

}

/*
Finds optimal vehicle drop-off transfer.
*/
Transfer *getVehicleOptimalDropOffTransfer(int vehicleId, int seaTransportVehicleId, MAP *destination)
{
	const bool TRACE = DEBUG && true;
	if (TRACE)
	{
		debug
		(
			"getVehicleOptimalDropOffTransfer"
			" [%4d]/[%4d]->(%3d,%3d)"
			"\n"
			, vehicleId
			, seaTransportVehicleId
			, getX(destination), getY(destination)
		);
	}

	VEH *vehicle = getVehicle(vehicleId);
	int factionId = vehicle->faction_id;
	MAP *origin = getVehicleMapTile(vehicleId);
	int originOceanAssociation = getOceanAssociation(origin, factionId);
	int destinationAssociation = getAssociation(destination, factionId);

	// land vehicle

	if (vehicle->triad() != TRIAD_LAND)
		return nullptr;

	// origin association is ocean

	if (originOceanAssociation == -1)
		return nullptr;

	// transfers

	std::vector<Transfer> *unboardTransfers = nullptr;
	bool measureTimeToDestination;

	if (originOceanAssociation == destinationAssociation) // at same ocean
	{
		// to base

		if (!map_has_item(destination, BIT_BASE_IN_TILE))
			return nullptr;

		unboardTransfers = &aiData.factionGeographys[factionId].getOceanBaseTransfers(destination);
		measureTimeToDestination = false;

	}
	else if (isConnected(originOceanAssociation, destinationAssociation, factionId)) // connected to current ocean
	{
		unboardTransfers = &aiData.factionGeographys[factionId].getAssociationTransfers(originOceanAssociation, destinationAssociation);
		measureTimeToDestination = true;
	}
	else // not connected
	{
		// find cross association

		int firstLandAssociation = getFirstLandAssociation(originOceanAssociation, destinationAssociation, factionId);

		if (firstLandAssociation == -1)
			return nullptr;

		unboardTransfers = &aiData.factionGeographys[factionId].getAssociationTransfers(originOceanAssociation, firstLandAssociation);
		measureTimeToDestination = false;

	}

	if (unboardTransfers == nullptr)
		return nullptr;

	// find optimal unboard transfer

	Transfer *optimalUnboardTransfer = nullptr;
	int optimalTravelTime = INT_MAX;

	for (Transfer &unboardTransfer : *unboardTransfers)
	{
		bool passengerStopOcean = is_ocean(unboardTransfer.passengerStop);
		TileInfo &passengerStopTileInfo = aiData.getTileInfo(unboardTransfer.passengerStop);
		TileFactionInfo &passengerStopTileFactionInfo = passengerStopTileInfo.factionInfos[vehicle->faction_id];

		// exclude land zoc for artifact

		if (isArtifactVehicle(vehicleId) && !passengerStopOcean && (passengerStopTileFactionInfo.blocked[0] || passengerStopTileFactionInfo.zoc[0]))
			continue;

		// travelTime

//		executionProfiles["| getVehicleATravelTime <- getVehicleOptimalDropOffTransfer"].start();
//		int transportTravelTime = getVehicleATravelTime(seaTransportVehicleId, origin, unboardTransfer.transportStop);
//		executionProfiles["| getVehicleATravelTime <- getVehicleOptimalDropOffTransfer"].stop();
		executionProfiles["| getVehicleLTravelTime <- getVehicleOptimalDropOffTransfer"].start();
		int transportTravelTime = getVehicleLTravelTime(seaTransportVehicleId, origin, unboardTransfer.transportStop);
		executionProfiles["| getVehicleLTravelTime <- getVehicleOptimalDropOffTransfer"].stop();

		if (transportTravelTime == -1 || transportTravelTime >= MOVEMENT_INFINITY)
			continue;

//		int vehicleTravelTime2 = measureTimeToDestination ? getVehicleATravelTime(vehicleId, unboardTransfer.passengerStop, destination) : 0;
		int vehicleTravelTime2 = measureTimeToDestination ? getVehicleLTravelTime(vehicleId, unboardTransfer.passengerStop, destination) : 0;

		if (vehicleTravelTime2 == -1 || vehicleTravelTime2 >= MOVEMENT_INFINITY)
			continue;

		// transport travelTime is more valuable as it is needed for other transportations

		int travelTime = 2 * transportTravelTime + vehicleTravelTime2;

		if (TRACE)
		{
			debug
			(
				"\t->(%3d,%3d)/(%3d,%3d)"
				" travelTime=%3d"
				" transportTravelTime=%3d"
				" vehicleTravelTime2=%3d"
				"\n"
				, getX(unboardTransfer.transportStop), getY(unboardTransfer.transportStop)
				, getX(unboardTransfer.passengerStop), getY(unboardTransfer.passengerStop)
				, travelTime
				, transportTravelTime
				, vehicleTravelTime2
			);
		}

		// update best

		if (travelTime < optimalTravelTime)
		{
			optimalUnboardTransfer = &unboardTransfer;
			optimalTravelTime = travelTime;
			if (TRACE) { debug("\t\t- best\n"); }
		}

	}

	if (optimalUnboardTransfer == nullptr)
		return nullptr;

	// return optimal transfer

	return optimalUnboardTransfer;

}

/*
Sends vehicle along safest travel waypoints.
*/
void setSafeMoveTo(int vehicleId, MAP *destination)
{
	debug("setSafeMoveTo [%4d] -> (%3d,%3d)\n", vehicleId, getX(destination), getY(destination));

	MAP *bestTile = destination;
	double bestTileWeight = DBL_MAX;

	bool dangerous = false;

	for (MovementAction &reachableLocation : getVehicleReachableLocations(vehicleId))
	{
		MAP *tile = reachableLocation.destination;
		double danger = getVehicleTileDanger(vehicleId, tile);

		double distance = getVectorDistance(tile, destination);
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

		debug
		(
			"(%3d,%3d)"
			" danger=%5.2f"
			" distance=%5.2f"
			" weight=%5.2f"
			" dangerous=%d"
			" best=%d"
			"\n"
			, getX(tile), getY(tile)
			, danger
			, distance
			, weight
			, dangerous
			, weight < bestTileWeight
		);

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

