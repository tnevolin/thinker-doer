#pragma once

#include "main.h"

extern int TechCostRatios[MaxDiffNum];

HOOK_API int mod_tech_rate(int faction);
HOOK_API int mod_tech_selection(int faction);
HOOK_API int mod_tech_value(int tech, int faction, int flag);

