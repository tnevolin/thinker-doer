#pragma once

#include <functional>
#include "engine.h"
#include "wtp_aiData.h"

int const MIN_LANDMARK_DISTANCE = 20;
double const A_DISTANCE_TRESHOLD = 10.0;

const double RANGED_AIR_TRAVEL_TIME_COEFFICIENT = 1.5;
const double SEA_TRANSPORT_WAIT_TIME_COEFFICIENT = 4.0;

const double IMPEDIMENT_SEA_TERRITORY_HOSTILE = 2.0;
const double IMPEDIMENT_LAND_TERRITORY_HOSTILE = 2.0;
const double IMPEDIMENT_LAND_INTERBASE_UNFRIENDLY = 4.0;

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

/**
Shared faction geographical data.
*/
struct FactionMovementInfo
{
};

struct BaseAdjacentClusterSet
{
	int baseId;
	MAP *baseTile;
	robin_hood::unordered_flat_set<int> adjacentClusters;
};

struct SeaLandmarkTileInfo
{
	int distance = INT_MAX;
	double movementCost = INF;
	double seaMovementCost = INF;
};
struct SeaLandmark
{
	MAP *tile;
	std::vector<SeaLandmarkTileInfo> tileInfos;
};

struct LandTransportedLandmarkTileInfo
{
	double movementCost = INF;
	double seaMovementCost = INF;
	double landMovementCost = INF;
	int crossingSeaRegion = -1;
};
struct LandTransportedLandmark
{
	MAP *tile;
	std::vector<LandTransportedLandmarkTileInfo> tileInfos;
};

struct LandmarkData
{
	// sea regions
	int seaRegionCount;
	std::vector<int> seaRegions;
	
	// seaLandmarks
	// [seaMovementType]
	std::array<std::vector<SeaLandmark>, SeaMovementTypeCount> seaLandmarks;
	
	// landTransportedLandmarks
	// [baseId][landMovementType][orgIndex]
	std::vector<Landmark> landTransportedLandmarks;
	
	int getSeaRegion(int tileIndex)
	{
		return seaRegions.at(tileIndex);
	}
	int getSeaRegion(MAP *tile)
	{
		return seaRegions.at(tile - *MapTiles);
	}
	
};

/**
Unfriendly factions geographical data.
*/
struct EnemyFactionMovementInfo
{
	// airbases
	std::vector<MAP *> airbases;
	// airClusters
	// [chassisId][speed][tileIndex] = clusterIndex
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<int>>> airClusters;
	
	// impediments
	std::vector<double> seaTransportImpediments;
	std::vector<double> seaCombatImpediments;
	std::vector<double> landCombatImpediments;
	
	// seaTransportWaitTimes
	// [tileIndex] = waitTime
	std::vector<double> seaTransportWaitTimes;
	
	// sea combat clusters
	// [tileIndex] = clusterIndex
	std::vector<int> seaCombatClusters;
	
	// land combat clusters
	// [tileIndex] = clusterIndex
	std::vector<int> landCombatClusters;
	
	// seaApproachHexCostLandmarks
	// [baseId][seaMovementType][orgIndex]
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<double>>> seaApproachHexCosts;
	
	// landApproachHexCosts
	// [baseId][landMovementType][orgIndex]
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<std::array<double,3>>>> landApproachHexCosts;
	
};

/**
Player faction geographical data.
*/
struct PlayerFactionMovementInfo
{
	// airbases
	std::vector<MAP *> airbases;
	// airClusters
	// [chassisId][speed][tileIndex] = clusterIndex
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<int>>> airClusters;
	
	// impediments
	std::vector<double> seaTransportImpediments;
	std::vector<double> seaCombatImpediments;
	std::vector<double> landCombatImpediments;
	
	// seaTransportWaitTimes
	// [tileIndex] = waitTime
	std::vector<double> seaTransportWaitTimes;
	
	// sea combat clusters
	// [tileIndex] = clusterIndex
	std::vector<int> seaCombatClusters;
	
	// land combat clusters
	// [tileIndex] = clusterIndex
	std::vector<int> landCombatClusters;
	
	// seaApproachHexCosts
	// [baseId][seaMovementType][orgIndex]
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<double>>> seaApproachHexCosts;
	
	// landApproachHexCosts
	// [baseId][landMovementType][orgIndex]
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<MovementType, std::vector<std::array<double,3>>>> landApproachHexCosts;
	
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
	std::map<int, std::map<int, std::vector<Transfer>>> transfers;
	std::map<MAP *, std::vector<Transfer>> oceanBaseTransfers;
	
	// connected clusters
	std::map<int, std::set<int>> connectedClusters;
	// firstConnectedClusters
	// [initial][terminal] = first connection
	std::map<int, std::map<int, std::set<int>>> firstConnectedClusters;
	
	// locations reachable by our vehicles
	std::vector<bool> reachableTileIndexes;
	
	// sea clusters with other faction presense
	// [tileIndex] = false/true
	std::vector<bool> sharedSeas;
	
	// adjacent clusters
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<robin_hood::unordered_flat_set<int>>>> adjacentAirClusters;
	std::vector<robin_hood::unordered_flat_set<int>> adjacentSeaClusters;
	std::vector<robin_hood::unordered_flat_set<int>> adjacentLandClusters;
	std::vector<robin_hood::unordered_flat_set<int>> adjacentLandTransportedClusters;
	
};

void populateRouteData();
//void populateSeaRegions();
//void populateSeaLandmarks();
void precomputeRouteData();

// ==================================================
// faction movement data
// ==================================================

