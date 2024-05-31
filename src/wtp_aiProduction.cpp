#include <math.h>
#include <set>
#include <map>
#include "wtp_aiProduction.h"
#include "engine.h"
#include "game.h"
#include "wtp_game.h"
#include "wtp_terranx.h"
#include "wtp_ai.h"
#include "wtp_aiData.h"
#include "wtp_aiMoveColony.h"
#include "wtp_aiRoute.h"
#include "wtp_aiHurry.h"

// global parameters

double normalRawProductionPriority;

double globalBaseDemand;
double baseColonyDemandMultiplier;
robin_hood::unordered_flat_map<int, double> weakestEnemyBaseProtection;

int selectedProject = -1;

// currently processing base production demand

ProductionDemand productionDemand;

// ProductionDemand

void ProductionDemand::initialize(int _baseId)
{
	baseId = _baseId;
	base = getBase(_baseId);
	priorities.clear();
	colonyCoefficient = 1.0;
}

void ProductionDemand::add(int item, double priority, bool upkeep)
{
	if (item >= 0) // unit
	{
		// upkeepPriority

		double upkeepPriority = 0.0;

		if (upkeep && item != - FAC_STOCKPILE_ENERGY)
		{
			int cost = getBaseItemCost(baseId, item);
			int buildTime = getBaseItemBuildTime(baseId, item, true);
			double upkeepScore = getResourceScore(-1, 0);

			double upkeepGain = getGain(buildTime, 0.0, upkeepScore, 0.0);
			upkeepPriority = getProductionPriority(upkeepGain, cost);

		}

		// combined priority

		double combinedPriority = upkeepPriority + priority;

		// unit do not accumulate priority from different purposes

		priorities[item] = std::max(priorities[item], combinedPriority);

	}
	else // facility
	{
		// insert entry with upkeep priority if none exists

		if (priorities.count(item) == 0)
		{
			// upkeep priority

			double upkeepPriority = 0.0;

			if (upkeep && item != - FAC_STOCKPILE_ENERGY)
			{
				int cost = getBaseItemCost(baseId, item);
				int buildTime = getBaseItemBuildTime(baseId, item, true);
				double upkeepScore = item >= 0 ? getResourceScore(-1, 0) : getResourceScore(0, -getFacility(-item)->maint);

				double upkeepGain = getGain(buildTime, 0.0, upkeepScore, 0.0);
				upkeepPriority = getProductionPriority(upkeepGain, cost);

			}

			priorities.insert({item, upkeepPriority});

		}

		// add priority to item entry

		priorities[item] += priority;

	}

	debug("\t%5.2f = %+6.2f\n", priority, priorities[item]);

}

double ProductionDemand::getItemPriority(int item)
{
	return (priorities.count(item) == 0 ? 0.0 : priorities.at(item));
}

ItemPriority ProductionDemand::get()
{
	ItemPriority bestItemPriority;

	for (robin_hood::pair<int, double> &priorityEntry : priorities)
	{
		int item = priorityEntry.first;
		double priority = priorityEntry.second;

		if (priority > bestItemPriority.priority)
		{
			bestItemPriority.item = item;
			bestItemPriority.priority = priority;
		}

	}

	return bestItemPriority;

}

/*
Prepare production choices.
*/
void productionStrategy()
{
	// set normal value

	normalRawProductionPriority = getColonyRawProductionPriority();

	// evaluate global demands
	// populate global variables

	profileFunction("1.4.1. evaluateGlobalBaseDemand", evaluateGlobalBaseDemand);
	profileFunction("1.4.2. evaluateGlobalFormerDemand", evaluateGlobalFormerDemand);
	profileFunction("1.4.3. evaluateGlobalProtectionDemand", evaluateGlobalProtectionDemand);
	profileFunction("1.4.4. evaluateGlobalPoliceDemand", evaluateGlobalPoliceDemand);
	profileFunction("1.4.5. evaluateGlobalLandCombatDemand", evaluateGlobalLandCombatDemand);
	profileFunction("1.4.6. evaluateGlobalTerritoryLandCombatDemand", evaluateGlobalTerritoryLandCombatDemand);
	profileFunction("1.4.7. evaluateGlobalSeaCombatDemand", evaluateGlobalSeaCombatDemand);
	profileFunction("1.4.8. evaluateGlobalLandAlienCombatDemand", evaluateGlobalLandAlienCombatDemand);

	// set production

	profileFunction("1.4.9. setProduction", setProduction);

	// build project

	profileFunction("1.4.A. selectProject", selectProject);
	profileFunction("1.4.B. assignProject", assignProject);

	// hurry protective unit production

	profileFunction("1.4.C. hurryProtectiveUnit", hurryProtectiveUnit);

}

/*
Evaluates need for new bases based on current inefficiency.
*/
void evaluateGlobalBaseDemand()
{
	debug("evaluateGlobalBaseDemand - %s\n", MFactions[aiFactionId].noun_faction);

	Faction *faction = getFaction(aiFactionId);

	// set default global base demand multiplier

	globalBaseDemand = 1.0;

	// do not compute if there are no bases to produce anything

	if (aiData.baseIds.size() == 0)
		return;

	// --------------------------------------------------
	// occupancy coverage
	// --------------------------------------------------

	int baseRadiusTileCount = 0;

	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		if (map_has_item(tile, BIT_BASE_RADIUS))
		{
			baseRadiusTileCount++;
		}

	}

	double occupancyCoverageThreshold = 0.25;
	double occupancyCoverage = (double)baseRadiusTileCount / (double)*MapAreaTiles;

	// increase base demand with low map occupancy

	if (occupancyCoverage < occupancyCoverageThreshold)
	{
		globalBaseDemand *= 1.0 + 0.50 * (1.0 - occupancyCoverage / occupancyCoverageThreshold);
		debug("\toccupancyCoverage globalBaseDemand=%f\n", globalBaseDemand);
	}

	// --------------------------------------------------
	// inefficiency
	// --------------------------------------------------

	// compute how many b-drones new base generates

	int baseLimit = (int)floor((double)((std::max(0, faction->SE_effic_pending) + 4) * 5) * sqrt(*MapAreaTiles / 3200)) / 2;
	int moreBDrones = faction->base_count / baseLimit;

	// collect statistics on inefficiency and estimate on b-drone spent
	// energy intake
	// energy inefficiency
	// energy spending per drone

	int totalEnergyIntake = 0;
	int totalEnergyInefficiency = 0;
	int totalBDrones = 0;
	double totalDroneEnergySpending = 0.0;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);

		// summarize energy intake

		totalEnergyIntake += base->energy_intake;

		// summarize energy inefficiency

		totalEnergyInefficiency += base->energy_inefficiency;

		// summarize b-drones

		totalBDrones += getBaseBDrones(baseId);

		// calculate psych multiplier

		double psychMultiplier = 1.0;

		if (isBaseHasFacility(baseId, FAC_HOLOGRAM_THEATRE))
		{
			psychMultiplier += 0.5;
		}

		if (isBaseHasFacility(baseId, FAC_TREE_FARM))
		{
			psychMultiplier += 0.5;
		}

		if (isBaseHasFacility(baseId, FAC_HYBRID_FOREST))
		{
			psychMultiplier += 0.5;
		}

		if (isBaseHasFacility(baseId, FAC_RESEARCH_HOSPITAL))
		{
			psychMultiplier += 0.25;
		}

		if (isBaseHasFacility(baseId, FAC_NANOHOSPITAL))
		{
			psychMultiplier += 0.25;
		}

		// estimate energy spending per drone

		double droneEnergySpending = 2.0 / psychMultiplier;

		// summarize drone spending

		totalDroneEnergySpending += droneEnergySpending;

	}

	double averageDroneEnergySpending = totalDroneEnergySpending / (double)aiData.baseIds.size();

	// energy loss

	double relativeInefficiencyEnergyLoss = totalEnergyIntake > 0 ? (double)totalEnergyInefficiency / (double)totalEnergyIntake : 0.0;

	double bDroneEnergyLoss = (double)moreBDrones * averageDroneEnergySpending;
	double bDroneGain = getIncomeGain(10, getResourceScore(0, bDroneEnergyLoss), 0.0);
	double relativeBDroneGain = bDroneGain / getNewBaseGain(10);

	double totalRelativeLossGain = std::min(1.0, 0.5 * relativeInefficiencyEnergyLoss + relativeBDroneGain);

	double inefficiencyDemandMultiplier = 1.0 - totalRelativeLossGain;

	debug("\tbaseLimit                      %6d\n", baseLimit);
	debug("\tmoreBDrones                    %6d\n", moreBDrones);
	debug("\ttotalEnergyIntake              %6d\n", totalEnergyIntake);
	debug("\ttotalEnergyInefficiency        %6d\n", totalEnergyInefficiency);
	debug("\ttotalBDrones                   %6d\n", totalBDrones);
	debug("\taverageDroneEnergySpending     %6.2f\n", averageDroneEnergySpending);
	debug("\trelativeInefficiencyEnergyLoss %6.2f\n", relativeInefficiencyEnergyLoss);
	debug("\tbDroneEnergyLoss               %6.2f\n", bDroneEnergyLoss);
	debug("\tbDroneGain                     %6.2f\n", bDroneGain);
	debug("\trelativeBDroneGain             %6.2f\n", relativeBDroneGain);
	debug("\ttotalRelativeLossGain          %6.2f\n", totalRelativeLossGain);
	debug("\tinefficiencyDemandMultiplier   %6.2f\n", inefficiencyDemandMultiplier);

	// reduce colony demand based on energy loss

	globalBaseDemand *= inefficiencyDemandMultiplier;
	debug("\tglobalBaseDemand=%5.2f\n", globalBaseDemand);

}

void evaluateGlobalFormerDemand()
{
	debug("evaluateGlobalFormerDemand - %s\n", MFactions[aiFactionId].noun_faction);

	int airFormerCount = aiData.airFormerVehicleIds.size();

	// land former demand

	aiData.production.landFormerDemands.clear();

	for (robin_hood::pair<int, int> &landTerraformingRequestCountEntry : aiData.production.landTerraformingRequestCounts)
	{
		int landAssociation = landTerraformingRequestCountEntry.first;
		int landTerraformingRequestCount = landTerraformingRequestCountEntry.second;
		int landFormerCount = aiData.landFormerVehicleIds[landAssociation].size();

		double landFormerDemand =
			getDemand(conf.ai_production_improvement_coverage_land * landTerraformingRequestCount, airFormerCount + landFormerCount);
		aiData.production.landFormerDemands[landAssociation] = landFormerDemand;

		debug
		(
			"\tlandFormerDemand=%f [%2d]"
			" conf.ai_production_improvement_coverage_land=%5.2f."
			" landTerraformingRequestCount=%4d"
			" airFormerCount=%4d"
			" landFormerCount=%4d"
			"\n"
			, landFormerDemand, landAssociation
			, conf.ai_production_improvement_coverage_land
			, landTerraformingRequestCount
			, airFormerCount
			, landFormerCount
		);

	}

	// sea former demand

	aiData.production.seaFormerDemands.clear();

	for (robin_hood::pair<int, int> &seaTerraformingRequestCountEntry : aiData.production.seaTerraformingRequestCounts)
	{
		int oceanAssociation = seaTerraformingRequestCountEntry.first;
		int seaTerraformingRequestCount = seaTerraformingRequestCountEntry.second;
		int seaFormerCount = aiData.seaFormerVehicleIds[oceanAssociation].size();

		double seaFormerDemand =
			getDemand(conf.ai_production_improvement_coverage_ocean * seaTerraformingRequestCount, airFormerCount + seaFormerCount);
		aiData.production.seaFormerDemands[oceanAssociation] = seaFormerDemand;

		debug
		(
			"\tseaFormerDemand=%f [%2d]"
			" conf.ai_production_improvement_coverage_ocean=%5.2f"
			" seaTerraformingRequestCount=%4d"
			" airFormerCount=%4d"
			" seaFormerCount=%4d"
			"\n"
			, seaFormerDemand, oceanAssociation
			, conf.ai_production_improvement_coverage_ocean
			, seaTerraformingRequestCount
			, airFormerCount
			, seaFormerCount
		);

	}

}

/*
Protection demand is unflexible. It should be satisfied completely.
Global demand is set to 1.0 whenever protection is required.
*/
void evaluateGlobalProtectionDemand()
{
	debug("evaluateGlobalProtectionDemand - %s\n", getMFaction(aiFactionId)->noun_faction);

	// set demand if at least one base is in demand

	aiData.production.protectionDemand = 0.0;

	for (int baseId : aiData.baseIds)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		BaseCombatData &baseCombatData = baseInfo.combatData;

		// set global protection demand if base protection is not satisfied

		if (!baseCombatData.isSatisfied(false))
		{
			aiData.production.protectionDemand = 1.0;
			break;
		}

	}

	debug("\taiData.production.protectionDemand=%5.2f\n", aiData.production.protectionDemand);

	// collect unit types

	std::fill(aiData.production.unitTypeFractions, aiData.production.unitTypeFractions + 3, 0.0);

	int unitCount = 0;
	int unitTypeCounts[3] = {0};
	for (int vehicleId : aiData.combatVehicleIds)
	{
		if (isInfantryDefensiveVehicle(vehicleId))
		{
			unitTypeCounts[0]++;
			unitCount++;
		}

		if (isOffensiveVehicle(vehicleId))
		{
			unitTypeCounts[1]++;
			unitCount++;
		}

		if (isArtilleryVehicle(vehicleId))
		{
			unitTypeCounts[2]++;
			unitCount++;
		}

	}

	for (int i = 0; i < 3; i++)
	{
		aiData.production.unitTypeFractions[i] = (double)unitTypeCounts[i] / (double)unitCount;
		debug
		(
			"\ttype=%d"
			" fraction=%5.2f"
			"\n"
			, i
			, aiData.production.unitTypeFractions[i]
		);
	}

	for (int i = 0; i < 3; i++)
	{
		aiData.production.unitTypePreferences[i] =
			pow(2.0, 1.0 - aiData.production.unitTypeFractions[i] / conf.ai_production_combat_unit_proportions[i])
		;
		debug
		(
			"\ttype=%d"
			" preference=%5.2f"
			"\n"
			, i
			, aiData.production.unitTypePreferences[i]
		);
	}

}

/*
Evaluates our ability to take enemy continental bases.
*/
void evaluateGlobalLandCombatDemand()
{
	debug("evaluateGlobalLandCombatDemand - %s\n", getMFaction(aiFactionId)->noun_faction);

	aiData.production.landCombatDemand = 0.0;

	// no enemies

	if (aiData.factionInfos[aiFactionId].enemyCount == 0)
		return;

	// find weakest base

	int targetBaseId = -1;
	double targetBaseProductionRatio = 0.0;

	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		// skip ourself

		if (factionId == aiFactionId)
			continue;

		// enemy

		if (!isVendetta(aiFactionId, factionId))
			continue;

		// no wars

		if (aiData.factionInfos[factionId].enemyCount == 0)
			continue;

		debug
		(
			"\t%-25s"
			" enemyCount=%d"
			"\n"
			, getMFaction(factionId)->noun_faction
			, aiData.factionInfos[factionId].enemyCount
		);

		// compute production weight ratio

		for (int potentialTargetBaseId = 0; potentialTargetBaseId < *BaseCount; potentialTargetBaseId++)
		{
			BASE *potentialTargetBase = getBase(potentialTargetBaseId);
			MAP *potentialTargetBaseTile = getBaseMapTile(potentialTargetBaseId);

			// faction

			if (potentialTargetBase->faction_id != factionId)
				continue;

			// close enough to ours

			int nearestBaseRange = getNearestBaseRange(potentialTargetBaseTile, aiData.baseIds, false);

			if (nearestBaseRange > 10)
				continue;

			debug("\t\t%-25s\n", potentialTargetBase->name);

			// our production weight

			double ourProductionWeight = 0.0;

			debug("\t\t\tour\n");
			for (int ourBaseId : aiData.baseIds)
			{
				BASE *ourBase = getBase(ourBaseId);
				MAP *ourBaseTile = getBaseMapTile(ourBaseId);

				// travel time

				int travelTime = getUnitAttackLTravelTime(ourBaseTile, potentialTargetBaseTile, aiFactionId, BSC_SCOUT_PATROL);

				if (travelTime == -1)
					continue;

				// other parameters

				int mineralIntake2 = std::max(1, ourBase->mineral_intake_2);
				double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				double weight = (double)mineralIntake2 * travelTimeCoefficient;

				// production weight

				ourProductionWeight += weight;

				debug
				(
					"\t\t\t\t%-25s"
					" mineralIntake2=%3d"
					" travelTime=%3d"
					" travelTimeCoefficient=%5.2f"
					" weight=%7.2f"
					" productionWeight=%7.2f"
					"\n"
					, ourBase->name
					, mineralIntake2
					, travelTime
					, travelTimeCoefficient
					, weight
					, ourProductionWeight
				);

			}

			// enemy production weight

			double enemyProductionWeight = 0.0;

			debug("\t\t\tenemy\n");
			for (int enemyBaseId = 0; enemyBaseId < *BaseCount; enemyBaseId++)
			{
				BASE *enemyBase = getBase(enemyBaseId);
				MAP *enemyBaseTile = getBaseMapTile(enemyBaseId);

				// correct faction

				if (enemyBase->faction_id != factionId)
					continue;

				// travel time

				int travelTime = getUnitLTravelTime(enemyBaseTile, potentialTargetBaseTile, factionId, BSC_SCOUT_PATROL, Rules->move_rate_roads, 0);

				if (travelTime == -1)
					continue;

				int mineralIntake2 = std::max(1, enemyBase->mineral_intake_2);
				double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);
				double weight = (double)mineralIntake2 * travelTimeCoefficient;

				enemyProductionWeight += weight;

				debug
				(
					"\t\t\t\t%-25s"
					" mineralIntake2=%3d"
					" travelTime=%3d"
					" travelTimeCoefficient=%5.2f"
					" weight=%7.2f"
					" productionWeight=%7.2f"
					"\n"
					, enemyBase->name
					, mineralIntake2
					, travelTime
					, travelTimeCoefficient
					, weight
					, enemyProductionWeight
				);

			}

			// compute ratio

			double productionRatio =
				(ourProductionWeight / (double)aiData.factionInfos[aiFactionId].enemyCount)
				/
				(enemyProductionWeight / (double)aiData.factionInfos[factionId].enemyCount)
			;

			debug
			(
				"\t\t\tproductionRatio=%5.2f"
				" ourProductionWeight=%7.2f"
				" our enemyCount=%1d"
				" enemyProductionWeight=%7.2f"
				" enemy enemyCount=%1d"
				"\n"
				, productionRatio
				, ourProductionWeight
				, aiData.factionInfos[aiFactionId].enemyCount
				, enemyProductionWeight
				, aiData.factionInfos[factionId].enemyCount
			);

			// update best

			if (productionRatio > targetBaseProductionRatio)
			{
				targetBaseId = potentialTargetBaseId;
				targetBaseProductionRatio = productionRatio;
			}

		}

	}

	// not found

	if (targetBaseId == -1)
		return;

	debug
	(
		"\ttargetBase=%-25s"
		" targetBaseProductionRatio=%5.2f"
		"\n"
		, getBase(targetBaseId)->name
		, targetBaseProductionRatio
	);

	// sufficient production ratio

	if (targetBaseProductionRatio >= conf.ai_production_global_combat_superiority_land)
	{
		aiData.production.landCombatDemand = 1.0;
	}

}

void evaluateGlobalTerritoryLandCombatDemand()
{
	debug("evaluateGlobalTerritoryLandCombatDemand - %s\n", getMFaction(aiFactionId)->noun_faction);

	// already set

	if (aiData.production.landCombatDemand > 0.0)
		return;

	// enemy units in our land territory

	bool enemyLand = false;
	robin_hood::unordered_flat_set<int> enemyOceanAssociations;
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		if (vehicle->faction_id == aiFactionId)
			continue;

		if (vehicle->faction_id == 0)
			continue;

		if (!isVendetta(aiFactionId, vehicle->faction_id))
			continue;

		if (vehicleTile->owner == aiFactionId)
		{
			if (is_ocean(vehicleTile))
			{
				enemyOceanAssociations.insert(getOceanAssociation(vehicleTile, aiFactionId));
			}
			else
			{
				enemyLand = true;
			}

		}

	}

	if (enemyLand)
	{
		aiData.production.landCombatDemand = 0.50;
	}
	else if (enemyOceanAssociations.size() > 0)
	{
//		aiData.production.combatDemand = 0.50;
	}
	else
	{
		bool atWar = false;
		for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
		{
			if (isVendetta(aiFactionId, factionId))
			{
				atWar = true;
				break;
			}

		}

		if (atWar)
		{
			aiData.production.landCombatDemand = 0.25;
		}

	}

}

