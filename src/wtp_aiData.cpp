
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
		defenseMultiplier *= this->terrainDefenseMultipliers.at(attackTriad);
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
	debug("CombatEffectTable::getCombatModeEffect( attackerKey=%d defenderKey=%d engagementMode=%d )\n", attackerKey, defenderKey, engagementMode);
	
	int key = FactionUnitCombat::encodeKey(attackerKey, defenderKey, engagementMode);
	
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		debug("\tnot found - computing\n");
		
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
	return getCombatModeEffect(attackerKey, defenderKey, engagementMode).value;
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

/*
Computes generic unit to unit combat effect.
*/
CombatModeEffect CombatEffectTable::getUnitCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode)
{
	debug("CombatEffectTable::getUnitCombatModeEffect( attackerFactionId=%d attackerUnitId=%d defenderFactionId=%d defenderUnitId=%d engagementMode=%d )\n", attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode);
	
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

/*
Calculates relative unit strength for melee attack.
How many defender units can attacker destroy until own complete destruction.
*/
double CombatEffectTable::getMeleeRelativeUnitStrength(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId)
{
	debug("CombatEffectTable::getMeleeRelativeUnitStrength( attackerFactionId=%d attackerUnitId=%d defenderFactionId=%d defenderUnitId=%d )\n", attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId);
	
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
double CombatEffectTable::getArtilleryDuelRelativeUnitStrength(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId)
{
	debug("CombatEffectTable::getArtilleryDuelRelativeUnitStrength( attackerFactionId=%d attackerUnitId=%d defenderFactionId=%d defenderUnitId=%d )\n", attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId);
	
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
double CombatEffectTable::getUnitBombardmentDamage(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId)
{
	debug("CombatEffectTable::getUnitBombardmentDamage( attackerFactionId=%d attackerUnitId=%d defenderFactionId=%d defenderUnitId=%d )\n", attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId);
	
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = Weapon[attackerUnit->weapon_id].offense_value;
	int defenderDefenseValue = Armor[defenderUnit->armor_id].defense_value;
	
	// attacker should be artillery capable and defender should not and should be surface unit
	
	if (!isArtilleryUnit(attackerUnitId) || isArtilleryUnit(defenderUnitId) || defenderUnit->triad() == TRIAD_AIR)
	{
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
	
	return relativeStrength / 10.0;
	
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
	debug("TileCombatEffectTable::getCombatModeEffect( attackerKey=%d defenderKey=%d engagementMode=%d )\n", attackerKey, defenderKey, engagementMode);
	
	int key = FactionUnitCombat::encodeKey(attackerKey, defenderKey, engagementMode);
	
	if (combatModeEffects.find(key) == combatModeEffects.end())
	{
		debug("\tnot found - computing\n");
		
		FactionUnit attackerFactionUnit(attackerKey);
		FactionUnit defenderFactionUnit(defenderKey);
		bool atTile = playerAssault ? attackerFactionUnit.factionId == playerFactionId : defenderFactionUnit.factionId == playerFactionId;

		CombatModeEffect combatModeEffect = getTileCombatModeEffect(combatEffectTable, attackerFactionUnit.factionId, attackerFactionUnit.unitId, defenderFactionUnit.factionId, defenderFactionUnit.unitId, engagementMode, atTile);
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
	return getCombatModeEffect(attackerKey, defenderKey, engagementMode).value;
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

CombatModeEffect TileCombatEffectTable::getTileCombatModeEffect(CombatEffectTable *combatEffectTable, int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, bool atTile)
{
	debug("CombatData::getTileCombatModeEffect( attackerFactionId=%d attackerUnitId=%d defenderFactionId=%d defenderUnitId=%d engagementMode=%d atTile=%d )\n", attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode, atTile);
	
	CombatModeEffect combatModeEffect = combatEffectTable->getCombatModeEffect(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode);
	
	// modify effect with tile modifiers if tile is set
	
	if (tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles)
	{
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		combatModeEffect.value *=
			1.0
			* tileInfo.sensorOffenseMultipliers.at(attackerFactionId)
			/ tileInfo.sensorDefenseMultipliers.at(defenderFactionId)
		;
		
		if (atTile)
		{
			AttackTriad attackTriad = getAttackTriad(attackerUnitId, defenderUnitId);
			combatModeEffect.value /= tileInfo.terrainDefenseMultipliers.at(attackTriad);
		}
		
	}
	
	return combatModeEffect;
	
}

// CombatData

void CombatData::reset(CombatEffectTable *combatEffectTable, MAP *tile)
{
	debug("CombatData::reset( combatEffectTable=%d )\n", (int) combatEffectTable);
	
	tileCombatEffectTable.clear();
	tileCombatEffectTable.combatEffectTable = combatEffectTable;
	tileCombatEffectTable.tile = tile;
	
	clearAssailants();
	clearProtectors();
	
}
void CombatData::clearAssailants()
{
	debug("CombatData::clearAssailants()\n");
	
	assailants.clear();
	assailantWeight = 0.0;
	computed = false;
	
}

void CombatData::clearProtectors()
{
	debug("CombatData::clearProtectors()\n");
	
	protectors.clear();
	protectorWeight = 0.0;
	computed = false;
	
}

void CombatData::addAssailantUnit(int factionId, int unitId, double weight)
{
	debug("CombatData::addAssailantUnit( factionId=%d unitId=%d weight=%5.2f )\n", factionId, unitId, weight);
	
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
	debug("CombatData::addProtectorUnit( factionId=%d unitId=%d weight=%5.2f )\n", factionId, unitId, weight);
	
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
	debug("CombatData::removeAssailantUnit( factionId=%d unitId=%d weight=%5.2f )\n", factionId, unitId, weight);
	
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
	debug("CombatData::removeProtectorUnit( factionId=%d unitId=%d weight=%5.2f )\n", factionId, unitId, weight);
	
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
	debug("CombatData::getAssailantUnitEffect( factionId=%d unitId=%d )\n", factionId, unitId);
	
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
	debug("CombatData::getProtectorUnitEffect( factionId=%d unitId=%d )\n", factionId, unitId);
	
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
	debug("CombatData::isSufficientAssault()\n");
	
	if (assailants.size() == 0)
		return false;
	else if (protectors.size() == 0)
		return true;
	
	compute();
	
	return remainingProtectorWeight > 0.0;
	
}

bool CombatData::isSufficientProtect()
{
	debug("CombatData::isSufficientProtect()\n");
	
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
	
	if (TRACE) { debug("CombatData::compute()\n"); }
	
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

void CombatData::resolveMutualCombat(double combatEffect, double &attackerHealth, double &defenderHealth)
{
	debug("CombatData::resolveMutualCombat( combatEffect=%5.2f attackerHealth=%5.2f defenderHealth=%5.2f )\n", combatEffect, attackerHealth, defenderHealth);
	
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


FactionUnitCombatEffect CombatData::getBestAttackerDefenderMeleeEffect(TileCombatEffectTable &tileCombatEffectTable, robin_hood::unordered_flat_map<int, double> &attackers, robin_hood::unordered_flat_map<int, double> &defenders)
{
	debug("CombatData::getBestAttackerDefenderMeleeEffect( ... )\n");
	
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

