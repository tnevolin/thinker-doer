
#include <cstring>
#include "wtp_game.h"
#include "wtp_aiData.h"
#include "wtp_ai.h"
#include "wtp_aiRoute.h"

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

// TileInfo

double TileInfo::getOffenseMultiplier(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId)
{
	double offenseMultiplier = this->sensorOffenseMultipliers.at(attackerFactionId);
	if (this->fungus && (isNativeUnit(attackerUnitId) || has_project(FAC_XENOEMPATHY_DOME, attackerFactionId)) && !(isNativeUnit(defenderUnitId) || has_project(FAC_XENOEMPATHY_DOME, defenderFactionId)))
	{
		offenseMultiplier *= 1.5;
	}
	return offenseMultiplier;
}

double TileInfo::getDefenseMultiplier(int /*attackerFactionId*/, int attackerUnitId, int defenderFactionId, int defenderUnitId, bool atTile)
{
	double defenseMultiplier = this->sensorDefenseMultipliers.at(defenderFactionId);
	if (atTile)
	{
		int attackTriad = getAttackTriad(attackerUnitId, defenderUnitId);
		defenseMultiplier *= this->structureDefenseMultipliers.at(attackTriad);
	}
	return defenseMultiplier;
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
	combatVehicleIds.clear();
	scoutVehicleIds.clear();
	outsideCombatVehicleIds.clear();
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
	int vehiclePad0 = vehicle->pad_0;
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
			debug("ERROR: stack vehicles are on different tiles.");
			return;
		}
		
	}
	
	// add combat vehicle
	
	if (!isArtifactVehicle(vehicleId) && !(isProbeVehicle(vehicleId) && getVehicleDefenseValue(vehicleId) == 1))
	{
		vehiclePad0s.push_back(vehiclePad0);
		
		if (triad == TRIAD_AIR && !airbase)
		{
			airVehiclePad0s.push_back(vehiclePad0);
		}
		else if (isArtilleryVehicle(vehicleId))
		{
			artilleryVehiclePad0s.push_back(vehiclePad0);
			artillery = true;
			targetable = true;
		}
		else
		{
			nonArtilleryVehiclePad0s.push_back(vehiclePad0);
			bombardment = true;
			targetable = true;
		}
		
		// add weight
		
		weight += getVehicleRelativeHealth(vehicleId);
		
	}
	
	// boolean flags
	
	hostile = isHostile(aiFactionId, vehicle->faction_id);
	
	if (!isHostile(aiFactionId, vehicle->faction_id))
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
	
	if (!conf.air_superiority_not_required_to_attack_needlejet && this->needlejetInFlight && !isUnitHasAbility(unitId, ABL_AIR_SUPERIORITY))
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
	this->offenseEffects.at(getUnitSlotById(unitId)).at(combatMode) = effect;
}

double EnemyStackInfo::getUnitOffenseEffect(int unitId, COMBAT_MODE combatMode) const
{
	return this->offenseEffects.at(getUnitSlotById(unitId)).at(combatMode);
}

double EnemyStackInfo::getVehicleOffenseEffect(int vehicleId, COMBAT_MODE combatMode) const
{
	return this->getUnitOffenseEffect(getVehicle(vehicleId)->unit_id, combatMode) * getVehicleStrenghtMultiplier(vehicleId);
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
	return getVehicleIdByPad0(this->vehiclePad0);
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
	return getVehicleIdByPad0(this->vehiclePad0);
}

void TransitRequest::setSeaTransportVehicleId(int seaTransportVehicleId)
{
	this->seaTransportVehiclePad0 = getVehicle(seaTransportVehicleId)->pad_0;
}

