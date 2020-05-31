#include <float.h>
#include <math.h>
#include "ai.h"

// terraforming choice optimization parameters

// TODO use breakpoint time instead
const double decayLifetime = 80.0;
const double mineralWeight = 1.0;
const double energyWeight = 3.0 / 8.0;

/**
AI terraforming options
*/
const int TERRAFORMING_OPTION_COUNT = 15;
TERRAFORMING_OPTION TERRAFORMING_OPTIONS[TERRAFORMING_OPTION_COUNT] =
{
	// land
	{false, true, true, FORMER_FARM, 0.0},
	{false, true, true, FORMER_SOIL_ENR, 0.0},
	{false, true, false, FORMER_MINE, 0.0},
	{false, true, false, FORMER_SOLAR, 0.0},
	{false, false, false, FORMER_CONDENSER, 0.0},
	{false, false, false, FORMER_ECH_MIRROR, 0.0},
	{false, false, false, FORMER_THERMAL_BORE, 0.0},
	{false, false, false, FORMER_FOREST, 0.0},
	{false, false, false, FORMER_PLANT_FUNGUS, 0.0},
	{false, false, false, FORMER_ROAD, 1.0},
	{false, false, false, FORMER_MAG_TUBE, 1.0},
	{false, false, false, FORMER_SENSOR, 0.1},
	// sea
	{true, true, false, FORMER_FARM, 0.0},
	{true, true, false, FORMER_MINE, 0.0},
	{true, true, false, FORMER_SOLAR, 0.0},
};

/**
Gives order to former.
*/
int giveOrderToFormer(int vehicleId)
{
	// variables

	VEH *vehicle = &tx_vehicles[vehicleId];
	int vehicleTriad = veh_triad(vehicleId);
	MAP *vehicleTile = mapsq(vehicle->x, vehicle->y);
	int vehicleBodyId = vehicleTile->body_id;
	int factionId = vehicle->faction_id;

	// set default return value

	int order = SYNC;

	// skip currently terraforming vehicles

	if (isVehicleTerraforming(vehicle))
		return order;

	// iterate through map tiles

	double bestTerraformingScore = -DBL_MAX;
	int bestTerraformingTileX = -1;
	int bestTerraformingTileY = -1;
	MAP *bestTerraformingTile = NULL;
	int bestTerraformingAction = -1;

	for (int y = 0; y < *tx_map_axis_y; y++)
	{
		for (int x = 0; x < *tx_map_axis_x; x++)
		{
			// skip impossible combinations

			if (x % 2 != y % 2)
				continue;

			// get map tile

			MAP *tile = mapsq(x, y);

			// skip incorrect map tiles

			if (!tile)
				continue;

			// skip unreachable territory

			if (vehicleTriad != TRIAD_AIR && tile->body_id != vehicleBodyId)
				continue;

			// skip not own tiles

			if (tile->owner != factionId)
				continue;

			// skip not our work tiles

			if (!isOwnWorkTile(factionId, x, y))
				continue;

			// exclude already taken tiles

			if (isTileTakenByOtherFormer(vehicle, tile))
				continue;

			// calculate terraforming score

			int terraformingAction = -1;
			double terraformingScore = calculateTerraformingScore(vehicleId, x, y, &terraformingAction);

			debug
			(
				"calculateTerraformingScore: (%d,%d), terraformingScore=%f, terraformingAction=%d\n",
				x,
				y,
				terraformingScore,
				terraformingAction
			)

			if (terraformingScore > bestTerraformingScore )
			{
				bestTerraformingScore = terraformingScore;
				bestTerraformingTileX = x;
				bestTerraformingTileY = y;
				bestTerraformingTile = tile;
				bestTerraformingAction = terraformingAction;

			}

			debug
			(
				"calculateBestTerraformingScore: (%d,%d), bestTerraformingScore=%f, bestTerraformingAction=%d\n",
				bestTerraformingTileX,
				bestTerraformingTileY,
				bestTerraformingScore,
				bestTerraformingAction
			)

		}

	}

	// could not find good terraforming option
	if (bestTerraformingTile == NULL || bestTerraformingAction == -1)
	{
		// build improvement

		order = buildImprovement(vehicleId);

	}
	// found good terraforming option
	else
	{
		// at destination
		if (bestTerraformingTile == vehicleTile)
		{
			// execute terraforming action

			order = setTerraformingAction(vehicleId, bestTerraformingAction);

			debug
			(
				"giveOrderToFormer: (%d,%d), action=%s\n",
				vehicle->x,
				vehicle->y,
				tx_terraform[bestTerraformingAction].name
			)
			;

		}
		// not at destination
		else
		{
			// send former to destination

			order = set_move_to(vehicleId, bestTerraformingTileX, bestTerraformingTileY);

			debug
			(
				"giveOrderToFormer: (%d,%d), move_to(%d,%d)\n",
				vehicle->x,
				vehicle->y,
				bestTerraformingTileX,
				bestTerraformingTileY
			)

		}

	}

	return order;

}