void populateAirbases(int factionId);
void populateAirClusters(int factionId);
void populateImpediments(int factionId);
void populateSeaTransportWaitTimes(int factionId);
void populateSeaCombatClusters(int factionId);
void populateLandCombatClusters(int factionId);
void populateSeaApproachHexCosts(int factionId);
void populateLandApproachHexCosts(int factionId);

void populateSeaClusters(int factionId);
void populateLandClusters();
void populateLandTransportedClusters();
void populateTransfers(int factionId);
void populateReachableLocations();
void populateAdjacentClusters();

void populateSharedSeas();

// ==================================================
// Generic travel time finding algorithm
// ==================================================

//double getTravelTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst);
//double getSeaTravelTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst);

// ==================================================
// Landmark travel time finding algorithm
// ==================================================

//double getSeaLTravelTime(int factionId, MovementType movementType, int speed, MAP *org, MAP *dst);

double getEnemyApproachTime(int baseId, int vehicleId);
double getEnemyGravshipApproachTime(int baseId, int vehicleId);
double getEnemyRangedAirApproachTime(int baseId, int vehicleId);
double getEnemySeaApproachTime(int baseId, int vehicleId);
double getEnemyLandApproachTime(int baseId, int vehicleId);

double getPlayerApproachTime(int baseId, int unitId, MAP *origin);
double getPlayerGravshipApproachTime(int baseId, int unitId, MAP *origin);
double getPlayerRangedAirApproachTime(int baseId, int unitId, MAP *origin);
double getPlayerSeaApproachTime(int baseId, int unitId, MAP *origin);
double getPlayerLandApproachHexCost(int baseId, int unitId, MAP *origin);

// ==================================================
// Generic, not path related travel computations
// ==================================================

double getGravshipUnitTravelTime(int /*unitId*/, int speed, MAP *org, MAP *dst);
double getGravshipUnitTravelTime(int unitId, MAP *org, MAP *dst);
double getGravshipVehicleTravelTime(int vehicleId, MAP *org, MAP *dst);

double getAirRangedUnitTravelTime(int unitId, int speed, MAP *org, MAP *dst);
double getAirRangedUnitTravelTime(int unitId, MAP *org, MAP *dst);
double getAirRangedVehicleTravelTime(int vehicleId, MAP *org, MAP *dst);

// ==================================================
// A* cached movement costs
// ==================================================

double getCachedATravelTime(MovementType movementType, int speed, int orgIndex, int dstIndex);
void setCachedATravelTime(MovementType movementType, int speed, int orgIndex, int dstIndex, double travelTime);

// ==================================================
// A* path finding algorithm
// ==================================================

double getATravelTime(MovementType movementType, int speed, MAP *org, MAP *dst);

double getUnitATravelTime(int unitId, int speed, MAP *org, MAP *dst);
double getUnitATravelTime(int unitId, MAP *org, MAP *dst);
double getSeaUnitATravelTime(int unitId, int speed, MAP *org, MAP *dst);
double getLandUnitATravelTime(int unitId, int speed, MAP *org, MAP *dst);

double getVehicleATravelTime(int vehicleId, MAP *org, MAP *dst);
double getVehicleATravelTime(int vehicleId, MAP *dst);

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
MovementType getUnitMovementType(int factionId, int unitId);
MovementType getVehicleMovementType(int vehicleId);

int getAirCluster(int chassisId, int speed, MAP *tile);
int getVehicleAirCluster(int vehicleId);
bool isSameAirCluster(int chassisId, int speed, MAP *tile1, MAP *tile2);
bool isVehicleSameAirCluster(int vehicleId, MAP *dst);
bool isMeleeAttackableFromAirCluster(int chassisId, int speed, MAP *org, MAP *target);
bool isVehicleMeleeAttackableFromAirCluster(int vehicleId, MAP *target);
int getSeaCluster(MAP *tile);
int getVehicleSeaCluster(int vehicleId);
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
std::set<int> &getConnectedClusters(int cluster);
std::set<int> &getConnectedSeaClusters(MAP *landTile);
std::set<int> &getConnectedLandClusters(MAP *seaTile);
std::set<int> &getFirstConnectedClusters(int orgCluster, int dstCluster);
std::set<int> &getFirstConnectedClusters(MAP *org, MAP *dst);

int getEnemyAirCluster(int factionId, int chassisId, int speed, MAP *tile);
bool isSameEnemyAirCluster(int factionId, int chassisId, int speed, MAP *tile1, MAP *tile2);
bool isSameEnemyAirCluster(int vehicleId, MAP *tile2);
int getEnemySeaCombatCluster(int factionId, MAP *tile);
bool isSameEnemySeaCombatCluster(int factionId, MAP *tile1, MAP *tile2);
int getEnemyLandCombatCluster(int factionId, MAP *tile);
bool isSameEnemyLandCombatCluster(int factionId, MAP *tile1, MAP *tile2);
int getCluster(MAP *tile);
int getVehicleCluster(int vehicleId);
int getBaseSeaCluster(int baseId);

bool isConnected(int cluster1, int cluster2);
bool isReachable(MAP *tile);
bool isSharedSea(MAP *tile);

int getSeaClusterArea(int seaCluster);

robin_hood::unordered_flat_set<int> getAdjacentSeaClusters(MAP *tile);
robin_hood::unordered_flat_set<int> getAdjacentLandClusters(MAP *tile);
robin_hood::unordered_flat_set<int> getAdjacentLandTransportedClusters(MAP *tile);
bool isAdjacentLandCluster(MAP *tile);
bool isAdjacentSeaOrLandCluster(MAP *tile);
bool isSameDstAdjacentAirCluster(int chassisId, int speed, MAP *src, MAP *dst);
bool isSameDstAdjacentSeaCluster(MAP *src, MAP *dst);
bool isSameDstAdjacentLandTransportedCluster(MAP *src, MAP *dst);

