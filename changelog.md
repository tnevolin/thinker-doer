# Version 318

* Base psych improved fine tuning. https://github.com/tnevolin/thinker-doer/blob/master/README.md#base-psych-improved
  * Psych is applied after other effects.
  * Psych generally improves happiness all over the base removing drones/superdrones first and then generating talents when no more drones left.
  * Psych effect is **NOT** limited. This is different from vanilla 2 x population limit.
  * It takes **6** psych to improve mood of the citizen instead of **2** in vanilla. This is to compensate above benefits and make psych effect more smooth.
  * Drone reduction effects (facilities, police, projects) work as in vanilla: make unhappy citizen content being them drone or superdrone.
  * Talent generating facilities/projects focus on turning leftmost citizen to talent instead of pacifying unhappy ones.
  * Drone riot definition is as in vanilla: more drones than talents.
  * Golden age definition is as in vanilla: no drones and at least half of talents.
* New more authentic superdrone design! Credits to Regina Jane.

# Version 317

* Base psych simplified: https://github.com/tnevolin/thinker-doer/blob/master/README.md#base-psych-simplified.

# Version 316

* Merged with Thinker 4.6.
* Reworded sea levels rising popup to warn player about potential base damaging tidal waves event.
* Habitation facilities add +1 GROWTH bonus unconditionally.
* Fixed Thinker bug when vehicle skipped on monolith does not get repaired.

# Version 315

* Removed RISK explanation from probe menu. They look ugly. Interested players can learn corresponding risks from here: https://github.com/tnevolin/thinker-doer/blob/master/game-mechanics.md#probe-actions-success-probability.
* Reverted air-to-air combat to Thinker computation. Interceptor gets AS bonus.
* Reverted conventional artillery duel to using weapon only.
* Base screen nutrient box displays "stagnant" and "pop boom" label instead of number of grow turns for corresponding events.
* Removed Thinker's "Population Boom" marker from the bottom right corner of the base screen as duplicate.

# Version 314

* Removed flat extra prototype cost.
* Removed RISK 1 for stealing tech.
* Fixed population limit facilities GROWTH bonus.
* Simplified base nutrient box title and added explanation in readme: https://github.com/tnevolin/thinker-doer/blob/master/README.md#base-nutrient-box-information.


# Version 313

* Player can scroll through other faction base screens with mouse and keyboard.

# Version 312

* Probe team tech stealing is now RISK 1 making it not 100% success.
* Base grows on same turn and carries over excess nutrients.

# Version 311

* SUPPORT changed to use vanilla mechanics with variable free units but scale is adjusted to avoid 2 support at -4 level.

# Version 310

* Added aaa_range setting. AAA ability can be extended to same faction units within given range. Off by default.

# Version 309

* Removed zoc_regular_army_sneaking_disabled setting.
* Added zoc_enabled setting. On by default. Experimental.

# Version 308

* Disabled zoc_regular_army_sneaking_disabled setting temporarily until fixed.

# Version 307

* Replaced counter espionage code with Thinker version.

# Version 306

* Removed alternative inefficiency computation. Vanilla one is good enough.

# Version 305

* Biology lab generates 4 labs.

# Version 304

* Fixed some help entries to not spill out of the window.

# Version 303

Few base hurry dialog improvements.

* Hurry cost is flat. (flat_hurry_cost, flat_hurry_cost_multiplier_unit, flat_hurry_cost_multiplier_facility, flat_hurry_cost_multiplier_project)
  * No penalty when less than retool excemption minerals accumulated.
  * No penalty for project when less than 4 mineral rows.
  * Units cost is flat and cotrolled by multiplier.
* Hurry dialog offers to hurry up to minimal number of minerals when production will be completed on next turn. (hurry_minimal_minerals)
  * Keep in mind that next turn production is an **estimate** that may change! Switch this option off if you prefer guaranteed next turn production.
* Ctrl-H automatically hurries production with enough credits.

# Version 302

Merged with Thinker 4.5.

* Conventional power contributes to psi combat (conventional_power_psi_percentage).
* Population limit facilities now grant +2 GROWTH when beyond limit.

# Version 301

* Fixed vanilla bug when it did not initialize tile owners at game setup.

# Version 300

* Major AI revamping. Alpha tested. Seems stable but requires some more thorough check.

# Version 299

* Fixed my bug in bombardment check that sometimese crashed the game.

# Version 298

