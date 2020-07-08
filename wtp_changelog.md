# Version 80

* Reactor value is weapon/armor percentage multiplier.
* Now actually renaming technologies. Didn't work first time.

# Version 79

* Reverted Base Square to 2, 1, 1. With 4 nutrients the growth is too fast and out of balance.
* Super Former cost = 4 mineral rows.
* Thermal Borehole construction time = 18.

# Version 78

* New technlogy tree version. Hopefully more lore sensible.
* Frictionless Surfaces renamed to Quantum Computers.
* Super Tensile Solids renamed to Applied Plasmadynamics (for plasma shard).
* Organic Superlubricant renamed to Cold Fusion (for fusion lab and fusion laser).
* Aquifer is enabled by Field Modulation (level 2).
* Thermal Borehole is enabled by Superconductor (level 3).
* Condenser is enabled by Ecological Engineering (level 4).
* Echelon Mirror is enabled by Environmental Economics (level 5).
* Hab Complex is enabled by Industrial Economics (level 3).
* Other technology wiring and association see in alpax.txt.

* Removed -1 POLICE shift from all factions. Now this mod does not require tweaking with factions anymore.
* SEs modified to give more negative POLICE ratings.
* SEs modified to give higher GROWTH ratings. This is now safe as population boom is fixed and doesn't break game anymore.
* Borehole Square yield is 0-2-6. Borehole construction time is 12.
* Forest yield is reverted to 1-2-1. Instead moved HF later in tree and restored its original cost/maint.
* Tree Farm cost/maint = 12/2.
* Hybrid Forest cost/maint = 24/4.
* Nutrient effect in mine square = 0.
* Centauri Genetics gives +1 mineral in fungus. By request of players to have some minerals in it.
* Biology Lab cost = 6.
* Fungicide Tanks cost = 1 row of minerals.
* Weapons and armors are shifted little bit later and distributed more evenly across the tree.
* Armor value progression adjusted to keep up with contemporary weapon.
* The Weather Paradigm cost = 40. Lowered due to plenty of advanced terraforming options are now available early.

# Version 77

* Sea Formers is given by Ecology.
* Terraforming Unit cost = 2.
* Spore Launcher is enabled by Field Modulation.
* Fungicide tanks cost = flat 1 mineral row.
* Super Former cost = 2.
* Unit cost is rounded before ability cost factor is applied so same ability added to the same cost units result in same total cost.
* Needlejet chassis cost = 4.
* Gravship chassis cost = 6.
* Decifer ability cost to human readable text in Datalink.

# Version 76

* The Universal Translator cost = 20. Reduced due to early availability and pathetic benfits.
* The Planetary Datalinks cost = 80. It is too beneficial for 60.
* The Cyborg Factory cost = 160. It should be more expensive than CBA.
* The Clinical Immortality cost = 200. Cost increased due to doubling planetary votes. That is quite a big aid for diplomatic victory.
* The Space Elevator cost = 160. Average cost within era.
* The Cloning Vats = 120. Increased cost a little to match the era.
* The Ascent to Transcendence cost = 500. Increased slightly since now all empire bases are contributing to it.

# Version 75

* The Hunter-Seeker Algorithm prevents regular probe action but don't destroy it.
* Human probes cannot sabotage defensive facilities either by targetting them or by random sabotage.

# Version 74

* Base Square produces 4 nutrients;
* Reactor does not discount module cost.
* Foil chassis cost = 2 (same as infantry).
* Cruiser chassis cost = 3 (same as speeder).
* Colony Module cost = 4.
* The Cloning Vats does not grant impunities.
* The Cloning Vats help entry is updated.

# Version 73

* [fix] AI hurrying project sometimes resulted in negative accumulated minerals.

# Version 72

* Satellites appearance order is changed to: Orbital power transmitter, Nessus mining station, Sky hydroponics lab.
* The Cloning Vats cost: 100.

# Version 71

* Population boom fixed. See Readme for details.
* The Cloning Vats fixed. See Readme for details.

# Version 70

* Both human and AI get extra colony and former as controlled by free_formers, free_colony_pods in thinker.ini.
* techtree version changes included.
* The Planetary Transit System fixed. See Readme for details.

# fastgame

Special testing version.

* Both human and AI get extra colony and former as controlled by free_formers, free_colony_pods in thinker.ini.
* Colony Module cost = 3.
* Terraforming Unit cost = 2.
* Nutrient requirement for base growth doesn't increas until size 4. Then they increase by 1 as usual.

# techtree

* Large technology tree redo for more sensible dependencies.
* Research Hospital: 8/2. Decreased cost/maint a little bit more to aid with drone quellying.
* Aquafarm: 16/4. Also moved later as it is a most OP out of aqua facilities.
* The Citizens' Defense Force cost is 60.

# Version 69.1

* [fix] Hurry production algorithm tried to hurry production in huma bases as well. Now it does not affect human.

# Version 69

* Bases contribute about 50% of their production to secret project construction. Contribution proportion is configurable.
* Every turn the popup shows project completion percentage for each project that was using bases contribution. Neat!

# Version 68

* AI factions get 2 formers at start. This should help out those struggling to built them.

# Version 67

* New AI terraforming algorithm. See readme for notable changes. Off by default. Set ai_useWTPAlgorithms=1 to enable it.

# Version 66

* Borehole Square,  0, 2, 8, 0. Construction time is down to 18. Compensation for reduced mineral output.
* Hab Complex and Habitation Dome maintenance is removed completely. I don't see much sense it paying for essencial facilities everybody has.

# Version 65

* Merged with Thinker 2.0.

# Version 64

