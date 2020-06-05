#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include "ai.h"

/**
AI terraforming options
*/
const int TERRAFORMING_OPTION_COUNT = 12;
TERRAFORMING_OPTION TERRAFORMING_OPTIONS[TERRAFORMING_OPTION_COUNT] =
{
	// land
	{false, false, 0.0, 2, {FORMER_MINE, FORMER_ROAD}},
	{false, false, 0.0, 3, {FORMER_FARM, FORMER_SOIL_ENR, FORMER_MINE}},
	{false, false, 0.0, 3, {FORMER_FARM, FORMER_SOIL_ENR, FORMER_SOLAR}},
	{false, false, 0.0, 3, {FORMER_FARM, FORMER_SOIL_ENR, FORMER_CONDENSER}},
	{false, false, 0.0, 3, {FORMER_FARM, FORMER_SOIL_ENR, FORMER_ECH_MIRROR}},
	{false, false, 0.0, 1, {FORMER_THERMAL_BORE}},
	{false, false, 0.0, 1, {FORMER_FOREST}},
	{false, false, 0.0, 1, {FORMER_PLANT_FUNGUS}},
	{false, false, conf.ai_terraforming_roadBonus, 1, {FORMER_ROAD}},
	{false, false, conf.ai_terraforming_tubeBonus, 1, {FORMER_MAGTUBE}},
	// sea
	{true, false, 0.0, 2, {FORMER_FARM, FORMER_MINE}},
	{true, false, 0.0, 2, {FORMER_FARM, FORMER_SOLAR}},
};

// global variables

int factionId;
std::vector<int> terraformingFormerIds;
std::vector<int> availableFormerIds;
std::unordered_set<MAP *> terraformedTiles;
std::unordered_set<MAP *> targetedTiles;
std::map<int, FormerOrder> formerOrders;

/**
AI strategy.
*/
void ai_strategy(int id)
{
	// set faction id

	factionId = id;

	// prepare former orders

	prepareFormerOrders();

}

/**
Prepares former orders.
*/
void prepareFormerOrders()
{
	// populate processing lists

	populateLists();

	// process available formers

	processAvailableFormers();

	// optimize former destinations

	optimizeFormerDestinations();

}

/**
Populate global lists before processing faction strategy.
*/
void populateLists()
{
	// clear lists

	terraformingFormerIds.clear();
	availableFormerIds.clear();
	terraformedTiles.clear();
	targetedTiles.clear();
	formerOrders.clear();

	// populate lists

	for (int vehicleIndex = 0; vehicleIndex < *total_num_vehicles; vehicleIndex++)
	{
		VEH *vehicle = &tx_vehicles[vehicleIndex];

		// exclude not own vehicles

		if (vehicle->faction_id != factionId)
			continue;

		// exclude not formers

		if (!isVehicleFormer(vehicle))
			continue;

		// create entry

		if (isVehicleTerraforming(vehicle))
		{
			terraformingFormerIds.push_back(vehicleIndex);

			// populate terraformed tile

			MAP *terraformedTile = mapsq(vehicle->x, vehicle->y);

			terraformedTiles.insert(terraformedTile);

		}
		else
		{
			availableFormerIds.push_back(vehicleIndex);

			formerOrders[vehicleIndex] = {vehicleIndex, vehicle, -1, -1, -1};

		}

	}

}

void processAvailableFormers()
{
	for
	(
		std::map<int, FormerOrder>::iterator formerOrdersIterator = formerOrders.begin();
		formerOrdersIterator != formerOrders.end();
		formerOrdersIterator++
	)
	{
		generateFormerOrder(&formerOrdersIterator->second);
	}

}

