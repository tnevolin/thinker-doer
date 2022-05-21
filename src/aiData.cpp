#include "aiData.h"
#include "game_wtp.h"
#include <cstring>

/**
Shared AI Data.
*/

// global constants

const char *UNIT_TYPE_NAMES[] = {"Melee", "Land Artillery"};

// global variables

int aiFactionId = -1;
MFaction *aiMetaFaction;
Data aiData;

// CombatEffectTable

CombatEffectTable::CombatEffectTable()
{
	combatEffects = std::vector<std::vector<std::vector<CombatEffect>>>(2 * MaxProtoFactionNum, std::vector<std::vector<CombatEffect>>(MaxPlayerNum, std::vector<CombatEffect>(2 * MaxProtoFactionNum)));
}

CombatEffect *CombatEffectTable::getCombatEffect(int ownUnitId, int foeFactionId, int foeUnitId)
{
	return &(combatEffects.at(getUnitIndex(ownUnitId)).at(foeFactionId).at(getUnitIndex(foeUnitId)));
}

// FoeUnitWeightTable

FoeUnitWeightTable::FoeUnitWeightTable()
{
	foeUnitWeights = std::vector<std::vector<double>>(MaxPlayerNum, std::vector<double>(2 * MaxProtoFactionNum));
}

double FoeUnitWeightTable::getFoeUnitWeight(int factionId, int unitId)
{
	return foeUnitWeights.at(factionId).at(getUnitIndex(unitId));
}
void FoeUnitWeightTable::setFoeUnitWeight(int factionId, int unitId, double weight)
{
	foeUnitWeights.at(factionId).at(getUnitIndex(unitId)) = weight;
}
void FoeUnitWeightTable::addFoeUnitWeight(int factionId, int unitId, double weight)
{
	foeUnitWeights.at(factionId).at(getUnitIndex(unitId)) += weight;
}

// BaseInfo

BaseInfo::BaseInfo()
{
	for (unsigned int unitType = 0; unitType < UNIT_TYPE_COUNT; unitType++)
	{
		unitTypeInfos[unitType].unitTypeName = UNIT_TYPE_NAMES[unitType];
	}
	
}

void BaseInfo::clear()
{
	for (unsigned int unitType = 0; unitType < UNIT_TYPE_COUNT; unitType++)
	{
		unitTypeInfos[unitType].clear();
	}

}

double BaseInfo::getAverageCombatEffect(int unitId)
{
	return averageCombatEffects.at(getUnitIndex(unitId));
}

void BaseInfo::setAverageCombatEffect(int unitId, double averageCombatEffect)
{
	averageCombatEffects.at(getUnitIndex(unitId)) = averageCombatEffect;
}

double BaseInfo::getVehicleAverageCombatEffect(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// adjust average combat effect to morale
	
	return getAverageCombatEffect(vehicle->unit_id) * getVehicleMoraleModifier(vehicleId, false);
	
}

UnitTypeInfo *BaseInfo::getUnitTypeInfo(int unitId)
{
	return &(unitTypeInfos[getUnitType(unitId)]);
}

void BaseInfo::setUnitTypeComputedValues()
{
	debug("\t\tsetUnitTypeComputedValues\n");
	for (UnitTypeInfo &unitTypeInfo : unitTypeInfos)
	{
		unitTypeInfo.setComputedValues();
	}
	
}

double BaseInfo::getTotalProtectionDemand()
{
	double totalProtectionDemand = 0.0;
	
	for (UnitTypeInfo &unitTypeInfo : unitTypeInfos)
	{
		totalProtectionDemand += unitTypeInfo.protectionDemand;
	}
	
	return totalProtectionDemand;
	
}

bool BaseInfo::isProtectionRequestSatisfied()
{
	bool protectionRequestSatisfied = true;
	
	for (UnitTypeInfo &unitTypeInfo : unitTypeInfos)
	{
		if (!unitTypeInfo.isProtectionRequestSatisfied())
		{
			protectionRequestSatisfied = false;
			break;
		}
		
	}
	
	return protectionRequestSatisfied;
	
}

double BaseInfo::getRemainingProtectionRequest()
{
	double remainingProtectionRequest = 0.0;
	
	for (unsigned int unitType = 0; unitType < UNIT_TYPE_COUNT; unitType++)
	{
		UnitTypeInfo *unitTypeInfo = &(unitTypeInfos[unitType]);
		
		remainingProtectionRequest += unitTypeInfo->getRemainingProtectionRequest();
		
	}
	
	return remainingProtectionRequest;
	
}

