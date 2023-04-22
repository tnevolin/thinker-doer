#include "aiMoveColony.h"
#include <algorithm>
#include "terranx_wtp.h"
#include "game_wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMove.h"
#include "aiRoute.h"

// expansion data

std::vector<ExpansionBaseInfo> expansionBaseInfos;
std::vector<ExpansionTileInfo> expansionTileInfos;
std::vector<MAP *> buildSites;
std::vector<MAP *> availableBuildSites;

double averageFutureYieldScore;

std::set<MAP *> landColonyLocations;
std::set<MAP *> seaColonyLocations;
std::set<MAP *> airColonyLocations;

clock_t s;

void setupExpansionData()
{
	// setup data
	
	expansionBaseInfos.clear();
	expansionBaseInfos.resize(MaxBaseNum, {});
	
	expansionTileInfos.clear();
	expansionTileInfos.resize(*map_area_tiles, {});
	
	// clear lists

	buildSites.clear();
	landColonyLocations.clear();
	seaColonyLocations.clear();
	airColonyLocations.clear();
	
}

// access expansion data arrays

ExpansionBaseInfo &getExpansionBaseInfo(int baseId)
{
	assert(baseId >= 0 && baseId < *total_num_bases);
	return expansionBaseInfos.at(baseId);
}

ExpansionTileInfo &getExpansionTileInfo(MAP *tile)
{
	assert(tile >= *MapPtr && tile < *MapPtr + *map_area_tiles);
	return expansionTileInfos.at(getMapIndexByPointer(tile));
}

// strategy

void moveColonyStrategy()
{
	// setup data
	
	profileFunction("1.3.2.1. setupExpansionData", setupExpansionData);
	
	// analyze base placement sites
	
	profileFunction("1.3.2.2. analyzeBasePlacementSites", analyzeBasePlacementSites);
	
}

