#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "game.h"
#include "terranx_wtp.h"
#include "wtp.h"
#include "ai.h"
#include "aiMoveFormer.h"

// global precomputed values

BaseTerraformingInfo *baseTerraformingInfos = new BaseTerraformingInfo[MaxBaseNum];
TileTerraformingInfo *tileTerraformingInfos;

std::vector<MAP *> terraformingSites;
std::vector<MAP *> conventionalTerraformingSites;

double networkDemand = 0.0;
std::unordered_set<MAP *> harvestedTiles;
std::unordered_set<MAP *> terraformedTiles;
std::unordered_set<MAP *> terraformedForestTiles;
std::unordered_set<MAP *> terraformedCondenserTiles;
std::unordered_set<MAP *> terraformedMirrorTiles;
std::unordered_set<MAP *> terraformedBoreholeTiles;
std::unordered_set<MAP *> terraformedAquiferTiles;
std::unordered_set<MAP *> terraformedRaiseTiles;
std::unordered_set<MAP *> terraformedSensorTiles;
std::vector<FORMER_ORDER> formerOrders;
std::map<int, FORMER_ORDER *> vehicleFormerOrders;
std::vector<MAP *> territoryTiles;
std::vector<TERRAFORMING_REQUEST> terraformingRequests;
std::unordered_set<MAP *> targetedTiles;
std::unordered_set<MAP *> connectionNetworkLocations;
std::unordered_map<MAP *, std::unordered_map<int, std::vector<BASE_INFO>>> nearbyBaseSets;
std::unordered_map<int, int> seaRegionMappings;
std::unordered_map<MAP *, int> coastalBaseRegionMappings;
std::unordered_set<MAP *> blockedLocations;
std::unordered_set<MAP *> warzoneLocations;
std::unordered_set<MAP *> zocLocations;

/*
Prepares former orders.
*/
void moveFormerStrategy()
{
	// initialize data
	
	initializeData();
	
	// populate data

	populateData();
	
	// formers
	
	cancelRedundantOrders();

	generateTerraformingRequests();

	sortTerraformingRequests();
	
	applyProximityRules();

	assignFormerOrders();

	finalizeFormerOrders();

	// clean up data

	cleanupData();
	
}

/*
Populate global lists before processing faction strategy.
*/
void initializeData()
{
	// create tile terraforming info
	
	tileTerraformingInfos = new TileTerraformingInfo[*map_area_tiles];
	
}

