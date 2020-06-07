#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "ai.h"
#include "wtp.h"

// global variables

int factionId;
std::vector<BASE_INFO> bases;
std::unordered_set<MAP *> baseLocations;
std::unordered_map<BASE *, int> baseTerraformingRanks;
std::unordered_set<MAP *> terraformedTiles;
std::unordered_set<MAP *> terraformedBoreholeTiles;
std::unordered_set<MAP *> terraformedAquiferTiles;
std::map<int, std::vector<FORMER_ORDER>> formerOrders;
std::map<int, FORMER_ORDER> finalizedFormerOrders;
std::map<MAP *, MAP_INFO> availableTiles;
std::map<BASE *, std::vector<TERRAFORMING_REQUEST>> terraformingRequests;
std::map<int, std::vector<TERRAFORMING_REQUEST>> rankedTerraformingRequests;
std::unordered_set<MAP *> targetedTiles;
std::map<int, FORMER_ORDER *> formerOrderReferences;
std::unordered_map<int, int> vehicleCount;
std::unordered_set<MAP *> connectionNetworkLocations;

/**
AI strategy.
*/
void ai_strategy(int id)
{
	// set faction id

	factionId = id;

	debug("ai_strategy: factionId=%d\n", factionId);

	// prepare former orders

	prepareFormerOrders();

}

/**
Prepares former orders.
*/
void prepareFormerOrders()
{
	// populate processing lists

	populateLists();

	// generate terraforming requests

	generateTerraformingRequests();

	// sort terraforming requests

	sortBaseTerraformingRequests();

	// rank terraforming requests within regions

	rankTerraformingRequests();

	// assign terraforming requests to available formers

	assignFormerOrders();

	// optimize former destinations

	optimizeFormerDestinations();

	// finalize former orders

	finalizeFormerOrders();

}

/**
Populate global lists before processing faction strategy.
*/
void populateLists()
{
	// clear lists

	bases.clear();
	baseLocations.clear();
	baseTerraformingRanks.clear();
	terraformedTiles.clear();
	terraformedBoreholeTiles.clear();
	terraformedAquiferTiles.clear();
	availableTiles.clear();
	terraformingRequests.clear();
	rankedTerraformingRequests.clear();
	formerOrders.clear();
	finalizedFormerOrders.clear();
	targetedTiles.clear();
	vehicleCount.clear();
	connectionNetworkLocations.clear();

	// populate bases

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &tx_bases[baseId];

		// exclude not own bases

		if (base->faction_id != factionId)
			continue;

		// add base

		bases.push_back({baseId, base});

		// add base location

		MAP *baseLocation = mapsq(base->x, base->y);

		baseLocations.insert(baseLocation);

	}

	// populated terraformed tiles and former orders

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &tx_vehicles[id];
		MAP *vehicleTile = mapsq(vehicle->x, vehicle->y);

		if (!vehicleTile)
			continue;

		// exclude not own vehicles

		if (vehicle->faction_id != factionId)
			continue;

		// exclude not formers

		if (!isVehicleFormer(vehicle))
			continue;

		// exclude terraforming vehicles

		if (isVehicleTerraforming(vehicle))
		{
			// populate terraformed tile

			MAP *terraformedTile = mapsq(vehicle->x, vehicle->y);

			terraformedTiles.insert(terraformedTile);

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

			// update base terraforming ranks

			for
			(
				std::vector<BASE_INFO>::iterator baseIterator = bases.begin();
				baseIterator != bases.end();
				baseIterator++
			)
			{
				BASE_INFO *baseInfo = &(*baseIterator);
				BASE *base = baseInfo->base;

				// calculate base distance to terraforming vehicle

				int baseDistance = map_distance(vehicle->x, vehicle->y, base->x, base->y);

				if (baseDistance <= 2)
				{
					if (baseTerraformingRanks.count(base) == 0)
					{
						baseTerraformingRanks[base] = 0;
					}

					baseTerraformingRanks[base]++;
				}

			}

		}
		else
		{
			// get vehicle location region

			int region = vehicleTile->region;

			if (formerOrders.count(region) == 0)
			{
				// add new region

				formerOrders[region] = std::vector<FORMER_ORDER>();

			}

			// add vehicle

			formerOrders[region].push_back({id, vehicle, -1, -1, -1});

		}

	}

	// populate availalbe tiles

	for
	(
		std::vector<BASE_INFO>::iterator baseIterator = bases.begin();
		baseIterator != bases.end();
		baseIterator++
	)
	{
		BASE *base = baseIterator->base;

		// iterate base tiles

		for (int tileOffsetIndex = 1; tileOffsetIndex < BASE_TILE_OFFSET_COUNT; tileOffsetIndex++)
		{
			int x = wrap(base->x + BASE_TILE_OFFSETS[tileOffsetIndex][0]);
			int y = base->y + BASE_TILE_OFFSETS[tileOffsetIndex][1];
			MAP *tile = mapsq(x, y);

			// exclude impossible coordinates

			if (!tile)
				continue;

			// exclude volcano mouth

			if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
				continue;

			// exclude base in tile

			if (tile->items & TERRA_BASE_IN_TILE)
				continue;

			// exclude not own territory

			if (tile->owner != factionId)
				continue;

			// exclude terraformed tile

			if (terraformedTiles.count(tile) != 0)
				continue;

			// store available tile

			availableTiles[tile] = {x, y, tile};

		}

	}

	// populate own vehicle count per region

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &tx_vehicles[id];

		// exclude not own vehicles

		if (vehicle->faction_id != factionId)
			continue;

		// get vehicle location

		MAP *location = mapsq(vehicle->x, vehicle->y);

		if (!location)
			continue;

		// get region

		int region = location->region;

		// add region if if not there yet

		if (vehicleCount.count(region) == 0)
		{
			vehicleCount[region] = 0;
		}

		// add vehicle

		vehicleCount[region]++;

	}

}

