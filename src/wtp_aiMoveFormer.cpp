#pragma GCC diagnostic ignored "-Wshadow"

#include <float.h>
#include <math.h>
#include <vector>
#include <set>
#include <map>
#include <map>
#include "game.h"
#include "wtp_terranx.h"
#include "wtp_mod.h"
#include "wtp_ai.h"
#include "wtp_aiData.h"
#include "wtp_aiMove.h"
#include "wtp_aiMoveFormer.h"
#include "wtp_aiProduction.h"
#include "wtp_aiRoute.h"

// variables

FactionTerraformingInfo factionTerraformingInfo;
std::vector<MAP *> raiseableCoasts;
robin_hood::unordered_flat_set<MAP *> significantTerraformingRequestLocations;

// FORMER_ORDER

FORMER_ORDER::FORMER_ORDER(int _vehicleId)
: vehicleId{_vehicleId}
{

}

// terraforming data

std::vector<TileTerraformingInfo> tileTerraformingInfos;
std::vector<BaseTerraformingInfo> baseTerraformingInfos;

std::vector<MAP *> terraformingSites;

double networkDemand = 0.0;
std::vector<FORMER_ORDER> formerOrders;
robin_hood::unordered_flat_map<int, FORMER_ORDER *> vehicleFormerOrders;
std::vector<TERRAFORMING_REQUEST> terraformingRequests;

// terraforming data operations

// access terraforming data arrays

BaseTerraformingInfo &getBaseTerraformingInfo(int baseId)
{
	assert(baseId >= 0 && baseId < *BaseCount);
	return baseTerraformingInfos.at(baseId);
}

TileTerraformingInfo &getTileTerraformingInfo(MAP *tile)
{
	assert(tile != nullptr && isOnMap(tile));
	return tileTerraformingInfos.at(tile - *MapTiles);
}

/*
Prepares former orders.
*/
void moveFormerStrategy()
{
	executionProfiles["1.5.6. moveFormerStrategy"].start();
	
	// initialize data

	setupTerraformingData();

	// populate data

	populateTerraformingData();

	// formers

	cancelRedundantOrders();
	generateTerraformingRequests();
	sortTerraformingRequests();
	applyProximityRules();
	removeTerraformedTiles();
	assignFormerOrders();
	finalizeFormerOrders();

	executionProfiles["1.5.6. moveFormerStrategy"].stop();
	
}

void setupTerraformingData()
{
	FactionInfo &aiFactionInfo = aiData.factionInfos.at(aiFactionId);
	
	// cleanup and setup data
	
	tileTerraformingInfos.clear();
	tileTerraformingInfos.resize(*MapAreaTiles, {});
	
	baseTerraformingInfos.clear();
	baseTerraformingInfos.resize(MaxBaseNum, {});
	
	terraformingSites.clear();
	
	terraformingRequests.clear();
	formerOrders.clear();
	vehicleFormerOrders.clear();
	
	// cluster former counts
	
	aiFactionInfo.airFormerCount = 0;
	aiFactionInfo.seaClusterFormerCounts.clear();
	aiFactionInfo.landTransportedClusterFormerCounts.clear();
	
	for (int vehicleId : aiData.formerVehicleIds)
	{
		VEH *vehicle = getVehicle(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		int triad = vehicle->triad();
		
		switch (triad)
		{
		case TRIAD_AIR:
			aiFactionInfo.airFormerCount++;
			break;
			
		case TRIAD_SEA:
			{
				int seaCluster = getSeaCluster(vehicleTile);
				aiFactionInfo.seaClusterFormerCounts[seaCluster]++;
			}
			break;
			
		case TRIAD_LAND:
			{
				int landTransportedCluster = getLandTransportedCluster(vehicleTile);
				aiFactionInfo.landTransportedClusterFormerCounts[landTransportedCluster]++;
			}
			break;
			
		}
		
	}
	
	// average former terraforming rate
	
	double factionNormalTerraformingRateMultipier = isFactionHasProject(aiFactionId, FAC_WEATHER_PARADIGM) ? 1.5 : 1.0;
	double factionFungusTerraformingRateMultipier = isFactionHasProject(aiFactionId, FAC_XENOEMPATHY_DOME) ? 2.0 : 1.0;
	
	double formerCount = 0.0;
	double sumNormalTerraformingRateMultiplier = 0.0;
	double sumPlantFungusTerraformingRateMultiplier = 0.0;
	double sumRemoveFungusTerraformingRateMultiplier = 0.0;
	
	for (int vehicleId : aiData.formerVehicleIds)
	{
		// count
		
		formerCount += 1.0;
		
		// terraforming rate
		
		double normalTerraformingRateMultiplier = factionNormalTerraformingRateMultipier * (isVehicleHasAbility(vehicleId, ABL_SUPER_TERRAFORMER) ? 2.0 : 1.0);
		double plantFungusTerraformingRateMultiplier = factionFungusTerraformingRateMultipier * (isVehicleHasAbility(vehicleId, ABL_SUPER_TERRAFORMER) ? 2.0 : 1.0);
		double removeFungusTerraformingRateMultiplier = factionFungusTerraformingRateMultipier * (isVehicleHasAbility(vehicleId, ABL_SUPER_TERRAFORMER) ? 2.0 : 1.0) * (isVehicleHasAbility(vehicleId, ABL_FUNGICIDAL) ? 2.0 : 1.0);
		
		sumNormalTerraformingRateMultiplier += normalTerraformingRateMultiplier;
		sumPlantFungusTerraformingRateMultiplier += plantFungusTerraformingRateMultiplier;
		sumRemoveFungusTerraformingRateMultiplier += removeFungusTerraformingRateMultiplier;
		
	}
	
	factionTerraformingInfo.averageNormalTerraformingRateMultiplier = formerCount == 0 ? 1.0 : sumNormalTerraformingRateMultiplier / formerCount;
	factionTerraformingInfo.averagePlantFungusTerraformingRateMultiplier = formerCount == 0 ? 1.0 : sumPlantFungusTerraformingRateMultiplier / formerCount;
	factionTerraformingInfo.averageRemoveFungusTerraformingRateMultiplier = formerCount == 0 ? 1.0 : sumRemoveFungusTerraformingRateMultiplier / formerCount;
	
	// bareLandScore
	
	int forestNutrient = ResInfo->forest_sq_nutrient + (isFactionHasTech(aiFactionId, Facility[FAC_TREE_FARM].preq_tech) ? 1 : 0) + (isFactionHasTech(aiFactionId, Facility[FAC_HYBRID_FOREST].preq_tech) ? 1 : 0);
	int forestMineral = ResInfo->forest_sq_mineral;
	int forestEnergy = ResInfo->forest_sq_energy + (isFactionHasTech(aiFactionId, Facility[FAC_HYBRID_FOREST].preq_tech) ? 1 : 0);
	double forestScore = getTerraformingResourceScore(forestNutrient, forestMineral, forestEnergy);
	
	double fungusScore = 0.0;
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		// fungus
		
		if (!map_has_item(tile, BIT_FUNGUS))
			continue;
		
		int x = getX(tile);
		int y = getY(tile);
		
		int fungusNutrient = mod_crop_yield(aiFactionId, -1, x, y, 0);
		int fungusMineral = mod_mine_yield(aiFactionId, -1, x, y, 0);
		int fungusEnergy = mod_energy_yield(aiFactionId, -1, x, y, 0);
		fungusScore = getTerraformingResourceScore(fungusNutrient, fungusMineral, fungusEnergy);
		
		break;
		
	}
	
	factionTerraformingInfo.bareLandScore = std::max(forestScore, fungusScore);
	
	// bareMineScore
	
	int mineNutrient = ResInfo->improved_land_nutrient + (isFactionHasTech(aiFactionId, Terraform[FORMER_SOIL_ENR].preq_tech) ? 1 : 0) + Rules->nutrient_effect_mine_sq;
	int mineMineral = 1;
	int mineEnergy = 0;
	
	factionTerraformingInfo.bareMineScore = getTerraformingResourceScore(mineNutrient, mineMineral, mineEnergy);
	
	// bareSolarScore
	
	int solarNutrient = ResInfo->improved_land_nutrient + (isFactionHasTech(aiFactionId, Terraform[FORMER_SOIL_ENR].preq_tech) ? 1 : 0);
	int solarMineral = 0;
	int solarEnergy = 1 + (isFactionHasTech(aiFactionId, Terraform[FORMER_ECH_MIRROR].preq_tech) ? 2 : 0);
	
	factionTerraformingInfo.bareSolarScore = getTerraformingResourceScore(solarNutrient, solarMineral, solarEnergy);
	
}

