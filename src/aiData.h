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

const double MIN_SIGNIFICANT_THREAT = 0.2;
const int COMBAT_ABILITY_FLAGS = ABL_AMPHIBIOUS | ABL_AIR_SUPERIORITY | ABL_AAA | ABL_COMM_JAMMER | ABL_EMPATH | ABL_ARTILLERY | ABL_BLINK_DISPLACER | ABL_TRANCE | ABL_NERVE_GAS | ABL_SOPORIFIC_GAS | ABL_DISSOCIATIVE_WAVE;

// unit group in stack to count required and provided effects
// some units can attack some groups and not others
const int UNIT_GROUP_COUNT = 5;
enum UNIT_GROUP
{
	UG_NEEDLEJET,		// needlejet in flight can be attacked only by unit with air superiority
	UG_LOW_AIR,			// low flying air unit can be attacked by close range capable units
	UG_SHIP,			// ship can engage in both close and long range combat
	UG_LAND_ARTILLERY,	// land artillery protects stack against bombardment
	UG_LAND_MELEE,		// surface unit can be attacked by any other unit and bombarded by artillery
};

const int ATTACK_TYPE_COUNT = 2;
enum ATTACK_TYPE
{
	AT_MELEE,
	AT_ARTILLERY,
};

const int NATURE_COUNT = 2;
enum NATURE
{
	NAT_PSI,
	NAT_CON,
};
int getCombatValueNature(int combatValue);

const int COMBAT_TYPE_COUNT = 3;
enum COMBAT_TYPE
{
	CT_MELEE,
	CT_ARTILLERY_DUEL,
	CT_BOMBARDMENT,
};

/*
Defines cross table of combatEffects
1. own units: 2 * MaxProtoFactionNum
2. factions: MaxPlayerNum
3. faction units: 2 * MaxProtoFactionNum
*/
class CombatEffectTable
{
	private:
		
		std::vector<double> combatEffects =
			std::vector<double>((2 * MaxProtoFactionNum) * (MaxPlayerNum) * (2 * MaxProtoFactionNum) * (2) * (COMBAT_TYPE_COUNT));

	public:
		
		void setCombatEffect(int ownUnitId, int foeFactionId, int foeUnitId, int attackingSide, COMBAT_TYPE combatType, double combatEffect);
		double getCombatEffect(int ownUnitId, int foeFactionId, int foeUnitId, int attackingSide, COMBAT_TYPE combatType);

};

/*
Defines weighted effectiveness of our units across all enemy vehicles.
*/
class BaseCombatData
{
	private:
		
		std::vector<double> unitEffects = std::vector<double>(2 * MaxProtoFactionNum);
		
	public:
		
		double triadThreats[3]{0.0};
		double requiredEffect = 0.0;
		double desiredEffect = 0.0;
		double providedEffect = 0.0;
		double currentProvidedEffect = 0.0;
		
		void clear();
		void setUnitEffect(int unitId, double effect);
		double getUnitEffect(int unitId);
		double getVehicleEffect(int vehicleId);
		void addProvidedEffect(int vehicleId, bool current);
		bool isSatisfied(bool current);
		double getDemand(bool current);
		
};

/*
Holds foe units weights.
*/
class FoeUnitWeightTable
{
	public:
		
		std::vector<double> weights = std::vector<double>((2 * MaxProtoFactionNum) * MaxPlayerNum);
		
		void clear();
		double get(int factionId, int unitId);
		void set(int factionId, int unitId, double weight);
		void add(int factionId, int unitId, double weight);

};

struct FactionInfo
{
	double offenseMultiplier;
	double defenseMultiplier;
	double fanaticBonusMultiplier;
	double threatCoefficient;
	int bestOffenseValue;
	int bestDefenseValue;
	int enemyCount;
	std::vector<MAP *> airbases;
};

