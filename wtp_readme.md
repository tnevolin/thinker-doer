# SMACX The Will to Power mod

The Will to Power is a playing experience enhancement mod for Alpha Centauri: Alien Crossfire.
It is built on top of Thinker mod. Read the complete description of Thinker mod in: Readme.md, Details.md, Changelog.md.

# Credits

Induktio: Created Thinker mod which greatly improves AI intelligence and makes game much more challenging.

# Why this mod?

There are tons of interesting features in the game. Designers did a good job on it. I don't think there is another so feature reach game. I DO want to try them all! :)
Unfortunately, I usually end up not using most them just because they don't make a difference. Trying them up just for fun and scenery is one time experience - no replayability. The range of unusability varies from completely broken to only marginally useful or not worth micromanaging and extra mouseclicks. Overall, about 2/3 of vanilla features are unusable or underpowered to extent of being strategically unviable. It is so sad to see multitude of otherwise cool features are not actually used by most players.
I do want them to make difference. Therefore, I started this mod to give them second chance! :)

In such option reach games like SMAX variations are endless. Due to such variations some strategical choice may shine sometimes depending on current conditions. Changing strategy in response to changing game situation is the nature of the play and that's why we have many to chose from. However, there should never be an option that never (or quite rarely) viable. Such unused option would just clutter game interface and player memory. Well balanced game should not have any unusable elements. In this mod I try to get rid of all outsiders or balance them somehow to make them usable.
In other words, any feature should be advantegeous without doubts in at least some play style of game situations. Proportion of games where it provides such advantage should be noticeable too. At least 1/4 of the games, I think.

# Design concepts

Whenever I see some feature worth improving/highlighting these are guidelines I use to decide whethere I should touch it at all and, if yes, to which extent. By default I try to not fix what is not obviously broken.

* Feature should do what it claims to. If it is impossible to make it work, such feature should be excluded. Example: Deep pressure hull.
* Feature should not outright contradict or break game concepts. Example of game concept: build-your-own-unit design workshop. Example of the feature directly contradicting this concept: mixed components (resonance armor and such).
* Game flow and features generally should be understandable to people and have clear visual effect on the game and player choices. This doesn't mean they should resemble real life. They should be only what is expected of them by average game player. There should be no hidden elements or algorithms those are difficult to grasp and apply. Example: unit cost formula. In vanilla infantry attacker 10-1-1 unit costs 5. With that in mind average player with previous Civ1/2 experience naturally would expect same strength infantry defender 1-10-1 cost the same. They would be shocked to discover that it is 20 in SMACX actually. This doesn't match player expectations.
* Overall game parameters configuration should not create an extremely overpowered or underpowered strategy. Each strategy should be viable at least at some circumstances according to this mod principles. This mod does not intent to "balance" anything but only to adjust some obvious favorite or underdog strategies. Overpowered stategy example: indestructible army.
* Being usable also means to come at the right time in research tree. Coming too early clutters game interface and takes space from other more needed features. Coming too late causes unnecessary slowdown and frustration. This mod adjusts item appearence times to match faction development level.
* Some items naturally come in sequence when each follower effectively obsolets its predecessor. Good example is weapons and armors. This mod tries to spread them evenly on a research scale so player can enjoy each for about same time. It also tries to link their corresponding technologies to assure they appear in right order. 
* Mutually exclusive items/features are adjusted in price and effect so that they can compete with each other and none is completely inferior. Good example is social models.

# Rearranging technology tree

A lot of above changes require moving items and features up and down technology tree. Apparently, rearranging technology tree is inevitable. This may seem like a big change for users. Therefore I dedicate a whole section to explain my reasons.

Rearranging technology tree is not something unheard of. A lot of mods do it and produce quite playable experience. The trick is in accuracy and placement since handling a dependency tree is non trivial work.

SMACX futuristicly named technologies have no roots in real scientific history except maybe Fusion Power :). This is done, obviously, on purpose to highlight a sci-fi atmosphere. Same story is with other in game concepts, items, and features. Nobody can rationaly explain why technology has such prerequisites or why it allows certain game features. I agree that *some* technology-feature relations make sense but most do not. In this regard I believe fiddling with technology tree is an acceptable modding approach. One could memorize some game concepts after thousands of games, of course but I doubt this is the way to go. Most of the time I fing myself browsing help to understand which technology uncover which feature. That is completely fine and that is what help is for.