/*
Evaluates our ability to take enemy ocean bases.
*/
void evaluateGlobalSeaCombatDemand()
{
	debug("evaluateGlobalSeaCombatDemand - %s\n", getMFaction(aiFactionId)->noun_faction);

	aiData.production.seaCombatDemands.clear();

	// enemy strengths

	robin_hood::unordered_flat_map<int, double> seaAssociationEnemyStrengths;

	for (int factionId = 1; factionId < MaxPlayerNum; factionId++)
	{
		// not us

		if (factionId == aiFactionId)
			continue;

		// enemy

		if (!isVendetta(aiFactionId, factionId))
			continue;

		// number of wars

		int warCount = 0;

		for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
		{
			// skip self

			if (otherFactionId == factionId)
				continue;

			// at war

			if (!isVendetta(factionId, otherFactionId))
				continue;

			// accumulate

			warCount++;

		}

		// no wars

		if (warCount == 0)
			continue;

		debug
		(
			"\t%-25s"
			" warCount=%d"
			"\n"
			, getMFaction(factionId)->noun_faction
			, warCount
		);

		// bases in ocean associations

		robin_hood::unordered_flat_set<int> presenseAssociations;

		debug("\t\tbases\n");
		for (int baseId = 0; baseId < *BaseCount; baseId++)
		{
			BASE *base = getBase(baseId);
			MAP *baseTile = getBaseMapTile(baseId);
			bool baseOcean = is_ocean(baseTile);
			int baseAssociation = getAssociation(baseTile, base->faction_id);

			// faction

			if (base->faction_id != factionId)
				continue;

			// ocean

			if (!baseOcean)
				continue;

			debug("\t\t\t%-25s\n", base->name);

			// add presense

			presenseAssociations.insert(baseAssociation);

		}

		// forces in associations

		debug("\t\tforces\n");

		double totalAirForceStrength = 0.0;
		robin_hood::unordered_flat_map<int, double> associationSurfaceForceStrengths;

		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			int triad = vehicle->triad();
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			int vehicleAssociation = getVehicleAssociation(vehicleId);

			// faction

			if (vehicle->faction_id != factionId)
				continue;

			// combat

			if (!isCombatVehicle(vehicleId))
				continue;

			// not land outside of bases

			if (triad == TRIAD_LAND && !isBaseAt(vehicleTile))
				continue;

			// vehicle strength

			double strength;

			if (isNativeVehicle(vehicleId))
			{
				strength = 1.0;
			}
			else
			{
				int offenseValue = (triad == TRIAD_LAND ? 0 : getVehicleOffenseValue(vehicleId));
				int defenseValue = getVehicleDefenseValue(vehicleId);
				double value = 0.5 * ((double)std::max(offenseValue, defenseValue) + (0.5 * (double)(offenseValue + defenseValue)));
				double relativeValue = value / (0.5 * (double)(aiData.bestOffenseValue + aiData.bestDefenseValue));
				strength = relativeValue;
			}

			// accumulate

			switch (triad)
			{
			case TRIAD_AIR:
				totalAirForceStrength += strength;
				break;
			default:
				associationSurfaceForceStrengths[vehicleAssociation]++;
			}

		}

		// proportionalAirForceStrength
		// air forces are thinned out with multiple wars

		double proportionalAirForceStrength = totalAirForceStrength / (double)warCount;

		// compare combinedForceStrengths

		for (int association : presenseAssociations)
		{
			double surfaceForceStrength = associationSurfaceForceStrengths[association];
			double combinedForceStrength = proportionalAirForceStrength + surfaceForceStrength;

			debug
			(
				"\t\t\t%3d"
				" combinedForceStrength=%7.2f"
				"\n"
				, association
				, combinedForceStrength
			);

			// update weakest

			if
			(
				seaAssociationEnemyStrengths.count(association) == 0
				||
				combinedForceStrength < seaAssociationEnemyStrengths.at(association)
			)
			{
				seaAssociationEnemyStrengths[association] = combinedForceStrength;
			}

		}

	}

	// our wars

	int ourWarCount = 0;

	for (int otherFactionId = 1; otherFactionId < MaxPlayerNum; otherFactionId++)
	{
		// skip self

		if (otherFactionId == aiFactionId)
			continue;

		// at war

		if (!isVendetta(aiFactionId, otherFactionId))
			continue;

		// accumulate

		ourWarCount++;

	}

	// no wars

	if (ourWarCount == 0)
		return;

	debug
	(
		"\t%-25s"
		" warCount=%d"
		"\n"
		, "us"
		, ourWarCount
	);

	// iterate enemy presence associations

	for (robin_hood::pair<int, double> &seaAssociationEnemyStrengthEntry : seaAssociationEnemyStrengths)
	{
		int association = seaAssociationEnemyStrengthEntry.first;
		double enemyStrength = seaAssociationEnemyStrengthEntry.second;

		// our production

		int totalProductionSurplus = 0;

		for (int baseId : aiData.baseIds)
		{
			BASE *base = getBase(baseId);
			int baseOceanAssociation = getBaseOceanAssociation(baseId);

			// this association

			if (baseOceanAssociation != association)
				continue;

			// accumulate surplus

			totalProductionSurplus += base->mineral_surplus;

		}

		// divide by number of wars to evaluate production we can dedicate to war

		double availableProductionSurplus = (double)totalProductionSurplus / (double)ourWarCount;

		// productionRatio

		double productionRatio = availableProductionSurplus / std::max(1.0, enemyStrength);

		// sufficient production ratio

		if (productionRatio >= conf.ai_production_global_combat_superiority_sea)
		{
			aiData.production.seaCombatDemands[association] = 1.0;
		}

		debug
		(
			"\t\taiData.production.seaCombatDemands[%3d]=%5.2f"
			" productionRatio=%5.2f"
			" totalProductionSurplus=%4d"
			" ourWarCount=%d"
			" availableProductionSurplus=%7.2f"
			" enemyStrength=%7.2f"
			"\n"
			, association
			, aiData.production.seaCombatDemands[association]
			, productionRatio
			, totalProductionSurplus
			, ourWarCount
			, availableProductionSurplus
			, enemyStrength
		);

	}

}

void evaluateGlobalLandAlienCombatDemand()
{
	debug("evaluateGlobalLandAlienCombatDemand - %s\n", getMFaction(aiFactionId)->noun_faction);

	aiData.production.landAlienCombatDemand = 0.0;

	// weight aliens

	debug("\taliens\n");

	double totalAlienWeight = 0.0;
	std::vector<int> alienVehicleIds;

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		// alien

		if (vehicle->faction_id != 0)
			continue;

		// land

		if (triad != TRIAD_LAND)
			continue;

		// spore launcher or fungal tower

		if (!(vehicle->unit_id == BSC_SPORE_LAUNCHER || vehicle->unit_id == BSC_FUNGAL_TOWER))
			continue;

		// player territory

		if (vehicleTile->owner != aiFactionId)
			continue;

		// range

		int nearestBaseRange = getNearestBaseRange(vehicleTile, aiData.baseIds, triad != TRIAD_AIR);

		if (nearestBaseRange > 4)
			continue;

		// add weight

		double weight = getVehicleStrenghtMultiplier(vehicleId);

		debug
		(
			"\t\t[%4d] (%3d,%3d)"
			" weight=%5.2f"
			"\n"
			, vehicleId, vehicle->x, vehicle->y
			, weight
		);

		totalAlienWeight += weight;
		alienVehicleIds.push_back(vehicleId);

	}

	debug
	(
		"\ttotalAlienWeight=%5.2f"
		"\n"
		, totalAlienWeight
	);

	// weight anti-aliens

	debug("\tanti-aliens\n");

	double totalHunterWeight = 0.0;

	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		// land

		if (vehicle->triad() != TRIAD_LAND)
			continue;

		// alien hunter

		if (!(isVehicleHasAbility(vehicleId, ABL_EMPATH) || isScoutVehicle(vehicleId)))
			continue;

		// not in base

		if (isBaseAt(vehicleTile))
			continue;

		// minimal range to alien

		int minAlienRange = INT_MAX;

		for (int alienVehicleId : alienVehicleIds)
		{
			VEH *alienVehicle = getVehicle(alienVehicleId);

			int range = map_range(vehicle->x, vehicle->y, alienVehicle->x, alienVehicle->y);

			minAlienRange = std::min(minAlienRange, range);

		}

		// add weight

		double weight =
			getVehicleStrenghtMultiplier(vehicleId)
			* getExponentialCoefficient(conf.ai_production_economical_combat_range_scale, (double)minAlienRange);
		;

		debug
		(
			"\t\t[%4d] (%3d,%3d)"
			" weight=%5.2f"
			"\n"
			, vehicleId, vehicle->x, vehicle->y
			, weight
		);

		totalHunterWeight += weight;

	}

	debug
	(
		"\ttotalHunterWeight=%5.2f"
		"\n"
		, totalHunterWeight
	);

	// compute demand

	double requiredHunterWeight = conf.ai_combat_field_attack_superiority_required * totalAlienWeight;

	if (requiredHunterWeight > 0.0 && totalHunterWeight < requiredHunterWeight)
	{
		aiData.production.landAlienCombatDemand = 1.0 - totalHunterWeight / requiredHunterWeight;
	}

	debug
	(
		"\taiData.production.landAlienCombatDemand=%5.2f"
		"\n"
		, aiData.production.landAlienCombatDemand
	);

}

/*
Sets faction all bases production.
*/
void setProduction()
{
    debug("\nsetProduction - %s\n\n", MFactions[*CurrentFaction].noun_faction);

    for (int baseId : aiData.baseIds)
	{
		BASE* base = &(Bases[baseId]);

		debug("setProduction - %s\n", base->name);fflush(debug_log);

		// reset base specific variables

		baseColonyDemandMultiplier = 1.0;

		// current production choice

		int choice = base->queue_items[0];

		debug("(%s)\n", prod_name(choice));

		// calculate vanilla priority

		double vanillaPriority = 0.0;

		// unit
		if (choice >= 0)
		{
			// only not managed unit types

			if (!isCombatUnit(choice) && MANAGED_UNIT_TYPES.count(Units[choice].weapon_id) == 0)
			{
				vanillaPriority = conf.ai_production_vanilla_priority_unit * getUnitPriorityCoefficient(baseId);
			}

		}
		// project
		else if (choice <= -SP_ID_First)
		{
			// project priority is handled by WTP
			vanillaPriority = 0.0;
		}
		// facility
		else
		{
			// only not managed facilities

			if (MANAGED_FACILITIES.count(-choice) == 0)
			{
				vanillaPriority = conf.ai_production_vanilla_priority_facility;
			}

		}

		// drop priority for impossible colony

		if (choice >= 0 && isColonyUnit(choice) && !canBaseProduceColony(baseId))
		{
			vanillaPriority = 0.0;
		}

		// WTP

		suggestBaseProduction(baseId);

		ItemPriority wtpItemPriority = productionDemand.get();
		int wtpChoice = wtpItemPriority.item;
		double wtpPriority = wtpItemPriority.priority;

		debug
		(
			"production selection\n"
			"\t%-10s(%5.2f) %s\n"
			"\t%-10s(%5.2f) %s\n"
			, "vanilla" , vanillaPriority, prod_name(choice)
			, "wtp", wtpPriority, prod_name(wtpChoice)
		);

		// select production based on priority

		if (wtpPriority >= vanillaPriority)
		{
			choice = wtpChoice;
		}

		debug("=> %-25s\n", prod_name(choice));

		// set base production

		base_prod_change(baseId, choice);

		debug("\n");

	}

}

void suggestBaseProduction(int baseId)
{
	BASE *base = &(Bases[baseId]);

    // initialize productionDemand

	productionDemand.initialize(baseId);

	// do not suggest production for base without mineral surplus

	if (base->mineral_surplus <= 0)
	{
		debug("base->mineral_surplus <= 0\n");
		return;
	}

	// default to stockpile energy

	productionDemand.add(-FAC_STOCKPILE_ENERGY, 0.15, false);

	// evaluate demands

	profileFunction("1.4.9.1. evaluateFacilities", evaluateFacilities);

	profileFunction("1.4.9.2. evaluateMandatoryDefenseDemand", evaluateMandatoryDefenseDemand);

	profileFunction("1.4.9.3. evaluatePolice", evaluatePolice);

	profileFunction("1.4.9.4. evaluateLandFormerDemand", evaluateLandFormerDemand);
	profileFunction("1.4.9.5. evaluateSeaFormerDemand", evaluateSeaFormerDemand);

	profileFunction("1.4.9.6. evaluateColony", evaluateColony);

	profileFunction("1.4.9.7. evaluatePodPoppingDemand", evaluatePodPoppingDemand);

	profileFunction("1.4.9.8. evaluateProtectionDemand", evaluateProtectionDemand);
	profileFunction("1.4.9.9. evaluateLandCombatDemand", evaluateLandCombatDemand);
	profileFunction("1.4.9.A. evaluateSeaCombatDemand", evaluateSeaCombatDemand);
	profileFunction("1.4.9.B. evaluateLandAlienCombatDemand", evaluateLandAlienCombatDemand);

	profileFunction("1.4.9.C. evaluateTransportDemand", evaluateTransportDemand);

	profileFunction("1.4.9.D. evaluateProjectDemand", evaluateProjectDemand);

}

/*
Generates mandatory defense demand and return true if generated.
*/
void evaluateMandatoryDefenseDemand()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	bool baseTileOcean = is_ocean(baseTile);

	debug("evaluateMandatoryDefenseDemand\n");

	// base wihtout land defender

	bool infantryDefender = false;

	for (int vehicleId : getBaseGarrison(baseId))
	{
		if (isInfantryDefensiveVehicle(vehicleId))
		{
			infantryDefender = true;
			break;
		}

	}

	if (infantryDefender)
		return;

	// find best anti-native defense unit

	int unitId = findAntiNativeDefenderUnit(baseTileOcean);

	// no unit found

	if (unitId == -1)
	{
		debug("\tno anti-native defense unit found\n");
		return;
	}

	// set priority

	double priority = 5.0 * conf.ai_production_base_protection_priority;

	debug("\t%-32s priority=%5.2f\n", getUnit(unitId)->name, priority);

	// add production demand

	productionDemand.add(unitId, priority, false);

}

void evaluateFacilities()
{
	debug("evaluateFacilities\n");

	evaluateHeadquarters();
	evaluatePopulationLimitFacilities();
	evaluatePunishmentSphere();
	evaluatePunishmentSphereRemoval();
	evaluatePsychFacilities();
	evaluateHospitalFacilities();
	evaluatePunishmentSphereRemoval();
	evaluateMineralMultiplyingFacilities();
	evaluateBudgetMultiplyingFacilities();
	evaluateChildrenCreche();
	evaluateTreeFacilities();
	evaluatePlanetFacilities();
	evaluateAquafarm();
	evaluateThermoclineTransducer();
	evaluateBiologyLab();
	evaluateBroodPit();
	evaluateDefensiveFacilities();
	evaluateMilitaryFacilities();
	evaluatePrototypingFacilities();

}

