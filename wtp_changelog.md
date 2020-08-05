# Version 111

* [fix] Late starting factions (aliens and Planet Cult) never got their extra colony. Now they do. Fixed incorrect calculation on game restart.

# Version 110

* Pressure Dome,                 8, 2.\
Same maintenance as for Recyclyng Tanks since it delivers same benefits.\
Should not be too dificult for sea bases to pay it.

# Version 109

* [fix] Late starting factions (aliens and Planet Cult) never got their extra colony. Now they do.

# Version 108

* Alternative inefficiency formula.\
https://github.com/tnevolin/thinker-doer#inefficiency

# Version 107

* Fungicide Tanks costs 0.\
I don't think they are so important to cost anything. The ability slot is big enough cost for it.
* Super Former costs 2.\
Again, not that superimportant ability.

# Version 106

* [fix] Zero division in Recycling Tanks mineral calculation when mineral intake is zero.

# Version 105

* [bug] Base screen population incorrectly shows superdrones those are not on psych screen. Fixed for alien factions as well.

# Version 104

* New basic units.
	* Gun Foil,               Foil,     Gun,          Scout,      3, 0, 0, DocFlex, -1, 00000000000000000000000000
	* Scout Rover,            Speeder,  Gun,          Scout,      3, 0, 0, Mobile,  -1, 00000000000000000000000000
	* Empath Rover,           Speeder,  Gun,          Scout,      3, 0, 0, CentEmp, -1, 00000000000000100000000000
* Empath Song costs 1 mineral row.

# Version 103

* Further modification to native life formulas.
	* Native life generation frequency is configurable.
	* Sea creatures generation frequency is configurable.
	* Native sudden death is configurable.

# Version 102

* Natives appears slightly less often. Previous version spawned too many of them.
* Police State,    DocLoy,   --EFFIC,     ++SUPPORT,   ++POLICE,     -PLANET\
With more abundant native life PLANET penalty for this choice is too harsh.

# Version 101

* Native life abundance modification.
	* Frequency of appearance is based on difficulty level rather than native life level.
	* Natives appears more often.
	* Natives do not suddenly die every 8th turn.

# Version 100

* Needlejet cost = 3.
* Gravship cost = 4.

# Version 99

* Pressure Dome increases minerals by 50% instead of improving base tile. Same effect as for Recycling Tank since it replaces it.

# Version 98

* Equally distant territory is given to base with smaller ID (presumably older one).

# Version 97

* [fix] Fixed bug in Knowledge SE.

# Version 96

* Cybernetic,      Algor,    ++EFFIC,  -----POLICE,    ++PLANET,    ++RESEARCH
It was too little benefits for such harsh POLICE penalty. Now it is more of less seems like future society.

# Version 95

* [bug] Base screen population incorrectly shows superdrones those are not on psych screen. They do not affect drone riot. This is fixed.

# Version 94

* AI tries to kill spore launcher near their bases.
* Artillery bonus per level of altitude is removed. Forgot to remove it earlier. I find it too cumbersome to measure altitudes of every tile around the battlefield. It is too not visual.

# Version 93

More distinct and bold SE modifications in a way to emphasise the lore of each SE.

* Democratic,      EthCalc,  ++EFFIC,     --SUPPORT,   ++GROWTH,     -INDUSTRY\
Returned original ++EFFIC and replaced PROBE penalty to INDUSTRY one. More distinct this way and probe was more of knowledge weakness.

* Planned,         InfNet,   --EFFIC,     ++GROWTH,     +INDUSTRY\
It was pretty fine original way. Didn't know why I changed it.

* Knowledge,       Cyber,     +EFFIC,      -GROWTH     --PROBE,   ++++RESEARCH\
Replaced EFFICIENCY penalty with GROWTH penalty. EFFICIENCY its negative values are quite uninteresting while GROWTH penalty offsets too much positive freebies in the game.
Also emphasised on research even more. This is, after all, an ultimate research focused SE choice.

* Wealth,          IndAuto,   +ECONOMY,   --MORALE,     -POLICE,    ++INDUSTRY\
This one gets one more well deserved INDUSTRY. This is an ultimate production focused SE choice. It was too weak originally.

