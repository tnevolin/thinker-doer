#pragma once

#include <sstream>
#include <iomanip>
#include <map>
#include "robin_hood.h"
#include "main.h"
#include "game.h"
#include "lib/tree.hh"

struct Profile
{
	std::string name;
	bool running = false;
	int executionCount = 0;
	clock_t startTime = 0;
	clock_t totalTime = 0;
	
	void start()
	{
		if (DEBUG)
		{
			if (running)
			{
				debug("ERROR: Profile::start. Profile is RUNNING: %s\n", name.c_str());flushlog();
				abort();
			}
			
			running = true;
			startTime = clock();
			
		}
	}
	void pause()
	{
		if (DEBUG)
		{
			if (!running)
			{
				debug("ERROR: Profile::pause. Profile is NOT RUNNING: %s\n", name.c_str());flushlog();
				abort();
			}
			
			running = false;
			totalTime += clock() - startTime;
			
		}
	}
	void resume()
	{
		if (DEBUG)
		{
			if (running)
			{
				debug("ERROR: Profile::resume. Profile is RUNNING: %s\n", name.c_str());flushlog();
				abort();
			}
			
			running = true;
			startTime = clock();
			
		}
	}
	void stop()
	{
		if (DEBUG)
		{
			if (!running)
			{
				debug("ERROR: Profile::stop. Profile is NOT RUNNING: %s\n", name.c_str());flushlog();
				abort();
			}
			
			running = false;
			executionCount++;
			totalTime += clock() - startTime;
			
		}
	}
	
};
struct ProfileName
{
	std::string name;
	Profile profile;
};
class Profiling
{
private:
	
	static int const NAME_LENGTH = 120;
	
	static tree<ProfileName> profiles;
	
	static Profile *getProfile(std::string name)
	{
		Profile *profile = nullptr;
		
		for (tree<ProfileName>::iterator iterator = profiles.begin(); iterator != profiles.end(); iterator++)
		{
			if (iterator->name == name)
			{
				profile = &(iterator->profile);
				break;
			}
		}
		if (profile == nullptr)
		{
			debug("ERROR: Profile::getProfile. Profile is NOT FOUND: %s\n", name.c_str());flushlog();
			abort();
		}
		
		return profile;
		
	}
	
	static Profile *addTopProfile(std::string name)
	{
		Profile *profile = nullptr;
		
		for (tree<ProfileName>::sibling_iterator iterator = profiles.begin(); iterator != profiles.end(); iterator++)
		{
			if (iterator->name == name)
			{
				profile = &(iterator->profile);
				break;
			}
		}
		
		if (profile == nullptr)
		{
			profile = &(profiles.insert(profiles.end(), {name, {}})->profile);
			profile->name = name;
		}
		
		return profile;
		
	}
	static Profile *addChildProfile(std::string name, std::string parentName)
	{
		// find parent iterator
		
		tree<ProfileName>::iterator parentIterator = nullptr;
		
		for (tree<ProfileName>::iterator iterator = profiles.begin(); iterator != profiles.end(); iterator++)
		{
			if (iterator->name == parentName)
			{
				parentIterator = iterator;
				break;
			}
		}
		assert(parentIterator != nullptr);
		
		// find child profile
		
		Profile *profile = nullptr;
		
		for (tree<ProfileName>::sibling_iterator iterator = profiles.begin(parentIterator); iterator != profiles.end(parentIterator); iterator++)
		{
			if (iterator->name == name)
			{
				profile = &(iterator->profile);
				break;
			}
		}
		
		if (profile == nullptr)
		{
			profile = &(profiles.append_child(parentIterator, {name, {}})->profile);
			profile->name = name;
		}
		
		return profile;
		
	}
	
public:
	
