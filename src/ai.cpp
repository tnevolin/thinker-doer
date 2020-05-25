#include "ai.h"
#include "move.h"
#include "wtp.h"

// terraforming choice optimization parameters

const double nutrientWeight = 10.0;
const double mineralWeight = 8.0;
const double energyWeight = 5.0;
const double improvementTimeWeight = -2.0;

/**
Various terraforming options given to AI.
1. requires farm
2. remove fungus before improve fungus technology
3. remove fungus after improve fungus technology
4. level
5. terraform action
6. tile items mask
*/
const int LAND_TERRAFORMING_OPTION_COUNT = 3;
TERRAFORMING_OPTION LAND_TERRAFORMING_OPTIONS[] =
{
	{false, true, true, true, FORMER_FARM, TERRA_FARM},
	{false, true, true, false, FORMER_SOLAR, TERRA_SOLAR},
	{false, true, true, false, FORMER_MINE, TERRA_MINE},
//	{FORMER_FOREST},
//	{FORMER_PLANT_FUNGUS},
//	{FORMER_AQUIFER},
//	{FORMER_CONDENSER},
//	{FORMER_ECH_MIRROR},
//	{FORMER_THERMAL_BORE},
//	{FORMER_SOIL_ENR},
}
;
const int SEA_TERRAFORMING_OPTION_COUNT = 3;
TERRAFORMING_OPTION SEA_TERRAFORMING_OPTIONS[] =
{
	{false, true, true, false, FORMER_FARM, TERRA_FARM},
	{false, true, true, false, FORMER_SOLAR, TERRA_SOLAR},
	{false, true, true, false, FORMER_MINE, TERRA_MINE},
//	{FORMER_FOREST},
//	{FORMER_PLANT_FUNGUS},
//	{FORMER_AQUIFER},
//	{FORMER_CONDENSER},
//	{FORMER_ECH_MIRROR},
//	{FORMER_THERMAL_BORE},
//	{FORMER_SOIL_ENR},
}
;

