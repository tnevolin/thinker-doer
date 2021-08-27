#include <algorithm>
#include "aiColony.h"

int moveColony(int vehicleId)
{
	switch (veh_triad(vehicleId))
	{
	case TRIAD_LAND:
		return moveLandColony(vehicleId);
		break;
	case TRIAD_SEA:
		return moveSeaColony(vehicleId);
		break;
	default:
		return mod_enemy_move(vehicleId);
	}
	
}

int moveLandColony(int vehicleId)
{
	MAP *vehicleLocation = getVehicleMapTile(vehicleId);
	
	// on land
	
	if (!is_ocean(vehicleLocation))
	{
		return moveLandColonyOnLand(vehicleId);
	}
	
	// default to Thinker
	
	return mod_enemy_move(vehicleId);
	
}

int moveSeaColony(int vehicleId)
{
	MAP *vehicleLocation = getVehicleMapTile(vehicleId);
	
	// on ocean
	
	if (is_ocean(vehicleLocation))
	{
		return moveSeaColonyOnOcean(vehicleId);
	}
	
	// default to Thinker
	
	return mod_enemy_move(vehicleId);
	
}

int moveLandColonyOnLand(int vehicleId)
{
	// search for best build site
	
	Location location = findLandBaseBuildLocation(vehicleId);
	
	if (!isValidLocation(location))
		return mod_enemy_move(vehicleId);
	
	// act upon it
	
	if (isVehicleAtLocation(vehicleId, location))
	{
		// remove fungus and rocks if any before building
		
		MAP *tile = getMapTile(location.x, location.y);
		
		tile->items &= ~((uint32_t)TERRA_FUNGUS);
		tile->val3 &= ~((byte)TILE_ROCKY);
		tile->val3 |= (byte)TILE_ROLLING;
		
		// build base
		
		return action_build(vehicleId, 0);
		
	}
	else
	{
		debug("move colony (%3d,%3d) -> (%3d,%3d)\n\n", Vehicles[vehicleId].x, Vehicles[vehicleId].y, location.x, location.y)
		
		// move to site
		
		return set_move_to(vehicleId, location.x, location.y);
		
	}
	
	return mod_enemy_move(vehicleId);
	
}

int moveSeaColonyOnOcean(int vehicleId)
{
	// search for best build site
	
	Location location = findOceanBaseBuildLocation(vehicleId);
	
	if (!isValidLocation(location))
		return mod_enemy_move(vehicleId);
	
	// act upon it
	
	if (isVehicleAtLocation(vehicleId, location))
	{
		return action_build(vehicleId, 0);
	}
	else
	{
		debug("move colony (%3d,%3d) -> (%3d,%3d)\n\n", Vehicles[vehicleId].x, Vehicles[vehicleId].y, location.x, location.y)
		return set_move_to(vehicleId, location.x, location.y);
	}
	
}

/*
Computes build location score for given colony.
*/
double getBuildLocationScore(int colonyVehicleId, int x, int y)
{
	VEH *colonyVehicle = &(Vehicles[colonyVehicleId]);
	int triad = colonyVehicle->triad();
	MAP *colonyVehicleTile = getVehicleMapTile(colonyVehicleId);
	MAP *buildLocationTile = getMapTile(x, y);
	
	// compute placement score
	
	double placementScore = getBuildLocationPlacementScore(colonyVehicle->faction_id, x, y);
	
	// compute range score
	// 10 extra travel turns reduce score in half
	
	int range = map_range(colonyVehicle->x, colonyVehicle->y, x, y);
	int speed = veh_speed_without_roads(colonyVehicleId);
    double turns = (double)range / (double)speed;
    
    if (buildLocationTile->region != colonyVehicleTile->region)
	{
		switch (triad)
		{
		case TRIAD_SEA:
			// sea unit cannot reach another region
			return 0.0;
			break;
		case TRIAD_LAND:
			// reaching another continent requires 2 times more time than traveling on ground
			turns *= 5.0;
			break;
		}
		
	}
	
    double turnsMultiplier = pow(0.5, turns / 20.0);
    
	// summarize the score

    double score = placementScore * turnsMultiplier;

    debug
    (
		"\t(%3d,%3d) placementScore=%5.2f, turns=%5.2f, turnsMultiplier=%5.3f, score=%5.2f\n",
		x,
		y,
		placementScore,
		turns,
		turnsMultiplier,
		score
	)
	;

    return score;

}

