#pragma once

#include <float.h>
#include <map>
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

// conventional terraforming options

const TERRAFORMING_OPTION TO_ROCKY_MINE = {"rocky mine", false, true, false, true, FORMER_MINE, true, {FORMER_ROAD, FORMER_MINE}};
const TERRAFORMING_OPTION TO_MINE = {"mine", false, false, false, true, FORMER_MINE, true, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE}};
const TERRAFORMING_OPTION TO_SOLAR_COLLECTOR = {"solar collector", false, false, false, true, FORMER_SOLAR, true, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR}};
const TERRAFORMING_OPTION TO_CONDENSER = {"condenser", false, false, true, true, FORMER_CONDENSER, false, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_CONDENSER}};
const TERRAFORMING_OPTION TO_ECHELON_MIRROR = {"echelon mirror", false, false, true, true, FORMER_ECH_MIRROR, false, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_ECH_MIRROR}};
const TERRAFORMING_OPTION TO_THERMAL_BOREHOLE = {"thermal borehole", false, false, false, true, FORMER_THERMAL_BORE, true, {FORMER_ROAD, FORMER_THERMAL_BORE}};
const TERRAFORMING_OPTION TO_FOREST = {"forest", false, false, false, true, FORMER_FOREST, true, {FORMER_ROAD, FORMER_FOREST}};
const TERRAFORMING_OPTION TO_LAND_FUNGUS = {"land fungus", false, false, false, true, FORMER_PLANT_FUNGUS, true, {FORMER_ROAD, FORMER_PLANT_FUNGUS}};
const TERRAFORMING_OPTION TO_MINING_PLATFORM = {"mining platform", true, false, false, true, FORMER_MINE, true, {FORMER_FARM, FORMER_MINE}};
const TERRAFORMING_OPTION TO_TIDAL_HARNESS = {"tidal harness", true, false, false, true, FORMER_SOLAR, true, {FORMER_FARM, FORMER_SOLAR}};
const TERRAFORMING_OPTION TO_SEA_FUNGUS = {"sea fungus", true, false, false, true, FORMER_PLANT_FUNGUS, true, {FORMER_PLANT_FUNGUS}};

const std::vector<const TERRAFORMING_OPTION *> CONVENTIONAL_TERRAFORMING_OPTIONS =
{
	// land
	&TO_ROCKY_MINE,
	&TO_MINE,
	&TO_SOLAR_COLLECTOR,
	&TO_CONDENSER,
	&TO_ECHELON_MIRROR,
	&TO_THERMAL_BOREHOLE,
	&TO_FOREST,
	&TO_LAND_FUNGUS,
	&TO_MINING_PLATFORM,
	&TO_TIDAL_HARNESS,
	&TO_SEA_FUNGUS,
};

