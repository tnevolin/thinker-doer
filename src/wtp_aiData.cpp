#include <cstring>
#include "wtp_game.h"
#include "wtp_aiData.h"
#include "wtp_ai.h"

/**
Shared AI Data.
*/

// global variables

int aiFactionId = -1;
Data aiData;

// Force

int getCombatValueNature(int combatValue)
{
	return (combatValue < 0 ? NAT_PSI : NAT_CON);
}

// CombatEffectTable

void CombatEffectTable::setCombatEffect(int ownUnitId, int foeFactionId, int foeUnitId, int attackingSide, COMBAT_TYPE combatType, double combatEffect)
{
	int index =
		+ getUnitSlotById(ownUnitId)	* (MaxPlayerNum)	* (2 * MaxProtoFactionNum)	* 2	* COMBAT_TYPE_COUNT
		+ foeFactionId										* (2 * MaxProtoFactionNum)	* 2	* COMBAT_TYPE_COUNT
		+ getUnitSlotById(foeUnitId)													* 2	* COMBAT_TYPE_COUNT
		+ attackingSide																		* COMBAT_TYPE_COUNT
		+ combatType
	;

	combatEffects.at(index) = combatEffect;

}
double CombatEffectTable::getCombatEffect(int ownUnitId, int foeFactionId, int foeUnitId, int attackingSide, COMBAT_TYPE combatType)
{
	int index =
		+ getUnitSlotById(ownUnitId)	* (MaxPlayerNum)	* (2 * MaxProtoFactionNum)	* 2	* COMBAT_TYPE_COUNT
		+ foeFactionId										* (2 * MaxProtoFactionNum)	* 2	* COMBAT_TYPE_COUNT
		+ getUnitSlotById(foeUnitId)													* 2	* COMBAT_TYPE_COUNT
		+ attackingSide																		* COMBAT_TYPE_COUNT
		+ combatType
	;

	return combatEffects.at(index);

}

// BaseCombatData

void BaseCombatData::clear()
{
	requiredEffect = 0.0;
	desiredEffect = 0.0;
	providedEffect = 0.0;
	currentProvidedEffect = 0.0;
}
void BaseCombatData::setUnitEffect(int unitId, double effect)
{
	unitEffects.at(getUnitSlotById(unitId)) = effect;
}
double BaseCombatData::getUnitEffect(int unitId)
{
	return unitEffects.at(getUnitSlotById(unitId));
}
double BaseCombatData::getVehicleEffect(int vehicleId)
{
	return getUnitEffect(getVehicle(vehicleId)->unit_id) * getVehicleStrenghtMultiplier(vehicleId);
}
void BaseCombatData::addProvidedEffect(int vehicleId, bool current)
{
	double vehicleEffect = getVehicleEffect(vehicleId);

	providedEffect += vehicleEffect;
	if (current)
	{
		currentProvidedEffect += vehicleEffect;
	}

}
bool BaseCombatData::isSatisfied(bool current)
{
	return getDemand(current) <= 0.0;
}
double BaseCombatData::getDemand(bool current)
{
	double required = requiredEffect;
	double provided = (current ? currentProvidedEffect : providedEffect);
	return ((required > 0.0 && provided < required) ? 1.0 - provided / required : 0.0);
}

// Force

Force::Force(int _vehicleId, ATTACK_TYPE _attackType, double _hastyCoefficient)
{
	this->vehiclePad0 = getVehicle(_vehicleId)->pad_0;
	this->attackType = _attackType;
	this->hastyCoefficient = _hastyCoefficient;
}

int Force::getVehicleId()
{
	return getVehicleIdByAIId(vehiclePad0);
}

// FoeUnitWeightTable

void FoeUnitWeightTable::clear()
{
	std::fill(weights.begin(), weights.end(), 0.0);
}
double FoeUnitWeightTable::get(int factionId, int unitId)
{
	return weights.at((2 * MaxProtoFactionNum) * factionId + getUnitSlotById(unitId));
}
void FoeUnitWeightTable::set(int factionId, int unitId, double weight)
{
	weights.at((2 * MaxProtoFactionNum) * factionId + getUnitSlotById(unitId)) = weight;
}
void FoeUnitWeightTable::add(int factionId, int unitId, double weight)
{
	weights.at((2 * MaxProtoFactionNum) * factionId + getUnitSlotById(unitId)) += weight;
}

// Transfer

Transfer::Transfer(MAP *_passengerStop, MAP *_transportStop)
: passengerStop{_passengerStop}, transportStop{_transportStop}
{

}

bool Transfer::isValid()
{
	return passengerStop != nullptr && transportStop != nullptr;
}

// PoliceData

void PoliceData::clear()
{
	unitsAllowed = 0;
	unitsProvided = 0;
	dronesExisting = 0;
	dronesQuelled = 0;
}

void PoliceData::addVehicle(int vehicleId)
{
	unitsProvided++;
	dronesQuelled += (effect + (isVehicleHasAbility(vehicleId, ABL_POLICE_2X) ? 1 : 0));
}

