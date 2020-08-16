#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "aiTerraforming.h"
#include "ai.h"
#include "wtp.h"
#include "game.h"

double networkDemand = 0.0;
std::unordered_map<MAP *, BASE_INFO> workedTileBases;
std::unordered_map<BASE *, int> baseTerraformingRanks;
std::unordered_set<MAP *> harvestedTiles;
std::unordered_set<MAP *> terraformedTiles;
std::unordered_set<MAP *> terraformedForestTiles;
std::unordered_set<MAP *> terraformedCondenserTiles;
std::unordered_set<MAP *> terraformedMirrorTiles;
std::unordered_set<MAP *> terraformedBoreholeTiles;
std::unordered_set<MAP *> terraformedAquiferTiles;
std::unordered_set<MAP *> terraformedRaiseTiles;
std::map<int, std::vector<FORMER_ORDER>> regionFormerOrders;
std::map<int, FORMER_ORDER> vehicleFormerOrders;
std::vector<MAP_INFO> territoryTiles;
std::map<MAP *, MAP_INFO> workableTiles;
std::map<MAP *, MAP_INFO> availableTiles;
std::map<BASE *, std::vector<TERRAFORMING_REQUEST>> baseTerraformingRequests;
std::vector<TERRAFORMING_REQUEST> freeTerraformingRequests;
std::map<int, std::vector<TERRAFORMING_REQUEST>> regionTerraformingRequests;
std::unordered_set<MAP *> targetedTiles;
std::unordered_set<MAP *> connectionNetworkLocations;
std::unordered_map<MAP *, std::unordered_map<int, std::vector<BASE_INFO>>> nearbyBaseSets;
std::unordered_map<int, int> seaRegionMappings;
std::unordered_map<MAP *, int> coastalBaseRegionMappings;
std::unordered_set<MAP *> blockedLocations;
std::unordered_set<MAP *> warzoneLocations;
std::unordered_set<MAP *> zocLocations;
std::unordered_map<BASE *, std::vector<YIELD>> unworkedTileYields;

/*
Prepares former orders.
*/
void aiTerraformingStrategy()
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

/*
Populate global lists before processing faction strategy.
*/
void populateLists()
{
	Faction *aiFaction = &(tx_factions[aiFactionId]);

	// clear lists

	workedTileBases.clear();
	baseTerraformingRanks.clear();
	harvestedTiles.clear();
	terraformedTiles.clear();
	terraformedForestTiles.clear();
	terraformedCondenserTiles.clear();
	terraformedMirrorTiles.clear();
	terraformedBoreholeTiles.clear();
	terraformedAquiferTiles.clear();
	terraformedRaiseTiles.clear();
	territoryTiles.clear();
	workableTiles.clear();
	availableTiles.clear();
	baseTerraformingRequests.clear();
	freeTerraformingRequests.clear();
	regionTerraformingRequests.clear();
	regionFormerOrders.clear();
	vehicleFormerOrders.clear();
	targetedTiles.clear();
	connectionNetworkLocations.clear();
	nearbyBaseSets.clear();
	seaRegionMappings.clear();
	coastalBaseRegionMappings.clear();
	blockedLocations.clear();
	warzoneLocations.clear();
	zocLocations.clear();
	unworkedTileYields.clear();

	// populate blocked/warzone and zoc locations

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(tx_vehicles[id]);
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
			for (int i = 1; i < ADJACENT_TILE_OFFSET_COUNT; i++)
			{
				int adjacentTileX = wrap(vehicle->x + BASE_TILE_OFFSETS[i][0]);
				int adjacentTileY = vehicle->y + BASE_TILE_OFFSETS[i][1];
				MAP *adjacentTile = getMapTile(adjacentTileX, adjacentTileY);

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
			vehicle->proto_id != BSC_FUNGAL_TOWER
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

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

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

		// populate worked tiles

		for (int offsetIndex = 1; offsetIndex < BASE_TILE_OFFSET_COUNT; offsetIndex++)
		{
			if (base->worked_tiles & (0x1 << offsetIndex))
			{
				MAP *workedTile = getMapTile(base->x + BASE_TILE_OFFSETS[offsetIndex][0], base->y + BASE_TILE_OFFSETS[offsetIndex][1]);
				workedTileBases[workedTile] = {id, base};
			}

		}

	}

	// populate sea region mappings for coastal bases connecting sea regions

//	debug("seaRegionLinkedGroups\n")

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		MAP *baseTile = getMapTile(base->x, base->y);

		if (!is_ocean(baseTile))
		{
			std::set<int> seaRegionLinkedGroup;

			for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
			{
				MAP *adjacentTile = getMapTile(base->x + BASE_TILE_OFFSETS[offsetIndex][0], base->y + BASE_TILE_OFFSETS[offsetIndex][1]);

				// skip tiles outside of map

				if (adjacentTile == NULL)
					continue;

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

//				if (DEBUG)
//				{
//					for
//					(
//						std::set<int>::iterator seaRegionLinkedGroupIterator = seaRegionLinkedGroup.begin();
//						seaRegionLinkedGroupIterator != seaRegionLinkedGroup.end();
//						seaRegionLinkedGroupIterator++
//					)
//					{
//						debug(" %3d", *seaRegionLinkedGroupIterator);
//					}
//					debug("\n");
//				}

			}

		}

	}

	// populate sea region mappings

//	debug("aggregation\n");

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

//				debug("\t%3d, %3d\n", i, j);

				for
				(
					std::set<int>::iterator seaRegionLinkedGroup2Iterator = seaRegionLinkedGroup2->begin();
					seaRegionLinkedGroup2Iterator != seaRegionLinkedGroup2->end();
					seaRegionLinkedGroup2Iterator++
				)
				{
					int seaRegionLinkedGroup2Element = *seaRegionLinkedGroup2Iterator;

//					debug("\t\t%3d\n", seaRegionLinkedGroup2Element);

					if (seaRegionLinkedGroup1->count(seaRegionLinkedGroup2Element) != 0)
					{
//						debug("\t\t\tfound\n");

						seaRegionLinkedGroup1->insert(seaRegionLinkedGroup2->begin(), seaRegionLinkedGroup2->end());
						seaRegionLinkedGroup2->clear();

						aggregated = true;

						break;

					}

//					debug("\t\t\t%3d %3d\n", seaRegionLinkedGroup1->size(), seaRegionLinkedGroup2->size());

				}

			}

		}

	}

//	debug("after aggregation\n");
//
//	if (DEBUG)
//	{
//		for (size_t i = 0; i < seaRegionLinkedGroups.size(); i++)
//		{
//			std::set<int> *seaRegionLinkedGroup = &(seaRegionLinkedGroups[i]);
//
//			debug("group:");
//
//			for
//			(
//				std::set<int>::iterator seaRegionLinkedGroupIterator = seaRegionLinkedGroup->begin();
//				seaRegionLinkedGroupIterator != seaRegionLinkedGroup->end();
//				seaRegionLinkedGroupIterator++
//			)
//			{
//				int region = *seaRegionLinkedGroupIterator;
//
//				debug(" %3d", region);
//
//			}
//
//			debug("\n");
//
//		}
//
//	}

//	debug("seaRegionMappings\n");

	for (size_t i = 0; i < seaRegionLinkedGroups.size(); i++)
	{
		std::set<int> *seaRegionLinkedGroup = &(seaRegionLinkedGroups[i]);

//		debug("group\n");

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

//			debug("\t%3d => %3d\n", region, mappingRegion);

		}

	}

	// populate coastal base sea region mappings

