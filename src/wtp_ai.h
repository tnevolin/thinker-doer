#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include "wtp_aiData.h"
#include "engine.h"

const int MAX_SAFE_LOCATION_SEARCH_RANGE = 6;
const int STACK_MAX_BASE_RANGE = 20;
const int STACK_MAX_COUNT = 20;
const int MAX_PROXIMITY_RANGE = 4;

// estimated average land colony travel time
const double AVERAGE_LAND_COLONY_TRAVEL_TIME = 10.0;

// project value multiplier comparing to mineral cost
const double BASE_CAPTURE_GAIN_COEFFICIENT = 0.75;
const double PROJECT_VALUE_MULTIPLIER = 1.0;

// base defense weight for remote vehicles
const double BASE_DEFENSE_RANGE_AIR		= 10.0;
const double BASE_DEFENSE_RANGE_SEA		=  5.0;
const double BASE_DEFENSE_RANGE_LAND	=  3.0;

// combat type proportions
const double COMBAT_PROPORTION_PSI			= 0.2;
const double COMBAT_PROPORTION_CON			= 0.8;
// conventional air/surface attack proportion
const double ATTACK_PROPORTION_CON_AIR		= 0.5;
const double ATTACK_PROPORTION_CON_SURFACE	= 0.5;

// forward declarations

struct MapIntValue
{
	MAP *tile = nullptr;
	int value = INT_MAX;
	
	MapIntValue(MAP *_tile, int _value)
	: tile(_tile), value(_value)
	{}
	
};

struct MapDoubleValue
{
	MAP *tile = nullptr;
	double value = INF;
	
	MapDoubleValue(MAP *_tile, double _value)
	: tile(_tile), value(_value)
	{}
	
};

struct VehicleLocation
{
	int vehicleId;
	MAP *location;
};

struct VehicleLocationTravelTime
{
	int vehicleId;
	MAP *location;
	int travelTime;
};

/*
Information about defend economic effect and remaining health.
*/
struct VehicleDefendInfo
{
	double economicEffect;
	double relativeDamage;
};

const int BASE_DEFENSE_TYPE_COUNT = 4;
enum BASE_DEFENSE_TYPE
{
	BD_LAND,
	BD_SEA,
	BD_AIR,
	BD_NATIVE,
};

struct AttackAction
{
	MAP *position;
	MAP *target;
	double hastyCoefficient;
	
	AttackAction(MAP *_position, MAP *_target, double _hastyCoefficient)
	: position(_position), target(_target), hastyCoefficient(_hastyCoefficient)
	{}
	
};

struct ReachSearch
{
	robin_hood::unordered_flat_map<int, int> values;
	robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<int>> layers;
	
	void set(int key, int value)
	{
		robin_hood::unordered_flat_map<int, int>::iterator valueIterator = values.find(key);
		if (valueIterator == values.end())
		{
			values.insert({key, value});
			layers[value].insert(key);
		}
		else
		{
			int oldValue = valueIterator->second;
			if (value > oldValue)
			{
				valueIterator->second = value;
				layers.at(oldValue).erase(key);
				layers[value].insert(key);
			}
		}
	}
	
};

struct Threat
{
	std::array<std::array<double, 4>, ENGAGEMENT_MODE_COUNT> values = {};
	void maxUnitWeight(ENGAGEMENT_MODE engagementMode, int extendedTriad, double weight)
	{
		this->values.at(engagementMode).at(extendedTriad) = std::max(this->values.at(engagementMode).at(extendedTriad), weight);
	}
	void addUnitWeight(ENGAGEMENT_MODE engagementMode, int extendedTriad, double weight)
	{
		this->values.at(engagementMode).at(extendedTriad) += weight;
	}
	void setThreat(Threat threat)
	{
		this->values = threat.values;
	}
	void addThreat(Threat threat)
	{
		for (int engagementMode : {EM_MELEE, EM_ARTILLERY})
		{
			for (int extendedTriad = 0; extendedTriad < 4; extendedTriad++)
			{
				this->values.at(engagementMode).at(extendedTriad) += threat.values.at(engagementMode).at(extendedTriad);
			}
		}
	}
	double getCombinedValue(ENGAGEMENT_MODE engagementMode)
	{
		double combinedValue = 0.0;
		for (int extendedTriad = 0; extendedTriad < 4; extendedTriad++)
		{
			combinedValue += this->values.at(engagementMode).at(extendedTriad);
		}
		return combinedValue;
	}
};

