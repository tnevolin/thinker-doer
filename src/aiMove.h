#pragma once

#include <memory>
#include "game_wtp.h"
#include "aiMoveTransport.h"
#include "aiTask.h"
#include "aiData.h"

struct SeaTransit
{
	bool valid = false;
	Transfer *boardTransfer = nullptr;
	Transfer *unboardTransfer = nullptr;
	int travelTime = -1;
	
	void set(Transfer *_boardTransfer, Transfer *_unboardTransfer, int _travelTime);
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
void disbandVehicles();
void balanceVehicleSupport();
int __cdecl modified_enemy_move(const int vehicleId);
MAP *getNearestFriendlyBase(int vehicleId);
MAP *getNearestMonolith(int x, int y, int triad);
SeaTransit getVehicleOptimalCrossOceanTransit(int vehicleId, int crossOceanAssociation, MAP *destination);
Transfer *getVehicleOptimalDropOffTransfer(int vehicleId, int seaTransportVehicleId, MAP *destination);
void setSafeMoveTo(int vehicleId, MAP *destination);

