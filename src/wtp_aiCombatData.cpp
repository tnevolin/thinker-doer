
#include "wtp_aiCombatData.h"

#include "wtp_aiData.h"

// CombatEffectTable

void CombatEffectTable::clear()
{
	combatModeEffects.clear();
}

void CombatEffectTable::setCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, COMBAT_MODE combatMode, double value)
{
	int key = FactionUnitCombat::encodeKey(FactionUnit::encodeKey(attackerFactionId, attackerUnitId), FactionUnit::encodeKey(defenderFactionId, defenderUnitId), engagementMode);
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		combatModeEffects.emplace(key, CombatModeEffect());
	}
	CombatModeEffect &combatModeEffect = combatModeEffects.at(key);
	combatModeEffect.combatMode = combatMode;
	combatModeEffect.value = value;
}

CombatModeEffect const &CombatEffectTable::getCombatModeEffect(int attackerKey, int defenderKey, ENGAGEMENT_MODE engagementMode)
{
	int key = FactionUnitCombat::encodeKey(attackerKey, defenderKey, engagementMode);
	
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		FactionUnit attackerFactionUnit(attackerKey);
		FactionUnit defenderFactionUnit(defenderKey);
		
		CombatModeEffect combatModeEffect = getUnitCombatModeEffect(attackerFactionUnit.factionId, attackerFactionUnit.unitId, defenderFactionUnit.factionId, defenderFactionUnit.unitId, engagementMode);
		combatModeEffects.emplace(key, combatModeEffect);
		
	}
	
	return combatModeEffects.at(key);
	
}

CombatModeEffect const &CombatEffectTable::getCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode)
{
	return getCombatModeEffect(FactionUnit::encodeKey(attackerFactionId, attackerUnitId), FactionUnit::encodeKey(defenderFactionId, defenderUnitId), engagementMode);
}

double CombatEffectTable::getCombatEffect(int attackerKey, int defenderKey, ENGAGEMENT_MODE engagementMode)
{
	return combatModeEffects.at(FactionUnitCombat::encodeKey(attackerKey, defenderKey, engagementMode)).value;
}

double CombatEffectTable::getCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode)
{
	return getCombatEffect(FactionUnit::encodeKey(attackerFactionId, attackerUnitId), FactionUnit::encodeKey(defenderFactionId, defenderUnitId), engagementMode);
}

double CombatEffectTable::getVehicleCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode)
{
	VEH *attackerVehicle = getVehicle(attackerVehicleId);
	VEH *defenderVehicle = getVehicle(defenderVehicleId);
	return getCombatEffect(attackerVehicle->faction_id, attackerVehicle->unit_id, defenderVehicle->faction_id, defenderVehicle->unit_id, engagementMode);
}

// TileCombatEffectTable

void TileCombatEffectTable::clear()
{
	combatModeEffects.clear();
}

void TileCombatEffectTable::setCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, COMBAT_MODE combatMode, double value)
{
	int key = FactionUnitCombat::encodeKey(FactionUnit::encodeKey(attackerFactionId, attackerUnitId), FactionUnit::encodeKey(defenderFactionId, defenderUnitId), engagementMode);
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		combatModeEffects.emplace(key, CombatModeEffect());
	}
	CombatModeEffect &combatModeEffect = combatModeEffects.at(key);
	combatModeEffect.combatMode = combatMode;
	combatModeEffect.value = value;
}

CombatModeEffect const &TileCombatEffectTable::getCombatModeEffect(int attackerKey, int defenderKey, ENGAGEMENT_MODE engagementMode)
{
	int key = FactionUnitCombat::encodeKey(attackerKey, defenderKey, engagementMode);
	
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		FactionUnit attackerFactionUnit(attackerKey);
		FactionUnit defenderFactionUnit(defenderKey);
		bool atTile = playerAssault ? attackerFactionUnit.factionId == playerFactionId : defenderFactionUnit.factionId == playerFactionId;

		CombatModeEffect combatModeEffect = getTileCombatModeEffect(combatEffectTable, attackerFactionUnit.factionId, attackerFactionUnit.unitId, defenderFactionUnit.factionId, defenderFactionUnit.unitId, engagementMode, tileCombatModifiers, atTile);
		combatModeEffects.emplace(key, combatModeEffect);
		
	}
	
	return combatModeEffects.at(key);
	
}

CombatModeEffect const &TileCombatEffectTable::getCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode)
{
	return getCombatModeEffect(FactionUnit::encodeKey(attackerFactionId, attackerUnitId), FactionUnit::encodeKey(defenderFactionId, defenderUnitId), engagementMode);
}

double TileCombatEffectTable::getCombatEffect(int attackerKey, int defenderKey, ENGAGEMENT_MODE engagementMode)
{
	return combatModeEffects.at(FactionUnitCombat::encodeKey(attackerKey, defenderKey, engagementMode)).value;
}

double TileCombatEffectTable::getCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode)
{
	return getCombatEffect(FactionUnit::encodeKey(attackerFactionId, attackerUnitId), FactionUnit::encodeKey(defenderFactionId, defenderUnitId), engagementMode);
}

double TileCombatEffectTable::getVehicleCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode)
{
	VEH *attackerVehicle = getVehicle(attackerVehicleId);
	VEH *defenderVehicle = getVehicle(defenderVehicleId);
	return getCombatEffect(attackerVehicle->faction_id, attackerVehicle->unit_id, defenderVehicle->faction_id, defenderVehicle->unit_id, engagementMode);
}

// CombatData