// land-ocean transfer
struct Transfer
{
	MAP *passengerStop = nullptr;
	MAP *transportStop = nullptr;
	bool isValid()
	{
		return passengerStop != nullptr && transportStop != nullptr;
	}
};

struct FactionGeography
{
	std::set<int> landAssociations;
	std::set<int> oceanAssociations;
	std::map<int, int> extendedRegionAssociations;
	std::map<MAP *, int> coastalBaseOceanAssociations;
	std::map<int, std::set<int>> associationConnections;
	std::set<int> immediatelyReachableAssociations;
	std::set<int> potentiallyReachableAssociations;
	std::map<int, std::vector<int>> oceanAssociationShipyards;
	std::map<int, std::vector<int>> oceanAssociationSeaTransports;
	std::map<int, std::map<int, std::vector<Transfer>>> associationTransfers;
	std::map<MAP *, std::vector<Transfer>> oceanBaseTransfers;
	std::map<int, int> associationAreas;
	std::vector<MAP *> raiseableCoasts;
	
	std::vector<Transfer> &getAssociationTransfers(int association1, int association2);
	std::vector<Transfer> &getOceanBaseTransfers(MAP *tile);

};

struct Geography
{
	std::map<int, std::vector<MAP *>> internalOceanConcavePoints;
	
	FactionGeography factions[MaxPlayerNum];

	Geography()
	{
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			factions[factionId] = FactionGeography();
		}

	}

};

struct Force
{
	private:
		int vehiclePad0;
	public:
		ATTACK_TYPE attackType;
		double hastyCoefficient;
		Force(int _vehicleId, ATTACK_TYPE _attackType, double _hastyCoefficient);
		int getVehicleId();
};

struct TileMovementInfo
{
	double travelImpediment = 0.0;
	std::map<MAP *, double> estimatedTravelTimes;
	
	bool blockedNeutral = false;
	bool zocNeutral = false;
	bool blocked = false;
	bool zoc = false;
	bool friendly = false;
	
	bool isBlocked(bool ignoreHostile);
	bool isZoc(bool ignoreHostile);
	
};

struct TileInfo
{
	bool warzone = false;
	bool warzoneBaseCapture = false;
	bool warzonePsi = false;
	int warzoneConventionalOffenseValue = 0;
	
	// [angle][movementType]
	int hexCosts[ANGLE_COUNT][MOVEMENT_TYPE_COUNT];
	
	TileMovementInfo movementInfos[MaxPlayerNum];
	// enemyOffensiveForces by vehicle.pad_0
	std::vector<Force> enemyOffensiveForces;
	
	bool isBlocked(int factionId, bool ignoreHostile);
	bool isZoc(int factionId, bool ignoreHostile);
	
	int landCluster[MaxPlayerNum];
	
};

struct UnitStrength
{
	double psiOffense = 0.0;
	double psiDefense = 0.0;
	double conventionalOffense = 0.0;
	double conventionalDefense = 0.0;
};

struct CombatEffect
{
	COMBAT_TYPE combatType;
	bool destructive;
	double effect = 0.0;
};

class EnemyStackInfo
{
private:
	
	std::vector<CombatEffect> unitEffects = std::vector<CombatEffect>(2 * MaxProtoFactionNum * ATTACK_TYPE_COUNT);
	
public:
		
	std::vector<int> vehicleIds;
	std::vector<int> artilleryVehicleIds;
	std::vector<int> bombardmentVehicleIds;
	
	int baseRange;
	bool base;
	bool baseOrBunker;
	bool airbase;
	
	bool alien = false;
	bool alienMelee = false;
	bool alienMindWorms = false;
	bool alienSporeLauncher = false;
	bool alienFungalTower = false;
	bool needlejetInFlight = false;
	bool artifact = false;
	double priority = 0.0;
	int lowestSpeed = -1;
	bool artillery = false;
	bool bombardment = false;
	bool targetable = false;
	bool bombardmentDestructive = false;
	
	void addVehicle(int vehicleId);
	
	// combat data
	
