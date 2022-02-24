#include <float.h>
#include <math.h>
#include <vector>
#include <set>
#include <map>
#include <map>
#include "game.h"
#include "terranx_wtp.h"
#include "wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMove.h"
#include "aiMoveFormer.h"

// terraforming data

std::vector<TerraformingBaseInfo> terraformingBaseInfos;
std::vector<TerraformingTileInfo> terraformingTileInfos;

std::vector<MAP *> terraformingSites;
std::vector<MAP *> conventionalTerraformingSites;

double networkDemand = 0.0;
std::vector<FORMER_ORDER> formerOrders;
std::map<int, FORMER_ORDER *> vehicleFormerOrders;
std::vector<TERRAFORMING_REQUEST> terraformingRequests;

// terraforming data operations

void setupTerraformingData()
{
	// setup data
	
	terraformingBaseInfos.resize(MaxBaseNum, {});
	terraformingTileInfos.resize(*map_area_tiles, {});
	
}

void cleanupTerraformingData()
{
	// cleanup data
	
	terraformingBaseInfos.clear();
	terraformingTileInfos.clear();
	
	// clear lists

	terraformingSites.clear();
	conventionalTerraformingSites.clear();

	terraformingRequests.clear();
	formerOrders.clear();
	vehicleFormerOrders.clear();

}

// access terraforming data arrays

TerraformingBaseInfo *getTerraformingBaseInfo(int baseId)
{
	assert(baseId >= 0 && baseId < *total_num_bases);
	return &(terraformingBaseInfos.at(baseId));
}

TerraformingTileInfo *getTerraformingTileInfo(int mapIndex)
{
	assert(mapIndex >= 0 && mapIndex < *map_area_tiles);
	return &(terraformingTileInfos.at(mapIndex));
}
TerraformingTileInfo *getTerraformingTileInfo(int x, int y)
{
	return getTerraformingTileInfo(getMapIndexByCoordinates(x, y));
}
TerraformingTileInfo *getTerraformingTileInfo(MAP *tile)
{
	return getTerraformingTileInfo(getMapIndexByPointer(tile));
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
	
	setFormerProductionDemands();

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
	// populated terraformed tiles and former orders

	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &Vehicles[vehicleId];
		TerraformingTileInfo *vehicleTerraformingTileInfo = getTerraformingTileInfo(vehicle->x, vehicle->y);

		// process supplies and formers

		// supplies
		if (isSupplyVehicle(vehicle))
		{
			// convoying vehicles
			if (isVehicleConvoying(vehicle))
			{
				vehicleTerraformingTileInfo->harvested = true;
			}

		}
		// formers
		else if (isFormerVehicle(vehicle))
		{
			// terraforming vehicles
			if (isVehicleTerraforming(vehicle))
			{
				// set sync task
				
				setTask(vehicleId, Task(NONE, vehicleId));
				
				// terraformed flag
				
				vehicleTerraformingTileInfo->terraformed = true;
				
				// conventional terraformed flag
				
				if
				(
					vehicle->move_status == ORDER_FARM
					||
					vehicle->move_status == ORDER_SOIL_ENRICHER
					||
					vehicle->move_status == ORDER_MINE
					||
					vehicle->move_status == ORDER_SOLAR_COLLECTOR
					||
					vehicle->move_status == ORDER_CONDENSER
					||
					vehicle->move_status == ORDER_ECHELON_MIRROR
					||
					vehicle->move_status == ORDER_THERMAL_BOREHOLE
					||
					vehicle->move_status == ORDER_PLANT_FOREST
					||
					vehicle->move_status == ORDER_PLANT_FUNGUS
				)
				{
					vehicleTerraformingTileInfo->terraformedConventional = true;
				}

				// populate terraformed forest tiles

				if (vehicle->move_status == ORDER_PLANT_FOREST)
				{
					vehicleTerraformingTileInfo->terraformedForest = true;
				}

				// populate terraformed condenser tiles

				if (vehicle->move_status == ORDER_CONDENSER)
				{
					vehicleTerraformingTileInfo->terraformedCondenser = true;
				}

				// populate terraformed mirror tiles

				if (vehicle->move_status == ORDER_ECHELON_MIRROR)
				{
					vehicleTerraformingTileInfo->terraformedMirror = true;
				}

				// populate terraformed borehole tiles

				if (vehicle->move_status == ORDER_THERMAL_BOREHOLE)
				{
					vehicleTerraformingTileInfo->terraformedBorehole = true;
				}

				// populate terraformed aquifer tiles

				if (vehicle->move_status == ORDER_DRILL_AQUIFIER)
				{
					vehicleTerraformingTileInfo->terraformedAquifer = true;
				}

				// populate terraformed raise tiles

				if (vehicle->move_status == ORDER_TERRAFORM_UP)
				{
					vehicleTerraformingTileInfo->terraformedRaise = true;
				}

				// populate terraformed sensor tiles

				if (vehicle->move_status == ORDER_SENSOR_ARRAY)
				{
					vehicleTerraformingTileInfo->terraformedSensor = true;
				}

			}
			// available vehicles
			else
			{
				// ignore those in warzone

				if (aiData.getTileInfo(vehicle->x, vehicle->y)->warzone)
					continue;

				// exclude air terraformers - let AI deal with them itself

				if (veh_triad(vehicleId) == TRIAD_AIR)
					continue;

				// store vehicle current id in pad_0 field

				vehicle->pad_0 = vehicleId;

				// add vehicle
				
				formerOrders.push_back(FORMER_ORDER(vehicleId, vehicle));

			}

		}

	}

	// populate terraformingTileInfos
	// populate terraformingSites
	// populate conventionalTerraformingSites
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		TerraformingBaseInfo *terraformingBaseInfo = getTerraformingBaseInfo(baseId);
		
		// initialize base terraforming info
		
		terraformingBaseInfo->rockyLandTileCount = 0;

		// base radius tiles
		
		for (int index = BASE_OFFSET_COUNT_CENTER; index < BASE_OFFSET_COUNT_RADIUS; index++)
		{
			int x = wrap(base->x + BASE_TILE_OFFSETS[index][0]);
			int y = base->y + BASE_TILE_OFFSETS[index][1];
			
			if (!isOnMap(x, y))
				continue;
			
			MAP *tile = getMapTile(x, y);
			TerraformingTileInfo *terraformingTileInfo = getTerraformingTileInfo(x, y);
			
			// valid terraforming site
			// below settings are for valid terraforming site only
			
			if (!isValidTerraformingSite(tile))
				continue;
			
			terraformingTileInfo->availableTerraformingSite = true;
			
			// update worked base
			
			terraformingTileInfo->workedBaseId = ((base->worked_tiles & (0x1 << index)) != 0);
			
			// update workable bases
			
			terraformingTileInfo->workableBaseIds.push_back(baseId);
			
			// valid conventional terraforming sites
			// below settings are for valid conventional terraforming site only
			
			if (!isValidConventionalTerraformingSite(tile))
				continue;
			
			terraformingTileInfo->availableConventionalTerraformingSite = true;
			
			// update base rocky land tile count
			
			if (!is_ocean(tile) && map_rockiness(tile) == 2)
			{
				terraformingBaseInfo->rockyLandTileCount++;
			}
			
		}
		
		// base radius adjacent tiles
		
		for (int index = BASE_OFFSET_COUNT_RADIUS; index < BASE_OFFSET_COUNT_RADIUS_ADJACENT; index++)
		{
			int x = wrap(base->x + BASE_TILE_OFFSETS[index][0]);
			int y = base->y + BASE_TILE_OFFSETS[index][1];
			
			if (!isOnMap(x, y))
				continue;
			
			MAP *tile = getMapTile(x, y);
			TerraformingTileInfo *terraformingTileInfo = getTerraformingTileInfo(x, y);
			
			// valid terraforming site
			
			if (!isValidTerraformingSite(tile))
				continue;
			
			terraformingTileInfo->availableTerraformingSite = true;
			
			// update area workable bases
			
			terraformingTileInfo->areaWorkableBaseIds.push_back(baseId);
			
		}
		
	}
	
	// calculate network demand

	int networkType = (has_tech(aiFactionId, Terraform[FORMER_MAGTUBE].preq_tech) ? TERRA_MAGTUBE : TERRA_ROAD);

	double networkCoverage = 0.0;

	for (int i = 0; i < *map_area_tiles; i++)
	{
		// build tile

		MAP *tile = &((*MapPtr)[i]);

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
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		
		if (terraformingTileInfos.at(mapIndex).availableTerraformingSite)
		{
			terraformingSites.push_back(tile);
		}
		
		if (terraformingTileInfos.at(mapIndex).availableConventionalTerraformingSite)
		{
			conventionalTerraformingSites.push_back(tile);
		}
		
	}
	
	// populate base conventionalTerraformingSites
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		TerraformingBaseInfo *terraformingBaseInfo = getTerraformingBaseInfo(baseId);
		
		for (MAP *baseRadiusTile : getBaseRadiusTiles(baseTile, false))
		{
			TerraformingTileInfo *baseRadiusTerraformingTileInfo = getTerraformingTileInfo(baseRadiusTile);
			
			// conventional terraforming site
			
			if (!baseRadiusTerraformingTileInfo->availableConventionalTerraformingSite)
				continue;
			
			// add to base tiles
			
			terraformingBaseInfo->conventionalTerraformingSites.push_back(baseRadiusTile);
			
		}
		
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

	// select allowed improvement locations

	selectConventionalTerraformingLocations();

	for (MAP *tile : conventionalTerraformingSites)
	{
		// generate request

		generateConventionalTerraformingRequest(tile);

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

}