///*
//Computes combat effects with tile combat bonuses.
//*/
//void CombatData::initialize(robin_hood::unordered_flat_map<int, std::vector<int>> const &assailantFactionUnitIds, robin_hood::unordered_flat_map<int, std::vector<int>> const &protectorFactionUnitIds, MAP *tile)
//{
//	TileInfo &tileInfo = aiData.getTileInfo(tile);
//	
//	CombatEffectTableData.combatModeEffects.clear();
//	computed = false;
//	
//	for (robin_hood::pair<int, std::vector<int>> const &assailantFactionUnitIdEntry : assailantFactionUnitIds)
//	{
//		int assailantFactionId = assailantFactionUnitIdEntry.first;
//		std::vector<int> assailantUnitIds = assailantFactionUnitIdEntry.second;
//		
//		for (int assailantUnitId : assailantUnitIds)
//		{
//			for (robin_hood::pair<int, std::vector<int>> const &protectorFactionUnitIdEntry : protectorFactionUnitIds)
//			{
//				int protectorFactionId = protectorFactionUnitIdEntry.first;
//				std::vector<int> protectorUnitIds = protectorFactionUnitIdEntry.second;
//				
//				for (int protectorUnitId : protectorUnitIds)
//				{
//					// assailant attacks
//					
//					for (ENGAGEMENT_MODE engagementMode : {EM_MELEE, EM_ARTILLERY})
//					{
//						COMBAT_MODE combatMode = getCombatMode(engagementMode, protectorUnitId);
//						double combatEffect = aiData.combatEffectData.getCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, engagementMode);
//						if (combatEffect == 0.0)
//							continue;
//						
//						// protector receives terrain bonus
//						AttackTriad attackTriad = getAttackTriad(assailantUnitId, protectorUnitId);
//						double tileCombatEffect =
//							combatEffect
//							* tileInfo.sensorOffenseMultipliers.at(assailantFactionId)
//							/ tileInfo.sensorDefenseMultipliers.at(protectorFactionId)
//							/ tileInfo.structureDefenseMultipliers.at(attackTriad)
//						;
//						
//						if (tileCombatEffect == 0.0)
//							continue;
//							
//						combatEffectData.setCombatModeEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, engagementMode, combatMode, tileCombatEffect);
//						
//					}
//					
//					// protector attacks
//					
//					for (ENGAGEMENT_MODE engagementMode : {EM_MELEE, EM_ARTILLERY})
//					{
//						COMBAT_MODE combatMode = getCombatMode(engagementMode, assailantUnitId);
//						double combatEffect = aiData.combatEffectData.getCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, engagementMode);
//						if (combatEffect == 0.0)
//							continue;
//						
//						// assailant does not receive terrain bonus
//						double tileCombatEffect =
//							combatEffect
//							* tileInfo.sensorOffenseMultipliers.at(protectorFactionId)
//							/ tileInfo.sensorDefenseMultipliers.at(assailantFactionId)
//						;
//							
//						combatEffectData.setCombatModeEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, engagementMode, combatMode, tileCombatEffect);
//						
//					}
//					
//				}
//				
//			}
//			
//		}
//		
//	}
//	
//}
//
void CombatData::clearAssailants()
{
	assailants.clear();
	assailantWeight = 0.0;
	computed = false;
}

void CombatData::clearProtectors()
{
	protectors.clear();
	protectorWeight = 0.0;
	computed = false;
}

void CombatData::addAssailantUnit(int factionId, int unitId, double weight)
{
	int key = FactionUnit::encodeKey(factionId, unitId);
	
	if (assailants.find(key) == assailants.end())
	{
		assailants.emplace(key, 0.0);
	}
	assailants.at(key) += weight;
	
	// summarize weight
	
	assailantWeight += weight;
	
	// clear computed flag
	
	computed = false;
	
	if (DEBUG)
	{
		debug("CombatData::addAssailantUnit\n");
		debug("\tassailants\n");
		for (robin_hood::pair<int, double> &assailantEntry : assailants)
		{
			int assailantKey = assailantEntry.first;
			double assailantWeight = assailantEntry.second;
			FactionUnit assailantFactionUnit(assailantKey);
			
			debug("\t\t%-24s %-32s %5.2f\n", MFactions[assailantFactionUnit.factionId].noun_faction, Units[assailantFactionUnit.unitId].name, assailantWeight);
			
		}
		
	}
		
}

void CombatData::addProtectorUnit(int factionId, int unitId, double weight)
{
	int key = FactionUnit::encodeKey(factionId, unitId);
	
	if (protectors.find(key) == protectors.end())
	{
		protectors.emplace(key, 0.0);
	}
	protectors.at(key) += weight;
	
	// summarize weight
	
	protectorWeight += weight;
	
	// clear computed flag
	
	computed = false;
	
}

void CombatData::addAssailant(int vehicleId, double weight)
{
	VEH *vehicle = getVehicle(vehicleId);
	
	double strengthMultiplier = getVehicleStrenghtMultiplier(vehicleId);
	weight *= strengthMultiplier;
	
	addAssailantUnit(vehicle->faction_id, vehicle->unit_id, weight);
	
}

void CombatData::addProtector(int vehicleId, double weight)
{
	VEH *vehicle = getVehicle(vehicleId);
	
	double strengthMultiplier = getVehicleStrenghtMultiplier(vehicleId);
	weight *= strengthMultiplier;
	
	addProtectorUnit(vehicle->faction_id, vehicle->unit_id, weight);
	
}