* Eudaimonic,      Eudaim,   ++ECONOMY,    -EFFIC,    ---MORALE,    ++GROWTH\
Looses one of its EFFICIENCY penalty. I think it was too cripled for powerful future society. Should be decently effective now.

# Version 92

* Police State,    DocLoy,   --EFFIC,     ++SUPPORT,   ++POLICE,    --PLANET\
Returned --EFFIC and ++SUPPORT to it and also extra PLANET penalty. Should be a little bit more interesting like that.

* Democratic,      EthCalc,   +EFFIC,     --SUPPORT,   ++GROWTH,    --PROBE\
Reduced SUPPORT penalty. It seemed too harsh especially at the beginning.

* Fundamentalist,  Brain,    ++MORALE,    ++PROBE,      +INDUSTRY,  --RESEARCH\
Removed +TALENT, added +MORALE to make it more combat oriented and different from Police State drone quellying focus.

* Cybernetic,      Algor,     +EFFIC,  -----POLICE,    ++PLANET,    ++RESEARCH\
More benefits and more penalties. Before it was too not interesting.

* Thought Control, WillPow, ---SUPPORT,   ++MORALE,    ++POLICE,    ++PROBE,      ++TALENT\
Added extra TALENT to it. Should be an ultimate war time drone solution. It's pretty good at that but other than war it is not good for anything. So I don't think it become too OP.

# Version 91

* [fix] Removed "+" from SE choice giving talent and reverted to their exact original names. Apparently it messes things up sometimes.

# Version 90

* recycling_tanks_mineral_multiplier configuration parameter is added.

# Version 89

* Research is 1.25 times slower.

# Version 88

* Another adjustment to weapons/armors value/cost. Now they use more of less round and same numbers to ease up on player memory.

# Version 88

* Some more adjustment to weapons/armors value and cost.

# Version 87

* [fix] Unit cost were calculated incorrectly sometimes.

* Recycling Tanks now increases minerals by 50% instead of improving base tile.
* Recycling Tanks cost/maint: 6/2.
* Clean minerals = 24.
* Research is 1.5 times slower.
* Fungus yield reshuffled to give energy before nutrients.
* Weapon and armor appearance and value/cost are adjusted to match each other progression.
* Some facility costs and maintenance are increased especially later ones.
* Some SP cost are increased slightly based on increased corresponding facility cost above.
* Lower tech get preference in blind research and AI pick.

# Version 86

**This version is recalled due to the bug in unit cost calculation**

* Armor value is adjusted based on its position in the tree.
* Borehole Square,  0, 2, 4, 0. Further reduces energy flow and limit its usage to barren lands.

# Version 85

**This version is recalled due to the bug in unit cost calculation**

* Disabled crawlers.
* Version cloning_vats_and_population_boom changes are included.
* Project contribution mechanics is removed.

# Version cloning_vats_and_population_boom

**This version is recalled due to the bug in unit cost calculation**

Test version for new CV and popboom mechanincs

* The Cloning Vats gives +2 GROWTH rating instead of triggering population boom directly.
* GROWTH rating upper cap = 9. This makes population boom very hard to achieve.
* Base nutrient view now displays population boom message when appropriate.
* Planned: --EFFIC, ++GROWTH, +INDUSTRY, -RESEARCH.
* Eudaimonic: ++ECONOMY, --EFFIC, ---MORALE, ++GROWTH.

# Version 84

**This version is recalled due to the bug in unit cost calculation**

* Cleaned AI terraforming code. It is supposed to work even better now but still use cautions as AI algorithms are very complex.

# Version 83

**This version is recalled due to the bug in unit cost calculation**

* \[fix\] Bug in sorting terraforming requests.

# Version 82

**This version is recalled due to the bug in unit cost calculation**

* The Manifold Harmonics is enabled by Sentient Resonance (~80% in research tree).

# Version 81

**This version is recalled due to the bug in unit cost calculation**

* \[fix\] Repair rate in base with corresponding facility configuration setting didn't work.

# Version 80

**This version is recalled due to the bug in unit cost calculation**

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

* Move plant fungus later.
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

