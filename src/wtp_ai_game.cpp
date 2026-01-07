
#include "wtp_ai_game.h"

#include <cstring>
#include <float.h>
#include <numeric>

#include "wtp_mod.h"
#include "wtp_terranx.h"
#include "wtp_aiRoute.h"

/**
AI related common functions.
*/

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
	tacticalTasks.clear();

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

BunkerInfo &Data::getBunkerInfo(MAP *tile)
{
	assert(tile >= *MapTiles && tile < *MapTiles + *MapAreaTiles);
	robin_hood::unordered_flat_map<MAP *, BunkerInfo>::iterator bunkerInfoIterator = aiData.bunkerInfos.find(tile);
	if (bunkerInfoIterator == aiData.bunkerInfos.end())
	{
		debug("ERROR: No bunker at tile = %s\n", getLocationString(tile)); flushlog();
		exit(1);
	}
	return bunkerInfoIterator->second;
}

// CombatEffectTable

inline uint64_t CombatEffectTable::makeKey(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, MAP *attackerTile, MAP *defenderTile) noexcept
{
	int attackerTileIndex = (attackerTile == nullptr ? 0xFFFF : attackerTile - *MapTiles);
	int defenderTileIndex = (defenderTile == nullptr ? 0xFFFF : defenderTile - *MapTiles);
	return
		static_cast<uint64_t>(attackerFactionId)			//  3 bit
		|
		static_cast<uint64_t>(attackerUnitId)		<<  3	// 10 bit
		|
		static_cast<uint64_t>(defenderFactionId)	<< 13	//  3 bit
		|
		static_cast<uint64_t>(defenderUnitId)		<< 16	// 10 bit
		|
		static_cast<uint64_t>(engagementMode)		<< 26	//  1 bit
		|
		static_cast<uint64_t>(attackerTileIndex)	<< 27	// 16 bit
		|
		static_cast<uint64_t>(defenderTileIndex)	<< 43	// 16 bit
	;
}

void CombatEffectTable::clear()
{
	combatEffects.clear();
}

/*
Computes combat effect using battle_compute.
Creates fake vehicles using units and optionally battle tile.
*/
double CombatEffectTable::computeUnitCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, MAP *attackerTile, MAP *defenderTile)
{
	UNIT &attackerUnit = Units[attackerUnitId];
	UNIT &defenderUnit = Units[defenderUnitId];
	
	CombatMode combatMode = getCombatMode(engagementMode, defenderUnitId);
	
	// fake vehicles
	
	int attackerVehicleId = conf.max_veh_num + 0;
	int defenderVehicleId = conf.max_veh_num + 1;
	createFakeVehicle(attackerVehicleId, attackerFactionId, attackerUnitId);
	createFakeVehicle(defenderVehicleId, defenderFactionId, defenderUnitId);
	
	// set battle tile if provided
	
	if (attackerTile != nullptr)
	{
		VEH &attackerVehicle = Vehs[attackerVehicleId];
		attackerVehicle.x = getX(attackerTile);
		attackerVehicle.y = getY(attackerTile);
	}
	if (defenderTile != nullptr)
	{
		VEH &defenderVehicle = Vehs[defenderVehicleId];
		defenderVehicle.x = getX(defenderTile);
		defenderVehicle.y = getY(defenderTile);
	}
	
	// build combat type
	
	int combat_type = 0;
	if (engagementMode == EM_ARTILLERY)
	{
		combat_type |= CT_CAN_ARTY;
	}
	if (combatMode == CM_ARTILLERY_DUEL)
	{
		combat_type |= CT_WEAPON_ONLY;
	}
	if (attackerUnit.is_air_not_missile() && defenderUnit.is_interceptor())
	{
        combat_type |= (CT_INTERCEPT|CT_AIR_DEFENSE|CT_WEAPON_ONLY);
	}
	
	// battle compute
	
	int attackerStrength;
	int defenderStrength;
	wtp_mod_battle_compute(attackerVehicleId, defenderVehicleId, &attackerStrength, &defenderStrength, combat_type);
	
	// compute combat effect
	
	double combatEffect;
	if (combatMode == CM_BOMBARDMENT)
	{
		// convert bombardment strength to damage
		
		int off_value = std::max(1, attackerStrength * Rules->artillery_dmg_numerator);
		int def_value = std::max(1, defenderStrength * Rules->artillery_dmg_denominator);
		double damage_value = (double) off_value / (double) def_value;
		damage_value *= (double) defenderUnit.reactor_id;
		combatEffect = damage_value / 10.0;
		
	}
	else
	{
		// mutual fight combat effect
		combatEffect = defenderStrength == 0.0 ? 10.0 : (double) attackerStrength / (double) defenderStrength;
	}
	
	// combattant without a tile inherits sensor bonus from the other combattant tile
	// this is not handled by battle_compute
	
	if (attackerTile == nullptr && defenderTile != nullptr)
	{
		combatEffect *= getSensorOffenseMultiplier(attackerFactionId, defenderTile);
	}
	if (defenderTile == nullptr && attackerTile != nullptr)
	{
		combatEffect /= getSensorDefenseMultiplier(defenderFactionId, attackerTile);
	}
	
	return combatEffect;
	
}

double CombatEffectTable::getUnitCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, MAP *attackerTile, MAP *defenderTile)
{
	trace("CombatEffectTable::getCombatModeEffect( attackerFactionId=%d attackerUnitId=%d defenderFactionId=%d defenderUnitId=%d engagementMode=%d attackerTile=%s defenderTile=%s )\n", attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode, getLocationString(attackerTile), getLocationString(defenderTile));
	
	uint64_t key = this->makeKey(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode, attackerTile, defenderTile);
	
	if (combatEffects.find(key) == combatEffects.end())
	{
		double combatEffect = computeUnitCombatEffect(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode, attackerTile, defenderTile);
		combatEffects.emplace(key, combatEffect);
	}
	
	return combatEffects.at(key);
	
}

double CombatEffectTable::getUnitCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode)
{
	return getUnitCombatEffect(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode, nullptr, nullptr);
}

double CombatEffectTable::getVehicleCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode, MAP *attackerTile, MAP *defenderTile)
{
	assert(isValidVehicleId(attackerVehicleId));
	assert(isValidVehicleId(defenderVehicleId));
	
	VEH &attackerVehicle = Vehs[attackerVehicleId];
	VEH &defenderVehicle = Vehs[defenderVehicleId];
	
	double unitCombatEffect = getUnitCombatEffect(attackerVehicle.faction_id, attackerVehicle.unit_id, defenderVehicle.faction_id, defenderVehicle.unit_id, engagementMode, attackerTile, defenderTile);
	
	return
		unitCombatEffect
		* getVehicleMoraleMultiplier(attackerVehicleId)
		/ getVehicleMoraleMultiplier(defenderVehicleId)
	;
	
}

double CombatEffectTable::getVehicleCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode)
{
	return getVehicleCombatEffect(attackerVehicleId, defenderVehicleId, engagementMode, getVehicleMapTile(attackerVehicleId), getVehicleMapTile(defenderVehicleId));
}

/*
Calculates relative unit strength for melee attack.
How many defender units can attacker destroy until own complete destruction.
*/
double CombatEffectTable::getMeleeRelativeUnitStrength(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId)
{
//	debug("CombatEffectTable::getMeleeRelativeUnitStrength( attackerFactionId=%d attackerUnitId=%d defenderFactionId=%d defenderUnitId=%d )\n", attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId);
	
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
	
	// alien fight on half a strength in early game
	
	if (attackerFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength *= getAlienTurnOffenseModifier();
	}
	if (defenderFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength /= getAlienTurnDefenseModifier();
	}
	
//	debug("\t%5.2f\n", relativeStrength);
	return relativeStrength;
	
}

/**
Calculates relative unit strength for artillery duel attack.
How many defender units can attacker destroy until own complete destruction.
*/
double CombatEffectTable::getArtilleryDuelRelativeUnitStrength(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId)
{
//	debug("CombatEffectTable::getArtilleryDuelRelativeUnitStrength( attackerFactionId=%d attackerUnitId=%d defenderFactionId=%d defenderUnitId=%d )\n", attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId);
	
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
		relativeStrength *= getAlienTurnOffenseModifier();
	}
	if (defenderFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength /= getAlienTurnDefenseModifier();
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
//	debug("CombatEffectTable::getUnitBombardmentDamage( attackerFactionId=%d attackerUnitId=%d defenderFactionId=%d defenderUnitId=%d )\n", attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId);
	
	UNIT *attackerUnit = &(Units[attackerUnitId]);
	UNIT *defenderUnit = &(Units[defenderUnitId]);
	int attackerOffenseValue = Weapon[attackerUnit->weapon_id].offense_value;
	int defenderDefenseValue = Armor[defenderUnit->armor_id].defense_value;
	
	// attacker is artillery and defender is not
	
	if (!isArtilleryUnit(attackerUnitId) || isArtilleryUnit(defenderUnitId))
	{
		return 0.0;
	}
	
	// calculate relative damage
	
	double relativeStrength = 1.0;
	
	if (attackerOffenseValue < 0 || defenderDefenseValue < 0)
	{
		relativeStrength = getPsiCombatBaseOdds(attackerUnit->triad());
debug(">relativeStrength=%5.2f\n", relativeStrength);flushlog();
		
		// reactor
		// psi combat ignores reactor
		
		// PLANET bonus
		
		relativeStrength *= getFactionSEPlanetOffenseModifier(attackerFactionId);
debug(">relativeStrength=%5.2f\n", relativeStrength);flushlog();
		relativeStrength /= getFactionSEPlanetOffenseModifier(defenderFactionId);
debug(">relativeStrength=%5.2f\n", relativeStrength);flushlog();
		
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
		relativeStrength *= getAlienTurnOffenseModifier();
	}
	if (defenderFactionId == 0 && *CurrentTurn <= conf.native_weak_until_turn)
	{
		relativeStrength /= getAlienTurnDefenseModifier();
	}
	
	// artillery damage numerator/denominator
	
	relativeStrength *= (double)Rules->artillery_dmg_numerator / (double)Rules->artillery_dmg_denominator;
	
	// divide by 10 to convert damage to units destroyed
	
	return relativeStrength / 10.0;
	
}

Combattant::Combattant(int factionId, int unitId, double weight, bool airbase)
{
	assert(isValidFactionId(factionId));
	assert(isValidUnitId(unitId));
	assert(weight > 0.0);
	
	UNIT &unit = Units[unitId];
	
	this->factionId = factionId;
	this->unitId = unitId;
	this->key = FactionUnit::encodeKey(factionId, unitId);
	this->weight = weight;
	
	this->range = isArtilleryUnit(unitId);
	this->melee = isMeleeUnit(unitId);
	this->aircraftInFlight = !airbase && unit.is_air();
	this->needlejetInFlight = !airbase && unit.is_needlejet();
	this->canAttackNeedlejetInFlight = isUnitCanAttackNeedlejetInFlight(unitId);
	this->canBombardAircraftInFlight = isUnitCanBombardAircraftInFlight(unitId);
	
	this->pad0 = -1;
	this->strengthCoefficient = 1.0;
	this->health = 1.0;
	
}

Combattant::Combattant(int vehicleId, double weight, bool airbase)
: Combattant(Vehs[vehicleId].faction_id, Vehs[vehicleId].unit_id, weight, airbase)
{
	assert(isValidVehicleId(vehicleId));
	
	VEH &vehicle = Vehs[vehicleId];
	
	this->pad0 = vehicle.pad_0;
	this->strengthCoefficient = getVehicleMoraleMultiplier(vehicleId);
	this->health = getVehicleRelativeHealth(vehicleId);
	
}

void Combattant::initialize(robin_hood::unordered_flat_map<int, double> const &damageCoefficients)
{
	robin_hood::unordered_flat_map<int, double>::const_iterator damageCoefficientIterator = damageCoefficients.find(pad0);
	double damageCoefficient = damageCoefficientIterator == damageCoefficients.end() ? 0.0 : damageCoefficientIterator->second;
	assert(damageCoefficient >= 0.0 && damageCoefficient <= 1.0);
	this->remainingHealth = (1.0 - damageCoefficient) * this->health;
}

// CombatData

void CombatData::initialize(MAP *tile, bool playerAssaults, double targetGain)
{
	this->tile = tile;
	this->airbase = aiData.getTileInfo(tile).airbase;
	this->playerAssaults = playerAssaults;
	this->targetGain = targetGain;
	
	this->assailants.clear();
	this->protectors.clear();
	
	this->remainingAssailantUnitWeights.clear();
	this->remainingAssailantUnitWeightSum = 0.0;
	this->remainingProtectorUnitWeights.clear();
	this->remainingProtectorUnitWeightSum = 0.0;
	
	this->computed = false;
	
}

double CombatData::getUnitCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, bool attackerAtTile, bool defenderAtTile)
{
	return
		aiData.combatEffectTable.getUnitCombatEffect(attackerFactionId, attackerUnitId, defenderFactionId, defenderUnitId, engagementMode, attackerAtTile ? tile : nullptr, defenderAtTile ? tile : nullptr)
	;
}

double CombatData::getCombattantCombatEffect(Combattant const &attacker, Combattant const &defender, ENGAGEMENT_MODE engagementMode, bool attackerAtTile, bool defenderAtTile)
{
	return
		getUnitCombatEffect(attacker.factionId, attacker.unitId, defender.factionId, defender.unitId, engagementMode, attackerAtTile, defenderAtTile)
		* attacker.strengthCoefficient
		/ defender.strengthCoefficient
	;
}

Combattant &CombatData::addAssailant(int vehicleId, double weight)
{
	assailants.emplace_back(vehicleId, weight, /*airbase*/false);
	return assailants.back();
}
Combattant &CombatData::addProtector(int vehicleId, double weight)
{
	protectors.emplace_back(vehicleId, weight, /*airbase*/false);
	return protectors.back();
}

void CombatData::removeLastAssailant()
{
	assailants.pop_back();
}
void CombatData::removeLastProtector()
{
	protectors.pop_back();
}

double CombatData::getAssailantWeightSum()
{
	return std::accumulate(assailants.begin(), assailants.end(), 0.0, [](double sum, Combattant const &combattant) { return sum + combattant.weight; });
}

double CombatData::getProtectorWeightSum()
{
	return std::accumulate(protectors.begin(), protectors.end(), 0.0, [](double sum, Combattant const &combattant) { return sum + combattant.weight; });
}

double CombatData::getAssailantHealthSum(bool range)
{
	return std::accumulate(assailants.begin(), assailants.end(), 0.0, [range](double sum, Combattant const &combattant) { return sum + (!range || combattant.range ? combattant.health : 0.0); });
}

