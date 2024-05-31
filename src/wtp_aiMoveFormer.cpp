#include <float.h>
#include <math.h>
#include <vector>
#include <set>
#include <map>
#include <map>
#include "game.h"
#include "wtp_terranx.h"
#include "wtp_mod.h"
#include "wtp_ai.h"
#include "wtp_aiData.h"
#include "wtp_aiMove.h"
#include "wtp_aiMoveFormer.h"
#include "wtp_aiProduction.h"
#include "wtp_aiRoute.h"

// terraforming options

TERRAFORMING_OPTION TO_ROCKY_MINE =
	{"rocky mine", false, true, false, true, FORMER_MINE, true, {FORMER_ROAD, FORMER_MINE}};
TERRAFORMING_OPTION TO_MINE =
	{"mine", false, false, false, true, FORMER_MINE, true, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE}};
TERRAFORMING_OPTION TO_SOLAR_COLLECTOR =
	{"solar collector", false, false, false, true, FORMER_SOLAR, true, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR}};
TERRAFORMING_OPTION TO_CONDENSER =
	{"condenser", false, false, true, true, FORMER_CONDENSER, false, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_CONDENSER}};
TERRAFORMING_OPTION TO_ECHELON_MIRROR =
	{"echelon mirror", false, false, true, true, FORMER_ECH_MIRROR, false, {FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_ECH_MIRROR}};
TERRAFORMING_OPTION TO_THERMAL_BOREHOLE =
	{"thermal borehole", false, false, false, true, FORMER_THERMAL_BORE, true, {FORMER_ROAD, FORMER_THERMAL_BORE}};
TERRAFORMING_OPTION TO_FOREST =
	{"forest", false, false, false, true, FORMER_FOREST, true, {FORMER_ROAD, FORMER_FOREST}};
TERRAFORMING_OPTION TO_LAND_FUNGUS =
	{"land fungus", false, false, false, true, FORMER_PLANT_FUNGUS, true, {FORMER_ROAD, FORMER_PLANT_FUNGUS}};
TERRAFORMING_OPTION TO_MINING_PLATFORM =
	{"mining platform", true, false, false, true, FORMER_MINE, true, {FORMER_FARM, FORMER_MINE}};
TERRAFORMING_OPTION TO_TIDAL_HARNESS =
	{"tidal harness", true, false, false, true, FORMER_SOLAR, true, {FORMER_FARM, FORMER_SOLAR}};
TERRAFORMING_OPTION TO_SEA_FUNGUS =
	{"sea fungus", true, false, false, true, FORMER_PLANT_FUNGUS, true, {FORMER_PLANT_FUNGUS}};
// aquifer
TERRAFORMING_OPTION AQUIFER_TERRAFORMING_OPTION =
	{"aquifer"   , false, false, true , true , FORMER_AQUIFER     , false, {FORMER_AQUIFER}};
// raise land
TERRAFORMING_OPTION RAISE_LAND_TERRAFORMING_OPTION =
	{"raise"     , false, false, true , true , FORMER_RAISE_LAND  , false, {FORMER_RAISE_LAND}};
// network
TERRAFORMING_OPTION NETWORK_TERRAFORMING_OPTION =
	{"road/tube" , false, false, false, false, FORMER_ROAD        , false, {FORMER_ROAD, FORMER_MAGTUBE}};
// sensor
TERRAFORMING_OPTION LAND_SENSOR_TERRAFORMING_OPTION =
	{"sensor"    , false, false, false, false, FORMER_SENSOR      , false, {FORMER_SENSOR}};
TERRAFORMING_OPTION OCEAN_SENSOR_TERRAFORMING_OPTION =
	{"sensor"    , true , false, false, false, FORMER_SENSOR      , false, {FORMER_SENSOR}};

const std::vector<TERRAFORMING_OPTION *> CONVENTIONAL_TERRAFORMING_OPTIONS =
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

// FORMER_ORDER

FORMER_ORDER::FORMER_ORDER(int _vehicleId)
: vehicleId{_vehicleId}
{

}

// TERRAFORMING_REQUEST comparison

TERRAFORMING_REQUEST::TERRAFORMING_REQUEST(MAP *_tile, TERRAFORMING_OPTION *_option, int _action, double _score)
: tile{_tile}, option{_option}, action{_action}, score{_score}
{

}

TERRAFORMING_REQUEST::TERRAFORMING_REQUEST()
: TERRAFORMING_REQUEST(nullptr, nullptr, -1, 0.0)
{

}

bool compareTerraformingRequestDescending(const TERRAFORMING_REQUEST &a, const TERRAFORMING_REQUEST &b)
{
	return a.score > b.score;
}

// ConventionalTerraformingRequest

ConventionalTerraformingRequest::ConventionalTerraformingRequest()
{

}

bool compareConventionalTerraformingRequestDescending(const ConventionalTerraformingRequest &a, const ConventionalTerraformingRequest &b)
{
	return a.yieldScore > b.yieldScore;
}

// normal terraforming income gain

double normalTerraformingIncomeGain = 1.0;

// terraforming data

std::vector<BaseTerraformingInfo> baseTerraformingInfos;
std::vector<TileTerraformingInfo> tileTerraformingInfos;

std::vector<MAP *> terraformingSites;
std::vector<MAP *> conventionalTerraformingSites;

double networkDemand = 0.0;
std::vector<FORMER_ORDER> formerOrders;
robin_hood::unordered_flat_map<int, FORMER_ORDER *> vehicleFormerOrders;
std::vector<TERRAFORMING_REQUEST> terraformingRequests;

// terraforming data operations

void setupTerraformingData()
{
	// set normal terraforming gain

	normalTerraformingIncomeGain = getNormalTerraformingIncomeGain();
	if (normalTerraformingIncomeGain <= 0.0)
	{
		normalTerraformingIncomeGain = 1.0;
	}

	// setup data

	baseTerraformingInfos.resize(MaxBaseNum, {});
	tileTerraformingInfos.resize(*MapAreaTiles, {});

}

void cleanupTerraformingData()
{
	// cleanup data

	baseTerraformingInfos.clear();
	tileTerraformingInfos.clear();

	// clear lists

	terraformingSites.clear();
	conventionalTerraformingSites.clear();

	terraformingRequests.clear();
	formerOrders.clear();
	vehicleFormerOrders.clear();

}

// access terraforming data arrays

BaseTerraformingInfo &getBaseTerraformingInfo(int baseId)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	return baseTerraformingInfos.at(baseId);
}

TileTerraformingInfo &getTileTerraformingInfo(int mapIndex)
{
	assert(mapIndex >= 0 && mapIndex < *MapAreaTiles);
	return tileTerraformingInfos.at(mapIndex);
}
TileTerraformingInfo &getTileTerraformingInfo(int x, int y)
{
	return getTileTerraformingInfo(getMapIndexByCoordinates(x, y));
}
TileTerraformingInfo &getTileTerraformingInfo(MAP *tile)
{
	return getTileTerraformingInfo(getMapIndexByPointer(tile));
}

/*
Prepares former orders.
*/
void moveFormerStrategy()
{
	// initialize data

	setupTerraformingData();

	// populate data

	populateTerraformingData();

	// formers

	cancelRedundantOrders();
	generateTerraformingRequests();
	sortTerraformingRequests();
	applyProximityRules();
	removeTerraformedTiles();
	assignFormerOrders();
	finalizeFormerOrders();

	// clean up data

	cleanupTerraformingData();

}

