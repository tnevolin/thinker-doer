#pragma once

#include "main.h"
#include <float.h>
#include <map>
#include "game.h"
#include "move.h"
#include "wtp.h"
#include "aiFormer.h"
#include "aiCombat.h"
#include "aiColony.h"
#include "aiTransport.h"

struct FACTION_INFO
{
	double offenseMultiplier;
	double defenseMultiplier;
	double fanaticBonusMultiplier;
	double threatKoefficient;
};

struct BASE_STRATEGY
{
	BASE *base;
	std::vector<int> garrison;
	double nativeProtection;
	double nativeThreat;
	double nativeDefenseDemand;
	int unpopulatedTileCount;
	int unpopulatedTileRangeSum;
	double averageUnpopulatedTileRange;
	double conventionalDefenseDemand;
};

struct ESTIMATED_VALUE
{
	double demanding;
	double remaining;
};

struct ActiveFactionInfo
{
	FACTION_INFO factionInfos[8];
	std::vector<int> baseIds;
	std::unordered_map<MAP *, int> baseLocations;
	std::unordered_set<int> presenceRegions;
	std::map<int, std::vector<int>> regionBaseGroups;
	std::map<int, BASE_STRATEGY> baseStrategies;
	std::vector<int> combatVehicleIds;
	std::vector<int> outsideCombatVehicleIds;
	std::vector<int> prototypes;
	std::vector<int> colonyVehicleIds;
	std::vector<int> formerVehicleIds;
	double threatLevel;
	std::unordered_map<int, std::vector<int>> regionSurfaceCombatVehicleIds;
	std::unordered_map<int, std::vector<int>> regionSurfaceScoutVehicleIds;
	std::unordered_map<int, double> baseAnticipatedNativeAttackStrengths;
	std::unordered_map<int, double> baseRemainingNativeProtectionDemands;
	std::unordered_map<int, double> regionConventionalDefenseDemand;
	
	void clear()
	{
		baseIds.clear();
		baseLocations.clear();
		presenceRegions.clear();
		regionBaseGroups.clear();
		baseStrategies.clear();
		combatVehicleIds.clear();
		outsideCombatVehicleIds.clear();
		prototypes.clear();
		colonyVehicleIds.clear();
		formerVehicleIds.clear();
		threatLevel = 0.0;
		regionSurfaceCombatVehicleIds.clear();
		regionSurfaceScoutVehicleIds.clear();
		baseAnticipatedNativeAttackStrengths.clear();
		baseRemainingNativeProtectionDemands.clear();
		regionConventionalDefenseDemand.clear();
	}
	
};

extern int wtpAIFactionId;

extern int currentTurn;
extern int aiFactionId;
extern ActiveFactionInfo activeFactionInfo;

int aiFactionUpkeep(const int factionId);
int aiEnemyMove(const int vehicleId);
void aiStrategy();
void populateGlobalVariables();
VEH *getVehicleByAIId(int aiId);
Location getNearestPodLocation(int vehicleId);
void evaluateBaseNativeDefenseDemands();
void evaluateConventionalDefenseDemands();