void evaluateHeadquarters()
{
	debug("- evaluateHeadquarters\n");

	int baseId = productionDemand.baseId;
	int facilityId = getFirstAvailableFacility(baseId, {FAC_HEADQUARTERS});

	// cannot build

	if (facilityId == -1)
	{
		debug("\tcannot build\n");
		return;
	}

	// find HQ base
	// compute old total energy surplus

	int hqBaseId = -1;
	int oldTotalBudget = 0;

	for (int otherBaseId : aiData.baseIds)
	{
		BASE *otherBase = getBase(otherBaseId);

		if (isBaseHasFacility(otherBaseId, facilityId))
		{
			hqBaseId = otherBaseId;
		}

		computeBase(otherBaseId, false);

		int oldBudget = otherBase->economy_total + otherBase->psych_total + otherBase->labs_total;
		oldTotalBudget += oldBudget;

	}

	// relocate HQ
	// compute new total energy surplus

	if (hqBaseId != -1)
	{
		setBaseFacility(hqBaseId, facilityId, false);
	}
	setBaseFacility(baseId, facilityId, true);

	int newTotalBudget = 0;

	for (int otherBaseId : aiData.baseIds)
	{
		BASE *otherBase = getBase(otherBaseId);

		computeBase(otherBaseId, false);

		int newBudget = otherBase->economy_total + otherBase->psych_total + otherBase->labs_total;
		newTotalBudget += newBudget;

	}

	// restore HQ

	if (hqBaseId != -1)
	{
		setBaseFacility(hqBaseId, facilityId, true);
	}
	setBaseFacility(baseId, facilityId, false);

	for (int otherBaseId : aiData.baseIds)
	{
		computeBase(otherBaseId, false);
	}

	// find initial economical impact

	double income = getResourceScore(0, (double)(newTotalBudget - oldTotalBudget));
	double incomeGrowth = 0;

	// gain and priority

	double priority = getItemProductionPriority(baseId, -facilityId, 0, income, incomeGrowth);

	debug
	(
		"\tpriority=%5.2f"
		" income=%5.2f"
		" incomeGrowth=%5.2f"
		"\n"
		, priority
		, income
		, incomeGrowth
	)
	;

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

/*
Evaluates population limit facilities.
- allows GROWH
*/
void evaluatePopulationLimitFacilities()
{
	debug("- evaluatePopulationLimitFacilities\n");

	int baseId = productionDemand.baseId;
	BASE *base = getBase(baseId);
	int facilityId = getFirstAvailableFacility(baseId, {FAC_HAB_COMPLEX, FAC_HABITATION_DOME});

	// cannot build

	if (facilityId == -1)
	{
		debug("\tcannot build\n");
		return;
	}

	// parameters

	int populationLimit = getFacilityPopulationLimit(base->faction_id, facilityId);

	if (populationLimit == -1)
	{
		debug("\tcannot get population limit\n");
		return;
	}

	if (conf.habitation_facility_disable_explicit_population_limit)
	{
		populationLimit += (3 - conf.habitation_facility_absent_growth_penalty) + getFaction(aiFactionId)->SE_growth_pending;
	}

	// gain and priority

	double gain = getBasePopulationGain(baseId, {populationLimit, 100});
	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" populationLimit=%2d"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, populationLimit
		, gain
	)
	;

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

/*
Evaluates punishment sphere.
- set facility, remove all pure psych facilities, run doctors
- compare to current state
*/
void evaluatePunishmentSphere()
{
	debug("- evaluatePunishmentSphere\n");

	int baseId = productionDemand.baseId;
	BASE *base = getBase(baseId);

	// facilityId

	int facilityId = getFirstAvailableFacility(baseId, {FAC_PUNISHMENT_SPHERE});
	const int psychFacilityCount = 3;
	int psychFacilityIds[psychFacilityCount] = {FAC_RECREATION_COMMONS, FAC_HOLOGRAM_THEATRE, FAC_PARADISE_GARDEN};

	// cannot build

	if (facilityId == -1)
		return;

	// current base state

	int currentPopSize = base->pop_size;
	bool currentPsychFacilities[psychFacilityCount];
	for (int i = 0; i < psychFacilityCount; i++)
	{
		currentPsychFacilities[i] = isBaseHasFacility(baseId, psychFacilityIds[i]);
	}

	// projected population

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	int projectedPopSize = getBaseProjectedSize(baseId, buildTime);

	// set pop size

	base->pop_size = projectedPopSize;

	// income with facility and doctors

	for (int i = 0; i < psychFacilityCount; i++)
	{
		setBaseFacility(baseId, psychFacilityIds[i], false);
	}
	setBaseFacility(baseId, facilityId, true);
	computeBaseDoctors(baseId);

	double facilityIncome = getBaseIncome(baseId);

	// revert previous facility state

	for (int i = 0; i < psychFacilityCount; i++)
	{
		if (currentPsychFacilities[i])
		{
			setBaseFacility(baseId, psychFacilityIds[i], true);
		}
	}
	setBaseFacility(baseId, facilityId, false);
	computeBaseDoctors(baseId);

	int noFacilityDoctors = getBaseDoctors(baseId);
	double noFacilityIncome = getBaseIncome(baseId);

	// restore base

	base->pop_size = currentPopSize;
	computeBaseDoctors(baseId);

	// income

	double income = facilityIncome - noFacilityIncome;
	double incomeGrowth = 0.0;
	double workerIncomeCoefficient = (noFacilityDoctors > 0 ? 0.5 : 0.0);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, 0, income, incomeGrowth);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" income=%5.2f"
		" incomeGrowth=%5.2f"
		" workerIncomeCoefficient=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, income
		, incomeGrowth
		, workerIncomeCoefficient
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

/*
Evaluates punishment sphere removal.
Remove it if is not effective with all other psych facilities around.
*/
void evaluatePunishmentSphereRemoval()
{
	debug("- evaluatePunishmentSphereRemoval\n");

	int baseId = productionDemand.baseId;

	// facilityId

	int facilityId = FAC_PUNISHMENT_SPHERE;
	const int psychFacilityCount = 3;
	int psychFacilityIds[psychFacilityCount] = {FAC_RECREATION_COMMONS, FAC_HOLOGRAM_THEATRE, FAC_PARADISE_GARDEN};

	// not built

	if (!isBaseHasFacility(baseId, facilityId))
		return;

	// current base state

	bool currentPsychFacilities[psychFacilityCount];
	for (int i = 0; i < psychFacilityCount; i++)
	{
		currentPsychFacilities[i] = isBaseHasFacility(baseId, psychFacilityIds[i]);
	}

	// income without facility and doctors

	int psychFacilitiesMaintenance = 0;
	for (int psychFacilityId : psychFacilityIds)
	{
		if (isBaseFacilityAvailable(baseId, facilityId))
		{
			setBaseFacility(baseId, psychFacilityId, true);
			psychFacilitiesMaintenance += getFacility(psychFacilityId)->maint;
		}
	}
	setBaseFacility(baseId, facilityId, false);
	computeBaseDoctors(baseId);

	double noFacilityIncome = getBaseIncome(baseId);

	// revert to previous state

	for (int i = 0; i < psychFacilityCount; i++)
	{
		if (!currentPsychFacilities[i])
		{
			setBaseFacility(baseId, psychFacilityIds[i], false);
		}
	}
	setBaseFacility(baseId, facilityId, true);
	computeBaseDoctors(baseId);

	double facilityIncome = getBaseIncome(baseId);

	// income and maintenance score

	double removedMaintenanceScore = getResourceScore(0, +getFacility(facilityId)->maint);
	double psychFacilitiesMaintenanceScore = getResourceScore(0, -psychFacilitiesMaintenance);
	double income = noFacilityIncome - facilityIncome + removedMaintenanceScore + psychFacilitiesMaintenanceScore;

	// compare maintenance and income

	if (income > 0.0)
	{
		// remove facility

		setBaseFacility(baseId, facilityId, false);
		debug("\tremoved");

	}

	debug
	(
		"\t%-32s"
		" income=%5.2f"
		" facilityIncome=%5.2f"
		" noFacilityIncome=%5.2f"
		" removedMaintenanceScore=%5.2f"
		" psychFacilitiesMaintenanceScore=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, income
		, facilityIncome
		, noFacilityIncome
		, removedMaintenanceScore
		, psychFacilitiesMaintenanceScore
	);

}

/*
Evaluates psych facilities.
*/
void evaluatePsychFacilities()
{
	debug("- evaluatePsychFacilities\n");

	int baseId = productionDemand.baseId;

	// facilityId

	int facilityId = getFirstAvailableFacility(baseId, {FAC_RECREATION_COMMONS, FAC_HOLOGRAM_THEATRE, FAC_PARADISE_GARDEN});

	// not found

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// find min and max affected base sizes

	Interval baseSizeInterval = getPsychFacilityBaseSizeInterval(baseId, facilityId);

	// gain

	double gain = getBasePopulationGain(baseId, baseSizeInterval);

	// additional gain for HOLOGRAM_THEATRE

	if (facilityId == FAC_HOLOGRAM_THEATRE)
	{
		int buildTime = getBaseItemBuildTime(baseId, -facilityId);
		double psychAllocation = 0.1 * (double)(getFaction(aiFactionId)->SE_alloc_psych);
		gain += getBaseProportionalBudgetIntakeGain(baseId, buildTime, psychAllocation * 0.50);
	}

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" baseSizeInterval.min=%2d"
		" baseSizeInterval.max=%2d"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, baseSizeInterval.min
		, baseSizeInterval.max
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

/*
Evaluates hospital facilities.
*/
void evaluateHospitalFacilities()
{
	debug("- evaluateHospitalFacilities\n");

	int baseId = productionDemand.baseId;

	// facilityId

	int facilityId = getFirstAvailableFacility(baseId, {FAC_RESEARCH_HOSPITAL, FAC_NANOHOSPITAL});

	// not found

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// find min and max affected base sizes

	Interval baseSizeInterval = getPsychFacilityBaseSizeInterval(baseId, facilityId);

	// gain

	double gain = getBasePopulationGain(baseId, baseSizeInterval);

	// additional psych and labs gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	gain += getBaseProportionalBudgetIntakeGain(baseId, buildTime, 0.4 * 0.25 + 0.5 * 0.50);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateMineralMultiplyingFacilities()
{
	debug("- evaluateMineralMultiplyingFacilities\n");

	int baseId = productionDemand.baseId;

	// get facility

	int facilityId =
		getFirstAvailableFacility
		(
			baseId,
			{
				FAC_RECYCLING_TANKS,
				FAC_GENEJACK_FACTORY,
				FAC_ROBOTIC_ASSEMBLY_PLANT,
				FAC_QUANTUM_CONVERTER,
				FAC_NANOREPLICATOR
			}
		);

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	double gain = getBaseProportionalMineralIntakeGain(baseId, buildTime, 1.0 * 0.50);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateBudgetMultiplyingFacilities()
{
	debug("- evaluateBudgetMultiplyingFacilities\n");

	int baseId = productionDemand.baseId;

	// get facility

	int facilityId =
		getFirstAvailableFacility
		(
			baseId,
			{
				FAC_NETWORK_NODE,
				FAC_ENERGY_BANK,
				FAC_FUSION_LAB,
				FAC_QUANTUM_LAB,
			}
		);

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// multipliers

	double economyMultiplier;
	double labsMultiplier;

	switch (facilityId)
	{
	case FAC_NETWORK_NODE:
		economyMultiplier = 0.00;
		labsMultiplier = 0.50;
		break;
	case FAC_ENERGY_BANK:
		economyMultiplier = 0.50;
		labsMultiplier = 0.00;
		break;
	case FAC_FUSION_LAB:
	case FAC_QUANTUM_LAB:
		economyMultiplier = 0.50;
		labsMultiplier = 0.50;
		break;
	default:
		debug("\tunknown facility");
		return;
	}

	// gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	double gain = getBaseProportionalMineralIntakeGain(baseId, buildTime, 0.4 * economyMultiplier + 0.5 * labsMultiplier);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateChildrenCreche()
{
	debug("- evaluateChildrenCreche\n");

	int baseId = productionDemand.baseId;

	// get facility

	int facilityId =
		getFirstAvailableFacility
		(
			baseId,
			{
				FAC_CHILDREN_CRECHE,
			}
		);

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	double gain = getBasePopulationGrowthGain(baseId, buildTime, 0.2);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateTreeFacilities()
{
	debug("- evaluateTreeFacilities\n");

	int baseId = productionDemand.baseId;

	// get facility

	int facilityId =
		getFirstAvailableFacility
		(
			baseId,
			{
				FAC_TREE_FARM,
				FAC_HYBRID_FOREST,
			}
		);

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// budget gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	double gain = getBaseProportionalBudgetIntakeGain(baseId, buildTime, 0.5 * 0.50 + 0.4 * 0.50);

	// extra gain for EcoDamage reduction

	int ecoDamageReduction = getFacilityEcoDamageReduction(baseId, facilityId);
	double ecoDamageReductionProportionalProductivityImpact = std::min(1.0, (double)ecoDamageReduction / 40.0);
	gain += getBaseProportionalMineralIntake2Gain(baseId, buildTime, 1.0 * ecoDamageReductionProportionalProductivityImpact);
	gain += getBaseProportionalBudgetIntakeGain(baseId, buildTime, 1.0 * ecoDamageReductionProportionalProductivityImpact);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluatePlanetFacilities()
{
	debug("- evaluatePlanetFacilities\n");

	int baseId = productionDemand.baseId;

	// get facility

	int facilityId =
		getFirstAvailableFacility
		(
			baseId,
			{
				FAC_CENTAURI_PRESERVE,
				FAC_TEMPLE_OF_PLANET,
			}
		);

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// lifecycle gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	double moraleProportionalMineralBonus = getMoraleProportionalMineralBonus({0, 0, 0, 1});
	double gain = getBaseProportionalMineralIntake2Gain(baseId, buildTime, moraleProportionalMineralBonus);

	// extra gain for EcoDamage reduction

	int ecoDamageReduction = getFacilityEcoDamageReduction(baseId, facilityId);
	double ecoDamageReductionProportionalProductivityImpact = std::min(1.0, (double)ecoDamageReduction / 40.0);
	gain += getBaseProportionalMineralIntake2Gain(baseId, buildTime, 1.0 * ecoDamageReductionProportionalProductivityImpact);
	gain += getBaseProportionalBudgetIntakeGain(baseId, buildTime, 1.0 * ecoDamageReductionProportionalProductivityImpact);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateAquafarm()
{
	debug("- evaluateAquafarm\n");

	int baseId = productionDemand.baseId;

	// get facility

	int facilityId =
		getFirstAvailableFacility
		(
			baseId,
			{
				FAC_AQUAFARM,
			}
		);

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	double relativeGrowthIncrease = getFacilityRelativeGrowthIncrease(baseId, facilityId);
	double gain = getBasePopulationGrowthGain(baseId, buildTime, relativeGrowthIncrease);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateThermoclineTransducer()
{
	debug("- evaluateThermoclineTransducer\n");

	int baseId = productionDemand.baseId;

	// get facility

	int facilityId =
		getFirstAvailableFacility
		(
			baseId,
			{
				FAC_THERMOCLINE_TRANSDUCER,
			}
		);

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	double relativeBudgetIncrease = getFacilityRelativeBudgetIncrease(baseId, facilityId);
	double gain = getBaseProportionalBudgetIntake2Gain(baseId, buildTime, relativeBudgetIncrease);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateSubseaTrunkline()
{
	debug("- evaluateSubseaTrunkline\n");

	int baseId = productionDemand.baseId;

	// get facility

	int facilityId =
		getFirstAvailableFacility
		(
			baseId,
			{
				FAC_THERMOCLINE_TRANSDUCER,
			}
		);

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	double relativeMineralIncrease = getFacilityRelativeMineralIncrease(baseId, facilityId);
	double gain = getBaseProportionalMineralIntake2Gain(baseId, buildTime, relativeMineralIncrease);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateBiologyLab()
{
	debug("- evaluateBiologyLab\n");

	int baseId = productionDemand.baseId;

	// facilityId

	int facilityId = getFirstAvailableFacility(baseId, {FAC_BIOLOGY_LAB});

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	double labsIncome = (double)conf.biology_lab_bonus * getBaseLabsMultiplier(baseId);
	double gain = getFlatBudgetIntakeGain(buildTime, labsIncome);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

/*
Evaluates BroodPit.
- +1 lifecycle
- -25% cost for alien
- +2 POLICE
*/
void evaluateBroodPit()
{
	debug("- evaluateBroodPit\n");

	int baseId = productionDemand.baseId;

	// facilityId

	int facilityId = getFirstAvailableFacility(baseId, {FAC_BROOD_PIT});

	if (facilityId == -1)
	{
		debug("\tcanot build\n");
		return;
	}

	// gain

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);

	double proportionalMineralBonus = getMoraleProportionalMineralBonus({0, 0, 0, 3});
	double gain = getBaseProportionalMineralIntake2Gain(baseId, buildTime, proportionalMineralBonus);

	// priority

	double priority = getItemProductionPriority(baseId, -facilityId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" gain=%5.2f"
		"\n"
		, getFacility(facilityId)->name
		, priority
		, gain
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateDefensiveFacilities()
{
	debug("- evaluateDefensiveFacilities\n");

	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	bool ocean = is_ocean(baseTile);
	BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
	BaseCombatData &baseCombatData = baseInfo.combatData;

	// select facility

	int facilityId;

	if (!ocean)
	{
		facilityId = getFirstAvailableFacility(baseId, {FAC_PERIMETER_DEFENSE, FAC_AEROSPACE_COMPLEX, FAC_TACHYON_FIELD, });
	}
	else
	{
		facilityId = getFirstAvailableFacility(baseId, {FAC_NAVAL_YARD, FAC_AEROSPACE_COMPLEX, FAC_TACHYON_FIELD, });
	}

	// nothing selected

	if (facilityId == -1)
		return;

	// increase priority based on corresponding base threat

	double threat = 0.0;

	switch (facilityId)
	{
	case FAC_PERIMETER_DEFENSE:
		threat = baseCombatData.triadThreats[TRIAD_LAND];
		break;
	case FAC_NAVAL_YARD:
		threat = baseCombatData.triadThreats[TRIAD_SEA];
		break;
	case FAC_AEROSPACE_COMPLEX:
		threat = baseCombatData.triadThreats[TRIAD_AIR];
		break;
	case FAC_TACHYON_FIELD:
		threat =
			+ baseCombatData.triadThreats[TRIAD_LAND]
			+ baseCombatData.triadThreats[TRIAD_SEA]
			+ baseCombatData.triadThreats[TRIAD_AIR]
		;
		break;
	}

	// priority

	double priority = threat / conf.ai_production_defensive_facility_threat_threshold;

	debug
	(
		"\t\tfacility=%-25s"
		" priority=%5.2f"
		" conf.ai_production_defensive_facility_threat_threshold=%5.2f"
		" threat=%5.2f"
		"\n"
		, Facility[facilityId].name
		, priority
		, conf.ai_production_defensive_facility_threat_threshold
		, threat
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluateMilitaryFacilities()
{
	debug("- evaluateMilitaryFacilities\n");

	int baseId = productionDemand.baseId;
	BASE *base = getBase(baseId);
	int baseOceanAssociation = getBaseOceanAssociation(baseId);

	// select facility

	int facilityId;

	if (baseOceanAssociation == -1)
	{
		facilityId = getFirstAvailableFacility(baseId, {FAC_COMMAND_CENTER, FAC_AEROSPACE_COMPLEX, FAC_BIOENHANCEMENT_CENTER});
	}
	else
	{
		facilityId = getFirstAvailableFacility(baseId, {FAC_NAVAL_YARD, FAC_COMMAND_CENTER, FAC_AEROSPACE_COMPLEX, FAC_BIOENHANCEMENT_CENTER});
	}

	// nothing selected

	if (facilityId == -1)
		return;

	// priority

	double priority = 0.075 * base->mineral_surplus;

	debug
	(
		"\tfacility=%-25s"
		" priority=%5.2f"
		" base->mineral_surplus=%2d"
		"\n"
		, Facility[facilityId].name
		, priority
		, base->mineral_surplus
	);

	// add demand

	productionDemand.add(-facilityId, priority, true);

}

void evaluatePrototypingFacilities()
{
	debug("- evaluatePrototypingFacilities\n");

	int baseId = productionDemand.baseId;
	BASE *base = &(Bases[baseId]);

	// free prototype faction cannot build Skunkworks

	if (isFactionSpecial(aiFactionId, RFLAG_FREEPROTO))
	{
		debug("\tfree prototype faction cannot build Skunkworks\n");
		return;
	}

	// prototyping facility requires some minimal mineral surplus

	if (base->mineral_surplus < 10)
	{
		debug("\tbase->mineral_surplus < 10\n");
		return;
	}

	// check if all more productive bases already have Skunkworks

	bool next = true;

	for (int otherBaseId : aiData.baseIds)
	{
		BASE *otherBase = getBase(otherBaseId);

		// exclude self

		if (otherBaseId == baseId)
			continue;

		// exclude no better productive

		if (otherBase->mineral_intake_2 <= base->mineral_intake_2)
			continue;

		// check if more productive base has Skunkworks

		if (!isBaseHasFacility(baseId, FAC_SKUNKWORKS))
		{
			next = false;
			break;
		}

	}

	if (!next)
		return;

	// calculate approximate prototype cost and prototype cost coefficient

	int prototypeCost = std::max(aiData.factionInfos[aiFactionId].bestOffenseValue, aiData.factionInfos[aiFactionId].bestDefenseValue);
	double prototypeCostCoefficient = prototypeCost / 5.0;

	// priority coefficient

	double mineralSurplusCoefficient = (double)base->mineral_surplus / (double)aiData.maxMineralSurplus;
	double priorityCoefficient = pow(mineralSurplusCoefficient, 2);

	// Skunkworks

	if (has_facility_tech(aiFactionId, FAC_SKUNKWORKS) && !isBaseHasFacility(baseId, FAC_SKUNKWORKS))
	{
		int facilityId = FAC_SKUNKWORKS;

		double priority =
			prototypeCostCoefficient
			*
			priorityCoefficient
		;

		productionDemand.add(-facilityId, priority, true);

		debug
		(
			"\t\t%-25s"
			" priority=%5.2f"
			" prototypeCostCoefficient=%5.2f"
			" mineralSurplusCoefficient=%5.2f"
			" priorityCoefficient=%5.2f"
			, Facility[facilityId].name
			, priority
			, prototypeCostCoefficient
			, mineralSurplusCoefficient
			, priorityCoefficient
		);

	}

}

/*
Evaluates need for police units.
*/
void evaluatePolice()
{
	debug("evaluatePolice\n");

	int baseId = productionDemand.baseId;
	BaseInfo &baseInfo = aiData.getBaseInfo(baseId);

	// check police extra capacity

	int policeExtraCapacity = getBasePoliceExtraCapacity(baseId);

	if (policeExtraCapacity == 0)
		return;

	// police unit

	int unitId = findInfantryPoliceUnit(baseInfo.garrison.size() == 0);

	if (unitId == -1)
	{
		debug("\tunit not found\n");
		return;
	}

	// find min and max affected base sizes

	Interval baseSizeInterval = getPoliceBaseSizeInterval(baseId, unitId);

	// gain

	double gain = getBasePopulationGain(baseId, baseSizeInterval);

	// priority

	double priority = getItemProductionPriority(baseId, unitId, gain);

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" baseSizeInterval.min=%2d"
		" baseSizeInterval.max=%2d"
		" gain=%5.2f"
		"\n"
		, getUnit(unitId)->name
		, priority
		, baseSizeInterval.min
		, baseSizeInterval.max
		, gain
	);

	// add demand

	productionDemand.add(unitId, priority, true);

}

/*
Evaluates demand for land improvement.
*/
void evaluateLandFormerDemand()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	int baseLandAssociation = getBaseLandAssociation(baseId);

	debug("evaluateLandFormerDemand\n");

	// do not build second former in first 4 bases

	if (aiData.baseIds.size() <= 4)
	{
		int supportedFormers = 0;

		for (int vehicleId : aiData.formerVehicleIds)
		{
			VEH *vehicle = getVehicle(vehicleId);

			if (vehicle->home_base_id != baseId)
				continue;

			supportedFormers++;

		}

		if (supportedFormers >= 1)
		{
			debug("\tno more than 1 former in fist 4 bases\n");
			return;
		}

	}

	// find former unit

	int unitId = findFormerUnit(TRIAD_LAND);

	if (unitId == -1)
	{
		debug("\tno former unit\n");
		return;
	}

	debug("\t%-25s\n", getUnit(unitId)->name);

	// find range to demand associations

	robin_hood::unordered_flat_map<int, int> associationRanges;

	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		int tileLandAssociation = getLandAssociation(tile, aiFactionId);

		if (aiData.production.landFormerDemands.count(tileLandAssociation) == 0)
			continue;

		if (associationRanges.count(tileLandAssociation) == 0)
		{
			associationRanges.insert({tileLandAssociation, INT_MAX});
		}

		// update best range

		int range = getRange(baseTile, tile);

		associationRanges.at(tileLandAssociation) = std::min(associationRanges.at(tileLandAssociation), range);

	}

	// summarize total demand

	double landFormerDemand = 0.0;

	for (robin_hood::pair<int, double> &landFormerDemandEntry : aiData.production.landFormerDemands)
	{
		int landAssociation = landFormerDemandEntry.first;
		double demand = landFormerDemandEntry.second;
		int range = associationRanges.at(landAssociation);

		if (baseLandAssociation == landAssociation)
		{
			landFormerDemand += demand;
		}
		else
		{
			double rangeCoefficient = getExponentialCoefficient(10.0, range);
			landFormerDemand += rangeCoefficient * demand;
		}

	}

	landFormerDemand = std::min(1.0, landFormerDemand);

	// check demand

	if (landFormerDemand <= 0)
	{
		debug("\tlandFormerDemand <= 0\n");
		return;
	}

	// set priority

	double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId);

	// reduce priority for ocean bases building land unit

	double realmCoefficient = 1.0;

	if (baseLandAssociation == -1)
	{
		realmCoefficient = 0.2;
	}

	// priority

	double priority =
		conf.ai_production_improvement_priority
		* landFormerDemand
		* unitPriorityCoefficient
		* realmCoefficient
	;

	debug
	(
		"\tpriority=%5.2f"
		" conf.ai_production_improvement_priority=%5.2f"
		" landFormerDemand=%5.2f"
		" unitPriorityCoefficient=%5.2f"
		" realmCoefficient=%5.2f"
		"\n"
		, priority
		, conf.ai_production_improvement_priority
		, landFormerDemand
		, unitPriorityCoefficient
		, realmCoefficient
	);

	productionDemand.add(unitId, priority, false);

}

/*
Evaluates demand for sea improvement.
*/
void evaluateSeaFormerDemand()
{
	int baseId = productionDemand.baseId;
	int baseOceanAssociation = getBaseOceanAssociation(baseId);

	debug("evaluateSeaFormerDemand\n");

	// base is not a port

	if (baseOceanAssociation == -1)
	{
		debug("base is not a port\n");
		return;
	}

	// do not build second former in first 4 bases

	if (aiData.baseIds.size() <= 4)
	{
		int supportedFormers = 0;

		for (int vehicleId : aiData.formerVehicleIds)
		{
			VEH *vehicle = getVehicle(vehicleId);

			if (vehicle->home_base_id != baseId)
				continue;

			supportedFormers++;

		}

		if (supportedFormers >= 1)
		{
			debug("\tno more than 1 former in fist 4 bases\n");
			return;
		}

	}

	// find former unit

	int unitId = findFormerUnit(TRIAD_SEA);

	if (unitId == -1)
	{
		debug("\tno former unit\n");
		return;
	}

	debug("\t%-25s\n", getUnit(unitId)->name);

	// check demand

	if (aiData.production.seaFormerDemands.count(baseOceanAssociation) == 0)
	{
		debug("\tno demand in this ocean association\n");
		return;
	}

	double seaFormerDemand = aiData.production.seaFormerDemands.at(baseOceanAssociation);

	if (seaFormerDemand <= 0.0)
	{
		debug("\tseaFormerDemand <= 0.0\n");
		return;
	}

	// set priority

	double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId);
	double priority =
		conf.ai_production_improvement_priority
		*
		seaFormerDemand
		*
		unitPriorityCoefficient
	;

	debug
	(
		"\tpriority=%5.2f"
		", conf.ai_production_improvement_priority=%5.2f"
		", seaFormerDemand=%5.2f"
		", unitPriorityCoefficient=%5.2f\n"
		, priority
		, conf.ai_production_improvement_priority
		, seaFormerDemand
		, unitPriorityCoefficient
	);

	productionDemand.add(unitId, priority, false);

}

/*
Evaluates need for colony.
*/
void evaluateColony()
{
	const int TRACE = DEBUG && false;

	debug("evaluateColony TRACE=%d\n", TRACE);

	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;
	MAP *baseTile = getBaseMapTile(baseId);
	int baseLandAssociation = getBaseLandAssociation(baseId);
	int baseOceanAssociation = getBaseOceanAssociation(baseId);

	// verify base can build a colony

	if (!canBaseProduceColony(baseId))
	{
		debug("\tbase cannot build a colony\n");
		return;
	}

	// reduce land colony priority in land port base based on other land locked bases on same continent

	double landColonyPriorityCoefficient = 1.0;

	int totalContinentBaseCount = 0;
	int portContinentBaseCount = 0;

	if (baseLandAssociation != -1 && baseOceanAssociation != -1)
	{
		// get ocean unpopulated portion

		int totalOceanTiles = 0;
		int unpopulatedOceanTiles = 0;

		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int tileOceanAssociation = getOceanAssociation(tile, aiFactionId);

			if (tileOceanAssociation != baseOceanAssociation)
				continue;

			totalOceanTiles++;

			if (map_has_item(tile, BIT_BASE_RADIUS))
				continue;

			if (tile->owner == -1 || tile->owner == aiFactionId || isVendetta(aiFactionId, tile->owner))
			{
				unpopulatedOceanTiles++;
			}

		}

		double unpopulatedOceanTerritoryPortion = (totalOceanTiles == 0 ? 1.0 : (double)unpopulatedOceanTiles / (double)totalOceanTiles);

		for (int otherBaseId : aiData.baseIds)
		{
			int otherBaseLandAssociation = getBaseLandAssociation(otherBaseId);
			int otherBaseOceanAssociation = getBaseOceanAssociation(otherBaseId);

			// exclude not same continent

			if (otherBaseLandAssociation != baseLandAssociation)
				continue;

			// accumulate counters

			totalContinentBaseCount++;

			if (otherBaseOceanAssociation != -1)
			{
				portContinentBaseCount++;
			}

		}

		landColonyPriorityCoefficient *=
			+ (1.0 - unpopulatedOceanTerritoryPortion) * 1.0
			+ unpopulatedOceanTerritoryPortion * ((double)portContinentBaseCount / (double)totalContinentBaseCount)
		;

	}

	// find best build site score

	int bestColonyUnitId = -1;
	double bestColonyUnitPriority = 0.0;

	for (int unitId : aiData.colonyUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		int unitSpeed = getUnitSpeed(unitId);

		// base cannot build sea colony

		if (baseOceanAssociation == -1 && triad == TRIAD_SEA)
			continue;

		if (TRACE) { debug("\t[%3d] %-32s\n", unitId, unit->name); }

		// compute unit basic priority without extra delay

		int buildTime = getBaseItemBuildTime(baseId, unitId, true);

		// priority coefficient

		double priorityCoefficient = (triad == TRIAD_LAND ? landColonyPriorityCoefficient : 1.0);

		// iterate available build sites

		for (MAP *tile : availableBuildSites)
		{
			ExpansionTileInfo &expansionTileInfo = getExpansionTileInfo(tile);

			// exclude unavailable build sites

			if (aiData.production.unavailableBuildSites.count(tile) != 0)
				continue;

			// check unit can reach destination

			if (!isUnitDestinationReachable(base->faction_id, unitId, baseTile, tile, false))
				continue;

			// travel time

			executionProfiles["| getUnitATravelTime <- evaluateColony"].start();
			int travelTime = getUnitATravelTime(baseTile, tile, aiFactionId, unitId, unitSpeed, false, 1);
			executionProfiles["| getUnitATravelTime <- evaluateColony"].stop();

			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
				continue;

			// yield score
			// limit by 1.50 to not overwhelm by far lucrative landmarks

			double buildSiteScore = std::min(1.50, expansionTileInfo.buildSiteYieldScore);

			// same continent coefficient

			double sameContinentCoefficient =
				isSameLandAssociation(baseTile, tile, aiFactionId) ?
					conf.ai_production_expansion_same_continent_priority_multiplier
					:
					1.0
			;

			// priority

			double newBaseGain = getNewBaseGain(buildTime + travelTime);
			double newBasePriority = getProductionPriority(newBaseGain, unit->cost);

			double buildSiteNewBasePriority = newBasePriority * buildSiteScore;
			double priority = buildSiteNewBasePriority * sameContinentCoefficient * priorityCoefficient;

			if (TRACE)
			{
				debug
				(
					"\t\t(%3d,%3d)"
					" priority=%5.2f"
					" buildTime=%2d"
					" travelTime=%2d"
					" newBasePriority=%5.2f"
					" buildSiteScore=%5.2f"
					" buildSiteNewBasePriority=%5.2f"
					" sameContinentCoefficient=%5.2f"
					" priorityCoefficient=%5.2f"
					"\n"
					, getX(tile), getY(tile)
					, priority
					, buildTime
					, travelTime
					, newBasePriority
					, buildSiteScore
					, buildSiteNewBasePriority
					, sameContinentCoefficient
					, priorityCoefficient
				);
			}

			// update best

			if (priority > bestColonyUnitPriority)
			{
				bestColonyUnitId = unitId;
				bestColonyUnitPriority = priority;

				if (TRACE) { debug("\t\t\t- best\n"); }

			}

		}

	}

	// not found

	if (bestColonyUnitId == -1)
	{
		debug("\tcannot find best cololy unit\n");
		return;
	}

	// priority

	double priority = conf.ai_production_expansion_priority * globalBaseDemand * bestColonyUnitPriority * productionDemand.colonyCoefficient;

	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" conf.ai_production_expansion_priority=%5.2f"
		" globalBaseDemand=%5.2f"
		" bestColonyUnitPriority=%5.2f"
		" productionDemand.colonyCoefficient=%5.2f"
		"\n"
		, getUnit(bestColonyUnitId)->name
		, priority
		, conf.ai_production_expansion_priority
		, globalBaseDemand
		, bestColonyUnitPriority
		, productionDemand.colonyCoefficient
	)
	;

	// set base demand

	productionDemand.add(bestColonyUnitId, priority, false);

}

void evaluatePodPoppingDemand()
{
	evaluateLandPodPoppingDemand();
	evaluateSeaPodPoppingDemand();
}

void evaluateLandPodPoppingDemand()
{
	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);

	debug("evaluateLandPodPoppingDemand\n");

	// count pods within 10 cells from base

	int podCount = 0;
	robin_hood::unordered_flat_set<int> podAssociations;

	for (MAP *tile : getRangeTiles(baseTile, 10, false))
	{
		int x = getX(tile);
		int y = getY(tile);
		int association = getAssociation(tile, aiFactionId);

		// pod

		if (!mod_goody_at(x, y))
			continue;

		// land

		if (is_ocean(tile))
			continue;

		// reachable

		if (!isPotentiallyReachableAssociation(association, aiFactionId))
			continue;

		// not at war

		if (isVendetta(aiFactionId, tile->owner))
			continue;

		// accumulate

		podCount++;
		podAssociations.insert(association);

	}

	// no pods

	if (podCount == 0)
		return;

	// count scouts

	int scoutCount = 0;

	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int vehicleLandAssociation = getLandAssociation(vehicleTile, aiFactionId);

		// land

		if (triad != TRIAD_LAND)
			continue;

		// scout

		if (!isScoutVehicle(vehicleId))
			continue;

		// not in base

		if (isBaseAt(vehicleTile))
			continue;

		// within range

		if (getRange(baseTile, vehicleTile) > 10)
			continue;

		// in podAssociations

		if (podAssociations.count(vehicleLandAssociation) == 0)
			continue;

		// accumulate

		scoutCount++;

	}

	// set priority

	double scoutRequired = (double)podCount / conf.ai_production_pod_per_scout;
	double scoutDemand = getDemand(scoutRequired, scoutCount);

	double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId);
	double priority =
		conf.ai_production_pod_popping_priority
		*
		scoutDemand
		*
		unitPriorityCoefficient
	;

	debug
	(
		"\tpriority=%5.2f"
		" conf.ai_production_pod_popping_priority=%5.2f"
		" conf.ai_production_pod_per_scout=%5.2f"
		" podCount=%2d"
		" scoutRequired=%5.2f"
		" scoutCount=%2d"
		" scoutDemand=%5.2f"
		" unitPriorityCoefficient=%5.2f\n"
		, priority
		, conf.ai_production_pod_popping_priority
		, conf.ai_production_pod_per_scout
		, podCount
		, scoutRequired
		, scoutCount
		, scoutDemand
		, unitPriorityCoefficient
	);

	// select scout unit

	int unitId = findScoutUnit(TRIAD_LAND);

	if (unitId < 0)
	{
		debug("\tno scout unit\n");
		return;
	}

	debug("\t%-32s\n", getUnit(unitId)->name);

	// add production demand

	productionDemand.add(unitId, priority, false);

}

void evaluateSeaPodPoppingDemand()
{
	debug("evaluateSeaPodPoppingDemand\n");

	int baseId = productionDemand.baseId;
	MAP *baseTile = getBaseMapTile(baseId);
	int baseOceanAssociation = getBaseOceanAssociation(baseId);

	// not a port

	if (baseOceanAssociation == -1)
		return;

	// count pods within 20 cells from base

	int podCount = 0;

	for (MAP *tile : getRangeTiles(baseTile, 20, false))
	{
		int x = getX(tile);
		int y = getY(tile);
		bool ocean = is_ocean(tile);
		int association = getAssociation(tile, aiFactionId);

		// pod

		if (!mod_goody_at(x, y))
			continue;

		// sea

		if (!ocean)
			continue;

		// same ocean association

		if (baseOceanAssociation != association)
			continue;

		// accumulate

		podCount++;

	}

	// no pods

	if (podCount == 0)
		return;

	// count scouts

	int scoutCount = 0;

	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		MAP *vehicleTile = getVehicleMapTile(vehicleId);

		// sea

		if (triad != TRIAD_SEA)
			continue;

		// scout

		if (!isScoutVehicle(vehicleId))
			continue;

		// within range

		if (getRange(baseTile, vehicleTile) > 20)
			continue;

		// accumulate

		scoutCount++;

	}

	// set priority

	double scoutRequired = (double)podCount / conf.ai_production_pod_per_scout;
	double scoutDemand = getDemand(scoutRequired, scoutCount);

	double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId);
	double priority =
		conf.ai_production_pod_popping_priority
		*
		scoutDemand
		*
		unitPriorityCoefficient
	;

	debug
	(
		"\tpriority=%5.2f"
		", conf.ai_production_pod_popping_priority=%5.2f"
		", conf.ai_production_pod_per_scout=%5.2f"
		", unitPriorityCoefficient=%5.2f\n"
		, priority
		, conf.ai_production_pod_popping_priority
		, conf.ai_production_pod_per_scout
		, unitPriorityCoefficient
	);

	// select scout unit

	int podPopperUnitId = findScoutUnit(TRIAD_SEA);

	if (podPopperUnitId < 0)
	{
		debug("\tno scout unit\n");
		return;
	}

	debug("\tscout unit: %s\n", prod_name(podPopperUnitId));

	// add production demand

	productionDemand.add(podPopperUnitId, priority, false);

}

void evaluateProtectionDemand()
{
	debug("evaluateProtectionDemand\n");

	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
	BaseCombatData &baseCombatData = baseInfo.combatData;

	// no demand

	if (aiData.production.protectionDemand <= 0.0)
	{
		debug("\taiData.production.protectionDemand <= 0.0\n");
		return;
	}

	// find nearest base with melee protection demand

	int targetBaseId = -1;
	int targetBaseRange = INT_MAX;

	// base itself needs protection
	if (!baseCombatData.isSatisfied(true))
	{
		targetBaseId = baseId;
		targetBaseRange = 0;
	}
	// find other protection demanding base
	else
	{
		for (int otherBaseId : aiData.baseIds)
		{
			BASE *otherBase = &(Bases[otherBaseId]);
			BaseInfo &otherBaseInfo = aiData.getBaseInfo(otherBaseId);
			BaseCombatData &otherBaseCombatData = otherBaseInfo.combatData;

			// not this one

			if (otherBaseId == baseId)
				continue;

			// get protectionDemand

			double protectionDemand = otherBaseCombatData.getDemand(false);

			// exclude base without protectionDemand

			if (protectionDemand <= 0.0)
				continue;

			// get range

			int range = map_range(base->x, base->y, otherBase->x, otherBase->y);

			// update best

			if (range < targetBaseRange)
			{
				targetBaseId = otherBaseId;
				targetBaseRange = range;
			}

		}

	}

	// no target base

	if (targetBaseId == -1)
	{
		debug("\tno target base\n");
		return;
	}

	debug("\t-> %s\n", Bases[targetBaseId].name);

	// find unit

	int unitId = selectProtectionUnit(baseId, targetBaseId);

	// no unit found

	if (unitId == -1)
	{
		debug("\tno unit found\n");
		return;
	}

	debug("\t%s\n", Units[unitId].name);

	// unit priority coefficient is ignored for itself

	double unitPriorityCoefficient = (targetBaseId == baseId ? 2.0 : getUnitPriorityCoefficient(baseId));

	// calculate priority

	double priority =
		conf.ai_production_base_protection_priority
		*
		aiData.production.protectionDemand
		*
		unitPriorityCoefficient
	;

	debug
	(
		"\t%s -> %s, %s"
		", priority=%5.2f"
		" conf.ai_production_base_protection_priority=%5.2f"
		" aiData.production.protectionDemand=%5.2f"
		" unitPriorityCoefficient=%5.2f\n"
		, base->name, Bases[targetBaseId].name, Units[unitId].name
		, priority
		, conf.ai_production_base_protection_priority
		, aiData.production.protectionDemand
		, unitPriorityCoefficient
	);

	// add production

	productionDemand.add(unitId, priority, false);

}

void evaluateLandCombatDemand()
{
	int baseId = productionDemand.baseId;

	debug("evaluateLandCombatDemand\n");

	// no demand

	if (aiData.production.landCombatDemand <= 0.0)
	{
		debug("\taiData.production.landCombatDemand <= 0.0\n");
		return;
	}

	// find unit

	int unitId = selectCombatUnit(baseId, false);

	// no unit found

	if (unitId == -1)
	{
		debug("\tno unit found\n");
		return;
	}

	// calculate priority

	double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId);
	double priority =
		conf.ai_production_combat_priority
		*
		aiData.production.landCombatDemand
		*
		unitPriorityCoefficient
	;

	// add production

	productionDemand.add(unitId, priority, false);
	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" conf.ai_production_combat_priority=%5.2f"
		" unitPriorityCoefficient=%5.2f"
		" aiData.production.landCombatDemand=%5.2f\n"
		, getUnit(unitId)->name
		, priority
		, conf.ai_production_combat_priority
		, unitPriorityCoefficient
		, aiData.production.landCombatDemand
	);

}

void evaluateSeaCombatDemand()
{
	int baseId = productionDemand.baseId;
	int baseOceanAssociation = getBaseOceanAssociation(baseId);

	debug("evaluateSeaCombatDemand\n");

	// no demand

	if (aiData.production.seaCombatDemands[baseOceanAssociation] <= 0.0)
	{
		debug("\taiData.production.seaCombatDemands[baseOceanAssociation] <= 0.0\n");
		return;
	}

	// find unit

	int unitId = selectCombatUnit(baseId, true);

	// no unit found

	if (unitId == -1)
	{
		debug("\tno unit found\n");
		return;
	}

	// calculate priority

	double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId);
	double priority =
		conf.ai_production_combat_priority
		*
		aiData.production.seaCombatDemands[baseOceanAssociation]
		*
		unitPriorityCoefficient
	;

	// add production

	productionDemand.add(unitId, priority, false);
	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" conf.ai_production_combat_priority=%5.2f"
		" unitPriorityCoefficient=%5.2f"
		" aiData.production.seaCombatDemands[baseOceanAssociation]=%5.2f\n"
		, getUnit(unitId)->name
		, priority
		, conf.ai_production_combat_priority
		, unitPriorityCoefficient
		, aiData.production.seaCombatDemands[baseOceanAssociation]
	);

}

