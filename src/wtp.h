#pragma once

#include "main.h"
#include "game.h"

struct BASE_INFO
{
	int id;
	BASE *base;
};

struct FACTION_EXTRA
{
	std::vector<int> baseIds;
	int projectBaseId = -1;
};

struct BASE_EXTRA
{
	int projectContribution = 0;
};

struct BASE_HURRY_ALLOWANCE_PROPORTION
{
	BASE *base;
	double allowanceProportion;
};

const int DEFENSIVE_FACILITIES_COUNT = 4;
const int DEFENSIVE_FACILITIES[] = {FAC_PERIMETER_DEFENSE, FAC_NAVAL_YARD, FAC_AEROSPACE_COMPLEX, FAC_TACHYON_FIELD};
const int HABITATION_FACILITIES_COUNT = 2;
const int HABITATION_FACILITIES[] = {FAC_HAB_COMPLEX, FAC_HABITATION_DOME, };

// player setup helper variable

extern int balanceFactionId;

HOOK_API int read_basic_rules();
HOOK_API void mod_battle_compute(int attacker_vehicle_id, int defender_vehicle_id, int attacker_strength_pointer, int defender_strength_pointer, int flags);
HOOK_API void mod_battle_compute_compose_value_percentage(int output_string_pointer, int input_string_pointer);
HOOK_API int proto_cost(int chassisTypeId, int weaponTypeId, int armorTypeId, int abilities, int reactorTypeId);
HOOK_API int upgrade_cost(int faction_id, int new_unit_id, int old_unit_id);

HOOK_API int combat_roll
(
    int attacker_strength,
    int defender_strength,
    int attacker_vehicle_offset,
    int defender_vehicle_offset,
    int attacker_initial_power,
    int defender_initial_power,
    int *attacker_won_last_round
)
;

int standard_combat_mechanics_combat_roll
(
    int attacker_strength,
    int defender_strength
)
;

int alternative_combat_mechanics_combat_roll
(
    int attacker_strength,
    int defender_strength,
    int attacker_vehicle_offset,
    int defender_vehicle_offset,
    int attacker_initial_power,
    int defender_initial_power,
    int *attacker_won_last_round
)
;

void alternative_combat_mechanics_probabilities
(
    int attacker_strength,
    int defender_strength,
    double *p, double *q, double *pA, double *qA, double *pD, double *qD
)
;

double standard_combat_mechanics_calculate_attacker_winning_probability
(
    double p,
    int attacker_hp,
    int defender_hp
)
;

double binomial_koefficient(int n, int k);

double alternative_combat_mechanics_calculate_attacker_winning_probability
(
    double p,
    int attacker_hp,
    int defender_hp
)
;

double alternative_combat_mechanics_calculate_attacker_winning_probability_following_rounds
(
    bool attacker_won,
    double pA, double qA, double pD, double qD,
    int attacker_hp,
    int defender_hp
)
;

