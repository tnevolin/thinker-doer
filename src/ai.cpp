#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "ai.h"
#include "wtp.h"
#include "game.h"
#include "aiProduction.h"
#include "aiTerraforming.h"

// global variables

// Controls which faction uses WtP algorithm.
// for debugging
// -1 : all factions

int wtpAIFactionId = -1;

// global variables for faction upkeep

int aiFactionId;

std::vector<int> baseIds;
std::unordered_map<MAP *, int> baseLocations;
std::map<int, BASE_STRATEGY> baseStrategies;
std::vector<int> combatVehicleIds;
std::vector<int> outsideCombatVehicleIds;

/*
AI strategy.
*/
void aiStrategy(int id)
{
	// no natives

	if (id == 0)
		return;

	// set faction

	aiFactionId = id;

	// use WTP algorithm for selected faction only

	if (wtpAIFactionId != -1 && aiFactionId != wtpAIFactionId)
		return;

	debug("aiStrategy: aiFactionId=%d\n", aiFactionId);

	// populate shared strategy lists

	populateSharedLists();

	// prepare production orders

	aiProductionStrategy();

	// prepare former orders

	aiTerraformingStrategy();

	// prepare combat orders

	aiCombatStrategy();

}

void populateSharedLists()
{
	// clear lists

	baseIds.clear();
	baseLocations.clear();
	baseStrategies.clear();
	combatVehicleIds.clear();
	outsideCombatVehicleIds.clear();

	// populate bases

	debug("baseStrategies\n");

	for (int id = 0; id < *total_num_bases; id++)
	{
		BASE *base = &(tx_bases[id]);

		// exclude not own bases

		if (base->faction_id != aiFactionId)
			continue;

		// add base

		baseIds.push_back(id);

		// add base location

		MAP *baseLocation = getMapTile(base->x, base->y);

		baseLocations[baseLocation] = id;

		// add base strategy

		baseStrategies[id] = {};
		baseStrategies[id].base = base;

		debug("\n[%3d] %-25s\n", id, baseStrategies[id].base->name);

	}

	debug("\n");

	// populate vehicles

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(tx_vehicles[id]);

		// store all vehicle current id in pad_0 field

		vehicle->pad_0 = id;

		// further process only own vehicles

		if (vehicle->faction_id != aiFactionId)
			continue;

		// combat vehicles

		if (isCombatVehicle(id))
		{
			// add vehicle

			combatVehicleIds.push_back(id);

			// find if vehicle is at base

			MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);
			std::unordered_map<MAP *, int>::iterator baseLocationsIterator = baseLocations.find(vehicleLocation);

			if (baseLocationsIterator == baseLocations.end())
			{
				// add outside vehicle

				outsideCombatVehicleIds.push_back(id);

			}
			else
			{
				BASE_STRATEGY *baseStrategy = &(baseStrategies[baseLocationsIterator->second]);

				// add to garrison

				baseStrategy->garrison.push_back(id);

				// add to native protection

				double nativeProtection = calculateNativeDamageDefense(id) / 10.0;

				if (vehicle_has_ability(vehicle, ABL_TRANCE))
				{
					nativeProtection *= (1 + (double)tx_basic->combat_bonus_trance_vs_psi / 100.0);
				}

				baseStrategy->nativeProtection += nativeProtection;

			}

		}

	}

}

VEH *getVehicleByAIId(int aiId)
{
	// check if ID didn't change

	VEH *oldVehicle = &(tx_vehicles[aiId]);

	if (oldVehicle->pad_0 == aiId)
		return oldVehicle;

	// otherwise, scan all vehicles

	for (int id = 0; id < *total_num_vehicles; id++)
	{
		VEH *vehicle = &(tx_vehicles[id]);

		if (vehicle->pad_0 == aiId)
			return vehicle;

	}

	return NULL;

}

bool isReacheable(int id, int x, int y)
{
	VEH *vehicle = &(tx_vehicles[id]);
	int triad = veh_triad(id);
	MAP *vehicleLocation = getMapTile(vehicle->x, vehicle->y);
	MAP *destinationLocation = getMapTile(x, y);

	return (triad == TRIAD_AIR || getConnectedRegion(vehicleLocation->region) == getConnectedRegion(destinationLocation->region));

}