void evaluateLandAlienCombatDemand()
{
	debug("evaluateLandAlienCombatDemand\n");

	// do not bother if combat demand is set

	if (aiData.production.landCombatDemand > 0.0)
		return;

	int baseId = productionDemand.baseId;

	// no demand

	if (aiData.production.landAlienCombatDemand <= 0.0)
	{
		debug("\taiData.production.landAlienCombatDemand <= 0.0\n");
		return;
	}

	// find native attacker unit

	int unitId = findNativeAttackerUnit(false);

	if (unitId == -1)
	{
		debug("\tno native attacker unit\n");
		return;
	}

	// calculate priority

	double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId);
	double priority =
		conf.ai_production_alien_combat_priority
		*
		aiData.production.landAlienCombatDemand
		*
		unitPriorityCoefficient
	;

	// add production

	productionDemand.add(unitId, priority, false);
	debug
	(
		"\t%-32s"
		" priority=%5.2f"
		" conf.ai_production_alien_combat_priority=%5.2f"
		" aiData.production.landAlienCombatDemand=%5.2f"
		" unitPriorityCoefficient=%5.2f"
		"\n"
		, getUnit(unitId)->name
		, priority
		, conf.ai_production_alien_combat_priority
		, aiData.production.landAlienCombatDemand
		, unitPriorityCoefficient
	);

}

int findAntiNativeDefenderUnit(bool ocean)
{
	debug("findAntiNativeDefenderUnit\n");

    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];

		// infantry

		if (unit->chassis_id != CHS_INFANTRY)
			continue;

		// get true cost

		int cost = getUnitTrueCost(unitId, false);

		// compute protection against corresponding realm alien

		int basicAlienUnit = (ocean ? BSC_SEALURK : BSC_MIND_WORMS);
		double defendCombatEffect = aiData.combatEffectTable.getCombatEffect(unitId, 0, basicAlienUnit, 1, CT_MELEE);

		// compute effectiveness

		double effectiveness = defendCombatEffect / (double)cost;

		// prefer police

		if (unit_has_ability(unitId, ABL_POLICE_2X))
		{
			effectiveness *= 2.0;
		}

		// update best

		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

    return bestUnitId;

}

int findNativeAttackerUnit(bool ocean)
{
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.unitIds)
	{
		UNIT *unit = &(Units[unitId]);

		// skip wrong triad

		if (!(unit->triad() == (ocean ? TRIAD_SEA : TRIAD_LAND)))
			continue;

		// skip not scout

		if (!isScoutUnit(unitId))
			continue;

		// prefer empath unit

		double effectiveness;

		if (unit_has_ability(unitId, ABL_EMPATH))
		{
			effectiveness = 1.5;
		}
		else
		{
			effectiveness = 1.0;
		}

		// prefer fast unit after turn 100

		if (*CurrentTurn > 50)
		{
			effectiveness *= (double)unit->speed();
		}

		// update best

		if (bestUnitId == -1 || effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

    return bestUnitId;

}

int findScoutUnit(int triad)
{
	int bestUnitId = -1;
	double bestProportionalUnitChassisSpeed = -1;
	int bestUnitChassisSpeed = -1;

	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = &(Units[unitId]);

		// given triad only

		if (unit_triad(unitId) != triad)
			continue;

		// calculate speed and proportional speed

		int unitChassisSpeed = unit_chassis_speed(unitId);
		double proportionalUnitChassisSpeed = (double)unit_chassis_speed(unitId) / (double)unit->cost;

		// find fastest/cheapest unit

		if
		(
			bestUnitId == -1
			||
			proportionalUnitChassisSpeed > bestProportionalUnitChassisSpeed
			||
			(proportionalUnitChassisSpeed == bestProportionalUnitChassisSpeed && unitChassisSpeed > bestUnitChassisSpeed)
		)
		{
			bestUnitId = unitId;
			bestProportionalUnitChassisSpeed = proportionalUnitChassisSpeed;
			bestUnitChassisSpeed = unitChassisSpeed;
		}

	}

	return bestUnitId;

}