void Data::setup()
{
	// setup data
	
	tileInfos.resize(*map_area_tiles, {});
//	baseInfos.resize(MaxBaseNum, {});
	
//	combatEffects.resize(MaxBaseNum * (2 * MaxProtoFactionNum) * MaxProtoNum);
//	combatEffects = new CombatEffect[MaxBaseNum * (2 * MaxProtoFactionNum) * MaxProtoNum]();
//	std::memset(combatEffects, 0, sizeof *combatEffects * (MaxBaseNum * (2 * MaxProtoFactionNum) * MaxProtoNum));
	
}

void Data::cleanup()
{
	// cleanup data
	
	tileInfos.clear();
//	baseInfos.clear();
	
//	combatEffects.clear();
//	delete[] combatEffects;
	
	// cleanup lists
	
	production.clear();
	baseIds.clear();
	baseTiles.clear();
	presenceRegions.clear();
	regionBaseIds.clear();
	regionBaseGroups.clear();
	vehicleIds.clear();
	activeCombatUnitIds.clear();
	combatVehicleIds.clear();
	scoutVehicleIds.clear();
	outsideCombatVehicleIds.clear();
	unitIds.clear();
	combatUnitIds.clear();
	landColonyUnitIds.clear();
	seaColonyUnitIds.clear();
	airColonyUnitIds.clear();
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

double Data::getAverageCombatEffect(int unitId)
{
	return averageCombatEffects.at(getUnitIndex(unitId));
}

void Data::setAverageCombatEffect(int unitId, double averageCombatEffect)
{
	averageCombatEffects.at(getUnitIndex(unitId)) = averageCombatEffect;
}

double Data::getVehicleAverageCombatEffect(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);
	
	// adjust average combat effect to morale
	
	return getAverageCombatEffect(vehicle->unit_id) * getVehicleMoraleModifier(vehicleId, false);
	
}

void UnitTypeInfo::clear()
{
	foeTotalWeight = 0.0;
	ownTotalWeight = 0.0;
	ownTotalCombatEffect = 0.0;
	requiredProtection = 0.0;
	providedProtection = 0.0;
	protectionLevel = 0.0;
	protectionDemand = 0.0;
	protectors.clear();
}

bool UnitTypeInfo::isProtectionRequestSatisfied()
{
	return providedProtection >= requiredProtection - 0.1;
}

double UnitTypeInfo::getRemainingProtectionRequest()
{
	return std::max(0.0, requiredProtection - providedProtection);
}

void UnitTypeInfo::setComputedValues()
{
	// calculate required protection
	
	requiredProtection = foeTotalWeight * conf.ai_production_defense_superiority_coefficient;
	
	// calculate protection level
	
	if (ownTotalCombatEffect > requiredProtection && ownTotalCombatEffect > 0.0)
	{
		protectionLevel = +1.0 - (requiredProtection / ownTotalCombatEffect);
	}
	else if (ownTotalCombatEffect < requiredProtection && requiredProtection > 0.0)
	{
		protectionLevel = -1.0 + (ownTotalCombatEffect / requiredProtection);
	}
	else
	{
		protectionLevel = 0.0;
	}
	
	// calculate defense demand
	
	protectionDemand = (protectionLevel >= 0.0 ? 0.0 : - protectionLevel);
	
	debug("\t\t\t%-15s ownTotalCombatEffect=%5.2f, requiredProtection=%+5.2f, protectionLevel=%+5.2f, protectionDemand=%5.2f\n", unitTypeName, ownTotalCombatEffect, requiredProtection, protectionLevel, protectionDemand);
	
	// sort protectors by priority
	
	std::sort(protectors.begin(), protectors.end(), compareIdDoubleValueDescending);
	
}

// helper functions

int getUnitType(int unitId)
{
	UNIT *unit = &(Units[unitId]);
	
	int unitType;
	
	if (unit->triad() == TRIAD_LAND && isUnitHasAbility(unitId, ABL_ARTILLERY))
	{
		unitType = UT_LAND_ARTILLERY;
	}
	else
	{
		unitType = UT_MELEE;
	}
	
	return unitType;
	
}

const char *getUnitTypeName(int unitType)
{
	return UNIT_TYPE_NAMES[unitType];
}