I tried to minimize technology tree changes to satisfy my modding needs only and to not get highwire about it. I selected one primary feature for each technology among those it uncovers. Such primary feature is the most memorized and most important technology association. In other words, player usually researches certain technology for its primary feature. Example: Doctrine: Air Power for needlejet chassis. I firmly kept such assisiations. Everything else might change. However, I also tried to keep modified tree as close as possible to vanilla one. Technologies may float but they do not go far from where they were originally. Like Biogenetics is still early game technology while Advanced Spaceflight is still late game one. I also tried to preserve secondary assosiations whenever possible to not mix things up too much.

NOTE TO USERS
I used my own selection of primary feature. If someone believes there should be a different primary association - let me know. I'll gladly substitute. After all, the technology is just a placeholder for features and can be replaced or even renamed as needed.

I think I did good job on linking technologies. Vanilla technology level quite inaccurately predict technology appearance time. My tree is built with exactly 7 technologies per level. Each technology prerequisites are exactly from two below levels. This puts a pretty good timeline and value on a technology which is a great help for technology exchange. You know right away that any level 4 technology is clearly farther up the tree than any level 3 one - no need to look them up in datalink. Now it is easy to predict relative technology appearance time by its level.


COMBAT ENGINE
==================================================

Reactor power does not multiply unit max power anymore. All units (conventional and native) have max power of 10 regardless of reactor.

Round odds are proportional to unit corresponding strengths. That is a fix for vanilla which used incorrect formula.

Alternative combat mechanics favors previous round winner generating longer winning streaks thus reducing battle winning probability skew. The parameter is configurable and can be turn off.

Odds confirmation dialog now displays correct winning probability percentage. Vanilla odds numbers were cool but highly unusable without calculator not even mentioning they were incorrect to start with.


HEALING RATES
==================================================

Healing rates are lowered to eliminate fast and instant healing. Parameters are configurable.

Current settings:
Field: 10% up to 80% regardless of location.
Base/bunker/aribase: 10% up to 100%.
Base with facility: +10%.
SP for natives in base: +10%.
Nano Factory: + 10%.


BASE DEFENSIVE STRUCTURES
==================================================

Configurable parameters.

Current values:
Intrinsic base defense: 50%, configurable in alphax.txt.
Perimeter Defense: 100%, configurable in thinker.ini.
Tachion Field: +100%, configurable in thinker.ini.


OTHER CHANGES
==================================================

Attack/defense adjustment
--------------------------------------------------

Overpowered strategy: Indestructible army.
Cost/benefit imbalance: Conquering whole planet for near zero cost investment in army reinforcement.
Fix: adjust attack/defense ratio, combat unit costs, military facility costs so that invader lose units conquering enemy bases and need constant resupply to advance further. From economical point of view invader should incur about 2-3 times bigger loss against PREPARED defense of enemy at the same technological level. Hefty price on conquering enemy bases makes non-stop conquering strategy not always a best choice. War related economical stagnation could be a too severe consequence to pay for expansion.
Technological advantage gradually decreases loss ratio. One still need the 50% stronger weapon along with artillery superiority to capture bases with TD without losses. This is an improvement comparing to vanilla where weapon technology advantage and artillery is not required at all.

To support the point above weapon and armor rating in this mod go 1:1 until about end of the game. The top weapon is about 20% stronger than top armor. Not that this difference is important at the end game where winned is essentially determined already. This is the point of this mod: let factions effectivelly defend themselves throughout the game and do not crack under slightest pressure in the middle ages.
Weapon and armor strengths were redone to resemble Civ 1/2 slow exponential progression. Indeed, I cannot imagine why developers would left such weird weapon strength progression as 2-4-5 (?) or 12-13-16 (?). Whereas it is completely easy to correct it to normal growing progression by just changing text configuration. I've adjusted other weapon strenghts too to make their progression look more exponential as it should be. Each next level item is about 25% stronger starting from value 4 onwards. Unfortunately, early weapon/armor discoveries still chaotic in this way. Simple discovering strenght 2 weapon against strenght 1 armor is sudden 100% jump that is difficult to correct.