bool PoliceData::isSatisfied()
{
	return (unitsProvided >= unitsAllowed || dronesQuelled >= dronesExisting);
}

// Production

void Production::clear()
{
	unavailableBuildSites.clear();
	seaColonyDemands.clear();
	seaFormerDemands.clear();
	seaPodPoppingDemands.clear();
	landTerraformingRequestCounts.clear();
	seaTerraformingRequestCounts.clear();
}

void Production::addTerraformingRequest(MAP *tile)
{
	bool tileOcean = is_ocean(tile);
	int tileLandAssociation = getLandAssociation(tile, aiFactionId);
	int tileOceanAssociation = getOceanAssociation(tile, aiFactionId);

	debug
	(
		"addTerraformingRequest tileOcean=%d [%3d] (%3d,%3d)"
		"\n"
		, tileOcean
		, (tileOcean ? tileOceanAssociation : tileLandAssociation)
		, getX(tile), getY(tile)
	);

	if (!tileOcean)
	{
		landTerraformingRequestCounts[tileLandAssociation]++;
	}
	else
	{
		seaTerraformingRequestCounts[tileOceanAssociation]++;
	}

}

// Data

void Data::clear()
{
	// resize tileInfos to map size

	tileInfos.clear();
	tileInfos.resize(*MapAreaTiles, {});

	// cleanup lists

	enemyStacks.clear();
	production.clear();
	baseIds.clear();
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
	airFormerVehicleIds.clear();
	landFormerVehicleIds.clear();
	seaFormerVehicleIds.clear();
	seaTransportVehicleIds.clear();
	threatLevel = 0.0;
	regionSurfaceCombatVehicleIds.clear();
	regionSurfaceScoutVehicleIds.clear();
	regionDefenseDemand.clear();
	oceanAssociationMaxMineralSurpluses.clear();
	landCombatUnitIds.clear();
	seaCombatUnitIds.clear();
	airCombatUnitIds.clear();
	landAndAirCombatUnitIds.clear();
	seaAndAirCombatUnitIds.clear();
	seaTransportRequestCounts.clear();
	transportControl.clear();
	tasks.clear();

}

// access global data arrays

BaseInfo &Data::getBaseInfo(int baseId)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	return aiData.baseInfos.at(baseId);
}

TileInfo &Data::getTileInfo(int mapIndex)
{
	assert(mapIndex >= 0 && mapIndex < *MapAreaTiles);
	return aiData.tileInfos.at(mapIndex);
}
TileInfo &Data::getTileInfo(int x, int y)
{
	return getTileInfo(getMapIndexByCoordinates(x, y));
}
TileInfo &Data::getTileInfo(MAP *tile)
{
	assert(tile != nullptr && tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles);
	return getTileInfo(getMapIndexByPointer(tile));
}

TileFactionInfo &Data::getTileFactionInfo(MAP *tile, int factionId)
{
	assert(tile != nullptr && tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles);
	return getTileInfo(tile).factionInfos[factionId];
}

// EnemyStackInfo

void EnemyStackInfo::addVehicle(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);

	// add vehicle

	vehicleIds.emplace_back(vehicleId);

	if (isArtilleryVehicle(vehicleId))
	{
		artilleryVehicleIds.emplace_back(vehicleId);
		artillery = true;
		targetable = true;
	}

	if (isBombardmentVehicle(vehicleId))
	{
		bombardmentVehicleIds.push_back(vehicleId);
		bombardment = true;
		targetable = true;
	}

	// boolean flags

	if (vehicle->faction_id == 0)
	{
		alien = true;

		switch (vehicle->unit_id)
		{
		case BSC_MIND_WORMS:
			alienMelee = true;
			alienMindWorms = true;
			break;
		case BSC_SPORE_LAUNCHER:
			alienSporeLauncher = true;
			break;
		case BSC_FUNGAL_TOWER:
			alienFungalTower = true;
			break;
		case BSC_ISLE_OF_THE_DEEP:
		case BSC_SEALURK:
		case BSC_LOCUSTS_OF_CHIRON:
			alienMelee = true;
			break;
		}

	}

	if (isNeedlejetVehicle(vehicleId) && !isAtAirbase(vehicleId))
	{
		needlejetInFlight = true;
	}

	if (isArtifactVehicle(vehicleId))
	{
		artifact = true;
	}

	// vehicleSpeed

	int vehicleSpeed = getVehicleSpeed(vehicleId);
	if (lowestSpeed == -1 || vehicleSpeed < lowestSpeed)
	{
		lowestSpeed = vehicleSpeed;
	}

}

void EnemyStackInfo::setUnitEffect(int unitId, ATTACK_TYPE attackType, COMBAT_TYPE combatType, bool destructive, double effect)
{
	CombatEffect &combatEffect = unitEffects.at(getUnitSlotById(unitId) * ATTACK_TYPE_COUNT + attackType);
	combatEffect.combatType = combatType;
	combatEffect.destructive = destructive;
	combatEffect.effect = effect;
}

