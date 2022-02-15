#include "aiData.h"
#include "game_wtp.h"

/**
Shared AI Data.
*/

// global variables

int currentTurn = -1;
int aiFactionId = -1;
Data aiData;

void Data::setup()
{
	mapData = new MapData[*map_area_tiles];
	
}

void Data::cleanup()
{
	delete[] mapData;
	clear();
	
}

