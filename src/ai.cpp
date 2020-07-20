#include <float.h>
#include <math.h>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include "ai.h"
#include "wtp.h"
#include "game.h"
#include "aiTerraforming.h"

// global variables

// Controls which faction uses WtP terraforming algorithm.
// -1 : all factions

int wtpAIFactionId = -1;

// global variables for faction upkeep

int aiFactionId;

/*
AI strategy.
*/
void aiStrategy(int id)
{
	// no natives

	if (id == 0)
		return;

	// set faction

	aiFactionId = id;

	// use WTP algorithm for selected faction only

	if (wtpAIFactionId != -1 && aiFactionId != wtpAIFactionId)
		return;

	debug("aiStrategy: aiFactionId=%d\n", aiFactionId);

	// prepare former orders

	prepareFormerOrders();

}

