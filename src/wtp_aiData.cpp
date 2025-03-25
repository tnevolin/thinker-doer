
#include <cstring>
#include "wtp_game.h"
#include "wtp_aiData.h"
#include "wtp_ai.h"
#include "wtp_aiRoute.h"

//// BaseSnapshots
//
//std::vector<BASE> BaseSnapshots::baseSnapshots;
//
// global variables

Data aiData;
int aiFactionId = -1;
MFaction *aiMFaction;
Faction *aiFaction;
FactionInfo *aiFactionInfo;

// CombatStrength

void CombatStrength::accumulate(CombatStrength &combatStrength, double multiplier)
{
	for (COMBAT_TYPE attackerCOMBAT_TYPE : {CT_PSI, CT_CON})
	{
		for (COMBAT_TYPE defenderCOMBAT_TYPE : {CT_PSI, CT_CON})
		{
			this->values.at(attackerCOMBAT_TYPE).at(defenderCOMBAT_TYPE) += multiplier * combatStrength.values.at(attackerCOMBAT_TYPE).at(defenderCOMBAT_TYPE);
		}
		
	}
	
}

double CombatStrength::getAttackEffect(int vehicleId)
{
	COMBAT_TYPE defenderCombatType = getArmorType(vehicleId);
	
	double effect = 0.0;
	
	switch (defenderCombatType)
	{
	case CT_PSI:
		
		{
			double psiDefenseStrength = getVehiclePsiDefenseStrength(vehicleId);
			
			effect =
				+ this->values.at(CT_PSI).at(CT_PSI) / psiDefenseStrength
				+ this->values.at(CT_CON).at(CT_PSI) / psiDefenseStrength
			;
			
		}
		
		break;
		
	case CT_CON:
		
		{
			double psiDefenseStrength = getVehiclePsiDefenseStrength(vehicleId);
			double conDefenseStrength = getVehicleConDefenseStrength(vehicleId);
			
			effect =
				+ this->values.at(CT_PSI).at(CT_CON) / psiDefenseStrength
				+ this->values.at(CT_CON).at(CT_CON) / conDefenseStrength
			;
			
		}
		
		break;
		
	}
	
	return effect;
	
}

//double FoeUnitWeightTable::get(int factionId, int unitId)
//{
//	return weights.at(getUnitIndex(factionId, unitId));
//}
//void FoeUnitWeightTable::set(int factionId, int unitId, double weight)
//{
//	weights.at(getUnitIndex(factionId, unitId)) = weight;
//}
//void FoeUnitWeightTable::add(int factionId, int unitId, double weight)
//{
//	weights.at(getUnitIndex(factionId, unitId)) += weight;
//}

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

TileInfo &Data::getTileInfo(int tileIndex)
{
	assert(tileIndex >= 0 && tileIndex < *MapAreaTiles);
	return aiData.tileInfos.at(tileIndex);
}

TileInfo &Data::getTileInfo(MAP *tile)
{
	assert(isOnMap(tile));
	return aiData.tileInfos.at(tile - *MapTiles);
}

TileInfo &Data::getBaseTileInfo(int baseId)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	return aiData.tileInfos.at(getBaseMapTileIndex(baseId));
}

TileInfo &Data::getVehicleTileInfo(int vehicleId)
{
	assert(vehicleId >= 0 && vehicleId < *VehCount);
	return aiData.tileInfos.at(getVehicleMapTileIndex(vehicleId));
}

bool Data::isSea(MAP *tile)
{
	assert(isOnMap(tile));
	return aiData.tileInfos.at(tile - *MapTiles).ocean;
}

bool Data::isLand(MAP *tile)
{
	assert(isOnMap(tile));
	return !aiData.tileInfos.at(tile - *MapTiles).ocean;
}

bool Data::isSeaUnitAllowed(MAP *tile, int factionId)
{
	assert(isOnMap(tile));
	
	TileInfo &tileInfo = aiData.tileInfos.at(tile - *MapTiles);
	
	return tileInfo.ocean || (tileInfo.base && isFriendly(factionId, tile->owner));
	
}

bool Data::isLandUnitAllowed(MAP *tile)
{
	assert(isOnMap(tile));
	
	TileInfo &tileInfo = aiData.tileInfos.at(tile - *MapTiles);
	
	return !tileInfo.ocean;
	
}

BaseInfo &Data::getBaseInfo(int baseId)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	return aiData.baseInfos.at(baseId);
}

// ProtectedLocation