void CombatData::removeAssailantUnit(int factionId, int unitId, double weight)
{
	int key = FactionUnit::encodeKey(factionId, unitId);
	
	if (assailants.find(key) == assailants.end())
	{
		return;
	}
	assailants.at(key) = std::max(0.0, assailants.at(key) - weight);
	
	// clear computed flag
	
	computed = false;
	
}

void CombatData::removeProtectorUnit(int factionId, int unitId, double weight)
{
	int key = FactionUnit::encodeKey(factionId, unitId);
	
	if (protectors.find(key) == protectors.end())
	{
		return;
	}
	protectors.at(key) = std::max(0.0, protectors.at(key) - weight);
	
	// clear computed flag
	
	computed = false;
	
}

double CombatData::getRemainingAssailantWeight()
{
	compute();
	return remainingAssailantWeight;
}

double CombatData::getRemainingProtectorWeight()
{
	compute();
	return remainingProtectorWeight;
}

/*
Returns unit effect
*/
double CombatData::getAssailantUnitEffect(int factionId, int unitId)
{
	compute();
	
	int assailantKey = FactionUnit::encodeKey(factionId, unitId);
	
	if (assailantEffects.find(assailantKey) != assailantEffects.end())
	{
		return assailantEffects.at(assailantKey);
	}
	
	double bestAssailantEffect = 0.0;
	
	for (robin_hood::pair<int, double> remainingProtectorEntry : remainingProtectors)
	{
		int remainingProtectorKey = remainingProtectorEntry.first;
		
		// assailant attacks
		
		for (ENGAGEMENT_MODE engagementMode : {EM_MELEE, EM_ARTILLERY})
		{
			CombatModeEffect const &combatModeEffect = tileCombatEffectTable.getCombatModeEffect(assailantKey, remainingProtectorKey, engagementMode);
			double combatEffect = combatModeEffect.value;
			if (combatEffect == 0.0)
				continue;
			
			// max effects
			
			double assailantEffect = combatEffect;
			bestAssailantEffect = std::max(bestAssailantEffect, assailantEffect);
			
		}
		
		// protector attacks
		
		for (ENGAGEMENT_MODE engagementMode : {EM_MELEE, EM_ARTILLERY})
		{
			CombatModeEffect const &combatModeEffect = tileCombatEffectTable.getCombatModeEffect(remainingProtectorKey, assailantKey, engagementMode);
			double combatEffect = combatModeEffect.value;
			if (combatEffect == 0.0)
				continue;
			
			// max effects
			
			double assailantEffect = 1.0 / combatEffect;
			bestAssailantEffect = std::max(bestAssailantEffect, assailantEffect);
			
		}
		
	}
	
	assailantEffects.emplace(assailantKey, bestAssailantEffect);
	
	return bestAssailantEffect;
	
}

/*
Returns unit effect.
*/
double CombatData::getProtectorUnitEffect(int factionId, int unitId)
{
	compute();
	
	int protectorKey = FactionUnit::encodeKey(factionId, unitId);
	
	if (protectorEffects.find(protectorKey) != protectorEffects.end())
	{
		return protectorEffects.at(protectorKey);
	}
	
	double bestProtectorEffect = 0.0;
	
	for (robin_hood::pair<int, double> remainingAssailantEntry : remainingAssailants)
	{
		int remainingAssailantKey = remainingAssailantEntry.first;
		
		// protector attacks
		
		for (ENGAGEMENT_MODE engagementMode : {EM_MELEE, EM_ARTILLERY})
		{
			double combatEffect = tileCombatEffectTable.getCombatEffect(protectorKey, remainingAssailantKey, engagementMode);
			if (combatEffect == 0.0)
				continue;
			
			// max effects
			
			double protectorEffect = combatEffect;
			bestProtectorEffect = std::max(bestProtectorEffect, protectorEffect);
			
		}
		
		// assailant attacks
		
		for (ENGAGEMENT_MODE engagementMode : {EM_MELEE, EM_ARTILLERY})
		{
			double combatEffect = tileCombatEffectTable.getCombatEffect(remainingAssailantKey, protectorKey, engagementMode);
			if (combatEffect == 0.0)
				continue;
			
			// max effects
			
			double protectorEffect = 1.0 / combatEffect;
			bestProtectorEffect = std::max(bestProtectorEffect, protectorEffect);
			
		}
		
	}
	
	protectorEffects.emplace(protectorKey, bestProtectorEffect);
	
	return bestProtectorEffect;
	
}

double CombatData::getAssailantVehicleEffect(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	return getAssailantUnitEffect(vehicle->faction_id, vehicle->unit_id);
}

double CombatData::getProtectorVehicleEffect(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);
	return getProtectorUnitEffect(vehicle->faction_id, vehicle->unit_id);
}

bool CombatData::isSufficientAssault()
{
	if (assailants.size() == 0)
		return false;
	else if (protectors.size() == 0)
		return true;
	
	compute();
	
	return remainingProtectorWeight > 0.0;
	
}

bool CombatData::isSufficientProtect()
{
	if (assailants.size() == 0)
		return true;
	else if (protectors.size() == 0)
		return false;
	
	compute();
	
	return remainingProtectorWeight > 0.0;
	
}

