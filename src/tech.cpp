#include <math.h>
#include "tech.h"

void init_values(int fac) {
    Faction* f = &tx_factions[fac];
    if (f->thinker_header != THINKER_HEADER) {
        f->thinker_header = THINKER_HEADER;
        f->thinker_flags = 0;
        f->thinker_tech_id = f->tech_research_id;
        f->thinker_tech_cost = f->tech_cost;
    }
}

/**
Calculates tech level recursively.
*/
int tech_level(int id) {
    if (id < 0 || id > TECH_TranT)
    {
        return 0;
    }
    else
    {
        int v1 = tech_level(tx_techs[id].preq_tech1);
        int v2 = tech_level(tx_techs[id].preq_tech2);

        return max(v1, v2) + 1;
    }
}

/**
Calculates tech cost.
cost grows cubic from the beginning then linear.
S  = 20                                     // fixed shift (first tech cost)
C  = 0.025                                  // cubic coefficient
B  = 120 * (<map area> / <normal map area>) // linear slope
x0 = SQRT(B / (3 * C))                      // break point
A  = C * x0 ^ 3 - B * x0                    // linear intercept
x  = (<level> - 1) * 7

cost = S +
1.
when x < x0 then
C * x ^ 3
2.
else
A + B * x
*/
int tech_cost(int fac, int tech) {
    assert(fac >= 0 && fac < 8);
    FactMeta* m = &tx_factions_meta[fac];
    int level = 1;

    if (tech >= 0) {
        level = tech_level(tech);
    }

    double S = 20.0;
    double C = 0.025;
    double B = 120 * (*tx_map_area / 3200.0);
    double x0 = sqrt(B / (3 * C));
    double A = C * x0 * x0 * x0 - B * x0;
    double x = (level - 1) * 7;

    double base = S + (x < x0 ? C * x * x * x : A + B * x);

    double cost =
        base
        * (double)m->rule_techcost / 100.0
        * (*tx_scen_rules & RULES_TECH_STAGNATION ? 1.5 : 1.0)
        * (double)tx_basic->rules_tech_discovery_rate / 100.0
    ;

    double dw;

    if (is_human(fac)) {
        dw = 1.0 + 0.1 * (10.0 - tx_cost_ratios[*tx_diff_level]);
    }
    else {
        dw = 1.0;
    }

    cost *= dw;

    debug("tech_cost %d %d | %8.4f %8.4f %8.4f %d %d %s\n", *tx_current_turn, fac,
        base, dw, cost, level, tech, (tech >= 0 ? tx_techs[tech].name : NULL));

    return max(2, (int)cost);

}

HOOK_API int tech_rate(int fac) {
    /*
    Normally the game engine would recalculate research cost before the next tech
    is selected, but we need to wait until the tech is decided in tech_selection
    before recalculating the cost.
    */
    Faction* f = &tx_factions[fac];
    init_values(fac);

    if (f->tech_research_id != f->thinker_tech_id) {
        f->thinker_tech_cost = tech_cost(fac, f->tech_research_id);
        f->thinker_tech_id = f->tech_research_id;
    }
    return f->thinker_tech_cost;
}

HOOK_API int tech_selection(int fac) {
    Faction* f = &tx_factions[fac];
    int tech = tx_tech_selection(fac);
    init_values(fac);
    f->thinker_tech_cost = tech_cost(fac, tech);
    f->thinker_tech_id = tech;
    return tech;
}

