#include "move.h"
#include "ai.h"

int aiFormerMove(int id)
{
    VEH* vehicle = &tx_vehicles[id];
    int vehicleFactionId = vehicle->faction_id;
    int vehicleTriad = veh_triad(id);
//    MAP* sq = mapsq(x, y);
//    bool safe = pm_safety[x][y] >= PM_SAFE;
    int item;

    // iterate through own base workable tiles reachable by former

    for (int mapTileIndex = 0; mapTileIndex < *tx_map_area; mapTileIndex++)
    {
        MAP *mapTile = &((*tx_map_ptr)[mapTileIndex]);

        // skip not own territory

        if (mapTile->owner != vehicleFactionId)
            continue;

    }

    return veh_skip(id);

}

