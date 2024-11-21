#pragma once

#include <vector>
#include <set>
#include <map>
#include "robin_hood.h"
#include "main.h"
#include "engine.h"
#include "wtp_aiTask.h"
#include "wtp_aiMoveFormer.h"

const int INT_INFINITY = INT_MAX;
const double INF = std::numeric_limits<double>::infinity();

const double MIN_SIGNIFICANT_THREAT = 0.2;
const int COMBAT_ABILITY_FLAGS = ABL_AMPHIBIOUS | ABL_AIR_SUPERIORITY | ABL_AAA | ABL_COMM_JAMMER | ABL_EMPATH | ABL_ARTILLERY | ABL_BLINK_DISPLACER | ABL_TRANCE | ABL_NERVE_GAS | ABL_SOPORIFIC_GAS | ABL_DISSOCIATIVE_WAVE;

enum AttackingSide
{
	AS_OWN,
	AS_FOE,
};
const int ATTACKING_SIDE_COUNT = AS_FOE + 1;

struct IdIntValue
{
	int id;
	int value;
};

struct IdDoubleValue
{
	int id;
	double value;

};

/// strength for psi/con against psi/con
struct CombatStrength
{
	// [attacker gear type (psi/con)][defender gear type (psi/con)]
	std::array<std::array<double, COMBAT_TYPE_COUNT>, COMBAT_TYPE_COUNT> values;
	
	void accumulate(CombatStrength &combatStrength, double multiplier = 1.0);
	double getAttackEffect(int vehicleId);
	
};

/// strength for psi/con grouped by combat type
struct CombatTypeStrength
{
	// [combatType][attacker gear type (psi/con)][defender gear type (psi/con)]
	std::array<std::array<std::array<double, COMBAT_TYPE_COUNT>, COMBAT_TYPE_COUNT>, COMBAT_MODE_COUNT> values;
	
};

/// cross strength for psi/con
struct CombatStrengthMatrix
{
	// [attacker weapon type (psi/con)][defender weapon type (psi/con)]
	std::array<std::array<std::array<double, COMBAT_TYPE_COUNT>, COMBAT_TYPE_COUNT>, COMBAT_MODE_COUNT> values;
	
	void set(COMBAT_MODE combatMode, COMBAT_TYPE attackerCOMBAT_TYPE, COMBAT_TYPE defenderCOMBAT_TYPE, double value);
	void accumulate(CombatStrengthMatrix &_combatMatrix, double multiplier = 1.0);
	double computeDestruction(COMBAT_TYPE defenderCOMBAT_TYPE, CombatStrength defenderCombatStrength);
	
};

enum MovementType
{
	MT_AIR,
	MT_SEA_REGULAR,
	MT_SEA_NATIVE,
	MT_LAND_REGULAR,
	MT_LAND_NATIVE,
};
const size_t MovementTypeCount = MT_LAND_NATIVE + 1;
const std::vector<MovementType> seaMovementTypes {MT_SEA_REGULAR, MT_SEA_NATIVE, };
const size_t SeaMovementTypeCount = MT_SEA_NATIVE - MT_SEA_REGULAR + 1;
const std::vector<MovementType> landMovementTypes {MT_LAND_REGULAR, MT_LAND_NATIVE, };
const size_t LandMovementTypeCount = MT_LAND_NATIVE - MT_LAND_REGULAR + 1;

struct Offense
{
	std::array<bool, ENGAGEMENT_MODE_COUNT> engagementModes;
	double hastyCoefficient = 1.0;
};

struct TileCombatData
{
	robin_hood::unordered_flat_map<int, Offense> enemyOffenses;
};

struct TileInfo
{
	int index;
	MAP *tile;
	
	// surface type (in boolean and numeric form)
	bool land;
	bool ocean;
	SurfaceType surfaceType;
	bool coast;
	std::set<int> adjacentSeaRegions;
	
	// items
	int baseId = -1;
	bool base = false;
	bool bunker = false;
	bool airbase = false;
	bool port = false;
	