/*
Populate global lists before processing faction strategy.
*/
void populateData()
{
	Faction *aiFaction = &(Factions[aiFactionId]);

	// populate blocked/warzone and zoc locations

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(Vehicles[id]);
		MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);

		// exclude own vehicles

		if (vehicle->faction_id == aiFactionId)
			continue;

		// exclude vehicles with pact

		if (aiFaction->diplo_status[vehicle->faction_id] & DIPLO_PACT)
			continue;

		// store blocked location

		blockedLocations.insert(vehicleLocation);

		// calculate zoc locations

		if (!is_ocean(vehicleLocation))
		{
			for (MAP *adjacentTile : getBaseAdjacentTiles(vehicle->x, vehicle->y, false))
			{
				if (!is_ocean(adjacentTile))
				{
					zocLocations.insert(adjacentTile);
				}

			}

		}

		// store warzone location

		if
		(
			// any faction with vendetta
			(aiFaction->diplo_status[vehicle->faction_id] & DIPLO_VENDETTA)
			&&
			// except fungal tower
			vehicle->unit_id != BSC_FUNGAL_TOWER
		)
		{
			// 2 steps from artillery 1 step from anyone else

			int range = (vehicle_has_ability(vehicle, ABL_ARTILLERY) ? 2 : 1);

			for (int dx = -(2 * range); dx <= +(2 * range); dx++)
			{
				for (int dy = -((2 * range) - abs(dx)); dy <= +((2 * range) - abs(dx)); dy += 2)
				{
					MAP *tile = getMapTile(vehicle->x + dx, vehicle->y + dy);

					if (!tile)
						continue;

					warzoneLocations.insert(tile);

				}

			}

		}

	}

	// populate base related lists

	std::vector<std::set<int>> seaRegionLinkedGroups;

	for (int id : aiData.baseIds)
	{
		BASE *base = &(Bases[id]);

		// populate affected base sets

		for (int dy = -6; dy <= +6; dy++)
		{
			for (int dx = -(6 - abs(dy)); dx <= +(6-abs(dy)); dx++)
			{
				// exclude corners

				if (abs(dy) == 6 || abs(dx) == 6)
					continue;

				// get tile

				int x = base->x + dx;
				int y = base->y + dy;
				MAP *tile = getMapTile(wrap(x), y);

				// skip impossible combinations

				if (!tile)
					continue;

				// add this tile to affected base sets for range 1

				if (nearbyBaseSets.count(tile) == 0)
				{
					nearbyBaseSets[tile] = std::unordered_map<int, std::vector<BASE_INFO>>();
					nearbyBaseSets[tile][0] = std::vector<BASE_INFO>();
					nearbyBaseSets[tile][1] = std::vector<BASE_INFO>();
				}

				nearbyBaseSets[tile][1].push_back({id, base});

				// add this tile to affected base sets for range 0 if in base radius

				if (isWithinBaseRadius(base->x, base->y, x, y))
				{
					nearbyBaseSets[tile][0].push_back({id, base});
				}

			}

		}

	}

	// populate sea region mappings for coastal bases connecting sea regions

	for (int id : aiData.baseIds)
	{
		BASE *base = &(Bases[id]);

		MAP *baseTile = getMapTile(base->x, base->y);

		if (!is_ocean(baseTile))
		{
			std::set<int> seaRegionLinkedGroup;

			for (MAP *adjacentTile : getBaseAdjacentTiles(base->x, base->y, false))
			{
				// get sea region

				if (is_ocean(adjacentTile))
				{
					int region = adjacentTile->region;
					seaRegionLinkedGroup.insert(region);
				}

			}

			if (seaRegionLinkedGroup.size() >= 1)
			{
				seaRegionLinkedGroups.push_back(seaRegionLinkedGroup);

			}

		}

	}

	// populate sea region mappings

	bool aggregated = true;
	for (; aggregated == true;)
	{
		aggregated = false;

		for (size_t i = 0; i < seaRegionLinkedGroups.size(); i++)
		{
			std::set<int> *seaRegionLinkedGroup1 = &(seaRegionLinkedGroups[i]);

			// skip empty sets

			if (seaRegionLinkedGroup1->size() == 0)
				continue;

			for (size_t j = i + 1; j < seaRegionLinkedGroups.size(); j++)
			{
				std::set<int> *seaRegionLinkedGroup2 = &(seaRegionLinkedGroups[j]);

				for
				(
					std::set<int>::iterator seaRegionLinkedGroup2Iterator = seaRegionLinkedGroup2->begin();
					seaRegionLinkedGroup2Iterator != seaRegionLinkedGroup2->end();
					seaRegionLinkedGroup2Iterator++
				)
				{
					int seaRegionLinkedGroup2Element = *seaRegionLinkedGroup2Iterator;

					if (seaRegionLinkedGroup1->count(seaRegionLinkedGroup2Element) != 0)
					{
						seaRegionLinkedGroup1->insert(seaRegionLinkedGroup2->begin(), seaRegionLinkedGroup2->end());
						seaRegionLinkedGroup2->clear();

						aggregated = true;

						break;

					}

				}

			}

		}

	}

	for (size_t i = 0; i < seaRegionLinkedGroups.size(); i++)
	{
		std::set<int> *seaRegionLinkedGroup = &(seaRegionLinkedGroups[i]);

		// skip empty sets

		if (seaRegionLinkedGroup->size() == 0)
			continue;

		// get mapping region

		int mappingRegion = *(seaRegionLinkedGroup->begin());

		// map regions

		for
		(
			std::set<int>::iterator seaRegionLinkedGroupIterator = seaRegionLinkedGroup->begin();
			seaRegionLinkedGroupIterator != seaRegionLinkedGroup->end();
			seaRegionLinkedGroupIterator++
		)
		{
			int region = *seaRegionLinkedGroupIterator;

			seaRegionMappings[region] = mappingRegion;

		}

	}

	// populate coastal base sea region mappings

	for (int id : aiData.baseIds)
	{
		BASE *base = &(Bases[id]);

		MAP *baseTile = getMapTile(base->x, base->y);

		if (!is_ocean(baseTile))
		{
			for (MAP *adjacentTile : getBaseAdjacentTiles(base->x, base->y, false))
			{
				if (is_ocean(adjacentTile))
				{
					int terraformingRegion = getConnectedRegion(adjacentTile->region);
					coastalBaseRegionMappings[baseTile] = seaRegionMappings[terraformingRegion];

					break;

				}

			}

		}

	}

	// populated terraformed tiles and former orders

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &Vehicles[id];
		MAP *vehicleTile = getMapTile(vehicle->x, vehicle->y);

		if (!vehicleTile)
			continue;

		// exclude not own vehicles

		if (vehicle->faction_id != aiFactionId)
			continue;

		// process supplies and formers

		// supplies
		if (isVehicleSupply(vehicle))
		{
			// convoying vehicles
			if (isVehicleConvoying(vehicle))
			{
				MAP *harvestedTile = getMapTile(vehicle->x, vehicle->y);

				harvestedTiles.insert(harvestedTile);

			}

		}
		// formers
		else if (isFormerVehicle(vehicle))
		{
			// terraforming vehicles
			if (isVehicleTerraforming(vehicle))
			{
				// populate terraformed tile

				MAP *terraformedTile = getMapTile(vehicle->x, vehicle->y);

				terraformedTiles.insert(terraformedTile);

				// populate terraformed forest tiles

				if (vehicle->move_status == ORDER_PLANT_FOREST)
				{
					terraformedForestTiles.insert(terraformedTile);
				}

				// populate terraformed condenser tiles

				if (vehicle->move_status == ORDER_CONDENSER)
				{
					terraformedCondenserTiles.insert(terraformedTile);
				}

				// populate terraformed mirror tiles

				if (vehicle->move_status == ORDER_ECHELON_MIRROR)
				{
					terraformedMirrorTiles.insert(terraformedTile);
					debug("terraformedMirrorTiles: (%3d,%3d)\n", vehicle->x, vehicle->y);
				}

				// populate terraformed borehole tiles

				if (vehicle->move_status == ORDER_THERMAL_BOREHOLE)
				{
					terraformedBoreholeTiles.insert(terraformedTile);
				}

				// populate terraformed aquifer tiles

				if (vehicle->move_status == ORDER_DRILL_AQUIFIER)
				{
					terraformedAquiferTiles.insert(terraformedTile);
				}

				// populate terraformed raise tiles

				if (vehicle->move_status == ORDER_TERRAFORM_UP)
				{
					terraformedRaiseTiles.insert(terraformedTile);
				}

				// populate terraformed sensor tiles

				if (vehicle->move_status == ORDER_SENSOR_ARRAY)
				{
					terraformedSensorTiles.insert(terraformedTile);
				}

			}
			// available vehicles
			else
			{
				// get vehicle triad

				int triad = veh_triad(id);

				// exclude air terraformers - let AI deal with them itself

				if (triad == TRIAD_AIR)
					continue;

				// get vehicle location body type

				bool sea = is_ocean(vehicleTile);

				// exclude land formers on sea

				if (triad == TRIAD_LAND && sea)
					continue;

				// store vehicle current id in pad_0 field

				vehicle->pad_0 = id;

				// add vehicle
				
				formerOrders.push_back(FORMER_ORDER(id, vehicle));

			}

		}

	}

	// populate territory tiles
	// all available tiles on own territory

	for (int y = 0; y < *map_axis_y; y++)
	{
		for (int x = 0; x < *map_axis_x; x++)
		{
			MAP *tile = getMapTile(x, y);

			// exclude impossible combinations

			if (!tile)
				continue;

			// only own territory

			if (tile->owner != aiFactionId)
				continue;

			// exclude volcano mouth

			if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
				continue;

			// exclude base in tile

			if (tile->items & TERRA_BASE_IN_TILE)
				continue;

			// exclude blocked locations

			if (blockedLocations.count(tile) != 0)
				continue;

			// exclude warzone locations

			if (warzoneLocations.count(tile) != 0)
				continue;

			// store tile

			territoryTiles.push_back(tile);

		}

	}

	// populate tileTerraformingInfos
	// populate validTerraformingSites
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		TileTerraformingInfo *tileTerraformingInfo = &(tileTerraformingInfos[mapIndex]);
		MAP *tile = getMapTile(mapIndex);
		
		// valid terraforming site
		
		if (!isValidTerraformingSite(tile))
			continue;
		
		// tile map data
		
		tileTerraformingInfo->ocean = is_ocean(tile);
		tileTerraformingInfo->fungus = map_has_item(tile, TERRA_FUNGUS);
		tileTerraformingInfo->rocky = (tileTerraformingInfo->ocean ? false : map_rockiness(tile) == 2);
		
		// add to valid terraforming sites
		
		terraformingSites.push_back(tile);
		
		if (isValidConventionalTerraformingSite(tile))
		{
			// add to valid conventional terraforming sites
			
			conventionalTerraformingSites.push_back(tile);
			
		}
		
	}
	
	// populate base terraforming infos
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		BaseTerraformingInfo *baseTerraformingInfo = &(baseTerraformingInfos[baseId]);
		
		// initialize base terraforming info
		
		baseTerraformingInfo->unimprovedWorkedTileCount = getUnimprovedWorkedTileCount(baseId);
		baseTerraformingInfo->ongoingYieldTerraformingCount = getOngoingYieldTerraformingCount(baseId);
		baseTerraformingInfo->rockyLandTileCount = 0;

		// count formers currently improving base yield
		
		int ongoingYieldTerraformingCount = 0;

		// iterate base tiles

		for (int offset = 1; offset < BASE_OFFSET_COUNT_RADIUS; offset++)
		{
			int x = base->x + BASE_TILE_OFFSETS[offset][0];
			int y = base->y + BASE_TILE_OFFSETS[offset][1];
			int mapIndex = getMapIndexByCoordinates(x, y);
			MAP *tile = getMapTile(mapIndex);
			TileTerraformingInfo *tileTerraformingInfo = &(tileTerraformingInfos[mapIndex]);
			
			// valid base terraforming site
			
			if (!isValidTerraformingSite(tile))
				continue;
			
			// update worked base
			
			tileTerraformingInfos->workedBaseId = ((base->worked_tiles & (0x1 << offset)) != 0);
			
			// update workable bases
			
			tileTerraformingInfos->workableBaseIds.push_back(baseId);
			
			// update base rocky land tile count
			
			if (isValidConventionalTerraformingSite(tile) && tileTerraformingInfo->rocky)
			{
				baseTerraformingInfo->rockyLandTileCount++;
			}
			
		}
		
		// iterate base area expanded tiles

		for (int offset = 1; offset < BASE_OFFSET_COUNT_RADIUS_ADJACENT; offset++)
		{
			int x = base->x + BASE_TILE_OFFSETS[offset][0];
			int y = base->y + BASE_TILE_OFFSETS[offset][1];
			int mapIndex = getMapIndexByCoordinates(x, y);
			MAP *tile = getMapTile(mapIndex);
			
			// valid terraforming site
			
			if (!isValidTerraformingSite(tile))
				continue;
			
			// update area workable bases
			
			tileTerraformingInfos->areaWorkableBaseIds.push_back(baseId);
			
		}
		
		// store ongoing terraforming count
		
		baseTerraformingInfo->ongoingYieldTerraformingCount = ongoingYieldTerraformingCount;

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
	
}

