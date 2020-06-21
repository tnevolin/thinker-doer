#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "ai.h"
#include "wtp.h"
#include "game.h"

// global variables

int wtpFactionId = 4;

int factionId;
Faction *faction;

double networkDemand = 0.0;
std::vector<BASE_INFO> bases;
std::unordered_set<MAP *> baseLocations;
std::unordered_map<MAP *, BASE_INFO> workedTileBases;
std::unordered_map<BASE *, int> baseTerraformingRanks;
std::unordered_set<MAP *> harvestedTiles;
std::unordered_set<MAP *> terraformedTiles;
std::unordered_set<MAP *> terraformedCondenserTiles;
std::unordered_set<MAP *> terraformedBoreholeTiles;
std::unordered_set<MAP *> terraformedAquiferTiles;
std::map<int, std::vector<FORMER_ORDER>> formerOrders;
std::map<int, FORMER_ORDER> finalizedFormerOrders;
std::map<MAP *, MAP_INFO> availableTiles;
std::map<BASE *, std::vector<TERRAFORMING_REQUEST>> regularTerraformingRequests;
std::vector<TERRAFORMING_REQUEST> freeTerraformingRequests;
std::map<int, std::vector<TERRAFORMING_REQUEST>> rankedTerraformingRequests;
std::unordered_set<MAP *> targetedTiles;
std::map<int, FORMER_ORDER *> formerOrderReferences;
std::unordered_set<MAP *> connectionNetworkLocations;
std::unordered_map<MAP *, std::unordered_map<int, std::vector<BASE_INFO>>> nearbyBaseSets;
std::unordered_map<int, int> seaRegionMappings;
std::unordered_map<MAP *, int> coastalBaseRegionMappings;
std::unordered_set<MAP *> blockedLocations;
std::unordered_set<MAP *> warzoneLocations;
std::unordered_set<MAP *> zocLocations;
std::unordered_map<BASE *, std::vector<YIELD>> unworkedTileYields;