/*
Checks if base has enough population to issue a colony by the time it is built.
*/
bool canBaseProduceColony(int baseId)
{
	BASE *base = &(Bases[baseId]);

	// not allowed for mineral surplus less than 2

	if (base->mineral_surplus < 2)
		return false;

	// projectedTime

	int buildTime = getBaseItemBuildTime(baseId, BSC_COLONY_POD, true);
	int projectedSize = getBaseProjectedSize(baseId, buildTime);

	if (projectedSize == 1)
		return false;

	// current parameters

	int currentSize = base->pop_size;
	int currentSupport = base->mineral_intake_2 - base->mineral_surplus;

	// set projected

	base->pop_size = projectedSize - 1;
	computeBase(baseId, true);

	int projectedMineralIntake2 = base->mineral_intake_2;

	// restore

	base->pop_size = currentSize;
	computeBase(baseId, true);

	// check enough mineral intake

	if (projectedMineralIntake2 < currentSupport + 1)
		return false;

	// all checks passed

	return true;

}

int findFormerUnit(int triad)
{
	int bestUnitId = -1;
	double bestUnitValue = -1;

	for (int unitId : aiData.unitIds)
	{
		UNIT *unit = &(Units[unitId]);

		// formers only

		if (!isFormerUnit(unitId))
			continue;

		// given triad only or air

		if (!(unit_triad(unitId) == TRIAD_AIR || unit_triad(unitId) == triad))
			continue;

		// calculate value

		double unitTerraformingSpeedValue =
			(unit_has_ability(unitId, ABL_FUNGICIDAL) ? 1.2 : 1.0)
			*
			(unit_has_ability(unitId, ABL_SUPER_TERRAFORMER) ? 2.5 : 1.0)
		;
		double unitNativeProtectionValue = (unit_has_ability(unitId, ABL_TRANCE) ? 2.0 : 1.0);
		double unitChassisSpeedValue = 1.0 + (double)(unit_chassis_speed(unitId) - 1) * 1.5;
		double unitValue = unitTerraformingSpeedValue * unitNativeProtectionValue * unitChassisSpeedValue / (double)unit->cost;

		// find best unit

		if (bestUnitId == -1 || unitValue > bestUnitValue)
		{
			bestUnitId = unitId;
			bestUnitValue = unitValue;
		}

	}

	return bestUnitId;

}

int getRegionBasesMaxMineralSurplus(int factionId, int region)
{
	int maxMineralSurplus = 0;

	for (int baseId : getRegionBases(factionId, region))
	{
		BASE *base = &(Bases[baseId]);

		maxMineralSurplus = std::max(maxMineralSurplus, base->mineral_surplus);

	}

	return maxMineralSurplus;

}

int getRegionBasesMaxPopulationSize(int factionId, int region)
{
	int maxPopulationSize = 0;

	for (int baseId : getRegionBases(factionId, region))
	{
		BASE *base = &(Bases[baseId]);

		maxPopulationSize = std::max(maxPopulationSize, (int)base->pop_size);

	}

	return maxPopulationSize;

}

/*
Calculates number of surface units with specific weaponType in region: existing and building.
Only corresponding triad units are counted.
*/
int calculateRegionSurfaceUnitTypeCount(int factionId, int region, int weaponType)
{
	bool ocean = isOceanRegion(region);
	int triad = (ocean ? TRIAD_SEA : TRIAD_LAND);

	int regionUnitTypeCount = 0;

	for (int vehicleId : getRegionSurfaceVehicles(factionId, region, true))
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (Units[vehicle->unit_id].weapon_id != weaponType)
			continue;

		if (vehicle->triad() != triad)
			continue;

		regionUnitTypeCount++;

	}

	return regionUnitTypeCount;

}

/*
Calculates number of units with specific weaponType: existing and building.
Only corresponding triad units are counted.
*/
int calculateUnitTypeCount(int factionId, int weaponType, int triad, int excludedBaseId)
{
	int unitTypeCount = 0;

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);

		if (vehicle->faction_id != factionId)
			continue;

		if (Units[vehicle->unit_id].weapon_id != weaponType)
			continue;

		if ((triad != -1) && (vehicle->triad() != triad))
			continue;

		unitTypeCount++;

	}

	for (int baseId = 0; baseId < *BaseCount; baseId++)
	{
		// exclude this base

		if (baseId == excludedBaseId)
			continue;

		BASE *base = &(Bases[baseId]);
		int item = base->queue_items[0];

		if (base->faction_id != factionId)
			continue;

		if (item < 0)
			continue;

		if (Units[item].weapon_id != weaponType)
			continue;

		if (unit_triad(item) != triad)
			continue;

		unitTypeCount++;

	}

	return unitTypeCount;

}

/*
Checks for military purpose unit/structure.
*/
bool isMilitaryItem(int item)
{
	if (item >= 0)
	{
		UNIT *unit = &(Units[item]);
		int weaponId = unit->weapon_id;

		return
			(isCombatUnit(item) && !isScoutUnit(item))
			||
			weaponId == WPN_PROBE_TEAM
			||
			weaponId == WPN_CONVENTIONAL_PAYLOAD
			||
			weaponId == WPN_PLANET_BUSTER
		;

	}
	else
	{
		return false;
	}

}

int findAttackerUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	int baseOceanAssociation = getBaseOceanAssociation(baseId);
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseOceanAssociation = getBaseOceanAssociation(targetBaseId);
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();

    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();
        int chassisSpeed = unit->speed();

        // exclude land artillery

        if (triad == TRIAD_LAND && unit_has_ability(unitId, ABL_ARTILLERY))
			continue;

		// skip unmatching triad

		switch (triad)
		{
		case TRIAD_LAND:
			{
				// no land attacker for ocean base

				if (targetBaseOcean)
					continue;

			}
			break;

		case TRIAD_SEA:
			{
				// no sea attacker for land base

				if (!targetBaseOcean)
					continue;

				// no sea attacker if in different associations

				if (baseOceanAssociation == -1 || targetBaseOceanAssociation == -1 || targetBaseOceanAssociation != baseOceanAssociation)
					continue;

			}

		}

		// skip units unable to attack each other

		if
		(
			(foeUnitTriad == TRIAD_LAND && triad == TRIAD_SEA)
			||
			(foeUnitTriad == TRIAD_SEA && triad == TRIAD_LAND)
			||
			(foeUnit->chassis_id == CHS_NEEDLEJET && !unit_has_ability(unitId, ABL_AIR_SUPERIORITY))
		)
		{
			continue;
		}

		// relative strength

		double relativeStrength;

		if (unit->weapon_id == WPN_PSI_ATTACK || foeUnit->armor_id == ARM_PSI_DEFENSE)
		{
			relativeStrength =
				(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
				/ foeUnitStrength->psiDefense
			;

		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_id) * getNewUnitConventionalOffenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalDefense
				* getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;

		}

		// unit speed factor
		// land unit is assumed to move on roads

		int infantrySpeed = getLandUnitSpeedOnRoads(BSC_SCOUT_PATROL);
		int speed = (triad == TRIAD_LAND ? getLandUnitSpeedOnRoads(unitId) : chassisSpeed);
		double speedFactor = (double)(speed + 3) / (double)(infantrySpeed + 3);

		// calculate cost counting support

		int cost = getUnitTrueCost(unitId, true);

		// calculate effectiveness

		double effectiveness = relativeStrength * speedFactor / (double)cost;

		// update best effectiveness

		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

    return bestUnitId;

}

int findDefenderUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	int baseOceanAssociation = getBaseOceanAssociation(baseId);
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseOceanAssociation = getBaseOceanAssociation(targetBaseId);
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();

    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();

        // exclude land artillery

        if (triad == TRIAD_LAND && unit_has_ability(unitId, ABL_ARTILLERY))
			continue;

		// exclude air

		if (triad == TRIAD_AIR)
			continue;

		// skip unmatching triad

		switch (triad)
		{
		case TRIAD_LAND:
			{
				// land defender is allowed for ocean base

			}
			break;

		case TRIAD_SEA:
			{
				// no sea defender for land base

				if (!targetBaseOcean)
					continue;

				// no sea defender if in different associations

				if (baseOceanAssociation == -1 || targetBaseOceanAssociation == -1 || targetBaseOceanAssociation != baseOceanAssociation)
					continue;

			}

		}

		// skip units unable to attack each other

		if
		(
			(foeUnitTriad == TRIAD_LAND && triad == TRIAD_SEA)
			||
			(foeUnitTriad == TRIAD_SEA && triad == TRIAD_LAND)
		)
		{
			continue;
		}

		// relative strength

		double relativeStrength;

		if (unit->armor_id == ARM_PSI_DEFENSE || foeUnit->weapon_id == WPN_PSI_ATTACK)
		{
			relativeStrength =
				(10.0 * getNewUnitPsiDefenseStrength(unitId, baseId))
				/ foeUnitStrength->psiOffense
			;


		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_id) * getNewUnitConventionalDefenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalOffense
				/ getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;

		}

		// chassis speed factor
		// not important for base defender

		double speedFactor = 1.0;

		// calculate cost counting maintenance

		int cost = getUnitTrueCost(unitId, false);

		// calculate effectiveness

		double effectiveness = relativeStrength * speedFactor / (double)cost;

		// update best effectiveness

		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

    return bestUnitId;

}

int findMixedUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	int baseOceanAssociation = getBaseOceanAssociation(baseId);
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	int targetBaseOceanAssociation = getBaseOceanAssociation(targetBaseId);
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();

    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();

        // exclude land artillery

        if (triad == TRIAD_LAND && unit_has_ability(unitId, ABL_ARTILLERY))
			continue;

		// exclude air

		if (triad == TRIAD_AIR)
			continue;

		// skip unmatching triad

		switch (triad)
		{
		case TRIAD_LAND:
			{
				// no land attacker/mixed for ocean base

				if (targetBaseOcean)
					continue;

			}
			break;

		case TRIAD_SEA:
			{
				// no sea attacker/mixed for land base

				if (!targetBaseOcean)
					continue;

				// no sea attacker/mixed if in different associations

				if (baseOceanAssociation == -1 || targetBaseOceanAssociation == -1 || targetBaseOceanAssociation != baseOceanAssociation)
					continue;

			}

		}

		// skip units unable to attack each other

		if
		(
			(foeUnitTriad == TRIAD_LAND && triad == TRIAD_SEA)
			||
			(foeUnitTriad == TRIAD_SEA && triad == TRIAD_LAND)
		)
		{
			continue;
		}

		// relative strength

		double relativeStrength = 1.0;

		if (unit->weapon_id == WPN_PSI_ATTACK || foeUnit->armor_id == ARM_PSI_DEFENSE)
		{
			relativeStrength *=
				(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
				/ foeUnitStrength->psiDefense
			;

		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_id) * getNewUnitConventionalOffenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalDefense
				* getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;

		}

		if (unit->armor_id == ARM_PSI_DEFENSE || foeUnit->weapon_id == WPN_PSI_ATTACK)
		{
			relativeStrength *=
				(10.0 * getNewUnitPsiDefenseStrength(unitId, baseId))
				/ foeUnitStrength->psiOffense
			;

		}
		else
		{
			relativeStrength =
				((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_id) * getNewUnitConventionalDefenseStrength(unitId, baseId))
				/ foeUnitStrength->conventionalOffense
				/ getConventionalCombatBonusMultiplier(unitId, foeUnitId)
			;

		}

		// chassis speed factor
		// average between attacker and defender

		int chassisSpeed = unit->speed() * (triad == TRIAD_LAND ? 3 : 1);
		double speedFactor = (double)(chassisSpeed + 3) / (double)(Chassis[CHS_INFANTRY].speed + 3);
		speedFactor = sqrt(speedFactor);

		// calculate cost counting maintenance

		int cost = getUnitTrueCost(unitId, false);

		// calculate effectiveness

		double effectiveness = relativeStrength * speedFactor / (double)cost;

		// update best effectiveness

		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

    return bestUnitId;

}

int findArtilleryUnit(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	MAP *targetBaseTile = getBaseMapTile(targetBaseId);
	bool targetBaseOcean = is_ocean(targetBaseTile);
	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();

    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int triad = unit->triad();

        // exclude everything but land artillery

        if (!(triad == TRIAD_LAND && unit_has_ability(unitId, ABL_ARTILLERY)))
			continue;

		// skip unmatching triad

		switch (triad)
		{
		case TRIAD_LAND:
			{
				// no land artillery for ocean base

				if (targetBaseOcean)
					continue;

			}
			break;

		}

		// skip units unable to attack each other
		// artillery cannot attack air

		if (foeUnitTriad == TRIAD_AIR)
		{
			continue;
		}

		// relative strength

		double relativeStrength;

		if (unit_has_ability(foeUnitId, ABL_ARTILLERY))
		{
			if (unit->weapon_id == WPN_PSI_ATTACK || foeUnit->weapon_id == WPN_PSI_ATTACK)
			{
				relativeStrength =
					(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
					/ foeUnitStrength->psiOffense
				;

			}
			else
			{
				if (conf.conventional_artillery_duel_uses_weapon_and_armor)
				{
					relativeStrength =
						(
							(conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_id)
							*
							(getNewUnitConventionalOffenseStrength(unitId, baseId) + getNewUnitConventionalDefenseStrength(unitId, baseId))
						)
						/
						(foeUnitStrength->conventionalOffense + foeUnitStrength->conventionalDefense)
					;

				}
				else
				{
					relativeStrength =
						((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_id) * getNewUnitConventionalOffenseStrength(unitId, baseId))
						/ foeUnitStrength->conventionalOffense
					;

				}

			}

		}
		else
		{
			if (unit->weapon_id == WPN_PSI_ATTACK || foeUnit->armor_id == ARM_PSI_DEFENSE)
			{
				relativeStrength =
					(10.0 * getNewUnitPsiOffenseStrength(unitId, baseId))
					/ foeUnitStrength->psiDefense
				;

			}
			else
			{
				relativeStrength =
					((conf.ignore_reactor_power_in_combat ? 10.0 : unit->reactor_id) * getNewUnitConventionalOffenseStrength(unitId, baseId))
					/ foeUnitStrength->conventionalDefense
				;

			}

		}

		// chassis speed factor
		// average between attacker and defender

		int chassisSpeed = unit->speed() * (triad == TRIAD_LAND ? 3 : 1);
		double speedFactor = (double)(chassisSpeed + 3) / (double)(Chassis[CHS_INFANTRY].speed + 3);
		speedFactor = sqrt(speedFactor);

		// calculate cost counting maintenance

		int cost = getUnitTrueCost(unitId, true);

		// calculate effectiveness

		double effectiveness = relativeStrength * speedFactor / (double)cost;

		// update best effectiveness

		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

    return bestUnitId;

}

int findPrototypeUnit()
{
	int bestPrototypeUnitId = -1;
	int bestPrototypeUnitIdCost = INT_MAX;

	for (int unitId : aiData.prototypeUnitIds)
	{
		UNIT *unit = getUnit(unitId);

		int cost = unit->cost;

		if (cost < bestPrototypeUnitIdCost)
		{
			bestPrototypeUnitId = unitId;
			bestPrototypeUnitIdCost = cost;
		}

	}

	return bestPrototypeUnitId;

}

/*
Finds best combat unit built at source base to help target base against target base threats.
*/
int selectProtectionUnit(int baseId, int targetBaseId)
{
	bool TRACE = DEBUG && false;

	if (TRACE) { debug("selectProtectionUnit\n"); }

	int baseOceanAssociation = getBaseOceanAssociation(baseId);

	// no target base

	if (targetBaseId == -1)
	{
		return -1;
	}

	// get target base strategy

	bool targetBaseOcean = is_ocean(getBaseMapTile(targetBaseId));
	int targetBaseOceanAssociation = getBaseOceanAssociation(targetBaseId);
	BaseInfo &targetBaseInfo = aiData.getBaseInfo(targetBaseId);
	BaseCombatData &targetBaseCombatData = targetBaseInfo.combatData;

	// iterate best protectors at target base

	int bestUnitId = -1;
	double bestUnitPreference = 0.0;

	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		int offenseValue = getUnitOffenseValue(unitId);
		int defenseValue = getUnitDefenseValue(unitId);
		double averageCombatEffect = targetBaseCombatData.getUnitEffect(unitId);

		// infantry defender

		if (!isInfantryDefensiveUnit(unitId))
			continue;

		// skip that unfit for target base or cannot travel there

		switch (triad)
		{
		case TRIAD_LAND:
			{
				// do not send any land unit besides infantry defensive to ocean base unless amphibious

				if (targetBaseOcean && !isInfantryDefensiveUnit(unitId))
					continue;

			}
			break;

		case TRIAD_SEA:
			{
				// do not send sea units to different ocean association

				if (baseOceanAssociation == -1 || targetBaseOceanAssociation == -1 || baseOceanAssociation != targetBaseOceanAssociation)
					continue;

			}
			break;

		}

		// compute true cost
		// this should not include prototype cost

		int cost = getUnitTrueCost(unitId, false);

		// compute effectiveness

		double effectiveness = averageCombatEffect / (double)cost;

		// give slight preference to more advanced units

		double preferenceModifier = 1.0;

		if (!isNativeUnit(unitId))
		{
			preferenceModifier *= (1.0 + 0.2 * (double)((offenseValue - 1) + (defenseValue - 1)));
		}

		// give preference to police2x if it is already selected

		if (isInfantryPolice2xUnit(unitId, aiFactionId))
		{
			double policeUnitPriority = productionDemand.getItemPriority(unitId);
			preferenceModifier *= (1.0 + 3.0 * policeUnitPriority);
		}

		// combined preference

		double preference = effectiveness * preferenceModifier;

		// update best

		if (TRACE)
		{
			debug
			(
				"\t%-32s"
				" preference=%5.2f"
				" cost=%2d"
				" averageCombatEffect=%5.2f"
				" effectiveness=%5.2f"
				" preferenceModifier=%5.2f"
				"\n"
				, getUnit(unitId)->name
				, preference
				, cost
				, averageCombatEffect
				, effectiveness
				, preferenceModifier
			);
		}

		if (preference > bestUnitPreference)
		{
			bestUnitId = unitId;
			bestUnitPreference = preference;
			if (TRACE) { debug("\t\t- best\n"); }
		}

	}

	// return

	return bestUnitId;

}