void selectConventionalTerraformingLocations()
{
	for (int baseId : aiData.baseIds)
	{
		selectRockyMineLocation(baseId);
		selectBoreholeLocation(baseId);
		selectForestLocation(baseId);
		selectFungusLocation(baseId);
	}
	
}

void selectRockyMineLocation(int baseId)
{
	TerraformingBaseInfo *terraformingBaseInfo = getTerraformingBaseInfo(baseId);
	
	MAP *bestTile = nullptr;
	double bestScore;
	
	for (MAP *tile : terraformingBaseInfo->conventionalTerraformingSites)
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
	
	getTerraformingTileInfo(bestTile)->rockyMineAllowed = true;
	
}

void selectBoreholeLocation(int baseId)
{
	TerraformingBaseInfo *terraformingBaseInfo = getTerraformingBaseInfo(baseId);
	
	MAP *bestTile = nullptr;
	double bestScore;
	
	for (MAP *tile : terraformingBaseInfo->conventionalTerraformingSites)
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
	
	getTerraformingTileInfo(bestTile)->boreholeAllowed = true;
	
}

void selectForestLocation(int baseId)
{
	TerraformingBaseInfo *terraformingBaseInfo = getTerraformingBaseInfo(baseId);
	
	MAP *bestTile = nullptr;
	double bestScore;
	
	for (MAP *tile : terraformingBaseInfo->conventionalTerraformingSites)
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
	
	getTerraformingTileInfo(bestTile)->forestAllowed = true;
	
}

void selectFungusLocation(int baseId)
{
	TerraformingBaseInfo *terraformingBaseInfo = getTerraformingBaseInfo(baseId);
	
	MAP *bestTile = nullptr;
	double bestScore;
	
	for (MAP *tile : terraformingBaseInfo->conventionalTerraformingSites)
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
	
	getTerraformingTileInfo(bestTile)->fungusAllowed = true;
	
}