/**
AI strategy.
*/
void ai_moveUpkeep(int id)
{
	// set faction id

	factionId = id;
	faction = &(tx_factions[factionId]);

	// skip zero faction

	if (id == 0)
		return;

	// do not use WTP algorithm for other factions

	if (wtpFactionId != -1 && factionId != wtpFactionId)
		return;

	debug("ai_moveUpkeep: factionId=%d\n", factionId);

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
	workedTileBases.clear();
	baseTerraformingRanks.clear();
	harvestedTiles.clear();
	terraformedTiles.clear();
	terraformedCondenserTiles.clear();
	terraformedBoreholeTiles.clear();
	terraformedAquiferTiles.clear();
	availableTiles.clear();
	regularTerraformingRequests.clear();
	freeTerraformingRequests.clear();
	rankedTerraformingRequests.clear();
	formerOrders.clear();
	finalizedFormerOrders.clear();
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

		if (vehicle->faction_id == factionId)
			continue;

		// exclude vehicles with pact

		if (faction->diplo_status[vehicle->faction_id] & DIPLO_PACT)
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
			(faction->diplo_status[vehicle->faction_id] & DIPLO_VENDETTA)
			&&
			// except fungal tower and artillery
			!(vehicle->proto_id == BSC_FUNGAL_TOWER || vehicle_has_ability(vehicle, ABL_ARTILLERY))
		)
		{
			for (int dx = -2; dx <= +2; dx++)
			{
				for (int dy = -(2 - abs(dx)); dy <= +(2 - abs(dx)); dy += 2)
				{
					MAP *tile = getMapTile(vehicle->x + dx, vehicle->y + dy);

					if (!tile)
						continue;

					warzoneLocations.insert(tile);

				}

			}

		}

	}

	// populate bases

	std::vector<std::set<int>> seaRegionLinkedGroups;

	for (int id = 0; id < *total_num_bases; id++)
	{
		BASE *base = &tx_bases[id];

		// exclude not own bases

		if (base->faction_id != factionId)
			continue;

		// add base

		bases.push_back({id, base});

		// add base location

		MAP *baseLocation = getMapTile(base->x, base->y);

		baseLocations.insert(baseLocation);

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
				MAP *workedTile = mapsq(base->x + BASE_TILE_OFFSETS[offsetIndex][0], base->y + BASE_TILE_OFFSETS[offsetIndex][1]);
				workedTileBases[workedTile] = {id, base};
			}

		}

	}

	// populate sea region mappings for coastal bases connecting sea regions

	debug("seaRegionLinkedGroups\n")

	for
	(
		std::vector<BASE_INFO>::iterator baseIterator = bases.begin();
		baseIterator != bases.end();
		baseIterator++
	)
	{
		BASE_INFO *baseInfo = &(*baseIterator);
		BASE *base = baseInfo->base;

		MAP *baseTile = getMapTile(base->x, base->y);

		if (!is_ocean(baseTile))
		{
			std::set<int> seaRegionLinkedGroup;

			for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
			{
				MAP *adjacentTile = getMapTile(base->x + BASE_TILE_OFFSETS[offsetIndex][0], base->y + BASE_TILE_OFFSETS[offsetIndex][1]);

				if (is_ocean(adjacentTile))
				{
					int region = adjacentTile->region;
					seaRegionLinkedGroup.insert(region);
				}

			}

			if (seaRegionLinkedGroup.size() >= 1)
			{
				seaRegionLinkedGroups.push_back(seaRegionLinkedGroup);

				if (DEBUG)
				{
					for
					(
						std::set<int>::iterator seaRegionLinkedGroupIterator = seaRegionLinkedGroup.begin();
						seaRegionLinkedGroupIterator != seaRegionLinkedGroup.end();
						seaRegionLinkedGroupIterator++
					)
					{
						debug(" %3d", *seaRegionLinkedGroupIterator);
					}
					debug("\n");
				}

			}

		}

	}

	// populate sea region mappings

	debug("aggregation\n");

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

				debug("\t%3d, %3d\n", i, j);

				for
				(
					std::set<int>::iterator seaRegionLinkedGroup2Iterator = seaRegionLinkedGroup2->begin();
					seaRegionLinkedGroup2Iterator != seaRegionLinkedGroup2->end();
					seaRegionLinkedGroup2Iterator++
				)
				{
					int seaRegionLinkedGroup2Element = *seaRegionLinkedGroup2Iterator;

					debug("\t\t%3d\n", seaRegionLinkedGroup2Element);

					if (seaRegionLinkedGroup1->count(seaRegionLinkedGroup2Element) != 0)
					{
						debug("\t\t\tfound\n");

						seaRegionLinkedGroup1->insert(seaRegionLinkedGroup2->begin(), seaRegionLinkedGroup2->end());
						seaRegionLinkedGroup2->clear();

						aggregated = true;

						break;

					}

					debug("\t\t\t%3d %3d\n", seaRegionLinkedGroup1->size(), seaRegionLinkedGroup2->size());

				}

			}

		}

	}

	debug("after aggregation\n");

	if (DEBUG)
	{
		for (size_t i = 0; i < seaRegionLinkedGroups.size(); i++)
		{
			std::set<int> *seaRegionLinkedGroup = &(seaRegionLinkedGroups[i]);

			debug("group:");

			for
			(
				std::set<int>::iterator seaRegionLinkedGroupIterator = seaRegionLinkedGroup->begin();
				seaRegionLinkedGroupIterator != seaRegionLinkedGroup->end();
				seaRegionLinkedGroupIterator++
			)
			{
				int region = *seaRegionLinkedGroupIterator;

				debug(" %3d", region);

			}

			debug("\n");

		}

	}

	debug("seaRegionMappings\n");

	for (size_t i = 0; i < seaRegionLinkedGroups.size(); i++)
	{
		std::set<int> *seaRegionLinkedGroup = &(seaRegionLinkedGroups[i]);

		debug("group\n");

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

			debug("\t%3d => %3d\n", region, mappingRegion);

		}

	}

	// populate coastal base sea region mappings

	debug("coastalBaseRegionMappings\n");

	for
	(
		std::vector<BASE_INFO>::iterator baseIterator = bases.begin();
		baseIterator != bases.end();
		baseIterator++
	)
	{
		BASE_INFO *baseInfo = &(*baseIterator);
		BASE *base = baseInfo->base;

		MAP *baseTile = getMapTile(base->x, base->y);

		if (!is_ocean(baseTile))
		{
			for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
			{
				MAP *adjacentTile = getMapTile(base->x + BASE_TILE_OFFSETS[offsetIndex][0], base->y + BASE_TILE_OFFSETS[offsetIndex][1]);

				if (is_ocean(adjacentTile))
				{
					int terraformingRegion = getTerraformingRegion(adjacentTile->region);
					coastalBaseRegionMappings[baseTile] = seaRegionMappings[terraformingRegion];

					debug("\t(%3d,%3d) => %3d\n", base->x, base->y, terraformingRegion);

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

		if (vehicle->faction_id != factionId)
			continue;

		// clear pad_0 field for all formers

		vehicle->pad_0 = -1;

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

				// populate terraformed condenser tiles

				if (vehicle->move_status == ORDER_CONDENSER)
				{
					terraformedCondenserTiles.insert(terraformedTile);
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

						region = getTerraformingRegion(vehicleTile->region);

					}

					break;

				}

				if (formerOrders.count(region) == 0)
				{
					// add new region

					formerOrders[region] = std::vector<FORMER_ORDER>();

				}

				// store vehicle current id in pad_0 field

				vehicle->pad_0 = id;

				// add vehicle

				formerOrders[region].push_back({id, vehicle, -1, -1, -1});

			}

		}

	}

	if (DEBUG)
	{
		debug("FORMER_ORDERS\n");

		for
		(
			std::map<int, std::vector<FORMER_ORDER>>::iterator formerOrdersIterator = formerOrders.begin();
			formerOrdersIterator != formerOrders.end();
			formerOrdersIterator++
		)
		{
			int region = formerOrdersIterator->first;
			std::vector<FORMER_ORDER> *regionFormerOrders = &(formerOrdersIterator->second);

			debug("\tregion=%d\n", region);

			for
			(
				std::vector<FORMER_ORDER>::iterator regionFormerOrdersIterator = regionFormerOrders->begin();
				regionFormerOrdersIterator != regionFormerOrders->end();
				regionFormerOrdersIterator++
			)
			{
				FORMER_ORDER *formerOrder = &(*regionFormerOrdersIterator);

				debug("\t\t[%3d]\n", formerOrder->id);

			}

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
			MAP *tile = getMapTile(x, y);

			// exclude impossible coordinates

			if (!tile)
				continue;

			// exclude volcano mouth

			if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
				continue;

			// exclude base in tile

			if (tile->items & TERRA_BASE_IN_TILE)
				continue;

			// exclude enemy territory

			if (!(tile->owner == -1 || tile->owner == factionId))
				continue;

			// exclude blocked locations

			if (blockedLocations.count(tile) != 0)
				continue;

			// exclude warzone locations

			if (warzoneLocations.count(tile) != 0)
				continue;

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

	for
	(
		std::vector<BASE_INFO>::iterator basesIterator = bases.begin();
		basesIterator != bases.end();
		basesIterator++
	)
	{
		BASE_INFO *baseInfo = &(*basesIterator);
		int id = baseInfo->id;
		BASE *base = baseInfo->base;

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

			int nutrient = mod_nutrient_yield(factionId, id, x, y, 0);
			int mineral = mod_mineral_yield(factionId, id, x, y, 0);
			int energy = mod_energy_yield(factionId, id, x, y, 0);

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

	int networkType = (has_tech(factionId, tx_terraform[FORMER_MAGTUBE].preq_tech) ? TERRA_MAGTUBE : TERRA_ROAD);

	double networkCoverage = 0.0;

	for (int i = 0; i < *map_area_tiles; i++)
	{
		// build tile

		MAP *tile = &((*tx_map_ptr)[i]);

		// exclude ocean

		if (is_ocean(tile))
			continue;

		// exclude not own territory

		if (tile->owner != factionId)
			continue;

		// count network coverage

		if (map_has_item(tile, networkType))
		{
			networkCoverage += 1.0;
		}

	}

	double networkThreshold = conf.ai_terraforming_networkCoverageThreshold * (double)bases.size();

	if (networkCoverage < networkThreshold)
	{
		networkDemand = (networkThreshold - networkCoverage) / networkThreshold;
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
		MAP_INFO *mapInfo = &(availableTileIterator->second);

		// generate request

		generateTerraformingRequest(mapInfo);

	}

	// debug

	if (DEBUG)
	{
		debug("TERRAFORMING_REQUESTS\n");

		for
		(
			std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator regularTerraformingRequestsIterator = regularTerraformingRequests.begin();
			regularTerraformingRequestsIterator != regularTerraformingRequests.end();
			regularTerraformingRequestsIterator++
		)
		{
			BASE *base = regularTerraformingRequestsIterator->first;
			std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(regularTerraformingRequestsIterator->second);

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
					"\t\t(%3d,%3d) %6.2f %-10s -> %s\n",
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

void generateTerraformingRequest(MAP_INFO *mapInfo)
{
	// regular terraforming options

	{
		TERRAFORMING_SCORE terraformingScoreObject;
		TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
		calculateRegularTerraformingScore(mapInfo, terraformingScore);

		if (terraformingScore->base != NULL && terraformingScore->action != -1 && terraformingScore->score > 0.0)
		{
			BASE *base = terraformingScore->base;

			// generate terraforming request

			if (regularTerraformingRequests.count(base) == 0)
			{
				regularTerraformingRequests[base] = std::vector<TERRAFORMING_REQUEST>();
			}

			regularTerraformingRequests[base].push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score, terraformingScore->ranked, 0});

		}

	}

	// aquifier

	{
		TERRAFORMING_SCORE terraformingScoreObject;
		TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
		calculateAquiferTerraformingScore(mapInfo, terraformingScore);

		if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
		{
			// generate terraforming request

			freeTerraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score, terraformingScore->ranked, 0});

		}

	}

	// network

	{
		TERRAFORMING_SCORE terraformingScoreObject;
		TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
		calculateNetworkTerraformingScore(mapInfo, terraformingScore);

		if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
		{
			// generate terraforming request

			freeTerraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score, terraformingScore->ranked, 0});

		}

	}

}