/**
Populates global lists before processing faction strategy.
*/
void populateTerraformingData()
{
	debug("populateTerraformingData - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	// populate map states
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);
		
		setMapState(tileTerraformingInfo.mapState, tile);
		
	}
	
	// populate tileTerraformingInfos
	// populate terraformingSites
	// populate conventionalTerraformingSites
	
	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);
		
		// initialize base terraforming info
		
		baseTerraformingInfo.unworkedLandRockyTileCount = 0;
		
		// base radius tiles
		
		for (MAP *tile : getBaseRadiusTiles(base->x, base->y, false))
		{
			TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);
			
			// exclude not valid terraforming site
			
			if (!isValidTerraformingSite(tile))
				continue;
			
			tileTerraformingInfo.availableTerraformingSite = true;
			
			// worked
			
			if (isBaseWorkedTile(baseId, tile))
			{
				tileTerraformingInfo.worked = true;
				tileTerraformingInfo.workedBaseId = baseId;
			}
			
			// workable
			
			if (isWorkableTile(tile))
			{
				tileTerraformingInfo.workable = true;
				tileTerraformingInfo.workableBaseIds.push_back(baseId);
			}
			
			// conventional terraforming sites
			
			if (isValidConventionalTerraformingSite(tile))
			{
				tileTerraformingInfo.availableBaseTerraformingSite = true;
			}
			
			// update base rocky land tile count
			
			if (!is_ocean(tile))
			{
				if (map_rockiness(tile) == 2)
				{
					if (tileTerraformingInfo.workedBaseId == -1)
					{
						baseTerraformingInfo.unworkedLandRockyTileCount++;
					}
					
				}
				
			}
			
		}
		
		// base radius adjacent tiles
		
		for (int index = OFFSET_COUNT_RADIUS; index < OFFSET_COUNT_RADIUS_SIDE; index++)
		{
			int x = wrap(base->x + BASE_TILE_OFFSETS[index][0]);
			int y = base->y + BASE_TILE_OFFSETS[index][1];
			
			if (!isOnMap(x, y))
				continue;
			
			MAP *tile = getMapTile(x, y);
			TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(getMapTile(x, y));
			
			// valid terraforming site
			
			if (!isValidTerraformingSite(tile))
				continue;
			
			tileTerraformingInfo.availableTerraformingSite = true;
			
			// update area workable bases
			
			tileTerraformingInfo.areaWorkableBaseIds.push_back(baseId);
			
		}
		
	}
	
	// remove unavailable terraforming clusters
	
	if (!has_tech(Chassis[CHS_GRAVSHIP].preq_tech, aiFactionId))
	{
		// collect available terraforming clusters
		
		robin_hood::unordered_flat_set<int> availableTerraformingSeaClusters;
		robin_hood::unordered_flat_set<int> availableTerraformingLandTransportedClusters;
		
		// for formers
		
		for (int vehicleId : aiData.formerVehicleIds)
		{
			VEH *vehicle = getVehicle(vehicleId);
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			switch (vehicle->triad())
			{
			case TRIAD_SEA:
				{
					int seaCluster = getSeaCluster(vehicleTile);
					
					if (seaCluster != -1)
					{
						availableTerraformingSeaClusters.insert(seaCluster);
					}
					
				}
				break;
				
			case TRIAD_LAND:
				{
					int landTransportedCluster = getLandTransportedCluster(vehicleTile);
					
					if (landTransportedCluster != -1)
					{
						availableTerraformingLandTransportedClusters.insert(landTransportedCluster);
					}
					
				}
				break;
				
			}
			
		}
		
		// for bases
		
		if (has_tech(Units[BSC_SEA_FORMERS].preq_tech, aiFactionId))
		{
			for (int baseId : aiData.baseIds)
			{
				MAP *baseTile = getBaseMapTile(baseId);
				
				int seaCluster = getSeaCluster(baseTile);
				
				if (seaCluster != -1)
				{
					availableTerraformingSeaClusters.insert(seaCluster);
				}
				
			}
			
		}
		
		if (has_tech(Units[BSC_FORMERS].preq_tech, aiFactionId))
		{
			for (int baseId : aiData.baseIds)
			{
				MAP *baseTile = getBaseMapTile(baseId);
				
				int landTransportedCluster = getLandTransportedCluster(baseTile);
				
				if (landTransportedCluster != -1)
				{
					availableTerraformingLandTransportedClusters.insert(landTransportedCluster);
				}
				
			}
			
		}
		
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);
			
			if (is_ocean(tile))
			{
				int seaCluster = getSeaCluster(tile);
				
				if (seaCluster == -1 || availableTerraformingSeaClusters.find(seaCluster) == availableTerraformingSeaClusters.end())
				{
					tileTerraformingInfo.availableTerraformingSite = false;
					continue;
				}
				
			}
			else
			{
				int landTransportedCluster = getLandTransportedCluster(tile);
				
				if (landTransportedCluster == -1 || availableTerraformingLandTransportedClusters.find(landTransportedCluster) == availableTerraformingLandTransportedClusters.end())
				{
					tileTerraformingInfo.availableTerraformingSite = false;
					continue;
				}
				
			}
			
		}
		
	}
	
	if (DEBUG)
	{
		debug("\tavailableTerraformingSites\n");
		for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
		{
			int x = getX(tile);
			int y = getY(tile);
			TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(getMapTile(x, y));
			
			if (!tileTerraformingInfo.availableTerraformingSite)
				continue;
			
			debug("\t\t%s\n", getLocationString(tile).c_str());
			
		}
		
	}
	
	// populated terraformed tiles and former orders
	
	for (int vehicleId : aiData.vehicleIds)
	{
		VEH *vehicle = &Vehicles[vehicleId];
		int triad = vehicle->triad();
		int defenseValue = getVehicleDefenseValue(vehicleId);
		MAP *vehicleTile = getVehicleMapTile(vehicleId);
		TileTerraformingInfo &vehicleTileTerraformingInfo = getTileTerraformingInfo(getMapTile(vehicle->x, vehicle->y));
		
		// process supplies and formers
		
		// supplies
		if (isSupplyVehicle(vehicle))
		{
			// convoying vehicles
			if (isVehicleConvoying(vehicle))
			{
				vehicleTileTerraformingInfo.harvested = true;
			}
			
		}
		// formers
		else if (isFormerVehicle(vehicle))
		{
			// terraforming vehicles
			
			if (isVehicleTerraforming(vehicle))
			{
				// set sync task
				
				setTask(Task(vehicleId, TT_NONE));
				
				// terraformed flag
				
				vehicleTileTerraformingInfo.terraformed = true;
				
				// conventional terraformed flag
				
				if
				(
					vehicle->order == ORDER_FARM
					||
					vehicle->order == ORDER_SOIL_ENRICHER
					||
					vehicle->order == ORDER_MINE
					||
					vehicle->order == ORDER_SOLAR_COLLECTOR
					||
					vehicle->order == ORDER_CONDENSER
					||
					vehicle->order == ORDER_ECHELON_MIRROR
					||
					vehicle->order == ORDER_THERMAL_BOREHOLE
					||
					vehicle->order == ORDER_PLANT_FOREST
					||
					vehicle->order == ORDER_PLANT_FUNGUS
				)
				{
					vehicleTileTerraformingInfo.terraformedConventional = true;
				}
				
				// populate terraformed forest tiles
				
				if (vehicle->order == ORDER_PLANT_FOREST)
				{
					vehicleTileTerraformingInfo.terraformedForest = true;
				}
				
				// populate terraformed condenser tiles
				
				if (vehicle->order == ORDER_CONDENSER)
				{
					vehicleTileTerraformingInfo.terraformedCondenser = true;
				}
				
				// populate terraformed mirror tiles
				
				if (vehicle->order == ORDER_ECHELON_MIRROR)
				{
					vehicleTileTerraformingInfo.terraformedMirror = true;
				}
				
				// populate terraformed borehole tiles
				
				if (vehicle->order == ORDER_THERMAL_BOREHOLE)
				{
					vehicleTileTerraformingInfo.terraformedBorehole = true;
				}
				
				// populate terraformed aquifer tiles
				
				if (vehicle->order == ORDER_DRILL_AQUIFER)
				{
					vehicleTileTerraformingInfo.terraformedAquifer = true;
				}
				
				// populate terraformed raise tiles
				
				if (vehicle->order == ORDER_TERRAFORM_UP)
				{
					vehicleTileTerraformingInfo.terraformedRaise = true;
				}
				
				// populate terraformed sensor tiles
				
				if (vehicle->order == ORDER_SENSOR_ARRAY)
				{
					vehicleTileTerraformingInfo.terraformedSensor = true;
				}
				
			}
			// available vehicles
			else
			{
				// ignore those in terraformingWarzone except land vehicle in ocean
				
				if (!(triad == TRIAD_LAND && is_ocean(vehicleTile)))
				{
					double vehicleTileDanger = aiData.getTileInfo(getMapTile(vehicle->x, vehicle->y)).danger;
					
					if (defenseValue > 0 && vehicleTileDanger > defenseValue)
						continue;
					
				}
				
				// exclude air terraformers - let AI deal with them itself
				
				if (veh_triad(vehicleId) == TRIAD_AIR)
					continue;
				
				// store vehicle current id in pad_0 field
				
				vehicle->pad_0 = vehicleId;
				
				// add vehicle
				
				formerOrders.push_back({vehicleId});
				
			}
			
		}
		
	}
	
	// calculate network demand
	
	int networkType = (isFactionHasTech(aiFactionId, Terraform[FORMER_MAGTUBE].preq_tech) ? BIT_MAGTUBE : BIT_ROAD);
	
	int eligibleTileCount = 0;
	int coveredTileCount = 0;
	
	for (int tileIndex = 0; tileIndex < *MapAreaTiles; tileIndex++)
	{
		MAP *tile = *MapTiles + tileIndex;
		TileInfo &tileInfo = aiData.getTileInfo(tileIndex);
		
		// land
		
		if (tileInfo.ocean)
			continue;
		
		// player territory
		
		if (tile->owner != aiFactionId)
			continue;
		
		// base radius
		
		if (!map_has_item(tile, BIT_BASE_RADIUS))
			continue;
		
		// count network coverage
		
		eligibleTileCount++;
		
		
		if (map_has_item(tile, networkType))
		{
			coveredTileCount++;
		}
		
	}
	
	double networkDensity = (double)coveredTileCount / (double)eligibleTileCount;
	
	networkDemand = std::max(0.0, 1.0 - networkDensity / conf.ai_terraforming_networkDensityThreshold);
	
	// populate terraforming site lists
	
	for (MAP *tile = *MapTiles; tile < *MapTiles + *MapAreaTiles; tile++)
	{
		TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);
		
		if (tileTerraformingInfo.availableTerraformingSite)
		{
			terraformingSites.push_back(tile);
		}
		
	}
	
	if (DEBUG)
	{
		debug("\tterraformingSites\n");
		for (MAP *tile : terraformingSites)
		{
			debug("\t\t%s ocean=%d\n", getLocationString(tile).c_str(), is_ocean(tile));
		}
		
	}
	
	// populate base projected sizes
	
	for (int baseId : aiData.baseIds)
	{
		BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);
		
		// 10 turns and mininum 3 pop
		
		baseTerraformingInfo.projectedPopSize = std::max(3, getBaseProjectedSize(baseId, 10));
		
	}
	
	// populate base conventionalTerraformingSites
	
	for (int baseId : aiData.baseIds)
	{
		MAP *baseTile = getBaseMapTile(baseId);
		BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);
		
		for (MAP *baseRadiusTile : getBaseRadiusTiles(baseTile, false))
		{
			TileTerraformingInfo &baseRadiusTileTerraformingInfo = getTileTerraformingInfo(baseRadiusTile);
			
			// conventional terraforming site
			
			if (!baseRadiusTileTerraformingInfo.availableBaseTerraformingSite)
				continue;
			
			// not worked by other base
			
			if (!(baseRadiusTileTerraformingInfo.workedBaseId == -1 || baseRadiusTileTerraformingInfo.workedBaseId == baseId))
				continue;
			
			// add to base tiles
			
			baseTerraformingInfo.terraformingSites.push_back(baseRadiusTile);
			
		}
		
	}
	
	// populate base minimal yields
	
	for (int baseId : aiData.baseIds)
	{
		BaseTerraformingInfo &baseTerraformingInfo = getBaseTerraformingInfo(baseId);
		
		std::vector<MAP *> baseWorkedTiles = getBaseWorkedTiles(baseId);
		
		if (baseWorkedTiles.empty())
		{
			baseTerraformingInfo.minimalNutrientYield = 0;
			baseTerraformingInfo.minimalMineralYield = 0;
			baseTerraformingInfo.minimalEnergyYield = 0;
		}
		else
		{
			baseTerraformingInfo.minimalNutrientYield = INT_MAX;
			baseTerraformingInfo.minimalMineralYield = INT_MAX;
			baseTerraformingInfo.minimalEnergyYield = INT_MAX;
			
			for (MAP *tile : getBaseWorkedTiles(baseId))
			{
				baseTerraformingInfo.minimalNutrientYield = std::min(baseTerraformingInfo.minimalNutrientYield, mod_crop_yield(aiFactionId, baseId, getX(tile), getY(tile), 0));
				baseTerraformingInfo.minimalMineralYield = std::min(baseTerraformingInfo.minimalMineralYield, mod_mine_yield(aiFactionId, baseId, getX(tile), getY(tile), 0));
				baseTerraformingInfo.minimalEnergyYield = std::min(baseTerraformingInfo.minimalEnergyYield, mod_energy_yield(aiFactionId, baseId, getX(tile), getY(tile), 0));
			}
			
		}
		
	}
	
}

/*
Checks and removes redundant orders.
*/
void cancelRedundantOrders()
{
	for (int id : aiData.formerVehicleIds)
	{
		if (isVehicleTerrafomingOrderCompleted(id))
		{
			setVehicleOrder(id, ORDER_NONE);
		}

	}

}

void generateTerraformingRequests()
{
	// conventional requests
	
	debug("CONVENTIONAL\n");
	
	significantTerraformingRequestLocations.clear();
	
	for (int baseId : aiData.baseIds)
	{
		generateBaseConventionalTerraformingRequests(baseId);
	}
	
	// remove tile overlapping conventional terraforming requests
	
	std::sort(terraformingRequests.begin(), terraformingRequests.end(), compareConventionalTerraformingRequests);
	
	robin_hood::unordered_flat_set<MAP *> terraformingRequestLocations;
	
	for
	(
		std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestIterator = terraformingRequests.begin();
		terraformingRequestIterator != terraformingRequests.end();
	)
	{
		TERRAFORMING_REQUEST &terraformingRequest = *terraformingRequestIterator;

		if (terraformingRequestLocations.find(terraformingRequest.tile) != terraformingRequestLocations.end())
		{
			terraformingRequestIterator = terraformingRequests.erase(terraformingRequestIterator);
		}
		else
		{
			terraformingRequestLocations.insert(terraformingRequest.tile);
			terraformingRequestIterator++;
		}
		
	}
	
	// aquifer
	
	debug("AQUIFER\n");
	
	for (MAP *tile : terraformingSites)
	{
		// generate request
		
		generateAquiferTerraformingRequest(tile);
		
	}
	
	// raise workable tiles to increase energy output
	
	debug("RAISE LAND TERRAFORMING_REQUESTS\n");
	
	for (MAP *tile : terraformingSites)
	{
		// generate request
		
		generateRaiseLandTerraformingRequest(tile);
		
	}
	
	// network
	
	debug("NETWORK TERRAFORMING_REQUESTS\n");
	
	for (MAP *tile : terraformingSites)
	{
		// land
		
		if (is_ocean(tile))
			continue;
		
		// generate request
		
		generateNetworkTerraformingRequest(tile);
		
	}
	
	// sensors
	
	debug("SENSOR TERRAFORMING_REQUESTS\n");
	
	for (MAP *tile : terraformingSites)
	{
		// generate request
		
		generateSensorTerraformingRequest(tile);
		
	}
	
	// land bridges
	
	if (*CurrentTurn > 20)
	{
		debug("LAND BRIDGE TERRAFORMING_REQUESTS\n");
		generateLandBridgeTerraformingRequests();
	}
	
}

/**
Generates conventional terraforming request.
*/
void generateBaseConventionalTerraformingRequests(int baseId)
{
	debug("generateBaseConventionalTerraformingRequests - %s\n", Bases[baseId].name);
	
	BASE *base = getBase(baseId);
	BaseTerraformingInfo &baseTerraformingInfo = baseTerraformingInfos[baseId];
	
	// generate all possible requests
	
	debug("\tavailableBaseTerraformingRequests\n");
	
	std::vector<TERRAFORMING_REQUEST> availableBaseTerraformingRequests;
	
	for (MAP *tile : baseTerraformingInfo.terraformingSites)
	{
		debug("\t\t%s\n", getLocationString(tile).c_str());
		
		// exclude already selected significant request location
		
		if (significantTerraformingRequestLocations.find(tile) != significantTerraformingRequestLocations.end())
			continue;
		
		TileInfo &tileInfo = aiData.getTileInfo(tile);
		
		for (TERRAFORMING_OPTION *option : BASE_TERRAFORMING_OPTIONS[tileInfo.ocean])
		{
			// rocky option requires rocky tile
			
			if (option->rocky && !tile->is_rocky())
				continue;
			
			// do not build mining platrofm for bases in possession of undeveloped land rocky spots
			
			if (option == &TO_MINING_PLATFORM && baseTerraformingInfo.unworkedLandRockyTileCount > 0)
				continue;
			
			// generate request
			
			TERRAFORMING_REQUEST terraformingRequest = calculateConventionalTerraformingScore(baseId, tile, option);
			
			// not generated or not positive score
			
			if (terraformingRequest.firstAction == -1)
				continue;
			
			// collect
			
			availableBaseTerraformingRequests.push_back(terraformingRequest);
			
			debug
			(
				"\t\t\t%-20s %d-%d-%d"
				" terraformingTime=%5.2f"
				" improvementIncome=%5.2f"
				" fitnessScore=%5.2f"
				" income=%5.2f"
				" gain=%5.2f"
				"\n"
				, option->name
				, terraformingRequest.yield.nutrient, terraformingRequest.yield.mineral, terraformingRequest.yield.energy
				, terraformingRequest.terraformingTime
				, terraformingRequest.improvementIncome
				, terraformingRequest.fitnessScore
				, terraformingRequest.income
				, terraformingRequest.gain
			);
			
		}
		
	}
	
	// sort by score
	
	std::sort(availableBaseTerraformingRequests.begin(), availableBaseTerraformingRequests.end(), compareConventionalTerraformingRequests);
	
	// select requests
	
	debug("\tselectedTerraformingRequests\n");
	
	std::vector<TERRAFORMING_REQUEST> selectedBaseTerraformingRequests;
	
	// set projected population
	
	int currentPopSize = base->pop_size;
	base->pop_size = baseTerraformingInfo.projectedPopSize;
	
	robin_hood::unordered_flat_set<MAP *> appliedLocations;
	robin_hood::unordered_flat_set<MAP *> selectedLocations;
	
	for (TERRAFORMING_REQUEST &terraformingRequest : availableBaseTerraformingRequests)
	{
		// skip allready selected locations
		
		if (selectedLocations.find(terraformingRequest.tile) != selectedLocations.end())
			continue;
		
		// apply changes
		
		applyMapState(terraformingRequest.improvedMapState, terraformingRequest.tile);
		appliedLocations.insert(terraformingRequest.tile);
		
		// compute base
		
		computeBase(baseId, true);
		
		// verify all already selected tiles are still worked
		// stop processing if not
		
		bool allWorked = true;

		for (MAP *selectedTile : selectedLocations)
		{
			if (!isBaseWorkedTile(baseId, selectedTile))
			{
				allWorked = false;
				break;
			}
		}
		
		if (!allWorked)
			break;
		
		// verify tile is worked
		// stop processing if not
		
		if (!isBaseWorkedTile(baseId, terraformingRequest.tile))
			break;
		
		// select request
		
		selectedBaseTerraformingRequests.push_back(terraformingRequest);
		selectedLocations.insert(terraformingRequest.tile);
		
		debug("\t\t\t%s %s\n", getLocationString(terraformingRequest.tile).c_str(), terraformingRequest.option->name);
		
	}
	
	// restore all applied changes
	
	for (MAP *tile : appliedLocations)
	{
		restoreTileMapState(tile);
	}
	
	// restore population
	
	base->pop_size = currentPopSize;
	computeBase(baseId, true);
	
	// record existing significant request locations
	
	for (TERRAFORMING_REQUEST &terraformingRequest : selectedBaseTerraformingRequests)
	{
		significantTerraformingRequestLocations.insert(terraformingRequest.tile);
	}
	
	// store significant requests
	
	terraformingRequests.insert(terraformingRequests.end(), selectedBaseTerraformingRequests.begin(), selectedBaseTerraformingRequests.end());
	
}

