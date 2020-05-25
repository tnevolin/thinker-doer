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
	bool requiresFarm;
	bool removeFungusBeforeTech;
	bool removeFungusAfterTech;
	bool level;
	int action;
	int item;
};

int giveOrderToFormer(int vehicleId);
bool isTileOccupiedByBase(MAP *tile);
bool isVehicleTerraforming(VEH *vehicle);
bool isVehicleFormer(VEH *vehicle);
bool isVehicleTerraforming(VEH *vehicle);
bool isVehicleMoving(VEH *vehicle);
MAP *getVehicleDestination(VEH *vehicle);
bool isTileTakenByOtherFormer(VEH *primaryVehicle, MAP *primaryVehicleTargetTile);
bool isTileWorkedByOtherBase(BASE *base, MAP *tile);
double calculateTileYieldScore(int baseId, BASE *base, int tileX, int tileY);
double calculateImprovementScore(int vehicleId, int baseIndex, BASE *base, MAP *tile, int tileX, int tileY, double previousYieldScore, int *action);

#endif // __AI_H__