/*
Populate global lists before processing faction strategy.
*/
void populateTerraformingData()
{
	debug("populateTerraformingData - %s\n", getMFaction(aiFactionId)->noun_faction);

	// populate available terraforming associations

	robin_hood::unordered_flat_set<int> availableTerraformingAssociations;

	bool hasGravship = has_tech(aiFactionId, getChassis(CHS_GRAVSHIP)->preq_tech);
	bool hasLandFormer = has_tech(aiFactionId, getUnit(BSC_FORMERS)->preq_tech);
	bool hasSeaFormer = has_tech(aiFactionId, getUnit(BSC_SEA_FORMERS)->preq_tech);

	if (hasGravship)
	{
		// everything is accessible with gravship

		for (int association : aiData.factionGeographys[aiFactionId].associations[0])
		{
			availableTerraformingAssociations.insert(association);
		}

		for (int association : aiData.factionGeographys[aiFactionId].associations[1])
		{
			availableTerraformingAssociations.insert(association);
		}

	}
	else
	{
		bool landFormerExists = false;

		// direct access from existing formers

		for (int vehicleId : aiData.formerVehicleIds)
		{
			VEH *vehicle = getVehicle(vehicleId);
			int triad = vehicle->triad();
			int vehicleAssociation = getVehicleAssociation(vehicleId);

			if (triad == TRIAD_SEA)
			{
				availableTerraformingAssociations.insert(vehicleAssociation);
			}
			else if (triad == TRIAD_LAND)
			{
				availableTerraformingAssociations.insert(vehicleAssociation);
				landFormerExists = true;
			}

		}

		// carrying land formers to other continents

		if (hasLandFormer || landFormerExists)
		{
			for (int association : aiData.factionGeographys[aiFactionId].associations[0])
			{
				// potentially reachable

				if (!isPotentiallyReachableAssociation(association, aiFactionId))
					continue;

				// add to available associations

				availableTerraformingAssociations.insert(association);

			}

		}

		// build sea formers in other oceans

		if (hasSeaFormer)
		{
			for (int baseId : aiData.baseIds)
			{
				int baseOceanAssociation = getBaseOceanAssociation(baseId);

				// port

				if (baseOceanAssociation == -1)
					continue;

				// add to available associations

				availableTerraformingAssociations.insert(baseOceanAssociation);

			}

		}

	}

	// populate tileTerraformingInfos
	// populate terraformingSites
	// populate conventionalTerraformingSites

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);

		// initialize base terraforming info

		baseTerraformingInfo.rockyLandTileCount = 0;
		baseTerraformingInfo.unusedMineralTileCount = 0;
		baseTerraformingInfo.currentTerraformingCount = 0;

		// base radius tiles

		for (MAP *tile : getBaseRadiusTiles(base->x, base->y, false))
		{
			TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);

			// exclude not valid terraforming site

			if (!isValidTerraformingSite(tile))
				continue;

			tileTerraformingInfo.availableTerraformingSite = true;

			// worked

			if (isBaseWorkedTile(baseId, tile))
			{
				tileTerraformingInfo.worked = true;
				tileTerraformingInfo.workedBaseId = baseId;
			}

			// workable

			if (isWorkableTile(tile))
			{
				tileTerraformingInfo.workable = true;
				tileTerraformingInfo.workableBaseIds.push_back(baseId);
			}

			// conventional terraforming sites

			if (isValidConventionalTerraformingSite(tile))
			{
				tileTerraformingInfo.availableConventionalTerraformingSite = true;
			}

			// update base rocky land tile count

			if (!is_ocean(tile))
			{
				if (map_rockiness(tile) == 2)
				{
					baseTerraformingInfo.rockyLandTileCount++;

					if (!map_has_item(tile, BIT_MINE) || tileTerraformingInfo.workedBaseId == -1)
					{
						// potential source of minerals
						baseTerraformingInfo.unusedMineralTileCount++;
					}

				}
				else
				{
					if (map_has_item(tile, BIT_FOREST) && tileTerraformingInfo.workedBaseId == -1)
					{
						// potential source of minerals
						baseTerraformingInfo.unusedMineralTileCount++;
					}
					else
					{
						int rainfall = map_rainfall(tile);
						int rockiness = map_rockiness(tile);
						int elevation = map_elevation(tile);

						int tileQuality = rainfall + rockiness + elevation;

						if (tileQuality < 2)
						{
							// potential source of minerals
							baseTerraformingInfo.unusedMineralTileCount++;
						}

					}

				}

			}

		}

		// base radius adjacent tiles

		for (int index = OFFSET_COUNT_RADIUS; index < OFFSET_COUNT_RADIUS_SIDE; index++)
		{
			int x = wrap(base->x + BASE_TILE_OFFSETS[index][0]);
			int y = base->y + BASE_TILE_OFFSETS[index][1];

			if (!isOnMap(x, y))
				continue;

			MAP *tile = getMapTile(x, y);
			TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(x, y);

			// valid terraforming site

			if (!isValidTerraformingSite(tile))
				continue;

			tileTerraformingInfo.availableTerraformingSite = true;

			// update area workable bases

			tileTerraformingInfo.areaWorkableBaseIds.push_back(baseId);

		}

	}

	if (DEBUG)
	{
		debug("\tavailableTerraformingSites\n");
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int x = getX(tile);
			int y = getY(tile);
			TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(x, y);

			if (!tileTerraformingInfo.availableTerraformingSite)
				continue;

			debug("\t\t(%3d,%3d)\n", x, y);

		}

	}

	// populated terraformed tiles and former orders

	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &Vehicles[vehicleId];
		int triad = vehicle->triad();
		int defenseValue = getVehicleDefenseValue(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		TileTerraformingInfo &vehicleTileTerraformingInfo = getTileTerraformingInfo(vehicle->x, vehicle->y);

		// process supplies and formers

		// supplies
		if (isSupplyVehicle(vehicle))
		{
			// convoying vehicles
			if (isVehicleConvoying(vehicle))
			{
				vehicleTileTerraformingInfo.harvested = true;
			}

		}
		// formers
		else if (isFormerVehicle(vehicle))
		{
			// terraforming vehicles

			if (isVehicleTerraforming(vehicle))
			{
				// set sync task

				setTask(Task(vehicleId, TT_NONE));

				// terraformed flag

				vehicleTileTerraformingInfo.terraformed = true;

				// conventional terraformed flag

				if
				(
					vehicle->order == ORDER_FARM
					||
					vehicle->order == ORDER_SOIL_ENRICHER
					||
					vehicle->order == ORDER_MINE
					||
					vehicle->order == ORDER_SOLAR_COLLECTOR
					||
					vehicle->order == ORDER_CONDENSER
					||
					vehicle->order == ORDER_ECHELON_MIRROR
					||
					vehicle->order == ORDER_THERMAL_BOREHOLE
					||
					vehicle->order == ORDER_PLANT_FOREST
					||
					vehicle->order == ORDER_PLANT_FUNGUS
				)
				{
					vehicleTileTerraformingInfo.terraformedConventional = true;
				}

				// populate terraformed forest tiles

				if (vehicle->order == ORDER_PLANT_FOREST)
				{
					vehicleTileTerraformingInfo.terraformedForest = true;
				}

				// populate terraformed condenser tiles

				if (vehicle->order == ORDER_CONDENSER)
				{
					vehicleTileTerraformingInfo.terraformedCondenser = true;
				}

				// populate terraformed mirror tiles

				if (vehicle->order == ORDER_ECHELON_MIRROR)
				{
					vehicleTileTerraformingInfo.terraformedMirror = true;
				}

				// populate terraformed borehole tiles

				if (vehicle->order == ORDER_THERMAL_BOREHOLE)
				{
					vehicleTileTerraformingInfo.terraformedBorehole = true;
				}

				// populate terraformed aquifer tiles

				if (vehicle->order == ORDER_DRILL_AQUIFER)
				{
					vehicleTileTerraformingInfo.terraformedAquifer = true;
				}

				// populate terraformed raise tiles

				if (vehicle->order == ORDER_TERRAFORM_UP)
				{
					vehicleTileTerraformingInfo.terraformedRaise = true;
				}

				// populate terraformed sensor tiles

				if (vehicle->order == ORDER_SENSOR_ARRAY)
				{
					vehicleTileTerraformingInfo.terraformedSensor = true;
				}

				// accumulate current terraformings around bases

				for (int baseId : vehicleTileTerraformingInfo.workableBaseIds)
				{
					getBaseTerraformingInfo(baseId).currentTerraformingCount++;
				}

			}
			// available vehicles
			else
			{
				// ignore those in terraformingWarzone except land vehicle in ocean

				if (!(triad == TRIAD_LAND && is_ocean(vehicleTile)))
				{
					if (defenseValue > 0 && aiData.getTileInfo(vehicle->x, vehicle->y).warzoneConventionalOffenseValue > defenseValue)
						continue;
				}

				// exclude air terraformers - let AI deal with them itself

				if (veh_triad(vehicleId) == TRIAD_AIR)
					continue;

				// store vehicle current id in pad_0 field

				vehicle->pad_0 = vehicleId;

				// add vehicle

				formerOrders.push_back({vehicleId});

			}

		}

	}

	// populate oneTurnTerraformingLocations

	executionProfiles["1.3.3.A. oneTurnTerraformingLocations"].start();

	for (FORMER_ORDER &formerOrder : formerOrders)
	{
		formerOrder.oneTurnTerraformingLocations = getOneTurnActionLocations(formerOrder.vehicleId);
	}

	executionProfiles["1.3.3.A. oneTurnTerraformingLocations"].stop();

	if (DEBUG)
	{
		debug("oneTurnTerraformingLocations\n");

		for (FORMER_ORDER &formerOrder : formerOrders)
		{
			debug("\t[%4d] (%3d,%3d)\n", formerOrder.vehicleId, getVehicle(formerOrder.vehicleId)->x, getVehicle(formerOrder.vehicleId)->y);

			for (MAP *location : formerOrder.oneTurnTerraformingLocations)
			{
				debug("\t\t-> (%3d,%3d)\n", getX(location), getY(location));
			}

		}

	}

	// calculate network demand

	int networkType = (has_tech(aiFactionId, Terraform[FORMER_MAGTUBE].preq_tech) ? BIT_MAGTUBE : BIT_ROAD);

	double networkCoverage = 0.0;

	for (int i = 0; i < *MapAreaTiles; i++)
	{
		// build tile

		MAP *tile = &((*MapTiles)[i]);

		// exclude ocean

		if (is_ocean(tile))
			continue;

		// exclude not own territory

		if (tile->owner != aiFactionId)
			continue;

		// count network coverage

		if (map_has_item(tile, networkType))
		{
			networkCoverage += 1.0;
		}

	}

	double networkThreshold = conf.ai_terraforming_networkCoverageThreshold * (double)aiData.baseIds.size();

	if (networkCoverage < networkThreshold)
	{
		networkDemand = (networkThreshold - networkCoverage) / networkThreshold;
	}

	// populate terraforming site lists

	for (int mapIndex = 0; mapIndex < *MapAreaTiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);

		if (tileTerraformingInfo.availableTerraformingSite)
		{
			terraformingSites.push_back(tile);
		}

		if (tileTerraformingInfo.availableConventionalTerraformingSite)
		{
			conventionalTerraformingSites.push_back(tile);
		}

	}

	if (DEBUG)
	{
		debug("\tterraformingSites\n");
		for (MAP *tile : terraformingSites)
		{
			debug("\t\t(%3d,%3d) ocean=%d\n", getX(tile), getY(tile), is_ocean(tile));
		}

	}

	if (DEBUG)
	{
		debug("\tconventionalTerraformingSites\n");
		for (MAP *tile : conventionalTerraformingSites)
		{
			debug("\t\t(%3d,%3d) ocean=%d\n", getX(tile), getY(tile), is_ocean(tile));
		}

	}

	// populate base conventionalTerraformingSites

	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);

		for (MAP *baseRadiusTile : getBaseRadiusTiles(baseTile, false))
		{
			TileTerraformingInfo &baseRadiusTileTerraformingInfo = getTileTerraformingInfo(baseRadiusTile);

			// conventional terraforming site

			if (!baseRadiusTileTerraformingInfo.availableConventionalTerraformingSite)
				continue;

			// add to base tiles

			baseTerraformingInfo.conventionalTerraformingSites.push_back(baseRadiusTile);

		}

	}

	// exclude terraformed sites not in available associations

	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int association = getAssociation(tile, aiFactionId);
		TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);

		// skip available

		if (availableTerraformingAssociations.count(association) != 0)
			continue;

		// exclude unavailable

		tileTerraformingInfo.availableTerraformingSite = false;
		tileTerraformingInfo.availableConventionalTerraformingSite = false;

	}

}

/*
Checks and removes redundant orders.
*/
void cancelRedundantOrders()
{
	for (int id : aiData.formerVehicleIds)
	{
		if (isVehicleTerrafomingOrderCompleted(id))
		{
			setVehicleOrder(id, ORDER_NONE);
		}

	}

}

void generateTerraformingRequests()
{
	// conventional requests

	debug("CONVENTIONAL TERRAFORMING_REQUESTS\n");

	for (int baseId : aiData.baseIds)
	{
		generateBaseConventionalTerraformingRequests(baseId);
	}

	// remove duplicate terraforming requests

	robin_hood::unordered_flat_set<MAP *> terraformingRequestLocations;

	for
	(
		std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestIterator = terraformingRequests.begin();
		terraformingRequestIterator != terraformingRequests.end();
	)
	{
		TERRAFORMING_REQUEST &terraformingRequest = *terraformingRequestIterator;

		if (terraformingRequestLocations.count(terraformingRequest.tile) != 0)
		{
			terraformingRequestIterator = terraformingRequests.erase(terraformingRequestIterator);
		}
		else
		{
			terraformingRequestLocations.insert(terraformingRequest.tile);
			terraformingRequestIterator++;
		}

	}

	// aquifer

	debug("AQUIFER TERRAFORMING_REQUESTS\n");

	for (MAP *tile : terraformingSites)
	{
		// generate request

		generateAquiferTerraformingRequest(tile);

	}

	// raise workable tiles to increase energy output

	debug("RAISE LAND TERRAFORMING_REQUESTS\n");

	for (MAP *tile : terraformingSites)
	{
		// generate request

		generateRaiseLandTerraformingRequest(tile);

	}

	// network

	debug("NETWORK TERRAFORMING_REQUESTS\n");

	for (MAP *tile : terraformingSites)
	{
		// land

		if (is_ocean(tile))
			continue;

		// generate request

		generateNetworkTerraformingRequest(tile);

	}

	// sensors

	debug("SENSOR TERRAFORMING_REQUESTS\n");

	for (MAP *tile : terraformingSites)
	{
		// generate request

		generateSensorTerraformingRequest(tile);

	}

	// land bridges

	if (*CurrentTurn > 20)
	{
		debug("LAND BRIDGE TERRAFORMING_REQUESTS\n");
		generateLandBridgeTerraformingRequests();
	}

}

void generateLandBridgeTerraformingRequests()
{
	debug("generateLandBridgeTerraformingRequests\n");

	Geography &factionGeography = aiData.factionGeographys[aiFactionId];

	robin_hood::unordered_flat_map<int, MapValue> bridgeRequests;

	for (MAP *tile : factionGeography.raiseableCoasts)
	{
		int bridgeAssociation = -1;
		int bridgeRange = -1;

		for (MAP *rangeTile : getRangeTiles(tile, 10, false))
		{
			bool rangeTileOcean = is_ocean(rangeTile);
			int rangeTileLandAssociation = getLandAssociation(rangeTile, aiFactionId);
			TileInfo &tileInfo = aiData.getTileInfo(rangeTile);
			TileFactionInfo &tileFactionInfo = tileInfo.factionInfos[aiFactionId];

			// land

			if (rangeTileOcean)
				continue;

			// different association

			if (isSameLandAssociation(tile, rangeTile, aiFactionId))
				continue;

			// not the populated non-combat cluster

			if (tileFactionInfo.clusterIds[0][0] >= 0)
				continue;

			// lock the minRange

			bridgeAssociation = rangeTileLandAssociation;
			bridgeRange = getRange(tile, rangeTile);
			break;

		}

		if (bridgeAssociation == -1)
			continue;

		if (bridgeRequests.count(bridgeAssociation) == 0)
		{
			MapValue mapValue(nullptr, INT_MAX);
			bridgeRequests.insert({bridgeAssociation, mapValue});
		}

		if (bridgeRange < bridgeRequests.at(bridgeAssociation).value)
		{
			bridgeRequests.at(bridgeAssociation).tile = tile;
			bridgeRequests.at(bridgeAssociation).value = bridgeRange;
		}

	}

	for (robin_hood::pair<int, MapValue> &bridgeRequestEntry : bridgeRequests)
	{
		MapValue bridgeRequest = bridgeRequestEntry.second;

		// score

		double score =
			conf.ai_terraforming_landBridgeValue
			* getExponentialCoefficient(conf.ai_terraforming_landBridgeRangeScale, bridgeRequest.value - 2)
		;

		debug
		(
			"\t(%3d,%3d)"
			" score=%5.2f"
			" conf.ai_terraforming_landBridgeValue=%5.2f"
			" conf.ai_terraforming_landBridgeRangeScale=%5.2f"
			" range=%2d"
			"\n"
			, getX(bridgeRequest.tile), getY(bridgeRequest.tile)
			, score
			, conf.ai_terraforming_landBridgeValue
			, conf.ai_terraforming_landBridgeRangeScale
			, bridgeRequest.value
		);

		terraformingRequests.emplace_back(bridgeRequest.tile, &RAISE_LAND_TERRAFORMING_OPTION, FORMER_RAISE_LAND, score);

	}

}

void selectConventionalTerraformingLocations()
{
	for (int baseId : aiData.baseIds)
	{
		selectRockyMineLocation(baseId);
		selectPlatformLocation(baseId);
		selectBoreholeLocation(baseId);
		selectForestLocation(baseId);
		selectFungusLocation(baseId);
	}

}

void selectRockyMineLocation(int baseId)
{
	BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);

	MAP *bestTile = nullptr;
	double bestScore;

	for (MAP *tile : baseTerraformingInfo.conventionalTerraformingSites)
	{
		// land rocky tile

		if (is_ocean(tile) || map_rockiness(tile) < 2)
			continue;

		// terraforming available

		if (!isTerraformingAvailable(tile, FORMER_MINE))
			continue;

		// calculate fitness score

		double score = calculateFitnessScore(tile, FORMER_MINE, false);

		if (bestTile == nullptr || score > bestScore)
		{
			bestTile = tile;
			bestScore = score;
		}

	}

	// not found

	if (bestTile == nullptr)
		return;

	getTileTerraformingInfo(bestTile).rockyMineAllowed = true;

}

void selectPlatformLocation(int baseId)
{
	BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);

	bool undevelopedLandRockyTile = false;

	for (MAP *tile : baseTerraformingInfo.conventionalTerraformingSites)
	{
		// exclude not land rocky tile

		if (is_ocean(tile) || map_rockiness(tile) < 2)
			continue;

		// exclude improved

		if (map_has_item(tile, BIT_MINE | BIT_SOLAR | BIT_CONDENSER | BIT_ECH_MIRROR | BIT_THERMAL_BORE))
			continue;

		// set undevelopedLandRockyTile

		undevelopedLandRockyTile = true;
		break;

	}

	// set platform for all sea tiles

	for (MAP *tile : baseTerraformingInfo.conventionalTerraformingSites)
	{
		// exclude not sea tiles

		if (!is_ocean(tile))
			continue;

		// set platform allowed

		getTileTerraformingInfo(tile).platformAllowed = !undevelopedLandRockyTile;

	}

}

