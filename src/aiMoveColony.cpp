#include "aiMoveColony.h"
#include <algorithm>
#include "game_wtp.h"
#include "ai.h"
#include "aiMove.h"
#include "terranx_wtp.h"

double averageLandRainfall;
double averageLandRockiness;
double averageLandElevation;
double averageSeaShelf;

void moveColonyStrategy()
{
	// analyze base placement sites
	
	clock_t s = clock();
	analyzeBasePlacementSites();
    debug("(time) [WTP] analyzeBasePlacementSites: %6.3f\n", (double)(clock() - s) / (double)CLOCKS_PER_SEC);
	
}

void analyzeBasePlacementSites()
{
	std::vector<std::pair<int, double>> sortedBuildSiteScores;
	std::unordered_set<int> colonyVehicleIds;
	std::unordered_set<int> accessibleAssociations;
	
	debug("analyzeBasePlacementSites - %s\n", MFactions[aiFactionId].noun_faction);
	
	// calculate accessible associations by current colonies
	
	for (int vehicleId : aiData.vehicleIds)
	{
		// colony
		
		if (!isColonyVehicle(vehicleId))
			continue;
		
		// get vehicle associations
		
		int vehicleAssociation = getVehicleAssociation(vehicleId);
		
		// update accessible associations
		
		accessibleAssociations.insert(vehicleAssociation);
		
	}
	
	// calculate average quality values on overall map
	
	int landTileCount = 0;
	int seaTileCount = 0;
	int sumLandRainfall = 0;
	int sumLandRockiness = 0;
	int sumLandElevation = 0;
	int sumSeaShelf = 0;
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		bool ocean = is_ocean(tile);
		
		if (!ocean)
		{
			landTileCount++;
			sumLandRainfall += map_rainfall(tile);
			sumLandRockiness += map_rockiness(tile);
			sumLandElevation += map_elevation(tile);
		}
		else
		{
			seaTileCount++;
			sumSeaShelf = (map_elevation(tile) == -1 ? 1 : 0);
		}
		
	}
	
	averageLandRainfall = (double)sumLandRainfall / (double)landTileCount;
	averageLandRockiness = (double)sumLandRockiness / (double)landTileCount;
	averageLandElevation = (double)sumLandElevation / (double)landTileCount;
	averageSeaShelf = (double)sumSeaShelf / (double)seaTileCount;
	
	// populate tile yield scores
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		int association = getAssociation(tile, aiFactionId);
		
		// calculate and store expansionRange
		
		int expansionRange = std::min(getNearestBaseRange(mapIndex), getNearestColonyRange(mapIndex));
		
		aiData.mapData[mapIndex].expansionRange = expansionRange;
		
		// limit expansion range
		
		if (expansionRange > MAX_EXPANSION_RANGE + 2)
			continue;
		
		// accessible or reachable
		
		if (!(accessibleAssociations.count(association) != 0 || isReachableAssociation(association, aiFactionId)))
			continue;
		
		// valid work site
		
		if (!isValidWorkSite(tile, aiFactionId))
		{
			aiData.mapData[mapIndex].qualityScore = 0.0;
			continue;
		}
		
		// calculate tileQualityScore
		
		double tileQualityScore = getTileQualityScore(tile);
		
		// store tileQualityScore
		
		aiData.mapData[mapIndex].qualityScore = tileQualityScore;
		
	}
	
	// populate buildSiteScores and set production priorities
	
	aiData.production.landColonyDemand = -INT_MAX;
	aiData.production.seaColonyDemands.clear();
	
	for (int mapIndex = 0; mapIndex < *map_area_tiles; mapIndex++)
	{
		MAP *tile = getMapTile(mapIndex);
		int association = getAssociation(tile, aiFactionId);
		
		// limit expansion range
		
		int expansionRange = std::min(getNearestBaseRange(mapIndex), getNearestColonyRange(mapIndex));
		
		if (expansionRange > MAX_EXPANSION_RANGE)
			continue;
		
		// accessible or reachable
		
		if (!(accessibleAssociations.count(association) != 0 || isReachableAssociation(association, aiFactionId)))
			continue;
		
		// valid build site
		
		if (!isValidBuildSite(tile, aiFactionId))
		{
			aiData.mapData[mapIndex].buildScore = 0.0;
			continue;
		}
		
		// calculate and store the score
		
		double buildSiteScore = getBuildSiteRangeScore(tile);
		aiData.mapData[mapIndex].buildScore = buildSiteScore;
		sortedBuildSiteScores.push_back({mapIndex, buildSiteScore});
		
		// update build priorities
		
		if (!is_ocean(tile))
		{
			aiData.production.landColonyDemand = std::max(aiData.production.landColonyDemand, buildSiteScore);
			
		}
		else
		{
			if (aiData.production.seaColonyDemands.count(association) == 0)
			{
				aiData.production.seaColonyDemands.insert({association, -INT_MAX});
			}
			aiData.production.seaColonyDemands.at(association) = std::max(aiData.production.seaColonyDemands.at(association), buildSiteScore);
			
		}
		
	}
	
	// normalize production priorities
	
	aiData.production.landColonyDemand = std::max(0.0, 1.0 + aiData.production.landColonyDemand);
	for (auto &seaColonyDemandsEntry : aiData.production.seaColonyDemands)
	{
		int association = seaColonyDemandsEntry.first;
		aiData.production.seaColonyDemands.at(association) = std::max(0.0, 1.0 + aiData.production.seaColonyDemands.at(association));
	}
	
	if (DEBUG)
	{
		debug("\tlandColonyDemand = %6.2f\n", aiData.production.landColonyDemand);
		debug("\tseaColonyDemands\n");
		for (const auto &seaColonyDemandsEntry : aiData.production.seaColonyDemands)
		{
			int association = seaColonyDemandsEntry.first;
			double priority = seaColonyDemandsEntry.second;
			
			debug("\t\t%4d = %6.2f\n", association, priority);
			
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
			
			if (range < conf.base_spacing)
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
		int nearestColonyVehicleRange = INT_MAX;
		
		for (int vehicleId : colonyVehicleIds)
		{
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			// proper triad
			
			if (!(vehicle->triad() == TRIAD_AIR || vehicle->triad() == ocean))
				continue;
			
			// can reach
			
			if (!isDestinationReachable(vehicleId, x, y))
				continue;
			
			// get range
			
			int range = map_range(vehicle->x, vehicle->y, x, y);
			
			// update best
			
			if (range < nearestColonyVehicleRange)
			{
				nearestColonyVehicleId = vehicleId;
				nearestColonyVehicleRange = range;
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
	
}

/*
Evaluates build location score based on yield score, base radius overlap and distance to existing bases.
*/
double getBuildSiteRangeScore(MAP *tile)
{
	// compute placement score
	
	double placementScore = getBuildSitePlacementScore(tile);
	
	// reduce placement score with range
	// site score at the edge of max expansion range is reduced by 1
	
	int range = aiData.mapData[getMapIndexByPointer(tile)].expansionRange;
	double speed = (is_ocean(tile) ? 4.0 : 1.5);
	double turns = (double)range / speed;
    double turnPenalty = conf.ai_production_build_turn_penalty * turns;
    
    double score = placementScore - turnPenalty;

//    debug
//    (
//		"\t(%3d,%3d) placementScore=%5.2f, turns=%5.1f, turnsMultiplier=%5.3f, score=%5.2f\n",
//		x,
//		y,
//		placementScore,
//		turns,
//		turnsMultiplier,
//		score
//	)
//	;

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

	// summarize tile yield scores

	std::vector<double> tileScores;

	for (MAP *workTile : getBaseRadiusTiles(tile, false))
	{
		bool workTileOcean = is_ocean(workTile);
		
		// valid work site
		
		if (!isValidWorkSite(workTile, aiFactionId))
			continue;
		
		// ignore not owned land tiles for ocean base

		if (ocean && !workTileOcean && workTile->owner != aiFactionId)
			continue;

		// collect yield score

		tileScores.push_back(aiData.mapData[getMapIndexByPointer(workTile)].qualityScore);
		
	}
	
	// sort scores
	
	std::sort(tileScores.begin(), tileScores.end(), std::greater<int>());
	
	// average weigthed score
	
	double weight = 1.0;
	double weightMultiplier = 0.8;
	double sumWeights = 0.0;
	double sumWeightedScore = 0.0;
	
	for (double tileScore : tileScores)
	{
		double weightedScore = weight * tileScore;
		
		sumWeights += weight;
		sumWeightedScore += weightedScore;
		
		weight *= weightMultiplier;
		
	}
	
	double score = 0.0;
	
	if (sumWeights > 0.0)
	{
		score = sumWeightedScore / sumWeights;
	}
	
	// prefer land coast over land internal tiles
	
	if (!ocean)
	{
		if (isCoast(x, y))
		{
			score += 0.10;
		}
		else
		{
			score -= 0.10;
		}
		
	}
	
	// discourage border radius intersection
	
	score -= 0.10 * (double)getFriendlyIntersectedBaseRadiusTileCount(aiFactionId, x, y);
	
	// encourage land usage
	
	score += 0.20 * (double)getFriendlyLandBorderedBaseRadiusTileCount(aiFactionId, x, y);
	
	// return score
	
	return score;
	
}

/*
Evaluates tile quality score based on how territory is more beneficial than average.
* resource/landmark bonus
* rainfall 
* rockiness
* altitude
*/
double getTileQualityScore(MAP *tile)
{
	bool ocean = is_ocean(tile);
	
	double score = 0.0;
	
	// bonus
	
	int nutrientBonus = getNutrientBonus(tile);
	int mineralBonus = getMineralBonus(tile);
	int energyBonus = getEnergyBonus(tile);
	
	if (!ocean)
	{
		if (map_has_item(tile, TERRA_MONOLITH))
		{
			score +=
				conf.ai_production_build_weight_nutrient_bonus * (double)nutrientBonus
				+
				conf.ai_production_build_weight_mineral_bonus * (double)mineralBonus
				+
				conf.ai_production_build_weight_energy_bonus * (double)energyBonus
				+
				conf.ai_production_build_weight_rainfall * 0.0
				+
				conf.ai_production_build_weight_rockiness * 1.0
				+
				conf.ai_production_build_weight_elevation * 1.0
			;
			
		}
		else
		{
			// quality
			
			double rainfall = (double)map_rainfall(tile) - averageLandRainfall;
			double rockiness = (double)map_rockiness(tile) - averageLandRockiness;
			double elevation = (double)map_elevation(tile) - averageLandElevation;
			
			if (map_rockiness(tile) == 2)
			{
				double rockyMineYieldScore =
					conf.ai_production_build_weight_nutrient_bonus * (double)nutrientBonus
					+
					conf.ai_production_build_weight_mineral_bonus * (double)(mineralBonus == 0 ? 0 : mineralBonus + 1)
					+
					conf.ai_production_build_weight_energy_bonus * (double)energyBonus
					+
					conf.ai_production_build_weight_rockiness * rockiness
				;
				
				double solarYieldScore =
					conf.ai_production_build_weight_nutrient_bonus * (double)nutrientBonus
					+
					conf.ai_production_build_weight_mineral_bonus * (double)mineralBonus
					+
					conf.ai_production_build_weight_energy_bonus * (double)energyBonus
					+
					conf.ai_production_build_weight_rainfall * rainfall
					+
					conf.ai_production_build_weight_rockiness * (rockiness - 1.0)
					+
					conf.ai_production_build_weight_elevation * elevation
				;
				
				score += std::max(rockyMineYieldScore, solarYieldScore);
				
			}
			else
			{
				double mineYieldScore =
					conf.ai_production_build_weight_nutrient_bonus * (double)nutrientBonus
					+
					conf.ai_production_build_weight_mineral_bonus * (double)(mineralBonus == 0 ? 0 : mineralBonus + 1)
					+
					conf.ai_production_build_weight_energy_bonus * (double)energyBonus
					+
					conf.ai_production_build_weight_rainfall * (rainfall + Rules->nutrient_effect_mine_sq)
					+
					conf.ai_production_build_weight_rockiness * rockiness
				;
				
				double solarYieldScore =
					conf.ai_production_build_weight_nutrient_bonus * (double)nutrientBonus
					+
					conf.ai_production_build_weight_mineral_bonus * (double)mineralBonus
					+
					conf.ai_production_build_weight_energy_bonus * (double)energyBonus
					+
					conf.ai_production_build_weight_rainfall * rainfall
					+
					conf.ai_production_build_weight_rockiness * rockiness
					+
					conf.ai_production_build_weight_elevation * elevation
				;
				
				score += std::max(mineYieldScore, solarYieldScore);
				
			}
			
		}
		
		// add extra energy bonus for river
		
		if (map_has_item(tile, TERRA_RIVER))
		{
			score += conf.ai_production_build_weight_elevation;
		}
		
		// mine adds to mineral bonus but reduces nutrients
		
		double mineEffectOnMineralBonus = 0.0;
		
		if (mineralBonus > 0)
		{
			mineEffectOnMineralBonus =
				conf.ai_production_build_weight_rainfall * (double)Rules->nutrient_effect_mine_sq
				+
				conf.ai_production_build_weight_mineral_bonus * 1.0
			;
				
		}
		
		mineEffectOnMineralBonus = std::max(0.0, mineEffectOnMineralBonus);
		
		score += mineEffectOnMineralBonus;
		
	}
	else
	{
		score +=
			conf.ai_production_build_weight_nutrient_bonus * (double)nutrientBonus
			+
			conf.ai_production_build_weight_mineral_bonus * (double)mineralBonus
			+
			conf.ai_production_build_weight_energy_bonus * (double)energyBonus
		;
		
		// devalue deep ocean
		
		if (map_elevation(tile) < -1)
		{
			score += conf.ai_production_build_weight_deep;
		}
		
	}
	
	// return score
	
	return score;
	
}

bool isValidBuildSite(MAP *tile, int factionId)
{
	int x = getX(tile);
	int y = getY(tile);
	
	// cannot build at volcano
	
	if (tile->landmarks & LM_VOLCANO && tile->art_ref_id == 0)
		return false;
	
	// cannot build in base tile and on monolith
	
	if (map_has_item(tile, TERRA_BASE_IN_TILE | TERRA_MONOLITH))
		return false;
	
	// no rocky tile unless allowed by configuration
	
	if (!is_ocean(tile) && tile->is_rocky() && !conf.ai_base_allowed_fungus_rocky)
		return false;
	
	// no fungus tile unless allowed by configuration
	
	if (map_has_item(tile, TERRA_FUNGUS) && !conf.ai_base_allowed_fungus_rocky)
		return false;
	
	// allowed territory
	
	if (!(tile->owner == -1 || tile->owner == factionId))
		return false;
	
	// no bases closer than allowed spacing
	
	if (nearby_items(x, y, conf.base_spacing - 1, TERRA_BASE_IN_TILE) > 0)
		return false;
	
	// no more than allowed number of bases at minimal range
	
	if (conf.base_nearby_limit >= 0 && nearby_items(x, y, conf.base_spacing, TERRA_BASE_IN_TILE) > conf.base_nearby_limit)
		return false;
	
	// not occupied by unfrendly vehicles
	
	int otherVehicleId = veh_at(x, y);
	
	if (otherVehicleId >= 0)
	{
		VEH *otherVehicle = &(Vehicles[otherVehicleId]);
		
		if (!(otherVehicle->faction_id == factionId || has_pact(otherVehicle->faction_id, factionId)))
			return false;
		
	}
	
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

