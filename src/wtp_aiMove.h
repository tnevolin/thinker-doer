#pragma once

#include "wtp_game.h"
#include "wtp_ai.h"
#include "wtp_aiTask.h"
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
int __cdecl wtp_mod_ai_enemy_move(const int vehicleId);
MAP *getNearestFriendlyBase(int vehicleId);
MAP *getNearestMonolith(int x, int y, int triad);
Transfer getOptimalPickupTransfer(MAP *org, MAP *dst);
Transfer getOptimalDropoffTransfer(MAP *org, MAP *dst, int passengerVehicleId, int transportVehicleId);
void setSafeMoveTo(int vehicleId, MAP *destination);
void setCombatMoveTo(int vehicleId, MAP *destination);

MapDoubleValue findClosestMonolith(int vehicleId, int maxSearchRange, bool avoidWarzone);

MAP *getSafeLocation(int vehicleId, bool hostile);