//HOOK_API int base_find3(int x, int y, int unknown_1, int body_id, int unknown_2, int unknown_3);
int map_distance(int x1, int y1, int x2, int y2);
bool isWithinBaseRadius(int x1, int y1, int x2, int y2);
HOOK_API int roll_artillery_damage(int attacker_strength, int defender_strength, int attacker_firepower);
HOOK_API int se_accumulated_resource_adjustment(int a1, int a2, int faction_id, int a4, int a5);
HOOK_API int mod_hex_cost(int unit_id, int faction_id, int from_x, int from_y, int to_x, int to_y, int a7);
int wtp_tech_level(int id);
int wtp_tech_cost(int fac, int tech);
HOOK_API int sayBase(char *buffer, int baseId);
bool isBaseFacilityBuilt(int baseId, int facilityId);
bool isBaseFacilityBuilt(BASE *base, int facilityId);
int getBasePopulationLimit(BASE *base);
int getnextAvailableGrowthFacility(BASE *base);
HOOK_API int baseInit(int factionId, int x, int y);
HOOK_API char *getAbilityCostText(int number, char *destination, int radix);
HOOK_API int modifiedSocialCalc(int seSelectionsPointer, int seRatingsPointer, int factionId, int ignored4, int seChoiceEffectOnly);
HOOK_API void displayBaseNutrientCostFactor(int destinationStringPointer, int sourceStringPointer);
HOOK_API void correctGrowthTurnsIndicator(int destinationStringPointer, int sourceStringPointer);
HOOK_API int modifiedRecyclingTanksMinerals(int facilityId, int baseId, int queueSlotId);
HOOK_API int modifiedInefficiency(int energyIntake);
HOOK_API void modifiedSetupPlayer(int factionId, int a2, int a3);
HOOK_API void modifiedVehInitInBalance(int unitId, int factionId, int x, int y);
void createFreeVehicles(int factionId);
double getVehicleSpeedWithoutRoads(int id);
double getLandVehicleSpeedOnRoads(int id);
double getLandVehicleSpeedOnTubes(int id);
HOOK_API void modifiedWorldBuild();
HOOK_API int modifiedZocMoveToFriendlyUnitTileCheck(int x, int y);
int calculateNotPrototypedComponentsCost(int factionId, int chassisId, int weaponId, int armorId);
HOOK_API int calculateNotPrototypedComponentsCostForProduction(int unitId);
HOOK_API int calculateNotPrototypedComponentsCostForDesign(int chassisId, int weaponId, int armorId, int chassisPrototyped, int weaponPrototyped, int armorPrototyped);
HOOK_API int getActiveFactionMineralCostFactor();
HOOK_API void displayArtifactMineralContributionInformation(int input_string_pointer, int output_string_pointer);
HOOK_API int getCurrentBaseProductionMineralCost();
HOOK_API void displayHurryCostScaledToBasicMineralCostMultiplierInformation(int input_string_pointer, int output_string_pointer);
int scaleValueToBasicMinieralCostMultiplier(int factionId, int value);
HOOK_API void displayPartialHurryCostToCompleteNextTurnInformation(int input_string_pointer, int output_string_pointer);
int getPartialHurryCostToCompleteNextTurn();
HOOK_API int modifiedSpyingForPactBaseProductionDisplay(int factionId);
void expireInfiltrations(int factionId);
void setInfiltrationDeviceCount(int infiltratingFactionId, int infiltratedFactionId, int deviceCount);
int getInfiltrationDeviceCount(int infiltratingFactionId, int infiltratedFactionId);
HOOK_API void modifiedProbeActionRisk(int action, int riskPointer);
HOOK_API int modifiedInfiltrateDatalinksOptionTextGet();
HOOK_API void modifiedSetTreatyForInfiltrationExpiration(int initiatingFactionId, int targetFactionId, int diploState, int set_clear);
HOOK_API int modifiedHurryCost();
int getFlatHurryCost(int baseId);
int getPartialFlatHurryCost(int baseId, int minerals);
HOOK_API int modifiedFindReturnedProbeBase(int vehicleId);
HOOK_API int modifiedBestDefender(int defenderVehicleId, int attackerVehicleId, int bombardment);
HOOK_API void modifiedVehSkipForActionDestroy(int vehicleId);
HOOK_API void appendAbilityCostTextInWorkshop(int output_string_pointer, int input_string_pointer);
bool isInRangeOfFriendlySensor(int x, int y, int range, int factionId);
HOOK_API void modifiedBattleFight2(int attackerVehicleId, int angle, int tx, int ty, int do_arty, int flag1, int flag2);
HOOK_API int modifiedSocialWinDrawSocialCalculateSpriteOffset(int spriteIndex, int effectValue);
HOOK_API void modifiedTechResearch(int factionId, int labs);
HOOK_API void modifiedBaseGrowth(int a1, int a2, int a3);
HOOK_API int modifiedNutrientCostFactorSEGrowth(int factionId, int baseId);
int getHabitationFacilitiesBaseGrowthModifier(int baseId);
HOOK_API int modifiedActionTerraform(int vehicleId, int action, int execute);
HOOK_API void modifiedActionMoveForArtillery(int vehicleId, int x, int y);
HOOK_API int modifiedVehicleCargoForAirTransportUnload(int vehicleId);
HOOK_API int modifiedEnemyMove(const int vehicleId);
HOOK_API int modifiedFactionUpkeep(const int factionId);
HOOK_API void captureAttackerOdds(const int position, const int value);
HOOK_API void captureDefenderOdds(const int position, const int value);
HOOK_API void modifiedDisplayOdds(const char* file_name, const char* label, int a3, const char* pcx_file_name, int a5);
double calculateWinningProbability(double p, int attackerHP, int defenderHP);
void simplifyOdds(int *attackerOdds, int *defenderOdds);
HOOK_API int modifiedTechRate(int factionId);
HOOK_API int modifiedTechPick(int factionId, int a2, int a3, int a4);
int getLowestLevelAvilableTech(int factionId);
bool isTechDiscovered(int factionId, int techId);
bool isTechAvailable(int factionId, int techId);
HOOK_API void modifiedBattleReportItemNameDisplay(int destinationPointer, int sourcePointer);
HOOK_API void modifiedBattleReportItemValueDisplay(int destinationPointer, int sourcePointer);
HOOK_API void modifiedResetTerritory();
HOOK_API int modifiedOrbitalYieldLimit();
HOOK_API int modifiedBreakTreaty(int actingFactionId, int targetFactionId, int bitmask);
int __cdecl modifiedBaseMaking(int item, int baseId);
int __cdecl modifiedMindControlCost(int baseId, int probeFactionId, int cornerMarket);
int __cdecl getBasicAlternativeSubversionCost(int vehicleId);
int getBasicAlternativeSubversionCostWithHQDistance(int vehicleId, int hqDistance);
int __cdecl modifiedMindControlCaptureBase(int baseId, int faction, int probe);
void __cdecl modifiedSubveredVehicleDrawTile(int probeVehicleId, int subvertedVehicleId, int radius);
void __cdecl interceptBaseWinDrawSupport(int output_string_pointer, int input_string_pointer);

