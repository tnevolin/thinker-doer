# Version 197

* Exposed turn until aliens fight at half strength in thinker.ini: aliens_fight_half_strength_unit_turn.

# Version 196

* Removed +50% fungal tower defense bonus. Configuration parameter: remove_fungal_tower_defense_bonus. They don't need it anymore with 1:1 land psi combat odds.

# Version 195

* RESEARCH bonus percentage parameter is added in thiker.ini: se_research_bonus_percentage.
* RESEARCH bonus is doubled.
* Fundamentalist,  Brain,    ++MORALE,    ++PROBE,      +INDUSTRY,  --RESEARCH. It loses extra RESEARCH penalty which was added previously just because RESEARCH bonus was too weak.

# Version 194

* Knowledge,       Cyber,    ++EFFIC,    --PROBE,     +++RESEARCH. Lost GROWTH penalty and some RESEARCH bonus.
* University gets +3 RESEARCH.
* Also removed test effects in Frontier.

# Version 193

* Also made lines in SE description help window more compact to fit 5 effects as well (down-left when player hover mouse over SE choice).

# Version 192

* Display compact effect icons in SE dialog to fit 5 of them in model description. Off by default because it also requires large icons redesign.

# Version 191

* Display TALENT effect in SE dialog. Only for SE choices! Max buttons and faction inherent bonuses are not changed. Currently using some ugly icon waiting for better design.

# Version 190

* FIX: Vanilla counted only predefined colonies for purpose of player elimination computation which caused some aquatic factions to be incorrectly eliminated. Now it counts all of them.

# Version 189

* FIX: Game now requests to breaking a treaty before goind into combat stuff preventing many treaty related bugs. Setting: break_treaty_before_fight. Off by default due to unknown consequences.

# Version 188

* Sensor bonus applies on ocean and on attack. Exposed options.

# Version 187

* Territory bonus = 25%.
* Sensor defense bonus = 25%.
* AI constructs sensors on land and sea.
* Sensor confers defense bonus on sea too.

# Version 186

* Added ability cost explanation to the end of the ability selection rows in workshop.

# Version 185

* FIX: Bombarding empty square with adjacent enemy artillery exhausted attacker movement points and initiated artillery duel resulting in imminient and untimely attacker death. Fixed.

# Version 184

* FIX: Sea unit directly attacking sea unit stack would pick a defender with higher weapon rather than higher armor. Fixed.

# Version 183

* BUG: Extra prototype cost was not reduced to zero for factions with free prototype bonus.

# Version 182

* BUG: Sometimes tech cost is stored as zero in savegame. Didn't find why it happened but added a safety code that overrides it with some small positive value when calculated for F2 screen.

# Version 181

* Configuration option to allow AI deviate from its social priority choice (social_ai_soft_priority). Turned off by default.

# Version 180

* BUG: Accumulated minerals scaling to changed INDUSTRY rating didn't account for mid turn retooling. Fixed. https://github.com/tnevolin/thinker-doer/issues/36

# Version 179

* FIX: Probe expelled from other faction territory was sent to largest population base which could be not appropriate for sea probes. Now probes are sent to **closest** base and for sea probes they also should be connected to same region so it should not get stranded on land or magically appear in another ocean. If such base cannot be found the probe is killed.

# Version 178

A lot of ASM code redone. May break some previously working functionality related to hurrying and minerals. Proceed with caution.

* Exposed hurry cost multipliers for all categories.

# Version 177

* Computer player won't buy technology from human player when explicitly offered if they don't have enough cash.

# Version 176

* Infiltrate Datalinks option is always available even if already infiltrated.
* Infiltrate Datalinks option displays number of currently planted infiltration devices.
* Infiltrating Datalinks sets number of infiltration devices to the maximum regardless of current device number.

# Version 175

* Hooked up probe actions. Now I can set risks for them manually.
  * Set Itroducting Genetic Plague action risk to 2 (was 0 in vanilla). Now Disciplined and Hardened probes cannot even succeed in it without opponent negaitve PROBE help.