void generateLandBridgeTerraformingRequests()
{
	debug("generateLandBridgeTerraformingRequests\n");
	
	robin_hood::unordered_flat_map<int, MapIntValue> bridgeRequests;
	
	for (MAP *tile : raiseableCoasts)
	{
		int bridgeLandCluster = -1;
		int bridgeRange = -1;
		
		for (MAP *rangeTile : getRangeTiles(tile, 10, false))
		{
			bool rangeTileOcean = is_ocean(rangeTile);
			int rangeTileLandCluster = getLandCluster(rangeTile);
			
			// land
			
			if (rangeTileOcean)
				continue;
			
			// different cluster
			
			if (isSameLandCluster(tile, rangeTile))
				continue;
			
			// lock the minRange
			
			bridgeLandCluster = rangeTileLandCluster;
			bridgeRange = getRange(tile, rangeTile);
			break;
			
		}
		
		if (bridgeLandCluster == -1)
			continue;
		
		if (bridgeRequests.count(bridgeLandCluster) == 0)
		{
			MapIntValue mapValue(nullptr, INT_MAX);
			bridgeRequests.insert({bridgeLandCluster, mapValue});
		}
		
		if (bridgeRange < bridgeRequests.at(bridgeLandCluster).value)
		{
			bridgeRequests.at(bridgeLandCluster).tile = tile;
			bridgeRequests.at(bridgeLandCluster).value = bridgeRange;
		}
		
	}
	
	for (robin_hood::pair<int, MapIntValue> &bridgeRequestEntry : bridgeRequests)
	{
		MapIntValue bridgeRequest = bridgeRequestEntry.second;
		
		// improvementIncome
		
		double improvementIncome =
			conf.ai_terraforming_landBridgeValue
			* getExponentialCoefficient(conf.ai_terraforming_landBridgeRangeScale, bridgeRequest.value - 2)
		;
		
		// terraformingTime
		
		MapState improvedMapState;
		setMapState(improvedMapState, bridgeRequest.tile);
		double terraformingTime = getTerraformingTime(improvedMapState, FORMER_RAISE_LAND);
		
		// gain
		
		double income = improvementIncome;
		double gain = getTerraformingGain(income, terraformingTime);
		
		debug
		(
			"\t%s"
			" gain=%5.2f"
			" terraformingTime=%5.2f"
			" conf.ai_terraforming_landBridgeValue=%5.2f"
			" conf.ai_terraforming_landBridgeRangeScale=%5.2f"
			" bridgeRequest.value=%d"
			" income=%5.2f"
			"\n"
			, getLocationString(bridgeRequest.tile).c_str()
			, gain
			, terraformingTime
			, conf.ai_terraforming_landBridgeValue
			, conf.ai_terraforming_landBridgeRangeScale
			, bridgeRequest.value
			, income
		);
		
		TERRAFORMING_REQUEST terraformingRequest(bridgeRequest.tile, &TO_RAISE_LAND);
		terraformingRequest.improvedMapState = improvedMapState;
		terraformingRequest.firstAction = FORMER_RAISE_LAND;
		terraformingRequest.terraformingTime = terraformingTime;
		terraformingRequest.improvementIncome = improvementIncome;
		terraformingRequest.income = income;
		terraformingRequest.gain = gain;
		
		terraformingRequests.push_back(terraformingRequest);
		
	}
	
}

/*
Generate request for aquifer.
*/
void generateAquiferTerraformingRequest(MAP *tile)
{
	TERRAFORMING_REQUEST terraformingRequest = calculateAquiferTerraformingScore(tile);
	
	if (terraformingRequest.firstAction == -1)
		return;
	
	terraformingRequests.push_back(terraformingRequest);

}

/*
Generate request to raise energy collection.
*/
void generateRaiseLandTerraformingRequest(MAP *tile)
{
	TERRAFORMING_REQUEST terraformingRequest = calculateRaiseLandTerraformingScore(tile);
	
	if (terraformingRequest.firstAction == -1)
		return;
	
	terraformingRequests.push_back(terraformingRequest);
	
}

/**
Generates request for network (road/tube).
*/
void generateNetworkTerraformingRequest(MAP *tile)
{
	TERRAFORMING_REQUEST terraformingRequest = calculateNetworkTerraformingScore(tile);
	
	if (terraformingRequest.firstAction == -1)
		return;
	
	terraformingRequests.push_back(terraformingRequest);
	
}

/**
Generate request for sensor.
*/
void generateSensorTerraformingRequest(MAP *tile)
{
	TERRAFORMING_REQUEST terraformingRequest = calculateSensorTerraformingScore(tile);
	
	if (terraformingRequest.firstAction == -1)
		return;
	
	terraformingRequests.push_back(terraformingRequest);
	
}

/*
Sort terraforming requests by score.
*/
void sortTerraformingRequests()
{
	debug("sortTerraformingRequests - %s\n", getMFaction(aiFactionId)->noun_faction);

	// sort requests

	std::sort(terraformingRequests.begin(), terraformingRequests.end(), compareTerraformingRequests);

	if (DEBUG)
	{
		for (const TERRAFORMING_REQUEST &terraformingRequest : terraformingRequests)
		{
			debug
			(
				"\t%s"
				" %-20s"
				" %6.2f"
				"\n"
				, getLocationString(terraformingRequest.tile).c_str()
				, terraformingRequest.option->name
				, terraformingRequest.gain
			);
		}

	}

}

/*
Removes terraforming requests violating proximity rules.
*/
void applyProximityRules()
{
	// apply proximity rules

	for
	(
		std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestsIterator = terraformingRequests.begin();
		terraformingRequestsIterator != terraformingRequests.end();
	)
	{
		TERRAFORMING_REQUEST *terraformingRequest = &(*terraformingRequestsIterator);
		
		assert(isOnMap(terraformingRequest->tile));

		// proximity rule

		bool tooClose = false;

		robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(terraformingRequest->firstAction);
		if (proximityRulesIterator != PROXIMITY_RULES.end())
		{
			const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

			// there is higher ranked similar action

			tooClose =
				hasNearbyTerraformingRequestAction
				(
					terraformingRequests.begin(),
					terraformingRequestsIterator,
					terraformingRequest->firstAction,
					getX(terraformingRequest->tile),
					getY(terraformingRequest->tile),
					proximityRule->buildingDistance
				)
			;

		}

		if (tooClose)
		{
			// delete this request and advance iterator

			terraformingRequestsIterator = terraformingRequests.erase(terraformingRequestsIterator);

		}
		else
		{
			// advance iterator

			terraformingRequestsIterator++;

		}

	}

}

/*
Removes terraforming requests in terraformed tiles.
*/
void removeTerraformedTiles()
{
	for
	(
		std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestsIterator = terraformingRequests.begin();
		terraformingRequestsIterator != terraformingRequests.end();
	)
	{
		TERRAFORMING_REQUEST &terraformingRequest = *terraformingRequestsIterator;
		TileTerraformingInfo &terraformingRequestTileInfo = getTileTerraformingInfo(terraformingRequest.tile);

		if (terraformingRequestTileInfo.terraformed)
		{
			terraformingRequestsIterator = terraformingRequests.erase(terraformingRequestsIterator);
		}
		else
		{
			terraformingRequestsIterator++;
		}

	}

}

void assignFormerOrders()
{
	debug("assignFormerOrders - %s\n", MFactions[aiFactionId].noun_faction);
	
	// distribute orders
	
	robin_hood::unordered_flat_set<FORMER_ORDER *> assignedOrders;
	robin_hood::unordered_flat_map<TERRAFORMING_REQUEST *, TerraformingRequestAssignment> assignments;
	
	bool changed;
	
	do
	{
		changed = false;
		
		for (FORMER_ORDER &formerOrder : formerOrders)
		{
			int vehicleId = formerOrder.vehicleId;
			VEH *vehicle = &(Vehicles[vehicleId]);
			int triad = vehicle->triad();
			MAP *vehicleTile = getVehicleMapTile(vehicleId);
			
			// skip assigned
			
			if (assignedOrders.count(&formerOrder) != 0)
				continue;
			
			debug("\t[%4d] %s\n", vehicleId, getLocationString({vehicle->x, vehicle->y}).c_str());
			
			// find best terraformingRequest
			
			TERRAFORMING_REQUEST *bestTerraformingRequest = nullptr;
			double bestTerraformingRequestTravelTime = 0.0;
			double bestTerraformingRequestGain = 0.0;
			
			for (TERRAFORMING_REQUEST &terraformingRequest : terraformingRequests)
			{
				MAP *tile = terraformingRequest.tile;
				bool tileOcean = is_ocean(tile);
				
				// proper triad
				
				if ((tileOcean && triad == TRIAD_LAND) || (!tileOcean && triad == TRIAD_SEA))
					continue;
				
				// same cluster
				
				if ((triad == TRIAD_SEA && !isSameSeaCluster(vehicleTile, tile)) || (triad == TRIAD_LAND && !isSameLandTransportedCluster(vehicleTile, tile)))
					continue;
				
				// reachable
				
				if (!isVehicleDestinationReachable(vehicleId, tile))
					continue;
				
				// travelTime
				
				double travelTime = conf.ai_terraforming_travel_time_multiplier * getVehicleATravelTime(vehicleId, vehicleTile, tile);
				
				// for assigned request check if this terraformer is closer
				
				if (assignments.find(&terraformingRequest) != assignments.end())
				{
					TerraformingRequestAssignment &assignment = assignments.at(&terraformingRequest);
					
					// not closer than already assigned former
					
					if (travelTime >= assignment.travelTime)
					{
						debug
						(
							"\t\t\tnot closer than [%4d] %s"
							"\n"
							, assignment.formerOrder->vehicleId
							, getLocationString(getVehicleMapTile(assignment.formerOrder->vehicleId)).c_str()
						);
						
						continue;
						
					}
					
				}
				
				// estimate former incomeGain for this improvement
				
				double effectiveTravelTime = conf.ai_terraforming_travel_time_multiplier * travelTime;
				double effectiveTotalTime = effectiveTravelTime + terraformingRequest.terraformingTime;
				double gain = getGainDelay(getGainIncome(terraformingRequest.income), effectiveTotalTime);
				
				// update best
				
				if (gain > bestTerraformingRequestGain)
				{
					bestTerraformingRequest = &terraformingRequest;
					bestTerraformingRequestTravelTime = travelTime;
					bestTerraformingRequestGain = gain;
				}
				
				debug
				(
					"\t\t->%s %-16s"
					" income=%5.2f"
					" travelTime=%7.2f"
					" terraformingTime=%5.2f"
					" gain=%5.2f"
					"\n"
					, getLocationString(tile).c_str(), terraformingRequest.option->name
					, terraformingRequest.income
					, travelTime
					, terraformingRequest.terraformingTime
					, gain
				);
				
			}
			
			// not found

			if (bestTerraformingRequest == nullptr)
			{
				debug("\t\tbestTerraformingRequest not found\n");
				continue;
			}
			
			// remove existing assignment if any
			
			if (assignments.count(bestTerraformingRequest) != 0)
			{
				TerraformingRequestAssignment &assignment = assignments.at(bestTerraformingRequest);
				
				debug
				(
					"\t\t\tremove existing assignment [%4d] %s"
					"\n"
					, assignment.formerOrder->vehicleId
					, getLocationString(getVehicleMapTile(assignment.formerOrder->vehicleId)).c_str()
				);
				
				assignedOrders.erase(assignment.formerOrder);
				
			}
			
			debug
			(
				"\t\t\tbest: %s"
				"\n"
				, getLocationString(bestTerraformingRequest->tile).c_str()
			);
			
			// set assignment
			
			assignments[bestTerraformingRequest] = {&formerOrder, bestTerraformingRequestTravelTime};
			assignedOrders.insert(&formerOrder);
			changed = true;
			
		}
		
	}
	while (changed);
	
	// set tasks
	
	for (robin_hood::pair<TERRAFORMING_REQUEST *, TerraformingRequestAssignment> &assignmentEntry : assignments)
	{
		TERRAFORMING_REQUEST *terraformingRequest = assignmentEntry.first;
		TerraformingRequestAssignment &assignment = assignmentEntry.second;
		FORMER_ORDER *formerOrder = assignment.formerOrder;
		
		formerOrder->tile = terraformingRequest->tile;
		formerOrder->action = terraformingRequest->firstAction;
		
		debug
		(
			"\t[%4d] %s -> %s"
			"\n"
			, formerOrder->vehicleId
			, getLocationString(getVehicleMapTile(formerOrder->vehicleId)).c_str()
			, getLocationString(terraformingRequest->tile).c_str()
		);
		
	}
	
	// populate terraformingProductionRequests
	
	debug("terraformingProductionRequests - %s\n", getMFaction(aiFactionId)->noun_faction);
	
	std::vector<FormerRequest> &formerRequests = aiData.production.formerRequests;
	formerRequests.clear();
	
	// store requests
	
	for (TERRAFORMING_REQUEST &terraformingRequest : terraformingRequests)
	{
		// skip assigned
		
		if (assignments.find(&terraformingRequest) != assignments.end())
			continue;
		
		// add request
		
		formerRequests.push_back({terraformingRequest.tile, terraformingRequest.option, terraformingRequest.terraformingTime, terraformingRequest.income});
		
	}
	
}

