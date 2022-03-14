#include "aiMoveColony.h"
#include <algorithm>
#include "terranx_wtp.h"
#include "game_wtp.h"
#include "ai.h"
#include "aiData.h"
#include "aiMove.h"

// expansion data

std::vector<ExpansionBaseInfo> expansionBaseInfos;
std::vector<ExpansionTileInfo> expansionTileInfos;

double workerConsumptionYieldScore;
double seaSquareFutureYieldScore;

std::set<MAP *> landColonyLocations;
std::set<MAP *> seaColonyLocations;
std::set<MAP *> airColonyLocations;

void setupExpansionData()
{
	// setup data
	
	expansionBaseInfos.resize(MaxBaseNum, {});
	expansionTileInfos.resize(*map_area_tiles, {});
	
}

void cleanupExpansionData()
{
	// cleanup data
	
	expansionBaseInfos.clear();
	expansionTileInfos.clear();
	
	// clear lists

	landColonyLocations.clear();
	seaColonyLocations.clear();
	airColonyLocations.clear();
	
}

// access expansion data arrays

ExpansionBaseInfo *getExpansionBaseInfo(int baseId)
{
	assert(baseId >= 0 && baseId < *total_num_bases);
	return &(expansionBaseInfos.at(baseId));
}

ExpansionTileInfo *getExpansionTileInfo(int mapIndex)
{
	assert(mapIndex >= 0 && mapIndex < *map_area_tiles);
	return &(expansionTileInfos.at(mapIndex));
}
ExpansionTileInfo *getExpansionTileInfo(int x, int y)
{
	return getExpansionTileInfo(getMapIndexByCoordinates(x, y));
}
ExpansionTileInfo *getExpansionTileInfo(MAP *tile)
{
	return getExpansionTileInfo(getMapIndexByPointer(tile));
}

// strategy

