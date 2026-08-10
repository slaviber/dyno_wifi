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
*   init.h contains the public initialization function declarations.
*
*/

#include "data.h"
#include <driver/ledc.h>
#include <soc/pcnt_struct.h>
#include <driver/uart.h>
#include <esp_log.h>
#pragma once

void inputs_init();
void tg0_timer0_init();
void main_init(void);
void nvs_preconfig();
void init_pc_consts(pc_consts *c);