#pragma once

#include <float.h>
#include <unordered_map>
#include "main.h"
#include "game.h"
#include "move.h"
#include "wtp.h"
#include "game_wtp.h"

struct TERRAFORMING_OPTION
{
	// recognizable name
	const char *name;
	// land or ocean
	bool ocean;
	// applies to rocky tile only
	bool rocky;
	// area effect
	bool area;
	// requires base yield computation
	bool yield;
	// main action that is required to be discovered
	int requiredAction;
	// marks competing terraforming options around same base
	bool ranked;
	// list of actions
	std::vector<int> actions;
};

// regular terraforming options
const std::vector<TERRAFORMING_OPTION> CONVENTIONAL_TERRAFORMING_OPTIONS =
{
	// land
	{"rocky mine", false, true , false, true , FORMER_MINE        , true , {FORMER_MINE, FORMER_ROAD}},								// 00
	{"mine"      , false, false, false, true , FORMER_MINE        , true , {FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE, FORMER_ROAD}},	// 01
	{"collector" , false, false, false, true , FORMER_SOLAR       , true , {FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR}},				// 02
	{"condenser" , false, false, true , true , FORMER_CONDENSER   , false, {FORMER_CONDENSER, FORMER_FARM, FORMER_SOIL_ENR}},			// 03
	{"mirror"    , false, false, true , true , FORMER_ECH_MIRROR  , false, {FORMER_ECH_MIRROR, FORMER_FARM, FORMER_SOIL_ENR}},		// 04
	{"borehole"  , false, false, false, true , FORMER_THERMAL_BORE, true , {FORMER_THERMAL_BORE}},									// 05
	{"forest"    , false, false, false, true , FORMER_FOREST      , true , {FORMER_FOREST}},											// 06
	{"fungus"    , false, false, false, true , FORMER_PLANT_FUNGUS, true , {FORMER_PLANT_FUNGUS}},									// 07
	// sea
	{"platform"  , true , false, false, true , FORMER_MINE        , true , {FORMER_FARM, FORMER_MINE}},								// 08
	{"harness"   , true , false, false, true , FORMER_SOLAR       , true , {FORMER_FARM, FORMER_SOLAR}},								// 09
};
// aquifer
const TERRAFORMING_OPTION AQUIFER_TERRAFORMING_OPTION =
	// land
	{"aquifer"   , false, false, true , true , FORMER_AQUIFER     , false, {FORMER_AQUIFER}}
;
// raise land
const TERRAFORMING_OPTION RAISE_LAND_TERRAFORMING_OPTION =
	// land
	{"raise"     , false, false, true , true , FORMER_RAISE_LAND  , false, {FORMER_RAISE_LAND}}
;
// network
const TERRAFORMING_OPTION NETWORK_TERRAFORMING_OPTION =
	// land
	{"road/tube" , false, false, false, false, FORMER_ROAD        , false, {FORMER_ROAD, FORMER_MAGTUBE}}
;
// sensor
const TERRAFORMING_OPTION LAND_SENSOR_TERRAFORMING_OPTION =
	// land
	{"sensor"    , false, false, false, false, FORMER_SENSOR      , false, {FORMER_SENSOR}}
;
const TERRAFORMING_OPTION OCEAN_SENSOR_TERRAFORMING_OPTION =
	// ocean
	{"sensor"    , true , false, false, false, FORMER_SENSOR      , false, {FORMER_SENSOR}}
;

/*
Prohibits building improvements too close to each other or existing improvements.
*/
struct PROXIMITY_RULE
{
	// cannot build within this range of same existing improvement
	int existingDistance;
	// cannot build within this range of same building improvement
	int buildingDistance;
};
const std::unordered_map<int, PROXIMITY_RULE> PROXIMITY_RULES =
{
	{FORMER_FOREST, {0, 2}},
	{FORMER_CONDENSER, {0, 2}},
	{FORMER_ECH_MIRROR, {0, 1}},
	{FORMER_THERMAL_BORE, {1, 1}},
	{FORMER_AQUIFER, {1, 3}},
	{FORMER_RAISE_LAND, {0, 1}},
	{FORMER_SENSOR, {2, 2}},
};

struct VEHICLE_INFO
{
	int id;
	VEH *vehicle;
};

struct FORMER_ORDER
{
	int id;
	VEH *vehicle;
	int x = -1;
	int y = -1;
	int action = -1;
	
	FORMER_ORDER(int _id, VEH *_vehicle)
	{
		this->id = _id;
		this->vehicle = _vehicle;
		
	}
	
};

struct TERRAFORMING_SCORE
{
	const TERRAFORMING_OPTION *option = NULL;
	int action = -1;
	double score = 0.0;
};

struct SURPLUS_EFFECT
{
	BASE *base = NULL;
	double score = 0.0;
};