* Reverted disabled probe tech stealing from faction with vendetta.

# Version 297

* Updated The Cloning Vats description.
* Reduced likelyhood of tech trade.
* Disabled probe tech stealing from faction with vendetta.

# Version 296

## AI

* Major AI improvement. See details in Readme.

## Other changes

* Amphibious cost = 0.
* Added Patrol Battery predesigned unit to aid AI in building first conventional artillery.
* Added Empath Battery predesigned unit to aid AI in building first anti-native artillery.
* Alternative combat mechanics (alternative_combat_mechanics) is disabled. That was there to aid stupid AI. Not needed anymore.
* Flat hurry costs changed. Unit and facility: 2 credits / mineral. Project: 4 credits / mineral.
* Biology lab research bonus (biology_lab_research_bonus) changed to 6.
* Sensor cannot be destroyed by bombardment or pillage. The only way to destroy it is to terraform some incompatible item on top of it.
* Pressure dome grants +1 mineral.
* More fine grained AI switches.
  * ai_useWTPAlgorithms: turns WTP AI on or off completely.
  * wtp_enabled_factions: turns WTP AI on or off for specific normal faction
* Artillery combat uses combat bonuses. For example, unit in base uses base defensive bonuses.
* Some modification to vanilla transport/passengers handling to make sure they are not jumping from one to another by themselves.
* Disabled my previous modification to not allow units to sneak one by one after the probe. The code turned out to be very complicated. This fix broke more than fixed.
* [BUG fix] Game sometimes crashes when last base defender is killed.
* [BUG fix] Game sometimes crashes when fireball is drawn out of viewport.
* [BUG fix] Disabled alien spore launcher shooting from transport.
* Patched vanilla worker distribution algorithm to assure acceptable nutrient surplus. Sometimes it allows base to stagnate.
* [BUG fix] Captured base should receive both factions bonuses. Sometimes it is other base who receives them.

# Version 295

* Native unit requires support even when in fungus.

# Version 294

* BUG: Game crashed under certain conditions when drawing boom. Fixed. Hopefully now for all conditions.

# Version 293

* BUG: Game crashed under certain conditions when drawing boom. Fixed.

# Version 292

Super update mainly targeting AI but includes some other changes as well.

## Regular changes

* BUG: Game crashed under certain conditions when last defender at base destroyed. Fixed.
* BUG: Game crashed under certain conditions when air unit damaged in the field at the end of turn. Fixed.
* FIX: Vanilla transports steal passengers from each other. Now transports are forbidden to share same tile.
* Added unit: Patrol Battery - anti native artillery.
* Added unit: Empath Battery - anti native artillery.
* Hurry cost is reduced to 2 for facilities and units and to 4 for projects.
* Biology labs bonus increased to 8.
* Pressure Dome adds 1 mineral in base square.
* Computer player distributes unit support between bases to not overburden some of them too much.
* All player units gets assigned to player bases. There are no more **free** units and computer player is no longer support other factions formers.
* Artillery duelists gets **same exactly** bonuses as melee units: base defense, sensors, etc.

## AI specifics

* New configuration parameter: wtp_factions_enabled. Allows to apply either Thinker or WTP AI algorithms to computer players.
* AI builds military and defensive facilities.
* AI tries to evaluate base threats and beef them up accordingly.
* AI tries to evaluate base exposure (internal/external) and more often builds defensive structures in exposed bases.
* AI considers ocean more penetrable than land and may build sea defensive structure even deep inside ocean territory.
* AI now properly understand continuous continent and ocean regions despite stupid vanilla polar cap regions.
* AI now properly understand how coastal bases connect ocean regions and treat them as a continuous one.
* AI handles base police demands and prefers police2x units.
* AI more specifically determines dangerous locations and don't send colonies there. Still far from ideal algorithm.
* AI generates more unit design choices.
* AI explicitly obsoletes inferior unit designs.
* AI uses reworked area connection algorithm allowing it precisely understand borders of continent/ocean bodies.
* AI reuses Thinker Path finding algorithm imported from vanilla. Thanks to Induktio.
* AI collects more details about base threat: each unit type, their proximity, their morale, their damage, etc.
* AI distinguish melee and land artillery unit groups. It now builds artillery to counter artillery. Still not perfect, though.
* AI more directly request production for certain units if it lacks them at the unit movement stage.
* AI tries to emphasize prototype production to use less number of stronger units and reduce support burden.
* AI evaluates **each and every** potential base placement spot and makes both transportation and production decisions based on that.
* AI tries to not travel too far with early bases except for some exceptionally lucrative location nearby.
* AI tries to place bases closer to each other to use as much land as possible without too much cramming.
* AI tries to weight unit usefulness against specific base threat and distribute protectors accordingly. The algorithm is still pretty crude.
* AI tries to produce most useful counter units against specific base threat. Should produce more natives against heavy regulars, scouts against natives, etc.
* AI tries to complete tile terraforming. Previously it was leaving too many farms without mine/solar.

