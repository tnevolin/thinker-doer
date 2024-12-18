# Original vanilla analysis article

https://alphacentauri.miraheze.org/wiki/Social_Engineering_Mod

https://web.archive.org/web/20180810045412/http://alphacentauri2.info/wiki/Social_Engineering_Mod

The SE model proposed in article above is for vanilla. WTP uses a different one since it uses different rules and, therefore, SE effects are somewhat differrently weighted comparing to each other. However, ideas about balancing and different effect comparison methods are still generally valid.

# Programmatic emulation

I have created a program that emulates different factions and SE combinations and values models based on average output across different historical periods. Periods statistics is collected from random player games.

## Periods

| turn | base count | base size | base minerals | base energy |
| ----: | ----: | ----: | ----: | ----: |
| 50 |  5 | 2 |  4 |  3 |
| 50 | 15 | 3 |  6 |  8 |
| 50 | 25 | 4 |  8 | 13 |
| 50 | 35 | 5 | 10 | 18 |

## Evaluation method

* Model value is the average contribution this model brings to the table when it is used.
* Model chance is the estimate on how often this model is going to be part of the most lucrative SE mix comparing to other models in same category.

## Effects

| effect | average range | when valued most |
| ---- | ---- | ---- |
| ECONOMY | 0.5 - 1.5 | low worker yield |
| EFFICIENCY | 0.0 - 2.0 | large empire |
| SUPPORT | 0.5 - 1.5 | many units per base |
| MORALE | 0.5 - 1.0 | conventional combat |
| POLICE | 0.0 - 2.0 | many drones |
| GROWTH | 1.5 - 2.0 | early-mid game |
| PLANET | 0.5 - 1.0 | psi combat, eco damage |
| PROBE | 0.5 - 1.0 | info warfare |
| INDUSTRY | 1.0 - 1.0 |  |
| RESEARCH | 0.5 - 0.5 |  |
| TALENT | 0.0 - 1.0 | many drones |

## Vanilla

```
                         value    ---1  ---2  ---3  ---4     chance    ---1  ---2  ---3  ---4
Politics
    Police State          0.71    0.82  0.24  0.84  0.94       0.29    0.27  0.06  0.39  0.45
    Democratic            1.30    0.44  1.62  1.64  1.47       0.49    0.13  0.75  0.56  0.52
    Fundamentalist        0.64    1.19  0.58  0.42  0.36       0.22    0.60  0.19  0.06  0.03
Economics
    Free Market           0.93    1.71  1.06  0.59  0.35       0.22    0.20  0.20  0.33  0.14
    Planned               1.74    3.33  2.39  1.00  0.22       0.53    0.80  0.80  0.45  0.07
    Green                 0.74    0.00  0.01  0.68  2.28       0.25    0.00  0.00  0.21  0.79
Values
    Power                 1.24          1.25  1.26  1.22       0.52          0.73  0.50  0.33
    Knowledge             1.05          0.68  1.06  1.42       0.27          0.12  0.25  0.43
    Wealth                0.74          0.82  0.81  0.60       0.21          0.14  0.25  0.24
Future Society
    Cybernetic            1.74                1.87  1.60       0.16                0.14  0.17
    Eudaimonic            4.79                5.58  4.01       0.78                0.83  0.74
    Thought Control       0.97                0.56  1.38       0.06                0.03  0.09
```

Expected trends for most vanilla models.

## WTP v337

```
                         value    ---1  ---2  ---3  ---4     chance    ---1  ---2  ---3  ---4
Politics
    Police State          0.22    0.01  0.00  0.27  0.59       0.20    0.00  0.00  0.23  0.58
    Democratic            1.34    1.40  2.22  1.31  0.44       0.49    0.41  0.78  0.59  0.21
    Fundamentalist        0.93    1.49  0.98  0.65  0.59       0.30    0.59  0.22  0.18  0.21
Economics
    Free Market           0.91    2.68  0.67  0.21  0.08       0.25    0.74  0.13  0.09  0.03
    Planned               1.40    1.43  2.03  1.39  0.76       0.59    0.26  0.87  0.83  0.40
    Green                 0.48    0.00  0.02  0.40  1.51       0.16    0.00  0.00  0.08  0.57
Values
    Power                 1.40          1.15  1.44  1.60       0.76          0.81  0.72  0.75
    Knowledge             0.07          0.02  0.05  0.14       0.01          0.00  0.00  0.01
    Wealth                0.69          0.83  0.80  0.45       0.23          0.19  0.27  0.24
Future Society
    Cybernetic            1.97                2.59  1.35       0.76                0.89  0.62
    Eudaimonic            0.16                0.26  0.07       0.03                0.04  0.02
    Thought Control       0.84                0.41  1.26       0.21                0.07  0.35
```


