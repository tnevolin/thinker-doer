#pragma once

#include "main.h"

void exit_fail_over();
void write_call_over(int addr, int func);
void patch_setup_wtp(Config* cf);
void shiftVehicleAddress(byte *bytes, int position);

