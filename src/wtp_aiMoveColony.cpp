
#include <algorithm>

#include "wtp_aiMoveColony.h"

#include "wtp_ai_game.h"
#include "wtp_aiRoute.h"
#include "wtp_aiMove.h"
#include "wtp_aiMoveFormer.h"

std::vector<std::array<int, 2>> a1 = {{1, 2}};

// YieldScore comparator

bool compareYieldInfoByScoreAndResourceScore(const YieldInfo &a, const YieldInfo &b)
{
    return a.score > b.score || (a.score == b.score && a.resourceScore > b.resourceScore);
}

// expansion data

std::vector<TileExpansionInfo> tileExpansionInfos;
std::vector<MAP *> buildSites;
std::vector<MAP *> availableBuildSites;
robin_hood::unordered_flat_map<MAP *, std::vector<MAP *>> buildSiteWorkTiles;
robin_hood::unordered_flat_map<MAP *, double> internalConcaveNodes;
robin_hood::unordered_flat_map<MAP *, double> externalConcaveNodes;

void moveColonyStrategy()
{
	Profiling::start("moveColonyStrategy", "moveStrategy");
	
	// setup data

	populateExpansionData();

	// analyze base placement sites

	analyzeBasePlacementSites();

	Profiling::stop("moveColonyStrategy");
	
}

void populateExpansionData()
{
	// reset
	
	tileExpansionInfos.clear();
	tileExpansionInfos.resize(*MapAreaTiles, {});
	
	buildSites.clear();
	availableBuildSites.clear();
	
	// concave tiles
	
	Profiling::start("populateConcaveTiles", "moveColonyStrategy");
	populateConcaveTiles();
	Profiling::stop("populateConcaveTiles");
	
	// worked tiles
	
	for (int baseId : aiData.baseIds)
	{
		for (MAP *baseRadisuTile : getBaseWorkedTiles(baseId))
		{
			TileExpansionInfo &baseRadisuTileExpansionInfo = getTileExpansionInfo(baseRadisuTile);
			baseRadisuTileExpansionInfo.worked = true;
		}
		
	}
	
	// work tiles
	
	Profiling::start("valid locations", "moveColonyStrategy");
	
	buildSiteWorkTiles.clear();
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		TileExpansionInfo &tileExpansionInfo = getTileExpansionInfo(tile);
		
		// within expansion range
		
		if (getExpansionRange(tile) > MAX_EXPANSION_RANGE)
			continue;
		
		// set expansionRange
		
		tileExpansionInfo.expansionRange = true;
		
		// allowed base location

		if (!isAllowedBaseLocation(aiFactionId, tile))
			continue;
		
		// valid build site
		
		if (!isValidBuildSite(tile, aiFactionId))
			continue;
		
		// set valid build site
		
		tileExpansionInfo.validBuildSite = true;
		buildSiteWorkTiles.emplace(tile, std::vector<MAP *>());
		
		// iterate work tiles
		
		for (MAP *workTile : getBaseRadiusTiles(tile, false))
		{
			TileExpansionInfo &workTileExpansionInfo = getTileExpansionInfo(workTile);
			
			// valid work tile
			
			if (!isValidWorkTile(tile, workTile))
				continue;
			
			// set valid work tile
			
			workTileExpansionInfo.validWorkTile = true;
			buildSiteWorkTiles.at(tile).push_back(workTile);
			
		}
		
	}
	
//	if (DEBUG)
//	{
//		debug("validBuildSite\n");
//		
//		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
//		{
//			TileExpansionInfo const &tileExpansionInfo = getTileExpansionInfo(tile);
//			
//			if (tileExpansionInfo.validBuildSite)
//			{
//				debug("\t%s\n", getLocationString(tile));
//			}
//			
//		}
//		
//	}
	
//	if (DEBUG)
//	{
//		debug("validWorkTiles\n");
//		
//		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
//		{
//			TileExpansionInfo const &tileExpansionInfo = getTileExpansionInfo(tile);
//			
//			if (tileExpansionInfo.validWorkTile)
//			{
//				debug("\t%s\n", getLocationString(tile));
//			}
//			
//		}
//		
//	}
	
	Profiling::stop("valid locations");
	
}

TileExpansionInfo &getTileExpansionInfo(MAP *tile)
{
	assert(isOnMap(tile));
	return tileExpansionInfos.at(tile - *MapTiles);
}

void analyzeBasePlacementSites()
{
	std::vector<int> activeColonyVehicleIds;
	
	debug("analyzeBasePlacementSites - %s\n", MFactions[aiFactionId].noun_faction);
	
	// populate tile yields
	
	Profiling::start("tile yield scores", "moveColonyStrategy");
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		TileExpansionInfo &tileExpansionInfo = getTileExpansionInfo(tile);
		
		// valid work tile
		
		if (!tileExpansionInfo.validWorkTile)
			continue;
		
		// get average yield
		
		tileExpansionInfo.averageYield = getAverageTileYield(tile);
		
	}
	
	Profiling::stop("tile yield scores");
	
	// populate buildSites
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		TileExpansionInfo &tileExpansionInfo = getTileExpansionInfo(tile);
		
		// valid build site
		
		if (!tileExpansionInfo.validBuildSite)
			continue;
		
		// add to buildSites
		
		buildSites.push_back(tile);
		
		// base gain
		
		tileExpansionInfo.buildSiteBaseGain = getBuildSiteBaseGain(tile);
		
		// placement score
		
		tileExpansionInfo.buildSitePlacementScore = getBuildSitePlacementScore(tile);
		
		// score
		
		tileExpansionInfo.buildSiteScore = tileExpansionInfo.buildSiteBaseGain * (1.0 + tileExpansionInfo.buildSitePlacementScore);
		
	}
	