/*
Computes build location placement score irrespective to the range to it.
*/
double getBuildLocationPlacementScore(int factionId, int x, int y)
{
	MAP *tile = getMapTile(x, y);
	bool oceanBase = is_ocean(tile);

	// collect tile yield scores

	std::vector<double> tileScores;

	for (MAP_INFO tileInfo : getBaseRadiusTileInfos(x, y, false))
	{
		// ignore claimed tiles

		if (tileInfo.tile->owner != -1 && tileInfo.tile->owner != factionId)
			continue;

		// ignore not owned land tiles for ocean base

		if (oceanBase && !is_ocean(tileInfo.tile) && tileInfo.tile->owner != factionId)
			continue;

		// collect yield score

		tileScores.push_back(getBaseRadiusTileScore(tileInfo));

	}

	// summarize top scores from top down

	std::sort(tileScores.begin(), tileScores.end());
	std::reverse(tileScores.begin(), tileScores.end());

	double score = 0.0;

	double weight = 1.0;
	double weightMultiplier = 0.90;
	for (double tileScore : tileScores)
	{
		score += weight * tileScore;
		weight *= weightMultiplier;
	}

	// discourage border radius intersection

	score -= 1.0 * (double)getFriendlyIntersectedBaseRadiusTileCount(factionId, x, y);

	// encourage land usage

	score += 2.0 * (double)getFriendlyLandBorderedBaseRadiusTileCount(factionId, x, y);

	// prefer coast

	if (isCoast(x, y))
	{
		score += 4.0;
	}

	// return score

	return score;

}

