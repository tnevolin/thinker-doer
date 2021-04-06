#pragma once

#include "main.h"
#include <float.h>
#include <map>
#include "game.h"
#include "move.h"
#include "wtp.h"
#include "aiTerraforming.h"
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

extern int wtpAIFactionId;
extern int aiFactionId;

extern FACTION_INFO factionInfos[8];
extern std::vector<int> baseIds;
extern std::unordered_map<MAP *, int> baseLocations;
extern std::map<int, std::vector<int>> regionBaseGroups;
extern std::map<int, BASE_STRATEGY> baseStrategies;
extern std::vector<int> combatVehicleIds;
extern std::vector<int> outsideCombatVehicleIds;
extern std::vector<int> prototypes;
extern std::vector<int> colonyVehicleIds;
extern std::vector<int> formerVehicleIds;

int aiEnemyMove(const int vehicleId);
int aiFactionUpkeep(const int factionId);
void aiStrategy(int id);
void populateSharedLists();
VEH *getVehicleByAIId(int aiId);
double estimateVehicleBaseLandNativeProtection(int factionId, int vehicleId);