//	if (DEBUG)
//	{
//		debug("buildSites - %s\n", MFactions[aiFactionId].noun_faction);
//		
//		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
//		{
//			TileExpansionInfo &tileExpansionInfo = getTileExpansionInfo(tile);
//			
//			// valid build site
//			
//			if (!tileExpansionInfo.validBuildSite)
//				continue;
//			
//			debug
//			(
//				"\t%s"
//				" buildSiteBaseGain=%7.2f"
//				" buildSitePlacementScore=%7.2f"
//				" buildSiteScore=%7.2f"
//				"\n"
//				, getLocationString(tile)
//				, tileExpansionInfo.buildSiteBaseGain
//				, tileExpansionInfo.buildSitePlacementScore
//				, tileExpansionInfo.buildSiteScore
//			);
//			
//		}
//		
//	}
	
	// move colonies out of warzone
	
	debug("move colonies out of warzone - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int vehicleId : aiData.colonyVehicleIds)
	{
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		TileInfo const &vehicleTileInfo = aiData.getVehicleTileInfo(vehicleId);
		
		// warzone
		
		if (vehicleTileInfo.hostileDangerZone)
		{
			// safe location
			
			MAP *safeLocation = getSafeLocation(vehicleId, false);
			transitVehicle(Task(vehicleId, TT_MOVE, safeLocation));
			
			debug("\t%s -> %s\n", getLocationString(vehicleTile), getLocationString(safeLocation));
			
		}
		else
		{
			// active colony available for placement
			
			activeColonyVehicleIds.push_back(vehicleId);
			
		}
		
	}
		
    // distribute colonies to build sites
	
	Profiling::start("distribute colonies", "moveColonyStrategy");
	
	debug("distribute colonies to build sites - %s\n", MFactions[aiFactionId].noun_faction);
	
	robin_hood::unordered_flat_set<MAP *> &unavailableBuildSites = aiData.production.unavailableBuildSites;
	unavailableBuildSites.clear();
	
	std::vector<int> vehicleIds;
	std::vector<MAP *> destinations;
	std::vector<int> vehicleDestinations;
	
	for (int vehicleId : activeColonyVehicleIds)
	{
		VEH &vehicle = Vehs[vehicleId];
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle.triad();
		
		debug("\t%s %-32s\n", getLocationString({vehicle.x, vehicle.y}), Units[vehicle.unit_id].name);
		
		// travel time multiplier increases in vicinity of enemy colonies
		
		int enemyColonyRange = INT_MAX;
		for (int enemyVehicleId = 0; enemyVehicleId < *VehCount; enemyVehicleId++)
		{
			VEH &enemyVehicle = Vehs[enemyVehicleId];
			int enemyVehicleTriad = enemyVehicle.triad();
			
			// other faction
			
			if (enemyVehicle.faction_id == aiFactionId)
				continue;
			
			// colony
			
			if (!isColonyVehicle(enemyVehicleId))
				continue;
			
			// targets same realm
			
			if ((triad == TRIAD_SEA && enemyVehicleTriad == TRIAD_LAND) || (triad == TRIAD_LAND && enemyVehicleTriad == TRIAD_SEA))
				continue;
			
			// update range
			
			int range = map_range(vehicle.x, vehicle.y, enemyVehicle.x, enemyVehicle.y);
			enemyColonyRange = std::min(enemyColonyRange, range);
			
			
		}
		
		double expantionTravelTimeScale = conf.ai_expansion_travel_time_scale;
		if (enemyColonyRange <= 10)
		{
			expantionTravelTimeScale /= 1.0 + 2.0 * (1.0 - (double)enemyColonyRange / 10.0);
		}
		
		// find best buildSite
		
		MAP *bestBuildSite = nullptr;
		double bestBuildSiteScore = -DBL_MAX;
		
		for (MAP *tile : buildSites)
		{
			bool ocean = is_ocean(tile);
			TileExpansionInfo &tileExpansionInfo = getTileExpansionInfo(tile);
			
			// exclude unavailableBuildSite
			
			if (unavailableBuildSites.count(tile) != 0)
				continue;
			
			// exclude unmatching realm
			
			if ((triad == TRIAD_LAND && ocean) || (triad == TRIAD_SEA && !ocean))
				continue;
			
			// same sea cluster for sea unit or same land transported cluster for land unit
			
			switch (triad)
			{
			case TRIAD_SEA:
				
				if (!isSameSeaCluster(vehicleTile, tile))
					continue;
				
				break;
				
			case TRIAD_LAND:
				
				if (!isSameLandTransportedCluster(vehicleTile, tile))
					continue;
				
				break;
				
			}
			
			// get travel time
			// estimate it as range
			// plus transfer cost if different clusters
			
			double travelTime = getVehicleTravelTime(vehicleId, tile);
			if (travelTime == INF)
				continue;
			double travelTimeCoefficient = getExponentialCoefficient(expantionTravelTimeScale, travelTime);
			
			// get combined score
			
			double buildSiteScore = travelTimeCoefficient * tileExpansionInfo.buildSiteScore;
			
			debug
			(
				"\t\t%s"
				" buildSiteScore=%5.2f"
				" tileExpansionInfo.buildSiteScore=%5.2f"
				" enemyColonyRange=%2d"
				" expantionTravelTimeScale=%5.2f"
				" travelTime=%7.2f"
				" travelTimeCoefficient=%5.2f"
				"\n"
				, getLocationString(tile)
				, buildSiteScore
				, tileExpansionInfo.buildSiteScore
				, enemyColonyRange
				, expantionTravelTimeScale
				, travelTime
				, travelTimeCoefficient
			);
			
			// update best
			
			if (buildSiteScore > bestBuildSiteScore)
			{
				debug("\t\t\tbest\n");
				bestBuildSite = tile;
				bestBuildSiteScore = buildSiteScore;
			}
			
		}
		
		// not found
		
		if (bestBuildSite == nullptr)
			continue;
		
		// send colony
		
		vehicleIds.push_back(vehicleId);
		destinations.push_back(bestBuildSite);
		vehicleDestinations.push_back(vehicleDestinations.size());
		
		// disable nearby build sites
		
		std::vector<MAP *> thisSiteUnavailableBuildSites = getUnavailableBuildSites(bestBuildSite);
		unavailableBuildSites.insert(thisSiteUnavailableBuildSites.begin(), thisSiteUnavailableBuildSites.end());
		
	}
	
	Profiling::stop("distribute colonies");
	
	// optimize travel time
	
	Profiling::start("optimize travel time", "moveColonyStrategy");
	
	debug("\tvehicleIds.size()=%2d\n", vehicleIds.size());
	debug("\tdestinations.size()=%2d\n", destinations.size());
	
	std::vector<double> travelTimes(vehicleIds.size() * destinations.size());
	for (unsigned int vehicleIndex = 0; vehicleIndex < vehicleIds.size(); vehicleIndex++)
	{
		int vehicleId = vehicleIds.at(vehicleIndex);
		
		for (unsigned int destinationIndex = 0; destinationIndex < destinations.size(); destinationIndex++)
		{
			MAP *destination = destinations.at(destinationIndex);
			
			double travelTime = getVehicleTravelTime(vehicleId, destination);
			travelTimes.at(destinations.size() * vehicleIndex + destinationIndex) = travelTime;
			
			debug
			(
				"\t[%4d] %s->%s"
				" %5.2f"
				"\n"
				, vehicleId, getLocationString(getVehicleMapTile(vehicleId)), getLocationString(destination)
				, travelTime
			);
			
		}
		
	}
	
    // optimize
	
	bool improved;
	do
	{
		improved = false;
		
		for (unsigned int vehicleIndex1 = 0; vehicleIndex1 < vehicleIds.size(); vehicleIndex1++)
		{
			int destinationIndex1 = vehicleDestinations.at(vehicleIndex1);
			
			for (unsigned int vehicleIndex2 = 0; vehicleIndex2 < vehicleIds.size(); vehicleIndex2++)
			{
				int destinationIndex2 = vehicleDestinations.at(vehicleIndex2);
				
				double oldTravelTime1 = travelTimes.at(destinations.size() * vehicleIndex1 + destinationIndex1);
				double oldTravelTime2 = travelTimes.at(destinations.size() * vehicleIndex2 + destinationIndex2);
				double newTravelTime1 = travelTimes.at(destinations.size() * vehicleIndex1 + destinationIndex2);
				double newTravelTime2 = travelTimes.at(destinations.size() * vehicleIndex2 + destinationIndex1);
				
				debug
				(
					"\t\t"
					" oldTravelTime1=%5.2f"
					" oldTravelTime2=%5.2f"
					" newTravelTime1=%5.2f"
					" newTravelTime2=%5.2f"
					"\n"
					, oldTravelTime1
					, oldTravelTime2
					, newTravelTime1
					, newTravelTime2
				);
				
				if (newTravelTime1 == INF || newTravelTime2 == INF)
					continue;
				
				if (oldTravelTime1 == INF || oldTravelTime2 == INF || newTravelTime1 + newTravelTime2 < oldTravelTime1 + oldTravelTime2)
				{
					vehicleDestinations.at(vehicleIndex1) = destinationIndex2;
					vehicleDestinations.at(vehicleIndex2) = destinationIndex1;
					improved = true;
					
					debug
					(
						"\t\tswapped"
						" %2d->%2d"
						" %2d->%2d"
						"\n"
						, vehicleIndex1, destinationIndex2
						, vehicleIndex2, destinationIndex1
					);
					
				}
				
			}
			
		}
		
	}
	while (improved);
	
	for (unsigned int vehicleIndex = 0; vehicleIndex < vehicleIds.size(); vehicleIndex++)
	{
		int vehicleId = vehicleIds.at(vehicleIndex);
		int destinationIndex = vehicleDestinations.at(vehicleIndex);
		MAP *destination = destinations.at(destinationIndex);

		TaskType const taskType = aiData.getTileInfo(destination).unfriendlyDangerZone ? TT_SKIP : TT_BUILD;
		transitVehicle(Task(vehicleId, taskType, destination));
		
	}
	
	Profiling::stop("optimize travel time");
	
    // populate remaining available build sites
	
    availableBuildSites.clear();
	for (MAP *tile : buildSites)
	{
		// exclude unavailableBuildSite
		
		if (unavailableBuildSites.count(tile) != 0)
			continue;
		
		// add available site
		
		availableBuildSites.push_back(tile);
		
	}
	
}