void moveColonyStrategy()
{
	// setup data
	
	setupExpansionData();
	
	// analyze base placement sites
	
	clock_t s = clock();
	analyzeBasePlacementSites();
    debug("(time) [WTP] analyzeBasePlacementSites: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
	// cleanup data
	
	cleanupExpansionData();
	
}

void analyzeBasePlacementSites()
{
	std::vector<std::pair<int, double>> sortedBuildSiteScores;
	std::set<int> colonyVehicleIds;
	std::set<int> accessibleAssociations;
	
	debug("analyzeBasePlacementSites - %s\n", MFactions[aiFactionId].noun_faction);
	
	// find value for best sea tile
	// it'll be used as a scale for yield quality evaluation
	
	workerConsumptionYieldScore = getWorkerConsumptionYieldScore();
	seaSquareFutureYieldScore = getSeaSquareFutureYieldScore();
	
	// calculate accessible associations by current colonies
	// populate colonyLocations
	
	for (int vehicleId : aiData.colonyVehicleIds)
	{
		// get vehicle associations
		
		int vehicleAssociation = getVehicleAssociation(vehicleId);
		
		// update accessible associations
		
		accessibleAssociations.insert(vehicleAssociation);
		
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
	
	// calculate accessible associations by bases
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		
		// get base associations
		
		int baseAssociation = getAssociation(baseTile, aiFactionId);
		
		// update accessible associations
		
		accessibleAssociations.insert(baseAssociation);
		
		// same for ocean association
		
		int baseOceanAssociation = getOceanAssociation(baseTile, aiFactionId);
		
		if (baseOceanAssociation != -1)
		{
			accessibleAssociations.insert(baseOceanAssociation);
		}
		
	}
	
	// populate tile yield scores
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		int association = getAssociation(tile, aiFactionId);
		ExpansionTileInfo *expansionTileInfo = getExpansionTileInfo(mapIndex);
		
		// calculate and store expansionRange
		
		int expansionRange = getExpansionRange(tile);
		expansionTileInfo->expansionRange = expansionRange;
		
		// limit expansion range
		
		if (expansionRange > MAX_EXPANSION_RANGE + 2)
			continue;
		
		// calculate and store nearest colony and base
		
		std::pair<int, double> baseTravelTime = getNearestBaseTravelTime(tile);
		
		if (baseTravelTime.first != -1)
		{
			expansionTileInfo->nearestBaseId = baseTravelTime.first;
			expansionTileInfo->nearestBaseTravelTime = baseTravelTime.second;
		}
		
		std::pair<int, double> colonyTravelTime = getNearestColonyTravelTime(tile);
		
		if (colonyTravelTime.first != -1)
		{
			expansionTileInfo->nearestColonyId = colonyTravelTime.first;
			expansionTileInfo->nearestColonyTravelTime = colonyTravelTime.second;
		}
		
		// calculate and store enemy base range
		
		expansionTileInfo->enemyBaseRange = getNearestEnemyBaseRange(tile);
		
		// accessible or potentially reachable
		
		if (!(accessibleAssociations.count(association) != 0 || isPotentiallyReachableAssociation(association, aiFactionId)))
			continue;
		
		// valid work site
		
		if (!isValidWorkSite(tile, aiFactionId))
			continue;
		
		// accessible
		
		if (accessibleAssociations.count(association) != 0)
		{
			getExpansionTileInfo(mapIndex)->accessible = true;
		}
		
		// immediately reachable
		
		if (accessibleAssociations.count(association) != 0 || isImmediatelyReachableAssociation(association, aiFactionId))
		{
			getExpansionTileInfo(mapIndex)->immediatelyReachable = true;
		}
		
		// calculate yieldScore
		// subtract base yield score as minimal requirement to establish surplus baseline
		// divide by sea square yield score to normalize values in 0.0 - 1.0 range
		// don't let denominator value be less than 1.0
		
		double yieldScore = (getTileFutureYieldScore(tile) - workerConsumptionYieldScore) / std::max(1.0, seaSquareFutureYieldScore - workerConsumptionYieldScore);
		debug("yieldScore(%3d,%3d)=%f\n", getX(tile), getY(tile), yieldScore);
		if (DEBUG) fflush(debug_log);
		
		// store yieldScore
		
		getExpansionTileInfo(mapIndex)->yieldScore = yieldScore;
		
	}
	
	// populate buildSiteScores and set production priorities
	
	aiData.production.landColonyDemand = -INT_MAX;
	aiData.production.seaColonyDemands.clear();
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		ExpansionTileInfo *expansionTileInfo = getExpansionTileInfo(mapIndex);
		int association = getAssociation(tile, aiFactionId);
		
		// limit expansion range
		
		if (expansionTileInfo->expansionRange > MAX_EXPANSION_RANGE)
			continue;
		
		// accessible or potentially reachable
		
		if (!(accessibleAssociations.count(association) != 0 || isPotentiallyReachableAssociation(association, aiFactionId)))
			continue;

		// disabled it for now as it takes too long
//		// should be within expansion range of bases or colonies
//		// important: accounting for same realm colonies ability to reach destination
//		
//		if (!isWithinExpansionRange(getX(tile), getY(tile), MAX_EXPANSION_RANGE))
//			continue;
		
		// valid build site
		
		if (!isValidBuildSite(tile, aiFactionId))
		{
			getExpansionTileInfo(mapIndex)->buildScore = 0.0;
			continue;
		}
		
		// calculate basic score
		
		double yieldScore = getBuildSiteYieldScore(tile);
		double placementScore = getBuildSitePlacementScore(tile);
		double enemyRangeScore = getBuildSiteEnemyRangeScore(tile);
		
		// update build site score
		
		if (expansionTileInfo->nearestColonyId >= 0)
		{
			double colonyTravelTimeScore = getBuildSiteTravelTimeScore(expansionTileInfo->nearestColonyTravelTime);
			double buildScore = yieldScore + placementScore + colonyTravelTimeScore + enemyRangeScore;
			
			expansionTileInfo->buildScore = buildScore;
			sortedBuildSiteScores.push_back({mapIndex, buildScore});
			
		}
		
		// update production demand
		
		if (expansionTileInfo->nearestBaseId >= 0)
		{
			double baseTravelTimeScore = getBuildSiteTravelTimeScore(expansionTileInfo->nearestBaseTravelTime);
			double productionDemand = yieldScore + baseTravelTimeScore + enemyRangeScore;
			
			if (is_ocean(tile))
			{
				if (aiData.production.seaColonyDemands.count(association) == 0)
				{
					aiData.production.seaColonyDemands.insert({association, -INT_MAX});
				}
				aiData.production.seaColonyDemands.at(association) = std::max(aiData.production.seaColonyDemands.at(association), productionDemand);
			}
			else
			{
				aiData.production.landColonyDemand = std::max(aiData.production.landColonyDemand, productionDemand);
			}
			
		}
		
	}
	
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
	
	// sort sites by score descending
	
	std::sort
	(
		sortedBuildSiteScores.begin(),
		sortedBuildSiteScores.end(),
		[ ](const std::pair<int, double> &buildSiteScore1, const std::pair<int, double> &buildSiteScore2)
		{
			return buildSiteScore1.second > buildSiteScore2.second;
		}
	)
	;
	
	if (DEBUG)
	{
		debug("sortedBuildSiteScores - %s\n", MFactions[aiFactionId].noun_faction);
		
		for (std::pair<int, double> buildSiteScore : sortedBuildSiteScores)
		{
			debug("(%3d,%3d) %f\n", getX(buildSiteScore.first), getY(buildSiteScore.first), buildSiteScore.second);
		}
		
	}
	
	// collect existing colonies
	
	colonyVehicleIds.insert(aiData.colonyVehicleIds.begin(), aiData.colonyVehicleIds.end());
	
	// match build sites with existing colonies and set colony production priorities
	
	debug("\tmatch build sites with existing colonies\n");
	
	std::vector<int> matchedBuildSiteMapIndexes;
	
	for (const auto &sortedBuildSiteScoresEntry : sortedBuildSiteScores)
	{
		int buildSiteMapIndex = sortedBuildSiteScoresEntry.first;
		double buildSiteScore = sortedBuildSiteScoresEntry.second;
		
		int x = getX(buildSiteMapIndex);
		int y = getY(buildSiteMapIndex);
		MAP *buildSiteTile = getMapTile(buildSiteMapIndex);
		bool ocean = is_ocean(buildSiteTile);
		
		if (DEBUG)
		{
			int association = getAssociation(buildSiteTile, aiFactionId);
			debug("\t\t(%3d,%3d) = %6.2f %-4s %4d\n", x, y, buildSiteScore, (ocean ? "sea" : "land"), association);
		}
		
		// verify this build site is within allowed distance from already matched ones
		
		bool validBuildSite = true;
		
		for (int matchedBuildSiteMapIndex : matchedBuildSiteMapIndexes)
		{
			int matchedX = getX(matchedBuildSiteMapIndex);
			int matchedY = getY(matchedBuildSiteMapIndex);
			
			int range = map_range(matchedX, matchedY, x, y);
			
			if
			(
				range < conf.base_spacing
				||
				(
					range <= 2
					||
					range <= 3 && (matchedX != x && matchedY != y)
				)
			)
			{
				validBuildSite = false;
				debug("\t\t\ttoo close to other matched site: (%3d,%3d)\n", matchedX, matchedY);
				break;
			}
			
		}
		
		if (!validBuildSite)
			continue;
		
		// find nearest colony that can reach the site
		
		int nearestColonyVehicleId = -1;
		double nearestColonyTravelTime = DBL_MAX;
		
		for (int vehicleId : colonyVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			// proper triad
			
			if (!(vehicle->triad() == TRIAD_AIR || vehicle->triad() == ocean))
				continue;
			
			// can reach
			
			if (!isDestinationReachable(vehicleId, x, y, false))
				continue;
			
			// get travel time
			
			double travelTime = getTravelTime(vehicleId, x, y);
			
			// update best
			
			if (travelTime < nearestColonyTravelTime)
			{
				nearestColonyVehicleId = vehicleId;
				nearestColonyTravelTime = travelTime;
			}
			
		}
		
		// not found
		
		if (nearestColonyVehicleId == -1)
		{
			debug("\t\t\tno colony can reach the destination\n");
			continue;
		}
		
		// add to matched build sites
		
		matchedBuildSiteMapIndexes.push_back(buildSiteMapIndex);
		
		// set order
		
		transitVehicle(nearestColonyVehicleId, Task(BUILD, nearestColonyVehicleId, getMapTile(x, y)));
		
		// remove nearestColonyVehicleId from list
		
		colonyVehicleIds.erase(nearestColonyVehicleId);
		
		debug("\t\t\t[%3d] (%3d,%3d) ->\n", nearestColonyVehicleId, Vehicles[nearestColonyVehicleId].x, Vehicles[nearestColonyVehicleId].y);
		
		// exit if no more colonies left
		
		if (colonyVehicleIds.size() == 0)
			break;
			
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
	int x = getX(tile);
	int y = getY(tile);
	
	debug("getBuildSiteYieldScore (%3d,%3d)\n", x, y);

	// summarize tile yield scores

	std::vector<double> yieldScores;

	for (MAP *baseRadiusTile : getBaseRadiusTiles(tile, false))
	{
		// valid work site
		
		if (!isValidWorkSite(baseRadiusTile, aiFactionId))
			continue;
		
		// ignore not owned land tiles for ocean base

		if (is_ocean(tile) && !is_ocean(baseRadiusTile) && baseRadiusTile->owner != aiFactionId)
			continue;

		// collect yield score

		yieldScores.push_back(getExpansionTileInfo(baseRadiusTile)->yieldScore);
		
	}
	
	// sort scores
	
	std::sort(yieldScores.begin(), yieldScores.end(), std::greater<double>());
	
	// average weigthed score
	
	double weight = 1.0;
	double weightMultiplier = 0.9;
	double sumWeights = 0.0;
	double sumWeightedScore = 0.0;
	
	for (double yieldScore : yieldScores)
	{
		debug("\t\t%+6.2f\n", yieldScore);
		
		double weightedScore = weight * yieldScore;
		
		sumWeights += weight;
		sumWeightedScore += weightedScore;
		
		weight *= weightMultiplier;
		
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
Evaluates build location score based on distance to existing bases.
*/
double getBuildSiteTravelTimeScore(double travelTime)
{
	debug("getBuildSiteTravelTimeScore(%f)\n", travelTime);

	// turn penalty coefficient
	
	double turnPenaltyCoefficient = conf.ai_expansion_time_penalty;
	
	// increase turn penalty for small number of bases
	
	if (conf.ai_expansion_time_penalty_base_threshold > 0 && (double)aiData.baseIds.size() < conf.ai_expansion_time_penalty_base_threshold)
	{
		turnPenaltyCoefficient +=
			(conf.ai_expansion_time_penalty_early - conf.ai_expansion_time_penalty)
			*
			(1.0 - (double)aiData.baseIds.size() / conf.ai_expansion_time_penalty_base_threshold)
		;
		
	}
	
	// calculate rangeScore
	
    double score = - turnPenaltyCoefficient * travelTime;
    
    debug("\tscore=%+f, travelTime=%f, turnPenaltyCoefficient=%f\n", score, travelTime, turnPenaltyCoefficient);
    
    return score;
    
}

/*
Computes build location placement score.
*/
double getBuildSitePlacementScore(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	
	debug("getBuildSitePlacementScore (%3d,%3d)\n", x, y);
	
	double score = 0.0;

	// prefer coast over inland
	
	if (!ocean)
	{
		if (isCoast(x, y))
		{
			score += conf.ai_expansion_coastal_base;
		}
		else
		{
			score += conf.ai_expansion_inland_base;
		}
		
	}
	debug("\t+coast=%+f\n", score);

	// compute placement quality toward other bases
	
	int placementQuality = 0;
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		
		// get coordinate difference
		
		int dx = abs(base->x - x);
		int dy = abs(base->y - y);
		
		// adjust dx for cylindrical map
		
		if (*map_toggle_flat == 0 && dx > *map_half_x)
		{
			dx = *map_axis_x - dx;
		}
		
		// order diagonal coordinates
		
		int minDC = std::min(dx, dy);
		int maxDC = std::max(dx, dy);
		
		// convert to rectangular coordinates
		
		int minRC = (maxDC - minDC) / 2;
		int maxRC = (maxDC + minDC) / 2;
		
		// limit to near range
		
		if (maxRC > PLACEMENT_MAX_RANGE)
			continue;
		
		// get quality from table
		
		int quality = PLACEMENT_QUALITY[minRC][maxRC];
		
		// update total value
		
		placementQuality += quality;
		
	}
	
	score += conf.ai_expansion_placement * (double)placementQuality;
	debug("\t+placement=%+f, ai_expansion_placement=%+f, placementQuality=%d\n", score, conf.ai_expansion_placement, placementQuality);
	
	// return score
	
	return score;
	
}

/*
Computes build location placement score.
*/
double getBuildSiteEnemyRangeScore(MAP *tile)
{
	debug("getBuildSiteEnemyRangeScore (%3d,%3d)\n", getX(tile), getY(tile));
	debug("\tscore=%f\n", conf.ai_expansion_enemy_base_range * (double)getExpansionTileInfo(tile)->enemyBaseRange);
	
	return conf.ai_expansion_enemy_base_range * (double)getExpansionTileInfo(tile)->enemyBaseRange;
	
}

bool isValidBuildSite(MAP *tile, int factionId)
{
	int x = getX(tile);
	int y = getY(tile);
	
	// unclaimed territory
	
	if (!(tile->owner == -1 || tile->owner == factionId))
		return false;
	
	// cannot build in base tile and on monolith
	
	if (map_has_item(tile, TERRA_BASE_IN_TILE | TERRA_MONOLITH))
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
	
	// no bases closer than allowed spacing
	
	if (nearby_items(x, y, conf.base_spacing - 1, TERRA_BASE_IN_TILE) > 0)
		return false;
	
	// no more than allowed number of bases at minimal range
	
	if (conf.base_nearby_limit >= 0 && nearby_items(x, y, conf.base_spacing, TERRA_BASE_IN_TILE) > conf.base_nearby_limit)
		return false;
	
	// not blocked
	
	if (aiData.getTileInfo(tile)->blocked)
		return false;
	
	// not warzone
	
	if (aiData.getTileInfo(tile)->warzone)
		return false;
	
	// all conditions met
	
	return true;
	
}

bool isValidWorkSite(MAP *tile, int factionId)
{
	// available territory
	
	if (!(tile->owner == -1 || tile->owner == factionId))
		return false;
	
	// not in base radius
	
	if (map_has_item(tile, TERRA_BASE_RADIUS))
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
	
	if (aiData.baseTiles.count(tile) != 0 || colonyLocations->count(tile) != 0)
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
	
			for (MAP *adjacentTile : getAdjacentTiles(currentTile, false))
			{
				debug("adjacentTile (%3d,%3d)\n", getX(adjacentTile), getY(adjacentTile));
		
				int adjacentAssociation = getAssociation(adjacentTile, aiFactionId);
				debug("adjacentAssociation = %4d\n", adjacentAssociation);
				
				// exit condition
				
				if (aiData.baseTiles.count(adjacentTile) != 0 || colonyLocations->count(adjacentTile) != 0)
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
Returns all base radius land tiles touching friendly base radius land tiles.
*/
int getBaseRadiusTouchCount(int x, int y, int factionId)
{
	int baseRadiusTouchCount = 0;
	
	for (int offset = BASE_OFFSET_COUNT_ADJACENT; offset < BASE_OFFSET_COUNT_RADIUS; offset++)
	{
		int radiusTileX = wrap(x + BASE_TILE_OFFSETS[offset][0]);
		int radiusTileY = y + BASE_TILE_OFFSETS[offset][1];
		
		if (!isOnMap(radiusTileX, radiusTileY))
			continue;
		
		MAP *radiusTile = getMapTile(radiusTileX, radiusTileY);
		
		// land
		
		if (is_ocean(radiusTile))
			continue;
		
		// unclaimed territory
		
		if (!(radiusTile->owner == -1 || radiusTile->owner == factionId))
			continue;
		
		// iterate side touching tiles
		
		for (MAP *touchingTile : getAdjacentTiles(radiusTileX, radiusTileY, false))
		{
			int touchingTileX = getX(touchingTile);
			int touchingTileY = getY(touchingTile);
			
			int touchingTileDX = touchingTileX - x;
			int touchingTileDY = touchingTileY - y;
			
			// should be touching by side
			
			if (touchingTileX == radiusTileX || touchingTileY == radiusTileY)
				continue;
			
			// should be further away from the center
			
			if (!((abs(touchingTileDX) + abs(touchingTileDY) == 6) || (abs(touchingTileDX) == 0 && abs(touchingTileDY) == 4) || (abs(touchingTileDX) == 4 && abs(touchingTileDY) == 0)))
				continue;
			
			// land
			
			if (is_ocean(touchingTile))
				continue;
			
			// unclaimed territory
			
			if (!(touchingTile->owner == -1 || touchingTile->owner == factionId))
				continue;
			
			// base radius
			
			if (!map_has_item(touchingTile, TERRA_BASE_RADIUS))
				continue;
			
			// increment touch count
			
			baseRadiusTouchCount++;
			
		}
		
	}

	return baseRadiusTouchCount;

}

/*
Returns all tiles within base radius belonging to friendly base radiuses.
*/
int getBaseRadiusOverlapCount(int x, int y, int factionId)
{
	int baseRadiusOverlapCount = 0;

	for (int offset = 0; offset < BASE_OFFSET_COUNT_RADIUS; offset++)
	{
		MAP *tile = getMapTile(wrap(x + BASE_TILE_OFFSETS[offset][0]), y + BASE_TILE_OFFSETS[offset][1]);

		if (tile == nullptr)
			continue;

		// unclaimed territory
		
		if (!(tile->owner == -1 || tile->owner == factionId))
			continue;
		
		// add overlapped base radius

		if (map_has_item(tile, TERRA_BASE_RADIUS))
		{
			baseRadiusOverlapCount++;
		}

	}

	return baseRadiusOverlapCount;

}

/*
Searches for nearest AI faction base range capable to issue colony to build base at this tile.
*/
int getNearestBaseRange(MAP *tile)
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
		
		if (!isDestinationReachable(vehicleId, x, y, false))
			continue;
		
		nearestColonyRange = std::min(nearestColonyRange, map_range(x, y, vehicle->x, vehicle->y));

	}

	return nearestColonyRange;

}

int getExpansionRange(MAP *tile)
{
	return std::min(getNearestBaseRange(tile), getNearestColonyRange(tile));
}

/*
Searches for nearest AI faction base range capable to issue colony to build base at this tile.
*/
std::pair<int, double> getNearestBaseTravelTime(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	int association = getAssociation(tile, aiFactionId);
	bool gravshipChassisAvailable = has_tech(aiFactionId, Chassis[CHS_GRAVSHIP].preq_tech);
	bool cruiserChassisAvailable = has_tech(aiFactionId, Chassis[CHS_CRUISER].preq_tech);
	bool foilChassisAvailable = has_tech(aiFactionId, Chassis[CHS_FOIL].preq_tech);
	bool hovertankChassisAvailable = has_tech(aiFactionId, Chassis[CHS_HOVERTANK].preq_tech);
	bool speederChassisAvailable = has_tech(aiFactionId, Chassis[CHS_SPEEDER].preq_tech);
	bool infantryChassisAvailable = has_tech(aiFactionId, Chassis[CHS_INFANTRY].preq_tech);
	
	int nearestBaseId = -1;
	double nearestBaseTravelTime = DBL_MAX;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		int baseOceanAssociation = getBaseOceanAssociation(baseId);

		// only corresponding association for ocean if gravship chassis is not available
		
		if (!gravshipChassisAvailable && ocean && baseOceanAssociation != association)
			continue;
		
		// get range
		
		int range = map_range(base->x, base->y, x, y);
		
		// get assumed speed
		
		double speed;
		
		if (gravshipChassisAvailable)
		{
			speed = (double)Chassis[CHS_GRAVSHIP].speed;
		}
		else
		{
			if (ocean)
			{
				if (cruiserChassisAvailable)
				{
					speed = (double)Chassis[CHS_CRUISER].speed;
				}
				else if (foilChassisAvailable)
				{
					speed = (double)Chassis[CHS_FOIL].speed;
				}
				else
				{
					return {-1, DBL_MAX};
				}
				
			}
			else
			{
				if (hovertankChassisAvailable)
				{
					speed = (double)Chassis[CHS_HOVERTANK].speed;
				}
				else if (speederChassisAvailable)
				{
					speed = (double)Chassis[CHS_SPEEDER].speed;
				}
				else if (infantryChassisAvailable)
				{
					speed = (double)Chassis[CHS_INFANTRY].speed;
				}
				else
				{
					return {-1, DBL_MAX};
				}
				
			}
			
		}
		
		// estimate 1.5 speed increase due to roads for land vehicles
		
		if (!gravshipChassisAvailable && !ocean)
		{
			speed *= 1.5;
		}
		
		// zero speed ?
		
		if (speed <= 0.0)
			return {-1, DBL_MAX};
		
		// calculate travel time
		
		double travelTime = (double)range / speed;
		
		// update best values
		
		if (travelTime < nearestBaseTravelTime)
		{
			nearestBaseId = baseId;
			nearestBaseTravelTime = travelTime;
		}
		
	}

	// return zero travel time if not modified
	
	if (nearestBaseTravelTime == DBL_MAX)
	{
		nearestBaseTravelTime = 0.0;
	}

	return {nearestBaseId, nearestBaseTravelTime};
	
}