void finalizeFormerOrders()
{
	// iterate former orders
	
	for (FORMER_ORDER &formerOrder : formerOrders)
	{
		if (formerOrder.action == -1)
		{
			int vehicleId = formerOrder.vehicleId;
			VEH *vehicle = &(Vehicles[vehicleId]);
			
			// supported
			
			if (vehicle->home_base_id < 0)
			{
				// hold independent former without order
				setTask(Task(vehicleId, TT_HOLD));
			}
			else
			{
				// kill supported formers without orders
				setTask(Task(vehicleId, TT_KILL));
			}
			
		}
		else
		{
			transitVehicle(Task(formerOrder.vehicleId, TT_TERRAFORM, formerOrder.tile, formerOrder.tile, nullptr, -1, formerOrder.action));
		}
		
	}
	
}

/**
Selects best terraforming option around given base and calculates its terraforming score.
*/
TERRAFORMING_REQUEST calculateConventionalTerraformingScore(int baseId, MAP *tile, TERRAFORMING_OPTION const *option)
{
	assert(isOnMap(tile));
	
	bool ocean = is_ocean(tile);
	bool rocky = (map_rockiness(tile) == 2);
	
	TERRAFORMING_REQUEST terraformingRequest(tile, option);
	
	// matching surface
	
	if (option->ocean != ocean)
		return terraformingRequest;
	
	// matching rockiness
	
	if (option->rocky && !(!ocean && rocky))
		return terraformingRequest;
	
	// do not level terrain for small bases (they desperately need nutrients and may destroy perfect rocky mine spots for that)
	
	if (Bases[baseId].pop_size <= 2 && !option->rocky && (!ocean && rocky))
		return terraformingRequest;
	
	// check if required action technology is available
	
	if (option->requiredAction != -1 && !isTerraformingAvailable(tile, option->requiredAction))
		return terraformingRequest;
	
	// initialize variables
	
	int firstAction = -1;
	double terraformingTime = 0.0;
	double penaltyScore = 0.0;
	bool levelTerrain = false;
	
	// store tile values
	
	MapState improvedMapState;
	setMapState(improvedMapState, tile);
	
	// process actions
	
	int availableActionCount = 0;
	int completedActionCount = 0;
	
	for(int action : option->actions)
	{
		// skip unavailable action
		
		if (!isTerraformingAvailable(tile, action))
			continue;
		
		// increment available actions
		
		availableActionCount++;
		
		// skip completed action
		
		if (isTerraformingCompleted(tile, action))
		{
			completedActionCount++;
			continue;
		}
		
		// remove fungus if needed
		
		if (((improvedMapState.items & BIT_FUNGUS) != 0) && isRemoveFungusRequired(action))
		{
			int removeFungusAction = FORMER_REMOVE_FUNGUS;
			
			// store first action
			
			if (firstAction == -1)
			{
				firstAction = removeFungusAction;
			}
			
			// compute terraforming time
			
			terraformingTime += getTerraformingTime(improvedMapState, removeFungusAction);
			
			// generate terraforming change
			
			generateTerraformingChange(improvedMapState, removeFungusAction);
			
			// add penalty for nearby forest/kelp (except rocky)
			
			if (!rocky)
			{
				int nearbyForestKelpCount = nearby_items(getX(tile), getY(tile), 1, 9, (ocean ? BIT_FARM : BIT_FOREST));
				penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;
			}
			
		}
		
		// level terrain if needed
		
		if (!ocean && (improvedMapState.rockiness == 2) && isLevelTerrainRequired(ocean, action))
		{
			int levelTerrainAction = FORMER_LEVEL_TERRAIN;
			levelTerrain = true;
			
			// store first action
			
			if (firstAction == -1)
			{
				firstAction = levelTerrainAction;
			}
			
			// compute terraforming time
			
			terraformingTime += getTerraformingTime(improvedMapState, levelTerrainAction);
			
			// generate terraforming change
			
			generateTerraformingChange(improvedMapState, levelTerrainAction);
			
		}
		
		// execute main action
		
		// store first action
		
		if (firstAction == -1)
		{
			firstAction = action;
		}
		
		// compute terraforming time
		
		terraformingTime += getTerraformingTime(improvedMapState, action);
		
		// generate terraforming change
		
		generateTerraformingChange(improvedMapState, action);
		
		// add penalty for nearby forest/kelp
		
		if (ocean && action == FORMER_FARM)
		{
			int nearbyKelpCount = nearby_items(getX(tile), getY(tile), 1, 9, BIT_FARM);
			penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyKelpCount;
		}
		else if (!ocean && action == FORMER_FOREST)
		{
			int nearbyForestCount = nearby_items(getX(tile), getY(tile), 1, 9, BIT_FOREST);
			penaltyScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestCount;
		}
		
	}
	
	// ignore unpopulated options
	
	if (firstAction == -1)
		return terraformingRequest;
	
	// generate and store yield
	
	applyMapState(improvedMapState, tile);
	terraformingRequest.yield =
		{
			mod_crop_yield(aiFactionId, baseId, getX(tile), getY(tile), 0),
			mod_mine_yield(aiFactionId, baseId, getX(tile), getY(tile), 0),
			mod_energy_yield(aiFactionId, baseId, getX(tile), getY(tile), 0),
		}
	;
	restoreTileMapState(tile);
	
	// calculate yield income
	
	double improvementIncome = computeBaseTileImprovementGain(baseId, tile, improvedMapState, option->area);
	
	// special yield computation for area effects
	
	improvementIncome += estimateCondenserExtraYieldScore(tile, &(option->actions));
	
	// ignore options not increasing income
	
	if (improvementIncome <= 0.0)
		return terraformingRequest;
	
	// fitness
	
	double fitnessScore = getFitnessScore(tile, option->requiredAction, levelTerrain);
	fitnessScore += penaltyScore;
	
	// summarize score
	
	double income =
		+ improvementIncome
		+ fitnessScore
	;
	
	// gain
	
	double gain = getTerraformingGain(income, terraformingTime);

//	debug
//	(
//		"\t\t\t%-20s %d-%d-%d"
//		" terraformingTime=%5.2f"
//		" improvementIncome=%5.2f"
//		" fitnessScore=%5.2f"
//		" income=%5.2f"
//		" gain=%5.2f"
//		"\n"
//		, option->name
//		, terraformingRequest.yield.nutrient, terraformingRequest.yield.mineral, terraformingRequest.yield.energy
//		, terraformingTime
//		, improvementIncome
//		, fitnessScore
//		, income
//		, gain
//	)
//	;
	
	// update return values
	
	terraformingRequest.conventional = true;
	terraformingRequest.improvedMapState = improvedMapState;
	terraformingRequest.firstAction = firstAction;
	terraformingRequest.terraformingTime = terraformingTime;
	terraformingRequest.improvementIncome = improvementIncome;
	terraformingRequest.fitnessScore = fitnessScore;
	terraformingRequest.income = income;
	terraformingRequest.gain = gain;
	
	return terraformingRequest;
	
}

/*
Aquifer calculations.
*/
TERRAFORMING_REQUEST calculateAquiferTerraformingScore(MAP *tile)
{
	bool ocean = is_ocean(tile);
	
	TERRAFORMING_OPTION const *option = &TO_AQUIFER;
	TERRAFORMING_REQUEST terraformingRequest(tile, option);
	
	// initialize variables
	
	int action = FORMER_AQUIFER;
	
	// land only
	
	if (ocean)
		return terraformingRequest;
	
	// skip unavailable actions
	
	if (!isTerraformingAvailable(tile, action))
		return terraformingRequest;
	
	// do not generate request for completed action
	
	if (isTerraformingCompleted(tile, action))
		return terraformingRequest;
	
	// set map state
	
	MapState improvedMapState;
	setMapState(improvedMapState, tile);
	
	// compute terraforming time
	
	double terraformingTime = getTerraformingTime(improvedMapState, action);
	
	// special yield computation for aquifer
	
	double improvementIncome = estimateAquiferIncome(tile);
	
	// gain
	
	double income = improvementIncome;
	double gain = getTerraformingGain(income, terraformingTime);
	
	// update return values

	terraformingRequest.firstAction = action;
	terraformingRequest.improvedMapState = improvedMapState;
	terraformingRequest.terraformingTime = terraformingTime;
	terraformingRequest.improvementIncome = improvementIncome;
	terraformingRequest.income = income;
	terraformingRequest.gain = gain;
	
	debug("calculateTerraformingScore: %s\n", getLocationString(tile).c_str());
	debug
	(
		"\t%-10s"
		" terraformingTime=%5.2f"
		" improvementIncome=%5.2f"
		" income=%5.2f"
		" gain=%6.3f"
		"\n"
		, option->name
		, terraformingTime
		, improvementIncome
		, income
		, gain
	)
	;
	
	return terraformingRequest;
	
}

/*
Raise land calculations.
*/
TERRAFORMING_REQUEST calculateRaiseLandTerraformingScore(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	
	TERRAFORMING_OPTION const *option = &TO_RAISE_LAND;
	TERRAFORMING_REQUEST terraformingRequest(tile, option);
	
	// land only
	
	if (ocean)
		return terraformingRequest;
	
	// verify we have enough money
	
	int cost = terraform_cost(x, y, aiFactionId);
	if (cost > Factions[aiFactionId].energy_credits / 10)
		return terraformingRequest;
	
	// initialize variables
	
	int action = FORMER_RAISE_LAND;
	int firstAction = action;
	
	// skip unavailable actions
	
	if (!isTerraformingAvailable(tile, action))
		return terraformingRequest;
	
	// set action
	
	if (firstAction == -1)
	{
		firstAction = action;
	}
	
	// set map state
	
	MapState improvedMapState;
	setMapState(improvedMapState, tile);
	
	// compute terraforming time
	
	double terraformingTime = getTerraformingTime(improvedMapState, action);
	
	// skip if no action or no terraforming time
	
	if (firstAction == -1)
		return terraformingRequest;
	
	// special computation for raise land
	
	double improvementIncome = estimateRaiseLandIncome(tile, cost);
	
	// do not generate request without income
	
	if (improvementIncome <= 0.0)
		return terraformingRequest;
	
	// summarize income
	
	double income = improvementIncome;
	double gain = getTerraformingGain(income, terraformingTime);
	
	// update return values
	
	terraformingRequest.firstAction = firstAction;
	terraformingRequest.improvedMapState = improvedMapState;
	terraformingRequest.terraformingTime = terraformingTime;
	terraformingRequest.improvementIncome = income;
	terraformingRequest.income = income;
	terraformingRequest.gain = gain;

	debug("calculateTerraformingScore: %s\n", getLocationString(tile).c_str());
	debug
	(
		"\t%-10s"
		" terraformingTime=%5.2f"
		" improvementIncome=%5.2f"
		" income=%5.2f"
		" gain=%6.3f"
		"\n"
		, option->name
		, terraformingTime
		, improvementIncome
		, income
		, gain
	)
	;
	
	return terraformingRequest;
	
}

/*
Network calculations.
*/
TERRAFORMING_REQUEST calculateNetworkTerraformingScore(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	
	TERRAFORMING_OPTION const *option = &TO_NETWORK;
	TERRAFORMING_REQUEST terraformingRequest(tile, option);
	
	// land only
	
	if (ocean)
		return terraformingRequest;
	
	// initialize variables
	
	int firstAction = -1;
	double terraformingTime = 0.0;
	double fitnessScore = 0.0;
	
	// set map state
	
	MapState improvedMapState;
	setMapState(improvedMapState, tile);
	
	// process actions
	
	int action;
	
	if (!map_has_item(tile, BIT_ROAD))
	{
		action = FORMER_ROAD;
	}
	else if (!map_has_item(tile, BIT_MAGTUBE))
	{
		action = FORMER_MAGTUBE;
	}
	else
	{
		// everything is built
		return terraformingRequest;
	}
	
	// skip unavailable actions
	
	if (!isTerraformingAvailable(tile, action))
		return terraformingRequest;
	
	// remove fungus if needed
	
	if (((improvedMapState.items & BIT_FUNGUS) != 0) && isRemoveFungusRequired(action))
	{
		int removeFungusAction = FORMER_REMOVE_FUNGUS;
		
		// store first action
		
		if (firstAction == -1)
		{
			firstAction = removeFungusAction;
		}
		
		// compute terraforming time
		
		terraformingTime += getTerraformingTime(improvedMapState, removeFungusAction);
		
		// add penalty for nearby forest/kelp
		
		int nearbyForestKelpCount = nearby_items(x, y, 1, 9, (ocean ? BIT_FARM : BIT_FOREST));
		fitnessScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;
		
		// generate terraforming changes
		
		generateTerraformingChange(improvedMapState, removeFungusAction);
		
	}
	
	// store first action
	
	if (firstAction == -1)
	{
		firstAction = action;
	}
	
	// compute terraforming time
	
	terraformingTime += getTerraformingTime(improvedMapState, action);
	
	// special network bonus
	
	double improvementIncome = (1.0 + networkDemand) * calculateNetworkScore(tile, action);
	
	// do not create request for zero score
	
	if (improvementIncome <= 0.0)
		return terraformingRequest;
	
	// gain
	
	double income =
		+ improvementIncome
		+ fitnessScore
	;
	
	double gain = getTerraformingGain(income, terraformingTime);
	
	// update return values
	
	terraformingRequest.firstAction = action;
	terraformingRequest.improvedMapState = improvedMapState;
	terraformingRequest.terraformingTime = terraformingTime;
	terraformingRequest.improvementIncome = improvementIncome;
	terraformingRequest.income = income;
	terraformingRequest.gain = gain;
	
	debug("calculateTerraformingScore: %s\n", getLocationString(tile).c_str());
	debug
	(
		"\t%-10s"
		" terraformingTime=%5.2f"
		" improvementIncome=%5.2f"
		" fitnessScore=%5.2f"
		" income=%5.2f"
		" gain=%6.3f"
		"\n"
		, option->name
		, terraformingTime
		, improvementIncome
		, fitnessScore
		, income
		, gain
	)
	;
	
	return terraformingRequest;
	
}

