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
	double conventionalDefenseMultiplier[3];
	bool withinFriendlySensorRange;
};

struct ESTIMATED_VALUE
{
	double demanding;
	double remaining;
};

struct MILITARY_STRENGTH
{
	int totalUnitCount = 0;
	int regularUnitCount = 0;
	double psiStrength = 0.0;
	double conventionalStrength = 0.0;
};

struct ActiveFactionInfo
{
	FACTION_INFO factionInfos[8];
	std::vector<int> baseIds;
	std::unordered_map<MAP *, int> baseLocations;
	std::unordered_set<int> presenceRegions;
	std::unordered_map<int, std::unordered_set<int>> regionBaseIds;
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
	std::unordered_map<int, double> regionDefenseDemand;
	
	void clear()
	{
		baseIds.clear();
		baseLocations.clear();
		presenceRegions.clear();
		regionBaseIds.clear();
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
		regionDefenseDemand.clear();
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
int getNearestFactionBaseRange(int factionId, int x, int y);
int getNearestBaseId(int x, int y, std::unordered_set<int> baseIds);
int getNearestBaseRange(int x, int y, std::unordered_set<int> baseIds);