void analyzeBasePlacementSites()
{
//	std::vector<std::pair<int, double>> sortedBuildSiteScores;
	std::set<int> colonyVehicleIds;
	std::set<int> colonyAssociations;
	
	debug("analyzeBasePlacementSites - %s\n", MFactions[aiFactionId].noun_faction);
	
	// find value for best sea tile
	// it'll be used as a scale for yield quality evaluation
	
	averageFutureYieldScore = getAverageLandSquareFutureYieldScore();
	
	// calculate colony associations
	// populate colonyLocations
	
	executionProfiles["1.3.2.2.1. colonyLocations"].start();
	for (int vehicleId : aiData.colonyVehicleIds)
	{
		// get vehicle associations
		
		int vehicleAssociation = getVehicleAssociation(vehicleId);
		
		// update accessible associations
		
		colonyAssociations.insert(vehicleAssociation);
		
		// populate colonyLocation
		
		switch (veh_triad(vehicleId))
		{
		case TRIAD_LAND:
			landColonyLocations.insert(getVehicleMapTile(vehicleId));
			break;
			
		case TRIAD_SEA:
			seaColonyLocations.insert(getVehicleMapTile(vehicleId));
			break;
			
		case TRIAD_AIR:
			airColonyLocations.insert(getVehicleMapTile(vehicleId));
			break;
			
		}
		
	}
	
	executionProfiles["1.3.2.2.1. colonyLocations"].stop();
	
	// valid locations
	
	executionProfiles["1.3.2.2.2. valid locations"].start();
	
	for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
	{
		int association = getAssociation(tile, aiFactionId);
		ExpansionTileInfo &expansionTileInfo = getExpansionTileInfo(tile);
		
		// within expansion range
		
		if (getExpansionRange(tile) > MAX_EXPANSION_RANGE)
			continue;
		
		// set expansionRange
		
		expansionTileInfo.expansionRange = true;
		
		// accessible or potentially reachable
		
		if (!(colonyAssociations.count(association) != 0 || isPotentiallyReachableAssociation(association, aiFactionId)))
			continue;
		
		// allowed base location
		
		if (!isAllowedBaseLocation(aiFactionId, tile))
			continue;
		
		// valid build site
		
		if (!isValidBuildSite(tile, aiFactionId))
			continue;
		
		// don't build on edge
		
		if (getY(tile) == 0 || getY(tile) == *map_axis_y - 1 || (*map_toggle_flat == 1 && (getX(tile) == 0 || getX(tile) == *map_axis_x - 1)))
			continue;
		
		// set valid build site
		
		expansionTileInfo.validBuildSite = true;
		
		// iterate work tiles
		
		for (MAP *workTile : getBaseRadiusTiles(tile, false))
		{
			ExpansionTileInfo &workExpansionTileInfo = getExpansionTileInfo(workTile);
			
			// valid work tile
			
			if (!isValidWorkTile(workTile, aiFactionId))
				continue;
			
			// set valid work tile
			
			workExpansionTileInfo.validWorkTile = true;
			
		}
		
	}
	
	executionProfiles["1.3.2.2.2. valid locations"].stop();
	
	// base radiuses
	
	for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
	{
		ExpansionTileInfo &expansionTileInfo = getExpansionTileInfo(tile);
		
		// land
		
		if (is_ocean(tile))
			continue;
		
		// baseRadius
		
		if (map_has_item(tile, TERRA_BASE_RADIUS))
		{
			expansionTileInfo.baseRadius = true;
		}
		
	}
	
	// populate tile yield scores
	
	executionProfiles["1.3.2.2.3. tile yield scores"].start();
	for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
	{
		ExpansionTileInfo &expansionTileInfo = getExpansionTileInfo(tile);
		
		// valid work tile
		
		if (!expansionTileInfo.validWorkTile)
			continue;
		
		// calculate yieldScore
		// subtract base yield score as minimal requirement to establish surplus baseline
		// divide by sea square yield score to normalize values in 0.0 - 1.0 range
		// don't let denominator value be less than 1.0
		
		double yieldScore = (getTileFutureYieldScore(tile) / averageFutureYieldScore - 1.0) * 2.0 + 1.0;
		
		// reduce yield score for tiles withing base radius
		
		if (map_has_item(tile, TERRA_BASE_RADIUS))
		{
			yieldScore *= 0.75;
		}
		
		debug
		(
			"yieldScore(%3d,%3d)=%5.2f"
			" averageFutureYieldScore=%5.2f"
			"\n"
			, getX(tile), getY(tile), yieldScore
			, averageFutureYieldScore
		);
		
		// store yieldScore
		
		getExpansionTileInfo(tile).yieldScore = yieldScore;
		
	}
	executionProfiles["1.3.2.2.3. tile yield scores"].stop();
	
	// populate buildSites
	
	aiData.production.landColonyDemand = -INT_MAX;
	aiData.production.seaColonyDemands.clear();
	
	executionProfiles["1.3.2.2.4. buildSiteScores"].start();
	
	for (MAP *tile = *MapPtr; tile < *MapPtr + *map_area_tiles; tile++)
	{
		ExpansionTileInfo &expansionTileInfo = getExpansionTileInfo(tile);
		
		// valid build site
		
		if (!expansionTileInfo.validBuildSite)
			continue;
		
		// add to buildSites
		
		buildSites.push_back(tile);
		
	}
	
	// populate placementScores
	
	executionProfiles["1.3.2.2.4.1. buildSitePlacementScores"].start();
	
	debug("buildSitePlacementScores\n");
	
	for (MAP *tile : buildSites)
	{
		ExpansionTileInfo &expansionTileInfo = getExpansionTileInfo(tile);
		expansionTileInfo.buildSitePlacementScore = getBuildSitePlacementScore(tile);
	}
	
	executionProfiles["1.3.2.2.4.1. buildSitePlacementScores"].stop();
	
	// populate placementScores
	
	executionProfiles["1.3.2.2.4.2. buildSiteYieldScores"].start();
	
	debug("buildSiteYieldScore\n");
	
	for (MAP *tile : buildSites)
	{
		ExpansionTileInfo &expansionTileInfo = getExpansionTileInfo(tile);
		expansionTileInfo.buildSiteYieldScore = getBuildSiteYieldScore(tile);
	}
	
	executionProfiles["1.3.2.2.4.2. buildSiteYieldScores"].stop();
	
	// populate buildScores
	
	executionProfiles["1.3.2.2.4.3. buildScores"].start();
	
	for (MAP *tile : buildSites)
	{
		ExpansionTileInfo &expansionTileInfo = getExpansionTileInfo(tile);
		double buildSitePlacementScore = expansionTileInfo.buildSitePlacementScore;
		double buildSiteYieldScore = expansionTileInfo.buildSiteYieldScore;
		
		// update build site score
		
		double buildSiteScore = buildSiteYieldScore + buildSitePlacementScore;
		expansionTileInfo.buildSiteBuildScore = buildSiteScore;
		
	}
	
	executionProfiles["1.3.2.2.4.3. buildScores"].stop();
	
	executionProfiles["1.3.2.2.4. buildSiteScores"].stop();
	
	if (DEBUG)
	{
		debug("colonyDemands - %s\n", MFactions[aiFactionId].noun_faction);
		debug("\tlandColonyDemand     = %6.2f\n", aiData.production.landColonyDemand);
		for (const auto &seaColonyDemandsEntry : aiData.production.seaColonyDemands)
		{
			int association = seaColonyDemandsEntry.first;
			double priority = seaColonyDemandsEntry.second;
			
			debug("\tseaColonyDemand[%3d] = %6.2f\n", association, priority);
			
		}
		
	}
	
	// precompute travel times
	
	profileFunction("1.3.2.2.A. populateColonyMovementCosts", populateColonyMovementCosts);
	
    // distribute colonies to build sites
    
	executionProfiles["1.3.2.2.5. distribute colonies"].start();
	
	debug("distribute colonies to build sites - %s\n", MFactions[aiFactionId].noun_faction);
	
	std::set<MAP *> &unavailableBuildSites = aiData.production.unavailableBuildSites;
	unavailableBuildSites.clear();
	
	std::vector<int> vehicleIds;
	std::vector<MAP *> destinations;
	std::vector<int> vehicleDestinations;
	
	for (int vehicleId : aiData.colonyVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int triad = vehicle->triad();
		int vehicleAssociation = getVehicleAssociation(vehicleId);
		
		debug("\t(%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
		
		// find best buildSite
		
		MAP *bestBuildSite = nullptr;
		double bestBuildSiteScore = -DBL_MAX;
		
		for (MAP *tile : buildSites)
		{
			bool ocean = is_ocean(tile);
			int association = getAssociation(tile, aiFactionId);
			int x = getX(tile);
			int y = getY(tile);
			ExpansionTileInfo &expansionTileInfo = getExpansionTileInfo(tile);
			
			// exclude unavailableBuildSite
			
			if (unavailableBuildSites.count(tile) != 0)
				continue;
			
			// exclude unmatching realm
			
			if (triad == TRIAD_LAND && ocean || triad == TRIAD_SEA && !ocean)
				continue;
			
			// exclude other ocean associations for sea unit
			
			if (triad == TRIAD_SEA && ocean && association != vehicleAssociation)
				continue;
			
			// exclude unreachable locations
			
			if (!isVehicleDestinationReachable(vehicleId, tile, true))
				continue;
			
			// get travel time
			// estimate it as range
			// plus transfer cost if different association
			
//			int travelTime = divideIntegerRoundUp(getRange(vehicleTile, tile), getVehicleSpeedWithoutRoads(vehicleId));
			int travelTime = getVehicleATravelTime(vehicleId, tile);
			
			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
				continue;
			
			double travelTimeCoefficient = getColonyTravelTimeCoefficient(travelTime);
			
			// get combined score
			
			double buildSiteScore = expansionTileInfo.buildSiteBuildScore * travelTimeCoefficient;
			
			debug
			(
				"\t\t(%3d,%3d)"
				" buildSiteScore=%5.2f"
				" expansionTileInfo->buildSiteBuildScore=%5.2f"
				" travelTime=%3d"
				" travelTimeCoefficient=%5.2f\n"
				, x, y
				, buildSiteScore
				, expansionTileInfo.buildSiteBuildScore
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
	executionProfiles["1.3.2.2.5. distribute colonies"].stop();
	
	// optimize travel time
	
	executionProfiles["1.3.2.2.6. optimize travel time"].start();
	
	debug("\tvehicleIds.size()=%2d\n", vehicleIds.size());
	debug("\tdestinations.size()=%2d\n", destinations.size());
	
	std::vector<int> travelTimes(vehicleIds.size() * destinations.size());
	for (unsigned int vehicleIndex = 0; vehicleIndex < vehicleIds.size(); vehicleIndex++)
	{
		int vehicleId = vehicleIds.at(vehicleIndex);
		
		for (unsigned int destinationIndex = 0; destinationIndex < destinations.size(); destinationIndex++)
		{
			MAP *destination = destinations.at(destinationIndex);
			
			int travelTime = getVehicleActionATravelTime(vehicleId, destination);
			travelTimes.at(destinations.size() * vehicleIndex + destinationIndex) = travelTime;
			
			debug
			(
				"\t[%4d] (%3d,%3d)->(%3d,%3d)"
				" %4d"
				"\n"
				, vehicleId, getVehicle(vehicleId)->x, getVehicle(vehicleId)->y, getX(destination), getY(destination)
				, travelTime
			);
			
		}
		
	}
	
    // optimize
    
    s = clock();
    
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
				
				int oldTravelTime1 = travelTimes.at(destinations.size() * vehicleIndex1 + destinationIndex1);
				int oldTravelTime2 = travelTimes.at(destinations.size() * vehicleIndex2 + destinationIndex2);
				int newTravelTime1 = travelTimes.at(destinations.size() * vehicleIndex1 + destinationIndex2);
				int newTravelTime2 = travelTimes.at(destinations.size() * vehicleIndex2 + destinationIndex1);
				
				debug
				(
					"\t\t"
					" oldTravelTime1=%3d"
					" oldTravelTime2=%3d"
					" newTravelTime1=%3d"
					" newTravelTime2=%3d"
					"\n"
					, oldTravelTime1
					, oldTravelTime2
					, newTravelTime1
					, newTravelTime2
				);
				
				if (newTravelTime1 == -1 || newTravelTime2 == -1)
					continue;
				
				if (oldTravelTime1 == -1 || oldTravelTime2 == -1 || newTravelTime1 + newTravelTime2 < oldTravelTime1 + oldTravelTime2)
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
	
	// cleanup global collections
	
	landColonyLocations.clear();
	seaColonyLocations.clear();
	airColonyLocations.clear();
	
}

/*
Computes build location yield score.
*/
double getBuildSiteYieldScore(MAP *tile)
{
	debug("getBuildSiteYieldScore (%3d,%3d)\n", getX(tile), getY(tile));
	
	// summarize tile yield scores

	std::vector<double> yieldScores;

	for (MAP *baseRadiusTile : getBaseRadiusTiles(tile, true))
	{
		int baseRadiusTileX = getX(baseRadiusTile);
		int baseRadiusTileY = getY(baseRadiusTile);
		
		// verify tile will be ours
		
		if (baseRadiusTile->owner != aiFactionId && map_has_item(baseRadiusTile, TERRA_BASE_RADIUS))
		{
			bool exclude = false;
			
			// check tile is closer to our base than to other
			
			for (int otherBaseId = 0; otherBaseId < *total_num_bases; otherBaseId++)
			{
				BASE *otherBase = getBase(otherBaseId);
				MAP *otherBaseTile = getBaseMapTile(otherBaseId);
				
				// exclude own bases
				
				if (otherBase->faction_id == aiFactionId)
					continue;
				
				// get range
				
				int range = getRange(tile, otherBaseTile);
				
				// exclude base further than 4 tiles apart
				
				if (range > 4)
					continue;
				
				// calculate distances
				
				int ourBaseDistance = vector_dist(getX(tile), getY(tile), baseRadiusTileX, baseRadiusTileY);
				int otherBaseDistance = vector_dist(otherBase->x, otherBase->y, baseRadiusTileX, baseRadiusTileY);
				
				// exclude tiles not closer to our base than to other
				
				if (ourBaseDistance >= otherBaseDistance)
				{
					exclude = true;
					break;
				}
				
			}
			
			// exclude excluded tile
			
			if (exclude)
				continue;
			
		}
		
		// valid work site
		
		if (!isValidWorkTile(baseRadiusTile, aiFactionId))
			continue;
		
		// ignore not owned land tiles for ocean base

		if (is_ocean(tile) && !is_ocean(baseRadiusTile) && baseRadiusTile->owner != aiFactionId)
			continue;
		
		// reduce score for already covered tiles
		
		double yieldScore = getExpansionTileInfo(baseRadiusTile).yieldScore;
		if (baseRadiusTile->owner == aiFactionId && map_has_item(baseRadiusTile, TERRA_BASE_RADIUS))
		{
			yieldScore *= 0.6;
		}

		// collect yield score

		yieldScores.push_back(yieldScore);
		debug("\t\t(%3d,%3d) %+6.2f (%+6.2f)\n", baseRadiusTileX, baseRadiusTileY, getExpansionTileInfo(baseRadiusTile).yieldScore, yieldScore);
		
	}
	
	// pad yieldScores to full base radius
	
	yieldScores.resize(21, 0.0);
	
	// sort scores
	
	std::sort(yieldScores.begin(), yieldScores.end(), std::greater<double>());
	
	// average weigthed score
	
	double weight = 1.0;
	double weightMultiplier = 0.8;
	double sumWeights = 0.0;
	double sumWeightedScore = 0.0;
	
	for (double yieldScore : yieldScores)
	{
		double weightedScore = weight * yieldScore;
		
		sumWeights += weight;
		sumWeightedScore += weightedScore;
		
		weight *= weightMultiplier;
		
		debug("\t\tyieldScore=%+6.2f weightedScore=%5.2f\n", yieldScore, weightedScore);
		
	}
	
	double score = 0.0;
	
	if (sumWeights > 0.0)
	{
		score = sumWeightedScore / sumWeights;
	}
	
	debug("\tscore=%+f\n", score);
	
	return score;
	
}

/*
Computes build location placement score.
*/
double getBuildSitePlacementScore(MAP *tile)
{
	bool ocean = is_ocean(tile);
	
	debug("getBuildSitePlacementScore (%3d,%3d)\n", getX(tile), getY(tile));
	
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
	
	// prefer coast over inland
	
	double coastScore = 0.0;
	
	if (!ocean)
	{
		std::set<int> oceanAssociations = getOceanAssociations(tile);
		
		if (oceanAssociations.size() == 1)
		{
			int oceanAssociation = *(oceanAssociations.begin());
			int connectedOceanArea = aiData.geography.factions[aiFactionId].associationAreas[oceanAssociation];
			coastScore += conf.ai_expansion_coastal_base * std::min(1.0, 0.1 * (double)connectedOceanArea);
		}
		else if (oceanAssociations.size() > 1)
		{
			int connectedOceanArea1 = 0;
			int connectedOceanArea2 = 0;
			
			for (int oceanAssociation : oceanAssociations)
			{
				int connectedOceanArea = aiData.geography.factions[aiFactionId].associationAreas[oceanAssociation];
				
				if (connectedOceanArea > connectedOceanArea1)
				{
					connectedOceanArea1 = connectedOceanArea;
				}
				else if (connectedOceanArea > connectedOceanArea2)
				{
					connectedOceanArea2 = connectedOceanArea;
				}
				
			}
			
			coastScore += conf.ai_expansion_ocean_connection_base * std::min(1.0, 0.1 * (double)connectedOceanArea2);
			
		}
		
	}
	
	debug("\t%-20s%+5.2f\n", "coastScore", coastScore);

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
	
	// return score
	
	return landUseScore + radiusOverlapScore + coastScore + landmarkScore;
	
}

bool isValidBuildSite(MAP *tile, int factionId)
{
	int x = getX(tile);
	int y = getY(tile);
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	TileMovementInfo &tileMovementInfo = tileInfo.movementInfos[factionId];
	
	// allowed base build location
	
	if (!isAllowedBaseLocation(factionId, tile))
		return false;
	
	// cannot build in base tile and on monolith
	
	if (map_has_item(tile, TERRA_BASE_IN_TILE | TERRA_MONOLITH))
		return false;
	
	// no adjacent bases
	
	if (nearby_items(x, y, 1, TERRA_BASE_IN_TILE) > 0)
		return false;
	
	// cannot build at volcano
	
	if (tile->landmarks & LM_VOLCANO && tile->art_ref_id == 0)
		return false;
	
	// no rocky tile unless allowed by configuration
	
	if (!is_ocean(tile) && tile->is_rocky() && !conf.ai_base_allowed_fungus_rocky)
		return false;
	
	// no fungus tile unless allowed by configuration
	
	if (map_has_item(tile, TERRA_FUNGUS) && !conf.ai_base_allowed_fungus_rocky)
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
	
	if (tileMovementInfo.blocked)
		return false;
	
	// not warzone
	
	if (tileInfo.warzone)
		return false;
	
	// all conditions met
	
	return true;
	
}

bool isValidWorkTile(MAP *tile, int factionId)
{
	// available territory or adjacent to available territory (potentially available)
	
	bool available = (tile->owner == -1 || tile->owner == factionId);
	
	if (!available)
	{
		for (MAP *adjacentTile : getAdjacentTiles(tile))
		{
			if (adjacentTile->owner == -1 || adjacentTile->owner == factionId)
			{
				available = true;
				break;
			}
			
		}
		
	}
	
	if (!available)
		return false;
	
	return true;
	
}

/*
Verifies destination is reachable within same region by expansionRange steps.
Quick check not counting ZOC.
*/
bool isWithinExpansionRangeSameAssociation(int x, int y, int expansionRange)
{
	debug("isWithinExpansionRangeSameAssociation (%3d,%3d)\n", x, y);
	
	MAP *tile = getMapTile(x, y);
	bool ocean = is_ocean(tile);
	int association = getAssociation(tile, aiFactionId);
	std::set<MAP *> *colonyLocations = (ocean ? &seaColonyLocations : &landColonyLocations);
	
	debug("association = %4d\n", association);
	
	if (DEBUG)
	{
		debug("colonyLocations\n");
		
		for (MAP *colonyLocation : *colonyLocations)
		{
			debug("\t(%3d,%3d)\n", getX(colonyLocation), getY(colonyLocation));
		}
			
	}
	
	// check initial condition
	
	if (isFactionBaseAt(tile, aiFactionId) || colonyLocations->count(tile) != 0)
		return true;
	
	// create processing arrays

	std::set<MAP *> visitedTiles;
	std::set<MAP *> currentTiles;
	std::set<MAP *> furtherTiles;
	
	// initialize search
	
	visitedTiles.insert(tile);
	currentTiles.insert(tile);
	
	// run search
	
	for (int step = 0; step <= expansionRange; step++)
	{
		// populate further Tiles
		
		for (MAP *currentTile : currentTiles)
		{
			debug("currentTile (%3d,%3d)\n", getX(currentTile), getY(currentTile));
	
			for (MAP *adjacentTile : getAdjacentTiles(currentTile))
			{
				debug("adjacentTile (%3d,%3d)\n", getX(adjacentTile), getY(adjacentTile));
		
				int adjacentAssociation = getAssociation(adjacentTile, aiFactionId);
				debug("adjacentAssociation = %4d\n", adjacentAssociation);
				
				// exit condition
				
				if (isFactionBaseAt(adjacentTile, aiFactionId) || colonyLocations->count(adjacentTile) != 0)
					return true;
				
				// same association
				
				if (adjacentAssociation != association)
					continue;
				
				// not visited one
				
				if (visitedTiles.count(adjacentTile) != 0)
					continue;
				
				// add further Tile
				
				furtherTiles.insert(adjacentTile);
				debug("added\n");
				
			}

		}
		
		// rotate tiles
		
		currentTiles.clear();
		currentTiles.insert(furtherTiles.begin(), furtherTiles.end());
		visitedTiles.insert(furtherTiles.begin(), furtherTiles.end());
		furtherTiles.clear();
		
	}
	
	return false;

}

/*
Verifies destination is either:
reachable within same region by expansionRange steps
or in direct range for other regions or air colonies
*/
bool isWithinExpansionRange(int x, int y, int expansionRange)
{
	MAP *tile = getMapTile(x, y);
	bool ocean = is_ocean(tile);
	int association = getAssociation(tile, aiFactionId);
	
	// air colony in range
	
	for (MAP *airColonyLocation : airColonyLocations)
	{
		int range = map_range(x, y, getX(airColonyLocation), getY(airColonyLocation));
		
		if (range <= expansionRange)
			return true;
		
	}
	
	if (!ocean)
	{
		// other association land colony in range
		
		for (MAP *landColonyLocation : landColonyLocations)
		{
			int vehicleAssociation = (ocean ? getOceanAssociation(landColonyLocation, aiFactionId) : getAssociation(landColonyLocation, aiFactionId));
			int range = map_range(x, y, getX(landColonyLocation), getY(landColonyLocation));
			
			if ((vehicleAssociation == -1 || vehicleAssociation != association) && range <= expansionRange)
				return true;
			
		}
		
	}
	
	// other association base in range
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);
		
		int baseAssociation = (ocean ? getOceanAssociation(baseTile, aiFactionId) : getAssociation(baseTile, aiFactionId));
		int range = map_range(x, y, base->x, base->y);
		
		if ((baseAssociation == -1 || baseAssociation != association) && range <= expansionRange)
			return true;
		
	}
	
	// direct access same association
	
	return isWithinExpansionRangeSameAssociation(x, y, expansionRange);
	
}

/*
Searches for nearest AI faction base range capable to issue colony to build base at this tile.
*/
int getBuildSiteNearestBaseRange(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	int association = getAssociation(tile, aiFactionId);
	
	int nearestBaseRange = INT_MAX;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		int baseOceanAssociation = getBaseOceanAssociation(baseId);

		// only corresponding association for ocean
		
		if (ocean && baseOceanAssociation != association)
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
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	int association = getAssociation(tile, aiFactionId);
	
	int nearestColonyRange = INT_MAX;

	for (int vehicleId : aiData.colonyVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int vehicleAssociation = getVehicleAssociation(vehicleId);
		
		// proper triad only
		
		if (ocean && vehicle->triad() == TRIAD_LAND || !ocean && vehicle->triad() == TRIAD_SEA)
			continue;
		
		// only corresponding association for ocean
		
		if (ocean && vehicleAssociation != association)
			continue;
		
		// reachable destination
		
		if (!isVehicleDestinationReachable(vehicleId, tile, false))
			continue;
		
		nearestColonyRange = std::min(nearestColonyRange, map_range(x, y, vehicle->x, vehicle->y));

	}

	return nearestColonyRange;

}

int getExpansionRange(MAP *tile)
{
	return std::min(getBuildSiteNearestBaseRange(tile), getNearestColonyRange(tile));
}

/*
Calculates best terraforming option for average land tile (moist, rolling, 1000) and returns its score.
This is a baseline score for the entire game and it assumes that all terraforming technologies are available.
*/
double getAverageLandSquareFutureYieldScore()
{
	debug("getAverageLandSquareFutureYieldScore\n");
	
	// find best terraforming option
	
	double bestTerraformingOptionYieldScore = 0.0;

	for (const int mainAction : {FORMER_MINE, FORMER_SOLAR})
	{
		double nutrients = 1.0;
		double minerals = 1.0;
		double energy = 0.0;
		
		// farm and corresponding techs
		
		if (has_tech(aiFactionId, getTerraform(FORMER_CONDENSER)->preq_tech))
		{
			nutrients += 0.5;
		}
		
		nutrients += *TERRA_IMPROVED_LAND_NUTRIENT;
		
		if (has_tech(aiFactionId, getTerraform(FORMER_SOIL_ENR)->preq_tech))
		{
			nutrients += 1.0;
		}
		
		switch (mainAction)
		{
		// mine
		
		case FORMER_MINE:
			{
				nutrients += (double)Rules->nutrient_effect_mine_sq;
				minerals += 1.0;
			}
			break;
			
		// solar
		
		case FORMER_SOLAR:
			{
				energy += 3.0;
				
				if (has_tech(aiFactionId, getTerraform(FORMER_ECH_MIRROR)->preq_tech))
				{
					energy += 1.5;
				}
				
			}
			break;
			
		}
		
		// calculate yield score
		
		double yieldScore = getEffectiveYieldScore(nutrients, minerals, energy);
		
		debug
		(
			"\tyieldScore=%f\n",
			yieldScore
		)
		;

		// update score
		
		bestTerraformingOptionYieldScore = std::max(bestTerraformingOptionYieldScore, yieldScore);
		
	}
	
	return std::max(1.0, bestTerraformingOptionYieldScore);
	
}

/*
Finds best terraforming option for given tile and calculates its yield score.
*/
double getTileFutureYieldScore(MAP *tile)
{
	bool ocean = is_ocean(tile);
	bool fungus = map_has_item(tile, TERRA_FUNGUS);
	bool rocky = (map_rockiness(tile) == 2);
	
	debug("getTileFutureYieldScore: (%3d,%3d)\n", getX(tile), getY(tile));
	
	// monolith does not require terraforming
	
	if (map_has_item(tile, TERRA_MONOLITH))
	{
		// effective yield score
		
		double monolithFutureYieldScore = getEffectiveYieldScore(*TERRA_MONOLITH_NUTRIENT, *TERRA_MONOLITH_MINERALS, *TERRA_MONOLITH_ENERGY);
		
		// add bonus score because there is no need to spend former on this tile
		
		monolithFutureYieldScore += 1.0;
		
		return monolithFutureYieldScore;
		
	}
	
	// get map state
	
	MAP currentMapState = *tile;
	
	// find best terraforming option
	
	double bestTerraformingOptionYieldScore = 0.0;

	for (const TERRAFORMING_OPTION *option : CONVENTIONAL_TERRAFORMING_OPTIONS)
	{
		// count only basic terraforming options
		
		if
		(
			!(
				option->requiredAction == FORMER_MINE
				||
				option->requiredAction == FORMER_SOLAR
				||
				option->requiredAction == FORMER_FOREST
				||
				option->requiredAction == FORMER_PLANT_FUNGUS
			)
		)
		{
			continue;
		}
		
		// exclude area effect improvements from future yield estimate
		// they will be factored in for each tile yield computation
		
		if (option->requiredAction == FORMER_CONDENSER || option->requiredAction == FORMER_ECH_MIRROR)
			continue;
		
		// process only correct realm

		if (option->ocean != ocean)
			continue;

		// rocky option is for land rocky tile only

		if (option->rocky && !(!ocean && rocky))
			continue;

		// check if required action is discovered

		if (!(option->requiredAction == -1 || has_terra(aiFactionId, option->requiredAction, ocean)))
			continue;
		
		debug("\t%s\n", option->name);
		
		// initialize variables

		MAP improvedMapState = currentMapState;
		
		// process actions
		
		for(int action : option->actions)
		{
			// exclude all improvement from deep ocean tile unless AQUATIC faction with proper tech
			
			if (ocean && map_elevation(tile) < -1 && !(isFactionSpecial(aiFactionId, FACT_AQUATIC) && has_tech(aiFactionId, Rules->tech_preq_mining_platform_bonus)))
				continue;
			
			// skip not discovered action
			
			if (!has_terra(aiFactionId, action, ocean))
				continue;
			
			// remove fungus if needed

			if (fungus && isRemoveFungusRequired(action))
			{
				generateTerraformingChange(&improvedMapState, FORMER_REMOVE_FUNGUS);
			}

			// level terrain if needed

			if (!ocean && rocky && isLevelTerrainRequired(ocean, action))
			{
				generateTerraformingChange(&improvedMapState, FORMER_LEVEL_TERRAIN);
			}

			// execute main action

			// generate terraforming change

			generateTerraformingChange(&improvedMapState, action);

		}

		// calculate yield score

		double yieldScore = getTerraformingOptionFutureYieldScore(tile, &currentMapState, &improvedMapState);
		debug("\t\tyieldScore=%f\n", yieldScore);
		
		// update score
		
		bestTerraformingOptionYieldScore = std::max(bestTerraformingOptionYieldScore, yieldScore);
		
	}
	
	return bestTerraformingOptionYieldScore;
	
}

/*
Calculates weighted resource score for given terraforming action set.
*/
double getTerraformingOptionFutureYieldScore(MAP *tile, MAP *currentMapState, MAP *improvedMapState)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	
	// set improved state
	
	*tile = *improvedMapState;
	
	// collect yields
	
	double nutrientYield = (double)mod_nutrient_yield(aiFactionId, -1, x, y, 0);
	double mineralYield = (double)mod_mineral_yield(aiFactionId, -1, x, y, 0);
	double energyYield = (double)mod_energy_yield(aiFactionId, -1, x, y, 0);
	
	// estimate condenser area effect
	
	if (!ocean && map_rockiness(tile) < 2 && map_rainfall(tile) < 2 && has_terra(aiFactionId, FORMER_CONDENSER, ocean))
	{
		nutrientYield += 0.75;
	}

	// estimate echelon mirror area effect
	
	if (!ocean && map_has_item(tile, TERRA_SOLAR) && has_terra(aiFactionId, FORMER_ECH_MIRROR, ocean))
	{
		energyYield += 2.0;
	}
	
	// estimate aquatic faction mineral effect
	
	if (ocean && map_elevation(tile) == -1 && isFactionSpecial(aiFactionId, FACT_AQUATIC) && !conf.disable_aquatic_bonus_minerals)
	{
		mineralYield++;
	}

	// estimate aquafarm effect
	
	if (ocean && map_has_item(tile, TERRA_FARM) && has_facility_tech(aiFactionId, FAC_AQUAFARM))
	{
		nutrientYield++;
	}

	// estimate subsea trunkline effect
	
	if (ocean && map_has_item(tile, TERRA_MINE) && has_facility_tech(aiFactionId, FAC_SUBSEA_TRUNKLINE))
	{
		mineralYield++;
	}

	// estimate thermocline transducer effect
	
	if (ocean && map_has_item(tile, TERRA_SOLAR) && has_facility_tech(aiFactionId, FAC_THERMOCLINE_TRANSDUCER))
	{
		energyYield++;
	}

	// estimate tree farm effect
	
	if (!ocean && map_has_item(tile, TERRA_FOREST) && has_facility_tech(aiFactionId, FAC_TREE_FARM))
	{
		nutrientYield++;
	}

	// estimate hybrid forest effect
	
	if (!ocean && map_has_item(tile, TERRA_FOREST) && has_facility_tech(aiFactionId, FAC_HYBRID_FOREST))
	{
		nutrientYield++;
		energyYield++;
	}
	
	// restore state
	
	*tile = *currentMapState;
	
	// compute score
	
	return getEffectiveYieldScore(nutrientYield, mineralYield, energyYield);

}

int getNearestEnemyBaseRange(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	
	int minRange = INT_MAX;
	
	for (int baseId = 0; baseId < *total_num_bases; baseId++)
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
Evaluate build site placement score.
If potentialBuildSite is set it is counted as another base.
*/
double getBasePlacementLandUse(MAP *buildSite)
{
	double landUse = 0.0;
	
	// positive score
	
	for (MAP *baseRadiusTile : getBaseRadiusTiles(buildSite, true))
	{
		bool baseRadiusTileOcean = is_ocean(baseRadiusTile);
		
		// land
		
		if (baseRadiusTileOcean)
			continue;
		
		// not base radius
		
		if (map_has_item(baseRadiusTile, TERRA_BASE_RADIUS))
			continue;
		
		// count side base radius on own or neutral territory
		
		for (MAP *sideTile : getSideTiles(baseRadiusTile))
		{
			bool sideTileOcean = is_ocean(sideTile);
			
			// land
			
			if (sideTileOcean)
				continue;
			
			// accumulate land use
			
			if ((sideTile->owner == -1 || sideTile->owner == aiFactionId) && map_has_item(sideTile, TERRA_BASE_RADIUS))
			{
				landUse++;
			}
			
		}
		
	}
	
	// negative score
	
	for (unsigned int index = TABLE_square_block_radius_base_internal; index < TABLE_square_block_radius_base_external; index++)
	{
		MAP *tile = getSquareBlockRadiusTile(buildSite, index);
		
		if (tile == nullptr)
			continue;
		
		// extensionAngles
		
		std::vector<int> extensionAngles;
		
		switch (index - TABLE_square_block_radius_base_internal)
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
					break;
				
				// stop at own base radius
				
				if ((currentTile->owner == -1 || currentTile->owner == aiFactionId) && map_has_item(currentTile, TERRA_BASE_RADIUS))
					break;
				
				// populate variables
				
				bool currentTileOcean = is_ocean(currentTile);
				
				if (!currentTileOcean)
				{
					landCount++;
				}
				
			}
			
			// touching base radius is already counted as positive
			
			if (extensionLength == 0)
				continue;
			
			// far enough
			
			if (extensionLength >= 4)
				continue;
			
			// no land
			
			if (landCount == 0)
				continue;
			
			// count land loss
			
			double landLoss = 1.0 * ((double)(4 - extensionLength) / 3.0);
debug(">(%3d,%3d) extensionAngle=%d extensionLength=%d\n", getX(tile), getY(tile), extensionAngle, extensionLength);
			
			landUse -= landLoss;
			
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
		
		if ((baseRadiusTile->owner == -1 || baseRadiusTile->owner == aiFactionId) && map_has_item(baseRadiusTile, TERRA_BASE_RADIUS))
		{
			totalOverlap++;
		}
		
	}
	
	return totalOverlap;
	
}

/*
Counts connected ocean associations.
*/
std::set<int> getOceanAssociations(MAP *tile)
{
	std::set<int> oceanAssociations;
	
	if (is_ocean(tile))
		return oceanAssociations;
	
	for (MAP *adjacentTile : getAdjacentTiles(tile))
	{
		if (is_ocean(adjacentTile))
		{
			oceanAssociations.insert(getOceanAssociation(adjacentTile, aiFactionId));
		}
		
	}
	
	return oceanAssociations;
	
}

/*
Returns yield score without worker nutrient consumption.
*/
double getEffectiveYieldScore(double nutrient, double mineral, double energy)
{
	return getYieldScore(nutrient - Rules->nutrient_intake_req_citizen, mineral, energy);
}

/*
Colony travel time is more valuable early because they delay the development of whole faction.
*/
double getColonyTravelTimeCoefficient(int travelTime)
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

