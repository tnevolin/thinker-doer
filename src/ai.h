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

struct TERRAFORMING_OPTION
{
	bool sea;
	bool removeFungus;
	bool levelTerrain;
	int action;
	double bonusValue;
};

int giveOrderToFormer(int vehicleId);
double calculateTerraformingScore(int vehicleId, int x, int y, int *bestTerraformingAction);
double calculateYieldScore(int factionId, int tileX, int tileY);
bool isOwnWorkTile(int factionId, int tileX, int tileY);
bool isTerraformingAvailable(int factionId, int x, int y, int action);
void generateTerraformingChange(int *improvedTileRocks, int *improvedTileItems, int action);
int calculateTerraformingTime(int vehicleId, int action);
bool isVehicleTerraforming(int vehicleId);
bool isVehicleFormer(VEH *vehicle);
bool isVehicleTerraforming(VEH *vehicle);
bool isVehicleMoving(VEH *vehicle);
MAP *getVehicleDestination(VEH *vehicle);
bool isTileTakenByOtherFormer(VEH *primaryVehicle, MAP *tile);
bool isTileWorkedByOtherBase(BASE *base, MAP *tile);
bool isVehicleTargetingTile(int vehicleId, int x, int y);
void computeBase(int baseId);
int setTerraformingAction(int vehicleId, int action);
int buildImprovement(int vehicleId);
int canBuildRoadOrTube(int factionId, MAP *tile);
int calculateLostTerraformingTime(int vehicleId, int tileItems, int improvedTileItems);

#endif // __AI_H__