void generateTerraformingRequests()
{
	for
	(
		std::map<MAP *, MAP_INFO>::iterator availableTileIterator = availableTiles.begin();
		availableTileIterator != availableTiles.end();
		availableTileIterator++
	)
	{
		MAP_INFO *availableTileMapInfo = &(availableTileIterator->second);

		generateTerraformingRequest(availableTileMapInfo);

	}

	// debug

	if (DEBUG)
	{
		debug("TERRAFORMING_REQUESTS\n");

		for
		(
			std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator terraformingRequestIterator = terraformingRequests.begin();
			terraformingRequestIterator != terraformingRequests.end();
			terraformingRequestIterator++
		)
		{
			BASE *base = terraformingRequestIterator->first;
			std::vector<TERRAFORMING_REQUEST> *baseTerraformingRequests = &(terraformingRequestIterator->second);

			debug("\tBASE: (%2d,%2d)\n", base->x, base->y);

			for
			(
				std::vector<TERRAFORMING_REQUEST>::iterator baseTerraformingRequestIterator = baseTerraformingRequests->begin();
				baseTerraformingRequestIterator != baseTerraformingRequests->end();
				baseTerraformingRequestIterator++
			)
			{
				TERRAFORMING_REQUEST *terraformingRequest = &(*baseTerraformingRequestIterator);

				debug
				(
					"\t\t(%2d,%2d) %6.4f %s\n",
					terraformingRequest->x,
					terraformingRequest->y,
					terraformingRequest->score,
					tx_terraform[terraformingRequest->action].name
				)
				;

			}

		}

		fflush(debug_log);

	}

}

void generateTerraformingRequest(MAP_INFO *mapInfo)
{
	// calculate terraforming score

	TERRAFORMING_SCORE terraformingScore;
	calculateTerraformingScore(mapInfo, &terraformingScore);

	if (terraformingScore.base != NULL && terraformingScore.action != -1 && terraformingScore.score > 0.0)
	{
		// generate terraforming request

		if (terraformingRequests.count(terraformingScore.base) == 0)
		{
			terraformingRequests[terraformingScore.base] = std::vector<TERRAFORMING_REQUEST>();
		}

		terraformingRequests[terraformingScore.base].push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore.action, terraformingScore.score, -1});

	}

}

/**
Comparison function for base terraforming requests.
Sort in scoring order descending.
*/
bool compareBaseTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2)
{
	return (terraformingRequest1.score >= terraformingRequest2.score);
}

void sortBaseTerraformingRequests()
{
	for
	(
		std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator terraformingRequestIterator = terraformingRequests.begin();
		terraformingRequestIterator != terraformingRequests.end();
		terraformingRequestIterator++
	)
	{
		std::vector<TERRAFORMING_REQUEST> *baseTerraformingRequests = &(terraformingRequestIterator->second);

		std::sort(baseTerraformingRequests->begin(), baseTerraformingRequests->end(), compareBaseTerraformingRequests);

	}

	// debug

	if (DEBUG)
	{
		debug("TERRAFORMING_REQUESTS - SORTED\n");

		for
		(
			std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator terraformingRequestIterator = terraformingRequests.begin();
			terraformingRequestIterator != terraformingRequests.end();
			terraformingRequestIterator++
		)
		{
			BASE *base = terraformingRequestIterator->first;
			std::vector<TERRAFORMING_REQUEST> *baseTerraformingRequests = &(terraformingRequestIterator->second);

			debug("\tBASE: (%2d,%2d)\n", base->x, base->y);

			for
			(
				std::vector<TERRAFORMING_REQUEST>::iterator baseTerraformingRequestIterator = baseTerraformingRequests->begin();
				baseTerraformingRequestIterator != baseTerraformingRequests->end();
				baseTerraformingRequestIterator++
			)
			{
				TERRAFORMING_REQUEST *terraformingRequest = &(*baseTerraformingRequestIterator);

				debug
				(
					"\t\t(%2d,%2d) %6.4f %s\n",
					terraformingRequest->x,
					terraformingRequest->y,
					terraformingRequest->score,
					tx_terraform[terraformingRequest->action].name
				)
				;

			}

		}

		fflush(debug_log);

	}

}

/**
Comparison function for terraforming requests.
Sort in ranking order ascending then in scoring order descending.
*/
bool compareTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2)
{
	return (terraformingRequest1.rank != terraformingRequest2.rank ? terraformingRequest1.rank < terraformingRequest2.rank : terraformingRequest1.score >= terraformingRequest2.score);
}