void ProtectedLocation::addVehicle(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int triad = vehicle->triad();
	
	// set stack tile
	
	if (tile == nullptr)
	{
		tile = vehicleTile;
	}
	else
	{
		assert(vehicleTile == tile);
	}
	
	// add combat vehicle
	
	if (isCombatVehicle(vehicleId))
	{
		vehicleIds.push_back(vehicleId);
		
		if (triad == TRIAD_AIR && !airbase)
		{
			airVehicleIds.push_back(vehicleId);
		}
		else if (isArtilleryVehicle(vehicleId))
		{
			artilleryVehicleIds.push_back(vehicleId);
			artillery = true;
		}
		else
		{
			nonArtilleryVehicleIds.push_back(vehicleId);
		}
		
	}
	
}

// EnemyStackInfo

void EnemyStackInfo::addVehicle(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	int triad = vehicle->triad();
	
	// set stack tile
	
	if (tile == nullptr)
	{
		tile = vehicleTile;
	}
	else
	{
		if (tile != vehicleTile)
		{
			debug("ERROR: stack vehicles are on different tiles."); exit(1);
		}
		
	}
	
	// add combat vehicle
	
	if (!isArtifactVehicle(vehicleId) && !(isProbeVehicle(vehicleId) && getVehicleDefenseValue(vehicleId) == 1))
	{
		vehicleIds.push_back(vehicleId);
		
		if (triad == TRIAD_AIR && !airbase)
		{
			airVehicleIds.push_back(vehicleId);
		}
		else if (isArtilleryVehicle(vehicleId))
		{
			artilleryVehicleIds.push_back(vehicleId);
			artillery = true;
			targetable = true;
		}
		else
		{
			nonArtilleryVehicleIds.push_back(vehicleId);
			bombardment = true;
			targetable = true;
		}
		
		// add weight
		
		weight += getVehicleRelativePower(vehicleId);
		
	}
	
	// boolean flags
	
	hostile = isHostile(aiFactionId, vehicle->faction_id);
	
	if (isPact(aiFactionId, vehicle->faction_id) || isTreaty(aiFactionId, vehicle->faction_id))
	{
		breakTreaty = true;
	}
	
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

bool EnemyStackInfo::isUnitCanMeleeAttackStack(int unitId, MAP *position) const
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	bool ocean = is_ocean(this->tile);
	bool base = this->base;
	
	// melee unit
	
	if (!isMeleeUnit(unitId))
		return false;
	
	// cannot attack needlejet in flight without air superiority
	
	if (this->needlejetInFlight && !isUnitHasAbility(unitId, ABL_AIR_SUPERIORITY))
		return false;
	
	// check movement
	
	switch (triad)
	{
	case TRIAD_SEA:
		
		if (unitId == BSC_SEALURK)
		{
			// sealurk can melee attack land
		}
		else
		{
			// sea vehicle cannot attack enemy on land (except sealurk)
			
			if (!ocean)
				return false;
			
		}
		
		break;
		
	case TRIAD_LAND:
		
		if (isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
		{
			// land amphibious vehicle can attack sea base but not open sea
			
			if (ocean && !base)
				return false;
		
		}
		else
		{
			// land non-amphibious vehicle cannot attack enemy on ocean
			
			if (ocean)
				return false;
			
		}
		
		break;
		
	}
	
	// check position if given
	
	if (position != nullptr)
	{
		bool positionOcean = is_ocean(position);
		bool positionBase = map_has_item(position, BIT_BASE_IN_TILE);
		
		switch (triad)
		{
		case TRIAD_SEA:
			
			// sea vehicle can attack from ocean or base
			
			if (!positionOcean && !positionBase)
				return false;
			
			break;
			
		case TRIAD_LAND:
			
			if (isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
			{
				// land amphibious vehicle can attack from sea
			}
			else
			{
				// land non-amphibious can attack from land only
				
				if (positionOcean)
					return false;
				
			}
			
			break;
			
		}
		
	}
	
	// all checks passed
	
	return true;
	
}

bool EnemyStackInfo::isUnitCanArtilleryAttackStack(int unitId, MAP *position) const
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	
	// artillery unit
	
	if (!isArtilleryUnit(unitId))
		return false;
	
	// available attack targets
	
	if (!this->targetable)
		return false;
	
	// check position if given
	
	if (position != nullptr)
	{
		bool positionOcean = is_ocean(position);
		bool positionBase = map_has_item(position, BIT_BASE_IN_TILE);
		
		switch (triad)
		{
		case TRIAD_SEA:
			
			// sea artillery can fire from ocean or base
			
			if (!positionOcean && !positionBase)
				return false;
			
			break;
			
		case TRIAD_LAND:
			
			// land artillery can fire from land or base
			
			if (positionOcean && !positionBase)
				return false;
			
			break;
			
		}
		
	}
	
	// all checks passed
	
	return true;
	
}

void EnemyStackInfo::setUnitOffenseEffect(int unitId, COMBAT_MODE combatMode, double effect)
{
	this->offenseEffects.at(getUnitSlotById(unitId) * COMBAT_MODE_COUNT + combatMode) = effect;
}

double EnemyStackInfo::getUnitOffenseEffect(int unitId, COMBAT_MODE combatMode) const
{
	return this->offenseEffects.at(getUnitSlotById(unitId) * COMBAT_MODE_COUNT + combatMode);
}

double EnemyStackInfo::getVehicleOffenseEffect(int vehicleId, COMBAT_MODE combatMode) const
{
	return this->getUnitOffenseEffect(getVehicle(vehicleId)->unit_id, combatMode) * getVehicleStrenghtMultiplier(vehicleId);
}

void EnemyStackInfo::setUnitDefenseEffect(int unitId, double effect)
{
	this->defenseEffects.at(getUnitSlotById(unitId)) = effect;
}

double EnemyStackInfo::getUnitDefenseEffect(int unitId) const
{
	return this->defenseEffects.at(getUnitSlotById(unitId));
}

double EnemyStackInfo::getVehicleDefenseEffect(int vehicleId) const
{
	return this->getUnitDefenseEffect(getVehicle(vehicleId)->unit_id) * getVehicleStrenghtMultiplier(vehicleId);
}

double EnemyStackInfo::getUnitBombardmentEffect(int unitId) const
{
	return this->getUnitOffenseEffect(unitId, CM_BOMBARDMENT);
}

double EnemyStackInfo::getVehicleBombardmentEffect(int vehicleId) const
{
	return this->getUnitBombardmentEffect(getVehicle(vehicleId)->unit_id) * getVehicleMoraleMultiplier(vehicleId);
}

double EnemyStackInfo::getUnitDirectEffect(int unitId) const
{
	return std::max(this->getUnitOffenseEffect(unitId, CM_ARTILLERY_DUEL), this->getUnitOffenseEffect(unitId, CM_MELEE));
}

double EnemyStackInfo::getVehicleDirectEffect(int vehicleId) const
{
	return this->getUnitDirectEffect(getVehicle(vehicleId)->unit_id) * getVehicleStrenghtMultiplier(vehicleId);
}

/**
Estimates total bombardment effect for n-turn bombardment.
*/
double EnemyStackInfo::getTotalBombardmentEffect() const
{
	if (artillery || !bombardment || combinedBombardmentEffect == 0.0)
		return 0.0;
	
	return std::min(maxBombardmentEffect, (double)BOMBARDMENT_ROUNDS * combinedBombardmentEffect);
	
}

/**
Estimates total bombardment effect for n-turn bombardment.
Ratio of bombardment effect to the weight.
*/
double EnemyStackInfo::getTotalBombardmentEffectRatio() const
{
	return getTotalBombardmentEffect() / weight;
}

/**
Computes required superiority melee effect.
*/
double EnemyStackInfo::getRequiredEffect() const
{
	double totalBombardmentEffect = getTotalBombardmentEffect();
	double requiredMeleeEffect = requiredSuperiority * (weight - totalBombardmentEffect + extraWeight);
	return requiredMeleeEffect;
}

/**
Returns total destruction ratio after bombardment if any.
*/
double EnemyStackInfo::getDestructionRatio() const
{
	double totalBombardmentEffect = getTotalBombardmentEffect();
	double destructionRatio = combinedDirectEffect / (weight - totalBombardmentEffect + extraWeight);
	return destructionRatio;
}

/**
Checks sufficient destruction ratio.
*/
bool EnemyStackInfo::isSufficient(bool desired) const
{
	return getDestructionRatio() >= (desired ? desiredSuperiority : requiredSuperiority);
}

/**
Attempts adding vehicle to stack attackers.
Rejects artillery if provided desired artillery effect is already satisfied.
Rejects melee if provided desired melee effect is already satisfied.
Updates attack parameters.
Returns true if accepted.
*/
bool EnemyStackInfo::addAttacker(IdDoubleValue vehicleTravelTime)
{
	int vehicleId = vehicleTravelTime.id;
	
	bool accepted = false;
	
	double bombardmentEffect = getVehicleBombardmentEffect(vehicleId);
	double directEffect = getVehicleDirectEffect(vehicleId);
	
	if (bombardmentEffect > 0.0 && !artillery && bombardment && !bombardmentSufficient)
	{
		combinedBombardmentEffect += bombardmentEffect;
		bombardmentSufficient = (double)BOMBARDMENT_ROUNDS * combinedBombardmentEffect >= maxBombardmentEffect;
		bombardmentVechileTravelTimes.push_back(vehicleTravelTime);
		accepted = true;
	}
	else if (directEffect > 0.0 && !directSufficient)
	{
		combinedDirectEffect += directEffect;
		directSufficient = getDestructionRatio() >= desiredSuperiority;
		directVechileTravelTimes.push_back(vehicleTravelTime);
		accepted = true;
	}
	
	return accepted;
	
}

/**
Computes atttack parameters after all attackers are added.
*/
void EnemyStackInfo::computeAttackParameters()
{
	debug("enemyStack computeAttackParameters %s\n", getLocationString(tile).c_str());
	
	// sort direct ascending
	
	std::sort(directVechileTravelTimes.begin(), directVechileTravelTimes.end(), compareIdDoubleValueAscending);
	
	// find a coordinator travel time
	// the latest one to amount to the required superiority
	
	double requiredEffect = getRequiredEffect();
	double accumulatedEffect = 0.0;
	coordinatorTravelTime = INF;
	
	for (IdDoubleValue const &vehicleTravelTime : directVechileTravelTimes)
	{
		int vehicleId = vehicleTravelTime.id;
		double travelTime = vehicleTravelTime.value;
		
		double effect = getVehicleDirectEffect(vehicleId);
		accumulatedEffect += effect;
		
		debug
		(
			"\t[%4d] %s"
			" travelTime=%7.2f"
			" requiredEffect=%5.2f"
			" effect=%5.2f"
			" accumulatedEffect=%5.2f"
			"\n"
			, vehicleTravelTime.id, getLocationString(getVehicleMapTile(vehicleTravelTime.id)).c_str()
			, vehicleTravelTime.value
			, requiredEffect
			, effect
			, accumulatedEffect
		);
		
		if (accumulatedEffect >= requiredEffect)
		{
			coordinatorTravelTime = travelTime;
			break;
		}
		
	}
		
	debug("\tcoordinatorTravelTime=%5.2f\n", coordinatorTravelTime)
	
	// compute additional weight
	
	if (baseId != -1)
	{
		BASE *base = getBase(baseId);
		int factionId = base->faction_id;
		FactionInfo const &factionInfo = aiData.factionInfos.at(factionId);
		
		double scoutPatrolBuildTime = getBaseItemBuildTime(baseId, BSC_SCOUT_PATROL);
		double extraScoutPatrols = scoutPatrolBuildTime <= 0.0 ? 0.0 : coordinatorTravelTime / scoutPatrolBuildTime;
		extraWeight = factionInfo.maxConDefenseValue <= 0 ? INF : extraScoutPatrols / (double)factionInfo.maxConDefenseValue;
		
	}
	
	debug("\textraWeight=%5.2f\n", extraWeight);
	
	// recompute destructionRatio
	
	destructionRatio = getDestructionRatio();
	requiredSuperioritySatisfied = destructionRatio >= requiredSuperiority;
	
	debug("\tdestructionRatio=%5.2f requiredSuperioritySatisfied=%d\n", destructionRatio, requiredSuperioritySatisfied);
	
}

// utility methods

void Data::addSeaTransportRequest(int seaCluster)
{
	seaTransportRequestCounts[seaCluster]++;
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
	return aiData.tileInfos.at(tile - *MapTiles).hostileDangerZone;
}

// MutualLoss

void MutualLoss::accumulate(MutualLoss &mutualLoss)
{
	own += mutualLoss.own;
	foe += mutualLoss.foe;
}

bool isBlocked(int tileIndex)
{
	assert(tileIndex >= 0 && tileIndex < *MapAreaTiles);
	return aiData.tileInfos.at(tileIndex).blocked;
}

bool isBlocked(MAP *tile)
{
	assert(isOnMap(tile));
	return aiData.tileInfos.at(tile - *MapTiles).blocked;
}

bool isZoc(int orgTileIndex, int dstTileIndex)
{
	assert(orgTileIndex >= 0 && orgTileIndex < *MapAreaTiles);
	assert(dstTileIndex >= 0 && dstTileIndex < *MapAreaTiles);
	return aiData.tileInfos.at(orgTileIndex).orgZoc && aiData.tileInfos.at(dstTileIndex).dstZoc;
}

bool isZoc(MAP *orgTile, MAP *dstTile)
{
	assert(isOnMap(orgTile) && isOnMap(dstTile));
	return aiData.tileInfos.at(orgTile - *MapTiles).orgZoc && aiData.tileInfos.at(dstTile - *MapTiles).dstZoc;
}