double CombatData::getProtectorHealthSum(bool range)
{
	return std::accumulate(protectors.begin(), protectors.end(), 0.0, [range](double sum, Combattant const &combattant) { return sum + (!range || combattant.range ? combattant.health : 0.0); });
}

double CombatData::getAssailantRemainingHealthSum(bool range)
{
	return std::accumulate(assailants.begin(), assailants.end(), 0.0, [range](double sum, Combattant const &combattant) { return sum + (!range || combattant.range ? combattant.remainingHealth : 0.0); });
}

double CombatData::getProtectorRemainingHealthSum(bool range)
{
	return std::accumulate(protectors.begin(), protectors.end(), 0.0, [range](double sum, Combattant const &combattant) { return sum + (!range || combattant.range ? combattant.remainingHealth : 0.0); });
}

double CombatData::getAssaultSufficiency(bool range)
{
	trace("CombatData::getAssaultSufficiency( range=%d )\n", range);
	
	if (assailants.empty())
	{
		// no assailants - zero sufficiency
		return 0.0;
	}
	if (protectors.empty())
	{
		// no protectors - full sufficiency
		return 1.0;
	}
	
	compute();
	
	double assailantHealthSum = getAssailantHealthSum(range);
	double protectorHealthSum = getProtectorHealthSum(range);
	double assailantRemainingHealthSum = getAssailantRemainingHealthSum(range);
	double protectorRemainingHealthSum = getProtectorRemainingHealthSum(range);
	double assailantDestroyedHealthSum = assailantHealthSum - assailantRemainingHealthSum;
	double protectorDestroyedHealthSum = protectorHealthSum - protectorRemainingHealthSum;
	trace("\tassailantHealthSum=%5.2f protectorHealthSum=%5.2f assailantRemainingHealthSum=%5.2f protectorRemainingHealthSum=%5.2f \n", assailantHealthSum, protectorHealthSum, assailantRemainingHealthSum, protectorRemainingHealthSum);
	
	if (assailantHealthSum <= 0.0)
	{
		return 0.0;
	}
	if (protectorHealthSum <= 0.0)
	{
		return 1.0;
	}
	
	// compute sufficiency
	
	double sufficiency;
	
	if (assailantRemainingHealthSum <= 0.0 && protectorRemainingHealthSum <= 0.0)
	{
		// both parties are destroyed
		sufficiency = 1.0;
	}
	else if (assailantRemainingHealthSum <= 0.0 && protectorRemainingHealthSum >= protectorHealthSum)
	{
		// no casualties in protectors while all assailants are destroyed
		sufficiency = 0.0;
	}
	else if (assailantRemainingHealthSum <= 0.0 && protectorRemainingHealthSum > 0.0)
	{
		// protectors survived
		// ratio of actually destroyed opponent to what it should be
		sufficiency = protectorDestroyedHealthSum / protectorHealthSum;
	}
	else if (protectorRemainingHealthSum <= 0.0 && assailantRemainingHealthSum > assailantHealthSum)
	{
		// no casualties in assailants while all protectors are destroyed
		sufficiency = INF;
	}
	else if (protectorRemainingHealthSum <= 0.0 && assailantRemainingHealthSum > 0.0)
	{
		// assailants survived
		// ratio of initial aiplayer forces to destroyed ones
		sufficiency = assailantHealthSum / assailantDestroyedHealthSum;
	}
	else
	{
		debug("ERROR: assailantRemainingHealthSum > 0.0 && protectorRemainingHealthSum > 0.0 after compute.\n");
		sufficiency = 0.5;
	}
	
	return sufficiency;
	
}

double CombatData::getProtectSufficiency(bool artillery)
{
	trace("CombatData::getProtectSufficiency( artillery=%d )\n", artillery);
	
	if (assailants.empty())
	{
		// no assailants - full sufficiency
		trace("\tno assailants\n");
		trace("\tprotectSufficiency=1.0\n");
		return 1.0;
	}
	if (protectors.empty())
	{
		// no protectors - zero sufficiency
		trace("\tno protectors\n");
		trace("\tprotectSufficiency=0.0\n");
		return 0.0;
	}
	
	compute();
	
	if (TRACE)
	{
		trace("\tassailants\n")
		for (Combattant const &assailant : assailants)
		{
			trace("\t\t[%4d] weight=%5.2f health=%5.2f remainingHealth=%5.2f\n", getInitialVehicleIdByPad0(assailant.pad0), assailant.weight, assailant.health, assailant.remainingHealth);
		}
		trace("\tprotectors\n")
		for (Combattant const &protector : protectors)
		{
			trace("\t\t[%4d] weight=%5.2f health=%5.2f remainingHealth=%5.2f\n", getInitialVehicleIdByPad0(protector.pad0), protector.weight, protector.health, protector.remainingHealth);
		}
	}
	
	double assailantHealthSum = getAssailantHealthSum(artillery);
	double protectorHealthSum = getProtectorHealthSum(artillery);
	double assailantRemainingHealthSum = getAssailantRemainingHealthSum(artillery);
	double protectorRemainingHealthSum = getProtectorRemainingHealthSum(artillery);
	double assailantDestroyedHealthSum = assailantHealthSum - assailantRemainingHealthSum;
	double protectorDestroyedHealthSum = protectorHealthSum - protectorRemainingHealthSum;
	trace("\tassailantHealthSum=%5.2f protectorHealthSum=%5.2f assailantRemainingHealthSum=%5.2f protectorRemainingHealthSum=%5.2f \n", assailantHealthSum, protectorHealthSum, assailantRemainingHealthSum, protectorRemainingHealthSum);
	
	if (assailantHealthSum <= 0.0)
	{
		trace("\tprotectSufficiency=1.0\n");
		return 1.0;
	}
	if (protectorHealthSum <= 0.0)
	{
		trace("\tprotectSufficiency=0.0\n");
		return 0.0;
	}
	
	// compute sufficiency
	
	double sufficiency;
	
	if (assailantRemainingHealthSum <= 0.0 && protectorRemainingHealthSum <= 0.0)
	{
		// both parties are destroyed
		sufficiency = 1.0;
	}
	else if (assailantRemainingHealthSum <= 0.0 && protectorRemainingHealthSum >= protectorHealthSum)
	{
		// no casualties in protectors while all assailants are destroyed
		trace("\tprotectSufficiency=Inf\n");
		sufficiency = INF;
	}
	else if (assailantRemainingHealthSum <= 0.0 && protectorRemainingHealthSum > 0.0)
	{
		// protectors survived
		// ratio of initial aiplayer forces to destroyed ones
		sufficiency = protectorHealthSum / protectorDestroyedHealthSum;
	}
	else if (protectorRemainingHealthSum <= 0.0 && assailantRemainingHealthSum > assailantHealthSum)
	{
		// no casualties in assailants while all protectors are destroyed
		trace("\tprotectSufficiency=0.0\n");
		sufficiency = 0.0;
	}
	else if (protectorRemainingHealthSum <= 0.0 && assailantRemainingHealthSum > 0.0)
	{
		// assailants survived
		// ratio of actually destroyed opponent to what it should be
		sufficiency = assailantDestroyedHealthSum / assailantHealthSum;
	}
	else
	{
		debug("ERROR: assailantRemainingHealthSum=%5.2f protectorRemainingHealthSum=%5.2f after compute.\n", assailantRemainingHealthSum, protectorRemainingHealthSum);
		sufficiency = 0.5;
	}
	
	return sufficiency;
	
}

/*
Computes sufficiency contribution by comparing sufficiency with and without the combattant.
*/
double CombatData::getAssailantCombattantContribution(Combattant &combattant)
{
	double combattantHealth = combattant.health;
	
	// compute sufficiency without combattant
	
	combattant.health = 0.0;
	
	computed = false;
	double oldSufficiency = getAssaultSufficiency(false);
	double oldArtillerySufficiency = getAssaultSufficiency(true);
	
	// compute sufficiency with combattant
	
	combattant.health = combattantHealth;
	
	computed = false;
	double newSufficiency = getAssaultSufficiency(false);
	double newArtillerySufficiency = getAssaultSufficiency(true);
	
	// compute contribution
	
	computed = false;
	double contribution = newSufficiency - oldSufficiency;
	double artilleryContribution = newArtillerySufficiency - oldArtillerySufficiency;
	
	return std::max(contribution, artilleryContribution);
	
}

/*
Computes sufficiency contribution by comparing sufficiency with and without the combattant.
*/
double CombatData::getProtectorCombattantContribution(Combattant &combattant)
{
	double combattantHealth = combattant.health;
	
	// compute sufficiency without combattant
	
	combattant.health = 0.0;
	
	computed = false;
	double oldSufficiency = getProtectSufficiency(false);
	double oldArtillerySufficiency = getProtectSufficiency(true);
	
	// compute sufficiency with combattant
	
	combattant.health = combattantHealth;
	
	computed = false;
	double newSufficiency = getProtectSufficiency(false);
	double newArtillerySufficiency = getProtectSufficiency(true);
	
	// compute contribution
	
	double contribution = newSufficiency - oldSufficiency;
	double artilleryContribution = newArtillerySufficiency - oldArtillerySufficiency;
	
	return std::max(contribution, artilleryContribution);
	
}

double CombatData::getAssailantUnitContribution(int factionId, int unitId)
{
	assailants.emplace_back(factionId, unitId, 1.0, /*airbase*/false);
	Combattant &combattant = assailants.back();
	double contribution = getAssailantCombattantContribution(combattant);
	assailants.pop_back();
	return contribution;
}

double CombatData::getProtectorUnitContribution(int factionId, int unitId)
{
	protectors.emplace_back(factionId, unitId, 1.0, /*airbase*/airbase);
	Combattant &combattant = protectors.back();
	double contribution = getProtectorCombattantContribution(combattant);
	protectors.pop_back();
	return contribution;
}

double CombatData::getAssailantContribution(int vehicleId)
{
	Combattant &combattant = addAssailant(vehicleId);
	double contribution = getAssailantCombattantContribution(combattant);
	removeLastAssailant();
	return contribution;
}

double CombatData::getProtectorContribution(int vehicleId)
{
	Combattant &combattant = addProtector(vehicleId);
	double contribution = getProtectorCombattantContribution(combattant);
	removeLastProtector();
	return contribution;
}

bool CombatData::isSufficientAssault()
{
	trace("CombatData::isSufficientAssault()\n");
	return getAssaultSufficiency(false) >= 1.0;
}

bool CombatData::isSufficientProtect()
{
	trace("CombatData::isSufficientProtect()\n");
	return getProtectSufficiency(false) >= 1.0;
}

void CombatData::compute()
{
	return compute(robin_hood::unordered_flat_map<int, double>{}, robin_hood::unordered_flat_map<int, double>{});
}

