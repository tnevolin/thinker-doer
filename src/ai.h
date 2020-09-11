#ifndef __AI_H__
#define __AI_H__

#include <float.h>
#include <map>
#include "main.h"
#include "game.h"
#include "move.h"
#include "wtp.h"
#include "aiTerraforming.h"
#include "aiCombat.h"

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
};

extern int wtpAIFactionId;
extern int aiFactionId;

extern FACTION_INFO factionInfos[8];
extern std::vector<int> baseIds;
extern std::unordered_map<MAP *, int> baseLocations;
extern std::map<int, std::vector<int>> regionBases;
extern std::map<int, BASE_STRATEGY> baseStrategies;
extern std::vector<int> combatVehicleIds;
extern std::vector<int> outsideCombatVehicleIds;
extern std::vector<int> prototypes;

void aiStrategy(int id);
void populateSharedLists();
VEH *getVehicleByAIId(int aiId);
bool isReacheable(int id, int x, int y);
double estimateUnitBaseLandNativeProtection(int factionId, int unitId);
double estimateVehicleBaseLandNativeProtection(int factionId, int vehicleId);
double getFactionOffenseMultiplier(int id);
double getFactionDefenseMultiplier(int id);
double getFactionFanaticBonusMultiplier(int id);

#endif // __AI_H__