/*
Generate request for conventional incompatible terraforming options.
*/
void generateConventionalTerraformingRequest(MAP *tile)
{
	TERRAFORMING_SCORE terraformingScore;
	calculateConventionalTerraformingScore(tile, &terraformingScore);

	// terraforming request should have option, action and score set

	if (terraformingScore.option == NULL || terraformingScore.action == -1 || terraformingScore.score <= 0.0)
		return;

	// add terraforming request

	terraformingRequests.push_back({tile, terraformingScore.option, terraformingScore.action, terraformingScore.score});

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
		terraformingRequests.push_back({tile, terraformingScore->option, terraformingScore->action, terraformingScore->score});
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
		terraformingRequests.push_back({tile, terraformingScore->option, terraformingScore->action, terraformingScore->score});
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
		terraformingRequests.push_back({tile, terraformingScore->option, terraformingScore->action, terraformingScore->score});
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
		terraformingRequests.push_back({tile, terraformingScore->option, terraformingScore->action, terraformingScore->score});
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
			debug("\t(%3d,%3d) %-20s %6.3f\n", getX(terraformingRequest.tile), getY(terraformingRequest.tile), terraformingRequest.option->name, terraformingRequest.score);
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

		std::map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(terraformingRequest->action);
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

void setFormerProductionDemands()
{
	debug("setFormerProductionDemands\n");
	
	aiData.production.landFormerDemand = 0.0;
	
	for (TERRAFORMING_REQUEST &terraformingRequest : terraformingRequests)
	{
		if (!is_ocean(terraformingRequest.tile))
		{
			aiData.production.landFormerDemand = std::max(aiData.production.landFormerDemand, terraformingRequest.score);
		}
		else
		{
			int oceanAssociation = getOceanAssociation(terraformingRequest.tile, aiFactionId);
			
			if (oceanAssociation == -1)
				continue;
			
			if (aiData.production.seaFormerDemands.count(oceanAssociation) == 0)
			{
				aiData.production.seaFormerDemands.insert({oceanAssociation, 0.0});
			}
			aiData.production.seaFormerDemands.at(oceanAssociation) = std::max(aiData.production.seaFormerDemands.at(oceanAssociation), terraformingRequest.score);
			
		}
		
	}
	
}

void assignFormerOrders()
{
	// clear production demand
	
	aiData.production.landFormerDemand = 0.0;
	
	// distribute orders
	
	for (TERRAFORMING_REQUEST &terraformingRequest : terraformingRequests)
	{
		int x = getX(terraformingRequest.tile);
		int y = getY(terraformingRequest.tile);
		MAP *tile = getMapTile(x, y);
		
		// find nearest available former
		
		FORMER_ORDER *nearestFormerOrder = nullptr;
		int minRange = INT_MAX;
		
		for (FORMER_ORDER &formerOrder : formerOrders)
		{
			int vehicleId = formerOrder.id;
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			// skip assigned
			
			if (formerOrder.action != -1)
				continue;
			
			// proper triad
			
			if (!(vehicle->triad() == TRIAD_AIR || vehicle->triad() == is_ocean(tile)))
				continue;
			
			// reachable
			
			if (!isDestinationReachable(vehicleId, x, y, false))
				continue;
			
			// get range
			
			int range = map_range(vehicle->x, vehicle->y, x, y);
			
			// update best
			
			if (range < minRange)
			{
				nearestFormerOrder = &(formerOrder);
				minRange = range;
			}
			
		}
		
		// former found - set task
		if (nearestFormerOrder != nullptr)
		{
			// set task
			
			nearestFormerOrder->x = getX(terraformingRequest.tile);
			nearestFormerOrder->y = getY(terraformingRequest.tile);
			nearestFormerOrder->action = terraformingRequest.action;
			
		}
		// former not found - update production demand
		else
		{
			if (!is_ocean(terraformingRequest.tile))
			{
				aiData.production.landFormerDemand = std::max(aiData.production.landFormerDemand, terraformingRequest.score);
			}
			else
			{
				int oceanAssociation = getOceanAssociation(terraformingRequest.tile, aiFactionId);
				
				if (oceanAssociation == -1)
					continue;
				
				if (aiData.production.seaFormerDemands.count(oceanAssociation) == 0)
				{
					aiData.production.seaFormerDemands.insert({oceanAssociation, 0.0});
				}
				aiData.production.seaFormerDemands.at(oceanAssociation) = std::max(aiData.production.seaFormerDemands.at(oceanAssociation), terraformingRequest.score);
				
			}
			
		}
		
	}
	
	// kill supported formers without orders
	
	for (FORMER_ORDER &formerOrder : formerOrders)
	{
		int vehicleId = formerOrder.id;
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// supported
		
		if (vehicle->home_base_id < 0)
			continue;
		
		// disband former without order
		
		if (formerOrder.action == -1)
		{
			setTask(vehicleId, Task(KILL, vehicleId));
		}
		
	}
	
	// normalize production demand
	
	double normalYieldScore = conf.ai_terraforming_nutrientWeight + conf.ai_terraforming_mineralWeight + conf.ai_terraforming_energyWeight;
	
	aiData.production.landFormerDemand /= normalYieldScore;
	
	for (auto &seaFormerDemandEntry : aiData.production.seaFormerDemands)
	{
		seaFormerDemandEntry.second /= normalYieldScore;
	}
	
}

void finalizeFormerOrders()
{
	// iterate former orders

	for (FORMER_ORDER &formerOrder : formerOrders)
	{
		transitVehicle(formerOrder.id, Task(TERRAFORMING, formerOrder.id, getMapTile(formerOrder.x, formerOrder.y), -1, -1, formerOrder.action));
	}

}

/*
Selects best terraforming option around given base and calculates its terraforming score.
*/
void calculateConventionalTerraformingScore(MAP *tile, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = getX(tile);
	int y = getY(tile);
	TerraformingTileInfo *terraformingTileInfo = getTerraformingTileInfo(tile);
	bool ocean = is_ocean(tile);
	bool fungus = map_has_item(tile, TERRA_FUNGUS);
	bool rocky = (map_rockiness(tile) == 2);

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(tile);

	// store tile values

	MAP_STATE currentMapState;
	getMapState(tile, &currentMapState);

	// find best terraforming option

	const TERRAFORMING_OPTION *bestOption = nullptr;
	int bestOptionFirstAction = -1;
	double bestOptionTerraformingScore = 0.0;

	for (const TERRAFORMING_OPTION &option : CONVENTIONAL_TERRAFORMING_OPTIONS)
	{
		// process only correct region type

		if (option.ocean != ocean)
			continue;

		// process only correct rockiness

		if (option.rocky && (!ocean && !rocky))
			continue;

		// check if required action technology is available when required

		if (option.requiredAction != -1 && !isTerraformingAvailable(tile, option.requiredAction))
			continue;
		
		debug("\t%s\n", option.name);
		
		// check if required action is allowed
		
		double fitnessScore = 0.0;
		
		if (option.requiredAction == FORMER_MINE && option.rocky)
		{
			if (!terraformingTileInfo->rockyMineAllowed)
			{
				debug("\tinferior placement\n");
				continue;
			}
		}
		else if (option.requiredAction == FORMER_THERMAL_BORE)
		{
			if (!terraformingTileInfo->boreholeAllowed)
			{
				debug("\tinferior placement\n");
				continue;
			}
		}
		else if (option.requiredAction == FORMER_FOREST)
		{
			if (!terraformingTileInfo->forestAllowed)
			{
				debug("\tinferior placement\n");
				continue;
			}
		}
		else if (option.requiredAction == FORMER_PLANT_FUNGUS)
		{
			if (!terraformingTileInfo->fungusAllowed)
			{
				debug("\tinferior placement\n");
				continue;
			}
		}
		else
		{
			fitnessScore = calculateFitnessScore(tile, option.requiredAction, (map_rockiness(tile) == 2 ? true : false));
		}
		
		// initialize variables

		MAP_STATE improvedMapState;
		copyMapState(&improvedMapState, &currentMapState);
		
		int firstAction = -1;
		int terraformingTime = 0;
		double penaltyScore = 0.0;
		double improvementScore = 0.0;
		bool levelTerrain = false;

		// process actions
		
		int availableActionCount = 0;
		int completedActionCount = 0;
		
		for(int action : option.actions)
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

			if (fungus && isRemoveFungusRequired(action))
			{
				int removeFungusAction = FORMER_REMOVE_FUNGUS;

				// store first action

				if (firstAction == -1)
				{
					firstAction = removeFungusAction;
				}

				// compute terraforming time

				terraformingTime +=
					getDestructionAdjustedTerraformingTime
					(
						removeFungusAction,
						improvedMapState.items,
						improvedMapState.rocks,
						nullptr
					)
				;

				// generate terraforming change

				generateTerraformingChange(&improvedMapState, removeFungusAction);

				// add penalty for nearby forest/kelp

				int nearbyForestKelpCount = nearby_items(x, y, 1, (ocean ? TERRA_FARM : TERRA_FOREST));
				penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;

			}

			// level terrain if needed

			if (!ocean && rocky && isLevelTerrainRequired(ocean, action))
			{
				levelTerrain = true;
				
				int levelTerrainAction = FORMER_LEVEL_TERRAIN;
				
				// store first action

				if (firstAction == -1)
				{
					firstAction = levelTerrainAction;
				}

				// compute terraforming time

				terraformingTime +=
					getDestructionAdjustedTerraformingTime
					(
						levelTerrainAction,
						improvedMapState.items,
						improvedMapState.rocks,
						nullptr
					)
				;

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
					improvedMapState.rocks,
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
					int nearbyKelpCount = nearby_items(x, y, 1, TERRA_FARM);
					penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyKelpCount;
				}

			}
			else
			{
				// forest

				if (action == FORMER_FOREST)
				{
					int nearbyForestCount = nearby_items(x, y, 1, TERRA_FOREST);
					penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestCount;
				}

			}

		}

		// ignore unpopulated options

		if (firstAction == -1 || terraformingTime == 0)
			continue;

		// calculate yield improvement score

		double surplusEffectScore = computeImprovementSurplusEffectScore(tile, &currentMapState, &improvedMapState);
		
		// ignore options not increasing yield

		if (surplusEffectScore <= 0.0)
			continue;
		
		// special yield computation for area effects

		double condenserExtraYieldScore = estimateCondenserExtraYieldScore(tile, &(option.actions));

		// completion bonus is given if improvement set is partially completed and no improvement will be destroyed in the process
		
		double completionScore;
		
		if
		(
			completedActionCount > 0
			&&
			// no leveling
			!levelTerrain
			&&
			// current state (except fungus) is a subset of improved state
			((currentMapState.items & ~TERRA_FUNGUS) & improvedMapState.items) == (currentMapState.items & ~TERRA_FUNGUS)
		)
		{
			completionScore = conf.ai_terraforming_completion_bonus;
		}
		else
		{
			completionScore = 0.0;
		}
		
		// summarize score

		improvementScore = penaltyScore + surplusEffectScore + condenserExtraYieldScore + fitnessScore + completionScore;
		
		// calculate terraforming score

		double terraformingScore = improvementScore * pow(0.5, ((double)terraformingTime + conf.ai_terraforming_travel_time_multiplier * travelTime) / conf.ai_terraforming_time_scale);


		debug
		(
			"\t\t%-20s=%6.3f : penalty=%6.3f, surplusEffect=%6.3f, condenser=%6.3f, fitness=%6.3f, completion=%6.3f, combined=%6.3f, time=%2d, travel=%f\n",
			"terraformingScore",
			terraformingScore,
			penaltyScore,
			surplusEffectScore,
			condenserExtraYieldScore,
			fitnessScore,
			completionScore,
			improvementScore,
			terraformingTime,
			travelTime
		)
		;

		// update score

		if (bestOption == NULL || terraformingScore > bestOptionTerraformingScore)
		{
			bestOption = &(option);
			bestOptionFirstAction = firstAction;
			bestOptionTerraformingScore = terraformingScore;
		}

	}

	// no option selected

	if (bestOption == NULL)
		return;

	// update return values

	bestTerraformingScore->option = bestOption;
	bestTerraformingScore->action = bestOptionFirstAction;
	bestTerraformingScore->score = bestOptionTerraformingScore;

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

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(tile);

	// get option

	const TERRAFORMING_OPTION *option = &(AQUIFER_TERRAFORMING_OPTION);

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

	// calculate terraforming score

	double terraformingScore = improvementScore * pow(0.5, ((double)terraformingTime + conf.ai_terraforming_travel_time_multiplier * travelTime) / conf.ai_terraforming_time_scale);

	debug
	(
		"\t%-10s: combined=%6.3f, time=%2d, travel=%f, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		travelTime,
		terraformingScore
	)
	;

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s: score=%6.3f, terraformingTime=%2d, travelTime=%f, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		travelTime,
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

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(tile);

	// get option

	const TERRAFORMING_OPTION *option = &(RAISE_LAND_TERRAFORMING_OPTION);

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

	// calculate terraforming score

	double terraformingScore = improvementScore * pow(0.5, ((double)terraformingTime + conf.ai_terraforming_travel_time_multiplier * travelTime) / conf.ai_terraforming_time_scale);

	debug
	(
		"\t%-10s: combined=%6.3f, time=%2d, travel=%f, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		travelTime,
		terraformingScore
	)
	;

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s: score=%6.3f, terraformingTime=%2d, travelTime=%f, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		travelTime,
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

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(tile);

	// get option

	const TERRAFORMING_OPTION *option = &(NETWORK_TERRAFORMING_OPTION);

	// initialize variables

	int firstAction = -1;
	int terraformingTime = 0;
	double improvementScore = 0.0;

	// process actions

	int action;

	if (!map_has_item(tile, TERRA_ROAD))
	{
		action = FORMER_ROAD;
	}
	else if (!map_has_item(tile, TERRA_MAGTUBE))
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

	if (map_has_item(tile, TERRA_FUNGUS) && isRemoveFungusRequired(action))
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

		int nearbyForestKelpCount = nearby_items(x, y, 1, (ocean ? TERRA_FARM : TERRA_FOREST));
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

	// calculate terraforming score

	double terraformingScore = improvementScore * pow(0.5, ((double)terraformingTime + conf.ai_terraforming_travel_time_multiplier * travelTime) / conf.ai_terraforming_time_scale);

	debug
	(
		"\t%-10s: combined=%6.3f, time=%2d, travel=%f, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		travelTime,
		terraformingScore
	)
	;

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s: score=%6.3f, terraformingTime=%2d, travelTime=%f, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		travelTime,
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

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(tile);

	// get option

	const TERRAFORMING_OPTION *option = (ocean ? &(OCEAN_SENSOR_TERRAFORMING_OPTION) : &(LAND_SENSOR_TERRAFORMING_OPTION));

	// initialize variables

	int firstAction = -1;
	int terraformingTime = 0;
	double improvementScore = 0.0;

	// process actions

	int action;

	if (!map_has_item(tile, TERRA_SENSOR))
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

	if (map_has_item(tile, TERRA_FUNGUS) && isRemoveFungusRequired(action))
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

		int nearbyForestKelpCount = nearby_items(x, y, 1, (ocean ? TERRA_FARM : TERRA_FOREST));
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

	double sensorScore = calculateSensorScore(tile, action);

	// calculate exclusivity bonus

	double fitnessScore = calculateFitnessScore(tile, option->requiredAction, false);

	// summarize score

	improvementScore = sensorScore + fitnessScore;

	// do not create request for zero score

	if (improvementScore <= 0.0)
		return;

	// calculate terraforming score

	double terraformingScore = improvementScore * pow(0.5, ((double)terraformingTime + conf.ai_terraforming_travel_time_multiplier * travelTime) / conf.ai_terraforming_time_scale);

	debug
	(
		"\t%-10s: combined=%6.3f, time=%2d, travel=%f, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		travelTime,
		terraformingScore
	)
	;

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s: sensorScore=%6.3f, fitnessScore=%6.3f, improvementScore=%6.3f, terraformingTime=%2d, travelTime=%f, final=%6.3f\n",
		option->name,
		sensorScore,
		fitnessScore,
		improvementScore,
		terraformingTime,
		travelTime,
		terraformingScore
	)
	;

}

