#pragma once

#include <float.h>
#include <map>
#include "main.h"
#include "game.h"
#include "move.h"
#include "wtp_game.h"
#include "wtp_mod.h"

struct TileYield
{
	int nutrient;
	int mineral;
	int energy;
	
	TileYield(int _nutrient, int _mineral, int _energy)
	: nutrient(_nutrient), mineral(_mineral), energy(_energy)
	{}
	
	TileYield()
	: TileYield(0, 0, 0)
	{}
	
	/*
	Verifies equal or superior yields.
	*/
	static bool isEqualOrSuperior(TileYield o1, TileYield o2)
	{
		return
			(o1.nutrient >= o2.nutrient && o1.mineral >= o2.mineral && o1.energy >= o2.energy)
		;
		
	}
	
	/*
	Verifies equal yields.
	*/
	static bool isEqual(TileYield o1, TileYield o2)
	{
		return
			(o1.nutrient == o2.nutrient && o1.mineral == o2.mineral && o1.energy == o2.energy)
		;
		
	}
	
	/*
	Verifies first is superior to second.
	*/
	static bool isSuperior(TileYield o1, TileYield o2)
	{
		return
			(o1.nutrient >= o2.nutrient && o1.mineral >= o2.mineral && o1.energy >= o2.energy)
			&&
			(o1.nutrient > o2.nutrient || o1.mineral > o2.mineral || o1.energy > o2.energy)
		;
		
	}
	
};

struct MapState
{
	int rockiness;
	uint32_t items;
};

struct FactionTerraformingInfo
{
	double averageNormalTerraformingRateMultiplier;
	double averagePlantFungusTerraformingRateMultiplier;
	double averageRemoveFungusTerraformingRateMultiplier;
	
	double bareLandScore;
	double bareMineScore;
	double bareSolarScore;

};

/*
Tile potentially can be terraformed.
*/
struct TileTerraformingInfo
{
	// original map state
	MapState mapState;
	
	// terraformable tile
	bool availableTerraformingSite = false;
	// terraformable tile for base yield
	bool availableBaseTerraformingSite = false;
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
	bool terraformedBunker = false;
	
};

struct BaseTerraformingInfo
{
	int projectedPopSize;
	std::vector<MAP *> terraformingSites;
	int unworkedLandRockyTileCount;
	int minimalNutrientYield;
	int minimalMineralYield;
	int minimalEnergyYield;
};

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
	// list of actions
	std::vector<int> actions;
};

// terraforming options

TERRAFORMING_OPTION const TO_ROCKY_MINE			{"rocky mine"		, false, true , false, true , FORMER_MINE			, {FORMER_ROAD, FORMER_MINE}};
TERRAFORMING_OPTION const TO_MINE				{"mine"				, false, false, false, true , FORMER_MINE			, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE}};
TERRAFORMING_OPTION const TO_SOLAR_COLLECTOR	{"solar collector"	, false, false, false, true , FORMER_SOLAR			, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR}};
TERRAFORMING_OPTION const TO_CONDENSER			{"condenser"		, false, false, true , true , FORMER_CONDENSER		, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_CONDENSER}};
TERRAFORMING_OPTION const TO_ECHELON_MIRROR		{"echelon mirror"	, false, false, true , true , FORMER_ECH_MIRROR		, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_ECH_MIRROR}};
TERRAFORMING_OPTION const TO_THERMAL_BOREHOLE	{"thermal borehole"	, false, false, false, true , FORMER_THERMAL_BORE	, {FORMER_ROAD, FORMER_THERMAL_BORE}};
TERRAFORMING_OPTION const TO_FOREST				{"forest"			, false, false, false, true , FORMER_FOREST			, {FORMER_ROAD, FORMER_FOREST}};
TERRAFORMING_OPTION const TO_LAND_FUNGUS		{"land fungus"		, false, false, false, true , FORMER_PLANT_FUNGUS	, {FORMER_ROAD, FORMER_PLANT_FUNGUS}};
TERRAFORMING_OPTION const TO_MINING_PLATFORM	{"mining platform"	, true , false, false, true , FORMER_MINE			, {FORMER_FARM, FORMER_MINE}};
TERRAFORMING_OPTION const TO_TIDAL_HARNESS		{"tidal harness"	, true , false, false, true , FORMER_SOLAR			, {FORMER_FARM, FORMER_SOLAR}};
TERRAFORMING_OPTION const TO_SEA_FUNGUS			{"sea fungus"		, true , false, false, true , FORMER_PLANT_FUNGUS	, {FORMER_PLANT_FUNGUS}};
TERRAFORMING_OPTION const TO_AQUIFER			{"aquifer"			, false, false, true , true , FORMER_AQUIFER		, {FORMER_AQUIFER}};
TERRAFORMING_OPTION const TO_RAISE_LAND			{"raise land"		, false, false, true , true , FORMER_RAISE_LAND		, {FORMER_RAISE_LAND}};
TERRAFORMING_OPTION const TO_NETWORK			{"road/tube"		, false, false, false, false, FORMER_ROAD			, {FORMER_ROAD, FORMER_MAGTUBE}};
TERRAFORMING_OPTION const TO_LAND_SENSOR		{"sensor (land)"	, false, false, false, false, FORMER_SENSOR			, {FORMER_SENSOR}};
TERRAFORMING_OPTION const TO_SEA_SENSOR			{"sensor (sea)"		, true , false, false, false, FORMER_SENSOR			, {FORMER_SENSOR}};
TERRAFORMING_OPTION const TO_LAND_BUNKER		{"bunker (land)"	, false, false, false, false, FORMER_BUNKER			, {FORMER_BUNKER}};