/**
Computes build location future base gain.
*/
double getBuildSiteBaseGain(MAP *buildSite)
{
	Profiling::start("summarize tile yield scores", "moveColonyStrategy");
	
	debug("getBuildSiteBaseGain %s\n", getLocationString(buildSite));
	
	// tile yieldInfos
	
	std::vector<YieldInfo> yieldInfos;
	
	for (MAP *tile : buildSiteWorkTiles.at(buildSite))
	{
		TileExpansionInfo const &tileExpansionInfo = getTileExpansionInfo(tile);
		
		// compute yieldInfo
		
		Resource const &averageYield = tileExpansionInfo.averageYield;
		double nutrientSurplus = averageYield.nutrient - (double)Rules->nutrient_intake_req_citizen;
		double mineralIntake = averageYield.mineral;
		double energyIntake = averageYield.energy;
		
		// reduce value for tiles in existing base radius
		
		if (map_has_item(tile, BIT_BASE_RADIUS))
		{
			nutrientSurplus *= 0.75;
			mineralIntake *= 0.75;
			energyIntake *= 0.75;
		}
		
		// compute score
		
		double score = getResourceScore(nutrientSurplus, mineralIntake, energyIntake);
		double resourceScore = getResourceScore(mineralIntake, energyIntake);
		
//		// bonus score for bonus resources
//		
//		score +=
//			+ getNutrientBonus(tile)
//			+ getMineralBonus(tile)
//			+ getEnergyBonus(tile)
//		;
		
		// add bonus for monolith as it does not need to be terraformed
		
		if (map_has_item(tile, BIT_MONOLITH))
		{
			score += 2.0;
			resourceScore += 2.0;
		}
		
		yieldInfos.push_back({tile, score, nutrientSurplus, resourceScore});
		
	}
	
	if (yieldInfos.empty())
		return 0.0;
	
	Profiling::stop("summarize tile yield scores");
	
	// sort scores
	
	Profiling::start("sort scores", "moveColonyStrategy");
	std::sort(yieldInfos.begin(), yieldInfos.end(), compareYieldInfoByScoreAndResourceScore);
	Profiling::stop("sort scores");
	
	int const nutrientCostFactor = mod_cost_factor(aiFactionId, RSC_NUTRIENT, -1);
	
	int popSize = 1;
	int nutrientBoxSize = nutrientCostFactor * (1 + popSize);
	double nutrientAccumulated = 0.0;
	double totalBonus = 0.0;
	int evaluationTurns = 50;
	
	double yieldScore = (popSize - 1) < (int)yieldInfos.size() ? yieldInfos.at(popSize - 1).score : 0.0;
	double yieldNutrientSurplus = 0.5 * yieldScore / getResourceScore(1.0, 0.0, 0.0);
	double yieldResourceScore = 0.5 * yieldScore;
	double nutrientSurplus = (double)ResInfo->base_sq.nutrient + yieldNutrientSurplus;
	double resourceScore = getResourceScore(ResInfo->base_sq.mineral, ResInfo->base_sq.energy) + yieldResourceScore;
	
	for (int turn = 0; turn < evaluationTurns; turn++)
	{
		nutrientAccumulated += nutrientSurplus;
		totalBonus += resourceScore;
		
		if (nutrientAccumulated >= (double)nutrientBoxSize)
		{
			popSize++;
			nutrientBoxSize = nutrientCostFactor * (1 + popSize);
			nutrientAccumulated = 0.0;
			
			yieldScore = (popSize - 1) < (int)yieldInfos.size() ? yieldInfos.at(popSize - 1).score : 0.0;
			yieldNutrientSurplus = 0.5 * yieldScore / getResourceScore(1.0, 0.0, 0.0);
			yieldResourceScore = 0.5 * yieldScore;
			nutrientSurplus += yieldNutrientSurplus;
			resourceScore += yieldResourceScore;
			
		}
		
	}
	
	double gain = (totalBonus / (double)evaluationTurns) * (1.0 + getExponentialCoefficient(aiData.developmentScale, evaluationTurns));
	
//	if (DEBUG)
//	{
//		debug("\tyieldInfos\n");
//		
//		for (YieldInfo const &yieldInfo : yieldInfos)
//		{
//			debug
//			(
//				"\t\t%s"
//				" score=%5.2f"
//				" nutrientSurplus=%5.2f"
//				" resourceScore=%5.2f"
//				"\n"
//				, getLocationString(yieldInfo.tile).c_str()
//				, yieldInfo.score
//				, yieldInfo.nutrientSurplus
//				, yieldInfo.resourceScore
//			);
//			
//		}
//		
//		/*
//		debug
//		(
//			"\tmeanBaseGain=%5.2f"
//			" averagePopGrowth=%5.2f"
//			" averageResourceScore=%5.2f"
//			" income=%5.2f"
//			" incomeGrowth=%5.2f"
//			" incomeGain=%5.2f"
//			" incomeGrowthGain=%5.2f"
//			" gain=%5.2f"
//			"\n"
//			, getMeanBaseGain(0)
//			, averagePopGrowth
//			, averageResourceScore
//			, income
//			, incomeGrowth
//			, incomeGain
//			, incomeGrowthGain
//			, gain
//		);
//		*/
//		debug
//		(
//			"\tmeanBaseGain=%5.2f"
//			" gain=%5.2f"
//			"\n"
//			, getMeanBaseGain(0)
//			, gain
//		);
//		
//	}
	
	return gain;
	
}