/*
Computes workable bases surplus effect.
*/
double computeImprovementSurplusEffectScore(MAP *tile, MAP_STATE *currentMapState, MAP_STATE *improvedMapState)
{
	int mapIndex = getMapIndexByPointer(tile);
	TerraformingTileInfo *terraformingTileInfo = getTerraformingTileInfo(mapIndex);
	
	double resultScore = 0.0;

	// calculate affected range
	// building or destroying mirror requires recomputation of all bases in range 1

	if
	(
		((currentMapState->items & TERRA_ECH_MIRROR) != 0 && (improvedMapState->items & TERRA_ECH_MIRROR) == 0)
		||
		((currentMapState->items & TERRA_ECH_MIRROR) == 0 && (improvedMapState->items & TERRA_ECH_MIRROR) != 0)
	)
	{
		// compute score
		// summarize all scores
		
		for (int baseId : terraformingTileInfo->areaWorkableBaseIds)
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
		
		for (int baseId : terraformingTileInfo->workableBaseIds)
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

	if (!(vehicle->move_status >= ORDER_FARM && vehicle->move_status <= ORDER_PLACE_MONOLITH))
		return false;

	MAP *tile = getMapTile(vehicle->x, vehicle->y);

	return (tile->items & Terraform[vehicle->move_status - ORDER_FARM].bit);

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

	bool aquatic = MFactions[aiFactionId].rule_flags & FACT_AQUATIC;

	// action is considered available if is already built

	if (map_has_item(tile, Terraform[action].bit))
		return true;

	// action is available only when enabled and researched

	if (!has_terra(aiFactionId, action, ocean))
		return false;

	// terraforming is not available in base square

	if (tile->items & TERRA_BASE_IN_TILE)
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
		if (~tile->items & TERRA_FARM)
			return  false;
		break;
	case FORMER_FOREST:
		// forest should not be built near another building forest
		if (isNearbyForestUnderConstruction(x, y))
			return  false;
		break;
	case FORMER_MAGTUBE:
		// tube requires road
		if (~tile->items & TERRA_ROAD)
			return  false;
		break;
	case FORMER_REMOVE_FUNGUS:
		// removing fungus requires fungus
		if (~tile->items & TERRA_FUNGUS)
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
	return vehicle->move_status == ORDER_CONVOY;
}

bool isVehicleTerraforming(VEH *vehicle)
{
	return vehicle->move_status >= ORDER_FARM && vehicle->move_status <= ORDER_PLACE_MONOLITH;
}

bool isNearbyForestUnderConstruction(int x, int y)
{
	std::map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_FOREST);

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

			if (getTerraformingTileInfo(tile)->terraformedForest)
                return true;

        }

    }

    return false;

}

