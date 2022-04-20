#pragma once

#include <vector>
#include <set>
#include <map>
#include "main.h"
#include "terranx.h"
#include "terranx_types.h"
#include "terranx_enums.h"
#include "ai.h"
#include "aiTask.h"
#include "aiMoveFormer.h"

const double MIN_SIGNIFICANT_THREAT = 0.2;
const int COMBAT_ABILITY_FLAGS = ABL_AMPHIBIOUS | ABL_AIR_SUPERIORITY | ABL_AAA | ABL_COMM_JAMMER | ABL_EMPATH | ABL_ARTILLERY | ABL_BLINK_DISPLACER | ABL_TRANCE | ABL_NERVE_GAS | ABL_SOPORIFIC_GAS | ABL_DISSOCIATIVE_WAVE;

const unsigned int UNIT_TYPE_COUNT = 2;
extern const char *UNIT_TYPE_NAMES[];
enum UnitType
{
	UT_MELEE,
	UT_LAND_ARTILLERY,
};

struct CombatEffect
{
	double attack;
	double defend;
	double average;
};

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

struct UnitStrength
{
	double psiOffense = 0.0;
	double psiDefense = 0.0;
	double conventionalOffense = 0.0;
	double conventionalDefense = 0.0;
};

struct UnitTypeInfo
{
	const char *unitTypeName = nullptr;
	double foeTotalWeight = 0.0;
	double ownTotalWeight = 0.0;
	double ownTotalCombatEffect = 0.0;
	double requiredProtection = 0.0;
	double providedProtection = 0.0;
	// relative protectionLevel on a scale -1.0 to +1.0
	// -1.0 = unprotected
	//  0.0 = adequate
	// +1.0 = overprotected
	double protectionLevel;
	double protectionDemand;
	// best counter units by combat effectiveness
	std::vector<IdDoubleValue> protectors;
	
	void clear();
	bool isProtectionSatisfied();
	double getRemainingProtectionRequest();
	void setComputedValues();
	
};

struct BaseInfo
{
	double sensorOffenseMultiplier;
	double sensorDefenseMultiplier;
	double intrinsicDefenseMultiplier;
	double conventionalDefenseMultipliers[3];
	int borderBaseDistance;
	int enemyAirbaseDistance;
	int policeEffect;
	int policeAllowed;
	int policeDrones;
	// foe vehicles weight grouped by unit id
	std::vector<double> foeUnitWeights = std::vector<double>(MaxProtoNum);
	// own vehicles weight grouped by unit id
	std::vector<double> ownUnitWeights = std::vector<double>(2 * MaxProtoFactionNum);
	// own units combat effects by foe unit id
	std::vector<CombatEffect> combatEffects = std::vector<CombatEffect>((2 * MaxProtoFactionNum) * MaxProtoNum);
	// own units average combat effect
	std::vector<double> averageCombatEffects = std::vector<double>(2 * MaxProtoFactionNum);
	// unit type data
	UnitTypeInfo unitTypeInfos[UNIT_TYPE_COUNT];
	
	BaseInfo();
	void clear();
	double getFoeUnitWeight(int unitId);
	void setFoeUnitWeight(int unitId, double weight);
	void increaseFoeUnitWeight(int unitId, double weight);
	void decreaseFoeUnitWeight(int unitId, double weight);
	double getOwnUnitWeight(int unitId);
	void setOwnUnitWeight(int unitId, double weight);
	void increaseOwnUnitWeight(int unitId, double weight);
	void decreaseOwnUnitWeight(int unitId, double weight);
	CombatEffect *getCombatEffect(int ownUnitId, int foeUnitId);
	double getAverageCombatEffect(int unitId);
	void setAverageCombatEffect(int unitId, double averageCombatEffect);
	double getVehicleAverageCombatEffect(int vehicleId);
	UnitTypeInfo *getUnitTypeInfo(int unitId);
	void setUnitTypeComputedValues();
	double getTotalProtectionDemand();
	
};

struct Production
{
	double globalProtectionDemand = 0.0;
	
	double landColonyDemand;
	std::map<int, double> seaColonyDemands;
	
	int landTerraformingRequestCount;
	std::map<int, int> seaTerraformingRequestCounts;
	
	double landFormerDemand;
	std::map<int, double> seaFormerDemands;
	
	double landPodPoppingDemand;
	std::map<int, double> seaPodPoppingDemand;
	
	double infantryPolice2xDemand;
	double defenseDemand;
	
	void clear()
	{
		globalProtectionDemand = 0.0;
		seaColonyDemands.clear();
		seaTerraformingRequestCounts.clear();
		seaFormerDemands.clear();
		seaPodPoppingDemand.clear();
	}
	
};
	
struct Data
{
	FactionInfo factionInfos[8];
	int bestOffenseValue;
	int bestDefenseValue;
	
	// geography
	
	Geography geography;
	
	// map data
	
	std::vector<TileInfo> tileInfos;
	
	// base data
	
	std::vector<BaseInfo> baseInfos = std::vector<BaseInfo>(MaxBaseNum);
	
	// own units average combat effect weighet across all bases
	std::vector<double> averageCombatEffects = std::vector<double>(2 * MaxProtoFactionNum);
	// opponent active combat units
	std::vector<int> foeActiveCombatUnitIds;
	
	std::vector<int> baseIds;
	std::map<MAP *, int> baseTiles;
	std::set<int> presenceRegions;
	std::map<int, std::set<int>> regionBaseIds;
	std::map<int, std::vector<int>> regionBaseGroups;
	std::vector<int> vehicleIds;
	std::vector<int> combatVehicleIds;
	std::vector<int> activeCombatUnitIds;
	std::vector<int> scoutVehicleIds;
	std::vector<int> outsideCombatVehicleIds;
	std::vector<int> unitIds;
	std::vector<int> combatUnitIds;
	std::vector<int> landColonyUnitIds;
	std::vector<int> seaColonyUnitIds;
	std::vector<int> airColonyUnitIds;
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
	double medianBaseDefenseDemand;
	std::map<int, double> regionDefenseDemand;
	int maxBaseSize;
	int maxMineralSurplus;
	int maxMineralIntake2;
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
	
	void setup();
	void cleanup();
	
	// access global data arrays
	
	TileInfo *getTileInfo(int mapIndex);
	TileInfo *getTileInfo(int x, int y);
	TileInfo *getTileInfo(MAP *tile);
	BaseInfo *getBaseInfo(int baseId);
	
	// combat data
	
	double getAverageCombatEffect(int unitId);
	void setAverageCombatEffect(int unitId, double averageCombatEffect);
	double getVehicleAverageCombatEffect(int vehicleId);
	
};

extern int aiFactionId;
extern Data aiData;

// helper functions

int getUnitType(int unitId);
const char *getUnitTypeName(int unitType);