/**
Sort terraforming requests within regions by rank and score.
*/
void rankTerraformingRequests()
{
	// distribute terraforming requests by regions

	for (size_t rank = 0; ; rank++)
	{
		bool collected = false;

		for
		(
			std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator terraformingRequestIterator = terraformingRequests.begin();
			terraformingRequestIterator != terraformingRequests.end();
			terraformingRequestIterator++
		)
		{
			BASE *base = terraformingRequestIterator->first;
			std::vector<TERRAFORMING_REQUEST> *baseTerraformingRequests = &(terraformingRequestIterator->second);

			// skip exhausted bases

			if (rank >= baseTerraformingRequests->size())
				continue;

			// collect request

			TERRAFORMING_REQUEST *terraformingRequest = &(baseTerraformingRequests->operator[](rank));
			terraformingRequest->rank = getBaseTerraformingRank(base) + rank;

			// get region

			int region = terraformingRequest->tile->region;

			// create region bucket if needed

			if (rankedTerraformingRequests.count(region) == 0)
			{
				rankedTerraformingRequests[region] = std::vector<TERRAFORMING_REQUEST>();
			}

			// add terraforming request

			rankedTerraformingRequests[region].push_back(*terraformingRequest);

			// set collected flag

			collected = true;

		}

		// check exit condition

		if (!collected)
			break;

	}

	// sort terraforming requests within regions

	for
	(
		std::map<int, std::vector<TERRAFORMING_REQUEST>>::iterator rankedTerraformingRequestIterator = rankedTerraformingRequests.begin();
		rankedTerraformingRequestIterator != rankedTerraformingRequests.end();
		rankedTerraformingRequestIterator++
	)
	{
		std::vector<TERRAFORMING_REQUEST> *regionTerraformingRequests = &(rankedTerraformingRequestIterator->second);

		std::sort(regionTerraformingRequests->begin(), regionTerraformingRequests->end(), compareTerraformingRequests);

	}

	// debug

	if (DEBUG)
	{
		debug("TERRAFORMING_REQUESTS - RANKED\n");

		for
		(
			std::map<int, std::vector<TERRAFORMING_REQUEST>>::iterator rankedTerraformingRequestIterator = rankedTerraformingRequests.begin();
			rankedTerraformingRequestIterator != rankedTerraformingRequests.end();
			rankedTerraformingRequestIterator++
		)
		{
			int region = rankedTerraformingRequestIterator->first;
			std::vector<TERRAFORMING_REQUEST> *regionRankedTerraformingRequests = &(rankedTerraformingRequestIterator->second);

			debug("\tregion: %2d\n", region);

			for
			(
				std::vector<TERRAFORMING_REQUEST>::iterator regionRankedTerraformingRequestIterator = regionRankedTerraformingRequests->begin();
				regionRankedTerraformingRequestIterator != regionRankedTerraformingRequests->end();
				regionRankedTerraformingRequestIterator++
			)
			{
				TERRAFORMING_REQUEST *terraformingRequest = &(*regionRankedTerraformingRequestIterator);

				debug
				(
					"\t\t(%2d,%2d) %d %6.4f %s\n",
					terraformingRequest->x,
					terraformingRequest->y,
					terraformingRequest->rank,
					terraformingRequest->score,
					tx_terraform[terraformingRequest->action].name
				)
				;

			}

		}

		fflush(debug_log);

	}

}

void assignFormerOrders()
{
	for
	(
		std::map<int, std::vector<FORMER_ORDER>>::iterator formerOrderIterator = formerOrders.begin();
		formerOrderIterator != formerOrders.end();
		formerOrderIterator++
	)
	{
		int region = formerOrderIterator->first;
		std::vector<FORMER_ORDER> *regionFormerOrders = &(formerOrderIterator->second);

		// skip formers without request in this region

		if (rankedTerraformingRequests.count(region) == 0)
			continue;

		std::vector<TERRAFORMING_REQUEST> *regionRankedTerraformingRequests = &(rankedTerraformingRequests[region]);

		for (size_t i = 0; i < min(regionFormerOrders->size(), regionRankedTerraformingRequests->size()); i++)
		{
			regionFormerOrders->operator[](i).x = regionRankedTerraformingRequests->operator[](i).x;
			regionFormerOrders->operator[](i).y = regionRankedTerraformingRequests->operator[](i).y;
			regionFormerOrders->operator[](i).action = regionRankedTerraformingRequests->operator[](i).action;

		}

	}

}

