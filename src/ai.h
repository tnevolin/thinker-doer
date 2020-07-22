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

extern std::vector<int> baseIds;
extern std::unordered_map<MAP *, int> baseLocations;
extern std::map<int, BASE_STRATEGY> baseStrategies;
extern std::vector<int> combatVehicleIds;
extern std::vector<int> outsideCombatVehicleIds;

void aiStrategy(int id);
void populateSharedLists();
VEH *getVehicleByAIId(int aiId);
bool isReacheable(int id, int x, int y);

#endif // __AI_H__