// conventional terraforming options

const std::array<const std::vector<TERRAFORMING_OPTION *>, 2> BASE_TERRAFORMING_OPTIONS
{{
	// land
	{
		(TERRAFORMING_OPTION *)&TO_ROCKY_MINE,
		(TERRAFORMING_OPTION *)&TO_MINE,
		(TERRAFORMING_OPTION *)&TO_SOLAR_COLLECTOR,
		(TERRAFORMING_OPTION *)&TO_CONDENSER,
		(TERRAFORMING_OPTION *)&TO_ECHELON_MIRROR,
		(TERRAFORMING_OPTION *)&TO_THERMAL_BOREHOLE,
		(TERRAFORMING_OPTION *)&TO_FOREST,
		(TERRAFORMING_OPTION *)&TO_LAND_FUNGUS,
	},
	// sea
	{
		(TERRAFORMING_OPTION *)&TO_MINING_PLATFORM,
		(TERRAFORMING_OPTION *)&TO_TIDAL_HARNESS,
		(TERRAFORMING_OPTION *)&TO_SEA_FUNGUS,
	},
}};

/// Prohibits building improvements too close to each other or existing improvements.
struct PROXIMITY_RULE
{
	// cannot build within this range of same existing improvement
	int existingDistance;
	// cannot build within this range of same building improvement
	int buildingDistance;
};
const robin_hood::unordered_flat_map<int, PROXIMITY_RULE> PROXIMITY_RULES =
{
	{FORMER_CONDENSER, {0, 1}},
	{FORMER_ECH_MIRROR, {0, 1}},
	{FORMER_THERMAL_BORE, {1, 1}},
	{FORMER_AQUIFER, {1, 3}},
	{FORMER_RAISE_LAND, {0, 1}},
	{FORMER_SENSOR, {2, 2}},
	{FORMER_BUNKER, {2, 2}},
};

struct FORMER_ORDER
{
	int vehicleId;
	MAP *tile = nullptr;
	int action = -1;
	
	FORMER_ORDER(int _vehicleId);
	
};

struct TerraformingRequestAssignment
{
	FORMER_ORDER *formerOrder;
	double travelTime;
};

struct TerraformingImprovement
{
	const MAP *tile;
	TERRAFORMING_OPTION const *option;
	int action = -1;
	int nutrient;
	int mineral;
	int energy;
	double score = 0.0;
};

struct TERRAFORMING_SCORE
{
	MAP *tile;
	TERRAFORMING_OPTION const *option;
	
	int action = -1;
	double score = 0.0;
	double terraformingTime = 0.0;
	
	TERRAFORMING_SCORE(MAP *_tile, TERRAFORMING_OPTION const *_option)
	: tile{_tile}, option{_option}
	{}
	
};

/// Final terraforming request assigned to formers.
struct TERRAFORMING_REQUEST
{
	MAP *tile;
	TERRAFORMING_OPTION const *option;
	
	// conventional terraforming (tile items modification resulting in yield change)
	bool conventional = false;
	// a target map state with all option improvements
	MapState improvedMapState;
	// first former action
	int firstAction = -1;
	// combined option actions terraforming time
	double terraformingTime = 0.0;
	// improvement generated income
	double improvementIncome = 0.0;
	// adjustment to how well this improvement fits in this tile by reducing its income
	double fitnessScore = 0.0;
	// fitness adjusted improvement generated income
	double income = 0.0;
	// improvement generated gain (with terraformingTime delay)
	double gain = 0.0;
	
	TileYield yield;
	
	TERRAFORMING_REQUEST(MAP *_tile, TERRAFORMING_OPTION const *_option)
	: tile{_tile}, option{_option}
	{}
	
//	TERRAFORMING_REQUEST(MAP *_tile, TERRAFORMING_OPTION const *_option, double _terraformingTime, int _firstAction, double _score)
//	: tile{_tile}, option{_option}, terraformingTime(_terraformingTime), firstAction{_firstAction}, score{_score}
//	{}
//	
};

// access terraforming data arrays
TileTerraformingInfo &getTileTerraformingInfo(MAP *tile);
BaseTerraformingInfo &getBaseTerraformingInfo(int baseId);