/**
Selects best terraforming option for given tile and calculates its terraforming score.
*/
void calculateTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;

	debug
	(
		"calculateTerraformingScore: (%2d,%2d)\n",
		x,
		y
	)

	bool sea = is_ocean(mapInfo->tile);

	// connection network parameters

	int regionVehicleCount = getRegionVehicleCount(tile->region);

	// store tile values in the 3x3 grid area

	TERRAFORMING_STATE currentLocalTerraformingState[9];
	storeLocalTerraformingState(x, y, currentLocalTerraformingState);

	// find best terraforming option

	for (int optionIndex = 0; optionIndex < TERRAFORMING_OPTION_COUNT; optionIndex++)
	{
		TERRAFORMING_OPTION option = TERRAFORMING_OPTIONS[optionIndex];

		// skip wrong body type

		if (option.sea != sea)
			continue;

		// skip wrong terrain type

		if (option.rocky && !(tile->rocks & TILE_ROCKY))
			continue;

		// initialize variables

		TERRAFORMING_STATE improvedLocalTerraformingState[9];
		copyLocalTerraformingState(currentLocalTerraformingState, improvedLocalTerraformingState);

		TERRAFORMING_STATE *improvedTileTerraformedState = &improvedLocalTerraformingState[0];
		int terraformingTime = 0;
		int firstAction = -1;
		int affectedRange = 0;
		double improvementScore = 0.0;

		// process actions

		for (int actionIndex = 0; actionIndex < option.count; actionIndex++)
		{
			int action = option.actions[actionIndex];

			// skip unavailable actions

			if (!isTerraformingAvailable(x, y, action))
				continue;

			// skip unneeded actions

			if (!isTerraformingRequired(tile, action))
				continue;

			// remove fungus if needed

			if ((improvedTileTerraformedState->items & TERRA_FUNGUS) && isRemoveFungusRequired(action))
			{
				int removeFungusAction = FORMER_REMOVE_FUNGUS;

				// compute terraforming time

				terraformingTime +=
					calculateTerraformingTime
					(
						removeFungusAction,
						improvedTileTerraformedState->items,
						improvedTileTerraformedState->rocks,
						NULL
					)
				;

				// generate terraforming change

				generateTerraformingChange(improvedTileTerraformedState, removeFungusAction);

				// store first removeFungusAction

				if (firstAction == -1)
				{
					firstAction = removeFungusAction;
				}

			}

			// level terrain if needed

			if ((improvedTileTerraformedState->rocks & TILE_ROCKY) && isLevelTerrainRequired(action))
			{
				int levelTerrainAction = FORMER_LEVEL_TERRAIN;

				// compute terraforming time

				terraformingTime +=
					calculateTerraformingTime
					(
						levelTerrainAction,
						improvedTileTerraformedState->items,
						improvedTileTerraformedState->rocks,
						NULL
					)
				;

				// generate terraforming change

				generateTerraformingChange(improvedTileTerraformedState, levelTerrainAction);

				// store first levelTerrainAction

				if (firstAction == -1)
				{
					firstAction = levelTerrainAction;
				}

			}

			// execute main action

			// compute terraforming time

			terraformingTime +=
				calculateTerraformingTime
				(
					action,
					improvedTileTerraformedState->items,
					improvedTileTerraformedState->rocks,
					NULL
				)
			;

			// generate terraforming change

			generateTerraformingChange(improvedTileTerraformedState, action);

			// store first action

			if (firstAction == -1)
			{
				firstAction = action;
			}

			// special cases

			switch (action)
			{
			case FORMER_ROAD:
				// add road bonus
				improvementScore += conf.ai_terraforming_roadWeight;
				break;

			case FORMER_MAGTUBE:
				// add road bonus
				improvementScore += conf.ai_terraforming_tubeWeight;
				break;

			case FORMER_CONDENSER:
				// condenser increases moisture around
				increaseMoistureAround(improvedTileTerraformedState);
				affectedRange = max(affectedRange, 1);
				break;

			case FORMER_ECH_MIRROR:
				// mirror affects collectors around
				affectedRange = max(affectedRange, 1);
				break;

			case FORMER_AQUIFER:
				// aquifer creates rivers around (supposedly)
				createRiversAround(improvedTileTerraformedState);
				affectedRange = max(affectedRange, 1);
				break;

			}

			// special bonus for connecting network

			if ((action == FORMER_ROAD || action == FORMER_MAGTUBE) && isConnectionNetworkLocation(x, y, tile, action))
			{
				improvementScore += conf.ai_terraforming_connectionNetworkFactor * regionVehicleCount;
			}

		}

		// calculate yield improvement score

		TERRAFORMING_SCORE optionTerraformingScore;
		calculateYieldImprovementScore(mapInfo->x, mapInfo->y, affectedRange, currentLocalTerraformingState, improvedLocalTerraformingState, &optionTerraformingScore);

		// skip ineffective improvements

		if (optionTerraformingScore.base == NULL || optionTerraformingScore.score <= 0.0)
			continue;

		// update improvement score

		improvementScore += optionTerraformingScore.score;

		// factor improvement score with decay coefficient

		double terraformingScore = improvementScore * exp(-terraformingTime / conf.ai_terraforming_resourceLifetime);

		debug
		(
			"\t%2d: improvementScore=%6.4f, terraformingTime=%2d, terraformingScore=%6.4f\n",
			optionIndex,
			improvementScore,
			terraformingTime,
			terraformingScore
		)
		;

		// update score

		if (firstAction != -1 && optionTerraformingScore.base != NULL && terraformingScore > bestTerraformingScore->score)
		{
			bestTerraformingScore->score = terraformingScore;
			bestTerraformingScore->base = optionTerraformingScore.base;
			bestTerraformingScore->action = firstAction;

		}

	}

//	debug
//	(
//		"\tbestTerraformingScore=%f, bestTerraformingOptionIndex=%d, bestTerraformingAction=%s\n",
//		bestTerraformingScore->score,
//		bestTerraformingOptionIndex,
//		(bestTerraformingScore->action == -1 ? "none" : tx_terraform[bestTerraformingScore->action].name)
//	)
//	;
//
}

