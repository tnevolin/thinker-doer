# Probe actions success probability

Old thread on AC2: https://web.archive.org/web/20230912072952/https://alphacentauri2.info/index.php?topic=2761.0

## Sequence of steps

1. Probe attempts to carry out the mission.
   * If unsuccessful, action is not performed and probe is eliminated.
   * If successful, action is performed and next step follows.
2. Probe attempts to escape after successfully completed mission.
   * If unsuccessful, probe is eliminated.
   * If successful, probe is returned to the nearest friendly base and promoted.

## Success factors

Probe success probability depends on following factors:

* Action risk level
* Probe current morale
* Target faction negative PROBE rating
* Target base Covert Ops Center facility existence

## Risks

| action | RISK |
| ---- | ----: |
| Infiltrate Datalinks | 0 |
| Procure Research Data - first time from a base | 0 |
| Procure Research Data - after first time from a base | 1 |
| Activate Sabotage Virus - random | 0 |
| Activate Sabotage Virus - specific | 1 |
| Activate Sabotage Virus - defensive facility | 2 |
| Activate Sabotage Virus - any facility in HQ | 2 |
| Drain Energy Reserves | 0 |
| Insite Drone Riots | 0 |
| Assassinate Prominent Researchers | 1 |
| Introduce Genetic Plague | 0 |

Attempting to frame other faction increases risk by 1. If such attempt fails, diplomatic consequences may follow with framed faction.

RISK 3 is only possible when framing other faction for RISK 2 action.

## Probe morale

Probe current morale is combined from vehicle base morale plus variable bonuses.
Any probe morale computation below is limited to range: Disciplined - Elite.

```
morale = base morale + [positive PROBE] + [2 if faction possesses The Telepathic Matrix]
```

## Probability formula

Probability is computed comparing probe effectiveness to risk level.

### Action

```
probe effectiveness = morale / 2
+1 if target faction PROBE is -1 and target base DOES NOT have COC
+2 if target faction PROBE is -2 and target base DOES NOT have COC
```

```
success probability = (effectiveness + 1 - RISK) / (effectiveness + 1)
```

### Escape

```
probe effectiveness = morale
+1 if target faction PROBE is -1 and target base DOES NOT have COC
+2 if target faction PROBE is -2 and target base DOES NOT have COC
```

```
success probability = (effectiveness - RISK) / (effectiveness + 1)
```

## The Hunter-Seeker Algorithm and Algorithmically Enhanced probe effects

| HSA | AE | effect |
| :----: | :----: | ---- |
|  |  |  |
| + |  | probing is impossible |
|  | + | chance of failure is halved |
| + | + | chance of success is halved |

## Example probability computation

### Normal probe

#### action

| morale | RISK0 | RISK1 | RISK2 | RISK3 |
| ---- | ----: | ----: | ----: | ----: |
| Disciplined<br>Hardened | 100% | 50% |  0% |  0% |
| Veteran<br>Commando     | 100% | 67% | 33% |  0% |
| Elite                   | 100% | 75% | 50% | 25% |

#### escape

| morale | RISK0 | RISK1 | RISK2 | RISK3 |
| ---- | ----: | ----: | ----: | ----: |
| Disciplined | 67% | 33% | 0% | 0% |
| Hardened    | 75% | 50% | 25% | 0% |
| Veteran     | 80% | 60% | 40% | 20% |
| Commando    | 83% | 67% | 50% | 33% |
| Elite       | 86% | 71% | 57% | 43% |

# Base submersion

When sea level rises some shore tiles may go under water. If such tile has a base on it and the base **does not** have Pressure Dome, it will sustain severe population loss.

1. Minimal population loss is the random number in 2-4 range (inclusive).
2. Maximal population loss is 10.
3. Resulting population loss is half of the current population rounded down but between minimum and maximum above.

If base loses all of its population it is lost. If base have some population left, it survives and acquires Pressure Dome.

#### Base surviving submerging chances by population

| population | chance |
| ----: | ----: |
| <= 2 | 0% |
| 3 | 33% |
| 4 | 67% |
| >= 5 | 100% |

# Riot intesifying

When base is rioting for more than one turn the "riot intensifying" popup is shown for the base.

#### Exclusions

* Nothing ever happens to headquarter base.
* Nothing ever hanppes to captured base for the first **five** turns.

#### Events and conditions

Conditions are checked in following order.

1. If POLICE >= -5, there is a `(POLICE + 6) / (POLICE + 7)` random chance that nothing happens. Otherwise, go to next step.
2. If populatioin < 4, there is a `1/3` random chance that nothing happes. Otherwise, go to next step.
3. Game makes `population + 20 - difficulty - ranking` attempts to destroy a building with random ID.
   * Headquarter cannot be destroyed.
   * Faction bonus facilities cannot be destroyed.
   * If given random ID buiding does not exist in this base - go to next loop.
4. If game can successfully find a building to destroy, it destroys it and nothing else happens. Otherwise, it goes to next step.
5. If POLICE >= -1 (at least one police unit allowed), nothing happens. Otherwise, it goes to next step.
6. If faction is alien, nothing happens. Otherwise, it goes to next step.
7. If faction is AI (not human), nothing happens. Otherwise, it goes to next step. Damn! AI bases **never** defect.
8. The rest of the code is broken. If one of your bases is ever tries to defect, the game will likely crash. Supposedly, there is an equal chance to defect to any faction with bonus to drones.

