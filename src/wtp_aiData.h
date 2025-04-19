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

struct Offense
{
	std::array<bool, ENGAGEMENT_MODE_COUNT> engagementModes;
	double hastyCoefficient = 1.0;
};

struct TileInfo;
struct Adjacent
{
	int angle;
	TileInfo *tileInfo;
};
struct HexCost
{
	int exact;
	int approximate;
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
	bool rough;
	std::set<int> adjacentSeaRegions;
	
	// items
	int baseId = -1;
	bool base = false;
	bool bunker = false;
	bool airbase = false;
	bool port = false;
	std::array<bool, MaxPlayerNum> sensors {};
	// player base range
	int baseRange = INT_MAX;
	int landBaseRange = INT_MAX;
	
	// land vehicle is allowed here without transport
	bool landAllowed = false;
	
	// vehicles at tile
	
	std::vector<int> vehicleIds;
	bool needlejetInFlight;
	
	// adjacent tiles
	ArrayVector<Adjacent, ANGLE_COUNT> adjacents;
	// range tiles
	std::array<TileInfo *, 25> range2TileInfosArray;
	ArrayView<TileInfo *, 25> range2CenterTileInfos;
	ArrayView<TileInfo *, 25> range2NoCenterTileInfos;
	
	// hex costs
	// [movementType][angle]
	std::array<std::array<HexCost, ANGLE_COUNT>, MOVEMENT_TYPE_COUNT> hexCosts;
	
	// movement data
	
	bool blocked;
	bool orgZoc;
	bool dstZoc;
	
	// danger zones
	// artifact or empty base can be captured
	bool unfriendlyDangerZone;
	// units can be attacked by hostiles
	bool hostileDangerZone;
	// units can be bombarded by hostiles
	bool artilleryDangerZone;
	
	// nearest bases
	std::array<IdIntValue, 3> nearestBaseRanges;
	
};

// combat data

struct FoeUnitWeightTable
{
	// [factionId][unitId]
	std::array<robin_hood::unordered_flat_map<int, double>, MaxPlayerNum> weights;
	
	void clear()
	{
		for (robin_hood::unordered_flat_map<int, double> &factionWeights : weights)
		{
			factionWeights.clear();
		}
	}
	
};

struct AssaultEffect
{
	double speedFactor = 0.0;
	double attack = 0.0;
	double defend = 0.0;
	
	void clear()
	{
		this->speedFactor = 0.0;
		this->attack = 0.0;
		this->defend = 0.0;
	}
	
	double getEffect(double attackMultiplier, double defendMultiplier) const
	{
		double attackEffect = this->attack * attackMultiplier;
		double defendEffect = this->defend * defendMultiplier;
		
		double effect = 0.0;
		
		if (attackEffect <= defendEffect)
		{
			effect = attackEffect;
		}
		else
		{
			effect = speedFactor * attackEffect + (1.0 - speedFactor) * defendEffect;
		}
		
		return effect;
		
	}
	
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
	
	std::array<std::array<std::array<std::array<std::array<double, COMBAT_MODE_COUNT>, MaxProtoNum>, MaxPlayerNum>, MaxProtoNum>, MaxPlayerNum> combatEffects;
	std::array<std::array<std::array<std::array<AssaultEffect, MaxProtoNum>, MaxPlayerNum>, MaxProtoNum>, MaxPlayerNum> assaultEffects;

public:
	
	robin_hood::unordered_flat_set<int> ownCombatUnitIds;
	robin_hood::unordered_flat_set<int> foeFactionIds;
	robin_hood::unordered_flat_set<int> hostileFoeFactionIds;
	robin_hood::unordered_flat_set<int> foeCombatVehicleIds;
	std::array<robin_hood::unordered_flat_set<int> , MaxPlayerNum> foeUnitIds;
	std::array<robin_hood::unordered_flat_set<int> , MaxPlayerNum> foeCombatUnitIds;
	
	void clear()
	{
		for (int attackerFactionId = 0; attackerFactionId < MaxPlayerNum; attackerFactionId++)
		{
			for (int attackerUnitId = 0; attackerUnitId < MaxProtoNum; attackerUnitId++)
			{
				for (int defenderFactionId = 0; defenderFactionId < MaxPlayerNum; defenderFactionId++)
				{
					for (int defenderUnitId = 0; defenderUnitId < MaxProtoNum; defenderUnitId++)
					{
						for (COMBAT_MODE combatMode : {CM_MELEE, CM_ARTILLERY_DUEL, CM_BOMBARDMENT})
						{
							combatEffects.at(attackerFactionId).at(attackerUnitId).at(defenderFactionId).at(defenderUnitId).at(combatMode) = 0.0;
						}
						assaultEffects.at(attackerFactionId).at(attackerUnitId).at(defenderFactionId).at(defenderUnitId).clear();
					}
				}
			}
		}
	}
	
