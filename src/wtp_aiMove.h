#pragma once

#include <memory>
#include "wtp_game.h"
#include "wtp_ai.h"
#include "wtp_aiMoveTransport.h"
#include "wtp_aiTask.h"
#include "wtp_aiData.h"
#include "wtp_aiRoute.h"

struct SeaTransit
{
	bool valid = false;
	Transfer *boardTransfer = nullptr;
	Transfer *unboardTransfer = nullptr;
	double travelTime = INF;
	
	void set(Transfer *_boardTransfer, Transfer *_unboardTransfer, double _travelTime);
	void copy(SeaTransit &seaTransit);
	
};

struct VehicleDestination
{
	int vehicleId;
	MAP *destination;
};

void moveStrategy();
void fixUndesiredTransportDropoff();
void fixUndesiredTransportPickup();
void moveAllStrategy();
void healStrategy();
int enemyMoveVehicle(const int vehicleId);
bool transitVehicle(Task task);
bool transitLandVehicle(Task task);
void balanceVehicleSupport();
int __cdecl modified_enemy_move(const int vehicleId);
MAP *getNearestFriendlyBase(int vehicleId);
MAP *getNearestMonolith(int x, int y, int triad);
Transfer getOptimalPickupTransfer(MAP *org, MAP *dst);
Transfer getOptimalDropoffTransfer(MAP *org, MAP *dst, int passengerVehicleId, int transportVehicleId);
void setSafeMoveTo(int vehicleId, MAP *destination);

MapValue findClosestItemLocation(int vehicleId, MapItem item, int maxSearchRange, bool avoidWarzone);

MAP *getSafeLocation(int vehicleId);
MAP *getSafeLocation(int vehicleId, int baseRange);

