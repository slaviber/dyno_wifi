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
*   data.c contains the the zero initialisation code for the global
*   variables, and the global function definitions.
*
*/

#include "data_def.h"
#include <xtensa/core-macros.h>

const char *TAG = "DYNO9126v2";

atomic_int captured[N_INPUTS] = {0};  // tenso input
atomic_int rpm[N_INPUTS] = {RPM_MAX}; // rpm input
// filtered tenso/speed input w/ 27 Hz bw, 50 ms delay
atomic_int filt_in[N_INPUTS] = {0};
// even more filtered tenso/speed input w/ 9 Hz bw, 150 ms delay
atomic_int graph_val[N_INPUTS] = {0};
atomic_uint dither_counter = 0;      // 1 kHz counter
atomic_int current_frq = FRQ_UNLOAD; // analog out current val
// filtered analog out, 9 Hz bw, 150 ms delay
atomic_int disp_frq_prg[2] = {FRQ_UNLOAD, 0};
atomic_int no_rpm[N_INPUTS] = {0};             // rpm overflow of rpm[]
atomic_int input_arr[N_INPUTS] = {INPUT_NONE}; // mapped input types
atomic_int input_tag[256] = {-1};              // tagged inputs
atomic_char tag_arr[N_INPUTS] = {' '};         // tagged inputs rmap
atomic_int adc_idx = {0};                      // ADC input index, increasing
atomic_int puls_idx = {0};                     // Pulse input index, increasing
atomic_int adc_arr[N_INPUTS] = {-1};           // indexed ADC inputs
atomic_int puls_arr[N_INPUTS] = {-1};          // indexed Pulse inputs

atomic_int current_prg = 0;
_Atomic float drag_coef = 0.f;
_Atomic float whr_inv = 0.f;

atomic_bool mop_change = true; // mop update
atomic_bool n10_change = true; // target_n10 update
atomic_bool kmh_change = true; // target_kmh update

_Atomic float dyn_state[5] = {0}; // For the display of the setpoints

atomic_bool pc_state[STATE_END] = {false};

sweep_state sws = {
    .buffer_side = 0,
    .remaining_size = {0, 0},
    .sweep_buf = {},
    .sweep_run = false,
    .sweep_mode = MOP_END,
};

const DEF_STRUCT(HWINFO, hwinfo, hwdesc);
DEF_STRUCT(SETTINGS_T, settings_t, cnt_seti);

//Call this at least once per ~8 seconds
uint64_t IRAM_ATTR pc_clock() {
  static unsigned lastcc = 0;
  static uint64_t ticks = 0; // TODO: FIXME: Overflow in 2314 years!

  unsigned cc = (unsigned)XTHAL_GET_CCOUNT();
  ticks += (unsigned)(cc - lastcc);
  lastcc = cc;

  return ticks;
}

mcpwm_timer_handle_t adc_timer = NULL;
pc_consts consts = {}; // I hate C :(