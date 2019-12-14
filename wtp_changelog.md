Version 21
==================================================

TODO
--------------------------------------------------

Add off switches for all WtP functionalities. Not sure if combined or separately.

Modify weapon icon selection to be NOT dependent on the value.

Remove minerals restriction on hurry cost.
Set unit hurry cost to purchased minerals x 4.
Set unit upgrade cost to hurry cost difference.

First technology should be discoverable in 6-10 turns.
Technology cost should not rise more than 10%
Technology cost map size multiplier should be proportional to number of map squares.

Combat
--------------------------------------------------

Conventional combat now ignores reactor power same way as psi combat does.

Odds calculation now ignore reactor power for both conventional and psi combat.
For psi combat it was actually a bug that nobody ever fixed yet.


Unit cost
--------------------------------------------------

Each higher reactors now decrease unit cost approximately by 20% comparing to predecessor.
Chaged weapon and armor cost so it increases at about 50-60% rate of the item value. This way unit cost still growth but slower than strength.


Rules
--------------------------------------------------

Returned back to original values
50,      ; Extra percentage cost of prototype LAND unit
50,      ; Extra percentage cost of prototype SEA unit
50,      ; Extra percentage cost of prototype AIR unit


Facilities
--------------------------------------------------

Returned back to original cost/maintenance
Skunkworks,                    6, 1, Subat,   Disable,  Prototypes Free


Factions
--------------------------------------------------

Added TECH, Mobile - protection from natives
#MORGAN
  TECH, Indust, TECH, Mobile, ENERGY, 100, SOCIAL, +ECONOMY, POPULATION, 3, COMMERCE, 1, SOCIAL, -SUPPORT, SOCIAL, -POLICE

Added TECH, PrPsych - Biologic Lab for more research
#UNIV
  TECH, Physic, TECH, PrPsych, SOCIAL, ++RESEARCH, SOCIAL, --PROBE, SOCIAL, -POLICE, DRONE, 4, FACILITY, 8

Added TECH, Physic - first level weapon
#SPARTANS
  TECH, Mobile, TECH, Physic, FREEPROTO, 0, SOCIAL, ++MORALE, SOCIAL, -INDUSTRY

Added TECH, Psych - Recreation Commons to cope with -2 POLICE
#GAIANS
  TECH, Ecology, TECH, Psych, SOCIAL, -MORALE, SOCIAL, --POLICE, SOCIAL, +EFFIC, SOCIAL, +PLANET, FUNGNUTRIENT, 1

Added TECH, Ecology - they are slow on research and need terraforming
Lowered RESEARCH to -2 and raised POLICE to 0 - they have pretty harsh research penalty already so I let them easy on police
#BELIEVE
  TECH, Psych, TECH, Ecology, SOCIAL, --RESEARCH, SOCIAL, +PROBE, SOCIAL, +SUPPORT, SOCIAL, -PLANET, FANATIC, 0

Added TECH, DocFlex
#HIVE
  TECH, Psych, TECH, DocFlex, FACILITY, 4, SOCIAL, +GROWTH, SOCIAL, +INDUSTRY, SOCIAL, --ECONOMY, SOCIAL, -POLICE, IMMUNITY, EFFIC

Added TECH, Indust
#PEACE
  TECH, DocFlex, TECH, Indust, TALENT, 4, SOCIAL, -EFFIC, SOCIAL, -POLICE, POPULATION, -2, VOTES, 2

Added TECH, Ecology
#CYBORG
  TECH, Physic, TECH, Ecology, IMPUNITY, Cybernetic, SOCIAL, +RESEARCH, SOCIAL, +EFFIC, SOCIAL, -GROWTH, SOCIAL, -POLICE, TECHSTEAL, 0

Added TECH, PrPsych - Biologic Lab to compensate research penalty
#DRONE
  TECH, Indust, TECH, PrPsych, SOCIAL, ++INDUSTRY, SOCIAL, --RESEARCH, SOCIAL, -POLICE, NODRONE, 1, REVOLT, 75

