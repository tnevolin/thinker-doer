#pragma once

#include "main.h"
#include "engine.h"

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