bool isNearbyCondeserUnderConstruction(int x, int y)
{
	std::map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_CONDENSER);

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

			if (getTerraformingTileInfo(tile)->terraformedCondenser)
                return true;

        }

    }

    return false;

}

bool isNearbyMirrorUnderConstruction(int x, int y)
{
	std::map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_ECH_MIRROR);

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

			if (getTerraformingTileInfo(tile)->terraformedMirror)
			{
				debug("terraformedMirrorTiles found: (%3d,%3d)\n", wrap(x + dx), y + dy)
                return true;
			}

        }

    }

    return false;

}

bool isNearbyBoreholePresentOrUnderConstruction(int x, int y)
{
	std::map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_THERMAL_BORE);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check existing items

	if (nearby_items(x, y, proximityRule->existingDistance, TERRA_THERMAL_BORE) >= 1)
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

			if (getTerraformingTileInfo(tile)->terraformedBorehole)
                return true;

        }

    }

    return false;

}

bool isNearbyRiverPresentOrUnderConstruction(int x, int y)
{
	std::map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_AQUIFER);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check existing items

	if (nearby_items(x, y, proximityRule->existingDistance, TERRA_RIVER) >= 1)
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

			if (getTerraformingTileInfo(tile)->terraformedAquifer)
                return true;

        }

    }

    return false;

}

bool isNearbyRaiseUnderConstruction(int x, int y)
{
	std::map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_RAISE_LAND);

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

			if (getTerraformingTileInfo(tile)->terraformedRaise)
                return true;

        }

    }

    return false;

}

