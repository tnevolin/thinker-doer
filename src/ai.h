#ifndef __AI_H__
#define __AI_H__

#include <float.h>
#include "main.h"
#include "game.h"
#include "move.h"

/**
This array is used for both adjacent tiles and base radius tiles.
*/
const int ADJACENT_TILE_OFFSET_COUNT = 9;
const int BASE_TILE_OFFSET_COUNT = 21;
const int BASE_TILE_OFFSETS[BASE_TILE_OFFSET_COUNT][2] =
{
	{+0,+0},
	{+1,-1},
	{+2,+0},
	{+1,+1},
	{+0,+2},
	{-1,+1},
	{-2,+0},
	{-1,-1},
	{+0,-2},
	{+2,-2},
	{+2,+2},
	{-2,+2},
	{-2,-2},
	{+1,-3},
	{+3,-1},
	{+3,+1},
	{+1,+3},
	{-1,+3},
	{-3,+1},
	{-3,-1},
	{-1,-3},
};

struct TERRAFORMING_OPTION
{
	// recognizable name
	const char *name;
	// land or sea
	bool sea;
	// applies to rocky tile only
	bool rocky;
	// area effect
	bool area;
	// requires base yield computation
	bool yield;
	// ignores this option until technology for first action is available
	bool firstActionRequired;
	// marks competing terraforming options around same base
	bool ranked;
	std::vector<int> actions;
};

// regular terraforming options
const std::vector<TERRAFORMING_OPTION> REGULAR_TERRAFORMING_OPTIONS =
{
	// land
	{"rocky mine", false, true , false, true , false, true , {FORMER_MINE, FORMER_ROAD}},								// 00
	{"mine"      , false, false, false, true , false, true , {FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE, FORMER_ROAD}},	// 01
	{"collector" , false, false, false, true , false, true , {FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR}},				// 02
	{"condenser" , false, false, true , true , true , false, {FORMER_CONDENSER, FORMER_FARM, FORMER_SOIL_ENR}},			// 03
	{"mirror"    , false, false, true , true , true , false, {FORMER_ECH_MIRROR, FORMER_FARM, FORMER_SOIL_ENR}},		// 04
	{"borehole"  , false, false, false, true , true , true , {FORMER_THERMAL_BORE}},									// 05
	{"forest"    , false, false, false, true , true , true , {FORMER_FOREST}},											// 06
	{"fungus"    , false, false, false, true , true , true , {FORMER_PLANT_FUNGUS}},									// 07
	// sea
	{"platform"  , true , false, false, true , false, true , {FORMER_FARM, FORMER_MINE}},								// 11
	{"harness"   , true , false, false, true , false, true , {FORMER_FARM, FORMER_SOLAR}},								// 12
};
// aquifer
const std::vector<TERRAFORMING_OPTION> AQUIFER_TERRAFORMING_OPTIONS =
{
	// land
	{"aquifer"   , false, false, true , true , true , false, {FORMER_AQUIFER}},											// 08
};
// network
const std::vector<TERRAFORMING_OPTION> NETWORK_TERRAFORMING_OPTIONS =
{
	// land
	{"road/tube" , false, false, false, false, true , false, {FORMER_ROAD, FORMER_MAGTUBE}},											// 10
};

/**
Prohibits building improvements too close to each other or existing improvements.
*/
const std::unordered_map<int, int> PROXIMITY_RULES =
{
	{FORMER_CONDENSER, 2},
	{FORMER_THERMAL_BORE, 1},
	{FORMER_AQUIFER, 1},
};

struct MAP_INFO
{
	int x;
	int y;
	MAP *tile;
};

struct BASE_INFO
{
	int id;
	BASE *base;
};

struct BASE_INCOME
{
	int id;
	BASE *base;
	int workedTiles;
	int nutrientSurplus;
	int mineralSurplus;
	int energySurplus;
};

struct VEHICLE_INFO
{
	int id;
	VEH *vehicle;
};

struct MAP_STATE
{
    byte climate;
    byte rocks;
    int items;
};

struct FORMER_ORDER
{
	int id;
	VEH *vehicle;
	int x;
	int y;
	int action;
};

struct TERRAFORMING_SCORE
{
	BASE *base = NULL;
	const TERRAFORMING_OPTION *option = NULL;
	int action = -1;
	double score = 0.0;
	bool ranked = false;
};

struct TERRAFORMING_REQUEST
{
	int x;
	int y;
	MAP *tile;
	const TERRAFORMING_OPTION *option;
	int action;
	double score;
	bool ranked;
	int rank;
};

struct AFFECTED_BASE_SET
{
	std::set<BASE_INFO> affectedBaseSets[2];
};

struct PATH_ELEMENT
{
	int x;
	int y;
	MAP *tile;
	bool zoc;
	int movementPoints;
	int firstStepX;
	int firstStepY;
	bool zocEncountered;
};

struct YIELD
{
	int nutrient;
	int mineral;
	int energy;
};