void selectBoreholeLocation(int baseId)
{
	BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);

	MAP *bestTile = nullptr;
	double bestScore;

	for (MAP *tile : baseTerraformingInfo.conventionalTerraformingSites)
	{
		// land tile

		if (is_ocean(tile))
			continue;

		// terraforming available

		if (!isTerraformingAvailable(tile, FORMER_THERMAL_BORE))
			continue;

		// calculate fitness score

		double score = calculateFitnessScore(tile, FORMER_THERMAL_BORE, (map_rockiness(tile) == 2 ? true : false));

		if (bestTile == nullptr || score > bestScore)
		{
			bestTile = tile;
			bestScore = score;
		}

	}

	// not found

	if (bestTile == nullptr)
		return;

	getTileTerraformingInfo(bestTile).boreholeAllowed = true;

}

void selectForestLocation(int baseId)
{
	BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);

	MAP *bestTile = nullptr;
	double bestScore;

	for (MAP *tile : baseTerraformingInfo.conventionalTerraformingSites)
	{
		// land tile

		if (is_ocean(tile))
			continue;

		// terraforming available

		if (!isTerraformingAvailable(tile, FORMER_FOREST))
			continue;

		// calculate fitness score

		double score = calculateFitnessScore(tile, FORMER_FOREST, (map_rockiness(tile) == 2 ? true : false));

		if (bestTile == nullptr || score > bestScore)
		{
			bestTile = tile;
			bestScore = score;
		}

	}

	// not found

	if (bestTile == nullptr)
		return;

	getTileTerraformingInfo(bestTile).forestAllowed = true;
	debug
	(
		"selectForestLocation - %s: (%3d,%3d)\n"
		, getBase(baseId)->name, getX(bestTile), getY(bestTile)
	);

}

void selectFungusLocation(int baseId)
{
	BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);

	MAP *bestTile = nullptr;
	double bestScore;

	for (MAP *tile : baseTerraformingInfo.conventionalTerraformingSites)
	{
		// land tile

		if (is_ocean(tile))
			continue;

		// terraforming available

		if (!isTerraformingAvailable(tile, FORMER_PLANT_FUNGUS))
			continue;

		// calculate fitness score

		double score = calculateFitnessScore(tile, FORMER_PLANT_FUNGUS, (map_rockiness(tile) == 2 ? true : false));

		if (bestTile == nullptr || score > bestScore)
		{
			bestTile = tile;
			bestScore = score;
		}

	}

	// not found

	if (bestTile == nullptr)
		return;

	getTileTerraformingInfo(bestTile).fungusAllowed = true;

}

/*
Generate base conventional terraforming requests.

1. Evaluate terraforming score for all accessible base tiles (not worked by other bases).
2. Pick best option for each tile.
3. Sort them in descending order.
4. Apply them one by one until they are no longer wanted by the base.
5. Restore map state.
6. Add improvements for not yet terraformed tiles to the gloat terraforming requests.
*/
void generateBaseConventionalTerraformingRequests(int baseId)
{
	BASE *base = getBase(baseId);
	MAP *baseTile = getBaseMapTile(baseId);

	debug("generateBaseConventionalTerraformingRequests - %s\n", base->name);

	// current parameters

	int currentSize = base->pop_size;

	// project 10 turns size

	int projectedSize = getBaseProjectedSize(baseId, 10);

	// set base

	base->pop_size = projectedSize;
	computeBase(baseId, true);

	// generate terraforming requests

	std::vector<ConventionalTerraformingRequest> conventionalTerraformingRequests;

	for (MAP *tile : getBaseRadiusTiles(baseTile, false))
	{
		bool ocean = is_ocean(tile);
		TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);

		// available for conventional terraforming

		if (!tileTerraformingInfo.availableConventionalTerraformingSite)
			continue;

		// exclude worked by other base

		if (tileTerraformingInfo.workedBaseId != -1 && tileTerraformingInfo.workedBaseId != baseId)
			continue;

		// select best option scores

		ConventionalTerraformingRequest bestRequest;

		for (TERRAFORMING_OPTION *option : CONVENTIONAL_TERRAFORMING_OPTIONS)
		{
			if (option->ocean != ocean)
				continue;

			ConventionalTerraformingRequest request = calculateConventionalTerraformingScore(baseId, tile, option, true);

			if (request.action == -1)
				continue;

			// update the best

			if (request.score > bestRequest.score)
			{
				bestRequest = request;
			}

		}

		// not found

		if (bestRequest.tile == nullptr)
			continue;

		// store best request

		conventionalTerraformingRequests.push_back(bestRequest);

		debug("\t(%3d,%3d) %-15s %5.2f\n", getX(bestRequest.tile), getY(bestRequest.tile), bestRequest.option->name, bestRequest.score);

	}

	// sort conventional terraforming requests by score

	std::sort(conventionalTerraformingRequests.begin(), conventionalTerraformingRequests.end(), compareTerraformingRequestDescending);
//	std::sort(conventionalTerraformingRequests.begin(), conventionalTerraformingRequests.end(), compareConventionalTerraformingRequestDescending);

	debug("\tconventionalTerraformingRequests sorted\n");
	for (ConventionalTerraformingRequest &request : conventionalTerraformingRequests)
	{
		debug("\t\t(%3d,%3d) score=%5.2f yieldScore=%5.2f\n", getX(request.tile), getY(request.tile), request.score, request.yieldScore);
	}

	// add conventional terraforming requests to all requests

	terraformingRequests.insert(terraformingRequests.end(), conventionalTerraformingRequests.begin(), conventionalTerraformingRequests.end());

	// evaluate base terraforming requirement

	robin_hood::unordered_flat_set<MAP *> selectedTiles;
	std::vector<ConventionalTerraformingRequest> modifiedMapStates;

	for (ConventionalTerraformingRequest &request : conventionalTerraformingRequests)
	{
		TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(request.tile);

		// not at already selected tile

		if (selectedTiles.count(request.tile) != 0)
			continue;

		// apply changes

		*(request.tile) = request.improvedMapState;
		computeBase(baseId, true);

		// store current state for future restore

		modifiedMapStates.push_back(request);

		// verify tile is worked after improvement

		bool worked = isBaseWorkedTile(baseId, request.tile);

		// not worked - stop condition

		if (!worked)
			break;

		// add request if not terraformed yet

		if (!tileTerraformingInfo.terraformed)
		{
			aiData.production.addTerraformingRequest(request.tile);
		}

	}

	// restore original state

	for (ConventionalTerraformingRequest &request : modifiedMapStates)
	{
		*(request.tile) = request.currentMapState;
	}

	// restore base

	base->pop_size = currentSize;
	computeBase(baseId, true);

}

/*
Generate request for aquifer.
*/
void generateAquiferTerraformingRequest(MAP *tile)
{
	TERRAFORMING_SCORE terraformingScoreObject;
	TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
	calculateAquiferTerraformingScore(tile, terraformingScore);

	if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
	{
		terraformingRequests.emplace_back(tile, terraformingScore->option, terraformingScore->action, terraformingScore->score);
	}

}

/*
Generate request to raise energy collection.
*/
void generateRaiseLandTerraformingRequest(MAP *tile)
{
	TERRAFORMING_SCORE terraformingScoreObject;
	TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
	calculateRaiseLandTerraformingScore(tile, terraformingScore);

	if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
	{
		terraformingRequests.emplace_back(tile, terraformingScore->option, terraformingScore->action, terraformingScore->score);
	}

}

/*
Generate request for network (road/tube).
*/
void generateNetworkTerraformingRequest(MAP *tile)
{
	TERRAFORMING_SCORE terraformingScoreObject;
	TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
	calculateNetworkTerraformingScore(tile, terraformingScore);

	if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
	{
		terraformingRequests.emplace_back(tile, terraformingScore->option, terraformingScore->action, terraformingScore->score);
	}

}

/*
Generate request for Sensor (road/tube).
*/
void generateSensorTerraformingRequest(MAP *tile)
{
	TERRAFORMING_SCORE terraformingScoreObject;
	TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
	calculateSensorTerraformingScore(tile, terraformingScore);

	if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
	{
		terraformingRequests.emplace_back(tile, terraformingScore->option, terraformingScore->action, terraformingScore->score);
	}

}

/*
Comparison function for terraforming requests.
Sort in ranking order ascending then in scoring order descending.
*/
bool compareTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2)
{
	return (terraformingRequest1.score > terraformingRequest2.score);
}

/*
Sort terraforming requests by score.
*/
void sortTerraformingRequests()
{
	debug("sortTerraformingRequests\n");

	// sort requests

	std::sort(terraformingRequests.begin(), terraformingRequests.end(), compareTerraformingRequests);

	if (DEBUG)
	{
		for (const TERRAFORMING_REQUEST &terraformingRequest : terraformingRequests)
		{
			debug
			(
				"\t(%3d,%3d)"
				" %-20s"
				" %6.3f"
				"\n"
				, getX(terraformingRequest.tile), getY(terraformingRequest.tile)
				, terraformingRequest.option->name
				, terraformingRequest.score
			);
		}

	}

}

/*
Removes terraforming requests violating proximity rules.
*/
void applyProximityRules()
{
	// apply proximity rules

	for
	(
		std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestsIterator = terraformingRequests.begin();
		terraformingRequestsIterator != terraformingRequests.end();
	)
	{
		TERRAFORMING_REQUEST *terraformingRequest = &(*terraformingRequestsIterator);

		// proximity rule

		bool tooClose = false;

		robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(terraformingRequest->action);
		if (proximityRulesIterator != PROXIMITY_RULES.end())
		{
			const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

			// there is higher ranked similar action

			tooClose =
				hasNearbyTerraformingRequestAction
				(
					terraformingRequests.begin(),
					terraformingRequestsIterator,
					terraformingRequest->action,
					getX(terraformingRequest->tile),
					getY(terraformingRequest->tile),
					proximityRule->buildingDistance
				)
			;

		}

		if (tooClose)
		{
			// delete this request and advance iterator

			terraformingRequestsIterator = terraformingRequests.erase(terraformingRequestsIterator);

		}
		else
		{
			// advance iterator

			terraformingRequestsIterator++;

		}

	}

}

/*
Removes terraforming requests in terraformed tiles.
*/
void removeTerraformedTiles()
{
	for
	(
		std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestsIterator = terraformingRequests.begin();
		terraformingRequestsIterator != terraformingRequests.end();
	)
	{
		TERRAFORMING_REQUEST &terraformingRequest = *terraformingRequestsIterator;
		TileTerraformingInfo &terraformingRequestTileInfo = getTileTerraformingInfo(terraformingRequest.tile);

		if (terraformingRequestTileInfo.terraformed)
		{
			terraformingRequestsIterator = terraformingRequests.erase(terraformingRequestsIterator);
		}
		else
		{
			terraformingRequestsIterator++;
		}

	}

}