/**
Selects best terraforming option for given square and calculates its improvement score after terraforming.
Returns improvement score and modifies bestTerraformingOptionIndex reference.
*/
double calculateTerraformingScore(int vehicleId, int x, int y, int *bestTerraformingAction)
{
	VEH *vehicle = &tx_vehicles[vehicleId];
	int factionId = vehicle->faction_id;
	int triad = veh_triad(vehicleId);
	MAP *tile = mapsq(x, y);
	bool sea = is_ocean(tile);

	// calculate previous yield score

	double previousYieldScore = calculateYieldScore(factionId, x, y);

	// store tile values

	const int tileRocks = tile->rocks;
	const int tileItems = tile->items;

	// running variables

	int improvedTileRocks;
	int improvedTileItems;
	int terraformingTime;
	int lostTerraformingTime;
	int action;

	// find best terraforming option

	double bestTerrafromningScore = -DBL_MAX;
	*bestTerraformingAction = -1;

	for (int optionIndex = 0; optionIndex < TERRAFORMING_OPTION_COUNT; optionIndex++)
	{
		TERRAFORMING_OPTION option = TERRAFORMING_OPTIONS[optionIndex];

		// skip wrong body type

		if (option.sea != sea)
			continue;

		// initialize variables

		improvedTileRocks = tileRocks;
		improvedTileItems = tileItems;
		terraformingTime = 0;
		lostTerraformingTime = 0;

		// skip unavailable actions

		if (!isTerraformingAvailable(factionId, x, y, option.action))
			continue;

		// process actions

		int firstAction = -1;

		// remove fungus if needed

		if (tile->items & TERRA_FUNGUS)
		{
			// fungus build technology is different for road and improvements

			int fungusBuildTechnology = (option.action == FORMER_ROAD ? tx_basic->tech_preq_build_road_fungus : tx_basic->tech_preq_improv_fungus);

			if (option.removeFungus || !has_tech(factionId, fungusBuildTechnology))
			{
				action = FORMER_REMOVE_FUNGUS;

				// compute terraforming time

				terraformingTime += calculateTerraformingTime(vehicleId, action);

				// generate terraforming change

				generateTerraformingChange(&improvedTileRocks, &improvedTileItems, action);

				// store first action

				if (firstAction == -1)
				{
					firstAction = action;
				}

			}

		}

		// level terrain if needed

		if (tile->rocks & TILE_ROCKY)
		{
			if (option.levelTerrain)
			{
				action = FORMER_LEVEL_TERRAIN;

				// compute terraforming time

				terraformingTime += calculateTerraformingTime(vehicleId, action);

				// generate terraforming change

				generateTerraformingChange(&improvedTileRocks, &improvedTileItems, action);

				// store first action

				if (firstAction == -1)
				{
					firstAction = action;
				}

			}

		}

		// execute main action

		action = option.action;

		// compute terraforming time

		terraformingTime += calculateTerraformingTime(vehicleId, action);

		// generate terraforming change

		generateTerraformingChange(&improvedTileRocks, &improvedTileItems, action);

		// store first action

		if (firstAction == -1)
		{
			firstAction = action;
		}

		// set changes

		tile->rocks = improvedTileRocks;
		tile->items = improvedTileItems;

		// calculate lost terraforming time

		lostTerraformingTime = calculateLostTerraformingTime(vehicleId, tileItems, improvedTileItems);

		// test

		double improvedYieldScore = calculateYieldScore(factionId, x, y);

		// revert changes

		tile->rocks = tileRocks;
		tile->items = tileItems;

		// calculate yield improvement score

		double improvementScore = (improvedYieldScore - previousYieldScore);

		// update improvement score with bonus

		improvementScore += decayLifetime * option.bonusValue;

		// calculate path length

		double travelPathLength = (double)map_range(vehicle->x, vehicle->y, x, y);

		// estimate movement rate along the path

		double estimatedMovementCost;
        double movementCostWithoutRoad = (double)tx_basic->mov_rate_along_roads;
        double movementCostAlongRoad = (conf.tube_movement_rate_multiplier > 0 ? (double)tx_basic->mov_rate_along_roads / conf.tube_movement_rate_multiplier : (double)tx_basic->mov_rate_along_roads);
        double movementCostAlongTube = (conf.tube_movement_rate_multiplier > 0 ? 1 : 0);
        double currentTurn = (double)*tx_current_turn;

		switch (triad)
		{
		case TRIAD_LAND:

			// approximating 2 turns per tile at the beginning and then decrease movement close to the middle-end.

            if (*tx_current_turn < 100)
            {
                estimatedMovementCost = 2.0 * movementCostWithoutRoad - (2.0 * movementCostWithoutRoad - 1.0 * movementCostWithoutRoad) * ((currentTurn - 0.0) / (100.0 - 0.0));
            }
            else if (*tx_current_turn < 200)
            {
                estimatedMovementCost = 1.0 * movementCostWithoutRoad - (1.0 * movementCostWithoutRoad - movementCostAlongRoad) * ((currentTurn - 100.0) / (200.0 - 100.0));
            }
            else if (*tx_current_turn < 400)
            {
                estimatedMovementCost = 1.0 * movementCostAlongRoad - (1.0 * movementCostAlongRoad - 1.0 * movementCostAlongTube) * ((currentTurn - 200.0) / (400.0 - 200.0));
            }
            else
            {
                estimatedMovementCost = movementCostAlongTube;
            }

            break;

		default:

			// no roads for sea and air units

			estimatedMovementCost = movementCostWithoutRoad;

		}

        // estimate travel time

        double estimatedTravelTime = travelPathLength * estimatedMovementCost / veh_speed(vehicleId);

        // summarize total time penalty

        double totalTimePenalty = estimatedTravelTime + (double)terraformingTime + (double)lostTerraformingTime;

		// factor improvement score with decay coefficient

		double terraformingScore = improvementScore * exp(-totalTimePenalty / decayLifetime);

        debug
        (
            "calculateTerraformingOptionScore: (%d,%d), optionIndex=%d, improvementScore=%f, decayLifetime=%f, estimatedTravelTime=%f, terraformingTime=%d, lostTerraformingTime=%d, terraformingScore=%f\n",
            x,
            y,
            optionIndex,
            improvementScore,
            decayLifetime,
            estimatedTravelTime,
            terraformingTime,
            lostTerraformingTime,
            terraformingScore
        )
        ;

		// update score

		if (terraformingScore > bestTerrafromningScore)
		{
			bestTerrafromningScore = terraformingScore;
			*bestTerraformingAction = firstAction;

		}

	}

	// return

	return bestTerrafromningScore;

}