void optimizeFormerDestinations()
{
	// iterate until optimized or max iterations

	for (int iteration = 0; iteration < 1000; iteration++)
	{
		bool swapped = false;

		// iterate through former pairs

		for
		(
			std::map<int, FormerOrder>::iterator formerOrdersIterator1 = formerOrders.begin();
			formerOrdersIterator1 != formerOrders.end();
			formerOrdersIterator1++
		)
		{
			for
			(
				std::map<int, FormerOrder>::iterator formerOrdersIterator2 = formerOrders.begin();
				formerOrdersIterator2 != formerOrders.end();
				formerOrdersIterator2++
			)
			{
				// skip itself

				if (formerOrdersIterator2->first == formerOrdersIterator1->first)
					continue;

				FormerOrder *formerOrder1 = &formerOrdersIterator1->second;
				FormerOrder *formerOrder2 = &formerOrdersIterator2->second;

				VEH *vehicle1 = formerOrder1->vehicle;
				VEH *vehicle2 = formerOrder2->vehicle;

				int vehicle1Speed = veh_speed_without_roads(formerOrder1->vehicleId);
				int vehicle2Speed = veh_speed_without_roads(formerOrder2->vehicleId);

				int destination1X = formerOrder1->x;
				int destination1Y = formerOrder1->y;
				int destination2X = formerOrder2->x;
				int destination2Y = formerOrder2->y;

				int action1 = formerOrder1->action;
				int action2 = formerOrder2->action;

				// calculate current travel time

				double vehicle1CurrentTravelTime = (double)map_range(vehicle1->x, vehicle1->y, destination1X, destination1Y) / (double)vehicle1Speed;
				double vehicle2CurrentTravelTime = (double)map_range(vehicle2->x, vehicle2->y, destination2X, destination2Y) / (double)vehicle2Speed;

				double currentTravelTime = vehicle1CurrentTravelTime + vehicle2CurrentTravelTime;

				// calculate swapped travel time

				double vehicle1SwappedTravelTime = (double)map_range(vehicle1->x, vehicle1->y, destination2X, destination2Y) / (double)vehicle1Speed;
				double vehicle2SwappedTravelTime = (double)map_range(vehicle2->x, vehicle2->y, destination1X, destination1Y) / (double)vehicle2Speed;

				double swappedTravelTime = vehicle1SwappedTravelTime + vehicle2SwappedTravelTime;

				// swap orders if beneficial

				if (swappedTravelTime < currentTravelTime)
				{
					formerOrder1->x = destination2X;
					formerOrder1->y = destination2Y;
					formerOrder1->action = action2;

					formerOrder2->x = destination1X;
					formerOrder2->y = destination1Y;
					formerOrder2->action = action1;

					swapped = true;

				}

			}

		}

		// stop cycle if nothing is swapped

		if (!swapped)
			break;

	}
}

/**
Handles movement phase.
*/
int enemyMoveFormer(int vehicleId)
{
	VEH *vehicle = &tx_vehicles[vehicleId];

	// skip not formers

	if (!isVehicleFormer(vehicle))
		return SYNC;

	// skip terraforming vehicles

	if (isVehicleTerraforming(vehicle))
		return SYNC;

	// find former order

	std::map<int, FormerOrder>::iterator formerOrderIterator = formerOrders.find(vehicleId);

	// skip vehicle if not found

	if (formerOrderIterator == formerOrders.end())
		return SYNC;

	// get order

	FormerOrder *formerOrder = &formerOrderIterator->second;

	// skip orders without action

	if (formerOrder->action == -1)
		return SYNC;

	// execute order

	setFormerOrder(formerOrder);

	return SYNC;

}

void setFormerOrder(FormerOrder *formerOrder)
{
	int vehicleId = formerOrder->vehicleId;
	VEH *vehicle = formerOrder->vehicle;
	int x = formerOrder->x;
	int y = formerOrder->y;
	int action = formerOrder->action;

	// at destination
	if (vehicle->x == x && vehicle->y == y)
	{
		// execute terraforming action

		setTerraformingAction(vehicleId, action);

		debug
		(
			"generateFormerOrder: location=(%d,%d), action=%s\n",
			vehicle->x,
			vehicle->y,
			tx_terraform[action].name
		)
		;

	}
	// not at destination
	else
	{
		// send former to destination

		sendVehicleToDestination(vehicleId, x, y);

		debug
		(
			"generateFormerOrder: location=(%d,%d), move_to(%d,%d)\n",
			vehicle->x,
			vehicle->y,
			x,
			y
		)

	}

}

