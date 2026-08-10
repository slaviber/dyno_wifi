/*
*    dyno_wifi - The Lozdar Process Controller v1.0 software
*    Copyright (C) 2026 Lozdar <support@lozdar.com> https://lozdar.com
*
*    This program is free software; you can redistribute it and/or
*    modify it under the terms of the GNU General Public License
*    version 2 as published by the Free Software Foundation
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program; if not, see
*    <https://www.gnu.org/licenses/>.
*/

/*
*
*   pid.h contains the PID control function declarations.
*
*/

#include "data.h"
#pragma once

void IRAM_ATTR n10_pid(float kmh, float n10_pct, float slip, float target_pct) ;
float IRAM_ATTR kmh_pid(float kmh, float kmh_target);
void IRAM_ATTR acc_pid(float kmh, float acc, float slip, float acc_target);