void CombatData::compute(robin_hood::unordered_flat_map<int, double> assailantVehicleDamageCoefficients, robin_hood::unordered_flat_map<int, double> protectorVehicleDamageCoefficients)
{
	if (computed)
		return;
	
	trace("CombatData::compute( %s )\n", getLocationString(tile));
	
	// initialize combattants
	
	for (Combattant &assailant : assailants)
	{
		assailant.initialize(assailantVehicleDamageCoefficients);
	}
	for (Combattant &protector : protectors)
	{
		protector.initialize(protectorVehicleDamageCoefficients);
	}
	
	// set advantage for the opponent
	
	std::vector<Combattant> &opponentCombattants = (playerAssaults ? protectors : assailants);
	double opponentRelativeHealthBonus = getOpponentRelativeHealthBonus(opponentCombattants);
	trace("\topponentRelativeHealthBonus = %5.2f\n", opponentRelativeHealthBonus);
	
	for (Combattant &combattant : opponentCombattants)
	{
		combattant.remainingHealth *= (1.0 + opponentRelativeHealthBonus);
	}
	
	if (TRACE)
	{
		trace("\tassailants\n")
		for (Combattant const &assailant : assailants)
		{
			trace("\t\t[%4d] weight=%5.2f health=%5.2f remainingHealth=%5.2f\n", getInitialVehicleIdByPad0(assailant.pad0), assailant.weight, assailant.health, assailant.remainingHealth);
		}
		trace("\tprotectors\n")
		for (Combattant const &protector : protectors)
		{
			trace("\t\t[%4d] weight=%5.2f health=%5.2f remainingHealth=%5.2f\n", getInitialVehicleIdByPad0(protector.pad0), protector.weight, protector.health, protector.remainingHealth);
		}
	}
	
	// artillery combattants
	
	std::list<Combattant *> rangeAssailants;
	std::list<Combattant *> rangeProtectors;
	for (Combattant &assailant : assailants)
	{
		if (assailant.remainingHealth > 0.0 && assailant.range)
		{
			rangeAssailants.push_back(&assailant);
		}
	}
	for (Combattant &protector : protectors)
	{
		if (protector.remainingHealth > 0.0 && protector.range)
		{
			rangeProtectors.push_back(&protector);
		}
	}
	trace("\t\trangeAssailants.size()=%d rangeProtectors.size()=%d\n", rangeAssailants.size(), rangeProtectors.size());
	
	// artillery duels
	trace("\tartillery duels\n");
	
	while (!rangeAssailants.empty() && !rangeProtectors.empty())
	{
		CombattantEffect bestAssailantProtectorEffect = getBestCombattantEffect(rangeAssailants, rangeProtectors, EM_ARTILLERY, false, true);
		CombattantEffect bestProtectorAssailantEffect = getBestCombattantEffect(rangeProtectors, rangeAssailants, EM_ARTILLERY, true, false);
		if (TRACE)
		{
			if (bestAssailantProtectorEffect.attacker != nullptr && bestAssailantProtectorEffect.defender != nullptr)
			{
				trace
				(
					"\t\tbestAssailantProtectorEffect: effect=%5.2f attackerFactionId=%d attackerUnitId=%3d defenderFactionId=%d defenderUnitId=%3d\n"
					, bestAssailantProtectorEffect.effect
					, bestAssailantProtectorEffect.attacker->factionId, bestAssailantProtectorEffect.attacker->unitId
					, bestAssailantProtectorEffect.defender->factionId, bestAssailantProtectorEffect.defender->unitId
				);
			}
			else
			{
				trace("\t\tbestAssailantProtectorEffect: not found\n");
			}
			if (bestProtectorAssailantEffect.attacker != nullptr && bestProtectorAssailantEffect.defender != nullptr)
			{
				trace
				(
					"\t\tbestProtectorAssailantEffect: effect=%5.2f attackerFactionId=%d attackerUnitId=%3d defenderFactionId=%d defenderUnitId=%3d\n"
					, bestProtectorAssailantEffect.effect
					, bestProtectorAssailantEffect.attacker->factionId, bestProtectorAssailantEffect.attacker->unitId
					, bestProtectorAssailantEffect.defender->factionId, bestProtectorAssailantEffect.defender->unitId
				);
			}
			else
			{
				trace("\t\tbestProtectorAssailantEffect: not found\n");
			}
		}
		
		if (bestAssailantProtectorEffect.effect <= 0.0)
		{
			// assailant cannot attack and loses all their vehicles
			trace("\t\tassailant cannot attack\n");
			std::for_each(rangeAssailants.begin(), rangeAssailants.end(), [](Combattant *combattant) { combattant->remainingHealth = 0.0; });
		}
		else if (bestProtectorAssailantEffect.effect >= 1.0 / bestAssailantProtectorEffect.effect)
		{
			// protector attacks because it is better than defend
			trace("\t\tprotector attacks because it is better than defend\n");
			resolveMutualCombat(bestProtectorAssailantEffect);
		}
		else
		{
			// assailant attacks because protector does not want to
			trace("\t\tassailant attacks because protector does not want to\n");
			resolveMutualCombat(bestAssailantProtectorEffect);
		}
		
		// remove destroyed combattants
		
		rangeAssailants.remove_if([](Combattant *combattant) { return combattant->remainingHealth <= 0.0; });
		rangeProtectors.remove_if([](Combattant *combattant) { return combattant->remainingHealth <= 0.0; });
		
	}
	
	// no artillery at one side after artillery duels
	
	if (!rangeAssailants.empty() && !rangeProtectors.empty())
	{
		debug("ERROR: !rangeAssailants.empty() && !rangeProtectors.empty() - after artillery duels\n");
	}
	
	if (TRACE)
	{
		trace("\tassailants\n")
		for (Combattant const &assailant : assailants)
		{
			trace("\t\t[%4d] weight=%5.2f health=%5.2f remainingHealth=%5.2f\n", getInitialVehicleIdByPad0(assailant.pad0), assailant.weight, assailant.health, assailant.remainingHealth);
		}
		trace("\tprotectors\n")
		for (Combattant const &protector : protectors)
		{
			trace("\t\t[%4d] weight=%5.2f health=%5.2f remainingHealth=%5.2f\n", getInitialVehicleIdByPad0(protector.pad0), protector.weight, protector.health, protector.remainingHealth);
		}
	}
	
	// bombardment
	trace("\tbombardment\n");
	
	if (rangeAssailants.empty() && rangeProtectors.empty())
	{
		// Artillerys mutual destruction
		trace("\t\tartillery mutual destruction\n");
	}
	else if (!rangeAssailants.empty() && rangeProtectors.empty())
	{
		// assailants bombards protectors
		trace("\t\tassailants bombard protectors\n");
		
		bool rangeAssailantCanBombardAircraftInFlight = false;
		for (Combattant *rangeAssailant : rangeAssailants)
		{
			if (rangeAssailant->canBombardAircraftInFlight)
			{
				rangeAssailantCanBombardAircraftInFlight = true;
				break;
			}
			
		}
		
		double minBombardmentHealth = 1.0 - aiData.getTileInfo(this->tile).maxBombardmentDamage;
		
		for (Combattant &protector : protectors)
		{
			if (protector.aircraftInFlight && !rangeAssailantCanBombardAircraftInFlight)
				continue;
			
			protector.remainingHealth = std::min(minBombardmentHealth, protector.remainingHealth);
			
		}
		
	}
	else if (rangeAssailants.empty() && !rangeProtectors.empty())
	{
		// protectors bombards assailants
		trace("\t\tprotectors bombard assailants\n");
		
		bool rangeProtectorCanBombardAircraftInFlight = false;
		for (Combattant *rangeProtector : rangeProtectors)
		{
			if (rangeProtector->canBombardAircraftInFlight)
			{
				rangeProtectorCanBombardAircraftInFlight = true;
				break;
			}
			
		}
		
		for (Combattant *rangeProtector : rangeProtectors)
		{
			for (Combattant &assailant : assailants)
			{
				if (assailant.aircraftInFlight && !rangeProtectorCanBombardAircraftInFlight)
					continue;
				
				double combatEffect = getCombattantCombatEffect(*rangeProtector, assailant, EM_ARTILLERY, true, false);
				assailant.remainingHealth = std::max(0.0, assailant.remainingHealth - combatEffect);
				
			}
			
		}
		
	}
	
	if (TRACE)
	{
		trace("\tassailants\n")
		for (Combattant const &assailant : assailants)
		{
			trace("\t\t[%4d] weight=%5.2f health=%5.2f remainingHealth=%5.2f\n", getInitialVehicleIdByPad0(assailant.pad0), assailant.weight, assailant.health, assailant.remainingHealth);
		}
		trace("\tprotectors\n")
		for (Combattant const &protector : protectors)
		{
			trace("\t\t[%4d] weight=%5.2f health=%5.2f remainingHealth=%5.2f\n", getInitialVehicleIdByPad0(protector.pad0), protector.weight, protector.health, protector.remainingHealth);
		}
	}
	
	// melee combattants
	
	std::list<Combattant *> meleeAssailants;
	std::list<Combattant *> meleeProtectors;
	for (Combattant &assailant : assailants)
	{
		if (assailant.remainingHealth > 0.0)
		{
			meleeAssailants.push_back(&assailant);
		}
	}
	for (Combattant &protector : protectors)
	{
		if (protector.remainingHealth > 0.0)
		{
			meleeProtectors.push_back(&protector);
		}
	}
	trace("\t\tmeleeAssailants.size()=%d meleeProtectors.size()=%d\n", meleeAssailants.size(), meleeProtectors.size());
	
	// melee duels
	trace("\tmelee duels\n");
	
	while (!meleeAssailants.empty() && !meleeProtectors.empty())
	{
		CombattantEffect bestAssailantProtectorEffect = getBestCombattantEffect(meleeAssailants, meleeProtectors, EM_MELEE, false, true);
		CombattantEffect bestProtectorAssailantEffect = getBestCombattantEffect(meleeProtectors, meleeAssailants, EM_MELEE, true, false);
		if (TRACE)
		{
			if (bestAssailantProtectorEffect.attacker != nullptr && bestAssailantProtectorEffect.defender != nullptr)
			{
				trace
				(
					"\t\tbestAssailantProtectorEffect: effect=%5.2f attackerFactionId=%d attackerUnitId=%3d defenderFactionId=%d defenderUnitId=%3d\n"
					, bestAssailantProtectorEffect.effect
					, bestAssailantProtectorEffect.attacker->factionId, bestAssailantProtectorEffect.attacker->unitId
					, bestAssailantProtectorEffect.defender->factionId, bestAssailantProtectorEffect.defender->unitId
				);
			}
			else
			{
				trace("\t\tbestAssailantProtectorEffect: not found\n");
			}
			if (bestProtectorAssailantEffect.attacker != nullptr && bestProtectorAssailantEffect.defender != nullptr)
			{
				trace
				(
					"\t\tbestProtectorAssailantEffect: effect=%5.2f attackerFactionId=%d attackerUnitId=%3d defenderFactionId=%d defenderUnitId=%3d\n"
					, bestProtectorAssailantEffect.effect
					, bestProtectorAssailantEffect.attacker->factionId, bestProtectorAssailantEffect.attacker->unitId
					, bestProtectorAssailantEffect.defender->factionId, bestProtectorAssailantEffect.defender->unitId
				);
			}
			else
			{
				trace("\t\tbestProtectorAssailantEffect: not found\n");
			}
		}
		
		if (bestAssailantProtectorEffect.effect <= 0.0)
		{
			// assailant cannot attack and loses all their vehicles
			trace("\t\tassailant cannot attack\n");
			std::for_each(meleeAssailants.begin(), meleeAssailants.end(), [](Combattant *combattant) { combattant->remainingHealth = 0.0; });
		}
		else if (bestProtectorAssailantEffect.effect >= 1.0 / bestAssailantProtectorEffect.effect)
		{
			// protector attacks because it is better than defend
			trace("\t\tprotector attacks because it is better than defend\n");
			resolveMutualCombat(bestProtectorAssailantEffect);
		}
		else
		{
			// assailant attacks because protector does not want to
			trace("\t\tassailant attacks because protector does not want to\n");
			resolveMutualCombat(bestAssailantProtectorEffect);
		}
		
		// remove destroyed combattants
		
		meleeAssailants.remove_if([](Combattant const *combattant) { return combattant->remainingHealth <= 0.0; });
		meleeProtectors.remove_if([](Combattant const *combattant) { return combattant->remainingHealth <= 0.0; });
		
	}
	
	computed = true;
	
}

void CombatData::resolveMutualCombat(CombattantEffect &combattantEffect)
{
	trace("CombatData::resolveMutualCombat( combatEffect=%5.2f )\n", combattantEffect.effect);
	trace("\tattacker->weight=%5.2f defender->weight=%5.2f\n", combattantEffect.attacker->weight, combattantEffect.defender->weight);
	
	if (combattantEffect.attacker->remainingHealth <= 0.0 || combattantEffect.defender->remainingHealth <= 0.0)
	{
		trace("\tone of the combattants is dead\n");
		return;
	}
	
	double weightedCombatEffect = combattantEffect.effect * combattantEffect.attacker->weight / combattantEffect.defender->weight;
	double equalCombatEffect = combattantEffect.defender->remainingHealth / combattantEffect.attacker->remainingHealth;
	trace("\tweightedCombatEffect=%5.2f equalCombatEffect=%5.2f\n", weightedCombatEffect, equalCombatEffect);
	
	trace("\told: attacker->remainingHealth=%5.2f defender->remainingHealth=%5.2f\n", combattantEffect.attacker->remainingHealth, combattantEffect.defender->remainingHealth);
	if (weightedCombatEffect == equalCombatEffect)
	{
		// both die
		combattantEffect.attacker->remainingHealth = 0.0;
		combattantEffect.defender->remainingHealth = 0.0;
	}
	else if (weightedCombatEffect > equalCombatEffect)
	{
		// defender dies
		double attackerDamage = combattantEffect.defender->remainingHealth / weightedCombatEffect;
		combattantEffect.attacker->remainingHealth -= attackerDamage;
		combattantEffect.defender->remainingHealth = 0.0;
	}
	else
	{
		double defenderDamage = combattantEffect.attacker->remainingHealth * weightedCombatEffect;
		combattantEffect.attacker->remainingHealth = 0.0;
		combattantEffect.defender->remainingHealth -= defenderDamage;
	}
	trace("\tnew: attacker->remainingHealth=%5.2f defender->remainingHealth=%5.2f\n", combattantEffect.attacker->remainingHealth, combattantEffect.defender->remainingHealth);
	
}


CombattantEffect CombatData::getBestCombattantEffect(std::list<Combattant *> &attackers, std::list<Combattant *> &defenders, ENGAGEMENT_MODE engagementMode, bool attackerAtTile, bool defenderAtTile)
{
	trace("CombatData::getBestCombattantEffect( ... )\n");
	
	Combattant *bestAttacker = nullptr;
	Combattant *bestDefender = nullptr;
	double bestCombatEffect = 0.0;
	
	for (Combattant *attacker : attackers)
	{
		Combattant *bestAttackerDefender = nullptr;
		double bestAttackerDefenderCombatEffect = +DBL_MAX;
		
		for (Combattant *defender : defenders)
		{
			// able to attack
			
			switch (engagementMode)
			{
			case EM_ARTILLERY:
				if (!attacker->range)
					continue;
				if (defender->aircraftInFlight && !attacker->canBombardAircraftInFlight)
					continue;
				break;
			case EM_MELEE:
				if (!attacker->melee)
					continue;
				if (defender->needlejetInFlight && !attacker->canAttackNeedlejetInFlight)
					continue;
				break;
			}
			
			double combatEffect = getCombattantCombatEffect(*attacker, *defender, engagementMode, attackerAtTile, defenderAtTile);
			
			// defender looks for the lowest effect
			
			if (combatEffect < bestAttackerDefenderCombatEffect)
			{
				bestAttackerDefender = defender;
				bestAttackerDefenderCombatEffect = combatEffect;
			}
			
		}
		
		// no defender selected
		
		if (bestAttackerDefender == nullptr)
		{
			continue;
		}
		
		// attacker looks for the lowest effect
		
		if (bestAttackerDefenderCombatEffect > bestCombatEffect)
		{
			bestAttacker = attacker;
			bestDefender = bestAttackerDefender;
			bestCombatEffect = bestAttackerDefenderCombatEffect;
		}
		
	}
	
	if (TRACE)
	{
		if (bestAttacker != nullptr && bestDefender != nullptr)
		{
			trace("\tbestAttacker=[%4d] bestDefender=[%4d] bestCombatEffect=%5.2f\n", getInitialVehicleIdByPad0(bestAttacker->pad0), getInitialVehicleIdByPad0(bestDefender->pad0), bestCombatEffect);
		}
		else
		{
			trace("\tnot found\n");
		}
	}
	return {bestAttacker, bestDefender, bestCombatEffect};
	
}