//	debug("coastalBaseRegionMappings\n");

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		MAP *baseTile = getMapTile(base->x, base->y);

		if (!is_ocean(baseTile))
		{
			for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
			{
				MAP *adjacentTile = getMapTile(base->x + BASE_TILE_OFFSETS[offsetIndex][0], base->y + BASE_TILE_OFFSETS[offsetIndex][1]);

				if (is_ocean(adjacentTile))
				{
					int terraformingRegion = getConnectedRegion(adjacentTile->region);
					coastalBaseRegionMappings[baseTile] = seaRegionMappings[terraformingRegion];

//					debug("\t(%3d,%3d) => %3d\n", base->x, base->y, terraformingRegion);

					break;

				}

			}

		}

	}

	// populated terraformed tiles and former orders

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &tx_vehicles[id];
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
		else if (isVehicleFormer(vehicle))
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

				// update base terraforming ranks for yield terraforming actions

				if (yieldTerraformingOrders.count(vehicle->move_status))
				{
					// this tile affects single base only
					if (wideRangeTerraformingOrders.count(vehicle->move_status) == 0 && workedTileBases.count(terraformedTile) != 0)
					{
						BASE_INFO *baseInfo = &(workedTileBases.find(terraformedTile)->second);
						BASE *base = baseInfo->base;
						baseTerraformingRanks[base]++;
					}
					// this tile may affect multiple nearby bases
					else
					{
						// get terraforming affecting range

						int affectingRange = (wideRangeTerraformingOrders.count(vehicle->move_status) == 0 ? 0 : 1);
						std::vector<BASE_INFO> *nearbyBases = &(nearbyBaseSets[terraformedTile][affectingRange]);
						for
						(
							std::vector<BASE_INFO>::iterator nearbyBasesIterator = nearbyBases->begin();
							nearbyBasesIterator != nearbyBases->end();
							nearbyBasesIterator++
						)
						{
							BASE_INFO *baseInfo = &(*nearbyBasesIterator);
							BASE *base = baseInfo->base;

							baseTerraformingRanks[base]++;

						}

					}

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

				// get terraforming region

				int region;

				switch (triad)
				{
				case TRIAD_LAND:

					// get land region

					region = vehicleTile->region;

					break;

				case TRIAD_SEA:

					if (coastalBaseRegionMappings.count(vehicleTile) != 0)
					{
						// get coastal base region if in coastal base

						region = coastalBaseRegionMappings[vehicleTile];

					}
					else
					{
						// get sea region otherwise

						region = getConnectedRegion(vehicleTile->region);

					}

					break;

				}

				if (regionFormerOrders.count(region) == 0)
				{
					// add new region

					regionFormerOrders[region] = std::vector<FORMER_ORDER>();

				}

				// store vehicle current id in pad_0 field

				vehicle->pad_0 = id;

				// add vehicle

				regionFormerOrders[region].push_back({id, vehicle, -1, -1, -1});

			}

		}

	}

	if (DEBUG)
	{
		debug("FORMER_ORDERS\n");

		for
		(
			std::map<int, std::vector<FORMER_ORDER>>::iterator regionFormerOrdersIterator = regionFormerOrders.begin();
			regionFormerOrdersIterator != regionFormerOrders.end();
			regionFormerOrdersIterator++
		)
		{
			int region = regionFormerOrdersIterator->first;
			std::vector<FORMER_ORDER> *formerOrders = &(regionFormerOrdersIterator->second);

			debug("\tregion=%d\n", region);

			for
			(
				std::vector<FORMER_ORDER>::iterator formerOrdersIterator = formerOrders->begin();
				formerOrdersIterator != formerOrders->end();
				formerOrdersIterator++
			)
			{
				FORMER_ORDER *formerOrder = &(*formerOrdersIterator);

				debug("\t\t%d [%3d] %d\n", (int)formerOrder, formerOrder->id, (int)formerOrder->vehicle);

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

			territoryTiles.push_back({x, y, tile});

		}

	}

	// populate availalbe tiles

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		// iterate base tiles

		for (int tileOffsetIndex = 1; tileOffsetIndex < BASE_TILE_OFFSET_COUNT; tileOffsetIndex++)
		{
			int x = wrap(base->x + BASE_TILE_OFFSETS[tileOffsetIndex][0]);
			int y = base->y + BASE_TILE_OFFSETS[tileOffsetIndex][1];
			MAP *tile = getMapTile(x, y);

			// exclude impossible coordinates

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

			// store workable tile

			workableTiles[tile] = {x, y, tile};

			// exclude harvested tiles

			if (harvestedTiles.count(tile) != 0)
				continue;

			// exclude terraformed tiles

			if (terraformedTiles.count(tile) != 0)
				continue;

			// store available tile

			availableTiles[tile] = {x, y, tile};

		}

	}

	if (DEBUG)
	{
		debug("availableTiles\n");

		for
		(
			std::map<MAP *, MAP_INFO>::iterator availableTilesIterator = availableTiles.begin();
			availableTilesIterator != availableTiles.end();
			availableTilesIterator++
		)
		{
			MAP_INFO *mapInfo = &(availableTilesIterator->second);

			debug("\t(%3d, %3d)\n", mapInfo->x, mapInfo->y);

		}

	}

	// populate unworked tiles

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		unworkedTileYields[base] = std::vector<YIELD>();
		std::vector<YIELD> *baseunworkedTileYields = &(unworkedTileYields[base]);

		for (int offsetIndex = 1; offsetIndex < BASE_TILE_OFFSET_COUNT; offsetIndex++)
		{
			int x = wrap(base->x + BASE_TILE_OFFSETS[offsetIndex][0]);
			int y = base->y + BASE_TILE_OFFSETS[offsetIndex][1];
			MAP *tile = getMapTile(x, y);

			// process available tiles only

			if (availableTiles.count(tile) == 0)
				continue;

			// exclude worked tiles

			if (workedTileBases.count(tile) != 0)
				continue;

			// calculate and store yield

			int nutrient = mod_nutrient_yield(aiFactionId, id, x, y, 0);
			int mineral = mod_mineral_yield(aiFactionId, id, x, y, 0);
			int energy = mod_energy_yield(aiFactionId, id, x, y, 0);

			baseunworkedTileYields->push_back({nutrient, mineral, energy});

		}

		// remove inferior yields

		for
		(
			std::vector<YIELD>::iterator baseUnworkedTileYieldsIterator = baseunworkedTileYields->begin();
			baseUnworkedTileYieldsIterator != baseunworkedTileYields->end();
		)
		{
			YIELD *yield = &(*baseUnworkedTileYieldsIterator);

			if (isInferiorYield(baseunworkedTileYields, yield->nutrient, yield->mineral, yield->energy, &baseUnworkedTileYieldsIterator))
			{
				baseUnworkedTileYieldsIterator = baseunworkedTileYields->erase(baseUnworkedTileYieldsIterator);
			}
			else
			{
				baseUnworkedTileYieldsIterator++;
			}

		}

	}

	// calculate network demand

	int networkType = (has_tech(aiFactionId, tx_terraform[FORMER_MAGTUBE].preq_tech) ? TERRA_MAGTUBE : TERRA_ROAD);

	double networkCoverage = 0.0;

	for (int i = 0; i < *map_area_tiles; i++)
	{
		// build tile

		MAP *tile = &((*tx_map_ptr)[i]);

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

	double networkThreshold = conf.ai_terraforming_networkCoverageThreshold * (double)baseIds.size();

	if (networkCoverage < networkThreshold)
	{
		networkDemand = (networkThreshold - networkCoverage) / networkThreshold;
	}

	fflush(debug_log);

}

void generateTerraformingRequests()
{
	// conventional requests

	debug("BASE TERRAFORMING_REQUESTS\n");

	for
	(
		std::map<MAP *, MAP_INFO>::iterator availableTilesIterator = availableTiles.begin();
		availableTilesIterator != availableTiles.end();
		availableTilesIterator++
	)
	{
		MAP_INFO *mapInfo = &(availableTilesIterator->second);

		// generate request

		generateConventionalTerraformingRequest(mapInfo);

	}

	if (DEBUG)
	{
		for
		(
			std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator baseTerraformingRequestsIterator = baseTerraformingRequests.begin();
			baseTerraformingRequestsIterator != baseTerraformingRequests.end();
			baseTerraformingRequestsIterator++
		)
		{
			BASE *base = baseTerraformingRequestsIterator->first;
			std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(baseTerraformingRequestsIterator->second);

			debug("\tBASE: (%3d,%3d)\n", base->x, base->y);

			for
			(
				std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestIterator = terraformingRequests->begin();
				terraformingRequestIterator != terraformingRequests->end();
				terraformingRequestIterator++
			)
			{
				TERRAFORMING_REQUEST *terraformingRequest = &(*terraformingRequestIterator);

				debug
				(
					"\t\t(%3d,%3d) %6.3f %-10s -> %s\n",
					terraformingRequest->x,
					terraformingRequest->y,
					terraformingRequest->score,
					terraformingRequest->option->name,
					getTerraformingActionName(terraformingRequest->action)
				)
				;

			}

		}

		fflush(debug_log);

	}

	// aquifer

	debug("AQUIFER TERRAFORMING_REQUESTS\n");

	for
	(
		std::map<MAP *, MAP_INFO>::iterator workableTilesIterator = workableTiles.begin();
		workableTilesIterator != workableTiles.end();
		workableTilesIterator++
	)
	{
		MAP_INFO *mapInfo = &(workableTilesIterator->second);

		// generate request

		generateAquiferTerraformingRequest(mapInfo);

	}

	// raise workable tiles to increase energy output

	debug("RAISE LAND TERRAFORMING_REQUESTS\n");

	for
	(
		std::map<MAP *, MAP_INFO>::iterator workableTilesIterator = workableTiles.begin();
		workableTilesIterator != workableTiles.end();
		workableTilesIterator++
	)
	{
		MAP_INFO *mapInfo = &(workableTilesIterator->second);

		// generate request

		generateRaiseLandTerraformingRequest(mapInfo);

	}

	// network

	debug("NETWORK TERRAFORMING_REQUESTS\n");

	for
	(
		std::vector<MAP_INFO>::iterator territoryTilesIterator = territoryTiles.begin();
		territoryTilesIterator != territoryTiles.end();
		territoryTilesIterator++
	)
	{
		MAP_INFO *mapInfo = &(*territoryTilesIterator);

		// generate request

		generateNetworkTerraformingRequest(mapInfo);

	}

}

