#pragma once

#include "game.h"
#include "game_wtp.h"

struct LOCATION_RANGE
{
	int x;
	int y;
	int range;
};

void moveTranportStrategy();
void moveSeaTransportStrategy(int vehicleId);
void moveEmptySeaTransportStrategy(int vehicleId);
void moveLoadedSeaTransportStrategy(int vehicleId, int passengerId);
int getCarryingArtifactVehicleId(int transportVehicleId);
bool isCarryingArtifact(int vehicleId);
int getCarryingColonyVehicleId(int transportVehicleId);
bool isCarryingColony(int vehicleId);
int getCarryingFormerVehicleId(int transportVehicleId);
bool isCarryingFormer(int vehicleId);
bool isCarryingVehicle(int vehicleId);
bool deliverArtifact(int transportVehicleId, int carryingVehicleId);
bool deliverFormer(int transportVehicleId, int formerVehicleId);
bool deliverScout(int transportVehicleId, int scoutVehicleId);
bool pickupColony(int vehicleId);
bool pickupFormer(int vehicleId);
void popPodStrategy(int vehicleId);
MAP *getSeaTransportUnboardLocation(int seaTransportVehicleId, MAP *destination);
MAP *getSeaTransportUnloadLocation(int seaTransportVehicleId, MAP *destination, MAP *unboardLocation);
//bool deliverVehicle(const int transportVehicleId, const Location destinationLocation, const int vehicleId);
bool isInOceanRegion(int vehicleId, int region);
int getCarryingScoutVehicleId(int transportVehicleId);
int getCrossOceanAssociation(MAP *initialLocation, MAP *terminalLocation, int factionId);
int getAvailableSeaTransport(int oceanRegion, int vehicleId);
MAP *getSeaTransportLoadLocation(int seaTransportVehicleId, int passengerVehicleId);

