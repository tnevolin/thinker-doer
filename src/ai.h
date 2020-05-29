#ifndef __AI_H__
#define __AI_H__

#include "main.h"
#include "game.h"

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
	int count;
	int actions[10];
};

int giveOrderToFormer(int vehicleId);
double calculateTerraformingScore(int vehicleId, int tileX, int tileY, int *bestTerraformingOptionIndex);
double calculateYieldScore(int factionId, int tileX, int tileY);
bool isOwnWorkTile(int factionId, int tileX, int tileY);
bool isTerraformingAvailable(int factionId, int x, int y, int action);
void applyTerraforming(MAP *tile, int action);
int calculateTerraformingTime(int vehicleId, int action);
void carryFormerAIOrders(int vehicleId);
bool isFormerCarryingAIOrders(int vehicleId);
bool isVehicleTerraforming(int vehicleId);
bool isVehicleFormer(VEH *vehicle);
bool isVehicleTerraforming(VEH *vehicle);
bool isVehicleMoving(int vehicleId);
MAP *getVehicleDestination(int vehicleId);
bool isTileTakenByOtherFormer(int primaryVehicleId, int x, int y);
bool isTileWorkedByOtherBase(BASE *base, MAP *tile);
bool isFormerTargettingTile(int vehicleId, int x, int y);

#endif // __AI_H__