/*
Generate request for conventional incompatible terraforming options.
*/
void generateConventionalTerraformingRequest(MAP_INFO *mapInfo)
{
	TERRAFORMING_SCORE terraformingScoreObject;
	TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
	calculateConventionalTerraformingScore(mapInfo, terraformingScore);

	// terraforming request should have option, action and score set

	if (terraformingScore->option == NULL || terraformingScore->action == -1 || terraformingScore->score <= 0.0)
		return;

	if (terraformingScore->option->ranked)
	{
		// ranked option should affect base

		if (terraformingScore->base == NULL)
			return;

		// extract base

		BASE *base = terraformingScore->base;

		// generate base terraforming request

		if (baseTerraformingRequests.count(base) == 0)
		{
			baseTerraformingRequests[base] = std::vector<TERRAFORMING_REQUEST>();
		}

		baseTerraformingRequests[base].push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score, 0});

	}
	else
	{
		// generate free terraforming request

		freeTerraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score, 0});

	}

}

/*
Generate request for aquifer.
*/
void generateAquiferTerraformingRequest(MAP_INFO *mapInfo)
{
	TERRAFORMING_SCORE terraformingScoreObject;
	TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
	calculateAquiferTerraformingScore(mapInfo, terraformingScore);

	if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
	{
		freeTerraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score, 0});
	}

}

/*
Generate request to raise energy collection.
*/
void generateRaiseLandTerraformingRequest(MAP_INFO *mapInfo)
{
	TERRAFORMING_SCORE terraformingScoreObject;
	TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
	calculateRaiseLandTerraformingScore(mapInfo, terraformingScore);

	if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
	{
		freeTerraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score, 0});
	}

}

/*
Generate request for network (road/tube).
*/
void generateNetworkTerraformingRequest(MAP_INFO *mapInfo)
{
	TERRAFORMING_SCORE terraformingScoreObject;
	TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
	calculateNetworkTerraformingScore(mapInfo, terraformingScore);

	if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
	{
		freeTerraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score, 0});
	}

}

/*
Comparison function for base terraforming requests.
Sort in scoring order descending.
*/
bool compareTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2)
{
	return (terraformingRequest1.score > terraformingRequest2.score);
}

void sortBaseTerraformingRequests()
{
	for
	(
		std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator baseTerraformingRequestsIterator = baseTerraformingRequests.begin();
		baseTerraformingRequestsIterator != baseTerraformingRequests.end();
		baseTerraformingRequestsIterator++
	)
	{
		std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(baseTerraformingRequestsIterator->second);

		std::sort(terraformingRequests->begin(), terraformingRequests->end(), compareTerraformingRequests);

	}

	// debug

	if (DEBUG)
	{
		debug("TERRAFORMING_REQUESTS - SORTED\n");

		for
		(
			std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator baseTerraformingRequestsIterator = baseTerraformingRequests.begin();
			baseTerraformingRequestsIterator != baseTerraformingRequests.end();
			baseTerraformingRequestsIterator++
		)
		{
			BASE *base = baseTerraformingRequestsIterator->first;
			std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(baseTerraformingRequestsIterator->second);

			debug("\tBASE: (%3d,%3d)\n", base->x, base->y);

			for
			(
				std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestIterator = terraformingRequests->begin();
				terraformingRequestIterator != terraformingRequests->end();
				terraformingRequestIterator++
			)
			{
				TERRAFORMING_REQUEST *terraformingRequest = &(*terraformingRequestIterator);

				debug
				(
					"\t\t(%3d,%3d) %6.3f %-10s -> %s\n",
					terraformingRequest->x,
					terraformingRequest->y,
					terraformingRequest->score,
					terraformingRequest->option->name,
					getTerraformingActionName(terraformingRequest->action)
				)
				;

			}

		}

		fflush(debug_log);

	}

}

/*
Comparison function for terraforming requests.
Sort in ranking order ascending then in scoring order descending.
*/
bool compareRankedTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2)
{
	return (terraformingRequest1.rank != terraformingRequest2.rank ? terraformingRequest1.rank < terraformingRequest2.rank : terraformingRequest1.score > terraformingRequest2.score);
}

/*
Sort terraforming requests within regions by rank and score.
*/
void rankTerraformingRequests()
{
	// base terraforming requests

	for
	(
		std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator baseTerraformingRequestsIterator = baseTerraformingRequests.begin();
		baseTerraformingRequestsIterator != baseTerraformingRequests.end();
		baseTerraformingRequestsIterator++
	)
	{
		BASE *base = baseTerraformingRequestsIterator->first;
		std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(baseTerraformingRequestsIterator->second);

		// rank base requests and generate ranked request by region

		int rank = getBaseTerraformingRank(base);

		for
		(
			std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestsIterator = terraformingRequests->begin();
			terraformingRequestsIterator != terraformingRequests->end();
			terraformingRequestsIterator++
		)
		{
			TERRAFORMING_REQUEST *terraformingRequest = &(*terraformingRequestsIterator);

			// rank ranked requests but not others

			if (terraformingRequest->option->ranked)
			{
				for (int i = 0; i < rank; i++)
				{
					terraformingRequest->score *= conf.ai_terraforming_rankMultiplier;
				}

				rank++;

			}

			// get region

			int region = terraformingRequest->tile->region;

			// create region bucket if needed

			if (regionTerraformingRequests.count(region) == 0)
			{
				regionTerraformingRequests[region] = std::vector<TERRAFORMING_REQUEST>();
			}

			// add terraforming request

			regionTerraformingRequests[region].push_back(*terraformingRequest);

		}

	}

	// free terraforming requests

	for
	(
		std::vector<TERRAFORMING_REQUEST>::iterator freeTerraformingRequestsIterator = freeTerraformingRequests.begin();
		freeTerraformingRequestsIterator != freeTerraformingRequests.end();
		freeTerraformingRequestsIterator++
	)
	{
		TERRAFORMING_REQUEST *terraformingRequest = &(*freeTerraformingRequestsIterator);

		// get region

		int region = terraformingRequest->tile->region;

		// create region bucket if needed

		if (regionTerraformingRequests.count(region) == 0)
		{
			regionTerraformingRequests[region] = std::vector<TERRAFORMING_REQUEST>();
		}

		// add terraforming request

		regionTerraformingRequests[region].push_back(*terraformingRequest);

	}

	// sort terraforming requests within regions

	for
	(
		std::map<int, std::vector<TERRAFORMING_REQUEST>>::iterator regionTerraformingRequestsIterator = regionTerraformingRequests.begin();
		regionTerraformingRequestsIterator != regionTerraformingRequests.end();
		regionTerraformingRequestsIterator++
	)
	{
		std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(regionTerraformingRequestsIterator->second);

		// sort requests

		std::sort(terraformingRequests->begin(), terraformingRequests->end(), compareRankedTerraformingRequests);

		// apply proximity rules

		for
		(
			std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestsIterator = terraformingRequests->begin();
			terraformingRequestsIterator != terraformingRequests->end();
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
						terraformingRequests->begin(),
						terraformingRequestsIterator,
						terraformingRequest->action,
						terraformingRequest->x,
						terraformingRequest->y,
						proximityRule->buildingDistance
					)
				;

			}

			if (tooClose)
			{
				// delete this request and advance iterator

				terraformingRequestsIterator = terraformingRequests->erase(terraformingRequestsIterator);

			}
			else
			{
				// advance iterator

				terraformingRequestsIterator++;

			}

		}

	}

	// debug

	if (DEBUG)
	{
		debug("TERRAFORMING_REQUESTS - RANKED\n");

		for
		(
			std::map<int, std::vector<TERRAFORMING_REQUEST>>::iterator rankedTerraformingRequestIterator = regionTerraformingRequests.begin();
			rankedTerraformingRequestIterator != regionTerraformingRequests.end();
			rankedTerraformingRequestIterator++
		)
		{
			int region = rankedTerraformingRequestIterator->first;
			std::vector<TERRAFORMING_REQUEST> *regionRankedTerraformingRequests = &(rankedTerraformingRequestIterator->second);

			debug("\tregion: %3d\n", region);

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
					"\t\t(%3d,%3d) %d %6.3f %-10s -> %s\n",
					terraformingRequest->x,
					terraformingRequest->y,
					terraformingRequest->rank,
					terraformingRequest->score,
					terraformingRequest->option->name,
					getTerraformingActionName(terraformingRequest->action)
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
		std::map<int, std::vector<FORMER_ORDER>>::iterator regionFormerOrdersIterator = regionFormerOrders.begin();
		regionFormerOrdersIterator != regionFormerOrders.end();
	)
	{
		int region = regionFormerOrdersIterator->first;
		std::vector<FORMER_ORDER> *formerOrders = &(regionFormerOrdersIterator->second);

		// delete formers without requests in this region
		if (regionTerraformingRequests.count(region) == 0)
		{
			// erasing region and automatically advance region iterator

			regionFormerOrdersIterator = regionFormerOrders.erase(regionFormerOrdersIterator);

		}
		// otherwise assign orders
		else
		{
			// assign as much orders as possible

			std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(regionTerraformingRequests[region]);

			std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestsIterator = terraformingRequests->begin();
			std::vector<FORMER_ORDER>::iterator formerOrdersIterator = formerOrders->begin();

			for
			(
				;
				terraformingRequestsIterator != terraformingRequests->end() && formerOrdersIterator != formerOrders->end();
				formerOrdersIterator++
			)
			{
				FORMER_ORDER *formerOrder = &(*formerOrdersIterator);
				TERRAFORMING_REQUEST *terraformingRequest = &(*terraformingRequestsIterator);

				formerOrder->x = terraformingRequest->x;
				formerOrder->y = terraformingRequest->y;
				formerOrder->action = terraformingRequest->action;

				// erase assigned ranked terraforming request

				terraformingRequestsIterator = terraformingRequests->erase(terraformingRequestsIterator);

			}

			// erase the rest of unassigned former orders if any

			formerOrders->erase(formerOrdersIterator, formerOrders->end());

			// advance region former orders iterator

			regionFormerOrdersIterator++;

		}

	}

	// debug

	if (DEBUG)
	{
		debug("FORMER ORDERS\n");

		for
		(
			std::map<int, std::vector<FORMER_ORDER>>::iterator regionFormerOrdersIterator = regionFormerOrders.begin();
			regionFormerOrdersIterator != regionFormerOrders.end();
			regionFormerOrdersIterator++
		)
		{
			int region = regionFormerOrdersIterator->first;
			std::vector<FORMER_ORDER> *formerOrders = &(regionFormerOrdersIterator->second);

			debug("\tregion=%3d\n", region);

			for
			(
				std::vector<FORMER_ORDER>::iterator formerOrdersIterator = formerOrders->begin();
				formerOrdersIterator != formerOrders->end();
				formerOrdersIterator++
			)
			{
				FORMER_ORDER *formerOrder = &(*formerOrdersIterator);

				debug
				(
					"\t\t[%4d](%3d,%3d) -> (%3d,%3d) %s\n",
					formerOrder->id,
					formerOrder->vehicle->x,
					formerOrder->vehicle->y,
					formerOrder->x,
					formerOrder->y,
					getTerraformingActionName(formerOrder->action)
				)
				;

			}

		}

		fflush(debug_log);

	}

}

