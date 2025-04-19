#pragma once

#include <functional>
#include "engine.h"
#include "wtp_aiData.h"

int const MIN_LANDMARK_DISTANCE = 20;
double const A_DISTANCE_TRESHOLD = 10.0;

const double RANGED_AIR_TRAVEL_TIME_COEFFICIENT = 1.5;
const double SEA_TRANSPORT_WAIT_TIME_COEFFICIENT = 4.0;

const double IMPEDIMENT_SEA_TERRITORY_HOSTILE = 0.5;
const double IMPEDIMENT_LAND_TERRITORY_HOSTILE = 2.0;
const double IMPEDIMENT_LAND_INTERBASE_UNFRIENDLY = 4.0;

int const A_RANGE = 10;
double const A_FACTOR = 2.0;

struct BaseChange
{
	bool friendly;
	bool friendlySea;
	bool friendlyPort;
	bool unfriendly;
	bool unfriendlySea;
	bool unfriendlyLand;
	
	void clear()
	{
		friendly = false;
		friendlyPort = false;
		unfriendly = false;
		unfriendlySea = false;
		unfriendlyLand = false;
	}
	
	void setFriendly()
	{
		friendly = true;
	}
	void setFriendlySea()
	{
		friendly = true;
		friendlySea = true;
	}
	void setFriendlyPort()
	{
		friendly = true;
		friendlyPort = true;
	}
	void setUnfriendly()
	{
		unfriendly = true;
	}
	void setUnfriendlySea()
	{
		unfriendly = true;
		unfriendlySea = true;
	}
	void setUnfriendlyLand()
	{
		unfriendly = true;
		unfriendlyLand = true;
	}
	
};
struct MapSnapshot
{
	MAP *mapTilesReferencingAddress;
	std::vector<int> surfaceTypes;
	robin_hood::unordered_flat_map<MAP *, int> mapBases;
	bool mapChanged;
	std::array<BaseChange, MaxPlayerNum> baseChanges;
};

/// Route node for A* path search algorithm.
struct ANode
{
	MAP *previousTile;
	int movementCost;
	int f;
	
	ANode(MAP *_previousTile, int _movementCost, int _f)
	{
		this->previousTile = _previousTile;
		this->movementCost = _movementCost;
		this->f = _f;
	}
	
};

/// F value map element.
struct FValue
{
	int tileIndex;
	double f;
	
	FValue(int _tileIndex, double _f)
	: tileIndex(_tileIndex)
	, f(_f)
	{}
	
};

struct FValueComparator
{
	bool operator() (FValue const &o1, FValue const &o2)
	{
	   return o1.f > o2.f;
	}

};

/// land-unload transfer
struct Transfer
{
	MAP *passengerStop = nullptr;
	MAP *transportStop = nullptr;
	
	Transfer() {}
	
	Transfer(MAP *_passengerStop, MAP *_transportStop)
	: passengerStop{_passengerStop}
	, transportStop{_transportStop}
	{}
	
	bool valid()
	{
		return passengerStop != nullptr && transportStop != nullptr;
	}
	
};

struct SeaLandmarkTileInfo
{
	int distance = INT_MAX;
	double movementCost = INF;
};
struct SeaLandmark
{
	int tileIndex;
	std::vector<SeaLandmarkTileInfo> tileInfos;
};
struct LandLandmarkTileInfo
{
	int distance = INT_MAX;
	double travelTime = INF;
	double seaTransportWaitTime = 0.0;
	double seaMovementCost = 0.0;
	double landMovementCost = 0.0;
};
struct LandLandmark
{
	int tileIndex;
	std::vector<LandLandmarkTileInfo> tileInfos;
};

/**
Generic factions geographical data.
*/
struct FactionMovementInfo
{
	// impediments
	std::vector<double> impediments;
	
	// airbases
	std::vector<MAP *> airbases;
	// airClusters
	// [chassisId][speed][tileIndex] = clusterIndex
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<int>>> airClusters;
	
	// seaTransportWaitTimes
	// [tileIndex] = waitTime
	std::vector<double> seaTransportWaitTimes;
	
	// sea combat clusters
	// [tileIndex] = clusterIndex
	std::vector<int> seaCombatClusters;
	
	// land combat clusters
	// [tileIndex] = clusterIndex
	std::vector<int> landCombatClusters;
	
	// seaLandmarks
	// [movementTypeIndex][landmarkIndex]
	std::array<std::vector<SeaLandmark>, SEA_MOVEMENT_TYPE_COUNT> seaLandmarks;
	
	// landLandmarks
	// [basic movementTypeIndex][landmarkIndex]
	std::array<std::vector<LandLandmark>, BASIC_LAND_MOVEMENT_TYPE_COUNT> landLandmarks;
	
};