double getBaseRadiusTileScore(MAP_INFO tileInfo)
{
	double score = 0.0;

	double nutrientWeight = 1.0;
	double mineralWeight = 2.0;
	double energyWeight = 1.0;

	// select best weighted yield

	double bestWeightedYield = 0.0;

	if (map_rockiness(tileInfo.tile) == 2)
	{
		// mine

		{
			int actionsCount = 1;
			int actions[] = {FORMER_MINE};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

		// level, farm, mine

		{
			int actionsCount = 3;
			int actions[] = {FORMER_LEVEL_TERRAIN, FORMER_FARM, FORMER_MINE};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

		// level, farm, solar

		{
			int actionsCount = 3;
			int actions[] = {FORMER_LEVEL_TERRAIN, FORMER_FARM, FORMER_SOLAR};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

	}
	else
	{
		// farm, mine

		{
			int actionsCount = 2;
			int actions[] = {FORMER_FARM, FORMER_MINE};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

		// farm, solar

		{
			int actionsCount = 2;
			int actions[] = {FORMER_FARM, FORMER_SOLAR};
			bestWeightedYield = std::max(bestWeightedYield, getImprovedTileWeightedYield(&tileInfo, actionsCount, actions, nutrientWeight, mineralWeight, energyWeight));
		}

	}

	// add best weighted yield to score

	score += bestWeightedYield;

	// discourage base radius overlap

	if (map_has_item(tileInfo.tile, TERRA_BASE_RADIUS))
	{
		score /= 2.0;
	}

	// return score

	return score;

}

/*
Searches for best land base location on all continents.
*/
Location findLandBaseBuildLocation(int colonyVehicleId)
{
	VEH *colonyVehicle = &(Vehicles[colonyVehicleId]);

    debug("findLandBaseBuildLocation - %s (%3d,%3d)\n", MFactions[colonyVehicle->faction_id].noun_faction, colonyVehicle->x, colonyVehicle->y);

	// search best build location

	Location buildLocation;
	double maxScore = 0.0;

	for (int mapTileIndex = 0; mapTileIndex < *map_area_tiles; mapTileIndex++)
	{
		Location location = getMapIndexLocation(mapTileIndex);
		MAP* tile = getMapTile(mapTileIndex);
		
		// limit search area with 20 range
		
		if (map_range(colonyVehicle->x, colonyVehicle->y, location.x, location.y) > 20)
			continue;
		
		// check location
		
		if
		(
			// land region
			isLandRegion(tile->region)
			&&
			// unclaimed territory
			(tile->owner == -1 || tile->owner == colonyVehicle->faction_id)
			&&
			// can build base
			isValidBuildSite(colonyVehicle->faction_id, TRIAD_LAND, location.x, location.y)
			&&
			// only available site
			isUnclaimedBuildSite(colonyVehicle->faction_id, location.x, location.y, colonyVehicleId)
		)
		{
			double score = getBuildLocationScore(colonyVehicleId, location.x, location.y);

			if (score > maxScore)
			{
				buildLocation.set(location.x, location.y);
				maxScore = score;
			}

		}

	}

    debug("\n");

	return buildLocation;

}

/*
Searches for best ocean base location in same region.
*/
Location findOceanBaseBuildLocation(int colonyVehicleId)
{
	VEH *colonyVehicle = &(Vehicles[colonyVehicleId]);
	MAP *colonyVehicleMapTile = getVehicleMapTile(colonyVehicleId);

    debug("findOceanBaseBuildLocation - %s (%3d,%3d)\n", MFactions[colonyVehicle->faction_id].noun_faction, colonyVehicle->x, colonyVehicle->y);

	// search best build location

	Location buildLocation;
	double maxScore = 0.0;

	for (int mapTileIndex = 0; mapTileIndex < *map_area_tiles; mapTileIndex++)
	{
		Location location = getMapIndexLocation(mapTileIndex);
		MAP* tile = getMapTile(mapTileIndex);
		
		// limit search area with 20 range
		
		if (map_range(colonyVehicle->x, colonyVehicle->y, location.x, location.y) > 20)
			continue;
		
		// check location
		
		if
		(
			// same region
			tile->region == colonyVehicleMapTile->region
			&&
			// unclaimed territory
			(tile->owner == -1 || tile->owner == colonyVehicle->faction_id)
			&&
			// can build base
			isValidBuildSite(colonyVehicle->faction_id, TRIAD_SEA, location.x, location.y)
			&&
			// only available site
			isUnclaimedBuildSite(colonyVehicle->faction_id, location.x, location.y, colonyVehicleId)
		)
		{
			double score = getBuildLocationScore(colonyVehicleId, location.x, location.y);

			if (score > maxScore)
			{
				buildLocation.set(location.x, location.y);
				maxScore = score;
			}

		}

	}

    debug("\n");

	return buildLocation;

}

/*
Searches for best land base location for transport with colony.
*/
Location findTransportLandBaseBuildLocation(int transportVehicleId)
{
	VEH *transportVehicle = &(Vehicles[transportVehicleId]);

	// search best build location

	Location buildLocation;
	double maxScore = 0.0;

	for (int mapTileIndex = 0; mapTileIndex < *map_area_tiles; mapTileIndex++)
	{
		Location location = getMapIndexLocation(mapTileIndex);
		MAP* tile = getMapTile(mapTileIndex);
		
		// limit search area with 20 range
		
		if (map_range(transportVehicle->x, transportVehicle->y, location.x, location.y) > 20)
			continue;
		
		// check location
		
		if
		(
			// land region
			isLandRegion(tile->region)
			&&
			// unclaimed territory
			(tile->owner == -1 || tile->owner == transportVehicle->faction_id)
			&&
			// can build base
			isValidBuildSite(transportVehicle->faction_id, TRIAD_LAND, location.x, location.y)
			&&
			// only sites unclaimed by other colonies
			isUnclaimedBuildSite(transportVehicle->faction_id, location.x, location.y, -1)
		)
		{
			double score = getBuildLocationScore(transportVehicleId, location.x, location.y);

			if (score > maxScore)
			{
				buildLocation.set(location.x, location.y);
				maxScore = score;
			}

		}

	}

	return buildLocation;

}

/*
Checks if base can be built in this location.
*/
bool isValidBuildSite(int factionId, int triad, int x, int y)
{
	MAP *tile = getMapTile(x, y);
	
	if (!tile)
		return false;
	
	// no unmatching triad location
	
	if ((triad == TRIAD_LAND && is_ocean(tile)) || (triad == TRIAD_SEA && !is_ocean(tile)))
		return false;
	
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
	
	// cannot build in friend territory
	
	if (tile->owner > 0 && tile->owner != factionId && !at_war(factionId, tile->owner))
		return false;
	
	// no bases closer than allowed spacing
	
	if (nearby_items(x, y, conf.base_spacing - 1, TERRA_BASE_IN_TILE) > 0)
		return false;
	
	// no more than allowed number of bases at minimal range
	
	if (conf.base_nearby_limit >= 0 && nearby_items(x, y, conf.base_spacing, TERRA_BASE_IN_TILE) > conf.base_nearby_limit)
		return false;
	
	return true;
	
}

/*
Checks whether given tile/area is not targeted by other colony.
Given colony is excluded from check if set (>=0).
*/
bool isUnclaimedBuildSite(int factionId, int x, int y, int colonyVehicleId)
{
	for (int otherVehicleId = 0; otherVehicleId < *total_num_vehicles; otherVehicleId++)
	{
		VEH *otherVehicle = &(Vehicles[otherVehicleId]);

		// skip current vehicle

		if (otherVehicleId == colonyVehicleId)
			continue;

		// only this faction

		if (otherVehicle->faction_id != factionId)
			continue;

		// only colony

		if (!isVehicleColony(otherVehicleId))
			continue;

		// detect colony destination
		
		int otherVehicleDestinationX;
		int otherVehicleDestinationY;

		switch (otherVehicle->move_status)
		{
		case ORDER_NONE:
			// destination is location
			otherVehicleDestinationX = otherVehicle->x;
			otherVehicleDestinationY = otherVehicle->y;
			break;
			
		case ORDER_MOVE_TO:
			// destination is waypont
			otherVehicleDestinationX = otherVehicle->waypoint_1_x;
			otherVehicleDestinationY = otherVehicle->waypoint_1_y;
			break;
			
		default:
			// no destination
			continue;
			
		}

		// check build site is not close to someone's destination

		if (map_range(otherVehicleDestinationX, otherVehicleDestinationY, x, y) <= 2)
			return false;

	}

	return true;

}

