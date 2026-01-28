# Purpose

A description of theory and practice of WTP AI.

# Generic implementation approach

Game is a mix of many author contributions. Following approach is used to preserve existing logic and advance iteratively.

1. Use vanilla/Thinker algorithms as a base line.
2. Improve game in small area of operations.
3. Fall back to default (#1) for not covered or impossible to compute cases.

# Generic AI paradigm

1. Develop a common measure/denominator that allows expessing player benefit in a single numerical value.
2. Express different game benefits in this measure and compare them to each other to pick a best option.

# Economical model

## Resource score

This game uses three major resources: nutrients, minerals, energy. They are not equal in value. Game AI uses a conversion coefficients (weights) to lump up different resource types into single resource score. Currently, they are like below, but can be adjusted as needed.

| resource  | weight |
| ---- | ---- |
| nutrient  | 1.00 |
| mineral   | 1.50 |
| energy    | 0.50 |

This is just a base line. Some cases require specific adjustment coefficients, obviously. Starving base may value nutrients more, etc.

## Economy development

AI assumes faction production power grows exponentially by e (2.78) every 40 turns. That is a statistical average. The model is not correct for the whole game duration but good enough as approximation for shorter time periods. This 40 turns scale is called "development scale" or just "scale" below.
That allows converting between differently timed benefits: one time bonus, steady income, income growth.

```
bonus value         = <bonus resource score>         / <development scale>
income value        = <income resource score>
income growth value = <income growth resource score> * <development scale>
```

They can be converted to each other using above rules. Income is picked as a baseline as it is the most common bonus and the most easiest to visualise.

## Evaluation examples

### Base mineral and energy production increase

Mineral and energy income is a direct economical income and is classified as such.

```
resource income increase value = <income increase resource score>
```

### Base nutrient production increase

Nutrient increase does not immediately impact resource income but speeds up growth to make more income in future. Thus it is classified as an income _growth_. The measure of such growth depends on base current size, nutrient box, number or farmers, etc.

```
income grouwth increase value = <income growth resource score> * <development scale>
```

Absolute measure of income growth is usually much less than direct income. For example, 1 nutrient yield improvement for 10 size base creates 0.01 (1%) growth. However, it is multiplied by development scale due to continuous impact.

# Civic unit movement

That can be computed easily using above economical model as each action (founding a base, terraforming) results in direct economical impact. This part is pretty much complete. May require further parameter tune up.

# Combat unit movement

This is currently in progress. Suggestions are welcome.

The theory is simple.

1. Devise a model to evaluate different actions/positions in relation to economical factors.
2. Iterate available unit actions and evaluate them with #1 above.
3. Pick the best one.

The problem is in #1. There are many units and each one need to be evaluated based on other unit actions/positions, both player's and opponent's. Opponent next turn should be taken into account, and other factors increase number of combinations astronomically.
The solution is to use generalized and superfluous evaluation to flatten the model, but keep it sophisticated enough for AI improvement.

## Problem examples

### Enemy unit destruction value

Enemy unit primary target is player's bases. Destruction of enemy unit is equivalent to protecting base and continuing reaping its economical benefits. The difficult problem here is distributing such economical benefit across multiple enemy units.
For example, if only one unit is adjacent to the base, destroying the threat is as good as saving a whole base.
What if there are two similar units adjacent to the base? Should AI assume the threat (and the corresponding benefit of eliminating it) is now twice as much or it is the same because it is stil just one base lost at the worst? Or should be something in between?

### Player unit destruction value

This seems to be more or less transparent. Losing own unit is economically equivalent to its mineral cost investments.

### Unit value coefficients

There are few coefficients modifying enemy unit threat already accounted for. Feel free to propose more.

#### Travel time

The longer it takes for it to reach the destination the less focused AI should be on it.

#### Strength (offense, defense)

Conventional offense is probably the most straightforward one. It can be valued as 1:1 coefficient.
Conventional defense is somewhat more vague. Currently it uses 0.5 coefficient as it is a passive property and opponent may not desire to attack.
Psi offense/defense as well as conventional one that results in psi combat can be evaluated as a proportional strength against average unit in opponent army.

#### Speed

Higher speed put more emphasis on offense and less on defense and also adds slightly to the value itself due to mobility. Current coefficient is 50% more value for each extra move comparing to basic triad type level.
So speeder gets +50% comparing to infantrly, cruiser gets 25% comparing to foil, etc.

#### Triad

Triad is very hard to value. I can take more suggestions here. Primarily I think there is a bonus for operational range: the more the better. There also should be a bonus for ignoring ZOC.

#### Damage

A highly damaged unit gets extra destruction points as it prevents it from healing and regaining strength. I take it 90% damaged unit costs twice as much to destroy.

### Positional value: blocking/defending

That one I have not even started thinking about. From the top of my head, I would say it is equivalent of defending/keeping something (base, bunker, other friendly units). However, what is the coefficient for preventing opponent advance, I do not know.
How much more beneficial is to move toward enemy rather than stay at base/bunker? Does it worth to attack assailant adjacent to the base or better sit tight?