	// land vehicle is allowed here without transport
	bool landAllowed = false;
	
	// vehicles at tile
	
	std::vector<int> vehicleIds;
	bool needlejetInFlight;
	
	// adjacent tiles
	
	std::vector<int> adjacentTileIndexes;
	std::vector<MAP *> adjacentTiles;
	std::vector<MapAngle> adjacentMapAngles;
	
	// hex costs
	// [movementType][angle]
	int hexCosts[MovementTypeCount][ANGLE_COUNT];
	
	// movement data
	
	bool blocked;
	bool zoc;
	
	// combat data
	TileCombatData combatData;
	
	// destruction proportion of regular non combat not armored vehicle
	double danger;
	// warzone (>= 0.25 chance of destruction)
	bool warzone = false;
	
};

// combat data

class FoeUnitWeightTable
{
	public:
		
		std::vector<double> weights = std::vector<double>((2 * MaxProtoFactionNum) * MaxPlayerNum);
		
		void clear();
		double get(int factionId, int unitId);
		void set(int factionId, int unitId, double weight);
		void add(int factionId, int unitId, double weight);

};

/**
CombatEffects table.
Combat effect is the number of enemy units this unit can destroy for its life.
[1. own units: 2 * MaxProtoFactionNum]
[2. factions: MaxPlayerNum]
[3. faction units: 2 * MaxProtoFactionNum]
[4. attacking side: 2]
[5. combat type: 3]
*/
class CombatEffectTable
{
	private:
		
		std::array<std::array<std::array<std::array<std::array<double, COMBAT_MODE_COUNT>, ATTACKING_SIDE_COUNT>, (2 * MaxProtoFactionNum)>, (MaxPlayerNum)>, (2 * MaxProtoFactionNum)> combatEffects {};

	public:
		
		std::vector<int> ownCombatUnitIds;
		std::vector<int> foeFactionIds;
		std::vector<int> hostileFoeFactionIds;
		std::vector<int> foeCombatVehicleIds;
		std::array<std::vector<int>, MaxPlayerNum> foeUnitIds;
		std::array<std::vector<int>, MaxPlayerNum> foeCombatUnitIds;
		
		double getCombatEffect(int ownUnitId, int foeFactionId, int foeUnitId, AttackingSide attackingSide, COMBAT_MODE combatMode);
		void setCombatEffect(int ownUnitId, int foeFactionId, int foeUnitId, AttackingSide attackingSide, COMBAT_MODE combatMode, double combatEffect);
		double getCombatEffect(int factionId1, int unitId1, int factionId2, int unitId2, COMBAT_MODE combatMode);

};

struct BasePoliceData
{
	int allowedUnitCount;
	std::array<int, 2> providedUnitCounts;
	int requiredPower;
	int providedPower;
	std::array<int, 2> policePowers;
	std::array<double, 2> policeGains;
	
	double getUnitPoliceGain(int unitId, int factionId)
	{
		return policeGains.at(isPolice2xUnit(unitId, factionId));
	}
	double getVehiclePoliceGain(int vehicleId)
	{
		return policeGains.at(isPolice2xVehicle(vehicleId));
	}
	
	void reset()
	{
		std::fill(providedUnitCounts.begin(), providedUnitCounts.end(), 0);
		providedPower = 0;
	}
	
	void addVehicle(int vehicleId)
	{
		if (isPolice2xVehicle(vehicleId))
		{
			providedUnitCounts.at(0)++;
			providedUnitCounts.at(1)++;
			providedPower += policePowers.at(1);
		}
		else
		{
			providedUnitCounts.at(0)++;
			providedPower += policePowers.at(0);
		}
		
	}
	
	bool isSatisfied2x() const
	{
		return (providedUnitCounts.at(1) >= allowedUnitCount || providedPower >= requiredPower);
	}
	
	bool isSatisfied(int vehicleId) const
	{
		return (providedUnitCounts.at(isPolice2xVehicle(vehicleId)) >= allowedUnitCount || providedPower >= requiredPower);
	}
	
};