TERRAFORMING_REQUEST calculateSensorTerraformingScore(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);
	
	TERRAFORMING_OPTION const *option = ocean ? &TO_SEA_SENSOR : &TO_LAND_SENSOR;
	TERRAFORMING_REQUEST terraformingRequest(tile, option);
	
	// initialize variables
	
	int firstAction = -1;
	double terraformingTime = 0.0;
	double improvementScore = 0.0;
	
	// process actions
	
	int action;
	
	if (!map_has_item(tile, BIT_SENSOR))
	{
		action = FORMER_SENSOR;
	}
	else
	{
		// everything is built
		return terraformingRequest;
	}
	
	// skip unavailable actions
	
	if (!isTerraformingAvailable(tile, action))
		return terraformingRequest;
	
	// set map state
	
	MapState improvedMapState;
	setMapState(improvedMapState, tile);
	
	// remove fungus if needed
	
	if (map_has_item(tile, BIT_FUNGUS) && isRemoveFungusRequired(action))
	{
		int removeFungusAction = FORMER_REMOVE_FUNGUS;
		
		// store first action
		
		if (firstAction == -1)
		{
			firstAction = removeFungusAction;
		}
		
		// compute terraforming time
		
		terraformingTime += getTerraformingTime(tileTerraformingInfo.mapState, removeFungusAction);
		generateTerraformingChange(improvedMapState, removeFungusAction);
	
		
		// add penalty for nearby forest/kelp
		
		int nearbyForestKelpCount = nearby_items(x, y, 1, 9, (ocean ? BIT_FARM : BIT_FOREST));
		improvementScore -= conf.ai_terraforming_nearbyForestKelpPenalty * nearbyForestKelpCount;
		
	}
	
	// store first action
	
	if (firstAction == -1)
	{
		firstAction = action;
	}
	
	// compute terraforming time
	
	terraformingTime += getTerraformingTime(improvedMapState, action);
	
	// special sensor bonus
	
	double improvementIncome = estimateSensorIncome(tile);
	
	// ignore option without income
	
	if (improvementIncome <= 0.0)
		return terraformingRequest;
	
	// calculate exclusivity bonus
	
	double fitnessScore = getFitnessScore(tile, option->requiredAction, false);
	
	// summarize income
	
	double income =
		improvementIncome
		+ fitnessScore
	;
	
	// gain
	
	double gain = getTerraformingGain(income, terraformingTime);
	
	// update return values

	terraformingRequest.firstAction = action;
	terraformingRequest.improvedMapState = improvedMapState;
	terraformingRequest.terraformingTime = terraformingTime;
	terraformingRequest.improvementIncome = improvementIncome;
	terraformingRequest.income = income;
	terraformingRequest.gain = gain;
	
	debug("calculateTerraformingScore: %s\n", getLocationString(tile).c_str());
	debug
	(
		"\t%-10s"
		" terraformingTime=%5.2f"
		" improvementIncome=%5.2f"
		" fitnessScore=%5.2f"
		" income=%5.2f"
		" gain=%6.3f"
		"\n"
		, option->name
		, terraformingTime
		, improvementIncome
		, fitnessScore
		, income
		, gain
	)
	;
	
	return terraformingRequest;
	
}

/**
Computes base tile improvement surplus effect.
*/
double computeBaseTileImprovementGain(int baseId, MAP *tile, MapState &improvedMapState, bool areaEffect)
{
	TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);
	
	// affected bases
	
	std::vector<int> affectedBaseIds = areaEffect ? tileTerraformingInfo.areaWorkableBaseIds : std::vector<int>{baseId};
	
	// compute improvement income
	
	bool worked = false;
	double sumImprovementGain = 0.0;
	
	for (int affectedBaseId : affectedBaseIds)
	{
		BASE *affectedBase = getBase(affectedBaseId);
		BaseTerraformingInfo &baseTerraformingInfo = baseTerraformingInfos.at(baseId);
		
		int currentPopSize = affectedBase->pop_size;
		affectedBase->pop_size = baseTerraformingInfo.projectedPopSize;
		computeBase(affectedBaseId, true);
		
		// old intake
		
		Resource oldResourceIntake2 = getBaseResourceIntake2(baseId);
		
		// apply improvement
		
		applyMapState(improvedMapState, tile);
		computeBase(affectedBaseId, true);
		
		// verify square is worked by this base for nonArea effect
		
		if (!areaEffect && affectedBaseId == baseId && isBaseWorkedTile(baseId, tile))
		{
			worked = true;
		}
		
		// new intake
		
		Resource newResourceIntake2 = getBaseResourceIntake2(baseId);
		
		// restore map
		
		restoreTileMapState(tile);
		affectedBase->pop_size = currentPopSize;
		computeBase(affectedBaseId, true);
		
		// accumulate improvementGain
		
		double improvementGain = getBaseImprovementGain(baseId, oldResourceIntake2, newResourceIntake2);
		sumImprovementGain += improvementGain;
		
	}
	
	// discard not worked tile
	
	if (!areaEffect && !worked)
		return 0.0;
	
	// return improvement income
	
	return sumImprovementGain;
	
}

/*
Determines whether terraforming is already completed in this tile.
*/
bool isTerraformingCompleted(MAP *tile, int action)
{
	return (tile->items & Terraform[action].bit);

}

/*
Determines whether vehicle order is a terraforming order and is already completed in this tile.
*/
bool isVehicleTerrafomingOrderCompleted(int vehicleId)
{
	VEH *vehicle = &(Vehicles[vehicleId]);

	if (!(vehicle->order >= ORDER_FARM && vehicle->order <= ORDER_PLACE_MONOLITH))
		return false;

	MAP *tile = getMapTile(vehicle->x, vehicle->y);

	return (tile->items & Terraform[vehicle->order - ORDER_FARM].bit);

}

/**
Determines whether terraforming can be done in this square with removing fungus and leveling terrain if needed.
Terraforming is considered available if this item is already built.
*/
bool isTerraformingAvailable(MAP *tile, int action)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);
	bool ocean = is_ocean(tile);
	bool oceanDeep = is_ocean_deep(tile);

	bool aquatic = MFactions[aiFactionId].rule_flags & RFLAG_AQUATIC;

	// action is not available if is already built

	if (map_has_item(tile, Terraform[action].bit))
		return false;

	// action is available only when enabled and researched

	if (!has_terra((FormerItem)action, ocean, aiFactionId))
		return false;

	// terraforming is not available in base square

	if (tile->items & BIT_BASE_IN_TILE)
		return false;

	// ocean improvements in deep ocean are available for aquatic faction with Adv. Ecological Engineering only

	if (oceanDeep)
	{
		if (!(aquatic && isFactionHasTech(aiFactionId, TECH_EcoEng2)))
			return false;
	}

	// special cases

	switch (action)
	{
	case FORMER_SOIL_ENR:
		// enricher requires farm
		if (~tile->items & BIT_FARM)
			return  false;
		break;
	case FORMER_FOREST:
//		// forest should not be built near another building forest
//		if (isNearbyForestUnderConstruction(x, y))
//			return  false;
		break;
	case FORMER_MAGTUBE:
		// tube requires road
		if (~tile->items & BIT_ROAD)
			return  false;
		break;
	case FORMER_REMOVE_FUNGUS:
		// removing fungus requires fungus
		if (~tile->items & BIT_FUNGUS)
			return  false;
		break;
	case FORMER_CONDENSER:
		// condenser should not be built near another building condenser
		if (isNearbyCondeserUnderConstruction(x, y))
			return  false;
		break;
	case FORMER_ECH_MIRROR:
		// mirror should not be built near another building mirror
		if (isNearbyMirrorUnderConstruction(x, y))
			return  false;
		break;
	case FORMER_THERMAL_BORE:
		// borehole should not be adjacent to another borehole
		if (isNearbyBoreholePresentOrUnderConstruction(x, y))
			return  false;
		break;
	case FORMER_AQUIFER:
		// aquifer cannot be drilled next to river
		if (isNearbyRiverPresentOrUnderConstruction(x, y))
			return  false;
		break;
	case FORMER_RAISE_LAND:
		// cannot raise beyond limit
		if (map_level(tile) == (is_ocean(tile) ? ALT_OCEAN_SHELF : ALT_THREE_ABOVE_SEA))
			return false;
		// raise land should not disturb other faction bases
		if (!isRaiseLandSafe(tile))
			return false;
		// raise land should not be built near another building raise
		if (isNearbyRaiseUnderConstruction(x, y))
			return false;
		break;
	case FORMER_LOWER_LAND:
		// lower land should not disturb other faction bases
		// TODO
		return false;
		break;
	case FORMER_SENSOR:
		// sensor don't need to be build too close to each other
		if (isNearbySensorPresentOrUnderConstruction(x, y))
			return  false;
		break;
	}

	return true;

}

/*
Determines whether fungus need to be removed before terraforming.
*/
bool isRemoveFungusRequired(int action)
{
	// no need to remove fungus for planting fungus

	if (action == FORMER_PLANT_FUNGUS)
		return false;

	// always remove fungus for basic improvements even with fungus improvement technology

	if (action >= FORMER_FARM && action <= FORMER_SOLAR)
		return true;

	// no need to remove fungus for road with fungus road technology

	if (action == FORMER_ROAD && isFactionHasTech(aiFactionId, Rules->tech_preq_build_road_fungus))
		return false;

	// for everything else remove fungus only without fungus improvement technology

	return !isFactionHasTech(aiFactionId, Rules->tech_preq_improv_fungus);

}

/*
Determines whether rocky terrain need to be leveled before terraforming.
*/
bool isLevelTerrainRequired(bool ocean, int action)
{
	return !ocean && (action == FORMER_FARM || action == FORMER_SOIL_ENR || action == FORMER_FOREST);

}

bool isVehicleConvoying(VEH *vehicle)
{
	return vehicle->order == ORDER_CONVOY;
}

bool isVehicleTerraforming(VEH *vehicle)
{
	return vehicle->order >= ORDER_FARM && vehicle->order <= ORDER_PLACE_MONOLITH;
}

bool isNearbyForestUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_FOREST);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);
	int range = proximityRule->buildingDistance;

	// check building item

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -(2 * range - abs(dx)); dy <= +(2 * range - abs(dx)); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedForest)
                return true;

        }

    }

    return false;

}

bool isNearbyCondeserUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_CONDENSER);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);
	int range = proximityRule->buildingDistance;

	// check terraformed condensers

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -(2 * range - abs(dx)); dy <= +(2 * range - abs(dx)); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedCondenser)
                return true;

        }

    }

    return false;

}

bool isNearbyMirrorUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_ECH_MIRROR);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);
	int range = proximityRule->buildingDistance;

	// check terraformed mirrors

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -(2 * range - abs(dx)); dy <= +(2 * range - abs(dx)); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (!tile)
				continue;

			if (getTileTerraformingInfo(tile).terraformedMirror)
			{
				debug("terraformedMirrorTiles found: %s\n", getLocationString({wrap(x + dx), y + dy}).c_str());
                return true;
			}

        }

    }

    return false;

}

bool isNearbyBoreholePresentOrUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_THERMAL_BORE);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check existing items

	if (isMapRangeHasItem(x, y, proximityRule->existingDistance, BIT_THERMAL_BORE))
		return true;

	// check building items

	int range = proximityRule->buildingDistance;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedBorehole)
                return true;

        }

    }

    return false;

}

bool isNearbyRiverPresentOrUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_AQUIFER);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check existing items

	if (isMapRangeHasItem(x, y, proximityRule->existingDistance, BIT_RIVER))
		return true;

	// check building items

	int range = proximityRule->buildingDistance;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedAquifer)
                return true;

        }

    }

    return false;

}

bool isNearbyRaiseUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_RAISE_LAND);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check building items

	int range = proximityRule->buildingDistance;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedRaise)
                return true;

        }

    }

    return false;

}

bool isNearbySensorPresentOrUnderConstruction(int x, int y)
{
	robin_hood::unordered_flat_map<int, PROXIMITY_RULE>::const_iterator proximityRulesIterator = PROXIMITY_RULES.find(FORMER_SENSOR);

	// do not do anything if proximity rule is not defined

	if (proximityRulesIterator == PROXIMITY_RULES.end())
		return false;

	// get proximity rule

	const PROXIMITY_RULE *proximityRule = &(proximityRulesIterator->second);

	// check existing items

	if (isMapRangeHasItem(x, y, proximityRule->existingDistance, BIT_SENSOR))
		return true;

	// check building items

	int range = proximityRule->buildingDistance;

	for (int dx = -2 * range; dx <= 2 * range; dx++)
	{
		for (int dy = -2 * range + abs(dx); dy <= 2 * range - abs(dx); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (getTileTerraformingInfo(tile).terraformedSensor)
                return true;

        }

    }

    return false;

}

/**
Calculates terraforming time based on faction, tile, and former abilities.
*/
double getTerraformingTime(MapState &mapState, int action)
{
	assert(action >= FORMER_FARM && action <= FORMER_MONOLITH);
	
	int rockiness = mapState.rockiness;
	int items = mapState.items;
	
	// get base terraforming time
	
	double terraformingTime = Terraform[action].rate;
	
	// adjust for terrain properties
	
	switch (action)
	{
	case FORMER_SOLAR:
	
		// rockiness increases solar collector construction time
		
		switch (rockiness)
		{
		case 1:
			terraformingTime += 2;
			break;
		case 2:
			terraformingTime += 4;
			break;
		}
		
		break;
		
	case FORMER_ROAD:
	case FORMER_MAGTUBE:
		
		// forest increases road construction time and ignores rockiness
		
		if (items & BIT_FOREST)
		{
			terraformingTime += 2;
		}
		else
		{
			// fungus increases road construction time
			
			if (items & BIT_FUNGUS)
			{
				terraformingTime += 2;
			}
			
			// rockiness increases road construction time
			
			switch (rockiness)
			{
			case 1:
				terraformingTime += 1;
				break;
			case 2:
				terraformingTime += 2;
				break;
			}
			
		}
		
		// river increases road construction time and ignores rockiness
		
		if (items & BIT_RIVER)
		{
			terraformingTime += 1;
		}
		
	}
	
	// adjust for former multipliers
	
	switch (action)
	{
	case FORMER_PLANT_FUNGUS:
		terraformingTime = ceil(terraformingTime / factionTerraformingInfo.averagePlantFungusTerraformingRateMultiplier);
		break;
	case FORMER_REMOVE_FUNGUS:
		terraformingTime = ceil(terraformingTime / factionTerraformingInfo.averageRemoveFungusTerraformingRateMultiplier);
		break;
	default:
		terraformingTime = ceil(terraformingTime / factionTerraformingInfo.averageNormalTerraformingRateMultiplier);
		break;
	}
	
	return terraformingTime;
	
}

