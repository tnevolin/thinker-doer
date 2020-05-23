//#ifndef __AI_H__
//#define __AI_H__
//
//#include "main.h"
//#include "game.h"
//
//extern const int BASE_WORKING_TILE_OFFSETS[][2] =
//    {
//        {-1, -3},
//        {+1, -3},
//        {-2, -2},
//        {+0, -2},
//        {+2, -2},
//        {-3, -1},
//        {-1, -1},
//        {+1, -1},
//        {+3, -1},
//        {-1, +0},
//        {+1, +0},
//        {-3, +1},
//        {-1, +1},
//        {+1, +1},
//        {+3, +1},
//        {-2, +2},
//        {+0, +2},
//        {+2, +2},
//        {-1, +3},
//        {+1, +3}
//    }
//;
//
//
//struct BaseNode
//{
//    BASE *base;
//    int distance;
//    bool selected;
//};
//
//class BaseSearch
//{
//private:
//    BaseNode nodes[QSIZE];
//    int count;
//    int x, y;
//
//public:
//    BaseSearch(int faction_id, int body_id, int x, int y);
//    BASE *next();
//
//};
//
//class BaseWorkingTileSearch
//{
//private:
//    BASE *base;
//    int triad;
//    int body_id;
//    int tile_index;
//
//public:
//    BaseWorkingTileSearch(BASE *base, int body_id, int x, int y);
//    BASE *next();
//
//};
//
//int ai_former_move(int id, PMTable pm_safety, PMTable pm_former);
//
//#endif // __AI_H__