NOTE TO USERS
Large number of conventional weapon items (12) in the game presents two potential improvement areas.
One should experience war conflict in each of 12 first technology levels to enjoy each and every weapon/armor item. This is practically impossible and, therefore, big part of weapon/armor items is inevitable unused. It does not contradict my principle as each one can still be used over the course of multiple games. Yet, if anyone believe there are too many of them, I can reduce number of weapons to 8 or something.


Unit cost (general discussion)
--------------------------------------------------

Great news! I've hacked into exe thanks to Induktio and his Thinker mod. That let me fulfil my long time dream of modifying unit cost calculation. Now it is much-Much-MUCH easier to understand unit cost. Both 6-1-1 and 1-6-1 infantry units now cost 6 rows of minerals. Imagine such simplicity! That was not like that in vanilla.
Moreover, it completely removes a quadratic armor cost growth problem. High end mixed inrantry units now cost comparable to speeder and fully armored foil units are comparable to hovertank.

Conventional combat unit cost calculation is redone to resemble Civ 1/2 unit costs. Unit base cost is defined by its primary statistics item cost. Pure infantry unit (attacker or defender) with lowest reactor costs exactly as its primary statistics item. Example: 4 weapon costs 4 => 4-1-1*1 unit costs 4. Same story with modules. Setting colony module cost to 6 makes (no armor) infantry colony to cost exactly 6!
Faster chassis are slightly more expensive. Speeder, foil, needlejet and copter are 1.5 more expensive than infantry. Hovertank and gravship are 2.0 times more expensive than infantry. Mixed units (full weapon + full armor) are 50% more expensive. More advanced items are stronger but their price doesn't grow as fast as their strenght. Therefore, it is generally beneficial to use stronger items.

Reactors decrease unit cost by about 20% each level. That applies to all units reducing late game multi-abilities units cost to bearable level.
Another good news about this reactor decrease cost is that it is beneficial to upgrade units to higher reactor versions. They do not become stronger but player get 50% refund for negative cost upgrade! How cool is that?

I appreciate Yitzi's attempt at adding flexibility to abilities cost. However, I don't see much value in it. It is too fine grained and too not visually clear to understand ability value. I've introduced just one additional ability pricing model instead: flat cost. That is for all abilities not improving or not related to weapon/armor values. I believe this is fair and enough to fix the problem with insane Hypnotic Trance cost for combat units.
Conventional combat related abilities naturally improve weapon/armor values - unit fights as if it get stronger weapon/armor. Such abilities correctly increase unit price proportionally. Others have nothing to do with the wearing items like anti native improvements, all non combat improvements, deep radar, etc. I believe such abilities should be priced flat. That allows them to be used on beefed up units as well without sacrificint hand and leg. Hypnotic Trance is one example. Why cannot I add it on a high end unit for the flat cost to protect them from enemy worm attacks? After all their worms are flat cost as well!
These two ability cost models can be used together. So one can configure ability to cost a proportion plust a flat cost if desired!

Cheap colony pod allows fueling expansion with nutrients excess only and ignores any economical development whatsoever. Nutrient reach faction keeps stamping colony pods and fills up all available space exponentially. Not surprisingly, such simple strategy is also a most effective way to get economical advantage early in the game. Higher colony pod price put expansion speed in check of both nutrients and minerals production encouraging early terraforming and development. Now player needs to invest into base growth and terraforming in order to expand faster.

Harvesting resources by crawler is a very lucrative investment. Harvesting 4 minerals from rocky mine pays for vanila crawler in 7.5 turns! Then it delivers 4 minerals each turn. That is just insane ROI. I suggest to price it as high as 120 minerals which brings its effectiveness closer to Genejack Factory. Even at this price it is still quite useful but it is not a single ultimate strategy anymore. You would think thrice if you want to build a crawler just to extract 2 units of production.

