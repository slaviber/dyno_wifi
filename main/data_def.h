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
*   data_def.h contains the global constants, functions, and 
*   variables, and stands the lowest in the file hierarchy.
*
*/

//TIP: All atomic esp32 types of and below 4 bytes are lock-free !!!

#include <esp_http_server.h>
#include <esp_types.h>

#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <driver/spi_master.h>
#include <driver/mcpwm_types.h>

#pragma once

extern const char DRAM_ATTR *TAG;
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

enum RESPONSE { REST_OK = 0, REST_BAD, REST_AGAIN };

#define FRQ_MIN 500
#define FRQ_MAX 9000
#define FRQ_UNLOAD FRQ_MIN
#define PC_CLOCK 10e3f
#define MIN_I 1.f        // min I
#define MAX_I 500.f      // max I
#define SWEEP_BUFLEN 200 // 10 secs of points
#define CPU_FREQ 240000000UL
#define ADC_MIN -16
#define ADC_MAX 815
#define PUL_MIN 0
#define PUL_MAX 200
#define PID1_MIN 0
#define PID1_MAX 600
#define PID2_MIN 30
#define PID2_MAX 180

#define MAX_STATUS_LINE_LENGTH 128 // max status line length! FIXME

// The number of data packets to accumulate before sending through the websocket
#define ACCUM_WS 10

#define RPM_MIN 1000  // 434 km/h
#define RPM_MAX 65535 // 6 km/h

#define CLK_IO 18
#define MOSI_IO 23
#define CS_IO 5
extern spi_device_handle_t spi3;

#include "filter_tables.h"

enum INPUT_SIGNAL { INPUT_ADC = 0, INPUT_PULS, INPUT_NONE };

extern DRAM_ATTR atomic_int captured[N_INPUTS]; // tenso input
extern DRAM_ATTR atomic_int rpm[N_INPUTS];      // rpm input
// filtered tenso/speed input w/ 27 Hz bw, 50 ms delay
extern DRAM_ATTR atomic_int filt_in[N_INPUTS];
// even more filtered tenso/speed input w/ 9 Hz bw, 150 ms delay
extern DRAM_ATTR atomic_int graph_val[N_INPUTS];
extern DRAM_ATTR atomic_uint dither_counter; // 1 kHz counter
extern DRAM_ATTR atomic_int current_frq;     // analog out current val
// filtered analog out, 9 Hz bw, 150 ms delay
extern DRAM_ATTR atomic_int disp_frq_prg[2];
extern DRAM_ATTR atomic_int no_rpm[N_INPUTS];    // rpm overflow of rpm[]
extern DRAM_ATTR atomic_int input_arr[N_INPUTS]; // mapped input types
extern DRAM_ATTR atomic_int input_tag[256];      // tagged inputs
extern DRAM_ATTR atomic_char tag_arr[N_INPUTS];  // tagged inputs rmap
extern DRAM_ATTR atomic_int adc_idx;             // ADC input index, increasing
extern DRAM_ATTR atomic_int puls_idx;           // Pulse input index, increasing
extern DRAM_ATTR atomic_int adc_arr[N_INPUTS];  // indexed ADC inputs
extern DRAM_ATTR atomic_int puls_arr[N_INPUTS]; // indexed Pulse inputs

extern DRAM_ATTR atomic_int current_prg;
extern DRAM_ATTR _Atomic float drag_coef;
extern DRAM_ATTR _Atomic float whr_inv;

extern DRAM_ATTR atomic_bool mop_change; // mop update
extern DRAM_ATTR atomic_bool n10_change; // target_n10 update
extern DRAM_ATTR atomic_bool kmh_change; // target_kmh update

extern DRAM_ATTR _Atomic float dyn_state[5]; // For the display of the setpoints

enum MODE_OF_OPERATION {
  CONST_FRQ = 0,
  CONST_N10,
  CONST_KMH,
  MOD_SWEEP,
  MOP_END
};

// I hate C :(
typedef struct pc_consts {
  float pid1_min_pct;
  float pid1_max_pct;
  int adc_force; // The brake force inp idx
  int kmh_brake; // The brake speed inp idx
  int kmh_idler; // The idler speed inp idx
} pc_consts;

typedef struct sweep_state {
  atomic_bool buffer_side;
  atomic_int remaining_size[2];
  atomic_bool sweep_run;
  atomic_int sweep_mode;
  float sweep_buf[2][SWEEP_BUFLEN]; // Dual buffer
} sweep_state;

extern DRAM_ATTR sweep_state sws;

enum PC_STATE_VARS {
  PHYS_IN_1 = 0, // alarm
  PHYS_IN_2,     // none
  PHYS_OUT_1,    // lift
  PHYS_OUT_2,    // fan
  LOCK_PID1,     // lock pid 1
  LOCK_PID2,     // lock pid 2
  SWP_STATE,     // sweep in progress
  PARAM_CHG,     // signal PUT change
  SLIP_FLAG,     // rpm1/rpm2 slip
  STATE_END
};