void optimizeFormerDestinations()
{
	// iterate through regions

	for
	(
		std::map<int, std::vector<FORMER_ORDER>>::iterator regionFormerOrdersIterator = regionFormerOrders.begin();
		regionFormerOrdersIterator != regionFormerOrders.end();
		regionFormerOrdersIterator++
	)
	{
		std::vector<FORMER_ORDER> *formerOrders = &(regionFormerOrdersIterator->second);

		// iterate until optimized or max iterations

		for (int iteration = 0; iteration < 100; iteration++)
		{
			bool swapped = false;

			// iterate through former order pairs

			for
			(
				std::vector<FORMER_ORDER>::iterator formerOrderIterator1 = formerOrders->begin();
				formerOrderIterator1 != formerOrders->end();
				formerOrderIterator1++
			)
			{
				for
				(
					std::vector<FORMER_ORDER>::iterator formerOrderIterator2 = formerOrders->begin();
					formerOrderIterator2 != formerOrders->end();
					formerOrderIterator2++
				)
				{
					FORMER_ORDER *formerOrder1 = &(*formerOrderIterator1);
					FORMER_ORDER *formerOrder2 = &(*formerOrderIterator2);

					// skip itself

					if (formerOrder1->id == formerOrder2->id)
						continue;

					VEH *vehicle1 = formerOrder1->vehicle;
					VEH *vehicle2 = formerOrder2->vehicle;

					int vehicle1Speed = veh_speed_without_roads(formerOrder1->id);
					int vehicle2Speed = veh_speed_without_roads(formerOrder2->id);

					int destination1X = formerOrder1->x;
					int destination1Y = formerOrder1->y;
					MAP *destination1Tile = getMapTile(destination1X, destination1Y);
					int destination2X = formerOrder2->x;
					int destination2Y = formerOrder2->y;
					MAP *destination2Tile = getMapTile(destination2X, destination2Y);

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
		std::map<int, std::vector<FORMER_ORDER>>::iterator regionFormerOrdersIterator = regionFormerOrders.begin();
		regionFormerOrdersIterator != regionFormerOrders.end();
		regionFormerOrdersIterator++
	)
	{
		std::vector<FORMER_ORDER> *formerOrders = &(regionFormerOrdersIterator->second);

		for
		(
			std::vector<FORMER_ORDER>::iterator formerOrderIterator = formerOrders->begin();
			formerOrderIterator != formerOrders->end();
			formerOrderIterator++
		)
		{
			FORMER_ORDER *formerOrder = &(*formerOrderIterator);

			vehicleFormerOrders[formerOrder->id] = *formerOrder;

		}

	}

	// debug

	if (DEBUG)
	{
		debug("FORMER ORDERS - FINALIZED\n");

		for
		(
			std::map<int, FORMER_ORDER>::iterator vehicleFormerOrdersIterator = vehicleFormerOrders.begin();
			vehicleFormerOrdersIterator != vehicleFormerOrders.end();
			vehicleFormerOrdersIterator++
		)
		{
			int id = vehicleFormerOrdersIterator->first;
			FORMER_ORDER *formerOrder = &(vehicleFormerOrdersIterator->second);

			debug
			(
				"\t[%4d] (%3d,%3d) %s\n",
				id,
				formerOrder->x,
				formerOrder->y,
				getTerraformingActionName(formerOrder->action)
			)
			;

		}

		fflush(debug_log);

	}

}

/*
Selects best terraforming option for given tile and calculates its terraforming score.
*/
void calculateConventionalTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	bool ocean = is_ocean(mapInfo->tile);

	// do not evavluate monolith for conventional options

	if (map_has_item(tile, TERRA_MONOLITH))
		return;

	debug
	(
		"calculateTerraformingScore: (%3d,%3d)\n",
		x,
		y
	)

	// calculate range penalty time

	int closestAvailableFormerRange = calculateClosestAvailableFormerRange(mapInfo);
	double rangePenaltyTime = (ocean ? conf.ai_terraforming_waterDistanceScale : conf.ai_terraforming_landDistanceScale) * (double)closestAvailableFormerRange;

	// store tile values

	MAP_STATE currentMapStateObject;
	MAP_STATE *currentMapState = &currentMapStateObject;
	getMapState(mapInfo, currentMapState);

	// find best terraforming option

	const TERRAFORMING_OPTION *bestOption = NULL;
	BASE *bestOptionAffectedBase = NULL;
	int bestOptionFirstAction = -1;
	double bestOptionTerraformingScore = 0.0;

	for (size_t optionIndex = 0; optionIndex < CONVENTIONAL_TERRAFORMING_OPTIONS.size(); optionIndex++)
	{
		const TERRAFORMING_OPTION *option = &(CONVENTIONAL_TERRAFORMING_OPTIONS[optionIndex]);

		// process only correct region type

		if (option->ocean != ocean)
			continue;

		// process only correct rockiness

		if (option->rocky && !(tile->rocks & TILE_ROCKY))
			continue;

		// check if main action technology is available when required

		int mainAction = *(option->actions.begin());

		if (option->firstActionRequired && !isTerraformingAvailable(mapInfo, mainAction))
			continue;

		// initialize variables

		MAP_STATE improvedMapStateObject;
		MAP_STATE *improvedMapState = &improvedMapStateObject;
		copyMapState(improvedMapState, currentMapState);

		BASE *affectedBase = NULL;
		int firstAction = -1;
		int terraformingTime = 0;
		double penaltyScore = 0.0;
		double improvementScore = 0.0;

		// process actions

		for
		(
			std::vector<int>::const_iterator actionsIterator = option->actions.begin();
			actionsIterator != option->actions.end();
			actionsIterator++
		)
		{
			int action = *actionsIterator;

			// skip unavailable actions

			if (!isTerraformingAvailable(mapInfo, action))
				continue;

			// generate terraforming change and add terraforming time for non completed action

			if (!isTerraformingCompleted(mapInfo, action))
			{
				// remove fungus if needed

				if ((improvedMapState->items & TERRA_FUNGUS) && isRemoveFungusRequired(action))
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
							improvedMapState->items,
							improvedMapState->rocks,
							NULL
						)
					;

					// generate terraforming change

					generateTerraformingChange(improvedMapState, removeFungusAction);

					// add penalty for nearby forest/kelp

					int nearbyForestKelpCount = nearby_items(x, y, 1, (ocean ? TERRA_FARM : TERRA_FOREST));
					penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;

				}

				// level terrain if needed

				if (!ocean && (improvedMapState->rocks & TILE_ROCKY) && isLevelTerrainRequired(action))
				{
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
							improvedMapState->items,
							improvedMapState->rocks,
							NULL
						)
					;

					// generate terraforming change

					generateTerraformingChange(improvedMapState, levelTerrainAction);

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
						improvedMapState->items,
						improvedMapState->rocks,
						NULL
					)
				;

				// generate terraforming change

				generateTerraformingChange(improvedMapState, action);

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

		}

		// ignore unpopulated options

		if (firstAction == -1 || terraformingTime == 0)
			continue;

		// calculate yield improvement score

		SURPLUS_EFFECT optionSurplusEffect;
		computeImprovementSurplusEffect(mapInfo, currentMapState, improvedMapState, &optionSurplusEffect);

		// update variables

		affectedBase = optionSurplusEffect.base;
		double surplusEffectScore = optionSurplusEffect.score;

		// special yield computation for area effects

		double condenserExtraYieldScore = estimateCondenserExtraYieldScore(mapInfo, &(option->actions));

		// calculate exclusivity bonus

		double exclusivityBonus = calculateExclusivityBonus(mapInfo, &(option->actions));

		// summarize score

		improvementScore = penaltyScore + surplusEffectScore + condenserExtraYieldScore + exclusivityBonus;

		// ignore ineffective options

		if (improvementScore <= 0.0)
			continue;

		// calculate terraforming score

		double terraformingScore =
			improvementScore
			/
			((double)terraformingTime + rangePenaltyTime)
		;

		debug
		(
			"\t%-10s: penalty=%6.3f, surplusEffect=%6.3f, condenser=%6.3f, exclusivity=%6.3f, combined=%6.3f, time=%2d, travel=%d, final=%6.3f\n",
			option->name,
			penaltyScore,
			surplusEffectScore,
			condenserExtraYieldScore,
			exclusivityBonus,
			improvementScore,
			terraformingTime,
			closestAvailableFormerRange,
			terraformingScore
		)
		;

		// update score

		if (bestOption == NULL || terraformingScore > bestOptionTerraformingScore)
		{
			bestOption = option;
			bestOptionAffectedBase = affectedBase;
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
	bestTerraformingScore->base = bestOptionAffectedBase;

}

/*
Aquifer calculations.
*/
void calculateAquiferTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	bool ocean = is_ocean(mapInfo->tile);

	// land only

	if (ocean)
		return;

	// get option

	const TERRAFORMING_OPTION *option = &(AQUIFER_TERRAFORMING_OPTION);

	// initialize variables

	int firstAction = -1;
	int terraformingTime = 0;
	double improvementScore = 0.0;

	int action = FORMER_AQUIFER;

	// skip unavailable actions

	if (!isTerraformingAvailable(mapInfo, action))
		return;

	// do not generate request for completed action

	if (isTerraformingCompleted(mapInfo, action))
		return;

	// set action

	if (firstAction == -1)
	{
		firstAction = action;
	}

	// compute terraforming time

	terraformingTime = calculateTerraformingTime(action, tile->items, tile->rocks, NULL);

	// skip if no action or no terraforming time

	if (firstAction == -1 || terraformingTime == 0)
		return;

	// special yield computation for aquifer

	improvementScore += estimateAquiferExtraYieldScore(mapInfo);

	// calculate range time penalty

	int closestAvailableFormerRange = calculateClosestAvailableFormerRange(mapInfo);
	double rangePenaltyTime = (double)closestAvailableFormerRange * (ocean ? conf.ai_terraforming_waterDistanceScale : conf.ai_terraforming_landDistanceScale);

	// calculate terraforming score

	double terraformingScore =
		improvementScore
		/
		((double)terraformingTime + rangePenaltyTime)
	;

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s: score=%6.3f, time=%2d, range=%2d, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		closestAvailableFormerRange,
		terraformingScore
	)
	;

}