class BaseCombatData
{
private:
	
	std::array<double, 2 * MaxProtoFactionNum> effects;
	
public:
	
	double triadThreats[3]{0.0};
	double requiredEffect = 0.0;
	double desiredEffect = 0.0;
	double providedEffect = 0.0;
	double providedEffectPresent = 0.0;
	
	void clear()
	{
		requiredEffect = 0.0;
		desiredEffect = 0.0;
		providedEffect = 0.0;
		providedEffectPresent = 0.0;
	}
	
	void setUnitEffect(int unitId, double effect)
	{
		effects.at(getUnitSlotById(unitId)) = effect;
	}
	double getUnitEffect(int unitId) const
	{
		return effects.at(getUnitSlotById(unitId));
	}
	double getVehicleEffect(int vehicleId) const
	{
		return getUnitEffect(Vehicles[vehicleId].unit_id) * getVehicleMoraleMultiplier(vehicleId);
	}
	
	void reset()
	{
		providedEffect = 0.0;
		providedEffectPresent = 0.0;
	}
	
	void addVehicle(int vehicleId, bool present)
	{
		double vehicleEffect = getVehicleEffect(vehicleId);

		providedEffect += vehicleEffect;
		
		if (present)
		{
			providedEffectPresent += vehicleEffect;
		}

	}
	
	bool isSatisfied(bool present) const
	{
		return (present ? providedEffectPresent : providedEffect) >= requiredEffect;
	}
	
	double getDemand(bool present) const
	{
		double required = requiredEffect;
		double provided = (present ? providedEffectPresent : providedEffect);
		return ((required > 0.0 && provided < required) ? 1.0 - provided / required : 0.0);
	}
	
};

/// base information
struct BaseInfo
{
	int id;
	int factionId;
	bool port;
	int enemyAirbaseDistance;
	
	// economical data
	double gain;
	
	// police data	
	BasePoliceData policeData;
	
	// combat data
	std::array<double, 4> moraleMultipliers;
	std::array<double, 4> defenseMultipliers;
	double sensorOffenseMultiplier;
	double sensorDefenseMultiplier;
	std::vector<int> garrison;
	bool artillery;
	BaseCombatData combatData;
	double safeTime;
	
	// protector operations
	
	void resetProtectors()
	{
		policeData.reset();
		combatData.reset();
	}
	
	void addProtector(int vehicleId)
	{
		policeData.addVehicle(vehicleId);
		combatData.addVehicle(vehicleId, getVehicleMapTile(vehicleId) == getBaseMapTile(id));
	}
	
	bool isSatisfied(int vehicleId, bool present) const
	{
		return policeData.isSatisfied(vehicleId) && combatData.isSatisfied(present);
	}
	
	// enemy base data
	
	// total gain received from capturing base
	// includes combat unit expenses (build, support)
	double captureGain;
	
	// protector weights
	std::vector<IdDoubleValue> protectorWeights;
	
	// combat data
	
	// averate range between player's bases and this base
	double averagePlayerBasesRange;
	// player unit base assault effects
	// how big part of base protectors can this unit destroy
	robin_hood::unordered_flat_map<int, double> assaultEffects;
	// estimated time for player troops to reach the base
	double estimatedApproachTime;
	// production superiority
	double playerProductionSuperiority;
	
};

/// faction data for all factions
struct FactionInfo
{
	std::vector<int> baseIds;
	
	std::array<std::array<bool, 3>, 2> canBuildMelee;
	double offenseMultiplier;
	double defenseMultiplier;
	double fanaticBonusMultiplier;
	int maxConOffenseValue;
	int maxConDefenseValue;
	double threatCoefficient;
	int enemyCount;
	int bestSeaTransportUnitId;
	int bestSeaTransportUnitSpeed;
	int bestSeaTransportUnitCapacity;
	
	// economical data
	
	double averageBaseGain;
	