/*
Finds best combat unit built at base to attack stack.
*/
int selectCombatUnit(int baseId, bool ocean)
{
	debug("selectCombatUnit ocean=%d\n", ocean);

	MAP *baseTile = getBaseMapTile(baseId);

	// iterate units

	int bestUnitId = -1;
	double bestUnitEffectiveness = 0.0;

	for (int unitId : aiData.combatUnitIds)
	{
		UNIT *unit = getUnit(unitId);
		int triad = unit->triad();
		int unitSpeed = getUnitSpeed(unitId);

		// corresponding realm

		if ((ocean && triad == TRIAD_LAND) || (!ocean && triad == TRIAD_SEA))
			continue;

		double totalWeight = 0.0;
		double totalWeightedEffect = 0.0;

		// empty bases

		for (MAP *enemyBaseTile : aiData.emptyEnemyBaseTiles)
		{
			bool enemyBaseTileOcean = is_ocean(enemyBaseTile);

			// corresponding realm

			if (enemyBaseTileOcean != ocean)
				continue;

			// can capture base

			if (!isUnitCanCaptureBase(aiFactionId, unitId, baseTile, enemyBaseTile))
				continue;

			// travelTime coefficient

			int travelTime = getUnitATravelTime(baseTile, enemyBaseTile, aiFactionId, unitId, unitSpeed, false, 0);

			if (travelTime == -1 || travelTime >= MOVEMENT_INFINITY)
				continue;

			double travelTimeCoefficient = getExponentialCoefficient(conf.ai_combat_travel_time_scale, travelTime);

			// combat priority

			double combatPriority = conf.ai_combat_attack_priority_base;

			// costCoefficient

			double costCoefficient = 2.0;

			// weight

			double weight = travelTimeCoefficient;
			totalWeight += weight;

			// weighted effect

			double weightedEffect = weight * combatPriority * costCoefficient;
			totalWeightedEffect += weightedEffect;

		}

		// stacks

		for (robin_hood::pair<MAP *, EnemyStackInfo> &enemyStackEntry : aiData.enemyStacks)
		{
			MAP *enemyStackTile = enemyStackEntry.first;
			EnemyStackInfo &enemyStackInfo = enemyStackEntry.second;

			// range coefficient

			double rangeCoefficient = getExponentialCoefficient(conf.ai_territory_threat_range_scale, enemyStackInfo.baseRange);

			// combat priority

			double combatPriority;

			if (isBaseAt(enemyStackTile))
			{
				combatPriority = conf.ai_combat_attack_priority_base;
			}
			else
			{
				combatPriority = 1.0;
			}

			// weight

			double weight = rangeCoefficient;
			totalWeight += weight;

			// melee attack

			if (isUnitCanMeleeAttackStack(aiFactionId, unitId, baseTile, enemyStackTile, enemyStackInfo))
			{
				// weighted effect

				double effect = enemyStackInfo.getUnitEffect(unitId, AT_MELEE).effect;
				double weightedEffect = weight * combatPriority * effect;
				totalWeightedEffect += weightedEffect;

			}

			// artillery attack

			if (isUnitCanArtilleryAttackStack(aiFactionId, unitId, baseTile, enemyStackTile, enemyStackInfo))
			{
				// weighted effect

				double effect = enemyStackInfo.getUnitEffect(unitId, AT_ARTILLERY).effect;
				double weightedEffect = weight * combatPriority * effect;
				totalWeightedEffect += weightedEffect;

			}

		}

		if (totalWeight == 0.0)
			continue;

		// unit weight

		double unitWeightedEffect = totalWeightedEffect / totalWeight;

		// compute true cost

		int cost = getUnitTrueCost(unitId, true);

		// compute effectiveness

		double effectiveness = unitWeightedEffect / (double)cost;

		debug
		(
			"\t%-32s"
			" effectiveness=%7.4f"
			" cost=%2d"
			" unitWeightedEffect=%5.2f"
			"\n"
			, unit->name
			, effectiveness
			, cost
			, unitWeightedEffect
		);

		// update best

		if (effectiveness > bestUnitEffectiveness)
		{
			debug("\t\t- best\n");
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

	// return

	return bestUnitId;

}

int selectAlienProtectorUnit()
{
	int bestUnitId = -1;
	int bestUnitTrance = 0;
	int bestUnitCost = INT_MAX;

	for (int unitId : aiData.unitIds)
	{
		UNIT *unit = &(Units[unitId]);

		// exclude not infantry

		if (unit->chassis_id != CHS_INFANTRY)
			continue;

		// get trance

		int trance = (isUnitHasAbility(unitId, ABL_TRANCE) ? 1 : 0);

		// get cost

		int cost = unit->cost;

		// update best

		if
		(
			(trance > bestUnitTrance)
			||
			(trance == bestUnitTrance && cost < bestUnitCost)
		)
		{
			bestUnitId = unitId;
			bestUnitTrance = trance;
			bestUnitCost = cost;
		}

	}

	return bestUnitId;

}

void evaluateTransportDemand()
{
	debug("evaluateTransportDemand\n");

	int baseId = productionDemand.baseId;

	for (const auto &k : aiData.seaTransportRequestCounts)
	{
		int association = k.first;
		int seaTransportDemand = k.second;

		// no demand

		if (seaTransportDemand <= 0)
		{
			debug("seaTransportDemand <= 0\n");
			continue;
		}

		// base in association

		if (getBaseOceanAssociation(baseId) != association)
			continue;

		debug("\tassociation=%d\n", association);

		// count number of available transports in association

		double transportationCoverage = 0.0;

		int transportFoilSpeed = getUnitSpeed(BSC_TRANSPORT_FOIL);
		for (int vehicleId : aiData.vehicleIds)
		{
			int vehicleSpeed = getVehicleSpeed(vehicleId);
			int remainingCapacity = getTransportRemainingCapacity(vehicleId);

			if (isTransportVehicle(vehicleId) && getVehicleAssociation(vehicleId) == association)
			{
				// adjust value for capacity and speed

				transportationCoverage += 0.5 * remainingCapacity * ((double)vehicleSpeed / (double)transportFoilSpeed);

			}

		}

		// demand is satisfied

		if (transportationCoverage >= (double)seaTransportDemand)
		{
			debug("\ttransportationCoverage=%5.2f >= seaTransportDemand=%d\n", transportationCoverage, seaTransportDemand);
			continue;
		}

		// calculate demand

		double transportDemand = getDemand((double)seaTransportDemand, transportationCoverage);

		debug
		(
			"\ttransportDemand=%5.2f"
			" seaTransportDemand=%2d"
			" transportationCoverage=%5.2f"
			"\n"
			, transportDemand
			, seaTransportDemand
			, transportationCoverage
		);

		// base count coefficient

		double baseCountCoefficient = (aiData.baseIds.size() < 8U ? (double)aiData.baseIds.size() / 8.0 : 1.0);

		// set priority

		double unitPriorityCoefficient = getUnitPriorityCoefficient(baseId);
		double priority =
			conf.ai_production_transport_priority
			*
			transportDemand
			*
			unitPriorityCoefficient
			*
			baseCountCoefficient
		;

		debug
		(
			"\tpriority=%5.2f"
			" conf.ai_production_transport_priority=%5.2f"
			" transportDemand=%5.2f"
			" unitPriorityCoefficient=%5.2f"
			" baseCountCoefficient=%5.2f"
			"\n"
			, priority
			, conf.ai_production_transport_priority
			, transportDemand
			, unitPriorityCoefficient
			, baseCountCoefficient
		);

		// find transport unit

		int unitId = findTransportUnit();

		if (unitId == -1)
		{
			debug("\tno transport unit\n");
			continue;
		}

		debug("\t%s\n", Units[unitId].name);

		// set demand

		productionDemand.add(unitId, priority, false);

	}

}

int findTransportUnit()
{
	int bestUnitId = -1;
	double bestUnitEffectiveness = 0.0;

	for (int unitId : aiData.unitIds)
	{
		UNIT *unit = &(Units[unitId]);
		int triad = unit->triad();

		// sea

		if (triad != TRIAD_SEA)
			continue;

		// transport

		if (!isTransportUnit(unitId))
			continue;

		// calculate effectiveness

		double effectiveness = 1.0;

		// faster unit

		effectiveness *= unit->speed();

		// more capacity unit

		if (unit_has_ability(unitId, ABL_HEAVY_TRANSPORT))
		{
			effectiveness *= 1.5;
		}

		// update best

		if (bestUnitId == -1 || effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

	return bestUnitId;

}

void evaluateProjectDemand()
{
	int baseId = productionDemand.baseId;
	BASE *base = productionDemand.base;

	debug("evaluateProjectDemand\n");

	if (isBaseBuildingProject(baseId))
	{
		productionDemand.add(base->queue_items[0], conf.ai_production_current_project_priority, true);
	}

}

/*
Returns first available but unbuilt facility from list.
*/
int getFirstAvailableFacility(int baseId, std::vector<int> facilityIds)
{
	BASE *base = &(Bases[baseId]);

	for (int facilityId : facilityIds)
	{
		if (has_facility_tech(base->faction_id, facilityId) && !isBaseHasFacility(baseId, facilityId))
		{
			return facilityId;
		}

	}

	// everything available is built

	return -1;

}

/*
Evaluates base summary resource score.
*/
double getBaseIncome(int baseId)
{
	BASE *base = getBase(baseId);
	return getResourceScore(base->mineral_intake_2, base->economy_total + base->labs_total);
}

/*
Averages base worker relative income.
*/
double getBaseWorkerRelativeIncome(int baseId)
{
	BASE *base = getBase(baseId);

	int workerCount = base->pop_size - base->specialist_total;

	if (workerCount <= 0)
		return 0.0;

	double baseTileYieldScore = getResourceScore(*BIT_BASE_SQ_MINERALS, *BIT_BASE_SQ_ENERGY);
	double totalYieldScore = getResourceScore(base->mineral_intake, base->energy_intake);

	if (totalYieldScore <= 0.0)
		return 0.0;

	return (1.0 - baseTileYieldScore / totalYieldScore) / (double)workerCount;

}

/*
Averages base worker income.
*/
double getBaseWorkerIncome(int baseId)
{
	double income = getBaseIncome(baseId);
	return getBaseWorkerRelativeIncome(baseId) * income;
}

/*
Averages base worker nutrient surplus.
*/
double getBaseWorkerNutrientIntake(int baseId)
{
	BASE *base = getBase(baseId);

	int workerCount = base->pop_size - base->specialist_total;

	if (workerCount <= 0)
		return 0.0;

	int baseTileNutrientIntake = *BIT_BASE_SQ_NUTRIENT;
	int totalNutrientIntake = base->nutrient_intake;

	if (totalNutrientIntake <= 0)
		return 0.0;

	return (double)(totalNutrientIntake - baseTileNutrientIntake) / (double)workerCount;

}

/*
Returns average worker generated income across all bases.
*/
double getCitizenIncome()
{
	return conf.ai_citizen_income;
}

///*
//Estimates income growth proportion based on statistical data.
//It uses base founded year stored in base information to know base age.
//*/
//double getBaseRelativeIncomeGrowth(int baseId)
//{
//	int age = getBaseAge(baseId);
//	return conf.ai_base_income_a / (conf.ai_base_income_a * (double)age + conf.ai_base_income_b);
//}
//
///*
//Estimates income growth based on citizen income and income growth proportion.
//It uses base founded year stored in base information to know base age.
//*/
//double getBaseIncomeGrowth(int baseId)
//{
//	int age = getBaseAge(baseId);
//
//	// average income
//
//	double averageIncome = conf.ai_base_income_a * (double)age + conf.ai_base_income_b;
//
//	// base income
//
//	double baseIncome = getBaseIncome(baseId);
//
//	// average income growth
//
//	double averageIncomeGrowth = conf.ai_base_income_a;
//
//	// base adjusted income growth
//
//	return averageIncomeGrowth * (baseIncome / averageIncome);
//
//}
//
/*
Estimates citizen contribution to base income growth.
*/
//double getCitizenIncomeGrowth(int baseId)
//{
//	BASE *base = getBase(baseId);
//
//	return getBaseIncomeGrowth(baseId) / (double)base->pop_size;
//
//}
//
std::vector<int> getProtectionCounterUnits(int baseId, int targetBaseId, int foeUnitId, UnitStrength *foeUnitStrength)
{
	debug("getProtectionCounterUnits\n");

	UNIT *foeUnit = &(Units[foeUnitId]);
	int foeUnitTriad = foeUnit->triad();

	std::vector<int> counterUnitIds;

	switch (foeUnitTriad)
	{
	case TRIAD_LAND:
		{
			if (unit_has_ability(foeUnitId, ABL_ARTILLERY))
			{
				int attackerUnit = findAttackerUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (attackerUnit != -1)
				{
					counterUnitIds.push_back(attackerUnit);
				}

				int artilleryUnit = findArtilleryUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (artilleryUnit != -1)
				{
					counterUnitIds.push_back(artilleryUnit);
				}

			}
			else
			{
				int defenderUnit = findDefenderUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (defenderUnit != -1)
				{
					counterUnitIds.push_back(defenderUnit);
				}

				int mixedUnit = findMixedUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (mixedUnit != -1)
				{
					counterUnitIds.push_back(mixedUnit);
				}

				int attackerUnit = findAttackerUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (attackerUnit != -1)
				{
					counterUnitIds.push_back(attackerUnit);
				}

				int artilleryUnit = findArtilleryUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
				if (artilleryUnit != -1)
				{
					counterUnitIds.push_back(artilleryUnit);
				}

			}

		}
		break;

	case TRIAD_SEA:
		{
			int defenderUnit = findDefenderUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (defenderUnit != -1)
			{
				counterUnitIds.push_back(defenderUnit);
			}

			int mixedUnit = findMixedUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (mixedUnit != -1)
			{
				counterUnitIds.push_back(mixedUnit);
			}

		}
		break;

	case TRIAD_AIR:
		{
			int defenderUnit = findDefenderUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (defenderUnit != -1)
			{
				counterUnitIds.push_back(defenderUnit);
			}

			int mixedUnit = findMixedUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (mixedUnit != -1)
			{
				counterUnitIds.push_back(mixedUnit);
			}

			int attackerUnit = findAttackerUnit(baseId, targetBaseId, foeUnitId, foeUnitStrength);
			if (attackerUnit != -1)
			{
				counterUnitIds.push_back(attackerUnit);
			}

		}
		break;

	}

	return counterUnitIds;

}

/*
Calculates unit priority reduction based on existing support.
*/
double getUnitPriorityCoefficient(int baseId)
{
	BASE *base = &(Bases[baseId]);

	// at max

	if (*VehCount >= MaxVehNum)
		return 0.0;

	// calculate current and allowed support

	int mineralIntake2 = base->mineral_intake_2;
	int mineralSurplus = base->mineral_surplus;
	int currentSupport = mineralIntake2 - mineralSurplus;
	double allowedSupport = conf.ai_production_support_ratio * (double)mineralIntake2;

	// coefficient range: 0.0 - 2.0

	return (allowedSupport > 0.0 && currentSupport < allowedSupport ? 2.0 * (1.0 - (double)currentSupport / allowedSupport) : 0.0);

}

int findInfantryPoliceUnit(bool first)
{
    int bestUnitId = -1;
    double bestUnitEffectiveness = 0.0;

    // search among police2x

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int defenseValue = getUnitDefenseValue(unitId);

        // infantry police2x

        if (!isInfantryDefensivePolice2xUnit(unitId, aiFactionId))
			continue;

		// calculate effectiveness

		double benefit = 1.0 + (first ? 0.0 : ((double)(defenseValue - 1) / (double)aiData.bestDefenseValue) / 2.0);
		double cost = (double)unit->cost;

		if (!isUnitHasAbility(unitId, ABL_CLEAN_REACTOR))
		{
			cost += 8.0;
		}

		if (isUnitHasAbility(unitId, ABL_TRANCE))
		{
			benefit *= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi / 2);
		}

		double effectiveness = (cost <= 0.0 ? 0.0 : benefit / cost);

		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

	if (bestUnitId != -1)
	{
		return bestUnitId;
	}

	// search among others

    for (int unitId : aiData.combatUnitIds)
	{
        UNIT *unit = &Units[unitId];
        int defenseValue = getUnitDefenseValue(unitId);

        // infantry

        if (!isInfantryDefensiveUnit(unitId))
			continue;

		// calculate effectiveness

		double benefit = 1.0 + (first ? 0.0 : ((double)(defenseValue - 1) / (double)aiData.bestDefenseValue) / 2.0);
		double cost = (double)unit->cost;

		if (!isUnitHasAbility(unitId, ABL_CLEAN_REACTOR))
		{
			cost += 8.0;
		}

		if (isUnitHasAbility(unitId, ABL_TRANCE))
		{
			benefit *= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi / 2);
		}

		double effectiveness = (cost <= 0.0 ? 0.0 : benefit / cost);

		if (effectiveness > bestUnitEffectiveness)
		{
			bestUnitId = unitId;
			bestUnitEffectiveness = effectiveness;
		}

	}

	return bestUnitId;

}

void evaluateGlobalPoliceDemand()
{
	debug("evaluateGlobalPoliceDemand - %s\n", getMFaction(aiFactionId)->noun_faction);

	// calculate police request

	int policeRequest = 0;

	for (int baseId : aiData.baseIds)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);
		PoliceData &basePoliceData = baseInfo.policeData;

		debug
		(
			"\t%-25s policeAllowed=%d, policeDrones=%d\n"
			, getBase(baseId)->name, basePoliceData.unitsAllowed, basePoliceData.dronesExisting
		);

		// exclude base without police need

		if (basePoliceData.isSatisfied())
			continue;

		// accumulate requests

		policeRequest++;

	}

	// compute demand

	aiData.production.policeDemand =
		(aiData.baseIds.size() == 0 ? 0.0 : (policeRequest > 0 ? 1.0 : 0.0));

	debug
	(
		"\tpoliceDemand=%5.2f, policeRequest=%d, baseIds.size=%d\n"
		, aiData.production.policeDemand
		, policeRequest
		, aiData.baseIds.size()
	);

}

void selectProject()
{
	debug("selectProject - %s\n", getMFaction(aiFactionId)->noun_faction);

	// find project

	int cheapestProjectFacilityId = -1;
	int cheapestProjectFacilityCost = INT_MAX;

	for (int projectFacilityId = FAC_HUMAN_GENOME_PROJECT; projectFacilityId <= FAC_PLANETARY_ENERGY_GRID; projectFacilityId++)
	{
		// exclude not available project

		if (!isProjectAvailable(aiFactionId, projectFacilityId))
			continue;

		// get project cost

		int cost = getFacility(projectFacilityId)->cost;

		debug("\t%-30s cost=%3d\n", getFacility(projectFacilityId)->name, getFacility(projectFacilityId)->cost);

		// update best

		if (cost < cheapestProjectFacilityCost)
		{
			debug("\t\t- best\n");
			cheapestProjectFacilityId = projectFacilityId;
			cheapestProjectFacilityCost = cost;
		}

	}

	if (cheapestProjectFacilityId != -1)
	{
		selectedProject = cheapestProjectFacilityId;
	}

}

void assignProject()
{
	debug("assignProject - %s\n", getMFaction(aiFactionId)->noun_faction);

	if (selectedProject == -1)
	{
		debug("\tno selected project\n");
		return;
	}

	CFacility *projectFacility = getFacility(selectedProject);

	debug("\t%s\n", projectFacility->name);

	// check if project is building

	bool projectBuilding = false;
	int totalMineralSurplus = 0;
	int mostSuitableBaseId = -1;
	double mostSuitableBaseValue = 0.0;
	int mostSuitableBaseMineralSurplus = 0;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = getBase(baseId);
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);

		if (isBaseBuildingProject(baseId))
		{
			projectBuilding = true;
			base->queue_items[0] = -selectedProject;
			break;
		}

		// mineral surplus

		int mineralSurplus = base->mineral_surplus;
		totalMineralSurplus += mineralSurplus;

		// threat

		double threat = baseInfo.combatData.requiredEffect;

		// value

		double value = (double)mineralSurplus - threat;

		if (value > mostSuitableBaseValue)
		{
			mostSuitableBaseId = baseId;
			mostSuitableBaseValue = value;
			mostSuitableBaseMineralSurplus = mineralSurplus;
		}

	}

	if (projectBuilding)
	{
		debug("\tproject is building\n");
		return;
	}

	if (mostSuitableBaseId == -1)
	{
		debug("\tmost suitable base not found\n");
		return;
	}

	BASE *mostSuitableBase = getBase(mostSuitableBaseId);

	debug("\t%s\n", mostSuitableBase->name);

	// compute base production fraction

	double baseProductionFraction = (double)mostSuitableBaseMineralSurplus / (double)std::max(1, totalMineralSurplus);

	if (baseProductionFraction > conf.ai_production_project_mineral_surplus_fraction)
	{
		debug
		(
			"\tbaseProductionFraction=%5.2f > conf.ai_production_project_mineral_surplus_fraction=%5.2f\n"
			, baseProductionFraction
			, conf.ai_production_project_mineral_surplus_fraction
		);
		return;
	}

	// exclude base building psych facility

	switch (getBaseBuildingItem(mostSuitableBaseId))
	{
	case -FAC_RECREATION_COMMONS:
	case -FAC_HOLOGRAM_THEATRE:
	case -FAC_PARADISE_GARDEN:
	case -FAC_RESEARCH_HOSPITAL:
	case -FAC_NANOHOSPITAL:
		return;
	}

	// build project

	mostSuitableBase->queue_items[0] = - selectedProject;

	debug("\tproject set\n");

	// build mineral multiplying facility instead if more effective
	// This is a hack. I should better balance all priorities.

	int mineralMultiplyingFacilityId =
		getFirstAvailableFacility
		(
			mostSuitableBaseId,
			{FAC_RECYCLING_TANKS, FAC_GENEJACK_FACTORY, FAC_ROBOTIC_ASSEMBLY_PLANT, FAC_QUANTUM_CONVERTER, FAC_NANOREPLICATOR}
		)
	;

	// no available mineral multiplying facility

	if (mineralMultiplyingFacilityId == -1)
		return;

	// this multiplying facility is already building

	int currentItem = getBaseBuildingItem(mostSuitableBaseId);

	if (currentItem == -mineralMultiplyingFacilityId)
		return;

	int currentMineralMultiplierNumerator = divideIntegerRoundUp(2 * mostSuitableBase->mineral_intake_2, mostSuitableBase->mineral_intake);
	int improvedMineralMultiplierNumerator = currentMineralMultiplierNumerator + 1;
	int currentMineralIntake2 = mostSuitableBase->mineral_intake_2;
	int improvedMineralIntake2 = mostSuitableBase->mineral_intake * improvedMineralMultiplierNumerator / 2;
	int currentMineralProduction = mostSuitableBase->mineral_surplus;
	int improvedMineralProduction = currentMineralProduction + (improvedMineralIntake2 - currentMineralIntake2);
	int currentItemBuildTime = getBaseItemBuildTime(mostSuitableBaseId, currentItem);
	int improvedItemBuildTime = divideIntegerRoundUp(getBaseMineralCost(mostSuitableBaseId, currentItem), improvedMineralProduction);
	int itemBuildTimeReduction = std::max(0, currentItemBuildTime - improvedItemBuildTime);
	int mineralMultiplyingFacilityBuildTime = getBaseItemBuildTime(mostSuitableBaseId, -mineralMultiplyingFacilityId);

	// not enough time reduction

	if (itemBuildTimeReduction < divideIntegerRoundUp(mineralMultiplyingFacilityBuildTime, 2))
		return;

	// build mineral multiplying facility instead

	mostSuitableBase->queue_items[0] = -mineralMultiplyingFacilityId;

	debug("\tbuild mineral multiplying facility first\n");

	debug
	(
		"\t%-32s"
		" itemBuildTimeReduction=%2d"
		" mineralMultiplyingFacilityBuildTime=%2d"
		" currentMineralMultiplierNumerator=%2d"
		" improvedMineralMultiplierNumerator=%2d"
		" currentMineralIntake2=%2d"
		" improvedMineralIntake2=%2d"
		" currentMineralProduction=%2d"
		" improvedMineralProduction=%2d"
		" currentItemBuildTime=%2d"
		" improvedItemBuildTime=%2d"
		"\n"
		, getFacility(mineralMultiplyingFacilityId)->name
		, itemBuildTimeReduction
		, mineralMultiplyingFacilityBuildTime
		, currentMineralMultiplierNumerator
		, improvedMineralMultiplierNumerator
		, currentMineralIntake2
		, improvedMineralIntake2
		, currentMineralProduction
		, improvedMineralProduction
		, currentItemBuildTime
		, improvedItemBuildTime
	);

}