/**
Gives order to former.
*/
int giveOrderToFormer(int vehicleId)
{
	int order = 0;

	// vehicle variables

	VEH *vehicle = &tx_vehicles[vehicleId];
	int vehicleTriad = veh_triad(vehicleId);
	int vehicleFactionId = vehicle->faction_id;
	MAP *vehicleTile = mapsq(vehicle->x, vehicle->y);

	// ignore formers with purpose

	if (isVehicleTerraforming(vehicle))
	{
		return SYNC;
	}

	// iterate through own bases

	double bestTargetTileImprovementScore = 0.0;
	MAP *bestTargetTile = NULL;
	int bestTargetTileX = vehicle->x;
	int bestTargetTileY = vehicle->y;
	int bestTargetTileAction = -1;

	for (int baseIndex = 0; baseIndex < *tx_total_num_bases; baseIndex++)
	{
		BASE *base = &tx_bases[baseIndex];

		// skip not owned bases

		if (base->faction_id != vehicleFactionId)
			continue;

		// find the worst worked tile yield

		double worstWorkedTileYieldScore = 0.0;
		MAP *worstWorkedTile = NULL;
		int worstWorkedTileX = base->x;
		int worstWorkedTileY = base->y;

		for (int baseTileOffsetIndex = 1; baseTileOffsetIndex < BASE_TILE_OFFSET_COUNT; baseTileOffsetIndex++)
		{
			// skip not worked tiles

			if (~base->worked_tiles & (0x1 << baseTileOffsetIndex))
				continue;

			// find map tile

			int tileX = wrap(base->x + BASE_TILE_OFFSETS[baseTileOffsetIndex][0]);
			int tileY = base->y + BASE_TILE_OFFSETS[baseTileOffsetIndex][1];
			MAP *tile = mapsq(tileX, tileY);

			// skip not existing tiles

			if (tile == NULL)
				continue;

			// calculate tile yield score

			double tileYieldScore = calculateTileYieldScore(baseIndex, base, tileX, tileY);

			// update worst score

			if ((worstWorkedTile == NULL) || (tileYieldScore < worstWorkedTileYieldScore))
			{
				worstWorkedTileYieldScore = tileYieldScore;
				worstWorkedTile = tile;
				worstWorkedTileX = tileX;
				worstWorkedTileY = tileY;
			}

		}

		debug
		(
			"evaluateImprovementScore: base(%d,%d), worstWorkedTile(%d,%d), worstWorkedTileYieldScore=%f\n",
			base->x,
			base->y,
			worstWorkedTileX,
			worstWorkedTileY,
			worstWorkedTileYieldScore
		)

		// find best base tile improvement

		double bestBaseTileImprovementScore = 0.0;
		MAP *bestBaseTile = NULL;
		int bestBaseTileX = vehicle->x;
		int bestBaseTileY = vehicle->y;
		int bestBaseTileAction = -1;

		for (int baseTileOffsetIndex = 1; baseTileOffsetIndex < BASE_TILE_OFFSET_COUNT; baseTileOffsetIndex++)
		{
			bool worked = base->worked_tiles & (0x1 << baseTileOffsetIndex);
			int tileX = wrap(base->x + BASE_TILE_OFFSETS[baseTileOffsetIndex][0]);
			int tileY = base->y + BASE_TILE_OFFSETS[baseTileOffsetIndex][1];
			MAP *tile = mapsq(tileX, tileY);
			int action = -1;

			// skip non existing tiles

			if (tile == NULL)
				continue;

			// exclude not own tiles

			if (tile->owner != vehicleFactionId)
				continue;

			// exclude unreachable tiles

			if (vehicleTriad != TRIAD_AIR && (tile->body_id != vehicleTile->body_id))
				continue;

			// exclude base tiles

			if (isTileOccupiedByBase(tile))
				continue;

			// exclude already taken tiles

			if (isTileTakenByOtherFormer(vehicle, tile))
				continue;

			// exclude tiles worked by other bases

			if (isTileWorkedByOtherBase(base, tile))
				continue;

			// calculate tile improvement score

			double previousYieldScore = (worked ? calculateTileYieldScore(baseIndex, base, tileX, tileY) : worstWorkedTileYieldScore);

			double tileImprovementScore = calculateImprovementScore(vehicleId, baseIndex, base, tile, tileX, tileY, previousYieldScore, &action);

			// skip tiles where is nothing to build anymore

			if (action == -1)
				continue;

			// update best score

			if ((bestBaseTile == NULL) || (tileImprovementScore > bestBaseTileImprovementScore))
			{
				bestBaseTileImprovementScore = tileImprovementScore;
				bestBaseTile = tile;
				bestBaseTileX = tileX;
				bestBaseTileY = tileY;
				bestBaseTileAction = action;
			}

			debug
			(
				"evaluateImprovementScore: base(%d,%d), tile(%d,%d), tileImprovementScore=%f\n",
				base->x,
				base->y,
				tileX,
				tileY,
				tileImprovementScore
			)

		}

		debug
		(
			"evaluateImprovementScore: base(%d,%d), bestBaseTile(%d,%d), bestBaseTileImprovementScore=%f\n",
			base->x,
			base->y,
			bestBaseTileX,
			bestBaseTileY,
			bestBaseTileImprovementScore
		)

		if (bestTargetTile == NULL || bestBaseTileImprovementScore > bestTargetTileImprovementScore)
		{
			bestTargetTileImprovementScore = bestBaseTileImprovementScore;
			bestTargetTile = bestBaseTile;
			bestTargetTileX = bestBaseTileX;
			bestTargetTileY = bestBaseTileY;
			bestTargetTileAction = bestBaseTileAction;
		}

	}

	debug
	(
		"evaluateImprovementScore: bestTargetTile(%d,%d), bestTargetTileImprovementScore=%f\n",
		bestTargetTileX,
		bestTargetTileY,
		bestTargetTileImprovementScore
	)

	if (bestTargetTile == NULL)
	{
		// do nothing

		order = veh_skip(vehicleId);

		debug
		(
			"giveOrderToFormer: vehicle(%d,%d), order=skip\n",
			vehicle->x,
			vehicle->y
		)

	}
	else
	{
		// at destination

		if (bestTargetTile == vehicleTile)
		{
			// give terraforming order to former

			order = set_action(vehicleId, bestTargetTileAction + 4, *tx_terraform[bestTargetTileAction].shortcuts);

			debug
			(
				"giveOrderToFormer: vehicle(%d,%d), order=%s\n",
				vehicle->x,
				vehicle->y,
				tx_terraform[bestTargetTileAction].name
			)

		}
		else
		{
			// send former to tile building connection

			order = set_move_road_tube_to(vehicleId, bestTargetTileX, bestTargetTileY);

			debug
			(
				"giveOrderToFormer: vehicle(%d,%d), order=%s(%d,%d)\n",
				vehicle->x,
				vehicle->y,
				MOVE_STATUS[vehicle->move_status].c_str(),
				bestTargetTileX,
				bestTargetTileY
			)

		}

	}

	return order;

}

