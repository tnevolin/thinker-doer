#pragma once

#include "wtp_ai.h"
#include "wtp_aiData.h"
#include "wtp_aiRoute.h"
#include "wtp_aiMove.h"

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