void setPlayerFactionReferences(int factionId);
void aiFactionUpkeep(const int factionId);
void __cdecl modified_enemy_units_check(int factionId);
void strategy(bool computer);
void executeTasks();

void populateAIData();
void populateTileInfos();
void populateTileInfoBaseRanges();
void populateRegionAreas();
void populatePlayerBaseIds();
void populatePlayerBaseRanges();
void populateFactionInfos();
void populateBaseInfos();

void populatePlayerGlobalVariables();
void populatePlayerFactionIncome();
void populateUnits();
void populateMaxMineralSurplus();
void populateVehicles();
void populateDangerZones();
void populateArtifactDangerZones();
void populateEnemyStacks();
void populateEmptyEnemyBaseTiles();
void populateBasePoliceData();

void computeCombatEffects();

void populateEnemyBaseInfos();
void populateEnemyBaseCaptureGains();
void populateEnemyBaseProtectorWeights();
void populateEnemyBaseAssaultEffects();

void evaluateFactionMilitaryPowers();
void evaluateEnemyStacks();
void evaluateBaseDefense();
void evaluateDefense(MAP *tile, ProtectCombatData &combatData);
void evaluateBaseProbeDefense();

void designUnits();
void proposeMultiplePrototypes(int factionId, std::vector<VehChassis> chassisIds, std::vector<VehWeapon> weaponIds, std::vector<VehArmor> armorIds, std::vector<std::vector<VehAbl>> potentialAbilityIdSets, VehReactor reactor, VehPlan plan, char const *name);
void checkAndProposePrototype(int factionId, VehChassis chassisId, VehWeapon weaponId, VehArmor armorId, std::vector<VehAbl> abilityIdSet, VehReactor reactor, VehPlan plan, char const *name);
void obsoletePrototypes(int factionId, robin_hood::unordered_flat_set<int> chassisIds, robin_hood::unordered_flat_set<int> weaponIds, robin_hood::unordered_flat_set<int> armorIds, robin_hood::unordered_flat_set<int> abilityFlagsSet, robin_hood::unordered_flat_set<int> reactors);

// --------------------------------------------------
// helper functions
// --------------------------------------------------