/*
Computes build location placement score.
*/
double getBuildSitePlacementScore(MAP *tile)
{
	debug("getBuildSitePlacementScore %s\n", getLocationString(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	// radius overlap
	
	Profiling::start("radius overlap", "moveColonyStrategy");
	
	double overlapScore = getBuildSiteOverlapScore(tile);
	debug("\t%-20s%+5.2f\n", "overlapScore", overlapScore);
	
	Profiling::stop("radius overlap");
	
	// border connection
	
	Profiling::start("border connection", "moveColonyStrategy");
	
	double connectionScore = getBuildSiteConnectionScore(tile);
	debug("\t%-20s%+5.2f\n", "connectionScore", connectionScore);
	
	Profiling::stop("border connection");
	
	// land use
	
	Profiling::start("land use", "moveColonyStrategy");
	
	double landUseScore = getBuildSiteLandUseScore(tile);
	debug("\t%-20s%+5.2f\n", "landUseScore", landUseScore);
	
	Profiling::stop("land use");
	
	// prefer coast over inland
	
	Profiling::start("coastScore", "moveColonyStrategy");
	
	double coastScore = 0.0;
	
	if (tileInfo.land && !tileInfo.adjacentSeaRegions.empty())
	{
		double oceanConnectionCoefficient = 1.0 + (double)(tileInfo.adjacentSeaRegions.size() - 1) * conf.ai_expansion_ocean_connection_base;
		
		int sumAdjacentSeaRegionArea = 0;
		for (int adjacentSeaRegion : tileInfo.adjacentSeaRegions)
		{
			sumAdjacentSeaRegionArea += aiData.regionAreas.at(adjacentSeaRegion);
		}
		
		bool landRegionOceanConnection = false;
		for (int baseId : aiData.baseIds)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			int baseTileIndex = baseTile - *MapTiles;
			TileInfo const &baseTileInfo = aiData.tileInfos.at(baseTileIndex);
			
			if (baseTile->region == tile->region)
			{
				int sumBaseAdjacentSeaRegionArea = 0;
				for (int baseAdjacentSeaRegion : baseTileInfo.adjacentSeaRegions)
				{
					sumBaseAdjacentSeaRegionArea += aiData.regionAreas.at(baseAdjacentSeaRegion);
				}
				if (sumBaseAdjacentSeaRegionArea >= 10)
				{
					landRegionOceanConnection = true;
					break;
				}
			}
			
		}
		double smallLandCoefficient = 1.0 + (landRegionOceanConnection ? 0.0 : conf.ai_expansion_coastal_base_small_land * (aiData.regionAreas.at(tile->region) >= 200 ? 0.0 : 1.0 - (double)aiData.regionAreas.at(tile->region) / 200.0));
		
		double coastScoreCoefficient = conf.ai_expansion_coastal_base_small + (conf.ai_expansion_coastal_base_large - conf.ai_expansion_coastal_base_small) * (sumAdjacentSeaRegionArea >= 100 ? 1.0 : (double)sumAdjacentSeaRegionArea / 100.0);
		coastScore += coastScoreCoefficient * oceanConnectionCoefficient * smallLandCoefficient;
		
	}
	
	debug("\t%-20s%+5.2f\n", "coastScore", coastScore);
	
	Profiling::stop("coastScore");
	
	// explicitly discourage placing base on some landmarks
	
	Profiling::start("landmarkScore", "moveColonyStrategy");
	
	double landmarkScore = (map_has_landmark(tile, LM_CRATER) || map_has_landmark(tile, LM_VOLCANO)) ? -0.50 : 0.0;
	debug("\t%-20s%+5.2f\n", "landmarkScore", landmarkScore);
	
	Profiling::stop("landmarkScore");
	
	// explicitly encourage placing base on bonuses
	
	Profiling::start("landmarkBonusScore", "moveColonyStrategy");
	
	double landmarkBonusScore = 0.0;
	for (MAP *baseRadiusTile : getBaseRadiusTiles(tile, true))
	{
		// exclude not owned land for sea base
		
		if (tileInfo.ocean && baseRadiusTile->owner != aiFactionId)
			continue;
		
		// landmarkBonus tile
		
		if (isLandmarkBonus(baseRadiusTile))
		{
			landmarkBonusScore += 0.05;
		}
		
	}
	landmarkBonusScore = std::min(0.10, landmarkBonusScore);
	debug("\t%-20s%+5.2f\n", "landmarkBonusScore", landmarkBonusScore);
	
	Profiling::stop("landmarkBonusScore");
	
	// discourage land fungus for faction having difficulties to enter it (just because it spawns worms)
	
	double fungusScore = (!is_ocean(tile) && map_has_item(tile, BIT_FUNGUS) && aiFaction->SE_planet < 1 ? -0.1 : 0.0);
	debug("\t%-20s%+5.2f\n", "fungusScore", fungusScore);
	
	// discourage near edge placement
	
	int edgeMargin = std::min(y, *MapAreaY - 1 - y);
	if (!*MapToggleFlat)
	{
		edgeMargin = std::min(edgeMargin, std::min(x, *MapAreaX - 1 - x));
	}
	double edgeScore = edgeMargin >= 3 ? 0.0 : -0.2 * (3.0 - (double)edgeMargin);
	debug("\t%-20s%+5.2f\n", "edgeScore", edgeScore);
	
	// return score
	
	return overlapScore + connectionScore + landUseScore + coastScore + landmarkScore + landmarkBonusScore + fungusScore + edgeScore;
	
}

bool isValidBuildSite(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	// cannot build at volcano
	
	if (tile->landmarks & LM_VOLCANO && tile->code_at() == 0)
		return false;
	
	// cannot build in base tile and on monolith
	
	if (map_has_item(tile, BIT_BASE_IN_TILE | BIT_MONOLITH))
		return false;
	
	// allowed base build location
	
	if (!isAllowedBaseLocation(factionId, tile))
		return false;
	
	// no adjacent bases
	
	if (nearby_items(x, y, 1, 9, BIT_BASE_IN_TILE) > 0)
		return false;
	
	// no rocky tile unless allowed by configuration
	
	if (!is_ocean(tile) && tile->is_rocky() && !conf.ai_base_allowed_fungus_rocky)
		return false;
	
	// no fungus tile unless allowed by configuration
	
	if (map_has_item(tile, BIT_FUNGUS) && !conf.ai_base_allowed_fungus_rocky)
		return false;
	
	// no own bases closer than allowed spacing
	// no more than allowed own number of bases at minimal range
	
	int ownBaseAtMinimalRangeCount = 0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		int range = map_range(x, y, base->x, base->y);
		
		if (range < conf.base_spacing)
		{
			return false;
		}
		else if (conf.base_nearby_limit >= 0 && range == conf.base_spacing)
		{
			ownBaseAtMinimalRangeCount++;
			
			if (ownBaseAtMinimalRangeCount > conf.base_nearby_limit)
			{
				return false;
			}
			
		}
		
	}
	
	// not blocked
	
	if (isBlocked(tile))
		return false;
	
	// not dangerous
	
	if (tileInfo.unfriendlyDangerZone)
		return false;
	
	// don't build on edge
	
	if (y == 0 || y == *MapAreaY - 1 || (*MapToggleFlat == 1 && (x == 0 || x == *MapAreaX - 1)))
		return false;
	
	// all conditions met
	
	return true;
	
}