	// terraforming data
	
	int airFormerCount;
	robin_hood::unordered_flat_map<int, int> seaClusterFormerCounts;
	robin_hood::unordered_flat_map<int, int> landTransportedClusterFormerCounts;
	
	// combat data
	
	// averate range between player and faction bases
	double averageBasesRange;
	double averagePlayerBasesRange;
	int totalVendettaCount; // excluding aliens and player
	double productionPower;
	
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
	COMBAT_MODE combatMode;
	bool destructive;
	double effect = 0.0;
};

struct ProtectedLocation
{
	MAP *tile = nullptr;
	
	std::vector<int> vehicleIds;
	std::vector<int> airVehicleIds;
	std::vector<int> artilleryVehicleIds;
	std::vector<int> nonArtilleryVehicleIds;
	
	int baseId;
	bool base;
	bool bunker;
	bool airbase;
	bool artillery = false;
	
	std::array<double, 4> defenseMultipliers;
	
	void addVehicle(int vehicleId);
	
};

struct OffenseEffect
{
	ENGAGEMENT_MODE engagementMode;
	double effect;
};

class EnemyStackInfo
{
private:
	
	static int const BOMBARDMENT_ROUNDS = 5;
	std::array<double, 2 * MaxProtoFactionNum * COMBAT_MODE_COUNT> offenseEffects;	// including location bonuses
	std::array<double, 2 * MaxProtoFactionNum> defenseEffects;						// excluding location bonuses
	std::vector<CombatEffect> unitEffects = std::vector<CombatEffect>(2 * MaxProtoFactionNum * COMBAT_MODE_COUNT);
	std::array<double, 2 * MaxProtoFactionNum> effects;
	
public:
	
	MAP *tile = nullptr;
	
	std::vector<int> vehicleIds;
	std::vector<int> airVehicleIds;
	std::vector<int> artilleryVehicleIds;
	std::vector<int> nonArtilleryVehicleIds;
	
	int baseRange;
	bool base;
	bool baseOrBunker;
	bool airbase;
	int baseId;
	
	bool hostile = false;
	bool breakTreaty = false;
	bool alien = false;
	bool alienMelee = false;
	bool alienMindWorms = false;
	bool alienSporeLauncher = false;
	bool alienFungalTower = false;
	bool needlejetInFlight = false;
	bool artifact = false;
	double averageUnitGain = 0.0;
	int lowestSpeed = -1;
	bool artillery = false;
	bool bombardment = false;
	bool targetable = false;
	bool bombardmentDestructive = false;
	
	// combat data
	
	double requiredSuperiority;
	double desiredSuperiority;
	// sum of relative powers
	double weight;
	// max relative power that can be destroyed by bombardment
	double maxBombardmentEffect;
	
	void addVehicle(int vehicleId);
	bool isUnitCanMeleeAttackStack(int unitId, MAP *position = nullptr) const;
	bool isUnitCanArtilleryAttackStack(int unitId, MAP *position = nullptr) const;
	
	// unit effects
	
	void setUnitEffect(int unitId, double effect)
	{
		effects.at(getUnitSlotById(unitId)) = effect;
	}
	double getUnitEffect(int unitId) const
	{
		return effects.at(getUnitSlotById(unitId));
	}
	double getVehicleEffect(int vehicleId) const
	{
		return getUnitEffect(Vehicles[vehicleId].unit_id) * getVehicleMoraleMultiplier(vehicleId);
	}
	void setUnitOffenseEffect(int unitId, COMBAT_MODE combatMode, double effect);
	double getUnitOffenseEffect(int unitId, COMBAT_MODE combatMode) const;
	double getVehicleOffenseEffect(int vehicleId, COMBAT_MODE combatMode) const;
	void setUnitDefenseEffect(int unitId, double effect);
	double getUnitDefenseEffect(int unitId) const;
	double getVehicleDefenseEffect(int vehicleId) const;
	double getUnitBombardmentEffect(int unitId) const;
	double getVehicleBombardmentEffect(int vehicleId) const;
	double getUnitDirectEffect(int unitId) const;
	double getVehicleDirectEffect(int vehicleId) const;
	
