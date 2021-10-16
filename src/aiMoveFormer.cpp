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
std::unordered_set<MAP *> terraformedSensorTiles;
std::vector<FORMER_ORDER> formerOrders;
std::map<int, FORMER_ORDER *> vehicleFormerOrders;
std::vector<MAP_INFO> territoryTiles;
std::map<MAP *, MAP_INFO> workableTiles;
std::map<MAP *, MAP_INFO> availableTiles;
std::vector<TERRAFORMING_REQUEST> terraformingRequests;
std::unordered_set<MAP *> targetedTiles;
std::unordered_set<MAP *> connectionNetworkLocations;
std::unordered_map<MAP *, std::unordered_map<int, std::vector<BASE_INFO>>> nearbyBaseSets;
std::unordered_map<int, int> seaRegionMappings;
std::unordered_map<MAP *, int> coastalBaseRegionMappings;
std::unordered_set<MAP *> blockedLocations;
std::unordered_set<MAP *> warzoneLocations;
std::unordered_set<MAP *> zocLocations;
std::unordered_map<BASE *, std::vector<YIELD>> unworkedTileYields;
std::unordered_map<MAP *, std::unordered_set<int>> workableTileBases;
std::unordered_map<int, BaseTerraformingInfo> baseTerraformingInfos;

/*
Prepares former orders.
*/
void moveFormerStrategy()
{
	// populate processing lists

	populateLists();
	
	// formers
	
	moveLandFormerStrategy();
	moveSeaFormerStrategy();

	cancelRedundantOrders();

	generateTerraformingRequests();

//	// sort terraforming requests
//
//	sortBaseTerraformingRequests();
//
	sortTerraformingRequests();
	
	applyProximityRules();

	assignFormerOrders();

//	// optimize former destinations
//
//	optimizeFormerDestinations();
//
	finalizeFormerOrders();

}

/*
Populate global lists before processing faction strategy.
*/
void populateLists()
{
	Faction *aiFaction = &(Factions[aiFactionId]);

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
	terraformedSensorTiles.clear();
	territoryTiles.clear();
	workableTiles.clear();
	availableTiles.clear();
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
	unworkedTileYields.clear();
	workableTileBases.clear();
	baseTerraformingInfos.clear();

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
			for (MAP *adjacentTile : getAdjacentTiles(vehicle->x, vehicle->y, false))
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

	for (int id : activeFactionInfo.baseIds)
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

		// populate worked tiles

		for (MAP *workedTile : getBaseWorkedTiles(id))
		{
			workedTileBases[workedTile] = {id, base};

		}

	}

	// populate sea region mappings for coastal bases connecting sea regions

