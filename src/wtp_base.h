#pragma once

#include "main.h"
#include "wtp_mod.h"
#include "robin_hood.h"

extern MAP *baseComputeExcludedTile;

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

void __cdecl wtp_mod_base_yield();
void mod_base_yield_base_energy(BaseEnergy const &baseEnergy, int energyIntake);
size_t populateBaseYieldsAndTiles(int baseId, std::array<TileValue, 21> &tiles);
void base_update_reset(BASE* base, int Ns, int Ms, int Es, std::array<TileValue, 21> &tiles, int const tileCount);
void wtp_normalize_happiness(BASE *base, bool subtractSpecialists = false);
void wtp_add_psych_row(BASE *base, int num);
void __cdecl wtp_mod_base_psych(int base_id);
int getBestSpecialist(int factionId, int basePopSize, double econValue, double labsValue, double psychValue);
bool wtp_base_pop_boom(int base_id);
int wtp_flat_hurry_cost(int base_id, int item_id, int hurry_mins);
int wtp_terraform_eco_damage(int base_id);
int findReplacementSpecialist(int factionId, int specialistId);

