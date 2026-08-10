
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
*   logic.h contains the public logic function declarations.
*
*/

#include "data.h"
#include "driver/pulse_cnt.h"
#include <driver/uart.h>
#include <driver/gptimer.h>
#pragma once

bool IRAM_ATTR pulse_ouflow_handler(pcnt_unit_handle_t unit,
                                    const pcnt_watch_event_data_t *edata,
                                    void *user_ctx);
void IRAM_ATTR Pulse_isr_handler(pcnt_isr_ctx *ctx);
void IRAM_ATTR ADC_isr_handler(pcnt_isr_ctx *ctx);
bool IRAM_ATTR timer_alarm_cb(gptimer_handle_t timer,
                              const gptimer_alarm_event_data_t *edata,
                              void *user_data);
void blockingStatusChaining();
void go_to_failsafe();
void update_bridge_state();