/*
Raise land calculations.
*/
void calculateRaiseLandTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	bool ocean = is_ocean(mapInfo->tile);

	// land only

	if (ocean)
		return;

	// verify we have enough money

	int cost = tx_terraform_cost(x, y, aiFactionId);
	if (cost > tx_factions[aiFactionId].energy_credits / 10)
		return;

	// get option

	const TERRAFORMING_OPTION *option = &(RAISE_LAND_TERRAFORMING_OPTION);

	// initialize variables

	int firstAction = -1;
	int terraformingTime = 0;
	double improvementScore = 0.0;

	int action = FORMER_RAISE_LAND;

	// skip unavailable actions

	if (!isTerraformingAvailable(mapInfo, action))
		return;

	// set action

	if (firstAction == -1)
	{
		firstAction = action;
	}

	// compute terraforming time

	terraformingTime = calculateTerraformingTime(action, tile->items, tile->rocks, NULL);

	// skip if no action or no terraforming time

	if (firstAction == -1 || terraformingTime == 0)
		return;

	// special yield computation for raise land

	improvementScore += estimateRaiseLandExtraYieldScore(mapInfo, cost);

	// do not generate request for non positive score

	if (improvementScore <= 0.0)
		return;

	// calculate range time penalty

	int closestAvailableFormerRange = calculateClosestAvailableFormerRange(mapInfo);
	double rangePenaltyTime = (double)closestAvailableFormerRange * (ocean ? conf.ai_terraforming_waterDistanceScale : conf.ai_terraforming_landDistanceScale);

	// calculate terraforming score

	double terraformingScore =
		improvementScore
		/
		((double)terraformingTime + rangePenaltyTime)
	;

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s: score=%6.3f, time=%2d, range=%2d, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		closestAvailableFormerRange,
		terraformingScore
	)
	;

}

/*
Network calculations.
*/
void calculateNetworkTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	bool ocean = is_ocean(mapInfo->tile);

	// land only

	if (ocean)
		return;

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

	if (!isTerraformingAvailable(mapInfo, action))
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

		terraformingTime += calculateTerraformingTime(removeFungusAction, tile->items, tile->rocks, NULL);

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

	terraformingTime += calculateTerraformingTime(action, tile->items, tile->rocks, NULL);

	// special network bonus

	improvementScore += (1.0 + networkDemand) * calculateNetworkScore(mapInfo, action);

	// do not create request for zero score

	if (improvementScore <= 0.0)
		return;

	// calculate range time penalty

	int closestAvailableFormerRange = calculateClosestAvailableFormerRange(mapInfo);
	double rangePenaltyTime = (double)closestAvailableFormerRange * (ocean ? conf.ai_terraforming_waterDistanceScale : conf.ai_terraforming_landDistanceScale);

	// calculate terraforming score

	double terraformingScore =
		improvementScore
		/
		((double)terraformingTime + rangePenaltyTime)
	;

	// update return values

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

	debug("calculateTerraformingScore: (%3d,%3d)\n", x, y);
	debug
	(
		"\t%-10s: score=%6.3f, time=%2d, range=%2d, final=%6.3f\n",
		option->name,
		improvementScore,
		terraformingTime,
		closestAvailableFormerRange,
		terraformingScore
	)
	;

}

/*
Handles movement phase.
*/
int enemyMoveFormer(int id)
{
	// use WTP algorithm for selected faction only

	if (wtpAIFactionId != -1 && aiFactionId != wtpAIFactionId)
		return former_move(id);

	// get vehicle

	VEH *vehicle = &(tx_vehicles[id]);
	MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);

	// use Thinker algorithm if in warzoone and in danger

	if (warzoneLocations.count(vehicleLocation) != 0 && !isSafe(vehicle->x, vehicle->y))
	{
		debug("warzone/unsafe - use Thinker escape algorithm: [%3d]\n", id);
		return former_move(id);
	}

	// restore ai id

	int aiVehicleId = vehicle->pad_0;

	// skip vehicles without former orders

	if (vehicleFormerOrders.count(aiVehicleId) == 0)
		return SYNC;

	// get former order

	FORMER_ORDER *formerOrder = &(vehicleFormerOrders[aiVehicleId]);

	// skip orders without action

	if (formerOrder->action == -1)
		return SYNC;

	// execute order

	setFormerOrder(id, vehicle, formerOrder);

	return SYNC;

}

