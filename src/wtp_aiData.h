#pragma once

#include <vector>
#include <set>
#include <map>
#include "robin_hood.h"
#include "main.h"
#include "engine.h"
#include "wtp_aiTask.h"
#include "wtp_aiMoveFormer.h"

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

const int MOVEMENT_TYPE_COUNT = 6;
enum MovementType
{
	MT_AIR,
	MT_SEA_NATIVE,
	MT_SEA_REGULAR,
	MT_LAND_NATIVE,
	MT_LAND_EASY,
	MT_LAND_REGULAR,
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
	
	Transfer(MAP *_passengerStop, MAP *_transportStop);
	bool isValid();
	
};

/*
Faction related geography.
*/
struct ClusterMovementInfo
{
	double averageHexCost;
	// landmark movementCosts
	// [landmarkId, tileIndex]
	std::vector<std::vector<double>> landmarkNetwork;
};
struct Cluster
{
	int type;
	int area;
	// landmark networks by movement type
	robin_hood::unordered_flat_map<MovementType, ClusterMovementInfo> movementInfos;
};
struct Geography
{
	robin_hood::unordered_flat_set<int> associations[2];
	robin_hood::unordered_flat_map<int, int> associationAreas;
	robin_hood::unordered_flat_map<int, int> extendedRegionAssociations;
	robin_hood::unordered_flat_map<MAP *, int> coastalBaseOceanAssociations;
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>> associationConnections;
	robin_hood::unordered_flat_set<int> immediatelyReachableAssociations;
	robin_hood::unordered_flat_set<int> potentiallyReachableAssociations;
	robin_hood::unordered_flat_map<int, std::vector<int>> oceanAssociationShipyards;
	robin_hood::unordered_flat_map<int, std::vector<int>> oceanAssociationSeaTransports;
	robin_hood::unordered_flat_set<int> oceanAssociationTargets;
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, std::vector<Transfer>>> associationTransfers;
	robin_hood::unordered_flat_map<MAP *, std::vector<Transfer>> oceanBaseTransfers;
	std::vector<MAP *> raiseableCoasts;
	
	std::vector<Transfer> &getAssociationTransfers(int association1, int association2);
	std::vector<Transfer> &getOceanBaseTransfers(MAP *tile);
	
	// clusters
	std::vector<Cluster> clusters;
	
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

struct TileFactionInfo
{
	// association
	int association = -1;
	// surfaceAssociations [continent/ocean]
	int surfaceAssociations[2] = {-1, -1};
	
	// movement restrictions
	bool blocked[2] = {false, false, };
	bool zoc[2] = {false, false, };
	double impediment[2] = {0.0, 0.0, };
	
	// non-combat and combat clusters
	// [non-combat/combat][continent/ocean]
	int clusterIds[2][2] = {{-1, -1}, {-1, -1}};
	
};

struct TileInfo
{
	bool ocean;
	
	// adjacent tiles
	int adjacentTileIndexes[ANGLE_COUNT];
	int validAdjacentTileCount;
	int validAdjacentTileAngles[ANGLE_COUNT];
	int validAdjacentTileIndexes[ANGLE_COUNT];
	
	// base radius tiles
	int baseRadiusTileIndexes[OFFSET_COUNT_RADIUS];
	
	// warzone
	bool warzone = false;
	bool warzoneBaseCapture = false;
	bool warzonePsi = false;
	int warzoneConventionalOffenseValue = 0;
	
	// hex costs
	// [movementType][angle]
	int hexCosts[MOVEMENT_TYPE_COUNT][ANGLE_COUNT];
	
	// faction infos
	TileFactionInfo factionInfos[MaxPlayerNum];
	
	// enemyOffensiveForces by vehicle.pad_0
	std::vector<Force> enemyOffensiveForces;
	
	// items
	int baseId = -1;
	bool base = false;
	bool bunker = false;
	
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
	robin_hood::unordered_flat_set<MAP *> unavailableBuildSites;
	double landColonyDemand;
	robin_hood::unordered_flat_map<int, double> seaColonyDemands;
	