/*
Calculates score for network actions (road, tube)
*/
double calculateNetworkScore(MAP *tile, int action)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	// ignore anything but road and tube

	if (!(action == FORMER_ROAD || action == FORMER_MAGTUBE))
		return 0.0;

	// ignore impossible actions

	if
	(
		(action == FORMER_ROAD && (tile->items & BIT_ROAD))
		||
		(action == FORMER_MAGTUBE && ((~tile->items & BIT_ROAD) || (tile->items & BIT_MAGTUBE)))
	)
		return 0.0;

	// get terrain improvement flag

	int improvementFlag;

	switch (action)
	{
	case FORMER_ROAD:
		improvementFlag = BIT_ROAD;
		break;
	case FORMER_MAGTUBE:
		improvementFlag = BIT_MAGTUBE;
		break;
	default:
		return 0.0;
	}

	// check items around the tile

	bool m[OFFSET_COUNT_ADJACENT];

	for (int offsetIndex = 1; offsetIndex < OFFSET_COUNT_ADJACENT; offsetIndex++)
	{
		MAP *adjacentTile = getMapTile(wrap(x + BASE_TILE_OFFSETS[offsetIndex][0]), y + BASE_TILE_OFFSETS[offsetIndex][1]);

		m[offsetIndex] = (adjacentTile && !is_ocean(adjacentTile) && (adjacentTile->items & (BIT_BASE_IN_TILE | improvementFlag)));

	}

	// don't build connection to nothing

	if(!m[1] && !m[2] && !m[3] && !m[4] && !m[5] && !m[6] && !m[7] && !m[8])
	{
		return 0.0;
	}

	// needs connection

	if
	(
		// opposite corners
		(m[2] && m[6] && (!m[3] || !m[5]) && (!m[7] || !m[1]))
		||
		(m[4] && m[8] && (!m[5] || !m[7]) && (!m[1] || !m[3]))
		||
		// do not connect next corners
//		// next corners
//		(m[2] && m[4] && !m[3] && (!m[5] || !m[7] || !m[1]))
//		||
//		(m[4] && m[6] && !m[5] && (!m[7] || !m[1] || !m[3]))
//		||
//		(m[6] && m[8] && !m[7] && (!m[1] || !m[3] || !m[5]))
//		||
//		(m[8] && m[2] && !m[1] && (!m[3] || !m[5] || !m[7]))
//		||
		// opposite sides
		(m[1] && m[5] && !m[3] && !m[7])
		||
		(m[3] && m[7] && !m[5] && !m[1])
		||
		// side to corner
		(m[1] && m[4] && !m[3] && (!m[5] || !m[7]))
		||
		(m[1] && m[6] && !m[7] && (!m[3] || !m[5]))
		||
		(m[3] && m[6] && !m[5] && (!m[7] || !m[1]))
		||
		(m[3] && m[8] && !m[1] && (!m[5] || !m[7]))
		||
		(m[5] && m[8] && !m[7] && (!m[1] || !m[3]))
		||
		(m[5] && m[2] && !m[3] && (!m[7] || !m[1]))
		||
		(m[7] && m[2] && !m[1] && (!m[3] || !m[5]))
		||
		(m[7] && m[4] && !m[5] && (!m[1] || !m[3]))
	)
	{
		// return connection value

		return conf.ai_terraforming_networkConnectionValue;

	}

	// needs improvement

	if
	(
		// opposite corners
		(m[2] && m[6])
		||
		(m[4] && m[8])
		||
		// do not connect next corners
//		// next corners
//		(m[2] && m[4] && !m[3])
//		||
//		(m[4] && m[6] && !m[5])
//		||
//		(m[6] && m[8] && !m[7])
//		||
//		(m[8] && m[2] && !m[1])
//		||
		// opposite sides
		(m[1] && m[5] && !m[3] && !m[7])
		||
		(m[3] && m[7] && !m[5] && !m[1])
		||
		// side to corner
		(m[1] && m[4] && !m[3])
		||
		(m[1] && m[6] && !m[7])
		||
		(m[3] && m[6] && !m[5])
		||
		(m[3] && m[8] && !m[1])
		||
		(m[5] && m[8] && !m[7])
		||
		(m[5] && m[2] && !m[3])
		||
		(m[7] && m[2] && !m[1])
		||
		(m[7] && m[4] && !m[5])
	)
	{
		// return connection value

		return conf.ai_terraforming_networkImprovementValue;

	}

	// needs extension

	bool towardBase = false;

	if (m[1] && !m[3] && !m[4] && !m[5] && !m[6] && !m[7])
	{
		towardBase |= isTowardBaseDiagonal(x, y, -1, +1);
	}
	if (m[3] && !m[5] && !m[6] && !m[7] && !m[8] && !m[1])
	{
		towardBase |= isTowardBaseDiagonal(x, y, -1, -1);
	}
	if (m[5] && !m[7] && !m[8] && !m[1] && !m[2] && !m[3])
	{
		towardBase |= isTowardBaseDiagonal(x, y, +1, -1);
	}
	if (m[7] && !m[1] && !m[2] && !m[3] && !m[4] && !m[5])
	{
		towardBase |= isTowardBaseDiagonal(x, y, +1, +1);
	}

	if (m[2] && !m[4] && !m[5] && !m[6] && !m[7] && !m[8])
	{
		towardBase |= isTowardBaseHorizontal(x, y, -1);
	}
	if (m[6] && !m[8] && !m[1] && !m[2] && !m[3] && !m[4])
	{
		towardBase |= isTowardBaseHorizontal(x, y, +1);
	}

	if (m[4] && !m[6] && !m[7] && !m[8] && !m[1] && !m[2])
	{
		towardBase |= isTowardBaseVertical(x, y, -1);
	}
	if (m[8] && !m[2] && !m[3] && !m[4] && !m[5] && !m[6])
	{
		towardBase |= isTowardBaseVertical(x, y, +1);
	}

	if (towardBase)
	{
		// return base extension value

		return conf.ai_terraforming_networkBaseExtensionValue;

	}
	else
	{
		// return wild extension value

		return conf.ai_terraforming_networkWildExtensionValue;

	}

}

bool isTowardBaseDiagonal(int x, int y, int dxSign, int dySign)
{
	for (int dx = 0; dx <= 10; dx++)
	{
		for (int dy = 0; dy <= 10; dy++)
		{
			// cut the triangle

			if (dx + dy > 10)
				continue;

			// get map tile

			MAP *tile = getMapTile(x + dxSign * dx, y + dySign * dy);

			// skip impossible combinations

			if (!tile)
				continue;

			// check base in tile

			if (isFactionBaseAt(tile, aiFactionId))
			{
				return true;
			}

		}

	}

	return false;

}

bool isTowardBaseHorizontal(int x, int y, int dxSign)
{
	for (int dx = 0; dx <= 10; dx++)
	{
		for (int dy = -dx; dy <= dx; dy++)
		{
			// cut the square

			if (dx + dy > 10)
				continue;

			// get map tile

			MAP *tile = getMapTile(x + dxSign * dx, y + dy);

			// skip impossible combinations

			if (!tile)
				continue;

			// check base in tile

			if (isFactionBaseAt(tile, aiFactionId))
			{
				return true;
			}

		}

	}

	return false;

}

bool isTowardBaseVertical(int x, int y, int dySign)
{
	for (int dy = 0; dy <= 10; dy++)
	{
		for (int dx = -dy; dx <= dy; dx++)
		{
			// cut the square

			if (dx + dy > 10)
				continue;

			// get map tile

			MAP *tile = getMapTile(x + dx, y + dySign * dy);

			// skip impossible combinations

			if (!tile)
				continue;

			// check base in tile

			if (isFactionBaseAt(tile, aiFactionId))
			{
				return true;
			}

		}

	}

	return false;

}

/*
Calculates score for sensor actions
*/
double estimateSensorIncome(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	// don't build if already built

	if (map_has_item(tile, BIT_SENSOR))
		return 0.0;

	// find borderRange

	int borderRange = *MapHalfX;

	for (MAP *rangeTile : getRangeTiles(tile, *MapHalfX - 1, false))
	{
		// detect not own tile

		if (rangeTile->owner != aiFactionId)
		{
			borderRange = getRange(tile, rangeTile);
			break;
		}

	}

	// do not compute shoreRange for now - too complicated

	int shoreRange = *MapHalfX;

	// calculate values

	double borderRangeValue = conf.ai_terraforming_sensorBorderRange / std::max(conf.ai_terraforming_sensorBorderRange, (double)borderRange);
	double shoreRangeValue = conf.ai_terraforming_sensorShoreRange / std::max(conf.ai_terraforming_sensorShoreRange, (double)shoreRange);

	double value = conf.ai_terraforming_sensorValue * std::max(borderRangeValue, shoreRangeValue);

	// increase value for coverning not yet covered base

	bool coveringBase = false;

	for (int baseId : aiData.baseIds)
	{
		BASE *base = &(Bases[baseId]);
		MAP *baseTile = getBaseMapTile(baseId);

		// within being constructing sensor range

		if (map_range(x, y, base->x, base->y) > 2)
			continue;

		// not yet covered

		if (isWithinFriendlySensorRange(base->faction_id, baseTile))
			continue;

		coveringBase = true;
		break;

	}

	if (coveringBase)
	{
		value *= 1.5;
	}

	// return value

	return value;

}

/*
Checks whether this tile is worked by base.
*/
bool isBaseWorkedTile(BASE *base, int x, int y)
{
	int baseId = (base - Bases) / sizeof(Bases[0]);

	// check square is within base radius

	if (!isWithinBaseRadius(base->x, base->y, x, y))
		return false;

	// get tile

	MAP *tile = getMapTile(x, y);

	if (tile == NULL)
		return false;

	// iterate worked tiles

	for (MAP *workedTile : getBaseWorkedTiles(baseId))
	{
		if (tile == workedTile)
			return true;

	}

	return false;

}

/**
Evaluates how fit this improvement is at this place based on resources it DOES NOT use.
*/
double getFitnessScore(MAP *tile, int action, bool levelTerrain)
{
	bool ocean = is_ocean(tile);
	
	// tile values
	
	double rainfall = (double)map_rainfall(tile);
	double rockiness = (double)map_rockiness(tile);
	double elevation = (double)map_elevation(tile);
	
	// land value
	
	double landValue = 0.0;
	
	if (!ocean && rockiness != 2)
	{
		// compute regular improvement score (farm + mine/solar)
		
		double regularImprovementScore = 0.0;
		
		// condenser increases rainfall
		
		if (isFactionHasTech(aiFactionId, Terraform[FORMER_CONDENSER].preq_tech) && rainfall < 2)
		{
			rainfall += 0.5;
		}
		
		// raise land increases elevation
		
		if (isFactionHasTech(aiFactionId, Terraform[FORMER_RAISE_LAND].preq_tech))
		{
			if (elevation < 1.0)
			{
				elevation += 1.0;
			}
			else if (elevation < 2.0)
			{
				elevation += 0.5;
			}
			
		}
		
		double landScore = getTerraformingResourceScore(rainfall, rockiness, elevation);
		
		// different option scores
		
		double mineScore = landScore + factionTerraformingInfo.bareMineScore + (isMineBonus(tile) ? 1.0 : 0.0);
		regularImprovementScore = std::max(regularImprovementScore, mineScore);
		
		double solarScore = landScore + factionTerraformingInfo.bareSolarScore;
		regularImprovementScore = std::max(regularImprovementScore, solarScore);
		
		// land value
		
		landValue = std::max(0.0, regularImprovementScore - factionTerraformingInfo.bareLandScore);
		
	}
	
	// summary effects
	
	double penaltyScore = 0.0;
	double nutrientEffect = 0.0;
	double mineralEffect = 0.0;
	double energyEffect = 0.0;
	
	if (ocean)
	{
		switch (action)
		{
		case FORMER_SENSOR:
			{
				// big penalty for existing improvement destruction
				
				if (map_has_item(tile, BIT_MINE | BIT_SOLAR))
				{
					penaltyScore += -2.0;
				}
				
				// big penalty for building on bonus
				
				if (isBonusAt(tile))
				{
					penaltyScore += -2.0;
				}
				
			}
			break;
			
		}
		
	}
	else
	{
		// check action
		
		switch(action)
		{
		case FORMER_MINE:
			
			// rocky tile cannot be farmed
			
			if (rockiness == 2)
			{
				nutrientEffect += -rainfall;
			}
			
			// not using altitude
			
			energyEffect += -elevation;
			
			break;
			
		case FORMER_CONDENSER:
			
			// not using altitude
			
			energyEffect += -elevation;
			
			break;
			
		case FORMER_THERMAL_BORE:
		case FORMER_FOREST:
		case FORMER_PLANT_FUNGUS:
			
			// not using anything
			// penalty is the land value
			
			penaltyScore += -landValue;
			
			break;
			
		case FORMER_SENSOR:
			
			if (map_has_item(tile, BIT_FOREST | BIT_MONOLITH))
			{
				// compatible with these items
			}
			else
			{
				// big penalty for destroying existing improvement
				
				if (map_has_item(tile, BIT_MINE | BIT_SOLAR | BIT_CONDENSER | BIT_ECH_MIRROR | BIT_THERMAL_BORE))
				{
					penaltyScore += -2.0;
				}
				
				// big penalty for building on bonus
				
				if (isBonusAt(tile))
				{
					penaltyScore += -2.0;
				}
				
				// not using anything (it is compatible with farm but single farm usually won't be worked anyway)
				// penalty is the land value
				
				penaltyScore += -landValue;
				
				// penalty for river
				
				if (map_has_item(tile, BIT_RIVER))
				{
					energyEffect += -1.0;
				}
				
			}
			
			break;
			
		}
		
		// not having mine in mineral bonus tile
		
		if (isMineBonus(tile) && action != FORMER_MINE)
		{
			mineralEffect += -1.0;
		}
		
		// penalty for destroying ideal rocky mine spot
		
		if (landValue <= 0.0 && rockiness == 2 && levelTerrain)
		{
			mineralEffect += -1.0;
		}
		
		// penalty for borehole on river
		
		if (action == FORMER_THERMAL_BORE && map_has_item(tile, BIT_RIVER))
		{
			energyEffect += -2.0;
		}
		
	}
	
	// destroying existing improvement
	
	if (map_has_item(tile, Terraform[action].bit_incompatible & (BIT_MINE | BIT_SOLAR | BIT_CONDENSER | BIT_ECH_MIRROR | BIT_THERMAL_BORE)))
	{
		penaltyScore += -2.0;
	}
	
	// combine direct penalty score with resource waste
	
	penaltyScore += getTerraformingResourceScore(nutrientEffect, mineralEffect, energyEffect);
	
	double fitnessScore = conf.ai_terraforming_fitnessMultiplier * penaltyScore;
	
//	debug
//	(
//		"\t\t%-20s=%6.3f:"
//		" ocean=%d"
//		" rainfall=%d"
//		" rockiness=%d"
//		" elevation=%d"
//		" nutrientEffect=%f"
//		" mineralEffect=%f"
//		" energyEffect=%f"
//		" multiplier=%f"
//		"\n",
//		"fitnessScore",
//		fitnessScore,
//		ocean,
//		rainfall,
//		rockiness,
//		elevation,
//		nutrientEffect,
//		mineralEffect,
//		energyEffect,
//		conf.ai_terraforming_fitnessMultiplier
//	);
	
	return fitnessScore;
	
}

