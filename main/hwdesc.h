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
*   hwdesc.h contains the hardware description and the default values
*   for all tweakable parameters of the Process Controller. The
*   modified values are stored into the NVS flash, and are loaded
*   during startup.
*
*/

#pragma once
// typedef unsigned short ushort;

// dac_range: Nr. of counts per 1 % of DAC output
// adc_range_inv: 1/Nr of counts per 1% of ADC input
// pul_range_inv: 1/Nr of counts per 1% of PULSE input
// PC_CLOCK/(FILTER_LENGTH/FILTER_STREAMS): Blackman output rate, Hz
// max_I, min_I: X100 !!
// clang-format off
#define HWINFO(R, s, t, u, v)                                                                                           \
  EXPAND(R, s, t, u, v,   const int,     dac_min,          FRQ_MIN,                                             Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     dac_unload,       FRQ_MIN,                                             Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     dac_max,          FRQ_MAX,                                             Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     pid1_min,         PID1_MIN,                                            Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     pid1_max,         PID1_MAX,                                            Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     pid2_min,         PID2_MIN,                                            Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     pid2_max,         PID2_MAX,                                            Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     hwver,            100,                                                 Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   dac_range,        (FRQ_MAX - FRQ_MIN) * 0.01f,                         Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     nr_mops,          MOP_END,                                             Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     mop_manual,       CONST_FRQ,                                           Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     mop_pid1,         CONST_N10,                                           Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     mop_pid2,         CONST_KMH,                                           Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   coef_min,         -1e6,                                                Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   coef_max,         1e6,                                                 Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     input_type_adc,   INPUT_ADC,                                           Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     input_type_pulse, INPUT_PULS,                                          Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     input_type_none,  INPUT_NONE,                                          Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     nr_states,        STATE_END,                                           Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   pc_clock,         PC_CLOCK,                                            Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   pid_freq,         (PC_CLOCK / PID2_STREAM_LENGTH),                     Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   pid_bw,           (PC_CLOCK / PID2_FILTER_LENGTH) / BLACK_ATTEN_COEF,  Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   graph_freq,       (PC_CLOCK / GRAPH_STREAM_LENGTH),                    Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   graph_bw,         (PC_CLOCK / GRAPH_FILTER_LENGTH) / BLACK_ATTEN_COEF, Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   min_I,            MIN_I,                                               Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   max_I,            MAX_I,                                               Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     buflen,           SWEEP_BUFLEN,                                        Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   adc_min,          ADC_MIN,                                             Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   adc_max,          ADC_MAX,                                             Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   pulse_min,        PUL_MIN,                                             Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   pulse_max,        PUL_MAX,                                             Number, , , ,) \
  EXPAND(R, s, t, u, v,   const int,     cpu_freq,         CPU_FREQ,                                            Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   adc_range_inv,    100.f / (ADC_MAX - ADC_MIN),                         Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   pul_range_inv,    100.f / (PUL_MAX - PUL_MIN),                         Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   max_slip,         10.f,                                                Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   max_drag,         2.5f,                                                Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   max_rho2,         1.f,                                                 Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   min_whr,          0.2f,                                                Number, , , ,) \
  EXPAND(R, s, t, u, v,   const float,   max_whr,          0.4f,                                                Number, , , ,)
// clang-format on

// adc_range_inv == 0.196078
// pul_range_inv == 0.5

//target_n10: in kp
//target_kmh: in kmh
//target_frq: in val
// inr: x100
// mult_adc: UI corr. factor
// mult_pul: UI corr. factor
// s1,2m: slip coef multipliers 1,2
// slk: slip limit in kmh
// cda: drag area
// rho2: air density / 2
// whr: wheel radius
// clang-format off
#define SETTINGS_T(R, s, t, u, v)                                                                                                     \
  EXPAND(R, s, t, u, v, enum MODE_OF_OPERATION, mop,            MOP_END,    Number, 0,               MOP_END,         int,   fmop() ) \
  EXPAND(R, s, t, u, v, float,                  target_n10,     30,         Number, hwdesc.pid1_min, hwdesc.pid1_max, float, ftn10()) \
  EXPAND(R, s, t, u, v, float,                  target_kmh,     60,         Number, hwdesc.pid2_min, hwdesc.pid2_max, float, ftkmh()) \
  EXPAND(R, s, t, u, v, int,                    target_frq,     FRQ_UNLOAD, Number, hwdesc.dac_min,  hwdesc.dac_max,  int,   ftfrq()) \
  EXPAND(R, s, t, u, v, float,                  lp,             1.8f,       Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  li,             0.1f,       Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  ld,             0.0f,       Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  kp,             0.15f,      Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  ki,             0.1f,       Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  kd,             0.8f,       Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  kmh_div,        596850.f,   Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, ushort,                 kmh_off,        20,         Number, 0,               65535,           int,          ) \
  EXPAND(R, s, t, u, v, float,                  n10_div,        9.2f,       Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, ushort,                 n10_off,        6750,       Number, 0,               65535,           int,          ) \
  EXPAND(R, s, t, u, v, uint8_t,                lb_state,       LB_DN,      Bool  , LB_DN,           LB_UP,           int,          ) \
  EXPAND(R, s, t, u, v, int,                    ff_pid1,        0,          Number, 0,               hwdesc.dac_max,  int,          ) \
  EXPAND(R, s, t, u, v, int,                    ff_pid2,        0,          Number, 0,               hwdesc.dac_max,  int,          ) \
  EXPAND(R, s, t, u, v, float,                  tyre_traction,  1.0f,       Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  p3p,            0.83f,      Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  p3i,            0.0f,       Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  p3d,            0.0f,       Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  inr,            41.1f,      Number, hwdesc.min_I,    hwdesc.max_I,    float,        ) \
  EXPAND(R, s, t, u, v, float,                  s1m,            5.f,        Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  s2m,            4.f,        Number, hwdesc.coef_min, hwdesc.coef_max, float,        ) \
  EXPAND(R, s, t, u, v, float,                  slk,            2.f,        Number, 0,               hwdesc.max_slip, float,        ) \
  EXPAND(R, s, t, u, v, float,                  cda,            0.5f,       Number, 0,               hwdesc.max_drag, float, cdrag()) \
  EXPAND(R, s, t, u, v, float,                  rho2,           0.58f,      Number, 0,               hwdesc.max_rho2, float, cdrag()) \
  EXPAND(R, s, t, u, v, float,                  whr,            0.3f,       Number, hwdesc.min_whr,  hwdesc.max_whr,  float, cdrag())
// clang-format on

DEC_STRUCT(HWINFO, hwinfo);
DEC_STRUCT(SETTINGS_T, settings_t);
