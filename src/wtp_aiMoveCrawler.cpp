#pragma GCC diagnostic ignored "-Wshadow"

#include "wtp_aiMoveCrawler.h"

#include <vector>

#include "main.h"
#include "wtp_game.h"
#include "wtp_ai_game.h"
#include "wtp_aiRoute.h"
#include "wtp_aiMove.h"

// convoy data

std::vector<int> crawlerIds;
std::vector<TileConvoyInfo> tileConvoyInfos;
std::array<std::array<TileConvoyInfo, 21>, MaxBaseNum> baseTileConvoyInfos;

void moveCrawlerStrategy()
{
	Profiling::start("moveCrawlerStrategy", "moveStrategy");
	
	populateConvoyData();
	assignCrawlerOrders();
	
	Profiling::stop("moveCrawlerStrategy");
	
}

void populateConvoyData()
{
	Profiling::start("populateConvoyData", "moveFormerStrategy");
	
	debug("populateConvoyData - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// available crawlers
	
	debug("\tavailableCrawlers\n");
	
	crawlerIds.clear();
	for (int vehicleId : aiData.vehicleIds)
	{
		// crawler
		
		if (!isSupplyVehicle(vehicleId))
			continue;
		
		if (isVehicleConvoying(vehicleId))
		{
			// keep convoying crawlers convoying
			setTask(Task(vehicleId, TT_CONVOY));
		}
		else
		{
			// available crawler
			crawlerIds.push_back(vehicleId);
			debug("\t\t[%4d] %s\n", vehicleId, getLocationString(getVehicleMapTile(vehicleId)));
		}
		
	}
	
	// base resource effects
	
	std::array<std::array<double, 3>, MaxBaseNum> baseResourceGains;
	std::array<int, 3> bestBaseResourceGainBaseId = {-1, -1, -1};
	std::array<double, 3> bestBaseResourceGain = {0.0, 0.0, 0.0};
	
	for (int baseId : aiData.baseIds)
	{
		BASE &base = Bases[baseId];
		
		// sufficient support
		
		if (base.mineral_surplus < 2)
			continue;
		
		Resource oldBaseIntake2 = getBaseResourceIntake2(baseId);
		double oldBaseGain = getBaseGain(baseId);
		for (BaseResType baseResType : {RSC_NUTRIENT, RSC_MINERAL, RSC_ENERGY})
		{
			Resource newBaseIntake2 = oldBaseIntake2;
			
			switch (baseResType)
			{
			case RSC_NUTRIENT:
				newBaseIntake2.nutrient += 1.0;
				break;
			case RSC_MINERAL:
				newBaseIntake2.mineral += 1.0;
				break;
			case RSC_ENERGY:
				newBaseIntake2.energy += 1.0;
				break;
			case RSC_UNUSED:
				continue;
				break;
			}
			
			double newBaseGain = getBaseGain(baseId, newBaseIntake2);
			double gain = newBaseGain - oldBaseGain;
			
			baseResourceGains.at(baseId).at(baseResType) = gain;
			if (gain > bestBaseResourceGain.at(baseResType))
			{
				bestBaseResourceGainBaseId.at(baseResType) = baseId;
				bestBaseResourceGain.at(baseResType) = gain;
			}
			
		}
		
	}
	
	// coveredBaseIds
	
	std::vector<std::vector<int>> coveredBaseIds;
	coveredBaseIds.resize(*MapAreaTiles);
	
	for (int baseId : aiData.baseIds)
	{
		BASE &base = Bases[baseId];
		
		for (MAP *tile : getBaseRadiusTiles(base.x, base.y, false))
		{
			coveredBaseIds.at(tile - *MapTiles).push_back(baseId);
		}
		
	}
	
	// workedBaseIds
	
	std::vector<int> workedBaseIds;
	workedBaseIds.resize(*MapAreaTiles, -1);
	
	for (int baseId : aiData.baseIds)
	{
		BASE &base = Bases[baseId];
		
		for (int workerNumber = 1; workerNumber < 21; workerNumber++)
		{
			if ((base.worked_tiles & (0x1 << workerNumber)) == 0)
				continue;
			
			MAP *tile = getBaseWorkerTile(baseId, workerNumber);
			workedBaseIds.at(tile - *MapTiles) = baseId;
			
		}
		
	}
	
	// convoyed tiles
	
	robin_hood::unordered_flat_set<MAP *> convoyedTiles;
	
	for (int vehicleId : aiData.vehicleIds)
	{
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		
		// convoy
		
		if (!isConvoyVehicle(vehicleId))
			continue;
		
		// convoying
		
		if (!isVehicleConvoying(vehicleId))
			continue;
		
		// convoyed tile
		
		convoyedTiles.insert(vehicleTile);
		
	}
	
	// tile convoy infos
	
	tileConvoyInfos.resize(*MapAreaTiles);
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		int tileX = getX(tileIndex);
		int tileY = getY(tileIndex);
		TileConvoyInfo &tileConvoyInfo = tileConvoyInfos.at(tileIndex);
		TileInfo &tileInfo = aiData.tileInfos.at(tileIndex);
		
		// default values
		
		tileConvoyInfo.available = false;
		tileConvoyInfo.baseid = -1;
		tileConvoyInfo.gain = 0.0;
		
		// not other faction territory
		
		if (!(tile->owner == -1 || tile->owner == aiFactionId))
			continue;
		
		// not blocked
		
		if (tileInfo.blocked)
			continue;
		
		// not base
		
		if (tileInfo.base)
			continue;
		
		// not convoyed
		
		if (convoyedTiles.find(tile) != convoyedTiles.end())
			continue;
		
		// set available tile
		
		tileConvoyInfo.available = true;
		
		// compute gain
		
		BaseResType bestBaseResType = RSC_UNUSED;
		int bestBaseResTypeBaseId = -1;
		double bestBaseResTypeGain = 0.0;
		
		for (BaseResType baseResType : {RSC_NUTRIENT, RSC_MINERAL, RSC_ENERGY})
		{
			int yield = resource_yield(baseResType, aiFactionId, -1, tileX, tileY);
			int bestBaseId = bestBaseResourceGainBaseId.at(baseResType);
			double bestGain = (double) yield * bestBaseResourceGain.at(baseResType);
			
			for (int baseId : coveredBaseIds.at(tileIndex))
			{
				int baseYield = resource_yield(baseResType, aiFactionId, baseId, tileX, tileY);
				double baseGain = (double) baseYield * baseResourceGains.at(baseId).at(baseResType);
				
				if (baseGain > bestGain)
				{
					bestBaseId = baseId;
					bestGain = baseGain;
				}
				
			}
			
			if (bestGain > bestBaseResTypeGain)
			{
				bestBaseResType = baseResType;
				bestBaseResTypeBaseId = bestBaseId;
				bestBaseResTypeGain = bestGain;
				
			}
			
		}
		
		// subtract potential base gain loss
		
		double baseGainChange = 0.0;
		
		int baseId = workedBaseIds.at(tile - *MapTiles);
		if (baseId != -1)
		{
			// emulate crawler on this tile to estimate base gain loss
			
			double oldBaseGain = getBaseGain(baseId);
			computeBase(baseId, true, tile);
			double newBaseGain = getBaseGain(baseId);
			aiData.resetBase(baseId);
			baseGainChange = newBaseGain - oldBaseGain;
			
		}
		
		// final numbers
		
		tileConvoyInfo.baseResType = bestBaseResType;
		tileConvoyInfo.baseid = bestBaseResTypeBaseId;
		tileConvoyInfo.gain = bestBaseResTypeGain + baseGainChange;
		
//		if (DEBUG)
//		{
//			debug("\t%s %d [%3d] %7.2f + %7.2f = %7.2f\n", getLocationString(tile), tileConvoyInfo.baseResType, tileConvoyInfo.baseid, bestBaseResTypeGain, baseGainChange, tileConvoyInfo.gain);
//		}
		
	}
	
	Profiling::stop("populateConvoyData");
	
}