	// attack variables
	
	std::vector<IdDoubleValue> bombardmentVechileTravelTimes;
	std::vector<IdDoubleValue> directVechileTravelTimes;
	double combinedBombardmentEffect;	// one turn relative destruction of nonArtillery part of the stack
	double combinedDirectEffect;		// one to one combat effect: either artillery duel or melee
	bool bombardmentSufficient;
	bool directSufficient;
	// time until attackers gather required superiority
	double coordinatorTravelTime;
	// extra weight that can be added by building or pulling in
	double extraWeight;
	// 
	double destructionRatio;
	bool requiredSuperioritySatisfied;
	
	void resetAttackParameters()
	{
		bombardmentVechileTravelTimes.clear();
		directVechileTravelTimes.clear();
		combinedBombardmentEffect = 0.0;
		combinedDirectEffect = 0.0;
		bombardmentSufficient = false;
		directSufficient = false;
		coordinatorTravelTime = 0.0;
		extraWeight = 0.0;
		destructionRatio = 0.0;
		requiredSuperioritySatisfied = false;
	}
	
	double getTotalBombardmentEffect() const;
	double getTotalBombardmentEffectRatio() const;
	double getRequiredEffect() const;
	double getDestructionRatio() const;
	bool isSufficient(bool desired) const;
	bool addAttacker(IdDoubleValue vehicleTravelTime);
	void computeAttackParameters();
	
};

struct FormerRequest
{
	MAP *tile;
	TERRAFORMING_OPTION const *option;
	double terraformingTime;
	double income;
};

struct Production
{
	robin_hood::unordered_flat_set<MAP *> unavailableBuildSites;
	
	// formers
	std::vector<FormerRequest> formerRequests;
	
	double landPodPoppingDemand;
	robin_hood::unordered_flat_map<int, double> seaPodPoppingDemands;
	
	// combat unit demands
	
	robin_hood::unordered_flat_map<int, double> basePoliceGains;
	robin_hood::unordered_flat_map<int, double> baseProtectionGains;
	robin_hood::unordered_flat_map<int, double> enemyBaseCaptureGains;
	std::vector<EnemyStackInfo *> untargetedEnemyStackInfos;
	
	void clear()
	{
		unavailableBuildSites.clear();
		seaPodPoppingDemands.clear();
		basePoliceGains.clear();
		baseProtectionGains.clear();
		enemyBaseCaptureGains.clear();
		untargetedEnemyStackInfos.clear();
	}
	
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
	// map data
	
	std::vector<TileInfo> tileInfos;
	std::map<int, int> seaRegionAreas;
	
	// base infos
	
	std::array<BaseInfo, MaxBaseNum> baseInfos;
	
	// faction infos
	
	std::array<FactionInfo, MaxPlayerNum> factionInfos;
	
	// statistical data
	
	double averageCitizenMineralIntake;
	double averageCitizenEconomyIntake;
	double averageCitizenLabsIntake;
	double averageCitizenPsychIntake;
	double averageCitizenMineralIntake2;
	double averageCitizenEconomyIntake2;
	double averageCitizenLabsIntake2;
	double averageCitizenPsychIntake2;
	double averageCitizenResourceIncome;
	
	double averageWorkerNutrientIntake2;
	double averageWorkerMineralIntake2;
	double averageWorkerEnergyIntake2;
	double averageWorkerResourceIncome;
	
	// development parameters
	
	double developmentScale;
	
	// combat values
	
	int maxConOffenseValue;
	int maxConDefenseValue;
	double avgConOffenseValue;
	double avgConDefenseValue;
	
	// transports assigned passengers and capacity
	// Engine has passenger -> transport relationship but for transport it is pretty difficult to find its passengers. Here is the cashed passengers.
	
	robin_hood::unordered_flat_map<int, std::vector<int>> allTransportPassengers;
	