CombatEffect EnemyStackInfo::getUnitEffect(int unitId, ATTACK_TYPE attackType)
{
	return unitEffects.at(getUnitSlotById(unitId) * ATTACK_TYPE_COUNT + attackType);
}

CombatEffect EnemyStackInfo::getVehicleEffect(int vehicleId, ATTACK_TYPE attackType)
{
	CombatEffect combatEffect = getUnitEffect(getVehicle(vehicleId)->unit_id, attackType);
	combatEffect.effect *=  getVehicleStrenghtMultiplier(vehicleId);
	return combatEffect;
}

//void EnemyStackInfo::addProvidedEffect(int vehicleId, COMBAT_TYPE combatType)
//{
//	providedEffect += getVehicleEffect(vehicleId, combatType);
//}
//
double EnemyStackInfo::getAverageUnitEffect(int unitId)
{
	int effectCount = 0;
	double effectSum = 0.0;

	if (isMeleeUnit(unitId))
	{
		effectCount++;
		effectSum += getUnitEffect(unitId, AT_MELEE).effect;
	}

	if (isArtilleryUnit(unitId))
	{
		effectCount++;
		effectSum += getUnitEffect(unitId, AT_ARTILLERY).effect;
	}

	return (effectCount == 0 ? 0.0 : effectSum / (double)effectCount);

}

double EnemyStackInfo::getRequiredEffect(bool bombarded)
{
	return (bombarded ? requiredEffectBombarded : requiredEffect);
}

// combat data

double Data::getGlobalAverageUnitEffect(int unitId)
{
	return globalAverageUnitEffects.at(getUnitSlotById(unitId));
}

void Data::setGlobalAverageUnitEffect(int unitId, double effect)
{
	globalAverageUnitEffects.at(getUnitSlotById(unitId)) = effect;
}

// utility methods

void Data::addSeaTransportRequest(int oceanAssociation)
{
	seaTransportRequestCounts[oceanAssociation]++;
}

std::vector<Transfer> &Geography::getAssociationTransfers(int association1, int association2)
{
	return associationTransfers[association1][association2];
}

std::vector<Transfer> &Geography::getOceanBaseTransfers(MAP *tile)
{
	return oceanBaseTransfers[tile];
}

// UnloadRequest

UnloadRequest::UnloadRequest(int _vehicleId, MAP *_destination, MAP *_unboardLocation)
{
	this->vehiclePad0 = getVehicle(_vehicleId)->pad_0;
	this->destination = _destination;
	this->unboardLocation = _unboardLocation;
}

int UnloadRequest::getVehicleId()
{
	return getVehicleIdByAIId(this->vehiclePad0);
}

// TransitRequest

TransitRequest::TransitRequest(int _vehicleId, MAP *_origin, MAP *_destination)
{
	this->vehiclePad0 = getVehicle(_vehicleId)->pad_0;
	this->origin = _origin;
	this->destination = _destination;
}

int TransitRequest::getVehicleId()
{
	return getVehicleIdByAIId(this->vehiclePad0);
}

void TransitRequest::setSeaTransportVehicleId(int seaTransportVehicleId)
{
	this->seaTransportVehiclePad0 = getVehicle(seaTransportVehicleId)->pad_0;
}

int TransitRequest::getSeaTransportVehicleId()
{
	return getVehicleIdByAIId(this->seaTransportVehiclePad0);
}

bool TransitRequest::isFulfilled()
{
	return this->seaTransportVehiclePad0 != -1;
}

// TransportControl

void TransportControl::clear()
{
	unloadRequests.clear();
	transitRequests.clear();
}

void TransportControl::addUnloadRequest(int seaTransportVehicleId, int vehicleId, MAP *destination, MAP *unboardLocation)
{
	VEH *seaTransportVehicle = getVehicle(seaTransportVehicleId);
	this->unloadRequests[seaTransportVehicle->pad_0].emplace_back(vehicleId, destination, unboardLocation);
}

std::vector<UnloadRequest> TransportControl::getSeaTransportUnloadRequests(int seaTransportVehicleId)
{
	VEH *seaTransportVehicle = getVehicle(seaTransportVehicleId);
	return this->unloadRequests[seaTransportVehicle->pad_0];
}

bool isWarzone(MAP *tile)
{
	return aiData.getTileInfo(tile).warzone;
}

// MovementAction

MovementAction::MovementAction(MAP *_destination, int _movementAllowance)
: destination{_destination}, movementAllowance{_movementAllowance}
{
}

// CombatAction

CombatAction::CombatAction(MAP *_destination, int _movementAllowance, ATTACK_TYPE _attackType, MAP *_attackTarget)
: MovementAction(_destination, _movementAllowance), attackType{_attackType}, attackTarget{_attackTarget}
{
}

// MutualLoss

void MutualLoss::accumulate(MutualLoss &mutualLoss)
{
	own += mutualLoss.own;
	foe += mutualLoss.foe;
}