void CombatData::compute()
{
	constexpr bool TRACE = DEBUG && true;
	
	if (computed)
		return;
	
	if (TRACE) { debug("CombatData::compute\n"); }
	
	// clear effects
	
	assailantEffects.clear();
	protectorEffects.clear();
	
	// reset remaining weights
	
	this->remainingAssailants = this->assailants;
	this->remainingProtectors = this->protectors;
	
	if (TRACE)
	{
		debug("\tassailants\n");
		for (robin_hood::pair<int, double> &assailantEntry : this->remainingAssailants)
		{
			int assailantKey = assailantEntry.first;
			double assailantWeight = assailantEntry.second;
			FactionUnit assailantFactionUnit(assailantKey);
			
			debug("\t\t%-24s %-32s %5.2f\n", MFactions[assailantFactionUnit.factionId].noun_faction, Units[assailantFactionUnit.unitId].name, assailantWeight);
			
		}
		
		debug("\tprotectors\n");
		for (robin_hood::pair<int, double> &protectorEntry : this->remainingProtectors)
		{
			int protectorKey = protectorEntry.first;
			double protectorWeight = protectorEntry.second;
			FactionUnit protectorFactionUnit(protectorKey);
			
			debug("\t\t%-24s %-32s %5.2f\n", MFactions[protectorFactionUnit.factionId].noun_faction, Units[protectorFactionUnit.unitId].name, protectorWeight);
			
		}
		
	}
	
	// resolve artillery duels
	
	if (TRACE) { debug("\tresolve artillery duels\n"); }
	bool assailantArtillerySurvives = false;
	for (robin_hood::unordered_flat_map<int, double>::iterator assailantIterator = this->remainingAssailants.begin(); assailantIterator != this->remainingAssailants.end(); )
	{
		int assailantKey = assailantIterator->first;
		double assailantWeight = assailantIterator->second;
		FactionUnit assailantFactionUnit(assailantKey);
		
		if (!isArtilleryUnit(assailantFactionUnit.unitId))
		{
			assailantIterator++;
			continue;
		}
		
		if (assailantWeight <= 0.0)
		{
			assailantIterator = this->remainingAssailants.erase(assailantIterator);
			continue;
		}
		
		for (robin_hood::unordered_flat_map<int, double>::iterator protectorIterator = this->remainingProtectors.begin(); protectorIterator != this->remainingProtectors.end(); )
		{
			int protectorKey = protectorIterator->first;
			double protectorWeight = protectorIterator->second;
			FactionUnit protectorFactionUnit(protectorKey);
			
			if (!isArtilleryUnit(protectorFactionUnit.unitId))
			{
				protectorIterator++;
				continue;
			}
			
			if (protectorWeight <= 0.0)
			{
				protectorIterator = this->remainingProtectors.erase(protectorIterator);
				continue;
			}
			if (TRACE) { debug("\t\t-\n"); }
			if (TRACE) { debug("\t\tassailantKey=%d protectorKey=%d\n", assailantKey, protectorKey); }
			if (TRACE) { debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight); }
			
			double combatEffect = tileCombatEffectTable.getCombatEffect(assailantFactionUnit.encodeKey(), protectorFactionUnit.encodeKey(), EM_ARTILLERY);
			if (TRACE) { debug("\t\tcombatEffect=%5.2f\n", combatEffect); }
			
			resolveMutualCombat(combatEffect, assailantWeight, protectorWeight);
			if (TRACE) { debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight); }
			
			if (protectorWeight <= 0.0)
			{
				// protector is destroyed
				protectorIterator = this->remainingProtectors.erase(protectorIterator);
			}
			else
			{
				// protector survives
				protectorIterator->second = protectorWeight;
				protectorIterator++;
			}
			
			if (assailantWeight <= 0.0)
			{
				// assailant is destroyed
				break;
			}
			
		}
		
		if (assailantWeight <= 0.0)
		{
			// assailant is destroyed
			assailantIterator = this->remainingAssailants.erase(assailantIterator);
		}
		else
		{
			// assailant survives
			assailantIterator->second = assailantWeight;
			assailantArtillerySurvives = true;
			if (TRACE) { debug("\t\tassailantArtillerySurvives\n"); }
			break;
		}
		
	}
	
	// halve protector non-artillery weights because of bombardment
	
	if (assailantArtillerySurvives)
	{
		for (robin_hood::pair<int, double> &protectorEntry : this->remainingProtectors)
		{
			int protectorKey = protectorEntry.first;
			double protectorWeight = protectorEntry.second;
			FactionUnit protectorFactionUnit(protectorKey);
			int protectorUnitId = protectorFactionUnit.unitId;
			
			if (protectorWeight <= 0.0)
				continue;
			
			if (isArtilleryUnit(protectorUnitId))
				continue;
			
			protectorEntry.second /= 2.0;
			
		}
			
	}
	
	// melee cycle
	
	if (TRACE) { debug("\tmelee cycle\n"); }
	while (true)
	{
		if (TRACE) { debug("\t\t-\n"); }
		
		FactionUnitCombatEffect bestAssailantProtectorMeleeEffect = getBestAttackerDefenderMeleeEffect(tileCombatEffectTable, this->remainingAssailants, this->remainingProtectors);
		FactionUnitCombatEffect bestProtectorAssailantMeleeEffect = getBestAttackerDefenderMeleeEffect(tileCombatEffectTable, this->remainingProtectors, this->remainingAssailants);
		if (TRACE)
		{
			debug
			(
				"\t\tbestAssailantProtectorMeleeEffect: effect=%5.2f attackerFactionId=%d attackerUnitId=%3d defenderFactionId=%d defenderUnitId=%3d\n"
				, bestAssailantProtectorMeleeEffect.effect
				, bestAssailantProtectorMeleeEffect.factionUnitCombat.attackerFactionUnit.factionId, bestAssailantProtectorMeleeEffect.factionUnitCombat.attackerFactionUnit.unitId
				, bestAssailantProtectorMeleeEffect.factionUnitCombat.defenderFactionUnit.factionId, bestAssailantProtectorMeleeEffect.factionUnitCombat.defenderFactionUnit.unitId
			);
			debug
			(
				"\t\tbestProtectorAssailantMeleeEffect: effect=%5.2f attackerFactionId=%d attackerUnitId=%3d defenderFactionId=%d defenderUnitId=%3d\n"
				, bestProtectorAssailantMeleeEffect.effect
				, bestProtectorAssailantMeleeEffect.factionUnitCombat.attackerFactionUnit.factionId, bestProtectorAssailantMeleeEffect.factionUnitCombat.attackerFactionUnit.unitId
				, bestProtectorAssailantMeleeEffect.factionUnitCombat.defenderFactionUnit.factionId, bestProtectorAssailantMeleeEffect.factionUnitCombat.defenderFactionUnit.unitId
			);
		}
		
		if (bestAssailantProtectorMeleeEffect.effect <= 0.0)
		{
			// assailant cannot attack
			if (TRACE) { debug("\t\tassailant cannot attack\n"); }
			break;
		}
		else if (bestProtectorAssailantMeleeEffect.effect >= 1.0 / bestAssailantProtectorMeleeEffect.effect)
		{
			// protector attacks because it is better than defend
			if (TRACE) { debug("\t\tprotector attacks because it is better than defend\n"); }
			
			double combatEffect = bestProtectorAssailantMeleeEffect.effect;
			int assailantKey = bestProtectorAssailantMeleeEffect.factionUnitCombat.defenderFactionUnit.encodeKey();
			int protectorKey = bestProtectorAssailantMeleeEffect.factionUnitCombat.attackerFactionUnit.encodeKey();
			
			double assailantWeight = this->remainingAssailants.at(assailantKey);
			double protectorWeight = this->remainingProtectors.at(protectorKey);
			if (TRACE) { debug("\t\tassailantKey=%d protectorKey=%d\n", assailantKey, protectorKey); }
			if (TRACE) { debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight); }
			
			if (TRACE) { debug("\t\tcombatEffect=%5.2f\n", combatEffect); }
			resolveMutualCombat(combatEffect, protectorWeight, assailantWeight);
			if (TRACE) { debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight); }
			
			this->remainingAssailants.at(assailantKey) = assailantWeight;
			this->remainingProtectors.at(protectorKey) = protectorWeight;
			
		}
		else
		{
			// assailant attacks because protector does not want to
			if (TRACE) { debug("\t\tassailant attacks because protector does not want to\n"); }
			
			double combatEffect = bestAssailantProtectorMeleeEffect.effect;
			int assailantKey = bestAssailantProtectorMeleeEffect.factionUnitCombat.attackerFactionUnit.encodeKey();
			int protectorKey = bestAssailantProtectorMeleeEffect.factionUnitCombat.defenderFactionUnit.encodeKey();
			
			double assailantWeight = this->remainingAssailants.at(assailantKey);
			double protectorWeight = this->remainingProtectors.at(protectorKey);
			if (TRACE) { debug("\t\tassailantKey=%d protectorKey=%d\n", assailantKey, protectorKey); }
			if (TRACE) { debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight); }
			
			if (TRACE) { debug("\t\tcombatEffect=%5.2f\n", combatEffect); }
			resolveMutualCombat(combatEffect, assailantWeight, protectorWeight);
			if (TRACE) { debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight); }
			
			this->remainingAssailants.at(assailantKey) = assailantWeight;
			this->remainingProtectors.at(protectorKey) = protectorWeight;
			
		}
		
	}
	
	// count remaining melee unit weights
	
	remainingAssailantWeight = 0.0;
	for (robin_hood::pair<int, double> &assailantEntry : this->remainingAssailants)
	{
		int assailantKey = assailantEntry.first;
		double assailantWeight = assailantEntry.second;
		int assailantUnitId = FactionUnit(assailantKey).unitId;
		
		if (assailantWeight <= 0.0)
			continue;
			
		if (!isMeleeUnit(assailantUnitId))
			continue;
		
		remainingAssailantWeight += assailantWeight;
		
	}
	sufficientAssault = remainingAssailantWeight > 0.0;
	
	remainingProtectorWeight = 0.0;
	for (robin_hood::pair<int, double> &protectorEntry : this->remainingProtectors)
	{
		int protectorKey = protectorEntry.first;
		double protectorWeight = protectorEntry.second;
		int protectorUnitId = FactionUnit(protectorKey).unitId;
		
		if (protectorWeight <= 0.0)
			continue;
			
		if (!isMeleeUnit(protectorUnitId))
			continue;
		
		remainingProtectorWeight += protectorWeight;
		
	}
	sufficientProtect = remainingProtectorWeight > 0.0;
	
	if (TRACE) { debug("\tremainingAssailantWeight=%5.2f remainingProtectorWeight=%5.2f\n", remainingAssailantWeight, remainingProtectorWeight); }
	computed = true;
	
}

