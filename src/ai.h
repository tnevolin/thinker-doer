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
	// land or sea
	bool sea;
	// applies to rocky tile only
	bool rocky;
	// modifies yield
	bool yield;
	// do not evaluate until last action is available
	bool lastActionRequired;
	std::vector<int> actions;
};

/**
AI terraforming options
*/
const std::vector<TERRAFORMING_OPTION> TERRAFORMING_OPTIONS =
{
	// land
	{false, true , true , false, {FORMER_MINE, FORMER_ROAD}},							// 00
	{false, false, true , false, {FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE}}, 			// 01
	{false, false, true , false, {FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR}},			// 02
	{false, false, true , true , {FORMER_FARM, FORMER_SOIL_ENR, FORMER_CONDENSER}},		// 03
	{false, false, true , true , {FORMER_FARM, FORMER_SOIL_ENR, FORMER_ECH_MIRROR}},	// 04
	{false, false, true , true , {FORMER_THERMAL_BORE}},								// 05
	{false, false, true , false, {FORMER_FOREST}},										// 06
	{false, false, true , true , {FORMER_PLANT_FUNGUS}},								// 07
	{false, false, true , true , {FORMER_AQUIFER}},										// 08
	{false, false, false, false, {FORMER_ROAD}},										// 09
	{false, false, false, true , {FORMER_MAGTUBE}},										// 10
	// sea
	{true , false, true , false, {FORMER_FARM, FORMER_MINE}},							// 11
	{true , false, true , false, {FORMER_FARM, FORMER_SOLAR}},							// 12
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

struct TERRAFORMING_STATE
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
	bool yield = false;
	BASE *base = NULL;
	int action = -1;
	double score = -DBL_MAX;
};

struct TERRAFORMING_REQUEST
{
	int x;
	int y;
	MAP *tile;
	bool yield;
	int action;
	double score;
	int rank;
};

struct AFFECTED_BASE_SET
{
	std::set<BASE_INFO> affectedBaseSets[2];
};

void ai_strategy(int id);
void prepareFormerOrders();
void populateLists();
void generateTerraformingRequests();
void generateTerraformingRequest(MAP_INFO *mapInfo);
bool compareBaseTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1,TERRAFORMING_REQUEST terraformingRequest2);
void sortBaseTerraformingRequests();
void rankTerraformingRequests();
void assignFormerOrders();
void optimizeFormerDestinations();
void finalizeFormerOrders();
int enemyMoveFormer(int vehicleId);
void setFormerOrder(FORMER_ORDER *formerOrder);
void calculateTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateYieldImprovementScore(MAP_INFO *mapInfo, int affectedRange, TERRAFORMING_STATE currentLocalTerraformingState[9], TERRAFORMING_STATE improvedLocalTerraformingState[9], TERRAFORMING_SCORE *terraformingScore);
bool isOwnWorkableTile(int x, int y);
bool isTerraformingCompleted(int x, int y, int action);
bool isTerraformingAvailable(int x, int y, int action);
bool isTerraformingRequired(MAP *tile, int action);
bool isRemoveFungusRequired(int action);
bool isLevelTerrainRequired(int action);
void generateTerraformingChange(TERRAFORMING_STATE *terraformingState, int action);
bool isVehicleTerraforming(int vehicleId);
bool isVehicleFormer(VEH *vehicle);
bool isTileTargettedByVehicle(VEH *vehicle, MAP *tile);
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
void sendVehicleToDestination(int vehicleId, int x, int y);
bool isAdjacentBoreholePresentOrUnderConstruction(int x, int y);
bool isAdjacentRiverPresentOrUnderConstruction(int x, int y);
void storeLocalTerraformingState(int x, int y, TERRAFORMING_STATE localTerraformingState[9]);
void setLocalTerraformingState(int x, int y, TERRAFORMING_STATE localTerraformingState[9]);
void copyLocalTerraformingState(TERRAFORMING_STATE sourceLocalTerraformingState[9], TERRAFORMING_STATE destinationLocalTerraformingState[9]);
void increaseMoistureAround(TERRAFORMING_STATE localTerraformingState[9]);
void createRiversAround(TERRAFORMING_STATE localTerraformingState[9]);
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

#endif // __AI_H__