/*
Searches for nearest AI faction colony and turns to reach this destination.
*/
std::pair<int, double> getNearestColonyTravelTime(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	int association = getAssociation(tile, aiFactionId);
	
	int nearestColonyVehicleId = -1;
	double nearestColonyTravelTime = DBL_MAX;

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
		
		if (!isDestinationReachable(vehicleId, x, y, false))
			continue;
		
		// calculate travel time
		
		double travelTime = getTravelTime(vehicleId, x, y);
		
		// update best values
		
		if (travelTime < nearestColonyTravelTime)
		{
			nearestColonyVehicleId = vehicleId;
			nearestColonyTravelTime = travelTime;
		}
		
	}
	
	// return zero travel time if not modified
	
	if (nearestColonyTravelTime == DBL_MAX)
	{
		nearestColonyTravelTime = 0.0;
	}

	return {nearestColonyVehicleId, nearestColonyTravelTime};

}

double getTravelTime(int vehicleId, int x, int y)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// get range
	
	int range = map_range(vehicle->x, vehicle->y, x, y);
	
	// get vehicle speed
	
	double speed = (double)veh_speed_without_roads(vehicleId);
	
	// estimate 1.5 speed increase due to roads for land vehicles
	
	if (vehicle->triad() == TRIAD_LAND)
	{
		speed *= 1.5;
	}
	
	// zero speed ?
	
	if (speed <= 0.0)
		return DBL_MAX;
	
	// calculate travel time
	
	return (double)range / speed;
	
}

