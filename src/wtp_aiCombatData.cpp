
#include "wtp_aiCombatData.h"

#include "wtp_aiData.h"

// CombatEffectData

void CombatEffectData::clear()
{
	combatModeEffects.clear();
}

void CombatEffectData::setCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, COMBAT_MODE combatMode, double value)
{
	int key = FactionUnitCombat(FactionUnit(attackerFactionId, attackerUnitId), FactionUnit(defenderFactionId, defenderUnitId)).key;
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		combatModeEffects.emplace(key, std::array<CombatModeEffect, ENGAGEMENT_MODE_COUNT>());
	}
	CombatModeEffect &combatModeEffect = combatModeEffects.at(key).at(engagementMode);
	combatModeEffect.valid = true;
	combatModeEffect.combatMode = combatMode;
	combatModeEffect.value = value;
}

CombatModeEffect const CombatEffectData::getCombatModeEffect(int key, ENGAGEMENT_MODE engagementMode)
{
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		debug("ERROR: no combat effect for given units.\n");
		return CombatModeEffect();
	}
	
	CombatModeEffect const combatModeEffect = combatModeEffects.at(key).at(engagementMode);
	
	return combatModeEffect;

}

double CombatEffectData::getCombatEffect(int attackerFactionUnitKey, int defenderFactionUnitKey, ENGAGEMENT_MODE engagementMode)
{
	int key = FactionUnitCombat(FactionUnit(attackerFactionUnitKey), FactionUnit(defenderFactionUnitKey)).key;
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		debug("ERROR: no combat effect for given units.\n");
		return 0.0;
	}
	
	CombatModeEffect const &combatModeEffect = combatModeEffects.at(key).at(engagementMode);
	if (!combatModeEffect.valid)
		return 0.0;
	
	return combatModeEffect.value;
	
}

double CombatEffectData::getCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode)
{
	int key = FactionUnitCombat(FactionUnit(attackerFactionId, attackerUnitId), FactionUnit(defenderFactionId, defenderUnitId)).key;
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		debug("ERROR: no combat effect for given units.\n");
		return 0.0;
	}
	
	CombatModeEffect const &combatModeEffect = combatModeEffects.at(key).at(engagementMode);
	if (!combatModeEffect.valid)
		return 0.0;
	
	return combatModeEffect.value;
	
}

double CombatEffectData::getCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, COMBAT_MODE combatMode)
{
	int key = FactionUnitCombat(FactionUnit(attackerFactionId, attackerUnitId), FactionUnit(defenderFactionId, defenderUnitId)).key;
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		debug("ERROR: no combat effect for given units.\n");
		return 0.0;
	}
	
	ENGAGEMENT_MODE engagementMode = combatMode == CM_MELEE ? EM_MELEE : EM_ARTILLERY;
	
	CombatModeEffect const &combatModeEffect = combatModeEffects.at(key).at(engagementMode);
	if (!combatModeEffect.valid)
		return 0.0;
	
	if (combatModeEffect.combatMode != combatMode)
		return 0.0;
	
	return combatModeEffect.value;
	
}

double CombatEffectData::getVehicleCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode)
{
	VEH *attackerVehicle = getVehicle(attackerVehicleId);
	VEH *defenderVehicle = getVehicle(defenderVehicleId);
	return getCombatEffect(attackerVehicle->faction_id, attackerVehicle->unit_id, defenderVehicle->faction_id, defenderVehicle->unit_id, engagementMode);
}

// CombatData