	void setCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, COMBAT_MODE combatMode, double combatEffect)
	{
		this->combatEffects.at(attackerFactionId).at(attackerUnitId).at(defenderFactionId).at(defenderUnitId).at(combatMode) = combatEffect;
	}
	double getCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, COMBAT_MODE combatMode) const
	{
		return this->combatEffects.at(attackerFactionId).at(attackerUnitId).at(defenderFactionId).at(defenderUnitId).at(combatMode);
	}
	
	void setAssaultEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, AssaultEffect const &assaultEffect)
	{
		this->assaultEffects.at(attackerFactionId).at(attackerUnitId).at(defenderFactionId).at(defenderUnitId) = assaultEffect;
	}
	AssaultEffect const &getAssaultEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId) const
	{
		return this->assaultEffects.at(attackerFactionId).at(attackerUnitId).at(defenderFactionId).at(defenderUnitId);
	}
	
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
	
	void resetProvided()
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

struct CounterEffect
{
	double required = 0.0;
	double provided = 0.0;
	double providedPresent = 0.0;
	
	CounterEffect(double _required, double _provided, double _providedPresent)
	: required(_required), provided(_provided), providedPresent(_providedPresent)
	{}
	CounterEffect(double _required)
	: CounterEffect(_required, 0.0, 0.0)
	{}
	
	void resetProvided()
	{
		provided = 0.0;
		providedPresent = 0.0;
	}
	bool isSatisfied(bool present) const
	{
		return (present ? providedPresent : provided) >= required;
	}
	
};

struct DefenseCombatData
{
	std::array<double, 4> defenseMultipliers;
	double sensorOffenseMultiplier;
	double sensorDefenseMultiplier;
	std::vector<int> garrison;
	
	std::array<robin_hood::unordered_flat_map<int, CounterEffect>, MaxPlayerNum> counterEffects = {};
	double requiredEffect;
	
	void clear()
	{
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			counterEffects.at(factionId).clear();
		}
	}
	void resetProvided()
	{
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			for (robin_hood::pair<int, CounterEffect> &counterEffectEntry : counterEffects.at(factionId))
			{
				CounterEffect &counterEffect = counterEffectEntry.second;
				counterEffect.resetProvided();
			}
		}
	}
	double getProvidedEffect(bool present)
	{
		double providedEffect = 0.0;
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			for (robin_hood::pair<int, CounterEffect> const &factionCounterEffectEntry : counterEffects.at(factionId))
			{
				CounterEffect const &factionCounterEffect = factionCounterEffectEntry.second;
				providedEffect += present ? factionCounterEffect.providedPresent : factionCounterEffect.provided;
			}
		}
		return providedEffect;
	}
	
	double getUnitEffect(int unitId) const;
	double getVehicleEffect(int vehicleId) const
	{
		return getVehicleMoraleMultiplier(vehicleId) * getUnitEffect(Vehicles[vehicleId].unit_id);
	}
	
	double addCounterEffect(int foeFactionId, int foeUnitId, double effect, bool present)
	{
		CounterEffect &counterEffect = counterEffects.at(foeFactionId).at(foeUnitId);
		double requiredEffect = counterEffect.required - counterEffect.provided;
		double appliedEffect = std::min(requiredEffect, effect);
		double remainingEffect = effect - appliedEffect;
		return remainingEffect;
	}
	
	void addVehicle(int vehicleId, bool present)
	{
		// TODO
	}
	
	bool isSatisfied(bool present) const
	{
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			for (robin_hood::pair<int, CounterEffect> const &counterEffectEntry : counterEffects.at(factionId))
			{
				CounterEffect const &counterEffect = counterEffectEntry.second;
				if (!counterEffect.isSatisfied(present))
					return false;
			}
		}
		return true;
	}
	
};

struct BaseProbeData
{
	double requiredEffect = 0.0;
	double providedEffect = 0.0;
	double providedEffectPresent = 0.0;
	
	void clear()
	{
		requiredEffect = 0.0;
		providedEffect = 0.0;
		providedEffectPresent = 0.0;
	}
	
	void reset()
	{
		providedEffect = 0.0;
		providedEffectPresent = 0.0;
	}
	
