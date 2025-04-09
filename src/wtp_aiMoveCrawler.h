#pragma once

#include "wtp_game.h"
#include "wtp_mod.h"

struct TileConvoyInfo
{
	bool available;
	BaseResType baseResType;
	int baseid;
	double gain;
	
};

void moveCrawlerStrategy();
void populateConvoyData();
void assignCrawlerOrders();

