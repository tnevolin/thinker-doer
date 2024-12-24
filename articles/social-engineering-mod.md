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

| effect | value range | when valued most |
| ---- | :----: | ---- |
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
    Police State          0.70    0.79  0.16  0.85  1.02       0.13    0.26  0.02  0.14  0.12
    Democratic            3.10    1.13  2.87  3.94  4.46       0.75    0.36  0.93  0.85  0.88
    Fundamentalist        0.44    0.90  0.39  0.26  0.22       0.11    0.39  0.06  0.01  0.00
Economics
    Free Market           1.14    1.86  1.24  0.92  0.55       0.20    0.18  0.16  0.21  0.24
    Planned               3.19    4.18  3.59  2.93  2.08       0.80    0.82  0.84  0.79  0.74
    Green                 0.05    0.00  0.00  0.01  0.21       0.00    0.00  0.00  0.00  0.02
Values
    Power                 1.09          1.06  1.10  1.12       0.38          0.65  0.30  0.20
    Knowledge             1.38          0.82  1.46  1.86       0.37          0.21  0.38  0.52
    Wealth                0.87          0.73  1.05  0.82       0.25          0.15  0.33  0.28
Future Society
    Cybernetic            1.98                2.07  1.89       0.08                0.08  0.09
    Eudaimonic            8.18                8.73  7.64       0.89                0.91  0.88
    Thought Control       1.00                0.54  1.45       0.02                0.01  0.03
```

## Vanilla slightly more balanced

![se-v338.png](/images/se-vanilla3.png)

```
                         value    ---1  ---2  ---3  ---4     chance    ---1  ---2  ---3  ---4
Politics
    Police State          0.69    0.97  0.24  0.73  0.81       0.24    0.25  0.07  0.33  0.29
    Democratic            1.00    0.02  0.75  1.46  1.78       0.39    0.00  0.35  0.55  0.64
    Fundamentalist        0.91    1.69  0.93  0.55  0.47       0.38    0.75  0.57  0.12  0.06
Economics
    Free Market           0.93    1.34  1.13  0.78  0.45       0.35    0.32  0.41  0.45  0.24
    Planned               1.01    1.69  1.32  0.64  0.38       0.35    0.63  0.46  0.22  0.11
    Green                 1.03    0.38  0.66  1.10  2.00       0.29    0.05  0.13  0.33  0.65
Values
    Power                 1.19          1.28  1.15  1.14       0.44          0.71  0.37  0.25
    Knowledge             0.97          0.66  0.95  1.31       0.23          0.14  0.21  0.36
    Wealth                0.92          0.82  1.11  0.84       0.32          0.14  0.43  0.40
Future Society
    Cybernetic            1.11                1.19  1.04       0.36                0.39  0.33
    Eudaimonic            1.05                1.24  0.86       0.39                0.44  0.34
    Thought Control       1.11                0.67  1.54       0.25                0.17  0.32
```

