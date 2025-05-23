#pragma once

#include "main.h"

extern MAP *baseComputeDisabledTile;

const uint32_t GOV_ALLOW_COMBAT =
    (GOV_MAY_PROD_LAND_COMBAT | GOV_MAY_PROD_NAVAL_COMBAT | GOV_MAY_PROD_AIR_COMBAT);

struct BaseYield
{
	int worked_tiles		= 0;
	int specialist_total	= 0;
	int specialist_adjust	= 0;
	int nutrient			= 0;
	int mineral				= 0;
	int energy				= 0;
};
struct BaseEnergy
{
	int our_rank;
	std::array<int, MaxPlayerNum> otherFaciontRanks;
	int coeff_psych;
};

void __cdecl mod_base_reset(int base_id, bool has_gov);
int __cdecl mod_base_build(int base_id, bool has_gov);
void __cdecl base_first(int base_id);
void __cdecl set_base(int base_id);
void __cdecl base_compute(bool update_prev);
void __cdecl mod_base_support();
void __cdecl mod_base_yield();
void __cdecl wtp_mod_base_yield();
void __cdecl mod_base_nutrient();
void __cdecl mod_base_minerals();
void __cdecl mod_base_energy();
void mod_base_yield_base_energy(BaseEnergy const &baseEnergy, int energyIntake);
void __cdecl mod_base_psych(int base_id);
void __cdecl mod_base_research();
int __cdecl mod_base_production();
int __cdecl mod_base_growth();
void __cdecl mod_base_drones();
void __cdecl mod_base_maint();
int __cdecl mod_base_upkeep(int base_id);
int __cdecl mod_base_kill(int base_id);
int __cdecl mod_capture_base(int base_id, int faction, int is_probe);
int __cdecl base_psych_content_pop();
void __cdecl mod_psych_check(int faction_id, int32_t* content_pop, int32_t* base_limit);

char* prod_name(int item_id);
int prod_turns(int base_id, int item_id);
int mineral_cost(int base_id, int item_id);
int hurry_cost(int base_id, int item_id, int hurry_mins);
int base_unused_space(int base_id);
int base_growth_goal(int base_id);
int stockpile_energy(int base_id);
int stockpile_energy_active(int base_id);
int terraform_eco_damage(int base_id);
int mineral_output_modifier(int base_id);
int energy_grid_output(int base_id);
int satellite_output(int satellites, int pop_size, bool full_value);
bool satellite_bonus(int base_id, int* nutrient, int* mineral, int* energy);
int __cdecl own_base_rank(int base_id);
int __cdecl mod_base_rank(int faction_id, int position);
int __cdecl best_specialist(BASE* base, int econ_val, int labs_val, int psych_val);
int getBestSpecialist(int factionId, int basePopSize, double econValue, double labsValue, double psychValue);
int __cdecl mod_best_specialist();
int __cdecl mod_facility_avail(FacilityId item_id, int faction_id, int base_id, int queue_count);
bool can_build(int base_id, int item_id);
bool can_build_unit(int base_id, int unit_id);
bool can_staple(int base_id);
bool base_maybe_riot(int base_id);
bool base_can_riot(int base_id, bool allow_staple);
bool base_pop_boom(int base_id);
bool can_use_teleport(int base_id);
bool can_link_artifact(int base_id);
bool has_facility(FacilityId item_id, int base_id);
bool has_free_facility(FacilityId item_id, int faction_id);
int __cdecl redundant(FacilityId item_id, int faction_id);
int __cdecl has_fac(FacilityId item_id, int base_id, int queue_count);
int __cdecl has_fac_built(FacilityId item_id, int base_id);
void __cdecl set_fac(FacilityId item_id, int base_id, bool add);
int __cdecl fac_maint(int facility_id, int faction_id);
int __cdecl mod_drone_riot();
void clear_stack(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int, int);
int __cdecl mod_popb(char const *label, int flags, int sound_id, char const *pcx_filename, int a5);
int findReplacementSpecialist(int factionId, int specialistId);
double computeBaseTileScore(bool restrictions, double growthFactor, double energyValue, bool can_grow, int nutrientSurplus, int mineralSurplus, int energySurplus, int tileValueNutrient, int tileValueMineral, int tileValueEnergy);
void clearBaseComputeValues();
void precomputeBaseComputeValues(int factionId);
int getBasePsychCoefficient(int base_id);