bool isValidWorkTile(MAP *baseTile, MAP *workTile)
{
	assert(isOnMap(baseTile));
	assert(isOnMap(workTile));
	
	int baseTileX = getX(baseTile);
	int baseTileY = getY(baseTile);
	int workTileX = getX(workTile);
	int workTileY = getY(workTile);
	TileInfo const &baseTileInfo = aiData.getTileInfo(baseTile);
	TileInfo const &workTileInfo = aiData.getTileInfo(workTile);
	TileExpansionInfo const &workTileExpansionInfo = getTileExpansionInfo(workTile);
	
	// cannot terraform volcano
	
	if (workTile->landmarks & LM_VOLCANO && workTile->code_at() == 0)
		return false;
	
	// exclude not owned land tiles for ocean base
	
	if (baseTileInfo.ocean && !workTileInfo.ocean && workTile->owner != aiFactionId)
		return false;
	
	// exclude already worked tiles
	
	if (workTileExpansionInfo.worked)
		return false;
	
	// available territory or adjacent to available territory (potentially available)
	
	if (workTile->owner == -1 || workTile->owner == aiFactionId)
		return true;
	
	// check tile is closer to our base than to other
	
	bool exclude = false;
	
	for (int otherBaseId = 0; otherBaseId < *BaseCount; otherBaseId++)
	{
		BASE *otherBase = getBase(otherBaseId);
		MAP *otherBaseTile = getBaseMapTile(otherBaseId);
		
		// exclude player base
		
		if (otherBase->faction_id == aiFactionId)
			continue;
		
		// get range
		
		int range = getRange(otherBaseTile, workTile);
		
		// exclude base further than 4 tiles apart
		
		if (range > 4)
			continue;
		
		// calculate distances
		
		int playerBaseDistance = vector_dist(baseTileX, baseTileY, workTileX, workTileY);
		int otherBaseDistance = vector_dist(otherBase->x, otherBase->y, workTileX, workTileY);
		
		// exclude tiles not closer to our base than to other
		
		if (playerBaseDistance >= otherBaseDistance)
		{
			exclude = true;
			break;
		}
		
	}
	
	// exclude excluded tile
	
	return !exclude;
	
}

/*
Searches for nearest AI faction base range capable to issue colony to build base at this tile.
*/
int getBuildSiteNearestBaseRange(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);

	int nearestBaseRange = INT_MAX;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);

		// only corresponding cluster for ocean

		if (ocean && !isSameSeaCluster(tile, baseTile))
			continue;

		nearestBaseRange = std::min(nearestBaseRange, map_range(x, y, base->x, base->y));

	}

	return nearestBaseRange;

}

/*
Searches for nearest AI faction colony range.
*/
int getNearestColonyRange(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);

	int nearestColonyRange = INT_MAX;

	for (int vehicleId : aiData.colonyVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		// proper triad only

		if ((ocean && vehicle->triad() == TRIAD_LAND) || (!ocean && vehicle->triad() == TRIAD_SEA))
			continue;

		// only corresponding cluster for ocean

		if (ocean && !isSameSeaCluster(tile, vehicleTile))
			continue;

		// reachable destination

		if (!isVehicleDestinationReachable(vehicleId, tile))
			continue;

		nearestColonyRange = std::min(nearestColonyRange, map_range(x, y, vehicle->x, vehicle->y));

	}

	return nearestColonyRange;

}