/**
These terraforming orders affect base terraforming rank.
*/
const std::unordered_set<int> yieldTerraformingOrders = {ORDER_FARM, ORDER_SOIL_ENRICHER, ORDER_MINE, ORDER_SOLAR_COLLECTOR, ORDER_PLANT_FOREST, ORDER_REMOVE_FUNGUS, ORDER_PLANT_FUNGUS, ORDER_CONDENSER, ORDER_ECHELON_MIRROR, ORDER_THERMAL_BOREHOLE, ORDER_DRILL_AQUIFIER, ORDER_TERRAFORM_LEVEL};

/**
These terraforming orders affect surrounding tiles.
*/
const std::unordered_set<int> wideRangeTerraformingOrders = {ORDER_CONDENSER, ORDER_ECHELON_MIRROR, ORDER_DRILL_AQUIFIER};

void ai_moveUpkeep(int id);
void prepareFormerOrders();
void populateLists();
void generateTerraformingRequests();
void generateTerraformingRequest(MAP_INFO *mapInfo);
bool compareTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1,TERRAFORMING_REQUEST terraformingRequest2);
void sortBaseTerraformingRequests();
bool compareRankedTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2);
void rankTerraformingRequests();
void assignFormerOrders();
void optimizeFormerDestinations();
void finalizeFormerOrders();
int enemyMoveFormer(int vehicleId);
void setFormerOrder(int id, VEH *vehicle, FORMER_ORDER *formerOrder);
void calculateRegularTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateAquiferTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateNetworkTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateYieldImprovementScore(MAP_INFO *mapInfo, MAP_STATE *currentMapState, MAP_STATE *improvedMapState, TERRAFORMING_SCORE *terraformingScore);
bool isOwnWorkableTile(int x, int y);
bool isTerraformingCompleted(MAP_INFO *mapInfo, int action);
bool isTerraformingAvailable(int x, int y, int action);
bool isTerraformingRequired(MAP *tile, int action);
bool isRemoveFungusRequired(int action);
bool isLevelTerrainRequired(int action);
void generateTerraformingChange(MAP_STATE *mapState, int action);
bool isVehicleTerraforming(int vehicleId);
bool isVehicleSupply(VEH *vehicle);
bool isVehicleFormer(VEH *vehicle);
bool isTileTargettedByVehicle(VEH *vehicle, MAP *tile);
bool isVehicleConvoying(VEH *vehicle);
bool isVehicleTerraforming(VEH *vehicle);
bool isVehicleMoving(VEH *vehicle);
MAP *getVehicleDestination(VEH *vehicle);
bool isTileTakenByOtherFormer(MAP *tile);
bool isTileTerraformed(MAP *tile);
bool isTileTargettedByOtherFormer(MAP *tile);
bool isTileWorkedByOtherBase(BASE *base, MAP *tile);
bool isVehicleTargetingTile(int vehicleId, int x, int y);
void computeBase(int baseId);
void setTerraformingAction(int vehicleId, int action);
void buildImprovement(int vehicleId);
bool sendVehicleToDestination(int id, int x, int y);
bool isNearbyCondeserUnderConstruction(int x, int y);
bool isAdjacentBoreholePresentOrUnderConstruction(int x, int y);
bool isAdjacentRiverPresentOrUnderConstruction(int x, int y);
void getMapState(MAP_INFO *mapInfo, MAP_STATE *mapState);
void setMapState(MAP_INFO *mapInfo, MAP_STATE *mapState);
void copyMapState(MAP_STATE *destinationMapState, MAP_STATE *sourceMapState);
int calculateTerraformingTime(int action, int items, int rocks, VEH* vehicle);
int getBaseTerraformingRank(BASE *base);
BASE *findAffectedBase(int x, int y);
char *getTerraformingActionName(int action);
int calculateClosestAvailableFormerRange(MAP_INFO *mapInfo);
double calculateNetworkScore(MAP_INFO *mapInfo, int action);
bool isTowardBaseDiagonal(int x, int y, int dxSign, int dySign);
bool isTowardBaseHorizontal(int x, int y, int dxSign);
bool isTowardBaseVertical(int x, int y, int dySign);
MAP *getMapTile(int x, int y);
int getTerraformingRegion(int region);
bool isBaseWorkedTile(BASE *base, int x, int y);
double calculateExclusivityBonus(MAP_INFO *mapInfo, const std::vector<int> *actions);
bool hasNearbyTerraformingRequestAction(std::vector<TERRAFORMING_REQUEST>::iterator begin, std::vector<TERRAFORMING_REQUEST>::iterator end, int action, int x, int y, int range);
double estimateCondenserExtraYieldScore(MAP_INFO *mapInfo);
double estimateEchelonMirrorExtraYieldScore(MAP_INFO *mapInfo);
double calculateEchelonMirrorRemovalScore(MAP_INFO *mapInfo);
double estimateAquiferExtraYieldScore(MAP_INFO *mapInfo);
void findPathStep(int id, int x, int y, MAP_INFO *step);
bool isInferiorYield(std::vector<YIELD> *yields, int nutrient, int mineral, int energy, std::vector<YIELD>::iterator *self);
double calculateYieldWeightedScore(double nutrientDemand, double mineralDemand, double nutrient, double mineral, double energy);

#endif // __AI_H__