void CombatData::initialize(robin_hood::unordered_flat_map<int, std::vector<int>> const &assailantFactionUnitIds, robin_hood::unordered_flat_map<int, std::vector<int>> const &protectorFactionUnitIds, MAP *tile)
{
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	combatEffectData.combatModeEffects.clear();
	assailantEffects.clear();
	protectorEffects.clear();
	
	for (robin_hood::pair<int, std::vector<int>> const &assailantFactionUnitIdEntry : assailantFactionUnitIds)
	{
		int assailantFactionId = assailantFactionUnitIdEntry.first;
		std::vector<int> assailantUnitIds = assailantFactionUnitIdEntry.second;
		
		for (int assailantUnitId : assailantUnitIds)
		{
			int assailantFactionUnitKey = FactionUnit(assailantFactionId, assailantUnitId).key;
			
			for (robin_hood::pair<int, std::vector<int>> const &protectorFactionUnitIdEntry : protectorFactionUnitIds)
			{
				int protectorFactionId = protectorFactionUnitIdEntry.first;
				std::vector<int> protectorUnitIds = protectorFactionUnitIdEntry.second;
				
				for (int protectorUnitId : protectorUnitIds)
				{
					int protectorFactionUnitKey = FactionUnit(protectorFactionId, protectorUnitId).key;
					
					// assailant attacks
					
					for (ENGAGEMENT_MODE engagementMode : {EM_MELEE, EM_ARTILLERY})
					{
						COMBAT_MODE combatMode = getCombatMode(engagementMode, protectorUnitId);
						double combatEffect = aiData.combatEffectData.getCombatEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, combatMode);
						if (combatEffect == 0.0)
							continue;
						
						// protector receives terrain bonus
						AttackTriad attackTriad = getAttackTriad(assailantUnitId, protectorUnitId);
						double tileCombatEffect =
							combatEffect
							* tileInfo.sensorOffenseMultipliers.at(assailantFactionId)
							/ tileInfo.sensorDefenseMultipliers.at(protectorFactionId)
							/ tileInfo.structureDefenseMultipliers.at(attackTriad)
						;
						
						if (tileCombatEffect == 0.0)
							continue;
							
						combatEffectData.setCombatModeEffect(assailantFactionId, assailantUnitId, protectorFactionId, protectorUnitId, engagementMode, combatMode, tileCombatEffect);
						
						// max effects
						
						double assailantEffect = 1.00 * tileCombatEffect;
						double protectorEffect = 0.75 * 1.0 / tileCombatEffect;
						
					}
					
					// protector attacks
					
					for (ENGAGEMENT_MODE engagementMode : {EM_MELEE, EM_ARTILLERY})
					{
						COMBAT_MODE combatMode = getCombatMode(engagementMode, assailantUnitId);
						double combatEffect = aiData.combatEffectData.getCombatEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, combatMode);
						if (combatEffect == 0.0)
							continue;
						
						// assailant does not receive terrain bonus
						double tileCombatEffect =
							combatEffect
							* tileInfo.sensorOffenseMultipliers.at(protectorFactionId)
							/ tileInfo.sensorDefenseMultipliers.at(assailantFactionId)
						;
							
						combatEffectData.setCombatModeEffect(protectorFactionId, protectorUnitId, assailantFactionId, assailantUnitId, engagementMode, combatMode, tileCombatEffect);
						
						// max effects
						
						double assailantEffect = 0.50 * 1.0 / tileCombatEffect;
						double protectorEffect = 1.00 * tileCombatEffect;
						
					}
					
				}
				
			}
			
		}
		
	}
	
}

void CombatData::clearAssailants()
{
	assailants.clear();
	computed = false;
}

void CombatData::clearProtectors()
{
	protectors.clear();
	computed = false;
}

void CombatData::addAssailantUnit(int factionId, int unitId, double weight)
{
	int key = FactionUnit(factionId, unitId).key;
	
	if (assailants.find(key) == assailants.end())
	{
		assailants.emplace(key, 0.0);
	}
	assailants.at(key) += weight;
	
	// clear computed flag and protectorEffects
	
	computed = false;
	protectorEffects.clear();
	
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
	int key = FactionUnit(factionId, unitId).key;
	
	if (protectors.find(key) == protectors.end())
	{
		protectors.emplace(key, 0.0);
	}
	protectors.at(key) += weight;
	
	// clear computed flag and assailantEffects
	
	computed = false;
	assailantEffects.clear();
	
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
	int key = FactionUnit(factionId, unitId).key;
	
	if (assailants.find(key) == assailants.end())
	{
		return;
	}
	assailants.at(key) = std::max(0.0, assailants.at(key) - weight);
	
	// clear computed flag and protectorEffects
	
	computed = false;
	protectorEffects.clear();
	
}