int getVehicleIdByAIId(int aiId);
VEH *getVehicleByAIId(int aiId);
MAP *getClosestPod(int vehicleId);
int getNearestAIFactionBaseRange(int x, int y);
int getNearestBaseRange(MAP *tile, std::vector<int> baseIds, bool sameRealm);
int getFactionMaxConOffenseValue(int factionId);
int getFactionMaxConDefenseValue(int factionId);
std::array<int, 3> getFactionFastestTriadChassisIds(int factionId);
double evaluateCombatStrength(double offenseValue, double defenseValue);
double evaluateOffenseStrength(double offenseValue);
double evaluateDefenseStrength(double DefenseValue);
bool isVehicleThreatenedByEnemyInField(int vehicleId);
bool isUnitArtilleryTargetReachable(int unitId, MAP *origin, MAP *target);
bool isVehicleArtilleryTargetReachable(int vehicleId, MAP *target);
int getNearestEnemyBaseDistance(int baseId);
double getConventionalCombatBonusMultiplier(int attackerUnitId, int defenderUnitId);
char* getAbilitiesString(int unitType);
bool isWithinAlienArtilleryRange(int vehicleId);
bool isWtpEnabledFaction(int factionId);
int getMaxBaseSize(int factionId);
bool compareIdIntValueAscending(const IdIntValue &a, const IdIntValue &b);
bool compareIdIntValueDescending(const IdIntValue &a, const IdIntValue &b);
bool compareIdDoubleValueAscending(const IdDoubleValue &a, const IdDoubleValue &b);
bool compareIdDoubleValueDescending(const IdDoubleValue &a, const IdDoubleValue &b);
double getMeleeRelativeUnitStrength(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId);
double getArtilleryDuelRelativeUnitStrength(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId);
double getUnitBombardmentDamage(int attackerUnitId, int attackerFactionId, int defenderUnitId, int defenderFactionId);
double getVehicleBombardmentDamage(int attackerVehicleId, int defenderVehicleId);
double getVehiclePsiOffensePowerModifier(int vehicleId);
double getVehiclePsiDefenseModifier(int vehicleId);
double getVehicleConventionalOffenseModifier(int vehicleId);
double getVehicleConventionalDefenseModifier(int vehicleId);
bool isOffensiveUnit(int unitId, int factionId);
bool isOffensiveVehicle(int vehicleId);
double getExponentialCoefficient(double scale, double value);
int getBaseItemBuildTime(int baseId, int item, bool countAccumulatedMinerals = false);
double getStatisticalBaseSize(double age);
double getBaseEstimatedSize(double currentSize, double currentAge, double futureAge);
double getBaseStatisticalWorkerCount(double age);
double getBaseStatisticalProportionalWorkerCountIncrease(double currentAge, double futureAge);
int getBaseProjectedSize(int baseId, int turns);
int getBaseGrowthTime(int baseId, int targetSize);
double getResourceScore(double nutrient, double mineral, double energy);
double getResourceScore(double mineral, double energy);
int getBaseFoundingTurn(int baseId);
int getBaseAge(int baseId);
double getBaseSizeGrowth(int baseId);
double getBonusDelayEffectCoefficient(double delay);
double getBuildTimeEffectCoefficient(double delay);
int getUnitGroup(int unitId, bool airbase);
double getVehicleStrenghtMultiplier(int vehicleId);
double getVehicleBombardmentStrenghtMultiplier(int vehicleId);
double getHarmonicMean(std::vector<std::vector<double>> parameters);
robin_hood::unordered_flat_map<int, int> getVehicleReachableLocations(int vehicleId, bool ignoreVehicles = false);
robin_hood::unordered_flat_map<int, int> getAirVehicleReachableLocations(MAP *origin, int movementAllowance, bool ignoreVehicles);
robin_hood::unordered_flat_map<int, int> getSeaVehicleReachableLocations(MovementType movementType, MAP *origin, int movementAllowance, bool ignoreVehicles);
robin_hood::unordered_flat_map<int, int> getLandVehicleReachableLocations(MovementType movementType, MAP *origin, int movementAllowance, bool ignoreVehicles);
std::vector<AttackAction> getMeleeAttackActions(int vehicleId);
std::vector<AttackAction> getArtilleryAttackActions(int vehicleId);
robin_hood::unordered_flat_map<int, double> getMeleeAttackLocations(int vehicleId);
robin_hood::unordered_flat_set<int> getArtilleryAttackLocations(int vehicleId);
bool isVehicleAllowedMove(int vehicleId, MAP *from, MAP *to);
int getBestSeaCombatSpeed(int factionId);
int getBestSeaTransportSpeed(int factionId);
void disbandOrversupportedVehicles(int factionId);
void disbandUnneededVehicles();
bool isUnitCanCaptureBase(int unitId, MAP *baseTile);
bool isUnitCanCaptureBase(int unitId, MAP *origin, MAP *baseTile);
bool isVehicleCanCaptureBase(int vehicleId, MAP *baseTile);
double getDestroyingEnemyVehicleGain(int vehicleId);
int getCombatUnitTrueCost(int unitId);
int getCombatVehicleTrueCost(int vehicleId);
double getGain(double bonus, double income, double incomeGrowth, double incomeGrowth2);
double getGainDelay(double gain, double delay);
double getGainTimeInterval(double gain, double beginTime, double endTime);
double getGainRepetion(double gain, double probability, double period);
double getGainBonus(double bonus);
double getGainIncome(double income);
double getGainIncomeGrowth(double incomeGrowth);
double getGainIncomeGrowth2(double incomeGrowth2);
double getBaseIncome(int baseId, bool withNutrients = false);
double getAverageBaseIncome();
double getBaseCitizenIncome(int baseId);
double getMeanBaseIncome(double age);
double getMeanBaseGain(double age);
double getNewBaseGain();
double getLandColonyGain();
double getBaseGain(int popSize, int nutrientCostFactor, Resource baseIntake2);
double getBaseGain(int baseId, Resource baseIntake2);
double getBaseGain(int baseId);
double getBaseValue(int baseId);
double getBaseImprovementGain(int baseId, Resource oldBaseIntake2, Resource newBaseIntake2);
COMBAT_TYPE getWeaponType(int vehicleId);
COMBAT_TYPE getArmorType(int vehicleId);
CombatStrength getMeleeAttackCombatStrength(int vehicleId);
bool isVehicleCanMeleeAttackTile(int vehicleId, MAP *attackPosition, MAP *attackTarget);
bool isVehicleCanArtilleryAttack(int vehicleId);
void assignVehiclesToTransports();
bool isUnitCanMeleeAttackFromTileToTile(int unitId, MAP *position, MAP *target);
bool isUnitCanArtilleryAttackFromTile(int unitId, MAP *position);
bool isUnitCanMeleeAttackTile(int unitId, MAP *target, MAP *position = nullptr);
bool isVehicleCanMeleeAttackTile(int vehicleId, MAP *target, MAP *position = nullptr);
double getBaseExtraWorkerGain(int baseId);
double getOffenseEffect(int attackerVehicleId, int defenderVehicleId, COMBAT_MODE combatMode, MAP *tile);
AssaultEffect computeAssaultEffect(int assailantFactionId, int assailantUnitId, int protectorFactionId, int protectorUnitId);
bool isUnitCanMeleeAttackUnitAtTile(int attackerUnitId, int defenderUnitId, MAP *tile);
bool isUnitCanMeleeAttackUnitFromTile(int attackerUnitId, int defenderUnitId, MAP *tile);
bool isUnitCanArtilleryAttackUnitAtTile(int attackerUnitId, int defenderUnitId, MAP *tile);
bool isUnitCanArtilleryAttackUnitFromTile(int attackerUnitId, int defenderUnitId, MAP *tile);
bool isUnitCanMeleeAttackUnitAtBase(int attackerUnitId, int defenderUnitId);
bool isUnitCanMeleeAttackUnitFromBase(int attackerUnitId, int defenderUnitId);
bool isUnitCanArtilleryAttackUnitAtBase(int attackerUnitId, int defenderUnitId);
bool isUnitCanArtilleryAttackUnitFromBase(int attackerUnitId, int defenderUnitId);
MapDoubleValue getMeleeAttackPosition(int unitId, MAP *origin, MAP *target);
MapDoubleValue getMeleeAttackPosition(int vehicleId, MAP *target);
MapDoubleValue getArtilleryAttackPosition(int unitId, MAP *origin, MAP *target);
MapDoubleValue getArtilleryAttackPosition(int vehicleId, MAP *target);
double getWinningProbability(double combatEffect);
double getSurvivalEffect(double combatEffect);
double getCombatEffectCoefficient(double combatEffect);
double getSensorOffenseMultiplier(int factionId, MAP *tile);
double getSensorDefenseMultiplier(int factionId, MAP *tile);
double getUnitMeleeOffenseStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation);
double getUnitArtilleryOffenseStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation);
int getBasePoliceRequiredPower(int baseId);

