#ifndef __AI_H__
#define __AI_H__

#include "main.h"
#include "game.h"
#include "move.h"
#include "wtp.h"

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
}
;

struct FormerOrder
{
	int vehicleId;
	VEH *vehicle;
	int x;
	int y;
	int action;
};

struct TERRAFORMING_OPTION
{
	bool sea;
	bool worldRainfall;
	double bonusValue;
	int count;
	int actions[10];
};

void ai_strategy(int id);
void prepareFormerOrders();
void populateLists();
void processAvailableFormers();
void optimizeFormerDestinations();
int enemyMoveFormer(int vehicleId);
void setFormerOrder(FormerOrder *formerOrder);
void generateFormerOrder(FormerOrder *formerOrder);
double calculateTerraformingScore(int vehicleId, int x, int y, int *bestTerraformingAction);
double calculateYieldScore(int x, int y);
bool isOwnWorkableTile(int x, int y);
bool isOwnWorkedTile(int x, int y);
bool isTerraformingAvailable(int x, int y, int action);
void generateTerraformingChange(int *improvedTileRocks, int *improvedTileItems, int action);
int calculateTerraformingTime(int vehicleId, int x, int y, int action);
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
int calculateLostTerraformingTime(int vehicleId, int x, int y, int tileItems, int improvedTileItems);
void sendVehicleToDestination(int vehicleId, int x, int y);

#endif // __AI_H__
