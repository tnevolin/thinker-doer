const unsigned int MAX_CLUSTER_SIZE = 10;
const int MOVEMENT_INFINITY = INT_MAX / 2;

struct Cluster
{
	Location coordinates;
	std::vector<MAP *> transferLocations;
	
	Cluster(Location _coordinates);
	
};

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

long long getCachedMovementCostIndex(MAP *origin, MAP *destination, MOVEMENT_TYPE movementType, int factionId, bool ignoreHostile);
int getCachedMovementCost(long long index);
void setCachedMovementCost(long long index, int movementCost);

void precomputeAIRouteData();

// current and potential colony movement
void populateBuildSiteDMovementCosts(MAP *origin, MOVEMENT_TYPE movementType, int factionId, bool ignoreHostile);
void populateColonyMovementCosts();
std::set<MAP *> getOneTurnActionLocations(int vehicleId);

int getAMovementCost(MAP *origin, MAP *destination, MOVEMENT_TYPE movementType, int factionId, bool ignoreHostile);

int getUnitATravelTime(MAP *origin, MAP *destination, int factionId, int unitId, int speed, bool ignoreHostile, int action);
int getGravshipUnitATravelTime(MAP *origin, MAP *destination, int speed, int action);
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

int getRouteVectorDistance(MAP *tile1, MAP *tile2);

