#pragma once

#include "game.h"
#include "game_wtp.h"

struct LOCATION_RANGE
{
	int x;
	int y;
	int range;
};

int moveSeaTransport(int vehicleId);
int getCarryingArtifactVehicleId(int transportVehicleId);
bool isCarryingArtifact(int vehicleId);
int getCarryingColonyVehicleId(int transportVehicleId);
bool isCarryingColony(int vehicleId);
int getCarryingFormerVehicleId(int transportVehicleId);
bool isCarryingFormer(int vehicleId);
bool isCarryingVehicle(int vehicleId);
bool transportArtifact(int transportVehicleId, int carryingVehicleId);
bool transportColony(int transportVehicleId, int colonyVehicleId);
bool transportFormer(int transportVehicleId, int formerVehicleId);
bool pickupColony(int vehicleId);
bool pickupFormer(int vehicleId);
Location getSeaTransportUnloadLocation(int transportVehicleId, const Location targetLocation);
bool transportVehicles(const int transportVehicleId, const Location destinationLocation);
bool isInOceanRegion(int vehicleId, int region);

