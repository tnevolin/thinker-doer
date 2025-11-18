#pragma once

#include "wtp_ai_game.h"

enum ENEMY_MOVE_RETURN_VALUE
{
	EM_SYNC = 0,
	EM_DONE = 1,
};

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
bool transitVehicle(Task const &task);
bool transitLandVehicle(Task const &task);
void balanceVehicleSupport();
int aiEnemyMove(const int vehicleId);
MAP *getNearestFriendlyBase(int vehicleId);
MAP *getNearestMonolith(int x, int y, int triad);
Transfer getOptimalPickupTransfer(MAP *org, MAP *dst);
Transfer getOptimalDropoffTransfer(MAP *org, MAP *dst, int passengerVehicleId, int transportVehicleId);
void setSafeMoveTo(int vehicleId, MAP *destination);
MapDoubleValue findClosestMonolith(int vehicleId, int maxSearchRange, bool avoidWarzone);
MAP *getSafeLocation(int vehicleId, bool hostile);
void enemyMoveVehicleUpdateMapData();
int setMoveTo(int vehicleId, MAP *destination);
int setMoveTo(int vehicleId, const std::vector<MAP *> &waypoints);
void aiEnemyMoveVehicles();