* Put risk values on all probe team action options. This allows roughly estimate success chances.

# Version 174

* Computer players do not lose their infiltration.
* Human player to human player infiltration lasts 20 turns on average.
* Human player to computer player infiltration lasts 60 turns on average.

# Version 173

* Infiltration expiration is not broken into multiple steps providing slightly more predictable infiltration lifetime and more insigth for player.

# Version 172

* Changed infiltration expiration configuration parameters to lifetime based instead of probability.

# Version 171

* Updated infiltration expired dialogs text.

# Version 170

* Infiltration may randomly expire every turn. Infiltrated faction PROBE rating increases probability in exposing infiltrator.
  * infiltration_expire
  * infiltration_expire_probability_base
  * infiltration_expire_probability_probe_effect_multiplier

# Version 169

* Pact bases display production on a map.

# Version 168

* BUG: Base production prototype cost was incorrectly calculated.

# Version 167

* AI expansion demand algorithm. The populatable tile weight is now reduced by distance quadratically, not linearly as before to reduce the need to populate whole planet.
* AI unit move algorithm now manages sea explorers without order.
  * Send to other connected ocean region through connected base (own or allied).
	* Disband immediatelly if there is nothing more to explore in this region to save on support.
* ai_production_expansion_priority=1.0. Experimental settings to see how it affects expansion.
* Introduced more fine grained control on when WTP production suggestion beats default one (Vanilla + Thinker). Added thresholds for each category.
  * ai_production_vanilla_priority_unit=0.50
  * ai_production_vanilla_priority_project=1.00
  * ai_production_vanilla_priority_facility=0.25
* Adjusted terraforming resouce priorities. Previously it was too high on minerals IMHO.
  * ai_terraforming_nutrientWeight=1.00
  * ai_terraforming_mineralWeight=1.25
  * ai_terraforming_energyWeight=0.75
* Synchronized MP configuration. Sorry, it gets behind sometimes.

# Version 166

* Removed Combat bonus: Infantry vs. Base. Attacker does not need any more economical advantage. Infantry is still preferrable for base attack since it is cheaper.
* Increased native life generation frequency. Now it will be 2-5-8 by levels (vanilla was 2-4-6).
  * native_life_generator_constant=2
  * native_life_generator_multiplier=3
* Reinstated "natives doesn't die" parameters. This alone should slightly increase native pressure.
  * native_disable_sudden_death=1

# Version 165

* Introduced negative ability values into unit cost calculation.
* Introduced (010000000000 = Cost increased for land units) ability flag into unit cost calculation.
* Deep Radar is free for sea/air and 1 for land.
* Air Superiority is free for sea/air and 1 for land.


# Version 164

* Wealth,          IndAuto,   +ECONOMY,   --MORALE,     -PLANET,    ++INDUSTRY. Replaced POLICE penalty with PLANET one.

# Version 163

* Reverted all native life generator parameters to vanilla. Native life should be back to normal.
  * native_life_generator_constant=2
	* native_life_generator_multiplier=2
	* native_life_generator_more_sea_creatures=0
	* native_disable_sudden_death=0

* Scaled tech cost up a little. It seems that research are coming in too quick succestion. This'll also give some value back to RESEARCH.
  * tech_cost_scale=1.25

# Version 162

* Partial hurry option in hurry dialog displays addition information on minimal hurry cost to completion.

# Version 161

* Police State: replaced -INDUSTRY with -GROWTH. People say former is to harsh penalty for it.

# Version 160

* Removed: Combat units are reassigned to avoid oversupport. Configuration parameter: unit_home_base_reassignment_production_threshold.\
Apparently, this is too narrow focused fix that breaks AI in other areas. Not worth it.

# Version 159

* BUG: Too long line in help entry caused game crash. Fixed Tachyon Field, Growth, Industry entries.

# Version 158

* Added build support and installation step for multiplayer configuration files.

# Version 157