void assignFormerOrders()
{
	debug("assignFormerOrders - %s\n", MFactions[aiFactionId].noun_faction);

	// distribute orders

	robin_hood::unordered_flat_set<FORMER_ORDER *> assignedOrders;
	robin_hood::unordered_flat_map<TERRAFORMING_REQUEST *, TerraformingRequestAssignment> assignments;

	bool changed;

	do
	{
		changed = false;

		for (FORMER_ORDER &formerOrder : formerOrders)
		{
			int vehicleId = formerOrder.vehicleId;
			VEH *vehicle = &(Vehicles[vehicleId]);
			int triad = vehicle->triad();
			int vehicleMovementSpeed = getVehicleSpeedWithoutRoads(vehicleId);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);

			// skip assigned

			if (assignedOrders.count(&formerOrder) != 0)
				continue;

			debug("\t[%4d] (%3d,%3d)\n", vehicleId, vehicle->x, vehicle->y);

			// find best terraformingRequest

			TERRAFORMING_REQUEST *bestTerraformingRequest = nullptr;
			int bestTerraformingRequestTravelTime = -1;
			double bestTerraformingRequestScore = 0.0;

			for (TERRAFORMING_REQUEST &terraformingRequest : terraformingRequests)
			{
				if (terraformingRequest.tile == nullptr)
				{
					debug("[ERROR] terraformingRequest.tile == nullptr\n");
					exit(1);
				}

				MAP *tile = terraformingRequest.tile;
				bool tileOcean = is_ocean(tile);

				// proper triad

				if ((tileOcean && triad == TRIAD_LAND) || (!tileOcean && triad == TRIAD_SEA))
					continue;

				// same association for sea fomer

				if (triad == TRIAD_SEA && !isSameOceanAssociation(vehicleTile, tile, aiFactionId))
					continue;

				// land for land fomer

				if (triad == TRIAD_LAND && tileOcean)
					continue;

				// reachable

				if (!isVehicleDestinationReachable(vehicleId, tile, false))
					continue;

				// estimate travelTime

				int range = getRange(vehicleTile, tile);
				int travelTime = divideIntegerRoundUp(range, vehicleMovementSpeed);

				if (triad == TRIAD_LAND && !isSameAssociation(vehicleTile, tile, aiFactionId))
				{
					travelTime += 10;
				}

				debug("\t\t->(%3d,%3d)\n", getX(tile), getY(tile));

				// for assigned request check if this terraformer is closer

				if (assignments.count(&terraformingRequest) != 0)
				{
					TerraformingRequestAssignment &assignment = assignments.at(&terraformingRequest);

					// not closer than already assigned former

					if (travelTime >= assignment.travelTime)
					{
						debug
						(
							"\t\t\tnot closer than [%4d] (%3d,%3d)"
							"\n"
							, assignment.formerOrder->vehicleId
							, getVehicle(assignment.formerOrder->vehicleId)->x, getVehicle(assignment.formerOrder->vehicleId)->y
						);

						continue;

					}

				}

				// travelTimeCoefficient

				double travelTimeCoefficient = getExponentialCoefficient(aiData.developmentScale, travelTime);

				// oneTurnAction coefficient

				double oneTurnActionCoefficient = (formerOrder.oneTurnTerraformingLocations.count(tile) == 0 ? 1.0 : 1.1);

				// score

				double score = terraformingRequest.score * travelTimeCoefficient * oneTurnActionCoefficient;

				// update best

				if (score > bestTerraformingRequestScore)
				{
					bestTerraformingRequest = &terraformingRequest;
					bestTerraformingRequestTravelTime = travelTime;
					bestTerraformingRequestScore = score;
				}

			}

			// not found

			if (bestTerraformingRequest == nullptr)
			{
				debug("\t\tbestTerraformingRequest not found\n");
				continue;
			}

			// remove existing assignment if any

			if (assignments.count(bestTerraformingRequest) != 0)
			{
				TerraformingRequestAssignment &assignment = assignments.at(bestTerraformingRequest);

				debug
				(
					"\t\t\tremove existing assignment [%4d] (%3d,%3d)"
					"\n"
					, assignment.formerOrder->vehicleId
					, getVehicle(assignment.formerOrder->vehicleId)->x, getVehicle(assignment.formerOrder->vehicleId)->y
				);

				assignedOrders.erase(assignment.formerOrder);

			}

			debug
			(
				"\t\t\tbest: (%3d,%3d)"
				"\n"
				, getX(bestTerraformingRequest->tile), getY(bestTerraformingRequest->tile)
			);

			// set assignment

			assignments[bestTerraformingRequest] = {&formerOrder, bestTerraformingRequestTravelTime};
			assignedOrders.insert(&formerOrder);
			changed = true;

		}

	}
	while (changed);

	// set tasks

	for (robin_hood::pair<TERRAFORMING_REQUEST *, TerraformingRequestAssignment> &assignmentEntry : assignments)
	{
		TERRAFORMING_REQUEST *terraformingRequest = assignmentEntry.first;
		TerraformingRequestAssignment &assignment = assignmentEntry.second;
		FORMER_ORDER *formerOrder = assignment.formerOrder;

		formerOrder->tile = terraformingRequest->tile;
		formerOrder->action = terraformingRequest->action;

		debug
		(
			"\t[%4d] (%3d,%3d) -> (%3d,%3d)"
			"\n"
			, formerOrder->vehicleId
			, getVehicle(formerOrder->vehicleId)->x, getVehicle(formerOrder->vehicleId)->y
			, getX(terraformingRequest->tile), getY(terraformingRequest->tile)
		);

	}

}

void finalizeFormerOrders()
{
	// iterate former orders

	for (FORMER_ORDER &formerOrder : formerOrders)
	{
		if (formerOrder.action == -1)
		{
			// kill supported formers without orders

			int vehicleId = formerOrder.vehicleId;
			VEH *vehicle = &(Vehicles[vehicleId]);

			// supported

			if (vehicle->home_base_id < 0)
				continue;

			// disband former without order

			if (formerOrder.action == -1)
			{
				setTask(Task(vehicleId, TT_KILL));
			}

		}
		else
		{
			transitVehicle(Task(formerOrder.vehicleId, TT_TERRAFORM, formerOrder.tile, nullptr, -1, formerOrder.action));
		}

	}

}

/*
Selects best terraforming option around given base and calculates its terraforming score.
*/
ConventionalTerraformingRequest calculateConventionalTerraformingScore(int baseId, MAP *tile, TERRAFORMING_OPTION *option, bool fitness)
{
	bool ocean = is_ocean(tile);
	bool rocky = (map_rockiness(tile) == 2);
	bool fungus = map_has_item(tile, BIT_FUNGUS);

	debug("calculateTerraformingScore: (%3d,%3d) %s\n", getX(tile), getY(tile), option->name);

	ConventionalTerraformingRequest request;
	request.tile = tile;
	request.option = option;

	// store tile values

	MAP currentMapState = *tile;

	// process only correct region type

	if (option->ocean != ocean)
		return request;

	// process only correct rockiness

	if (option->rocky && !(!ocean && rocky))
		return request;

	// check if required action technology is available when required

	if (!(option->requiredAction == -1 || isTerraformingAvailable(tile, option->requiredAction)))
		return request;

	debug("\t%s\n", option->name);

	// initialize variables

	MAP improvedMapState = currentMapState;

	int firstAction = -1;
	int terraformingTime = 0;
	double penaltyScore = 0.0;
	double improvementScore = 0.0;

	// process actions

	int availableActionCount = 0;
	int completedActionCount = 0;
	bool optionFungus = fungus;

	for(int action : option->actions)
	{
		// skip unavailable action

		if (!isTerraformingAvailable(tile, action))
			continue;

		// increment available actions

		availableActionCount++;

		// skip completed action

		if (isTerraformingCompleted(tile, action))
		{
			completedActionCount++;
			continue;
		}

		// remove fungus if needed

		if (optionFungus && isRemoveFungusRequired(action))
		{
			optionFungus = false;
			int removeFungusAction = FORMER_REMOVE_FUNGUS;

			// store first action

			if (firstAction == -1)
			{
				firstAction = removeFungusAction;
			}

			// compute terraforming time

			int removeFungusTerraformingTime =
				getDestructionAdjustedTerraformingTime
				(
					removeFungusAction,
					improvedMapState.items,
					improvedMapState.val3, // rocks
					nullptr
				)
			;

			// reduce effect of axilliary action

			terraformingTime += removeFungusTerraformingTime / 2;

			// generate terraforming change

			generateTerraformingChange(&improvedMapState, removeFungusAction);

			// add penalty for nearby forest/kelp (except rocky)

			if (!rocky)
			{
				int nearbyForestKelpCount = nearby_items(getX(tile), getY(tile), 1, (ocean ? BIT_FARM : BIT_FOREST));
				penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;
			}

		}

		// level terrain if needed

		if (!ocean && rocky && isLevelTerrainRequired(ocean, action))
		{
			int levelTerrainAction = FORMER_LEVEL_TERRAIN;

			// store first action

			if (firstAction == -1)
			{
				firstAction = levelTerrainAction;
			}

			// compute terraforming time

			int levelTerrainTerraformingTime =
				getDestructionAdjustedTerraformingTime
				(
					levelTerrainAction,
					improvedMapState.items,
					improvedMapState.val3, // rocks
					nullptr
				)
			;

			// reduce effect of axilliary action

			terraformingTime += levelTerrainTerraformingTime / 2;

			// generate terraforming change

			generateTerraformingChange(&improvedMapState, levelTerrainAction);

		}

		// execute main action

		// store first action

		if (firstAction == -1)
		{
			firstAction = action;
		}

		// compute terraforming time

		terraformingTime +=
			getDestructionAdjustedTerraformingTime
			(
				action,
				improvedMapState.items,
				improvedMapState.val3, // rocks
				NULL
			)
		;

		// generate terraforming change

		generateTerraformingChange(&improvedMapState, action);

		// add penalty for nearby forest/kelp

		if (ocean)
		{
			// kelp farm

			if (action == FORMER_FARM)
			{
				int nearbyKelpCount = nearby_items(getX(tile), getY(tile), 1, BIT_FARM);
				penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyKelpCount;
			}

		}
		else
		{
			// forest

			if (action == FORMER_FOREST)
			{
				int nearbyForestCount = nearby_items(getX(tile), getY(tile), 1, BIT_FOREST);
				penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestCount;
			}

		}

	}

	// ignore unpopulated options

	if (firstAction == -1 || terraformingTime == 0)
		return request;

	// calculate yield improvement score

	double surplusEffectScore = computeImprovementSurplusEffectScore(tile, &currentMapState, &improvedMapState);
	double yieldScore = computeBaseImprovementYieldScore(baseId, tile, &currentMapState, &improvedMapState);

	// ignore options not increasing yield

	if (surplusEffectScore <= 0.0)
		return request;

	// special yield computation for area effects

	double condenserExtraYieldScore = estimateCondenserExtraYieldScore(tile, &(option->actions));

	// fitness score

	double fitnessScore = fitness ? calculateFitnessScore(tile, option->requiredAction, rocky && !option->rocky) : 0.0;

	// completion bonus is given if improvement set is partially completed and no improvement will be destroyed in the process

	double completionScore = 0.0;

	if (option->requiredAction == FORMER_MINE || option->requiredAction == FORMER_SOLAR || option->requiredAction == FORMER_CONDENSER || option->requiredAction == FORMER_ECH_MIRROR)
	{
		if (map_has_item(tile, BIT_FARM))
		{
			if (!map_has_item(tile, BIT_MINE | BIT_SOLAR | BIT_CONDENSER | BIT_ECH_MIRROR))
			{
				completionScore = conf.ai_terraforming_completion_bonus;
			}
		}
	}

	// summarize score

	improvementScore = penaltyScore + surplusEffectScore + condenserExtraYieldScore + fitnessScore + completionScore;

	// calculate operation time coefficient

	int operationTime = /*conf.ai_terraforming_travel_time_multiplier * travelTime + */terraformingTime;

	// calculate terraforming score

	double terraformingScore = getNormalizedTerraformingIncomeGain(operationTime, improvementScore);

	debug
	(
		"\t\t%-20s=%6.3f"
		" penalty=%6.3f"
		" surplusEffect=%6.3f"
		" condenser=%6.3f"
		" fitness=%6.3f"
		" completion=%6.3f"
		" combined=%6.3f"
		" operationTime=%2d"
		" terraformingTime=%2d"
//		" travelTime=%2d"
		"\n"
		, "terraformingScore"
		, terraformingScore
		, penaltyScore
		, surplusEffectScore
		, condenserExtraYieldScore
		, fitnessScore
		, completionScore
		, improvementScore
		, operationTime
		, terraformingTime
//		, travelTime
	)
	;

	// no option selected

	if (terraformingScore <= 0.0)
		return request;

	// update return values

	request.action = firstAction;
	request.score = terraformingScore;
	request.currentMapState = currentMapState;
	request.improvedMapState = improvedMapState;
	request.yieldScore = yieldScore;

	return request;

}

/*
Aquifer calculations.
*/
void calculateAquiferTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);

	// land only

	if (ocean)
		return;

//	// calculate travel time
//
//	int travelTime = getClosestAvailableFormerTravelTime(tile);
//
//	if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
//		return;
//
	// get option

	TERRAFORMING_OPTION *option = &(AQUIFER_TERRAFORMING_OPTION);

	// initialize variables

	int firstAction = -1;
	int terraformingTime = 0;
	double improvementScore = 0.0;

	int action = FORMER_AQUIFER;

	// skip unavailable actions

	if (!isTerraformingAvailable(tile, action))
		return;

	// do not generate request for completed action

	if (isTerraformingCompleted(tile, action))
		return;

	// set action

	if (firstAction == -1)
	{
		firstAction = action;
	}

	// compute terraforming time

	terraformingTime = getDestructionAdjustedTerraformingTime(action, tile->items, tile->val3, NULL);

	// skip if no action or no terraforming time

	if (firstAction == -1 || terraformingTime == 0)
		return;

	// special yield computation for aquifer

	improvementScore += estimateAquiferExtraYieldScore(tile);

	// calculate operation time coefficient

	int operationTime = /*conf.ai_terraforming_travel_time_multiplier * travelTime + */terraformingTime;

	// calculate terraforming score

	double terraformingScore = getNormalizedTerraformingIncomeGain(operationTime, improvementScore);

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s"
		" score=%6.3f"
		" terraformingTime=%2d"
//		" travelTime=%2d"
		" final=%6.3f"
		"\n",
		option->name,
		improvementScore,
		terraformingTime,
//		travelTime,
		terraformingScore
	)
	;

}

/*
Raise land calculations.
*/
void calculateRaiseLandTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);

	// land only

	if (ocean)
		return;

	// verify we have enough money

	int cost = terraform_cost(x, y, aiFactionId);
	if (cost > Factions[aiFactionId].energy_credits / 10)
		return;

//	// calculate travel time
//
//	int travelTime = getClosestAvailableFormerTravelTime(tile);
//
//	if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
//		return;
//
	// get option

	TERRAFORMING_OPTION *option = &(RAISE_LAND_TERRAFORMING_OPTION);

	// initialize variables

	int firstAction = -1;
	int terraformingTime = 0;
	double improvementScore = 0.0;

	int action = FORMER_RAISE_LAND;

	// skip unavailable actions

	if (!isTerraformingAvailable(tile, action))
		return;

	// set action

	if (firstAction == -1)
	{
		firstAction = action;
	}

	// compute terraforming time

	terraformingTime = getDestructionAdjustedTerraformingTime(action, tile->items, tile->val3, NULL);

	// skip if no action or no terraforming time

	if (firstAction == -1 || terraformingTime == 0)
		return;

	// special yield computation for raise land

	improvementScore += estimateRaiseLandExtraYieldScore(tile, cost);

	// do not generate request for non positive score

	if (improvementScore <= 0.0)
		return;

	// calculate operation time coefficient

	int operationTime = /*conf.ai_terraforming_travel_time_multiplier * travelTime + */terraformingTime;

	// calculate terraforming score

	double terraformingScore = getNormalizedTerraformingIncomeGain(operationTime, improvementScore);

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s"
		" score=%6.3f"
		" terraformingTime=%2d"
