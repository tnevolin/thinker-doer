const unsigned int MAX_CLUSTER_SIZE = 10;
const int MOVEMENT_INFINITY = INT_MAX / 2;

/*
Route node for D path search algorithm.
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
void populateBaseEnemyMovementImpediments();

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

void populateAIFactionMovementInfo();
void populateAIFactionNonCombatMovementInfo();
void generateNonCombatCluster(int initialTileIndex, int surfaceIndex, int factionId);
int initializeNonCombatCluster(int initialTileIndex, int surfaceIndex, int factionId, int clusterId, robin_hood::unordered_flat_set<MovementType> &movementTypes);
int generateNonCombatClusterLandmark(int initialTileIndex, int surfaceIndex, int factionId, int clusterId, MovementType movementType, int landmarkId);

long long getCachedLMovementCostKey(MAP *origin, MAP *destination, int factionId, int clusterType, int surfaceType, MovementType movementType);
int getCachedLMovementCost(long long index);
void setCachedLMovementCost(long long index, int movementCost);

int getLMovementCost(MAP *origin, MAP *destination, int factionId, int clusterType, int surfaceType, MovementType movementType);

int getUnitLTravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, int action);
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
MovementType getUnitApproximateMovementType(int factionId, int unitId);

