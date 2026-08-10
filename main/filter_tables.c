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
*   filter_tables.c contains the filter initialization.
*
*/

#include "data_def.h"

//   blackman_state pid_filter = {
//     0,        PID_FILTER_STREAMS, PID_STREAM_LENGTH, Blackman1000,
//     filt_in, pid_filter_mem};
blackman_state pid_filter = {
    0,       PID2_FILTER_STREAMS, PID2_STREAM_LENGTH, Blackman504,
    filt_in, pid2_filter_mem};
blackman_state graph_filter = {
    0,         GRAPH_FILTER_STREAMS, GRAPH_STREAM_LENGTH, Blackman3000,
    graph_val, graph_filter_mem};
blackman_state gp_graph_filter = {0,
                                  GRAPH_FILTER_STREAMS,
                                  GRAPH_STREAM_LENGTH,
                                  Blackman3000,
                                  disp_frq_prg,
                                  gp_filter_mem};

#define PID_STREAM_LENGTH (PID_FILTER_LENGTH / PID_FILTER_STREAMS)
//   float pid_filter_mem[N_INPUTS][PID_FILTER_STREAMS + 2] = {0};
// 119.0476 Hz output rate, -60 dB @ 56.5Hz (53.62)
float pid2_filter_mem[N_INPUTS][PID2_FILTER_STREAMS + 2] = {0};
// 20 Hz output rate, -60 dB @ 9 Hz
float graph_filter_mem[N_INPUTS][GRAPH_FILTER_STREAMS + 2] = {0};
float gp_filter_mem[2][GRAPH_FILTER_STREAMS + 2] = {0};