//	debug("seaRegionLinkedGroups\n")

	for (int id : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[id]);

		MAP *baseTile = getMapTile(base->x, base->y);

		if (!is_ocean(baseTile))
		{
			std::set<int> seaRegionLinkedGroup;

			for (MAP *adjacentTile : getAdjacentTiles(base->x, base->y, false))
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

	for (int id : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[id]);

		MAP *baseTile = getMapTile(base->x, base->y);

		if (!is_ocean(baseTile))
		{
			for (MAP *adjacentTile : getAdjacentTiles(base->x, base->y, false))
			{
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

			territoryTiles.push_back({x, y, tile});

		}

	}

	// populate availalbe tiles

	for (int baseId : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[baseId]);

		// crete base terraforming info
		
		baseTerraformingInfos[baseId] = {};
		baseTerraformingInfos[baseId].unimprovedWorkedTileCount = getUnimprovedWorkedTileCount(baseId);
		baseTerraformingInfos[baseId].ongoingYieldTerraformingCount = getOngoingYieldTerraformingCount(baseId);

		// count formers currently improving base yield
		
		int ongoingYieldTerraformingCount = 0;

		// iterate base tiles

		for (MAP_INFO mapInfo : getBaseRadiusTileInfos(base->x, base->y, false))
		{
			int x = mapInfo.x;
			int y = mapInfo.y;
			MAP *tile = mapInfo.tile;

			// own or unclaimed territory

			if (!(tile->owner == -1 || tile->owner == aiFactionId))
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

			// store workable tile to bases reference
			
			workableTileBases[tile].insert(baseId);

			// exclude harvested tiles

			if (harvestedTiles.count(tile) != 0)
				continue;

			// exclude terraformed tiles

			if (terraformedTiles.count(tile) != 0)
				continue;

			// store available tile

			availableTiles[tile] = {x, y, tile};
			
		}
		
		// store ongoing terraforming count
		
		baseTerraformingInfos[baseId].ongoingYieldTerraformingCount = ongoingYieldTerraformingCount;

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

			debug("\t(%3d,%3d)\n", mapInfo->x, mapInfo->y);

		}

	}
	
	// populate base rocky tile count
	
	for (const auto &k : workableTileBases)
	{
		MAP *tile = k.first;
		const std::unordered_set<int> *baseIds = &(k.second);
		
		if (isLandRockyTile(tile))
		{
			for (int baseId : *baseIds)
			{
				baseTerraformingInfos[baseId].landRockyTiles.push_back(tile);
			}
			
		}
		
	}

	// populate unworked tiles

	for (int id : activeFactionInfo.baseIds)
	{
		BASE *base = &(Bases[id]);

		unworkedTileYields[base] = std::vector<YIELD>();
		std::vector<YIELD> *baseunworkedTileYields = &(unworkedTileYields[base]);

		// set base
		// that is needed because below functions require it and they are usually called in context where base is set already

		set_base(id);

		for (MAP_INFO mapInfo : getBaseRadiusTileInfos(base->x, base->y, false))
		{
			int x = mapInfo.x;
			int y = mapInfo.y;
			MAP *tile = mapInfo.tile;

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

	double networkThreshold = conf.ai_terraforming_networkCoverageThreshold * (double)activeFactionInfo.baseIds.size();

	if (networkCoverage < networkThreshold)
	{
		networkDemand = (networkThreshold - networkCoverage) / networkThreshold;
	}

	fflush(debug_log);

}

void moveLandFormerStrategy()
{
	// 
}

void moveSeaFormerStrategy()
{
	//
}

/*
Checks and removes redundant orders.
*/
void cancelRedundantOrders()
{
	for (int id : activeFactionInfo.formerVehicleIds)
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

	// sensors

	debug("SENSOR TERRAFORMING_REQUESTS\n");

	for (MAP_INFO mapInfo : territoryTiles)
	{
		// generate request

		generateSensorTerraformingRequest(mapInfo);

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

	// generate free terraforming request

	terraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score});

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
		terraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score});
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
		terraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score});
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
		terraformingRequests.push_back({mapInfo->x, mapInfo->y, mapInfo->tile, terraformingScore->option, terraformingScore->action, terraformingScore->score});
	}

}