Native warfare should be slightly worse to conventional as they have other benefits. They ignore base defensive structures. They are naturally both full scale attacker and defender. Their price is fixed and is much lower comparing to fully equipped top level attacker-defender units. They do not require prototyping. IoD can transport. Sealurk can attack shore units. LoC does not need refueling and can capture bases. They all can repair up to 100% in fungus squares. They do not require maintenance while in fungus square. All together they are no brainer units and as such should be a little bit less effective to not become a superior choice. I've increased most native unit cost except spore launcher to encourage its usage due pathetic damage.


Unit cost (formula)
--------------------------------------------------

unit cost in mineral rows = [<primary item cost> + (<secondary item cost> - 1) / 2] * <reactor cost factor> * <proportional abilities cost factors> + <flat abilities costs>
(rounded normally)

primary item = higher cost module/weapon/armor
secondary item = lower cost module/weapon/armor

reactor cost factor is set in thinker.ini as a ratio of <reactor cost value from thinker.ini> / <Fission reactor cost value from thinker.ini>. So that Fission reactor cost factor is always 1.


Weapon and armor value progression
--------------------------------------------------

Vanilla game has very weird code that selects regular conventional weapon icon based on its offensive value. There is nothing like that for any other item types (non regular-conventional weapons, armor, chassis, ability). Wery precise, specific, and meaningless piece of programmatic machinery.
This is fixed now. Modders are free to set any offensive values to regular conventional weapons without breaking their respective icons! Woo-hoo, thanks to me.

With this in mind I was able to correct weapon strenght progression to smoothen it while keeping proper icons for each weapon.
Here is the current game weapon and strength progression by technology levels:
weapon: 1, 2, 3, 4, 5, 6, 8, 10, 13, 16, 20, 24, 30
armor:  1, 2, 3, 4, 5, 6,  , 10,   , 16,   , 24,
There are less armor items in the game so they do not appear every research level at second half of the game. When they appear they match same level weapon strength.


Facility cost
--------------------------------------------------

Energy Bank = 6, Network Node = 6, Research Hospital = 9, Nanohospital = 18
Cost is reduced slightly for these. They seem to be a pretty interesing facilities not getting build early because of their price.

Biology Lab = 4
Unattractive facility with fixed income. I don't see much sense in paying a fixed maintenance for fixed income. What, you pay one energy to get two labs - what's the point? In this mod it has no maintenance and cost is slightly reduced. This gives player insentive to build it before Network Node.

Naval Yard = 5
Reduced price to give player insentive to actually build it. It is priced as a combination of morale boosting and defensive facility. However, one usually needs morale boosting benefit at rich unit producing bases and defense benefit at weak distant ones - rarely both at once.

Bioenhancement Center = 16/4
Morale boosting facility for all unit realms. Together with other facilities it produces elite units. Should have very high cost and maintainance.

Geosynchronous Survery Pod = 16/2
Dropped maintenance since this is defense helping facility. I rarely use it myself. Maybe with lower maintenance it'll be more attractive.

Aquafarm = 12/2
This is the most beneficial facility out of three aquatic yield related ones. It affects all work squares and not only half as other two. Besides, nutrient surplus is the most powerful resource that in time compensates lack of two others due to population growth and increase in workers. I think the above increased price is not even enough to compensate for it but 12 rows is already quite high for early game.


Secret Project cost
--------------------------------------------------

General consideration about SPs: they are very lucrative. Especially those affecting whole empire. Even after they just built they are pretty useful and their value keep growing with your empire size. I believe making them cost at least same as replaced facility in 10 bases is still not fair but fairer than their original price. For SPs bringing constant benefit every turn I price them as <benefit per base per turn> * 10 bases * 20 turns. Your empire suppose to grow bigger than that eventually and you are not paying maintenance too.

I also cleary understand that exact SP cost is not that relevant. It easily can be moved 50% up or down and nothing changes much in the game. My main concern was their way to low cost in vanilla. I feel like they should be about 2-5 times more expensive based on benefits. Other than that I welcome your suggestions. 

The Human Genome				400
2 minerals worth quelled drone * 10 bases * 20 turns

The Command Nexus				400
40 minerals worth facility * 10 bases.

The Weather Paradigm			600
Tough to evaluate. Great boost to the faction via advanced terraforming but advantage doesn't last whole game. Should be most expensive in its time.

The Merchant Exchange			200
Local improvement - no change.