void hurryProtectiveUnit()
{
	debug("hurryProtectiveUnit - %s\n", getMFaction(aiFactionId)->noun_faction);

	int mostVulnerableBaseId = -1;
	double mostVulnerableBaseRelativeRemainingEffect = 0.50;

	for (int baseId : aiData.baseIds)
	{
		BaseInfo &baseInfo = aiData.getBaseInfo(baseId);

		debug("\t%-25s\n", getBase(baseId)->name);

		int item = getBaseBuildingItem(baseId);

		debug("\t\titem=%3d\n", item);

		if (item < 0 || !isInfantryDefensiveUnit(item))
		{
			debug("\t\tnot infantry devensive\n");
			continue;
		}

		if (baseInfo.combatData.requiredEffect <= 0.0)
			continue;

		double relativeRemainingEffect = (1.0 - baseInfo.combatData.providedEffect / baseInfo.combatData.requiredEffect);

		debug("\t\tremainingEffect=%3d\n", item);

		if (relativeRemainingEffect > mostVulnerableBaseRelativeRemainingEffect)
		{
			mostVulnerableBaseId = baseId;
			mostVulnerableBaseRelativeRemainingEffect = relativeRemainingEffect;
		}

	}

	if (mostVulnerableBaseId == -1)
		return;

	debug
	(
		"\tmostVulnerableBase=%-25s"
		" mostVulnerableBaseRelativeRemainingEffect=%5.2f"
		"\n"
		, getBase(mostVulnerableBaseId)->name
		, mostVulnerableBaseRelativeRemainingEffect
	);

	int item = getBaseBuildingItem(mostVulnerableBaseId);

	if (item < 0 || !isInfantryDefensiveUnit(item))
	{
		debug("\t\tnot infantry devensive\n");
		return;
	}

	// hurry production

	int spendPool = getFaction(aiFactionId)->energy_credits;

	hurryProductionPartially(mostVulnerableBaseId, spendPool);

	debug("\t%-25s spendPool=%4d\n", getBase(mostVulnerableBaseId)->name, spendPool);

}

/*
Evaluates production item gain in base giving gain.
*/
double getItemProductionPriority(int baseId, int item, double gain)
{
	// cost and priority

	int cost = getBaseItemCost(baseId, item);
	return getProductionPriority(gain, cost);

}

/*
Evaluates facility gain in base giving generated income and incomeGrowth.
Delay is time when bonus kicks in after facility is built.
*/
double getItemProductionPriority(int baseId, int item, int delay, double bonus, double income, double incomeGrowth)
{
	// build time

	int buildTime = getBaseItemBuildTime(baseId, item, true);

	// gain

	double gain = getGain(buildTime + delay, bonus, income, incomeGrowth);

	// cost

	int cost = getBaseItemCost(baseId, item);

	// priority

	return getProductionPriority(gain, cost);

}

double getItemProductionPriority(int baseId, int item, int delay, double income, double incomeGrowth)
{
	return getItemProductionPriority(baseId, item, delay, 0, income, incomeGrowth);
}

/*
Computes base modification gain by given parameters.
*/
double getComputedProductionPriority
(
	int baseId,
	int item,
	int delay,
	int currentWorkerCount,
	double currentIncome,
	double currentIncomeGrowth,
	double currentRelativeIncomeGrowth,
	double currentNutrientIntake,
	double workerRelativeIncome,
	double workerNutrientIntake,
	double itemIncome,
	int projectedSize,
	int oldWorkerCount,
	int oldEcoDamage,
	int newWorkerCount,
	int newEcoDamage
)
{
	bool TRACE = DEBUG && false;

	if (TRACE) { debug("getComputedProductionPriority"); }

	BASE *base = getBase(baseId);
	int factionId = base->faction_id;

	// estimate resource based income

	double oldResourceIncome = currentIncome * (1.0 + (oldWorkerCount - currentWorkerCount) * workerRelativeIncome);
	double newResourceIncome = itemIncome * (1.0 + (newWorkerCount - currentWorkerCount) * workerRelativeIncome);
	double resourceIncome = newResourceIncome - oldResourceIncome;
	double resourceIncomeGrowth = 0.75 * resourceIncome * currentRelativeIncomeGrowth;

	// estimate nutrient based income

	double oldNutrientIntake = currentNutrientIntake + (double)(oldWorkerCount - currentWorkerCount) * workerNutrientIntake;
	double newNutrientIntake = currentNutrientIntake + (double)(newWorkerCount - currentWorkerCount) * workerNutrientIntake;
	double oldNutrientSurplus = oldNutrientIntake - Rules->nutrient_intake_req_citizen * projectedSize;
	double newNutrientSurplus = newNutrientIntake - Rules->nutrient_intake_req_citizen * projectedSize;
	double maxNutrientSurplus = std::max(oldNutrientSurplus, newNutrientSurplus);

	double relativeNutrientSurplusIncrease = 0.0;
	double nutrientIncomeGrowth = 0.0;

	if (maxNutrientSurplus > 0.0)
	{
		relativeNutrientSurplusIncrease = (newNutrientSurplus - oldNutrientSurplus) / maxNutrientSurplus;
		nutrientIncomeGrowth = currentIncomeGrowth * relativeNutrientSurplusIncrease;
	}

	// estimate eco damage effect
	// assume eco damage is the chance of number of workers to lose half or their productivity

	double ecodamageIncome = 0.0;

	if (newEcoDamage != oldEcoDamage)
	{
		// fungal pop frequency increase

		int ecoDamageIncrease = newEcoDamage - oldEcoDamage;
		double fungalPopFrequencyIncrease = (double)ecoDamageIncrease / 100.0;

		// fungal pop damage

		double fungalPopTiles = (double)getFaction(factionId)->clean_minerals_modifier / 3.0;
		double incomeReduction = 0.5 * getCitizenIncome() * fungalPopTiles;

		// income effect

		ecodamageIncome = incomeReduction * fungalPopFrequencyIncrease;

	}

	// summarize

	double income = resourceIncome + ecodamageIncome;
	double incomeGrowth = resourceIncomeGrowth + nutrientIncomeGrowth;

	// compute priority

	double priority =
		getItemProductionPriority
		(
			baseId,
			item,
			delay,
			0.0,
			income,
			incomeGrowth
		)
	;

	if (TRACE)
	{
		debug
		(
			"\tpriority=%5.2f"
			" income=%5.2f"
			" incomeGrowth=%5.2f"
			" currentWorkerCount=%2d"
			" currentIncome=%5.2f"
			" currentIncomeGrowth=%5.2f"
			" currentRelativeIncomeGrowth=%5.4f"
			" workerRelativeIncome=%5.4f"
			" workerNutrientIntake=%5.4f"
			" itemIncome=%5.2f"
			" projectedSize=%2d"
			" oldWorkerCount=%2d"
			" newWorkerCount=%2d"
			" oldEcoDamage=%2d"
			" newEcoDamage=%2d"
			" oldResourceIncome=%5.2f"
			" newResourceIncome=%5.2f"
			" resourceIncome=%5.2f"
			" resourceIncomeGrowth=%5.4f"
			" oldNutrientSurplus=%5.4f"
			" newNutrientSurplus=%5.4f"
			" relativeNutrientSurplusIncrease=%5.4f"
			" nutrientIncomeGrowth=%5.4f"
			" ecodamageIncome=%5.2f"
			"\n"
			, priority
			, income
			, incomeGrowth
			, currentWorkerCount
			, currentIncome
			, currentIncomeGrowth
			, currentRelativeIncomeGrowth
			, workerRelativeIncome
			, workerNutrientIntake
			, itemIncome
			, projectedSize
			, oldWorkerCount
			, newWorkerCount
			, oldEcoDamage
			, newEcoDamage
			, oldResourceIncome
			, newResourceIncome
			, resourceIncome
			, resourceIncomeGrowth
			, oldNutrientSurplus
			, newNutrientSurplus
			, relativeNutrientSurplusIncrease
			, nutrientIncomeGrowth
			, ecodamageIncome
		);

	}

	// compute priority

	return
		getItemProductionPriority
		(
			baseId,
			item,
			delay,
			0.0,
			income,
			incomeGrowth
		)
	;

}

/*
Evaluates facility generated income by building this facility and comparing to previous base state.
*/
double getBaseFacilityIncome(int baseId, int facilityId)
{
	// cannot build

	if (!isBaseFacilityAvailable(baseId, facilityId))
	{
		return 0.0;
	}

	// get base snapshot

	BaseMetric facilityImprovement = getFacilityImprovement(baseId, facilityId);

	// compute income difference

	return getResourceScore(facilityImprovement.mineral, facilityImprovement.economy + facilityImprovement.labs);

}

/*
Computes average faction development score at given point in time.
*/
double getDevelopmentScore(int turn)
{
	double mineralIntake2 = getFactionStatisticalMineralIntake2(turn);
	double budgetIntake2 = getFactionStatisticalBudgetIntake2(turn);
	return getResourceScore(mineralIntake2, budgetIntake2);
}

/*
Computes average faction development score tangent.
The development score speed of changes at this point in time.
*/
double getDevelopmentScoreTangent(int turn)
{
	return getDevelopmentScore(turn + 1) - getDevelopmentScore(turn);
}

/*
Computes development scale at current point in time.
Approximate time when faction power growth 2 times.
*/
double getDevelopmentScale()
{
	int adjustedTurn = std::max(20, *CurrentTurn);
	return getDevelopmentScore(adjustedTurn) / getDevelopmentScoreTangent(adjustedTurn);
}

/*
Evaluates new base gain.
*/
double getNewBaseGain(int delay)
{
	assert(delay >= 1);

	int initialTurn = *CurrentTurn + delay;
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;

		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		int futureBaseAge = futureTurn - initialTurn;

		double futureBaseStatisticalMineralIntake2 = getBaseStatisticalMineralIntake2(futureBaseAge);
		double futureBaseMineralWeight = futureBaseStatisticalMineralIntake2 * futureMineralDecayCoefficient;

		double futureBaseStatisticalBudgetIntake2 = getBaseStatisticalBudgetIntake2(futureBaseAge);
		double futureBaseBudgetWeight = futureBaseStatisticalBudgetIntake2 * futureBudgetDecayCoefficient;

		gain += getResourceScore(futureBaseMineralWeight, futureBaseBudgetWeight);

	}

	return gain;

}

/*
Evaluates generic colony production priority.
This value is used as normalization for other production items.
*/
double getColonyRawProductionPriority()
{
	debug("getColonyRawProductionPriority - %s\n", getMFaction(aiFactionId)->noun_faction);

	int cost = getUnit(BSC_COLONY_POD)->cost;

	double averageNewBaseGain = getNewBaseGain(10);
	double colonyRawProductionPriority = getRawProductionPriority(averageNewBaseGain, cost);

	debug
	(
		"\tcolonyRawProductionPriority=%5.2f"
		" averageNewBaseGain=%5.2f"
		" cost=%2d"
		"\n"
		, colonyRawProductionPriority
		, averageNewBaseGain
		, cost
	);

	return colonyRawProductionPriority;

}

double getGain(int delay, double bonus, double income, double incomeGrowth)
{
	return
		(
			+ bonus
			+ aiData.developmentScale * income
			+ aiData.developmentScale * aiData.developmentScale * incomeGrowth
		)
		* getExponentialCoefficient(aiData.developmentScale, delay)
	;

}

double getIncomeGain(int delay, double income, double incomeGrowth)
{
	return getGain(delay, 0, income, incomeGrowth);
}

/*
Evaluates gain production priority based on cost.
*/
double getRawProductionPriority(double gain, double cost)
{
	assert(cost > 0.0);
	return gain / cost;
}

/*
Normalizes production priority by normal raw production priority.
*/
double getProductionPriority(double gain, double cost)
{
	return getRawProductionPriority(gain, cost) / normalRawProductionPriority;
}

/*
Computes base production constant income gain.
*/
double getProductionConstantIncomeGain(double score, double buildTime, double bonusDelay)
{
	return
		score
		* aiData.developmentScale
		* getBuildTimeEffectCoefficient(buildTime) * getBonusDelayEffectCoefficient(bonusDelay)
	;
}

/*
Computes base production linear income gain.
*/
double getProductionLinearIncomeGain(double score, double buildTime, double bonusDelay)
{
	return
		score
		* aiData.developmentScale * aiData.developmentScale
		* getBuildTimeEffectCoefficient(buildTime) * getBonusDelayEffectCoefficient(bonusDelay)
	;
}

/*
Computes base production proportional income gain.
Assuming the one time income turns into proportional growth.
*/
double getProductionProportionalIncomeGain(double score, int buildTime, int bonusDelay)
{
	return
		(
			+ score * aiData.developmentScale
			+ score / aiData.baseGrowthScale * aiData.developmentScale * aiData.developmentScale
		)
		* getBuildTimeEffectCoefficient(buildTime) * getBonusDelayEffectCoefficient(bonusDelay)
	;
}

/*
Estimates 1 psych generated income growth.
Assuming 3 psych quell a drone.
*/
double getPsychIncomeGrowth(int baseId, double psychGrowth)
{
	double dronesGrowth = getBaseSizeGrowth(baseId);
	double psychQuelledDronesGrowth = psychGrowth / 3.0;
	double quelledDronesGrowth = std::min(dronesGrowth, psychQuelledDronesGrowth);
	return getCitizenIncome() * quelledDronesGrowth;
}

/*
Estimates morale/lifecycle effect.
Returns proportional mineral bonus for average faction base.
*/
double getMoraleProportionalMineralBonus(std::vector<int> levels)
{
	// summarize costs and supports

	int totalUnitWeightedCount = 0;
	int totalUnitWeightedCost = 0;

	for (int vehicleId : aiData.combatVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		int triad = vehicle->triad();
		int unitCost = getVehicleUnitCost(vehicleId);
		bool native = isNativeVehicle(vehicleId);

		// type

		int type = (native ? 3 : triad);

		// accumulate weighted values

		totalUnitWeightedCount += 1 * levels[type];
		totalUnitWeightedCost += unitCost * levels[type];

	}

	// reduction in build cost and support
	// assuming every 20-th combat unit is dying every turn

	double mineralReduction = 0.125 * (totalUnitWeightedCount + 0.05 * totalUnitWeightedCost);

	// proportional bonus on average base production

	return (aiData.totalMineralIntake2 <= 0 ? 0.0 : std::min(1.0, mineralReduction / (double)aiData.totalMineralIntake2));

}

BaseMetric getBaseMetric(int baseId)
{
	BASE *base = getBase(baseId);

	BaseMetric baseMetric;

	baseMetric.nutrient = base->nutrient_surplus;
	baseMetric.mineral = base->mineral_surplus;
	baseMetric.economy = base->economy_total;
	baseMetric.psych = base->economy_total;
	baseMetric.labs = base->labs_total;

	return baseMetric;

}

/*
Evaluates facility gain by building this facility and checking base constant and growth improvement.
If adjustPopulation is set it tries to evaluate base size by the time item is built.
*/
BaseMetric getFacilityImprovement(int baseId, int facilityId)
{
	BaseMetric facilityImprovement;

	// can build

	if (!isBaseFacilityAvailable(baseId, facilityId))
		return facilityImprovement;

	// set facility

	setBaseFacility(baseId, facilityId, true);
	BaseMetric newMetric = getBaseMetric(baseId);

	// remove facility

	setBaseFacility(baseId, facilityId, false);
	BaseMetric oldMetric = getBaseMetric(baseId);

	facilityImprovement.nutrient = newMetric.nutrient - oldMetric.nutrient;
	facilityImprovement.mineral = newMetric.mineral - oldMetric.mineral;
	facilityImprovement.economy = newMetric.economy - oldMetric.economy;
	facilityImprovement.psych = newMetric.psych - oldMetric.psych;
	facilityImprovement.labs = newMetric.labs - oldMetric.labs;

	return facilityImprovement;

}

/*
Evaluates facility extra worker count.
The calculation occured in the future after facility is built.
*/
int getFacilityWorkerCountEffect(int baseId, int facilityId)
{
	BASE *base = getBase(baseId);

	// can build

	if (!isBaseFacilityAvailable(baseId, facilityId))
		return 0;

	int buildTime = getBaseItemBuildTime(baseId, -facilityId);
	int projectedPopSize = getBaseProjectedSize(baseId, buildTime);

	// store current parameters

	int currentSize = base->pop_size;

	// set pop size

	base->pop_size = projectedPopSize;

	// set facility

	setBaseFacility(baseId, facilityId, true);
	computeBaseDoctors(baseId);
	int newWorkerCount = base->pop_size - base->specialist_total;

	// remove faciltiy

	setBaseFacility(baseId, facilityId, false);
	computeBaseDoctors(baseId);
	int oldWorkerCount = base->pop_size - base->specialist_total;

	// restore base

	base->pop_size = currentSize;
	computeBaseDoctors(baseId);

	// psych effect

	return newWorkerCount - oldWorkerCount;

}

double getDemand(double required, double provided)
{
	double demand;

	if (required <= 0.0)
	{
		demand = 0.0;
	}
	else if (provided >= required)
	{
		demand = 0.0;
	}
	else
	{
		demand = 1.0 - provided / required;
	}

	return demand;

}

//=======================================================
// statistical estimates
//=======================================================

/*
Returns faction statistical mineral intake2.
*/
double getFactionStatisticalMineralIntake2(double turn)
{
	double value;

	if (turn < conf.ai_faction_minerals_t1)
	{
		value =
			+ conf.ai_faction_minerals_a1 * turn * turn * turn
			+ conf.ai_faction_minerals_b1 * turn * turn
			+ conf.ai_faction_minerals_c1 * turn
			+ conf.ai_faction_minerals_d1
		;
	}
	else
	{
		value =
			+ conf.ai_faction_minerals_a2 * turn
			+ conf.ai_faction_minerals_b2
		;
	}

	return std::max(1.0, value);

}

/*
Returns faction statistical budget intake2.
*/
double getFactionStatisticalBudgetIntake2(double turn)
{
	double value;

	if (turn < conf.ai_faction_budget_t1)
	{
		value =
			+ conf.ai_faction_budget_a1 * turn * turn * turn
			+ conf.ai_faction_budget_b1 * turn * turn
			+ conf.ai_faction_budget_c1 * turn
			+ conf.ai_faction_budget_d1
		;
	}
	else
	{
		value =
			+ conf.ai_faction_budget_a2 * turn
			+ conf.ai_faction_budget_b2
		;
	}

	return std::max(1.0, value);

}

/*
Returns base statistical size with age.
*/
double getBaseStatisticalSize(double age)
{
	double value =
		+ conf.ai_base_size_a * log(age - conf.ai_base_size_b)
		+ conf.ai_base_size_c
	;

	return std::max(1.0, value);

}

/*
Returns base statistical mineral intake.
*/
double getBaseStatisticalMineralIntake(double age)
{
	double value =
		+ conf.ai_base_mineral_intake_a * log(age - conf.ai_base_mineral_intake_b)
		+ conf.ai_base_mineral_intake_c
	;

	return std::max(1.0, value);

}

/*
Returns base statistical mineral intake2.
*/
double getBaseStatisticalMineralIntake2(double age)
{
	double value =
		+ conf.ai_base_mineral_intake2_a * age
		+ conf.ai_base_mineral_intake2_b
	;

	return std::max(1.0, value);

}

/*
Returns base statistical mineral multiplier.
*/
double getBaseStatisticalMineralMultiplier(double age)
{
	double value =
		+ conf.ai_base_mineral_multiplier_a * age * age
		+ conf.ai_base_mineral_multiplier_b * age
		+ conf.ai_base_mineral_multiplier_c
	;

	return std::max(1.0, value);

}