Added TECH, Physic - didn't know who else needs it they wouldn't hert anybody early in the ocean anyway
#PIRATES
  TECH, DocFlex, TECH, Physic, SOCIAL, -GROWTH, SOCIAL, -EFFIC, SOCIAL, ---SUPPORT, SOCIAL, -POLICE, FREEFAC, 28, AQUATIC, 0, FREEABIL, 26

Added TECH, DocFlex
#FUNGBOY
  TECH, Ecology, TECH, DocFlex, WORMPOLICE, 0, SOCIAL, ++PLANET, SOCIAL, -INDUSTRY, SOCIAL, -ECONOMY, SOCIAL, -POLICE, FREEFAC, 35, UNIT, 8

Added TECH, Psych - Recreation Commons to cope with -2 POLICE
#ANGELS
  TECH, Mobile, TECH, Psych, SOCIAL, ++PROBE, SOCIAL, --POLICE, PROBECOST, 75, FREEFAC, 34, SHARETECH, 3, UNIT, 6

Added TECH, Indust - emphasis on defense
#CARETAKE
  TECH, PrPsych, TECH, Indust, ALIEN, 0, DEFENSE, 125, SOCIAL, +PLANET, SOCIAL, -INDUSTRY, SOCIAL, -POLICE

Added TECH, Mobile
#USURPER
  TECH, PrPsych, TECH, Mobile, ALIEN, 0, OFFENSE, 125, SOCIAL, +GROWTH, SOCIAL, -PLANET, SOCIAL, -INDUSTRY, SOCIAL, -POLICE


Version 20
==================================================

Technology changes
--------------------------------------------------

No technology has this anymore
;            000000001 = "Secrets": first discoverer gains free tech

No technology has this anymore
;            000000010 = Improves Probe Team success rate

Fungus production technologies
Centauri Ecology			+1n
Progenitor Psych			+1m - reassigned
Field Modulation			+1e - reassigned
Bioadaptive Resonance		+1e - reassigned
Centauri Psi				+1n
Centauri Meditation			+1e
Secrets of Alpha Centauri	+1e
Centauri Genetics			+1m

Prerequisites
Look into supplied alpahx.txt or browse datalink in game. I don't have graphic tree.


Technology feature association changes
--------------------------------------------------

Optical Computers			->	Probe teams
Applied Gravitonics			->	Missile chassis
Information Networks		->	Comm jammer
Bioadaptive Resonance		->	Empath song
Frictionless Surfaces		->	Dissociative wave
Secrets of Alpha Centauri	->	Non-lethal methods
Field Modulation			->	Aquafarm
Organic Superlubricant		->	Subsea Trunkline
The Will to Power			->	Tachyon field
Polymorphic Software		->	Hologram theatre
Secrets of the Manifolds	->	Subspace Generator
Progenitor Psych			->	Biology lab
Nanometallurgy				->	Fusion lab
Singularity Mechanics		->	Quantum lab
Temporal Mechanics			->	Nanoreplicator
Secrets of the Manifolds	->	Nessus mining station
Nonlinear Mathematics		->	The Universal Translator
Superconductor				->	The Living Refinery
Gene Splicing				->	The Ascetic Virtues
Environmental Economics		->	The Manifold Harmonics
Advanced Subatomic Theory	->	The Planetary Energy Grid
Homo Superior				->	The Nethack Terminus
The Will to Power			->	The Cloudbase Academy
Secrets of Creation			->	The Clinical Immortality
Graviton Theory				->	The Cyborg Factory
Secrets of the Manifolds	->	The Telepathic Matrix
Sentient Resonance			->	The Space Elevator
Adaptive Doctrine			->	Plasma shard
Silksteel Alloys			->	Planned
Intellectual Integrity		->	Wealth
Quantum Power				->	Thought Control
Matter Compression			->	Cybernetic
Adaptive Economics			->	Global Trade Pact, Repeal Global Trade Pact
Probability Mechanics		->	Economic victory
Applied Relativity			->	Salvage Unity Fusion Core
Super Tensile Solids		->	Melt Polar Caps
Planetary Economics			->	Launch Solar Shade, Increase Solar Shade
Superstring Theory			->	Engineer
Monopole Magnets			->	free Recycling Tanks
Matter Editation			->	free Recreation Commons
Sentient Econometrics		->	free Energy Bank
Secrets of Creation			->	free Network Node


