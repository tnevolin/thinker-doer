#pragma once

#include "game.h"
#include "wtp_game.h"

struct LOCATION_RANGE
{
	int x;
	int y;
	int range;
};

struct DeliveryLocation
{
	MAP *unloadLocation = nullptr;
	MAP *unboardLocation = nullptr;
};

void moveTranportStrategy();
void moveSeaTransportStrategy(int vehicleId);
void moveAvailableSeaTransportStrategy(int vehicleId);
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
DeliveryLocation getSeaTransportDeliveryLocation(int seaTransportVehicleId, int passengerVehicleId, MAP *destination);
bool isInOceanRegion(int vehicleId, int region);
int getCarryingScoutVehicleId(int transportVehicleId);
int getCrossOceanAssociation(MAP *origin, int destinationAssociation, int factionId);
int getFirstLandAssociation(int oceanAssociation, int destinationAssociation, int factionId);
int getAvailableSeaTransport(int oceanRegion, int vehicleId);
int getAvailableSeaTransportInStack(int vehicleId);
MAP *getSeaTransportLoadLocation(int seaTransportVehicleId, int passengerVehicleId, MAP *destination);

