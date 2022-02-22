#include "aiMoveArtifact.h"
#include "game_wtp.h"
#include "aiData.h"
#include "aiMove.h"

void moveArtifactStrategy()
{
	// iterate artifacts
	
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &(Vehicles[vehicleId]);
		
		// artifact
		
		if (!isArtifactVehicle(vehicleId))
			continue;
		
		// find base to deliver artifact to (building project or closest)
		
		MAP *baseLocation = nullptr;
		int minRange = INT_MAX;
		
		for (int baseId : aiData.baseIds)
		{
			BASE *base = &(Bases[baseId]);
			
			// check base building project
			
			if(isBaseBuildingProject(baseId))
			{
				baseLocation = getBaseMapTile(baseId);
				break;
			}
			
			int range = map_range(vehicle->x, vehicle->y, base->x, base->y);
			
			if (range < minRange)
			{
				baseLocation = getBaseMapTile(baseId);
				minRange = range;
			}
			
		}
		
		// not found
		
		if (baseLocation == nullptr)
			continue;
		
		// deliver vehicle
		
		transitVehicle(vehicleId, Task(ARTIFACT_CONTRIBUTE, vehicleId, baseLocation));
		
	}
	
}

