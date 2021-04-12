#include <algorithm>
#include <vector>
#include <unordered_set>
#include "aiHurry.h"
#include "wtp.h"
#include "game_wtp.h"

const std::unordered_set<int> IMPORTANT_FACILITIES =
{
	FAC_RECYCLING_TANKS,
	FAC_RECREATION_COMMONS,
	FAC_HOLOGRAM_THEATRE,
	FAC_HAB_COMPLEX,
	FAC_HABITATION_DOME,
};

/*
Analyzes all bases at once and decides what to hurry.
This is in addition to consider_hurry().
It kicks in only when flat hurry cost is enabled.
*/
void considerHurryingProduction(int factionId)
{
    Faction* faction = &Factions[factionId];
    debug("considerHurryingProduction\n")

	// apply only when flat hurry cost is enabled

	if (!conf.flat_hurry_cost)
		return;

	// calculate spend pool

	int reserve = 20 + std::min(900, std::max(0, faction->current_num_bases * std::min(30, (*current_turn - 20)/5)));
	int spendPool = std::max(0, faction->energy_credits - reserve);

	// apply only when there is something to spend

	if (spendPool == 0)
		return;

	// sort out bases

	double importantFacilityBasesWeightSum = 0.0;
	std::vector<BASE_WEIGHT> importantFacilityBases;
	double facilityBasesWeightSum = 0.0;
	std::vector<BASE_WEIGHT> facilityBases;
	double unitBasesWeightSum = 0.0;
	std::vector<BASE_WEIGHT> unitBases;

	for (int baseId = 0; baseId < *total_num_bases; baseId++)
	{
		BASE *base = &Bases[baseId];

		// skip not own bases

		if (base->faction_id != factionId)
			continue;

		// get built item

		int item = base->queue_items[0];

		// get base mineral surplus

		int mineralSurplus = base->mineral_surplus;

		// calculate weight

		double weight = 1.0 / (double)(std::max(1, mineralSurplus));

		// sort bases

		if (item >= 0)
		{
			// hurry unit only with positive production

			if (mineralSurplus >= 1)
			{
				unitBases.push_back({baseId, weight});
				unitBasesWeightSum += weight;
			}

		}
		else if (item >= -PROJECT_ID_LAST && item <= -PROJECT_ID_FIRST)
		{
			importantFacilityBases.push_back({baseId, weight});
			importantFacilityBasesWeightSum += weight;
		}
		else
		{
			if (IMPORTANT_FACILITIES.count(-item))
			{
				importantFacilityBases.push_back({baseId, weight});
				importantFacilityBasesWeightSum += weight;
			}
			else
			{
				facilityBases.push_back({baseId, weight});
				facilityBasesWeightSum += weight;
			}

		}

	}

	// hurry production by priority

	if (importantFacilityBases.size() >= 1)
	{
		for
		(
			std::vector<BASE_WEIGHT>::iterator importantFacilityBasesIterator = importantFacilityBases.begin();
			importantFacilityBasesIterator != importantFacilityBases.end();
			importantFacilityBasesIterator++
		)
		{
			BASE_WEIGHT *baseWeight = &(*importantFacilityBasesIterator);

			int allowance = (int)(floor((double)spendPool * baseWeight->weight / importantFacilityBasesWeightSum));
			hurryProductionPartially(baseWeight->baseId, allowance);
		}

	}
	else if (facilityBases.size() >= 1)
	{
		for
		(
			std::vector<BASE_WEIGHT>::iterator facilityBasesIterator = facilityBases.begin();
			facilityBasesIterator != facilityBases.end();
			facilityBasesIterator++
		)
		{
			BASE_WEIGHT *baseWeight = &(*facilityBasesIterator);

			int allowance = (int)(floor((double)spendPool * baseWeight->weight / facilityBasesWeightSum));
			hurryProductionPartially(baseWeight->baseId, allowance);
		}

	}
	else if (unitBases.size() >= 1)
	{
		for
		(
			std::vector<BASE_WEIGHT>::iterator unitBasesIterator = unitBases.begin();
			unitBasesIterator != unitBases.end();
			unitBasesIterator++
		)
		{
			BASE_WEIGHT *baseWeight = &(*unitBasesIterator);

			int allowance = (int)(floor((double)spendPool * baseWeight->weight / unitBasesWeightSum));
			hurryProductionPartially(baseWeight->baseId, allowance);
		}

	}

    debug("\n")

}

/*
Spend maximal available amount not greater than allowance on hurrying production.
Works only when flat hurry cost is enabled.
*/
void hurryProductionPartially(int baseId, int allowance)
{
	BASE *base = &(Bases[baseId]);

	// apply only when flat hurry cost is enabled

	if (!conf.flat_hurry_cost)
		return;

	// get remaining minerals

	int remainingMinerals = getRemainingMinerals(baseId);

	// already completed

	if (remainingMinerals <= 0)
		return;

	// get flat hurry cost

	int hurryCost = getFlatHurryCost(baseId);

	// nothing to hurry

	if (hurryCost <= 0)
		return;

	// calculate minerals and hurry price

	int partialHurryMinerals;
	int partialHurryCost;

	if (allowance >= hurryCost)
	{
		partialHurryMinerals = remainingMinerals;
		partialHurryCost = hurryCost;
	}
	else
	{
		partialHurryMinerals = remainingMinerals * allowance / hurryCost;
		partialHurryCost = allowance;
	}

	// hurry production

	hurryProduction(base, partialHurryMinerals, partialHurryCost);

}