bool isNearbySensorPresentOrUnderConstruction(int x, int y)
{
	std::map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_SENSOR);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check existing items

	if (nearby_items(x, y, proximityRule->existingDistance, TERRA_SENSOR) >= 1)
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

			if (getTerraformingTileInfo(tile)->terraformedSensor)
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

		if (items & TERRA_FOREST)
		{
			terraformingTime += 2;
		}
		else
		{
			// fungus increases road construction time

			if (items & TERRA_FUNGUS)
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

		if (items & TERRA_RIVER)
		{
			terraformingTime += 1;
		}

	}

	// adjust for faction projects

	if (has_project(aiFactionId, FAC_WEATHER_PARADIGM))
	{
		// reduce all terraforming time except fungus by 1/3

		if (action != FORMER_REMOVE_FUNGUS && action != FORMER_PLANT_FUNGUS)
		{
			terraformingTime -= terraformingTime / 3;
		}

	}

	if (has_project(aiFactionId, FAC_XENOEMPATHY_DOME))
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
		if ((items & (TERRA_SOLAR | TERRA_CONDENSER | TERRA_ECH_MIRROR | TERRA_THERMAL_BORE)) != 0)
		{
			terraformingTime += Terraform[FORMER_MINE].rate;
		}
		break;
		
	case FORMER_SOLAR:
		// no penalty for replacing collector<->mirror
		if ((items & (TERRA_MINE | TERRA_CONDENSER | TERRA_THERMAL_BORE)) != 0)
		{
			terraformingTime += Terraform[FORMER_SOLAR].rate;
		}
		break;
		
	case FORMER_CONDENSER:
		if ((items & (TERRA_MINE | TERRA_SOLAR | TERRA_ECH_MIRROR | TERRA_THERMAL_BORE)) != 0)
		{
			terraformingTime += Terraform[FORMER_CONDENSER].rate;
		}
		break;
		
	case FORMER_ECH_MIRROR:
		// no penalty for replacing collector<->mirror
		if ((items & (TERRA_MINE | TERRA_CONDENSER | TERRA_THERMAL_BORE)) != 0)
		{
			terraformingTime += Terraform[FORMER_ECH_MIRROR].rate;
		}
		break;
		
	case FORMER_THERMAL_BORE:
		if ((items & (TERRA_MINE | TERRA_SOLAR | TERRA_CONDENSER | TERRA_ECH_MIRROR)) != 0)
		{
			terraformingTime += Terraform[FORMER_THERMAL_BORE].rate;
		}
		break;
		
	}
	
	return terraformingTime;
	
}

double getSmallestAvailableFormerTravelTime(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	
	bool updated = false;
	double smallestTravelTime = 0;
	
	if (isLandRegion(tile->region))
	{
		// iterate land former in all regions
		
		for (FORMER_ORDER formerOrder : formerOrders)
		{
			int vehicleId = formerOrder.id;
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// land former
			
			if (vehicle->triad() != TRIAD_LAND)
				continue;
			
			// calculate time
			
			int range = map_range(vehicle->x, vehicle->y, x, y);
			int speed = veh_chassis_speed(vehicleId);
			
			double time = (double)range / (double)speed;
			
			// penalize time for different regions
			
			if (vehicleTile->region != tile->region)
			{
				time *= 2;
			}
			
			// update the best
			
			if (!updated || time < smallestTravelTime)
			{
				smallestTravelTime = time;
				updated = true;
			}

		}

	}
	else
	{
		// iterate sea former able to serve this tile
		
		for (FORMER_ORDER formerOrder : formerOrders)
		{
			int vehicleId = formerOrder.id;
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// sea former
			
			if (vehicle->triad() != TRIAD_SEA)
				continue;
			
			// serving this tile
			
			int vehicleOceanAssociation = getOceanAssociation(vehicleTile, vehicle->faction_id);
			int tileOceanAssociation = getOceanAssociation(tile, vehicle->faction_id);
			
			if (tileOceanAssociation != vehicleOceanAssociation)
				continue;
			
			// calculate time
			
			int range = map_range(vehicle->x, vehicle->y, x, y);
			int speed = veh_chassis_speed(vehicleId);
			
			double time = (double)range / (double)speed;

			// update the best

			if (!updated || time < smallestTravelTime)
			{
				smallestTravelTime = time;
				updated = true;
			}

		}

	}
	
	return smallestTravelTime;

}

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
		(action == FORMER_ROAD && (tile->items & TERRA_ROAD))
		||
		(action == FORMER_MAGTUBE && ((~tile->items & TERRA_ROAD) || (tile->items & TERRA_MAGTUBE)))
	)
		return 0.0;

	// get terrain improvement flag

	int improvementFlag;

	switch (action)
	{
	case FORMER_ROAD:
		improvementFlag = TERRA_ROAD;
		break;
	case FORMER_MAGTUBE:
		improvementFlag = TERRA_MAGTUBE;
		break;
	default:
		return 0.0;
	}

	// check items around the tile

	bool m[BASE_OFFSET_COUNT_ADJACENT];

	for (int offsetIndex = 1; offsetIndex < BASE_OFFSET_COUNT_ADJACENT; offsetIndex++)
	{
		MAP *adjacentTile = getMapTile(wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]), y + BASE_TILE_OFFSETS[offsetIndex][1]);

		m[offsetIndex] = (adjacentTile && !is_ocean(adjacentTile) && (adjacentTile->items & (TERRA_BASE_IN_TILE | improvementFlag)));

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

			if (aiData.baseTiles.count(tile) != 0)
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

			if (aiData.baseTiles.count(tile) != 0)
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

			if (aiData.baseTiles.count(tile) != 0)
			{
				return true;
			}

		}

	}

	return false;

}

/*
Calculates score for sensor actions (road, tube)
*/
double calculateSensorScore(MAP *tile, int action)
{
	int x = getX(tile);
	int y = getX(tile);

	// ignore anything but sensors

	if (!(action == FORMER_SENSOR))
		return 0.0;

	// don't build if already built

	if (map_has_item(tile, TERRA_SENSOR))
		return 0.0;

	// compute distance from border

	int borderRange = 9999;

	for (int otherX = 0; otherX < *map_axis_x; otherX++)
	{
		for (int otherY = 0; otherY < *map_axis_y; otherY++)
		{
			MAP *otherTile = getMapTile(otherX, otherY);

			if (otherTile == NULL)
				continue;

			// only same region

			if (otherTile->region != tile->region)
				continue;

			// not own

			if (otherTile->owner == aiFactionId)
				continue;

			// calculate range

			int range = map_range(x, y, otherX, otherY);

			// update range

			borderRange = std::min(borderRange, range);

		}

	}

	// compute distance from shore for land region

	int shoreRange = 9999;

	if (!isOceanRegion(tile->region))
	{
		for (int otherX = 0; otherX < *map_axis_x; otherX++)
		{
			for (int otherY = 0; otherY < *map_axis_y; otherY++)
			{
				MAP *otherTile = getMapTile(otherX, otherY);

				if (otherTile == NULL)
					continue;

				// ocean only

				if (!isOceanRegion(otherTile->region))
					continue;

				// calculate range

				int range = map_range(x, y, otherX, otherY);

				// update range

				shoreRange = std::min(shoreRange, range);

			}

		}

	}

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
		value *= 2;
	}
	
	// return value
	
	return value;

}