extern DRAM_ATTR atomic_bool pc_state[STATE_END];
#define LIFT_OUT PHYS_OUT_1
#define FAN_OUT PHYS_OUT_2
#define ALRM_IN PHYS_IN_1

#define LIFT_OUT_GPIO GPIO_NUM_25
#define FAN_OUT_GPIO GPIO_NUM_33
#define ALRM_IN_GPIO GPIO_NUM_32

#define LB_DN 0
#define LB_UP 1

struct async_resp_arg {
  httpd_handle_t hd;
  int fd;
};

// MACRO REFLECTION
#define EXPAND(R, s, t, u, v, a, b, c, d, e, f, g, h)                          \
  R(s, t, u, v, a, b, c, d, e, f, g, h)

#define DEC_SYNTAX(s, t, u, v, a, b, c, d, e, f, g, h) a b;
#define DEC_STRUCT(data, name)                                                 \
  typedef struct name {                                                        \
    data(DEC_SYNTAX, , , , )                                                   \
  } name

#define DEF_SYNTAX(s, t, u, v, a, b, c, d, e, f, g, h) .b = c,
#define DEF_STRUCT(data, name, varname)                                        \
  DRAM_ATTR name varname = {data(DEF_SYNTAX, , , , )}

#define JSN_SYNTAX(s, t, u, v, a, b, c, d, e, f, g, h)                         \
  cJSON_AddItemToObject(s, #b, cJSON_Create##d(t.b));
#define JSN_STRUCT(data, varname, object) data(JSN_SYNTAX, object, varname, , )

#define NEW_STRUCT(data, name, varname, modifier)                              \
  DEC_STRUCT(data, name);                                                      \
  modifier DEF_STRUCT(data, name, varname)

#define PARSE_SYNTAX(s, t, u, v, a, b, c, d, e, f, g, h)                       \
  s = cJSON_GetObjectItem(res, #b);                                            \
  if (s) {                                                                     \
    g val = sub->value##g;                                                     \
    if (val >= e && val <= f) {                                                \
      u.b = (a)val;                                                            \
      h;                                                                       \
    } else v = REST_BAD;                                                       \
  }
#define JSN_PARSE(data, sub, res, varname, resp)                               \
  data(PARSE_SYNTAX, sub, res, varname, resp)

// #define QUOTE(str) #str
// #define CRASH(Z) QUOTE((Z))
// END MACRO REFLECTION

static inline void fmop();
static inline void ftn10();
static inline void ftkmh();
static inline void ftfrq();
static inline void cdrag();
static inline void cdrag();

#include "foreach.h"
#include "hwdesc.h"
// // The PC description
// NEW_STRUCT(HWINFO, hwinfo, hwdesc, const);
// // Failsafe default config, the current one is loaded from NVS
// NEW_STRUCT(SETTINGS_T, settings_t, cnt_seti, );

extern const hwinfo hwdesc;
extern settings_t cnt_seti;

void sws_reset();
int sweep_top(float *data, int len);
uint64_t pc_clock();
void lock_reset();

static inline void fmop() {
  // Allow for the 4th "OFF/UNLOADED mop!"
  // Do not allow changing when the sweep is in action
  sws_reset();
  mop_change = true;
  current_frq = hwdesc.dac_unload;
  lock_reset();
}

static inline void ftn10() {
  // Do not allow changing when the sweep is in action
  sws_reset();
  n10_change = true;
}

static inline void ftkmh() {
  // Do not allow changing when the sweep is in action
  sws_reset();
  kmh_change = true;
}

static inline void ftfrq() {
  // Do not allow changing when the sweep is in action
  sws_reset();
}

static inline void cdrag() {
  // Calculate the combined drag coefficient
  drag_coef = cnt_seti.cda * cnt_seti.rho2 * cnt_seti.whr;
  whr_inv = 1.0f / cnt_seti.whr;
}

typedef struct pcnt_isr_ctx {
  pcnt_unit_handle_t unit;
  pcnt_unit_handle_t unit_c; // complementary unit
  gpio_num_t control;
  atomic_int *value;
  uint8_t input_nr;
} pcnt_isr_ctx;

extern mcpwm_timer_handle_t adc_timer;

// All non-constexpr constants
extern DRAM_ATTR pc_consts consts;

// static inline float IRAM_ATTR calc_tq(float n10) {
//   return n10 * 1.63f; // TODO: FIXME: !!! use tq instead of force directly?!
// }

static inline float IRAM_ATTR calc_percent(float min, float inv_range,
                                           float val) {
  return (val - min) * inv_range;
}

static inline float IRAM_ATTR rev_percent(float min, float max, float val) {
  return min + (max - min) * val * 0.01f;
}

static const uint32_t send_interval =
    (uint32_t)(CPU_FREQ / (PC_CLOCK / GRAPH_STREAM_LENGTH));