//		" travelTime=%2d"
		" final=%6.3f"
		"\n",
		option->name,
		improvementScore,
		terraformingTime,
//		travelTime,
		terraformingScore
	)
	;

}

/*
Network calculations.
*/
void calculateNetworkTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);

	// land only

	if (ocean)
		return;

//	// calculate travel time
//
//	int travelTime = getClosestAvailableFormerTravelTime(tile);
//
//	if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
//		return;
//
	// get option

	TERRAFORMING_OPTION *option = &(NETWORK_TERRAFORMING_OPTION);

	// initialize variables

	int firstAction = -1;
	int terraformingTime = 0;
	double improvementScore = 0.0;

	// process actions

	int action;

	if (!map_has_item(tile, BIT_ROAD))
	{
		action = FORMER_ROAD;
	}
	else if (!map_has_item(tile, BIT_MAGTUBE))
	{
		action = FORMER_MAGTUBE;
	}
	else
	{
		// everything is built
		return;
	}

	// skip unavailable actions

	if (!isTerraformingAvailable(tile, action))
		return;

	// remove fungus if needed

	if (map_has_item(tile, BIT_FUNGUS) && isRemoveFungusRequired(action))
	{
		int removeFungusAction = FORMER_REMOVE_FUNGUS;

		// store first action

		if (firstAction == -1)
		{
			firstAction = removeFungusAction;
		}

		// compute terraforming time

		terraformingTime += getDestructionAdjustedTerraformingTime(removeFungusAction, tile->items, tile->val3, NULL);

		// add penalty for nearby forest/kelp

		int nearbyForestKelpCount = nearby_items(x, y, 1, (ocean ? BIT_FARM : BIT_FOREST));
		improvementScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;

	}

	// store first action

	if (firstAction == -1)
	{
		firstAction = action;
	}

	// compute terraforming time

	terraformingTime += getDestructionAdjustedTerraformingTime(action, tile->items, tile->val3, NULL);

	// special network bonus

	improvementScore += (1.0 + networkDemand) * calculateNetworkScore(tile, action);

	// do not create request for zero score

	if (improvementScore <= 0.0)
		return;

	// calculate operation time coefficient

	int operationTime = /*conf.ai_terraforming_travel_time_multiplier * travelTime + */terraformingTime;

	// calculate terraforming score

	double terraformingScore = getNormalizedTerraformingIncomeGain(operationTime, improvementScore);

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s"
		" score=%6.3f"
		" networkDemand=%5.2f"
		" terraformingTime=%2d"
//		" travelTime=%2d"
		" final=%6.3f"
		"\n",
		option->name,
		improvementScore,
		networkDemand,
		terraformingTime,
//		travelTime,
		terraformingScore
	)
	;

}

/*
Sensor calculations.
*/
void calculateSensorTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);

//	// calculate travel time
//
//	int travelTime = getClosestAvailableFormerTravelTime(tile);
//
//	if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
//		return;
//
	// get option

	TERRAFORMING_OPTION *option = (ocean ? &(OCEAN_SENSOR_TERRAFORMING_OPTION) : &(LAND_SENSOR_TERRAFORMING_OPTION));

	// initialize variables

	int firstAction = -1;
	int terraformingTime = 0;
	double improvementScore = 0.0;

	// process actions

	int action;

	if (!map_has_item(tile, BIT_SENSOR))
	{
		action = FORMER_SENSOR;
	}
	else
	{
		// everything is built
		return;
	}

	// skip unavailable actions

	if (!isTerraformingAvailable(tile, action))
		return;

	// remove fungus if needed

	if (map_has_item(tile, BIT_FUNGUS) && isRemoveFungusRequired(action))
	{
		int removeFungusAction = FORMER_REMOVE_FUNGUS;

		// store first action

		if (firstAction == -1)
		{
			firstAction = removeFungusAction;
		}

		// compute terraforming time

		terraformingTime += getDestructionAdjustedTerraformingTime(removeFungusAction, tile->items, tile->val3, NULL);

		// add penalty for nearby forest/kelp

		int nearbyForestKelpCount = nearby_items(x, y, 1, (ocean ? BIT_FARM : BIT_FOREST));
		improvementScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;

	}

	// store first action

	if (firstAction == -1)
	{
		firstAction = action;
	}

	// compute terraforming time

	terraformingTime += getDestructionAdjustedTerraformingTime(action, tile->items, tile->val3, NULL);

	// special sensor bonus

	double sensorScore = calculateSensorScore(tile);

	// calculate exclusivity bonus

	double fitnessScore = calculateFitnessScore(tile, option->requiredAction, false);

	// summarize score

	improvementScore = sensorScore + fitnessScore;

	// do not create request for zero score

	if (improvementScore <= 0.0)
		return;

	// calculate operation time coefficient

	int operationTime = /*conf.ai_terraforming_travel_time_multiplier * travelTime + */terraformingTime;

	// calculate terraforming score

	double terraformingScore = getNormalizedTerraformingIncomeGain(operationTime, improvementScore);

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s"
		" score=%6.3f"
		" fitnessScore=%6.3f"
		" improvementScore=%6.3f"
		" terraformingTime=%2d"
//		" travelTime=%2d"
		" final=%6.3f"
		"\n",
		option->name,
		sensorScore,
		fitnessScore,
		improvementScore,
		terraformingTime,
//		travelTime,
		terraformingScore
	)
	;

}

/*
Computes workable bases surplus effect.
*/
double computeImprovementSurplusEffectScore(MAP *tile, MAP *currentMapState, MAP *improvedMapState)
{
	int mapIndex = getMapIndexByPointer(tile);
	TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(mapIndex);

	double resultScore = 0.0;

	// calculate affected range
	// building or destroying mirror requires recomputation of all bases in range 1

	if
	(
		((currentMapState->items & BIT_ECH_MIRROR) != 0 && (improvedMapState->items & BIT_ECH_MIRROR) == 0)
		||
		((currentMapState->items & BIT_ECH_MIRROR) == 0 && (improvedMapState->items & BIT_ECH_MIRROR) != 0)
	)
	{
		// compute score
		// summarize all scores

		for (int baseId : tileTerraformingInfo.areaWorkableBaseIds)
		{
			// compute base score

			double score = computeImprovementBaseSurplusEffectScore(baseId, tile, currentMapState, improvedMapState);

			// update score

			if (score > 0.0)
			{
				resultScore += score;
			}

		}

	}
	else
	{
		// compute score
		// return first non zero score

		for (int baseId : tileTerraformingInfo.workableBaseIds)
		{
			// compute base score

			double score = computeImprovementBaseSurplusEffectScore(baseId, tile, currentMapState, improvedMapState);

			// update best score

			if (score > 0.0)
			{
				resultScore = score;
				break;
			}

		}

	}

	return resultScore;

}

/*
Determines whether terraforming is already completed in this tile.
*/
bool isTerraformingCompleted(MAP *tile, int action)
{
	return (tile->items & Terraform[action].bit);

}

/*
Determines whether vehicle order is a terraforming order and is already completed in this tile.
*/
bool isVehicleTerrafomingOrderCompleted(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	if (!(vehicle->order >= ORDER_FARM && vehicle->order <= ORDER_PLACE_MONOLITH))
		return false;

	MAP *tile = getMapTile(vehicle->x, vehicle->y);

	return (tile->items & Terraform[vehicle->order - ORDER_FARM].bit);

}

/*
Determines whether terraforming can be done in this square with removing fungus and leveling terrain if needed.
Terraforming is considered available if this item is already built.
*/
bool isTerraformingAvailable(MAP *tile, int action)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	bool oceanDeep = is_ocean_deep(tile);

	bool aquatic = MFactions[aiFactionId].rule_flags & RFLAG_AQUATIC;

	// action is considered available if is already built

	if (map_has_item(tile, Terraform[action].bit))
		return true;

	// action is available only when enabled and researched

	if (!has_terra(aiFactionId, (FormerItem)action, ocean))
		return false;

	// terraforming is not available in base square

	if (tile->items & BIT_BASE_IN_TILE)
		return false;

	// ocean improvements in deep ocean are available for aquatic faction with Adv. Ecological Engineering only

	if (oceanDeep)
	{
		if (!(aquatic && has_tech(aiFactionId, TECH_EcoEng2)))
			return false;
	}

	// special cases

	switch (action)
	{
	case FORMER_SOIL_ENR:
		// enricher requires farm
		if (~tile->items & BIT_FARM)
			return  false;
		break;
	case FORMER_FOREST:
//		// forest should not be built near another building forest
//		if (isNearbyForestUnderConstruction(x, y))
//			return  false;
		break;
	case FORMER_MAGTUBE:
		// tube requires road
		if (~tile->items & BIT_ROAD)
			return  false;
		break;
	case FORMER_REMOVE_FUNGUS:
		// removing fungus requires fungus
		if (~tile->items & BIT_FUNGUS)
			return  false;
		break;
	case FORMER_CONDENSER:
		// condenser should not be built near another building condenser
		if (isNearbyCondeserUnderConstruction(x, y))
			return  false;
		break;
	case FORMER_ECH_MIRROR:
		// mirror should not be built near another building mirror
		if (isNearbyMirrorUnderConstruction(x, y))
			return  false;
		break;
	case FORMER_THERMAL_BORE:
		// borehole should not be adjacent to another borehole
		if (isNearbyBoreholePresentOrUnderConstruction(x, y))
			return  false;
		break;
	case FORMER_AQUIFER:
		// aquifer cannot be drilled next to river
		if (isNearbyRiverPresentOrUnderConstruction(x, y))
			return  false;
		break;
	case FORMER_RAISE_LAND:
		// cannot raise beyond limit
		if (map_level(tile) == (is_ocean(tile) ? ALT_OCEAN_SHELF : ALT_THREE_ABOVE_SEA))
			return false;
		// raise land should not disturb other faction bases
		if (!isRaiseLandSafe(tile))
			return false;
		// raise land should not be built near another building raise
		if (isNearbyRaiseUnderConstruction(x, y))
			return false;
		break;
	case FORMER_LOWER_LAND:
		// lower land should not disturb other faction bases
		// TODO
		return false;
		break;
	case FORMER_SENSOR:
		// sensor don't need to be build too close to each other
		if (isNearbySensorPresentOrUnderConstruction(x, y))
			return  false;
		break;
	}

	return true;

}

/*
Determines whether fungus need to be removed before terraforming.
*/
bool isRemoveFungusRequired(int action)
{
	// no need to remove fungus for planting fungus

	if (action == FORMER_PLANT_FUNGUS)
		return false;

	// always remove fungus for basic improvements even with fungus improvement technology

	if (action >= FORMER_FARM && action <= FORMER_SOLAR)
		return true;

	// no need to remove fungus for road with fungus road technology

	if (action == FORMER_ROAD && has_tech(aiFactionId, Rules->tech_preq_build_road_fungus))
		return false;

	// for everything else remove fungus only without fungus improvement technology

	return !has_tech(aiFactionId, Rules->tech_preq_improv_fungus);

}

/*
Determines whether rocky terrain need to be leveled before terraforming.
*/
bool isLevelTerrainRequired(bool ocean, int action)
{
	return !ocean && (action == FORMER_FARM || action == FORMER_SOIL_ENR || action == FORMER_FOREST);

}

bool isVehicleConvoying(VEH *vehicle)
{
	return vehicle->order == ORDER_CONVOY;
}

bool isVehicleTerraforming(VEH *vehicle)
{
	return vehicle->order >= ORDER_FARM && vehicle->order <= ORDER_PLACE_MONOLITH;
}

bool isNearbyForestUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_FOREST);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);
	int range = proximityRule->buildingDistance;

	// check building item

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -(2 * range - abs(dx)); dy <= +(2 * range - abs(dx)); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedForest)
                return true;

        }

    }

    return false;

}

bool isNearbyCondeserUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_CONDENSER);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);
	int range = proximityRule->buildingDistance;

	// check terraformed condensers

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -(2 * range - abs(dx)); dy <= +(2 * range - abs(dx)); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedCondenser)
                return true;

        }

    }

    return false;

}

bool isNearbyMirrorUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_ECH_MIRROR);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);
	int range = proximityRule->buildingDistance;

	// check terraformed mirrors

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -(2 * range - abs(dx)); dy <= +(2 * range - abs(dx)); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (!tile)
				continue;

			if (getTileTerraformingInfo(tile).terraformedMirror)
			{
				debug("terraformedMirrorTiles found: (%3d,%3d)\n", wrap(x + dx), y + dy);
                return true;
			}

        }

    }

    return false;

}

bool isNearbyBoreholePresentOrUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_THERMAL_BORE);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check existing items

	if (nearby_items(x, y, proximityRule->existingDistance, BIT_THERMAL_BORE) >= 1)
		return true;

	// check building items

	int range = proximityRule->buildingDistance;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedBorehole)
                return true;

        }

    }

    return false;

}

bool isNearbyRiverPresentOrUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_AQUIFER);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check existing items

	if (nearby_items(x, y, proximityRule->existingDistance, BIT_RIVER) >= 1)
		return true;

	// check building items

	int range = proximityRule->buildingDistance;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedAquifer)
                return true;

        }

    }

    return false;

}

bool isNearbyRaiseUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_RAISE_LAND);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check building items

	int range = proximityRule->buildingDistance;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedRaise)
                return true;

        }

    }

    return false;

}

bool isNearbySensorPresentOrUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_SENSOR);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check existing items

	if (nearby_items(x, y, proximityRule->existingDistance, BIT_SENSOR) >= 1)
		return true;

	// check building items

	int range = proximityRule->buildingDistance;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedSensor)
                return true;

        }

    }

    return false;

}

/*
Calculates terraforming time based on faction, tile, and former abilities.
*/
int getTerraformingTime(int action, int items, int rocks, VEH* vehicle)
{
	assert(action >= FORMER_FARM && action <= FORMER_MONOLITH);

	// get base terraforming time

	int terraformingTime = Terraform[action].rate;

	// adjust for terrain properties

	switch (action)
	{
	case FORMER_SOLAR:

		// rockiness increases solar collector construction time

		if (rocks & TILE_ROLLING)
		{
			terraformingTime += 2;
		}
		else if (rocks & TILE_ROCKY)
		{
			terraformingTime += 4;
		}

		break;

	case FORMER_ROAD:
	case FORMER_MAGTUBE:

		// forest increases road construction time and ignores rockiness

		if (items & BIT_FOREST)
		{
			terraformingTime += 2;
		}
		else
		{
			// fungus increases road construction time

			if (items & BIT_FUNGUS)
			{
				terraformingTime += 2;
			}

			// rockiness increases road construction time

			if (rocks & TILE_ROLLING)
			{
				terraformingTime += 1;
			}
			else if (rocks & TILE_ROCKY)
			{
				terraformingTime += 2;
			}

		}

		// river increases road construction time and ignores rockiness

		if (items & BIT_RIVER)
		{
			terraformingTime += 1;
		}

	}

	// adjust for faction projects

	if (isProjectBuilt(aiFactionId, FAC_WEATHER_PARADIGM))
	{
		// reduce all terraforming time except fungus by 1/3

		if (action != FORMER_REMOVE_FUNGUS && action != FORMER_PLANT_FUNGUS)
		{
			terraformingTime -= terraformingTime / 3;
		}

	}

	if (isProjectBuilt(aiFactionId, FAC_XENOEMPATHY_DOME))
	{
		// reduce fungus terraforming time by half

		if (action == FORMER_REMOVE_FUNGUS || action == FORMER_PLANT_FUNGUS)
		{
			terraformingTime -= terraformingTime / 2;
		}

	}

	// adjust for vehicle abilities

	if (vehicle)
	{
		if (action == FORMER_REMOVE_FUNGUS && vehicle_has_ability(vehicle, ABL_FUNGICIDAL))
		{
			terraformingTime -= terraformingTime / 2;
		}

		if (vehicle_has_ability(vehicle, ABL_SUPER_TERRAFORMER))
		{
			terraformingTime -= terraformingTime / 2;
		}

	}

	return terraformingTime;

}

/*
Calculates terraforming time adjusted on improvement destruction.
*/
int getDestructionAdjustedTerraformingTime(int action, int items, int rocks, VEH* vehicle)
{
	// get pure terraforming time

	int terraformingTime = getTerraformingTime(action, items, rocks, vehicle);

	// process improvements

	switch (action)
	{
	case FORMER_MINE:
		if ((items & (BIT_SOLAR | BIT_CONDENSER | BIT_ECH_MIRROR | BIT_THERMAL_BORE)) != 0)
		{
			terraformingTime += Terraform[FORMER_MINE].rate;
		}
		break;

	case FORMER_SOLAR:
		// no penalty for replacing collector<->mirror
		if ((items & (BIT_MINE | BIT_CONDENSER | BIT_THERMAL_BORE)) != 0)
		{
			terraformingTime += Terraform[FORMER_SOLAR].rate;
		}
		break;

	case FORMER_CONDENSER:
		if ((items & (BIT_MINE | BIT_SOLAR | BIT_ECH_MIRROR | BIT_THERMAL_BORE)) != 0)
		{
			terraformingTime += Terraform[FORMER_CONDENSER].rate;
		}
		break;

	case FORMER_ECH_MIRROR:
		// no penalty for replacing collector<->mirror
		if ((items & (BIT_MINE | BIT_CONDENSER | BIT_THERMAL_BORE)) != 0)
		{
			terraformingTime += Terraform[FORMER_ECH_MIRROR].rate;
		}
		break;

	case FORMER_THERMAL_BORE:
		if ((items & (BIT_MINE | BIT_SOLAR | BIT_CONDENSER | BIT_ECH_MIRROR)) != 0)
		{
			terraformingTime += Terraform[FORMER_THERMAL_BORE].rate;
		}
		break;

	}

	return terraformingTime;

}

//int getClosestAvailableFormerTravelTime(MAP *tile)
//{
//	int closestAvailableFormerTravelTime = -1;
//
//	if (!is_ocean(tile))
//	{
//		// iterate available land formers
//
//		for (FORMER_ORDER formerOrder : formerOrders)
//		{
//			int vehicleId = formerOrder.id;
//			VEH *vehicle = &(Vehicles[vehicleId]);
//
//			// land former
//
//			if (vehicle->triad() != TRIAD_LAND)
//				continue;
//
//			// travel time
//
//			int travelTime = getVehicleATravelTime(vehicleId, tile);
//
//			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
//				continue;
//
//			// update the best
//
//			if (closestAvailableFormerTravelTime == -1 || travelTime < closestAvailableFormerTravelTime)
//			{
//				closestAvailableFormerTravelTime = travelTime;
//			}
//
//		}
//
//		if (closestAvailableFormerTravelTime == -1)
//		{
//			// try travel time from nearest base instead
//
//			int unitId = getFactionFastestFormerUnitId(aiFactionId);
//			int unitSpeed = getUnitSpeed(unitId);
//
//			for (int baseId : aiData.baseIds)
//			{
//				MAP *baseTile = getBaseMapTile(baseId);
//
//				// travel time
//
//				int travelTime = getUnitATravelTime(baseTile, tile, aiFactionId, unitId, unitSpeed, false, 1);
//
//				if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
//					continue;
//
//				// update the best
//
//				if (closestAvailableFormerTravelTime == -1 || travelTime < closestAvailableFormerTravelTime)
//				{
//					closestAvailableFormerTravelTime = travelTime;
//				}
//
//			}
//
//		}
//
//	}
//	else
//	{
//		// iterate sea former able to serve this tile
//
//		for (FORMER_ORDER formerOrder : formerOrders)
//		{
//			int vehicleId = formerOrder.id;
//			VEH *vehicle = &(Vehicles[vehicleId]);
//			MAP *vehicleTile = getVehicleMapTile(vehicleId);
//
//			// sea former
//
//			if (vehicle->triad() != TRIAD_SEA)
//				continue;
//
//			// serving this tile
//
//			if (!isSameOceanAssociation(vehicleTile, tile, aiFactionId))
//				continue;
//
//			// travel time
//
//			int travelTime = getVehicleATravelTime(vehicleId, tile);
//
//			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
//				continue;
//
//			// update the best
//
//			if (closestAvailableFormerTravelTime == -1 || travelTime < closestAvailableFormerTravelTime)
//			{
//				closestAvailableFormerTravelTime = travelTime;
//			}
//
//		}
//
//		if (closestAvailableFormerTravelTime == -1)
//		{
//			// try travel time from nearest base instead
//
//			int unitId = getFactionFastestFormerUnitId(aiFactionId);
//			int unitSpeed = getUnitSpeed(unitId);
//
//			for (int baseId : aiData.baseIds)
//			{
//				MAP *baseTile = getBaseMapTile(baseId);
//
//				// same ocean association
//
//				if (!isSameOceanAssociation(baseTile, tile, aiFactionId))
//					continue;
//
//				// travel time
//
//				int travelTime = getUnitATravelTime(baseTile, tile, aiFactionId, unitId, unitSpeed, false, 1);
//
//				if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
//					continue;
//
//				// update the best
//
//				if (closestAvailableFormerTravelTime == -1 || travelTime < closestAvailableFormerTravelTime)
//				{
//					closestAvailableFormerTravelTime = travelTime;
//				}
//
//			}
//
//		}
//
//	}
//
//	return closestAvailableFormerTravelTime;
//
//}
//
/*
Calculates score for network actions (road, tube)
*/
double calculateNetworkScore(MAP *tile, int action)
{
	int x = getX(tile);
	int y = getY(tile);

	// ignore anything but road and tube

	if (!(action == FORMER_ROAD || action == FORMER_MAGTUBE))
		return 0.0;

	// ignore impossible actions

	if
	(
		(action == FORMER_ROAD && (tile->items & BIT_ROAD))
		||
		(action == FORMER_MAGTUBE && ((~tile->items & BIT_ROAD) || (tile->items & BIT_MAGTUBE)))
	)
		return 0.0;

	// get terrain improvement flag

	int improvementFlag;

	switch (action)
	{
	case FORMER_ROAD:
		improvementFlag = BIT_ROAD;
		break;
	case FORMER_MAGTUBE:
		improvementFlag = BIT_MAGTUBE;
		break;
	default:
		return 0.0;
	}

	// check items around the tile

	bool m[OFFSET_COUNT_ADJACENT];

	for (int offsetIndex = 1; offsetIndex < OFFSET_COUNT_ADJACENT; offsetIndex++)
	{
		MAP *adjacentTile = getMapTile(wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]), y + BASE_TILE_OFFSETS[offsetIndex][1]);

		m[offsetIndex] = (adjacentTile && !is_ocean(adjacentTile) && (adjacentTile->items & (BIT_BASE_IN_TILE | improvementFlag)));

	}

	// don't build connection to nothing

	if(!m[1] && !m[2] && !m[3] && !m[4] && !m[5] && !m[6] && !m[7] && !m[8])
	{
		return 0.0;
	}

	// needs connection

	if
	(
		// opposite corners
		(m[2] && m[6] && (!m[3] || !m[5]) && (!m[7] || !m[1]))
		||
		(m[4] && m[8] && (!m[5] || !m[7]) && (!m[1] || !m[3]))
		||
		// do not connect next corners
//		// next corners
//		(m[2] && m[4] && !m[3] && (!m[5] || !m[7] || !m[1]))
//		||
//		(m[4] && m[6] && !m[5] && (!m[7] || !m[1] || !m[3]))
//		||
//		(m[6] && m[8] && !m[7] && (!m[1] || !m[3] || !m[5]))
//		||
//		(m[8] && m[2] && !m[1] && (!m[3] || !m[5] || !m[7]))
//		||
		// opposite sides
		(m[1] && m[5] && !m[3] && !m[7])
		||
		(m[3] && m[7] && !m[5] && !m[1])
		||
		// side to corner
		(m[1] && m[4] && !m[3] && (!m[5] || !m[7]))
		||
		(m[1] && m[6] && !m[7] && (!m[3] || !m[5]))
		||
		(m[3] && m[6] && !m[5] && (!m[7] || !m[1]))
		||
		(m[3] && m[8] && !m[1] && (!m[5] || !m[7]))
		||
		(m[5] && m[8] && !m[7] && (!m[1] || !m[3]))
		||
		(m[5] && m[2] && !m[3] && (!m[7] || !m[1]))
		||
		(m[7] && m[2] && !m[1] && (!m[3] || !m[5]))
		||
		(m[7] && m[4] && !m[5] && (!m[1] || !m[3]))
	)
	{
		// return connection value

		return conf.ai_terraforming_networkConnectionValue;

	}

	// needs improvement

	if
	(
		// opposite corners
		(m[2] && m[6])
		||
		(m[4] && m[8])
		||
		// do not connect next corners
//		// next corners
//		(m[2] && m[4] && !m[3])
//		||
//		(m[4] && m[6] && !m[5])
//		||
//		(m[6] && m[8] && !m[7])
//		||
//		(m[8] && m[2] && !m[1])
//		||
		// opposite sides
		(m[1] && m[5] && !m[3] && !m[7])
		||
		(m[3] && m[7] && !m[5] && !m[1])
		||
		// side to corner
		(m[1] && m[4] && !m[3])
		||
		(m[1] && m[6] && !m[7])
		||
		(m[3] && m[6] && !m[5])
		||
		(m[3] && m[8] && !m[1])
		||
		(m[5] && m[8] && !m[7])
		||
		(m[5] && m[2] && !m[3])
		||
		(m[7] && m[2] && !m[1])
		||
		(m[7] && m[4] && !m[5])
	)
	{
		// return connection value

		return conf.ai_terraforming_networkImprovementValue;

	}

	// needs extension

	bool towardBase = false;

	if (m[1] && !m[3] && !m[4] && !m[5] && !m[6] && !m[7])
	{
		towardBase |= isTowardBaseDiagonal(x, y, -1, +1);
	}
	if (m[3] && !m[5] && !m[6] && !m[7] && !m[8] && !m[1])
	{
		towardBase |= isTowardBaseDiagonal(x, y, -1, -1);
	}
	if (m[5] && !m[7] && !m[8] && !m[1] && !m[2] && !m[3])
	{
		towardBase |= isTowardBaseDiagonal(x, y, +1, -1);
	}
	if (m[7] && !m[1] && !m[2] && !m[3] && !m[4] && !m[5])
	{
		towardBase |= isTowardBaseDiagonal(x, y, +1, +1);
	}

	if (m[2] && !m[4] && !m[5] && !m[6] && !m[7] && !m[8])
	{
		towardBase |= isTowardBaseHorizontal(x, y, -1);
	}
	if (m[6] && !m[8] && !m[1] && !m[2] && !m[3] && !m[4])
	{
		towardBase |= isTowardBaseHorizontal(x, y, +1);
	}

	if (m[4] && !m[6] && !m[7] && !m[8] && !m[1] && !m[2])
	{
		towardBase |= isTowardBaseVertical(x, y, -1);
	}
	if (m[8] && !m[2] && !m[3] && !m[4] && !m[5] && !m[6])
	{
		towardBase |= isTowardBaseVertical(x, y, +1);
	}

	if (towardBase)
	{
		// return base extension value

		return conf.ai_terraforming_networkBaseExtensionValue;

	}
	else
	{
		// return wild extension value

		return conf.ai_terraforming_networkWildExtensionValue;

	}

}