void resolveMutualCombat(double combatEffect, double &attackerHealth, double &defenderHealth)
{
	double defenderDamage = attackerHealth * combatEffect;
	if (defenderDamage <= defenderHealth)
	{
		attackerHealth = 0.0;
		defenderHealth -= defenderDamage;
	}
	else
	{
		attackerHealth *= (defenderDamage / defenderHealth - 1.0);
		defenderHealth = 0.0;
	}
}


FactionUnitCombatEffect getBestAttackerDefenderMeleeEffect(TileCombatEffectTable &tileCombatEffectTable, robin_hood::unordered_flat_map<int, double> &attackers, robin_hood::unordered_flat_map<int, double> &defenders)
{
	int bestAttackerKey = 0;
	int bestDefenderKey = 0;
	double bestCombatEffect = 0.0;
	
	for (robin_hood::pair<int, double> &attackerEntry : attackers)
	{
		int attackerKey = attackerEntry.first;
		double attackerWeight = attackerEntry.second;
		FactionUnit attackerFactionUnit(attackerKey);
		
		if (!isMeleeUnit(attackerFactionUnit.unitId))
			continue;
		
		if (attackerWeight <= 0.0)
			continue;
		
		int bestAttackerDefenderKey = 0;
		double bestAttackerDefenderCombatEffect = DBL_MAX;
		
		for (robin_hood::pair<int, double> &defenderEntry : defenders)
		{
			int defenderKey = defenderEntry.first;
			double defenderWeight = defenderEntry.second;
			FactionUnit defenderFactionUnit(defenderKey);
			
			if (!isMeleeUnit(defenderFactionUnit.unitId))
				continue;
			
			if (defenderWeight <= 0.0)
				continue;
			
			double combatEffect = tileCombatEffectTable.getCombatEffect(attackerKey, defenderKey, EM_MELEE);
			if (combatEffect <= 0.0)
				continue;
			
			// defender looks for the lowest effect
			
			if (combatEffect < bestAttackerDefenderCombatEffect)
			{
				bestAttackerDefenderKey = defenderKey;
				bestAttackerDefenderCombatEffect = combatEffect;
			}
			
		}
		
		// no defender selected
		
		if (bestAttackerDefenderCombatEffect == DBL_MAX)
		{
			continue;
		}
		
		// attacker looks for the lowest effect
		
		if (bestAttackerDefenderCombatEffect > bestCombatEffect)
		{
			bestAttackerKey = attackerKey;
			bestDefenderKey = bestAttackerDefenderKey;
			bestCombatEffect = bestAttackerDefenderCombatEffect;
		}
		
	}
	
	return FactionUnitCombatEffect(FactionUnitCombat(bestAttackerKey, bestDefenderKey, EM_MELEE), bestCombatEffect);
	
}