double calculateYieldScore(int factionId, int x, int y)
{
	double yieldScore = 0.0;

	// iterate bases

	for (int baseIndex = 0; baseIndex < *tx_total_num_bases; baseIndex++)
	{
		BASE *base = &tx_bases[baseIndex];

		// skip not own bases

		if (base->faction_id != factionId)
			continue;

		// calculate distance

		int absDx = abs(base->x - x);
		int absDy = abs(base->y - y);

		// skip too far bases

		if ((absDx + absDy > 4) || (absDx >= 4) || (absDy >= 4))
			continue;

		// set current base

        tx_set_base(baseIndex);

        // store current worked tiles

        int currentWorkedTiles = base->worked_tiles;

        // clear working tiles

        base->worked_tiles = 0;

		// compute base

		computeBase(baseIndex);

		// calculate current yield

		double yield = mineralWeight * (double)base->mineral_surplus + energyWeight * (double)base->energy_surplus;

		// calculate population growth and yield growth

		double absolutePopulationGrowth = (double)base->nutrient_surplus / (double)((base->pop_size + 1) * 10);
		double relativePopulationGrowth = absolutePopulationGrowth / (double)(base->pop_size + 1);
		double yieldGrowth = yield * relativePopulationGrowth;

		// calculate base yield score

		double baseYieldScore = decayLifetime * yield + decayLifetime * decayLifetime * yieldGrowth;

		// restore worked tiles

        base->worked_tiles = currentWorkedTiles;

		debug
		(
			"calculateBaseYieldScore: (%d,%d), base(%d,%d), pop_size=%d, nutrient_surplus=%d, mineral_surplus=%d, energy_surplus=%d, baseYieldScore=%f\n",
			x,
			y,
			base->x,
			base->y,
			base->pop_size,
			base->nutrient_surplus,
			base->mineral_surplus,
			base->energy_surplus,
			baseYieldScore
		)
		;

		// accumulate yield score

		yieldScore += baseYieldScore;

    }

    // add value for not tangible assets

	debug
	(
		"calculateYieldScore: (%d,%d), yieldScore=%f\n",
		x,
		y,
		yieldScore
	)
	;

	return yieldScore;

}