The Empath Guild				600
Way to Governorship and energy income multiplier. Should be quite expensive.

The Citizens' Defense Force		500
50 minerals worth facility * 10 bases.

The Virtual World				600
60 minerals worth facility * 10 bases.

The Planetary Transit System	400
Drone quellying + immediate growth boost. Proportional to empire size but limited to small bases only. Slight increase.

The Xenoempathy Dome			400
Tactical advantage, aid to fungus terrafirming, +1 lifecycle = slight increase.

The Neural Amplifier			800
Tough to evaluate but I tend to price combat effectiveness boosting projects quite high.

The Maritime Control Center		500
50 minerals worth facility * 10 bases.

The Planetary Datalinks			600
Again pretty vague but should be quite high priced since technology advantage is everything in this game.

The Supercollider				300
Local effect = no change.

The Ascetic Virtues				600
+POLICE + growth = slight increase.

The Longevity Vaccine			600
Drone quellying. Slightly higher valued than The Human Genome due to more advanced era.

The Hunter-Seeker Algorithm		600
No clue how to deal with this. People tend to value it quite high. Doubled the price.

The Pholus Mutagen				600
Allows higher production without ecology impact. Should be slightly more expensive.

The Cyborg Factory				1600
160 minerals worth facility * 10 bases.

The Theory of Everything		400
Local effect = no change.

The Dream Twister				800
Same as The Neural Amplifier.

The Universal Translator		400
Pretty limited usage of two technologies advancement. There are no unlimited artifacts in a game. No change.

The Network Backbone			1600
Large source of labs multiplied by the number of all world bases. Very high.

The Nano Factory				400
Slight tactical and unit upgrade advantage. Nothing seriosly game changing. No change.

The Living Refinery				400
about 2 minerals per base per turn * 10 bases * 20 turns.

The Cloning Vats				4000
Endless population boom. It is the most expensive SP in my mod (besides AtT).

The Self-Aware Colony			2000
About 25% raw energy income per base multiplied by number of bases in your empire. Also very highly priced.

The Clinical Immortality		1200
Drones and diplomatic victory. Should be quite high.

The Space Elevator				1000
Production boost for satellites. The boost itself is limited but satellites are the most lucrative facilities in the game. Priced high.

The Singularity Inductor		2000
200 minerals worth facility * 10 bases.

The Bulk Matter Transmitter		3000
+50% minerals at every base. Insane game breaker.

The Telepathic Matrix			2400
Total drone solution. Very high price.

The Voice of Planet				1200

The Ascent to Transcendence		5000
End game bases with crawlers should be able to build it in reasonable time but not instantly.

The Manifold Harmonics			600
Powerful fungus production. However, advantageous for high PLANET rating only and fully develop only in later game.

The Nethack Terminus			600
Some help to probe teams. Other than that nothing much.

The Cloudbase Academy			1200
80 minerals worth facility * 10 bases. Plus all benefits of Aerospace Complex (defense, morale, satellites yield, etc).

The Planetary Energy Grid		800
80 minerals worth facility * 10 bases.


Feature appearance time
--------------------------------------------------

Foil is available to be researched at start. Other modders said much about that already.

Network Node, Energy Bank
Moved to level 3.
These facilities are not effective at the beginning. Even with 20 raw energy yield they contribute 5 labs/energy = 2.5 worth of minerals and break even time is 32 turn. Only averagely beneficial and you need to get your first bases at 20 energy yield first.

Habitaion Dome
Moved to 50% in research tree (was 75%).
I often struggle to get it when needed. It's too far in the future.

Non-lethal methods
Moved to 75% in research tree (was 50%).
Doubling police power should not come too early.

Bioenhancement Center
Moved to 75% in research tree (was 50%).
Generic facility should come after all specific ones.

Hybrid Forest
Moved to 70% in research tree (was 50%).
Not needed immediatelly after Tree Farm. With its price nobody is going to build it right away.

Satellites
Moved to 80%, 85%, 90% in research tree (were somewhere at 60-70%).
Game breaker goes to the very end. Discussed many times by other modders.

The Living Refinery, The Manifold Harmonics
Relatively weak SPs moved earlier.


Special weapon and armor
--------------------------------------------------

Disabled all hybrid and psi items.