/*
Calculates worker consumption yield score.
This will be subtracted from tile yield score to get net intake.
*/
double getWorkerConsumptionYieldScore()
{
	debug("getWorkerConsumptionYieldScore\n");
	
	int nutrients = Rules->nutrient_intake_req_citizen;
	int minerals = 0;
	int energy = 0;
	
	// calculate yield score
	
	double yieldScore = getYieldScore(nutrients, minerals, energy);
	debug("\tyieldScore=%f\n", yieldScore);
	
	return yieldScore;
	
}

/*
Calculates best terraforming option for barren sea tile and returns its score.
This is a baseline score for the entire game and it assumes that all terraforming technologies are available.
*/
double getSeaSquareFutureYieldScore()
{
	debug("getSeaSquareFutureYieldScore\n");
	
	// find best terraforming option
	
	double bestTerraformingOptionYieldScore = 0.0;

	for (const int mainAction : {FORMER_MINE, FORMER_SOLAR})
	{
		int nutrients = *TERRA_OCEAN_SQ_NUTRIENT;
		int minerals = *TERRA_OCEAN_SQ_MINERALS;
		int energy = *TERRA_OCEAN_SQ_ENERGY;
		
		// kelp farm and corresponding multiplying facility
		
		nutrients += *TERRA_IMPROVED_SEA_NUTRIENT;
		
		if (has_facility_tech(aiFactionId, FAC_AQUAFARM))
		{
			nutrients++;
		}
		
		switch (mainAction)
		{
		// mining platform and corresponding multiplying facility
		
		case FORMER_MINE:
			{
				nutrients += Rules->nutrient_effect_mine_sq;
				minerals += *TERRA_IMPROVED_SEA_MINERALS;
				
				if (has_facility_tech(aiFactionId, FAC_SUBSEA_TRUNKLINE))
				{
					minerals++;
				}
				
				if (has_tech(aiFactionId, Rules->tech_preq_mining_platform_bonus))
				{
					minerals++;
				}
				
			}
			break;
			
		// tidal harness and corresponding multiplying facility
		
		case FORMER_SOLAR:
			if (has_terra(aiFactionId, FORMER_SOLAR, true))
			{
				energy += *TERRA_IMPROVED_SEA_ENERGY;
				
				if (has_facility_tech(aiFactionId, FAC_THERMOCLINE_TRANSDUCER))
				{
					energy++;
				}
				
			}
			break;
			
		}
		
		// calculate yield score
		
		double yieldScore =
			conf.ai_terraforming_nutrientWeight * nutrients
			+
			conf.ai_terraforming_mineralWeight * minerals
			+
			conf.ai_terraforming_energyWeight * energy
		;
		
		debug
		(
			"\tyieldScore=%f\n",
			yieldScore
		)
		;

		// update score
		
		bestTerraformingOptionYieldScore = std::max(bestTerraformingOptionYieldScore, yieldScore);
		
	}
	
	return bestTerraformingOptionYieldScore;
	
}