/*
Generate request for Sensor (road/tube).
*/
void generateSensorTerraformingRequest(MAP_INFO mapInfo)
{
	TERRAFORMING_SCORE terraformingScoreObject;
	TERRAFORMING_SCORE *terraformingScore = &terraformingScoreObject;
	calculateSensorTerraformingScore(mapInfo, terraformingScore);

	if (terraformingScore->action != -1 && terraformingScore->score > 0.0)
	{
		terraformingRequests.push_back({mapInfo.x, mapInfo.y, mapInfo.tile, terraformingScore->option, terraformingScore->action, terraformingScore->score});
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
			debug("\t(%3d,%3d) %-20s %6.3f\n", terraformingRequest.x, terraformingRequest.y, terraformingRequest.option->name, terraformingRequest.score);
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
					terraformingRequest->x,
					terraformingRequest->y,
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
		int x = terraformingRequest.x;
		int y = terraformingRequest.y;
		MAP *tile = terraformingRequest.tile;
		bool ocean = isOceanRegion(tile->region);
		
		// find nearest available former
		
		FORMER_ORDER *nearestFormerOrder = nullptr;
		int minRange = INT_MAX;
		
		for (FORMER_ORDER &formerOrder : formerOrders)
		{
			int vehicleId = formerOrder.id;
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// skip assigned
			
			if (formerOrder.action != -1)
				continue;
			
			// reachable
			
			if (!ocean)
			{
				// no sea former for land tile
				
				if (vehicle->triad() == TRIAD_SEA)
					continue;
					
			}
			else
			{
				// no land former for ocean tile
				
				if (vehicle->triad() == TRIAD_LAND)
					continue;
				
				// same ocean region
				
				if (vehicleTile->region != tile->region)
					continue;
				
			}
			
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
		
		nearestFormerOrder->x = terraformingRequest.x;
		nearestFormerOrder->y = terraformingRequest.y;
		nearestFormerOrder->action = terraformingRequest.action;
		
	}
	
	// kill formers without orders
	
	for (FORMER_ORDER &formerOrder : formerOrders)
	{
		int vehicleId = formerOrder.id;
		
		if (formerOrder.action == -1)
		{
			setTask(vehicleId, std::shared_ptr<Task>(new KillTask(vehicleId)));
		}
		
	}

}

//void optimizeFormerDestinations()
//{
//	// iterate through regions
//
//	for
//	(
//		std::map<int, std::vector<FORMER_ORDER>>::iterator regionFormerOrdersIterator = regionFormerOrders.begin();
//		regionFormerOrdersIterator != regionFormerOrders.end();
//		regionFormerOrdersIterator++
//	)
//	{
//		std::vector<FORMER_ORDER> *formerOrders = &(regionFormerOrdersIterator->second);
//
//		// iterate until optimized or max iterations
//
//		for (int iteration = 0; iteration < 100; iteration++)
//		{
//			bool swapped = false;
//
//			// iterate through former order pairs
//
//			for
//			(
//				std::vector<FORMER_ORDER>::iterator formerOrderIterator1 = formerOrders->begin();
//				formerOrderIterator1 != formerOrders->end();
//				formerOrderIterator1++
//			)
//			{
//				for
//				(
//					std::vector<FORMER_ORDER>::iterator formerOrderIterator2 = formerOrders->begin();
//					formerOrderIterator2 != formerOrders->end();
//					formerOrderIterator2++
//				)
//				{
//					FORMER_ORDER *formerOrder1 = &(*formerOrderIterator1);
//					FORMER_ORDER *formerOrder2 = &(*formerOrderIterator2);
//
//					// skip itself
//
//					if (formerOrder1->id == formerOrder2->id)
//						continue;
//
//					VEH *vehicle1 = formerOrder1->vehicle;
//					VEH *vehicle2 = formerOrder2->vehicle;
//
//					int vehicle1Speed = veh_speed_without_roads(formerOrder1->id);
//					int vehicle2Speed = veh_speed_without_roads(formerOrder2->id);
//
//					int destination1X = formerOrder1->x;
//					int destination1Y = formerOrder1->y;
//					MAP *destination1Tile = getMapTile(destination1X, destination1Y);
//					int destination2X = formerOrder2->x;
//					int destination2Y = formerOrder2->y;
//					MAP *destination2Tile = getMapTile(destination2X, destination2Y);
//
//					int action1 = formerOrder1->action;
//					int action2 = formerOrder2->action;
//
//					// calculate current travel time
//
//					double vehicle1CurrentTravelTime = (double)map_range(vehicle1->x, vehicle1->y, destination1X, destination1Y) / (double)vehicle1Speed;
//					double vehicle2CurrentTravelTime = (double)map_range(vehicle2->x, vehicle2->y, destination2X, destination2Y) / (double)vehicle2Speed;
//
//					double currentTravelTime = vehicle1CurrentTravelTime + vehicle2CurrentTravelTime;
//
//					// calculate current terraforming time
//
//					double vehicle1CurrentTerraformingTime = calculateTerraformingTime(action1, destination1Tile->items, destination1Tile->val3, vehicle1);
//					double vehicle2CurrentTerraformingTime = calculateTerraformingTime(action2, destination2Tile->items, destination2Tile->val3, vehicle2);
//
//					double currentTerraformingTime = vehicle1CurrentTerraformingTime + vehicle2CurrentTerraformingTime;
//
//					// calculate current completion time
//
//					double currentCompletionTime = currentTravelTime + currentTerraformingTime;
//
//					// calculate swapped travel time
//
//					double vehicle1SwappedTravelTime = (double)map_range(vehicle1->x, vehicle1->y, destination2X, destination2Y) / (double)vehicle1Speed;
//					double vehicle2SwappedTravelTime = (double)map_range(vehicle2->x, vehicle2->y, destination1X, destination1Y) / (double)vehicle2Speed;
//
//					double swappedTravelTime = vehicle1SwappedTravelTime + vehicle2SwappedTravelTime;
//
//					// calculate swapped terraforming time
//
//					double vehicle1SwappedTerraformingTime = calculateTerraformingTime(action2, destination2Tile->items, destination2Tile->val3, vehicle1);
//					double vehicle2SwappedTerraformingTime = calculateTerraformingTime(action1, destination1Tile->items, destination1Tile->val3, vehicle2);
//
//					double swappedTerraformingTime = vehicle1SwappedTerraformingTime + vehicle2SwappedTerraformingTime;
//
//					// calculate swapped completion time
//
//					double swappedCompletionTime = swappedTravelTime + swappedTerraformingTime;
//
//					// swap orders if beneficial
//
//					if (swappedCompletionTime < currentCompletionTime)
//					{
//						formerOrder1->x = destination2X;
//						formerOrder1->y = destination2Y;
//						formerOrder1->action = action2;
//
//						formerOrder2->x = destination1X;
//						formerOrder2->y = destination1Y;
//						formerOrder2->action = action1;
//
//						swapped = true;
//
//					}
//
//				}
//
//			}
//
//			// stop cycle if nothing is swapped
//
//			if (!swapped)
//				break;
//
//		}
//
//	}
//
//}
//
void finalizeFormerOrders()
{
	// iterate former orders

	for (FORMER_ORDER &formerOrder : formerOrders)
	{
		transitVehicle(formerOrder.id, std::shared_ptr<Task>(new TerraformingTask(formerOrder.id, formerOrder.x, formerOrder.y, formerOrder.action)));
	}

}

/*
Selects best terraforming option around given base and calculates its terraforming score.
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
	;

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(mapInfo);

	// store tile values

	MAP_STATE currentMapStateObject;
	MAP_STATE *currentMapState = &currentMapStateObject;
	getMapState(mapInfo, currentMapState);

	// find best terraforming option

	const TERRAFORMING_OPTION *bestOption = NULL;
	int bestOptionFirstAction = -1;
	double bestOptionTerraformingScore = 0.0;

	for (size_t optionIndex = 0; optionIndex < CONVENTIONAL_TERRAFORMING_OPTIONS.size(); optionIndex++)
	{
		const TERRAFORMING_OPTION *option = &(CONVENTIONAL_TERRAFORMING_OPTIONS[optionIndex]);

		// process only correct region type

		if (option->ocean != ocean)
			continue;

		// process only correct rockiness

		if (!ocean && option->rocky && !(tile->val3 & TILE_ROCKY))
			continue;

		// check if required action technology is available when required

		if (option->requiredAction != -1 && !isTerraformingAvailable(mapInfo, option->requiredAction))
			continue;
		
		debug("\t%s\n", option->name);
		
		// initialize variables

		MAP_STATE improvedMapStateObject;
		MAP_STATE *improvedMapState = &improvedMapStateObject;
		copyMapState(improvedMapState, currentMapState);
		
		int firstAction = -1;
		int terraformingTime = 0;
		double penaltyScore = 0.0;
		double improvementScore = 0.0;

		// process actions
		
		int availableActionCount = 0;
		int incompleteActionCount = 0;
		bool levelTerrain = false;
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
			
			// increment available actions
			
			availableActionCount++;
			
			// generate terraforming change and add terraforming time for non completed action

			if (!isTerraformingCompleted(mapInfo, action))
			{
				// increment incomplete actions
				
				incompleteActionCount++;
				
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

				if (!ocean && (improvedMapState->rocks & TILE_ROCKY) && isLevelTerrainRequired(ocean, action))
				{
					int levelTerrainAction = FORMER_LEVEL_TERRAIN;
					
					levelTerrain = true;

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

		double surplusEffectScore = computeImprovementSurplusEffectScore(mapInfo, currentMapState, improvedMapState);
		
		// ignore options not increasing yield

		if (surplusEffectScore <= 0.0)
			continue;
		
		// special yield computation for area effects

		double condenserExtraYieldScore = estimateCondenserExtraYieldScore(mapInfo, &(option->actions));

		// calculate exclusivity bonus

		double exclusivityBonus = calculateExclusivityBonus(mapInfo, &(option->actions), levelTerrain);
		
		// summarize score

		improvementScore = penaltyScore + surplusEffectScore + condenserExtraYieldScore + exclusivityBonus;
		
		// apply rank penalty
		
		int maxExtraTerraformingCount = 0;
		
		for (int baseId : workableTileBases[tile])
		{
			int extraTerraformingCount = std::max(0, baseTerraformingInfos[baseId].ongoingYieldTerraformingCount - baseTerraformingInfos[baseId].unimprovedWorkedTileCount);
			maxExtraTerraformingCount = std::max(maxExtraTerraformingCount, extraTerraformingCount);
		}
		
		improvementScore *= pow(conf.ai_terraforming_rank_multiplier, (double)maxExtraTerraformingCount);
		
		// apply resource bonus
		
		if (bonus_at(x, y) != 0)
		{
			improvementScore += 2.0;
		}

		// calculate terraforming score
		// every 15 turns reduce score in half

		double terraformingScore = improvementScore * pow(0.5, ((double)terraformingTime + conf.ai_terraforming_travel_time_multiplier * travelTime) / conf.ai_terraforming_time_scale);

		debug
		(
			"\t\t%-20s=%6.3f : penalty=%6.3f, surplusEffect=%6.3f, condenser=%6.3f, exclusivity=%6.3f, maxExtraTerraformingCount=%d, combined=%6.3f, time=%2d, travel=%f\n",
			"terraformingScore",
			terraformingScore,
			penaltyScore,
			surplusEffectScore,
			condenserExtraYieldScore,
			exclusivityBonus,
			maxExtraTerraformingCount,
			improvementScore,
			terraformingTime,
			travelTime
		)
		;

		// update score

		if (bestOption == NULL || terraformingScore > bestOptionTerraformingScore)
		{
			bestOption = option;
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
void calculateAquiferTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	bool ocean = is_ocean(mapInfo->tile);

	// land only

	if (ocean)
		return;

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(mapInfo);

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

	terraformingTime = calculateTerraformingTime(action, tile->items, tile->val3, NULL);

	// skip if no action or no terraforming time

	if (firstAction == -1 || terraformingTime == 0)
		return;

	// special yield computation for aquifer

	improvementScore += estimateAquiferExtraYieldScore(mapInfo);

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

	int cost = terraform_cost(x, y, aiFactionId);
	if (cost > Factions[aiFactionId].energy_credits / 10)
		return;

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(mapInfo);

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

	terraformingTime = calculateTerraformingTime(action, tile->items, tile->val3, NULL);

	// skip if no action or no terraforming time

	if (firstAction == -1 || terraformingTime == 0)
		return;

	// special yield computation for raise land

	improvementScore += estimateRaiseLandExtraYieldScore(mapInfo, cost);

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
void calculateNetworkTerraformingScore(MAP_INFO *mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	bool ocean = is_ocean(mapInfo->tile);

	// land only

	if (ocean)
		return;

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(mapInfo);

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

	improvementScore += (1.0 + networkDemand) * calculateNetworkScore(mapInfo, action);

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
void calculateSensorTerraformingScore(MAP_INFO mapInfo, TERRAFORMING_SCORE *bestTerraformingScore)
{
	int x = mapInfo.x;
	int y = mapInfo.y;
	MAP *tile = mapInfo.tile;
	bool ocean = is_ocean(tile);

	// calculate travel time

	double travelTime = getSmallestAvailableFormerTravelTime(&mapInfo);

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

	if (!isTerraformingAvailable(&mapInfo, action))
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

	double sensorScore = calculateSensorScore(mapInfo, action);

	// calculate exclusivity bonus

	double exclusivityBonus = calculateExclusivityBonus(&mapInfo, &(option->actions), false) / 4.0;

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

	// ignore vehicles without former orders

	if (vehicleFormerOrders.count(aiVehicleId) == 0)
		return mod_enemy_move(vehicleId);

	// get former order

	FORMER_ORDER *formerOrder = vehicleFormerOrders[aiVehicleId];

	// ignore orders without action

	if (formerOrder->action == -1)
		return mod_enemy_move(vehicleId);

//	// execute order
//
//	setFormerOrder(vehicleId, formerOrder);
//
	return SYNC;

}

//void setFormerOrder(int vehicleId, FORMER_ORDER *formerOrder)
//{
//	VEH *vehicle = &(Vehicles[vehicleId]);
//	int triad = veh_triad(vehicleId);
//	int x = formerOrder->x;
//	int y = formerOrder->y;
//	int action = formerOrder->action;
//
//	// at destination
//	if (vehicle->x == x && vehicle->y == y)
//	{
//		// execute terraforming action
//
//		setTerraformingAction(vehicleId, action);
//
//		debug
//		(
//			"generateFormerOrder: location=(%3d,%3d), action=%s\n",
//			vehicle->x,
//			vehicle->y,
//			getTerraformingActionName(action)
//		)
//		;
//
//	}
//	// not at destination
//	else
//	{
//		// send former to destination
//
//		if (triad == TRIAD_LAND)
//		{
//			if (!sendVehicleToDestination(vehicleId, x, y))
//			{
//				debug("location inaccessible: [%d] (%3d,%3d)\n", vehicleId, x, y);
//
//				// get vehicle region
//
//				MAP *location = getMapTile(vehicle->x, vehicle->y);
//				int region = location->region;
//
//				std::vector<TERRAFORMING_REQUEST> *regionRankedTerraformingRequests = &(regionTerraformingRequests[region]);
//
//				bool success = false;
//
//				for
//				(
//					std::vector<TERRAFORMING_REQUEST>::iterator regionRankedTerraformingRequestsIterator = regionRankedTerraformingRequests->begin();
//					regionRankedTerraformingRequestsIterator != regionRankedTerraformingRequests->end();
//				)
//				{
//					TERRAFORMING_REQUEST *terraformingRequest = &(*regionRankedTerraformingRequestsIterator);
//
//					// try new destination
//
//					debug("send vehicle to another destination: [%d] (%3d,%3d)\n", vehicleId, terraformingRequest->x, terraformingRequest->y);
//
//					success = sendVehicleToDestination(vehicleId, terraformingRequest->x, terraformingRequest->y);
//
//					// erase request
//
//					regionRankedTerraformingRequestsIterator = regionRankedTerraformingRequests->erase(regionRankedTerraformingRequestsIterator);
//
//					// quit the cycle if successful
//
//					if (success)
//						break;
//
//				}
//
//				// skip turn if unsuccessful
//
//				if (!success)
//				{
//					veh_skip(vehicleId);
//				}
//
//			}
//
//		}
//		else
//		{
//			sendVehicleToDestination(vehicleId, x, y);
//		}
//
//		debug
//		(
//			"generateFormerOrder: location=(%3d,%3d), move_to(%3d,%3d)\n",
//			vehicle->x,
//			vehicle->y,
//			vehicle->waypoint_1_x,
//			vehicle->waypoint_1_y
//		)
//
//	}
//
//}
//
/*
Computes all sharing bases combined surplus effect.
*/
double computeImprovementSurplusEffectScore(MAP_INFO *mapInfo, MAP_STATE *currentMapState, MAP_STATE *improvedMapState)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;

	// calculate affected range
	// building or destroying mirror requires recomputation of all bases in range 1

	int affectedRange = 0;

	if
	(
		((currentMapState->items & TERRA_ECH_MIRROR) != 0 && (improvedMapState->items & TERRA_ECH_MIRROR) == 0)
		||
		((currentMapState->items & TERRA_ECH_MIRROR) == 0 && (improvedMapState->items & TERRA_ECH_MIRROR) != 0)
	)
	{
		affectedRange = 1;
	}
	
	// collect affected tiles
	
	std::vector<MAP *> affectedTiles = (affectedRange == 1 ? getAdjacentTiles(x, y, true) : std::vector<MAP *>{tile});
	
	// collect affected bases
	
	std::unordered_set<int> affectedBases;
	
	for (MAP *affectedTile : affectedTiles)
	{
		affectedBases.insert(workableTileBases[affectedTile].begin(), workableTileBases[affectedTile].end());
	}
	
	// compute base scores
	
	double bestScore = 0.0;
	
	for (int baseId : affectedBases)
	{
		// compute base score

		double score = computeImprovementBaseSurplusEffectScore(baseId, mapInfo, currentMapState, improvedMapState);
		
		// update total
		
		bestScore = std::max(bestScore, score);
		
	}
	
	return bestScore;

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
bool isTerraformingCompleted(MAP_INFO *mapInfo, int action)
{
	MAP *tile = mapInfo->tile;

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
bool isTerraformingAvailable(MAP_INFO *mapInfo, int action)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
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

int getBaseTerraformingRank(BASE *base)
{
	return (baseTerraformingRanks.count(base) == 0 ? 0 : baseTerraformingRanks[base]);
}

BASE *findAffectedBase(int x, int y)
{
	for (int id : activeFactionInfo.baseIds)
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

double getSmallestAvailableFormerTravelTime(MAP_INFO *mapInfo)
{
	int x = mapInfo->x;
	int y = mapInfo->y;
	MAP *tile = mapInfo->tile;
	int region = tile->region;
	
	double smallestTravelTime = 1000;
	
	if (isLandRegion(region))
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
			
			if (vehicleTile->region != region)
			{
				time *= 2;
			}
			
			// update the best

			smallestTravelTime = std::min(smallestTravelTime, time);

		}

	}
	else
	{
		// iterate sea former in this regions
		
		for (FORMER_ORDER formerOrder : formerOrders)
		{
			int vehicleId = formerOrder.id;
			VEH *vehicle = &(Vehicles[vehicleId]);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// sea former
			
			if (vehicle->triad() != TRIAD_SEA)
				continue;
			
			// this region
			
			if (vehicleTile->region != region)
				continue;
			
			// calculate time
			
			int range = map_range(vehicle->x, vehicle->y, x, y);
			int speed = veh_chassis_speed(vehicleId);
			
			double time = (double)range / (double)speed;

			// update the best

			smallestTravelTime = std::min(smallestTravelTime, time);

		}

	}
	
	return smallestTravelTime;

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

			if (activeFactionInfo.baseLocations.count(tile) != 0)
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

			if (activeFactionInfo.baseLocations.count(tile) != 0)
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

			if (activeFactionInfo.baseLocations.count(tile) != 0)
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
double calculateSensorScore(MAP_INFO mapInfo, int action)
{
	int x = mapInfo.x;
	int y = mapInfo.y;
	MAP *tile = mapInfo.tile;

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
	
	for (int baseId : activeFactionInfo.baseIds)
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

double calculateExclusivityBonus(MAP_INFO *mapInfo, const std::vector<int> *actions, bool levelTerrain)
{
	MAP *tile = mapInfo->tile;

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
				
				for (int baseId : workableTileBases[tile])
				{
					minBaseLandRockyTileCount = std::min(minBaseLandRockyTileCount, (int)baseTerraformingInfos[baseId].landRockyTiles.size());
					
				}
				
				debug("(%3d,%3d) minBaseLandRockyTileCount=%d\n", mapInfo->x, mapInfo->y, minBaseLandRockyTileCount);
				
				if (minBaseLandRockyTileCount < conf.ai_terraforming_land_rocky_tile_threshold)
				{
					mineral += minBaseLandRockyTileCount - conf.ai_terraforming_land_rocky_tile_threshold;
				}
				
			}

			// leveling rocky tile is penalizable if there are other rocky tiles at higher elevation

			if (actionSet.count(FORMER_LEVEL_TERRAIN) != 0 && rockiness == 2)
			{
				int minBaseHigherElevationLandRockyTileCount = INT_MAX;
				
				for (int baseId : workableTileBases[tile])
				{
					// get base higher elevation rocky tile count
					
					int baseHigherElevationLandRockyTileCount = 0;
					
					for (MAP *otherTile : baseTerraformingInfos[baseId].landRockyTiles)
					{
						// skip self
						
						if (otherTile == tile)
							continue;
						
						// get other tile elevation
						
						int otherTileElevation = map_elevation(otherTile);
						
						// update total
						
						if (otherTileElevation > elevation)
						{
							baseHigherElevationLandRockyTileCount += otherTileElevation - elevation;
						}
						
					}
					
					// update total across bases
					
					minBaseHigherElevationLandRockyTileCount = std::min(minBaseHigherElevationLandRockyTileCount, baseHigherElevationLandRockyTileCount);
					
				}
				
				debug("(%3d,%3d) minBaseHigherElevationLandRockyTileCount=%d\n", mapInfo->x, mapInfo->y, minBaseHigherElevationLandRockyTileCount);
				
				mineral += -minBaseHigherElevationLandRockyTileCount;
				
			}

			// not building mine on mineral bonus wastes 1 mineral
			
			if (actionSet.count(FORMER_MINE) == 0 && isMineBonus(mapInfo->x, mapInfo->y))
			{
				mineral += -1;
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

	for (MAP *tile : getAdjacentTiles(x, y, true))
	{
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
	VEH *vehicle = &(Vehicles[id]);
	MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);
	int vehicleTriad = veh_triad(id);
	int vehicleSpeed = mod_veh_speed(id);

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

			for (MAP_INFO adjacentTileInfo : getAdjacentTileInfos(element->x, element->y, false))
			{
				int adjacentTileX = adjacentTileInfo.x;
				int adjacentTileY = adjacentTileInfo.y;
				MAP *adjacentTile = adjacentTileInfo.tile;

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

				if (activeFactionInfo.baseLocations.count(element->tile) == 0 && activeFactionInfo.baseLocations.count(adjacentTile) == 0 && element->zoc && adjacentTileZOC)
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
double computeImprovementBaseSurplusEffectScore(int baseId, MAP_INFO *mapInfo, MAP_STATE *currentMapState, MAP_STATE *improvedMapState)
{
	BASE *base = &(Bases[baseId]);

	// set initial state
	
	setMapState(mapInfo, currentMapState);
	computeBase(baseId);

	// gather current surplus

	int currentNutrientSurplus = base->nutrient_surplus;
	int currentMineralSurplus = base->mineral_surplus;
	int currentEnergySurplus = base->economy_total + base->psych_total + base->labs_total;

	// apply changes

	setMapState(mapInfo, improvedMapState);
	computeBase(baseId);

	// gather improved surplus

	int improvedNutrientSurplus = base->nutrient_surplus;
	int improvedMineralSurplus = base->mineral_surplus;
	int improvedEnergySurplus = base->economy_total + base->psych_total + base->labs_total;

	// restore original state
	
	setMapState(mapInfo, currentMapState);
	computeBase(baseId);

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

bool isInferiorImprovedTile(BASE_INFO *baseInfo, MAP_INFO *mapInfo, MAP_STATE *currentMapState, MAP_STATE *improvedMapState)
{
	int id = baseInfo->id;
	BASE *base = baseInfo->base;
	int x = mapInfo->x;
	int y = mapInfo->y;

	// apply changes

	setMapState(mapInfo, improvedMapState);

	// set base
	// that is needed because below functions require it and they are usually called in context where base is set already

	set_base(id);

	// compute tile yield

	int nutrient = mod_nutrient_yield(aiFactionId, id, x, y, 0);
	int mineral = mod_mineral_yield(aiFactionId, id, x, y, 0);
	int energy = mod_energy_yield(aiFactionId, id, x, y, 0);

	// revert changes

	setMapState(mapInfo, currentMapState);

	// check inferior yield

	return isInferiorYield(&(unworkedTileYields[base]), nutrient, mineral, energy, NULL);

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

	for (int vehicleId : activeFactionInfo.vehicleIds)
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