void optimizeFormerDestinations()
{
	// iterate through regions

	for
	(
		std::map<int, std::vector<FORMER_ORDER>>::iterator formerOrderIterator = formerOrders.begin();
		formerOrderIterator != formerOrders.end();
		formerOrderIterator++
	)
	{
		std::vector<FORMER_ORDER> *regionFormerOrders = &(formerOrderIterator->second);

		// iterate until optimized or max iterations

		for (int iteration = 0; iteration < 100; iteration++)
		{
			bool swapped = false;

			// iterate through former order pairs

			for
			(
				std::vector<FORMER_ORDER>::iterator regionFormerOrderIterator1 = regionFormerOrders->begin();
				regionFormerOrderIterator1 != regionFormerOrders->end();
				regionFormerOrderIterator1++
			)
			{
				for
				(
					std::vector<FORMER_ORDER>::iterator regionFormerOrderIterator2 = regionFormerOrders->begin();
					regionFormerOrderIterator2 != regionFormerOrders->end();
					regionFormerOrderIterator2++
				)
				{
					FORMER_ORDER *formerOrder1 = &(*regionFormerOrderIterator1);
					FORMER_ORDER *formerOrder2 = &(*regionFormerOrderIterator2);

					// skip itself

					if (formerOrder1->id == formerOrder2->id)
						continue;

					VEH *vehicle1 = formerOrder1->vehicle;
					VEH *vehicle2 = formerOrder2->vehicle;

					int vehicle1Speed = veh_speed_without_roads(formerOrder1->id);
					int vehicle2Speed = veh_speed_without_roads(formerOrder2->id);

					int destination1X = formerOrder1->x;
					int destination1Y = formerOrder1->y;
					MAP *destination1Tile = mapsq(destination1X, destination1Y);
					int destination2X = formerOrder2->x;
					int destination2Y = formerOrder2->y;
					MAP *destination2Tile = mapsq(destination2X, destination2Y);

					int action1 = formerOrder1->action;
					int action2 = formerOrder2->action;

					// calculate current travel time

					double vehicle1CurrentTravelTime = (double)map_range(vehicle1->x, vehicle1->y, destination1X, destination1Y) / (double)vehicle1Speed;
					double vehicle2CurrentTravelTime = (double)map_range(vehicle2->x, vehicle2->y, destination2X, destination2Y) / (double)vehicle2Speed;

					double currentTravelTime = vehicle1CurrentTravelTime + vehicle2CurrentTravelTime;

					// calculate current terraforming time

					double vehicle1CurrentTerraformingTime = calculateTerraformingTime(action1, destination1Tile->items, destination1Tile->rocks, vehicle1);
					double vehicle2CurrentTerraformingTime = calculateTerraformingTime(action2, destination2Tile->items, destination2Tile->rocks, vehicle2);

					double currentTerraformingTime = vehicle1CurrentTerraformingTime + vehicle2CurrentTerraformingTime;

					// calculate current completion time

					double currentCompletionTime = currentTravelTime + currentTerraformingTime;

					// calculate swapped travel time

					double vehicle1SwappedTravelTime = (double)map_range(vehicle1->x, vehicle1->y, destination2X, destination2Y) / (double)vehicle1Speed;
					double vehicle2SwappedTravelTime = (double)map_range(vehicle2->x, vehicle2->y, destination1X, destination1Y) / (double)vehicle2Speed;

					double swappedTravelTime = vehicle1SwappedTravelTime + vehicle2SwappedTravelTime;

					// calculate swapped terraforming time

					double vehicle1SwappedTerraformingTime = calculateTerraformingTime(action2, destination2Tile->items, destination2Tile->rocks, vehicle1);
					double vehicle2SwappedTerraformingTime = calculateTerraformingTime(action1, destination1Tile->items, destination1Tile->rocks, vehicle2);

					double swappedTerraformingTime = vehicle1SwappedTerraformingTime + vehicle2SwappedTerraformingTime;

					// calculate swapped completion time

					double swappedCompletionTime = swappedTravelTime + swappedTerraformingTime;

					// swap orders if beneficial

					if (swappedCompletionTime < currentCompletionTime)
					{
						formerOrder1->x = destination2X;
						formerOrder1->y = destination2Y;
						formerOrder1->action = action2;

						formerOrder2->x = destination1X;
						formerOrder2->y = destination1Y;
						formerOrder2->action = action1;

						swapped = true;

					}

				}

			}

			// stop cycle if nothing is swapped

			if (!swapped)
				break;

		}

	}

}

void finalizeFormerOrders()
{
	// iterate former orders

	for
	(
		std::map<int, std::vector<FORMER_ORDER>>::iterator formerOrderIterator = formerOrders.begin();
		formerOrderIterator != formerOrders.end();
		formerOrderIterator++
	)
	{
		std::vector<FORMER_ORDER> *regionFormerOrders = &(formerOrderIterator->second);

		for
		(
			std::vector<FORMER_ORDER>::iterator regionFormerOrderIterator = regionFormerOrders->begin();
			regionFormerOrderIterator != regionFormerOrders->end();
			regionFormerOrderIterator++
		)
		{
			FORMER_ORDER *regionFormerOrder = &(*regionFormerOrderIterator);

			finalizedFormerOrders[regionFormerOrder->id] = *regionFormerOrder;

		}

	}

	// debug

	if (DEBUG)
	{
		debug("FORMER ORDERS - FINALIZED\n");

		for
		(
			std::map<int, FORMER_ORDER>::iterator finalizedFormerOrderIterator = finalizedFormerOrders.begin();
			finalizedFormerOrderIterator != finalizedFormerOrders.end();
			finalizedFormerOrderIterator++
		)
		{
			int id = finalizedFormerOrderIterator->first;
			FORMER_ORDER *formerOrder = &(finalizedFormerOrderIterator->second);

			debug
			(
				"\t[%4d] (%2d,%2d) %s\n",
				id,
				formerOrder->x,
				formerOrder->y,
				tx_terraform[formerOrder->action].name
			)
			;

		}

		fflush(debug_log);

	}

}

/**
Handles movement phase.
*/
int enemyMoveFormer(int id)
{
	// skip vehicles without former orders

	if (finalizedFormerOrders.count(id) == 0)
		return SYNC;

	// get former order

	FORMER_ORDER formerOrder = finalizedFormerOrders[id];

	// skip orders without action

	if (formerOrder.action == -1)
		return SYNC;

	// execute order

	setFormerOrder(formerOrder);

	return SYNC;

}

void setFormerOrder(FORMER_ORDER formerOrder)
{
	int id = formerOrder.id;
	VEH *vehicle = formerOrder.vehicle;
	int x = formerOrder.x;
	int y = formerOrder.y;
	int action = formerOrder.action;

	// at destination
	if (vehicle->x == x && vehicle->y == y)
	{
		// execute terraforming action

		setTerraformingAction(id, action);

		debug
		(
			"generateFormerOrder: location=(%d,%d), action=%s\n",
			vehicle->x,
			vehicle->y,
			tx_terraform[action].name
		)
		;

	}
	// not at destination
	else
	{
		// send former to destination

		sendVehicleToDestination(id, x, y);

		debug
		(
			"generateFormerOrder: location=(%d,%d), move_to(%d,%d)\n",
			vehicle->x,
			vehicle->y,
			x,
			y
		)

	}

}