bool isTowardBaseDiagonal(int x, int y, int dxSign, int dySign)
{
	for (int dx = 0; dx <= 10; dx++)
	{
		for (int dy = 0; dy <= 10; dy++)
		{
			// cut the triangle

			if (dx + dy > 10)
				continue;

			// get map tile

			MAP *tile = getMapTile(x + dxSign * dx, y + dySign * dy);

			// skip impossible combinations

			if (!tile)
				continue;

			// check base in tile

			if (isFactionBaseAt(tile, aiFactionId))
			{
				return true;
			}

		}

	}

	return false;

}

bool isTowardBaseHorizontal(int x, int y, int dxSign)
{
	for (int dx = 0; dx <= 10; dx++)
	{
		for (int dy = -dx; dy <= dx; dy++)
		{
			// cut the square

			if (dx + dy > 10)
				continue;

			// get map tile

			MAP *tile = getMapTile(x + dxSign * dx, y + dy);

			// skip impossible combinations

			if (!tile)
				continue;

			// check base in tile

			if (isFactionBaseAt(tile, aiFactionId))
			{
				return true;
			}

		}

	}

	return false;

}

bool isTowardBaseVertical(int x, int y, int dySign)
{
	for (int dy = 0; dy <= 10; dy++)
	{
		for (int dx = -dy; dx <= dy; dx++)
		{
			// cut the square

			if (dx + dy > 10)
				continue;

			// get map tile

			MAP *tile = getMapTile(x + dx, y + dySign * dy);

			// skip impossible combinations

			if (!tile)
				continue;

			// check base in tile

			if (isFactionBaseAt(tile, aiFactionId))
			{
				return true;
			}

		}

	}

	return false;

}

/*
Calculates score for sensor actions
*/
double calculateSensorScore(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);

	// don't build if already built

	if (map_has_item(tile, BIT_SENSOR))
		return 0.0;

	// find borderRange

	int borderRange = *MapHalfX;

	for (MAP *rangeTile : getRangeTiles(tile, *MapHalfX - 1, false))
	{
		// detect not own tile

		if (rangeTile->owner != aiFactionId)
		{
			borderRange = getRange(tile, rangeTile);
			break;
		}

	}

	// do not compute shoreRange for now - too complicated

	int shoreRange = *MapHalfX;

	// calculate values

	double borderRangeValue = conf.ai_terraforming_sensorBorderRange / std::max(conf.ai_terraforming_sensorBorderRange, (double)borderRange);
	double shoreRangeValue = conf.ai_terraforming_sensorShoreRange / std::max(conf.ai_terraforming_sensorShoreRange, (double)shoreRange);

	double value = conf.ai_terraforming_sensorValue * std::max(borderRangeValue, shoreRangeValue);

	// increase value for coverning not yet covered base

	bool coveringBase = false;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);

		// within being constructing sensor range

		if (map_range(x, y, base->x, base->y) > 2)
			continue;

		// not yet covered

		if (isWithinFriendlySensorRange(base->faction_id, base->x, base->y))
			continue;

		coveringBase = true;
		break;

	}

	if (coveringBase)
	{
		value *= 1.5;
	}

	// return value

	return value;

}

/*
Checks whether this tile is worked by base.
*/
bool isBaseWorkedTile(BASE *base, int x, int y)
{
	int baseId = (base - Bases) / sizeof(Bases[0]);

	// check square is within base radius

	if (!isWithinBaseRadius(base->x, base->y, x, y))
		return false;

	// get tile

	MAP *tile = getMapTile(x, y);

	if (tile == NULL)
		return false;

	// iterate worked tiles

	for (MAP *workedTile : getBaseWorkedTiles(baseId))
	{
		if (tile == workedTile)
			return true;

	}

	return false;

}

/*
Evaluates how fit this improvement is at this place based on resources it DOES NOT use.
*/
double calculateFitnessScore(MAP *tile, int action, bool levelTerrain)
{
	int mapIndex = getMapIndexByPointer(tile);
	int x = getX(mapIndex);
	int y = getY(mapIndex);
	bool ocean = is_ocean(tile);
	TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);

	// summary effects

	double nutrientEffect = 0.0;
	double mineralEffect = 0.0;
	double energyEffect = 0.0;

	// collect tile values

	int rainfall = map_rainfall(tile);
	int rockiness = map_rockiness(tile);
	int elevation = map_elevation(tile);

	if (ocean)
	{
		switch (action)
		{
		case FORMER_SENSOR:
			{
				if (tileTerraformingInfo.workedBaseId != -1 || map_has_item(tile, BIT_MINE | BIT_SOLAR))
				{
					// not compatible with these items

					mineralEffect += -1.0;
					energyEffect += -3.0;

				}

			}
			break;

		case FORMER_MINE:
			{
				int minUnusedMineralTileCount = INT_MAX;

				for (int baseId : tileTerraformingInfo.workableBaseIds)
				{
					BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);

					minUnusedMineralTileCount = std::min(minUnusedMineralTileCount, baseTerraformingInfo.unusedMineralTileCount);

				}

				if (minUnusedMineralTileCount < INT_MAX)
				{
					double potentialMineralTileCoefficient = std::min(1.0, (double)minUnusedMineralTileCount / 4.0);

					nutrientEffect += potentialMineralTileCoefficient * -1.0;
					energyEffect += potentialMineralTileCoefficient * -3.0;

				}

			}
			break;

		}

	}
	else
	{
		// average levels

		double averageRainfall = 0.00;
		double averageRockiness = 0.00;
		double averageElevation = 0.00;

		switch(action)
		{
		case FORMER_MINE:

			// not used altitude

			energyEffect += -((double)elevation - averageElevation);

			break;

		case FORMER_SOLAR:

			// no fitness

			break;

		case FORMER_CONDENSER:

			// not used altitude

			energyEffect += -((double)elevation - averageElevation);

			break;

		case FORMER_ECH_MIRROR:

			// no fitness

			break;

		case FORMER_THERMAL_BORE:
		case FORMER_FOREST:
		case FORMER_PLANT_FUNGUS:

			// not used everything

			nutrientEffect += -((double)rainfall - averageRainfall);
			mineralEffect += -((double)rockiness - averageRockiness);
			energyEffect += -((double)elevation - averageElevation);

			break;

		case FORMER_SENSOR:

			if (map_has_item(tile, BIT_FOREST | BIT_FUNGUS | BIT_MONOLITH))
			{
				// compatible with these items
			}
			else
			{
				// not used everything

				nutrientEffect += -((double)rainfall - averageRainfall);
				mineralEffect += -((double)rockiness - averageRockiness);
				energyEffect += -((double)elevation - averageElevation);

				// penalty for river

				if (map_has_item(tile, BIT_RIVER))
				{
					energyEffect += -1.0;
				}

				// penalty for bonus

				switch (bonus_at(x, y))
				{
				case 1:
					nutrientEffect += -2.0;
					break;
				case 2:
					mineralEffect += -2.0;
					break;
				case 3:
					energyEffect += -2.0;
					break;
				}

			}

			break;

		}

		// levelTerrain

		if (levelTerrain && rockiness == 2)
		{
			// loss in minerals

			mineralEffect += -1.0;

		}

		// not having mine in mineral bonus tile

		if (action != FORMER_MINE && getMineralBonus(tile) > 0)
		{
			mineralEffect += -1.0;
		}

	}

	// compute total fitness score

	double fitnessScore = conf.ai_terraforming_fitnessMultiplier * getYieldScore(nutrientEffect, mineralEffect, energyEffect);

	debug
	(
		"\t\t%-20s=%6.3f:"
		" ocean=%d"
		" rainfall=%d"
		" rockiness=%d"
		" elevation=%d"
		" nutrientEffect=%f"
		" mineralEffect=%f"
		" energyEffect=%f"
		" multiplier=%f"
		"\n",
		"fitnessScore",
		fitnessScore,
		ocean,
		rainfall,
		rockiness,
		elevation,
		nutrientEffect,
		mineralEffect,
		energyEffect,
		conf.ai_terraforming_fitnessMultiplier

	);

	return fitnessScore;

}

bool hasNearbyTerraformingRequestAction(std::vector<TERRAFORMING_REQUEST>::iterator begin, std::vector<TERRAFORMING_REQUEST>::iterator end, int action, int x, int y, int range)
{
	for
	(
		std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestsIterator = begin;
		terraformingRequestsIterator != end;
		terraformingRequestsIterator++
	)
	{
		TERRAFORMING_REQUEST *terraformingRequest = &(*terraformingRequestsIterator);

		if (terraformingRequest->action == action)
		{
			if (map_range(x, y, getX(terraformingRequest->tile), getY(terraformingRequest->tile)) <= range)
			{
				return true;
			}

		}

	}

	return false;

}

/*
Estimates condenser yield score in adjacent tiles.
*/
double estimateCondenserExtraYieldScore(MAP *improvedTile, const std::vector<int> *actions)
{
	int mapIndex = getMapIndexByPointer(improvedTile);
	MAP *tile = getMapTile(mapIndex);

	// check whether condenser was built or destroyed

	bool condenserAction = false;

	for (int action : *actions)
	{
		if (action == FORMER_CONDENSER)
		{
			condenserAction = true;
			break;
		}
	}

	int sign;

	// condenser is built
	if (!map_has_item(improvedTile, BIT_CONDENSER) && condenserAction)
	{
		sign = +1;
	}
	// condenser is destroyed
	else if (map_has_item(improvedTile, BIT_CONDENSER) && !condenserAction)
	{
		sign = -1;
	}
	// neither built nor destroyed
	else
	{
		return 0.0;
	}

	// estimate rainfall improvement
	// [0.0, 9.0]

	double totalRainfallImprovement = 0.0;

	for (MAP *boxTile : getRangeTiles(tile, 1, true))
	{
		TileTerraformingInfo &boxTileTerraformingInfo = getTileTerraformingInfo(boxTile);

		// exclude non terraformable site

		if (!boxTileTerraformingInfo.availableTerraformingSite)
			continue;

		// exclude non conventional terraformable site

		if (!boxTileTerraformingInfo.availableConventionalTerraformingSite)
			continue;

		// exclude ocean - not affected by condenser

		if (is_ocean(boxTile))
			continue;

		// exclude great dunes - not affected by condenser

		if (map_has_landmark(boxTile, LM_DUNES))
			continue;

		// exclude borehole

		if (map_has_item(boxTile, BIT_THERMAL_BORE))
			continue;

		// exclude rocky areas

		if (map_rockiness(boxTile) == 2)
			continue;

		// condenser was there but removed
		if (sign == -1)
		{
			switch(map_rainfall(boxTile))
			{
			case 1:
				totalRainfallImprovement += 1.0;
				break;
			case 2:
				totalRainfallImprovement += 0.5;
				break;
			}

		}
		// condenser wasn't there and built
		else if (sign == +1)
		{
			switch(map_rainfall(boxTile))
			{
			case 0:
			case 1:
				totalRainfallImprovement += 1.0;
				break;
			}

		}
		// safety check
		else
		{
			return 0.0;
		}

	}

	// calculate bonus
	// rainfall bonus should be at least 2 for condenser to make sense

	double extraScore = sign * getYieldScore(totalRainfallImprovement - 2.0, 0.0, 0.0);

	debug("\t\tsign=%+1d, totalRainfallImprovement=%6.3f, extraScore=%+6.3f\n", sign, totalRainfallImprovement, extraScore);

	// return score

	return extraScore;

}