/**
Player faction geographical data.
*/
struct PlayerFactionMovementInfo
{
	// sea clusters
	// [tileIndex] = clusterIndex
	std::vector<int> seaClusters;
	std::map<int, int> seaClusterAreas;
	int seaClusterCount;
	
	// land clusters
	// [tileIndex] = clusterIndex
	std::vector<int> landClusters;
	std::map<int, int> landClusterAreas;
	int landClusterCount;
	
	// land clusters spanned across transportable oceans
	// [tileIndex] = clusterIndex
	std::vector<int> landTransportedClusters;
	
	// transfers to connected sea transport cluster
	// [landClusterIndex][seaClusterIndex][transferIndex] = transfer
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<Transfer>>> transfers;
	robin_hood::unordered_flat_map<MAP *, std::vector<Transfer>> oceanBaseTransfers;
	
	// connected clusters
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>> connectedClusters;
	// firstConnectedClusters
	// [initial][terminal] = first connection
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>>> firstConnectedClusters;
	
	// locations reachable by our vehicles
	std::vector<bool> reachableTileIndexes;
	
	// sea clusters with other faction presense
	// [tileIndex] = false/true
	std::vector<bool> sharedSeas;
	
};

void populateRouteData();
void populateMapSnapshot();
void precomputeRouteData();

// ==================================================
// faction movement data
// ==================================================

void populateImpediments(int factionId);
void populateAirbases(int factionId);
void populateAirClusters(int factionId);
void populateSeaTransportWaitTimes(int factionId);
void populateSeaCombatClusters(int factionId);
void populateLandCombatClusters(int factionId);
void populateSeaLandmarks(int factionId);
void populateLandLandmarks(int factionId);

void populateSeaClusters(int factionId);
void populateLandClusters();
void populateLandTransportedClusters();
void populateTransfers(int factionId);
void populateReachableLocations();

void populateSharedSeas();

// ==================================================
// Landmark travel time finding algorithm
// ==================================================

double getVehicleAttackApproachTime(int vehicleId, MAP *dst);
double getVehicleAttackApproachTime(int vehicleId, MAP *org, MAP *dst);
double getVehicleApproachTime(int vehicleId, MAP *dst, bool includeDestination);
double getVehicleApproachTime(int vehicleId, MAP *org, MAP *dst, bool includeDestination);
double getUnitApproachTime(int factionId, int unitId, MAP *org, MAP *dst, bool includeDestination);
double getGravshipTravelTime(int speed, MAP *org, MAP *dst, bool includeDestination);
double getRangedAirTravelTime(int factionId, int chassisId, int speed, MAP *org, MAP *dst, bool includeDestination);
double getSeaLApproachTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst, bool includeDestination);
double getLandLApproachTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst, bool includeDestination);
double getLandLMovementCost(int factionId, MovementType movementType, MAP *org, MAP *dst, bool includeDestination);

double getUnitTravelTime(int factionId, int unitId, int speed, MAP *org, MAP *dst, bool includeDestination = true);
double getUnitTravelTime(int factionId, int unitId, MAP *org, MAP *dst, bool includeDestination = true);
double getVehicleTravelTime(int vehicleId, MAP *org, MAP *dst, bool includeDestination = true);
double getVehicleTravelTime(int vehicleId, MAP *dst, bool includeDestination = true);

// ==================================================
// A* path finding algorithm
// ==================================================

double getATravelTime(MovementType movementType, int speed, MAP *org, MAP *dst, bool includeDestination);
//double getUnitATravelTime(int unitId, int speed, MAP *org, MAP *dst, bool includeDestination);
//double getUnitATravelTime(int unitId, MAP *org, MAP *dst, bool includeDestination);
//double getSeaUnitATravelTime(int unitId, int speed, MAP *org, MAP *dst, bool includeDestination);
//double getLandUnitATravelTime(int unitId, int speed, MAP *org, MAP *dst, bool includeDestination);
//
//double getVehicleATravelTime(int vehicleId, MAP *org, MAP *dst);
//double getVehicleATravelTime(int vehicleId, MAP *dst);

// ============================================================
// Reachability
// ============================================================

bool isUnitDestinationReachable(int unitId, MAP *org, MAP *dst);
bool isVehicleDestinationReachable(int vehicleId, MAP *org, MAP *dst);
bool isVehicleDestinationReachable(int vehicleId, MAP *dst);

// ==================================================
// Helper functions
// ==================================================