I really cannot grasp this concept. Why having half strength ability while you can attach fully powered one for the same price? Besides, mere melting ability to the item breaks the unique SMACX unit workshop build-it-yourself paradigm! Why to introduce constructor tool and then provide pieces those cannot be disassembled to elementary properties. I almost never use them for their added ability. Merely like another regular piece of equipment. And now in my mod they don't exist anymore and nobody cries.

Same story with psi weapon and armor. By the time you get them you have full spectrum of natives who are much cheaper than psi attack + psi defense unit. I personally never used them. Anybody does? Let me know if you think they are valuable assets.


Removed abilities
--------------------------------------------------

Cloaking device and Deep pressure hull abilities are broken by game shared map implementation. Other people said enough about it. Disabled them.

Repair Bay ability is useless in my mind as well. I cannot even imagine where would you massively transport wounded units.

Soporific Gas Pods. People reported AI doesn't use them due to the bug. I think it would be fair not to give them to humans too until this is fixed.


Yield restrictions
--------------------------------------------------

This is another feature I cannot understand. What good does restricting yield do? Why I cannot farm a rainy tile to boost my base growth a little? Why I cannot mine a rocky tile to get myself some minerals? In vanilla game these restriction are lifted when you discover advanced terraforming. At this time I don't need a rocky mine anymore since borehole is better. As the result I never build mines to work on - only to harvest by crawler. I don't know how to disable them completely so I moved them all onto level 1 technologies.

I guess this was a blind copy of Civ 1 Despotism restrictions. Despotism reduces any factor of production by 1 where you have 3+ of such production. However, this was compensated by unit production and settler food supports. Meaning you harvest less but you spend less on support too. SMACX lost this second part separating support into its own effect. Thus leaving this nicely thought and designed element half cut and stupid looking.


Fungus production
--------------------------------------------------

Fungus production is not some crucial part of the game. However, it is still some alternative yield source that is mostly overlooked due to very slow development comparing to conventional terrafirming. I'd like to add few touches on it.

Get it to at least 1-1-0 yield relatively early in the game to allow minimal support for barren land and sea bases.
Focus on energy yield in the mid game to compliment forest instead of competing with it.
Use green technologies for fungus production to simplify research priorities for green/PLANET factions.

Centauri Ecology			+1n
Progenitor Psych			+1m - reassigned
Field Modulation			+1e - reassigned
Bioadaptive Resonance		+1e - reassigned
Centauri Psi				+1n
Centauri Meditation			+1e
Secrets of Alpha Centauri	+1e
Centauri Genetics			+1m


Aquatic faction advantage
--------------------------------------------------

I was able to remove AQUATIC +1 sea mineral advantage. Now they are back to normal and expected growth. They will be slow on start but they have 50-100 years of undisturbed development. Once their old bases are developped enought to stamp sea pods they explosivly expand but then other factions start barraging waters and set their sea colonies. Seems to be a good balance. However, aquatic factions still develop quite differently. That requires a lot of play testing.


Forest
--------------------------------------------------

Many people before me mentioned overpowed forest. Indeed, it is capable of turning dry and barren 0-0-0 terrain into 1-2-1 adding 4 resourses in 4 turns with yield comparable to rocky mine. It is just insane terraforming effectiveness. As if this is not enough it spreads by itself = zero further investments. It is pretty nice option for poor bases but it should cost more than mine to be not the-only-viable-option. Currently I set its terraforming time to 12 and reduced yield to 1-2-0 to make it just enough helpful option for barren land bases but not too much to beat rocky mine.


Social engineering
--------------------------------------------------

This mod treats a combination of SE choices as an option, not a single SE model in isolation. It also doesn't try to "balance" SE models between each other but rather distribute SE effect changes across models to conveniently provide all the spectrum of possible SE effect ratings making all of them reacheable. I have devised a model to "compare" SE effects average value to each other with certain degree of assumption. Even though they can be "compared" to each other it is obvious that their use varies greatly during the course of the game and depending on circumstances. They had to be weighed based not only on their average value but on the other game related factors.

