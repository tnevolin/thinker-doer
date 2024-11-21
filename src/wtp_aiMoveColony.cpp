#include "wtp_aiMoveColony.h"
#include <algorithm>
#include "wtp_terranx.h"
#include "wtp_game.h"
#include "wtp_ai.h"
#include "wtp_aiData.h"
#include "wtp_aiMove.h"
#include "wtp_aiRoute.h"

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

void moveColonyStrategy()
{
	executionProfiles["1.5.5. moveColonyStrategy"].start();
	
	// setup data

	populateExpansionData();

	// analyze base placement sites

	analyzeBasePlacementSites();

	executionProfiles["1.5.5. moveColonyStrategy"].stop();
	
}

void populateExpansionData()
{
	// reset
	
	tileExpansionInfos.clear();
	tileExpansionInfos.resize(*MapAreaTiles, {});
	
	buildSites.clear();
	availableBuildSites.clear();
	buildSiteWorkTiles.clear();
	
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
	
	executionProfiles["1.3.2.2.2. valid locations"].start();
	
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
		
		// iterate work tiles
		
		for (MAP *workTile : getBaseRadiusTiles(tile, false))
		{
			TileExpansionInfo &workTileExpansionInfo = getTileExpansionInfo(workTile);
			
			// valid work tile
			
			if (!isValidWorkTile(tile, workTile))
				continue;
			
			// set valid work tile
			
			workTileExpansionInfo.validWorkTile = true;
			buildSiteWorkTiles[tile].push_back(workTile);
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("validBuildSite\n");
		
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			TileExpansionInfo const &tileExpansionInfo = getTileExpansionInfo(tile);
			
			if (tileExpansionInfo.validBuildSite)
			{
				debug("\t%s\n", getLocationString(tile).c_str());
			}
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("validWorkTiles\n");
		
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			TileExpansionInfo const &tileExpansionInfo = getTileExpansionInfo(tile);
			
			if (tileExpansionInfo.validWorkTile)
			{
				debug("\t%s\n", getLocationString(tile).c_str());
			}
			
		}
		
	}
	
	executionProfiles["1.3.2.2.2. valid locations"].stop();
	
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
	
	executionProfiles["1.3.2.2.3. tile yield scores"].start();
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		TileExpansionInfo &tileExpansionInfo = getTileExpansionInfo(tile);
		
		// valid work tile
		
		if (!tileExpansionInfo.validWorkTile)
			continue;
		
		// get average yield
		
		tileExpansionInfo.averageYield = getAverageTileYield(tile);
		
	}
	
	executionProfiles["1.3.2.2.3. tile yield scores"].stop();
	
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
	
	if (DEBUG)
	{
		debug("buildSites - %s\n", MFactions[aiFactionId].noun_faction);
		
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			TileExpansionInfo &tileExpansionInfo = getTileExpansionInfo(tile);
			
			// valid build site
			
			if (!tileExpansionInfo.validBuildSite)
				continue;
			
			debug
			(
				"\t%s"
				" buildSiteBaseGain=%7.2f"
				" buildSitePlacementScore=%7.2f"
				" buildSiteScore=%7.2f"
				"\n"
				, getLocationString(tile).c_str()
				, tileExpansionInfo.buildSiteBaseGain
				, tileExpansionInfo.buildSitePlacementScore
				, tileExpansionInfo.buildSiteScore
			);
			
		}
		
	}
	
	// move colonies out of warzone
	
	debug("move colonies out of warzone - %s\n", MFactions[aiFactionId].noun_faction);
	
	for (int vehicleId : aiData.colonyVehicleIds)
	{
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		TileInfo const &vehicleTileInfo = aiData.getVehicleTileInfo(vehicleId);
		
		// warzone
		
		if (vehicleTileInfo.warzone)
		{
			// safe location
			
			MAP *safeLocation = getSafeLocation(vehicleId);
			transitVehicle(Task(vehicleId, TT_MOVE, safeLocation));
			
			debug("\t%s -> %s\n", getLocationString(vehicleTile).c_str(), getLocationString(safeLocation).c_str());
			
		}
		else
		{
			// active colony available for placement
			
			activeColonyVehicleIds.push_back(vehicleId);
			
		}
		
	}
		
    // distribute colonies to build sites
	
	executionProfiles["1.3.2.2.5. distribute colonies"].start();
	
	debug("distribute colonies to build sites - %s\n", MFactions[aiFactionId].noun_faction);
	
	robin_hood::unordered_flat_set<MAP *> &unavailableBuildSites = aiData.production.unavailableBuildSites;
	unavailableBuildSites.clear();
	
	std::vector<int> vehicleIds;
	std::vector<MAP *> destinations;
	std::vector<int> vehicleDestinations;
	
	for (int vehicleId : activeColonyVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();
		
		debug("\t%s %-32s\n", getLocationString({vehicle->x, vehicle->y}).c_str(), Units[vehicle->unit_id].name);
		
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
			
			double travelTime = conf.ai_expansion_travel_time_multiplier * getVehicleATravelTime(vehicleId, tile);
			
			if (travelTime == INF)
				continue;
			
			// get combined score
			
			double buildSiteScore = getGainDelay(tileExpansionInfo.buildSiteScore, travelTime);
			
			debug
			(
				"\t\t%s"
				" buildSiteScore=%5.2f"
				" tileExpansionInfo.buildSiteScore=%5.2f"
				" travelTime=%7.2f"
				"\n"
				, getLocationString(tile).c_str()
				, buildSiteScore
				, tileExpansionInfo.buildSiteScore
				, travelTime
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
	
	executionProfiles["1.3.2.2.5. distribute colonies"].stop();
	
	// optimize travel time
	
	executionProfiles["1.3.2.2.6. optimize travel time"].start();
	
	debug("\tvehicleIds.size()=%2d\n", vehicleIds.size());
	debug("\tdestinations.size()=%2d\n", destinations.size());
	
	std::vector<double> travelTimes(vehicleIds.size() * destinations.size());
	for (unsigned int vehicleIndex = 0; vehicleIndex < vehicleIds.size(); vehicleIndex++)
	{
		int vehicleId = vehicleIds.at(vehicleIndex);
		
		for (unsigned int destinationIndex = 0; destinationIndex < destinations.size(); destinationIndex++)
		{
			MAP *destination = destinations.at(destinationIndex);
			
			double travelTime = getVehicleATravelTime(vehicleId, destination);
			travelTimes.at(destinations.size() * vehicleIndex + destinationIndex) = travelTime;
			
			debug
			(
				"\t[%4d] %s->%s"
				" %5.2f"
				"\n"
				, vehicleId, getLocationString(getVehicleMapTile(vehicleId)).c_str(), getLocationString(destination).c_str()
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
		
		transitVehicle(Task(vehicleId, TT_BUILD, destination));
		
	}
	
	executionProfiles["1.3.2.2.6. optimize travel time"].stop();
	
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
	executionProfiles["1.3.2.2.4.2.1. summarize tile yield scores"].start();
	
	debug("getBuildSiteBaseGain %s\n", getLocationString(buildSite).c_str());
	
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
		
		// bonus score for bonus resources
		
		score +=
			+ getNutrientBonus(tile)
			+ getMineralBonus(tile)
			+ getEnergyBonus(tile)
		;
		
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
	
	executionProfiles["1.3.2.2.4.2.1. summarize tile yield scores"].stop();
	
	executionProfiles["1.3.2.2.4.2.3. sort scores"].start();
	
	// sort scores
	
	std::sort(yieldInfos.begin(), yieldInfos.end(), compareYieldInfoByScoreAndResourceScore);
	
	executionProfiles["1.3.2.2.4.2.3. sort scores"].stop();
	
	/*
	// weight drop from better to worse square
	
	double weightDrop = 0.7;
	
	// average weighted yield score
	
	double sumWeight = 0.0;
	double sumWeightPopGrowth = 0.0;
	double sumWeightResourceScore = 0.0;
	
	int popSize = 0;
	double baseNutrientSurplus = (double)ResInfo->base_sq_nutrient;
	double baseResourceScore = getResourceScore(ResInfo->base_sq_mineral, ResInfo->base_sq_energy);
	int const nutrientCostFactor = mod_cost_factor(aiFactionId, RSC_NUTRIENT, -1);
	
	double weight = 1.0;
	
	for (YieldInfo const &yieldInfo : yieldInfos)
	{
		popSize++;
		double yieldNutrientSurplus = 0.5 * yieldInfo.score / getResourceScore(1.0, 0.0, 0.0);
		baseNutrientSurplus += yieldNutrientSurplus;
		double yieldResourceScore = 0.5 * yieldInfo.score;
		baseResourceScore += yieldResourceScore;
		
		int nutrientBoxSize = nutrientCostFactor * (1 + popSize);
		double popGrowth = 1.0 / ((double)nutrientBoxSize / baseNutrientSurplus);
		
		double weightPopGrowth = weight * popGrowth;
		double weightResourceScore = weight * baseResourceScore;
		
		sumWeight += weight;
		sumWeightPopGrowth += weightPopGrowth;
		sumWeightResourceScore += weightResourceScore;
		
		weight *= weightDrop;
		
	}
	
	double averagePopGrowth = sumWeightPopGrowth/ sumWeight;
	double averageResourceScore = sumWeightResourceScore / sumWeight;
	
	// gain
	
	double income = averageResourceScore;
	double incomeGain = getGainIncome(income);
	
	double incomeGrowth = averagePopGrowth * averageResourceScore;
	double incomeGrowthGain = getGainIncomeGrowth(incomeGrowth);
	
	double gain = incomeGain + incomeGrowthGain;
	*/
	
	int const nutrientCostFactor = mod_cost_factor(aiFactionId, RSC_NUTRIENT, -1);
	
	int popSize = 1;
	int nutrientBoxSize = nutrientCostFactor * (1 + popSize);
	double nutrientSurplus = (double)ResInfo->base_sq_nutrient + yieldInfos.at(popSize - 1).nutrientSurplus;
	double resourceScore = getResourceScore(ResInfo->base_sq_mineral, ResInfo->base_sq_energy) + yieldInfos.at(popSize - 1).resourceScore;
	double nutrientAccumulated = 0.0;
	double totalBonus = 0.0;
	int evaluationTurns = 50;
	for (int turn = 0; turn < evaluationTurns; turn++)
	{
		nutrientAccumulated += nutrientSurplus;
		totalBonus += resourceScore;
		
		if (nutrientAccumulated >= (double)nutrientBoxSize)
		{
			popSize++;
			nutrientBoxSize = nutrientCostFactor * (1 + popSize);
			nutrientAccumulated = 0.0;
			
			double yieldNutrientSurplus = popSize - 1 < (int)yieldInfos.size() ? 0.5 * yieldInfos.at(popSize - 1).score / getResourceScore(1.0, 0.0, 0.0) : 0.0;
			double yieldResourceScore = popSize - 1 < (int)yieldInfos.size() ? 0.5 * yieldInfos.at(popSize - 1).score : 0.0;
			
			nutrientSurplus += yieldNutrientSurplus;
			resourceScore += yieldResourceScore;
			
		}
		
	}
	
	double gain = (totalBonus / (double)evaluationTurns) * (1.0 + getExponentialCoefficient(aiData.developmentScale, evaluationTurns));
	
	if (DEBUG)
	{
		debug("\tyieldInfos\n");
		
		for (YieldInfo const &yieldInfo : yieldInfos)
		{
			debug
			(
				"\t\t%s"
				" score=%5.2f"
				" nutrientSurplus=%5.2f"
				" resourceScore=%5.2f"
				"\n"
				, getLocationString(yieldInfo.tile).c_str()
				, yieldInfo.score
				, yieldInfo.nutrientSurplus
				, yieldInfo.resourceScore
			);
			
		}
		
		/*
		debug
		(
			"\tmeanBaseGain=%5.2f"
			" averagePopGrowth=%5.2f"
			" averageResourceScore=%5.2f"
			" income=%5.2f"
			" incomeGrowth=%5.2f"
			" incomeGain=%5.2f"
			" incomeGrowthGain=%5.2f"
			" gain=%5.2f"
			"\n"
			, getMeanBaseGain(0)
			, averagePopGrowth
			, averageResourceScore
			, income
			, incomeGrowth
			, incomeGain
			, incomeGrowthGain
			, gain
		);
		*/
		debug
		(
			"\tmeanBaseGain=%5.2f"
			" gain=%5.2f"
			"\n"
			, getMeanBaseGain(0)
			, gain
		);
		
	}
	
	return gain;
	
}

/*
Computes build location placement score.
*/
double getBuildSitePlacementScore(MAP *tile)
{
	debug("getBuildSitePlacementScore %s\n", getLocationString(tile).c_str());
	
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	executionProfiles["1.3.2.2.4.1.1. land use"].start();
	
	// land use
	
	double landUse = getBasePlacementLandUse(tile);
	double landUseScore = conf.ai_expansion_land_use_coefficient * (landUse - conf.ai_expansion_land_use_base_value);
	
	debug
	(
		"\t%-20s%+5.2f"
		" ai_expansion_land_use_base_value=%5.2f"
		" ai_expansion_land_use_coefficient=%+5.2f"
		" landUse=%5.2f"
		"\n"
		, "landUseScore", landUseScore
		, conf.ai_expansion_land_use_base_value
		, conf.ai_expansion_land_use_coefficient
		, landUse
	);
	
	executionProfiles["1.3.2.2.4.1.1. land use"].stop();
	
	executionProfiles["1.3.2.2.4.1.2. radius overlap"].start();
	
	// radius overlap
	
	double radiusOverlap = getBasePlacementRadiusOverlap(tile);
	double radiusOverlapScore =
		conf.ai_expansion_radius_overlap_coefficient * std::max(0.0, radiusOverlap - conf.ai_expansion_radius_overlap_base_value);
	
	debug
	(
		"\t%-20s%+5.2f"
		" ai_expansion_radius_overlap_base_value=%5.2f"
		" ai_expansion_radius_overlap_coefficient=%+5.2f"
		" radiusOverlap=%+5.2f"
		"\n"
		, "radiusOverlapScore", radiusOverlapScore
		, conf.ai_expansion_radius_overlap_base_value
		, conf.ai_expansion_radius_overlap_coefficient
		, radiusOverlap
	);
	
	executionProfiles["1.3.2.2.4.1.2. radius overlap"].stop();
	
	executionProfiles["1.3.2.2.4.1.3. coastScore"].start();
	
	// prefer coast over inland
	
	double coastScore = 0.0;
	
	if (tileInfo.land && !tileInfo.adjacentSeaRegions.empty())
	{
		int sumAdjacentSeaClusterArea = 0;
		double connectingOceanCoefficient = 5.0 * (double)(tileInfo.adjacentSeaRegions.size() - 1);
		
		for (int adjacentSeaRegion : tileInfo.adjacentSeaRegions)
		{
			int adjacentSeaRegionArea = aiData.seaRegionAreas.at(adjacentSeaRegion);
			
			sumAdjacentSeaClusterArea += adjacentSeaRegionArea;
			connectingOceanCoefficient *= adjacentSeaRegionArea >= 100 ? 1.0 : (double)adjacentSeaRegionArea / 100.0;
			
		}
		
		// coast
		
		double coastScoreCoefficient = sumAdjacentSeaClusterArea >= 4 ? 1.0 : (double)sumAdjacentSeaClusterArea / 4.0;
		coastScore += conf.ai_expansion_coastal_base * coastScoreCoefficient;
		
		// connecting oceans
		
		coastScore += conf.ai_expansion_coastal_base * connectingOceanCoefficient;
		
	}
	
	debug("\t%-20s%+5.2f\n", "coastScore", coastScore);
	
	executionProfiles["1.3.2.2.4.1.3. coastScore"].stop();
	
	executionProfiles["1.3.2.2.4.1.4. landmarkScore"].start();
	
	// explicitly discourage placing base on some landmarks
	
	double landmarkScore;
	if
	(
		map_has_landmark(tile, LM_CRATER)
		||
		map_has_landmark(tile, LM_VOLCANO)
	)
	{
		landmarkScore = -1.0;
	}
	else
	{
		landmarkScore = 0.0;
	}
	debug("\t%-20s%+5.2f\n", "landmarkScore", landmarkScore);
	
	executionProfiles["1.3.2.2.4.1.4. landmarkScore"].stop();
	
	executionProfiles["1.3.2.2.4.1.4. landmarkBonusScore"].start();
	
	// explicitly encourage placing base on bonuses
	
	double landmarkBonusScore = 0.0;
	for (MAP *baseRadiusTile : getBaseRadiusTiles(tile, true))
	{
		// exclude not owned land for sea base
		
		if (tileInfo.ocean && baseRadiusTile->owner != aiFactionId)
			continue;
		
		// landmarkBonus tile
		
		if (isLandmarkBonus(baseRadiusTile))
		{
			landmarkBonusScore += 0.1;
		}
		
	}
	landmarkBonusScore = std::min(0.3, landmarkBonusScore);
	debug("\t%-20s%+5.2f\n", "landmarkBonusScore", landmarkBonusScore);
	
	executionProfiles["1.3.2.2.4.1.4. landmarkBonusScore"].stop();
	
	// discourage land fungus for faction having difficulties to enter it (just because it spawns worms)
	
	double fungusScore = (!is_ocean(tile) && map_has_item(tile, BIT_FUNGUS) && aiFaction->SE_planet < 1 ? -0.1 : 0.0);
	debug("\t%-20s%+5.2f\n", "fungusScore", fungusScore);
	
	// return score
	
	return landUseScore + radiusOverlapScore + coastScore + landmarkScore + landmarkBonusScore + fungusScore;
	
}

bool isValidBuildSite(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	TileInfo &tileInfo = aiData.getTileInfo(tile);

	// allowed base build location

	if (!isAllowedBaseLocation(factionId, tile))
		return false;

	// cannot build in base tile and on monolith

	if (map_has_item(tile, BIT_BASE_IN_TILE | BIT_MONOLITH))
		return false;

	// no adjacent bases

	if (nearby_items(x, y, 1, 9, BIT_BASE_IN_TILE) > 0)
		return false;

	// cannot build at volcano

	if (tile->landmarks & LM_VOLCANO && tile->art_ref_id == 0)
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

	// not warzone

	if (tileInfo.warzone)
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
		VEH *vehicle = &(Vehicles[vehicleId]);
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
	
	debug("getTileYield: %s\n", getLocationString(tile).c_str());
	
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

/**
Evaluates build site placement score.
If potentialBuildSite is set it is counted as another base.
*/
double getBasePlacementLandUse(MAP *buildSite)
{
	assert(isOnMap(buildSite));
	
	int buildSiteX = getX(buildSite);
	int buildSiteY = getY(buildSite);
	
	// do not compute anything if too far from own bases
	
	int nearestOwnBaseRange = getNearestBaseRange(buildSite, aiData.baseIds, false);
	
	if (nearestOwnBaseRange > 8)
		return 0.0;
	
	// compute land use
	
	double landUse = 0.0;
	
	// negative score
	
	for (int offsetIndex = TABLE_square_block_radius_base_internal; offsetIndex < TABLE_square_block_radius_base_external; offsetIndex++)
	{
		int x = buildSiteX + TABLE_square_offset_x[offsetIndex];
		int y = buildSiteY + TABLE_square_offset_y[offsetIndex];
		
		if (!isOnMap(x, y))
			continue;
		
		MAP *tile = getMapTile(x, y);
		
		// extensionAngles
		
		std::vector<int> extensionAngles;
		
		switch (offsetIndex - TABLE_square_block_radius_base_internal)
		{
		case 0:
			extensionAngles.push_back(0);
			break;
		case 1:
			extensionAngles.push_back(2);
			break;
		case 2:
			extensionAngles.push_back(4);
			break;
		case 3:
			extensionAngles.push_back(6);
			break;
		case 11:
		case 4:
			extensionAngles.push_back(6);
			extensionAngles.push_back(0);
			break;
		case 5:
		case 6:
			extensionAngles.push_back(0);
			extensionAngles.push_back(2);
			break;
		case 7:
		case 8:
			extensionAngles.push_back(2);
			extensionAngles.push_back(4);
			break;
		case 9:
		case 10:
			extensionAngles.push_back(4);
			extensionAngles.push_back(6);
			break;
		}
		
		for (int extensionAngle : extensionAngles)
		{
			MAP *currentTile = tile;
			int extensionLength = 0;
			int landCount = 0;
			
			for (; extensionLength < 4; extensionLength++)
			{
				currentTile = getTileByAngle(currentTile, extensionAngle);
				
				// stop when get out of map
				
				if (currentTile == nullptr)
				{
					extensionLength = 4;
					break;
				}
				
				// stop at own base radius
				
				if ((currentTile->owner == -1 || currentTile->owner == aiFactionId) && map_has_item(currentTile, BIT_BASE_RADIUS))
					break;
				
				// populate variables
				
				bool currentTileOcean = is_ocean(currentTile);
				
				if (!currentTileOcean)
				{
					landCount++;
				}
				
			}
			
			if (extensionLength == 0)
			{
				landUse += 1.0;
				continue;
			}
			
			// no land
			
			if (landCount == 0)
				continue;
			
			
			// far enough
			
			if (extensionLength >= 4)
				continue;
			
			landUse -= 2.0 / (double)extensionLength;
			
		}
		
	}
	
	return landUse;
	
}

int getBasePlacementRadiusOverlap(MAP *tile)
{
	// collect score

	int totalOverlap = 0;

	for (MAP *baseRadiusTile : getBaseRadiusTiles(tile, true))
	{
		// our or neutral territory

		if ((baseRadiusTile->owner == -1 || baseRadiusTile->owner == aiFactionId) && map_has_item(baseRadiusTile, BIT_BASE_RADIUS))
		{
			totalOverlap++;
		}

	}

	return totalOverlap;

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

