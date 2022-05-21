#pragma once

#include <memory>
#include "game_wtp.h"
#include "aiTransport.h"
#include "aiTask.h"

void moveStrategy();
void populateVehicles();
void fixUndesiredTransportDropoff();
void fixUndesiredTransportPickup();
void moveAllStrategy();
void healStrategy();
int moveVehicle(const int vehicleId);
void transitVehicle(int vehicleId, Task task);
void deleteVehicles();
void balanceVehicleSupport();

// ==================================================
// enemy_move entry point
// ==================================================

int __cdecl modified_enemy_move(const int vehicleId);