void calculateYieldImprovementScore(int x, int y, int affectedRange, TERRAFORMING_STATE currentLocalTerraformingState[9], TERRAFORMING_STATE improvedLocalTerraformingState[9], TERRAFORMING_SCORE *terraformingScore)
{
	int affectedBaseRange = 4 + affectedRange;

	// list nearby bases

	std::vector<BASE_INCOME> nearbyBases;

	for
	(
		std::vector<BASE_INFO>::iterator baseIterator = bases.begin();
		baseIterator != bases.end();
		baseIterator++
	)
	{
		BASE_INFO baseInfo = *baseIterator;
		int id = baseInfo.id;
		BASE *base = baseInfo.base;

		// calculate range

		int absDx = abs(base->x - x);
		int absDy = abs(base->y - y);

		// skip too far bases

		if ((absDx + absDy > affectedBaseRange) || (absDx >= affectedBaseRange) || (absDy >= affectedBaseRange))
			continue;

        // compute base

        base->worked_tiles = 0;
		computeBase(id);

		// calculate surplus

		int nutrientSurplus = base->nutrient_surplus;
		int mineralSurplus = base->mineral_surplus;
		int energySurplus = base->economy_total + base->psych_total + base->labs_total;

		// add base

		nearbyBases.push_back({id, base, base->pop_size, base->worked_tiles, nutrientSurplus, mineralSurplus, energySurplus});

	}

	// set improved local terraforming state

	setLocalTerraformingState(x, y, improvedLocalTerraformingState);

	// calculate yield improvement score

	double yieldImprovementScore = 0.0;

	for
	(
		std::vector<BASE_INCOME>::iterator baseIterator = nearbyBases.begin();
		baseIterator != nearbyBases.end();
		baseIterator++
	)
	{
		BASE_INCOME *baseIncome = &(*baseIterator);
		int id = baseIncome->id;
		BASE *base = baseIncome->base;

		// set current base

        tx_set_base(id);

        // compute base

        base->worked_tiles = 0;
		computeBase(id);

		// calculate improved surplus increase

		int nutrientSurplusIncrease = base->nutrient_surplus - baseIncome->nutrientSurplus;
		int mineralSurplusIncrease = base->mineral_surplus - baseIncome->mineralSurplus;
		int energySurplusIncrease = (base->economy_total + base->psych_total + base->labs_total) - baseIncome->energySurplus;

		// calculate base yield score

		double baseYieldImprovementScore =
			conf.ai_terraforming_nutrientWeight * (double)nutrientSurplusIncrease
			+
			conf.ai_terraforming_mineralWeight * (double)mineralSurplusIncrease
			+
			conf.ai_terraforming_energyWeight * (double)energySurplusIncrease
		;

		// set affected base

		if (baseYieldImprovementScore > 0.0)
		{
			terraformingScore->base = base;
		}

		// accumulate total score

		yieldImprovementScore += baseYieldImprovementScore;

	}

	// set current local terraforming state

	setLocalTerraformingState(x, y, currentLocalTerraformingState);

	// restore bases

	for
	(
		std::vector<BASE_INCOME>::iterator baseIterator = nearbyBases.begin();
		baseIterator != nearbyBases.end();
		baseIterator++
	)
	{
		BASE_INCOME baseIncome = *baseIterator;
		int id = baseIncome.id;
		BASE *base = baseIncome.base;

		// set current base

        tx_set_base(id);

		// restore population

        base->pop_size = baseIncome.population;

		// restore worked tiles

        base->worked_tiles = baseIncome.workedTiles;

        // compute base

        base->worked_tiles = 0;
		computeBase(id);

    }

	terraformingScore->score = yieldImprovementScore;

}

/**
Checks whether tile is own workable tile but not base square.
*/
bool isOwnWorkableTile(int x, int y)
{
	MAP *tile = mapsq(x, y);

	// exclude base squares

	if (tile->items & TERRA_BASE_IN_TILE)
		return false;

	// exclude not own territory

	if (tile->owner != factionId)
		return false;

	// iterate through own bases

	for (int baseIndex = 0; baseIndex < *total_num_bases; baseIndex++)
	{
		BASE *base = &tx_bases[baseIndex];

		// skip not own bases

		if (base->faction_id != factionId)
			continue;

		// check base radius

		return isWithinBaseRadius(base->x, base->y, x, y);

	}

	// matched workable tile not found

	return false;

}

/**
Determines whether terraforming can be done in this square with removing fungus and leveling terrain if needed.
*/
bool isTerraformingAvailable(int x, int y, int action)
{
	MAP *tile = mapsq(x, y);

	// action is available only when enabled and researched

	if (!has_terra(factionId, action, is_ocean(tile)))
		return false;

	// terraforming is not available in base square

	if (tile->items & TERRA_BASE_IN_TILE)
		return false;

	// check if improvement already exists

	if (tile->items & tx_terraform[action].added_items_flag)
		return false;

	// special cases

	switch (action)
	{
	case FORMER_SOIL_ENR:
		// enricher requires farm
		return (tile->items & TERRA_FARM);
		break;
	case FORMER_MAGTUBE:
		// tube requires road
		return (tile->items & TERRA_ROAD);
		break;
	case FORMER_REMOVE_FUNGUS:
		// removing fungus requires fungus
		return (tile->items & TERRA_FUNGUS);
		break;
	case FORMER_THERMAL_BORE:
		// borehole should not be adjacent to another borehole
		return !isAdjacentBoreholePresentOrUnderConstruction(x, y);
		break;
	case FORMER_AQUIFER:
		// aquifer cannot be drilled next to river
		return !isAdjacentRiverPresentOrUnderConstruction(x, y);
		break;
	case FORMER_RAISE_LAND:
		// raise land should not disturb other faction bases
		// TODO
		return false;
		break;
	case FORMER_LOWER_LAND:
		// lower land should not disturb other faction bases
		// TODO
		return false;
		break;
	}

	return true;

}

