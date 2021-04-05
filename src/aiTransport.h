#pragma once

#include "game.h"

struct LOCATION_RANGE
{
	int x;
	int y;
	int range;
};

int moveTransport(int vehicleId);
bool isCarryingColony(int vehicleId);
bool isCarryingFormer(int vehicleId);
bool isCarryingVehicle(int vehicleId);
bool transportColony(int transportVehicleId);
void transportFormer(int vehicleId);
bool pickupColony(int vehicleId);
bool pickupFormer(int vehicleId);