int getExpansionRange(MAP *tile)
{
	return std::min(getBuildSiteNearestBaseRange(tile), getNearestColonyRange(tile));
}

Resource getAverageTileYield(MAP *tile)
{
	bool monolith = map_has_item(tile, BIT_MONOLITH);
	bool ocean = is_ocean(tile);
	bool rocky = !ocean && (map_rockiness(tile) == 2);
	
	Resource averageYield;
	
	if (monolith)
	{
		TileYield yield = getTerraformingYield(-1, tile, {});
		averageYield = {(double)yield.nutrient, (double)yield.mineral, (double)yield.energy};
	}
	else if (ocean)
	{
		// average of mining platform and tidal harness
		
		TileYield miningPlatformYield = getTerraformingYield(-1, tile, {FORMER_FARM, FORMER_MINE});
		TileYield tidalHarnessYield = getTerraformingYield(-1, tile, {FORMER_FARM, FORMER_SOLAR});
		
		averageYield =
		{
			(miningPlatformYield.nutrient + tidalHarnessYield.nutrient) / 2.0,
			(miningPlatformYield.mineral + tidalHarnessYield.mineral) / 2.0,
			(miningPlatformYield.energy + tidalHarnessYield.energy) / 2.0,
		};
		
	}
	else
	{
		std::vector<std::vector<int>> terraformingOptions;
		
		if (rocky)
		{
			terraformingOptions.push_back({FORMER_REMOVE_FUNGUS, FORMER_ROAD, FORMER_MINE});
			terraformingOptions.push_back({FORMER_REMOVE_FUNGUS, FORMER_LEVEL_TERRAIN, FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE});
			terraformingOptions.push_back({FORMER_REMOVE_FUNGUS, FORMER_LEVEL_TERRAIN, FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR});
			terraformingOptions.push_back({FORMER_REMOVE_FUNGUS, FORMER_LEVEL_TERRAIN, FORMER_ROAD, FORMER_FOREST});
			terraformingOptions.push_back({FORMER_PLANT_FUNGUS});
			
		}
		else
		{
			terraformingOptions.push_back({FORMER_REMOVE_FUNGUS, FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE});
			terraformingOptions.push_back({FORMER_REMOVE_FUNGUS, FORMER_ROAD, FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR});
			terraformingOptions.push_back({FORMER_REMOVE_FUNGUS, FORMER_ROAD, FORMER_FOREST});
			terraformingOptions.push_back({FORMER_PLANT_FUNGUS});
			
		}
		
		TileYield bestYield;
		double bestYieldScore = 0.0;
		
		for (std::vector<int> const &terraformingOption : terraformingOptions)
		{
			TileYield yield = getTerraformingYield(-1, tile, terraformingOption);
			double yieldScore = getTerraformingResourceScore(yield);
			
			if (yieldScore > bestYieldScore)
			{
				bestYield = yield;
				bestYieldScore = yieldScore;
			}
			
		}
		
		TileYield yield = bestYield;
		averageYield = {(double)yield.nutrient, (double)yield.mineral, (double)yield.energy};
		
	}
	
	debug("getTileYield%s = (%5.2f,%5.2f,%5.2f)\n", getLocationString(tile), averageYield.nutrient, averageYield.mineral, averageYield.energy);
	
	return averageYield;
	
}

int getNearestEnemyBaseRange(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	int minRange = INT_MAX;

	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &(Bases[baseId]);

		// enemy base

		if (base->faction_id == aiFactionId)
			continue;

		// get range

		int range = map_range(x, y, base->x, base->y);

		// update minRange

		minRange = std::min(minRange, range);

	}

	// return zero if not modified

	if (minRange == INT_MAX)
	{
		minRange = 0;
	}

	return minRange;

}

/*
Returns unavailable build sites around already taken one.
*/
std::vector<MAP *> getUnavailableBuildSites(MAP *buildSite)
{
	bool buildSiteOcean = is_ocean(buildSite);

	std::vector<MAP *> unavailableBuildSites;

	// add radius 0-3 tiles
	// radius 0-1 = always
	// radius 2-3 = same realm

	for (MAP *tile : getRangeTiles(buildSite, 3, true))
	{
		int range = getRange(buildSite, tile);
		bool tileOcean = is_ocean(tile);

		if (range > 1 && tileOcean != buildSiteOcean)
			continue;

		unavailableBuildSites.push_back(tile);

	}

	return unavailableBuildSites;

}

/*
1.0 = exceptionally bad overlap
*/
double getBuildSiteOverlapScore(MAP *buildSite)
{
	// overlap count
	
	int overlapCount = getTileBaseOverlapCount(buildSite);
	
	// find the least adjacent overlap count
	
	MAP *minOverlapTile = buildSite;
	int minOverlapCount = overlapCount;
	
	bool changed;
	do
	{
		changed = false;
		MAP *currentTile = minOverlapTile;
		
		for (MAP *adjacentTile : getAdjacentTiles(currentTile))
		{
			int adjacentTileRadiusOverlapCount = getTileBaseOverlapCount(adjacentTile);
			
			if (minOverlapCount > conf.ai_expansion_radius_overlap_ignored && adjacentTileRadiusOverlapCount < minOverlapCount)
			{
				minOverlapTile = adjacentTile;
				minOverlapCount = adjacentTileRadiusOverlapCount;
				changed = true;
			}
			
		}
		
	}
	while (changed);
	
	// adjust buildSite overlap by best adjacent overlap
	
	int overlapCountAdjusted = overlapCount - std::max(0, minOverlapCount - conf.ai_expansion_radius_overlap_ignored);
	
	// normalize overlap by scale
	
	double overlapScore =
		overlapCountAdjusted <= conf.ai_expansion_radius_overlap_ignored ? 0.0 :
			conf.ai_expansion_radius_overlap_coefficient * (double)std::max(0, overlapCountAdjusted - conf.ai_expansion_radius_overlap_ignored)
	;
	
	debug
	(
		"getoverlapScore%s"
		" overlapCount=%d"
		" minOverlapTile=%s"
		" minOverlapCount=%d"
		" overlapCountAdjusted=%d"
		" ai_expansion_radius_overlap_coefficient=%5.2f"
		" overlapScore=%5.2f"
		"\n"
		, getLocationString(buildSite)
		, overlapCount
		, getLocationString(minOverlapTile)
		, minOverlapCount
		, overlapCountAdjusted
		, conf.ai_expansion_radius_overlap_coefficient
		, overlapScore
	);
	
	return overlapScore;
	
}