void CombatData::removeProtectorUnit(int factionId, int unitId, double weight)
{
	int key = FactionUnit(factionId, unitId).key;
	
	if (protectors.find(key) == protectors.end())
	{
		return;
	}
	protectors.at(key) = std::max(0.0, protectors.at(key) - weight);
	
	// clear computed flag and assailantEffects
	
	computed = false;
	assailantEffects.clear();
	
}

double CombatData::getInitialAssailantWeight()
{
	if (!initialAssailantWeightComputed)
	{
		double initialAssailantWeight = 0.0;
		for (robin_hood::pair<int, double> const &assailantEntry : assailants)
		{
			double weight = assailantEntry.second;
			initialAssailantWeight += weight;
		}
		initialAssailantWeightComputed = true;
	}
	return initialAssailantWeight;
}

double CombatData::getInitialProtectorWeight()
{
	if (!initialProtectorWeightComputed)
	{
		double initialProtectorWeight = 0.0;
		for (robin_hood::pair<int, double> const &protectorEntry : protectors)
		{
			double weight = protectorEntry.second;
			initialProtectorWeight += weight;
		}
		initialProtectorWeightComputed = true;
	}
	return initialProtectorWeight;
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
Computes assailant prevalence after battle.
*/
double CombatData::getAssailantPrevalence()
{
	compute();
	
	double assailantPrevalence;
	
	if (initialAssailantWeight <= 0.0)
	{
		assailantPrevalence = 0.0;
	}
	else if (initialProtectorWeight <= 0.0)
	{
		assailantPrevalence = INF;
	}
	else if (remainingAssailantWeight <= 0.0 && remainingProtectorWeight >= initialProtectorWeight)
	{
		assailantPrevalence = 0.0;
	}
	else if (remainingProtectorWeight <= 0.0 && remainingAssailantWeight >= initialAssailantWeight)
	{
		assailantPrevalence = INF;
	}
	else if (remainingProtectorWeight > 0.0)
	{
		assailantPrevalence = (initialProtectorWeight - remainingProtectorWeight) / initialProtectorWeight;
	}
	else if (remainingProtectorWeight <= 0.0 && remainingAssailantWeight < initialAssailantWeight)
	{
		assailantPrevalence = initialAssailantWeight / (initialAssailantWeight - remainingAssailantWeight);
	}
	else
	{
		assailantPrevalence = 0.0;
	}
	
	return assailantPrevalence;
	
}

/*
Computes protector prevalence after battle.
*/
double CombatData::getProtectorPrevalence()
{
	double assailantPrevalence = getAssailantPrevalence();
	return assailantPrevalence == 0.0 ? INF : assailantPrevalence == INF ? 0.0 : 1.0 / assailantPrevalence;
}

/*
Returns unit effect
*/
double CombatData::getAssailantUnitEffect(int factionId, int unitId)
{
	int assailantKey = FactionUnit(factionId, unitId).key;
	
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
			CombatModeEffect const &combatModeEffect = combatEffectData.getCombatModeEffect(FactionUnitCombat(assailantKey, remainingProtectorKey).key, engagementMode);
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
			CombatModeEffect const &combatModeEffect = combatEffectData.getCombatModeEffect(FactionUnitCombat(remainingProtectorKey, assailantKey).key, engagementMode);
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
	int protectorKey = FactionUnit(factionId, unitId).key;
	
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
			CombatModeEffect const &combatModeEffect = combatEffectData.getCombatModeEffect(FactionUnitCombat(protectorKey, remainingAssailantKey).key, engagementMode);
			double combatEffect = combatModeEffect.value;
			if (combatEffect == 0.0)
				continue;
			
			// max effects
			
			double protectorEffect = combatEffect;
			bestProtectorEffect = std::max(bestProtectorEffect, protectorEffect);
			
		}
		
		// assailant attacks
		
		for (ENGAGEMENT_MODE engagementMode : {EM_MELEE, EM_ARTILLERY})
		{
			CombatModeEffect const &combatModeEffect = combatEffectData.getCombatModeEffect(FactionUnitCombat(remainingAssailantKey, protectorKey).key, engagementMode);
			double combatEffect = combatModeEffect.value;
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
	
	if (TRACE) debug("CombatData::compute\n");
	
	// create current weights
	
	robin_hood::unordered_flat_map<int, double> assailants(this->assailants);
	robin_hood::unordered_flat_map<int, double> protectors(this->protectors);
	
	if (TRACE)
	{
		debug("\tassailants\n");
		for (robin_hood::pair<int, double> &assailantEntry : assailants)
		{
			int assailantKey = assailantEntry.first;
			double assailantWeight = assailantEntry.second;
			FactionUnit assailantFactionUnit(assailantKey);
			
			debug("\t\t%-24s %-32s %5.2f\n", MFactions[assailantFactionUnit.factionId].noun_faction, Units[assailantFactionUnit.unitId].name, assailantWeight);
			
		}
		
		debug("\tprotectors\n");
		for (robin_hood::pair<int, double> &protectorEntry : protectors)
		{
			int protectorKey = protectorEntry.first;
			double protectorWeight = protectorEntry.second;
			FactionUnit protectorFactionUnit(protectorKey);
			
			debug("\t\t%-24s %-32s %5.2f\n", MFactions[protectorFactionUnit.factionId].noun_faction, Units[protectorFactionUnit.unitId].name, protectorWeight);
			
		}
		
	}
	
	// resolve artillery duels
	
	if (TRACE) debug("\tresolve artillery duels\n");
	bool assailantArtillerySurvives = false;
	for (robin_hood::unordered_flat_map<int, double>::iterator assailantIterator = assailants.begin(); assailantIterator != assailants.end(); )
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
			assailantIterator = assailants.erase(assailantIterator);
			continue;
		}
		
		for (robin_hood::unordered_flat_map<int, double>::iterator protectorIterator = protectors.begin(); protectorIterator != protectors.end(); )
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
				protectorIterator = protectors.erase(protectorIterator);
				continue;
			}
			if (TRACE) debug("\t\t-\n");
			if (TRACE) debug("\t\tassailantKey=%d protectorKey=%d\n", assailantKey, protectorKey);
			if (TRACE) debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight);
			
			double combatEffect = combatEffectData.getCombatEffect(assailantFactionUnit.factionId, assailantFactionUnit.unitId, protectorFactionUnit.factionId, protectorFactionUnit.unitId, EM_ARTILLERY);
			if (TRACE) debug("\t\tcombatEffect=%5.2f\n", combatEffect);
			
			resolveMutualCombat(combatEffect, assailantWeight, protectorWeight);
			if (TRACE) debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight);
			
			if (protectorWeight <= 0.0)
			{
				// protector is destroyed
				protectorIterator = protectors.erase(protectorIterator);
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
			assailantIterator = assailants.erase(assailantIterator);
		}
		else
		{
			// assailant survives
			assailantIterator->second = assailantWeight;
			assailantArtillerySurvives = true;
			if (TRACE) debug("\t\tassailantArtillerySurvives\n");
			break;
		}
		
	}
	
	// halve protector non-artillery weights because of bombardment
	
	if (assailantArtillerySurvives)
	{
		for (robin_hood::pair<int, double> &protectorEntry : protectors)
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
	
	if (TRACE) debug("\tmelee cycle\n");
	while (true)
	{
		if (TRACE) debug("\t\t-\n");
		
		FactionUnitCombatEffect bestAssailantProtectorMeleeEffect = getBestAttackerDefenderMeleeEffect(combatEffectData, assailants, protectors);
		FactionUnitCombatEffect bestProtectorAssailantMeleeEffect = getBestAttackerDefenderMeleeEffect(combatEffectData, protectors, assailants);
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
			if (TRACE) debug("\t\tassailant cannot attack\n");
			break;
		}
		else if (bestProtectorAssailantMeleeEffect.effect >= 1.0 / bestAssailantProtectorMeleeEffect.effect)
		{
			// protector attacks because it is better than defend
			if (TRACE) debug("\t\tprotector attacks because it is better than defend\n");
			
			double combatEffect = bestProtectorAssailantMeleeEffect.effect;
			int assailantKey = bestProtectorAssailantMeleeEffect.factionUnitCombat.defenderFactionUnit.key;
			int protectorKey = bestProtectorAssailantMeleeEffect.factionUnitCombat.attackerFactionUnit.key;
			
			double assailantWeight = assailants.at(assailantKey);
			double protectorWeight = protectors.at(protectorKey);
			if (TRACE) debug("\t\tassailantKey=%d protectorKey=%d\n", assailantKey, protectorKey);
			if (TRACE) debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight);
			
			if (TRACE) debug("\t\tcombatEffect=%5.2f\n", combatEffect);
			resolveMutualCombat(combatEffect, protectorWeight, assailantWeight);
			if (TRACE) debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight);
			
			assailants.at(assailantKey) = assailantWeight;
			protectors.at(protectorKey) = protectorWeight;
			
		}
		else
		{
			// assailant attacks because protector does not want to
			if (TRACE) debug("\t\tassailant attacks because protector does not want to\n");
			
			double combatEffect = bestAssailantProtectorMeleeEffect.effect;
			int assailantKey = bestAssailantProtectorMeleeEffect.factionUnitCombat.attackerFactionUnit.key;
			int protectorKey = bestAssailantProtectorMeleeEffect.factionUnitCombat.defenderFactionUnit.key;
			
			double assailantWeight = assailants.at(assailantKey);
			double protectorWeight = protectors.at(protectorKey);
			if (TRACE) debug("\t\tassailantKey=%d protectorKey=%d\n", assailantKey, protectorKey);
			if (TRACE) debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight);
			
			if (TRACE) debug("\t\tcombatEffect=%5.2f\n", combatEffect);
			resolveMutualCombat(combatEffect, assailantWeight, protectorWeight);
			if (TRACE) debug("\t\tassailantWeight=%5.2f protectorWeight=%5.2f\n", assailantWeight, protectorWeight);
			
			assailants.at(assailantKey) = assailantWeight;
			protectors.at(protectorKey) = protectorWeight;
			
		}
		
	}
	
	// count remaining melee unit weights
	
	remainingAssailantWeight = 0.0;
	for (robin_hood::pair<int, double> &assailantEntry : assailants)
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
	for (robin_hood::pair<int, double> &protectorEntry : protectors)
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
	
	if (TRACE) debug("\tremainingAssailantWeight=%5.2f remainingProtectorWeight=%5.2f\n", remainingAssailantWeight, remainingProtectorWeight);
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


FactionUnitCombatEffect getBestAttackerDefenderMeleeEffect(CombatEffectData &combatEffectData, robin_hood::unordered_flat_map<int, double> &attackers, robin_hood::unordered_flat_map<int, double> &defenders)
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
			
			double combatEffect = combatEffectData.getCombatEffect(attackerKey, defenderKey, EM_MELEE);
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
	
	return FactionUnitCombatEffect(FactionUnitCombat(bestAttackerKey, bestDefenderKey), bestCombatEffect);
	
}