/*
Computes opponent relative health bonus to ensure player 80% win chance sufficiency.
*/
double CombatData::getOpponentRelativeHealthBonus(std::vector<Combattant> opponentCombattants)
{
	double opponentTotalHealth = std::accumulate(opponentCombattants.begin(), opponentCombattants.end(), 0.0, [](double sum, Combattant const &combattant) { return sum + combattant.health; });
	double opponentTotalHP = std::max(1.0, 10.0 * opponentTotalHealth);
	
	return std::max(MIN_OPPONENT_RELATIVE_HEALTH_BONUS, sqrt(1.0 / opponentTotalHP));
	
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

void EnemyStackInfo::setUnitOffenseEffect(int unitId, CombatMode combatMode, double effect)
{
	this->offenseEffects.at(getUnitSlotById(unitId)).at(combatMode) = effect;
}

double EnemyStackInfo::getUnitOffenseEffect(int unitId, CombatMode combatMode) const
{
	return this->offenseEffects.at(getUnitSlotById(unitId)).at(combatMode);
}

double EnemyStackInfo::getVehicleOffenseEffect(int vehicleId, CombatMode combatMode) const
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

/*
Returns total destruction ratio after bombardment if any.
*/
double EnemyStackInfo::getDestructionRatio() const
{
	double totalBombardmentEffect = getTotalBombardmentEffect();
	double destructionRatio = combinedDirectEffect / (weight - totalBombardmentEffect + extraWeight);
	return destructionRatio;
}

/*
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
	debug("enemyStack computeAttackParameters %s\n", getLocationString(tile));
	
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
			, vehicleTravelTime.id, getLocationString(getVehicleMapTile(vehicleTravelTime.id))
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

bool isVendettaStoppedWith(int enemyFactionId)
{
	return (aiFactionInfo->diplo_status[enemyFactionId] & DIPLO_VENDETTA) != 0 && (aiFaction->diplo_status[enemyFactionId] & DIPLO_VENDETTA) == 0;
}

double getVehicleDestructionGain(int vehicleId)
{
	VEH &vehicle = Vehs[vehicleId];
	
	double vehicleDestructionGain = aiData.unitDestructionGains.at(vehicle.unit_id);
	
	// increase destruction gain for damaged vehicle preventing it from healing
	
	vehicleDestructionGain *= 1.00 + conf.ai_combat_damage_destruction_value_coefficient * getVehicleRelativeDamage(vehicleId);
	
	// increase destruction gain for combat vehicle close to player base
	
	if (isHostile(aiFactionId, vehicle.faction_id))
	{
		TileInfo &tileInfo = aiData.getVehicleTileInfo(vehicleId);
		
		int baseRange = tileInfo.baseRanges.at(vehicle.triad());
		
		double travelTime = (double) baseRange / (double) getVehicleSpeed(vehicleId);
		double travelTimeCoefficient = getExponentialCoefficient(conf.ai_base_threat_travel_time_scale, travelTime);
		
		vehicleDestructionGain *= travelTimeCoefficient;
		
	}
	
	return vehicleDestructionGain;
	
}

void clearPlayerFactionReferences()
{
	aiFactionId = -1;
	aiMFaction = nullptr;
	aiFaction = nullptr;
	aiFactionInfo = nullptr;
}

void setPlayerFactionReferences(int factionId)
{
	aiFactionId = factionId;
	aiMFaction = getMFaction(aiFactionId);
	aiFaction = getFaction(aiFactionId);
	aiFactionInfo = &(aiData.factionInfos.at(aiFactionId));
}

void computeUnitDestructionGains()
{
	debug("computeUnitDestructionGains - %s\n", aiMFaction->noun_faction);
	
	for (int unitId = 0; unitId < MaxProtoNum; unitId++)
	{
		UNIT &unit = Units[unitId];
		
		if (!unit.is_active())
		{
			aiData.unitDestructionGains.at(unitId) = 0.0;
			continue;
		}
		
		aiData.unitDestructionGains.at(unitId) = getUnitDestructionGain(unitId);
		
	}
	
	if (DEBUG)
	{
		for (int unitId = 0; unitId < MaxProtoNum; unitId++)
		{
			UNIT *unit = getUnit(unitId);
			
			if (!unit->is_active())
				continue;
			
			debug("\t%-32s %7.2f\n", unit->name, aiData.unitDestructionGains.at(unitId));
			
		}
		
	}
	
}

int getVehicleIdByPad0(int pad0)
{
	robin_hood::unordered_flat_map<int, int>::iterator pad0VehicleIdIterator = aiData.pad0VehicleIds.find(pad0);
	
	if (pad0VehicleIdIterator == aiData.pad0VehicleIds.end())
	{
		return -1;
	}
	else
	{
		return pad0VehicleIdIterator->second;
	}
	
}

VEH *getVehicleByPad0(int pad0)
{
	int vehicleId = getVehicleIdByPad0(pad0);
	return vehicleId == -1 ? nullptr : getVehicle(vehicleId);
}

MAP *getClosestPod(int vehicleId)
{
	MAP *closestPod = nullptr;
	double minTravelTime = DBL_MAX;
	
	for (MAP *tile : aiData.pods)
	{
		// reachable
		
		if (!isVehicleDestinationReachable(vehicleId, tile))
			continue;
		
		// get travel time
		
		double travelTime = getVehicleTravelTime(vehicleId, tile);
		if (travelTime == INF)
			continue;
		
		// update best
		
		if (travelTime < minTravelTime)
		{
			closestPod = tile;
			minTravelTime = travelTime;
		}
		
	}
	
	return closestPod;
	
}

int getFactionMaxConOffenseValue(int factionId)
{
	int bestWeaponOffenseValue = 0;

	for (int unitId : getDesignedFactionUnitIds(factionId, true, true))
	{
		UNIT *unit = &(Units[unitId]);

		// combat

		if (!isCombatUnit(unitId))
			continue;

		// weapon offense value

		int weaponOffenseValue = Weapon[unit->weapon_id].offense_value;

		// update best weapon offense value

		bestWeaponOffenseValue = std::max(bestWeaponOffenseValue, weaponOffenseValue);

	}

	return bestWeaponOffenseValue;

}

int getFactionMaxConDefenseValue(int factionId)
{
	int bestArmorDefenseValue = 0;

	for (int unitId : getDesignedFactionUnitIds(factionId, true, true))
	{
		UNIT *unit = &(Units[unitId]);

		// skip non combat units

		if (!isCombatUnit(unitId))
			continue;

		// get armor defense value

		int armorDefenseValue = Armor[unit->armor_id].defense_value;

		// update best armor defense value

		bestArmorDefenseValue = std::max(bestArmorDefenseValue, armorDefenseValue);

	}

	return bestArmorDefenseValue;

}

std::array<int, 3> getFactionFastestTriadChassisIds(int factionId)
{
	std::array<int, 3> fastestTriadChassisIds;
	std::fill(fastestTriadChassisIds.begin(), fastestTriadChassisIds.end(), -1);
	
    for (VehChassis chassisId : {CHS_NEEDLEJET, CHS_COPTER, CHS_GRAVSHIP, })
	{
		if (has_tech(Chassis[chassisId].preq_tech, factionId))
		{
			fastestTriadChassisIds.at(TRIAD_AIR) = chassisId;
		}
	}
    
    for (VehChassis chassisId : {CHS_FOIL, CHS_CRUISER, })
	{
		if (has_tech(Chassis[chassisId].preq_tech, factionId))
		{
			fastestTriadChassisIds.at(TRIAD_SEA) = chassisId;
		}
	}
    
    for (VehChassis chassisId : {CHS_INFANTRY, CHS_SPEEDER, CHS_HOVERTANK, })
	{
		if (has_tech(Chassis[chassisId].preq_tech, factionId))
		{
			fastestTriadChassisIds.at(TRIAD_AIR) = chassisId;
		}
	}
	
	return fastestTriadChassisIds;
	
}

bool isWithinAlienArtilleryRange(int vehicleId)
{
	VEH *vehicle = getVehicle(vehicleId);

	for (int otherVehicleId = 0; otherVehicleId < *VehCount; otherVehicleId++)
	{
		VEH *otherVehicle = getVehicle(otherVehicleId);

		if (otherVehicle->faction_id == 0 && otherVehicle->unit_id == BSC_SPORE_LAUNCHER && map_range(vehicle->x, vehicle->y, otherVehicle->x, otherVehicle->y) <= 2)
			return true;

	}

	return false;

}

/*
Checks if faction is enabled to use WTP algorithms.
*/
bool isWtpEnabledFaction(int factionId)
{
	return (factionId != 0 && !is_human(factionId) && thinker_enabled(factionId) && conf.ai_useWTPAlgorithms && conf.wtp_enabled_factions[factionId]);
}

bool compareIdIntValueAscending(const IdIntValue &a, const IdIntValue &b)
{
	return a.value < b.value;
}

bool compareIdIntValueDescending(const IdIntValue &a, const IdIntValue &b)
{
	return a.value > b.value;
}

bool compareIdDoubleValueAscending(const IdDoubleValue &a, const IdDoubleValue &b)
{
	return a.value < b.value;
}

bool compareIdDoubleValueDescending(const IdDoubleValue &a, const IdDoubleValue &b)
{
	return a.value > b.value;
}

bool isOffensiveUnit(int unitId, int factionId)
{
	UNIT *unit = getUnit(unitId);
	int offenseValue = getUnitOffenseValue(unitId);
	
	// non infantry unit is always offensive
	
	if (unit->chassis_id != CHS_INFANTRY)
		return true;
	
	// artillery unit is always offensive
	
	if (isArtilleryUnit(unitId))
		return true;
	
	// psi offense makes it offensive unit
	
	if (offenseValue < 0)
		return true;
	
	// conventional offense should be in higher range of this faction offense
	
	return offenseValue > aiData.factionInfos.at(factionId).maxConOffenseValue / 2;
	
}

bool isOffensiveVehicle(int vehicleId)
{
	return isOffensiveUnit(Vehs[vehicleId].unit_id, Vehs[vehicleId].faction_id);
}

double getExponentialCoefficient(double scale, double value)
{
	return exp(-value / scale);
}

double getStatisticalBaseSize(double age)
{
	double adjustedAge = std::max(0.0, age - conf.ai_base_size_b);
	double baseSize = std::max(1.0, conf.ai_base_size_a * sqrt(adjustedAge) + conf.ai_base_size_c);
	return baseSize;
}

/*
Estimates base size based on statistics.
*/
double getBaseEstimatedSize(double currentSize, double currentAge, double futureAge)
{
	return currentSize * (getStatisticalBaseSize(futureAge) / getStatisticalBaseSize(currentAge));
}

double getBaseStatisticalWorkerCount(double age)
{
	double adjustedAge = std::max(0.0, age - conf.ai_base_size_b);
	return std::max(1.0, conf.ai_base_size_a * sqrt(adjustedAge) + conf.ai_base_size_c);
}

/*
Estimates base size based on statistics.
*/
double getBaseStatisticalProportionalWorkerCountIncrease(double currentAge, double futureAge)
{
	return getBaseStatisticalWorkerCount(futureAge) / getBaseStatisticalWorkerCount(currentAge);
}

/*
Projects base population in given number of turns.
*/
int getBaseProjectedSize(int baseId, int turns)
{
	BASE *base = &(Bases[baseId]);
	
	int nutrientCostFactor = mod_cost_factor(base->faction_id, RSC_NUTRIENT, baseId);
	int nutrientsAccumulated = base->nutrients_accumulated + base->nutrient_surplus * turns;
	int projectedPopulation = base->pop_size;
	
	while (nutrientsAccumulated >= nutrientCostFactor * (projectedPopulation + 1))
	{
		nutrientsAccumulated -= nutrientCostFactor * (projectedPopulation + 1);
		projectedPopulation++;
	}
	
	// do not go over population limit
	
	int populationLimit = getBasePopulationLimit(baseId);
	projectedPopulation = std::min(projectedPopulation, std::max((int)base->pop_size, populationLimit));
	
	return projectedPopulation;
	
}

/*
Estimates time for base to reach given population size.
*/
int getBaseGrowthTime(int baseId, int targetSize)
{
	BASE *base = &(Bases[baseId]);

	// already reached

	if (base->pop_size >= targetSize)
		return 0;

	// calculate total number of nutrient rows to fill

	int totalNutrientRows = (targetSize + base->pop_size + 1) * (targetSize - base->pop_size) / 2;

	// calculate total number of nutrients

	int totalNutrients = totalNutrientRows * mod_cost_factor(base->faction_id, RSC_NUTRIENT, baseId);

	// calculate time to accumulate these nutrients

	int projectedTime = (totalNutrients + (base->nutrient_surplus - 1)) / std::max(1, base->nutrient_surplus) + (targetSize - base->pop_size);

	return projectedTime;

}

/**
Calculates nutrient + minerals + energy resource score.
*/
double getResourceScore(double nutrient, double mineral, double energy)
{
	return
		+ conf.ai_resource_score_nutrient * nutrient
		+ conf.ai_resource_score_mineral * mineral
		+ conf.ai_resource_score_energy * energy
	;
}

/**
Calculates mineral + energy resource score.
*/
double getResourceScore(double mineral, double energy)
{
	return getResourceScore(0, mineral, energy);
}

int getBaseFoundingTurn(int baseId)
{
	BASE *base = getBase(baseId);

	return base->pad_1;

}

/*
Gets base age.
*/
int getBaseAge(int baseId)
{
	BASE *base = getBase(baseId);

	return (*CurrentTurn) - base->pad_1;

}

/*
Estimates base population size growth rate.
*/
double getBaseSizeGrowth(int baseId)
{
	BASE *base = getBase(baseId);
	double age = std::max(10.0, std::max(conf.ai_base_size_b, (double)getBaseAge(baseId)));
	double sqrtValue = std::max(1.0, sqrt(age - conf.ai_base_size_b));
	double growthRatio =
		(conf.ai_base_size_a / 2.0 / sqrtValue)
		/
		std::max(1.0, (conf.ai_base_size_a * sqrtValue + conf.ai_base_size_c))
	;

	return growthRatio * base->pop_size;

}

/*
Estimates single action value diminishing with delay.
Example: colony is building a base.
*/
double getBonusDelayEffectCoefficient(double delay)
{
	return exp(- delay / aiData.developmentScale);
}

/*
Estimates multi action value diminishing with delay.
Example: base is building production.
*/
double getBuildTimeEffectCoefficient(double delay)
{
	return 1.0 / (exp(delay / aiData.developmentScale) - 1.0);
}

double getHarmonicMean(std::vector<std::vector<double>> parameters)
{
	int count = 0;
	double reciprocalSum = 0.0;

	for (std::vector<double> &fraction : parameters)
	{
		if (fraction.size() != 2)
			continue;

		double numerator = fraction.at(0);
		double denominator = fraction.at(1);

		if (numerator <= 0.0 || denominator <= 0.0)
			continue;

		double value = numerator / denominator;
		double reciprocal = 1.0 / value;

		count++;
		reciprocalSum += reciprocal;

	}

	if (count == 0)
		return 0.0;

	return 1.0 / (reciprocalSum / (double)count);

}

std::vector<MoveAction> getMoveActions(MovementType movementType, MAP *origin, int initialMovementAllowance, bool regardObstacle, bool regardZoc)
{
	robin_hood::unordered_flat_map<int, int> movementAllowances;
	robin_hood::unordered_flat_set<int> openNodes;
	robin_hood::unordered_flat_set<int> newOpenNodes;
	
	// set initial values
	
	int originIndex = origin - *MapTiles;
	
	movementAllowances.emplace(originIndex, initialMovementAllowance);
	openNodes.insert(originIndex);
	
	// iterate locations
	
	while (!openNodes.empty())
	{
		for (int currentTileIndex : openNodes)
		{
			TileInfo const &currentTileInfo = aiData.getTileInfo(currentTileIndex);
			int currentMovementAllowance = movementAllowances.at(currentTileIndex);
			
			for (AngleTileInfo const &adjacentAngleTileInfo : currentTileInfo.adjacentAngleTileInfos)
			{
				int angle = adjacentAngleTileInfo.angle;
				TileInfo const *adjacentTileInfo = adjacentAngleTileInfo.tileInfo;
				int adjacentTileIndex = adjacentTileInfo->tile - *MapTiles;
				
				// regard obstacle and ZoC
				
				if (regardObstacle)
				{
					if (adjacentTileInfo->blocked)
						continue;
				}
				
				// regard ZOC
				
				if (regardZoc)
				{
					if (currentTileInfo.orgZoc && adjacentTileInfo->dstZoc)
						continue;
				}
				
				// hexCost
				
				int hexCost = currentTileInfo.hexCosts.at(movementType).at(angle);
				if (hexCost == -1)
					continue;
				
				// allowance change
				
				int adjacentMovementAllowance = std::max(0, currentMovementAllowance - hexCost);
				
				// update best
				
				if (movementAllowances.find(adjacentTileIndex) == movementAllowances.end())
				{
					movementAllowances.emplace(adjacentTileIndex, adjacentMovementAllowance);
				}
				else if (adjacentMovementAllowance > movementAllowances.at(adjacentTileIndex))
				{
					movementAllowances.at(adjacentTileIndex) = adjacentMovementAllowance;
				}
				else
				{
					// not a best path
					continue;
				}
				
				// add open node
				
				if (movementAllowances.at(adjacentTileIndex) > 0)
				{
					newOpenNodes.insert(adjacentTileIndex);
				}
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	std::vector<MoveAction> moveActions;
	
	for (robin_hood::pair<int, int> movementAllowanceEntry : movementAllowances)
	{
		int tileIndex = movementAllowanceEntry.first;
		int movementAllowance = movementAllowanceEntry.second;
		
		MAP *tile = *MapTiles + tileIndex;
		
		moveActions.push_back({tile, movementAllowance});
		
	}
	
	return moveActions;
	
}

/*
Returns vehicle available move destinations.
Player vehicles subtract current moves spent as they are currently moving.
*/
std::vector<MoveAction> getVehicleMoveActions(int vehicleId, bool regardObstacle)
{
	Profiling::start("- getVehicleMoveActions");
	
	VEH *vehicle = getVehicle(vehicleId);
	MAP *vehicleTile = getVehicleMapTile(vehicleId);
	MovementType movementType = getVehicleMovementType(vehicleId);
	int movementAllowance = getVehicleMaxMoves(vehicleId) - (vehicle->faction_id == aiFactionId ? vehicle->moves_spent : 0);
	bool regardZoc = regardObstacle ? isVehicleZocRestricted(vehicleId) : false;
	
	std::vector<MoveAction> moveActions = getMoveActions(movementType, vehicleTile, movementAllowance, regardObstacle, regardZoc);
	
	Profiling::stop("- getVehicleMoveActions");
	
	return moveActions;
	
}

/*
Searches for potentially enemy attackable locations.
*/
robin_hood::unordered_flat_map<MAP *, double> getPotentialMeleeAttackTargets(int vehicleId)
{
	robin_hood::unordered_flat_map<MAP *, double> attackTargets;
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return attackTargets;
	
	// explore reachable locations
	
	Profiling::start("- getPotentialMeleeAttackTargets");
	for (AttackAction const &attackAction : getMeleeAttackActions(vehicleId, false, false))
	{
		MAP *target = attackAction.target;
		double hastyCoefficient = attackAction.hastyCoefficient;
		
		// add action
		
		if (attackTargets.find(target) == attackTargets.end())
		{
			attackTargets.emplace(target, hastyCoefficient);
		}
		else if (hastyCoefficient > attackTargets.at(target))
		{
			attackTargets.at(target) = hastyCoefficient;
		}
		
	}
	Profiling::stop("- getPotentialMeleeAttackTargets");
	
	return attackTargets;
	
}

/*
Searches for potentially attackable locations.
Path search excludes hostile vehicles.
*/
robin_hood::unordered_flat_map<MAP *, double> getPotentialArtilleryAttackTargets(int vehicleId)
{
	robin_hood::unordered_flat_map<MAP *, double> attackTargets;
	
	// artillery vehicle
	
	if (!isArtilleryVehicle(vehicleId))
		return attackTargets;
	
	// explore reachable locations
	
	Profiling::start("- getPotentialArtilleryAttackTargets");
	for (AttackAction const &attackAction : getArtilleryAttackActions(vehicleId, false, false))
	{
		MAP *target = attackAction.target;
		
		// add action
		
		if (attackTargets.find(target) == attackTargets.end())
		{
			attackTargets.emplace(target, 1.0);
		}
		
	}
	Profiling::stop("- getPotentialArtilleryAttackTargets");
	
	return attackTargets;
	
}

/*
Searches for locations vehicle can melee attack at.
*/
std::vector<AttackAction> getMeleeAttackActions(int vehicleId, bool regardObstacle, bool requireEnemy)
{
	std::vector<AttackAction> attackActions;
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return attackActions;
	
	// explore reachable locations
	
	Profiling::start("- getMeleeAttackActions - getVehicleReachableLocations");
	for (MoveAction const &moveAction : getVehicleMoveActions(vehicleId, regardObstacle))
	{
		MAP *tile = moveAction.destination;
		int remainingMovementPoints = moveAction.remainingMovementPoints;
		
		// need movement points to attack
		
		if (remainingMovementPoints <= 0)
			continue;
		
		double hastyCoefficient =
			remainingMovementPoints >= Rules->move_rate_roads ?
				1.0
				:
				static_cast<double>(remainingMovementPoints) / static_cast<double>(Rules->move_rate_roads)
		;
		
		// explore adjacent tiles
		
		for (AngleTileInfo const &adjacentAngleTileInfo : aiData.getTileInfo(tile).adjacentAngleTileInfos)
		{
			MAP *targetTile = adjacentAngleTileInfo.tileInfo->tile;
			
			// enemy in tile if required
			
			if (requireEnemy && !aiData.hasEnemyStack(targetTile))
				continue;
			
			// can melee attack tile
			
			if (!isVehicleCanMeleeAttack(vehicleId, tile, targetTile))
				continue;
			
			// add action
			
			attackActions.push_back({tile, targetTile, hastyCoefficient});
			
		}
		
	}
	Profiling::stop("- getMeleeAttackActions - getVehicleReachableLocations");
	
	return attackActions;
	
}

/**
Searches for locations vehicle can artillery attack at.
*/
std::vector<AttackAction> getArtilleryAttackActions(int vehicleId, bool regardObstacle, bool requireEnemy)
{
	std::vector<AttackAction> attackActions;
	
	// artillery vehicle
	
	if (!isArtilleryVehicle(vehicleId))
		return attackActions;
	
	// explore reachable locations
	
	Profiling::start("- getArtilleryAttackActions - getVehicleReachableLocations");
	for (MoveAction const &moveAction : getVehicleMoveActions(vehicleId, regardObstacle))
	{
		MAP *tile = moveAction.destination;
		int remainingMovementPoints = moveAction.remainingMovementPoints;
		
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		// need movement points to attack
		
		if (remainingMovementPoints <= 0)
			continue;
		
		// explore artillery tiles
		
		for (TileInfo const *targetTileInfo : tileInfo.range2NoCenterTileInfos)
		{
			MAP *targetTile = targetTileInfo->tile;
			
			// enemy in tile if required
			
			if (requireEnemy && !aiData.hasEnemyStack(targetTile))
				continue;
			
			// add action
			
			attackActions.push_back({tile, targetTile, 1.0});
			
		}
		
	}
	Profiling::stop("- getArtilleryAttackActions - getVehicleReachableLocations");
	
	return attackActions;
	
}

/**
Searches for locations vehicle can melee attack at.
*/
robin_hood::unordered_flat_map<int, double> getMeleeAttackLocations(int vehicleId)
{
	Profiling::start("- getMeleeAttackLocations");
	
	VEH &vehicle = Vehs[vehicleId];
	int vehicleTileIndex = getVehicleMapTileIndex(vehicleId);
	int factionId = vehicle.faction_id;
	int unitId = vehicle.unit_id;
	int movementAllowance = getVehicleMaxMoves(vehicleId) - (factionId == aiFactionId ? vehicle.moves_spent : 0);
	MovementType movementType = getUnitMovementType(factionId, unitId);
	
	robin_hood::unordered_flat_map<int, int> moveLocations;
	robin_hood::unordered_flat_map<int, double> attackLocations;
	
	// melee vehicle
	
	if (!isMeleeVehicle(vehicleId))
	{
		Profiling::stop("- getMeleeAttackLocations");
		return attackLocations;
	}
	
	// not on transport
	
	if (isVehicleOnTransport(vehicleId))
	{
		Profiling::stop("- getArtilleryAttackLocations");
		return attackLocations;
	}
	
	// has moves
	
	if (movementAllowance <= 0)
	{
		Profiling::stop("- getMeleeAttackLocations");
		return attackLocations;
	}
	
	// explore reachable locations
	
	robin_hood::unordered_flat_set<int> openNodes;
	robin_hood::unordered_flat_set<int> newOpenNodes;
	
	moveLocations.emplace(vehicleTileIndex, movementAllowance);
	openNodes.insert(vehicleTileIndex);
	
	while (!openNodes.empty())
	{
		for (int currentTileIndex : openNodes)
		{
			MAP *currentTile = *MapTiles + currentTileIndex;
			TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
			int currentTileMovementAllowance = moveLocations.at(currentTileIndex);
			
			// iterate adjacent tiles
			
			for (AngleTileInfo const &adjacentAngleTileInfo : currentTileInfo.adjacentAngleTileInfos)
			{
				int angle = adjacentAngleTileInfo.angle;
				TileInfo &adjacentTileInfo = *(adjacentAngleTileInfo.tileInfo);
				int adjacentTileIndex = adjacentTileInfo.index;
				MAP *adjacentTile = *MapTiles + adjacentTileIndex;
				
				// not blocked
				
				if (adjacentTileInfo.blocked)
					continue;
				
				// hexCost
				
				int hexCost = currentTileInfo.hexCosts.at(movementType).at(angle);
				if (hexCost == -1)
					continue;
				
				// new movementAllowance
				
				int adjacentTileMovementAllowance = currentTileMovementAllowance - hexCost;
				
				// update value
				
				if (moveLocations.find(adjacentTileIndex) == moveLocations.end() || adjacentTileMovementAllowance > moveLocations.at(adjacentTileIndex))
				{
					moveLocations[adjacentTileIndex] = adjacentTileMovementAllowance;
					
					if (adjacentTileMovementAllowance > 0)
					{
						newOpenNodes.insert(adjacentTileIndex);
					}
					
				}
				
				// can melee attack
				
				if (!isVehicleCanMeleeAttack(vehicleId, currentTile, adjacentTile))
					continue;
				
				// update attack
				
				double hastyCoefficient = currentTileMovementAllowance >= Rules->move_rate_roads ? 1.0 : (double)currentTileMovementAllowance / (double)Rules->move_rate_roads;
				if (attackLocations.find(adjacentTileIndex) == attackLocations.end() || hastyCoefficient > attackLocations.at(adjacentTileIndex))
				{
					attackLocations[adjacentTileIndex] = hastyCoefficient;
				}
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	Profiling::stop("- getMeleeAttackLocations");
	return attackLocations;
	
}

/**
Searches for locations vehicle can melee attack at.
*/
robin_hood::unordered_flat_set<int> getArtilleryAttackLocations(int vehicleId)
{
	Profiling::start("- getArtilleryAttackLocations");
	
	VEH &vehicle = Vehs[vehicleId];
	int vehicleTileIndex = getVehicleMapTileIndex(vehicleId);
	int factionId = vehicle.faction_id;
	int unitId = vehicle.unit_id;
	int movementAllowance = getVehicleMaxMoves(vehicleId) - (factionId == aiFactionId ? vehicle.moves_spent : 0);
	MovementType movementType = getUnitMovementType(factionId, unitId);
	
	robin_hood::unordered_flat_map<int, int> moveLocations;
	robin_hood::unordered_flat_set<int> attackLocations;
	
	// artillery vehicle
	
	if (!isArtilleryVehicle(vehicleId))
	{
		Profiling::stop("- getArtilleryAttackLocations");
		return attackLocations;
	}
	
	// not on transport
	
	if (isVehicleOnTransport(vehicleId))
	{
		Profiling::stop("- getArtilleryAttackLocations");
		return attackLocations;
	}
	
	// has moves
	
	if (movementAllowance <= 0)
	{
		Profiling::stop("- getArtilleryAttackLocations");
		return attackLocations;
	}
	
	// explore reachable locations
	
	robin_hood::unordered_flat_set<int> openNodes;
	robin_hood::unordered_flat_set<int> newOpenNodes;
	
	moveLocations.emplace(vehicleTileIndex, movementAllowance);
	openNodes.insert(vehicleTileIndex);
	
	while (!openNodes.empty())
	{
		for (int currentTileIndex : openNodes)
		{
			MAP *currentTile = *MapTiles + currentTileIndex;
			TileInfo &currentTileInfo = aiData.tileInfos.at(currentTileIndex);
			int currentTileMovementAllowance = moveLocations.at(currentTileIndex);
			
			// update attack
			
			for (MAP *artilleryRangeTile : getRangeTiles(currentTile, Rules->artillery_max_rng, false))
			{
				int artilleryRangeTileIndex = artilleryRangeTile - *MapTiles;
				attackLocations.insert(artilleryRangeTileIndex);
			}
			
			// iterate adjacent tiles
			
			for (AngleTileInfo const &adjacentAngleTileInfo : currentTileInfo.adjacentAngleTileInfos)
			{
				int angle = adjacentAngleTileInfo.angle;
				TileInfo &adjacentTileInfo = *(adjacentAngleTileInfo.tileInfo);
				int adjacentTileIndex = adjacentTileInfo.index;
				
				// not blocked
				
				if (adjacentTileInfo.blocked)
					continue;
				
				// hexCost
				
				int hexCost = currentTileInfo.hexCosts.at(movementType).at(angle);
				if (hexCost == -1)
					continue;
				
				// new movementAllowance
				
				int adjacentTileMovementAllowance = currentTileMovementAllowance - hexCost;
				
				// update value
				
				if (moveLocations.find(adjacentTileIndex) == moveLocations.end() || adjacentTileMovementAllowance > moveLocations.at(adjacentTileIndex))
				{
					moveLocations[adjacentTileIndex] = adjacentTileMovementAllowance;
					
					if (adjacentTileMovementAllowance > 0)
					{
						newOpenNodes.insert(adjacentTileIndex);
					}
					
				}
				
			}
			
		}
		
		openNodes.clear();
		openNodes.swap(newOpenNodes);
		
	}
	
	Profiling::stop("- getArtilleryAttackLocations");
	return attackLocations;
	
}

bool isVehicleAllowedMove(int vehicleId, MAP *from, MAP *to)
{
	VEH *vehicle = getVehicle(vehicleId);
	bool toTileBlocked = isBlocked(to);
	bool zoc = isZoc(from, to);

	if (toTileBlocked)
		return false;

	if (vehicle->triad() == TRIAD_LAND && !is_ocean(from) && !is_ocean(to) && zoc)
		return false;

	return true;

}

/*
Disband unit to reduce support burden.
*/
void disbandOrversupportedVehicles(int factionId)
{
	debug("disbandOrversupportedVehicles - %s\n", MFactions[aiFactionId].noun_faction);

	// oversupported bases delete outside units then inside

	int sePolice = getFaction(aiFactionId)->SE_police_pending;
	int warWearinessLimit = (sePolice <= -4 ? 0 : (sePolice == -3 ? 1 : -1));

	std::vector<int> killVehicleIds;

	if (warWearinessLimit != -1)
	{
		debug("\tremove war weariness units outside\n");

		robin_hood::unordered_flat_map<int, std::vector<int>> outsideVehicles;

		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			VEH *vehicle = getVehicle(vehicleId);
			int triad = vehicle->triad();
			MAP *vehicleTile = getVehicleMapTile(vehicleId);

			if (vehicle->faction_id != factionId)
				continue;

			if (!isCombatVehicle(vehicleId))
				continue;

			if (vehicle->home_base_id == -1)
				continue;

			BASE *base = getBase(vehicle->home_base_id);

			if ((triad == TRIAD_AIR || vehicleTile->owner != aiFactionId) && base->pop_size < 5)
			{
				outsideVehicles[vehicle->home_base_id].push_back(vehicleId);
				debug("\t\toutsideVehicles: %s\n", getLocationString(vehicleTile));
			}

		}

		for (robin_hood::pair<int, std::vector<int>> &outsideVehicleEntry : outsideVehicles)
		{
			std::vector<int> &baseOutsideVehicles = outsideVehicleEntry.second;

			for (unsigned int i = 0; i < baseOutsideVehicles.size() - warWearinessLimit; i++)
			{
				int vehicleId = baseOutsideVehicles.at(i);

				killVehicleIds.push_back(vehicleId);
				debug
				(
					"\t\t[%4d] %s : %-25s\n"
					, vehicleId, getLocationString(getVehicleMapTile(vehicleId)), getBase(getVehicle(vehicleId)->home_base_id)->name
				);

			}

		}

	}

	std::sort(killVehicleIds.begin(), killVehicleIds.end(), std::greater<int>());

	for (int vehicleId : killVehicleIds)
	{
		mod_veh_kill(vehicleId);
	}

}

/*
Disband unneeded vehicles.
*/
void disbandUnneededVehicles()
{
	debug("disbandUnneededVehicles - %s\n", MFactions[aiFactionId].noun_faction);

	// ship without task in internal sea

	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();

		// ours

		if (vehicle->faction_id != aiFactionId)
			continue;

		// combat

		if (!isCombatVehicle(vehicleId))
			continue;

		// ship

		if (triad != TRIAD_SEA)
			continue;

		// does not have task

		if (hasTask(vehicleId))
			continue;

		// internal sea

		if (isSharedSea(vehicleTile))
			continue;

		// disband

		mod_veh_kill(vehicleId);

	}

}

/**
Checks if unit can capture given base in general.
*/
bool isUnitCanCaptureBase(int unitId, MAP *baseTile)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	bool baseTileOcean = is_ocean(baseTile);
	
	switch (triad)
	{
	case TRIAD_AIR:
		
		// some air chassis cannot capture base
		
		switch (unit->chassis_id)
		{
		case CHS_NEEDLEJET:
		case CHS_COPTER:
		case CHS_MISSILE:
			return false;
			break;
		
		}
		
		break;
	
	case TRIAD_SEA:
		
		// sea unit can not capture land base
		
		if (!baseTileOcean)
			return false;
		
		break;
		
	case TRIAD_LAND:
		
		// non-amphibious land unit can not capture sea base
		
		if (baseTileOcean && !isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
			return false;
		
		break;
		
	}
	
	// all checks passed
	
	return true;
	
}

/**
Estimates unit combined cost and support.
- mineral row = 10
- combat unit lives 40 turns
*/
int getCombatUnitTrueCost(int unitId)
{
	return 10 * Units[unitId].cost + 40 * getUnitSupport(unitId);
}

int getCombatVehicleTrueCost(int vehicleId)
{
	return getCombatUnitTrueCost(Vehs[vehicleId].unit_id);
}

/**
Computes economical effect by given standard parameters.
delay:			how far from now the effect begins.
bonus:			one time economical bonus.
income:			income per turn.
incomeGrowth:	income growth per turn.
incomeGrowth2:	income growth growth per turn.
*/
double getGain(double bonus, double income, double incomeGrowth, double incomeGrowth2)
{
	double scale = aiData.developmentScale;
	return (bonus + income * scale + incomeGrowth * scale * scale + incomeGrowth2 * 2 * scale * scale * scale) / scale;
}

/**
Computes delayed gain.
gain:			economical gain without delay.
delay:			how far from now the effect begins.
*/
double getGainDelay(double gain, double delay)
{
	double scale = aiData.developmentScale;
	return exp(- delay / scale) * gain;
}

/**
Computes time interval gain.
gain:			economical gain without delay and lasting forever.
beginTime:		how far from now the effect begins.
endTime:		how far from now the effect ends.
*/
double getGainTimeInterval(double gain, double beginTime, double endTime)
{
	double scale = aiData.developmentScale;
	return (exp(- beginTime / scale) - exp(- endTime / scale)) * gain;
}

/**
Computes repetitive gain.
probability - the probability gain will be repeated next time
period - repetition period
*/
double getGainRepetion(double gain, double probability, double period)
{
//	period = std::max(1.0, period);
	double scale = aiData.developmentScale;
	return exp(- period / scale) / (1.0 - probability * exp(- period / scale)) * gain;
}

/**
Computes one time bonus gain.
*/
double getGainBonus(double bonus)
{
	double scale = aiData.developmentScale;
	return bonus / scale;
}

/**
Computes steady income gain.
*/
double getGainIncome(double income)
{
	return income;
}

/**
Computes linear income growth gain.
*/
double getGainIncomeGrowth(double incomeGrowth)
{
	double scale = aiData.developmentScale;
	return incomeGrowth * scale;
}

/**
Computes quadradic income growth gain.
*/
double getGainIncomeGrowth2(double incomeGrowth)
{
	double scale = aiData.developmentScale;
	return incomeGrowth * 2 * scale * scale;
}

/**
Computes base summary resource score.
*/
double getBaseIncome(int baseId, bool withNutrients)
{
	BASE *base = getBase(baseId);
	
	double income;
	
	if (!withNutrients)
	{
		income = getResourceScore(base->mineral_intake_2, base->economy_total + base->labs_total);
	}
	else
	{
		income = getResourceScore(base->nutrient_surplus, base->mineral_intake_2, base->economy_total + base->labs_total);
	}
	
	return income;
	
}

/**
Computes average base citizen income.
*/
double getBaseCitizenIncome(int baseId)
{
	BASE *base = getBase(baseId);
	return getBaseIncome(baseId) / (double)base->pop_size;
}

/**
Mean base income.
*/
double getMeanBaseIncome(double age)
{
	return
		getResourceScore
		(
			conf.ai_base_mineral_intake2_a * age + conf.ai_base_mineral_intake2_b,
			conf.ai_base_budget_intake2_a * age * age + conf.ai_base_budget_intake2_b * age + conf.ai_base_budget_intake2_c
		)
	;
}

/**
Estimates average new base gain from current age to eternity.
*/
double getMeanBaseGain(double age)
{
	return
		// mineral intake 2
		+ getGain
			(
				0.0,
				getResourceScore(conf.ai_base_mineral_intake2_a * age + conf.ai_base_mineral_intake2_b, 0.0),
				getResourceScore(conf.ai_base_mineral_intake2_a, 0.0),
				0.0
			)
		// budget intake 2
		+ getGain
			(
				0.0,
				getResourceScore(0.0, conf.ai_base_budget_intake2_a * age * age + conf.ai_base_budget_intake2_b * age + conf.ai_base_budget_intake2_c),
				getResourceScore(0.0, conf.ai_base_budget_intake2_a * 2 * age + conf.ai_base_budget_intake2_b),
				getResourceScore(0.0, conf.ai_base_budget_intake2_a)
			)
	;
}

/**
Estimates expected new base gain from inception to eternity.
*/
double getNewBaseGain()
{
	return getMeanBaseGain(0);
}

/**
Estimates expected colony gain accounting for travel time and colony maintenance.
*/
double getLandColonyGain()
{
	return getGainDelay(getNewBaseGain(), AVERAGE_LAND_COLONY_TRAVEL_TIME) + getGainTimeInterval(getGainIncome(getResourceScore(-1.0, 0.0)), 0.0, AVERAGE_LAND_COLONY_TRAVEL_TIME);
}

/**
Computes average base income.
*/
double getAverageBaseIncome()
{
	if (aiData.baseIds.size() == 0)
		return 0.0;
	
	double sumBaseIncome = 0.0;
	
	for (int baseId : aiData.baseIds)
	{
		sumBaseIncome += getBaseIncome(baseId);
	}
	
	return sumBaseIncome / (double)aiData.baseIds.size();
	
}

/**
Computes expected base gain adjusting for current base income.
*/
double getBaseGain(int popSize, int nutrientCostFactor, Resource baseIntake2)
{
	// gain
	
	double income = getResourceScore(baseIntake2.mineral, baseIntake2.energy);
	double incomeGain = getGainIncome(income);
	
	double popualtionGrowth = baseIntake2.nutrient / (double)(nutrientCostFactor * (1 + popSize));
	double incomeGrowth = aiData.averageCitizenResourceIncome * popualtionGrowth;
	double incomeGrowthGain = getGainIncomeGrowth(incomeGrowth);
	
	double gain =
		+ incomeGain
		+ incomeGrowthGain
	;
	
//	if (DEBUG)
//	{
//		debug
//		(
//			"getBaseGain(popSize=%2d, nutrientCostFactor=%2d, baseIntake2=%f,%f,%f)\n"
//			, popSize, nutrientCostFactor, baseIntake2.nutrient, baseIntake2.mineral, baseIntake2.energy
//		);
//		
//		debug
//		(
//			"\tincome=%5.2f"
//			" incomeGain=%5.2f"
//			" averageCitizenResourceIncome=%5.2f"
//			" popualtionGrowth=%5.2f"
//			" incomeGrowth=%5.2f"
//			" incomeGrowthGain=%5.2f"
//			" gain=%5.2f"
//			"\n"
//			, income
//			, incomeGain
//			, aiData.averageCitizenResourceIncome
//			, popualtionGrowth
//			, incomeGrowth
//			, incomeGrowthGain
//			, gain
//		);
//		
//	}
	
	return gain;
	
}

double getBaseGain(int baseId, Resource baseIntake2)
{
	BASE *base = getBase(baseId);
	int nutrientCostFactor = mod_cost_factor(aiFactionId, RSC_NUTRIENT, baseId);
	
	return getBaseGain(base->pop_size, nutrientCostFactor, baseIntake2);
	
}

/**
Computes expected base gain adjusting for current base income.
*/
double getBaseGain(int baseId)
{
	BASE *base = getBase(baseId);
	int nutrientCostFactor = mod_cost_factor(aiFactionId, RSC_NUTRIENT, baseId);
	Resource baseIntake2 = getBaseResourceIntake2(baseId);
	
	return getBaseGain(base->pop_size, nutrientCostFactor, baseIntake2);
	
}

/**
Computes base value for keeping or capturing.
Base gain + SP values.
*/
double getBaseValue(int baseId)
{
	// base gain
	
	double baseGain = getBaseGain(baseId);
	
	// SP value
	
	double spValue = 0.0;
	
	for (int spFacilityId = SP_ID_First; spFacilityId <= SP_ID_Last; spFacilityId++)
	{
		if (isBaseHasFacility(baseId, spFacilityId))
		{
			double spCost = Rules->mineral_cost_multi * getFacility(spFacilityId)->cost;
			spValue += getGainBonus(spCost);
		}
		
	}
	
	return baseGain + spValue;
	
}

/**
Computes expected base improvement gain adjusting for current base income.
Compares the mean base gain adjusted for current age with same but with improved growth.
*/
double getBaseImprovementGain(int baseId, Resource oldBaseIntake2, Resource newBaseIntake2)
{
	double oldGain = getBaseGain(baseId, oldBaseIntake2);
	double newGain = getBaseGain(baseId, newBaseIntake2);
	
	double improvementGain = newGain - oldGain;
	
	return improvementGain;
	
}

COMBAT_TYPE getWeaponType(int vehicleId)
{
	return (getVehicleOffenseValue(vehicleId) < 0 ? CT_PSI : CT_CON);
}

COMBAT_TYPE getArmorType(int vehicleId)
{
	return (getVehicleDefenseValue(vehicleId) < 0 ? CT_PSI : CT_CON);
}

CombatStrength getMeleeAttackCombatStrength(int vehicleId)
{
	COMBAT_TYPE attackerCOMBAT_TYPE = getWeaponType(vehicleId);
	
	CombatStrength combatStrength;
	
	// melee capable vehicle
	
	if (!isMeleeVehicle(vehicleId))
		return combatStrength;
		
	switch (attackerCOMBAT_TYPE)
	{
	case CT_PSI:
		
		{
			double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId);
			
			// psi combat agains any armor
			
			combatStrength.values.at(CT_PSI).at(CT_PSI) = psiOffenseStrength;
			combatStrength.values.at(CT_PSI).at(CT_CON) = psiOffenseStrength;
			
		}
		
		break;
		
		
	case CT_CON:
		
		{
			double psiOffenseStrength = getVehiclePsiOffenseStrength(vehicleId);
			double conOffenseStrength = getVehicleConOffenseStrength(vehicleId);
			
			// psi combat agains psi armor
			// con combat agains con armor
			
			combatStrength.values.at(CT_CON).at(CT_PSI) = psiOffenseStrength;
			combatStrength.values.at(CT_CON).at(CT_CON) = conOffenseStrength;
			
		}
		
		break;
		
	}
	
	return combatStrength;
	
}

/**
Engine sometimes forgets to assign vehicle to transport.
This function ensures land units at sea are assigned to transport.
*/
void assignVehiclesToTransports()
{
	debug("assignVehiclesToTransports - %s\n", aiMFaction->noun_faction);
	
	std::vector<int> orphanPassengerIds;
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// player faction
		
		if (vehicle->faction_id != aiFactionId)
			continue;
		
		// land unit
		
		if (vehicle->triad() != TRIAD_LAND)
			continue;
		
		// at sea and not at base
		
		if (!(is_ocean(vehicleTile) && !map_has_item(vehicleTile, BIT_BASE_IN_TILE)))
			continue;
		
		// not assigned to transport
		
		if (getVehicleTransportId(vehicleId) != -1)
			continue;
		
		debug("\t[%4d] %s\n", vehicleId, getLocationString(vehicleTile));
		
		// find available sea transport at this location and assign vehicle to it
		
		int transportId = -1;
		
		for (int stackVechileId : getStackVehicleIds(vehicleId))
		{
			// sea transport
			
			if (!isSeaTransportVehicle(stackVechileId))
				continue;
			
			// has capacity
			
			if (!isTransportHasCapacity(stackVechileId))
				continue;
			
			// select available transport
			
			transportId = stackVechileId;
			break;
			
		}
		
		// assign vehicle to selected transport or kill vehicle if no available transport
		
		if (transportId != -1)
		{
			board(vehicleId, transportId);
			debug("\t\tassigned to transport: [%4d]\n", transportId);
		}
		else
		{
			orphanPassengerIds.push_back(vehicleId);
			debug("\t\tkilled\n");
		}
		
	}
	
	std::sort(orphanPassengerIds.begin(), orphanPassengerIds.end(), std::greater<int>());
	
	for (int orphanPassengerId : orphanPassengerIds)
	{
		killVehicle(orphanPassengerId);
	}
	
}

/*
Unit can attack enemy at the tile.
*/
bool isUnitCanMeleeAttack(int factionId, int unitId, MAP *position, MAP *target)
{
	UNIT *unit = getUnit(unitId);
	int triad = unit->triad();
	
	// non melee unit cannot melee attack
	
	if (!isMeleeUnit(unitId))
		return false;
	
	// check position if provided
	
	if (position != nullptr)
	{
		TileInfo &positionTileInfo = aiData.getTileInfo(position);
		
		switch (triad)
		{
		case TRIAD_SEA:
			
			// sea unit cannot attack from land without a base
			
			if (!positionTileInfo.ocean && !map_has_item(position, BIT_BASE_IN_TILE))
				return false;
			
			break;
			
		case TRIAD_LAND:
			
			if (isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
			{
				// amphibious unit can attack from land and sea
			}
			else
			{
				// other land units cannot attack from sea
				
				if (positionTileInfo.ocean)
					return false;
				
			}
			
			break;
			
		}
			
	}
	
	// check target if provided
	
	if (target != nullptr)
	{
		TileInfo &targetTileInfo = aiData.getTileInfo(target);
		
		// cannot attack needlejet in flight without air superiority
		
		if (targetTileInfo.unfriendlyNeedlejetInFlights.at(factionId) && !isUnitHasAbility(unitId, ABL_AIR_SUPERIORITY))
			return false;
		
		// check movement
		
		switch (triad)
		{
		case TRIAD_SEA:
			
			if (unitId == BSC_SEALURK)
			{
				// sealurk can attack sea and land
			}
			else
			{
				// other sea units cannot attack land
					
				if (!targetTileInfo.ocean)
					return false;
				
			}
			
			break;
			
		case TRIAD_LAND:
			
			if (isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
			{
				// amphibious unit cannot attack sea without a base
				
				if (targetTileInfo.ocean && !map_has_item(target, BIT_BASE_IN_TILE))
					return false;
				
			}
			else
			{
				// other land units cannot attack sea
				
				if (targetTileInfo.ocean)
					return false;
				
			}
			
			break;
			
		}
		
	}
	
	// all checks passed
	
	return true;
	
}

/*
Vehilce can attack enemy at the tile.
*/
bool isVehicleCanMeleeAttack(int vehicleId, MAP *position, MAP *target)
{
	VEH &vehicle = Vehs[vehicleId];
	return isUnitCanMeleeAttack(vehicle.faction_id, vehicle.unit_id, position, target);
}

/*
Unit can attack enemy at the tile.
*/
bool isUnitCanArtilleryAttack(int unitId, MAP *position)
{
	UNIT &unit = Units[unitId];
	int triad = unit.triad();
	
	// non artillery unit cannot artillery attack
	
	if (!isArtilleryUnit(unitId))
		return false;
	
	// additionally check position tile if provided
	
	if (position != nullptr)
	{
		TileInfo &positionTileInfo = aiData.getTileInfo(position);
		
		switch (triad)
		{
		case TRIAD_AIR:
			
			// air unit can attack from anywhere
			
			break;
			
		case TRIAD_SEA:
			
			// sea unit can attack from sea and a base
			
			if (!(positionTileInfo.ocean || positionTileInfo.base))
				return false;
			
			break;
			
		case TRIAD_LAND:
			
			if (isUnitHasAbility(unitId, ABL_AMPHIBIOUS))
			{
				// amphibious unit can attack from land and sea
			}
			else
			{
				// other land units cannot attack from sea
				
				if (positionTileInfo.ocean)
					return false;
				
			}
			
			break;
			
		}
			
	}
	
	// all checks passed
	
	return true;
	
}

/*
Checks if vehicle can use artillery.
*/
bool isVehicleCanArtilleryAttack(int vehicleId, MAP *position)
{
	return isUnitCanArtilleryAttack(Vehs[vehicleId].unit_id, position);
}

double getBaseExtraWorkerGain(int baseId)
{
	// basic base gain
	
	Resource oldBaseIntake2 = getBaseResourceIntake2(baseId);
	double oldBaseGain = getBaseGain(baseId, oldBaseIntake2);
	
	// extra worker base gain
	
	Resource newBaseIntake2 =
		Resource::combine
		(
			oldBaseIntake2,
			{
				aiData.averageWorkerNutrientIntake2,
				aiData.averageWorkerMineralIntake2,
				aiData.averageWorkerEnergyIntake2,
 			}
		)
	;
	double newBaseGain = getBaseGain(baseId, newBaseIntake2);
	
	// extra worker gain
	
	double extraWorkerGain = std::max(0.0, newBaseGain - oldBaseGain);
	return extraWorkerGain;
	
}

MapDoubleValue getMeleeAttackPosition(int unitId, MAP *origin, MAP *target)
{
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	
	// find closest attack position
	
	MapDoubleValue closestAttackPosition(nullptr, INF);
	
	for (AngleTileInfo const &positionAdjacentAngleTileInfo : targetTileInfo.adjacentAngleTileInfos)
	{
		TileInfo *positionTileInfo = positionAdjacentAngleTileInfo.tileInfo;
		MAP *positionTile = positionTileInfo->tile;
		
		// reachable
		
		if (!isUnitDestinationReachable(unitId, origin, positionTile))
			continue;
		
		// not blocked
		
		if (positionTileInfo->blocked)
			continue;
		
		// can melee attack
		
		if (!isUnitCanMeleeAttack(aiFactionId, unitId, positionTile, target))
			continue;
		
		// travelTime
		
		double travelTime = getUnitApproachTime(aiFactionId, unitId, origin, positionTile);
		if (travelTime == INF)
			continue;
		
		// update best
		
		if (travelTime < closestAttackPosition.value)
		{
			closestAttackPosition.tile = positionTile;
			closestAttackPosition.value = travelTime;
		}
		
	}
	
	return closestAttackPosition;
	
}

MapDoubleValue getMeleeAttackPosition(int vehicleId, MAP *target)
{
	return getMeleeAttackPosition(getVehicle(vehicleId)->unit_id, getVehicleMapTile(vehicleId), target);
}

MapDoubleValue getArtilleryAttackPosition(int unitId, MAP *origin, MAP *target)
{
	TileInfo &targetTileInfo = aiData.getTileInfo(target);
	
	// find closest attack position
	
	MapDoubleValue closestAttackPosition(nullptr, INF);
	
	for (TileInfo *positionTileInfo : targetTileInfo.range2NoCenterTileInfos)
	{
		MAP *positionTile = positionTileInfo->tile;
		
		// reachable
		
		if (!isUnitDestinationReachable(unitId, origin, positionTile))
			continue;
		
		// not blocked
		
		if (positionTileInfo->blocked)
			continue;
		
		// can artillery attack
		
		if (!isUnitCanArtilleryAttack(unitId, positionTile))
			continue;
		
		// travelTime
		
		double travelTime = getUnitApproachTime(aiFactionId, unitId, origin, positionTile);
		if (travelTime == INF)
			continue;
		
		// update best
		
		if (travelTime < closestAttackPosition.value)
		{
			closestAttackPosition.tile = positionTile;
			closestAttackPosition.value = travelTime;
		}
		
	}
	
	return closestAttackPosition;
	
}

MapDoubleValue getArtilleryAttackPosition(int vehicleId, MAP *target)
{
	return getArtilleryAttackPosition(getVehicle(vehicleId)->unit_id, getVehicleMapTile(vehicleId), target);
}

/*
Winning probability by combat effect.
*/
double getWinningProbability(double combatEffect)
{
	// div/0 is eliminated by condition
	return combatEffect <= 1.0 ? 0.5 * combatEffect * combatEffect : 1.0 - (0.5 * (1.0 / combatEffect) * (1.0 / combatEffect));
}

/*
Winning vehicle remaining heath ratio to initial one.
*/
double getWinningHealthRatio(double combatEffect)
{
	return 0.35 + 0.12* combatEffect;
}

double getSensorOffenseMultiplier(int factionId, MAP *tile)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	
	TileInfo &tileInfo = aiData.tileInfos.at(tile - *MapTiles);
	
	return
		Rules->combat_defend_sensor != 0 && tileInfo.sensors.at(factionId) && (conf.sensor_offense && (!is_ocean(tile) || conf.sensor_offense_ocean)) ?
			getPercentageBonusMultiplier(Rules->combat_defend_sensor)
			:
			1.0
	;
	
}

double getSensorDefenseMultiplier(int factionId, MAP *tile)
{
	assert(factionId >= 0 && factionId < MaxPlayerNum);
	
	TileInfo &tileInfo = aiData.tileInfos.at(tile - *MapTiles);
	
	return
		Rules->combat_defend_sensor != 0 && tileInfo.sensors.at(factionId) && ((!is_ocean(tile) || conf.sensor_offense_ocean)) ?
			getPercentageBonusMultiplier(Rules->combat_defend_sensor)
			:
			1.0
	;
	
}

/**
Computes melee attack strength multiplier taking all bonuses/penalties into account.
exactLocation: whether combat happens at this exact location (base)
*/
double getUnitMeleeOffenseStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	bool ocean = is_ocean(tile);
	bool fungus = map_has_item(tile, BIT_FUNGUS);
	bool landRough = !ocean && (map_has_item(tile, BIT_FUNGUS | BIT_FOREST) || map_rockiness(tile) == 2);
	bool landOpen = !ocean && !(map_has_item(tile, BIT_FUNGUS | BIT_FOREST) || map_rockiness(tile) == 2);
	
	double offenseStrengthMultiplier = 1.0;
	
	// sensor
	
	offenseStrengthMultiplier *= getSensorOffenseMultiplier(attackerFactionId, tile);
	offenseStrengthMultiplier /= getSensorDefenseMultiplier(defenderFactionId, tile);
	
	// other modifiers are difficult to guess at not exact location
	
	if (!exactLocation)
		return offenseStrengthMultiplier;
	
	// base and terrain
	
	int baseId = base_at(x, y);
	if (baseId != -1)
	{
		// base
		
		offenseStrengthMultiplier /= getBaseDefenseMultiplier(baseId, attackerUnitId, defenderUnitId);
		
		// infantry vs. base attack bonus
		
		if (isInfantryUnit(attackerUnitId))
		{
			offenseStrengthMultiplier *= getPercentageBonusMultiplier(Rules->combat_infantry_vs_base);
		}
		
	}
	else
	{
		// native -> regular (fungus) = 50% attack bonus
		
		if (isNativeUnit(attackerUnitId) && !isNativeUnit(defenderUnitId) && fungus)
		{
			offenseStrengthMultiplier *= getPercentageBonusMultiplier(50);
		}
		
		// conventional defense in land rough = 50% defense bonus
		
		if (!isPsiCombat(attackerUnitId, defenderUnitId) && landRough)
		{
			offenseStrengthMultiplier /= getPercentageBonusMultiplier(50);
		}
		
		// conventional defense against mobile in land rough bonus
		
		if (!isPsiCombat(attackerUnitId, defenderUnitId) && isMobileUnit(attackerUnitId) && landRough)
		{
			offenseStrengthMultiplier /= getPercentageBonusMultiplier(Rules->combat_mobile_def_in_rough);
		}
		
		// mobile attack in open bonus

		if (!isPsiCombat(attackerUnitId, defenderUnitId) && isMobileUnit(attackerUnitId) && landOpen)
		{
			offenseStrengthMultiplier *= getPercentageBonusMultiplier(Rules->combat_mobile_open_ground);
		}
		
	}
	
	return offenseStrengthMultiplier;
	
}

/**
Computes artillerry attack strength multiplier taking all bonuses/penalties into account.
exactLocation: whether combat happens at this exact location (base)
*/
double getUnitArtilleryOffenseStrengthMultipler(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, MAP *tile, bool exactLocation)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	
	double attackStrengthMultiplier = 1.0;
	
	// sensor
	
	attackStrengthMultiplier *= getSensorOffenseMultiplier(attackerFactionId, tile);
	attackStrengthMultiplier /= getSensorDefenseMultiplier(defenderFactionId, tile);
	
	// other modifiers are difficult to guess at not exact location
	
	if (!exactLocation)
		return attackStrengthMultiplier;
	
	if (isArtilleryUnit(defenderUnitId)) // artillery duel
	{
		// base and terrain
		
		int baseId = base_at(x, y);
		
		if (baseId != -1) // base
		{
			// artillery duel defender may get base bonus if configured
			
			if (conf.artillery_duel_uses_bonuses)
			{
				attackStrengthMultiplier /= getBaseDefenseMultiplier(baseId, attackerUnitId, defenderUnitId);
			}
			
		}
		else
		{
			// no other bonuses for artillery duel in open
		}
		
	}
	else // bombardment
	{
		// base and terrain
		
		int baseId = base_at(x, y);
		
		if (baseId != -1) // base
		{
			// base defense
			
			attackStrengthMultiplier /= getBaseDefenseMultiplier(baseId, attackerUnitId, defenderUnitId);
			
		}
		else if (isRoughTerrain(tile))
		{
			// bombardment defense in rough terrain bonus
			
			attackStrengthMultiplier /= getPercentageBonusMultiplier(50);
			
		}
		else
		{
			// bombardment defense in open get 50% bonus
			
			attackStrengthMultiplier /= getPercentageBonusMultiplier(50);
			
		}
		
	}
	
	return attackStrengthMultiplier;
	
}

/**
Determines number of drones could be quelled by police.
*/
int getBasePoliceRequiredPower(int baseId)
{
	BASE *base = getBase(baseId);
	
	// compute base without specialists

	Profiling::start("- getBasePoliceRequiredPower - computeBase");
	computeBase(baseId, true);
	Profiling::stop("- getBasePoliceRequiredPower - computeBase");
	
	// get drones currently quelled by police
	
	int quelledDrones = *CURRENT_BASE_DRONES_FACILITIES - *CURRENT_BASE_DRONES_POLICE;
	
	// get drones left after projects applied
	
	int leftDrones = *CURRENT_BASE_DRONES_PROJECTS;
	
	// calculate potential number of drones can be quelled with police
	
	int requiredPolicePower = quelledDrones + leftDrones - base->talent_total;
	
	// restore base
	
	aiData.resetBase(baseId);
	
	return requiredPolicePower;
	
}

void setTileBlockedAndZoc(TileInfo &tileInfo)
{
	// blocked
	
	tileInfo.blocked =
		// unfriendly base blocks
		tileInfo.unfriendlyBase
		||
		// unfriendly vehicle blocks
		tileInfo.unfriendlyVehicle
	;
	
	// zoc
	
	tileInfo.orgZoc =
		// not base
		!tileInfo.base
		&&
		// unfriendly vehicle zoc
		tileInfo.unfriendlyVehicleZoc
	;
	
	tileInfo.dstZoc =
		// not base
		!tileInfo.base
		&&
		// not friendly vehicle
		!tileInfo.friendlyVehicle
		&&
		// unfriendly vehicle zoc
		tileInfo.unfriendlyVehicleZoc
	;
	
}

/*
Estimates unit destruction gain.
*/
double getUnitDestructionGain(int unitId)
{
	UNIT &unit = Units[unitId];
	
	int mineralCost = Rules->mineral_cost_multi * unit.cost;
	double resourceScore = getResourceScore(0.0, (double) mineralCost, 0.0);
	double destructionGain = getGainBonus(resourceScore);
	
	if (isCombatUnit(unitId))
	{
		// no change
	}
	else
	{
		switch (unit.weapon_id)
		{
		case WPN_PROBE_TEAM:
			// increase the cost for additional threat
			destructionGain *= 2.0;
			break;
		case WPN_ALIEN_ARTIFACT:
			// faction loss + capturer gain
			destructionGain *= 2.0;
			break;
		}
		
	}
	
	return destructionGain;
	
}

/*
Returns the proportion of the given value from the lowest (0.0) to the highest (1.0) value.
*/
double getProportionalCoefficient(double minValue, double maxValue, double value)
{
	if (minValue == maxValue)
		return 0.5;
	
	return std::max(0.0, std::min(1.0, (value - minValue) / (maxValue - minValue)));
	
}

int generatePad0FromVehicleId(int vehicleId)
{
	// shift pad_0 by 2 * MaxVehModNum to make sure it is distinct from any vehicleId
	return 2 * conf.max_veh_num + vehicleId;
}

int getInitialVehicleIdByPad0(int pad0)
{
	return pad0 >= 2 * conf.max_veh_num ? pad0 - 2 * conf.max_veh_num : -1;
}

/*
Populates pad0 -> vehicle maps using currently assigned pad0s.
*/
void populateVehiclePad0Map(bool initialize)
{
	Profiling::start("- populateVehiclePad0Map");
	
	// assign pad0 values if requested
	
	if (initialize)
	{
		for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
		{
			Vehs[vehicleId].pad_0 = generatePad0FromVehicleId(vehicleId);
		}
		
	}
	
	// populate map
	
	aiData.pad0VehicleIds.clear();
	
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH &vehicle = Vehs[vehicleId];
		
		if (vehicle.pad_0 == 0)
			continue;
		
		aiData.pad0VehicleIds.emplace(vehicle.pad_0, vehicleId);
		aiData.savedVehicles.at(vehicleId) = vehicle;
		
	}
	
	Profiling::stop("- populateVehiclePad0Map");
	
}

/*
Repopulates vehicle locations and corresponding blocked and zoc after vehicle moved/created/destroyed.
*/
void updateVehicleTileBlockedAndZocs()
{
	Profiling::start("- updateVehicleTileBlockedAndZocs");
	
	// reset values
	
	for (TileInfo &tileInfo : aiData.tileInfos)
	{
		tileInfo.vehicleIds.clear();
		tileInfo.playerVehicleIds.clear();
		std::fill(tileInfo.factionNeedlejetInFlights.begin(), tileInfo.factionNeedlejetInFlights.end(), false);
		std::fill(tileInfo.unfriendlyNeedlejetInFlights.begin(), tileInfo.unfriendlyNeedlejetInFlights.end(), false);
		
		tileInfo.friendlyVehicle = false;
		tileInfo.unfriendlyVehicle = false;
		tileInfo.unfriendlyVehicleZoc = false;
		
		tileInfo.blocked = false;
		tileInfo.orgZoc = false;
		tileInfo.dstZoc = false;
		
	}
	
	// vehicles
		
	for (int vehicleId = 0; vehicleId < *VehCount; vehicleId++)
	{
		VEH &vehicle = Vehs[vehicleId];
		TileInfo &tileInfo = aiData.getVehicleTileInfo(vehicleId);
		
		// tile vehicles
		
		tileInfo.vehicleIds.push_back(vehicleId);
		
		// tile player vehicles
		
		if (Vehs[vehicleId].faction_id == aiFactionId)
		{
			tileInfo.playerVehicleIds.push_back(vehicleId);
		}
		
		// tile faction needlejet in flight
		
		if (!tileInfo.airbase && isNeedlejetVehicle(vehicleId))
		{
			tileInfo.factionNeedlejetInFlights.at(vehicle.faction_id) = true;
		}
		
		// friendly/unfriendly vehicles
		
		if (isFriendly(aiFactionId, vehicle.faction_id))
		{
			tileInfo.friendlyVehicle = true;
		}
		else
		{
			tileInfo.unfriendlyVehicle = true;
			
			// zoc is exerted only from land
			
			if (tileInfo.land)
			{
				for (AngleTileInfo const &adjacentAngleTileInfo : tileInfo.adjacentAngleTileInfos)
				{
					TileInfo *adjacentTileInfo = adjacentAngleTileInfo.tileInfo;
					
					// zoc is exerted only to land
					
					if (adjacentTileInfo->ocean)
						continue;
					
					// zoc is not exerted to base
					
					if (adjacentTileInfo->base)
						continue;
					
					// zoc
					
					adjacentTileInfo->unfriendlyVehicleZoc = true;
					
				}
				
			}
			
		}
		
	}
	
	// blocked and zoc
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		setTileBlockedAndZoc(tileInfo);
	}
	
	// needlejet in flight
	
	for (TileInfo &tileInfo : aiData.tileInfos)
	{
		for (int factionId = 0; factionId < MaxPlayerNum; factionId++)
		{
			for (int otherFactionId = 0; otherFactionId < MaxPlayerNum; otherFactionId++)
			{
				if (isUnfriendly(factionId, otherFactionId) && tileInfo.factionNeedlejetInFlights.at(otherFactionId))
				{
					tileInfo.unfriendlyNeedlejetInFlights.at(factionId) = true;
				}
			}
			
		}
		
	}
	
	Profiling::stop("- updateVehicleTileBlockedAndZocs");
	
}