int getTileBaseOverlapCount(MAP *tile)
{
	int overlapCount = 0;
	
	for (MAP *baseRadiusTile : getBaseRadiusTiles(tile, true))
	{
		if (map_has_item(baseRadiusTile, BIT_BASE_RADIUS))
		{
			overlapCount++;
			
			// inner tile overlap is double
			
			for (MAP *adjacentTile : getAdjacentTiles(baseRadiusTile))
			{
				if (map_has_item(adjacentTile, BIT_BASE_IN_TILE))
				{
					overlapCount++;
				}
				
			}
			
		}
		
	}
	
	return overlapCount;
	
}

double getBuildSiteConnectionScore(MAP *buildSite)
{
	assert(isOnMap(buildSite));
	
	double internalConnectionCongestion = 0.0;
	
	for (MAP *radiusTile : getBaseRadiusTiles(buildSite, true))
	{
		if (internalConcaveNodes.find(radiusTile) != internalConcaveNodes.end())
		{
			internalConnectionCongestion += internalConcaveNodes.at(radiusTile);
		}
	}
	
	double externalConnectionCongestion = 0.0;
	
	for (MAP *radiusTile : getBaseRadiusTiles(buildSite, true))
	{
		if (externalConcaveNodes.find(radiusTile) != externalConcaveNodes.end())
		{
			externalConnectionCongestion += externalConcaveNodes.at(radiusTile);
		}
	}
	
	double connectionScore =
		+ conf.ai_expansion_internal_border_connection_coefficient * internalConnectionCongestion
		+ conf.ai_expansion_external_border_connection_coefficient * externalConnectionCongestion
	;
	
	debug
	(
		"getconnectionScore%s"
		" ai_expansion_internal_border_connection_coefficient=%5.2f"
		" internalConnectionCongestion=%5.2f"
		" ai_expansion_external_border_connection_coefficient=%5.2f"
		" externalConnectionCongestion=%5.2f"
		" connectionScore=%5.2f"
		"\n"
		, getLocationString(buildSite)
		, conf.ai_expansion_internal_border_connection_coefficient
		, internalConnectionCongestion
		, conf.ai_expansion_external_border_connection_coefficient
		, externalConnectionCongestion
		, connectionScore
	);
	
	return connectionScore;
	
}

/**
Evaluates build site placement score.
If potentialBuildSite is set it is counted as another base.
*/
double getBuildSiteLandUseScore(MAP *buildSite)
{
	assert(isOnMap(buildSite));
	
	int buildSiteX = getX(buildSite);
	int buildSiteY = getY(buildSite);
	
	double landCongestion = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = getBase(baseId);
		
		int proximity = getProximity(buildSiteX, buildSiteY, base->x, base->y);
		
		if (proximity > PLACEMENT_CONGESTION_PROXIMITY_MAX)
			continue;
		
		landCongestion += PLACEMENT_CONGESTION[proximity];
		
	}
	
	double landUseScore = conf.ai_expansion_land_use_coefficient * landCongestion;
	
	debug
	(
		"getBuildSiteLandUseScore%s"
		" landCongestion=%5.2f"
		" ai_expansion_land_use_coefficient=%5.2f"
		" landUseScore=%5.2f"
		"\n"
		, getLocationString(buildSite)
		, landCongestion
		, conf.ai_expansion_land_use_coefficient
		, landUseScore
	);
	
	return landUseScore;
	
}

/*
Returns yield score without worker nutrient consumption.
*/
double getEffectiveYieldScore(double nutrient, double mineral, double energy)
{
	return getResourceScore(nutrient - (double)Rules->nutrient_intake_req_citizen, mineral, energy);
}

/*
Colony travel time is more valuable early because they delay the development of whole faction.
*/
double getColonyTravelTimeCoefficient(double travelTime)
{
	int futureBaseCount = aiData.colonyVehicleIds.size() + aiData.baseIds.size();

	if (futureBaseCount <= 0)
	{
		debug("[ERROR] faction has no bases and no colonies.\n");
		exit(1);
	}

	double travelTimeMultiplier = 1.0 + 1.5 / (double)futureBaseCount;

	return getExponentialCoefficient(aiData.developmentScale, travelTimeMultiplier * travelTime);

}