	static void reset()
	{
		profiles.clear();
	}
	static void start(std::string name)
	{
		if (DEBUG)
		{
			if (profiles.empty())
			{
				profiles.insert(profiles.end(), {"", {}});
			}
			
//			debug("Profiling(%s) - start\n", name.c_str());flushlog();
			Profile *profile = addTopProfile(name);
			profile->start();
			
		}
	}
	static void start(std::string name, std::string parentName)
	{
		if (DEBUG)
		{
			if (profiles.empty())
			{
				profiles.insert(profiles.end(), {"", {}});
			}
			
//			debug("Profiling(%s) - start\n", name.c_str());flushlog();
			Profile *profile = addChildProfile(name, parentName);
			profile->start();
			
		}
	}
	static void pause(std::string name)
	{
		if (DEBUG)
		{
//			debug("Profiling(%s) - pause\n", name.c_str());flushlog();
			Profile *profile = getProfile(name);
			profile->pause();
		}
	}
	static void resume(std::string name)
	{
		if (DEBUG)
		{
//			debug("Profiling(%s) - resume\n", name.c_str());flushlog();
			Profile *profile = getProfile(name);
			profile->resume();
		}
	}
	static void stop(std::string name)
	{
		if (DEBUG)
		{
//			debug("Profiling(%s) - stop\n", name.c_str());flushlog();
			Profile *profile = getProfile(name);
			profile->stop();
		}
	}
	
	static void print()
	{
		debug("executionProfiles\n");
		
		for (tree<ProfileName>::iterator iterator = profiles.begin(); iterator != profiles.end(); iterator++)
		{
			int depth = profiles.depth(iterator);
			std::string const name = iterator->name;
			Profile const &profile = iterator->profile;
			
			std::string prefixedName = std::string(4 * depth, ' ') + name;
			std::string displayName = prefixedName + " " + std::string(std::max(0, NAME_LENGTH - 1 - (int)prefixedName.length()), '.');
			int executionCount = profile.executionCount;
			double totalExecutionTime = (double)profile.totalTime / (double)CLOCKS_PER_SEC;
			double averageExecutionTime = (executionCount == 0 ? 0.0 : totalExecutionTime / (double)executionCount);
			
			debug
			(
				"\t%-*s"
				"   count=%8d"
				"   totalTime=%7.3f"
				"   averageTime=%10.6f"
				"\n"
				, NAME_LENGTH, displayName.c_str()
				, executionCount
				, totalExecutionTime
				, averageExecutionTime
			);
			
		}
		
		flushlog();
		
	}
	
};

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

__cdecl void wtp_mod_battle_compute(int attackerVehicleId, int defenderVehicleId, int *attackerStrengthPointer, int *defenderStrengthPointer, int combat_type);
__cdecl int wtp_mod_proto_cost(int chassisTypeId, int weaponTypeId, int armorTypeId, int abilities, int reactorTypeId);

double standard_combat_mechanics_calculate_attacker_winning_probability(double p, int attacker_hp, int defender_hp);
double binomial_koefficient(int n, int k);