/**
Gives order to former.
*/
void generateFormerOrder(FormerOrder *formerOrder)
{
	int vehicleId = formerOrder->vehicleId;
	VEH *vehicle = formerOrder->vehicle;
	int vehicleTriad = veh_triad(vehicleId);
	MAP *vehicleTile = mapsq(vehicle->x, vehicle->y);

	// iterate through map tiles

	double bestTerraformingScore = -DBL_MAX;
	int bestTerraformingTileX = -1;
	int bestTerraformingTileY = -1;
	MAP *bestTerraformingTile = NULL;
	int bestTerraformingAction = -1;

	for (int y = 0; y < *map_axis_y; y++)
	{
		for (int x = 0; x < *map_axis_x; x++)
		{
			// skip impossible combinations

			if (x % 2 != y % 2)
				continue;

			// get map tile

			MAP *tile = mapsq(x, y);

			// exclude incorrect map tiles

			if (!tile)
				continue;

			// exclude base tiles

			if (tile->items & TERRA_BASE_IN_TILE)
				continue;

			// exclude unreachable territory

			if (vehicleTriad != TRIAD_AIR && tile->region != vehicleTile->region)
				continue;

			// exclude volcano mouth

			if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
				continue;

			// exclude everything but our workable tiles

			if (!isOwnWorkableTile(x, y))
				continue;

			// exclude already taken tiles

			if (isTileTakenByOtherFormer(tile))
				continue;

			// calculate terraforming score

			int terraformingAction = -1;
			double terraformingScore = calculateTerraformingScore(vehicleId, x, y, &terraformingAction);

			if (terraformingAction != -1 && terraformingScore > bestTerraformingScore)
			{
				bestTerraformingScore = terraformingScore;
				bestTerraformingTileX = x;
				bestTerraformingTileY = y;
				bestTerraformingTile = tile;
				bestTerraformingAction = terraformingAction;

			}

		}

	}

	// found good terraforming option
	if (bestTerraformingTile != NULL && bestTerraformingAction != -1)
	{
//		// at destination
//		if (bestTerraformingTile == vehicleTile)
//		{
//			// execute terraforming action
//
//			setTerraformingAction(vehicleId, bestTerraformingAction);
//
//			debug
//			(
//				"generateFormerOrder: location=(%d,%d), action=%s\n",
//				vehicle->x,
//				vehicle->y,
//				tx_terraform[bestTerraformingAction].name
//			)
//			;
//
//		}
//		// not at destination
//		else
//		{
//			// send former to destination
//
//			sendVehicleToDestination(vehicleId, bestTerraformingTileX, bestTerraformingTileY);
//
//			debug
//			(
//				"generateFormerOrder: location=(%d,%d), move_to(%d,%d)\n",
//				vehicle->x,
//				vehicle->y,
//				bestTerraformingTileX,
//				bestTerraformingTileY
//			)
//
//		}
//
		// set former order

		formerOrder->x = bestTerraformingTileX;
		formerOrder->y = bestTerraformingTileY;
		formerOrder->action = bestTerraformingAction;

		// add target to list

		targetedTiles.insert(bestTerraformingTile);

		debug
		(
			"generateFormerOrder: location=(%d,%d), move_to(%d,%d), action=%s\n",
			vehicle->x,
			vehicle->y,
			bestTerraformingTileX,
			bestTerraformingTileY,
			(bestTerraformingAction == -1 ? "none" : tx_terraform[bestTerraformingAction].name)
		)

	}

}