* BUG: Version 150: Alien Artifact is cached at base contributing to project or prototype production. \
That didn't actuially contributed anything due to incorrect ASM sequence. As warned! :)\
Should be fixed now. At least I have tested the case with not first base and with some existing accumulated minerals.

# Version 156

Cancelled.

# Version 155

Cancelled.

# Version 154

* Disabled enhanced native life generator. Now it is back to vanilla values. Players still can turn on on by using these properties: native_life_generator_constant, native_life_generator_multiplier.

# Version 153

* Added configuration option for former wake fix: fix_former_wake.

# Version 152

* BUG: Forgot to revert Pressure Dome maintenance back to 0 when detached Recycling Tanks functionality from it.

# Version 151

* Spore Launcher cost = 8. Otherwise people prefer to build it for defense. They should be equal in price with other low psi unit types.

# Version 150

**Experimental**

*Original functionality is scattered across the code and is heavily hardcoded. This modification may make game crash or display incorrect results.*

* All direct mineral contributions are scaled to basic mineral cost multiplier in alphax.ini. That effectivelly ignores INDUSTRY rating impact on such contributions.
  * Alien Artifact is cached at base contributing to project or prototype production.
  * Unit is disbanded at base contributing to current production. This is actually already scaled in vanilla so no changes were needed.
  * Hurrying production. Hurrying cost is not dependent on INDUSTRY rating anymore.

# Version 149

**Experimental**

*Original functionality is scattered across the code and is heavily hardcoded. This modification may make game crash or display incorrect results.*

