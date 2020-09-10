#include "aiProduction.h"
#include "aiCombat.h"
#include "ai.h"
#include "engine.h"

std::map<int, double> unitTypeProductionDemands;
std::set<int> bestProductionBaseIds;
std::map<int, std::map<int, double>> unitDemands;
std::map<int, PRODUCTION_DEMAND> productionDemands;
std::map<int, PRODUCTION_CHOICE> productionChoices;

/*
Prepares production orders.
*/
void aiProductionStrategy()
{
	// populate lists

	populateProducitonLists();

	// calculate native protection demand

	calculateNativeProtectionDemand();

	// calculate conventional defense demand

	calculateConventionalDefenseDemand();

	// calculate expansion demand

	calculateExpansionDemand();

	// sort production demands

	setProductionChoices();

}

void populateProducitonLists()
{
	unitTypeProductionDemands.clear();
	bestProductionBaseIds.clear();
	unitDemands.clear();
	productionDemands.clear();
	productionChoices.clear();

	// calculate average base production

	double totalBaseProduction = 0.0;

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		totalBaseProduction += (double)base->mineral_surplus_final;

	}

	double averageBaseProduction = totalBaseProduction / (double)baseIds.size();

	// populate best production bases

	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);

		if (base->mineral_surplus_final >= averageBaseProduction)
		{
			bestProductionBaseIds.insert(id);
		}

	}

}

/*
Calculates need for bases protection against natives.
*/
void calculateNativeProtectionDemand()
{
	for (int id : baseIds)
	{
		BASE *base = &(tx_bases[id]);
		BASE_STRATEGY *baseStrategy = &(baseStrategies[id]);

		// calculate current native protection

		double totalNativeProtection = 0.0;

		for (int vehicleId : baseStrategy->garrison)
		{
			totalNativeProtection += estimateVehicleBaseLandNativeProtection(base->faction_id, vehicleId);
		}

		// add currently built purely defensive vehicle to protection

		if (base->queue_items[0] >= 0 && isCombatVehicle(base->queue_items[0]) && tx_weapon[tx_units[base->queue_items[0]].weapon_type].offense_value == 1)
		{
			totalNativeProtection += estimateUnitBaseLandNativeProtection(base->faction_id, base->queue_items[0]);
		}

		// add protection demand

		int item = findStrongestNativeDefensePrototype(base->faction_id);

		if (totalNativeProtection < conf.ai_production_min_native_protection)
		{
			addProductionDemand(id, true, item, 1.0);
		}
		else if (totalNativeProtection < conf.ai_production_max_native_protection)
		{
			double protectionPriority = conf.ai_production_native_protection_priority_multiplier * (conf.ai_production_max_native_protection - totalNativeProtection) / (conf.ai_production_max_native_protection - conf.ai_production_min_native_protection);
			addProductionDemand(id, false, item, protectionPriority);
		}

	}

}

