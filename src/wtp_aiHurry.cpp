
#include "wtp_aiHurry.h"

#include <algorithm>
#include <vector>
#include <set>
#include "robin_hood.h"

#include "main.h"
#include "engine.h"
#include "wtp_game.h"

const robin_hood::unordered_flat_set<int> IMPORTANT_FACILITIES =
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
    debug("considerHurryingProduction - %s\n", MFactions[factionId].noun_faction);

	// apply only when flat hurry cost is enabled

	if (!conf.flat_hurry_cost)
		return;

	// calculate spend pool

	int reserve = 20 + std::min(900, std::max(0, faction->base_count * std::min(30, (*CurrentTurn - 20)/5)));
	int spendPool = std::max(0, faction->energy_credits - reserve);
    debug("\treserve=%d, spendPool=%d\n", reserve, spendPool);

	// apply only when there is something to spend

	if (spendPool <= 0)
		return;

	// sort out bases

	double unprotectedBasesWeightSum = 0.0;
	std::vector<BASE_WEIGHT> unprotectedBases;
	double importantFacilityBasesWeightSum = 0.0;
	std::vector<BASE_WEIGHT> importantFacilityBases;
	double facilityBasesWeightSum = 0.0;
	std::vector<BASE_WEIGHT> facilityBases;
	double unitBasesWeightSum = 0.0;
	std::vector<BASE_WEIGHT> unitBases;

	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		BASE *base = &Bases[baseId];

		// skip not own bases

		if (base->faction_id != factionId)
			continue;

		// check if base is unprotected

		bool unprotected = (getBaseInfantryDefenderGarrison(baseId).size() == 0);

		// get built item

		int item = base->queue_items[0];

		// get base mineral surplus

		int mineralSurplus = base->mineral_surplus;

		// do not rush unit production in bases with low mineral surplus unless unprotected

		if (!unprotected && item >= 0 && mineralSurplus < conf.ai_production_unit_min_mineral_surplus)
			continue;

		// calculate weight

		double weight = 1.0 / (double)(std::max(1, mineralSurplus));

		// sort bases

		if (unprotected && item >= 0 && isCombatUnit(item))
		{
			unprotectedBases.push_back({baseId, weight});
			unprotectedBasesWeightSum += weight;
		}
		else if (item >= 0)
		{
			// rush unit production in bases with enough mineral surplus

			if (mineralSurplus >= conf.ai_production_unit_min_mineral_surplus)
			{
				unitBases.push_back({baseId, weight});
				unitBasesWeightSum += weight;
			}

		}
		else if (item >= -SP_ID_Last && item <= -SP_ID_First)
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

	if (unprotectedBases.size() >= 1 && unprotectedBasesWeightSum > 0.0)
	{
		debug("\tunprotectedBases\n");

		double spendPortion = 1.0;

		for (BASE_WEIGHT &baseWeight : unprotectedBases)
		{
			int allowance = (int)(floor(spendPortion * (double)spendPool * baseWeight.weight / unprotectedBasesWeightSum));
			hurryProductionPartially(baseWeight.baseId, allowance);
			debug("\t\t%-25s allowance=%d, spendPool=%d, relativeWeight=%5.2f\n", Bases[baseWeight.baseId].name, allowance, spendPool, baseWeight.weight / unprotectedBasesWeightSum);

		}

	}
	else if (importantFacilityBases.size() >= 1 && importantFacilityBasesWeightSum > 0.0)
	{
		double spendPortion = 0.5;

		for
		(
			std::vector<BASE_WEIGHT>::iterator importantFacilityBasesIterator = importantFacilityBases.begin();
			importantFacilityBasesIterator != importantFacilityBases.end();
			importantFacilityBasesIterator++
		)
		{
			BASE_WEIGHT *baseWeight = &(*importantFacilityBasesIterator);

			int allowance = (int)(floor(spendPortion * (double)spendPool * baseWeight->weight / importantFacilityBasesWeightSum));
			hurryProductionPartially(baseWeight->baseId, allowance);
		}

	}
	else if (facilityBases.size() >= 1 && facilityBasesWeightSum > 0.0)
	{
		double spendPortion = 0.2;

		for
		(
			std::vector<BASE_WEIGHT>::iterator facilityBasesIterator = facilityBases.begin();
			facilityBasesIterator != facilityBases.end();
			facilityBasesIterator++
		)
		{
			BASE_WEIGHT *baseWeight = &(*facilityBasesIterator);

			int allowance = (int)(floor(spendPortion * (double)spendPool * baseWeight->weight / facilityBasesWeightSum));
			hurryProductionPartially(baseWeight->baseId, allowance);
		}

	}
	else if (unitBases.size() >= 1 && unitBasesWeightSum > 0.0)
	{
		double spendPortion = 0.2;

		for
		(
			std::vector<BASE_WEIGHT>::iterator unitBasesIterator = unitBases.begin();
			unitBasesIterator != unitBases.end();
			unitBasesIterator++
		)
		{
			BASE_WEIGHT *baseWeight = &(*unitBasesIterator);

			int allowance = (int)(floor(spendPortion * (double)spendPool * baseWeight->weight / unitBasesWeightSum));
			hurryProductionPartially(baseWeight->baseId, allowance);
		}

	}

}

/*
Spend maximal available amount not greater than allowance on hurrying production.
Works only when flat hurry cost is enabled.
*/
void hurryProductionPartially(int baseId, int allowance)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	
	BASE *base = &(Bases[baseId]);
	
	// verify allowance is positive
	
	if (allowance <= 0)
		return;
	
	// get remaining minerals
	
	int remainingMinerals = getRemainingMinerals(baseId);
	
	// already completed
	
	if (remainingMinerals <= 0)
		return;
	
	// get hurry cost
	
	int hurryCost = hurry_cost(baseId, base->queue_items[0], remainingMinerals);
	
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
		partialHurryCost = partialHurryMinerals * hurryCost / remainingMinerals;
	}
	
	// hurry production
	
	hurryProduction(base, partialHurryMinerals, partialHurryCost);
	
}