## AI sea transportation

* Serious rework in sea transportation. Land units are more straightforwardly moved to destination on another continents. Thanks to new geography system.
* AI faction now easily jumps in and out of water and onto another continents. Thanks to the above.
* Same way it distributes land combat units across all land/ocean bases regardless of their geographical placement.
* AI still may experience navigation problems in super convoluted continent/ocean configurations.

* FIX: Game sometimes crashed when last defender at base killed.
* FIX: Game sometimes crashed when drawing a fireball over air unit at the end of turn.
* Disabled AI units reassignment between bases.
* Unit forced base assignment is relaxed to make sure it does not overburden base.
* Biology Lab generates 8 labs (thinker.ini: biology_lab_research_bonus).

## Internal

* Introduced a patch that disables combat refresh thus speeds it up tremendously. It helps to advance AI vs. AI games by constantly pressing Enter. Now they can be run to turn 100 in 15 minutes or so. I can expose this setting in thinker.ini if anyone is interested.

# Version 291

* All units are assigned to this faction base. No more random free of support units.

# Version 290

* AI designs anti-native artillery.
* Artillery duel uses sensor and base bonuses same way as melee combat.

# Version 289

* Artillery damage is not multiplied by attacker reactor when ignore_reactor_power_in_combat is set.

# Version 288

* Added variable orbital resource limit as population fraction (thinker.ini: orbital_*_population_limit).
	* Currently set to 0.5 for each type. Seems to be adequate.

# Version 287

* FIX: Stockpile energy bug/exploit. Somehow it was not fixed in Thinker which includes scient patch which claimed this fixed.
	* The fix adds stockpile energy at production phase and subtract stockpile energy at energy phase. Thus moving this check to the right place.
	* The fix also computes the energy following vanilla algorithm adding 25% for Planetary Energy Grid.

# Version 286

* Reduced Tree Farm and Hybrid Forest cost and adjusted their maintenance a little
	* Tree Farm,                    16, 3, EnvEcon, Disable,  Econ/Psych/Forest
	* Hybrid Forest,                24, 6, PlaEcon, Disable,  Econ/Psych/Forest

# Version 285

* Fixed Thinker bug not setting social agenda bias and set such bias to 1000 to ensure AI sticks to their agenda.

# Version 284

* Fixed vanilla bug causing turn not to advance when force-ending turn while needlejet in flight has moves left.

# Version 283

* Fixed some nasty C++ std::sort bug causing crash on AI turn.

# Version 282

* Intellectual Integrity unlocks Non-Lethal Methods and corresponding Police Garrison predesigned unit.
* Amphibious Pods allowed for non-combat units including probes.
* Missile chassis cost increased to twice infantry chassis cost.
* Conventional Payload strength reduced to 15 (was 18).
* Conventional Payload cost is reduced by reactor. Same way as other weapons.
* Planet Buster cost is NOT reduced by reactor. This is to compensate its significant reactor power increase.

# Version 281

* Set Clean Reactor ability cost to 32 (again). Somehow it got changed and this change is undocumented. Maybe some random edit I didn't plan.

# Version 280

* [Bug] My code that fixed incorrect best defender selection for sea to sea combat was flawed itself ignoring damaged units. Should be fixed now.

# Version 279

* [Bug] Forced AI colony not to build bases adjacent to other bases. That happened in some cases.

# Version 278

* [Bug] Fixed the bug during pact withdrawal that prevented units to be sent home.

# Version 277

* Returned initial clean minerals to 24.
* Added eco damage facilities production.
* Reverted WTP and Thinker terraforming comparison parameter back to "all WTP".
* Reworked tech cost a little.

# Version 276

* Added configuration parameter to compare WTP and Thinker terraforming.

# Version 275

