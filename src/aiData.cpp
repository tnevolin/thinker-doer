#include "aiData.h"
#include "game_wtp.h"

/**
Shared AI Data.
*/

// global variables

int aiFactionId = -1;
Data aiData;

void Data::setup()
{
	// setup data
	
	baseInfos.resize(MaxBaseNum, {});
	tileInfos.resize(*map_area_tiles, {});
	
}

void Data::cleanup()
{
	// cleanup data
	
	baseInfos.clear();
	tileInfos.clear();
	
	// cleanup lists
	
	production.clear();
	baseIds.clear();
	baseTiles.clear();
	presenceRegions.clear();
	regionBaseIds.clear();
	regionBaseGroups.clear();
	vehicleIds.clear();
	combatVehicleIds.clear();
	scoutVehicleIds.clear();
	outsideCombatVehicleIds.clear();
	unitIds.clear();
	combatUnitIds.clear();
	prototypeUnitIds.clear();
	colonyVehicleIds.clear();
	formerVehicleIds.clear();
	threatLevel = 0.0;
	regionSurfaceCombatVehicleIds.clear();
	regionSurfaceScoutVehicleIds.clear();
	baseAnticipatedNativeAttackStrengths.clear();
	baseRemainingNativeProtectionDemands.clear();
	regionDefenseDemand.clear();
	oceanAssociationMaxMineralSurpluses.clear();
	landCombatUnitIds.clear();
	seaCombatUnitIds.clear();
	airCombatUnitIds.clear();
	landAndAirCombatUnitIds.clear();
	seaAndAirCombatUnitIds.clear();
	seaTransportRequestCounts.clear();
	tasks.clear();
	
}

// access global data arrays

BaseInfo *Data::getBaseInfo(int baseId)
{
	assert(baseId >= 0 && baseId < *total_num_bases);
	return &(aiData.baseInfos.at(baseId));
}

TileInfo *Data::getTileInfo(int mapIndex)
{
	assert(mapIndex >= 0 && mapIndex < *map_area_tiles);
	return &(aiData.tileInfos.at(mapIndex));
}
TileInfo *Data::getTileInfo(int x, int y)
{
	return getTileInfo(getMapIndexByCoordinates(x, y));
}
TileInfo *Data::getTileInfo(MAP *tile)
{
	return getTileInfo(getMapIndexByPointer(tile));
}

// other methods

void UnitStrength::normalize(double totalWeight)
{
	this->psiOffense /= this->weight;
	this->psiDefense /= this->weight;
	this->conventionalOffense /= this->weight;
	this->conventionalDefense /= this->weight;
	this->weight /= totalWeight;
}

void MilitaryStrength::normalize()
{
	for (int unitId = 0; unitId < MaxProtoNum; unitId++)
	{
		UnitStrength *unitStrength = &(unitStrengths[unitId]);
		
		if (unitStrength->weight > 0.0)
		{
			populatedUnitIds.push_back(unitId);
			unitStrength->normalize(totalWeight);
		}
		
	}
	
}
	