double getRouteVectorDistance(MAP *tile1, MAP *tile2);
MovementType getUnitBasicMovementType(int factionId, int unitId);
MovementType getUnitMovementType(int factionId, int unitId);
MovementType getVehicleBasicMovementType(int vehicleId);
MovementType getVehicleMovementType(int vehicleId);

int getAirCluster(int chassisId, int speed, MAP *tile);
int getVehicleAirCluster(int vehicleId);
bool isSameAirCluster(int chassisId, int speed, MAP *tile1, MAP *tile2);
bool isVehicleSameAirCluster(int vehicleId, MAP *dst);
bool isMeleeAttackableFromAirCluster(int chassisId, int speed, MAP *org, MAP *target);
bool isVehicleMeleeAttackableFromAirCluster(int vehicleId, MAP *target);
int getSeaCluster(MAP *tile);
int getVehicleSeaCluster(int vehicleId);
int getBaseSeaCluster(MAP *baseTile);
bool isSameSeaCluster(int tile1SeaCluster, MAP *tile2);
bool isSameSeaCluster(MAP *tile1, MAP *tile2);
bool isVehicleSameSeaCluster(int vehicleId, MAP *dst);
bool isMeleeAttackableFromSeaCluster(MAP *org, MAP *trg);
bool isVehicleMeleeAttackableFromSeaCluster(int vehicleId, MAP *trg);
bool isArtilleryAttackableFromSeaCluster(MAP *org, MAP *trg);
bool isVehicleArtilleryAttackableFromSeaCluster(int vehicleId, MAP *trg);
int getLandCluster(MAP *tile);
int getVehicleLandCluster(int vehicleId);
bool isSameLandCluster(MAP *tile1, MAP *tile2);
bool isVehicleSameLandCluster(int vehicleId, MAP *dst);
bool isMeleeAttackableFromLandCluster(MAP *org, MAP *trg);
bool isVehicleMeleeAttackableFromLandCluster(int vehicleId, MAP *trg);
bool isArtilleryAttackableFromLandCluster(MAP *org, MAP *trg);
bool isVehicleArtilleryAttackableFromLandCluster(int vehicleId, MAP *trg);
int getLandTransportedCluster(MAP *tile);
int getVehicleLandTransportedCluster(int vehicleId);
bool isSameLandTransportedCluster(MAP *tile1, MAP *tile2);
bool isVehicleSameLandTransportedCluster(int vehicleId, MAP *dst);
bool isMeleeAttackableFromLandTransportedCluster(MAP *org, MAP *trg);
bool isVehicleMeleeAttackableFromLandTransportedCluster(int vehicleId, MAP *trg);
bool isArtilleryAttackableFromLandTransportedCluster(MAP *org, MAP *trg);
bool isVehicleArtilleryAttackableFromLandTransportedCluster(int vehicleId, MAP *trg);
int getSeaCombatCluster(MAP *tile);
bool isSameSeaCombatCluster(MAP *tile1, MAP *tile2);
int getLandCombatCluster(MAP *tile);
bool isSameLandCombatCluster(MAP *tile1, MAP *tile2);

std::vector<Transfer> &getTransfers(int landCluster, int seaCluster);
std::vector<Transfer> &getOceanBaseTransfers(MAP *baseTile);
robin_hood::unordered_flat_set<int> &getConnectedClusters(int cluster);
robin_hood::unordered_flat_set<int> &getConnectedSeaClusters(MAP *landTile);
robin_hood::unordered_flat_set<int> &getConnectedLandClusters(MAP *seaTile);
robin_hood::unordered_flat_set<int> &getFirstConnectedClusters(int orgCluster, int dstCluster);
robin_hood::unordered_flat_set<int> &getFirstConnectedClusters(MAP *org, MAP *dst);

int getEnemyAirCluster(int factionId, int chassisId, int speed, MAP *tile);
bool isSameEnemyAirCluster(int factionId, int chassisId, int speed, MAP *tile1, MAP *tile2);
bool isSameEnemyAirCluster(int vehicleId, MAP *tile2);
int getEnemySeaCombatCluster(int factionId, MAP *tile);
bool isSameEnemySeaCombatCluster(int factionId, MAP *tile1, MAP *tile2);
bool isSameEnemySeaCombatCluster(int vehicleId, MAP *tile2);
int getEnemyLandCombatCluster(int factionId, MAP *tile);
bool isSameEnemyLandCombatCluster(int factionId, MAP *tile1, MAP *tile2);
bool isSameEnemyLandCombatCluster(int vehicleId, MAP *tile2);
int getCluster(MAP *tile);
int getVehicleCluster(int vehicleId);

bool isConnected(int cluster1, int cluster2);
bool isReachable(MAP *tile);
bool isSharedSea(MAP *tile);

int getSeaClusterArea(int seaCluster);