As always, I tried preserve social effects set whenever possible so vanilla SMs stay recognizable. Apparently, UI cannot display more than 4 effects per model. So that was another hard restriction for this reengineering project. Obviously, I did on purpose is tried to use up to 4 effects wherever possible to maximize effect usage and variety across the effect scale.

I timed them on research line so that first row of SMs comes at level 2, second at level 3, third at level 5 and fourth at level 7 which is about when 50% technologies are researched. This is somewhat earlier that in vanilla when you got last SE row at about 75% throughout the game. Certainly future society SMs are powerful but it is kinda useless to aquire them at the game sunset.
In accordance to this timing I tried to focus on most important effects at the time of appearance. Early models emphasize TALENT, SUPPORT, POLICE, GROWTH. While later models favor ECONOMY, EFFICIENCY, INDUSTRY, RESEARCH. Not necessarily exactly like this but as a rule of thumb.

ECONOMY and EFFICIENCY are by far the strongest effects. I tried to equalize their positive and negative impact whereever possible. Some people complained that I give by one hand and take by other. Well, sort of. I don't want Democracy-FreeMarket-Welth combination to rule the world. You want to play it over and over and over again - there is vanilla out there. I had my share and now I seek for more variability. You want to throw all goodies at a single SE choice - go ahead and be your own guest. I want a variety of option I can weigh and corretly apply to ever changing game situation instead. If you don't like the fact that some SE choices are not insanely overpowered - you probably shouldn't play my mod. See the mod goal at the very top.

ECONOMY changes are mostly positive in original. Even though negative effects are not that interesing I still added some negative ECONOMY changes here and there to outweigh positive changes in other SMs.

POLICE scale is assymetric and shifted toward negative values. The true middle point is at -1 rating. In vanilla game everybody essentially starts with inherent +1 rating. Besides, everybody can also build Brood Pit to get +2 POLICE later. That is too much inclination toward positive values which results in need of enourmous negative changes for some SE models like FM in vanilla. This mod tries to avoid this by subtracting 1 POLICE from ALL factions shifting starting position to where it should be. This allows applying positive and negative POLICE changes equally across SE choices.

GROWTH is very restricted on negative side. Just -3 rating halts base growth completely which is a pretty strong effect. On the other side of the spectrum +6 rating triggers population boom which is another pretty strong effect. It is a common concertn of all modders to limit population boom. In this mod it is attainable by Children Creche + SE + golden age. However, it is not attainable without golden age. One need to pour some amount of gold on people to make them happy and produce more children. Same as in old good Civ 2.

MORALE had a balanced scale by itself but morale facilities can easily raise unit morale even beyond what highest MORALE rating would do. That effectivelly renders high MORALE values useless. Don't forget to count Children Creche with its +1 morale bonus as well. As such I do not use too many MORALE modifiers across SE choices. SE provides +2 MORALE max only.

About the same story with PROBE and Covert ops center. Even though it is just one +2 facility the PROBE scale is shorter as well. SE provides +2 PROBE max only.


Probe morale boosing technologies
--------------------------------------------------

Some vanilla technologies boost probe team morale for free. That makes all new probe teams elite toward the end of the game. Err, what's the point in that if everybody gets it and this renders Covert Ops and PROBE ratings useless for probe team morale? I removed all such flags. Technologies do not improve probe team morale any more.


Unit hurry cost and unit upgrade cost
--------------------------------------------------

Unit hurry cost formula always puzzled me. Why make cost grow quadratically to become exorbitant for higher end units? Same story with unit upgrade cost. This one is, on the countrary, too cheap allowing "Building SP with upgraded crawler" exploit. I decided to simplify and flatten these as well as disabling that exploit along the way.

For now I've set unit hurry cost to be flat <minerals> x 4 value. The base upgrade cost now is an exact difference in hurry cost! Wow. Could anyone imagine game design could be that simple and transparent? :)
Since higher reactors now do nothing but decrease unit cost player also gets REFUND when upgrading to cheaper version of unit with higher reactor. How more fairer could it get? :)


Hurry cost penalty thresholds
--------------------------------------------------

Removed all hurry cost penalty thresholds. Never could grasp their strategical meaning since they do not affect course of the game as well. Let me know if anyone thinks otherwise.
Now all hurry costs are flat x2 for facilities and x4 for units and projects.


