//#include "move.h"
//#include "ai.h"
//
//BaseSearch::BaseSearch(int faction_id, int body_id, int x, int y)
//{
//    // populate bases for given faction and body
//
//    count = 0;
//
//    for (int i = 0; i < *tx_total_num_bases; i++)
//    {
//        BASE *base = &tx_bases[i];
//        MAP *base_map_square = mapsq(base->x, base->y);
//
//        if (base->faction_id != faction_id || base_map_square->body_id != body_id)
//            continue;
//
//        nodes[count].base = base;
//        nodes[count].distance = map_range(base->x, base->y, x, int y);
//        nodes[count].selected = false;
//        count++;
//
//    }
//
//}
//
//BASE* BaseSearch::next()
//{
//    BASE *base = NULL;
//    BaseNode node = NULL;
//    int distance = INT_MIN;
//
//    for (int i = 0; i < count; i++)
//    {
//        BaseNode node = nodes[i];
//
//        if (node.selected)
//            continue;
//
//        if (node.distance < distance)
//        {
//            node = node;
//            distance = node.distance;
//
//        }
//
//    }
//
//    if (node != NULL)
//    {
//        node.selected = true;
//        base = node.base;
//
//    }
//
//    return base;
//
//}
//
//int ai_former_move(int id, PMTable pm_safety, PMTable pm_former)
//{
//    VEH* veh = &tx_vehicles[id];
//    int triad = veh_triad(id);
//    MAP* sq = mapsq(x, y);
//    bool safe = pm_safety[x][y] >= PM_SAFE;
//    int item;
//    BASE *vehicle_home_base = vehicle_home_base(veh);
//    int vehicle_reacheable_body_id = vehicle_home_base(veh);
//
//    // escape in safety
//
//    if (!safe)
//        return escape_move(id);
//
//    debug
//    (
//        "former parameters: x=%d, y=%d, safe=%d, move_status=%s\n",
//        veh->x,
//        veh->y,
//        safe,
//        MOVE_STATUS[veh->move_status].c_str()
//    )
//    ;
//
//    // set initial search point at home base if it exists and reachable
//    if (vehicle_home_base != NULL && (triad == TRIAD_AIR || base_square(vehicle_home_base)->body_id == sq->body_id))
//    {
//        bx = vehicle_home_base->x;
//        by = vehicle_home_base->y;
//    }
//    // otherwise, set initial search point at vehicle location
//    else
//    {
//        int bx = x;
//        int by = y;
//    }
//
//    // search for base in needs of terraforming starting from home base
//
//    BaseSearch baseSearch(fac, (triad == TRIAD_AIR ? -1 : sq->body_id), bx, by);
//
//    int tscore = INT_MIN;
//    int tx = -1;
//    int ty = -1;
//
//    while ((BASE *base = baseSearch.next()) != NULL)
//    {
//        if (sq->owner != fac || sq->items & TERRA_BASE_IN_TILE
//        || pm_safety[ts.rx][ts.ry] < PM_SAFE
//        || pm_former[ts.rx][ts.ry] < 1
//        || other_in_tile(fac, sq))
//            continue;
//        int score = former_tile_score(bx, by, ts.rx, ts.ry, fac, sq);
//        if (score > tscore && (item = select_item(ts.rx, ts.ry, fac, sq)) != -1) {
//            tx = ts.rx;
//            ty = ts.ry;
//            tscore = score;
//        }
//    }
//    if (tx >= 0) {
//        pm_former[tx][ty] -= 2;
//        debug("former_move %d %d -> %d %d | %d %d | %d %d\n", x, y, tx, ty, fac, id, item, tscore);
//        return set_move_to(id, tx, ty);
//    } else if (!random(4)) {
//        return set_move_to(id, bx, by);
//    }
//
//    if (safe || (veh->move_status >= 16 && veh->move_status < 22))
//    {
//        if
//        (
//            (veh->move_status >= 4 && veh->move_status < 24)
//            ||
////            (triad != TRIAD_LAND && !at_target(veh))
//            // [WtP] do not disturb former in transit any kind including land ones
//            !at_target(veh)
//        )
//        {
//            return SYNC;
//        }
//
//        item = select_item(x, y, fac, sq);
//        if (item >= 0) {
//            pm_former[x][y] -= 2;
//            if (item == FORMER_RAISE_LAND) {
//                int cost = tx_terraform_cost(x, y, fac);
//                tx_factions[fac].energy_credits -= cost;
//                debug("bridge_cost %d %d %d %d\n", x, y, fac, cost);
//            }
//            debug("former_action %d %d %d %d %s\n", x, y, fac, id, MOVE_STATUS[item+4].c_str());
//            return set_action(id, item+4, *tx_terraform[item].shortcuts);
//        }
//    } else if (!safe) {
//        return escape_move(id);
//    }
//    debug("former_skip %d %d | %d %d\n", x, y, fac, id);
//    return veh_skip(id);
//}
//