const std::vector<const TERRAFORMING_OPTION *> GENERIC_TERRAFORMING_OPTIONS =
{
	&TO_MINE,
	&TO_SOLAR_COLLECTOR,
	&TO_CONDENSER,
	&TO_ECHELON_MIRROR,
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
const std::map<int, PROXIMITY_RULE> PROXIMITY_RULES =
{
	{FORMER_FOREST, {0, 2}},
	{FORMER_CONDENSER, {0, 2}},
	{FORMER_ECH_MIRROR, {0, 1}},
	{FORMER_THERMAL_BORE, {1, 1}},
	{FORMER_AQUIFER, {1, 3}},
	{FORMER_RAISE_LAND, {0, 1}},
	{FORMER_SENSOR, {2, 2}},
};

struct FORMER_ORDER
{
	int vehicleId;
	std::set<MAP *> oneTurnTerraformingLocations{};
	MAP *tile = nullptr;
	int action = -1;
	
};

struct TerraformingRequestAssignment
{
	FORMER_ORDER *formerOrder;
	int travelTime;
};

struct TerraformingImprovement
{
	const MAP *tile;
	const TERRAFORMING_OPTION *option;
	int action = -1;
	int nutrient;
	int mineral;
	int energy;
	double score = 0.0;
};

struct TERRAFORMING_SCORE
{
	const TERRAFORMING_OPTION *option = nullptr;
	int action = -1;
	double score = 0.0;
};

struct TERRAFORMING_REQUEST
{
	MAP *tile = nullptr;
	const TERRAFORMING_OPTION *option;
	int action = -1;
	double score = 0.0;
};

struct ConventionalTerraformingRequest : TERRAFORMING_REQUEST
{
	MAP currentMapState;
	MAP improvedMapState;
	double yieldScore;
};

struct BaseTerraformingInfo
{
	std::vector<MAP *> conventionalTerraformingSites;
	int rockyLandTileCount;
	int unusedMineralTileCount;
	int currentTerraformingCount;
};

struct TileTerraformingInfo
{
	// base works this tile
	bool worked = false;
	// baseId that works this tile
	int workedBaseId = -1;
	// base can work this tile (except base tile)
	bool workable = false;
	// baseIds those can work this tile
	std::vector<int> workableBaseIds;
	// baseIds those are affected by area improvement at this tile (condenser, echelon mirror)
	std::vector<int> areaWorkableBaseIds;
	// workable tile where a conventional improvement can be placed to increase yield
	bool availableConventionalTerraformingSite = false;
	// generally terraformable tile
	bool availableTerraformingSite = false;
	
	bool harvested = false;
	bool terraformed = false;
	bool terraformedConventional = false;
	bool terraformedForest = false;
	bool terraformedCondenser = false;
	bool terraformedMirror = false;
	bool terraformedBorehole = false;
	bool terraformedAquifer = false;
	bool terraformedRaise = false;
	bool terraformedSensor = false;
	bool rockyMineAllowed = false;
	bool platformAllowed = false;
	bool boreholeAllowed = false;
	bool forestAllowed = false;
	bool fungusAllowed = false;
	
};

// terraforming data operations
void setupTerraformingData();
void cleanupTerraformingData();
// access terraforming data arrays
BaseTerraformingInfo &getBaseTerraformingInfo(int baseId);
TileTerraformingInfo &getTileTerraformingInfo(int mapIndex);
TileTerraformingInfo &getTileTerraformingInfo(int x, int y);
TileTerraformingInfo &getTileTerraformingInfo(MAP *tile);
// strategy
void moveFormerStrategy();
void populateTerraformingData();
void cancelRedundantOrders();
void generateTerraformingRequests();
void generateLandBridgeTerraformingRequests();
void selectConventionalTerraformingLocations();
void selectRockyMineLocation(int baseId);
void selectPlatformLocation(int baseId);
void selectBoreholeLocation(int baseId);
void selectForestLocation(int baseId);
void selectFungusLocation(int baseId);
void generateBaseConventionalTerraformingRequests(int baseId);
void generateAquiferTerraformingRequest(MAP *tile);
void generateRaiseLandTerraformingRequest(MAP *tile);
void generateNetworkTerraformingRequest(MAP *tile);
void generateSensorTerraformingRequest(MAP *tile);
bool compareTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2);
void sortTerraformingRequests();
void applyProximityRules();
void removeTerraformedTiles();
void assignFormerOrders();
void finalizeFormerOrders();
double computeImprovementSurplusEffectScore(MAP *tile, MAP *currentMapState, MAP *improvedMapState);
double computeImprovementYieldScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState);
ConventionalTerraformingRequest calculateConventionalTerraformingScore(int baseId, MAP *tile, const TERRAFORMING_OPTION *option, bool fitness);
void calculateAquiferTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateRaiseLandTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateNetworkTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore);
void calculateSensorTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore);
bool isTerraformingCompleted(MAP *tile, int action);
bool isVehicleTerrafomingOrderCompleted(int vehicleId);
bool isTerraformingAvailable(MAP *tile, int action);
bool isRemoveFungusRequired(int action);
bool isLevelTerrainRequired(bool ocean, int action);
bool isVehicleConvoying(VEH *vehicle);
bool isVehicleTerraforming(VEH *vehicle);
bool isNearbyForestUnderConstruction(int x, int y);
bool isNearbyCondeserUnderConstruction(int x, int y);
bool isNearbyMirrorUnderConstruction(int x, int y);
bool isNearbyBoreholePresentOrUnderConstruction(int x, int y);
bool isNearbyRiverPresentOrUnderConstruction(int x, int y);
bool isNearbyRaiseUnderConstruction(int x, int y);
bool isNearbySensorPresentOrUnderConstruction(int x, int y);
int getTerraformingTime(int action, int items, int rocks, VEH* vehicle);
int getDestructionAdjustedTerraformingTime(int action, int items, int rocks, VEH* vehicle);
//int getClosestAvailableFormerTravelTime(MAP *tile);
double calculateNetworkScore(MAP *tile, int action);
bool isTowardBaseDiagonal(int x, int y, int dxSign, int dySign);
bool isTowardBaseHorizontal(int x, int y, int dxSign);
bool isTowardBaseVertical(int x, int y, int dySign);
double calculateSensorScore(MAP *tile);
bool isBaseWorkedTile(BASE *base, int x, int y);
double calculateFitnessScore(MAP *tile, int action, bool levelTerrain);
bool hasNearbyTerraformingRequestAction(std::vector<TERRAFORMING_REQUEST>::iterator begin, std::vector<TERRAFORMING_REQUEST>::iterator end, int action, int x, int y, int range);
double estimateCondenserExtraYieldScore(MAP *tile, const std::vector<int> *actions);
double estimateAquiferExtraYieldScore(MAP *tile);
double estimateRaiseLandExtraYieldScore(MAP *tile, int cost);
bool isRaiseLandSafe(MAP *tile);
double calculateBaseResourceScore(int baseId, int currentMineralIntake, int currentNutrientSurplus, int currentMineralSurplus, int currentEnergySurplus, int improvedMineralIntake, int improvedNutrientSurplus, int improvedMineralSurplus, int improvedEnergySurplus);
double computeImprovementBaseSurplusEffectScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState);
double computeBaseImprovementYieldScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState);
bool isWorkableTile(MAP *tile);
bool isValidConventionalTerraformingSite(MAP *tile);
bool isValidTerraformingSite(MAP *tile);
double getYieldScore(double nutrient, double mineral, double energy);
double getNormalTerraformingIncomeGain();
double getNormalizedTerraformingIncomeGain(int operationTime, double score);
int getInferiorImprovement(TerraformingImprovement &improvement1, TerraformingImprovement &improvement2, bool mineralsOnly);
void generateTerraformingChange(MAP *mapState, int action);