bool hasNearbyTerraformingRequestAction(std::vector<TERRAFORMING_REQUEST>::iterator begin, std::vector<TERRAFORMING_REQUEST>::iterator end, int action, int x, int y, int range)
{
	for
	(
		std::vector<TERRAFORMING_REQUEST>::iterator terraformingRequestsIterator = begin;
		terraformingRequestsIterator != end;
		terraformingRequestsIterator++
	)
	{
		TERRAFORMING_REQUEST *terraformingRequest = &(*terraformingRequestsIterator);
		
		assert(isOnMap(terraformingRequest->tile));
		
		if (terraformingRequest->firstAction == action)
		{
			if (map_range(x, y, getX(terraformingRequest->tile), getY(terraformingRequest->tile)) <= range)
			{
				return true;
			}
			
		}
		
	}
	
	return false;
	
}

/*
Estimates condenser yield score in adjacent tiles.
*/
double estimateCondenserExtraYieldScore(MAP *tile, const std::vector<int> *actions)
{
	// check whether condenser was built or destroyed
	
	bool condenserAction = false;
	
	for (int action : *actions)
	{
		if (action == FORMER_CONDENSER)
		{
			condenserAction = true;
			break;
		}
	}
	
	int sign;
	
	// condenser is built
	if (!map_has_item(tile, BIT_CONDENSER) && condenserAction)
	{
		sign = +1;
	}
	// condenser is destroyed
	else if (map_has_item(tile, BIT_CONDENSER) && !condenserAction)
	{
		sign = -1;
	}
	// neither built nor destroyed
	else
	{
		return 0.0;
	}
	
	// estimate rainfall improvement
	// [0.0, 9.0]
	
	double totalRainfallImprovement = 0.0;
	
	for (MAP *boxTile : getRangeTiles(tile, 1, true))
	{
		TileTerraformingInfo &boxTileTerraformingInfo = getTileTerraformingInfo(boxTile);
		
		// exclude non terraformable site
		
		if (!boxTileTerraformingInfo.availableTerraformingSite)
			continue;
		
		// exclude non conventional terraformable site
		
		if (!boxTileTerraformingInfo.availableBaseTerraformingSite)
			continue;
		
		// exclude ocean - not affected by condenser
		
		if (is_ocean(boxTile))
			continue;
		
		// exclude great dunes - not affected by condenser
		
		if (map_has_landmark(boxTile, LM_DUNES))
			continue;
		
		// exclude borehole
		
		if (map_has_item(boxTile, BIT_THERMAL_BORE))
			continue;
		
		// exclude rocky areas
		
		if (map_rockiness(boxTile) == 2)
			continue;
		
		// condenser was there but removed
		if (sign == -1)
		{
			switch(map_rainfall(boxTile))
			{
			case 1:
				totalRainfallImprovement += 1.0;
				break;
			case 2:
				totalRainfallImprovement += 0.5;
				break;
			}
			
		}
		// condenser wasn't there and built
		else if (sign == +1)
		{
			switch(map_rainfall(boxTile))
			{
			case 0:
			case 1:
				totalRainfallImprovement += 1.0;
				break;
			}
			
		}
		// safety check
		else
		{
			return 0.0;
		}
		
	}
	
	// calculate bonus
	// rainfall bonus should be at least 2 for condenser to make sense
	
	double extraScore = sign * getTerraformingResourceScore(totalRainfallImprovement - 2.0, 0.0, 0.0);
	
//	debug("\t\tsign=%+1d, totalRainfallImprovement=%6.3f, extraScore=%+6.3f\n", sign, totalRainfallImprovement, extraScore);
	
	// return score
	
	return extraScore;
	
}

/*
Estimates aquifer additional yield score.
This is very approximate.
*/
double estimateAquiferIncome(MAP *tile)
{
	assert(isOnMap(tile));
	
	int x = getX(tile);
	int y = getY(tile);

	// evaluate elevation

	// [2, 8]
    double elevationFactor = 2.0 * (double)(map_elevation(tile) + 1);

    // evaluate nearby bases worked tiles coverage

    int totalTileCount = 0;
    int workableTileCount = 0;

    for (int dx = -4; dx <= +4; dx++)
	{
		for (int dy = -(4 - abs(dx)); dy <= +(4 - abs(dx)); dy += 2)
		{
			MAP *closeTile = getMapTile(x + dx, y + dy);
			if (!closeTile)
				continue;

			TileTerraformingInfo &closeTileTerraformingInfo = getTileTerraformingInfo(closeTile);

			totalTileCount++;

			// workable tile

			if (closeTileTerraformingInfo.workable)
				workableTileCount++;

		}

	}

	// safety exit

	if (totalTileCount == 0)
		return 0.0;

	// [0.0, 1.0]
	double workedTileCoverage = (double)workableTileCount / (double)totalTileCount;

	// evaluate placement

	// [0, 12]
	int borderRiverCount = 0;

	for (int dx = -4; dx <= +4; dx++)
	{
		for (int dy = -(4 - abs(dx)); dy <= +(4 - abs(dx)); dy += 2)
		{
			// iterate outer rim only

			if (abs(dx) + abs(dy) != 4)
				continue;

			// build tile

			MAP *closeTile = getMapTile(x + dx, y + dy);

			if (!closeTile)
				continue;

			// exclude base

			if (map_has_item(closeTile, BIT_BASE_IN_TILE))
				continue;

			// count rivers

			if (map_has_item(closeTile, BIT_RIVER))
			{
				borderRiverCount++;
			}

			// discount oceans

			if (is_ocean(closeTile))
			{
				borderRiverCount--;
			}

		}

    }

    // return weighted totalEnergyImprovement

    return conf.ai_terraforming_energyWeight * (workedTileCoverage * elevationFactor) + (double)borderRiverCount / 2.0;

}

/*
Estimates raise land additional yield score.
*/
double estimateRaiseLandIncome(MAP *terrafromingTile, int cost)
{
	assert(isOnMap(terrafromingTile));
	
	int x = getX(terrafromingTile);
	int y = getY(terrafromingTile);

	// land only

	if (is_ocean(terrafromingTile))
		return 0.0;

	// calculate final raised elevation and affected range

	int raisedElevation = map_elevation(terrafromingTile) + 1;
	int affectedRange = raisedElevation;

	// evaluate energy benefit

	// [0.0, 45.0]
	double totalEnergyImprovement = 0.0;

	for (int dx = -(2 * affectedRange); dx <= +(2 * affectedRange); dx++)
	{
		for (int dy = -(2 * affectedRange - abs(dx)); dy <= +(2 * affectedRange - abs(dx)); dy += 2)
		{
			int nearbyTileX = wrap(x + dx);
			int nearbyTileY = y + dy;

			if (!isOnMap(nearbyTileX, nearbyTileY))
				continue;

			MAP *tile = getMapTile(nearbyTileX, nearbyTileY);

			if (!tile)
				continue;

			TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);

			// exclude not workable tiles

			if (!tileTerraformingInfo.workable)
				continue;

			// exclude ocean

			if (is_ocean(tile))
				continue;

			// calculate range

			int range = map_range(x, y, wrap(x + dx), y + dy);

			// calculate elevation and expected elevation

			int elevation = map_elevation(tile);
			int expectedElevation = raisedElevation - range;

			// calculate energy collection improvement

			if (elevation < expectedElevation && map_has_item(tile, BIT_SOLAR | BIT_ECH_MIRROR))
			{
				double energyImprovement = (double)(expectedElevation - elevation);

				// halve if not worked

				if (!tileTerraformingInfo.worked)
				{
					energyImprovement /= 2.0;
				}

				// update total

				totalEnergyImprovement += energyImprovement;

			}

		}

    }

    // return weighted totalEnergyImprovement

    return 1.0 * conf.ai_terraforming_energyWeight * (totalEnergyImprovement - (double)cost / conf.ai_terraforming_raiseLandPayoffTime);

}

/*
Determines whether raising land does not disturb others.
*/
bool isRaiseLandSafe(MAP *raisedTile)
{
	Faction *faction = &(Factions[aiFactionId]);

	// determine affected range

	int maxRange = is_ocean(raisedTile) ? 0 : map_elevation(raisedTile) + 1;

	// check tiles in range

	for (MAP *tile : getRangeTiles(raisedTile, maxRange, true))
	{
		// check if we have treaty/pact with owner

		if
		(
			(tile->owner != aiFactionId)
			&&
			(faction->diplo_status[tile->owner] & (DIPLO_TRUCE | DIPLO_TREATY | DIPLO_PACT))
		)
		{
			return false;
		}

	}

	return true;

}

/**
Calculates combined weighted resource score taking base additional demand into account.
*/
double calculateBaseResourceScore(int baseId, int currentMineralIntake2, int currentNutrientSurplus, int currentMineralSurplus, int currentEnergySurplus, int improvedNutrientSurplus, int improvedMineralSurplus, int improvedEnergySurplus)
{
	BASE *base = &(Bases[baseId]);
	
	// improvedNutrientSurplus <= 0 is unacceptable
	
	if (improvedNutrientSurplus <= 0)
		return 0.0;
	
	// calculate nutrient and mineral extra score
	
	double nutrientCostMultiplier = 1.0;
	double mineralCostMultiplier = 1.0;
	double energyCostMultiplier = 1.0;
	
	// multipliers are for bases size 2 and above
	
	if (base->pop_size >= 2)
	{
		// calculate nutrient cost multiplier
		
		double nutrientThreshold = conf.ai_terraforming_baseNutrientThresholdRatio * (double)currentNutrientSurplus;
		
		if (currentNutrientSurplus < nutrientThreshold)
		{
			double proportion = 1.0 - (double)currentNutrientSurplus / nutrientThreshold;
			nutrientCostMultiplier += (conf.ai_terraforming_baseNutrientCostMultiplier - 1.0) * proportion;
		}
		
		// calculate mineral cost multiplier
		
		double mineralThreshold = conf.ai_terraforming_baseMineralThresholdRatio * (double)currentNutrientSurplus;
		
		if (currentMineralIntake2 < mineralThreshold)
		{
			double proportion = 1.0 - (double)currentMineralIntake2 / mineralThreshold;
			mineralCostMultiplier += (conf.ai_terraforming_baseMineralCostMultiplier - 1.0) * proportion;
		}
		
		// calculate energy cost multiplier
		
		if (aiData.grossIncome > 0 && aiData.netIncome > 0)
		{
			double maxNetIncome = 0.5 * (double)aiData.grossIncome;
			double minNetIncome = 0.1 * maxNetIncome;
			energyCostMultiplier = maxNetIncome / std::min(maxNetIncome, std::max(minNetIncome, (double)aiData.netIncome));
		}
		
	}
	
	// compute final score
	
	return
		getTerraformingResourceScore
		(
			nutrientCostMultiplier * (improvedNutrientSurplus - currentNutrientSurplus),
			mineralCostMultiplier * (improvedMineralSurplus - currentMineralSurplus),
			energyCostMultiplier * (improvedEnergySurplus - currentEnergySurplus)
		)
	;
	
}

/*
Calculates yield improvement score for given base and improved tile.
*/
double computeImprovementBaseSurplusEffectScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState, bool areaEffect)
{
	assert(isOnMap(tile));
	
	BASE *base = &(Bases[baseId]);
	BaseTerraformingInfo &baseTerraformingInfo = baseTerraformingInfos[baseId];
	int x = getX(tile);
	int y = getY(tile);
	bool worked = false;
	
	// current surplus
	
	int currentMineralIntake2 = base->mineral_intake_2;
	int currentNutrientSurplus = base->nutrient_surplus;
	int currentMineralSurplus = base->mineral_surplus;
	int currentEnergySurplus = base->economy_total + base->psych_total + base->labs_total;
	
	// apply changes
	
	*tile = *improvedMapState;
	computeBase(baseId, true);
	
	// improved yield
	
	int nutrientYield = mod_crop_yield(aiFactionId, baseId, x, y, 0);
	int mineralYield = mod_mine_yield(aiFactionId, baseId, x, y, 0);
	int energyYield = mod_energy_yield(aiFactionId, baseId, x, y, 0);
	
	// improved surplus
	
	int improvedNutrientSurplus = base->nutrient_surplus;
	int improvedMineralSurplus = base->mineral_surplus;
	int improvedEnergySurplus = base->economy_total + base->psych_total + base->labs_total;
	
	// verify tile is worked after improvement
	
	worked = isBaseWorkedTile(baseId, tile);
	
	// restore original state
	
	*tile = *currentMapState;
	computeBase(baseId, true);
	
	// ignore improvement if not areaEffect and is inferior
	
	if (!areaEffect && nutrientYield <= baseTerraformingInfo.minimalNutrientYield && mineralYield <= baseTerraformingInfo.minimalMineralYield && energyYield <= baseTerraformingInfo.minimalEnergyYield)
		return 0.0;
	
	// ignore improvement is not areaEffect and is not worked
	
	if (!areaEffect && !worked)
		return 0.0;
	
	// calculate improvement score
	
	return
		calculateBaseResourceScore
		(
			baseId,
			currentMineralIntake2,
			currentNutrientSurplus,
			currentMineralSurplus,
			currentEnergySurplus,
			improvedNutrientSurplus,
			improvedMineralSurplus,
			improvedEnergySurplus
		)
	;
	
}

