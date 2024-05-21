#include "aiMoveArtifact.h"
#include "game_wtp.h"
#include "aiData.h"
#include "ai.h"
#include "aiRoute.h"
#include "aiMove.h"

void moveArtifactStrategy()
{
	debug("moveArtifactStrategy - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// iterate artifacts
	
	for (int vehicleId : aiData.vehicleIds)
	{
		MAP *vehicleLocation = getVehicleMapTile(vehicleId);
		
		// artifact
		
		if (!isArtifactVehicle(vehicleId))
			continue;
		
		debug("\t(%3d,%3d)\n", getX(vehicleLocation), getY(vehicleLocation));
		
		// escape warzone
		
		if (aiData.getTileInfo(vehicleLocation).warzone)
		{
			debug("\tescape warzone\n");
			
			// find safe location
			
			MAP *safeLocation = getSafeLocation(vehicleId);
			
			if (safeLocation == nullptr)
			{
				debug("\t\t\tsafe location is not found\n");
				continue;
			}
			
			debug("\t\t-> (%3d,%3d)\n", getX(safeLocation), getY(safeLocation));
			
			// add task
			
			transitVehicle(Task(vehicleId, TT_MOVE, safeLocation));
			
			continue;
			
		}
		
		debug("\tgo to base\n");
		
		// find base to deliver artifact to base building project
		
		MAP *closestProjectBaseLocation = nullptr;
		int closestProjectBaseTravelTime = INT_MAX;
		
		for (int baseId : aiData.baseIds)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			
			// building project
			
			if(!isBaseSetProductionProject(baseId))
				continue;
			
			// exclude base with blocked land location nearby
			
			bool blocked = false;
			
			for (MAP *baseRadiusTile : getBaseRadiusTiles(baseTile, false))
			{
				bool baseRadiusTileOcean = is_ocean(baseRadiusTile);
				TileInfo &baseRadiusTileInfo = aiData.getTileInfo(baseRadiusTile);
				
				// land
				
				if (baseRadiusTileOcean)
					continue;
				
				// blocked
				
				if (baseRadiusTileInfo.factionInfos[aiFactionId].blocked[0])
				{
					blocked = true;
					break;
				}
				
			}
			
			if (blocked)
				continue;
			
			// travelTime
			
			int travelTime = getVehicleATravelTime(vehicleId, baseTile);
			
			if (travelTime < closestProjectBaseTravelTime)
			{
				closestProjectBaseLocation = getBaseMapTile(baseId);
				closestProjectBaseTravelTime = travelTime;
			}
			
		}
		
		// found
		
		if (closestProjectBaseLocation != nullptr)
		{
			debug("\tmove to project base (%3d,%3d)\n", getX(closestProjectBaseLocation), getY(closestProjectBaseLocation));
			transitVehicle(Task(vehicleId, TT_ARTIFACT_CONTRIBUTE, closestProjectBaseLocation));
			continue;
		}
		
		// find closest base to store artifact
		
		if (isBaseAt(vehicleLocation))
		{
			debug("\tstay at this base (%3d,%3d)\n", getX(vehicleLocation), getY(vehicleLocation));
			setTask(Task(vehicleId, TT_HOLD));
			continue;
		}
		
		MAP *closestBaseLocation = nullptr;
		int closestBaseTravelTime = INT_MAX;
		
		for (int baseId : aiData.baseIds)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			
			// exclude base with blocked land location nearby
			
			bool blocked = false;
			
			for (MAP *baseRadiusTile : getBaseRadiusTiles(baseTile, false))
			{
				bool baseRadiusTileOcean = is_ocean(baseRadiusTile);
				TileInfo &baseRadiusTileInfo = aiData.getTileInfo(baseRadiusTile);
				
				// land
				
				if (baseRadiusTileOcean)
					continue;
				
				// blocked
				
				if (baseRadiusTileInfo.factionInfos[aiFactionId].blocked[0])
				{
					blocked = true;
					break;
				}
				
			}
			
			if (blocked)
				continue;
			
			// travelTime
			
			int travelTime = getVehicleATravelTime(vehicleId, baseTile);
			
			if (travelTime < closestBaseTravelTime)
			{
				closestBaseLocation = baseTile;
				closestBaseTravelTime = travelTime;
			}
			
		}
		
		// found
		
		if (closestBaseLocation != nullptr)
		{
			debug("\tmove to closest base (%3d,%3d)\n", getX(closestBaseLocation), getY(closestBaseLocation));
			transitVehicle(Task(vehicleId, TT_HOLD, closestBaseLocation));
			continue;
		}
		
	}
	
}