Rule changes
--------------------------------------------------

 50,     ; Technology discovery rate as a percentage of standard
100,     ; Extra percentage cost of prototype LAND unit
100,     ; Extra percentage cost of prototype SEA unit
100,     ; Extra percentage cost of prototype AIR unit
50,      ; Players' starting energy reserves
50,      ; Combat % -> intrinsic base defense
50,      ; Combat % -> Mobile unit in open ground
0,       ; Combat % -> Infantry vs. Base
50,      ; Combat % -> Fanatic attack bonus
0,       ; Combat penalty % -> Non-combat unit defending vs. combat unit
0,       ; Combat % -> Bonus vs. ships caught in port
50,      ; Combat % -> Defend in range of friendly Sensor
10,      ; Minimum # of turns between councils
Ecology, ; Technology to allow 3 nutrients in a square
Ecology, ; Technology to allow 3 minerals in a square
Ecology, ; Technology to allow 3 energy in a square
ProbMec, ; Technology for economic victory
2, 3     ; Numerator/Denominator for frequency of global warming (1,2 would be "half" normal warming).
Forest,           None,    ...,              Disable, 12,  Plant $STR0,     F, Shift+F
Bunker,           Disable, Bunker,           Disable,  5,  Construct $STR0, K, K
Raise Land,       EcoEng2, Raise Sea Floor,  EcoEng2, 12,  Terraform UP,    ], ]]
Lower Land,       EcoEng2, Lower Sea Floor,  EcoEng2, 12,  Terraform DOWN,  [, [[


Chassis cost / 2 is a factor for unit cost. Missile prerequisite changed.
#CHASSIS
Infantry,M1,  Squad,M1,      Sentinels,M2,   Garrison,M1,  1, 0, 0, 0, 1, 2, None,     Shock Troops,M2,   Elite Guard,M1,
Speeder,M1,   Rover,M1,      Defensive,M1,   Skirmisher,M1,2, 0, 0, 0, 1, 3, Mobile,   Dragon,M1,         Enforcer,M1,
Hovertank,M1, Tank,M1,       Skimmer,M1,     Evasive,M1,   3, 0, 0, 0, 1, 4, NanoMin,  Behemoth,M1,       Guardian,M1,
Foil,M1,      Skimship,M1,   Hoverboat,M1,   Coastal,M1,   4, 1, 0, 0, 2, 3, DocFlex,  Megafoil,M1,       Superfoil,M1,
Cruiser,M1,   Destroyer,M1,  Cutter,M1,      Gunboat,M1,   6, 1, 0, 0, 4, 4, DocInit,  Battleship,M1,     Monitor,M1,
Needlejet,M1, Penetrator,M1, Interceptor,M1, Tactical,M1,  8, 2, 2, 0, 1, 3, DocAir,   Thunderbolt,M1,    Sovereign,M1,
Copter,M1,    Chopper,M1,    Rotor,M1,       Lifter,M1,    8, 2, 1, 0, 1, 3, MindMac,  Gunship,M1,        Warbird,M1,
Gravship,M1,  Skybase,M1,    Antigrav,M1,    Skyfort,M1,   8, 2, 0, 0, 1, 4, Gravity,  Deathsphere,M1,    Doomwall,M1,
Missile,M1,   Missile,M1,    Missile,M1,     Missile,M1,  12, 2, 1, 1, 0, 5, AGrav,    Missile,M1,        Missile,M1,

Power field was not used by program. Now I use it as a unit cost factor.
#REACTORS
Fission Plant,        Fission,    10, None,
Fusion Reactor,       Fusion,     16, Fusion,
Quantum Chamber,      Quantum,    19, Quantum,
Singularity Engine,   Singularity,20, SingMec,

Many parameters changed - value, cost, prerequisite. All hybrid and psi items are disabled.
#WEAPONS
Hand Weapons,         Gun,            1, 0, 1, -1, None,
Laser,                Laser,          2, 0, 2, -1, Physic,
Particle Impactor,    Impact,         3, 0, 3, -1, Chaos,
Gatling Laser,        Gatling,        4, 1, 4, -1, Super,
Missile Launcher,     Missile,        5, 2, 5, -1, Fossil,
Chaos Gun,            Chaos,          6, 0, 6, -1, String,
Fusion Laser,         Fusion,         8, 1, 7, -1, SupLube,
Tachyon Bolt,         Tachyon,       10, 1, 8, -1, Unified,
Plasma Shard,         Shard,         13, 2, 9, -1, AdapDoc,
Quantum Laser,        Quantum,       16, 1,10, -1, QuanMac,
Graviton Gun,         Graviton,      20, 0,12, -1, AGrav,
Singularity Laser,    Singularity,   24, 1,14, -1, ConSing,
Resonance Laser,      R-Laser,        6, 1,11, -1, Disable,
Resonance Bolt,       R-Bolt,        12, 1,22, -1, Disable,
String Disruptor,     String,        30, 1,16, -1, BFG9000,
Psi Attack,           Psi,           -1, 2,10, -1, Disable,
Planet Buster,        Planet Buster, 99, 0,32, -1, Orbital,
Colony Module,        Colony Pod,     0, 8, 6, -1, None,     ; Noncombat packages
Terraforming Unit,    Formers,        0, 9, 4, -1, Ecology,
Troop Transport,      Transport,      0, 7, 4, -1, DocFlex,
Supply Transport,     Supply,         0,10, 8, -1, IndAuto,
Probe Team,           Probe Team,     0,11, 4, -1, OptComp,
Alien Artifact,       Artifact,       0,12,36, -1, Disable,
Conventional Payload, Conventional,  12, 0,12, -1, Orbital,
Tectonic Payload,     Tectonic,       0,13,24, -1, NewMiss
Fungal Payload,       Fungal,         0,14,24, -1, NewMiss

Many parameters changed - value, cost, prerequisite. All hybrid and psi items are disabled.
#DEFENSES
No Armor,            Scout,       1, 0, 1, None,
Synthmetal Armor,    Synthmetal,  2, 0, 2, Indust,
Plasma Steel Armor,  Plasma,      3, 2, 3, Chemist,
Silksteel Armor,     Silksteel,   4, 1, 4, Alloys,
Photon Wall,         Photon,      5, 1, 5, DocSec,
Probability Sheath,  Probability, 6, 2, 6, ProbMec,
Neutronium Armor,    Neutronium, 10, 1, 8, MatComp,
Antimatter Plate,    Antimatter, 16, 2,10, NanEdit,
Stasis Generator,    Stasis,     24, 2,14, TempMec,
Psi Defense,         Psi,        -1, 2, 6, Disable,
Pulse 3 Armor,       3-Pulse,     3, 1, 6, Disable,
Resonance 3 Armor,   3-Res,       3, 1, 5, Disable,
Pulse 8 Armor,       8-Pulse,     8, 1,11, Disable,
Resonance 8 Armor,   8-Res,       8, 1,10, Disable,

Abilities bytes 0-3 is cost multiplier, bytes 4-7 is flat addition.
Some prerequisites changed.
Some broken abilities are disabled.
#ABILITIES
Super Former,          32, EcoEng2,  Super,     000000010111, Terraform rate doubled
Deep Radar,             0, MilAlg,   ,          010000111111, Sees 2 spaces
Cloaking Device,        1, Disable,  Cloaked,   000001111001, Invisible; Ignores ZOCs
Amphibious Pods,        1, DocInit,  Amphibious,000000001001, Attacks from ship
Drop Pods,              2, MindMac,  Drop,      000000111001, Makes air drops
Air Superiority,        1, DocAir,   SAM,       000000001111, Attacks air units
Deep Pressure Hull,     1, Disable,  Sub,       000000111010, Operates underwater
Carrier Deck,           1, Metal,    Carrier,   000101101010, Mobile Airbase
AAA Tracking,           1, MilAlg,   AAA,       000010001011, x2 vs. air attacks
Comm Jammer,            1, InfNet,   ECM,       000010111001, +50% vs. fast units
Antigrav Struts,        2, Gravity,  Grav,      000000111001, +1 movement rate (or +Reactor*2 for Air)
Empath Song,           64, Bioadap,  Empath,    000010001111, +50% attack vs. Psi
Polymorphic Encryption, 1, Algor,    Secure,    000000111111, x2 cost to subvert
Fungicide Tanks,       16, Fossil,   Fungicidal,000000010111, Clear fungus at double speed
High Morale,            1, Disable,  Trained,   000000001111, Gains morale upgrade
Heavy Artillery,        1, Poly,     Artillery, 000010001001, Bombards
Clean Reactor,         64, BioEng,   Clean,     000000111111, Requires no support
Blink Displacer,        4, Matter,   Blink,     000000001111, Bypass base defenses
Hypnotic Trance,       32, Brain,    Trance,    000010111111, +50% defense vs. Psi
Heavy Transport,        1, Disable,  Heavy,     000100100111, +50% transport capacity
Nerve Gas Pods,         1, Chemist,  X,         000011001101, Can +50% offense (Atrocity)
Repair Bay,             1, Disable,  Repair,    000100100111, Repairs ground units on board
Non-Lethal Methods,    64, AlphCen,  Police,    000000001001, x2 Police powers
Slow Unit,              0, Disable,  Slow,      000000111111, -1 moves
Soporific Gas Pods,     2, Disable,  Gas,       000001001101, -2 Enemy morale vs. non-native
Dissociative Wave,      2, Surface,  Wave,      000000111111, Fizzles special abilities
Marine Detachment,      1, SupLube,  Marine,    000001001010, Capture enemy ships
Fuel Nanocells,         1, MatComp,  Nanocell,  000000111100, Increased air range
Algorithmic Enhancement,4, NanoMin,  Enhanced,  100000111111, Halves probe team failure

Renamed vanilla Probe Team to Speeder Probe Team to differentiate it from Infantry Probe Team.
Probe Team prerequisite changed to OptComp.
Added few more predesigned unit to aid AI and player a little.
#UNITS
28
Colony Pod,             Infantry, Colony Pod,   Scout,      8, 0, 0, None,    -1, 00000000000000000000000000
Formers,                Infantry, Formers,      Scout,      9, 0, 0, Ecology, -1, 00000000000000000000000000
Scout Patrol,           Infantry, Gun,          Scout,      3, 0, 0, None,    -1, 00000000000000000000000000
Transport Foil,         Foil,     Transport,    Scout,      7, 0, 0, DocFlex, -1, 00000000000000000000000000
*Sea Formers,           Foil,     Formers,      Scout,      9, 0, 0, Disable, -1, 00000000000000000000000000
Supply Crawler,         Infantry, Supply,       Scout,     10, 0, 0, IndAuto, -1, 00000000000000000000000000
Speeder Probe Team,     Speeder,  Probe Team,   Scout,     11, 0, 0, OptComp, -1, 00000000000000000000000000
Alien Artifact,         Infantry, Artifact,     Scout,     12,10, 0, Disable,  2, 00000000000000000000000000
Mind Worms,             Infantry, Psi,          Psi,        1, 8, 0, CentEmp,  3, 00000000000000000000000000
Isle of the Deep,       Foil,     Psi,          Psi,        7,12, 4, CentMed, -1, 00000000000000000000000000
Locusts of Chiron,      Gravship, Psi,          Psi,        4,16, 0, CentGen, -1, 00000000000000000000100000
Unity Rover,            Speeder,  Gun,          Scout,      3, 0, 0, Disable, -1, 00000000000000000000000000
Unity Scout Chopper,    Copter,   Gun,          Scout,      4, 0, 0, Disable, -1, 00000000000000000000100000
Unity Foil,             Foil,     Transport,    Scout,      7, 0, 0, Disable, -1, 00100000000000000000000000
Sealurk,                Foil,     Psi,          Psi,        6, 8, 0, CentPsi,  4, 00000000000000000001000000
Spore Launcher,         Infantry, Psi,          Psi,        0, 4, 0, Bioadap,  5, 00000000001000000000000000
Battle Ogre MK1,        Infantry, R-Laser,      3-Res,      0,10, 0, Disable,  6, 00010000000000000000000000
Battle Ogre MK2,        Infantry, R-Bolt,       8-Res,      0,15, 0, Disable,  6, 10010000000000000000000000
Battle Ogre MK3,        Speeder,  String,       Stasis,     0,20, 0, Disable,  6, 10010000000000000000000000
Fungal Tower,           Infantry, Psi,          Psi,        3, 0, 0, Disable,  1, 00000000000000000000000000
Unity Mining Laser,     Infantry, Laser,        Scout,      0, 0, 0, Disable, -1, 00000000000000000000000000
Sea Escape Pod,         Foil,     Colony Pod,   Scout,      8, 0, 0, Disable, -1, 00000000000000000000000000
Unity Gunship,          Foil,     Gun,          Scout,     -1, 0, 0, Disable, -1, 00000000000000000000000000
Infantry Probe Team,    Infantry, Probe Team,   Scout,     11, 0, 0, OptComp, -1, 00000000000000000000000000
Foil Probe Team,        Foil,     Probe Team,   Scout,     11, 0, 0, OptComp, -1, 00000000000000000000000000
Supply Trawler,         Foil,     Supply,       Scout,     10, 0, 0, IndAuto, -1, 00000000000000000000000000
Rover Colony Pod,       Speeder,  Colony Pod,   Scout,      8, 0, 0, Mobile,  -1, 00000000000000000000000000
Sea Colony Pod,         Foil,     Colony Pod,   Scout,      8, 0, 0, DocFlex, -1, 00000000000000000000000000

Facilities cost and maintenance changes
#FACILITIES
Energy Bank,                   6, 1, IndEcon, SentEco,  Economy Bonus
Network Node,                  6, 1, InfNet,  Create,   Labs Bonus
Biology Lab,                   4, 0, PrPsych, Disable,  Research and PSI
Skunkworks,                   20, 6, Subat,   Disable,  Prototypes Free
Research Hospital,             9, 3, Gene,    Disable,  Labs and Psych Bonus
Nanohospital,                 18, 4, HomoSup, Disable,  Labs and Psych Bonus
Naval Yard,                    5, 1, DocInit, Disable,  +2 Morale:Sea, Sea Def +100%
Bioenhancement Center,        16, 4, Neural,  Disable,  +2 Morale:ALL
Aquafarm,                     12, 2, FldMod,  Disable,  Sea Nutrients
Geosynchronous Survey Pod,    16, 2, NewMiss, Disable,  Increase sight\sensor bonus

Secret projects
Almost all SP cost changed. See complete list in README.

Technicial is obsoleted and Engineer is enabled by String.
#CITIZENS
Technician,       Technicians,         None,    String,  3, 0, 0, 0000000
Engineer,         Engineers,           String,  Disable, 3, 0, 2, 0000000

Society models changed signigicantly
#SOCIO
Frontier,        None,
Police State,    DocLoy,    -EFFIC,     ++SUPPORT,   ++POLICE,    --PLANET
Democratic,      EthCalc,   +EFFIC,     --SUPPORT,    +GROWTH,     -PROBE
Fundamentalist,  Brain,     +MORALE,     +PROBE,      +INDUSTRY,  --RESEARCH,   +TALENT
Simple,          None,
Free Market,     IndEcon,  ++ECONOMY,   --POLICE,    --PLANET,     -PROBE
Planned,         Alloys,    -EFFIC,      +GROWTH,     +INDUSTRY,   -RESEARCH,   +TALENT
Green,           CentEmp,  ++EFFIC,    ---GROWTH,    ++PLANET,    --INDUSTRY
Survival,        None,
Power,           MilAlg,   ++SUPPORT,   ++MORALE,     +PROBE,     --INDUSTRY
Knowledge,       Cyber,     -ECONOMY,    +EFFIC,     --PROBE,    +++RESEARCH
Wealth,          Integ,     +ECONOMY,   --MORALE,     -POLICE,     +INDUSTRY
None,            None,
Cybernetic,      MatComp,   +EFFIC,    ---POLICE,    ++PLANET,     +RESEARCH
Eudaimonic,      Eudaim,   ++ECONOMY,    -EFFIC,    ---MORALE,     +INDUSTRY
Thought Control, Quantum, ---SUPPORT,   ++MORALE,    ++POLICE,    ++PROBE,       +TALENT


Faction changes
--------------------------------------------------

All factions now have only one level 1 predesigned technology at start. 7 level one technologies are distributed among 14 original SMACX factions evenly to increase technology trading potential at start in random games.

All factions have POLICE decreased by 1. I.e. +1 POLICE becomes 0 POLICE, 0 POLICE becomes -1 POLICE, etc.

Alien do not have default technologies anymore. You need to assign them technologies in a normal way.

#MORGAN
  TECH, Indust, ENERGY, 100, SOCIAL, +ECONOMY, SOCIAL, -POLICE, POPULATION, 3, COMMERCE, 1, SOCIAL, -SUPPORT

#UNIV
  TECH, Physic, SOCIAL, ++RESEARCH, SOCIAL, --PROBE, DRONE, 4, FACILITY, 8, SOCIAL, -POLICE

#SPARTANS
  TECH, Mobile, FREEPROTO, 0, SOCIAL, ++MORALE, SOCIAL, -INDUSTRY

#GAIANS
  TECH, Ecology, SOCIAL, -MORALE, SOCIAL, --POLICE, SOCIAL, +EFFIC, SOCIAL, +PLANET, FUNGNUTRIENT, 1

#BELIEVE
  SOCIAL, -RESEARCH, SOCIAL, +PROBE, SOCIAL, +SUPPORT, SOCIAL, -PLANET, SOCIAL, -POLICE, FANATIC, 0

#HIVE
  TECH, Psych, FACILITY, 4, SOCIAL, +GROWTH, SOCIAL, +INDUSTRY, SOCIAL, --ECONOMY, SOCIAL, -POLICE, IMMUNITY, EFFIC

#PEACE
  TECH, DocFlex, TALENT, 4, SOCIAL, -EFFIC, SOCIAL, -POLICE, POPULATION, -2, VOTES, 2

#CYBORG
  TECH, Physic, IMPUNITY, Cybernetic, SOCIAL, +RESEARCH, SOCIAL, +EFFIC, SOCIAL, -GROWTH, SOCIAL, -POLICE, TECHSTEAL, 0

#DRONE
  TECH, Indust, SOCIAL, ++INDUSTRY, SOCIAL, --RESEARCH, SOCIAL, -POLICE, NODRONE, 1, REVOLT, 75

#PIRATES
  TECH, DocFlex, SOCIAL, -GROWTH, SOCIAL, -EFFIC, SOCIAL, ---SUPPORT, SOCIAL, -POLICE, FREEFAC, 28, AQUATIC, 0, FREEABIL, 26

#FUNGBOY
  TECH, Ecology, WORMPOLICE, 0, SOCIAL, ++PLANET, SOCIAL, -INDUSTRY, SOCIAL, -ECONOMY, SOCIAL, -POLICE, FREEFAC, 35, UNIT, 8

#ANGELS
  TECH, Mobile, SOCIAL, ++PROBE, SOCIAL, --POLICE, PROBECOST, 75, FREEFAC, 34, SHARETECH, 3, UNIT, 6

#CARETAKE
  TECH, PrPsych, ALIEN, 0, DEFENSE, 125, SOCIAL, +PLANET, SOCIAL, -INDUSTRY, SOCIAL, -POLICE

#USURPER
  TECH, PrPsych, ALIEN, 0, OFFENSE, 125, SOCIAL, +GROWTH, SOCIAL, -PLANET, SOCIAL, -INDUSTRY, SOCIAL, -POLICE

