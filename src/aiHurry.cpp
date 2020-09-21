#include <vector>
#include <unordered_set>
#include "aiHurry.h"

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
It kicks in only with simplified hurry cost set.
*/
void factionHurryProduction(int factionId)
{
    Faction* faction = &tx_factions[factionId];
    debug("factionHurryProduction\n")

	// apply only when simplified hurry is enabled

	if (!(conf.disable_hurry_penalty_threshold && conf.alternative_unit_hurry_formula))
		return;

	// calculate spend pool

	int reserve = 20 + min(900, max(0, faction->current_num_bases * min(30, (*current_turn - 20)/5)));
	int spendPool = max(0, faction->energy_credits - reserve);

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
		BASE *base = &tx_bases[baseId];

		// skip not own bases

		if (base->faction_id != factionId)
			continue;

		// get built item

		int item = base->queue_items[0];

		// get base mineral surplus

		int mineralSurplus = base->mineral_surplus;

		// calculate weight

		double weight = 1.0 / (double)(max(1, mineralSurplus));

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
Works only with simplified hurry cost calculation enabled.
*/
void hurryProductionPartially(int baseId, int allowance)
{
	BASE *base = &(tx_bases[baseId]);

	// apply only when simplified hurry cost calculation is enabled

	if (!(conf.disable_hurry_penalty_threshold && conf.alternative_unit_hurry_formula))
		return;

	// get item and minerals left to build

	int itemId = base->queue_items[0];
	int itemMineralCost = mineral_cost(base->faction_id, itemId, baseId);

	// calculate remaining minerals

	int fullMinerals = itemMineralCost - base->minerals_accumulated;

	// already completed

	if (fullMinerals <= 0)
		return;

	// calculate hurry mineral cost

	int hurryMineralCost;

	// unit
	if (itemId >= 0)
	{
		hurryMineralCost = 4;
	}
	// project
	else if (itemId >= -PROJECT_ID_LAST && itemId <= -PROJECT_ID_FIRST)
	{
		hurryMineralCost = 4;
	}
	// facility
	else
	{
		hurryMineralCost = 2;
	}

	// calculate minerals and credits to hurry

	int fullCredits = fullMinerals * hurryMineralCost;

	int hurryMinerals;
	int hurryCredits;

	if (allowance >= fullCredits)
	{
		hurryMinerals = fullMinerals;
		hurryCredits = fullCredits;
	}
	else
	{
		hurryMinerals = allowance / hurryMineralCost;
		hurryCredits = hurryMinerals * hurryMineralCost;
	}

	// hurry production

	hurry_item(base, hurryMinerals, hurryCredits);

}

