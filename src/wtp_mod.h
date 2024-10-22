#pragma once

#include <map>
#include "robin_hood.h"
#include "main.h"
#include "game.h"

struct Profile
{
	std::string name;
	int executionCount = 0;
	clock_t startTime = 0;
	clock_t totalTime = 0;
	void start();
	void pause();
	void resume();
	void stop();
	int getCount();
	double getTime();
};
extern std::map<std::string, Profile> executionProfiles;

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

__cdecl void wtp_mod_battle_compute(int attacker_vehicle_id, int defender_vehicle_id, int attacker_strength_pointer, int defender_strength_pointer, int flags);
__cdecl void wtp_mod_battle_compute_compose_value_percentage(int output_string_pointer, int input_string_pointer);
__cdecl int wtp_mod_proto_cost(int chassisTypeId, int weaponTypeId, int armorTypeId, int abilities, int reactorTypeId);

__cdecl int combat_roll(int attacker_strength, int defender_strength, int attacker_vehicle_offset, int defender_vehicle_offset, int attacker_initial_power, int defender_initial_power, int *attacker_won_last_round);
int standard_combat_mechanics_combat_roll(int attacker_strength, int defender_strength);
double standard_combat_mechanics_calculate_attacker_winning_probability(double p, int attacker_hp, int defender_hp);
double binomial_koefficient(int n, int k);