/**
Selects best terraforming option for given square and calculates its improvement score after terraforming.
Returns improvement score and modifies bestTerraformingOptionIndex reference.
*/
double calculateTerraformingScore(int vehicleId, int x, int y, int *bestTerraformingAction)
{
	debug
	(
		"calculateTerraformingScore: tile=(%2d,%2d)\n",
		x,
		y
	)

	VEH *vehicle = &tx_vehicles[vehicleId];
	int triad = veh_triad(vehicleId);
	MAP *tile = mapsq(x, y);
	bool sea = is_ocean(tile);

	// calculate previous yield score

	double previousYieldScore = calculateYieldScore(x, y);

	// store tile values

	const int tileRocks = tile->rocks;
	const int tileItems = tile->items;

	// running variables

	int improvedTileRocks;
	int improvedTileItems;
	int terraformingTime;
	int lostTerraformingTime;

	// find best terraforming option

	double bestTerrafromningScore = -DBL_MAX;
	int bestTerraformingOptionIndex = -1;
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
		int firstAction = -1;

		// process actions

		for (int actionIndex = 0; actionIndex < option.count; actionIndex++)
		{
			int action = option.actions[actionIndex];

			// skip unavailable actions

			if (!isTerraformingAvailable(x, y, action))
				continue;

			// remove fungus if needed

			if (action != FORMER_REMOVE_FUNGUS && (improvedTileItems & TERRA_FUNGUS))
			{
				int removeFungusAction = FORMER_REMOVE_FUNGUS;

				// compute terraforming time

				terraformingTime += calculateTerraformingTime(vehicleId, x, y, removeFungusAction);

				// generate terraforming change

				generateTerraformingChange(&improvedTileRocks, &improvedTileItems, removeFungusAction);

				// store first removeFungusAction

				if (firstAction == -1)
				{
					firstAction = removeFungusAction;
				}

			}

			// level terrain if needed

			if ((action == FORMER_FARM || action == FORMER_FOREST) && improvedTileRocks & TILE_ROCKY)
			{
				int levelTerrainAction = FORMER_LEVEL_TERRAIN;

				// compute terraforming time

				terraformingTime += calculateTerraformingTime(vehicleId, x, y, levelTerrainAction);

				// generate terraforming change

				generateTerraformingChange(&improvedTileRocks, &improvedTileItems, levelTerrainAction);

				// store first levelTerrainAction

				if (firstAction == -1)
				{
					firstAction = levelTerrainAction;
				}

			}

			// execute main action

			// compute terraforming time

			terraformingTime += calculateTerraformingTime(vehicleId, x, y, action);

			// generate terraforming change

			generateTerraformingChange(&improvedTileRocks, &improvedTileItems, action);

			// store first action

			if (firstAction == -1)
			{
				firstAction = action;
			}

		}

		// set changes

		tile->rocks = improvedTileRocks;
		tile->items = improvedTileItems;
		if (option.worldRainfall)
		{
			tx_world_rainfall();
		}

		// calculate lost terraforming time

		lostTerraformingTime = calculateLostTerraformingTime(vehicleId, x, y, tileItems, improvedTileItems);

		// test

		double improvedYieldScore = calculateYieldScore(x, y);

		// revert changes

		tile->rocks = tileRocks;
		tile->items = tileItems;
		if (option.worldRainfall)
		{
			tx_world_rainfall();
		}

		// calculate yield improvement score

		double improvementScore = (improvedYieldScore - previousYieldScore);

		// add worked tile bonus

		improvementScore += conf.ai_terraforming_resourceLifetime * conf.ai_terraforming_workedTileBonus;

		// add arbitrary bonus

		improvementScore += conf.ai_terraforming_resourceLifetime * option.bonusValue;

		// calculate path length

		double travelPathLength = (double)map_range(vehicle->x, vehicle->y, x, y);

		// estimate movement rate along the path

		double estimatedMovementCost;
        double movementCostWithoutRoad = (double)tx_basic->mov_rate_along_roads;
        double movementCostAlongRoad = (conf.tube_movement_rate_multiplier > 0 ? (double)tx_basic->mov_rate_along_roads / conf.tube_movement_rate_multiplier : (double)tx_basic->mov_rate_along_roads);
        double movementCostAlongTube = (conf.tube_movement_rate_multiplier > 0 ? 1 : 0);
        double currentTurn = (double)*current_turn;

		switch (triad)
		{
		case TRIAD_LAND:

			// approximating 2 turns per tile at the beginning and then decrease movement close to the middle-end.

            if (*current_turn < 100)
            {
                estimatedMovementCost = 2.0 * movementCostWithoutRoad - (2.0 * movementCostWithoutRoad - 1.0 * movementCostWithoutRoad) * ((currentTurn - 0.0) / (100.0 - 0.0));
            }
            else if (*current_turn < 200)
            {
                estimatedMovementCost = 1.0 * movementCostWithoutRoad - (1.0 * movementCostWithoutRoad - movementCostAlongRoad) * ((currentTurn - 100.0) / (200.0 - 100.0));
            }
            else if (*current_turn < 400)
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

		double terraformingScore = improvementScore * exp(-totalTimePenalty / conf.ai_terraforming_resourceLifetime);

        debug
        (
            "\toptionIndex=%d, improvementScore=%f, conf.ai_terraforming_resourceLifetime=%f, estimatedTravelTime=%f, terraformingTime=%d, lostTerraformingTime=%d, terraformingScore=%f\n",
            optionIndex,
            improvementScore,
            conf.ai_terraforming_resourceLifetime,
            estimatedTravelTime,
            terraformingTime,
            lostTerraformingTime,
            terraformingScore
        )
        ;

		// update score

		if (firstAction != -1 && terraformingScore > bestTerrafromningScore)
		{
			bestTerrafromningScore = terraformingScore;
			bestTerraformingOptionIndex = optionIndex;
			*bestTerraformingAction = firstAction;

		}

	}

	debug
	(
		"\tbestTerrafromningScore=%f, bestTerraformingOptionIndex=%d, bestTerraformingAction=%s\n",
		bestTerrafromningScore,
		bestTerraformingOptionIndex,
		(*bestTerraformingAction == -1 ? "none" : tx_terraform[*bestTerraformingAction].name)
	)
	;

	// return

	return bestTerrafromningScore;

}

