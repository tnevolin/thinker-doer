#pragma once

#include "wtp_game.h"

/*
unit-to-unit combat effect data.
combat mode and effect
*/
struct CombatModeEffect
{
	bool valid = false;
	COMBAT_MODE combatMode;
	double value;
};

/*
all unit-to-unit combat effect data
*/
struct CombatEffectData
{
	// each to each unit combat effects
	// [attackerFactionId][attackerUnitId][defenderFactionId][defenderUnitId][combatMode] = combatEffect
	robin_hood::unordered_flat_map<int, std::array<CombatModeEffect, ENGAGEMENT_MODE_COUNT>> combatModeEffects;
	
	void clear();
	void setCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode, COMBAT_MODE combatMode, double value);
	CombatModeEffect const *getCombatModeEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode);
	double getCombatEffect(int attackerFactionUnitKey, int defenderFactionUnitKey, ENGAGEMENT_MODE engagementMode);
	double getCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, ENGAGEMENT_MODE engagementMode);
	double getCombatEffect(int attackerFactionId, int attackerUnitId, int defenderFactionId, int defenderUnitId, COMBAT_MODE combatMode);
	double getVehicleCombatEffect(int attackerVehicleId, int defenderVehicleId, ENGAGEMENT_MODE engagementMode);
	
};

struct CombatData
{
	CombatEffectData combatEffectData;
	robin_hood::unordered_flat_map<int, double> assailantMaxEffects;
	robin_hood::unordered_flat_map<int, double> protectorMaxEffects;
	
	bool initialProtectorWeightComputed = true;
	bool initialAssailantWeightComputed = true;
	double initialProtectorWeight = 0.0;
	double initialAssailantWeight = 0.0;
	
	bool computed = true;
	double remainingProtectorWeight = 0.0;
	double remainingAssailantWeight = 0.0;
	bool sufficientProtection = true;
	bool sufficientAssault = false;
	
	// assailantWeights [FactionUnitKey]
	robin_hood::unordered_flat_map<int, double> assailants;
	// protectorWeights [FactionUnitKey]
	robin_hood::unordered_flat_map<int, double> protectors;
	
	void initialize(robin_hood::unordered_flat_map<int, std::vector<int>> const &assailantFactionUnitIds, robin_hood::unordered_flat_map<int, std::vector<int>> const &protectorFactionUnitIds, MAP *tile);
	
	void clearAssailants();
	void clearProtectors();
	
	void addAssailantUnit(int factionId, int unitId, double weight = 1.0);
	void addProtectorUnit(int factionId, int unitId, double weight = 1.0);
	void addAssailant(int vehicleId, double weight = 1.0);
	void addProtector(int vehicleId, double weight = 1.0);
	void removeAssailantUnit(int factionId, int unitId, double weight = 1.0);
	void removeProtectorUnit(int factionId, int unitId, double weight = 1.0);
	
	double getInitialAssailantWeight();
	double getInitialProtectorWeight();
	
	double getRemainingAssailantWeight();
	double getRemainingProtectorWeight();
	
	double getAssailantPrevalence();
	double getProtectorPrevalence();
	
	double getAssailantUnitMaxEffect(int factionId, int unitId);
	double getProtectorUnitMaxEffect(int factionId, int unitId);
	double getProtectorVehicleMaxEffect(int vehicleId);
	double getAssailantVehicleMaxEffect(int vehicleId);
	
	double getAssailantUnitCurrentEffect(int factionId, int unitId);
	double getProtectorUnitCurrentEffect(int factionId, int unitId);
	double getProtectorVehicleCurrentEffect(int vehicleId);
	double getAssailantVehicleCurrentEffect(int vehicleId);
	
	bool isSufficientProtection();
	bool isSufficientAssault();
	
	void compute();
	
};

void resolveMutualCombat(double combatEffect, double *attackerHealth, double *defenderHealth);
FactionUnitCombat getBestMeleeAttackerDefender(CombatEffectData &combatEffectData, robin_hood::unordered_flat_map<int, double> &attackers, robin_hood::unordered_flat_map<int, double> &defenders);

