#include "path.h"

/**
Wraps GO_TO order to avoid ZOC.
Non land vehicle is sent to final destinations.
Land vehicle is sent to final destination if there are no ZOC encountered along the shortest path.
Otherwize, land vehicle is sent to farthest one turn reacheable point alont the ZOC-safe path.
*/
//void moveVehicle(int id, int x, int y)