void calculateConventionalDefenseDemand()
{
	debug("%-24s calculateConventionalDefenseDemand\n", tx_metafactions[aiFactionId].noun_faction);

	// find total enemy offensive value in the vicinity of our bases

	debug("\tvehicle treat\n");

	double totalEnemyOffensiveStrength = 0.0;

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(tx_vehicles[id]);
		int triad = veh_triad(id);
		MAP *location = getMapTile(vehicle->x, vehicle->y);

		// exclude natives

		if (vehicle->faction_id == 0)
			continue;

		// exclude own vehicles

		if (vehicle->faction_id == aiFactionId)
			continue;

		// find vehicle weapon offensive value

		int vehicleWeaponOffensiveValue = tx_weapon[tx_units[vehicle->proto_id].weapon_type].offense_value;

		// count only conventional non zero offensive values

		if (vehicleWeaponOffensiveValue < 1)
			continue;

		// get vehicle region

		int region = (triad == TRIAD_AIR ? -1 : location->region);

		// calculate range to own nearest base

		int rangeToNearestOwnBase = findRangeToNearestOwnBase(vehicle->x, vehicle->y, region);

		// ignore vehicles on or farther than max range

		if (rangeToNearestOwnBase >= MAX_RANGE)
			continue;

		// calculate vehicle speed
		// for land units assuming there are roads everywhere

		double vehicleSpeed = (triad == TRIAD_LAND ? getLandVehicleSpeedOnRoads(id) : getVehicleSpeedWithoutRoads(id));

		// calculate threat time koefficient

		double threatTimeKoefficient = MIN_THREAT_TURNS / max(MIN_THREAT_TURNS, ((double)rangeToNearestOwnBase / vehicleSpeed));

		// calculate vehicle offensive strength

		double vehicleOffensiveStrength = (double)vehicleWeaponOffensiveValue;

		// adjust artillery value

		if (vehicle_has_ability(vehicle, ABL_ARTILLERY))
		{
			vehicleOffensiveStrength *= ARTILLERY_OFFENSIVE_VALUE_KOEFFICIENT;
		}

		// adjust vehicle strenght with faction coefficients

		double enemyOffensiveStrength = factionInfos[vehicle->faction_id].threatKoefficient * factionInfos[vehicle->faction_id].conventionalOffenseMultiplier * threatTimeKoefficient * vehicleOffensiveStrength;

		// update total

		totalEnemyOffensiveStrength += enemyOffensiveStrength;

		debug
		(
			"\t\t[%3d](%3d,%3d), %-25s, rangeToNearestOwnBase=%3d, threatTimeKoefficient=%4.2f, vehicleWeaponOffensiveValue=%2d, vehicleOffensiveStrength=%4.1f, enemyOffensiveStrength=%4.1f\n",
			id,
			vehicle->x,
			vehicle->y,
			tx_metafactions[vehicle->faction_id].noun_faction,
			rangeToNearestOwnBase,
			threatTimeKoefficient,
			vehicleWeaponOffensiveValue,
			vehicleOffensiveStrength,
			enemyOffensiveStrength
		);

	}

	// find total own defense value

	double totalOwnDefensiveStrength = 0.0;

	for (int id : combatVehicleIds)
	{
		VEH *vehicle = &(tx_vehicles[id]);

		// find vehicle armor defensive value

		int vehicleArmorDefensiveValue = tx_defense[tx_units[vehicle->proto_id].armor_type].defense_value;

		// calculate vehicle defensive strength

		double vehicleDefensiveStrength = (double)vehicleArmorDefensiveValue;

		// update total

		totalOwnDefensiveStrength += factionInfos[vehicle->faction_id].conventionalDefenseMultiplier * vehicleDefensiveStrength;

	}

	debug("totalEnemyOffensiveStrength=%.0f, totalOwnDefensiveStrength=%.0f, unitTypeProductionDemands[UT_DEFENSE]=%4.2f\n", round(totalEnemyOffensiveStrength), round(totalOwnDefensiveStrength), unitTypeProductionDemands[UT_DEFENSE]);
	debug("\n");

	// set defensive unit production demand

	if (totalOwnDefensiveStrength < totalEnemyOffensiveStrength)
	{
		double priority = (totalEnemyOffensiveStrength - totalOwnDefensiveStrength) / totalOwnDefensiveStrength;

		int baseDefender = findBaseDefenderUnit();
		int fastAttacker = findFastAttackerUnit();

		double baseDefenderPart = 0.75;

		for (int id : bestProductionBaseIds)
		{
			// random roll

			double randomRoll = (double)rand() / (double)(RAND_MAX + 1);

			// chose prodution item

			int item;

			if (randomRoll < baseDefenderPart)
			{
				item = baseDefender;
			}
			else
			{
				item = fastAttacker;
			}

			// add production demand

			addProductionDemand(id, false, item, priority);

		}

	}

}