int TransitRequest::getSeaTransportVehicleId()
{
	return getVehicleIdByPad0(this->seaTransportVehiclePad0);
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

//// ProtectCombatData
//
//void ProtectCombatData::addAssaultEffect(AssaultEffectCombatType assaultEffectCombatType, int assailantFactionId, int assailantUnitId, int protectorFactionId, int protectorUnitId, double assaultEffect)
//{
//	assaultEffects.at(assaultEffectCombatType).push_back({assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, assaultEffect});
//}
//
//void ProtectCombatData::addAssailant(int vehicleId, double weight)
//{
//	VEH *vehicle = getVehicle(vehicleId);
//	int factionUnitKey = encodeFactionUnitKey(vehicle->faction_id, vehicle->unit_id);
//	
//	double strengthCoefficient = getVehicleStrenghtMultiplier(vehicleId);
//	weight *= strengthCoefficient;
//	
//	if (assailantWeights.find(factionUnitKey) == assailantWeights.end())
//	{
//		assailantWeights.emplace(factionUnitKey, 0.0);
//	}
//	assailantWeights.at(factionUnitKey) += weight;
//	
//}
//
//void ProtectCombatData::addProtector(int vehicleId)
//{
//	VEH *vehicle = getVehicle(vehicleId);
//	double weight = 1.0;
//	int factionUnitKey = encodeFactionUnitKey(vehicle->faction_id, vehicle->unit_id);
//	
//	double strengthCoefficient = getVehicleMoraleMultiplier(vehicleId);
//	weight *= strengthCoefficient;
//	
//	if (protectorWeights.find(factionUnitKey) == protectorWeights.end())
//	{
//		protectorWeights.emplace(factionUnitKey, 0.0);
//	}
//	protectorWeights.at(factionUnitKey) += weight;
//	
//}
//
//void ProtectCombatData::compute()
//{
//	// count initial weight
//	
//	double initialAssailantWeight = 0.0;
//	double initialProtectorWeight = 0.0;
//	for (robin_hood::pair<int, double> assailantWeightEntry : assailantWeights)
//	{
//		double factionUnitKey = assailantWeightEntry.first;
//		double weight = assailantWeightEntry.second;
//		
//		// melee assailant only
//		if (meleeAssailants.find(factionUnitKey) == meleeAssailants.end())
//			continue;
//		
//		initialAssailantWeight += weight;
//		
//	}
//	for (robin_hood::pair<int, double> protectorWeightEntry : protectorWeights)
//	{
//		double weight = protectorWeightEntry.second;
//		initialProtectorWeight += weight;
//	}
//	
//	// simple cases
//	
//	if (initialAssailantWeight <= 0.0)
//	{
//		assaultPrevalence = 0.0;
//		return;
//	}
//	else if (initialProtectorWeight <= 0.0)
//	{
//		assaultPrevalence = INF;
//		return;
//	}
//	
//	// current weights
//	
//	robin_hood::unordered_flat_map<int, double> currentAssailantWeights(assailantWeights.begin(), assailantWeights.end());
//	robin_hood::unordered_flat_map<int, double> currentProtectorWeights(protectorWeights.begin(), protectorWeights.end());
//	
//	// match assailant artillery with protectors
//	
//	for (AssaultEffectCombatType assaultEffectCombatType : {AECT_ARTILLERY_ARTILLERY, AECT_ARTILLERY_NON_ARTILLERY})
//	{
//		for (AssaultEffect const &assaultEffect : assaultEffects.at(assaultEffectCombatType))
//		{
//			int assailantFactionUnitKey = encodeFactionUnitKey(assaultEffect.assailantFactionId, assaultEffect.assailantUnitId);
//			int protectorFactionUnitKey = encodeFactionUnitKey(assaultEffect.protectorFactionId, assaultEffect.protectorUnitId);
//			
//			// no such assailant or protector
//			if (currentAssailantWeights.find(assailantFactionUnitKey) == currentAssailantWeights.end())
//				continue;
//			if (currentProtectorWeights.find(protectorFactionUnitKey) == currentProtectorWeights.end())
//				continue;
//			
//			// current weights
//			double currentAssailantWeight = currentAssailantWeights.at(assailantFactionUnitKey);
//			double currentProtectorWeight = currentProtectorWeights.at(protectorFactionUnitKey);
//			
//			// no more assailant or protector
//			if (currentAssailantWeight <= 0.0)
//				continue;
//			if (currentProtectorWeight <= 0.0)
//				continue;
//			
//			// assailant is desroyed outright
//			if (assaultEffect.value == 0)
//			{
//				currentAssailantWeights.at(assailantFactionUnitKey) = 0.0;
//				continue;
//			}
//			// assailant is desroying protector
//			if (assaultEffect.value == INF)
//			{
//				currentProtectorWeights.at(protectorFactionUnitKey) = 0.0;
//				continue;
//			}
//			
//			// assailants weight reduction for destroying all protectors
//			
//			double maxAssailantWeightReduction = currentProtectorWeight / assaultEffect.value;
//			double assailantWeightReduction = std::min(maxAssailantWeightReduction, currentAssailantWeight);
//			double protectorWeightReduction = assailantWeightReduction * assaultEffect.value;
//			
//			currentAssailantWeights.at(assailantFactionUnitKey) -= assailantWeightReduction;
//			currentProtectorWeights.at(protectorFactionUnitKey) -= protectorWeightReduction;
//			
//		}
//		
//	}
//	
//	// check if any assailant artillery left
//	
//	bool assailantArtillery = false;
//	for (int artilleryAssailantFactionUnitKey : artilleryAssailants)
//	{
//		// no such assailant
//		if (currentAssailantWeights.find(artilleryAssailantFactionUnitKey) == currentAssailantWeights.end())
//			continue;
//		
//		// no more of this assilant
//		if (currentAssailantWeights.at(artilleryAssailantFactionUnitKey) <= 0.0)
//			continue;
//		
//		assailantArtillery = true;
//		break;
//		
//	}
//	
//	// reduce current protector health if assailant artillery remains
//	
//	if (assailantArtillery)
//	{
//		for (robin_hood::pair<int, double> &currentProtectorEntry : currentProtectorWeights)
//		{
//			currentProtectorEntry.second /= bombardmentHealthReduction;
//		}
//	}
//	
//	// match assailant non artillery with protectors
//	
//	for (AssaultEffectCombatType assaultEffectCombatType : {AECT_MELEE})
//	{
//		for (AssaultEffect const &assaultEffect : assaultEffects.at(assaultEffectCombatType))
//		{
//			int assailantFactionUnitKey = encodeFactionUnitKey(assaultEffect.assailantFactionId, assaultEffect.assailantUnitId);
//			int protectorFactionUnitKey = encodeFactionUnitKey(assaultEffect.protectorFactionId, assaultEffect.protectorUnitId);
//			
//			// no such assailant or protector
//			if (currentAssailantWeights.find(assailantFactionUnitKey) == currentAssailantWeights.end())
//				continue;
//			if (currentProtectorWeights.find(protectorFactionUnitKey) == currentProtectorWeights.end())
//				continue;
//			
//			// current weights
//			double currentAssailantWeight = currentAssailantWeights.at(assailantFactionUnitKey);
//			double currentProtectorWeight = currentProtectorWeights.at(protectorFactionUnitKey);
//			
//			// no more assailant or protector
//			if (currentAssailantWeight <= 0.0)
//				continue;
//			if (currentProtectorWeight <= 0.0)
//				continue;
//			
//			// assailant is desroyed outright
//			if (assaultEffect.value == 0)
//			{
//				currentAssailantWeights.at(assailantFactionUnitKey) = 0.0;
//				continue;
//			}
//			// assailant is desroying protector
//			if (assaultEffect.value == INF)
//			{
//				currentProtectorWeights.at(protectorFactionUnitKey) = 0.0;
//				continue;
//			}
//			
//			// assailants weight reduction for destroying all protectors
//			
//			double maxAssailantWeightReduction = currentProtectorWeight / assaultEffect.value;
//			double assailantWeightReduction = std::min(maxAssailantWeightReduction, currentAssailantWeight);
//			double protectorWeightReduction = assailantWeightReduction * assaultEffect.value;
//			
//			currentAssailantWeights.at(assailantFactionUnitKey) -= assailantWeightReduction;
//			currentProtectorWeights.at(protectorFactionUnitKey) -= protectorWeightReduction;
//			
//		}
//		
//	}
//	
//	// count terminal weight
//	
//	double terminalAssailantWeight = 0.0;
//	double terminalProtectorWeight = 0.0;
//	for (robin_hood::pair<int, double> assailantWeightEntry : currentAssailantWeights)
//	{
//		double factionUnitKey = assailantWeightEntry.first;
//		double weight = assailantWeightEntry.second;
//		
//		// melee assailant only
//		if (meleeAssailants.find(factionUnitKey) == meleeAssailants.end())
//			continue;
//		
//		terminalAssailantWeight += weight;
//		
//	}
//	for (robin_hood::pair<int, double> protectorWeightEntry : currentProtectorWeights)
//	{
//		double weight = protectorWeightEntry.second;
//		terminalProtectorWeight += weight;
//	}
//	
//	// compute prevalence
//	
//	if (terminalAssailantWeight > 0.0 && terminalProtectorWeight > 0.0)
//	{
//		debug("ERROR: terminalAssailantWeight > 0.0 && terminalProtectorWeight > 0.0\n");
//		assaultPrevalence = 1.0;
//	}
//	else if (terminalAssailantWeight > 0.0)
//	{
//		double spentAssailantWeight = initialAssailantWeight - terminalAssailantWeight;
//		assaultPrevalence = spentAssailantWeight <= 0.0 ? INF : initialAssailantWeight / spentAssailantWeight;
//	}
//	else
//	{
//		double spentProtectorWeight = initialProtectorWeight - terminalProtectorWeight;
//		assaultPrevalence = spentProtectorWeight <= 0.0 ? 0.0 : initialProtectorWeight / spentProtectorWeight;
//	}
//	
//}
//
///*
//Selects the best effect against not yet countered foe unit.
//*/
//UnitEffectResult ProtectCombatData::getUnitEffectResult(int unitId) const
//{
//	Profiling::start("- getUnitEffectResult");
//	
//	int bestFoeFactionId = -1;
//	int bestFoeUnitId = -1;
//	double bestEffect = 0.0;
//	
//	for (int foeFactionId = 0; foeFactionId < MaxPlayerNum; foeFactionId++)
//	{
//		robin_hood::unordered_flat_map<int, CounterEffect> const &foeFactionCounterEffect = counterEffects.at(foeFactionId);
//		
//		for (robin_hood::pair<int, CounterEffect> const &foeFactionUnitCounterEffectEntry : foeFactionCounterEffect)
//		{
//			int foeUnitId = foeFactionUnitCounterEffectEntry.first;
//			CounterEffect const &counterEffect = foeFactionUnitCounterEffectEntry.second;
//			
//			if (counterEffect.isSatisfied(false))
//				continue;
//			
//			double assaultEffect = getAssaultEffect(foeFactionId, foeUnitId, aiFactionId, unitId);
//			double protectEffect = assaultEffect <= 0.0 ? 0.0 : 1.0 / assaultEffect;
//			
//			if (protectEffect > bestEffect)
//			{
//				bestFoeFactionId = foeFactionId;
//				bestFoeUnitId = foeUnitId;
//				bestEffect = protectEffect;
//			}
//			
//		}
//		
//	}
//	
//	Profiling::stop("- getUnitEffectResult");
//	return {bestFoeFactionId, bestFoeUnitId, bestEffect};
//	
//}
//double ProtectCombatData::getUnitEffect(int unitId) const
//{
//	return getUnitEffectResult(unitId).effect;
//}
//double ProtectCombatData::getVehicleEffect(int vehicleId) const
//{
//	return getVehicleMoraleMultiplier(vehicleId) * getUnitEffect(Vehs[vehicleId].unit_id);
//}
///*
//Adds vehicle effect too all counter effects until it runs out.
//Assuming fully powered vehicle at the end of the turn.
//*/
//void ProtectCombatData::addVehicleEffect(int vehicleId, bool present)
//{
//	debug("ProtectCombatData::addVehicleEffect %s present=%d\n", getVehicleIdAndLocationString(vehicleId).c_str(), present);
//	debug("\n");
//	
//	int unitId = Vehs[vehicleId].unit_id;
//	double multiplier = getVehicleMoraleMultiplier(vehicleId);
//	debug("\tmultiplier=%5.2f\n", multiplier);
//	
//	double remainingPower = 1.0;
//	while (true)
//	{
//		UnitEffectResult unitEffectResult = getUnitEffectResult(unitId);
//		debug("\tunitEffectResult: foeFactionId=%d foeUnitId=%3d effect=%5.2f\n", unitEffectResult.foeFactionId, unitEffectResult.foeUnitId, unitEffectResult.effect);
//		
//		if (unitEffectResult.effect <= 0.0)
//			break;
//		
//		CounterEffect &counterEffect = counterEffects.at(unitEffectResult.foeFactionId).at(unitEffectResult.foeUnitId);
//		double effect = multiplier * unitEffectResult.effect * remainingPower;
//		
//		// compute counter
//		
//		double providing = effect * remainingPower;
//		double required = counterEffect.required;
//		double provided = counterEffect.provided;
//		double remaning = required - provided;
//		double applied = std::min(remaning, providing);
//		debug("\tproviding=%5.2f required=%5.2f provided=%5.2f remaning=%5.2f applied=%5.2f\n", providing, required, provided, remaning, applied);
//		
//		counterEffect.provided += applied;
//		if (present)
//		{
//			counterEffect.providedPresent += applied;
//		}
//		
//		if (applied >= providing)
//		{
//			// all our vehicle is used up
//			break;
//		}
//		else
//		{
//			// compute remaning power after combat
//			remainingPower *= applied / providing;
//			debug("\tremainingPower=%5.2f\n", remainingPower);
//		}
//		debug("\n");
//		
//	}
//	debug("\n");
//	
//}
//
double getVehicleDestructionGain(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	double unitDestructionGain = aiData.unitDestructionGains.at(vehicle->unit_id);
	double damageCoefficient = 1.00 + conf.ai_combat_damage_destruction_value_coefficient * getVehicleRelativeDamage(vehicleId);
	return damageCoefficient * unitDestructionGain;
}

bool isVendettaStoppedWith(int enemyFactionId)
{
	return (aiFactionInfo->diplo_status[enemyFactionId] & DIPLO_VENDETTA) != 0 && (aiFaction->diplo_status[enemyFactionId] & DIPLO_VENDETTA) == 0;
}