/**
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
		(has_project(factionId, FAC_XENOEMPATHY_DOME))
	)
	{
		return false;
	}

	// all other cases are required

	return true;

}

/**
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

	if (action == FORMER_ROAD && has_tech(factionId, tx_basic->tech_preq_build_road_fungus))
		return false;

	// for everything else remove fungus only without fungus improvement technology

	return !has_tech(factionId, tx_basic->tech_preq_improv_fungus);

}

/**
Determines whether rocky terrain need to be leveled before terraforming.
*/
bool isLevelTerrainRequired(int action)
{
	// level rocky terrain for farm, enricher, forest

	return (action == FORMER_FARM || action == FORMER_SOIL_ENR || action == FORMER_FOREST);

}

void generateTerraformingChange(TERRAFORMING_STATE *terraformingState, int action)
{
	// level terrain changes rockiness
	if (action == FORMER_LEVEL_TERRAIN)
	{
		if (terraformingState->rocks & TILE_ROCKY)
		{
			terraformingState->rocks &= ~TILE_ROCKY;
			terraformingState->rocks |= TILE_ROLLING;
		}
		else if (terraformingState->rocks & TILE_ROLLING)
		{
			terraformingState->rocks &= ~TILE_ROLLING;
		}

	}
	// other actions change items
	else
	{
		// remove items

		terraformingState->items &= ~tx_terraform[action].removed_items_flag;

		// add items

		terraformingState->items |= tx_terraform[action].added_items_flag;

	}

}

bool isVehicleFormer(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_TERRAFORMING_UNIT);
}

bool isTileTargettedByVehicle(VEH *vehicle, MAP *tile)
{
	return (vehicle->move_status == ORDER_MOVE_TO && (getVehicleDestination(vehicle) == tile));
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

	return mapsq(x, y);

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

void computeBase(int baseId)
{
	tx_set_base(baseId);
	tx_base_compute(1);

}

void setTerraformingAction(int vehicleId, int action)
{
	set_action(vehicleId, action + 4, veh_status_icon[action + 4]);
}

/**
Orders former to build other improvement besides base yield change.
*/
void buildImprovement(int vehicleId)
{
	VEH *vehicle = &tx_vehicles[vehicleId];

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

void sendVehicleToDestination(int vehicleId, int x, int y)
{
	tx_go_to(vehicleId, veh_status_icon[ORDER_MOVE_TO], x, y);
}

bool isAdjacentBoreholePresentOrUnderConstruction(int x, int y)
{
	if (nearby_items(x, y, 1, TERRA_THERMAL_BORE) >= 1)
		return true;

	// check terraformed boreholes

	int range = 1;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			MAP *tile = mapsq(wrap(x + dx), y + dy);

			if (terraformedBoreholeTiles.count(tile) == 1)
                return true;

        }

    }

    return false;

}

bool isAdjacentRiverPresentOrUnderConstruction(int x, int y)
{
	if (nearby_items(x, y, 1, TERRA_RIVER) >= 1)
		return true;

	// check terraformed aqufiers

	int range = 1;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			MAP *tile = mapsq(wrap(x + dx), y + dy);

			if (terraformedAquiferTiles.count(tile) == 1)
                return true;

        }

    }

    return false;

}

void storeLocalTerraformingState(int x, int y, TERRAFORMING_STATE localTerraformingState[9])
{
	for (int tileOffsetIndex = 0; tileOffsetIndex < ADJACENT_TILE_OFFSET_COUNT; tileOffsetIndex++)
	{
		MAP *tile = mapsq(wrap(x + BASE_TILE_OFFSETS[tileOffsetIndex][0]), y + BASE_TILE_OFFSETS[tileOffsetIndex][1]);

		if (tile)
		{
			localTerraformingState[tileOffsetIndex].climate = tile->level;
			localTerraformingState[tileOffsetIndex].rocks = tile->rocks;
			localTerraformingState[tileOffsetIndex].items = tile->items;
		}

	}

}

void setLocalTerraformingState(int x, int y, TERRAFORMING_STATE localTerraformingState[9])
{
	for (int tileOffsetIndex = 0; tileOffsetIndex < ADJACENT_TILE_OFFSET_COUNT; tileOffsetIndex++)
	{
		MAP *tile = mapsq(wrap(x + BASE_TILE_OFFSETS[tileOffsetIndex][0]), y + BASE_TILE_OFFSETS[tileOffsetIndex][1]);

		if (tile)
		{
			tile->level = localTerraformingState[tileOffsetIndex].climate;
			tile->rocks = localTerraformingState[tileOffsetIndex].rocks;
			tile->items = localTerraformingState[tileOffsetIndex].items;
		}

	}

}

void copyLocalTerraformingState(TERRAFORMING_STATE sourceLocalTerraformingState[9], TERRAFORMING_STATE destinationLocalTerraformingState[9])
{
	for (int tileOffsetIndex = 0; tileOffsetIndex < ADJACENT_TILE_OFFSET_COUNT; tileOffsetIndex++)
	{
		destinationLocalTerraformingState[tileOffsetIndex].climate = sourceLocalTerraformingState[tileOffsetIndex].climate;
		destinationLocalTerraformingState[tileOffsetIndex].rocks = sourceLocalTerraformingState[tileOffsetIndex].rocks;
		destinationLocalTerraformingState[tileOffsetIndex].items = sourceLocalTerraformingState[tileOffsetIndex].items;

	}

}

