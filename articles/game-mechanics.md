# Probe actions success probability

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