/*
Evaluates combat gain for attacker.
Could be positive or negative.
*/
double getCombatGain(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode, MAP *attackerTile, MAP *defenderTile, double attackerHealth, double defenderHealth)
{
	int defenderUnitId = Vehs[defenderVehicleId].unit_id;
	Triad defenderTriad = static_cast<Triad>(Units[defenderUnitId].triad());
	
	// destruction gains
	
	double attackerDestructionGain = getVehicleDestructionGain(attackerVehicleId);
	double defenderDestructionGain = getVehicleDestructionGain(defenderVehicleId);
	
	// attacker is already destroyed
	
	if (attackerHealth <= 0.0)
	{
		// attacker get negative gain for own destruction
		return -attackerDestructionGain;
	}
	
	// defender is already destroyed
	
	if (defenderHealth <= 0.0)
	{
		// attacker get positive gain for enemy destruction
		return +defenderDestructionGain;
	}
	
	// combat mode
	
	CombatMode combatMode = getCombatMode(engagementMode, defenderUnitId);
	bool mutualCombat = combatMode != CM_BOMBARDMENT;
	
	// combatEffect
	
	double combatEffect = aiData.combatEffectTable.getVehicleCombatEffect(attackerVehicleId, defenderVehicleId, engagementMode, attackerTile, defenderTile);
	
	// compute gain
	
	double gain;
	
	if (mutualCombat)
	{
		// modify combatEffect with combattants health
		
		combatEffect =
			combatEffect
			* attackerHealth
			/ defenderHealth
		;
		
		// winning probability
		
		double winningProbability = getWinningProbability(combatEffect);
		
		// losses
		
		double attackerLoss = 1.0 - winningProbability;
		double defenderLoss = winningProbability;
		
		// gain
		
		gain =
			- attackerLoss * attackerDestructionGain
			+ defenderLoss * defenderDestructionGain
		;
		
	}
	else
	{
		// defender loss
		
		double defenderLoss = combatEffect;
		
		// gain
		
		gain =
			+ defenderLoss * defenderDestructionGain
		;
		
		// non lethal bombardment reduces loss
		
		if (defenderTile != nullptr)
		{
			if (!isLethalBombardment(defenderTriad, defenderTile))
			{
				gain *= 0.5;
			}
			
		}
		
	}
	
	return gain;

}

