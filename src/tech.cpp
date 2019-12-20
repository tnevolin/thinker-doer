
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
*/
int tech_cost(int fac, int tech) {
    assert(fac >= 0 && fac < 8);
    FactMeta* m = &tx_factions_meta[fac];
    int level = 1;

    if (tech >= 0) {
        level = tech_level(tech);
    }

    double previous_level = (double)(level - 1);

    double endgame_linear_growth = 450.0 * ((double)(*tx_map_area) / 3200.0);

    double base = 20.0 + min(50.0 * previous_level * previous_level, endgame_linear_growth * previous_level);

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

