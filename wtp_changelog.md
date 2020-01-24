# Version 42

* Heavy Artillery cost = 0.
* Air Superiority cost = 0.
* Tachyon Field bonus = 50%.
* Homeland defense bonus = 25%.

# Version 41

* Removed morale bonus from home base Children Creche.
* Removed all morale bonuses from current base Children Creche.
* Increased cost/maintenance for Command Center: 8/2.
* Increased cost/maintenance for Naval Yard: 8/2.
* Increased cost/maintenance for Aerospace Complex: 12/3.
* Increased cost/maintenance for Bioenhancement Center: 20/5.
* Increased cost for The Command Nexus: 80.
* Increased cost for The Maritime Control Center: 80.
* Increased cost for The Cyborg Factory: 200.

# Version 40

* Alternative artillery damage.

# Version 39

* High Morale ability reinstated.
* 50,      ; Combat % -> Fanatic attack bonus
* Returned artillery damage back to vanilla setting: 3,2. Previous 5,1 was a temporary setting.

# Version 38

* Decreased Police State PLANET penalty to -1. PLANET seems to be more powerful now = too much of a penalty.
* Hypnotic Trance cost = flat 1, Empath Song cost = flat 2. With new combat higher values seem to be economically not effective.

# Version 37

* Nautilus Pirates: SOCIAL, -POLICE
* Removed Planetpearl energy reserve bonus.
* Unified promotion probability.
* Removed Very Green morale level defense bonus and its (+) display.
* 15,      ; Combat % -> Psi attack bonus/penalty per +PLANET
* 1,1,     ; Psi combat offense-to-defense ratio (LAND unit defending)
* PLANET combat bonus can now be applied on defense as well.
* Adjusted summary strength and power lines to the bottom in battle computation display. This way it is easier to read and compare them between attacker and defender.
* Territory extends from sea base by same distance as from land bases!
* Territory extends from coastal base into the adjacent sea by same distance as from sea bases!
* Sea formers can build Sensor Buoy now (similar to Sensor Array on land).

# Version 36

* Gaia's Stepdaughters: TECH, Ecology; SOCIAL, --MORALE; SOCIAL, -POLICE
* The Lord's Believers: TECH, Psych; , SOCIAL, --RESEARCH, SOCIAL, +PROBE, SOCIAL, +SUPPORT, SOCIAL, -PLANET, FANATIC, 0
* The Data Angels: TECH, Mobile; SOCIAL, -POLICE; SOCIAL, -RESEARCH
* Human Hive: TECH, Psych
* Peacekeeping Forces: TECH, Physic
* University of Planet: TECH, DocFlex
* Police State -EFFIC, +SUPPORT, ++POLICE, --PLANET
* Removed extra info in SM help.

# Version 35

* Updated unit cost help.

# Version 34

* Removed industry penalty from Cult of Planet. They are often develop very badly.
* Set Numerator & Denominator for artillery fire damage to 5,1.
* Set alternative_combat_mechanics_loss_divider=3.0.
* Set collateral_damage_value=0.
* Set missile chassis cost = 2.

# Version 33

* Odds confirmation dialog now displays correct battle winning probability percentage.

# Version 32

* Introduced alternative combat mechanics. Thanks to dino for idea.
* Removed firepower multiplier.
* Removed firepower randomization.

# Version 31

* Modified repair rates.

# Version 30

* Exposed repair rates. Didn't modify them.

# Version 29

* Fixed combat rolls.
* Added firepower multiplier.
* Added firepower randomization.
* Odds dialog now display true winning odds.

# Version 28

* Set Resonance Laser strength to 3.

# Version 27

* Unit cost is now rounded normally.

# Version 26

* Fixed combat ignore reactor for attacker.

# Version 25

* Colony foil/cruiser costs same as infantry/speeder.
* Former foil/cruiser costs same as infantry/speeder.
* Supply foil/cruiser costs same as infantry/speeder.
* Supply Transport module now costs 12.
* Removed Infantry Probe Team.
* Set Super Former ability cost to 3.
* Set Fungicide Tanks ability cost to 1.
* Set Clean Reactor ability cost to 32.
* Set Positive base upgrade cost to <mineral cost difference> x 4.
* Set Negative base upgrade cost to <mineral cost difference> x 2.
* Set Positive upgrade cost halved by Nano Factory, as before.

# Version 24

* Reverted all factions back to one initial technology for everybody.
* Rearranged initial technologies to give Psych to -2 POLICE and Ecology to research impeded factions.
* Removed +1 mineral AQUATIC faction bonus.
* Removed Pirates -3 SUPPORT penalty that was there earlier to neutralize their mineral bonus.
* Gave Pirates +1 POLICE. Now they start with 0 POLICE to help their accelerated growth in mineral deficit.
* Removed all hurry thresholds.
* Collateral damage computation is fixed to take exact percentage of the unit HPs. Vanilla computation for conventional combat multiply this by ATTACKER reactor power instead.
* Modified collateral damage to take 20% instead 30% in vanilla.

# Version 23

* Moved reactor cost factors configuration to thinker.ini.
* Added alternative_base_defensive_structure_bonuses switch to thinker.ini.
* Adjusted reactor appearence in technology tree to be closer to 25%, 50%, 75% marks.
* Set Fanatic attack bonus = 25.
* Set Mobile unit in open ground = 25.
* Set Infantry vs. Base = 25.
* Set Forest Square,    1, 2, 0, 0,

# Version 22

* Exposed Perimeter Defense and Tachyon Field bonuses in thinker.ini.
* Set Alternative unit hurry cost formula.
* Set Alternative upgrade cost formula.
* Weapon icon selection algorithm fix.

# Version 21

* Conventional combat now ignores reactor power same way as psi combat does.
* Odds calculation now ignore reactor power for both conventional and psi combat. For psi combat it was actually a bug that nobody ever fixed yet.
* Each higher reactors now decrease unit cost approximately by 20% comparing to predecessor. Chaged weapon and armor cost so it increases at about 50-60% rate of the item value. This way unit cost still growth but slower than strength.
* Returned all prototype cost back to vanilla values.
* Returned back to original cost/maintenance: Skunkworks.
* Added TECH, Mobile to MORGAN.
* Added TECH, PrPsych to UNIV.
* Added TECH, Physic to SPARTANS.
* Added TECH, Psych to GAIANS.
* Added TECH, Ecology to BELIEVE.
* Added TECH, DocFlex to HIVE.
* Added TECH, Indust to PEACE.
* Added TECH, Ecology to CYBORG.
* Added TECH, PrPsych to DRONE.
* Added TECH, Physic to PIRATES.
* Added TECH, DocFlex to FUNGBOY.
* Added TECH, Psych to ANGELS.
* Added TECH, Indust to CARETAKE.
* Added TECH, Mobile to USURPER.

# Version 20

Initial release. See list of changes in readme.

# TODO

* Make magtube movement cost 1/6.
* Make AI build more units, defensive structures, sensors.
* Artillery duel uses armor as well.
* Interceptor fight uses armor as well.
* Make ECM to affect sea units as well.
* Rework probe team success rates.