struct TERRAFORMING_REQUEST
{
	MAP *tile;
	const TERRAFORMING_OPTION *option;
	int action;
	double score;
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

const struct
{
	double rainfall = 1.0;
	double rockiness = 1.0;
	double elevation = 1.0;
} EXCLUSIVITY_LEVELS;

/*
These terraforming orders affect base terraforming rank.
*/
const std::unordered_set<int> yieldTerraformingOrders = {ORDER_FARM, ORDER_SOIL_ENRICHER, ORDER_MINE, ORDER_SOLAR_COLLECTOR, ORDER_PLANT_FOREST, ORDER_REMOVE_FUNGUS, ORDER_PLANT_FUNGUS, ORDER_CONDENSER, ORDER_ECHELON_MIRROR, ORDER_THERMAL_BOREHOLE, ORDER_DRILL_AQUIFIER, ORDER_TERRAFORM_LEVEL};

/*
These terraforming orders affect surrounding tiles.
*/
const std::unordered_set<int> wideRangeTerraformingOrders = {ORDER_CONDENSER, ORDER_ECHELON_MIRROR, ORDER_DRILL_AQUIFIER};

struct BaseTerraformingInfo
{
	int unimprovedWorkedTileCount;
	int ongoingYieldTerraformingCount;
	std::vector<MAP *> landRockyTiles;
};

void moveFormerStrategy();
void populateLists();
void moveLandFormerStrategy();
void moveSeaFormerStrategy();
void cancelRedundantOrders();
void generateTerraformingRequests();
void generateConventionalTerraformingRequest(MAP *tile);
void generateAquiferTerraformingRequest(MAP *tile);
void generateRaiseLandTerraformingRequest(MAP *tile);
void generateNetworkTerraformingRequest(MAP *tile);
void generateSensorTerraformingRequest(MAP *tile);
void generateTerraformingRequest(MAP *tile);
bool compareTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2);
void sortTerraformingRequests();
void applyProximityRules();
void assignFormerOrders();
void optimizeFormerDestinations();
void finalizeFormerOrders();
int moveFormer(int vehicleId);
void setFormerOrder(int vehicleId, FORMER_ORDER *formerOrder);
double computeImprovementSurplusEffectScore(MAP *tile, MAP_STATE *currentMapState, MAP_STATE *improvedMapState);
void calculateConventionalTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateAquiferTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateRaiseLandTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateNetworkTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateSensorTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore);
bool isOwnWorkableTile(int x, int y);
bool isTerraformingCompleted(MAP *tile, int action);
bool isVehicleTerrafomingOrderCompleted(int vehicleId);
bool isTerraformingAvailable(MAP *tile, int action);
bool isTerraformingRequired(MAP *tile, int action);
bool isRemoveFungusRequired(int action);
bool isLevelTerrainRequired(bool ocean, int action);
bool isVehicleTerraforming(int vehicleId);
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
void buildImprovement(int vehicleId);
bool sendVehicleToDestination(int id, int x, int y);
bool isNearbyForestUnderConstruction(int x, int y);
bool isNearbyCondeserUnderConstruction(int x, int y);
bool isNearbyMirrorUnderConstruction(int x, int y);
bool isNearbyBoreholePresentOrUnderConstruction(int x, int y);
bool isNearbyRiverPresentOrUnderConstruction(int x, int y);
bool isNearbyRaiseUnderConstruction(int x, int y);
bool isNearbySensorPresentOrUnderConstruction(int x, int y);
int calculateTerraformingTime(int action, int items, int rocks, VEH* vehicle);
int getBaseTerraformingRank(BASE *base);
BASE *findAffectedBase(int x, int y);
char *getTerraformingActionName(int action);
double getSmallestAvailableFormerTravelTime(MAP *tile);
double calculateNetworkScore(MAP *tile, int action);
bool isTowardBaseDiagonal(int x, int y, int dxSign, int dySign);
bool isTowardBaseHorizontal(int x, int y, int dxSign);
bool isTowardBaseVertical(int x, int y, int dySign);
double calculateSensorScore(MAP *tile, int action);
bool isBaseWorkedTile(BASE *base, int x, int y);
double calculateExclusivityBonus(MAP *tile, const std::vector<int> *actions, bool levelTerrain);
bool hasNearbyTerraformingRequestAction(std::vector<TERRAFORMING_REQUEST>::iterator begin, std::vector<TERRAFORMING_REQUEST>::iterator end, int action, int x, int y, int range);
double estimateCondenserExtraYieldScore(MAP *tile, const std::vector<int> *actions);
double estimateAquiferExtraYieldScore(MAP *tile);
double estimateRaiseLandExtraYieldScore(MAP *tile, int cost);
MAP *findPathStep(int id, int x, int y);
bool isInferiorYield(std::vector<YIELD> *yields, int nutrient, int mineral, int energy, std::vector<YIELD>::iterator *self);
bool isRaiseLandSafe(MAP *tile);
double calculateResourceScore(double nutrient, double mineral, double energy);
double calculateBaseResourceScore(double populationSize, double nutrientSurplus, double mineralSurplus, double nutrient, double mineral, double energy);
double computeImprovementBaseSurplusEffectScore(int baseId, MAP *tile, MAP_STATE *currentMapState, MAP_STATE *improvedMapState, std::vector<MAP *> affectedTiles);
bool isreachable(int id, int x, int y);
int getConnectedRegion(int region);
bool isLandRockyTile(MAP *tile);
int getUnimprovedWorkedTileCount(int baseId);
int getOngoingYieldTerraformingCount(int baseId);