	robin_hood::unordered_flat_map<int, int> landTerraformingRequestCounts;
	robin_hood::unordered_flat_map<int, int> seaTerraformingRequestCounts;
	robin_hood::unordered_flat_map<int, double> landFormerDemands;
	robin_hood::unordered_flat_map<int, double> seaFormerDemands;
	
	double landPodPoppingDemand;
	robin_hood::unordered_flat_map<int, double> seaPodPoppingDemands;
	
	double policeDemand;
	double protectionDemand;
	double landCombatDemand;
	robin_hood::unordered_flat_map<int, double> seaCombatDemands;
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
	robin_hood::unordered_flat_map<int, std::vector<UnloadRequest>> unloadRequests;
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
	
	Geography factionGeographys[MaxPlayerNum];
	double roadCoverage;
	double tubeCoverage;
	
	// map data
	
	std::vector<TileInfo> tileInfos;
	
	// combat data
	
	// combat effects
	CombatEffectTable combatEffectTable;
	
	// enemy stacks
	// also includes unprotected artifacts
	robin_hood::unordered_flat_map<MAP *, EnemyStackInfo> enemyStacks;
	// unprotected enemy bases
	std::vector<MAP *> emptyEnemyBaseTiles;
	// best units
	robin_hood::unordered_flat_map<int, double> unitWeightedEffects;
	
	// base info
	robin_hood::unordered_flat_map<int, int> baseBuildingItems;
	std::vector<BaseInfo> baseInfos = std::vector<BaseInfo>(MaxBaseNum);
	// average required protection across bases (for normalization)
	double factionBaseAverageRequiredProtection;
	
	std::vector<double> globalAverageUnitEffects = std::vector<double>(2 * MaxProtoFactionNum);
	
	// other global variables
	
	robin_hood::unordered_flat_map<MAP *, int> locationBaseIds;
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
	robin_hood::unordered_flat_map<int, std::vector<int>> landFormerVehicleIds;
	robin_hood::unordered_flat_map<int, std::vector<int>> seaFormerVehicleIds;
	std::vector<int> seaTransportVehicleIds;
	double threatLevel;
	robin_hood::unordered_flat_map<int, std::vector<int>> regionSurfaceCombatVehicleIds;
	robin_hood::unordered_flat_map<int, std::vector<int>> regionSurfaceScoutVehicleIds;
	int mostVulnerableBaseId;
	double mostVulnerableBaseThreat;
	double medianBaseDefenseDemand;
	robin_hood::unordered_flat_map<int, double> regionDefenseDemand;
	int maxBaseSize;
	int totalMineralIntake2;
	int maxMineralSurplus;
	robin_hood::unordered_flat_map<int, int> oceanAssociationMaxMineralSurpluses;
	int bestLandUnitId;
	int bestSeaUnitId;
	int bestAirUnitId;
	std::vector<int> landCombatUnitIds;
	std::vector<int> seaCombatUnitIds;
	std::vector<int> airCombatUnitIds;
	std::vector<int> landAndAirCombatUnitIds;
	std::vector<int> seaAndAirCombatUnitIds;
	double landAlienHunterDemand;
	robin_hood::unordered_flat_map<int, int> seaTransportRequestCounts;
	TransportControl transportControl;
	robin_hood::unordered_flat_map<int, Task> tasks;
	robin_hood::unordered_flat_map<int, TaskList> potentialTasks;
	double airColonyProductionPriority;
	double landColonyProductionPriority;
	robin_hood::unordered_flat_map<int, double> seaColonyProductionPriorities;
	Production production;
	int netIncome;
	int grossIncome;
	
	// police 2x unit available
	
	bool police2xUnitAvailable;
	
	// reset all variables
	
	void clear();
	
	// access global data arrays
	
	TileInfo &getTileInfo(int mapIndex);
	TileInfo &getTileInfo(int x, int y);
	TileInfo &getTileInfo(MAP *tile);
	TileFactionInfo &getTileFactionInfo(MAP *tile, int factionId);
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