/*
Finds best terraforming option for given tile and calculates its yield score.
*/
double getTileFutureYieldScore(MAP *tile)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	bool fungus = map_has_item(tile, TERRA_FUNGUS);
	bool rocky = (map_rockiness(tile) == 2);
	
	debug("getTileFutureYieldScore: (%3d,%3d)\n", x, y);
	
	// monolith does not require terraforming
	
	if (map_has_item(tile, TERRA_MONOLITH))
	{
		// calculate score directly and add some bonus score
		
		double monolithFutureYieldScore = getYieldScore(*TERRA_MONOLITH_NUTRIENT, *TERRA_MONOLITH_MINERALS, *TERRA_MONOLITH_ENERGY);
		return monolithFutureYieldScore += 2.0;
		
	}
	
	// get map state
	
	MAP_STATE currentMapState = getMapState(tile);
	
	// find best terraforming option
	
	double bestTerraformingOptionYieldScore = 0.0;

	for (const TERRAFORMING_OPTION &option : CONVENTIONAL_TERRAFORMING_OPTIONS)
	{
		// exclude area effect improvements from future yield estimate
		// they will be factored in for each tile yield computation
		
		if (option.requiredAction == FORMER_CONDENSER || option.requiredAction == FORMER_ECH_MIRROR)
			continue;
		
		// process only correct realm

		if (option.ocean != ocean)
			continue;

		// rocky option is for land rocky tile only

		if (option.rocky && !(!ocean && rocky))
			continue;

		// check if required action is discovered

		if (!(option.requiredAction == -1 || has_terra(aiFactionId, option.requiredAction, ocean)))
			continue;
		
		debug("\t%s\n", option.name);
		
		// initialize variables

		MAP_STATE improvedMapState;
		copyMapState(&improvedMapState, &currentMapState);
		
		// process actions
		
		for(int action : option.actions)
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
		debug("\tyieldScore=%f\n", yieldScore);
		
		// update score
		
		bestTerraformingOptionYieldScore = std::max(bestTerraformingOptionYieldScore, yieldScore);
		
	}
	
	return bestTerraformingOptionYieldScore;
	
}