/*
Checks whether this tile is worked by base.
*/
bool isBaseWorkedTile(BASE *base, int x, int y)
{
	// check square is within base radius

	if (!isWithinBaseRadius(base->x, base->y, x, y))
		return false;

	// get tile

	MAP *tile = getMapTile(x, y);

	if (tile == NULL)
		return false;

	// iterate worked tiles

	for (MAP *workedTile : getBaseWorkedTiles(base))
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
	TerraformingTileInfo *terraformingTileInfo = getTerraformingTileInfo(mapIndex);
	int x = getX(mapIndex);
	int y = getY(mapIndex);
	
	// no fitness score for ocean
	
	if (is_ocean(tile))
		return 0.0;
	
	// average levels
	
	double averageRainfall = 0.75;
	double averageRockiness = 0.75;
	double averageElevation = 0.50;
	
	// collect tile values

	int rainfall = map_rainfall(tile);
	int rockiness = map_rockiness(tile);
	int elevation = map_elevation(tile);
	
	// evaluate effect
	
	double nutrientEffect = 0.0;
	double mineralEffect = 0.0;
	double energyEffect = 0.0;
	
	switch(action)
	{
	case FORMER_MINE:
		
		if (rockiness == 2 && !levelTerrain) // rocky mine
		{
			// not used nutrients and not used energy
			
			nutrientEffect += -((double)rainfall - averageRainfall);
			energyEffect += -((double)elevation - averageElevation);
			
			// reduced nutrients from nutrient bonus
			
			if (bonus_at(x, y) == 1)
			{
				nutrientEffect += (double)(std::max(1, (2 + Rules->nutrient_effect_mine_sq)) - 2);
			}
			
		}
		else // open mine
		{
			// not used energy
			
			energyEffect += -((double)elevation - averageElevation);
			
		}
		
		break;
		
	case FORMER_SOLAR:
		
		// no fitness
		
		break;
		
	case FORMER_CONDENSER:
		
		// not used energy
		
		energyEffect += -((double)elevation - averageElevation);
			
		break;
		
	case FORMER_ECH_MIRROR:
		
		// no fitness
		
		break;
		
	case FORMER_THERMAL_BORE:
	case FORMER_FOREST:
	case FORMER_PLANT_FUNGUS:
		
		// not used nutrients and not used minerals and not used energy
		
		nutrientEffect += -((double)rainfall - averageRainfall);
		mineralEffect += -((double)rockiness - averageRockiness);
		energyEffect += -((double)elevation - averageElevation);
		
		break;
		
	case FORMER_SENSOR:
		
		if (map_has_item(tile, TERRA_FOREST | TERRA_FUNGUS | TERRA_MONOLITH))
		{
			// compatible with these items
		}
		else if (map_has_item(tile, TERRA_MINE | TERRA_SOLAR | TERRA_CONDENSER | TERRA_ECH_MIRROR | TERRA_THERMAL_BORE))
		{
			// not compatible with these items
			
			nutrientEffect += -1;
			mineralEffect += -1;
			energyEffect += -1;
			
		}
		else
		{
			// not used energy
			
			energyEffect += -((double)elevation - averageElevation);
			
		}
		
		break;
		
	}
	
	// levelTerrain
	
	if (levelTerrain)
	{
		// find min rocky tile count
		
		int minRockyTileCount = INT_MAX;
		
		for (int baseId : terraformingTileInfo->workableBaseIds)
		{
			minRockyTileCount = std::min(minRockyTileCount, terraformingBaseInfos.at(baseId).rockyLandTileCount);
		}
		
		// compute penalty
		
		if (minRockyTileCount < conf.ai_terraforming_land_rocky_tile_threshold)
		{
			mineralEffect += (double)(minRockyTileCount - conf.ai_terraforming_land_rocky_tile_threshold);
		}
		
	}
	
	// not having mine in mineral bonus tile
	
	if (action != FORMER_MINE && getMineralBonus(tile) > 0)
	{
		mineralEffect += -1.0;
	}
	
	// compute total fitness score

	double fitnessScore =
		conf.ai_terraforming_fitnessMultiplier
		*
		(
			conf.ai_terraforming_nutrientWeight * nutrientEffect
			+
			conf.ai_terraforming_mineralWeight * mineralEffect
			+
			conf.ai_terraforming_energyWeight * energyEffect
		)
	;

	debug
	(
		"\t\t%-20s=%6.3f : rainfall=%d, rockiness=%d, elevation=%d, nutrientEffect=%f, mineralEffect=%f, energyEffect=%f, multiplier=%f\n",
		"fitnessScore",
		fitnessScore,
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
	int x = getX(mapIndex);
	int y = getY(mapIndex);

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
	if (!map_has_item(improvedTile, TERRA_CONDENSER) && condenserAction)
	{
		sign = +1;
	}
	// condenser is destroyed
	else if (map_has_item(improvedTile, TERRA_CONDENSER) && !condenserAction)
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

	for (MAP *adjacentTile : getAdjacentTiles(x, y, true))
	{
		// exclude non terraformable site
		
		if (!isValidTerraformingSite(adjacentTile))
			continue;
		
		// exclude non conventional terraformable site
		
		if (!isValidConventionalTerraformingSite(adjacentTile))
			continue;
		
		// exclude ocean - not affected by condenser

		if (is_ocean(adjacentTile))
			continue;

		// exclude great dunes - not affected by condenser

		if (map_has_landmark(adjacentTile, LM_DUNES))
			continue;

		// exclude borehole

		if (map_has_item(adjacentTile, TERRA_THERMAL_BORE))
			continue;

		// exclude rocky areas

		if (map_rockiness(adjacentTile) == 2)
			continue;

		// condenser was there but removed
		if (sign == -1)
		{
			switch(map_rainfall(adjacentTile))
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
			switch(map_rainfall(adjacentTile))
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

	double extraScore = sign * (conf.ai_terraforming_nutrientWeight * (totalRainfallImprovement - 2.0));

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
    int workedTileCount = 0;

    for (int dx = -4; dx <= +4; dx++)
	{
		for (int dy = -(4 - abs(dx)); dy <= +(4 - abs(dx)); dy += 2)
		{
			MAP *closeTile = getMapTile(x + dx, y + dy);
			if (!closeTile)
				continue;

			totalTileCount++;
			
			// our territory within base radius
			
			if (isValidYieldSite(closeTile))
				workedTileCount++;

		}

	}

	// safety exit

	if (totalTileCount == 0)
		return 0.0;

	// [0.0, 1.0]
	double workedTileCoverage = (double)workedTileCount / (double)totalTileCount;

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

			if (map_has_item(closeTile, TERRA_BASE_IN_TILE))
				continue;

			// count rivers

			if (map_has_item(closeTile, TERRA_RIVER))
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
			TerraformingTileInfo *terraformingTileInfo = getTerraformingTileInfo(tile);

			if (!tile)
				continue;

			// exclude base

			if (map_has_item(tile, TERRA_BASE_IN_TILE))
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

			if (elevation < expectedElevation && map_has_item(tile, TERRA_SOLAR | TERRA_ECH_MIRROR))
			{
				double energyImprovement = (double)(expectedElevation - elevation);

				// halve if not worked tile

				if (!(isValidTerraformingSite(tile) && isValidConventionalTerraformingSite(tile) && terraformingTileInfo->workableBaseIds.size() > 0))
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
	int x = getX(raisedTile);
	int y = getY(raisedTile);

	Faction *faction = &(Factions[aiFactionId]);

	// raising ocena is always safe

	if (is_ocean(raisedTile))
		return true;

	// determine affected range

	int maxRange = map_elevation(raisedTile) + 1;

	// check tiles in range

	for (int dx = -(2 * maxRange); dx <= +(2 * maxRange); dx++)
	{
		for (int dy = -(2 * maxRange - abs(dx)); dy <= +(2 * maxRange - abs(dx)); dy += 2)
		{
			MAP *tile = getMapTile(x + dx, y + dy);

			if (!tile)
				continue;

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

	}

	return true;

}

/*
Calculates combined weighted resource score taking base additional demand into account.
*/
double calculateBaseResourceScore(int baseId, int currentNutrientSurplus, int currentMineralSurplus, int currentEnergySurplus, int improvedNutrientSurplus, int improvedMineralSurplus, int improvedEnergySurplus)
{
	BASE *base = &(Bases[baseId]);
	
	// calculate number of workers
	
	int workerCount = 0;
	
	for (int index = 1; index < BASE_OFFSET_COUNT_RADIUS; index++)
	{
		if ((base->worked_tiles & (0x1 << index)) != 0)
		{
			workerCount++;
		}
		
	}
	
	// calculate nutrient and mineral extra score
	
	double nutrientCostMultiplier = 1.0;
	double mineralCostMultiplier = 1.0;
	
	// multipliers are for bases size 3 and above
	
	if (base->pop_size >= 3)
	{
		// calculate nutrient cost multiplier
		
		double nutrientThreshold = conf.ai_terraforming_baseNutrientThresholdRatio * (double)(workerCount + 1);
		
		int minNutrientSurplus = std::min(currentNutrientSurplus, improvedNutrientSurplus);
		
		if (minNutrientSurplus < nutrientThreshold)
		{
			double proportion = 1.0 - (double)minNutrientSurplus / nutrientThreshold;
			nutrientCostMultiplier += (conf.ai_terraforming_baseMineralDemandMultiplier - 1.0) * proportion;
		}
		
		// calculate mineral cost multiplier
		
		double mineralThreshold = conf.ai_terraforming_baseMineralThresholdRatio * (double)minNutrientSurplus;
		
		int minMineralSurplus = std::min(currentMineralSurplus, improvedMineralSurplus);
		
		if (minMineralSurplus < mineralThreshold)
		{
			double proportion = 1.0 - (double)minMineralSurplus / mineralThreshold;
			mineralCostMultiplier += (conf.ai_terraforming_baseMineralDemandMultiplier - 1.0) * proportion;
		}
		
	}
	
	// compute final score
	
	return
		(conf.ai_terraforming_nutrientWeight * nutrientCostMultiplier) * (improvedNutrientSurplus - currentNutrientSurplus)
		+
		(conf.ai_terraforming_mineralWeight * mineralCostMultiplier) * (improvedMineralSurplus - currentMineralSurplus)
		+
		conf.ai_terraforming_energyWeight * (improvedEnergySurplus - currentEnergySurplus)
	;

}

/*
Calculates yield improvement score for given base and improved tile.
*/
double computeImprovementBaseSurplusEffectScore(int baseId, MAP *tile, MAP_STATE *currentMapState, MAP_STATE *improvedMapState)
{
	BASE *base = &(Bases[baseId]);
	
	bool worked = false;

	// set initial state
	
	setMapState(tile, currentMapState);
	computeBase(baseId);

	// gather current surplus

	int currentNutrientSurplus = base->nutrient_surplus;
	int currentMineralSurplus = base->mineral_surplus;
	int currentEnergySurplus = base->economy_total + base->psych_total + base->labs_total;

	// apply changes

	setMapState(tile, improvedMapState);
	computeBase(baseId);

	// gather improved surplus

	int improvedNutrientSurplus = base->nutrient_surplus;
	int improvedMineralSurplus = base->mineral_surplus;
	int improvedEnergySurplus = base->economy_total + base->psych_total + base->labs_total;
	
	// verify tile is worked after improvement
	
	worked = isBaseWorkedTile(baseId, tile);
	
	// restore original state
	
	setMapState(tile, currentMapState);
	computeBase(baseId);

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
			currentNutrientSurplus,
			currentMineralSurplus,
			currentEnergySurplus,
			improvedNutrientSurplus,
			improvedMineralSurplus,
			improvedEnergySurplus
		)
	;

	// return value

	return baseResourceScore;

}

bool isValidYieldSite(MAP *tile)
{
	if (tile == nullptr)
		return false;

	// own territory

	if (tile->owner != aiFactionId)
		return false;

	// base radius

	if (map_has_item(tile, TERRA_BASE_RADIUS))
		return false;

	// exclude volcano mouth

	if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
		return false;

	// all conditions met

	return true;

}

bool isValidTerraformingSite(MAP *tile)
{
	if (tile == nullptr)
		return false;
	
	TerraformingTileInfo *terraformingTileInfo = getTerraformingTileInfo(tile);
	
	// not claimed territory

	if (!(tile->owner == -1 || tile->owner == aiFactionId))
		return false;

	// base radius

	if (!map_has_item(tile, TERRA_BASE_RADIUS))
		return false;

	// exclude base in tile

	if (tile->items & TERRA_BASE_IN_TILE)
		return false;

	// exclude volcano mouth

	if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
		return false;

	// exclude blocked locations

	if (aiData.getTileInfo(tile)->blocked)
		return false;

	// exclude warzone locations

	if (aiData.getTileInfo(tile)->warzone)
		return false;

	// no current terraforming
	
	if (terraformingTileInfo->terraformed)
		return false;
	
	// all conditions met

	return true;

}

/*
Check if this tile can be worked by base and can be terraformed.
*/
bool isValidConventionalTerraformingSite(MAP *tile)
{
	if (tile == nullptr)
		return false;
	
	TerraformingTileInfo *terraformingTileInfo = getTerraformingTileInfo(tile);
	
	// no monolith
	
	if (map_has_item(tile, TERRA_MONOLITH))
		return false;
	
	// not harvested
	
	if (terraformingTileInfo->harvested)
		return false;
	
	// all conditions met

	return true;

}