* Clean minerals are reverted to vanilla default: 16.
* Reworked eco damage industry effect reduction formula (https://github.com/tnevolin/thinker-doer#alternative-eco-damage-industry-effect-reduction-formula).

# Version 274

* Moved **Non-lethal methods** much earlier. Police is generally not that effective due to unit maintenance. This can make it more useful.
* Moved ecodamage reducing facilities earlier. The need for them arise much earlier in this improvement and mineral reach mod.

# Version 273

* Explicitly defend bases.
* Increased former production priority.
* Increased road network building priority.
* Returned global warming frequency back to original.

# Version 272

* Fixed my bug when base can produce sea unit without access to ocean.
* Doubled average number of turns to discover an infiltration device.
* Native unit cost now grows slower with time (2 times in 350 turns, not 4 times as before).
* Limited displayed combat odds to 1:100 - 100:1 range to avoid displaying huge numbers.

# Version 271

* Revorked combat bonuses from previous version to use more of vanilla existing bonuses with minimal fix.
	* Reverted terrain bonuses.
	* Removed "faster unit" special bonus.
	* Introduced +25% attacking along the road bonus (in thinker.ini).

# Version 270

* Removed "mobile in open" bonus. Introduced "faster unit" bonus that consistently applies to any faster unit attacking slower one on any terrain. Experimental. https://github.com/tnevolin/thinker-doer/blob/master/README.md#mobilie-attack-bonus
* Fine grained terrain defensive bonuses. Rocks keep their hefty 50% bonus due to its rarity and impossibility to create with terraforming. Other more often and creatable rough terrains have 25%.
* Allowed disengagement from stack. Never understood that vanilla restriction. Not that victor is going to step on the battle tile anyway.

# Version 269

* Reduced Comm Jammer bonus to +25%.
* Mag Tubes are moved earlier on the tech tree (75%->50%). Somewhere when Air power is discovered. There is no big deal in having them earlier now since they are limited in speed.
* Mag Tube base terraforming time is increased to 6 to make it slightly more expensive even when it appears earlier.
* Hovertank chassis is reassigned to Probability Mechanics to keep them at about same time since Monopole Magnets moved earlier.

# Version 268

* Removed free Recycling Tank from Usurpers. Forgot to do it when I repurposed it.

# Version 267

* Further combat units production optimization.
* Reduced some early facilities maintenance. They put too much pressure on early factions.
	* Recycling Tanks,               8, 2
	* Genejack Factory,             12, 4
	* Robotic Assembly Plant,       18, 6
	* Quantum Converter,            27, 8
	* Nanoreplicator,               40,10
	* Hab Complex,                   8, 1
	* Habitation Dome,              16, 2
	* Command Center,                8, 1
	* Naval Yard,                    8, 1
	* Temple of Planet,             20, 4
	* Aquafarm,                     16, 3
	* Subsea Trunkline,             12, 2
	* Thermocline Transducer,        8, 1

# Version 266

* Found a purpose for pathetic Universal Translator and other single base labs multiplying projects. They were not that impactful and interesting in vanilla. This version turns them into regular faction wide projects affecting all bases with small labs bonus. This bonus is applied in the same order as before: after regular multiplying facilities like Network Node and before The Network Backbone.
	* The Supercollider,            30, Base labs +10%
	* The Theory of Everything,     60, Base labs +20%
	* The Universal Translator,    100, Base labs +30% + Two Free Techs

# Version 265

* Disabled AI designing defender ships.
* Reinstated Soporific Gas Pods ability and taught AI to design it.

# Version 264

* Fixed pact withdrawal sea unit moving to land locked base.

# Version 263

* Reverted cost of early energy multiplying facilities back to higher values. They seem to be rightly priced with abundance of energy after invention of boreholes.
  * Energy Bank,                   6, 1
  * Network Node,                  6, 1
	* Fusion Lab,                   16, 4
  * Quantum Lab,                  32, 8
  * Research Hospital,            12, 3
  * Nanohospital,                 24, 6
  * Tree Farm,                    18, 3
  * Hybrid Forest,                36, 6
* Increased maintenance of mineral multiplying facilities to match the 3:1 energy to mineral default conversion ratio.
  * Recycling Tanks,               8, 3
	* Genejack Factory,             12, 6
	* Robotic Assembly Plant,       18, 9
	* Quantum Converter,            27,12
	* Nanoreplicator,               40,15
* Moved weapon and armor discoveries slightly later. There is no much need it weapon escalation early in the game as conflicts are rare.

# Version 262

* Adjusted scout production priority.
* Ocean bases build transports when needed.
* Updated colony shipping algorithm.
* Updated alien hunting algorithm.
* Updated pod popping algorithm.
* Streamlined terraforming a little.
* QoL: Enemy treaties message now pops up. Previously it hid under other popups unnoticed.
* Fixed blind research tech randomization.

# Version 261

* Further enhanced returning sea probe to the base algorithm. Now it should find the nearest one.

# Version 260

* Disabled vanilla and Thinker base hurry algorithms. WTP does it itself.
* Enabled combat unit production in case of defense demand.
* Reworked returning sea probe to the base algorithm. Apparenly, previous attempt didn't work well.

# Version 259

* Mineral multiplying facilities affect all minerals. Whereas energy multiplyers affect only 1/2 - 2/3 of energy. Modified prices for all multiplying facilities accordingly.
	* Energy Bank,                   4, 1
	* Network Node,                  4, 1
	* Fusion Lab,                   16, 4
	* Quantum Lab,                  32, 8
	* Research Hospital,            12, 3
	* Nanohospital,                 24, 6
	* Tree Farm,                    18, 2
	* Hybrid Forest,                36, 4
	* Recycling Tanks,               8, 2
	* Genejack Factory,             12, 4
	* Robotic Assembly Plant,       18, 6
	* Quantum Converter,            26, 8
	* Nanoreplicator,               40,12
* Increased AI lust for minerals.
	* Base placement favors minerals.
	* Terraforming favors minerals.
	* Base favors mineral multiplying facilities.
* Reduced sea based chassis cost for non combat related modules (colony, former, transport). I forgot to do it when I changed the sea chassis cost in previous release.
* Some adjustment to production choices.
* Fixed bug when AI didn't hurry production. That could be a reason for major flaw in AI performance in recent versions. Should be better now.
* Reintroduced: Lower tech get preference in blind research and AI pick.
* Conventional artillery duel uses weapon+armor value. This is to disable ship attack/bombardment exploit and add additional dimension to the duel. This does not change display info, though. Only the total strength.
* Modified combat production choices to respond to threat.
* Increased coast bases attractiveness.
* Disabled resource restriction alltogether. There is no need to discover Centauri Ecology for that anymore. I never knew this is possible!

# Version 258

* Reverted relative chassis cost to make infantry the cheapest one. Other chassis: speeder/foil and hovertank/cruiser are 50% more expensive each.

# Version 257

* Reverted weapon and armor cost to match their strength. It was an interesting experiment but I didn't see it adding much value.
* Disable destroying sensors. Tested in single player.

# Version 256

* Updated alien factions description to match their bonuses.
* Nerve Gas Pods is allowed for sea units.
* Removed Infantry Probe Team predefined unit.

## Experimental faction balancing.

### Spartans

People say they are very strong with their +1 POLICE that turns into +3 POLICE while running PS and double police power. Adding -1 EFFICIENCY penalty to make their PS choice more difficult to support.

### Cult of Planet

People say they are very strong with their +2 PLANET and double police power for mind worms. Adding -1 POLICE penalty to make them more difficult to achieve higher police ratings.

# Version 255

## Attempt to revise factions and make them closer to original ones. At least in idea.

### Angels

* TECH: Flexibility -> Polymorphic Software. To allow immediate land/sea probes production.
* SHARETECH, 3. Infiltration is not required to steal research.
* SOCIAL: ---RESEARCH. Increased penalty to compensate for research stealing.

### Beleviers

* Reverted to original.

### Caretakers

* TECH: Progenitor Psych, Centauri Ecology. Both suit them well.
* SOCIAL, +PLANET, SOCIAL, -INDUSTRY. Industry penalty to compensate their extra colony at start.

### Cyborgs

* Reverted to original except below.
* SOCIAL: ++RESEARCH, ++EFFIC, --GROWTH. I decided to keep their original bonuses but penalize growth more instead.

### Drones

* Identical to original.

### Cultists

* Reverted to original.

### Gaians

* Identical to original.

### Hive

* Reverted to original.

### Morganites

* Identical to original.

### Peacekeepers

* Reverted to original.

### Pirates

* Reverted to original except below.
* Neither aquatic faction receives extra minelar from sea.

### Spartans

* Identical to original.

### University

* Reverted to original except below.
* SOCIAL: +++RESEARCH. Their lore is to be distinct researchers beyond anyone else in the game.

### Usurpers

* TECH: Progenitor Psych, Applied Physics. Both suit them well.
* SOCIAL, +GROWTH, SOCIAL, +MORALE, SOCIAL, -PLANET, SOCIAL, -INDUSTRY. Industry penalty to compensate their extra colony at start.

# Version 254

* Do not break treaty when attacking artifacts only stack.
* Updated Zone of Control concept wording.

# Version 253

* AI verifies prototype is legit before proposing it. Previously some not allowed combinations were generated. Not tested.

# Version 252

* Reduced free units to 2 to match vanilla closer. Anything higher seems excessive.
* Set base weapon and armor cost to 1. Previously it was set to 0 making Trance Scout Patrol too cheap.
* Updated Hive description to highlight EFFICIENCY immunity.
* Updated Cyborgs description to reflect their correct innate SE bonuses.

# Version 251

* All weapons and armor cost 1 less than their value. Unit still has minimal cost.
* Reduced native units cost.
* Fixed Knowledge SE effect list.
* Stopped maintaning multiplayer configuration. Nobody ever requested me any specific for that. They are just identical copies.

# Version 250

* AI adjustment: expand vs. defend.
* AI adjustment: facilities.
* Tech rate growth steeper.
* Tech cost corrected based on total discovered tech by all factions.
* Faster chassis are cheaper: speeder +25%, hovertank +50%, cruiser +25%.
* Colony costs 6.
* Pod instant completion is modified to add 20 minerals instead.

# Version 249

* AI production, prototype, combat modifications.
* Tech cost formula adjusted to use by level cost. Early levels are cheaper.

# Version 248

* Reverted subversion and MC cost global multiplier back to 1. Instead tuned specific cost parameters.
* Changed subversion and MC cost parameters.
  * Subversion cost multiplier = 6. Twice as much as hurry cost multiplier.
	* Doubled resource intake cost multipliers.

# Version 247

* Support formula changed.

# Version 246

* Added global scaling factor multiplying actual subversion and mind control cost (alternative_subversion_and_mind_control_scale). This does not change corner market cost!

# Version 245

* Fixed incorrect vanilla combat odds oversimplification that often looses 50% of value. Now odds are more precise.

# Version 244

* Fixed my bug that sometimes garbled reactor name in technology advancement screen.

# Version 243

* Fixed subversion crash.

# Version 242

* Excluded probe attack from sea-to-sea best defender selection fix.

# Version 241

* Fixed incorrect combat odd computation in some cases when best defender is not on the top of the stack.

# Version 240

* Supply Convoy and Info Warfare require support.

# Version 239

* Thought control doesn't cause controlled faction to wish betrayal and revenge against controlling faction.
* Added note that TC is useless during vendetta.

# Version 238

* Capturing base guaranteed destroys Recreation Common and Recycling Tanks in vanilla. Now they have same 1/3 destruction probability as other facilities.

# Version 237

* [Alternative subversion and mind control formulas and mechanics](https://github.com/tnevolin/thinker-doer#alternative-mind-control-and-subversion-mechanics).
	* Made happiness effect exponential to cover more dynamic range. All talents/drones increase/reduce cost four times.
	* Added help description for subversion and mind control.

# Version 236

* [Alternative subversion and mind control formulas and mechanics](https://github.com/tnevolin/thinker-doer#alternative-mind-control-and-subversion-mechanics).
	* Can subvert unit from stack.
	* Extended effect of Polymorphic Encryption.
	* Base MC cost is based on its value and citizen happiness.
	* Base MC cost includes subverted unit cost.
	* Distance to HQ factor is now more linear and moderate.
* Polymorphic encryption is assigned to Polymorphic Software to allow subversion protection when probes appear.

Usual word of caution: a lot of vanilla code patching. Could be unstable or work not as expected.

# Version 235

* Reverted combat randomness to vanilla state.

# Version 234

* Marine Detachment cost 2 (50%). It is reportedly too good.

# Version 233

## Minerals carry over and retooling penalty functionality

* Excess accumulated minerals are carried over to next production.
* Retooling is not penalized on the first production turn right after previous production completed.
* Retooling is not penalized when changing from already built facility or project.

That completely eliminates the fear to lose accumulated minerals in case project is completed by other base. Production can be retooled without penalty and losing accumulated minerals. There is also no need to select costly item to utilize all these excess minerals. Minerals will continue to carry over to next production without loss.

Moreover, retooling is free on the turn when base just completed previous production. That allows human player to change their mind multiple time this turn.

# Version 232

* Fixed combat odds display for psi combat.
* Made combat odds display computation more precise by taking actual unit strength instead of reverse engineering them from odds. Vanilla sometimes screws odds calculation pretty badly.
* Increased combat unit priority a little to make sure AI builds them.
* Forced prototype production to reduce AI unwillingness to build modern units.

# Version 231

* Wounded units go to monolith or base.

# Version 230

* AI computes relative threat from other factions to priorities military production.
* Injured units try to heal.

# Version 229

* Reverted Gaians to vanilla configuration. Don't remember why I changed them in first place.

# Version 228

* BUG FOUND: Faction alien units mistakenly get "+1 on defense at base" bonus when MORALE is 2 or 3. Decided not to fix it as it happens very rare.
* Removed Pirates bonus minerals from their description.

# Version 227

* Updated Angels description to not require infiltration to get technology from other factions.
* Temporarily disabled right of passage agreement and scorched earth modifications until teaching AI how to defend better.
* Removed morale mention from Childrens' Creche short description.

# Version 226

* Slightly adjusted some armor values.

# Version 225

* Increased military production priority. Hopefully, this will make AI build more units.
* Removed AI need for exploration. Instead made them attack natives on their territory.
* Adjusted AI base protector movement and holding.
* Energy market crash is multiplying energy reserves by 3/4 now, not 1/4 as it was in vanilla.
* Increased cost of level 1 technologies. They were coming too fast.
* Increased overall priority for military production (units, facilities). I hope this will make AI at least as aggressive as in vanilla/Thinker.
* Modified Morganites Datalinks description about population limit. For this to work morgan.txt need to be copied to game directory or the corrected description need to be pasted into existing morgan.txt file.

AI military production is not very well tested yet but it is endless iterative process anyway. I'll continue improving that in following versions.

# Version 224

* Design workshop display unit cost in rows.

# Version 223

* Sea transport delivers scouts to pods.

# Version 222

* Invokes hostile action vendetta dialog when commlink is established. Vanilla did not invoke it for non-combat hostile actions without pact/treaty/truce.

# Version 221

* AI removes fungus/rockiness when settles in such places.

# Version 220

* Fixed partial hurry cost computation that was off sometimes due to rounding problems. It is still can be off sometimes but will err on safe side.

# Version 219

* Merged with Thinker 2.5.

# Version 218

* Added orbital_yield_limit parameter to limit orbital yield proportional to base population. I have left it as 1.0 currently as I don't believe satellites are that OP in end game. Suggestions are welcome.
* Reverted orbital facilities to their vanilla order of appearance. I have reconsidered my view on them and now I believe that nutrient satellites are less OP of them all.
* Further doubled orbital facilities cost to the level of small global SP. I believe it is fair as they produce similar faction global effect. Now it will take a significant investment to harvest a benefit.

# Version 217

* Reverted road movement ratio to 3. Anything other than will probably throw player off.
* Reworded odds confirmation dialog. Hopefully should be much clearer now.
* Roads/tubes can be used only on own or ally territory with right of passage.
* When faction looses a base (captured or killed) all terrain improvement on lost territory are destroyed.

# Version 216

* Fixed interceptor scrambling. https://github.com/tnevolin/thinker-doer#scrambling-interceptor-fix

# Version 215

* Disabling territory bonus temporarily to see how it behaves.
* Tech cost computation is simplified by reusing game variables. It is not stored in save anymore reducing risk of multiplayer corruption.

# Version 214

* Reinstated alien faction offense and defense bonuses. Not sure they need to be crippled even more.
* Reinstated fanatic attack bonus to 50%. Also not sure Believers are too powerful with it.

# Version 213

* Integrated few more changes from Thinker.

# Version 212

* Corrected odds computation for unequal HPs.
* Updated multiplayer configurations.
* Set road movement rate to 2 and tube movement rate to 4.

# Version 211

* Restored terraforming functionality.
* Restored anti-native combat functionality.
* Restored transport functionality.
	* Transports pickup artifacts, colonies, formers from sea bases.
	* Transports deliver them to sensible destinations.

# Version 210

* Massively modified display odds dialog.
	* Vanilla odds are corrected to account for ignored reactor in combat.
	* An exact computer simulation winning odds are added as well for reference.
	* An exact computer simulation winning chances are added as well for reference.

# Version 209

* Merged with updated Thinker code (no version).
* AI factions are allowed to build bases in fungus and rocky tiles.

# Version 208

* Merged with Thinker 2.4.
	* A lot of code changes. I have fixed some but there could be others lurking around. Please use caution and report any problems.

# Version 207

* Sea transport delivers colony and former to proper landmass.

# Version 206

* BUG: Late starting factions (aliens and Planet Cult) never got their extra colony. Now they do.
* Modified base placement search algorithm.
	* Searches wider area.
	* Accounts for potential yield (farm+mine, farm+solar).
	* Emphasizes extra high yield tiles.
	* Tries to minimize base radiuses overlap.
	* Tries to keep base radiuses connected to minimize land waste.
	* Prefers coastal sites.

# Version 205

Further SE models tuning.

Frontier,        None,
Police State,    DocLoy,   --EFFIC,     ++SUPPORT,   ++POLICE,     -GROWTH
Democratic,      EthCalc,  ++EFFIC,     --SUPPORT,   ++GROWTH,     -PLANET
Fundamentalist,  Brain,    ++MORALE,    ++PROBE,      +INDUSTRY,  --RESEARCH
Simple,          None,
Free Market,     IndEcon,  ++ECONOMY, ----POLICE,    --PLANET,     -PROBE
Planned,         InfNet,   --EFFIC,      -SUPPORT,   ++GROWTH,     +INDUSTRY
Green,           CentEmp,   +EFFIC,     --GROWTH,    ++PLANET,     +TALENT
Survival,        None,
Power,           MilAlg,   ++SUPPORT,   ++MORALE,     +GROWTH,    --INDUSTRY
Knowledge,       Cyber,     +EFFIC,      -GROWTH     --PROBE,    +++RESEARCH
Wealth,          IndAuto,   +ECONOMY,  ---MORALE,     -PLANET,     +INDUSTRY
None,            None,
Cybernetic,      Algor,    ++EFFIC,    ---POLICE,    ++PLANET,    ++RESEARCH
Eudaimonic,      Eudaim,   ++ECONOMY,    -EFFIC,    ---MORALE,    ++GROWTH
Thought Control, WillPow,  --SUPPORT,   ++MORALE,    ++POLICE,    ++PROBE

# Version 204

* BUG: Air transport can unload in flight with keyboard shortcut or right action menu. Fixed.

# Version 203

* BUG: Right click long range fire option allowed land artillery to fire from transport. Fixed.

# Version 202

* Group terraforming option: group_terraforming.

# Version 201

* Fanatic bonus = 25.
* Believers do not have PLANET penalty.
* Aliens do not have combat bonuses.

# Version 200

* Alternative upgrade cost is disabled by default.
* Individual unit upgrade does not require full movement points and does not consume them upon upgrade.

# Version 199

* Habitation facilities functionality seriously changed.
	* Explicit population growth limit is removed. Bases can grow beyond the limit when not in stagnation.
	* Base without habitation facility receives -3 GROWTH penalty when at limit. Penalty gets higher with higher population.
	* Base with habitation facility receives GROWTH bonus for each citizen below the limit but no more than +3.
	* Habitation facilities maintenance is increased by 50% comparing to vanilla.
* Added visual aid to base nutrient display.
	* Header displays: (faction GROWTH) base GROWTH => nutrient box width. Much easier to understand numbers at work. No need to count bricks with mouse anymore.
	* Footer displays clear indication of pop boom (when GROWTH rating is above max) and stagnation (when it is below min).
	* Lighter text color and bolder font makes it more readable now. It was real pain to my eyes before.

# Version 198

* Removed +50% fungal tower defense bonus effect display in combat effects list.

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

* Infiltration expiration is now broken into multiple steps providing slightly more predictable infiltration lifetime and more insigth for player.

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

* BUG: Late starting factions (aliens and Planet Cult) never got their extra colony. Now they do. Fixed incorrect calculation on game restart.

# Version 110

* Pressure Dome,                 8, 2.\
Same maintenance as for Recyclyng Tanks since it delivers same benefits.\
Should not be too dificult for sea bases to pay it.

# Version 109

* BUG: Late starting factions (aliens and Planet Cult) never got their extra colony. Now they do.

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

* FIX: Unit cost was calculated incorrectly sometimes.

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