/*
Applies improvement and computes its maximal yield score for this base.
*/
double computeBaseImprovementYieldScore(int baseId, MAP *tile, MAP *currentMapState, MAP *improvedMapState)
{
	assert(isOnMap(tile));
	
	BASE *base = &(Bases[baseId]);
	int x = getX(tile);
	int y = getY(tile);

	// apply improved state

	*tile = *improvedMapState;

	// record yield

	int nutrient = mod_crop_yield(base->faction_id, baseId, x, y, 0);
	int mineral = mod_mine_yield(base->faction_id, baseId, x, y, 0);
	int energy = mod_energy_yield(base->faction_id, baseId, x, y, 0);

	// restore original state

	*tile = *currentMapState;

	// compute yield score

	return getTerraformingResourceScore(nutrient, mineral, energy);

}

/*
Checks that base can yield from this site.
*/
bool isWorkableTile(MAP *tile)
{
	if (tile == nullptr)
		return false;

	// exclude volcano mouth

	if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
		return false;

	// exclude claimed territory

	if (!(tile->owner == -1 || tile->owner == aiFactionId))
		return false;

	// exclude base

	if (map_has_item(tile, BIT_BASE_IN_TILE))
		return false;

	// exclude not base radius

	if (!map_has_item(tile, BIT_BASE_RADIUS))
		return false;

	// all conditions met

	return true;

}

/*
Checks that conventional improvement can be done at this site and base can work it.
conventional improvement = any yield improvement except river
*/
bool isValidConventionalTerraformingSite(MAP *tile)
{
	if (tile == nullptr)
		return false;

	// exclude not workable sites

	if (!isWorkableTile(tile))
		return false;

	// exclude monolith
	// it cannot be improved for yield

	if (map_has_item(tile, BIT_MONOLITH))
		return false;

	// all conditions met

	return true;

}

bool isValidTerraformingSite(MAP *tile)
{
	if (tile == nullptr)
		return false;
	
	// exclude volcano mouth
	
	if ((tile->landmarks & LM_VOLCANO) && (tile->art_ref_id == 0))
		return false;
	
	// exclude claimed territory
	
	if (!(tile->owner == -1 || tile->owner == aiFactionId))
		return false;
	
	// exclude base
	
	if (map_has_item(tile, BIT_BASE_IN_TILE))
		return false;
	
	// exclude blocked locations
	
	if (isBlocked(tile))
		return false;
	
	// all conditions met
	
	return true;
	
}

double getTerraformingResourceScore(double nutrient, double mineral, double energy)
{
	return
		+ conf.ai_terraforming_nutrientWeight * nutrient
		+ conf.ai_terraforming_mineralWeight * mineral
		+ conf.ai_terraforming_energyWeight * energy
	;
}

double getTerraformingResourceScore(TileYield yield)
{
	return getTerraformingResourceScore((double)yield.nutrient, (double)yield.mineral, (double)yield.energy);
}

void generateTerraformingChange(MapState &mapState, int action)
{
	switch (action)
	{
	case FORMER_LEVEL_TERRAIN:
		
		// level terrain changes rockiness
		
		mapState.rockiness = std::max(0, mapState.rockiness - 1);
		
		break;
		
	case FORMER_AQUIFER:
		
		mapState.items |= (BIT_RIVER_SRC | BIT_RIVER);
		
		break;
		
	case FORMER_REMOVE_FUNGUS:
		
		// drop fungus bit
		
		mapState.items &= (~BIT_FUNGUS);
		
		break;
		
	case FORMER_PLANT_FUNGUS:
		
		// remove items
		
		mapState.items &= (~Terraform[action].bit_incompatible);
		
		// add items
		
		mapState.items |= (Terraform[action].bit);
		
		break;
		
	default:
		
		// remove fungus
		
		mapState.items &= (~BIT_FUNGUS);
		
		// remove items
		
		mapState.items &= (~Terraform[action].bit_incompatible);
		
		// add items
		
		mapState.items |= (Terraform[action].bit);
		
	}
	
}

double getTerraformingGain(double income, double terraformingTime)
{
	return getGainDelay(getGainIncome(income), terraformingTime);
}

/**
Compares terraforming requests by gain.
*/
bool compareTerraformingRequests(TERRAFORMING_REQUEST const &terraformingRequest1, TERRAFORMING_REQUEST const &terraformingRequest2)
{
	return (terraformingRequest1.gain > terraformingRequest2.gain);
}

/**
Compares terraforming requests by improvementIncome then by fitnessScore.
1. compare by yield: superior yield goes first.
2. compare by gain.
*/
bool compareConventionalTerraformingRequests(TERRAFORMING_REQUEST const &terraformingRequest1, TERRAFORMING_REQUEST const &terraformingRequest2)
{
	return terraformingRequest1.gain > terraformingRequest2.gain;
}

void setMapState(MapState &mapState, MAP *tile)
{
	mapState.rockiness = map_rockiness(tile);
	mapState.items = tile->items;
}

void applyMapState(MapState &mapState, MAP *tile)
{
	// clear rockiness flags
	
	tile->val3 &= (~TILE_ROCKY);
	tile->val3 &= (~TILE_ROLLING);
	
	// set rockiness flag
	
	switch (mapState.rockiness)
	{
	case 2:
		tile->val3 |= TILE_ROCKY;
		break;
	case 1:
		tile->val3 |= TILE_ROLLING;
		break;
	}
	
	// set items
	
	tile->items = mapState.items;
	
}

void restoreTileMapState(MAP *tile)
{
	TileTerraformingInfo &tileTerraformingInfo = getTileTerraformingInfo(tile);
	
	applyMapState(tileTerraformingInfo.mapState, tile);
	
}

/**
Computes improved tile yield.
Ignores actions not available for player faction.
*/
TileYield getTerraformingYield(int baseId, MAP *tile, std::vector<int> actions)
{
	assert(isOnMap(tile));
	assert(baseId == -1 || (baseId >= 0 && baseId < *BaseCount));
	
	int x = getX(tile);
	int y = getY(tile);
	TileInfo &tileInfo = aiData.getTileInfo(tile);
	
	// store currentMapState
	
	MapState currentMapState;
	setMapState(currentMapState, tile);
	
	// apply terraforming
	
	MapState improvedMapState;
	setMapState(improvedMapState, tile);
	
	for (int action : actions)
	{
		CTerraform *terraform = getTerraform(action);
		int preqTech = tileInfo.ocean ? terraform->preq_tech_sea : terraform->preq_tech;
		
		if (!isFactionHasTech(aiFactionId, preqTech))
			continue;
		
		generateTerraformingChange(improvedMapState, action);
		
	}
	
	applyMapState(improvedMapState, tile);
	
	int nutrient = mod_crop_yield(aiFactionId, baseId, x, y, 0);
	int mineral = mod_mine_yield(aiFactionId, baseId, x, y, 0);
	int energy = mod_energy_yield(aiFactionId, baseId, x, y, 0);
	
	// estimate base effect in case base is not given
	
	if (baseId == -1)
	{
		// forest
		
		if (map_has_item(tile, BIT_FOREST))
		{
			if (isFactionHasTech(aiFactionId, getFacility(FAC_TREE_FARM)->preq_tech))
			{
				nutrient += 1;
			}
			if (isFactionHasTech(aiFactionId, getFacility(FAC_HYBRID_FOREST)->preq_tech))
			{
				nutrient += 1;
				energy += 1;
			}
		}
		
		// ocean
		
		if (tileInfo.ocean)
		{
			// farm
			
			if (map_has_item(tile, BIT_FARM))
			{
				if (isFactionHasTech(aiFactionId, getFacility(FAC_AQUAFARM)->preq_tech))
				{
					nutrient += 1;
				}
			}
			
			// mining platform
			
			if (map_has_item(tile, BIT_MINE))
			{
				if (isFactionHasTech(aiFactionId, getFacility(FAC_SUBSEA_TRUNKLINE)->preq_tech))
				{
					mineral += 1;
				}
				if (isFactionHasTech(aiFactionId, Rules->tech_preq_mining_platform_bonus))
				{
					mineral += 1;
				}
			}
			
			// tidal harness
			
			if (map_has_item(tile, BIT_SOLAR))
			{
				if (isFactionHasTech(aiFactionId, getFacility(FAC_THERMOCLINE_TRANSDUCER)->preq_tech))
				{
					energy += 1;
				}
			}
			
		}
		
	}
	
	// restore map state
	
	applyMapState(currentMapState, tile);
	
	// return yield
	
	return {nutrient, mineral, energy};
	
}

double getImprovementIncome(int popSize, Resource oldIntake, Resource newIntake)
{
	// resource values
	
	double nutrientWeight = conf.ai_terraforming_nutrientWeight;
	double mineralWeight = conf.ai_terraforming_mineralWeight;
	double energyWeight = conf.ai_terraforming_energyWeight;
	
	double oldNutrientSurplus = oldIntake.nutrient;
	double oldMineralIntake2 = oldIntake.mineral;
	double oldEnergyIntake2 = oldIntake.energy;
	
	double newNutrientSurplus = newIntake.nutrient;
	double newMineralIntake2 = newIntake.mineral;
	double newEnergyIntake2 = newIntake.energy;
	
	// nutrient threshold
	
	double nutrientThreshold = conf.ai_terraforming_baseNutrientThresholdRatio * (double)(1 + popSize);
	
	// mineral threshold
	
	double mineralThreshold = conf.ai_terraforming_baseMineralThresholdRatio * oldNutrientSurplus;
	
	// old score
	
	double oldScore =
		+ nutrientWeight * oldNutrientSurplus
		+ mineralWeight * oldMineralIntake2
		+ energyWeight * oldEnergyIntake2
	;
	
	if (oldNutrientSurplus < nutrientThreshold)
	{
		double oldNutrientExtraScore = 0.5 * (conf.ai_terraforming_baseNutrientCostMultiplier - 1.0) * (2.0 - oldNutrientSurplus / nutrientThreshold) * oldNutrientSurplus;
		oldScore += oldNutrientExtraScore;
	}
	
	if (oldMineralIntake2 < mineralThreshold)
	{
		double oldMineralExtraScore = 0.5 * (conf.ai_terraforming_baseMineralCostMultiplier - 1.0) * (2.0 - oldMineralIntake2 / mineralThreshold) * oldMineralIntake2;
		oldScore += oldMineralExtraScore;
	}
	
	// new score
	
	double newScore =
		+ nutrientWeight * newNutrientSurplus
		+ mineralWeight * newMineralIntake2
		+ energyWeight * newEnergyIntake2
	;
	
	if (newNutrientSurplus < nutrientThreshold)
	{
		double newNutrientExtraScore = 0.5 * (conf.ai_terraforming_baseNutrientCostMultiplier - 1.0) * (2.0 - newNutrientSurplus / nutrientThreshold) * newNutrientSurplus;
		newScore += newNutrientExtraScore;
	}
	
	if (newMineralIntake2 < mineralThreshold)
	{
		double newMineralExtraScore = 0.5 * (conf.ai_terraforming_baseMineralCostMultiplier - 1.0) * (2.0 - newMineralIntake2 / mineralThreshold) * newMineralIntake2;
		newScore += newMineralExtraScore;
	}
	
	// return difference
	
	return newScore - oldScore;
	
}

void addConventionalTerraformingRequest(std::vector<TERRAFORMING_REQUEST> &availableTerraformingRequests, TERRAFORMING_REQUEST &terraformingRequest)
{
	TERRAFORMING_REQUEST *equalTerraformingRequest = nullptr;
	size_t equalTerraformingRequestIndex;
	
	for (size_t i = 0; i < availableTerraformingRequests.size(); i++)
	{
		TERRAFORMING_REQUEST &availableTerraformingRequest = availableTerraformingRequests.at(i);
		
		// compare same realm
		
		if (is_ocean(availableTerraformingRequest.tile) != is_ocean(terraformingRequest.tile))
			continue;
		
		// superior terraforming exists - ignore this one
		
		if (TileYield::isSuperior(availableTerraformingRequest.yield, terraformingRequest.yield))
			return;
		
		// equal terraforming exists - save it for later analysis
		
		if (TileYield::isEqual(availableTerraformingRequest.yield, terraformingRequest.yield) && availableTerraformingRequest.option == terraformingRequest.option)
		{
			equalTerraformingRequest = &availableTerraformingRequest;
			equalTerraformingRequestIndex = i;
		}
		
	}
	
	if (equalTerraformingRequest == nullptr)
	{
		availableTerraformingRequests.push_back(terraformingRequest);
	}
	else
	{
		if (terraformingRequest.option == &TO_ROCKY_MINE || terraformingRequest.option == &TO_THERMAL_BOREHOLE || terraformingRequest.option == &TO_FOREST || terraformingRequest.option == &TO_LAND_FUNGUS)
		{
			// replace old request if new one has better fitness score
			
			if (terraformingRequest.fitnessScore > equalTerraformingRequest->fitnessScore)
			{
				availableTerraformingRequests[equalTerraformingRequestIndex] = terraformingRequest;
			}
			
		}
		else
		{
			// ignore duplicate standard option
		}
		
	}
	
}

