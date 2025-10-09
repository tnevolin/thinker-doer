#pragma once

#include "wtp_game.h"

/*
unit-to-unit combat effect data.
combat mode and effect
*/
struct CombatModeEffect
{
	COMBAT_MODE combatMode;
	double value;
};

/*
all unit-to-unit combat effects
*/
struct CombatEffectTable
{
	// each to each unit combat effects
	// [attackerFactionId][attackerUnitId][defenderFactionId][defenderUnitId][engagementMode] = combatEffect
	robin_hood::unordered_flat_map<int, CombatModeEffect> combatModeEffects;
	
	void clear();
	void setCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, COMBAT_MODE combatMode, double value);
	CombatModeEffect const &getCombatModeEffect(int attackerKey, int defenderKey, ENGAGEMENT_MODE engagementMode);
	CombatModeEffect const &getCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode);
	double getCombatEffect(int attackerKey, int defenderKey, ENGAGEMENT_MODE engagementMode);
	double getCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode);
	double getVehicleCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode);
	
};

struct TileCombatModifiers
{
	std::array<double, MaxPlayerNum> sensorOffenseMultipliers;
	std::array<double, MaxPlayerNum> sensorDefenseMultipliers;
	std::array<double, ATTACK_TRIAD_COUNT> terrainDefenseMultipliers;
};

/*
all unit-to-unit combat effects with terrain modifiers
*/
struct TileCombatEffectTable
{
	// each to each unit combat effects
	// [attackerFactionId][attackerUnitId][defenderFactionId][defenderUnitId][engagementMode] = combatEffect
	CombatEffectTable *combatEffectTable = nullptr;
	// TileCombatModifiers
	TileCombatModifiers tileCombatModifiers;
	// who is assailant
	int playerFactionId;
	bool playerAssault;
	// each to each unit combat effects for this tile
	// [attackerFactionId][attackerUnitId][defenderFactionId][defenderUnitId][engagementMode] = combatEffect
	robin_hood::unordered_flat_map<int, CombatModeEffect> combatModeEffects;
	
	void clear();
	void setCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, COMBAT_MODE combatMode, double value);
	CombatModeEffect const &getCombatModeEffect(int attackerKey, int defenderKey, ENGAGEMENT_MODE engagementMode);
	CombatModeEffect const &getCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode);
	double getCombatEffect(int attackerKey, int defenderKey, ENGAGEMENT_MODE engagementMode);
	double getCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode);
	double getVehicleCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode);
	
};

struct CombatData
{
	// combat effects
	TileCombatEffectTable tileCombatEffectTable;
	
	// participants
	
	robin_hood::unordered_flat_map<int, double> assailantEffects;
	robin_hood::unordered_flat_map<int, double> protectorEffects;
	
//	void initialize(robin_hood::unordered_flat_map<int, std::vector<int>> const &assailantFactionUnitIds, robin_hood::unordered_flat_map<int, std::vector<int>> const &protectorFactionUnitIds, MAP *tile);
		
	// assailantWeights [FactionUnitKey]
	robin_hood::unordered_flat_map<int, double> assailants;
	double assailantWeight = 0.0;
	// protectorWeights [FactionUnitKey]
	robin_hood::unordered_flat_map<int, double> protectors;
	double protectorWeight = 0.0;
	
	// assailantWeights [FactionUnitKey]
	robin_hood::unordered_flat_map<int, double> remainingAssailants;
	double remainingAssailantWeight;
	// protectorWeights [FactionUnitKey]
	robin_hood::unordered_flat_map<int, double> remainingProtectors;
	double remainingProtectorWeight;
	
	bool computed = false;
	bool sufficientAssault;
	bool sufficientProtect;
	
	void clearAssailants();
	void clearProtectors();
	
	void addAssailantUnit(int factionId, int unitId, double weight = 1.0);
	void addProtectorUnit(int factionId, int unitId, double weight = 1.0);
	void addAssailant(int vehicleId, double weight = 1.0);
	void addProtector(int vehicleId, double weight = 1.0);
	void removeAssailantUnit(int factionId, int unitId, double weight = 1.0);
	void removeProtectorUnit(int factionId, int unitId, double weight = 1.0);
	
	double getRemainingAssailantWeight();
	double getRemainingProtectorWeight();
	
	double getAssailantUnitEffect(int factionId, int unitId);
	double getProtectorUnitEffect(int factionId, int unitId);
	double getProtectorVehicleEffect(int vehicleId);
	double getAssailantVehicleEffect(int vehicleId);
	
	bool isSufficientAssault();
	bool isSufficientProtect();
	
	void compute();
	
};

void resolveMutualCombat(double combatEffect, double &attackerHealth, double &defenderHealth);
FactionUnitCombatEffect getBestAttackerDefenderMeleeEffect(TileCombatEffectTable &tileCombatEffectTable, robin_hood::unordered_flat_map<int, double> &attackers, robin_hood::unordered_flat_map<int, double> &defenders);

CombatModeEffect getUnitCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode);
CombatModeEffect getTileCombatModeEffect(CombatEffectTable *combatEffectTable, int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, TileCombatModifiers &tileCombatModifiers, bool atTile);
//CombatModeEffect getTileCombatModeEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode, MAP *tile);
double getMeleeRelativeUnitStrength(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId);
double getArtilleryDuelRelativeUnitStrength(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId);
double getUnitBombardmentDamage(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId);

