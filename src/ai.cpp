#include "ai.h"
#include "move.h"
#include "wtp.h"

// terraforming choice optimization parameters

const double growthWeight = 40.0;
const double mineralWeight = 1.0;
const double energyWeight = 0.5;
const double improvementBreakpointTime = 40.0;
const double roadValue = 1.0;
const double tubeValue = 1.0;

/**
AI terraforming options
*/
const int LAND_TERRAFORMING_OPTION_COUNT = 3;
TERRAFORMING_OPTION LAND_TERRAFORMING_OPTIONS[LAND_TERRAFORMING_OPTION_COUNT] =
{
	{7, {FORMER_REMOVE_FUNGUS, FORMER_LEVEL_TERRAIN, FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR, FORMER_ROAD, FORMER_MAG_TUBE}},
	{7, {FORMER_REMOVE_FUNGUS, FORMER_LEVEL_TERRAIN, FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE, FORMER_ROAD, FORMER_MAG_TUBE}},
	{4, {FORMER_REMOVE_FUNGUS, FORMER_MINE, FORMER_ROAD, FORMER_MAG_TUBE}},
//	{FORMER_FOREST},
//	{FORMER_PLANT_FUNGUS},
//	{FORMER_AQUIFER},
//	{FORMER_CONDENSER},
//	{FORMER_ECH_MIRROR},
//	{FORMER_THERMAL_BORE},
//	{FORMER_SOIL_ENR},
};
const int SEA_TERRAFORMING_OPTION_COUNT = 2;
TERRAFORMING_OPTION SEA_TERRAFORMING_OPTIONS[SEA_TERRAFORMING_OPTION_COUNT] =
{
	{3, {FORMER_REMOVE_FUNGUS, FORMER_FARM, FORMER_SOLAR}},
	{3, {FORMER_REMOVE_FUNGUS, FORMER_FARM, FORMER_MINE}},
//	{FORMER_FOREST},
//	{FORMER_PLANT_FUNGUS},
//	{FORMER_AQUIFER},
//	{FORMER_CONDENSER},
//	{FORMER_ECH_MIRROR},
//	{FORMER_THERMAL_BORE},
//	{FORMER_SOIL_ENR},
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

	// carry current former AI orders

	carryFormerAIOrders(vehicleId);

	// ignore formers with orders

    if (isFormerCarryingAIOrders(vehicleId))
		return SYNC;

	// set default order

	int order = SYNC;

	// iterate through map tiles

	double bestTerraformingScore = 0.0;
	MAP *bestTerraformingTile = NULL;
	int bestTerraformingTileX = -1;
	int bestTerraformingTileY = -1;
	int bestTerraformingOptionIndex = -1;

	for (int x = 0; x < *tx_map_axis_x; x++)
	{
		for (int y = 0; y < *tx_map_axis_y; y++)
		{
			// skip impossible combinations

			if (x % 2 != y % 2)
				continue;

			// get map tile

			MAP *tile = mapsq(x, y);

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

			if (isTileTakenByOtherFormer(vehicleId, x, y))
				continue;

			// calculate terraforming score

			int terraformingOptionIndex;
			double terraformingScore = calculateTerraformingScore(vehicleId, x, y, &terraformingOptionIndex);

			debug
			(
				"calculateTerraformingScore: (%d,%d), terraformingScore=%f, terraformingOptionIndex=%d\n",
				x,
				y,
				terraformingScore,
				terraformingOptionIndex
			)

			if (bestTerraformingTile == NULL || bestTerraformingScore > terraformingScore)
			{
				bestTerraformingScore = terraformingScore;
				bestTerraformingTile = tile;
				bestTerraformingTileX = x;
				bestTerraformingTileY = y;
				bestTerraformingOptionIndex = terraformingOptionIndex;

			}

		}
	}

	debug
	(
		"calculateBestTerraformingScore: (%d,%d), bestTerraformingScore=%f, bestTerraformingOptionIndex=%d\n",
		bestTerraformingTileX,
		bestTerraformingTileY,
		bestTerraformingScore,
		bestTerraformingOptionIndex
	)

	if (bestTerraformingTile == NULL)
	{
		// do nothing

		order = veh_skip(vehicleId);

		debug
		(
			"giveOrderToFormer: (%d,%d), order=skip\n",
			vehicle->x,
			vehicle->y
		)

	}
	else
	{
		// at destination

		if (bestTerraformingTile == vehicleTile)
		{
			// start carrying terraforming orders

			carryFormerAIOrders(vehicleId);

		}
		else
		{
			// send former to tile

			order = set_move_to(vehicleId, bestTerraformingTileX, bestTerraformingTileY);

			debug
			(
				"giveOrderToFormer: (%d,%d), order=%s(%d,%d)\n",
				vehicle->x,
				vehicle->y,
				MOVE_STATUS[vehicle->move_status].c_str(),
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
double calculateTerraformingScore(int vehicleId, int x, int y, int *bestTerraformingOptionIndex)
{
	VEH *vehicle = &tx_vehicles[vehicleId];
	int factionId = vehicle->faction_id;
	int triad = veh_triad(vehicleId);
	MAP *tile = mapsq(x, y);
	bool sea = is_ocean(tile);

	// calculate previous yield score

	double previousYieldScore = calculateYieldScore(factionId, x, y);

	// select terraforming options for body type

	int terraformingOptionCount;
	TERRAFORMING_OPTION *terraformingOptions;

	if (sea)
	{
		terraformingOptionCount = SEA_TERRAFORMING_OPTION_COUNT;
		terraformingOptions = SEA_TERRAFORMING_OPTIONS;
	}
	else
	{
		terraformingOptionCount = LAND_TERRAFORMING_OPTION_COUNT;
		terraformingOptions = LAND_TERRAFORMING_OPTIONS;
	}

	// store tile values

	const int tileRocks = tile->rocks;
	const int tileItems = tile->items;

	// running variables

	int improvedTileRocks;
	int improvedTileItems;
	int terraformingTime;

	// find best terraforming option

	*bestTerraformingOptionIndex = -1;
	double bestTerrafromningScore = 0.0;

	for (int optionIndex = 0; optionIndex < terraformingOptionCount; optionIndex++)
	{
		TERRAFORMING_OPTION option = terraformingOptions[optionIndex];

		// initialize variables

		improvedTileRocks = tileRocks;
		improvedTileItems = tileItems;
		terraformingTime = 0;

		// process actions

		for (int actionIndex = 0; actionIndex < option.count; actionIndex++)
		{
			int action = option.actions[actionIndex];

			// skip unavailable actions

			if (!isTerraformingAvailable(factionId, x, y, action))
				continue;

			// compute terraforming time

			terraformingTime += calculateTerraformingTime(vehicleId, action);

			// apply terraforming

			applyTerraforming(tile, action);

		}

		// set changes

		tile->rocks = improvedTileRocks;
		tile->items = improvedTileItems;

		// test

		double improvedYieldScore = calculateYieldScore(factionId, x, y);

		// revert changes

		tile->rocks = tileRocks;
		tile->items = tileItems;

		// calculate yield improvement score

		double improvementScore = (improvedYieldScore - previousYieldScore);

		// calculate path length

		double travelPathLength = (double)map_range(vehicle->x, vehicle->y, x, y);

		// estimate movement rate along the path

		double estimatedMovementRate;
        double movementRateWithoutRoad = (double)tx_basic->mov_rate_along_roads;
        double movementRateAlongRoad = (conf.tube_movement_rate_multiplier > 0 ? (double)tx_basic->mov_rate_along_roads / conf.tube_movement_rate_multiplier : (double)tx_basic->mov_rate_along_roads);
        double movementRateAlongTube = (conf.tube_movement_rate_multiplier > 0 ? 1 : 0);
        double currentTurn = (double)*tx_current_turn;

		switch (triad)
		{
		case TRIAD_LAND:

			// it takes 5 full turns on average per tile building roads in the beginning
			// then it decreases to tube movement allowance close to the middle-end.

            if (*tx_current_turn < 100)
            {
                estimatedMovementRate = 5.0 * movementRateWithoutRoad - (5.0 * movementRateWithoutRoad - 1.0 * movementRateWithoutRoad) * ((currentTurn - 0.0) / (100.0 - 0.0));
            }
            else if (*tx_current_turn < 200)
            {
                estimatedMovementRate = 1.0 * movementRateWithoutRoad - (1.0 * movementRateWithoutRoad - movementRateAlongRoad) * ((currentTurn - 100.0) / (200.0 - 100.0));
            }
            else if (*tx_current_turn < 400)
            {
                estimatedMovementRate = movementRateAlongRoad - (movementRateAlongRoad - movementRateAlongTube) * ((currentTurn - 200.0) / (400.0 - 200.0));
            }
            else
            {
                estimatedMovementRate = movementRateAlongTube;
            }

            break;

		default:

			// no roads for sea and air units

			estimatedMovementRate = movementRateWithoutRoad;

		}

        // estimate travel time

        double estimatedTravelTime = travelPathLength * estimatedMovementRate / veh_speed(vehicleId);

		// summarize score

		double terraformingScore = improvementScore * (improvementBreakpointTime - (estimatedTravelTime + (double)terraformingTime));

        debug
        (
            "calculateTerraformingScore: (%d,%d), improvementScore=%f, improvementBreakpointTime=%f, estimatedTravelTime=%f, terraformingTime=%d, terraformingScore=%f\n",
            x,
            y,
            improvementScore,
            improvementBreakpointTime,
            estimatedTravelTime,
            terraformingTime,
            terraformingScore
        )
        ;

		// update score

		if (*bestTerraformingOptionIndex == -1 || terraformingScore > bestTerrafromningScore)
		{
			*bestTerraformingOptionIndex = optionIndex;
			bestTerrafromningScore = terraformingScore;
		}

	}

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

		*tx_current_base_id = baseIndex;

		// calculate yield

		tx_base_yield();

		// calculate base yield score

		double baseYieldScore = 0.0;

		baseYieldScore += growthWeight * (double)base->nutrient_surplus / (double)((base->pop_size + 1) * 10);
		baseYieldScore += mineralWeight * (double)base->nutrient_surplus;
		baseYieldScore += energyWeight * (double)base->energy_surplus;

		debug
		(
			"calculateBaseYieldScore: (%d,%d), base(%d,%d), pop_size=%d, nutrient_surplus=%d, nutrient_surplus=%d, energy_surplus=%d, baseYieldScore=%f\n",
			x,
			y,
			base->x,
			base->y,
			base->pop_size,
			base->nutrient_surplus,
			base->nutrient_surplus,
			base->energy_surplus,
			baseYieldScore
		)
		;

		// accumulate yield score

		yieldScore += baseYieldScore;

    }

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

	bool terraformingAvailable = false;

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

void applyTerraforming(MAP *tile, int action)
{
	// remove items

	tile->items &= ~tx_terraform[action].removed_items_flag;

	// add items

	tile->items |= tx_terraform[action].added_items_flag;

}

void carryFormerAIOrders(int vehicleId)
{
	VEH *vehicle = &tx_vehicles[vehicleId];
	int x = vehicle->x;
	int y = vehicle->y;
	int factionId = vehicle->faction_id;

	short aiStatus = vehicle->pad_0;
	bool sea = aiStatus & 0x2;
	int terraformingOptionIndex = (aiStatus >> 2) & 0xF;
	int actionIndex = (aiStatus >> 6) & 0xF;

	TERRAFORMING_OPTION terraformingOption = (sea ? SEA_TERRAFORMING_OPTIONS : LAND_TERRAFORMING_OPTIONS)[terraformingOptionIndex];

	// skip vehicles without AI orders

	if (!isFormerCarryingAIOrders(vehicleId))
		return;

	// skip not idle vehicles

	if (vehicle->move_status != STATUS_IDLE)
		return;

	// advance to next action index

	actionIndex++;

	// execute next available action

	for (actionIndex++; actionIndex < (int)(sizeof(terraformingOption.actions)/sizeof(terraformingOption.actions[0])); actionIndex++)
	{
		int action = terraformingOption.actions[actionIndex];

		// check if current action is available

		if (isTerraformingAvailable(factionId, x, y, action))
		{
			set_action(vehicleId, action, veh_status_icon[action + 4]);
			return;
		}

	}

	// no next action were selected
	// clear AI orders and return

	vehicle->pad_0 = 0;
	return;

}

bool isFormerCarryingAIOrders(int vehicleId)
{
	VEH *vehicle = &tx_vehicles[vehicleId];

	return (vehicle->pad_0 & 0x1) != 0;
}

bool isVehicleFormer(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_TERRAFORMING_UNIT);
}

bool isFormerTargettingTile(int vehicleId, int x, int y)
{
	VEH *vehicle = &tx_vehicles[vehicleId];
	MAP *tile = mapsq(x, y);

	bool targetting = false;

	if (isVehicleFormer(vehicle))
	{
		if (isVehicleTerraforming(vehicleId))
		{
			if (mapsq(vehicle->x, vehicle->y) == tile)
			{
				targetting = true;
			}
		}
		else if (isVehicleMoving(vehicleId))
		{
			if (getVehicleDestination(vehicleId) == tile)
			{
				targetting = true;
			}
		}
	}

	return targetting;
}

bool isVehicleTerraforming(int vehicleId)
{
	VEH *vehicle = &tx_vehicles[vehicleId];

	return vehicle->move_status >= STATUS_FARM && vehicle->move_status <= STATUS_PLACE_MONOLITH;
}

bool isVehicleMoving(int vehicleId)
{
	VEH *vehicle = &tx_vehicles[vehicleId];

	return vehicle->move_status == STATUS_GOTO || vehicle->move_status == STATUS_MOVE || vehicle->move_status == STATUS_ROAD_TO || vehicle->move_status == STATUS_MAGTUBE_TO || vehicle->move_status == STATUS_AI_GO_TO;
}

MAP *getVehicleDestination(int vehicleId)
{
	VEH *vehicle = &tx_vehicles[vehicleId];
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

bool isTileTakenByOtherFormer(int primaryVehicleId, int x, int y)
{
	VEH *primaryVehicle = &tx_vehicles[primaryVehicleId];

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

		if (isFormerTargettingTile(vehicleIndex, x, y))
		{
			taken = true;
			break;
		}

	}

	return taken;

}