* [Flat extra prototype cost](https://github.com/tnevolin/thinker-doer#flat-extra-prototype-cost) formula is enabled by flat_extra_prototype_cost parameter in thinker.ini.

# Version 148

* Redid (FIX: Formers regained their moves after cancelling new terraforming action when old one just completed this turn.) to use patch instead of custom function. More concise this way.

# Version 147

* Increased tech cost steepness. Recent modification made research progress very fast.

# Version 146

This one lists both #145 and #146 changes. They were mixed up and I could not sort them out. This is only in description. Code wise this version includes everything.

* AI:
  * AI builds some facilities.
  * AI builds formers when needed.
  * AI builds explorers when needed.
  * AI builds colonies when needed.
  * AI builds native protectors and police.
  * AI suggests production for new bases.
* Global production hurry happens before production phase to produce fully completed items.
* FIX: Thinker production unit mineral cost calculation. Previously it didn't account for prototypes/Skunkworks and Brood Pit.
* Added predefined units: Trance Formers, Trance Sea Formers.
* Hab Complex is now enabled by FldMod (level 2). Sea and jungle bases still struggle to get it in time.
* Restored vanilla emergency retooling after base attack. That was previously overridden by Thinker.
* FIX: Sea explorers were stuck in land port. Now they are force kicked out of it.
* FIX: Formers regained their moves after cancelling new terraforming action when old one just completed this turn. They don't anymore. (Reported by MercantileInterest)

* Reduced cost of late mineral facilities. Otherwise, it is pretty difficult to reach the mineral intake when they become beneficial. Here I also listed the mineral intake when they roughly becomes beneficial.

| facility  | cost | mineral intake |
| ---- | ----:| ----:|
| Recycling Tanks | 6 | 8 |
| Genejack Factory | 12 | 24 |
| Robotic Assembly Plant | 18 | 48 |
| Quantum Converter | 24 | 80 |
| Nanoreplicator | 30 | 120 |

# Version 145

* AI: Added land improvement demand computation.
* Global production hurry happens before production phase to produce fully completed items.
* Fixed Thinker production unit mineral cost calculation. Previously it didn't account for prototypes/Skunkworks and Brood Pit.
* Added predefined units: Trance Formers, Trance Sea Formers.
* Hab Complex is now enabled by FldMod (level 2). Sea and jungle bases still struggle to get it in time.
* Restored vanilla emergency retooling after base attack. That was previously overriden by Thinker.
* AI production: added multiplying facilities.
* AI production: added psych facilities.
* AI production: added population limit facilities.

# Version 144

* Borehole Square,  0, 2, 6.
* Exposed condenser and enricher fix parameter: condenser_and_enricher_do_not_multiply_nutrients.

# Version 143

* [https://github.com/tnevolin/thinker-doer/issues/3]. HSA now exhausts probe team movement points.

# Version 142

* Modified expansion parameters to build proportional number of colonies.

# Version 141

* Enhanced Thinker threat computation. Previously sealurks scared land colonies away.

# Version 140

* BUG: Didn't check map boundaries when iterated through adjacent tiles. That crashes game when base is placed on map border.

# Version 139

* All Brood Pit morale effects are disables same way as for Children Creche.

# Version 138

* AI:
  * Fine tuned colony production in small bases to see if they have enough population issue it at completion.
  * Fixed exploration calculations to account for combat units explorers only.
  * Former redundant order are cancelled so they do not create improvement that is already there.

# Version 137

* AI:
  * Set production exploration demand.
  * Adjusted produciton expansion demand based on range to destination and availabe settlement room.

# Version 136

* Merged with scient's 2.1 help.

# Version 135

* Weapon help always shows cost even if it equals to firepower.

# Version 134

* Rearranged facility priorities: submerge protection, defense, population limit, psych, growth, mineral, project, energy, fixed labs.
* Discouraged building colonies in base size 1.
* Biology Lab costs 4 and produces 4 labs. Without that it seems to be not desirable early choice. Extra labs bonus will diminish later on with higher technology cost anyway.

# Version 133

* AI tuning.
  * Psych facilities are considered if drones outnumbers talents or there are doctors.
  * Psych facilities list is extended to FAC_RECREATION_COMMONS, FAC_HOLOGRAM_THEATRE, FAC_PARADISE_GARDEN
* Added multiplayer text files folder.
* Updated help text for The Planetary Transit System.
* Added configuration parameter for The Planetary Transit System new base size: pts_new_base_size_less_average.

# Version 132

* AI tuning.
  * Bases do not build units taking too much time. Configuration parameter: ai_production_unit_turns_limit.

# Version 131

* FIX: Captured base now properly receives free facilities from both capturing and captured factions. Didn't work in vanilla.

# Version 130

* AI base production and unit movement adjustment.
  * Base tries to build native protection if insufficient.
  * Unit in the base tries to hold to maintain native protection.
  * Base tries to build colonies if there is room for colonization.
* BUG: Battle status summary alignment messes up with Hasty and Gas modifiers. Feature removed.

# Version 129

* Experimental nutrient balance version #3.
  * Citizen nutrient intake = 2. Reverted to vanilla.
  * Farm nutrient bonus = 2 (both land and ocean).
  * Nutrient cost multiplier = 15.
  * Base Square produces 2 nutrients.

# Version 128

* Returned -1 Nutrient effect in mine square. This is in addition to nutrient balance. Otherwise some landmark mines produce too much nutrients.

# Version 127

* Further refinement to ZoC rule. Unit ignores ZoC only if target tile is occupied by own/pact unit that does not ignore ZoC by itself: probe, cloaked, air.

# Version 126

* Experimental nutrient balance version #2.
  * Citizen nutrient intake = 2. Reverted to vanilla.
  * Farm nutrient bonus = 2 (both land and ocean).
  * Nutrient cost multiplier = 15.
  * Base Square produces 1 nutrient.

# Version 125

* Returned Cloaking device into game: cost = 2, enabled by Quantum Computers.

# Version 124

* Unit cannot ignore ZoC moving to the tile occupied by friendly unit. Configuration option: zoc_move_to_friendly_unit_tile_disabled.

# Version 123

* Experimental nutrient balance version.
  * 1,       ; Nutrient intake requirement for citizens
  * 15,      ; Nutrient cost multiplier
  * Base Square,      1, 1, 1, 0,

# Version 122

* Ocean tiles depth is modified upon world build. Configuration option: ocean_depth_multiplier.

# Version 121

* SE reworked a little.\
http://alphacentauri2.info/wiki/Social_Engineering_Mod

# Version 120

* Experimental boost to specialists.

| specialist | output |
| ---- | ---- |
| Technician | 4, 0, 0 |
| Engineer | 5, 0, 1 |
| Librarian | 0, 0, 4 |
| Thinker | 0, 1, 5 |

# Version 119

* BUG: Tile yield calculation didn't set base. That caused crash for tile yield computation with collector on it and adjacent mirror when base is not set sometimes. Strangely it didn't bite me earlier.

# Version 118

* AAA bonus vs. air units = 50%.
* AAA Tracking ability cost = 1.

# Version 117

* Pressure Dome does not imply Recycling Tanks anymore. 
* [removed] Extra energy reserves to aquatic factions to support initial Pressure Dome maintenance.

# Version 116

* Extra energy reserves to aquatic factions to support initial Pressure Dome maintenance.

# Version 115

* BUG: Ignore independent units in reassignment code.

# Version 114

* Combat units are reassigned to avoid oversupport. Configuration parameter: unit_home_base_reassignment_production_threshold.

# Version 113

* Simplification in PTS functionality: https://github.com/tnevolin/thinker-doer#the-planetary-transit-system.

# Version 112

* AI build more defensive units based on: the combined strength of enemy attacking units, their distance from own bases, the diplomatic status, whether the player is human/AI.

# Version 111

* FIX: Late starting factions (aliens and Planet Cult) never got their extra colony. Now they do. Fixed incorrect calculation on game restart.

# Version 110

* Pressure Dome,                 8, 2.\
Same maintenance as for Recyclyng Tanks since it delivers same benefits.\
Should not be too dificult for sea bases to pay it.

# Version 109

* FIX: Late starting factions (aliens and Planet Cult) never got their extra colony. Now they do.

# Version 108

* Alternative inefficiency formula.\
https://github.com/tnevolin/thinker-doer#inefficiency

# Version 107

* Fungicide Tanks costs 0.\
I don't think they are so important to cost anything. The ability slot is big enough cost for it.
* Super Former costs 2.\
Again, not that superimportant ability.

# Version 106

* FIX: Zero division in Recycling Tanks mineral calculation when mineral intake is zero.

# Version 105

* BUG: Base screen population incorrectly shows superdrones those are not on psych screen. Fixed for alien factions as well.

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

* FIX: Fixed bug in Knowledge SE.

# Version 96

* Cybernetic,      Algor,    ++EFFIC,  -----POLICE,    ++PLANET,    ++RESEARCH
It was too little benefits for such harsh POLICE penalty. Now it is more of less seems like future society.

# Version 95

* BUG: Base screen population incorrectly shows superdrones those are not on psych screen. They do not affect drone riot. This is fixed.

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

* FIX: Removed "+" from SE choice giving talent and reverted to their exact original names. Apparently it messes things up sometimes.

# Version 90

* recycling_tanks_mineral_multiplier configuration parameter is added.

# Version 89

* Research is 1.25 times slower.

# Version 88

* Another adjustment to weapons/armors value/cost. Now they use more of less round and same numbers to ease up on player memory.

# Version 88

* Some more adjustment to weapons/armors value and cost.

# Version 87

* FIX: Unit cost were calculated incorrectly sometimes.

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

* FIX: Bug in sorting terraforming requests.

# Version 82

**This version is recalled due to the bug in unit cost calculation**

* The Manifold Harmonics is enabled by Sentient Resonance (~80% in research tree).

# Version 81

**This version is recalled due to the bug in unit cost calculation**

* FIX: Repair rate in base with corresponding facility configuration setting didn't work.

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

* FIX: AI hurrying project sometimes resulted in negative accumulated minerals.

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

* FIX: Hurry production algorithm tried to hurry production in huma bases as well. Now it does not affect human.

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

