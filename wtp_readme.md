CREDITS
==================================================

This mod continues my Fission Armor mod work combined with Induktio's Thinker mod which does great job on improving AI play. Now I am able to tap into exe patching (thanks to Thinker mod project code base!). Due to that I was able to overcome reactor problem and unit cost calculation problem that was previous unsolvable by simple text modding. All reactors are now back into game as well as other features. "Fission" doesn't belong to the name anymore. Hence I renamed it from "Fission Armor" to "Doer". Seems to be a logical name as I try to enable all the underused game features and encourage player to *DO* different things.


WHY THIS MOD?
==================================================

There are tons of interesting features in the game. Designers did a good job on it. I don't think there is another so feature reach game. I DO want to try them all! :)
Unfortunately, I usually end up not using them just because they don't give me any noticeable advantage. Trying them up just for fun and scenery is one time experience - no replayability. The range of unusability varies from completely broken to only marginally useful or not worth micromanaging and extra mouseclicks. Overall, about 2/3 of vanilla features are unusable or underpowered to extent of being strategically unviable. It is so sad to see multitude of otherwise cool features are not actually used by most players.
I do want them to make difference. Therefore, I started this mod to give them second chance! :)

In such option reach games like SMAX variations are endless. Due to such variations some strategical choice may shine sometimes depending on current conditions. Changing strategy in response to changing game situation is the nature of the play and that's why we have many to chose from. However, there should never be an option that never (or quite rarely) viable. Such unused option would just clutter game interface and player memory. Well balanced game should not have any unusable elements. In this mod I try to get rid of all outsiders or balance them somehow to make them usable.
In other words, any feature should be advantegeous without doubts in at least some play style of game situations. Proportion of games where it provides such advantage should be noticeable too. At least 1/4 of the games, I think.


DESIGN CONCEPTS
==================================================

Whenever I see some feature worth improving/highlighting these are guidelines I use to decide whethere I should touch it at all and, if yes, to which extent. By default I try to not fix what is not obviously broken.

Feature should do what it claims to. If it is impossible to make it work, such feature should be excluded. Example: Deep pressure hull.

Feature should not outright contradict or break game concepts. Example of game concept: build your own unit design workshop. Example of the feature directly contradicting this concept: mixed components (resonance armor and such).

Overall game parameters configuration should not create an extremely overpowered or underpowered strategy. Each strategy should be viable at least at some circumstances according to this mod principles. This mod does not intent to "balance" anything but only to adjust some obvious favorite or underdog strategies. Overpowered stategy example: indestructible army.

Being usable also means to come at the right time in research tree. Coming too early clutters game interface and takes space from other more needed features. Coming too lake causes unnecessary slowdown and frustration. This mod adjusts item appearence times to match contemporary development level.

Some items naturally come in sequence when each follower effectively obsolets its predecessor. Good example is weapons and armors. This mod tries to spread them evenly on a research scale so player can enjoy each for about same time. It also tries to link their corresponding technologies to assure they appear in right order. 

Mutually exclusive items/features are adjusted in price and effect so that they can compete with each other and none is completely inferior. Good example is social models.


REARRANGING TECHNOLOGY TREE
==================================================

A lot of above changes require moving items and features up and down technology tree. Apparently, rearranging technology tree is inevitable. This may seem like a big change for users. Therefore I dedicate a whole section to explain my reasons.

First of all, rearranging technology tree is not something unheard of. A lot of mods do it and produce quite playable experience. The trick is in accuracy and placement since handling a dependency tree is non trivial work.

SMACX futuristicly named technologies have no roots in real scientific history except maybe Fusion Power :). This is done, obviously, on purpose to highlight a sci-fi atmosphere. Same story is with other in game concepts, items, and features. Nobody can rationaly explain why technology has such prerequisites or why it allows certain game features. I agree that *some* technology-feature relations make sense but most do not. In this regard I believe fiddling with technology tree is an acceptable modding approach. One could memorize some game concepts after thousands of games, of course but I doubt this is the way to go. Most of the time I fing myself browsing help to understand which technology uncover withc feature. That is completely fine and that is what help is designed for.