void populateConcaveTiles()
{
	debug("populateConcaveTiles - %s\n", aiMFaction->noun_faction);
	
	internalConcaveNodes.clear();
	externalConcaveNodes.clear();
	
	robin_hood::unordered_flat_set<MAP *> processedNodes;
	
	std::vector<MAP *> openNodes;
	std::vector<MAP *> newOpenNodes;
	
	// ----------
	// internal
	// ----------
	
	// collect internal base radius tiles
	
	robin_hood::unordered_flat_set<MAP *> internalBaseRadiusTiles;
	
	for (int baseId : aiData.baseIds)
	{
		for (MAP *tile : getBaseRadiusTiles(getBaseMapTile(baseId), true))
		{
			// neutral or our territory
			
			if (!(tile->owner == -1 || tile->owner == aiFactionId))
				continue;
			
			// add tile
			
			internalBaseRadiusTiles.insert(tile);
			
		}
		
	}
	
	// initialize open nodes
	
	processedNodes.clear();
	openNodes.clear();
	for (MAP *tile : internalBaseRadiusTiles)
	{
		processedNodes.insert(tile);
		openNodes.push_back(tile);
	}
	
	// extend convex coverage
	
	while (!openNodes.empty())
	{
		for (MAP *currentTile : openNodes)
		{
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				MAP *stepTile = currentTile;
				std::vector<MAP *> connectedTiles;
				
				int stepCount = angle % 2 == 0 ? 6 : 5;
				
				for (int step = 0; step < stepCount; step++)
				{
					stepTile = getTileByAngle(stepTile, angle);
					
					// on map
					
					if (stepTile == nullptr)
						break;
					
					// neutral/our territory
					
					if (!(stepTile->owner == -1 || stepTile->owner == aiFactionId))
						break;
					
					// check processed
					
					if (processedNodes.find(stepTile) != processedNodes.end())
					{
						// stop processing and collect connectedTiles
						
						for (MAP *connectedTile : connectedTiles)
						{
							internalConcaveNodes.emplace(connectedTile, INT_MAX);
							processedNodes.insert(connectedTile);
							newOpenNodes.push_back(connectedTile);
						}
						
						break;
						
					}
					else
					{
						connectedTiles.push_back(stepTile);
					}
					
				}
				
			}
			
			openNodes.clear();
			openNodes.swap(newOpenNodes);
			
		}
		
	}
	
	// update congestion value
	
	for (robin_hood::pair<MAP *, double> internalConcaveNodeEntry : internalConcaveNodes)
	{
		MAP *tile = internalConcaveNodeEntry.first;
		
		double minTotalCongestion = DBL_MAX;
		
		for (int i = 0; i < 4; i++)
		{
			double totalCongestion = 0.0;
			
			for (MAP *radiusTile : getBaseRadiusTiles(tile, false))
			{
				// base radius or not concave tile
				
				double coefficient;
				
				if (radiusTile->is_base_radius())
				{
					coefficient = +1.00;
				}
				else if (internalConcaveNodes.find(radiusTile) == internalConcaveNodes.end())
				{
					coefficient = -0.25;
				}
				else
				{
					continue;
				}
				
				// exclude one side
				
				Location delta = getDelta(tile, radiusTile);
				Location rectangularDelta = getRectangularCoordinates(delta);
				
				if
				(
					(i == 0 && rectangularDelta.x >= +1)
					||
					(i == 1 && rectangularDelta.y >= +1)
					||
					(i == 2 && rectangularDelta.x <= -1)
					||
					(i == 3 && rectangularDelta.y <= -1)
				)
					continue;
				
				// congestion
				
				double distance = getEuclidianDistance(getX(tile), getY(tile), getX(radiusTile), getY(radiusTile));
				double congestion = coefficient * (1.0 / distance) / 1.5; // normalized by 1.5 because this is the lowest practical value
				totalCongestion += congestion;
				
			}
			
			minTotalCongestion = std::min(minTotalCongestion, std::max(0.0, totalCongestion));
			
		}
		
		internalConcaveNodes.at(tile) = minTotalCongestion;
		
	}
	
	// ----------
	// external
	// ----------
	
	// collect external base radius tiles
	
	robin_hood::unordered_flat_set<MAP *> externalBaseRadiusTiles;
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		// base radius
		
		if (!tile->is_base_radius())
			continue;
		
		// not internal
		
		if (internalBaseRadiusTiles.find(tile) != internalBaseRadiusTiles.end())
			continue;
		
		// add tile
		
		externalBaseRadiusTiles.insert(tile);
		
	}
	
	// initialize open nodes
	
	openNodes.clear();
	for (MAP *tile : processedNodes)
	{
		openNodes.push_back(tile);
	}
	for (MAP *tile : externalBaseRadiusTiles)
	{
		processedNodes.insert(tile);
	}
	
	// extend convex coverage
	
	while (!openNodes.empty())
	{
		for (MAP *currentTile : openNodes)
		{
			for (int angle = 0; angle < ANGLE_COUNT; angle++)
			{
				MAP *stepTile = currentTile;
				std::vector<MAP *> connectedTiles;
				
				int stepCount = angle % 2 == 0 ? 6 : 5;
				
				for (int step = 0; step < stepCount; step++)
				{
					stepTile = getTileByAngle(stepTile, angle);
					
					// on map
					
					if (stepTile == nullptr)
						break;
					
					// check processed
					
					if (processedNodes.find(stepTile) != processedNodes.end())
					{
						// stop processing and collect connectedTiles
						
						for (MAP *connectedTile : connectedTiles)
						{
							externalConcaveNodes.emplace(connectedTile, INT_MAX);
							processedNodes.insert(connectedTile);
							newOpenNodes.push_back(connectedTile);
						}
						
						break;
						
					}
					else
					{
						connectedTiles.push_back(stepTile);
					}
					
				}
				
			}
			
			openNodes.clear();
			openNodes.swap(newOpenNodes);
			
		}
		
	}
	
	// update congestion value
	
	for (robin_hood::pair<MAP *, double> externalConcaveNodeEntry : externalConcaveNodes)
	{
		MAP *tile = externalConcaveNodeEntry.first;
		
		double minTotalCongestion = DBL_MAX;
		
		for (int i = 0; i < 4; i++)
		{
			double totalCongestion = 0.0;
			
			for (MAP *radiusTile : getBaseRadiusTiles(tile, false))
			{
				// base radius or concave tile
				
				double coefficient;
				
				if (radiusTile->is_base_radius())
				{
					coefficient = +1.00;
				}
				else if (externalConcaveNodes.find(radiusTile) == externalConcaveNodes.end())
				{
					coefficient = -0.25
					;
				}
				else
				{
					continue;
				}
				
				// exclude one side
				
				Location delta = getDelta(tile, radiusTile);
				Location rectangularDelta = getRectangularCoordinates(delta);
				
				if
				(
					(i == 0 && rectangularDelta.x >= +1)
					||
					(i == 1 && rectangularDelta.y >= +1)
					||
					(i == 2 && rectangularDelta.x <= -1)
					||
					(i == 3 && rectangularDelta.y <= -1)
				)
					continue;
				
				// congestion
				
				double distance = getEuclidianDistance(getX(tile), getY(tile), getX(radiusTile), getY(radiusTile));
				double congestion = coefficient * (1.0 / distance) / 1.5; // normalized by 1.5 because this is the lowest practical value
				totalCongestion += congestion;
				
			}
			
			minTotalCongestion = std::min(minTotalCongestion, std::max(0.0, totalCongestion));
			
		}
		
		externalConcaveNodes.at(tile) = minTotalCongestion;
		
	}
	
//	if (DEBUG)
//	{
//		debug("\tinternal\n");
//		for (robin_hood::pair<MAP *, double> internalConcaveNodeEntry : internalConcaveNodes)
//		{
//			MAP *tile = internalConcaveNodeEntry.first;
//			double congestion = internalConcaveNodeEntry.second;
//			debug("\t\t%s %5.2f\n", getLocationString(tile), congestion);
//		}
//		
//		debug("\texternal\n");
//		for (robin_hood::pair<MAP *, double> externalConcaveNodeEntry : externalConcaveNodes)
//		{
//			MAP *tile = externalConcaveNodeEntry.first;
//			double congestion = externalConcaveNodeEntry.second;
//			debug("\t\t%s %5.2f\n", getLocationString(tile), congestion);
//		}
//		
//	}
	
}