char const *getVehiclePad0LocationNameString(int vehicleId)
{
	static int constexpr BUFFER_COUNT = 10;
	static int constexpr BUFFER_SIZE = 100;
	
    static char buffers[BUFFER_COUNT][BUFFER_SIZE];
    static int index = 0;
	
	assert(vehicleId >= 0 && vehicleId < *VehCount);
	
	VEH &vehicle = Vehs[vehicleId];
	
    int i = index;
    index = (index + 1) % BUFFER_COUNT;
    
    snprintf
    (
		buffers[i], sizeof(buffers[i]),
		"{%4d} (%3d,%3d) %-32s"
		, getInitialVehicleIdByPad0(vehicle.pad_0)
		, vehicle.x, vehicle.y
		, vehicle.name()
	);
    
    return buffers[i];
    
}

/*
Approximates CND probability by shift from mean in standard deviations.
*/
double getCNDProbability(double value)
{
	double probability;
	
	if (value >= 0.0)
	{
		double baseValue = (- value / CND_APPROXIMATION_SCALE + 1 );
		probability = 1.0 - 0.5 * baseValue * baseValue;
	}
	else
	{
		double baseValue = (+ value / CND_APPROXIMATION_SCALE + 1 );
		probability = 0.0 + 0.5 * baseValue * baseValue;
	}
	
	return probability;
	
}