/**
Checks whether tile is own work tile but not base square.
*/
bool isOwnWorkTile(int factionId, int x, int y)
{
	bool ownWorkTile = false;

	for (int baseIndex = 0; baseIndex < *tx_total_num_bases; baseIndex++)
	{
		BASE *base = &tx_bases[baseIndex];

		// skip not own bases

		if (base->faction_id != factionId)
			continue;

		// calculate distance

		int absDx = abs(base->x - x);
		int absDy = abs(base->y - y);

		// skip base in tile

		if (absDx == 0 && absDy == 0)
			continue;

		// skip too far bases

		if ((absDx + absDy > 4) || absDx >= 4 || absDy >= 4)
			continue;

		// return true

		ownWorkTile = true;
		break;

	}

	return ownWorkTile;

}

/**
Checks whether terraforming is available.
Returns true if corresponding technology exists and tile doesn't contain this improvement yet.
*/
bool isTerraformingAvailable(int factionId, int x, int y, int action)
{
	MAP *tile = mapsq(x, y);

	// action is available only when enabled and researched

	if (!has_terra(factionId, action, is_ocean(tile)))
		return false;

	// terraforming is not available in base square

	if (tile->items & TERRA_BASE_IN_TILE)
		return false;

	// check if improvement already exists

	if (tile->items & tx_terraform[action].added_items_flag)
		return false;

	// special cases

	bool terraformingAvailable = true;

	switch (action)
	{
	case FORMER_FARM:
		// farm cannot be placed in rocky area
		terraformingAvailable = ~tile->rocks & TILE_ROCKY;
		break;
	case FORMER_SOIL_ENR:
		// enricher requires farm
		terraformingAvailable = tile->items & TERRA_FARM;
		break;
	case FORMER_MAG_TUBE:
		// tube requires road
		terraformingAvailable = tile->items & TERRA_ROAD;
		break;
	case FORMER_REMOVE_FUNGUS:
		// removing fungus requires fungus
		terraformingAvailable = tile->items & TERRA_FUNGUS;
		break;
	case FORMER_THERMAL_BORE:
		// borehole should not be built on slope
		// borehole should not be adjacent to another borehole
		// TODO
		break;
	case FORMER_AQUIFER:
		// aquifier cannot be drilled next to river
		// TODO
		break;
	case FORMER_RAISE_LAND:
		// raise land should not disturb other faction bases
		// TODO
		break;
	case FORMER_LOWER_LAND:
		// lower land should not disturb other faction bases
		// TODO
		break;
	case FORMER_LEVEL_TERRAIN:
		// level only rocky area (specifically for AI terraforming purposes)
		terraformingAvailable = tile->rocks & TILE_ROCKY;
		break;
	}

	return terraformingAvailable;

}

int calculateTerraformingTime(int vehicleId, int action)
{
	return tx_action_terraform(vehicleId, action, 0) + 1;

}

void generateTerraformingChange(int *improvedTileRocks, int *improvedTileItems, int action)
{
	// level terrain changes rockiness
	if (action == FORMER_LEVEL_TERRAIN)
	{
		if (*improvedTileRocks & TILE_ROCKY)
		{
			*improvedTileRocks &= ~TILE_ROCKY;
			*improvedTileRocks |= TILE_ROLLING;
		}
		else if (*improvedTileRocks & TILE_ROLLING)
		{
			*improvedTileRocks &= ~TILE_ROLLING;
		}

	}
	// other actions change items
	else
	{
		// remove items

		*improvedTileItems &= ~tx_terraform[action].removed_items_flag;

		// add items

		*improvedTileItems |= tx_terraform[action].added_items_flag;

	}

}

bool isVehicleFormer(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_TERRAFORMING_UNIT);
}

bool isFormerTargetingTile(VEH *vehicle, MAP *tile)
{
	// ignore not formers

	if (!isVehicleFormer(vehicle))
		return false;

	// return true if former is going to the tile
	if (vehicle->move_status == STATUS_GOTO)
	{
		return (getVehicleDestination(vehicle) == tile);
	}
	// otherwise, return true if vehicle is on tile
	else
	{
		return (mapsq(vehicle->x, vehicle->y) == tile);
	}

}