/*
Estimates aquifer additional yield score.
This is very approximate.
*/
double estimateAquiferExtraYieldScore(MAP *tile)
{
	int mapIndex = getMapIndexByPointer(tile);
	int x = getX(mapIndex);
	int y = getY(mapIndex);

	// evaluate elevation

	// [2, 8]
    double elevationFactor = 2.0 * (double)(map_elevation(tile) + 1);

    // evaluate nearby bases worked tiles coverage

    int totalTileCount = 0;
    int workableTileCount = 0;

    for (int dx = -4; dx <= +4; dx++)
	{
		for (int dy = -(4 - abs(dx)); dy <= +(4 - abs(dx)); dy += 2)
		{
			MAP *closeTile = getMapTile(x + dx, y + dy);
			if (!closeTile)
				continue;

			TileTerraformingInfo &closeTileTerraformingInfo = getTileTerraformingInfo(closeTile);

			totalTileCount++;

			// workable tile

			if (closeTileTerraformingInfo.workable)
				workableTileCount++;

		}

	}

	// safety exit

	if (totalTileCount == 0)
		return 0.0;

	// [0.0, 1.0]
	double workedTileCoverage = (double)workableTileCount / (double)totalTileCount;

	// evaluate placement

	// [0, 12]
	int borderRiverCount = 0;

	for (int dx = -4; dx <= +4; dx++)
	{
		for (int dy = -(4 - abs(dx)); dy <= +(4 - abs(dx)); dy += 2)
		{
			// iterate outer rim only

			if (abs(dx) + abs(dy) != 4)
				continue;

			// build tile

			MAP *closeTile = getMapTile(x + dx, y + dy);

			if (!closeTile)
				continue;

			// exclude base

			if (map_has_item(closeTile, BIT_BASE_IN_TILE))
				continue;

			// count rivers

			if (map_has_item(closeTile, BIT_RIVER))
			{
				borderRiverCount++;
			}

			// discount oceans

			if (is_ocean(closeTile))
			{
				borderRiverCount--;
			}

		}

    }

    // return weighted totalEnergyImprovement

    return conf.ai_terraforming_energyWeight * (workedTileCoverage * elevationFactor) + (double)borderRiverCount / 2.0;

}

/*
Estimates raise land additional yield score.
*/
double estimateRaiseLandExtraYieldScore(MAP *terrafromingTile, int cost)
{
	int x = getX(terrafromingTile);
	int y = getY(terrafromingTile);

	// land only

	if (is_ocean(terrafromingTile))
		return 0.0;

	// calculate final raised elevation and affected range

	int raisedElevation = map_elevation(terrafromingTile) + 1;
	int affectedRange = raisedElevation;

	// evaluate energy benefit

	// [0.0, 45.0]
	double totalEnergyImprovement = 0.0;

	for (int dx = -(2 * affectedRange); dx <= +(2 * affectedRange); dx++)
	{
		for (int dy = -(2 * affectedRange - abs(dx)); dy <= +(2 * affectedRange - abs(dx)); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (!tile)
				continue;

			TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);

			// exclude not workable tiles

			if (!tileTerraformingInfo.workable)
				continue;

			// exclude ocean

			if (is_ocean(tile))
				continue;

			// calculate range

			int range = map_range(x, y, wrap(x + dx), y + dy);

			// calculate elevation and expected elevation

			int elevation = map_elevation(tile);
			int expectedElevation = raisedElevation - range;

			// calculate energy collection improvement

			if (elevation < expectedElevation && map_has_item(tile, BIT_SOLAR | BIT_ECH_MIRROR))
			{
				double energyImprovement = (double)(expectedElevation - elevation);

				// halve if not worked

				if (!tileTerraformingInfo.worked)
				{
					energyImprovement /= 2.0;
				}

				// update total

				totalEnergyImprovement += energyImprovement;

			}

		}

    }

    // return weighted totalEnergyImprovement

    return 1.0 * conf.ai_terraforming_energyWeight * (totalEnergyImprovement - (double)cost / conf.ai_terraforming_raiseLandPayoffTime);

}

/*
Determines whether raising land does not disturb others.
*/
bool isRaiseLandSafe(MAP *raisedTile)
{
	Faction *faction = &(Factions[aiFactionId]);

	// determine affected range

	int maxRange = is_ocean(raisedTile) ? 0 : map_elevation(raisedTile) + 1;

	// check tiles in range

	for (MAP *tile : getRangeTiles(raisedTile, maxRange, true))
	{
		// check if we have treaty/pact with owner

		if
		(
			(tile->owner != aiFactionId)
			&&
			(faction->diplo_status[tile->owner] & (DIPLO_TRUCE | DIPLO_TREATY | DIPLO_PACT))
		)
		{
			return false;
		}

	}

	return true;

}

/*
Calculates combined weighted resource score taking base additional demand into account.
*/
double calculateBaseResourceScore(int baseId, int currentMineralIntake2, int currentNutrientSurplus, int currentMineralSurplus, int currentEnergySurplus, int improvedMineralIntake2, int improvedNutrientSurplus, int improvedMineralSurplus, int improvedEnergySurplus)
{
	BASE *base = &(Bases[baseId]);

	// improvedNutrientSurplus <= 0 is unacceptable

	if (improvedNutrientSurplus <= 0)
		return 0.0;

	// calculate number of workers

	int workerCount = 0;

	for (int index = 1; index < OFFSET_COUNT_RADIUS; index++)
	{
		if ((base->worked_tiles & (0x1 << index)) != 0)
		{
			workerCount++;
		}

	}

	// calculate nutrient and mineral extra score

	double nutrientCostMultiplier = 1.0;
	double mineralCostMultiplier = 1.0;
	double energyCostMultiplier = 1.0;

	// multipliers are for bases size 2 and above

	if (base->pop_size >= 2)
	{
		// calculate nutrient cost multiplier

		double nutrientThreshold = conf.ai_terraforming_baseNutrientThresholdRatio * (double)(workerCount + 1);

		int minNutrientSurplus = std::min(currentNutrientSurplus, improvedNutrientSurplus);

		if (minNutrientSurplus < nutrientThreshold)
		{
			double proportion = 1.0 - (double)minNutrientSurplus / nutrientThreshold;
			nutrientCostMultiplier += (conf.ai_terraforming_baseNutrientDemandMultiplier - 1.0) * proportion;
		}

		// calculate mineral cost multiplier

		double mineralThreshold = conf.ai_terraforming_baseMineralThresholdRatio * (double)minNutrientSurplus;

		int minMineralIntake2 = std::min(currentMineralIntake2, improvedMineralIntake2);

		if (minMineralIntake2 < mineralThreshold)
		{
			double proportion = 1.0 - (double)minMineralIntake2 / mineralThreshold;
			mineralCostMultiplier += (conf.ai_terraforming_baseMineralDemandMultiplier - 1.0) * proportion;
		}

		// calculate energy cost multiplier

		if (aiData.grossIncome <= 0 || aiData.netIncome <= 0)
		{
			energyCostMultiplier = 1.0;
		}
		else
		{
			double maxNetIncome = 0.5 * (double)aiData.grossIncome;
			double minNetIncome = 0.1 * maxNetIncome;
			energyCostMultiplier = maxNetIncome / std::min(maxNetIncome, std::max(minNetIncome, (double)aiData.netIncome));
		}

	}

	// compute final score

	return
		getYieldScore
		(
			nutrientCostMultiplier * (improvedNutrientSurplus - currentNutrientSurplus),
			mineralCostMultiplier * (improvedMineralSurplus - currentMineralSurplus),
			energyCostMultiplier * (improvedEnergySurplus - currentEnergySurplus)
		)
	;

}

/*
Calculates yield improvement score for given base and improved tile.
*/
double computeImprovementBaseSurplusEffectScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState)
{
	BASE *base = &(Bases[baseId]);

	bool worked = false;

	// set initial state

	*tile = *currentMapState;
	computeBase(baseId, true);

	// gather current surplus

	int currentMineralIntake2 = base->mineral_intake_2;
	int currentNutrientSurplus = base->nutrient_surplus;
	int currentMineralSurplus = base->mineral_surplus;
	int currentEnergySurplus = base->economy_total + base->psych_total + base->labs_total;

	// apply changes

	*tile = *improvedMapState;
	computeBase(baseId, true);

	// gather improved surplus

	int improvedMineralIntake2 = base->mineral_intake_2;
	int improvedNutrientSurplus = base->nutrient_surplus;
	int improvedMineralSurplus = base->mineral_surplus;
	int improvedEnergySurplus = base->economy_total + base->psych_total + base->labs_total;

	// verify tile is worked after improvement

	worked = isBaseWorkedTile(baseId, tile);

	// restore original state

	*tile = *currentMapState;
	computeBase(baseId, true);

	// return zero if not worked

	if (!worked)
		return 0.0;

	// return zero if no changes at all

	if (improvedNutrientSurplus == currentNutrientSurplus && improvedMineralSurplus == currentMineralSurplus && improvedEnergySurplus == currentEnergySurplus)
		return 0.0;

	// calculate improvement score

	double baseResourceScore =
		calculateBaseResourceScore
		(
			baseId,
			currentMineralIntake2,
			currentNutrientSurplus,
			currentMineralSurplus,
			currentEnergySurplus,
			improvedMineralIntake2,
			improvedNutrientSurplus,
			improvedMineralSurplus,
			improvedEnergySurplus
		)
	;

	// return value

	return baseResourceScore;

}

/*
Applies improvement and computes its maximal yield score for this base.
*/
double computeBaseImprovementYieldScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState)
{
	BASE *base = &(Bases[baseId]);
	int x = getX(tile);
	int y = getY(tile);

	// apply improved state

	*tile = *improvedMapState;

	// record yield

	int nutrient = mod_nutrient_yield(base->faction_id, baseId, x, y, 0);
	int mineral = mod_mineral_yield(base->faction_id, baseId, x, y, 0);
	int energy = mod_energy_yield(base->faction_id, baseId, x, y, 0);

	// restore original state

	*tile = *currentMapState;

	// compute yield score

	return getYieldScore(nutrient, mineral, energy);

}

/*
Checks that base can yield from this site.
*/
bool isWorkableTile(MAP *tile)
{
	if (tile == nullptr)
		return false;

	// exclude volcano mouth

	if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
		return false;

	// exclude claimed territory

	if (!(tile->owner == -1 || tile->owner == aiFactionId))
		return false;

	// exclude base

	if (map_has_item(tile, BIT_BASE_IN_TILE))
		return false;

	// exclude not base radius

	if (!map_has_item(tile, BIT_BASE_RADIUS))
		return false;

	// all conditions met

	return true;

}

/*
Checks that conventional improvement can be done at this site and base can work it.
conventional improvement = any yield improvement except river
*/
bool isValidConventionalTerraformingSite(MAP *tile)
{
	if (tile == nullptr)
		return false;

	// exclude not workable sites

	if (!isWorkableTile(tile))
		return false;

	// exclude monolith
	// it cannot be improved for yield

	if (map_has_item(tile, BIT_MONOLITH))
		return false;

	// all conditions met

	return true;

}

bool isValidTerraformingSite(MAP *tile)
{
	if (tile == nullptr)
		return false;

	TileInfo &tileInfo = aiData.getTileInfo(tile);

	// exclude volcano mouth

	if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
		return false;

	// exclude claimed territory

	if (!(tile->owner == -1 || tile->owner == aiFactionId))
		return false;

	// exclude base

	if (map_has_item(tile, BIT_BASE_IN_TILE))
		return false;

	// exclude blocked locations

	if (tileInfo.factionInfos[aiFactionId].blocked[0])
		return false;

	// all conditions met

	return true;

}

double getYieldScore(double nutrient, double mineral, double energy)
{
	return
		+ conf.ai_terraforming_nutrientWeight * nutrient
		+ conf.ai_terraforming_mineralWeight * mineral
		+ conf.ai_terraforming_energyWeight * energy
	;
}

double getNormalTerraformingIncomeGain()
{
	return getIncomeGain(0, getYieldScore(1.0, 1.0, 1.0), 0.0);
}

double getNormalizedTerraformingIncomeGain(int operationTime, double score)
{
	return getIncomeGain(operationTime, score, 0.0) / normalTerraformingIncomeGain;
}

/*
Returns 1 if first result is inferior, 2 if second result is inferior, 0 if they are equal, -1 otherwise.
*/
int getInferiorImprovement(TerraformingImprovement &improvement1, TerraformingImprovement &improvement2, bool mineralsOnly)
{
	int nutrient = improvement1.nutrient == improvement2.nutrient ? 0 : (improvement1.nutrient < improvement2.nutrient ? 1 : 2);
	int mineral = improvement1.mineral == improvement2.mineral ? 0 : (improvement1.mineral < improvement2.mineral ? 1 : 2);
	int energy = improvement1.energy == improvement2.energy ? 0 : (improvement1.energy < improvement2.energy ? 1 : 2);

	if (mineralsOnly)
	{
		nutrient = 0;
		energy = 0;
	}

	if (nutrient == 0 && mineral == 0 && energy == 0)
	{
		return 0;
	}
	else if (nutrient != 2 && mineral != 2 && energy != 2)
	{
		return 1;
	}
	else if (nutrient != 1 && mineral != 1 && energy != 1)
	{
		return 2;
	}
	else
	{
		return -1;
	}

}

void generateTerraformingChange(MAP *mapState, int action)
{
	// level terrain changes rockiness
	if (action == FORMER_LEVEL_TERRAIN)
	{
		switch (map_rockiness(mapState))
		{
		case 2:
			mapState->val3 &= ~TILE_ROCKY;
			mapState->val3 |= TILE_ROLLING;
			break;
		case 1:
			mapState->val3 &= ~TILE_ROLLING;
			break;
		}

	}
	// other actions change items
	else
	{
		// special cases
		if (action == FORMER_AQUIFER)
		{
			mapState->items |= (BIT_RIVER_SRC | BIT_RIVER);
		}
		// regular cases
		else
		{
			// remove items

			mapState->items &= ~Terraform[action].bit_incompatible;

			// add items

			mapState->items |= Terraform[action].bit;

		}

	}

}

