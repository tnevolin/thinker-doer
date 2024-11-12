#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include "wtp_aiData.h"
#include "engine.h"

const int MAX_SAFE_LOCATION_SEARCH_RANGE = 10;
const int STACK_MAX_BASE_RANGE = 20;
const int STACK_MAX_COUNT = 20;
const int MAX_PROXIMITY_RANGE = 4;

// estimated average land colony travel time
const double AVERAGE_LAND_COLONY_TRAVEL_TIME = 10.0;

// project value multiplier comparing to mineral cost
const double BASE_CAPTURE_GAIN_COEFFICIENT = 0.75;
const double PROJECT_VALUE_MULTIPLIER = 10.0;

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

struct MapValue
{
	MAP *tile = nullptr;
	double value = INF;

	MapValue(MAP *_tile, double _value);

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

class AverageAccumulator
{
private:
	double sumWeight;
	double sumWeightedValue;
public:
	void clear()
	{
		this->sumWeight = 0.0;
		this->sumWeightedValue = 0.0;
	}
	void add(double weight, double value)
	{
		assert(weight >= 0.0);
		sumWeight += weight;
		sumWeightedValue += weight * value;
	}
	void add(double value)
	{
		add(1.0, value);
	}
	double get()
	{
		return sumWeight == 0.0 ? 0.0 : sumWeightedValue / sumWeight;
	}
};

void setPlayerFactionReferences(int factionId);
void aiFactionUpkeep(const int factionId);
void __cdecl modified_enemy_units_check(int factionId);
void strategy();
void executeTasks();

void populateAIData();
void populateTileInfos();
void populateSeaRegionAreas();
void populatePlayerBaseIds();
void populateFactionInfos();
void populateBaseInfos();

void populatePlayerGlobalVariables();
void populatePlayerFactionIncome();
void populateUnits();
void populateMaxMineralSurplus();
void populateVehicles();
void populateWarzones();
void populateTileCombatData();
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

void designUnits();
void setUnitVariables();
void proposeMultiplePrototypes(int factionId, std::vector<int> chassisIds, std::vector<int> weaponIds, std::vector<int> armorIds, std::vector<std::vector<int>> abilitiesSets, int reactor, int plan, char *name);
void checkAndProposePrototype(int factionId, int chassisId, int weaponId, int armorId, std::vector<int> abilityIds, int reactor, int plan, char *name);
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
double evaluateCombatStrength(double offenseValue, double defenseValue);
double evaluateOffenseStrength(double offenseValue);
double evaluateDefenseStrength(double DefenseValue);
bool isVehicleThreatenedByEnemyInField(int vehicleId);
bool isUnitMeleeTargetReachable(int unitId, MAP *origin, MAP *target);
bool isVehicleMeleeTargetReachable(int vehicleId, MAP *target);
bool isUnitArtilleryTargetReachable(int unitId, MAP *origin, MAP *target);
bool isVehicleArtilleryTargetReachable(int vehicleId, MAP *target);
int getNearestEnemyBaseDistance(int baseId);
double getConventionalCombatBonusMultiplier(int attackerUnitId, int defenderUnitId);
std::string getAbilitiesString(int unitType);
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
bool isOffensiveUnit(int unitId);
bool isOffensiveVehicle(int vehicleId);
double getExponentialCoefficient(double scale, double time);
int getBaseItemBuildTime(int baseId, int item, bool accumulatedMinerals);
int getBaseItemBuildTime(int baseId, int item);
double getStatisticalBaseSize(double age);
double getBaseEstimatedSize(double currentSize, double currentAge, double futureAge);
double getBaseStatisticalWorkerCount(double age);
double getBaseStatisticalProportionalWorkerCountIncrease(double currentAge, double futureAge);
int getBaseProjectedSize(int baseId, int turns);
int getBaseGrowthTime(int baseId, int targetSize);
int getBaseOptimalDoctors(int baseId);
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
double getUnitMeleeAssaultEffect(int ownFactionId, int ownUnitId, int foeVehicleId, MAP *tile);
double getUnitArtilleryDuelAttackEffect(int ownFactionId, int ownUnitId, int foeVehicleId, MAP *tile);
double getUnitBombardmentAttackEffect(int ownFactionId, int ownUnitId, int foeVehicleId, MAP *tile);
double getHarmonicMean(std::vector<std::vector<double>> parameters);
bool compareMoveActions(MoveAction &o1, MoveAction &o2);
std::vector<MoveAction> getVehicleReachableLocations(int vehicleId);
std::vector<AttackAction> getMeleeAttackActions(int vehicleId);
std::vector<AttackAction> getMeleeAttackTargets(int vehicleId);
std::vector<AttackAction> getArtilleryAttackActions(int vehicleId);
std::vector<AttackAction> getArtilleryAttackTargets(int vehicleId);
double getVehicleTileDanger(int vehicleId, MAP *tile);
bool isVehicleAllowedMove(int vehicleId, MAP *from, MAP *to);
std::vector<MAP *> getVehicleThreatenedTiles(int vehicleId);
int getBestSeaCombatSpeed(int factionId);
int getBestSeaTransportSpeed(int factionId);
void disbandOrversupportedVehicles(int factionId);
void disbandUnneededVehicles();
MAP *getVehicleArtilleryAttackPosition(int vehicleId, MAP *target);
bool isUnitCanCaptureBase(int unitId, MAP *baseTile);
bool isUnitCanCaptureBase(int unitId, MAP *origin, MAP *baseTile);
bool isVehicleCanCaptureBase(int vehicleId, MAP *baseTile);
double getEnemyVehicleAttackGain(int vehicleId, int baseRange);
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
bool isVehicleCanArtilleryAttackTile(int vehicleId, MAP *target);
void assignVehiclesToTransports();
bool isUnitCanMeleeAttackUnit(int attackerUnitId, int defenderUnitId);
bool isUnitCanMeleeAttackFromTileToTile(int unitId, MAP *position, MAP *target);
bool isUnitCanArtilleryAttackFromTile(int unitId, MAP *position);
bool isUnitCanMeleeAttackTile(int unitId, MAP *target, MAP *position = nullptr);
bool isVehicleCanMeleeAttackTile(int vehicleId, MAP *target, MAP *position = nullptr);
double getBaseExtraWorkerGain(int baseId);
double getOffenseEffect(int attackerVehicleId, int defenderVehicleId, COMBAT_MODE combatMode, MAP *tile);
double getAssaultEffect(int assailantFactionId, int assailantUnitId, int protectorFactionId, int protectorUnitId, MAP *tile);
bool isUnitCanMeleeAttackUnitAtTile(int attackerUnitId, int defenderUnitId, MAP *tile);
bool isUnitCanMeleeAttackUnitFromTile(int attackerUnitId, int defenderUnitId, MAP *tile);
MAP *getMeleeAttackPosition(int unitId, MAP *origin, MAP *target);
MAP *getMeleeAttackPosition(int vehicleId, MAP *target);
MAP *getArtilleryAttackPosition(int unitId, MAP *origin, MAP *target);
MAP *getArtilleryAttackPosition(int vehicleId, MAP *target);
double getSurvivalChance(double effect);

