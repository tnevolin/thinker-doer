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
bool deliverArtifact(int transportVehicleId, int carryingVehicleId);
bool deliverColony(int transportVehicleId, int colonyVehicleId);
bool deliverFormer(int transportVehicleId, int formerVehicleId);
bool deliverScout(int transportVehicleId, int scoutVehicleId);
bool pickupColony(int vehicleId);
bool pickupFormer(int vehicleId);
bool popPod(int transportVehicleId);
Location getSeaTransportUnloadLocation(int oceanRegion, const Location destination);
bool deliverVehicle(const int transportVehicleId, const Location destinationLocation, const int vehicleId);
bool isInOceanRegion(int vehicleId, int region);
int getCarryingScoutVehicleId(int transportVehicleId);
int getCrossOceanRegion(Location initialLocation, Location terminalLocation);
int getAvailableSeaTransport(int oceanRegion, int vehicleId);