/*
Computes generic unit to unit combat effect.
*/
CombatModeEffect getUnitCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode)
{
	Profiling::start("- calculateCombatEffect");
	
	COMBAT_MODE combatMode = getCombatMode(engagementMode, defenderUnitId);
	
	double combatEffect;
	
	switch (combatMode)
	{
	case CM_MELEE:
		combatEffect = getMeleeRelativeUnitStrength(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId);
		break;
		
	case CM_ARTILLERY_DUEL:
		combatEffect = getArtilleryDuelRelativeUnitStrength(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId);
		break;
		
	case CM_BOMBARDMENT:
		combatEffect = getUnitBombardmentDamage(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId);
		break;
		
	default:
		debug("ERROR: Unexpected combatMode.\n");
		combatEffect = 0.0;
		
	}
	
	CombatModeEffect combatModeEffect = {combatMode, combatEffect};
	
	Profiling::stop("- calculateCombatEffect");
	
	return combatModeEffect;
	
}

CombatModeEffect getTileCombatModeEffect(CombatEffectTable *combatEffectTable, int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, TileCombatModifiers &tileCombatModifiers, bool atTile)
{
	CombatModeEffect combatModeEffect = combatEffectTable->getCombatModeEffect(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode);
	
	combatModeEffect.value *=
		1.0
		* tileCombatModifiers.sensorOffenseMultipliers.at(attackerFactionId)
		/ tileCombatModifiers.sensorDefenseMultipliers.at(defenderFactionId)
	;
	
	if (atTile)
	{
		AttackTriad attackTriad = getAttackTriad(attackerUnitId, defenderUnitId);
		combatModeEffect.value /= tileCombatModifiers.terrainDefenseMultipliers.at(attackTriad);
	}
	
	return combatModeEffect;
	
}

