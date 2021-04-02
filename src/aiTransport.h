#ifndef __AITRANSPORT_H__
#define __AITRANSPORT_H__

#include "game.h"

struct LOCATION_RANGE
{
	int x;
	int y;
	int range;
};

void moveTransport(int vehicleId);
bool isCarryingColony(int vehicleId);
void transportColonyToClosestUnoccupiedCoast(int vehicleId);
bool isCarryingFormer(int vehicleId);
void transportFormerToMostDemandingLandmass(int vehicleId);

#endif // __AITRANSPORT_H__