void moveFormerStrategy();
// terraforming data operations
void setupTerraformingData();
void populateTerraformingData();
void cancelRedundantOrders();
void generateTerraformingRequests();
void generateBaseConventionalTerraformingRequests(int baseId);
void generateLandBridgeTerraformingRequests();
void generateAquiferTerraformingRequest(MAP *tile);
void generateRaiseLandTerraformingRequest(MAP *tile);
void generateNetworkTerraformingRequest(MAP *tile);
void generateSensorTerraformingRequest(MAP *tile);
void generateBunkerTerraformingRequest(MAP *tile);
void sortTerraformingRequests();
void applyProximityRules();
void removeTerraformedTiles();
void assignFormerOrders();
void finalizeFormerOrders();
double computeBaseTileImprovementGain(int baseId, MAP *tile, MapState &improvedMapState, bool areaEffect);
double computeBaseImprovementYieldScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState);
TERRAFORMING_REQUEST calculateConventionalTerraformingScore(int baseId, MAP *tile, TERRAFORMING_OPTION const *option);
TERRAFORMING_REQUEST calculateAquiferTerraformingScore(MAP *tile);
TERRAFORMING_REQUEST calculateRaiseLandTerraformingScore(MAP *tile);
TERRAFORMING_REQUEST calculateNetworkTerraformingScore(MAP *tile);
TERRAFORMING_REQUEST calculateSensorTerraformingScore(MAP *tile);
TERRAFORMING_REQUEST calculateBunkerTerraformingScore(MAP *tile);
bool isTerraformingCompleted(MAP *tile, int action);
bool isVehicleTerrafomingOrderCompleted(int vehicleId);
bool isTerraformingAvailable(MAP *tile, int action);
bool isRemoveFungusRequired(int action);
bool isLevelTerrainRequired(bool ocean, int action);
bool isVehicleConvoying(int vehicleId);
bool isVehicleTerraforming(VEH *vehicle);
bool isNearbyForestUnderConstruction(int x, int y);
bool isNearbyCondeserUnderConstruction(int x, int y);
bool isNearbyMirrorUnderConstruction(int x, int y);
bool isNearbyBoreholePresentOrUnderConstruction(int x, int y);
bool isNearbyRiverPresentOrUnderConstruction(int x, int y);
bool isNearbyRaiseUnderConstruction(int x, int y);
bool isNearbySensorPresentOrUnderConstruction(int x, int y);
bool isNearbyBunkerPresentOrUnderConstruction(int x, int y);
bool isNearbyBasePresent(int x, int y, int range);
double getTerraformingTime(MapState &mapState, int action);
double calculateNetworkScore(MAP *tile, int action);
bool isTowardBaseDiagonal(int x, int y, int dxSign, int dySign);
bool isTowardBaseHorizontal(int x, int y, int dxSign);
bool isTowardBaseVertical(int x, int y, int dySign);
double estimateSensorIncome(MAP *tile);
double estimateBunkerIncome(MAP *tile, bool existing = false);
bool isBaseWorkedTile(BASE *base, int x, int y);
double getFitnessScore(MAP *tile, int action, bool levelTerrain);
bool hasNearbyTerraformingRequestAction(std::vector<TERRAFORMING_REQUEST>::iterator begin, std::vector<TERRAFORMING_REQUEST>::iterator end, int action, int x, int y, int range);
double estimateCondenserExtraYieldScore(MAP *tile, const std::vector<int> *actions);
double estimateAquiferIncome(MAP *tile);
double estimateRaiseLandIncome(MAP *tile, int cost);
bool isRaiseLandSafe(MAP *tile);
double calculateBaseResourceScore(int baseId, int currentMineralIntake2, int currentNutrientSurplus, int currentMineralSurplus, int currentEnergySurplus, int improvedNutrientSurplus, int improvedMineralSurplus, int improvedEnergySurplus);
double computeImprovementBaseSurplusEffectScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState, bool areaEffect);
double computeBaseImprovementYieldScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState);
bool isWorkableTile(MAP *tile);
bool isValidConventionalTerraformingSite(MAP *tile);
bool isValidTerraformingSite(MAP *tile);
double getTerraformingResourceScore(double nutrient, double mineral, double energy);
double getTerraformingResourceScore(TileYield yield);
void generateTerraformingChange(MapState &mapState, int action);
double getTerraformingGain(double income, double terraformingTime);
bool compareTerraformingRequests(TERRAFORMING_REQUEST const &terraformingRequest1, TERRAFORMING_REQUEST const &terraformingRequest2);
bool compareConventionalTerraformingRequests(TERRAFORMING_REQUEST const &terraformingRequest1, TERRAFORMING_REQUEST const &terraformingRequest2);
void setMapState(MapState &mapState, MAP *tile);
void applyMapState(MapState &mapState, MAP *tile);
void restoreTileMapState(MAP *tile);
TileYield getTerraformingYield(int baseId, MAP *tile, std::vector<int> actions);
double getBaseImprovementIncome(int baseId, Resource oldIntake, Resource newIntake);
void addConventionalTerraformingRequest(std::vector<TERRAFORMING_REQUEST> &availableTerraformingRequests, TERRAFORMING_REQUEST &terraformingRequest);
void removeUnusedBunkers();