	void addVehicle(int vehicleId, bool present)
	{
		double vehicleEffect = getVehicleMoraleMultiplier(vehicleId);
		
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

///*
//Saves and restores all bases state.
//*/
//class BaseSnapshots
//{
//private:
//	
//	static std::vector<BASE> baseSnapshots;
//	
//public:
//		
//	static void takeSnapshots()
//	{
//		baseSnapshots.clear();
//		
//		for (size_t baseId = 0; baseId < (size_t)*BaseCount; baseId++)
//		{
//			baseSnapshots.emplace_back();
//			memcpy(&(baseSnapshots.at(baseId)), &(Bases[baseId]), sizeof(BASE));
//		}
//	}
//	static void restoreSnapshots()
//	{
//		for (size_t baseId = 0; baseId < baseSnapshots.size(); baseId++)
//		{
//			memcpy(&(Bases[baseId]), &(baseSnapshots.at(baseId)), sizeof(BASE));
//		}
//	}
//	
//};
//
/// base information
struct BaseInfo
{
	BASE snapshot;
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
	bool artillery;
	DefenseCombatData combatData;
	BaseProbeData probeData;
	
	// enemy base data
	
	// total gain received from capturing base
	// includes combat unit expenses (build, support)
	double captureGain;
	
	// protector unit weights
	// [factionId][unitId]
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_map<int, double>> protectorUnitWeights;
	
	// combat data
	
	// range to closest player base
	int closestPlayerBaseRange;
	// player unit base assault effects
	// how big part of base protectors can this unit destroy
	robin_hood::unordered_flat_map<int, double> assaultEffects;
	// estimated time for player troops to reach the base
	double estimatedApproachTime;
	// production superiority
	double playerProductionSuperiority;
	
	// reset base
	
	void reset()
	{
		Profiling::start("- BaseInfo::reset");
        memcpy(&Bases[id], &snapshot, sizeof(BASE));
		Profiling::stop("- BaseInfo::reset");
	}
	
	// protector operations
	
	void resetProtectors()
	{
		policeData.resetProvided();
		combatData.resetProvided();
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
	std::array<int, 3> fastestTriadChassisIds;
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
	
	int totalVendettaCount; // excluding aliens and player
	double productionPower;
	
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
	double averageAttackGain = 0.0;
	int lowestSpeed = -1;
	bool artillery = false;
	bool bombardment = false;
	bool targetable = false;
	bool bombardmentDestructive = false;
	double averageUnitCost = 0.0;
	
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

struct ConvoyRequest
{
	MAP *tile;
	double gain;
};

struct Production
{
	robin_hood::unordered_flat_set<MAP *> unavailableBuildSites;
	
	std::vector<FormerRequest> formerRequests;
	std::vector<ConvoyRequest> convoyRequests;
	
	// combat unit demands
	
	robin_hood::unordered_flat_map<int, double> basePoliceGains;
	robin_hood::unordered_flat_map<int, double> baseProtectionGains;
	robin_hood::unordered_flat_map<int, double> enemyBaseCaptureGains;
	std::vector<EnemyStackInfo *> untargetedEnemyStackInfos;
	
	void clear()
	{
		unavailableBuildSites.clear();
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
	robin_hood::unordered_flat_map<int, int> regionAreas;
	robin_hood::unordered_flat_set<int> enemyRegions;
	std::vector<MAP *> monoliths;
	std::vector<MAP *> pods;
	
	// base infos
	
	std::array<BaseInfo, MaxBaseNum> baseInfos;
	void resetBase(int baseId)
	{
		baseInfos.at(baseId).reset();
	}
	
	// bunker combat data
	
	robin_hood::unordered_flat_map<MAP *, DefenseCombatData> bunkerCombatDatas;
	
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
	robin_hood::unordered_flat_set<int> unfriendlyEndangeredVehicleIds;
	robin_hood::unordered_flat_set<int> hostileEndangeredVehicleIds;
	robin_hood::unordered_flat_set<int> artilleryEndangeredVehicleIds;
	robin_hood::unordered_flat_map<int, std::vector<int>> landFormerVehicleIds;
	robin_hood::unordered_flat_map<int, std::vector<int>> seaFormerVehicleIds;
	// sea transports by clusters
	robin_hood::unordered_flat_map<int, std::vector<int>> seaTransportVehicleIds;
	double threatLevel;
	robin_hood::unordered_flat_map<int, std::vector<int>> regionSurfaceCombatVehicleIds;
	robin_hood::unordered_flat_map<int, std::vector<int>> regionSurfaceScoutVehicleIds;
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
	
	// utility methods
	
	void addSeaTransportRequest(int seaCluster);
	
};

extern Data aiData;
extern int aiFactionId;
extern MFaction *aiMFaction;
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
	MoveAction()
	: MoveAction(nullptr, 0)
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
bool isZoc(int orgTileIndex, int dstTileIndex);
bool isZoc(MAP *orgTile, MAP *dstTile);