//double calculateTileCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode, MAP *tile)
//{
//	VEH *attackerVehicle = getVehicle(attackerVehicleId);
//	VEH *defenderVehicle = getVehicle(defenderVehicleId);
//	TileInfo &tileInfo = aiData.getTileInfo(tile);
//	AttackTriad attackTriad = getAttackTriad(attackerVehicle->unit_id, defenderVehicle->unit_id);
//	
//	double CombatModeEffect = aiData.combatEffectData.getCombatEffect(attackerVehicle->faction_id, attackerVehicle->unit_id, defenderVehicle->faction_id, defenderVehicle->unit_id, engagementMode);
//	if (CombatModeEffect == 0.0)
//		return 0.0;
//	
//	double sensorOffenseMultiplier = tileInfo.sensorOffenseMultipliers.at(attackerVehicle->faction_id);
//	double sensorDefenseMultiplier = tileInfo.sensorDefenseMultipliers.at(defenderVehicle->faction_id);
//	double terrainDefenseMultiplier = tileInfo.structureDefenseMultipliers.at(attackTriad);
//	
//	if (sensorOffenseMultiplier == 0.0 || sensorDefenseMultiplier == 0.0 || terrainDefenseMultiplier == 0.0)
//		return 0.0;
//	
//	double combatEffect =
//		CombatModeEffect
//		* tileInfo.sensorOffenseMultipliers.at(attackerVehicle->faction_id)
//		/ tileInfo.sensorDefenseMultipliers.at(defenderVehicle->faction_id)
//		/ tileInfo.structureDefenseMultipliers.at(attackTriad)
//	;
//	
//	return combatEffect;
//	
//}
//
/*
Calculates relative unit strength for melee attack.
How many defender units can attacker destroy until own complete destruction.
*/
double getMeleeRelativeUnitStrength(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId)
{
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = getUnitOffenseValue(attackerUnitId);
	int defenderDefenseValue = getUnitDefenseValue(defenderUnitId);
	Triad attackerTriad = (Triad) attackerUnit->triad();
	Triad defenderTriad = (Triad) defenderUnit->triad();
	
	// weapon to weapon combat
	
	if (attackerTriad == TRIAD_AIR && defenderTriad == TRIAD_AIR && has_abil(defenderUnitId, ABL_AIR_SUPERIORITY))
	{
		defenderDefenseValue = getUnitOffenseValue(defenderUnitId);
	}
	
	// attacker should be melee unit
	
	if (!isMeleeUnit(attackerUnitId))
	{
		return 0.0;
	}
	
	// calculate relative strength
	
	double relativeStrength;
	
	if (isPsiCombat(attackerUnitId, defenderUnitId))
	{
		relativeStrength = getPsiCombatBaseOdds(attackerTriad);
		
		// reactor
		// psi combat ignores reactor
		
		// abilities
		
		if (unit_has_ability(attackerUnitId, ABL_EMPATH))
		{
			relativeStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_empath_song_vs_psi);
		}
		
		if (unit_has_ability(defenderUnitId, ABL_TRANCE))
		{
			relativeStrength /= getPercentageBonusMultiplier(Rules->combat_bonus_trance_vs_psi);
		}
		
		// hybrid item
		
		if (attackerUnit->weapon_id == WPN_RESONANCE_LASER || attackerUnit->weapon_id == WPN_RESONANCE_LASER)
		{
			relativeStrength *= 1.25;
		}
		
		if (defenderUnit->armor_id == ARM_RESONANCE_3_ARMOR || defenderUnit->armor_id == ARM_RESONANCE_8_ARMOR)
		{
			relativeStrength /= 1.25;
		}
		
		// PLANET bonus
		
		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
 		if (conf.planet_defense_bonus)
		{
			relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);
		}
		
		// gear bonus
		
		if (conf.conventional_power_psi_percentage != 0)
		{
			if (attackerOffenseValue > 0)
			{
				relativeStrength *= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * attackerOffenseValue);
			}
			
			if (defenderDefenseValue > 0)
			{
				relativeStrength /= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * defenderDefenseValue);
			}
			
		}
		
	}
	else
	{
		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;
		
		// reactor
		
		if (!conf.ignore_reactor_power)
		{
			relativeStrength *= (double)attackerUnit->reactor_id / (double)defenderUnit->reactor_id;
		}
		
		// abilities
		
		if (attackerTriad == TRIAD_AIR && unit_has_ability(attackerUnitId, ABL_AIR_SUPERIORITY))
		{
			if (defenderUnit->triad() == TRIAD_AIR)
			{
				relativeStrength *= getPercentageBonusMultiplier(Rules->combat_bonus_air_supr_vs_air);
			}
			else
			{
				relativeStrength *= getPercentageBonusMultiplier(-Rules->combat_penalty_air_supr_vs_ground);
			}
		}
		
		if (defenderTriad == TRIAD_AIR && unit_has_ability(attackerUnitId, ABL_AIR_SUPERIORITY))
		{
			if (attackerUnit->triad() == TRIAD_AIR)
			{
				relativeStrength /= getPercentageBonusMultiplier(Rules->combat_bonus_air_supr_vs_air);
			}
		}
		
		if (unit_has_ability(attackerUnitId, ABL_NERVE_GAS))
		{
			relativeStrength *= 1.5;
		}
		
		// fanatic bonus
		
		relativeStrength *= getFactionFanaticBonusMultiplier(attackerFactionId);
		
	}
	
	// ability applied regardless of combat type
	
	if
	(
		attackerUnit->triad() == TRIAD_LAND && attackerUnit->speed() > 1
		&& unit_has_ability(defenderUnitId, ABL_COMM_JAMMER)
		&& !unit_has_ability(attackerUnitId, ABL_DISSOCIATIVE_WAVE)
	)
	{
		relativeStrength /= getPercentageBonusMultiplier(Rules->combat_comm_jammer_vs_mobile);
	}
	
	if
	(
		attackerUnit->triad() == TRIAD_AIR
		&& unit_has_ability(defenderUnitId, ABL_AAA)
		&& !unit_has_ability(attackerUnitId, ABL_DISSOCIATIVE_WAVE)
	)
	{
		relativeStrength /= getPercentageBonusMultiplier(Rules->combat_aaa_bonus_vs_air);
	}
	
	if (unit_has_ability(attackerUnitId, ABL_SOPORIFIC_GAS) && !isNativeUnit(defenderUnitId))
	{
		relativeStrength *= 1.25;
	}
	
	// faction bonuses
	
	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);
	
	// artifact and no armor probe are explicitly weak
	
	if (defenderUnit->weapon_id == WPN_ALIEN_ARTIFACT || (defenderUnit->weapon_id == WPN_PROBE_TEAM && defenderUnit->armor_id == ARM_NO_ARMOR))
	{
		relativeStrength *= 50.0;
	}
	
	// not anymore
