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

FACTION_INFO factionInfos[8];
std::vector<int> baseIds;
std::unordered_map<MAP *, int> baseLocations;
std::map<int, std::vector<int>> regionBases;
std::map<int, BASE_STRATEGY> baseStrategies;
std::vector<int> combatVehicleIds;
std::vector<int> outsideCombatVehicleIds;

/*
AI strategy.
*/
void aiStrategy(int id)
{
	// set faction

	aiFactionId = id;

	// no natives

	if (id == 0)
		return;

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
	regionBases.clear();
	baseStrategies.clear();
	combatVehicleIds.clear();
	outsideCombatVehicleIds.clear();

	// populate factions combat modifiers

	debug("%-24s\nfactionCombatModifiers:\n", tx_metafactions[aiFactionId].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// store combat modifiers

		factionInfos[id].conventionalOffenseMultiplier = getFactionConventionalOffenseMultiplier(id);
		factionInfos[id].conventionalDefenseMultiplier = getFactionConventionalDefenseMultiplier(id);

		debug("\t%-24s: conventionalOffenseMultiplier=%4.2f, conventionalDefenseMultiplier=%4.2f\n", tx_metafactions[id].noun_faction, factionInfos[id].conventionalOffenseMultiplier, factionInfos[id].conventionalDefenseMultiplier);

	}

	debug("\n");

	// populate other factions threat koefficients

	debug("%-24s\notherFactionThreatKoefficients:\n", tx_metafactions[aiFactionId].noun_faction);

	for (int id = 1; id < 8; id++)
	{
		// skip self

		if (id == aiFactionId)
			continue;

		// get relation from other faction

		int otherFactionRelation = tx_factions[id].diplo_status[aiFactionId];

		// calculate threat koefficient

		double threatKoefficient;

		if (otherFactionRelation & DIPLO_VENDETTA)
		{
			threatKoefficient = 1.00;
		}
		else if (otherFactionRelation & DIPLO_TRUCE)
		{
			threatKoefficient = 0.50;
		}
		else if (otherFactionRelation & DIPLO_TREATY)
		{
			threatKoefficient = 0.25;
		}
		else if (otherFactionRelation & DIPLO_PACT)
		{
			threatKoefficient = 0.00;
		}
		else
		{
			// no commlink or unknown status
			threatKoefficient = 0.50;
		}

		// add extra for treacherous human player

		if (is_human(id))
		{
			threatKoefficient += 0.50;
		}

		// store threat koefficient

		factionInfos[id].threatKoefficient = threatKoefficient;

		debug("\t%-24s: %08x => %4.2f\n", tx_metafactions[id].noun_faction, otherFactionRelation, threatKoefficient);

	}

	debug("\n");

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

		// add base regions

		std::set<int> baseConnectedRegions = getBaseConnectedRegions(id);

		for (int region : baseConnectedRegions)
		{
			if (regionBases.find(region) == regionBases.end())
			{
				regionBases[region] = std::vector<int>();
			}

			regionBases[region].push_back(id);

		}

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

/*
Estimates unit odds in base defense agains land native attack.
This doesn't account for vehicle morale.
*/
double estimateUnitBaseLandNativeProtection(int factionId, int unitId)
{
	// basic defense odds against land native attack

	double nativeProtection = 1 / getPsiCombatBaseOdds(TRIAD_LAND);

	// add base defense bonus

	nativeProtection *= 1.0 + (double)tx_basic->combat_bonus_intrinsic_base_def / 100.0;

	// add faction defense bonus

	nativeProtection *= factionInfos[factionId].conventionalDefenseMultiplier;

	// add PLANET

	nativeProtection *= getSEPlanetModifierDefense(factionId);

	// add trance

	if (unit_has_ability(unitId, ABL_TRANCE))
	{
		nativeProtection *= 1.0 + (double)tx_basic->combat_bonus_trance_vs_psi / 100.0;
	}

	// correction for native base attack penalty until turn 50

	if (*current_turn < 50)
	{
		nativeProtection *= 2;
	}

	return nativeProtection;

}

/*
Estimates vehicle odds in base defense agains land native attack.
This accounts for vehicle morale.
*/
double estimateVehicleBaseLandNativeProtection(int factionId, int vehicleId)
{
	// unit native protection

	double nativeProtection = estimateUnitBaseLandNativeProtection(factionId, tx_vehicles[vehicleId].proto_id);

	// add morale

	nativeProtection *= getMoraleModifierDefense(vehicleId);

	return nativeProtection;

}