double calculateYieldScore(int x, int y)
{
	double yieldScore = 0.0;

	// iterate bases

	for (int baseIndex = 0; baseIndex < *total_num_bases; baseIndex++)
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

        // recalculate worked tiles

        base->worked_tiles = 0;
		computeBase(baseIndex);

		// calculate income

		int mineralIncome = base->mineral_surplus;
		int energyIncome = base->economy_total + base->psych_total + base->labs_total;

		double income = conf.ai_terraforming_mineralWeight * (double)mineralIncome + conf.ai_terraforming_energyWeight * (double)energyIncome;

		// calculate population growth and income growth

		double absolutePopulationGrowth = (double)base->nutrient_surplus / (double)((base->pop_size + 1) * 10);
		double relativePopulationGrowth = absolutePopulationGrowth / (double)(base->pop_size + 1);
		double incomeGrowth = income * relativePopulationGrowth;

		// calculate base yield score

		double baseYieldScore = conf.ai_terraforming_resourceLifetime * income + conf.ai_terraforming_resourceLifetime * conf.ai_terraforming_resourceLifetime * incomeGrowth;

		// restore worked tiles

        base->worked_tiles = currentWorkedTiles;

//		debug
//		(
//			"calculateBaseYieldScore: (%d,%d), base(%d,%d), pop_size=%d, nutrient_surplus=%d, mineral_surplus=%d, energy_surplus=%d, baseYieldScore=%f\n",
//			x,
//			y,
//			base->x,
//			base->y,
//			base->pop_size,
//			base->nutrient_surplus,
//			base->mineral_surplus,
//			base->energy_surplus,
//			baseYieldScore
//		)
//		;
//
		// accumulate yield score

		yieldScore += baseYieldScore;

    }

    // add value for not tangible assets

//	debug
//	(
//		"calculateYieldScore: (%d,%d), yieldScore=%f\n",
//		x,
//		y,
//		yieldScore
//	)
//	;
//
	return yieldScore;

}

/**
Checks whether tile is own workable tile but not base square.
*/
bool isOwnWorkableTile(int x, int y)
{
	MAP *tile = mapsq(x, y);

	// exclude base squares

	if (tile->items & TERRA_BASE_IN_TILE)
		return false;

	// exclude not own territory

	if (tile->owner != factionId)
		return false;

	// iterate through own bases

	for (int baseIndex = 0; baseIndex < *total_num_bases; baseIndex++)
	{
		BASE *base = &tx_bases[baseIndex];

		// skip not own bases

		if (base->faction_id != factionId)
			continue;

		// calculate distance

		int absDx = abs(base->x - x);
		int absDy = abs(base->y - y);

		// exclude too far bases

		if ((absDx + absDy > 4) || absDx >= 4 || absDy >= 4)
			continue;

		// tile is workable tile

		return true;

	}

	// matched workable tile not found

	return false;

}