/*
Calculates weighted resource score for given terraforming action set.
*/
double getTerraformingOptionFutureYieldScore(MAP *tile, MAP_STATE *currentMapState, MAP_STATE *improvedMapState)
{
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	
	// set improved state
	
	setMapState(tile, improvedMapState);
	
	// collect yields
	
	double nutrients = (double)mod_nutrient_yield(aiFactionId, -1, x, y, 0);
	double minerals = (double)mod_mineral_yield(aiFactionId, -1, x, y, 0);
	double energy = (double)mod_energy_yield(aiFactionId, -1, x, y, 0);
	
	// estimate condenser area effect
	
	if (!ocean && map_rockiness(tile) < 2 && map_rainfall(tile) < 2 && has_terra(aiFactionId, FORMER_CONDENSER, ocean))
	{
		nutrients += 0.75;
	}

	// estimate echelon mirror area effect
	
	if (!ocean && map_has_item(tile, TERRA_SOLAR) && has_terra(aiFactionId, FORMER_ECH_MIRROR, ocean))
	{
		energy += 2.0;
	}
	
	// estimate aquatic faction mineral effect
	
	if (ocean && map_elevation(tile) == -1 && isFactionSpecial(aiFactionId, FACT_AQUATIC) && !conf.disable_aquatic_bonus_minerals)
	{
		minerals++;
	}

	// estimate aquafarm effect
	
	if (ocean && map_has_item(tile, TERRA_FARM) && has_facility_tech(aiFactionId, FAC_AQUAFARM))
	{
		nutrients++;
	}

	// estimate subsea trunkline effect
	
	if (ocean && map_has_item(tile, TERRA_MINE) && has_facility_tech(aiFactionId, FAC_SUBSEA_TRUNKLINE))
	{
		minerals++;
	}

	// estimate thermocline transducer effect
	
	if (ocean && map_has_item(tile, TERRA_SOLAR) && has_facility_tech(aiFactionId, FAC_THERMOCLINE_TRANSDUCER))
	{
		energy++;
	}

	// estimate tree farm effect
	
	if (!ocean && map_has_item(tile, TERRA_FOREST) && has_facility_tech(aiFactionId, FAC_TREE_FARM))
	{
		nutrients++;
	}

	// estimate hybrid forest effect
	
	if (!ocean && map_has_item(tile, TERRA_FOREST) && has_facility_tech(aiFactionId, FAC_HYBRID_FOREST))
	{
		nutrients++;
		energy++;
	}

	// restore state
	
	setMapState(tile, currentMapState);
	
	// compute score
	
	return
		conf.ai_terraforming_nutrientWeight * nutrients
		+
		conf.ai_terraforming_mineralWeight * minerals
		+
		conf.ai_terraforming_energyWeight * energy
	;

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

double getYieldScore(double nutrient, double mineral, double energy)
{
	return
		conf.ai_terraforming_nutrientWeight * nutrient
		+
		conf.ai_terraforming_mineralWeight * mineral
		+
		conf.ai_terraforming_energyWeight * energy
	;

}