/*
Calculates need for colonies.
*/
void calculateExpansionDemand()
{
	// calculate unpopulated tiles per region

	std::map<int, int> unpopulatedTileCounts;

	for (int x = 0; x < *map_axis_x; x++)
	{
		for (int y = 0; y < *map_axis_y; y++)
		{
			MAP *tile = getMapTile(x, y);

			if (tile == NULL)
				continue;

			// exclude territory claimed by someone else

			if (!(tile->owner == -1 || tile->owner == *active_faction))
				continue;

			// exclude base and base radius

			if (tile->items & (TERRA_BASE_IN_TILE | TERRA_BASE_RADIUS))
				continue;

			// inhabited regions only

			if (regionBases.find(tile->region) == regionBases.end())
				continue;

			// get region bases

			std::vector<int> *bases = &(regionBases[tile->region]);

			// calculate distance to nearest own base within region

			BASE *nearestBase = NULL;
			int nearestBaseRange = 0;

			for (int id : *bases)
			{
				BASE *base = &(tx_bases[id]);

				// calculate range to tile

				int range = map_range(x, y, base->x, base->y);

				if (nearestBase == NULL || range < nearestBaseRange)
				{
					nearestBase = base;
					nearestBaseRange = range;
				}

			}

			// exclude too far or uncertain tiles

			if (nearestBase == NULL || nearestBaseRange > conf.ai_production_max_unpopulated_range)
				continue;

			// count available tiles

			if (unpopulatedTileCounts.find(tile->region) == unpopulatedTileCounts.end())
			{
				unpopulatedTileCounts[tile->region] = 0;
			}

			unpopulatedTileCounts[tile->region]++;

		}

	}

	if (DEBUG)
	{
		debug("unpopulatedTileCounts\n");

		for
		(
			std::map<int, int>::iterator unpopulatedTileCountsIterator = unpopulatedTileCounts.begin();
			unpopulatedTileCountsIterator != unpopulatedTileCounts.end();
			unpopulatedTileCountsIterator++
		)
		{
			int region = unpopulatedTileCountsIterator->first;
			int unpopulatedTileCount = unpopulatedTileCountsIterator->second;

			debug("\t[%3d] %3d\n", region, unpopulatedTileCount);

		}

		debug("\n");

	}

	// set colony production demand per region

	for
	(
		std::map<int, std::vector<int>>::iterator regionBasesIterator = regionBases.begin();
		regionBasesIterator != regionBases.end();
		regionBasesIterator++
	)
	{
		int region = regionBasesIterator->first;
		std::vector<int> *regionBaseIds = &(regionBasesIterator->second);

		// evaluate need for expansion

		if (unpopulatedTileCounts.find(region) == unpopulatedTileCounts.end())
			continue;

		if (unpopulatedTileCounts[region] < conf.ai_production_min_unpopulated_tiles)
			continue;

		// select item

		int item = (isOceanRegion(region) ? BSC_SEA_ESCAPE_POD : BSC_COLONY_POD);

		for (int id : *regionBaseIds)
		{
			BASE *base = &(tx_bases[id]);

			// do not build colonies it bases size 1

			if (base->pop_size <= 1)
				continue;

			if (productionDemands.find(id) == productionDemands.end())
			{
				productionDemands[id] = PRODUCTION_DEMAND();
			}

			addProductionDemand(id, false, item, conf.ai_production_colony_priority);

		}

	}

}

/*
Set base production choices.
*/
void setProductionChoices()
{
	for
	(
		std::map<int, PRODUCTION_DEMAND>::iterator productionDemandsIterator = productionDemands.begin();
		productionDemandsIterator != productionDemands.end();
		productionDemandsIterator++
	)
	{
		int id = productionDemandsIterator->first;
		PRODUCTION_DEMAND *baseProductionDemands = &(productionDemandsIterator->second);

		// immediate priorities

		if (baseProductionDemands->immediate)
		{
			// create immediate production choice

			productionChoices[id] = {baseProductionDemands->immediateItem, true};

		}

		// not immediate priorities

		else if (baseProductionDemands->regular.size() > 0)
		{
			std::vector<PRODUCTION_PRIORITY> *regularBaseProductionPriorities = &(baseProductionDemands->regular);

			// summarize priorities

			double sumPriorities = 0.0;

			for
			(
				std::vector<PRODUCTION_PRIORITY>::iterator regularBaseProductionPrioritiesIterator = regularBaseProductionPriorities->begin();
				regularBaseProductionPrioritiesIterator != regularBaseProductionPriorities->end();
				regularBaseProductionPrioritiesIterator++
			)
			{
				PRODUCTION_PRIORITY *productionPriority = &(*regularBaseProductionPrioritiesIterator);

				sumPriorities += productionPriority->priority;

			}

			// normalize priorities

			if (sumPriorities > 1.0)
			{
				for
				(
					std::vector<PRODUCTION_PRIORITY>::iterator regularBaseProductionPrioritiesIterator = regularBaseProductionPriorities->begin();
					regularBaseProductionPrioritiesIterator != regularBaseProductionPriorities->end();
					regularBaseProductionPrioritiesIterator++
				)
				{
					PRODUCTION_PRIORITY *productionPriority = &(*regularBaseProductionPrioritiesIterator);

					productionPriority->priority /= sumPriorities;

				}

			}

			// random roll

			double randomRoll = (double)rand() / (double)(RAND_MAX + 1);

			// choose item

			for
			(
				std::vector<PRODUCTION_PRIORITY>::iterator regularBaseProductionPrioritiesIterator = regularBaseProductionPriorities->begin();
				regularBaseProductionPrioritiesIterator != regularBaseProductionPriorities->end();
				regularBaseProductionPrioritiesIterator++
			)
			{
				PRODUCTION_PRIORITY *productionPriority = &(*regularBaseProductionPrioritiesIterator);

				if (randomRoll < productionPriority->priority)
				{
					// create non-immediate production choice

					productionChoices[id] = {productionPriority->item, false};

					break;

				}
				else
				{
					randomRoll -= productionPriority->priority;
				}

			}

		}

	}

	if (DEBUG)
	{
		debug("productionDemands:\n");

		for
		(
			std::map<int, PRODUCTION_DEMAND>::iterator productionDemandsIterator = productionDemands.begin();
			productionDemandsIterator != productionDemands.end();
			productionDemandsIterator++
		)
		{
			int id = productionDemandsIterator->first;
			PRODUCTION_DEMAND *baseProductionDemands = &(productionDemandsIterator->second);

			debug("\t%-25s\n", tx_bases[id].name);

			debug("\t\timmediate\n");

			if (baseProductionDemands->immediate)
			{
				debug("\t\t\t%-25s %4.2f\n", prod_name(baseProductionDemands->immediateItem), baseProductionDemands->immediatePriority);

			}

			debug("\t\tregular\n");

			for
			(
				std::vector<PRODUCTION_PRIORITY>::iterator immediateBaseProductionPrioritiesIterator = baseProductionDemands->regular.begin();
				immediateBaseProductionPrioritiesIterator != baseProductionDemands->regular.end();
				immediateBaseProductionPrioritiesIterator++
			)
			{
				PRODUCTION_PRIORITY *productionPriority = &(*immediateBaseProductionPrioritiesIterator);

				debug("\t\t\t%-25s %4.2f\n", prod_name(productionPriority->item), productionPriority->priority);

			}

			debug("\t\tselected\n");

			if (productionChoices.find(id) != productionChoices.end())
			{
				debug("\t\t\t%-25s %s\n", prod_name(productionChoices[id].item), (productionChoices[id].immediate ? "immediate" : "regular"));
			}

		}

		debug("\n");

	}

}

