#include "wtp_aiMoveArtifact.h"
#include "wtp_game.h"
#include "wtp_aiData.h"
#include "wtp_ai.h"
#include "wtp_aiRoute.h"
#include "wtp_aiMove.h"

void moveArtifactStrategy()
{
	Profiling::start("moveArtifactStrategy", "moveStrategy");
	
	debug("moveArtifactStrategy - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// iterate artifacts
	
	for (int vehicleId : aiData.vehicleIds)
	{
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// artifact
		
		if (!isArtifactVehicle(vehicleId))
			continue;
		
		debug("\t%s\n", getLocationString(vehicleTile));
		
		// escape danger zone
		
		if (aiData.unfriendlyEndangeredVehicleIds.find(vehicleId) != aiData.unfriendlyEndangeredVehicleIds.end() && aiData.getTileInfo(vehicleTile).unfriendlyDangerZone)
		{
			debug("\tescape danger zone\n");
			
			// find safe location
			
			MAP *safeLocation = getSafeLocation(vehicleId, true);
			if (safeLocation == nullptr)
			{
				debug("\t\t\tsafe location is not found\n");
				continue;
			}
			
			debug("\t\t-> %s\n", getLocationString(safeLocation));
			
			// add task
			
			transitVehicle(Task(vehicleId, TT_MOVE, safeLocation));
			
			continue;
			
		}
		
		debug("\tgo to base\n");
		
		// find base to deliver artifact to base building project
		
		MAP *closestProjectBaseLocation = nullptr;
		double closestProjectBaseTravelTime = DBL_MAX;
		
		for (int baseId : aiData.baseIds)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			
			// building project
			
			if(!isBaseBuildingProject(baseId))
				continue;
			
			// exclude base with blocked land location nearby
			
			bool blocked = false;
			
			for (MAP *baseRadiusTile : getBaseRadiusTiles(baseTile, false))
			{
				bool baseRadiusTileOcean = is_ocean(baseRadiusTile);
				
				// land
				
				if (baseRadiusTileOcean)
					continue;
				
				// blocked
				
				if (isBlocked(baseRadiusTile))
				{
					blocked = true;
					break;
				}
				
			}
			
			if (blocked)
				continue;
			
			// travelTime
			
			double travelTime = getVehicleTravelTime(vehicleId, baseTile);
			if (travelTime == INF)
				continue;
			
			if (travelTime < closestProjectBaseTravelTime)
			{
				closestProjectBaseLocation = getBaseMapTile(baseId);
				closestProjectBaseTravelTime = travelTime;
			}
			
		}
		
		// found
		
		if (closestProjectBaseLocation != nullptr)
		{
			debug("\tmove to project base %s\n", getLocationString(closestProjectBaseLocation));
			transitVehicle(Task(vehicleId, TT_ARTIFACT_CONTRIBUTE, closestProjectBaseLocation));
			continue;
		}
		
		// find closest base to store artifact
		
		if (isBaseAt(vehicleTile))
		{
			debug("\tstay at this base %s\n", getLocationString(vehicleTile));
			setTask(Task(vehicleId, TT_HOLD));
			continue;
		}
		
		MAP *closestBaseLocation = nullptr;
		double closestBaseTravelTime = DBL_MAX;
		
		for (int baseId : aiData.baseIds)
		{
			MAP *baseTile = getBaseMapTile(baseId);
			
			// exclude base with blocked land location nearby
			
			bool blocked = false;
			
			for (MAP *baseRadiusTile : getBaseRadiusTiles(baseTile, false))
			{
				bool baseRadiusTileOcean = is_ocean(baseRadiusTile);
				
				// land
				
				if (baseRadiusTileOcean)
					continue;
				
				// blocked
				
				if (isBlocked(baseRadiusTile))
				{
					blocked = true;
					break;
				}
				
			}
			
			if (blocked)
				continue;
			
			// travelTime
			
			double travelTime = getVehicleTravelTime(vehicleId, baseTile);
			if (travelTime == INF)
				continue;
			
			if (travelTime < closestBaseTravelTime)
			{
				closestBaseLocation = baseTile;
				closestBaseTravelTime = travelTime;
			}
			
		}
		
		// found
		
		if (closestBaseLocation != nullptr)
		{
			debug("\tmove to closest base %s\n", getLocationString(closestBaseLocation));
			transitVehicle(Task(vehicleId, TT_HOLD, closestBaseLocation));
			continue;
		}
		
	}
	
	Profiling::stop("moveArtifactStrategy");
	
}