void cleanupData()
{
	// delete tile terraforming info
	
	delete[] tileTerraformingInfos;
	
	// clear lists

	terraformingSites.clear();
	conventionalTerraformingSites.clear();
	
	harvestedTiles.clear();
	terraformedTiles.clear();
	terraformedForestTiles.clear();
	terraformedCondenserTiles.clear();
	terraformedMirrorTiles.clear();
	terraformedBoreholeTiles.clear();
	terraformedAquiferTiles.clear();
	terraformedRaiseTiles.clear();
	terraformedSensorTiles.clear();
	territoryTiles.clear();
	terraformingRequests.clear();
	formerOrders.clear();
	vehicleFormerOrders.clear();
	targetedTiles.clear();
	connectionNetworkLocations.clear();
	nearbyBaseSets.clear();
	seaRegionMappings.clear();
	coastalBaseRegionMappings.clear();
	blockedLocations.clear();
	warzoneLocations.clear();
	zocLocations.clear();

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

		std::unordered_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(terraformingRequest->action);
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

void assignFormerOrders()
{
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
			
			if (!isDestinationReachable(vehicleId, x, y))
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
		
		// not found
		
		if (nearestFormerOrder == nullptr)
			continue;
		
		nearestFormerOrder->x = getX(terraformingRequest.tile);
		nearestFormerOrder->y = getY(terraformingRequest.tile);
		nearestFormerOrder->action = terraformingRequest.action;
		
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
	int mapIndex = getMapIndexByPointer(tile);
	TileTerraformingInfo *tileTerraformingInfo = &(tileTerraformingInfos[mapIndex]);
	bool ocean = tileTerraformingInfo->ocean;
	bool fungus = tileTerraformingInfo->fungus;
	bool rocky = tileTerraformingInfo->rocky;
	int x = getX(mapIndex);
	int y = getY(mapIndex);

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

		if (option.ocean != tileTerraformingInfo->ocean)
			continue;

		// process only correct rockiness

		if (option.rocky && (!ocean && !rocky))
			continue;

		// check if required action technology is available when required

		if (option.requiredAction != -1 && !isTerraformingAvailable(tile, option.requiredAction))
			continue;
		
		debug("\t%s\n", option.name);
		
		// initialize variables

		MAP_STATE improvedMapState;
		copyMapState(&improvedMapState, &currentMapState);
		
		int firstAction = -1;
		int terraformingTime = 0;
		double penaltyScore = 0.0;
		double improvementScore = 0.0;

		// process actions
		
		int availableActionCount = 0;
		int completedActionCount = 0;
		bool levelTerrain = false;
		
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
					calculateTerraformingTime
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
					calculateTerraformingTime
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
				calculateTerraformingTime
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

		// calculate exclusivity bonus

		double exclusivityBonus = calculateExclusivityBonus(tile, &(option.actions), levelTerrain);
		
		// summarize score

		improvementScore = penaltyScore + surplusEffectScore + condenserExtraYieldScore + exclusivityBonus;
		
		// calculate terraforming score

		double terraformingScore = improvementScore / ((double)terraformingTime + conf.ai_terraforming_travel_time_multiplier * travelTime);

		debug
		(
			"\t\t%-20s=%6.3f : penalty=%6.3f, surplusEffect=%6.3f, condenser=%6.3f, exclusivity=%6.3f, combined=%6.3f, time=%2d, travel=%f\n",
			"terraformingScore",
			terraformingScore,
			penaltyScore,
			surplusEffectScore,
			condenserExtraYieldScore,
			exclusivityBonus,
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

	terraformingTime = calculateTerraformingTime(action, tile->items, tile->val3, NULL);

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

	terraformingTime = calculateTerraformingTime(action, tile->items, tile->val3, NULL);

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

		terraformingTime += calculateTerraformingTime(removeFungusAction, tile->items, tile->val3, NULL);

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

	terraformingTime += calculateTerraformingTime(action, tile->items, tile->val3, NULL);

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

		terraformingTime += calculateTerraformingTime(removeFungusAction, tile->items, tile->val3, NULL);

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

	terraformingTime += calculateTerraformingTime(action, tile->items, tile->val3, NULL);

	// special sensor bonus

	double sensorScore = calculateSensorScore(tile, action);

	// calculate exclusivity bonus

	double exclusivityBonus = calculateExclusivityBonus(tile, &(option->actions), false) / 4.0;

	// summarize score

	improvementScore = sensorScore + exclusivityBonus;

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
		"\t%-10s: sensorScore=%6.3f, exclusivityBonus=%6.3f, improvementScore=%6.3f, terraformingTime=%2d, travelTime=%f, final=%6.3f\n",
		option->name,
		sensorScore,
		exclusivityBonus,
		improvementScore,
		terraformingTime,
		travelTime,
		terraformingScore
	)
	;

}

/*
Handles movement phase.
*/
int moveFormer(int vehicleId)
{
	// get vehicle

	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);

	// use Thinker algorithm if in warzone

	if (warzoneLocations.count(vehicleTile) > 0)
	{
		debug("[%3d] (%3d,%3d) - warzone - use Thinker algorithm: \n", vehicleId, vehicle->x, vehicle->y);
		return mod_enemy_move(vehicleId);
	}

	// retrieve ai id

	int aiVehicleId = vehicle->pad_0;
	
	// do not move formers with terraforming order
	
	if (vehicle->move_status >= ORDER_FARM && vehicle->move_status <= ORDER_PLACE_MONOLITH)
		return SYNC;

	// ignore vehicles without former orders

	if (vehicleFormerOrders.count(aiVehicleId) == 0)
		return mod_enemy_move(vehicleId);

	// get former order

	FORMER_ORDER *formerOrder = vehicleFormerOrders[aiVehicleId];

	// ignore orders without action

	if (formerOrder->action == -1)
		return mod_enemy_move(vehicleId);

	return SYNC;

}