I tried to minimize technology tree changes to satisfy my modding needs only and to not get highwire about it. I selected one primary feature for each technology among those it uncovers. Such primary feature is the most memorized and most important technology association. In other words, player usually researches certain technology for its primary feature. Example: Doctrine: Air Power for needlejet chassis. I firmly kept such assisiations. Everything else might change. However, I also tried to keep modified tree as close as possible to vanilla one. Technologies may float but they do not go far from where they were originally. Like Biogenetics is still early game technology same as Advanced Spaceflight is late game one. I also tried to preserve secondary assosiations whenever possible to not mixup things too much.

NOTE TO USERS
I used my own selection of primary feature. If someone believes there should be a different primary association - let me know. I'll gladly substitute. After all, the technology is just a placeholder for features and can be replaced or even renamed as needed.

I think I did good job on linking technologies. Vanilla technology level quite inaccurately predict technology appearance time. My tree is built with exactly 7 technologies per level. Each technology prerequisites are exactly from two below levels. This puts a pretty good timeline and value on a technology which is a great help for technology exchange. You know right away that any level 4 technology is clearly farther up the tree than any level 3 one - no need to look them up in datalink. Same way you understant approximate technology appearance time looking at its datalink description.


DESCRIPTION OF CHANGES
==================================================

Attack/defense
--------------------------------------------------

Overpowered strategy: Indestructible army.
Cost/benefit imbalance: Conquering whole planet for near zero cost investment in army reinforcement.
Fix: Balance attack/defense ratio, combat unit cost, military facility cost so that invader lose units conquering enemy bases and need constant resupply to advance further. From economical point of view invader should incur about 2-3 times bigger loss against PREPARED defense at the same technological level. Hefty price on conquering enemy bases makes non-stop conquering strategy not always a best choice. War related economical stagnation could be a too severe consequence to pay for expansion.

To support the point above weapon and armor rating in this mod go 1:1 until about end of the game. The top weapon is about 20% stronger than top armor. Not that this difference is important at the end game where faction power difference is too much to stop anybody with anything.
Weapon and armor strengths were redone to resemble Civ 1/2 slow exponential progression. Indeed, I cannot imagine why developers would left such weird weapon strength progression as 12-13-16. Whereas it is completely easy to correct it to normal looking one 12-14-16 by just changing text configuration. I've adjusted other weapon strenghts too to make their progression look more exponential as it should be. Each next level item is about 25% stronger.

NOTE TO USERS
Large number of conventional weapon items (12) in the game presents two potential improvement areas.
First, one should experience war conflict in each of 12 first technology levels to enjoy each and every weapon/armor item. This is practically impossible and, therefore, big part of weapon/armor items is inevitable skipped. It does not contradict my principle as each one can still be used over the course of multiple games. Yet, if anyone believe there are too many of them, I can reduce number of weapons to 8 or something.
Second, even with that slow progression or 25% a piece, 12 of them bring strength to 30 for the latest one. That in turn brings their cost to 60+ rows of minerals for best reactor. Such cost increase doesn't seem to be necessary as about half of items are not even get prototyped anyway due to quick research succession (see previous note). Game war element is still enough variative with lesser number of weapon item. After all there are plenty of other war strategy related features: abilities, facitilies, chassis, delivery mechanisms, natives, etc. Reducing number of weapon/armor items does not reduce strategical richness but does reduce unit cost climbing to exorbitant values. Think about it.


Unit cost
--------------------------------------------------

Great news! I've hacked into exe thanks to Induktio and his Thinker mod. That let me fulfil my long time dream of modifying unit cost calculation. Now it is much-Much-MUCH easier to understand unit cost. Both 6-1-1 and 1-6-1 infantry units now cost 6 rows of minerals. Imagine such simplicity! That was not like that in vanilla.
Moreover, it completely removes a quadratic armor cost growth problem. High end mixed inrantry units now cost comparable to speeder and fully armored foil units are comparable to hovertank.

Conventional combat unit cost calculation is redone to resemble Civ 1/2 unit costs. Unit base cost is defined by its primary statistics item cost. Pure infantry unit (attacker or defender) with lowest reactor costs exactly as its primary statistics item. Example: 4 weapon costs 4 => 4-1-1*1 unit costs 4. Same story with modules. Setting colony module cost to 6 makes (no armor) infantry colony to cost exactly 6!
Faster chassis are slightly more expensive. Speeder, foil, needlejet and copter are 1.5 more expensive than infantry. Hovertank and gravship are 2.0 times more expensive than infantry. Mixed units (full weapon + full armor) are 50% more expensive. More advanced items are stronger but their price doesn't grow as fast as their strenght. Therefore, it is generally beneficial to use stronger items. Reactors increase unit cost 80% slower than they increase HP making units 20% more economically effective. Therefore, it is generally beneficial to use stronger reactors.