	// player combat data
	
	// combat data
	
	// combat effects
	CombatEffectTable combatEffectTable;
	
	// enemy stacks
	robin_hood::unordered_flat_map<MAP *, EnemyStackInfo> enemyStacks;
	bool hasEnemyStack(MAP *tile)
	{
		return enemyStacks.find(tile) != enemyStacks.end();
	}
	bool isEnemyStackAt(MAP *tile)
	{
		return enemyStacks.find(tile) != enemyStacks.end();
	}
	EnemyStackInfo &getEnemyStackInfo(MAP *tile)
	{
		return enemyStacks.at(tile);
	}
	
	// unprotected enemy bases
	std::vector<int> emptyEnemyBaseIds;
	robin_hood::unordered_flat_set<MAP *> emptyEnemyBaseTiles;
	bool isEmptyEnemyBaseAt(MAP *tile)
	{
		return emptyEnemyBaseTiles.find(tile) != emptyEnemyBaseTiles.end();
	}
	
	// best units
	robin_hood::unordered_flat_map<int, double> unitWeightedEffects;
	
	// average required protection across bases (for normalization)
	double factionBaseAverageRequiredProtection;
	
	std::vector<double> globalAverageUnitEffects = std::vector<double>(2 * MaxProtoFactionNum);
	
	// other global variables
	
	std::vector<int> baseIds;
	std::vector<int> vehicleIds;
	std::vector<int> combatVehicleIds;
	std::vector<int> activeCombatUnitIds;
	std::vector<int> scoutVehicleIds;
	std::vector<int> outsideCombatVehicleIds;
	std::vector<int> unitIds;
	std::vector<int> combatUnitIds;
	std::vector<int> colonyUnitIds;
	std::vector<int> formerUnitIds;
	std::vector<int> landColonyUnitIds;
	std::vector<int> seaColonyUnitIds;
	std::vector<int> airColonyUnitIds;
	std::vector<int> colonyVehicleIds;
	std::vector<int> formerVehicleIds;
	std::vector<int> airFormerVehicleIds;
	robin_hood::unordered_flat_map<int, std::vector<int>> landFormerVehicleIds;
	robin_hood::unordered_flat_map<int, std::vector<int>> seaFormerVehicleIds;
	// sea transports by clusters
	robin_hood::unordered_flat_map<int, std::vector<int>> seaTransportVehicleIds;
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
	int maxMineralIntake2;
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
	
	TileInfo &getTileInfo(int tileIndex);
	TileInfo &getTileInfo(MAP *tile);
	TileInfo &getBaseTileInfo(int baseId);
	TileInfo &getVehicleTileInfo(int vehicleId);
	bool isSea(MAP *tile);
	bool isLand(MAP *tile);
	bool isSeaUnitAllowed(MAP *tile, int factionId);
	bool isLandUnitAllowed(MAP *tile);
	
	BaseInfo &getBaseInfo(int baseId);
	
	// combat data
	
	double getGlobalAverageUnitEffect(int unitId);
	void setGlobalAverageUnitEffect(int unitId, double effect);
	
	// utility methods
	
	void addSeaTransportRequest(int seaCluster);
	
};

extern Data aiData;
extern int aiFactionId;
extern MFaction *playerMFaction;
extern Faction *aiFaction;
extern FactionInfo *aiFactionInfo;

bool isWarzone(MAP *tile);

// Vehicle movement action (move).
struct MoveAction
{
	MAP *tile;
	int movementAllowance;
	
	MoveAction(MAP *_tile, int _movementAllowance)
	: tile{_tile}, movementAllowance{_movementAllowance}
	{}
	
};

struct MutualLoss
{
	double own = 0.0;
	double foe = 0.0;
	
	void accumulate(MutualLoss &mutualLoss);
	
};

bool isBlocked(int tileIndex);
bool isBlocked(MAP *tile);
bool isZoc(int tileIndex);
bool isZoc(MAP *tile);