/*
Computes workable bases surplus effect.
*/
double computeImprovementSurplusEffectScore(MAP *tile, MAP_STATE *currentMapState, MAP_STATE *improvedMapState)
{
	int mapIndex = getMapIndexByPointer(tile);
	TileTerraformingInfo *tileTerraformingInfo = &(tileTerraformingInfos[mapIndex]);
	
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
		
		for (int baseId : tileTerraformingInfo->areaWorkableBaseIds)
		{
			// compute base score

			double score = computeImprovementBaseSurplusEffectScore(baseId, tile, currentMapState, improvedMapState);
			
			// update best score
			
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
		
		for (int baseId : tileTerraformingInfo->workableBaseIds)
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
Checks whether tile is own workable tile but not base square.
*/
bool isOwnWorkableTile(int x, int y)
{
	MAP *tile = getMapTile(x, y);

	// exclude base squares

	if (tile->items & TERRA_BASE_IN_TILE)
		return false;

	// exclude not own territory

	if (tile->owner != aiFactionId)
		return false;

	// iterate through own bases

	for (int baseIndex = 0; baseIndex < *total_num_bases; baseIndex++)
	{
		BASE *base = &Bases[baseIndex];

		// skip not own bases

		if (base->faction_id != aiFactionId)
			continue;

		// check base radius

		return isWithinBaseRadius(base->x, base->y, x, y);

	}

	// matched workable tile not found

	return false;

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
Determines whether terraforming is required at all.
*/
bool isTerraformingRequired(MAP *tile, int action)
{
	// no need to build road in fungus with Xenoempathy Dome

	if
	(
		(tile->items & TERRA_FUNGUS)
		&&
		(action == FORMER_ROAD)
		&&
		(has_project(aiFactionId, FAC_XENOEMPATHY_DOME))
	)
	{
		return false;
	}

	// all other cases are required

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

bool isTileTargettedByVehicle(VEH *vehicle, MAP *tile)
{
	return (vehicle->move_status == ORDER_MOVE_TO && (getVehicleDestination(vehicle) == tile));
}

bool isVehicleConvoying(VEH *vehicle)
{
	return vehicle->move_status == ORDER_CONVOY;
}

bool isVehicleTerraforming(VEH *vehicle)
{
	return vehicle->move_status >= ORDER_FARM && vehicle->move_status <= ORDER_PLACE_MONOLITH;
}

bool isVehicleMoving(VEH *vehicle)
{
	return vehicle->move_status == ORDER_MOVE_TO || vehicle->move_status == ORDER_MOVE || vehicle->move_status == ORDER_ROAD_TO || vehicle->move_status == ORDER_MAGTUBE_TO || vehicle->move_status == ORDER_AI_GO_TO;
}

MAP *getVehicleDestination(VEH *vehicle)
{
	int x = vehicle->x;
	int y = vehicle->y;

	switch (vehicle->waypoint_count)
	{
	case 0:
		x = (vehicle->waypoint_1_x == -1 ? vehicle->x : vehicle->waypoint_1_x);
		y = (vehicle->waypoint_1_y == -1 ? vehicle->y : vehicle->waypoint_1_y);
		break;
	case 1:
		x = vehicle->waypoint_2_x;
		y = vehicle->waypoint_2_y;
		break;
	case 2:
		x = vehicle->waypoint_3_x;
		y = vehicle->waypoint_3_y;
		break;
	case 3:
		x = vehicle->waypoint_4_x;
		y = vehicle->waypoint_4_y;
		break;
	}

	return getMapTile(x, y);

}

bool isTileTakenByOtherFormer(MAP *tile)
{
	return isTileTerraformed(tile) || isTileTargettedByOtherFormer(tile);
}

bool isTileTerraformed(MAP *tile)
{
	return terraformedTiles.count(tile) == 1;
}

bool isTileTargettedByOtherFormer(MAP *tile)
{
	return targetedTiles.count(tile) == 1;
}

/*
Orders former to build other improvement besides base yield change.
*/
void buildImprovement(int vehicleId)
{
	VEH *vehicle = &Vehicles[vehicleId];

	// TODO
	// currently only skip turn

	veh_skip(vehicleId);

	debug
	(
		"generateFormerOrder: (%d,%d), order=skip\n",
		vehicle->x,
		vehicle->y
	)

}

/*
Sends vehicle to destination.
For land units calculates ZOC aware path and sends vehicle to correct adjacent tile.
*/
bool sendVehicleToDestination(int id, int x, int y)
{
	int triad = veh_triad(id);

	if (triad == TRIAD_LAND)
	{
		// find step direction

		MAP *stepLocation = findPathStep(id, x, y);

		// send vehicle to step or cancel orders
		if (stepLocation == nullptr)
		{
			return false;
		}
		else
		{
			tx_go_to(id, veh_status_icon[ORDER_MOVE_TO], getX(stepLocation), getY(stepLocation));
			return true;
		}

	}
	else
	{
		tx_go_to(id, veh_status_icon[ORDER_MOVE_TO], x, y);
		return true;
	}

}

bool isNearbyForestUnderConstruction(int x, int y)
{
	std::unordered_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_FOREST);

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
			MAP *tile = getMapTile(wrap(x + dx), y + dy);

			if (terraformedForestTiles.count(tile) == 1)
                return true;

        }

    }

    return false;

}

bool isNearbyCondeserUnderConstruction(int x, int y)
{
	std::unordered_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_CONDENSER);

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
			MAP *tile = getMapTile(wrap(x + dx), y + dy);

			if (terraformedCondenserTiles.count(tile) == 1)
                return true;

        }

    }

    return false;

}