/**
Comparison function for base terraforming requests.
Sort in scoring order descending.
*/
bool compareTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2)
{
	return (terraformingRequest1.score >= terraformingRequest2.score);
}

void sortBaseTerraformingRequests()
{
	for
	(
		std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator regularTerraformingRequestsIterator = regularTerraformingRequests.begin();
		regularTerraformingRequestsIterator != regularTerraformingRequests.end();
		regularTerraformingRequestsIterator++
	)
	{
		std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(regularTerraformingRequestsIterator->second);

		std::sort(terraformingRequests->begin(), terraformingRequests->end(), compareTerraformingRequests);

	}

	// debug

	if (DEBUG)
	{
		debug("TERRAFORMING_REQUESTS - SORTED\n");

		for
		(
			std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator regularTerraformingRequestsIterator = regularTerraformingRequests.begin();
			regularTerraformingRequestsIterator != regularTerraformingRequests.end();
			regularTerraformingRequestsIterator++
		)
		{
			BASE *base = regularTerraformingRequestsIterator->first;
			std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(regularTerraformingRequestsIterator->second);

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
					"\t\t(%3d,%3d) %6.2f %-10s -> %s\n",
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

/**
Comparison function for terraforming requests.
Sort in ranking order ascending then in scoring order descending.
*/
bool compareRankedTerraformingRequests(TERRAFORMING_REQUEST terraformingRequest1, TERRAFORMING_REQUEST terraformingRequest2)
{
	return (terraformingRequest1.rank != terraformingRequest2.rank ? terraformingRequest1.rank < terraformingRequest2.rank : terraformingRequest1.score >= terraformingRequest2.score);
}

/**
Sort terraforming requests within regions by rank and score.
*/
void rankTerraformingRequests()
{
	// base terraforming requests

	for
	(
		std::map<BASE *, std::vector<TERRAFORMING_REQUEST>>::iterator regularTerraformingRequestsIterator = regularTerraformingRequests.begin();
		regularTerraformingRequestsIterator != regularTerraformingRequests.end();
		regularTerraformingRequestsIterator++
	)
	{
		BASE *base = regularTerraformingRequestsIterator->first;
		std::vector<TERRAFORMING_REQUEST> *terraformingRequests = &(regularTerraformingRequestsIterator->second);

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

			if (terraformingRequest->ranked)
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

			if (rankedTerraformingRequests.count(region) == 0)
			{
				rankedTerraformingRequests[region] = std::vector<TERRAFORMING_REQUEST>();
			}

			// add terraforming request

			rankedTerraformingRequests[region].push_back(*terraformingRequest);

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

		if (rankedTerraformingRequests.count(region) == 0)
		{
			rankedTerraformingRequests[region] = std::vector<TERRAFORMING_REQUEST>();
		}

		// add terraforming request

		rankedTerraformingRequests[region].push_back(*terraformingRequest);

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

		// sort requests

		std::sort(regionTerraformingRequests->begin(), regionTerraformingRequests->end(), compareRankedTerraformingRequests);

		// apply proximity rules

		for
		(
			std::vector<TERRAFORMING_REQUEST>::iterator regionTerraformingRequestsIterator = regionTerraformingRequests->begin();
			regionTerraformingRequestsIterator != regionTerraformingRequests->end();
		)
		{
			TERRAFORMING_REQUEST *terraformingRequest = &(*regionTerraformingRequestsIterator);

			// proximity rule

			std::unordered_map<int, int>::const_iterator proximityRuleIterator = PROXIMITY_RULES.find(terraformingRequest->action);

			if
			(
				// action has proximity rule
				proximityRuleIterator != PROXIMITY_RULES.end()
				&&
				// there is another action above too close
				hasNearbyTerraformingRequestAction
				(
					regionTerraformingRequests->begin(),
					regionTerraformingRequestsIterator,
					terraformingRequest->action,
					terraformingRequest->x,
					terraformingRequest->y,
					proximityRuleIterator->second
				)
			)
			{
				// delete this request and advance iterator

				regionTerraformingRequestsIterator = regionTerraformingRequests->erase(regionTerraformingRequestsIterator);

			}
			else
			{
				// advance iterator

				regionTerraformingRequestsIterator++;

			}

		}

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
					"\t\t(%3d,%3d) %d %6.2f %-10s -> %s\n",
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
		std::map<int, std::vector<FORMER_ORDER>>::iterator formerOrderIterator = formerOrders.begin();
		formerOrderIterator != formerOrders.end();
	)
	{
		int region = formerOrderIterator->first;
		std::vector<FORMER_ORDER> *regionFormerOrders = &(formerOrderIterator->second);

		// delete formers without requests in this region
		if (rankedTerraformingRequests.count(region) == 0)
		{
			// erasing region and automatically advance region iterator

			formerOrderIterator = formerOrders.erase(formerOrderIterator);

		}
		// otherwise assign orders
		else
		{
			// assign as much orders as possible

			std::vector<TERRAFORMING_REQUEST> *regionRankedTerraformingRequests = &(rankedTerraformingRequests[region]);

			std::vector<TERRAFORMING_REQUEST>::iterator regionRankedTerraformingRequestsIterator = regionRankedTerraformingRequests->begin();
			std::vector<FORMER_ORDER>::iterator regionFormerOrdersIterator = regionFormerOrders->begin();

			for
			(
				;
				regionRankedTerraformingRequestsIterator != regionRankedTerraformingRequests->end() && regionFormerOrdersIterator != regionFormerOrders->end();
				regionFormerOrdersIterator++
			)
			{
				FORMER_ORDER *formerOrder = &(*regionFormerOrdersIterator);
				TERRAFORMING_REQUEST *terraformingRequest = &(*regionRankedTerraformingRequestsIterator);

				formerOrder->x = terraformingRequest->x;
				formerOrder->y = terraformingRequest->y;
				formerOrder->action = terraformingRequest->action;

				// erase assigned ranked terraforming request

				regionRankedTerraformingRequestsIterator = regionRankedTerraformingRequests->erase(regionRankedTerraformingRequestsIterator);

			}

			// erase the rest of unassigned orders if any

			regionFormerOrders->erase(regionFormerOrdersIterator, regionFormerOrders->end());

			// advance region iterator

			formerOrderIterator++;

		}

	}

	// debug

	if (DEBUG)
	{
		debug("FORMER ORDERS\n");

		for
		(
			std::map<int, std::vector<FORMER_ORDER>>::iterator formerOrdersIterator = formerOrders.begin();
			formerOrdersIterator != formerOrders.end();
			formerOrdersIterator++
		)
		{
			int region = formerOrdersIterator->first;
			std::vector<FORMER_ORDER> *regionFormerOrders = &(formerOrdersIterator->second);

			debug("\tregion=%3d\n", region);

			for
			(
				std::vector<FORMER_ORDER>::iterator regionFormerOrdersIterator = regionFormerOrders->begin();
				regionFormerOrdersIterator != regionFormerOrders->end();
				regionFormerOrdersIterator++
			)
			{
				FORMER_ORDER *formerOrder = &(*regionFormerOrdersIterator);

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

/**
Selects best terraforming option for given tile and calculates its terraforming score.
*/
void calculateRegularTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	bool sea = is_ocean(mapInfo->tile);

	// do not evavluate monolith for yield options

	if (map_has_item(tile, TERRA_MONOLITH))
		return;

	debug
	(
		"calculateTerraformingScore: (%3d,%3d)\n",
		x,
		y
	)

	// calculate range to the closest available former and range penalty multiplier

	int closestAvailableFormerRange = calculateClosestAvailableFormerRange(mapInfo);
	double rangePenaltyMultiplier = exp(-(double)closestAvailableFormerRange / (sea ? conf.ai_terraforming_waterDistanceScale : conf.ai_terraforming_landDistanceScale));

	// store tile values in the 3x3 grid area

	MAP_STATE currentMapStateObject;
	MAP_STATE *currentMapState = &currentMapStateObject;
	getMapState(mapInfo, currentMapState);

	// find best terraforming option

	for (size_t optionIndex = 0; optionIndex < REGULAR_TERRAFORMING_OPTIONS.size(); optionIndex++)
	{
		const TERRAFORMING_OPTION *option = &(REGULAR_TERRAFORMING_OPTIONS[optionIndex]);

		// exclude wrong body type

		if (option->sea != sea)
			continue;

		// exclude wrong terrain type

		if (option->rocky && !(tile->rocks & TILE_ROCKY))
			continue;

		// check if main action is available when required

		int mainAction = *(option->actions.begin());

		if (option->firstActionRequired && !isTerraformingAvailable(x, y, mainAction))
			continue;

		// initialize variables

		MAP_STATE improvedMapStateObject;
		MAP_STATE *improvedMapState = &improvedMapStateObject;
		copyMapState(improvedMapState, currentMapState);

		int terraformingTime = 0;
		int firstAction = -1;
		double improvementScore = 0.0;
		BASE *affectedBase = NULL;

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

			if (!isTerraformingAvailable(x, y, action))
				continue;

//			// skip unneeded actions
//
//			if (!isTerraformingRequired(tile, action))
//				continue;
//
			// generate terraforming change and add terraforming time for non completed action

			if (!isTerraformingCompleted(mapInfo, action))
			{
				// remove fungus if needed

				if ((improvedMapState->items & TERRA_FUNGUS) && isRemoveFungusRequired(action))
				{
					int removeFungusAction = FORMER_REMOVE_FUNGUS;

					// generate terraforming change

					generateTerraformingChange(improvedMapState, removeFungusAction);

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

					// store first action and compute terraforming time

					if (firstAction == -1)
					{
						// store first action

						firstAction = removeFungusAction;

					}

					// add penalty for nearby forest/kelp

					int nearbyForestKelpCount = nearby_items(x, y, 1, (sea ? TERRA_FARM : TERRA_FOREST));
					improvementScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;

				}

				// level terrain if needed

				if (!sea && (improvedMapState->rocks & TILE_ROCKY) && isLevelTerrainRequired(action))
				{
					int levelTerrainAction = FORMER_LEVEL_TERRAIN;

					// generate terraforming change

					generateTerraformingChange(improvedMapState, levelTerrainAction);

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

					// store first action and compute terraforming time

					if (firstAction == -1)
					{
						// store first action

						firstAction = levelTerrainAction;

					}

				}

				// execute main action

				// generate terraforming change

				generateTerraformingChange(improvedMapState, action);

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

				// store first action and compute terraforming time

				if (firstAction == -1)
				{
					// store first action

					firstAction = action;

				}

				// add penalty for nearby forest/kelp

				if (sea)
				{
					// kelp farm

					if (action == FORMER_FARM)
					{
						int nearbyKelpCount = nearby_items(x, y, 1, TERRA_FARM);
						improvementScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyKelpCount;
					}

				}
				else
				{
					// forest

					if (action == FORMER_FOREST)
					{
						int nearbyForestCount = nearby_items(x, y, 1, TERRA_FOREST);
						improvementScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestCount;
					}

				}

			}

			// special yield computation for area effects

			if (option->area)
			{
				switch (action)
				{
				case FORMER_CONDENSER:
					improvementScore += estimateCondenserExtraYieldScore(mapInfo);
					break;

				case FORMER_ECH_MIRROR:
					improvementScore += estimateEchelonMirrorExtraYieldScore(mapInfo);
					break;

				case FORMER_AQUIFER:
					improvementScore += estimateAquiferExtraYieldScore(mapInfo);
					break;

				}

				// find most affected base

				std::vector<BASE_INFO> *nearbyBases = &(nearbyBaseSets[mapInfo->tile][1]);

				int bestWorkedTileCount = 0;
				BASE *mostAffectedBase = NULL;

				for
				(
					std::vector<BASE_INFO>::iterator nearbyBasesIterator = nearbyBases->begin();
					nearbyBasesIterator != nearbyBases->end();
					nearbyBasesIterator++
				)
				{
					BASE_INFO *baseInfo = &(*nearbyBasesIterator);
					BASE *base = baseInfo->base;

					int workedTileCount = 0;

					for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
					{
						MAP *adjacentTile = getMapTile(x + BASE_TILE_OFFSETS[offsetIndex][0], y + BASE_TILE_OFFSETS[offsetIndex][1]);

						if (!adjacentTile)
							continue;

						// exclude ocean

						if (is_ocean(adjacentTile))
							continue;

						// increment worked tile count if this is the one

						if (workedTileBases.count(adjacentTile) && workedTileBases[adjacentTile].base == base)
						{
							workedTileCount++;
						}

					}

					if (workedTileCount > bestWorkedTileCount)
					{
						bestWorkedTileCount = workedTileCount;
						mostAffectedBase = base;
					}

				}

				// save affected base

				if (mostAffectedBase != NULL)
				{
					affectedBase = mostAffectedBase;
				}

			}

		}

		// calculate yield improvement score

		TERRAFORMING_SCORE optionTerraformingScore;
		calculateYieldImprovementScore(mapInfo, currentMapState, improvedMapState, &optionTerraformingScore);

		// set affected base if not yet

		if (affectedBase == NULL)
		{
			affectedBase = optionTerraformingScore.base;
		}

		// update score

		improvementScore += optionTerraformingScore.score;

		// factor improvement score with terraforming time and range penalty

		double terraformingScore = improvementScore * exp(-(double)terraformingTime / conf.ai_terraforming_resourceLifetime) * rangePenaltyMultiplier;

		debug
		(
			"\t%-10s: score=%6.2f, time=%2d, range=%2d, final=%6.2f\n",
			option->name,
			improvementScore,
			terraformingTime,
			closestAvailableFormerRange,
			terraformingScore
		)
		;

		// update score

		if (terraformingScore > bestTerraformingScore->score)
		{
			bestTerraformingScore->base = affectedBase;
			bestTerraformingScore->option = option;
			bestTerraformingScore->action = firstAction;
			bestTerraformingScore->score = terraformingScore;
			bestTerraformingScore->ranked = option->ranked;

		}

	}

	// calculate exclusivity bonus

	if (bestTerraformingScore->option)
	{
		double exclusivityBonus = calculateExclusivityBonus(mapInfo, &(bestTerraformingScore->option->actions));

		debug
		(
			"\texclusivity: score=%6.2f, bonus=%+6.2f, combined=%6.2f\n",
			bestTerraformingScore->score,
			exclusivityBonus,
			bestTerraformingScore->score + exclusivityBonus
		)
		;

		bestTerraformingScore->score += exclusivityBonus;

	}

}

/**
Aquifer calculations.
*/
void calculateAquiferTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	bool sea = is_ocean(mapInfo->tile);

	debug
	(
		"calculateTerraformingScore: (%3d,%3d)\n",
		x,
		y
	)

	// calculate range to the closest available former and range penalty multiplier

	int closestAvailableFormerRange = calculateClosestAvailableFormerRange(mapInfo);
	double rangePenaltyMultiplier = exp(-(double)closestAvailableFormerRange / (sea ? conf.ai_terraforming_waterDistanceScale : conf.ai_terraforming_landDistanceScale));

	// store tile values

	MAP_STATE currentMapStateObject;
	MAP_STATE *currentMapState = &currentMapStateObject;
	getMapState(mapInfo, currentMapState);

	const TERRAFORMING_OPTION *option = &(AQUIFER_TERRAFORMING_OPTIONS[0]);

	// exclude wrong body type

	if (option->sea != sea)
		return;

	// initialize variables

	int terraformingTime = 0;
	int firstAction = -1;
	double improvementScore = 0.0;

	int action = FORMER_AQUIFER;

	// skip unavailable actions

	if (!isTerraformingAvailable(x, y, action))
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

	terraformingTime = tx_terraform[action].rate;

	// special yield computation for aquifer

	improvementScore += estimateAquiferExtraYieldScore(mapInfo);

	// factor improvement score with terraforming time and range penalty

	double terraformingScore = improvementScore * exp(-(double)terraformingTime / conf.ai_terraforming_resourceLifetime) * rangePenaltyMultiplier;

	debug
	(
		"\t%-10s: score=%6.2f, time=%2d, range=%2d, final=%6.2f\n",
		option->name,
		improvementScore,
		terraformingTime,
		closestAvailableFormerRange,
		terraformingScore
	)
	;

	// update score

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

}

/**
Network calculations.
*/
void calculateNetworkTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	bool sea = is_ocean(mapInfo->tile);

	debug
	(
		"calculateTerraformingScore: (%3d,%3d)\n",
		x,
		y
	)

	// calculate range to the closest available former and range penalty multiplier

	int closestAvailableFormerRange = calculateClosestAvailableFormerRange(mapInfo);
	double rangePenaltyMultiplier = exp(-(double)closestAvailableFormerRange / (sea ? conf.ai_terraforming_waterDistanceScale : conf.ai_terraforming_landDistanceScale));

	// store tile values

	MAP_STATE currentMapStateObject;
	MAP_STATE *currentMapState = &currentMapStateObject;
	getMapState(mapInfo, currentMapState);

	const TERRAFORMING_OPTION *option = &(NETWORK_TERRAFORMING_OPTIONS[0]);

	// exclude wrong body type

	if (option->sea != sea)
		return;

	// initialize variables

	MAP_STATE improvedMapStateObject;
	MAP_STATE *improvedMapState = &improvedMapStateObject;
	copyMapState(improvedMapState, currentMapState);

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

	if (!isTerraformingAvailable(x, y, action))
		return;

	// remove fungus if needed

	if ((improvedMapState->items & TERRA_FUNGUS) && isRemoveFungusRequired(action))
	{
		int removeFungusAction = FORMER_REMOVE_FUNGUS;

		// store first action

		if (firstAction == -1)
		{
			firstAction = removeFungusAction;
		}

		// generate terraforming change

		generateTerraformingChange(improvedMapState, removeFungusAction);

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

		// add penalty for nearby forest/kelp

		int nearbyForestKelpCount = nearby_items(x, y, 1, (sea ? TERRA_FARM : TERRA_FOREST));
		improvementScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;

	}

	// store first action

	if (firstAction == -1)
	{
		firstAction = action;
	}

	// generate terraforming change

	generateTerraformingChange(improvedMapState, action);

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

	// special network bonus

	improvementScore += (1.0 + networkDemand) * calculateNetworkScore(mapInfo, action);

	// factor improvement score with terraforming time and range penalty

	double terraformingScore = improvementScore * exp(-(double)terraformingTime / conf.ai_terraforming_resourceLifetime) * rangePenaltyMultiplier;

	debug
	(
		"\t%-10s: score=%6.2f, time=%2d, range=%2d, final=%6.2f\n",
		option->name,
		improvementScore,
		terraformingTime,
		closestAvailableFormerRange,
		terraformingScore
	)
	;

	// update score

	bestTerraformingScore->option = option;
	bestTerraformingScore->action = firstAction;
	bestTerraformingScore->score = terraformingScore;

}

/**
Handles movement phase.
*/
int enemyMoveFormer(int id)
{
	// do not use WTP algorithm for other factions

	if (wtpFactionId != -1 && factionId != wtpFactionId)
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

	if (finalizedFormerOrders.count(aiVehicleId) == 0)
		return SYNC;

	// get former order

	FORMER_ORDER *formerOrder = &(finalizedFormerOrders[aiVehicleId]);

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

				std::vector<TERRAFORMING_REQUEST> *regionRankedTerraformingRequests = &(rankedTerraformingRequests[region]);

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

void calculateYieldImprovementScore(MAP_INFO *mapInfo, MAP_STATE *currentMapState, MAP_STATE *improvedMapState, TERRAFORMING_SCORE *terraformingScore)
{
	MAP *tile = mapInfo->tile;

	std::vector<BASE_INFO *> affectedBases;

	// populate affected bases

	if (workedTileBases.count(tile) != 0)
	{
		affectedBases.push_back(&(workedTileBases[tile]));
	}
	else
	{
		// get nearby bases

		std::vector<BASE_INFO> *nearbyBases = &(nearbyBaseSets[tile][0]);

		for
		(
			std::vector<BASE_INFO>::iterator nearbyBasesIterator = nearbyBases->begin();
			nearbyBasesIterator != nearbyBases->end();
			nearbyBasesIterator++
		)
		{
			BASE_INFO *baseInfo = &(*nearbyBasesIterator);

			affectedBases.push_back(baseInfo);

		}

	}

	// calculate yield improvement score

	for
	(
		std::vector<BASE_INFO *>::iterator affectedBasesIterator = affectedBases.begin();
		affectedBasesIterator != affectedBases.end();
		affectedBasesIterator++
	)
	{
		BASE_INFO *baseInfo = *affectedBasesIterator;
		int id = baseInfo->id;
		BASE *base = baseInfo->base;

		// calculate additional demand

		double nutrientDemand = 0.0;
		double nutrientThreshold = conf.ai_terraforming_baseNutrientThresholdRatio * (double)(base->pop_size + 1);
		if ((double)base->nutrient_surplus < nutrientThreshold)
		{
			nutrientDemand = conf.ai_terraforming_baseNutrientDemandMultiplier * (nutrientThreshold - (double)base->nutrient_surplus) / nutrientThreshold;
		}

		double mineralDemand = 0.0;
		double mineralThreshold = conf.ai_terraforming_baseMineralThresholdRatio * ((double)base->nutrient_surplus - nutrientThreshold);
		if ((double)base->mineral_surplus < mineralThreshold)
		{
			mineralDemand = (mineralThreshold - (double)base->mineral_surplus) / mineralThreshold;
		}

		// set improved local terraforming state

		setMapState(mapInfo, improvedMapState);

		// calculate improved yield

		int improvedNutrient = mod_nutrient_yield(factionId, id, mapInfo->x, mapInfo->y, 0);
		int improvedMineral = mod_mineral_yield(factionId, id, mapInfo->x, mapInfo->y, 0);
		int improvedEnergy = mod_energy_yield(factionId, id, mapInfo->x, mapInfo->y, 0);

		double improvedYieldScore = calculateYieldWeightedScore(nutrientDemand, mineralDemand, improvedNutrient, improvedMineral, improvedEnergy);

		// set current local terraforming state

		setMapState(mapInfo, currentMapState);

		// update best yield improvement score

		if (terraformingScore->base == NULL || improvedYieldScore > terraformingScore->score)
		{
			terraformingScore->score = improvedYieldScore;

			// set affected base on condition

			if (workedTileBases.count(tile) != 0)
			{
				// calculate current yield

				int currentNutrient = mod_nutrient_yield(factionId, id, mapInfo->x, mapInfo->y, 0);
				int currentMineral = mod_mineral_yield(factionId, id, mapInfo->x, mapInfo->y, 0);
				int currentEnergy = mod_energy_yield(factionId, id, mapInfo->x, mapInfo->y, 0);

				double currentYieldScore = calculateYieldWeightedScore(nutrientDemand, mineralDemand, currentNutrient, currentMineral, currentEnergy);

				// assign base only if resulting yield is better

				if (improvedYieldScore > currentYieldScore)
				{
					terraformingScore->base = base;
				}

			}
			else
			{
				// assign base only if resulting yield is not inferior

				if (!isInferiorYield(&unworkedTileYields[base], improvedNutrient, improvedMineral, improvedEnergy, NULL))
				{
					terraformingScore->base = base;
				}

			}

		}

	}

	return;

}

/**
Checks whether tile is own workable tile but not base square.
*/
bool isOwnWorkableTile(int x, int y)
{
	MAP *tile = getMapTile(x, y);

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
Determines whether terraforming is already completed in this tile.
*/
bool isTerraformingCompleted(MAP_INFO *mapInfo, int action)
{
	MAP *tile = mapInfo->tile;

	return (tile->items & tx_terraform[action].added_items_flag);

}

/**
Determines whether terraforming can be done in this square with removing fungus and leveling terrain if needed.
Terraforming is considered available if this item is already built.
*/
bool isTerraformingAvailable(int x, int y, int action)
{
	MetaFaction *metaFaction = &(tx_metafactions[factionId]);
	bool aquatic = metaFaction->rule_flags & FACT_AQUATIC;
	MAP *tile = mapsq(x, y);
	bool ocean = is_ocean(tile);
	bool oceanDeep = is_ocean_deep(tile);

	// action is considered available if is already built

	if (map_has_item(tile, tx_terraform[action].added_items_flag))
		return true;

	// action is available only when enabled and researched

	if (!has_terra(factionId, action, ocean))
		return false;

	// terraforming is not available in base square

	if (tile->items & TERRA_BASE_IN_TILE)
		return false;

	// check if improvement already exists

	if (tile->items & tx_terraform[action].added_items_flag)
		return false;

	// ocean improvements in deep ocean are available for aquatic faction with Adv. Ecological Engineering only

	if (oceanDeep)
	{
		if (!(aquatic && has_tech(factionId, TECH_EcoEng2)))
			return false;
	}

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
	case FORMER_CONDENSER:
		// condenser should not be built near another building condenser
		return !isNearbyCondeserUnderConstruction(x, y);
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

bool isVehicleSupply(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_SUPPLY_TRANSPORT);
}

bool isVehicleFormer(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_TERRAFORMING_UNIT);
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

/**
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
			MAP *tile = getMapTile(wrap(x + dx), y + dy);

			if (terraformedBoreholeTiles.count(tile) == 1)
                return true;

        }

    }

    return false;

}

bool isNearbyCondeserUnderConstruction(int x, int y)
{
	std::unordered_map<int, int>::const_iterator proximityRuleIterator = PROXIMITY_RULES.find(FORMER_CONDENSER);

	// proximity rule not found

	if (proximityRuleIterator == PROXIMITY_RULES.end())
		return false;

	int range = proximityRuleIterator->second;

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
			MAP *tile = getMapTile(wrap(x + dx), y + dy);

			if (terraformedAquiferTiles.count(tile) == 1)
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

int getBaseTerraformingRank(BASE *base)
{
	return (baseTerraformingRanks.count(base) == 0 ? 0 : baseTerraformingRanks[base]);
}

BASE *findAffectedBase(int x, int y)
{
	for
	(
		std::vector<BASE_INFO>::iterator baseIterator = bases.begin();
		baseIterator != bases.end();
		baseIterator++
	)
	{
		BASE_INFO *baseInfo = &(*baseIterator);
		BASE *base = baseInfo->base;

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

	if (formerOrders.count(region) == 0)
		return INT_MAX;

	// get regionFormerOrders

	std::vector<FORMER_ORDER> *regionFormerOrders = &(formerOrders[region]);

	// find nearest available former in this region

	int closestFormerRange = INT_MAX;

	for
	(
		std::vector<FORMER_ORDER>::iterator regionFormerOrderIterator = regionFormerOrders->begin();
		regionFormerOrderIterator != regionFormerOrders->end();
		regionFormerOrderIterator++
	)
	{
		FORMER_ORDER *regionFormerOrder = &(*regionFormerOrderIterator);
		VEH *vehicle = regionFormerOrder->vehicle;

		int range = map_range(x, y, vehicle->x, vehicle->y);

		if (range < closestFormerRange)
		{
			closestFormerRange = range;
		}

	}

	return closestFormerRange;

}

/**
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

MAP *getMapTile(int x, int y)
{
	// ignore impossible combinations

	if ((x + y)%2 != 0)
		return NULL;

	// return map tile with wrapped x if needed

	return mapsq(wrap(x), y);

}

int getTerraformingRegion(int region)
{
	return (seaRegionMappings.count(region) == 0 ? region : seaRegionMappings[region]);
}

/**
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
			int workedTileX = base->x + BASE_TILE_OFFSETS[workedTileIndex][0];
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

		nutrient = (2.0 / 2.0) - rainfall;
		mineral = (2.0 / 2.0) - rockiness;
		energy = (3.0 / 2.0) - elevation;

	}
	else
	{
		// mine is better built low

		if (actionSet.count(FORMER_MINE))
		{
			energy += (3.0 / 2.0) - elevation;
		}

		// rocky mine is also better built in dry tiles

		if (actionSet.count(FORMER_MINE) && rockiness == 2)
		{
			nutrient += (2.0 / 2.0) - rainfall;
		}

		// condenser is better built low

		if (actionSet.count(FORMER_CONDENSER))
		{
			energy += (3.0 / 2.0) - elevation;
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
		std::vector<TERRAFORMING_REQUEST>::iterator baseTerraformingRequestsIterator = begin;
		baseTerraformingRequestsIterator != end;
		baseTerraformingRequestsIterator++
	)
	{
		TERRAFORMING_REQUEST *terraformingRequest = &(*baseTerraformingRequestsIterator);

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

/**
Estimates condenser yield score in adjacent tiles.
*/
double estimateCondenserExtraYieldScore(MAP_INFO *mapInfo)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *improvedTile = mapInfo->tile;

	// calculate forest yield with best forest facilities available

	int forestNutrient = tx_resource->forest_sq_nutrient;
	int forestMineral = tx_resource->forest_sq_mineral;
	int forestEnergy = tx_resource->forest_sq_energy;

	if (has_facility_tech(factionId, FAC_TREE_FARM))
	{
		forestNutrient++;
	}

	if (has_facility_tech(factionId, FAC_HYBRID_FOREST))
	{
		forestNutrient++;
		forestNutrient++;
	}

	double forestYieldScore = calculateYieldWeightedScore(0, 0, forestNutrient, forestMineral, forestEnergy);

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

	double fungusYieldScore = calculateYieldWeightedScore(0, 0, fungusNutrient, fungusMineral, fungusEnergy);

	// pick the best yield score

	double alternativeYieldScore = max(forestYieldScore, fungusYieldScore);

	// calculate extra yield with technology

	double extraNutrient = (has_terra(factionId, FORMER_SOIL_ENR, false) ? 2.0 : 1.0);
	double extraEnergy = (has_terra(factionId, FORMER_ECH_MIRROR, false) ? 1.0 : 0.0);

	// estimate rainfall improvement

	// [0.0, 9.0]
	double totalRainfallImprovement = 0.0;

	for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
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

		// exclude not worked tiles

		if (workedTileBases.count(tile) == 0)
			continue;

		// exclude great dunes - they don't get more wet

		if (map_has_landmark(tile, LM_DUNES))
			continue;

		// exclude monotith - it is not affected by rainfall

		if (map_has_item(tile, TERRA_MONOLITH))
			continue;

		// exclude current rocky mines - they probably won't be leveled soon

		if (map_rockiness(tile) == 2 && map_has_item(tile, TERRA_MINE))
			continue;

		// estimate rainfall improved by condenser

		double rainfallImprovement = 0.0;

		if (map_has_item(improvedTile, TERRA_CONDENSER))
		{
			// calculate yield score

			double nutrient = (double)map_rainfall(tile) + extraNutrient;
			double mineral = (double)min(1, map_rockiness(tile));
			double energy = (double)(1 + map_elevation(tile)) + extraEnergy;

			double yieldScore = calculateYieldWeightedScore(0.0, 0.0, nutrient, mineral, energy);

			// add to improvement if it no less than forest one

			if (yieldScore > alternativeYieldScore)
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
		else
		{
			// calculate yield score with improved rainfall

			double nutrient = (double)max(2, map_rainfall(tile) + 1) + extraNutrient;
			double mineral = (double)min(1, map_rockiness(tile));
			double energy = (double)(1 + map_elevation(tile)) + extraEnergy;

			double yieldScore = calculateYieldWeightedScore(0, 0, nutrient, mineral, energy);

			// add to improvement if it no less than forest one

			if (yieldScore > alternativeYieldScore)
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

		// update totalRainfallImprovement

		totalRainfallImprovement += rainfallImprovement;

	}

	debug("\t\ttotalRainfallImprovement=%4.1f\n", totalRainfallImprovement);

	// calculate good placement
	// add a mismatch point for rainfall level below max for each side tile using rainfall

	// [0.0, 24.0]
	double borderMismatch = 0.0;

	for (int dx = -4; dx <= +4; dx++)
	{
		for (int dy = -(4 - abs(dx)); dy < +(4 - abs(dx)); dy += 2)
		{
			// only outer rim tiles

			if (abs(dx) + abs(dy) != 4)
				continue;

			// exclude corners

			if (abs(dx) == 4 || abs(dy) == 4)
				continue;

			// build tile

			MAP *tile = getMapTile(x + dx, y + dy);
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

			// exclude current rocky tiles

			if (map_rockiness(tile) == 2)
				continue;

			// add to borderMismatch

			borderMismatch += (double)(2 - map_rainfall(tile));

		}

	}

	debug("\t\tborderMismatch=%4.1f\n", borderMismatch);

	// calculate bonus

	return conf.ai_terraforming_nutrientWeight * totalRainfallImprovement - borderMismatch / 6.0;

}

/**
Estimates mirror extra energy yield on existing collectors.
*/
double estimateEchelonMirrorExtraYieldScore(MAP_INFO *mapInfo)
{
	int x = mapInfo->x;
	int y = mapInfo->y;

	double totalEnergyImprovement = 0.0;

	// iterate adjacent tiles

	for (int offsetIndex = 1; offsetIndex < ADJACENT_TILE_OFFSET_COUNT; offsetIndex++)
	{
		MAP *tile = getMapTile(x + BASE_TILE_OFFSETS[offsetIndex][0], y + BASE_TILE_OFFSETS[offsetIndex][1]);

		if (!tile)
			continue;

		// exclude ocean

		if (is_ocean(tile))
			continue;

		// exclude not worked tiles

		if (workedTileBases.count(tile) == 0)
			continue;

		// estimate tile energy improvement

		double energyImprovement = 0.0;

		if (map_has_item(tile, TERRA_SOLAR))
		{
			energyImprovement += 1.0;
		}

		// update total

		totalEnergyImprovement += energyImprovement;

    }

    return conf.ai_terraforming_energyWeight * totalEnergyImprovement;

}

/**
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

	// evaluate placement

	// [0, 16]
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

			// exclude ocean

			if (is_ocean(tile))
				continue;

			// count rivers

			if (map_has_item(tile, TERRA_RIVER))
			{
				borderRiverCount++;
			}

		}

    }

    // return weighted totalEnergyImprovement

    return elevationFactor + (double)borderRiverCount;

}

/**
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

				int movementCost = hex_cost(vehicle->proto_id, factionId, element->x, element->y, adjacentTileX, adjacentTileY, 0);

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

double calculateYieldWeightedScore(double nutrientDemand, double mineralDemand, double nutrient, double mineral, double energy)
{
	return
		(conf.ai_terraforming_nutrientWeight + nutrientDemand) * nutrient
		+
		(conf.ai_terraforming_mineralWeight + mineralDemand) * mineral
		+
		conf.ai_terraforming_energyWeight * energy
	;

}