/*
A helper function to estimate win probability by given destroyed weights.
*/
double getWinProbability(double destroyedWeight1, double destroyedWeight2, double remainingWeight1)
{
	assert(destroyedWeight1 >= 0.0 && destroyedWeight2 >= 0.0 && remainingWeight1 >= 0.0);
	
	if (destroyedWeight1 == 0)
	{
		return 1.0;
	}
	else if (destroyedWeight2 == 0.0)
	{
		return 0.0;
	}
	
	double p = destroyedWeight2 / (destroyedWeight1 + destroyedWeight2);
	double q = 1.0 - p;
	double tryVariance = p * q;
	double tryCount = 10.0 * (destroyedWeight1 + destroyedWeight2);
	double totalVariance = tryVariance * tryCount;
	double totalSTD = sqrt(totalVariance);
	double shiftSTD = remainingWeight1 / totalSTD;
	return getCNDProbability(shiftSTD);
	
}

/*
Combat gain to attacker.
*/
double getMutualCombatGain(double attackerDestructionGain, double defenderDestructionGain, double combatEffect)
{
debug(">attackerDestructionGain=%5.2f\n", attackerDestructionGain);
debug(">defenderDestructionGain=%5.2f\n", defenderDestructionGain);
	double winningProbability = getWinningProbability(combatEffect);
debug(">winningProbability=%5.2f\n", winningProbability);
	
	double attackerLoss = (1.0 - winningProbability);
	double defenderLoss = winningProbability;
	
	double attackerLossGain = attackerLoss * attackerDestructionGain;
	double defenderLossGain = defenderLoss * defenderDestructionGain;
	
	double gain =
		-attackerLossGain
		+defenderLossGain
	;
	
	return gain;
	
}