bool isNearbyMirrorUnderConstruction(int x, int y)
{
	std::unordered_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_ECH_MIRROR);

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
			MAP *tile = getMapTile(wrap(x + dx), y + dy);

			if (!tile)
				continue;

			if (terraformedMirrorTiles.count(tile) == 1)
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
	std::unordered_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_THERMAL_BORE);

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
			MAP *tile = getMapTile(wrap(x + dx), y + dy);

			if (terraformedBoreholeTiles.count(tile) == 1)
                return true;

        }

    }

    return false;

}

bool isNearbyRiverPresentOrUnderConstruction(int x, int y)
{
	std::unordered_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_AQUIFER);

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
			MAP *tile = getMapTile(wrap(x + dx), y + dy);

			if (terraformedAquiferTiles.count(tile) == 1)
                return true;

        }

    }

    return false;

}

bool isNearbyRaiseUnderConstruction(int x, int y)
{
	std::unordered_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_RAISE_LAND);

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
			MAP *tile = getMapTile(wrap(x + dx), y + dy);

			if (terraformedRaiseTiles.count(tile) == 1)
                return true;

        }

    }

    return false;

}

bool isNearbySensorPresentOrUnderConstruction(int x, int y)
{
	std::unordered_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_SENSOR);

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
			MAP *tile = getMapTile(wrap(x + dx), y + dy);

			if (terraformedSensorTiles.count(tile) == 1)
                return true;

        }

    }

    return false;

}

/*
Calculates terraforming time based on faction, tile, and former abilities.
*/
int calculateTerraformingTime(int action, int items, int rocks, VEH* vehicle)
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

BASE *findAffectedBase(int x, int y)
{
	for (int id : aiData.baseIds)
	{
		BASE *base = &(Bases[id]);

		if (isWithinBaseRadius(base->x, base->y, x, y))
			return base;

	}

	return NULL;

}