bool isTileOccupiedByBase(MAP *tile)
{
	bool tileOccupiedByBase = false;

	for (int baseIndex = 0; baseIndex < *tx_total_num_bases; baseIndex++)
	{
		BASE *base = &tx_bases[baseIndex];

		MAP *baseTile = mapsq(base->x, base->y);

		if (baseTile == tile)
		{
			tileOccupiedByBase = true;
			break;
		}

	}

	return tileOccupiedByBase;

}

bool isVehicleFormer(VEH *vehicle)
{
	return (tx_units[vehicle->proto_id].weapon_type == WPN_TERRAFORMING_UNIT);
}

bool isFormerTargettingTile(VEH *vehicle, MAP *tile)
{
	bool targetting = false;

	if (isVehicleFormer(vehicle))
	{
		if (isVehicleTerraforming(vehicle))
		{
			if (mapsq(vehicle->x, vehicle->y) == tile)
			{
				targetting = true;
			}
		}
		else if (isVehicleMoving(vehicle))
		{
			if (getVehicleDestination(vehicle) == tile)
			{
				targetting = true;
			}
		}
	}

	return targetting;
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

bool isTileTakenByOtherFormer(VEH *primaryVehicle, MAP *primaryVehicleTargetTile)
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

		if (isFormerTargettingTile(vehicle, primaryVehicleTargetTile))
		{
			taken = true;
			break;
		}

	}

	return taken;

}

bool isTileWorkedByOtherBase(BASE *primaryBase, MAP *primaryBaseTile)
{
	bool worked = false;

	for (int baseIndex = 0; baseIndex < *tx_total_num_bases; baseIndex++)
	{
		BASE *base = &tx_bases[baseIndex];

		// skip primary base

		if (base == primaryBase)
			continue;

		// exclude other faction bases

		if (base->faction_id != primaryBase->faction_id)
			continue;

		// iterate worked tiles

		for (int baseTileOffsetIndex = 1; baseTileOffsetIndex < BASE_TILE_OFFSET_COUNT; baseTileOffsetIndex++)
		{
			// skip not worked tiles

			if (~base->worked_tiles & (0x1 << baseTileOffsetIndex))
				continue;

			// find base map tile

			int baseTileX = wrap(base->x + BASE_TILE_OFFSETS[baseTileOffsetIndex][0]);
			int baseTileY = base->y + BASE_TILE_OFFSETS[baseTileOffsetIndex][1];
			MAP *baseTile = mapsq(baseTileX, baseTileY);

			// check match

			if (baseTile == primaryBaseTile)
			{
				worked = true;
				break;

			}

		}

		// break out of cycle if tile is already found

		if (worked)
			break;

	}

	return worked;

}

double calculateTileYieldScore(int baseId, BASE *base, int tileX, int tileY)
{
	double tileYieldScore = 0.0;

	// nutrient

	int baseNutrientSurplus = max(1, base->nutrient_surplus);
	int tileNutrientYield = nutrient_yield(base->faction_id, baseId, tileX, tileY, 0);

	tileYieldScore += nutrientWeight / (double)baseNutrientSurplus * (double)tileNutrientYield;

	// mineral

	int baseMineralSurplus = max(1, base->mineral_surplus);
	int tileMineralYield = mineral_yield(base->faction_id, baseId, tileX, tileY, 0);

	tileYieldScore += mineralWeight / (double)baseMineralSurplus * (double)tileMineralYield;

	// energy

	int baseEnergySurplus = max(1, base->energy_surplus);
	int tileEnergyYield = energy_yield(base->faction_id, baseId, tileX, tileY, 0);

	tileYieldScore += energyWeight / (double)baseEnergySurplus * (double)tileEnergyYield;

	return tileYieldScore;

}

