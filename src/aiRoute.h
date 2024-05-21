const unsigned int MAX_CLUSTER_SIZE = 10;
const int MOVEMENT_INFINITY = INT_MAX / 2;

const int IMPEDIMENT_RANGE = 3;
const double NEUTRAL_IMPEDIMENT = 1.0 / (double)(IMPEDIMENT_RANGE * 2 + 1);
const double ATTACKING_IMPEDIMENT_COMBAT = 1.0 * NEUTRAL_IMPEDIMENT;
const double ATTACKING_IMPEDIMENT_NON_COMBAT = 5.0 * NEUTRAL_IMPEDIMENT;

/*
Route node for path search algorithm.
*/
struct Node
{
	MAP *tile;
	int movementCost;
};

/*
Route node for A* path search algorithm.
*/
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

/*
F value map element.
*/
struct FValue
{
	MAP *tile;
	int f;
	
	FValue(MAP *_tile, int _f)
	{
		this->tile = _tile;
		this->f = _f;
	}
	
};

struct FValueComparator
{
	bool operator() (const FValue &o1, const FValue &o2);
};

class FValueHeap
{
private:
	
	std::vector<std::vector<MAP *>> fGroups;
	int minF = INT_MAX;
	
public:
	
	bool empty();
	void add(int f, MAP *tile);
	FValue top();
	
};

long long getCachedAMovementCostKey(MAP *origin, MAP *destination, MovementType movementType, int factionId, bool ignoreHostile);
int getCachedMovementCost(long long index);
void setCachedMovementCost(long long index, int movementCost);

void precomputeAIRouteData();
void populateTileMovementRestrictions();

// current and potential colony movement
void populateBuildSiteDMovementCosts(MAP *origin, MovementType movementType, int factionId, bool ignoreHostile);
void populateColonyMovementCosts();
robin_hood::unordered_flat_set<MAP *> getOneTurnActionLocations(int vehicleId);

// ==================================================
// Generic algorithms
// ==================================================

// ==================================================
// A* path finding algorithm
// ==================================================

int getAMovementCost(MAP *origin, MAP *destination, MovementType movementType, int factionId, bool ignoreHostile);

int getUnitATravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool ignoreHostile, int action);
int getAirRangedUnitATravelTime(MAP *origin, MAP *destination, int speed, int action);
int getSeaUnitATravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool ignoreHostile, bool action);
int getLandUnitATravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool ignoreHostile, bool action);

int getVehicleATravelTime(int vehicleId, MAP *origin, MAP *destination, bool ignoreHostile, int action);
int getLandVehicleATravelTime(int vehicleId, MAP *origin, MAP *destination, bool ignoreHostile, int action);
int getVehicleATravelTime(int vehicleId, MAP *origin, MAP *destination);
int getVehicleATravelTime(int vehicleId, MAP *destination);
int getVehicleActionATravelTime(int vehicleId, MAP *destination);
int getVehicleAttackATravelTime(int vehicleId, MAP *destination, bool ignoreHostile);
int getVehicleATravelTimeByTaskType(int vehicleId, MAP *destination, TaskType taskType);

// ==================================================
// Landmark estimated movementCost algorithm
// ==================================================

void populateMovementInfos();
void populateFactionMovementInfo(int factionId, int clusterType);
void generateCluster(int factionId, int clusterType, int surfaceIndex, int initialTileIndex);
int initializeCluster(int factionId, int clusterId, int surfaceIndex, std::vector<MovementType> &movementTypes, int initialTileIndex);
int generateLandmark(int factionId, int clusterId, int surfaceIndex, MovementType movementType, int landmarkId, int initialTileIndex);

long long getCachedLMovementCostKey(MAP *origin, MAP *destination, int factionId, int clusterType, int surfaceType, MovementType movementType);
int getCachedLMovementCost(long long key);
void setCachedLMovementCost(long long key, int movementCost);

int getLMovementCost(MAP *origin, MAP *destination, int factionId, int clusterType, int surfaceType, MovementType movementType);

int getUnitLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, int action);
int getUnitActionLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId);
int getUnitAttackLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId);
int getGravshipUnitLTravelTime(MAP *origin, MAP *destination, int speed, int action);
int getAirRangedUnitLTravelTime(MAP *origin, MAP *destination, int speed, int action);
int getSeaUnitLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool action);
int getLandUnitLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool action);

int getVehicleLTravelTime(int vehicleId, MAP *origin, MAP *destination, int action);
int getLandVehicleLTravelTime(int vehicleId, MAP *origin, MAP *destination, int action);
int getVehicleLTravelTime(int vehicleId, MAP *origin, MAP *destination);
int getVehicleLTravelTime(int vehicleId, MAP *destination);
int getVehicleActionLTravelTime(int vehicleId, MAP *destination);
int getVehicleAttackLTravelTime(int vehicleId, MAP *destination);
int getVehicleLTravelTimeByTaskType(int vehicleId, MAP *destination, TaskType taskType);

// ==================================================
// Helper functions
// ==================================================

int getRouteVectorDistance(MAP *tile1, MAP *tile2);
MovementType getUnitMovementType(int factionId, int unitId);