/*
Reages base statistical budget intake.
*/
double getBaseStatisticalBudgetIntake(double age)
{
	double value =
		+ conf.ai_base_budget_intake_a * age
		+ conf.ai_base_budget_intake_b
	;

	return std::max(1.0, value);

}

/*
Reages base statistical budget intake2.
*/
double getBaseStatisticalBudgetIntake2(double age)
{
	double value =
		+ conf.ai_base_budget_intake2_a * age * age
		+ conf.ai_base_budget_intake2_b * age
		+ conf.ai_base_budget_intake2_c
	;

	return std::max(1.0, value);

}

/*
Reages base statistical budget multiplier.
*/
double getBaseStatisticalBudgetMultiplier(double age)
{
	double value =
		+ conf.ai_base_budget_multiplier_a * age * age
		+ conf.ai_base_budget_multiplier_b * age
		+ conf.ai_base_budget_multiplier_c
	;

	return std::max(1.0, value);

}

/*
Estimates flat mineral intake gain.
*/
double getFlatMineralIntakeGain(int delay, double value)
{
	assert(delay >= 1);

	int initialTurn = *CurrentTurn + delay;
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;

		double futureMineralWeight = value * futureMineralDecayCoefficient;

		gain += getResourceScore(futureMineralWeight, 0);

	}

	return gain;

}

/*
Estimates flat budget intake gain.
*/
double getFlatBudgetIntakeGain(int delay, double value)
{
	assert(delay >= 1);

	int initialTurn = *CurrentTurn + delay;
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBudgetWeight = value * futureBudgetDecayCoefficient;

		gain += getResourceScore(futureBudgetWeight, 0);

	}

	return gain;

}

/*
Estimates base extra population gain.
Contributing population: (minSize, maxSize].
*/
double getBasePopulationGain(int baseId, const Interval &baseSizeInterval)
{
	bool TRACE = DEBUG & false;

	if (TRACE) { debug("getBasePopulationGain\n"); }

	BASE *base = getBase(baseId);

	int initialTurn = *CurrentTurn + 1;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);

	// base proportional coefficients

	int baseSize = base->pop_size;
	double baseStatisticalSize = getBaseStatisticalSize(baseAge);
	double baseSizeProportionalCoefficient = (double)baseSize / baseStatisticalSize;

	int baseMineralIntake2 = base->mineral_intake_2;
	double baseStatisticalMineralIntake2 = getBaseStatisticalMineralIntake2(baseAge);
	double baseMineralProportionalCoefficient = (double)baseMineralIntake2 / baseStatisticalMineralIntake2;

	int baseBudgetIntake2 = base->economy_total + base->psych_total + base->labs_total;
	double baseStatisticalBudgetIntake2 = getBaseStatisticalBudgetIntake2(baseAge);
	double baseBudgetProportionalCoefficient = (double)baseBudgetIntake2 / baseStatisticalBudgetIntake2;

	if (TRACE)
	{
		debug
		(
			"\tbaseId=%3d"
			" baseSizeInterval.min=%2d"
			" baseSizeInterval.max=%2d"
			" *CurrentTurn=%3d"
			" initialTurn=%3d"
			" baseAge=%3d"
			" currentFactionStatisticalMineralIntake2=%7.2f"
			" currentFactionStatisticalBudgetIntake2=%7.2f"
			" baseSize=%2d"
			" baseStatisticalSize=%5.2f"
			" baseSizeProportionalCoefficient=%5.2f"
			" baseMineralIntake2=%2d"
			" baseStatisticalMineralIntake2=%5.2f"
			" baseMineralProportionalCoefficient=%5.2f"
			" baseBudgetIntake2=%2d"
			" baseStatisticalBudgetIntake2=%5.2f"
			" baseBudgetProportionalCoefficient=%5.2f"
			"\n"
			, baseId
			, baseSizeInterval.min
			, baseSizeInterval.max
			, *CurrentTurn
			, initialTurn
			, baseAge
			, currentFactionStatisticalMineralIntake2
			, currentFactionStatisticalBudgetIntake2
			, baseSize
			, baseStatisticalSize
			, baseSizeProportionalCoefficient
			, baseMineralIntake2
			, baseStatisticalMineralIntake2
			, baseMineralProportionalCoefficient
			, baseBudgetIntake2
			, baseStatisticalBudgetIntake2
			, baseBudgetProportionalCoefficient
		);
	}

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);

		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBaseSize = getBaseStatisticalSize(futureBaseAge) * baseSizeProportionalCoefficient;

		if (futureBaseSize <= baseSizeInterval.min)
			continue;

		double populationProportion = (std::min(futureBaseSize, (double)baseSizeInterval.max) - (double)baseSizeInterval.min) / futureBaseSize;

		double futureBaseMineralIntake2 = getBaseStatisticalMineralIntake2(futureBaseAge) * baseMineralProportionalCoefficient;
		double futureBaseBudgetIntake2 = getBaseStatisticalBudgetIntake2(futureBaseAge) * baseBudgetProportionalCoefficient;

		double futureBaseMineralIntake2ExtraPopulation = futureBaseMineralIntake2 * populationProportion;
		double futureBaseBudgetIntake2ExtraPopulation = futureBaseBudgetIntake2 * populationProportion;

		double futureBaseMineralWeight = futureBaseMineralIntake2ExtraPopulation * futureMineralDecayCoefficient;
		double futureBaseBudgetWeight = futureBaseBudgetIntake2ExtraPopulation * futureBudgetDecayCoefficient;

		double turnGain = getResourceScore(futureBaseMineralWeight, futureBaseBudgetWeight);
		gain += turnGain;

		if (TRACE)
		{
			debug
			(
				"\t\tfutureTurn=%3d"
				" futureBaseAge=%3d"
				" futureFactionStatisticalMineralIntake2=%7.2f"
				" futureFactionStatisticalBudgetIntake2=%7.2f"
				" futureMineralDecayCoefficient=%5.2f"
				" futureBudgetDecayCoefficient=%5.2f"
				" futureBaseSize=%5.2f"
				" populationProportion=%5.2f"
				" futureBaseMineralIntake2=%7.2f"
				" futureBaseBudgetIntake2=%7.2f"
				" futureBaseMineralIntake2ExtraPopulation=%7.2f"
				" futureBaseBudgetIntake2ExtraPopulation=%7.2f"
				" futureBaseMineralWeight=%7.2f"
				" futureBaseBudgetWeight=%7.2f"
				" turnGain=%7.2f"
				" gain=%7.2f"
				"\n"
				, futureTurn
				, futureBaseAge
				, futureFactionStatisticalMineralIntake2
				, futureFactionStatisticalBudgetIntake2
				, futureMineralDecayCoefficient
				, futureBudgetDecayCoefficient
				, futureBaseSize
				, populationProportion
				, futureBaseMineralIntake2
				, futureBaseBudgetIntake2
				, futureBaseMineralIntake2ExtraPopulation
				, futureBaseBudgetIntake2ExtraPopulation
				, futureBaseMineralWeight
				, futureBaseBudgetWeight
				, turnGain
				, gain
			);
		}

	}

	if (TRACE)
	{
		debug
		(
			"\tgain=%7.2f"
			"\n"
			, gain
		);
	}

	return gain;

}

/*
Estimates base extra population gain.
Contributing population: (minSize, maxSize].
*/
double getBasePopulationGrowthGain(int baseId, int delay, double proportionalGrowthIncrease)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	BASE *base = getBase(baseId);
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);
	int initialTurn = *CurrentTurn + delay;

	// base proportional coefficients

	int baseMineralIntake2 = base->mineral_intake_2;
	double baseStatisticalMineralIntake2 = getBaseStatisticalMineralIntake2(baseId);
	double baseMineralProportionalCoefficient = (double)baseMineralIntake2 / baseStatisticalMineralIntake2;

	int baseBudgetIntake2 = base->economy_total + base->psych_total + base->labs_total;
	double baseStatisticalBudgetIntake2 = getBaseStatisticalBudgetIntake2(baseId);
	double baseBudgetProportionalCoefficient = (double)baseBudgetIntake2 / baseStatisticalBudgetIntake2;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		double futureBaseAge1 = (double)(baseAge + delay) + (double)(futureTurn - initialTurn);
		double futureBaseAge2 = (double)(baseAge + delay) + (double)(futureTurn - initialTurn) * proportionalGrowthIncrease;

		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);

		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBaseMineralIntake2Difference =
			+ getBaseStatisticalMineralIntake2(futureBaseAge2) * baseMineralProportionalCoefficient
			- getBaseStatisticalMineralIntake2(futureBaseAge1) * baseMineralProportionalCoefficient
		;
		double futureBaseBudgetIntake2Difference =
			+ getBaseStatisticalBudgetIntake2(futureBaseAge2) * baseBudgetProportionalCoefficient
			- getBaseStatisticalBudgetIntake2(futureBaseAge1) * baseBudgetProportionalCoefficient
		;

		double futureBaseMineralIntake2ExtraPopulation = futureBaseMineralIntake2Difference;
		double futureBaseBudgetIntake2ExtraPopulation = futureBaseBudgetIntake2Difference;

		double futureBaseMineralWeight = futureBaseMineralIntake2ExtraPopulation * futureMineralDecayCoefficient;
		double futureBaseBudgetWeight = futureBaseBudgetIntake2ExtraPopulation * futureBudgetDecayCoefficient;

		gain += getResourceScore(futureBaseMineralWeight, futureBaseBudgetWeight);

	}

	return gain;

}

/*
Estimates proportional mineral intake gain.
*/
double getBaseProportionalMineralIntakeGain(int baseId, int delay, double proportion)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	BASE *base = getBase(baseId);

	int initialTurn = *CurrentTurn + delay;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);

	// base proportional coefficients

	int baseMineralIntake = base->mineral_intake;
	double baseStatisticalMineralIntake = getBaseStatisticalMineralIntake(baseId);
	double baseMineralProportionalCoefficient = (double)baseMineralIntake / baseStatisticalMineralIntake;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;

		double futureBaseMineralIntake = getBaseStatisticalMineralIntake(futureBaseAge) * baseMineralProportionalCoefficient;
		double futureBaseMineralIntakeProportion = futureBaseMineralIntake * proportion;
		double futureBaseMineralWeight = futureBaseMineralIntakeProportion * futureMineralDecayCoefficient;

		gain += getResourceScore(futureBaseMineralWeight, 0);

	}

	return gain;

}

/*
Estimates proportional budget intake gain.
*/
double getBaseProportionalBudgetIntakeGain(int baseId, int delay, double proportion)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	int initialTurn = *CurrentTurn + delay;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);

	// base proportional coefficients

	Budget baseBudgetIntakeByType = getBaseBudgetIntake(baseId);
	int baseBudgetIntake = baseBudgetIntakeByType.economy + baseBudgetIntakeByType.psych + baseBudgetIntakeByType.labs;
	double baseStatisticalBudgetIntake = getBaseStatisticalBudgetIntake(baseId);
	double baseBudgetProportionalCoefficient = (double)baseBudgetIntake / baseStatisticalBudgetIntake;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBaseBudgetIntake = getBaseStatisticalBudgetIntake(futureBaseAge) * baseBudgetProportionalCoefficient;
		double futureBaseBudgetIntakeProportion = futureBaseBudgetIntake * proportion;
		double futureBaseBudgetWeight = futureBaseBudgetIntakeProportion * futureBudgetDecayCoefficient;

		gain += getResourceScore(0, futureBaseBudgetWeight);

	}

	return gain;

}

/*
Estimates proportional mineral intake gain.
*/
double getBaseProportionalMineralIntake2Gain(int baseId, int delay, double proportion)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	BASE *base = getBase(baseId);

	int initialTurn = *CurrentTurn + delay;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(*CurrentTurn);

	// base proportional coefficients

	int baseMineralIntake2 = base->mineral_intake_2;
	double baseStatisticalMineralIntake2 = getBaseStatisticalMineralIntake2(baseId);
	double baseMineralProportionalCoefficient = (double)baseMineralIntake2 / baseStatisticalMineralIntake2;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalMineralIntake2 = getFactionStatisticalMineralIntake2(futureTurn);
		double futureMineralDecayCoefficient = currentFactionStatisticalMineralIntake2 / futureFactionStatisticalMineralIntake2;

		double futureBaseMineralIntake2 = getBaseStatisticalMineralIntake2(futureBaseAge) * baseMineralProportionalCoefficient;
		double futureBaseMineralIntake2Proportion = futureBaseMineralIntake2 * proportion;
		double futureBaseMineralWeight = futureBaseMineralIntake2Proportion * futureMineralDecayCoefficient;

		gain += getResourceScore(futureBaseMineralWeight, 0);

	}

	return gain;

}

/*
Estimates proportional budget intake gain.
*/
double getBaseProportionalBudgetIntake2Gain(int baseId, int delay, double proportion)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	assert(delay >= 1);

	BASE *base = getBase(baseId);

	int initialTurn = *CurrentTurn + delay;
	int baseAge = getBaseAge(baseId);
	double currentFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(*CurrentTurn);

	// base proportional coefficients

	int baseBudgetIntake2 = base->economy_total + base->psych_total + base->labs_total;
	double baseStatisticalBudgetIntake2 = getBaseStatisticalBudgetIntake2(baseId);
	double baseBudgetProportionalCoefficient = (double)baseBudgetIntake2 / baseStatisticalBudgetIntake2;

	// summarize gain

	double gain = 0.0;

	for (int futureTurn = initialTurn; futureTurn <= LAST_TURN; futureTurn++)
	{
		int futureBaseAge = baseAge + (futureTurn - initialTurn);

		double futureFactionStatisticalBudgetIntake2 = getFactionStatisticalBudgetIntake2(futureTurn);
		double futureBudgetDecayCoefficient = currentFactionStatisticalBudgetIntake2 / futureFactionStatisticalBudgetIntake2;

		double futureBaseBudgetIntake2 = getBaseStatisticalBudgetIntake2(futureBaseAge) * baseBudgetProportionalCoefficient;
		double futureBaseBudgetIntake2Proportion = futureBaseBudgetIntake2 * proportion;
		double futureBaseBudgetWeight = futureBaseBudgetIntake2Proportion * futureBudgetDecayCoefficient;

		gain += getResourceScore(0, futureBaseBudgetWeight);

	}

	return gain;

}

//=======================================================
// production item helper functions
//=======================================================

/*
Determines affected base size interfals for given psych facility.
*/
Interval getPoliceBaseSizeInterval(int baseId, int unitId)
{
	assert(baseId >= 0 && baseId < *BaseCount);

	BASE *base = getBase(baseId);
	int currentPopSize = base->pop_size;

	Interval baseSizeInterval(0, 0);

	bool minSizeSet = false;
	bool maxSizeSet = false;
	for (int popSize = currentPopSize; popSize < 30; popSize++)
	{
		// set size

		base->pop_size = popSize;

		// workers without police

		computeBaseDoctors(baseId);

		int oldWorkerCount = base->pop_size - base->specialist_total;

		// workers with police

		int policeVehicleId = veh_init(unitId, base->faction_id, base->x, base->y);

		if (policeVehicleId == -1)
			break;

		computeBaseDoctors(baseId);

		int newWorkerCount = base->pop_size - base->specialist_total;

		// restore base

		veh_kill(policeVehicleId);
		base->pop_size = currentPopSize;
		computeBaseDoctors(baseId);

		// set minSize

		if (!minSizeSet)
		{
			if (newWorkerCount <= oldWorkerCount)
			{
				baseSizeInterval.min = popSize;
			}
			else
			{
				minSizeSet = true;
			}

		}

		// set maxSize

		if (!maxSizeSet)
		{
			if ((newWorkerCount - oldWorkerCount) >= (popSize - baseSizeInterval.min))
			{
				baseSizeInterval.max = popSize;
			}
			else
			{
				maxSizeSet = true;
				break;
			}

		}

	}

	// restore base

	base->pop_size = currentPopSize;
	computeBaseDoctors(baseId);

	return baseSizeInterval;

}

/*
Determines affected base size interfals for given psych facility.
*/
Interval getPsychFacilityBaseSizeInterval(int baseId, int facilityId)
{
	assert(baseId >= 0 && baseId < *BaseCount);

	BASE *base = getBase(baseId);
	int currentPopSize = base->pop_size;

	Interval baseSizeInterval(0, 0);

	// facility should not be present

	if (isBaseHasFacility(baseId, facilityId))
		return baseSizeInterval;

	bool minSizeSet = false;
	bool maxSizeSet = false;
	for (int popSize = currentPopSize; popSize < 30; popSize++)
	{
		// set size

		base->pop_size = popSize;

		// workers without facility

		computeBaseDoctors(baseId);
		int oldWorkerCount = base->pop_size - base->specialist_total;

		// workers with facility

		setBaseFacility(baseId, facilityId, true);
		computeBaseDoctors(baseId);
		int newWorkerCount = base->pop_size - base->specialist_total;

		// remove facility

		setBaseFacility(baseId, facilityId, false);

		// set minSize

		if (!minSizeSet)
		{
			if (newWorkerCount <= oldWorkerCount)
			{
				baseSizeInterval.min = popSize;
			}
			else
			{
				minSizeSet = true;
			}

		}

		// set maxSize

		if (!maxSizeSet)
		{
			if ((newWorkerCount - oldWorkerCount) >= (popSize - baseSizeInterval.min))
			{
				baseSizeInterval.max = popSize;
			}
			else
			{
				maxSizeSet = true;
				break;
			}

		}

	}

	// restore base

	base->pop_size = currentPopSize;
	computeBaseDoctors(baseId);

	return baseSizeInterval;

}

int getFacilityEcoDamageReduction(int baseId, int facilityId)
{
	BASE *base = getBase(baseId);

	// should not be built

	if (isBaseHasFacility(baseId, facilityId))
		return 0;

	// without facility

	int oldEcoDamage = base->eco_damage;

	// with facility

	setBaseFacility(baseId, facilityId, true);
	computeBaseDoctors(baseId);

	int newEcoDamage = base->eco_damage;

	// restore

	setBaseFacility(baseId, facilityId, false);
	computeBaseDoctors(baseId);

	// EcoDamage reduction

	return oldEcoDamage - newEcoDamage;

}

double getFacilityRelativeGrowthIncrease(int baseId, int facilityId)
{
	BASE *base = getBase(baseId);

	// should not be built

	if (isBaseHasFacility(baseId, facilityId))
		return 0.0;

	// old nutrient surplus

	int oldNutrientSurplus = base->nutrient_surplus;

	// new nutrient surplus

	setBaseFacility(baseId, facilityId, true);
	computeBaseDoctors(baseId);
	int newNutrientSurplus = base->nutrient_surplus;

	// restore

	setBaseFacility(baseId, facilityId, false);
	computeBaseDoctors(baseId);

	// return relative growth increase

	return (double)(newNutrientSurplus - oldNutrientSurplus) / (double)std::max(1, std::max(oldNutrientSurplus, newNutrientSurplus));

}

double getFacilityRelativeMineralIncrease(int baseId, int facilityId)
{
	BASE *base = getBase(baseId);

	// should not be built

	if (isBaseHasFacility(baseId, facilityId))
		return 0.0;

	// old mineral

	int oldMineral = base->economy_total + base->psych_total + base->labs_total;

	// new mineral

	setBaseFacility(baseId, facilityId, true);
	computeBaseDoctors(baseId);
	int newMineral = base->economy_total + base->psych_total + base->labs_total;

	// restore

	setBaseFacility(baseId, facilityId, false);
	computeBaseDoctors(baseId);

	// return relative mineral increase

	return (double)(newMineral - oldMineral) / (double)std::max(1, std::max(oldMineral, newMineral));

}

double getFacilityRelativeBudgetIncrease(int baseId, int facilityId)
{
	BASE *base = getBase(baseId);

	// should not be built

	if (isBaseHasFacility(baseId, facilityId))
		return 0.0;

	// old budget

	int oldBudget = base->economy_total + base->psych_total + base->labs_total;

	// new budget

	setBaseFacility(baseId, facilityId, true);
	computeBaseDoctors(baseId);
	int newBudget = base->economy_total + base->psych_total + base->labs_total;

	// restore

	setBaseFacility(baseId, facilityId, false);
	computeBaseDoctors(baseId);

	// return relative budget increase

	return (double)(newBudget - oldBudget) / (double)std::max(1, std::max(oldBudget, newBudget));

}

/*
Calculates number of extra police units can be added to the base.
*/
int getBasePoliceExtraCapacity(int baseId)
{
	// get base police allowed

	int policeAllowed = getBasePoliceAllowed(baseId);

	// no police allowed

	if (policeAllowed == 0)
		return 0;

	// count number of police units in base

	int policePresent = 0;

	for (int vehicleId : aiData.baseInfos[baseId].garrison)
	{
		// count only police2x if available

		if (aiData.police2xUnitAvailable && !isInfantryDefensivePolice2xVehicle(vehicleId))
			continue;

		// count police

		policePresent++;

	}

	return std::max(0, policeAllowed - policePresent);

}

