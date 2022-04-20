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
std::vector<MAP *> buildSites;

double minimalYieldScore;
double seaSquareFutureYieldScore;

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
	
	s = clock();
	analyzeBasePlacementSites();
    debug("(time) [WTP] analyzeBasePlacementSites: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
}

void analyzeBasePlacementSites()
{
//	std::vector<std::pair<int, double>> sortedBuildSiteScores;
	std::set<int> colonyVehicleIds;
	std::set<int> accessibleAssociations;
	
	debug("analyzeBasePlacementSites - %s\n", MFactions[aiFactionId].noun_faction);
	
	// find value for best sea tile
	// it'll be used as a scale for yield quality evaluation
	
	minimalYieldScore = getMinimalYieldScore();
	seaSquareFutureYieldScore = getSeaSquareFutureYieldScore();
	
	// calculate accessible associations by current colonies
	// populate colonyLocations
	
	s = clock();
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
    debug("(time) [WTP] analyzeBasePlacementSites.accessibleAssociationsByColonies: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
	// calculate accessible associations by bases
	
	s = clock();
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
    debug("(time) [WTP] analyzeBasePlacementSites.accessibleAssociationsByBases: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
	// populate tile yield scores
	
	s = clock();
	clock_t s1 = 0;
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
		
		double yieldScore = (getTileFutureYieldScore(tile) - minimalYieldScore) / std::max(1.0, seaSquareFutureYieldScore - minimalYieldScore);
		
		// reduce yield score for tiles withing base radius
		
		if (map_has_item(tile, TERRA_BASE_RADIUS))
		{
			yieldScore *= 0.75;
		}
		
		debug("yieldScore(%3d,%3d)=%f\n", getX(tile), getY(tile), yieldScore);
		
		// store yieldScore
		
		getExpansionTileInfo(mapIndex)->yieldScore = yieldScore;
		
	}
	debug("(time) [WTP] analyzeBasePlacementSites.tileYieldScores.travel time: %6.3f\n", (double)(s1) / (double)CLOCKS_PER_SEC);
    debug("(time) [WTP] analyzeBasePlacementSites.tileYieldScores: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
	// populate buildSiteScores
	
	aiData.production.landColonyDemand = -INT_MAX;
	aiData.production.seaColonyDemands.clear();
	
	s = clock();
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

		// valid build site
		
		if (!isValidBuildSite(tile, aiFactionId))
		{
			getExpansionTileInfo(mapIndex)->buildScore = 0.0;
			continue;
		}
		
		// add to buildSites
		
		buildSites.push_back(tile);
		
		// calculate basic scores
		
		double yieldScore = getBuildSiteYieldScore(tile);
		double placementScore = getBuildSitePlacementScore(tile);
		
		// update build site score
		
		double buildScore = yieldScore + placementScore;
		expansionTileInfo->buildScore = buildScore;
		
	}
    debug("(time) [WTP] analyzeBasePlacementSites.buildSiteScores: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
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
	
    // distribute colonies to build sites
    
	debug("distribute colonies to build sites - %s\n", MFactions[aiFactionId].noun_faction);
	
	std::set<MAP *> unavailableBuildSites;
	
	s = clock();
	for (int vehicleId : aiData.colonyVehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int vehicleAssociation = getVehicleAssociation(vehicleId);
		
		debug("\t(%3d,%3d) %-32s\n", vehicle->x, vehicle->y, Units[vehicle->unit_id].name);
		
		// find best buildSite
		
		MAP *bestBuildSite = nullptr;
		double bestBuildSiteScore = 0.0;
		
		for (MAP *tile : buildSites)
		{
			bool ocean = is_ocean(tile);
			int association = getAssociation(tile, aiFactionId);
			int x = getX(tile);
			int y = getY(tile);
			ExpansionTileInfo *expansionTileInfo = getExpansionTileInfo(tile);
			
			// exclude unavailableBuildSite
			
			if (unavailableBuildSites.count(tile) != 0)
				continue;
			
			// exclude unmatching realm
			
			if (triad == TRIAD_LAND && ocean || triad == TRIAD_SEA && !ocean)
				continue;
			
			// exclude other ocean associations for sea unit
			
			if (triad == TRIAD_SEA && ocean && association != vehicleAssociation)
				continue;
			
			// get travel time
			
			double travelTime = estimateTravelTime(vehicleTile, tile, vehicle->unit_id);
			
			// get travel time score
			
			double travelTimeScore = getBuildSiteTravelTimeScore(travelTime);
			
			// get combined score
			
			double buildSiteScore = expansionTileInfo->buildScore + travelTimeScore;
			
			debug("\t\t(%3d,%3d) buildSiteScore=%5.2f, expansionTileInfo->buildScore=%5.2f, travelTime=%5.2f, travelTimeScore=%5.2f\n", x, y, buildSiteScore, expansionTileInfo->buildScore, travelTime, travelTimeScore);
			
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
		
		transitVehicle(vehicleId, Task(vehicleId, BUILD, bestBuildSite));
		
		// disable nearby build sites
		
		std::vector<MAP *> thisSiteUnavailableBuildSites = getUnavailableBuildSites(bestBuildSite);
		unavailableBuildSites.insert(thisSiteUnavailableBuildSites.begin(), thisSiteUnavailableBuildSites.end());
		
	}
    debug("(time) [WTP] analyzeBasePlacementSites.select: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
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
	double weightMultiplier = 0.8;
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
	
	// calculate travel time score
	
    double score = - turnPenaltyCoefficient * travelTime;
    
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
	
	// explicitly discourage placing base on map edge
	
	int edgeDistanceY = std::min(y, (*map_axis_y - 1) - y);
	int edgeDistanceX = (*map_toggle_flat ? std::min(x, (*map_axis_x - 1) - x) : *map_axis_x);
	int edgeDistance = std::min(edgeDistanceX, edgeDistanceY);
	
	if (edgeDistance < 2)
	{
		score += -0.3 * (2 - edgeDistance);
	}
	debug("\t+edge=%+f\n", score);
	
	// explicitly discourage placing base on some landmarks
	
	if
	(
		map_has_landmark(tile, LM_CRATER)
		||
		map_has_landmark(tile, LM_VOLCANO)
	)
	{
		score += -1.0;
	}
	
	// explicitly discourage placing base on land fungus
	
	if (!is_ocean(tile) && map_has_item(tile, TERRA_FUNGUS))
	{
		score += -0.4;
	}
	
	// return score
	
	return score;
	
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
	// available territory or adjacent to available territory (potentially available)
	
	bool available = (tile->owner == -1 || tile->owner == factionId);
	
	if (!available)
	{
		for (MAP *adjacentTile : getAdjacentTiles(tile, false))
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
Estimate travel time by straight line including transporting.
*/
double estimateTravelTime(MAP *src, MAP *dst, int unitId)
{
	assert(src != nullptr);
	assert(dst != nullptr);
	assert(unitId >= 0 && unitId < MaxProtoNum);
	
	int srcX = getX(src);
	int srcY = getY(src);
	int dstX = getX(dst);
	int dstY = getY(dst);
	int srcAssociation = getAssociation(src, aiFactionId);
	int dstAssociation = getAssociation(dst, aiFactionId);
	UNIT *unit = &(Units[unitId]);
	int triad = unit->triad();
	int chassisSpeed = unit->speed();
	
	double travelTime = DBL_MAX;
	
	if (triad == TRIAD_SEA || triad == TRIAD_AIR)
	{
		// get range
		
		int range = map_range(srcX, srcY, dstX, dstY);
		
		// estimate travel time
		
		travelTime = (double)range / (double)chassisSpeed;
		
	}
	else if (triad == TRIAD_LAND)
	{
		// get range
		
		int range = map_range(srcX, srcY, dstX, dstY);
		
		// estimate travel time
		
		travelTime = (double)range / (double)chassisSpeed;
		
		// penalize land vehicle for traveling to other association
		
		if (dstAssociation != srcAssociation)
		{
			travelTime *= 2;
		}
		
	}
	
	return travelTime;
	
}

double estimateVehicleTravelTime(int vehicleId, MAP *destination)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	
	return estimateTravelTime(vehicleTile, destination, vehicle->unit_id);
	
}

/*
Calculates minimal yield score to compare to actual one.
This will be subtracted from tile yield score to get extra intake.
*/
double getMinimalYieldScore()
{
	debug("getMinimalYieldScore\n");
	
	int nutrients = *TERRA_BASE_SQ_NUTRIENT;
	int minerals = *TERRA_BASE_SQ_MINERALS;
	int energy = *TERRA_BASE_SQ_ENERGY;
//	int nutrients = Rules->nutrient_intake_req_citizen;
//	int minerals = 0;
//	int energy = 0;
	
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
		return monolithFutureYieldScore += 0.0;
		
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
	
	double nutrientYield = (double)mod_nutrient_yield(aiFactionId, -1, x, y, 0);
	double mineralYield = (double)mod_mineral_yield(aiFactionId, -1, x, y, 0);
	double energyYield = (double)mod_energy_yield(aiFactionId, -1, x, y, 0);
	double nutrientBonusYield = (double)getNutrientBonus(tile);
	double mineralBonusYield = (double)getMineralBonus(tile);
	double energyBonusYield = (double)getEnergyBonus(tile);
	
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
	
	setMapState(tile, currentMapState);
	
	// compute score
	
	return
		conf.ai_expansion_regular_weight_nutirent * nutrientYield
		+
		conf.ai_expansion_regular_weight_mineral * mineralYield
		+
		conf.ai_expansion_regular_weight_energy * energyYield
		+
		conf.ai_expansion_bonus_extra_weight_nutirent * nutrientBonusYield
		+
		conf.ai_expansion_bonus_extra_weight_mineral * mineralBonusYield
		+
		conf.ai_expansion_bonus_extra_weight_energy * energyBonusYield
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

/*
Returns unavailable build sites around already taken one.
*/
std::vector<MAP *> getUnavailableBuildSites(MAP *buildSite)
{
	int buildSiteX = getX(buildSite);
	int buildSiteY = getY(buildSite);
	
	std::vector<MAP *> unavailableBuildSites;
	
	// add radius 0-2 tiles
	
	std::vector<MAP *> radius2Tiles = getRangeTiles(buildSiteX, buildSiteY, 2, true);
	unavailableBuildSites.insert(unavailableBuildSites.end(), radius2Tiles.begin(), radius2Tiles.end());
	
	// add radius 3 tiles without corners
	
	for (MAP *tile : getEqualRangeTiles(buildSiteX, buildSiteY, 3))
	{
		int x = getX(tile);
		int y = getY(tile);
		
		// exclude corners
		
		if (x == buildSiteX || y == buildSiteY)
			continue;
		
		// add to list
		
		unavailableBuildSites.push_back(tile);
		
	}
	
	return unavailableBuildSites;
	
}