I appreciate Yitzi's attempt at adding flexibility to abilities cost. However, I don't see much value in it. It is too fine grained and too not visually clear to understand ability value. I've introduced just one additional ability pricing model instead: flat cost. That is for all abilities not improving or not related to weapon/armor values. I believe this is pretty enough.
Conventional combat related abilities naturally improve weapon/armor values - unit fights as if it get stronger weapon/armor. Such abilities correctly increase unit price proportionally. Others have nothing to do with the wearing items like anti native improvements, all non combat improvements, deep radar, etc. I believe such abilities should be priced flat. That allows them to be used on beefed up units as well without paying hand and leg for that. Hypnotic Trance is one example. Why cannot I add it on a high end unit for the flat cost to protect them from enemy worm attacks? After all their worms are flat cost as well!
These two ability cost models can be used together. So one can configure ability to cost a proportion plust a flat cost if desired!

Cheap colony pod allows fueling expansion with nutrients excess only and ignores any economical development whatsoever. Nutrient reach faction keeps stamping colony pods and fills up all available space exponentially. Not surprisingly, such simple strategy is also a most effective way to get economical advantage early in the game. Higher colony pod price put expansion speed in check of both nutrients and minerals production encouraging early terraforming and development. Now player needs to invest into base growth and terraforming in order to expand faster.

Harvesting resources by crawler is a very lucrative investment. Harvesting 4 minerals from rocky mine pays for vanila crawler in 7.5 turns! Then it delivers 4 minerals each turn. That is just insane ROI. I suggest to price it as high as 80 minerals which brings its effectiveness closer to Genejack Factory. Even at this price it is still quite useful but it is not an ultimate and only strategy anymore. You would think thrice if you want to build a crawler just to extract 2 units of production.

Native warfare should be slightly worse to conventional as they have other benefits. They ignore base defensive structures. They are naturally both full scale attacker and defender. Their price is fixed and is much lower comparing to fully equipped top level attacker-defender units. They do not require prototyping. IoD can transport. Sealurk can attack shore units. LoC does not need refueling and can capture bases. They all can repair up to 100% in fungus squares. They do not require maintenance while in fungus square. All together they are no brainer units and as such should be a little bit less effective to not become a superior choice. I've increased most native unit cost except spore launcher. Its damage is pathetic.

Exact cost formula.

unit cost in mineral rows = [primary item true adjusted cost * 2 + secondary item true adjusted cost] / 2 / FissionRC * CC / 2 * (4 + PAC1) / 4 * (4 + PAC2) / 4 + FAC

module true cost = weapon cost * Fission reactor cost factor
weapon/armor true cost = weapon/armor cost * reactor cost factor

primary item = one with higher true cost
secondary item = one with lower true cost

primary item true adjusted cost = primary item true cost
secondary module true adjusted cost = (secondary item cost - 1) * corresponding reactor cost factor

FissionRC = Fission reactor cost factor
CC = chassis cost
PAC = proportional ability cost factor for each ability if any
FAC = flat ability cost for each ability if any

Unit cost is a whole number rounded down. Programmatically all divisions goes after all multiplications to minimize rounding impact.
After all roundings minimal cost is set to Reactor level. This seems to be checked somewhere else in the code. At least I see it complains for Fusion unit to be less than 2.

NOTE TO USERS
This makes cheapest speeder 1-1-2 unit to cost 1 (1.5 rounded down). This doesn't present any problem to me. After all you researched new technology that allows cheaper and faster exploration. However, if anyone 


Weapon and armor value progression
--------------------------------------------------

Vanilla game has very weird code that selects weapon icon based on its offensive value set in text configuration. The configuration line clearly states something like "Particle Impactor,    Impact,         3, ...". The weapon ID and name is passed along the code. Yet there is an explicit (!) code that looks at given weapon and assign it a different (!) icon based on its offensive value. It does not do such strange reassigment for modules or for mixed items like Resonance Laser and Resonance Bolt but for conventional weapons only. Nothing like that for any other item types (armor, chassis, ability). Wery precise, specific and meaningless piece of machinery.
I wasn't able to cancel this completely. However, I found a place where it's done and now I can match values in the code to those in text configuration to make sure proper icons are selected.