double calculateImprovementScore(int vehicleId, int baseIndex, BASE *base, MAP *tile, int tileX, int tileY, double previousYieldScore, int *bestTerraformingAction)
{
	VEH *vehicle = &tx_vehicles[vehicleId];
	int factionId = vehicle->faction_id;
	int triad = veh_triad(vehicleId);
	int speed = veh_chassis_speed(vehicleId);
	bool sea = is_ocean(tile);

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

	int improvedTileRocks;
	int improvedTileItems;
	double fungusImprovementTime;
	double conventionalImprovementTime;
	int action;

	// find best terraforming option

	double bestImprovementScores[] = {0.0, 0,0};
	*bestTerraformingAction = -1;

	for (int optionIndex = 0; optionIndex < terraformingOptionCount; optionIndex++)
	{
		TERRAFORMING_OPTION option = terraformingOptions[optionIndex];

		// check improvement already exists

		if (tileItems & option.item)
			continue;

		// check technology prerequisite

		if (!has_terra(factionId, option.action, sea))
			continue;

		// check farm requirement

		if (option.requiresFarm && (~tileItems & TERRA_FARM))
			continue;

		// initialize variables

		improvedTileRocks = tileRocks;
		improvedTileItems = tileItems;
		fungusImprovementTime = 0.0;
		conventionalImprovementTime = 0.0;
		action = -1;

		// remove fungus

		bool hasFungusImprovementTech = has_tech(factionId, tx_basic->tech_preq_improv_fungus);
		bool removeFungus = (hasFungusImprovementTech ? option.removeFungusAfterTech : option.removeFungusBeforeTech);

		if (removeFungus && (tileItems & TERRA_FUNGUS))
		{
			improvedTileItems &= ~TERRA_FUNGUS;
			fungusImprovementTime += tx_terraform[FORMER_REMOVE_FUNGUS].rate;

			if (action == -1)
			{
				action = FORMER_REMOVE_FUNGUS;
			}

		}

		// level

		if (option.level && (improvedTileRocks & TILE_ROCKY))
		{
			improvedTileRocks = TILE_ROLLING;
			conventionalImprovementTime += tx_terraform[FORMER_LEVEL_TERRAIN].rate;

			if (action == -1)
			{
				action = FORMER_LEVEL_TERRAIN;
			}

		}

		// build improvement

		if (~improvedTileItems & option.item)
		{
			improvedTileItems |= option.item;
			conventionalImprovementTime += tx_terraform[option.action].rate;

			if (action == -1)
			{
				action = option.action;
			}

		}

		// verify action is set

		if (action == -1)
			continue;

		// set changes

		tile->rocks = improvedTileRocks;
		tile->items = improvedTileItems;

		// test

		double improvedTileYieldScore = calculateTileYieldScore(baseIndex, base, tileX, tileY);

		// revert changes

		tile->rocks = tileRocks;
		tile->items = tileItems;

		// calculate yield improvement score

		double yieldImprovementScore = (improvedTileYieldScore - previousYieldScore);

		// modify improvement time

		if (vehicle_has_ability(vehicle, ABL_FUNGICIDAL))
		{
			fungusImprovementTime /= 2.0;
		}

		if (has_project(factionId, FAC_WEATHER_PARADIGM))
		{
			conventionalImprovementTime /= 1.5;
		}

		double totalImprovementTime = fungusImprovementTime + conventionalImprovementTime;

		if (tx_units[vehicle->proto_id].ability_flags & ABL_SUPER_TERRAFORMER)
		{
			totalImprovementTime /= 2.0;
		}

		// calculate path length

		double travelPathLength = (double)map_range(vehicle->x, vehicle->y, tileX, tileY);

		// estimate travel time

		double travelTime = travelPathLength / (max(1.0, (double)*tx_current_turn / 40.0));
		double moveAllowance;
		double adjustedSpeed;

		switch (triad)
		{
		case TRIAD_LAND:

			// it takes 1 turn on average per tile building roads in the beginning
			// then it decreases to 1/9 close to the middle-end.

			moveAllowance = min(1.0, max(1.0/9.0, (200.0 - (double)*tx_current_turn) / 200.0));
			adjustedSpeed = (double)speed / moveAllowance;
			travelTime = travelPathLength / adjustedSpeed;

			break;

		default:

			// no roads for sea and air units

			travelTime = travelPathLength / speed;

		}


		// summarize score

		double improvementScore =
			yieldImprovementScore
			+
			improvementTimeWeight * (totalImprovementTime + travelTime)
		;

		// update score

		if (improvementScore > bestImprovementScores[0])
		{
			bestImprovementScores[1] = bestImprovementScores[0];
			bestImprovementScores[0] = improvementScore;
			*bestTerraformingAction = action;
		}
		else if (improvementScore > bestImprovementScores[1])
		{
			bestImprovementScores[1] = improvementScore;
		}

	}

	return bestImprovementScores[0];

}