double getFactionConventionalOffenseMultiplier(int id)
{
	double factionOffenseMultiplier = 1.0;

	MetaFaction *metaFaction = &(tx_metafactions[id]);

	// faction bonus

	for (int bonusIndex = 0; bonusIndex < metaFaction->faction_bonus_count; bonusIndex++)
	{
		int bonusId = metaFaction->faction_bonus_id[bonusIndex];

		if (bonusId == FCB_OFFENSE)
		{
			factionOffenseMultiplier *= ((double)metaFaction->faction_bonus_val1[bonusIndex] / 100.0);
		}

	}

	// fanatic bonus

	if (metaFaction->rule_flags & FACT_FANATIC)
	{
		factionOffenseMultiplier *= (1.0 + (double)tx_basic->combat_bonus_fanatic / 100.0);
	}

	return factionOffenseMultiplier;

}

double getFactionConventionalDefenseMultiplier(int id)
{
	double factionDefenseMultiplier = 1.0;

	MetaFaction *metaFaction = &(tx_metafactions[id]);

	// faction bonus

	for (int bonusIndex = 0; bonusIndex < metaFaction->faction_bonus_count; bonusIndex++)
	{
		int bonusId = metaFaction->faction_bonus_id[bonusIndex];

		if (bonusId == FCB_DEFENSE)
		{
			factionDefenseMultiplier *= ((double)metaFaction->faction_bonus_val1[bonusIndex] / 100.0);
		}

	}

	return factionDefenseMultiplier;

}

int findRangeToNearestOwnBase(int x, int y, int region)
{
	int rangeToNearestOwnBase = MAX_RANGE;

	// iterate through own bases to find the closest

	for (std::pair<MAP *, int> element : baseLocations)
	{
		MAP *location = element.first;
		int id = element.second;
		BASE *base = &(tx_bases[id]);

		// skip bases in other region

		if (region != -1 && location->region != region)
			continue;

		// calculate range

		int range = map_range(base->x, base->y, x, y);

		// find minimal range

		rangeToNearestOwnBase = min(rangeToNearestOwnBase, range);

	}

	return rangeToNearestOwnBase;

}

