# Original vanilla analysis article

https://alphacentauri.miraheze.org/wiki/Social_Engineering_Mod

https://web.archive.org/web/20180810045412/http://alphacentauri2.info/wiki/Social_Engineering_Mod

The SE model proposed in article above is for vanilla. WTP uses a different one since it uses different rules and, therefore, SE effects are somewhat differrently weighted comparing to each other. However, ideas about balancing and different effect comparison methods are still generally valid.

# WTP v305 differences from vanilla

* Pop boom is more difficult to trigger.
* -4 EFFICIENCY looses about 50% to inefficiency, not 100%.
* SUPPORT gives 0-8 free units across the whole scale.
* Bases in large empire are more succeptible to bureaucracy pressure.

# SE models analysis and proposed changes

![SE v336](/images/se-v336.png)

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