char *getTerraformingActionName(int action)
{
	if (action >= FORMER_FARM && action <= FORMER_MONOLITH)
	{
		return Terraform[action].name;
	}
	else
	{
		return NULL;
	}

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
	int y = getX(tile);

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

		m[offsetIndex] = (adjacentTile && (adjacentTile->items & (TERRA_BASE_IN_TILE | improvementFlag)));

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

double calculateExclusivityBonus(MAP *tile, const std::vector<int> *actions, bool levelTerrain)
{
	int mapIndex = getMapIndexByPointer(tile);
	TileTerraformingInfo *tileTerraformingInfo = &(tileTerraformingInfos[mapIndex]);
	
	// collect tile values

	int rainfall = map_rainfall(tile);
	int rockiness = map_rockiness(tile);
	int elevation = map_elevation(tile);

	// build action set

	std::set<int> actionSet;
	for
	(
		std::vector<int>::const_iterator actionsIterator = actions->begin();
		actionsIterator != actions->end();
		actionsIterator++
	)
	{
		int action = *actionsIterator;

		actionSet.insert(action);

	}
	
	if (levelTerrain)
	{
		actionSet.insert(FORMER_LEVEL_TERRAIN);
	}

	// calculate resource exclusivity bonuses

	double nutrient = 0.0;
	double mineral = 0.0;
	double energy = 0.0;

	if (!is_ocean(tile))
	{
		// improvements ignoring everything
		if
		(
			actionSet.count(FORMER_FOREST) != 0
			||
			actionSet.count(FORMER_PLANT_FUNGUS) != 0
			||
			actionSet.count(FORMER_THERMAL_BORE) != 0
		)
		{
			// nothing is used

			nutrient = EXCLUSIVITY_LEVELS.rainfall - rainfall;
			mineral = EXCLUSIVITY_LEVELS.rockiness - rockiness;
			energy = EXCLUSIVITY_LEVELS.elevation - elevation;

		}
		else
		{
			// leveling rocky tile is penalizable if there are not many rocky tiles around bases

			if (actionSet.count(FORMER_LEVEL_TERRAIN) != 0 && rockiness == 2)
			{
				int minBaseLandRockyTileCount = INT_MAX;
				
				for (int baseId : tileTerraformingInfo->workableBaseIds)
				{
					minBaseLandRockyTileCount = std::min(minBaseLandRockyTileCount, (int)baseTerraformingInfos[baseId].rockyLandTileCount);
				}
				
				debug("(%3d,%3d) minBaseLandRockyTileCount=%d\n", getX(tile), getY(tile), minBaseLandRockyTileCount);
				
				if (minBaseLandRockyTileCount < conf.ai_terraforming_land_rocky_tile_threshold)
				{
					mineral += minBaseLandRockyTileCount - conf.ai_terraforming_land_rocky_tile_threshold;
				}
				
			}

			// not building mine on mineral bonus wastes 3 mineral
			
			if (actionSet.count(FORMER_MINE) == 0 && isMineBonus(getX(tile), getY(tile)))
			{
				mineral += -3;
			}

			// not building solar collector or echelon mirror wastes elevation

			if (actionSet.count(FORMER_SOLAR) == 0 && actionSet.count(FORMER_ECH_MIRROR) == 0)
			{
				energy += EXCLUSIVITY_LEVELS.elevation - elevation;
			}

			// sensor is better built in flat

			if (actionSet.count(FORMER_SENSOR))
			{
				mineral += EXCLUSIVITY_LEVELS.rockiness - rockiness;
			}

		}
		
	}

	// additional penalty for replaced improvements
	
	double replacedImprovementPenalty = 0.0;

	if (actionSet.count(FORMER_SENSOR))
	{
		if (map_has_item(tile, TERRA_MINE))
		{
			replacedImprovementPenalty -= 1.0;
		}

		if (map_has_item(tile, TERRA_SOLAR))
		{
			replacedImprovementPenalty -= 1.0;
		}

		if (map_has_item(tile, TERRA_CONDENSER))
		{
			replacedImprovementPenalty -= 2.0;
		}

		if (map_has_item(tile, TERRA_ECH_MIRROR))
		{
			replacedImprovementPenalty -= 1.0;
		}

		if (map_has_item(tile, TERRA_THERMAL_BORE))
		{
			replacedImprovementPenalty -= 2.0;
		}

	}
	
	double exclusivityBonus =
		conf.ai_terraforming_exclusivityMultiplier
		*
		(
			conf.ai_terraforming_nutrientWeight * nutrient
			+
			conf.ai_terraforming_mineralWeight * mineral
			+
			conf.ai_terraforming_energyWeight * energy
		)
		+
		replacedImprovementPenalty
	;

	debug
	(
		"\t\t%-20s=%6.3f : rainfall=%d, rockiness=%d, elevation=%d, nutrient=%f, mineral=%f, energy=%f, replacedImprovementPenalty=%f\n",
		"exclusivityBonus",
		exclusivityBonus,
		rainfall,
		rockiness,
		elevation,
		nutrient,
		mineral,
		energy,
		replacedImprovementPenalty
	);

	return exclusivityBonus;

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

	Faction *faction = &(Factions[aiFactionId]);

	// build action set

	std::set<int> actionSet;
	for
	(
		std::vector<int>::const_iterator actionsIterator = actions->begin();
		actionsIterator != actions->end();
		actionsIterator++
	)
	{
		int action = *actionsIterator;

		actionSet.insert(action);

	}

	// check whether condenser was built or destroyed and set score sign as well

	int sign;

	// condenser is built
	if (!map_has_item(improvedTile, TERRA_CONDENSER) && actionSet.count(FORMER_CONDENSER) != 0)
	{
		sign = +1;
	}
	// condenser is destroyed
	else if (map_has_item(improvedTile, TERRA_CONDENSER) && actionSet.count(FORMER_CONDENSER) == 0)
	{
		sign = -1;
	}
	// neither built nor destroyed
	else
	{
		return 0.0;
	}

	// calculate forest yield with best forest facilities available

	int forestNutrient = Resource->forest_sq_nutrient;
	int forestMineral = Resource->forest_sq_mineral;
	int forestEnergy = Resource->forest_sq_energy;

	if (has_facility_tech(aiFactionId, FAC_TREE_FARM))
	{
		forestNutrient++;
	}

	if (has_facility_tech(aiFactionId, FAC_HYBRID_FOREST))
	{
		forestNutrient++;
		forestNutrient++;
	}

	double forestYieldScore = calculateResourceScore(forestNutrient, forestMineral, forestEnergy);

	// calculate fungus yield with current technologies and PLANET rating

	int fungusNutrient = faction->tech_fungus_nutrient;
	int fungusMineral = faction->tech_fungus_mineral;
	int fungusEnergy = faction->tech_fungus_energy;

	if (faction->SE_planet_pending < 0)
	{
		fungusNutrient = std::max(0, fungusNutrient + faction->SE_planet_pending);
		fungusMineral = std::max(0, fungusMineral + faction->SE_planet_pending);
		fungusEnergy = std::max(0, fungusEnergy + faction->SE_planet_pending);
	}

	double fungusYieldScore = calculateResourceScore(fungusNutrient, fungusMineral, fungusEnergy);

	// pick the best yield score

	double alternativeYieldScore = std::max(forestYieldScore, fungusYieldScore);

	// calculate rocky mine yield score

	int rockyMineMineral = 4;
	double rockyMineYieldScore = calculateResourceScore(0.0, (double)rockyMineMineral, 0.0);

	// calculate extra yield with technology

	double extraNutrient = (has_terra(aiFactionId, FORMER_SOIL_ENR, false) ? 2.0 : 1.0);
	double extraEnergy = (has_terra(aiFactionId, FORMER_ECH_MIRROR, false) ? 1.0 : 0.0);

	// estimate rainfall improvement

	// [0.0, 9.0]
	double totalRainfallImprovement = 0.0;

	for (MAP *tile : getBaseAdjacentTiles(x, y, true))
	{
		// exclude non terraformable site
		
		if (!isValidTerraformingSite(tile))
			continue;
		
		// exclude non conventional terraformable site
		
		if (!isValidConventionalTerraformingSite(tile))
			continue;
		
		// exclude ocean - not affected by condenser

		if (is_ocean(tile))
			continue;

		// exclude great dunes - not affected by condenser

		if (map_has_landmark(tile, LM_DUNES))
			continue;

		// estimate rainfall improved by condenser

		double rainfallImprovement = 0.0;

		// condenser was there but removed
		if (sign == -1)
		{
			// calculate yield score

			double nutrient = (double)map_rainfall(tile) + extraNutrient;
			double mineral = (double)std::min(1, map_rockiness(tile));
			double energy = (double)(1 + map_elevation(tile)) + extraEnergy;

			double yieldScore = calculateResourceScore(nutrient, mineral, energy);

			// add to improvement if it no less than alternative or rocky mine one

			if
			(
				yieldScore > alternativeYieldScore
				&&
				(map_rockiness(tile) < 2 || yieldScore > rockyMineYieldScore)
			)
			{
				switch (map_rainfall(tile))
				{
				case 0:
					// this case probably won't happen ever but just in case
					break;
				case 1:
					rainfallImprovement = 1.0;
					break;
				case 2:
					// give just half because this tile could be already rainy before - we don't know
					rainfallImprovement = 0.5;
					break;
				}
			}

		}
		// condenser wasn't there and built
		else if (sign == +1)
		{
			// calculate yield score with improved rainfall

			double nutrient = (double)std::max(2, map_rainfall(tile) + 1) + extraNutrient;
			double mineral = (double)std::min(1, map_rockiness(tile));
			double energy = (double)(1 + map_elevation(tile)) + extraEnergy;

			double yieldScore = calculateResourceScore(nutrient, mineral, energy);

			// add to improvement if it no less than forest one

			if
			(
				yieldScore > alternativeYieldScore
				&&
				(map_rockiness(tile) < 2 || yieldScore > rockyMineYieldScore)
			)
			{
				switch (map_rainfall(tile))
				{
				case 0:
					rainfallImprovement = 1.0;
					break;
				case 1:
					rainfallImprovement = 1.0;
					break;
				case 2:
					// rainy tile won't get better
					break;
				}
			}

		}
		// safety check
		else
		{
			return 0.0;
		}

		// halve score for borehole

		if (map_has_item(tile, TERRA_THERMAL_BORE))
		{
			rainfallImprovement /= 2.0;
		}

		// update totalRainfallImprovement

		totalRainfallImprovement += rainfallImprovement;

	}

	// calculate bonus
	// rainfall bonus should be at least 2 for condenser to make sense

	double bonus = sign * (conf.ai_terraforming_nutrientWeight * (totalRainfallImprovement - 2.0));

	debug("\t\tsign=%+1d, totalRainfallImprovement=%6.3f, bonus=%+6.3f\n", sign, totalRainfallImprovement, bonus);

	// return bonus

	return bonus;

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
			int mapIndex = getMapIndexByCoordinates(x + dx, y + dy);
			TileTerraformingInfo *tileTerraformingInfo = &(tileTerraformingInfos[mapIndex]);
			MAP *tile = getMapTile(mapIndex);

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

				if (!(isValidTerraformingSite(tile) && isValidConventionalTerraformingSite(tile) && tileTerraformingInfo->workableBaseIds.size() > 0))
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
Finds land path and populates first step.
*/
MAP *findPathStep(int id, int x, int y)
{
	VEH *vehicle = &(Vehicles[id]);
	MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);
	int vehicleTriad = veh_triad(id);
	int vehicleSpeed = mod_veh_speed(id);
	
	MAP *stepLocation = nullptr;

	// process land vehicles only

	if (vehicleTriad != TRIAD_LAND)
		return nullptr;

	// create processing arrays

	std::unordered_set<MAP *> processed;
	std::map<MAP *, PATH_ELEMENT> current;
	std::map<MAP *, PATH_ELEMENT> further;

	// initialize search

	further[vehicleLocation] = {vehicle->x, vehicle->y, vehicleLocation, zocLocations.count(vehicleLocation) != 0, 0, -1, -1, false};

	// run search

	for (int movementPoints = 0;;)
	{
		// check if there are further locations

		if (further.size() == 0)
			return stepLocation;

		// try to populate current set with current movementPoints

		for (;;)
		{
			for
			(
				std::map<MAP *, PATH_ELEMENT>::iterator furtherIterator = further.begin();
				furtherIterator != further.end();
				// no default increment
			)
			{
				MAP *location = furtherIterator->first;
				PATH_ELEMENT *element = &(furtherIterator->second);

				if (element->movementPoints == movementPoints)
				{
					// add to current

					current[location] = {element->x, element->y, element->tile, element->zoc, element->movementPoints, element->firstStepX, element->firstStepY, element->zocEncountered};

					// remove further

					furtherIterator = further.erase(furtherIterator);

				}
				else
				{
					// increment iterator

					furtherIterator++;

				}

			}

			if (current.size() == 0)
			{
				// increment movement points

				movementPoints++;

				// exit limit

				if (movementPoints >= 1000)
					return stepLocation;

			}
			else
			{
				// population was successful

				break;

			}

		}

		// iterate current locations

		for
		(
			std::map<MAP *, PATH_ELEMENT>::iterator currentIterator = current.begin();
			currentIterator != current.end();
			// no default increment
		)
		{
			PATH_ELEMENT *element = &(currentIterator->second);

			// check if this element is a destination

			if (element->x == x && element->y == y)
			{
				if (element->zocEncountered)
				{
					// set first step only

					stepLocation = getMapTile(element->firstStepX, element->firstStepY);

				}
				else
				{
					// set final destination

					stepLocation = getMapTile(x, y);

				}

				return stepLocation;

			}

			// evaluate adjacent tiles

			for (MAP *adjacentTile : getBaseAdjacentTiles(element->x, element->y, false))
			{
				int adjacentTileX = getX(adjacentTile);
				int adjacentTileY = getY(adjacentTile);

				bool adjacentTileZOC = zocLocations.count(adjacentTile) != 0;

				// exclude already processed and current locations

				if (processed.count(adjacentTile) != 0 || current.count(adjacentTile) != 0)
					continue;

				// exclude ocean

				if (is_ocean(adjacentTile))
					continue;

				// exclude enemy locations

				if (blockedLocations.count(adjacentTile) != 0)
					continue;

				// exclude zoc to zoc movement

				if (aiData.baseTiles.count(element->tile) == 0 && aiData.baseTiles.count(adjacentTile) == 0 && element->zoc && adjacentTileZOC)
					continue;

				// calculate movement cost

				int movementCost = mod_hex_cost(vehicle->unit_id, aiFactionId, element->x, element->y, adjacentTileX, adjacentTileY, 0);

				// calculate new movement points after move

				int wholeMoves = element->movementPoints / vehicleSpeed;
				int maxNextTurnMovementPoints = (wholeMoves + 1) * vehicleSpeed;
				int adjacentTileMovementPoints = std::min(maxNextTurnMovementPoints, element->movementPoints + movementCost);

				if (further.count(adjacentTile) == 0)
				{
					// store further location

					int firstStepX = (element->firstStepX == -1 ? adjacentTileX : element->firstStepX);
					int firstStepY = (element->firstStepY == -1 ? adjacentTileY : element->firstStepY);

					further[adjacentTile] = {adjacentTileX, adjacentTileY, adjacentTile, adjacentTileZOC, adjacentTileMovementPoints, firstStepX, firstStepY, element->zocEncountered || adjacentTileZOC};

				}
				else
				{
					// update further location

					PATH_ELEMENT *furtherLocation = &(further[adjacentTile]);

					if (adjacentTileMovementPoints < furtherLocation->movementPoints)
					{
						furtherLocation->movementPoints = adjacentTileMovementPoints;
						furtherLocation->firstStepX = element->firstStepX;
						furtherLocation->firstStepY = element->firstStepY;
						furtherLocation->zocEncountered = element->zocEncountered || adjacentTileZOC;
					}

				}

			}

			// copy current location to processed

			processed.insert(element->tile);

			// erase current location

			currentIterator = current.erase(currentIterator);

		}

	}
	
	return stepLocation;

}

bool isInferiorYield(std::vector<YIELD> *yields, int nutrient, int mineral, int energy, std::vector<YIELD>::iterator *self)
{
	bool inferior = false;

	for
	(
		std::vector<YIELD>::iterator yieldsIterator = yields->begin();
		yieldsIterator != yields->end();
		yieldsIterator++
	)
	{
		// skip self is set

		if (self != NULL && yieldsIterator == *self)
			continue;

		YIELD *yield = &(*yieldsIterator);

		if (nutrient <= yield->nutrient && mineral <= yield->mineral && energy <= yield->energy)
		{
			inferior = true;
			break;
		}

	}

	return inferior;

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
Calculates combined weighted resource score.
*/
double calculateResourceScore(double nutrient, double mineral, double energy)
{
	return
		conf.ai_terraforming_nutrientWeight * nutrient
		+
		conf.ai_terraforming_mineralWeight * mineral
		+
		conf.ai_terraforming_energyWeight * energy
	;

}

/*
Calculates combined weighted resource score taking base additional demand into account.
*/
double calculateBaseResourceScore(double populationSize, double nutrientSurplus, double mineralSurplus, double nutrient, double mineral, double energy)
{
	// calculate additional demand

	double nutrientDemand = 0.0;
	double mineralDemand = 0.0;

	// additional demand

	double nutrientThreshold = (populationSize + 1) * conf.ai_terraforming_baseNutrientThresholdRatio;
	
	if (nutrientSurplus < nutrientThreshold)
	{
		nutrientDemand = (nutrientThreshold - nutrientSurplus) * conf.ai_terraforming_baseNutrientDemandMultiplier;
	}
	
	double mineralThreshold = (nutrientSurplus) * conf.ai_terraforming_baseMineralThresholdRatio;
	
	if (mineralSurplus < mineralThreshold)
	{
		mineralDemand = (mineralThreshold - mineralSurplus) * conf.ai_terraforming_baseMineralDemandMultiplier;
	}

	return
		(conf.ai_terraforming_nutrientWeight + nutrientDemand) * nutrient
		+
		(conf.ai_terraforming_mineralWeight + mineralDemand) * mineral
		+
		conf.ai_terraforming_energyWeight * energy
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

	// calculate minimal nutrient and mineral surplus for demand calculation

	int minimalNutrientSurplus = std::min(currentNutrientSurplus, improvedNutrientSurplus);
	int minimalMineralSurplus = std::min(currentMineralSurplus, improvedMineralSurplus);

	// calculate improvement score

	double baseResourceScore =
		calculateBaseResourceScore
		(
			base->pop_size,
			minimalNutrientSurplus,
			minimalMineralSurplus,
			(improvedNutrientSurplus - currentNutrientSurplus),
			(improvedMineralSurplus - currentMineralSurplus),
			(improvedEnergySurplus - currentEnergySurplus)
		)
	;

	// return value

	return baseResourceScore;

}

bool isreachable(int id, int x, int y)
{
	VEH *vehicle = &(Vehicles[id]);
	int triad = veh_triad(id);
	MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);
	MAP *destinationLocation = getMapTile(x, y);

	return (triad == TRIAD_AIR || getConnectedRegion(vehicleLocation->region) == getConnectedRegion(destinationLocation->region));

}

int getConnectedRegion(int region)
{
	return (seaRegionMappings.count(region) == 0 ? region : seaRegionMappings[region]);
}

bool isLandRockyTile(MAP *tile)
{
	return !is_ocean(tile) && map_rockiness(tile) == 2;
}

/*
Counts number of worked but not completely improved tiles.
*/
int getUnimprovedWorkedTileCount(int baseId)
{
	int unimprovedWorkedTileCount = 0;

	for (MAP *workedTile : getBaseWorkedTiles(baseId))
	{
		// ignore already improved tiles

		if (map_has_item(workedTile, TERRA_MONOLITH | TERRA_FUNGUS | TERRA_FOREST | TERRA_MINE | TERRA_SOLAR | TERRA_CONDENSER | TERRA_ECH_MIRROR | TERRA_THERMAL_BORE))
			continue;

		// count terraforming needs

		unimprovedWorkedTileCount++;

	}
	
	return unimprovedWorkedTileCount;
	
}

int getOngoingYieldTerraformingCount(int baseId)
{
	BASE *base = &(Bases[baseId]);
	
	int ongoingYieldTerraformingCount = 0;

	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// former
		
		if (!isFormerVehicle(vehicleId))
			continue;
		
		// within base radius
		
		if (!isWithinBaseRadius(base->x, base->y, vehicle->x, vehicle->y))
			continue;
		
		// add to count if constructing yield improvement
		
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
			vehicle->move_status == ORDER_PLANT_FOREST
			||
			vehicle->move_status == ORDER_PLANT_FUNGUS
			||
			vehicle->move_status == ORDER_CONDENSER
			||
			vehicle->move_status == ORDER_ECHELON_MIRROR
			||
			vehicle->move_status == ORDER_THERMAL_BOREHOLE
		)
		{
			ongoingYieldTerraformingCount++;
		}
		
	}
	
	return ongoingYieldTerraformingCount;
	
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

	// own territory

	if (tile->owner != aiFactionId)
		return false;

	// base radius

	if (map_has_item(tile, TERRA_BASE_RADIUS))
		return false;

	// exclude base in tile

	if (tile->items & TERRA_BASE_IN_TILE)
		return false;

	// exclude volcano mouth

	if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
		return false;

	// exclude blocked locations

	if (blockedLocations.count(tile) != 0)
		return false;

	// exclude warzone locations

	if (warzoneLocations.count(tile) != 0)
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
	
	// no monolith
	
	if (map_has_item(tile, TERRA_MONOLITH))
		return false;
	
	// all conditions met

	return true;

}