	double requiredEffect = 0.0;
	double requiredEffectBombarded = 0.0;
	double desiredEffect = 0.0;
	double providedEffect = 0.0;
	
	void setUnitEffect(int unitId, ATTACK_TYPE attackType, COMBAT_TYPE combatType, bool destructive, double effect);
	CombatEffect getUnitEffect(int unitId, ATTACK_TYPE attackType);
	CombatEffect getVehicleEffect(int vehicleId, ATTACK_TYPE attackType);
//	void addProvidedEffect(int vehicleId, COMBAT_TYPE combatType);
	double getAverageUnitEffect(int unitId);
	double getRequiredEffect(bool bombarded);
	
};

struct EnemyBaseInfo
{
	int minTravelTime;
};

struct PoliceData
{
	int effect;
	int unitsAllowed;
	int unitsProvided;
	int dronesExisting;
	int dronesQuelled;

	void clear();
	void addVehicle(int vehicleId);
	bool isSatisfied();

};

struct BaseInfo
{
	int borderDistance;
	int enemyAirbaseDistance;
	// combat data
	std::vector<int> garrison;
	BaseCombatData combatData;
	int safeTime;
	// police data
	PoliceData policeData;
	
};

struct TargetBase
{
	int baseId;
	double protection;
};

struct Production
{
	std::set<MAP *> unavailableBuildSites;
	double landColonyDemand;
	std::map<int, double> seaColonyDemands;
	
	std::map<int, int> landTerraformingRequestCounts;
	std::map<int, int> seaTerraformingRequestCounts;
	std::map<int, double> landFormerDemands;
	std::map<int, double> seaFormerDemands;
	
	double landPodPoppingDemand;
	std::map<int, double> seaPodPoppingDemands;
	
	double policeDemand;
	double protectionDemand;
	double landCombatDemand;
	std::map<int, double> seaCombatDemands;
	double landAlienCombatDemand;
	// [infantry defenders, attackers, artillery]
	double unitTypeFractions[3];
	double unitTypePreferences[3];
	
	void clear();
	void addTerraformingRequest(MAP *tile);
	
};

struct UnloadRequest
{
	int vehiclePad0;
	MAP *destination;
	MAP *unboardLocation;
	
	UnloadRequest(int _vehicleId, MAP *_destination, MAP *_unboardLocation);
	int getVehicleId();
	
};
struct TransitRequest
{
	int vehiclePad0;
	MAP *origin;
	MAP *destination;
	int seaTransportVehiclePad0 = -1;
	
	TransitRequest(int _vehicleId, MAP *_origin, MAP *_destination);
	int getVehicleId();
	void setSeaTransportVehicleId(int seaTransportVehicleId);
	int getSeaTransportVehicleId();
	bool isFulfilled();
	
};
struct TransportControl
{
	std::map<int, std::vector<UnloadRequest>> unloadRequests;
	std::vector<TransitRequest> transitRequests;
	
	void clear();
	void addUnloadRequest(int seaTransportVehicleId, int vehicleId, MAP *destination, MAP *unboardLocation);
	std::vector<UnloadRequest> getSeaTransportUnloadRequests(int seaTransportVehicleId);
	
};

struct Data
{
	// development parameters

	double developmentScale;
	double baseGrowthScale;

	// faction info

	FactionInfo factionInfos[8];
	int bestOffenseValue;
	int bestDefenseValue;

	// geography

	Geography geography;
	double roadCoverage;
	double tubeCoverage;

	// map data

	std::vector<TileInfo> tileInfos;

	// combat data

	// ocean associations with enemy bases
	std::set<int> enemyBaseOceanAssociations;
	// combat effects
	CombatEffectTable combatEffectTable;

	// enemy stacks
	// also includes unprotected artifacts
	std::map<MAP *, EnemyStackInfo> enemyStacks;
	// unprotected enemy bases
	std::vector<MAP *> emptyEnemyBaseTiles;
	// best units
	std::map<int, double> unitWeightedEffects;