/**
Checks whether tile is own worked tile but not base square.
*/
bool isOwnWorkedTile(int x, int y)
{
	MAP *tile = mapsq(x, y);

	// exclude base squares

	if (tile->items & TERRA_BASE_IN_TILE)
		return false;

	// exclude not own territory

	if (tile->owner != factionId)
		return false;

	// iterate through own bases

	for (int baseIndex = 0; baseIndex < *total_num_bases; baseIndex++)
	{
		BASE *base = &tx_bases[baseIndex];

		// skip not own bases

		if (base->faction_id != factionId)
			continue;

		// calculate distance

		int absDx = abs(base->x - x);
		int absDy = abs(base->y - y);

		// exclude too far bases

		if ((absDx + absDy > 4) || absDx >= 4 || absDy >= 4)
			continue;

		// iterate through worked tiles

		for (int baseTileOffsetIndex = 1; baseTileOffsetIndex < BASE_TILE_OFFSET_COUNT; baseTileOffsetIndex++)
		{
			// skip not worked tile

			if ((base->worked_tiles & (0x1 << baseTileOffsetIndex)) == 0)
				continue;

			// check if this worked tile same as given tile

			int workedTileX = base->x + BASE_TILE_OFFSETS[baseTileOffsetIndex][0];
			int workedTileY = base->y + BASE_TILE_OFFSETS[baseTileOffsetIndex][1];

			if (workedTileX == x && workedTileY == y)
				return true;

		}

	}

	// matching worked tile not found

	return false;

}

/**
Checks whether terraforming is available.
Returns true if corresponding technology exists and tile doesn't contain this improvement yet.
*/
bool isTerraformingAvailable(int x, int y, int action)
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
	case FORMER_MAGTUBE:
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

int calculateTerraformingTime(int vehicleId, int x, int y, int action)
{
	VEH *vehicle = &tx_vehicles[vehicleId];
	int vehicleX = vehicle->x;
	int vehicleY = vehicle->y;

	int terraformingTime;

	// move vehicle to location

	vehicle->x = x;
	vehicle->y = y;

	// calculate terraforming time

	terraformingTime = tx_action_terraform(vehicleId, action, 0) + 1;

	// return vehicle back

	vehicle->x = vehicleX;
	vehicle->y = vehicleY;

	// return result

	return terraformingTime;

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

bool isTileTargettedByVehicle(VEH *vehicle, MAP *tile)
{
	return (vehicle->move_status == ORDER_MOVE_TO && (getVehicleDestination(vehicle) == tile));
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

	return mapsq(x, y);

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

void computeBase(int baseId)
{
	tx_set_base(baseId);
	tx_base_compute(1);

}

void setTerraformingAction(int vehicleId, int action)
{
	set_action(vehicleId, action + 4, veh_status_icon[action + 4]);
}

/**
Orders former to build other improvement besides base yield change.
*/
void buildImprovement(int vehicleId)
{
	VEH *vehicle = &tx_vehicles[vehicleId];

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
int calculateLostTerraformingTime(int vehicleId, int x, int y, int tileItems, int improvedTileItems)
{
	int lostTerraformingTime = 0;

	for (int improvementIndex = 0; improvementIndex < PRESERVED_IMRPOVEMENT_COUNT; improvementIndex++)
	{
		int terraformingAction = PRESERVED_IMRPOVEMENTS[improvementIndex][0];
		int itemFlag = PRESERVED_IMRPOVEMENTS[improvementIndex][1];

		if ((tileItems & itemFlag) && (~improvedTileItems & itemFlag))
		{
			lostTerraformingTime += calculateTerraformingTime(vehicleId, x, y, terraformingAction);
		}

	}

	return lostTerraformingTime;

}

void sendVehicleToDestination(int vehicleId, int x, int y)
{
	tx_go_to(vehicleId, veh_status_icon[ORDER_MOVE_TO], x, y);
}