int map_distance(int x1, int y1, int x2, int y2);
bool isWithinBaseRadius(int x1, int y1, int x2, int y2);
__cdecl int modified_artillery_damage(int attacker_strength, int defender_strength, int attacker_firepower);
__cdecl int se_accumulated_resource_adjustment(int a1, int a2, int faction_id, int a4, int a5);
__cdecl int wtp_mod_hex_cost(int unit_id, int faction_id, int from_x, int from_y, int to_x, int to_y, int a7);
int wtp_tech_level(int id);
int wtp_tech_cost(int fac, int tech);
__cdecl int sayBase(char *buffer, int baseId);
__cdecl int baseInit(int factionId, int x, int y);
__cdecl char *getAbilityCostText(int number, char *destination, int radix);
__cdecl int modifiedSocialCalc(int seSelectionsPointer, int seRatingsPointer, int factionId, int ignored4, int seChoiceEffectOnly);
__cdecl void displayBaseNutrientCostFactor(int destinationStringPointer, int sourceStringPointer);
__cdecl void correctGrowthTurnsIndicator(int destinationStringPointer, int sourceStringPointer);
void createFreeVehicles(int factionId);
int getLandUnitSpeedOnRoads(int unitId);
int getLandUnitSpeedOnTubes(int unitId);
int calculateNotPrototypedComponentsCost(int chassisId, int weaponId, int armorId, int chassisPrototyped, int weaponPrototyped, int armorPrototyped);
int calculateNotPrototypedComponentsCost(int factionId, int chassisId, int weaponId, int armorId);
int calculateNotPrototypedComponentsCost(int unitId);
__cdecl int calculateNotPrototypedComponentsCostForDesign(int chassisId, int weaponId, int armorId, int chassisPrototyped, int weaponPrototyped, int armorPrototyped);
__cdecl int getActiveFactionMineralCostFactor();
__cdecl void displayArtifactMineralContributionInformation(int input_string_pointer, int output_string_pointer);
__cdecl int getCurrentBaseProductionMineralCost();
int scaleValueToBasicMinieralCostMultiplier(int factionId, int value);
__cdecl int modifiedSpyingForPactBaseProductionDisplay(int factionId);
__cdecl void modifiedProbeActionRisk(int action, int riskPointer);
__cdecl int modifiedBestDefender(int defenderVehicleId, int attackerVehicleId, int bombardment);
__cdecl void modifiedVehSkipForActionDestroy(int vehicleId);
__cdecl void appendAbilityCostTextInWorkshop(int output_string_pointer, int input_string_pointer);
__cdecl void modifiedBattleFight2(int attackerVehicleId, int angle, int tx, int ty, int do_arty, int flag1, int flag2);
__cdecl int modifiedSocialWinDrawSocialCalculateSpriteOffset(int spriteIndex, int effectValue);
__cdecl void modified_tech_research(int factionId, int labs);
__cdecl void modifiedBaseGrowth(int a1, int a2, int a3);
__cdecl int modifiedNutrientCostFactorSEGrowth(int factionId, int baseId);
int getHabitationFacilitiesBaseGrowthModifier(int baseId);
__cdecl int modifiedActionTerraform(int vehicleId, int action, int execute);
__cdecl void modifiedActionMoveForArtillery(int vehicleId, int x, int y);
__cdecl int modifiedVehicleCargoForAirTransportUnload(int vehicleId);
__cdecl int modifiedFactionUpkeep(const int factionId);
__cdecl void captureAttackerOdds(const int position, const int value);
__cdecl void captureDefenderOdds(const int position, const int value);
__cdecl void modifiedDisplayOdds(const char* file_name, const char* label, int a3, const char* pcx_file_name, int a5);
double calculateWinningProbability(double p, int attackerHP, int defenderHP);
void simplifyOdds(int *attackerOdds, int *defenderOdds);
bool isTechDiscovered(int factionId, int techId);
bool isTechAvailable(int factionId, int techId);
__cdecl void modifiedBattleReportItemNameDisplay(int destinationPointer, int sourcePointer);
__cdecl void modifiedBattleReportItemValueDisplay(int destinationPointer, int sourcePointer);
__cdecl void modifiedResetTerritory();
__cdecl int modifiedOrbitalYieldLimit();
__cdecl int modifiedBreakTreaty(int actingFactionId, int targetFactionId, int bitmask);
int __cdecl modifiedBaseMaking(int item, int baseId);
int __cdecl modifiedMindControlCost(int baseId, int probeFactionId, int cornerMarket);
int __cdecl getBasicAlternativeSubversionCost(int vehicleId);
int getBasicAlternativeSubversionCostWithHQDistance(int vehicleId, int hqDistance);
int __cdecl modifiedMindControlCaptureBase(int baseId, int faction, int probe);
void __cdecl modifiedSubveredVehicleDrawTile(int probeVehicleId, int subvertedVehicleId, int radius);
void __cdecl interceptBaseWinDrawSupport(int output_string_pointer, int input_string_pointer);
int __cdecl modifiedTurnUpkeep();
int __cdecl isDestroyableImprovement(int terraformIndex, int items);
int __cdecl modified_tech_value(int techId, int factionId, int flag);
int getFactionHighestResearchedTechLevel(int factionId);
int __cdecl modified_pact_withdraw(int factionId, int pactFactionId);
int __cdecl modified_propose_proto(int factionId, int chassisId, int weaponId, int armorId, int abilities, int reactorId, int plan, char *name);
int __cdecl modified_base_double_labs(int labs);
int __cdecl modified_base_production();
int __cdecl modified_base_ecology();
int getStockpileEnergy(int baseId);
void __cdecl modified_base_yield();
int __cdecl wtp_mod_order_veh(int vehicleId, int angle, int a3);
bool isValidMovementAngle(int vehicleId, int angle);
bool isAdjacentTransportAtSea(int vehicleId, int angle);
void fixVehicleHomeBases(int factionId);
void __cdecl modified_vehicle_range_boom(int x, int y, int flags);
void __cdecl modified_stack_veh_disable_transport_pick_everybody(int vehicleId, int flag);
void __cdecl modified_veh_skip_disable_non_transport_stop_in_base(int vehicleId);
void __cdecl modified_alien_move(int vehicleId);
int __cdecl modified_can_arty_in_alien_move(int unitId, bool allowSeaArty);
void removeWrongVehiclesFromBases();
void __cdecl modified_kill(int vehicleId);
__cdecl void modified_text_get_for_tech_steal_1();
__cdecl void modified_text_get_for_tech_steal_2();
void endOfTurn();
void __cdecl wtp_mod_base_hurry();
void addAttackerBonus(int strengthPointer, int bonus, const char *label);
void addDefenderBonus(int strengthPointer, int bonus, const char *label);
__cdecl int modStatusWinBonus_bonus_at(int x, int y);
__cdecl int modified_load_game(int a0, int a1);
__cdecl int modified_zoc_veh(int a0, int a1, int a2);
__cdecl void modified_base_check_support();
void __cdecl wtp_mod_base_support();
void __cdecl displayHurryCostScaledToBasicMineralCostMultiplierInformation(int input_string_pointer, int output_string_pointer);
void __cdecl displayPartialHurryCostToCompleteNextTurnInformation(int input_string_pointer, int output_string_pointer);
int __cdecl getHurryMinimalMineralCost(int itemCost, int a1, int a2);
int getHurryMineralCost(int mineralCost);
int __thiscall wtp_BaseWin_popup_start(Win* This, const char* filename, const char* label, int a4, int a5, int a6, int a7);
int __cdecl wtp_BaseWin_ask_number(const char* label, int value, int a3);

int __cdecl wtp_mod_quick_zoc(int a0, int a1, int a2, int a3, int a4, int a5, int a6);
int __cdecl wtp_mod_zoc_any(int a0, int a1, int a2);
int __cdecl wtp_mod_zoc_veh(int a0, int a1, int a2);
int __cdecl wtp_mod_zoc_sea(int a0, int a1, int a2);
int __cdecl wtp_mod_zoc_move(int a0, int a1, int a2);
int __thiscall wtp_mod_zoc_path(Path *This, int a0, int a1, int a2);