	// base info
	std::map<int, int> baseBuildingItems;
	std::vector<BaseInfo> baseInfos = std::vector<BaseInfo>(MaxBaseNum);
	// average required protection across bases (for normalization)
	double factionBaseAverageRequiredProtection;

//	CombatData ownTerritoryCombatData;
//	CombatData foeTerritoryCombatData;
	std::vector<double> globalAverageUnitEffects = std::vector<double>(2 * MaxProtoFactionNum);

	// other global variables
	
	std::map<MAP *, int> locationBaseIds;
	std::vector<int> baseIds;
	std::vector<int> vehicleIds;
	std::vector<int> combatVehicleIds;
	std::vector<int> activeCombatUnitIds;
	std::vector<int> scoutVehicleIds;
	std::vector<int> outsideCombatVehicleIds;
	std::vector<int> unitIds;
	std::vector<int> combatUnitIds;
	std::vector<int> colonyUnitIds;
	std::vector<int> landColonyUnitIds;
	std::vector<int> seaColonyUnitIds;
	std::vector<int> airColonyUnitIds;
	std::vector<int> prototypeUnitIds;
	std::vector<int> colonyVehicleIds;
	std::vector<int> formerVehicleIds;
	std::vector<int> airFormerVehicleIds;
	std::map<int, std::vector<int>> landFormerVehicleIds;
	std::map<int, std::vector<int>> seaFormerVehicleIds;
	std::vector<int> seaTransportVehicleIds;
	double threatLevel;
	std::map<int, std::vector<int>> regionSurfaceCombatVehicleIds;
	std::map<int, std::vector<int>> regionSurfaceScoutVehicleIds;
	int mostVulnerableBaseId;
	double mostVulnerableBaseThreat;
	double medianBaseDefenseDemand;
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
	TransportControl transportControl;
	std::map<int, Task> tasks;
	std::map<int, TaskList> potentialTasks;
	double airColonyProductionPriority;
	double landColonyProductionPriority;
	std::map<int, double> seaColonyProductionPriorities;
	Production production;
	int netIncome;
	int grossIncome;

	// reset all variables

	void clear();

	// access global data arrays

	TileInfo &getTileInfo(int mapIndex);
	TileInfo &getTileInfo(int x, int y);
	TileInfo &getTileInfo(MAP *tile);
	TileMovementInfo &getTileMovementInfo(MAP *tile, int factionId);
	BaseInfo &getBaseInfo(int baseId);

	// combat data

	double getGlobalAverageUnitEffect(int unitId);
	void setGlobalAverageUnitEffect(int unitId, double effect);
	
	// utility methods
	
	void addSeaTransportRequest(int oceanAssociation);
	
};

extern int aiFactionId;
extern MFaction *aiMetaFaction;
extern Data aiData;

bool isWarzone(MAP *tile);

// Vehicle movement action (move).
struct MovementAction
{
	MAP *destination;
	int movementAllowance;
	
	MovementAction(MAP *_destination, int _movementAllowance);
	
};

// Vehicle combat action (melee attack, long range fire).
struct CombatAction
: MovementAction
{
	ATTACK_TYPE attackType;
	MAP *attackTarget;
	
	CombatAction(MAP *_destination, int _movementAllowance, ATTACK_TYPE _attackType, MAP *_attackTarget);
	
};

// vehicle activity (includes combat actions)
struct Activity
{
	bool goalAchievable = false;
	std::vector<MovementAction> movements;
	std::vector<MovementAction> hostileBaseCaptures;
	std::vector<MovementAction> neutralBaseCaptures;
	std::vector<CombatAction> meleeAttacks;
	std::vector<CombatAction> artilleryAttacks;
	
};

struct MutualLoss
{
	double own = 0.0;
	double foe = 0.0;
	
	void accumulate(MutualLoss &mutualLoss);
	
};

