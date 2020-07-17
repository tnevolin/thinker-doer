#ifndef __PROTOTYPE_H__
#define __PROTOTYPE_H__

#include "main.h"
#include "game.h"

struct MAP_INFO
{
	int x;
	int y;
	MAP *tile;
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

HOOK_API int read_basic_rules();
HOOK_API void battle_compute(int attacker_vehicle_id, int defender_vehicle_id, int attacker_strength_pointer, int defender_strength_pointer, int flags);
HOOK_API void battle_compute_compose_value_percentage(int output_string_pointer, int input_string_pointer);
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

HOOK_API void calculate_odds
(
    int attacker_vehicle_offset,
    int defender_vehicle_offset,
    int attacker_weapon_strength,
    int defender_armor_strength,
    int *attacker_odd,
    int *defender_odd
)
;

double standard_combat_mechanics_calculate_attacker_winning_probability
(
    int attacker_strength,
    int defender_strength,
    int attacker_power,
    int defender_power
)
;

double binomial_koefficient(int n, int k);

double alternative_combat_mechanics_calculate_attacker_winning_probability
(
    int attacker_strength,
    int defender_strength,
    int attacker_power,
    int defender_power
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

HOOK_API int calculate_distance_to_nearest_base(int x, int y, int unknown_1, int body_id, int unknown_2, int unknown_3);
int map_distance(int x1, int y1, int x2, int y2);
bool isWithinBaseRadius(int x1, int y1, int x2, int y2);
HOOK_API int roll_artillery_damage(int attacker_strength, int defender_strength, int attacker_firepower);
HOOK_API int mod_nutrient_yield(int faction_id, int a2, int x, int y, int a5);
HOOK_API int mod_mineral_yield(int faction_id, int a2, int x, int y, int a5);
HOOK_API int mod_energy_yield(int faction_id, int a2, int x, int y, int a5);
HOOK_API int se_accumulated_resource_adjustment(int a1, int a2, int faction_id, int a4, int a5);
HOOK_API int hex_cost(int unit_id, int faction_id, int from_x, int from_y, int to_x, int to_y, int a7);
int wtp_tech_level(int id);
int wtp_tech_cost(int fac, int tech);
HOOK_API int sayBase(char *buffer, int baseId);
bool isBaseFacilityBuilt(BASE *base, int facilityId);
int getBasePopulationLimit(BASE *base);
int refitToGrowthFacility(int id, BASE *base, int buildChoice);
int getnextAvailableGrowthFacility(BASE *base);
HOOK_API int baseInit(int factionId, int x, int y);
HOOK_API char *getAbilityCostText(int number, char *destination, int radix);
HOOK_API int modifiedSocialCalc(int seSelectionsPointer, int seRatingsPointer, int factionId, int ignored4, int seChoiceEffectOnly);
HOOK_API void correctGrowthTurnsForPopulationBoom(int destinationStringPointer, int sourceStringPointer);
MAP *getMapTile(int x, int y);
HOOK_API int modifiedRecyclingTanksMinerals(int facilityId, int baseId, int queueSlotId);

#endif // __PROTOTYPE_H__