void increaseMoistureAround(TERRAFORMING_STATE localTerraformingState[9])
{
	for (int tileOffsetIndex = 0; tileOffsetIndex < ADJACENT_TILE_OFFSET_COUNT; tileOffsetIndex++)
	{
		switch (localTerraformingState[tileOffsetIndex].climate & (byte)0x18)
		{
		case 0x00:
			localTerraformingState[tileOffsetIndex].climate |= (byte)0x08;
			break;
		case 0x08:
			localTerraformingState[tileOffsetIndex].climate &= ~((byte)0x08);
			localTerraformingState[tileOffsetIndex].climate |= (byte)0x10;
			break;
		}

	}

}

/**
Creates rivers around a square.
The higher the altitude the more rivers.
*/
void createRiversAround(TERRAFORMING_STATE localTerraformingState[9])
{
	localTerraformingState[0].items |= TERRA_RIVER;

	int extraRiverCount = (((localTerraformingState[0].climate & 0xE0) - 0x40) >> 5) * 2;

	for (int tileOffsetIndex = 1; tileOffsetIndex < ADJACENT_TILE_OFFSET_COUNT; tileOffsetIndex++)
	{
		if (random(8) < extraRiverCount)
		{
			localTerraformingState[tileOffsetIndex].items |= TERRA_RIVER;
		}

	}

}

/**
Calculates terraforming time based on faction, tile, and former abilities.
*/
int calculateTerraformingTime(int action, int items, int rocks, VEH* vehicle)
{
	assert(action >= FORMER_FARM && action <= FORMER_MONOLITH);

	// get base terraforming time

	int terraformingTime = tx_terraform[action].rate;

	// adjust for terrain properties

	switch (action)
	{
	case FORMER_SOLAR:
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
		if (items & TERRA_FOREST)
		{
			terraformingTime += 2;
		}
		else
		{
			if (items & TERRA_FUNGUS)
			{
				terraformingTime += 2;
			}

			if (rocks & TILE_ROLLING)
			{
				terraformingTime += 1;
			}
			else if (rocks & TILE_ROCKY)
			{
				terraformingTime += 2;
			}

		}

	}

	// adjust for faction projects

	if (has_project(factionId, FAC_WEATHER_PARADIGM))
	{
		// reduce all terraforming time except fungus by 1/3

		if (action != FORMER_REMOVE_FUNGUS && action != FORMER_PLANT_FUNGUS)
		{
			terraformingTime -= terraformingTime / 3;
		}

	}

	if (has_project(factionId, FAC_XENOEMPATHY_DOME))
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

/**
Check if tile is a part of base local connection network or connection for unconnected roads.
improvementFlag: TERRA_ROAD or TERRA_MAGTUBE
*/
bool isConnectionNetworkLocation(int x, int y, MAP *tile, int improvementFlag)
{
	// no connection on ocean

	if (is_ocean(tile))
		return false;

	// check for base connection tiles

	for (int offsetIndex = 0; offsetIndex < BASE_CONNECTION_TILE_OFFSET_COUNT; offsetIndex++)
	{
		MAP *baseConnectionTile = mapsq(wrap(x + BASE_CONNECTION_TILE_OFFSETS[offsetIndex][0]), x + BASE_CONNECTION_TILE_OFFSETS[offsetIndex][1]);

		if
		(
			baseConnectionTile
			&&
			baseLocations.count(baseConnectionTile) != 0
			&&
			baseConnectionTile->region == tile->region
		)
			return true;

	}

	// ignore already built improvement

	if (tile->items & improvementFlag)
		return false;

	// check improvements around the tile

	bool improvements[ADJACENT_TILE_OFFSET_COUNT];

	for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
	{
		MAP *adjacentTile = mapsq(wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]), y + BASE_TILE_OFFSETS[offsetIndex][1]);

		improvements[offsetIndex] = (adjacentTile && adjacentTile->items & improvementFlag);

	}

	// check for connecting unconnected roads on opposite sides

	if
	(
		// opposite corners
		(improvements[2] && improvements[6])
		||
		(improvements[4] && improvements[8])
		||
		// next corners
		(improvements[2] && improvements[4] && !improvements[3])
		||
		(improvements[4] && improvements[6] && !improvements[5])
		||
		(improvements[6] && improvements[8] && !improvements[7])
		||
		(improvements[8] && improvements[2] && !improvements[1])
		||
		// opposite sides
		(improvements[1] && improvements[5] && !improvements[3] && !improvements[7])
		||
		(improvements[3] && improvements[7] && !improvements[5] && !improvements[1])
		||
		// side to corner
		(improvements[1] && improvements[4] && !improvements[3])
		||
		(improvements[1] && improvements[6] && !improvements[7])
		||
		(improvements[3] && improvements[6] && !improvements[5])
		||
		(improvements[3] && improvements[8] && !improvements[1])
		||
		(improvements[5] && improvements[8] && !improvements[7])
		||
		(improvements[5] && improvements[2] && !improvements[3])
		||
		(improvements[7] && improvements[2] && !improvements[1])
		||
		(improvements[7] && improvements[4] && !improvements[5])
	)
		return true;

	// false otherwise

	return false;

}

int getRegionVehicleCount(int region)
{
	std::unordered_map<int, int>::iterator regionVehicleCountIterator = vehicleCount.find(region);
	return (regionVehicleCountIterator == vehicleCount.end() ? 0 : regionVehicleCountIterator->second);
}

int getBaseTerraformingRank(BASE *base)
{
	return (baseTerraformingRanks.count(base) == 0 ? 0 : baseTerraformingRanks[base]);
}