/*
Selects base production.
*/
int suggestBaseProduction(int id, bool baseProductionDone, int choice)
{
	debug("selectBaseProduction: %-25s, productionDone=%s\n", tx_bases[id].name, (baseProductionDone ? "y" : "n"));

	std::map<int, PRODUCTION_CHOICE>::iterator productionChoicesIterator = productionChoices.find(id);

	if (productionChoicesIterator != productionChoices.end())
	{
		PRODUCTION_CHOICE *productionChoice = &(productionChoicesIterator->second);

		debug("\t%-25s\n", prod_name(productionChoice->item));

		if (productionChoice->immediate || baseProductionDone)
		{
			choice = productionChoice->item;
			debug("\t\tselected\n");
		}

	}

	debug("\n");

	return choice;

}

void addProductionDemand(int id, bool immediate, int item, double priority)
{
	// create entry if not created yet

	if (productionDemands.find(id) == productionDemands.end())
	{
		productionDemands[id] = PRODUCTION_DEMAND();
	}

	PRODUCTION_DEMAND *productionDemand = &(productionDemands[id]);

	if (immediate)
	{
		if (!productionDemand->immediate)
		{
			productionDemand->immediate = true;
			productionDemand->immediateItem = item;
			productionDemand->immediatePriority = priority;
		}
		else if (priority > productionDemand->immediatePriority)
		{
			productionDemand->immediateItem = item;
			productionDemand->immediatePriority = priority;
		}
	}
	else
	{
		// lookup for existing item

		bool found = false;

		for
		(
			std::vector<PRODUCTION_PRIORITY>::iterator productionPrioritiesIterator = productionDemand->regular.begin();
			productionPrioritiesIterator != productionDemand->regular.end();
			productionPrioritiesIterator++
		)
		{
			PRODUCTION_PRIORITY *productionPriority = &(*productionPrioritiesIterator);

			if (productionPriority->item == item)
			{
				productionPriority->priority = max(productionPriority->priority, priority);
				found = true;
				break;
			}

		}

		if (!found)
		{
			productionDemand->regular.push_back({item, priority});
		}

	}

}

int findStrongestNativeDefensePrototype(int factionId)
{
	// initial prototype

    int bestUnitId = BSC_SCOUT_PATROL;
    UNIT *bestUnit = &(tx_units[bestUnitId]);

    for (int i = 0; i < 128; i++)
	{
        int id = (i < 64 ? i : (factionId - 1) * 64 + i);

        UNIT *unit = &tx_units[id];

		// skip current

		if (id == bestUnitId)
			continue;

        // skip empty

        if (strlen(unit->name) == 0)
			continue;

		// skip not enabled

		if (id < 64 && !has_tech(factionId, unit->preq_tech))
			continue;

		// skip non land

		if (unit_triad(id) != TRIAD_LAND)
			continue;

		// prefer trance or police unit

		if
		(
			((unit->ability_flags & ABL_TRANCE) && (~bestUnit->ability_flags & ABL_TRANCE))
			||
			((unit->ability_flags & ABL_POLICE_2X) && (~bestUnit->ability_flags & ABL_POLICE_2X))
		)
		{
			bestUnitId = id;
			bestUnit = unit;
		}

	}

    return bestUnitId;

}

int findBaseDefenderUnit()
{
	int bestUnitId = BSC_SCOUT_PATROL;
	double bestUnitDefenseEffectiveness = evaluateUnitDefenseEffectiveness(bestUnitId);

	for (int id : prototypes)
	{
		// skip non combat units

		if (!isCombatUnit(id))
			continue;

		// skip air units

		if (unit_triad(id) == TRIAD_AIR)
			continue;

		double unitDefenseEffectiveness = evaluateUnitDefenseEffectiveness(id);

		if (bestUnitId == -1 || unitDefenseEffectiveness > bestUnitDefenseEffectiveness)
		{
			bestUnitId = id;
			bestUnitDefenseEffectiveness = unitDefenseEffectiveness;
		}

	}

	return bestUnitId;

}

int findFastAttackerUnit()
{
	int bestUnitId = BSC_SCOUT_PATROL;
	double bestUnitOffenseEffectiveness = evaluateUnitOffenseEffectiveness(bestUnitId);

	for (int id : prototypes)
	{
		UNIT *unit = &(tx_units[id]);

		// skip non combat units

		if (!isCombatUnit(id))
			continue;

		// skip infantry units

		if (unit->chassis_type == CHS_INFANTRY)
			continue;

		double unitOffenseEffectiveness = evaluateUnitOffenseEffectiveness(id);

		if (bestUnitId == -1 || unitOffenseEffectiveness > bestUnitOffenseEffectiveness)
		{
			bestUnitId = id;
			bestUnitOffenseEffectiveness = unitOffenseEffectiveness;
		}

	}

	return bestUnitId;

}

