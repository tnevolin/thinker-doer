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
void transportColonyToClosestUnoccupiedCoast(int vehicleId);
bool isCarryingFormer(int vehicleId);
void transportFormerToMostDemandingLandmass(int vehicleId);