int map_distance(int x1, int y1, int x2, int y2);
bool isWithinBaseRadius(int x1, int y1, int x2, int y2);
__cdecl int modified_artillery_damage(int attacker_strength, int defender_strength, int attacker_firepower);
int wtp_tech_level(int id);
int wtp_tech_cost(int fac, int tech);
__cdecl int sayBase(int buffer, int baseId);
__cdecl int wtp_mod_base_init(int factionId, int x, int y);
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
__cdecl void appendAbilityCostTextInWorkshop(int output_string_pointer, int input_string_pointer);
int __cdecl wtp_mod_battle_fight_2(int veh_id_atk, int offset, int tx, int ty, int table_offset, int option, int* def_id);
__cdecl int modifiedSocialWinDrawSocialCalculateSpriteOffset(int spriteIndex, int effectValue);
__cdecl int modifiedVehicleCargoForAirTransportUnload(int vehicleId);
__cdecl int modifiedFactionUpkeep(const int factionId);
__cdecl void captureAttackerOdds(const int position, const int value);
__cdecl void captureDefenderOdds(const int position, const int value);
__cdecl void modifiedDisplayOdds(const char* file_name, const char* label, int a3, const char* pcx_file_name, int a5);
double calculateWinningProbability(double p, int attackerHP, int defenderHP);
void simplifyOdds(int *attackerOdds, int *defenderOdds);
bool isTechDiscovered(int factionId, int techId);
bool isTechAvailable(int factionId, int techId);
__cdecl int modifiedBreakTreaty(int actingFactionId, int targetFactionId, int bitmask);
int __cdecl modifiedBaseMaking(int item, int baseId);
int __cdecl modifiedMindControlCost(int baseId, int probeFactionId, int cornerMarket);
int __cdecl getBasicAlternativeSubversionCost(int vehicleId);
int getBasicAlternativeSubversionCostWithHQDistance(int vehicleId, int hqDistance);
void __cdecl modifiedSubveredVehicleDrawTile(int probeVehicleId, int subvertedVehicleId, int radius);
int __cdecl modifiedProbe(int vehicleId, int a2, int a3, int a4);
int __cdecl modifiedTurnUpkeep();
int __thiscall wtp_mod_Console_destroy(Console *This, int a0);
int __cdecl wtp_mod_action_destroy(int vehicleId, int terrainBit, int x, int y);
int __cdecl modified_tech_value(int techId, int factionId, int flag);
int getFactionHighestResearchedTechLevel(int factionId);
int __cdecl modified_pact_withdraw(int factionId, int pactFactionId);
int __cdecl modified_propose_proto(int factionId, int chassisId, int weaponId, int armorId, int abilities_int, int reactorId, int plan, char const *name);
int __cdecl modified_base_production();
int getStockpileEnergy(int baseId);
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
int __cdecl modified_kill(int vehicleId);
void __cdecl wtp_mod_base_hurry();
void addAttackerBonus(int *strengthPointer, double bonus, const char *label);
void addDefenderBonus(int *strengthPointer, double bonus, const char *label);
__cdecl int modStatusWinBonus_bonus_at(int x, int y);
__cdecl int modified_load_game(int a0, int a1);
__cdecl int modified_zoc_veh(int a0, int a1, int a2);
__cdecl void modified_base_check_support();
void __cdecl displayHurryCostScaledToBasicMineralCostMultiplierInformation(int input_string_pointer, int output_string_pointer);
void __cdecl displayPartialHurryCostToCompleteNextTurnInformation(int input_string_pointer, int output_string_pointer);
int __thiscall wtp_BaseWin_popup_start(Win* This, const char* filename, const char* label, int a4, int a5, int a6, int a7);
int __cdecl wtp_BaseWin_ask_number(const char* label, int value, int a3);

int __cdecl wtp_mod_quick_zoc(int a0, int a1, int a2, int a3, int a4, int a5, int a6);
int __cdecl wtp_mod_zoc_any(int a0, int a1, int a2);
int __cdecl wtp_mod_zoc_veh(int a0, int a1, int a2);
int __cdecl wtp_mod_zoc_sea(int a0, int a1, int a2);
int __cdecl wtp_mod_zoc_move(int a0, int a1, int a2);
int __thiscall wtp_mod_zoc_path(Path *This, int a0, int a1, int a2);
int __thiscall wtp_mod_BaseWin_psych_row(Win* This, int horizontal_pos, int vertical_pos, int a4, int a5, int talents, int drones, int sdrones);
//int __thiscall wtp_mod_BaseWin_pop_click(Win* This, int clicked_specialist_index, int a2, int a3, int a4);
int __thiscall wtp_mod_BaseWin_pop_click_popup_start(Win* This, const char* filename, const char* label, int a4, int a5, int a6, int a7);
//char * __cdecl wtp_mod_BaseWin_pop_click_specialist_cycle_strcat(char *dst, char const *src);
//int __cdecl wtp_mod_BaseWin_pop_click_specialist_has_tech(int tech_id, int faction_id);
int __cdecl wtp_mod_base_police_pending();
int getHurryMineralCost(int mineralCost);
int __cdecl wpt_mod_Base_hurry_cost_factor_mineral_cost(int itemCost, int a1, int a2);
int __thiscall wtp_mod_Console_human_turn(Console *This);
int __cdecl wtp_mod_action_terraform_cause_friction(int a1, int a2, int a3);
int __cdecl wtp_mod_action_terraform_set_treaty(int a1, int a2, int a3, int a4);
int __cdecl wtp_mod_enemy_diplomacy(int factionId);
int __cdecl wtp_mod_probe_treaty_on(int faction1Id, int faction2Id, int treaty);
int __cdecl wtp_mod_enemy_move(int vehicleId);
int __cdecl wtp_mod_steal_energy(int baseId);
int __cdecl wtp_mod_diplomacy_caption_say_fac_special(int dst, int src, int factionId);
int wtp_mod_probe_success_rates_procure_research_data(int position, int morale, int risk, int baseId);
int wtp_mod_probe_veh_skip(int vehicleId);