* Borehole Square,  0, 2, 6, 0.
* Retroviral Engineering is level 5.
* Advanced Military Algorithms is level 6.
* Industrial Nanorobotics is level 7.
* Planetary Economics is level 8.
* Power SE model is reassigned to Superstring Theory (L5).

# Version 63

* VoP doubles hurry cost. This is now correctly calculated in AI hurry code.

# Version 62

* Changed default value of alternative_combat_mechanics_loss_divider=2.0.

# Version 61

* Fixed adavanced movement cost again. This time everything that is treated as road movement is corrected to have road movement cost.

# Version 60

* AAA Tracking cost is 2.

# Version 59

* Fixed advanced movement on river. River direction is always straight across tile edge - never diagonally.
* Fixed advanced movement when leaving or entering a base. Previously base didn't count as having road.

# Version 58

* Fixed advanced movement on ocean squares. Somehow ocean has river flag on it. So I have to restrict river movement for land only.

# Version 57

* Citizen change.

| name | technology | level | economy | psych | labs | change |
|----|----|----:|----:|----:|----:|----|
| Doctor | | | | +2 | | |
| Empath | Centauri Meditation | 6 | +2 | +2 | |
| Transcend | Sentient Resonance | 12 | +2 | +2 | +2 | Moved to L12 |
| Technician | | | +3 | | | |
| Engineer | Planetary Economics | 7 | +3 | +2 | | Moved to L7 |
| Librarian | Planetary Networks | 4 | | | +3 | |
| Thinker | Advanced Subatomic Theory | 8 | | +1 | +4 | Moved to L 8 |

# Version 56

* Tree Farm cost/maint is 10/2
* Hybrid Forest cost/maint is 20/3
* Green has -1 INDUSTRY.
* Power has -1 INDUSTRY.
* AI land formers don't stop in transit.
* AI formers build sensor with 5% probability to not waste time on them.
* Industrial Economics prerequisite is Industrial Base (thanks to Hagen0).
* Field Modulation prerequisite is Applied Physics (thanks to Hagen0).
* Fixed my previous bug when river movement rate was same as tube.

# Version 55

* Tube movement rate is a multiplier of road movement rate.

# Version 54

* Set borehole to 0-6-4.
* Fixed Condenser and Enricher calculation and display.

# Version 53

* Removed dialog option for self-destruct.
* Accumulated nutrients and minerals are adjusted after GROWTH and INDUSTRY SE change to maintain the same completion percentage.

# Version 52

* Aerospace Complex cost/maint is 10/3.
* SP cost adjusted. See Readme.

# Version 51

* Default unit morale is Very Green.
* Forest terraforming time is 8 turns.
* Hologram Theatre cost/maint is 6/2.
* AI rushes SP when they can.

# Version 50

* Command Center cost/maint is 6/1. It is updated automatically in game to match reactor level.

# Version 49

* Condenser does not multiply nutrient yield.
* Soil Enricher does not multiply nutrient yield and instead adds 1.
* Borehole yield is 0-4-4.

# Version 48

* Rearranged fungus yield. See table in readme.

# Version 47

* Brood Pit cost/maint is 12/3.
* Brood Pit is assigned to Secrets of Alpha Centauri.

# Version 46

* Brood Pit cost/maint is 12/4.

# Version 45

* Command Center cost/maint is 6/2.
* The Command Nexus cost is 40.
* The Citizens' Defense Force cost is 40.
* The Maritime Control Center cost is 60.

# Version 44

* Reactor doesn't change Supply cost.
* Added "+" to SM with +TALENT. This SE otherwise is not visible in social engineering screen.
* Changed AI cost factors for mineral/nutrient production to 12,11,10,9,8,7. Lower level AI are pathetic. This way they are a little bit more useful.
* The Weather Paradigm moved to level 2. It should appear early enough to be beneficial.
* Reactor technologies moved up one level: 4, 7, 10. Due to their value they seem to be discovered first in their level.
* Raise/Lower Land and Sea Floor is assigned to Ecological Engineering. It fits this bucket more.
* Doctrine: Initiative is level 4.
* Advanced Military Algorithms is level 5.
* Applied Relativity is level 6.
* Intellectual Integrity is level 6.
* Cosmetics. Shortened some labels in F4 screen to fit longer base names and avoid line wrapping.
* Environmental Economics is level 5.
* Postponed each mineral facilities after Genejack Factory by 1 level.
* The Planetary Energy Grid is enabled by Adaptive Economics.
* The Planetary Energy Grid cost is 600.

# Version 43

* Renamed "Homeland" effect to "Territory" to avoid confusion. It applies on sea as well.
* Reactor technologies levels: 3, 6, 9.
* Society Models: Politics is level 2, Economics is level 3, Values is level 5, Future Society is level 7.
* Some technology tree rearrangement to adapt Reactor and Society Model technologies.
* Linked green technologies together through level.
* Chaned tech cost final slope from 120 to 80.
* Chaned tech cost cubic coefficient slope from 120 to 80.
* Modified technology weights a little. Made all weapon technologies non zero in all areas.

# Version 42

* Heavy Artillery cost = 0.
* Air Superiority cost = 0.
* Tachyon Field bonus = 50%.
* Sensor Array defense bonus = 0.
* Homeland combat bonus = 50%.

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

* Revert forest yield, delay HF and make it more expensive.
* Brood Pit give just +1 POLICE.
* Make AI build more units, defensive structures, sensors.
* Rework probe team success rates.
* Cloning Vats should not grant impunities.
* Copy basic units to player unit pool.
* Don't display basic units in design workshop.

* Prototype only new component, not the whole unit.

## AI

* Control transports better.