bool isVehicleTerraforming(VEH *vehicle)
{
	return vehicle->move_status >= STATUS_FARM && vehicle->move_status <= STATUS_PLACE_MONOLITH;
}

bool isVehicleMoving(VEH *vehicle)
{
	return vehicle->move_status == STATUS_GOTO || vehicle->move_status == STATUS_MOVE || vehicle->move_status == STATUS_ROAD_TO || vehicle->move_status == STATUS_MAGTUBE_TO || vehicle->move_status == STATUS_AI_GO_TO;
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

	return mapsq(x, y);

}

bool isTileTakenByOtherFormer(VEH *primaryVehicle, MAP *tile)
{
	bool taken = false;

	// iterate through vehicles

	for (int vehicleIndex = 0; vehicleIndex < *tx_total_num_vehicles; vehicleIndex++)
	{
		VEH *vehicle = &tx_vehicles[vehicleIndex];

		// skip itself

		if (vehicle == primaryVehicle)
			continue;

		// skip not own vehicles

		if (vehicle->faction_id != primaryVehicle->faction_id)
			continue;

		// skip not formers

		if (!isVehicleFormer(vehicle))
			continue;

		// check if vehicle targets given tile

		if (isFormerTargetingTile(vehicle, tile))
		{
			taken = true;
			break;

		}

	}

	return taken;

}

void computeBase(int baseId)
{
	tx_set_base(baseId);
	tx_base_compute(1);

}

int setTerraformingAction(int vehicleId, int action)
{
	return set_action(vehicleId, action + 4, veh_status_icon[action + 4]);
}

/**
Orders former to build other improvement besides base yield change.
*/
int buildImprovement(int vehicleId)
{
	VEH *vehicle = &tx_vehicles[vehicleId];

	int order;

	// TODO
	// currently only skip turn

	order = veh_skip(vehicleId);

	debug
	(
		"giveOrderToFormer: (%d,%d), order=skip\n",
		vehicle->x,
		vehicle->y
	)

	return order;

}

/**
Checks if former can build road or tube in square without disturbing fungus.
Returns available action or -1 if none available.
*/
int canBuildRoadOrTube(int factionId, MAP *tile)
{
	// skip ocean

	if (is_ocean(tile))
		return -1;

	// no road
	if (~tile->items & TERRA_ROAD)
	{
		// there is no fungus or there is fungus road tech/project
		if (~tile->items & TERRA_FUNGUS || has_tech(factionId, tx_basic->tech_preq_build_road_fungus) || has_project(factionId, FAC_XENOEMPATHY_DOME))
		{
			return FORMER_ROAD;
		}
		else
		{
			return -1;
		}
	}
	// road but no tube
	else if (tile->items & TERRA_ROAD && ~tile->items & TERRA_MAGTUBE)
	{
		return FORMER_MAG_TUBE;
	}
	// other
	else
	{
		return -1;
	}

}

/**
Calculate terraforming time needed to rebuild lost improvements.
*/
const int PRESERVED_IMRPOVEMENT_COUNT = 7;
const int PRESERVED_IMRPOVEMENTS[][2] =
{
	{FORMER_FARM, TERRA_FARM},
	{FORMER_SOIL_ENR, TERRA_SOIL_ENR},
	{FORMER_MINE, TERRA_MINE},
	{FORMER_SOLAR, TERRA_SOLAR},
	{FORMER_CONDENSER, TERRA_CONDENSER},
	{FORMER_ECH_MIRROR, TERRA_ECH_MIRROR},
	{FORMER_THERMAL_BORE, TERRA_THERMAL_BORE},
};
int calculateLostTerraformingTime(int vehicleId, int tileItems, int improvedTileItems)
{
	int lostTerraformingTime = 0;

	for (int improvementIndex = 0; improvementIndex < PRESERVED_IMRPOVEMENT_COUNT; improvementIndex++)
	{
		int terraformingAction = PRESERVED_IMRPOVEMENTS[improvementIndex][0];
		int itemFlag = PRESERVED_IMRPOVEMENTS[improvementIndex][1];

		if ((tileItems & itemFlag) && (~improvedTileItems & itemFlag))
		{
			lostTerraformingTime += calculateTerraformingTime(vehicleId, terraformingAction);
		}

	}

	return lostTerraformingTime;

}

