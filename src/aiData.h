#pragma once

#include <vector>
#include <set>
#include <map>
#include "main.h"
#include "terranx.h"
#include "terranx_types.h"
#include "terranx_enums.h"
#include "aiTask.h"
#include "aiMoveFormer.h"

const int COMBAT_ABILITY_FLAGS = ABL_AMPHIBIOUS | ABL_AIR_SUPERIORITY | ABL_AAA | ABL_COMM_JAMMER | ABL_EMPATH | ABL_ARTILLERY | ABL_BLINK_DISPLACER | ABL_TRANCE | ABL_NERVE_GAS | ABL_SOPORIFIC_GAS | ABL_DISSOCIATIVE_WAVE;

struct FactionInfo
{
	double offenseMultiplier;
	double defenseMultiplier;
	double fanaticBonusMultiplier;
	double threatCoefficient;
	int bestWeaponOffenseValue;
	int bestArmorDefenseValue;
	std::vector<MAP *> airbases;
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
	double landPsiOffense;
	double landPsiDefense;
	double seaPsiOffense;
	double seaPsiDefense;
	double airPsiOffense;
	double airPsiDefense;
	double conventionalOffense;
	double conventionalDefense;
	double psiDefense;
	double psiOffense;
	double artillery;
	double psiDefenseDemand;
	double psiOffenseDemand;
	double conventionalDefenseDemand;
	double conventionalOffenseDemand;
	double artilleryDemand;
};

struct UnitStrength
{
	double weight = 0.0;
	double psiOffense = 0.0;
	double psiDefense = 0.0;
	double conventionalOffense = 0.0;
	double conventionalDefense = 0.0;
	
	void normalize(double totalWeight)
	{
		this->psiOffense /= this->weight;
		this->psiDefense /= this->weight;
		this->conventionalOffense /= this->weight;
		this->conventionalDefense /= this->weight;
		this->weight /= totalWeight;
	}
	
};

struct MilitaryStrength
{
	double totalWeight = 0.0;
	std::map<int, UnitStrength> unitStrengths;
	
	void normalize()
	{
		for
		(
			std::map<int, UnitStrength>::iterator unitStrengthsIterator = this->unitStrengths.begin();
			unitStrengthsIterator != this->unitStrengths.end();
			unitStrengthsIterator++
		)
		{
			UnitStrength *unitStrength = &(unitStrengthsIterator->second);
			unitStrength->normalize(this->totalWeight);
			
		}
		
	}
	
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
	double sensorOffenseMultiplier;
	double sensorDefenseMultiplier;
	double intrinsicDefenseMultiplier;
	double conventionalDefenseMultipliers[3];
	double exposure;
	bool inSharedOceanRegion;
	double defenseDemand;
	int combatUnitId;
	int targetBaseId;
	MilitaryStrength defenderStrength;
	MilitaryStrength opponentStrength;
};

struct ESTIMATED_VALUE
{
	double demanding;
	double remaining;
};

struct VehicleWeight
{
	int id;
	double weight;
};

struct FactionGeography
{
	std::map<int, int> associations;
	std::map<MAP *, int> coastalBaseOceanAssociations;
	std::set<MAP *> oceanBaseTiles;
	std::map<int, std::set<int>> connections;
	std::set<int> immediatelyReachableAssociations;
	std::set<int> potentiallyReachableAssociations;
	
};
	
struct Geography
{
	std::map<int, int> associations;
	std::map<int, std::set<int>> connections;
	FactionGeography faction[MaxPlayerNum];
	
	Geography()
	{
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			faction[MaxPlayerNum] = FactionGeography();
		}
		
	}
	
};

struct TileInfo
{
	bool blocked = false;
	bool zoc = false;
	bool warzone = false;
	int workedBase = -1;
};
	
struct BaseInfo
{
	std::vector<MAP *> workedTiles;
	std::vector<MAP *> unworkedTiles;
};

struct Production
{
	double landColonyDemand;
	std::map<int, double> seaColonyDemands;
	
	double landFormerDemand;
	std::map<int, double> seaFormerDemands;
	
	double landPodPoppingDemand;
	std::map<int, double> seaPodPoppingDemand;
	
	void clear()
	{
		seaColonyDemands.clear();
		seaFormerDemands.clear();
		seaPodPoppingDemand.clear();
	}
	
};
	
struct Data
{
	FactionInfo factionInfos[8];
	int bestWeaponOffenseValue;
	int bestArmorDefenseValue;
	
	// geography
	Geography geography;
	
	// map data
	std::vector<TileInfo> tileInfos;
	
	// base data
	std::vector<BaseInfo> baseInfos;
	
	std::vector<int> baseIds;
	std::map<MAP *, int> baseTiles;
	std::set<int> presenceRegions;
	std::map<int, std::set<int>> regionBaseIds;
	std::map<int, std::vector<int>> regionBaseGroups;
	std::map<int, BaseStrategy> baseStrategies;
	std::vector<int> vehicleIds;
	std::vector<int> combatVehicleIds;
	std::vector<int> scoutVehicleIds;
	std::vector<int> outsideCombatVehicleIds;
	std::vector<int> unitIds;
	std::vector<int> combatUnitIds;
	std::vector<int> prototypeUnitIds;
	std::vector<int> colonyVehicleIds;
	std::vector<int> formerVehicleIds;
	double threatLevel;
	std::map<int, std::vector<int>> regionSurfaceCombatVehicleIds;
	std::map<int, std::vector<int>> regionSurfaceScoutVehicleIds;
	std::map<int, double> baseAnticipatedNativeAttackStrengths;
	std::map<int, double> baseRemainingNativeProtectionDemands;
	int mostVulnerableBaseId;
	double mostVulnerableBaseDefenseDemand;
	std::map<int, double> regionDefenseDemand;
	int maxBaseSize;
	int maxMineralSurplus;
	std::map<int, int> oceanAssociationMaxMineralSurpluses;
	int bestLandUnitId;
	int bestSeaUnitId;
	int bestAirUnitId;
	std::vector<int> landCombatUnitIds;
	std::vector<int> seaCombatUnitIds;
	std::vector<int> airCombatUnitIds;
	std::vector<int> landAndAirCombatUnitIds;
	std::vector<int> seaAndAirCombatUnitIds;
	double landAlienHunterDemand;
	std::map<int, int> seaTransportRequestCounts;
	std::map<int, Task> tasks;
	int bestVehicleWeaponOffenseValue;
	int bestVehicleArmorDefenseValue;
	double airColonyProductionPriority;
	double landColonyProductionPriority;
	std::map<int, double> seaColonyProductionPriorities;
	Production production;
	std::set<MAP *> blockedLocations;
	
	void setup();
	void cleanup();
	
	// access global data arrays
	TileInfo *getTileInfo(int mapIndex);
	TileInfo *getTileInfo(int x, int y);
	TileInfo *getTileInfo(MAP *tile);
	BaseInfo *getBaseInfo(int baseId);
	
};

extern int aiFactionId;
extern Data aiData;

