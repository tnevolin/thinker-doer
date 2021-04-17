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
	double nativeDefenseProductionDemand;
	int unpopulatedTileCount;
	int unpopulatedTileRangeSum;
	double averageUnpopulatedTileRange;
};

struct ActiveFactionInfo
{
	FACTION_INFO factionInfos[8];
	std::vector<int> baseIds;
	std::unordered_map<MAP *, int> baseLocations;
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
double estimateVehicleBaseLandNativeProtection(int factionId, int vehicleId);
Location getNearestPodLocation(int vehicleId);