/*
Combat gain to attacker.
*/
double getBombardmentGain(double defenderDestructionGain, double relativeBombardmentDamage)
{
	double defenderLoss = relativeBombardmentDamage;
	double defenderLossGain = defenderLoss * defenderDestructionGain;
	
	double gain =
		+defenderLossGain
	;
	
	return gain;
	
}

double getAssignedTaskProtectDefendGain(int vehicleId)
{
	Task *task = getTask(vehicleId);
	
	if (task == nullptr)
		return 0.0;
	
	if (task->attackTarget != nullptr)
		return 0.0;
	
	TileInfo &tileInfo = aiData.getTileInfo(task->destination);
	
	CombatData *combatData;
	if (tileInfo.base)
	{
		combatData = &(aiData.getBaseInfo(tileInfo.baseId).combatData);
	}
	else if (tileInfo.bunker)
	{
		combatData = &(aiData.getBunkerInfo(task->destination).combatData);
	}
	else
	{
		return 0.0;
	}
	
	double protectGain = aiData.factionInfos.at(aiFactionId).averageBaseGain * combatData->getProtectorContribution(vehicleId);
	return protectGain;
	
}

void createFakeVehicle(int vehicleId, int factionId, int unitId)
{
	assert(vehicleId >= conf.max_veh_num && vehicleId < conf.max_veh_num + 2);
	veh_clear(vehicleId, unitId, factionId);
}