void setFormerOrder(int id, VEH *vehicle, FORMER_ORDER *formerOrder)
{
	int triad = veh_triad(id);
	int x = formerOrder->x;
	int y = formerOrder->y;
	int action = formerOrder->action;

	// at destination
	if (vehicle->x == x && vehicle->y == y)
	{
		// execute terraforming action

		setTerraformingAction(id, action);

		debug
		(
			"generateFormerOrder: location=(%3d,%3d), action=%s\n",
			vehicle->x,
			vehicle->y,
			getTerraformingActionName(action)
		)
		;

	}
	// not at destination
	else
	{
		// send former to destination

		if (triad == TRIAD_LAND)
		{
			if (!sendVehicleToDestination(id, x, y))
			{
				debug("location inaccessible: [%d] (%3d,%3d)\n", id, x, y);

				// get vehicle region

				MAP *location = getMapTile(vehicle->x, vehicle->y);
				int region = location->region;

				std::vector<TERRAFORMING_REQUEST> *regionRankedTerraformingRequests = &(regionTerraformingRequests[region]);

				bool success = false;

				for
				(
					std::vector<TERRAFORMING_REQUEST>::iterator regionRankedTerraformingRequestsIterator = regionRankedTerraformingRequests->begin();
					regionRankedTerraformingRequestsIterator != regionRankedTerraformingRequests->end();
				)
				{
					TERRAFORMING_REQUEST *terraformingRequest = &(*regionRankedTerraformingRequestsIterator);

					// try new destination

					debug("send vehicle to another destination: [%d] (%3d,%3d)\n", id, terraformingRequest->x, terraformingRequest->y);

					success = sendVehicleToDestination(id, terraformingRequest->x, terraformingRequest->y);

					// erase request

					regionRankedTerraformingRequestsIterator = regionRankedTerraformingRequests->erase(regionRankedTerraformingRequestsIterator);

					// quit the cycle if successful

					if (success)
						break;

				}

				// skip turn if unsuccessful

				if (!success)
				{
					veh_skip(id);
				}

			}

		}
		else
		{
			sendVehicleToDestination(id, x, y);
		}

		debug
		(
			"generateFormerOrder: location=(%3d,%3d), move_to(%3d,%3d)\n",
			vehicle->x,
			vehicle->y,
			vehicle->waypoint_1_x,
			vehicle->waypoint_1_y
		)

	}

}

/*
Finds base affected by tile improvement and computes that base surplus effect.
*/
void computeImprovementSurplusEffect(MAP_INFO *mapInfo, MAP_STATE *currentMapState, MAP_STATE *improvedMapState, SURPLUS_EFFECT *surplusEffect)
{
	MAP *tile = mapInfo->tile;

	// calculate affected range
	// building or destroying mirror requires recomputation of all bases in range 1

	int affectedRange = 0;

	if
	(
		(currentMapState->items & TERRA_ECH_MIRROR && ~improvedMapState->items & TERRA_ECH_MIRROR)
		||
		(~currentMapState->items & TERRA_ECH_MIRROR && improvedMapState->items & TERRA_ECH_MIRROR)
	)
	{
		affectedRange = 1;
	}

	// local change
	if (affectedRange == 0)
	{
		// tile is arealdy worked
		if (workedTileBases.count(tile) != 0)
		{
			// compute only base working this tile

			BASE_INFO *baseInfo = &(workedTileBases[tile]);
			BASE *base = baseInfo->base;

			// ignore inferior improvements

			if (isInferiorImprovedTile(baseInfo, mapInfo, currentMapState, improvedMapState))
				return;

			// compute base score

			double score = computeImprovementBaseSurplusEffectScore(baseInfo, mapInfo, currentMapState, improvedMapState, true);

			if (score != 0.0)
			{
				surplusEffect->base = base;
				surplusEffect->score = score;
			}

		}
		// tile is not worked
		else
		{
			// compute all nearby bases

			std::vector<BASE_INFO> *nearbyBases = &(nearbyBaseSets[tile][0]);

			for
			(
				std::vector<BASE_INFO>::iterator nearbyBasesIterator = nearbyBases->begin();
				nearbyBasesIterator != nearbyBases->end();
				nearbyBasesIterator++
			)
			{
				BASE_INFO *baseInfo = &(*nearbyBasesIterator);
				BASE *base = baseInfo->base;

				// ignore inferior improvements

				if (isInferiorImprovedTile(baseInfo, mapInfo, currentMapState, improvedMapState))
					continue;

				// compute base score

				double score = computeImprovementBaseSurplusEffectScore(baseInfo, mapInfo, currentMapState, improvedMapState, true);

				if (score != 0.0)
				{
					surplusEffect->base = base;
					surplusEffect->score = score;

					break;

				}

			}

		}

	}
	// area change
	else if (affectedRange == 1)
	{
		// compute all nearby bases

		std::vector<BASE_INFO> *nearbyBases = &(nearbyBaseSets[tile][1]);

		BASE *mostAffectedBase = NULL;
		double mostAffectedBaseScore = 0.0;

		for
		(
			std::vector<BASE_INFO>::iterator nearbyBasesIterator = nearbyBases->begin();
			nearbyBasesIterator != nearbyBases->end();
			nearbyBasesIterator++
		)
		{
			BASE_INFO *baseInfo = &(*nearbyBasesIterator);
			BASE *base = baseInfo->base;

			// compute base score

			double score = computeImprovementBaseSurplusEffectScore(baseInfo, mapInfo, currentMapState, improvedMapState, false);

			// update most affected base

			if (mostAffectedBase == NULL || score > mostAffectedBaseScore)
			{
				mostAffectedBase = base;
				mostAffectedBaseScore = score;
			}

			// accumulate score

			surplusEffect->score += score;

		}

		// update most affected base

		surplusEffect->base = mostAffectedBase;

	}

	return;

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
		BASE *base = &tx_bases[baseIndex];

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
bool isTerraformingCompleted(MAP_INFO *mapInfo, int action)
{
	MAP *tile = mapInfo->tile;

	return (tile->items & tx_terraform[action].added_items_flag);

}

/*
Determines whether terraforming can be done in this square with removing fungus and leveling terrain if needed.
Terraforming is considered available if this item is already built.
*/
bool isTerraformingAvailable(MAP_INFO *mapInfo, int action)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	bool ocean = is_ocean(tile);
	bool oceanDeep = is_ocean_deep(tile);

	bool aquatic = tx_metafactions[aiFactionId].rule_flags & FACT_AQUATIC;

	// action is considered available if is already built

	if (map_has_item(tile, tx_terraform[action].added_items_flag))
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
		if (map_level(tile) == (is_ocean(tile) ? LEVEL_OCEAN_SHELF : LEVEL_THREE_ABOVE_SEA))
			return false;
		// raise land should not disturb other faction bases
		if (!isRaiseLandSafe(mapInfo))
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

	if (action == FORMER_ROAD && has_tech(aiFactionId, tx_basic->tech_preq_build_road_fungus))
		return false;

	// for everything else remove fungus only without fungus improvement technology

	return !has_tech(aiFactionId, tx_basic->tech_preq_improv_fungus);

}

/*
Determines whether rocky terrain need to be leveled before terraforming.
*/
bool isLevelTerrainRequired(int action)
{
	// level rocky terrain for farm, enricher, forest

	return (action == FORMER_FARM || action == FORMER_SOIL_ENR || action == FORMER_FOREST);

}

