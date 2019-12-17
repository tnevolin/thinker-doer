#ifndef __PROTOTYPE_H__
#define __PROTOTYPE_H__

#include "main.h"
#include "game.h"

HOOK_API int proto_cost(int chassisTypeId, int weaponTypeId, int armorTypeId, int abilities, int reactorTypeId);
HOOK_API int upgrade_cost(int faction_id, int new_unit_id, int old_unit_id);

#endif // __PROTOTYPE_H__

