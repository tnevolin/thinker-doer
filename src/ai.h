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

struct FactionInfo
{
	double offenseMultiplier;
	double defenseMultiplier;
	double fanaticBonusMultiplier;
	double threatKoefficient;
	int bestWeaponOffenseValue;
	int bestArmorDefenseValue;
	std::vector<Location> airbases;
};

struct Threat
{
	double psi;
	double infantry;
	double psiDefense;
	double psiOffense;
	double conventionalDefense;
	double conventionalOffense;
	double artillery;
	double psiDefenseDemand;
	double psiOffenseDemand;
	double conventionalDefenseDemand;
	double conventionalOffenseDemand;
	double artilleryDemand;
};

struct DefenseDemand
{
	double psiThreat;
	double conventionalThreat;
	double psiDefense;
	double psiOffense;
	double conventionalDefense;
	double conventionalOffense;
	double artillery;
	double psiDefenseDemand;
	double psiOffenseDemand;
	double conventionalDefenseDemand;
	double conventionalOffenseDemand;
	double artilleryDemand;
};

struct BaseStrategy
{
	BASE *base;
	std::vector<int> garrison;
	double nativeProtection;
	double nativeThreat;
	double nativeDefenseDemand;
	int unpopulatedTileCount;
	int unpopulatedTileRangeSum;
	double averageUnpopulatedTileRange;
	double psiDefenseMultiplier;
	double conventionalDefenseMultipliers[3];
	double conventionalDefenseDemand;
	bool withinFriendlySensorRange;
	double exposure;
	bool inSharedOceanRegion;
	DefenseDemand defenseDemand;
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
	FactionInfo factionInfos[8];
	std::vector<int> baseIds;
	std::unordered_map<MAP *, int> baseLocations;
	std::unordered_set<int> presenceRegions;
	std::unordered_map<int, std::unordered_set<int>> regionBaseIds;
	std::map<int, std::vector<int>> regionBaseGroups;
	std::map<int, BaseStrategy> baseStrategies;
	std::vector<int> vehicleIds;
	std::vector<int> combatVehicleIds;
	std::vector<int> scoutVehicleIds;
	std::vector<int> outsideCombatVehicleIds;
	std::vector<int> prototypes;
	std::vector<int> colonyVehicleIds;
	std::vector<int> formerVehicleIds;
	double threatLevel;
	std::unordered_map<int, std::vector<int>> regionSurfaceCombatVehicleIds;
	std::unordered_map<int, std::vector<int>> regionSurfaceScoutVehicleIds;
	std::unordered_map<int, double> baseAnticipatedNativeAttackStrengths;
	std::unordered_map<int, double> baseRemainingNativeProtectionDemands;
	double defenseDemand;
	std::unordered_map<int, double> regionDefenseDemand;
	int maxMineralSurplus;
	std::unordered_map<int, int> regionMaxMineralSurpluses;
	int bestLandUnitId;
	int bestSeaUnitId;
	int bestAirUnitId;
	
	void clear()
	{
		baseIds.clear();
		baseLocations.clear();
		presenceRegions.clear();
		regionBaseIds.clear();
		regionBaseGroups.clear();
		baseStrategies.clear();
		vehicleIds.clear();
		combatVehicleIds.clear();
		scoutVehicleIds.clear();
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
		regionMaxMineralSurpluses.clear();
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
void setSharedOceanRegions();
VEH *getVehicleByAIId(int aiId);
Location getNearestPodLocation(int vehicleId);
void designUnits();
void proposeMultiplePrototypes(int factionId, std::vector<int> chassisIds, std::vector<int> weaponIds, std::vector<int> armorIds, std::vector<int> abilitiesSets, int reactorId, int plan, char *name);
void checkAndProposePrototype(int factionId, int chassisId, int weaponId, int armorId, int abilities, int reactorId, int plan, char *name);
void evaluateBaseNativeDefenseDemands();
int getNearestFactionBaseRange(int factionId, int x, int y);
int getNearestBaseId(int x, int y, std::unordered_set<int> baseIds);
int getNearestBaseRange(int x, int y, std::unordered_set<int> baseIds);
void evaluateBaseExposures();
int getFactionBestPrototypedWeaponOffenseValue(int factionId);
int getFactionBestPrototypedArmorDefenseValue(int factionId);
double evaluateCombatStrength(double offenseValue, double defenseValue);
double evaluateOffenseStrength(double offenseValue);
double evaluateDefenseStrength(double DefenseValue);
//void evaluateBaseDefenseDemands();
//void evaluateLandBaseDefenseDemand(int baseId);
//void evaluateOceanBaseDefenseDemand(int baseId);
bool isVehicleThreatenedByEnemyInField(int vehicleId);
bool isDestinationReachable(int vehicleId, int x, int y);
int getRangeToNearestFactionAirbase(int x, int y, int factionId);
int getVehicleTurnsToDestination(int vehicleId, int x, int y);
void populateBaseExposures();
int getNearestEnemyBaseDistance(int baseId);