void generateTerraformingChange(MAP_STATE *mapState, int action)
{
	// level terrain changes rockiness
	if (action == FORMER_LEVEL_TERRAIN)
	{
		if (mapState->rocks & TILE_ROCKY)
		{
			mapState->rocks &= ~TILE_ROCKY;
			mapState->rocks |= TILE_ROLLING;
		}
		else if (mapState->rocks & TILE_ROLLING)
		{
			mapState->rocks &= ~TILE_ROLLING;
		}

	}
	// other actions change items
	else
	{
		// special cases
		if (action == FORMER_AQUIFER)
		{
			mapState->items |= (TERRA_RIVER_SRC | TERRA_RIVER);
		}
		// regular cases
		else
		{
			// remove items

			mapState->items &= ~tx_terraform[action].removed_items_flag;

			// add items

			mapState->items |= tx_terraform[action].added_items_flag;

		}

	}

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

void setTerraformingAction(int id, int action)
{
	VEH *vehicle = &(tx_vehicles[id]);

	// subtract raise/lower land cost

	if (action == FORMER_RAISE_LAND || action == FORMER_LOWER_LAND)
	{
		int cost = tx_terraform_cost(vehicle->x, vehicle->y, aiFactionId);
		tx_factions[aiFactionId].energy_credits -= cost;
	}

	// set action

	set_action(id, action + 4, veh_status_icon[action + 4]);

}

/*
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

		MAP_INFO step;
		findPathStep(id, x, y, &step);

		// send vehicle to step or cancel orders
		if (step.tile == NULL)
		{
			return false;
		}
		else
		{
			tx_go_to(id, veh_status_icon[ORDER_MOVE_TO], step.x, step.y);
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

	if (nearby_items(x, y, proximityRule->buildingDistance, TERRA_THERMAL_BORE) >= 1)
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

void getMapState(MAP_INFO *mapInfo, MAP_STATE *mapState)
{
	MAP *tile = mapInfo->tile;

	mapState->climate = tile->level;
	mapState->rocks = tile->rocks;
	mapState->items = tile->items;

}

void setMapState(MAP_INFO *mapInfo, MAP_STATE *mapState)
{
	MAP *tile = mapInfo->tile;

	tile->level = mapState->climate;
	tile->rocks = mapState->rocks;
	tile->items = mapState->items;

}

void copyMapState(MAP_STATE *destinationMapState, MAP_STATE *sourceMapState)
{
	destinationMapState->climate = sourceMapState->climate;
	destinationMapState->rocks = sourceMapState->rocks;
	destinationMapState->items = sourceMapState->items;

}

/*
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

int getBaseTerraformingRank(BASE *base)
{
	return (baseTerraformingRanks.count(base) == 0 ? 0 : baseTerraformingRanks[base]);
}

BASE *findAffectedBase(int x, int y)
{
	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		if (isWithinBaseRadius(base->x, base->y, x, y))
			return base;

	}

	return NULL;

}

char *getTerraformingActionName(int action)
{
	if (action >= FORMER_FARM && action <= FORMER_MONOLITH)
	{
		return tx_terraform[action].name;
	}
	else
	{
		return NULL;
	}

}

int calculateClosestAvailableFormerRange(MAP_INFO *mapInfo)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;

	// get region

	int region = tile->region;

	// give it max value if there are no formers in this region

	if (regionFormerOrders.count(region) == 0)
		return INT_MAX;

	// get regionFormerOrders

	std::vector<FORMER_ORDER> *formerOrders = &(regionFormerOrders[region]);

	// find nearest available former in this region

	int closestFormerRange = INT_MAX;

	for
	(
		std::vector<FORMER_ORDER>::iterator formerOrderIterator = formerOrders->begin();
		formerOrderIterator != formerOrders->end();
		formerOrderIterator++
	)
	{
		FORMER_ORDER *formerOrder = &(*formerOrderIterator);
		VEH *vehicle = formerOrder->vehicle;

		int range = map_range(x, y, vehicle->x, vehicle->y);

		if (range < closestFormerRange)
		{
			closestFormerRange = range;
		}

	}

	return closestFormerRange;

}

/*
Calculates score for network actions (road, tube)
*/
double calculateNetworkScore(MAP_INFO *mapInfo, int action)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;

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

	bool m[ADJACENT_TILE_OFFSET_COUNT];

	for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
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

			if (baseLocations.count(tile) != 0)
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

			if (baseLocations.count(tile) != 0)
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

			if (baseLocations.count(tile) != 0)
			{
				return true;
			}

		}

	}

	return false;

}

int getConnectedRegion(int region)
{
	return (seaRegionMappings.count(region) == 0 ? region : seaRegionMappings[region]);
}

/*
Checks whether this tile is worked by base.
*/
bool isBaseWorkedTile(BASE *base, int x, int y)
{
	// check square is within base radius

	if (!isWithinBaseRadius(base->x, base->y, x, y))
		return false;

	for (int workedTileIndex = 1; workedTileIndex < BASE_TILE_OFFSET_COUNT; workedTileIndex++)
	{
		bool worked = (base->worked_tiles & (0x1 << workedTileIndex));

		if (worked)
		{
			int workedTileX = wrap(base->x + BASE_TILE_OFFSETS[workedTileIndex][0]);
			int workedTileY = base->y + BASE_TILE_OFFSETS[workedTileIndex][1];

			if (workedTileX == x && workedTileY == y)
				return true;

		}

	}

	return false;

}

double calculateExclusivityBonus(MAP_INFO *mapInfo, const std::vector<int> *actions)
{
	MAP *tile = mapInfo->tile;

	// ocean has not exclusivity

	if (is_ocean(tile))
		return 0.0;

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

	// calculate resource exclusivity bonuses

	double nutrient = 0.0;
	double mineral = 0.0;
	double energy = 0.0;

	// improvements ignoring everything
	if
	(
		actionSet.count(FORMER_FOREST)
		||
		actionSet.count(FORMER_PLANT_FUNGUS)
		||
		actionSet.count(FORMER_THERMAL_BORE)
	)
	{
		// nothing is used

		nutrient = EXCLUSIVITY_LEVELS.rainfall - rainfall;
		mineral = EXCLUSIVITY_LEVELS.rockiness - rockiness;
		energy = EXCLUSIVITY_LEVELS.elevation - elevation;

	}
	else
	{
		// mine is better built low

		if (actionSet.count(FORMER_MINE))
		{
			energy += EXCLUSIVITY_LEVELS.elevation - elevation;
		}

		// rocky mine is also better built in dry tiles

		if (actionSet.count(FORMER_MINE) && rockiness == 2)
		{
			nutrient += EXCLUSIVITY_LEVELS.rainfall - rainfall;
		}

		// condenser is better built low

		if (actionSet.count(FORMER_CONDENSER))
		{
			energy += EXCLUSIVITY_LEVELS.elevation - elevation;
		}

	}

	return
		conf.ai_terraforming_exclusivityMultiplier
		*
		(
			conf.ai_terraforming_nutrientWeight * nutrient
			+
			conf.ai_terraforming_mineralWeight * mineral
			+
			conf.ai_terraforming_energyWeight * energy
		)
	;

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
			if (map_range(x, y, terraformingRequest->x, terraformingRequest->y) <= range)
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
double estimateCondenserExtraYieldScore(MAP_INFO *mapInfo, const std::vector<int> *actions)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *improvedTile = mapInfo->tile;

	Faction *faction = &(tx_factions[aiFactionId]);

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

	int forestNutrient = tx_resource->forest_sq_nutrient;
	int forestMineral = tx_resource->forest_sq_mineral;
	int forestEnergy = tx_resource->forest_sq_energy;

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
		fungusNutrient = max(0, fungusNutrient + faction->SE_planet_pending);
		fungusMineral = max(0, fungusMineral + faction->SE_planet_pending);
		fungusEnergy = max(0, fungusEnergy + faction->SE_planet_pending);
	}

	double fungusYieldScore = calculateResourceScore(fungusNutrient, fungusMineral, fungusEnergy);

	// pick the best yield score

	double alternativeYieldScore = max(forestYieldScore, fungusYieldScore);

	// calculate rocky mine yield score

	int rockyMineMineral = 4;
	double rockyMineYieldScore = calculateResourceScore(0.0, (double)rockyMineMineral, 0.0);

	// calculate extra yield with technology

	double extraNutrient = (has_terra(aiFactionId, FORMER_SOIL_ENR, false) ? 2.0 : 1.0);
	double extraEnergy = (has_terra(aiFactionId, FORMER_ECH_MIRROR, false) ? 1.0 : 0.0);

	// estimate rainfall improvement

	// [0.0, 9.0]
	double totalRainfallImprovement = 0.0;

	for (int offsetIndex = 0; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
	{
		int u = wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]);
		int v = y + BASE_TILE_OFFSETS[offsetIndex][1];
		MAP *tile = getMapTile(u, v);

		if (!tile)
			continue;

		// exclude ocean

		if (is_ocean(tile))
			continue;

		// exclude base

		if (map_has_item(tile, TERRA_BASE_IN_TILE))
			continue;

		// exclude great dunes - they don't get more wet

		if (map_has_landmark(tile, LM_DUNES))
			continue;

		// exclude monotith - it is not affected by rainfall

		if (map_has_item(tile, TERRA_MONOLITH))
			continue;

		// exclude not workable tiles

		if (workableTiles.count(tile) == 0)
			continue;

		// estimate rainfall improved by condenser

		double rainfallImprovement = 0.0;

		// condenser was there but removed
		if (sign == -1)
		{
			// calculate yield score

			double nutrient = (double)map_rainfall(tile) + extraNutrient;
			double mineral = (double)min(1, map_rockiness(tile));
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

			double nutrient = (double)max(2, map_rainfall(tile) + 1) + extraNutrient;
			double mineral = (double)min(1, map_rockiness(tile));
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

//	// calculate good placement
//	// add a mismatch point for rainfall level below max for each side tile using rainfall
//
//	// [0.0, 24.0]
//	double borderMismatch = 0.0;
//
//	for (int dx = -4; dx <= +4; dx++)
//	{
//		for (int dy = -(4 - abs(dx)); dy <= +(4 - abs(dx)); dy += 2)
//		{
//			// only outer rim tiles
//
//			if (abs(dx) + abs(dy) != 4)
//				continue;
//
//			// exclude corners
//
//			if (abs(dx) == 4 || abs(dy) == 4)
//				continue;
//
//			// build tile
//
//			MAP *tile = getMapTile(x + dx, y + dy);
//			if (!tile)
//				continue;
//
//			// exclude ocean
//
//			if (is_ocean(tile))
//				continue;
//
//			// exclude base
//
//			if (map_has_item(tile, TERRA_BASE_IN_TILE))
//				continue;
//
//			// exclude great dunes - they don't get more wet
//
//			if (map_has_landmark(tile, LM_DUNES))
//				continue;
//
//			// exclude monotith - it is not affected by rainfall
//
//			if (map_has_item(tile, TERRA_MONOLITH))
//				continue;
//
//			// exclude current rocky tiles
//
//			if (map_rockiness(tile) == 2)
//				continue;
//
//			// add to borderMismatch
//
//			borderMismatch += (double)(2 - map_rainfall(tile));
//
//		}
//
//	}
//
	// calculate bonus
	// rainfall bonus should be at least 2 for condenser to make sense

//	double bonus = sign * (conf.ai_terraforming_nutrientWeight * (2.0 * (totalRainfallImprovement - 4.0)) - borderMismatch / 4.0);
	double bonus = sign * (conf.ai_terraforming_nutrientWeight * (totalRainfallImprovement - 2.0));

	debug("\t\tsign=%+1d, totalRainfallImprovement=%6.3f, bonus=%+6.3f\n", sign, totalRainfallImprovement, bonus);

	// return bonus

	return bonus;

}

