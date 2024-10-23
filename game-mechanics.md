# Probe actions success probability

## Sequence of steps

1. Probe attempts to carry out the mission.
   * If unsuccessful, action is not performed and probe is eliminated.
   * If successful, action is performed and next step follows.
2. Probe attempts to escape after successfully completed mission.
   a. If unsuccessful, probe is eliminated.
   b. If successful, probe is returned to the nearest friendly base and promoted.

## Success factors

Probe success probability depends on following factors:

* Action risk level
* Probe current morale
* Target faction negative PROBE rating
* Target base Covert Ops Center facility existence

## Risks

// TODO list of risks

Attempting to frame other faction increases risk by 1. If such attempt fails, diplomatic consequences may follow with framed faction.

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