void assignCrawlerOrders()
{
	Profiling::start("assignCrawlerOrders", "moveCrawlerStrategy");
	
	debug("assignCrawlerOrders - %s\n", MFactions[aiFactionId].noun_faction);
	
	// distribute orders
	
	robin_hood::unordered_flat_set<MAP *> assignments;
	
	for (int vehicleId : crawlerIds)
	{
		VEH &vehicle = Vehs[vehicleId];
		int triad = vehicle.triad();
		
		// find best assignment location
		
		MAP *bestTile = nullptr;
		double bestTileGain = 0.0;
		
		for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
		{
			MAP *tile = *MapTiles + tileIndex;
			TileConvoyInfo const &tileConvoyInfo = tileConvoyInfos.at(tileIndex);
			
			debug("\t%s\n", getLocationString(tile));
			
			// available
			
			if (!tileConvoyInfo.available)
				continue;
			
			// sufficient support
			
			if (Bases[tileConvoyInfo.baseid].mineral_surplus < 2)
				continue;
			
			// matching triad
			
			if (is_ocean(tile))
			{
				if (triad == TRIAD_LAND)
					continue;
			}
			else
			{
				if (triad == TRIAD_SEA)
					continue;
			}
			
			// not yet assigned
			
			if (assignments.find(tile) != assignments.end())
				continue;
			
			// travel time
			
			double travelTime = getVehicleApproachTime(vehicleId, tile);
			if (travelTime == INF)
				continue;
			
			// gain
			
			double gain = getGainDelay(tileConvoyInfo.gain, travelTime);
			debug("\t\tgain = %5.2f\n", gain);
			
			if (gain > bestTileGain)
			{
				bestTile = tile;
				bestTileGain = gain;
			}
			
		}
		
		if (bestTile == nullptr)
			continue;
		
		// transit vehicle
		
		if (!transitVehicle(Task(vehicleId, TT_CONVOY, bestTile)))
			continue;
		
		// set other parameters
		
		TileConvoyInfo const &tileConvoyInfo = tileConvoyInfos.at(bestTile - *MapTiles);
		
		int oldHomeBaseId = vehicle.home_base_id;
		int newHomeBaseId = tileConvoyInfo.baseid;
		vehicle.home_base_id = newHomeBaseId;
		if (oldHomeBaseId != -1)
		{
			computeBase(oldHomeBaseId, false);
		}
		if (newHomeBaseId != -1)
		{
			computeBase(newHomeBaseId, false);
		}
		
		vehicle.order_auto_type = tileConvoyInfo.baseResType;
		
		assignments.insert(bestTile);
		
		debug("\t[%4d] %s -> %s [%3d] %d\n", vehicleId, getLocationString({vehicle.x, vehicle.y}), getLocationString(bestTile), tileConvoyInfo.baseid, tileConvoyInfo.baseResType);
		
	}
	
	// populate convoyProductionRequests
	
	debug("terraformingProductionRequests - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	std::vector<ConvoyRequest> &convoyRequests = aiData.production.convoyRequests;
	convoyRequests.clear();
	
	// store requests
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileConvoyInfo const &tileConvoyInfo = tileConvoyInfos.at(tileIndex);
		
		if (!tileConvoyInfo.available)
			continue;
		
		if (assignments.find(tile) != assignments.end())
			continue;
		
		convoyRequests.push_back({tile, tileConvoyInfo.gain});
		
	}
	
	Profiling::stop("assignCrawlerOrders");
	
}