# WTP v305 differences from vanilla

* Pop boom is more difficult to trigger.
* -4 EFFICIENCY looses about 50% to inefficiency, not 100%.
* SUPPORT gives 0-8 free units across the whole scale.
* Bases in large empire are more succeptible to bureaucracy pressure.

# SE models analysis and proposed changes

_v337_

![SE v337](/images/se-v337.png)

_Overall intent_

* Reduce EFFICIENCY usage, especially positive values. It is a powerful effect. Strangely, vanilla grants it left and right in large quantities.
* Increase POLICE/SUPPORT negative usage. It is very easy to bump them up to the top.

## Police State

Very powerful early, weak later due to bad EFFICIENCY. However, there are other means to bump up EFFICIENCY including CC. Overall, seems strong in vanilla and even stronger in this mod due to less harsh -4 EFFICIENCY penalty, which allows everybody using it.

_Proposed change_

* +1 SUPPORT. Not lore required and makes it slightly less powerful early game.
* -1 GROWTH. Optional, for the same reason above.

Both of the above changes make it slightly less powerful early game but practially do not affect it late game, which is the intent.

## Democratic

Weak early, strong late. Opposite of PS. Overall, good enough.

_Proposed change_

* +1 EFFICIENCY. Reducing efficiency bonuses across the board.
* +3 GROWTH. Make it more usable earlier.
* -2 POLICE.

## Fundamentalist

Complete crap. Need serious boost.

_Proposed change_

* +1 MORALE.
* +1 INDUSTRY.

## Free Market

Percieved as a very strong boost even at a cost of serious police impact. Maybe may benefit from some minor penalty.

_Proposed change_

* -1 PROBE. Open to industrial espionage.

## Planned

Valuable early but bad at future. May need some future boost. Maybe make higher productive but economy and efficiency impactful?

_Proposed change_

* +2 INDUSTRY.
* -1 ECONOMY.

## Green

Generally weakish until late due to EFFICIENCY boost.

_Proposed change_

* +1 EFFICIENCY. Efficiency bonuses should be reduced all over the board. They are too valuable to give away.
* +1/2 TALENT. May make it valuable early.

## Power

Kind of not doing what it is supposed to. Trash choice.

_Proposed change_

* +1 SUPPORT.
* +2 MORALE.
* +2 POLICE. Makes more workers to offset industry penalty.

## Knowledge

Adequate.

_Proposed change_

* +4 RESEARCH. Lore.
* -1 POLICE.

## Wealth

Adequate.

_Proposed change_

* -4 MORALE. Lore. Even more disadvantage to not wage a war with this choice.
* -1 GROWTH. Same.

## Cybernetic

Adequate.

## Eudaimonic

Overpowered.

_Proposed change_

* -1 EFFICIENCY instead of +2 INDUSTRY. Otherwise, it is too OP.
* -3 MORALE.

## Thought Control

Barely adequate. However, it lacks hard economical benefits.

# Usage analysis

Attempt to weight out how often and how wide certain effects are used in models to give each one fair representation.

| effect | # use | sum + | sum - | maximize | comment |
| ---- | ----: | ----: | ----: | ---- | ---- |
| ECONOMY | 4 | 5 | 1 | medium |  |
| EFFIC | (+)6 | 5 | 5 | easy | Still used too often. Why vanilla is so fixated on shoving it in every model? |
| SUPPORT | 4 | 2 | 5 | hard |  |
| MORALE | 5 | 5 | 7 | medium |  |
| POLICE | (+)7 | 6 | 11 | v.easy | Used too often. Easy to maximize. |
| GROWTH | 5 | 7 | 4 | medium |  |
| PLANET | (-)3 | 4 | 2 | easy | Rare use. Maybe add one entry somewhere. |
| PROBE | 4 | 4 | 3 | easy |  |
| INDUSTRY | 4 | 4 | 2 | hard | Do we need more negatives? |
| RESEARCH | (-)3 | 6 | 2 | easy | Rare use. More negatives? |

## Recomendations

* Carefully replace EFFICIENCY/POLICE WITH PLANET/RESEARCH where it does not make big impact.
* Maybe use more negative RESEARCH/PLANET as penalty. They are not that strong effects.

