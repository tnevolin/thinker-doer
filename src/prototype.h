#ifndef __PROTOTYPE_H__
#define __PROTOTYPE_H__

#include "main.h"
#include "game.h"

HOOK_API void read_reactor_cost_factor();
HOOK_API int proto_cost(int chassisTypeId, int weaponTypeId, int armorTypeId, int abilities, int reactorTypeId);

#endif // __PROTOTYPE_H__