/*
Estimates aquifer additional yield score.
This is very approximate.
*/
double estimateAquiferExtraYieldScore(MAP_INFO *mapInfo)
{
	int x = mapInfo->x;
	int y = mapInfo->y;

	// evaluate elevation

	// [2, 8]
    double elevationFactor = 2.0 * (double)(map_elevation(mapInfo->tile) + 1);

    // evaluate nearby bases worked tiles coverage

    int totalTileCount = 0;
    int workedTileCount = 0;

    for (int dx = -4; dx <= +4; dx++)
	{
		for (int dy = -(4 - abs(dx)); dy <= +(4 - abs(dx)); dy += 2)
		{
			MAP *tile = getMapTile(x + dx, y + dy);
			if (!tile)
				continue;

			totalTileCount++;

			if (workedTileBases.count(tile) != 0)
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

			MAP *tile = getMapTile(x + dx, y + dy);

			if (!tile)
				continue;

			// exclude base

			if (map_has_item(tile, TERRA_BASE_IN_TILE))
				continue;

			// count rivers

			if (map_has_item(tile, TERRA_RIVER))
			{
				borderRiverCount++;
			}

			// discount oceans

			if (is_ocean(tile))
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
double estimateRaiseLandExtraYieldScore(MAP_INFO *mapInfo, int cost)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *terrafromingTile = mapInfo->tile;

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
			// build tile

			MAP *tile = getMapTile(x + dx, y + dy);

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

				if (workedTileBases.count(tile) == 0)
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
void findPathStep(int id, int x, int y, MAP_INFO *step)
{
	VEH *vehicle = &(tx_vehicles[id]);
	MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);
	int vehicleTriad = veh_triad(id);
	int vehicleSpeed = veh_speed(id);

	// process land vehicles only

	if (vehicleTriad != TRIAD_LAND)
		return;

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
			return;

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
					return;

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

					step->x = element->firstStepX;
					step->y = element->firstStepY;
					step->tile = element->tile;

				}
				else
				{
					// set final destination

					step->x = x;
					step->y = y;
					step->tile = element->tile;

				}

				return;

			}

			// evaluate adjacent tiles

			for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
			{

				int adjacentTileX = wrap(element->x + BASE_TILE_OFFSETS[offsetIndex][0]);
				int adjacentTileY = element->y + BASE_TILE_OFFSETS[offsetIndex][1];
				MAP *adjacentTile = getMapTile(adjacentTileX, adjacentTileY);
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

				if (baseLocations.count(element->tile) == 0 && baseLocations.count(adjacentTile) == 0 && element->zoc && adjacentTileZOC)
					continue;

				// calculate movement cost

				int movementCost = hex_cost(vehicle->proto_id, aiFactionId, element->x, element->y, adjacentTileX, adjacentTileY, 0);

				// calculate new movement points after move

				int wholeMoves = element->movementPoints / vehicleSpeed;
				int maxNextTurnMovementPoints = (wholeMoves + 1) * vehicleSpeed;
				int adjacentTileMovementPoints = min(maxNextTurnMovementPoints, element->movementPoints + movementCost);

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
bool isRaiseLandSafe(MAP_INFO *mapInfo)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *raisedTile = mapInfo->tile;

	Faction *faction = &(tx_factions[aiFactionId]);

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

	// additional demand turn on only from population = 3

	if (populationSize >= 3)
	{
		double nutrientThreshold = conf.ai_terraforming_baseNutrientThresholdRatio * (populationSize + 1);
		if (nutrientSurplus < nutrientThreshold)
		{
			nutrientDemand = conf.ai_terraforming_baseNutrientDemandMultiplier * ((nutrientThreshold - nutrientSurplus) / nutrientThreshold);
		}

		double mineralThreshold = conf.ai_terraforming_baseMineralThresholdRatio * (nutrientSurplus - nutrientThreshold);
		if (mineralSurplus < mineralThreshold)
		{
			mineralDemand = conf.ai_terraforming_baseMineralDemandMultiplier * ((mineralThreshold - mineralSurplus) / mineralThreshold);
		}

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
double computeImprovementBaseSurplusEffectScore(BASE_INFO *baseInfo, MAP_INFO *mapInfo, MAP_STATE *currentMapState, MAP_STATE *improvedMapState, bool requireWorked)
{
	int id = baseInfo->id;
	BASE *base = baseInfo->base;
	int x = mapInfo->x;
	int y = mapInfo->y;

	// gather current surplus

	int currentNutrientSurplus = base->nutrient_surplus;
	int currentMineralSurplus = base->mineral_surplus;
	int currentEnergySurplus = base->economy_total + base->psych_total + base->labs_total;

	// apply changes

	setMapState(mapInfo, improvedMapState);
	computeBase(id);

	// gather improved surplus

	int improvedNutrientSurplus = base->nutrient_surplus;
	int improvedMineralSurplus = base->mineral_surplus;
	int improvedEnergySurplus = base->economy_total + base->psych_total + base->labs_total;

	// check if tile now worked

	bool worked = isBaseWorkedTile(base, x, y);

	// restore original state

	setMapState(mapInfo, currentMapState);
	computeBase(id);

	// return zero if improved tile is not worked

	if (requireWorked && !worked)
		return 0.0;

	// return zero if no changes at all

	if (improvedNutrientSurplus == currentNutrientSurplus && improvedMineralSurplus == currentMineralSurplus && improvedEnergySurplus == currentEnergySurplus)
		return 0.0;

	// calculate minimal nutrient and mineral surplus for demand calculation

	int minimalNutrientSurplus = min(currentNutrientSurplus, improvedNutrientSurplus);
	int minimalMineralSurplus = min(currentMineralSurplus, improvedMineralSurplus);

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

bool isInferiorImprovedTile(BASE_INFO *baseInfo, MAP_INFO *mapInfo, MAP_STATE *currentMapState, MAP_STATE *improvedMapState)
{
	int id = baseInfo->id;
	BASE *base = baseInfo->base;
	int x = mapInfo->x;
	int y = mapInfo->y;

	// apply changes

	setMapState(mapInfo, improvedMapState);

	// compute tile yield

	int nutrient = mod_nutrient_yield(aiFactionId, id, x, y, 0);
	int mineral = mod_mineral_yield(aiFactionId, id, x, y, 0);
	int energy = mod_energy_yield(aiFactionId, id, x, y, 0);

	// revert changes

	setMapState(mapInfo, currentMapState);

	// check inferior yield

	return isInferiorYield(&(unworkedTileYields[base]), nutrient, mineral, energy, NULL);

}