With this in mind I was able to correct weapon strenght progression to more smooth one while keeping proper icons for each weapon.
Here is the current game weapon and strength progression by technology levels:
weapon: 1, 2, 3, 4, 5, 6, 8, 10, 13, 16, 20, 24, 30
armor:  1, 2, 3, 4, 5, 6,  , 10,   , 16,   , 24,
There are less armor items in the game so they do not appear every research level at second half of the game. When they appear they match same level weapon strength.


Facility cost
--------------------------------------------------

Energy Bank = 6, Network Node = 6, Research Hospital = 9, Nanohospital = 18
Cost is reduced slightly. They seem to be a pretty interesing facilities those don't get build early because of their price.

Biology Lab = 4
Unattractive facility with fixed income. I don't see much sense in paying a fixed maintenance for fixed income. What, you pay one energy to get two labs - what's the point? In this mod it has no maintenance and cost is slightly reduced. This gives player insentive to build it before Network Node.

Naval Yard = 5
Reduced price to give player insentive to actually build it. It is priced as a combination of morale boosting and defensive facility. However, one usually needs morale boosting benefit at rich unit producing bases and defense benefit at weak distant ones - rarely both at once.

Skunkworks = 20/6
They are usually built just in few highly producing bases. Therefore, cost and maintainance should outweigh its rare use.

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

Sea bases is an amazing and wonderful addition in SMACX comparing to ALL Civ games where ocean is just an obstacle to reach another continent. Balancing sea and land bases is a challenge, though. I like the way it is done now in SMAX. Sea bases are quite poor on minerals but rich on nutrients and energy especially with sea improvement facilities in SMAX. Assuming one build equal number of tidal harnesses and mining platforms base mineral yield is about half of its size before Adv. Eco. Eng. is discovered. This about 2-3 times lower comparing to land bases. However, nutrient excess makes them grow about 1.5-2 times faster than land bases. That patially compensates mineral shortage. Their energy surplus may be seen as additional compensation for low productivity as well as additional contribution to research. Even if that is not enough extra population gives you governorship and fuels faster expansion. Overall, I'd say they are definitely NOT inferior to land bases and probably even better. I always see sea factions or those expanding to sea advancint in overall power.

Aquatic faction is given +1 mineral in shore tiles FOR FREE. This is an insane advantage. Effectively it triples mineral yield going from 0.5 of population to 1.5 of population. Sea terraforming is faster and more beneficial. Only some land tiles give you 3 nutrients or 3 energy. Sea squares are all like that. When land base growths to size 4 with about 5-7 mineral yield, aquatic sea base is already at size 7 producing +7 free minerals on top of their worker yield just because of the base size. If we count regular sea bases at least equal to land ones than aquatic sea bases are at least 2-3 times as better. Pirates AI always comes first or second in early game even with yield restrictions in place. They start booming exponentially when restriction are lifted. Something need to be done about it.

Unfortunately, I cannot take their bonus yet. My solution is to give them -3 SUPPORT. This sucks some amount of minerals bringing them close to regular sea base. Even with this setting their bases are still slightly better as they keep outgrowing the support hurdle. However, their land bases would suffer more should they go there. So, I guess, this is more or less fine.
I figured -4 SUPPORT would be to harsh on them from the beginning. They are still impacted by this threat, though, and should chose any -1 SUPPORT social choices wisely.


Forest
--------------------------------------------------

Many people before me mentioned overpowed forest. Indeed, it is capable of turning dry and barren 0-0-0 terrain into 1-2-1 adding 4 resourses in 4 turns. Its yield is comparable to the rocky mine. It spreads by itself. It has defensive bonus. It is pretty nice option for poor bases but it should cost more than mine to be not the-only-viable-option. In different versions of the game I tried these options: 1) set its terraforming time to 12, 2) set its terraforming time to 8 and reduce its production to 1-2-0. Second option appeals to me more. I believe forest is the always available option for mineral boost. With that in mind it should not provide any additional energy for free. You chose to have either cheap minerals or energy.


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

Some vanilla technologies boost probe team morale for free. That makes all probe teams elite toward the end of the game. Err, what's the point in that if everybody gets it and this renders Covert Ops and PROBE ratings useless for probe team morale? I removed all such flags. Technologies do not improve probe team morale any more.