//	// alien fight on half a strength in early game
//	
//	if (attackerFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
//	{
//		relativeStrength /= 2.0;
//	}
//	if (defenderFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
//	{
//		relativeStrength *= 2.0;
//	}
	
	// defending bonus for disengagement
	
	if (attackerTriad != TRIAD_AIR && defenderTriad != TRIAD_AIR && defenderUnit->speed() > attackerUnit->speed() && !has_abil(attackerUnitId, ABL_COMM_JAMMER))
	{
		relativeStrength /= 1.25;
	}
	
	return relativeStrength;
	
}

/**
Calculates relative unit strength for artillery duel attack.
How many defender units can attacker destroy until own complete destruction.
*/
double getArtilleryDuelRelativeUnitStrength(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId)
{
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = getUnitOffenseValue(attackerUnitId);
	int defenderDefenseValue = getUnitDefenseValue(defenderUnitId);
	
	// both units should be artillery capable
	
	if (!isArtilleryUnit(attackerUnitId) || !isArtilleryUnit(defenderUnitId))
	{
		return 0.0;
	}
	
	// calculate relative strength
	
	double relativeStrength;
	
	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
		relativeStrength = getPsiCombatBaseOdds(attackerUnit->triad());
		
		// reactor
		// psi combat ignores reactor
		
		// PLANET bonus
		
		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
		relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);
		
		// gear bonus
		
		if (conf.conventional_power_psi_percentage != 0)
		{
			if (attackerOffenseValue > 0)
			{
				relativeStrength *= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * attackerOffenseValue);
			}
			
			if (defenderDefenseValue > 0)
			{
				relativeStrength /= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * defenderDefenseValue);
			}
			
		}
		
	}
	else
	{
		attackerOffenseValue = Weapon[attackerUnit->weapon_id].offense_value;
		defenderDefenseValue = Weapon[defenderUnit->weapon_id].offense_value;
		
		// get relative strength
		
		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;
		
		// reactor
		
		if (!conf.ignore_reactor_power)
		{
			relativeStrength *= (double)attackerUnit->reactor_id / (double)defenderUnit->reactor_id;
		}
		
	}
	
	// ability applied regardless of combat type
	
	// faction bonuses
	
	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);
	
	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength /= 2.0;
	}
	if (defenderFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength *= 2.0;
	}
	
	// land vs. sea guns bonus
	
	if (attackerUnit->triad() == TRIAD_LAND && defenderUnit->triad() == TRIAD_SEA)
	{
		relativeStrength *= getPercentageBonusMultiplier(Rules->combat_land_vs_sea_artillery);
	}
	else if (attackerUnit->triad() == TRIAD_SEA && defenderUnit->triad() == TRIAD_LAND)
	{
		relativeStrength /= getPercentageBonusMultiplier(Rules->combat_land_vs_sea_artillery);
	}
	
	return relativeStrength;
	
}

/**
Calculates relative bombardment damage for units.
How many defender units can attacker destroy with single shot.
*/
double getUnitBombardmentDamage(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId)
{
	Profiling::start("getUnitBombardmentDamage", "combatEffects");
	
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = Weapon[attackerUnit->weapon_id].offense_value;
	int defenderDefenseValue = Armor[defenderUnit->armor_id].defense_value;
	
	// attacker should be artillery capable and defender should not and should be surface unit
	
	if (!isArtilleryUnit(attackerUnitId) || isArtilleryUnit(defenderUnitId) || defenderUnit->triad() == TRIAD_AIR)
	{
		Profiling::stop("getUnitBombardmentDamage");
		return 0.0;
	}
	
	// calculate relative damage
	
	double relativeStrength = 1.0;
	
	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
		relativeStrength = getPsiCombatBaseOdds(attackerUnit->triad());
		
		// reactor
		// psi combat ignores reactor
		
		// PLANET bonus
		
		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
		relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);
		
		// gear bonus
		
		if (conf.conventional_power_psi_percentage != 0)
		{
			if (attackerOffenseValue > 0)
			{
				relativeStrength *= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * attackerOffenseValue);
			}
			
			if (defenderDefenseValue > 0)
			{
				relativeStrength /= getPercentageBonusMultiplier(conf.conventional_power_psi_percentage * defenderDefenseValue);
			}
			
		}
		
	}
	else
	{
		relativeStrength = (double)attackerOffenseValue / (double)defenderDefenseValue;
		
		// reactor
		
		if (!conf.ignore_reactor_power)
		{
			relativeStrength *= 1.0 / (double)defenderUnit->reactor_id;
		}
		
	}
	
	// faction bonuses
	
	relativeStrength *= getFactionOffenseMultiplier(attackerFactionId);
	relativeStrength /= getFactionDefenseMultiplier(defenderFactionId);
	
	// artifact and no armor probe are explicitly weak
	
	if (defenderUnit->weapon_id == WPN_ALIEN_ARTIFACT || (defenderUnit->weapon_id == WPN_PROBE_TEAM && defenderUnit->armor_id == ARM_NO_ARMOR))
	{
		relativeStrength *= 50.0;
	}
	
	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength /= 2.0;
	}
	if (defenderFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength *= 2.0;
	}
	
	// artillery damage numerator/denominator
	
	relativeStrength *= (double)Rules->artillery_dmg_numerator / (double)Rules->artillery_dmg_denominator;
	
	// divide by 10 to convert damage to units destroyed
	
	Profiling::stop("getUnitBombardmentDamage");
	return relativeStrength / 10.0;
	
}

